/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : sentinel.cpp
*
* Description: Defines the entry point for the console application.
*/

//
#include <cstdlib>
#include <list>
#include <map>

#ifdef WIN32
#include <windows.h>
#include <winioctl.h>
#include <mmsystem.h>
#include <psapi.h>
#include "Tlhelp32.h"
#endif

#ifdef SV_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/prctl.h>
#include "utilfdsafeclose.h"
#endif

#include <ace/Init_ACE.h>

#include "entity.h"
#include "synchronize.h"
#include "configurevxagent.h"
#include "localconfigurator.h"
#include "logger.h"
#include "configurator2.h"
#include "portableheaders.h"
#include "sentinel.h"
#include "device.h"
#include "devicestream.h"
#include "volumemanager.h"
#include "volumestream.h"
#include "compress.h"
#include "error.h"
#include "cxtransport.h"
#include "volumechangedata.h"
#include "prismvolume.h"
#include "protectedgroupvolumes.h"
#include "inmageproduct.h"
#include "event.h"
#include "volumegroupsettings.h"
#include "threadmanager.h"
#include "host.h"
#include "genericstream.h"
#include "globs.h"

#include "HandlerCurl.h"
#include "portablehelpers.h"
#include "boost/bind.hpp"
#include "boost/ref.hpp"
#include "portablehelpersmajor.h"
#include "configwrapper.h"
#include "inmageex.h"
#include "prismsettings.h"
#include "volumeinfocollector.h"
#include "serializationtype.h"
#include "volumeop.h"
#include "serializevolumegroupsettings.h"
#include "terminateonheapcorruption.h"

#include "filterdrvifmajor.h"
#include "LogCutter.h"

#ifdef SV_UNIX
#include "persistentdevicenames.h"
#endif

using namespace std;
using namespace Logger;
//static bool g_OnChangeHostVolumeGroupFirstTime = true;

extern void GlobalInitialize();
extern Log gLog;

Configurator* TheConfigurator = NULL;


static void LogCallback(unsigned int logLevel, const char *msg)
{
    DebugPrintf((SV_LOG_LEVEL)logLevel, msg);
}


//using namespace inmage::foundation;

//namespace inmage {

/*
* FUNCTION NAME : Sentinel 
*
* DESCRIPTION : default constructor
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 	Initializes the the protection status(m_bProtecting),
the master thread id(m_liMasterGroupId),
the default time to live(m_ExitTime),
and the quit status(m_Quit)
*
* return value : NONE
*
*/


/* PR # 6513 : START 
* Added a member m_liProcessState to keep track of s2 process state.
* Initialized m_liProcessState to PROCESS_RUN in constructor of Sentinel 
* PR # 6513 : END */


Sentinel::Sentinel():	m_bProtecting(false),
m_liMasterGroupId(0),
m_bShutdownInProgress(false),
m_liProcessState(PROCESS_RUN),
m_ExitTime(GetSentinelExitTime()),
m_pMirrorError(NULL),
m_bLogFileXfer(false),
m_MetadataReadBufLen(0),
m_ProactorTask(NULL)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_pStatsNMetricsManager.reset();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
* FUNCTION NAME : ~Sentinel 
*
* DESCRIPTION : default destructor
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : Responsible for cleanup of sentinel object. Calls Destroy function	
*
* return value : NONE
*
*/
Sentinel::~Sentinel()
{
    // __asm int 3;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    Destroy();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);


}

/*
* FUNCTION NAME : 	GetSentinelExitTime
*
* DESCRIPTION : 	Acquires the default value of time to live from the configurator.
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :	The sentinels life cycle is bound by the number of seconds defined by the user. After being spawned by service(svagents)
*			the sentinel is alive of this specified duration in seconds and then exits gracefully on completion of this 
*			time and is respawned again by the service. The default is 15 minutes
*
* return value : 	Returns the default value of exit time of type integer. Key defined in the configurator is
*					SentinelExitTime
*
*/

int Sentinel::GetSentinelExitTime()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int iExitTime = 0;

    try {
        LocalConfigurator localConfigurator;
        iExitTime = localConfigurator.getSentinelExitTimeV2();
    }
    catch(...) {
        iExitTime = DEFAULT_SENTINEL_EXIT_TIME;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iExitTime;
}

/*
* FUNCTION NAME : 	RegisterWithDriver
*
* DESCRIPTION :	The sentinel signals the driver that it has started up by issuing the IOCTL_INMAGE_PROCESS_START_NOTIFY
*					ioctl. This enables the driver to set its required context appropriately. The ioctl issued is asynchronous.
*					In windows this is not completed by the driver. When Sentinel exits the ioctl is cancelled, thereby signalling 
*					the driver that it has exited. In *nix async ioct is not supported
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
* 
* NOTES :
*
* return value : true/false
*/

bool Sentinel::RegisterWithDriver(const std::string &driverName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool isregistered = false;

    //Create notifier
    try {
        FilterDriverNotifier *p = new FilterDriverNotifier(driverName);
        m_pFilterDriverNotifier.reset(p);
        DebugPrintf(SV_LOG_DEBUG, "Created filter driver notifier.\n");
    }
    catch (std::bad_alloc &e) {
        DebugPrintf(SV_LOG_ERROR, "FAILED: Memory allocation for FilterDriverNotifier object failed with error %s.@LINE %d in FILE %s\n", e.what(), LINE_NO, FILE_NAME);
        return false;
    }

    try {
        //Notify
        m_pFilterDriverNotifier->Notify(FilterDriverNotifier::E_PROCESS_START_NOTIFICATION);
        DebugPrintf(SV_LOG_DEBUG, "Filter driver %s notified.\n", driverName.c_str());

        //Set driver features
        isregistered = (SV_SUCCESS == GetDriverFeatures());
    }
    catch (FilterDriverNotifier::Exception &e) {
        //Failure to notify: log error
        DebugPrintf(SV_LOG_ERROR, "Failed to issue process start notify with error: %s\n", e.what());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);	
    return isregistered;
}


/*
* FUNCTION NAME : Init
*
* DESCRIPTION : Initializes the state of sentinel object, by initializing the quit event first, followed by registering with the driver
*				followed by creating a configuator instance, and lastly starting the master thread(which is responsible for starting up
*				the replication pairs)
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : It is called from the constructor
*
* return value : SV_SUCCESS/SV_FAILURE
*
*/

int Sentinel::Init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if ( IsInitialized() )
    {
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
        return SV_SUCCESS;
    }
    InitQuitEvent();

    m_Quit->Signal(false);
    try {
        if (!GetConfigurator(&TheConfigurator))
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to instantiate configurator. Quitting sentinel\n");
            return SV_FAILURE;
        }
        m_HostVolumeGroupSettings = TheConfigurator->getHostVolumeGroupSettings();
        m_PrismSettings = TheConfigurator->getPrismSettings();
        TheConfigurator->getHostVolumeGroupSignal().connect(this, &Sentinel::OnChangeHostVolumeGroup);
        TheConfigurator->getPrismSettingsSignal().connect(this, &Sentinel::OnChangePrismSettings);
        TheConfigurator->Start();
        m_HostId = TheConfigurator->getVxAgent().getHostId();
        m_bLogFileXfer = TheConfigurator->getVxAgent().getLogFileXfer();
        m_MetadataReadBufLen = TheConfigurator->getVxAgent().getMetadataReadBufLen();
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to instantiate configurator. Quitting sentinel\n");
        throw;
    }
    m_bInitialized = true;
    m_MasterThread.Start(RunProtect, this);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return SV_SUCCESS;
}


/*
* FUNCTION NAME : Destroy	
*
* DESCRIPTION : Cleans up the sentinel object on destruction. Disconnect from the configurator signal, followed by delete the
*				configuator object,followed by deletion of volumemanager,host,threadmanager,inmageprodut,transportfactory singleton
*				objects.
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : Called from from the destructor of sentinel 
*
* return value : SV_SUCCESS
*
*/
int Sentinel::Destroy()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Stopping Configurator...\n");
    if ( NULL != TheConfigurator ) 
    {
        TheConfigurator->getHostVolumeGroupSignal().disconnect( this );     
        TheConfigurator->getPrismSettingsSignal().disconnect( this );
        TheConfigurator->Stop();        
        TheConfigurator = NULL;
    }
    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. StoppedConfigurator.\n");

    if (m_CxTransportXferLogMonitor)
    {
        DebugPrintf(SV_LOG_DEBUG, "Waiting for file xfer log monitor\n");
        cxTransportStopXferLogMonitor();
        m_CxTransportXferLogMonitor->join();
    }

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Cleaning up protected volume objects...\n");

    //Stop the Monitoring Thread Group
    m_pStatsNMetricsManager.reset();

    MASTER_PROTECTED_VOLUME_GROUP::iterator volIter;
    for(volIter = m_MasterProtectedVolumeGroup.begin(); volIter != m_MasterProtectedVolumeGroup.end(); ++volIter )
    {
        delete volIter->first;
        volIter->second = NULL;
    }

    if ( !m_MasterProtectedVolumeGroup.empty() )
        m_MasterProtectedVolumeGroup.clear();

    MASTER_PRISM_VOLUMES::iterator prismIter;
    for (prismIter = m_MasterPrismVolumes.begin(); prismIter != m_MasterPrismVolumes.end(); prismIter++)
    {
        delete prismIter->first;
        prismIter->second = NULL;
    }
    if (!m_MasterPrismVolumes.empty())
    {
        m_MasterPrismVolumes.clear();
    }

    if (m_pMirrorError)
    {
        delete m_pMirrorError;
        m_pMirrorError = NULL;
    }

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Closing device stream ...\n");

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Closing thread manager ...\n");
    if ( false == ThreadManager::Destroy() )
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to destroy the thread manager instance. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Closing host ...\n");
    if ( false == Host::Destroy() )
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to destroy the Host instance. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Closing volume manager ...\n");
    if ( false == VolumeManager::Destroy() )
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to destroy the VolumeManager instance. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Closing product manager ...\n");
    if ( false == InmageProduct::Destroy() )
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to destroy the InmageProduct instance. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Closing transport manager ...\n");
   
    m_bInitialized = false;
    //UnInitQuitEvent();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return SV_SUCCESS;
}

/*
* FUNCTION NAME : StopProtection	
*
* DESCRIPTION : StopProtection stops all the threads responsible for replication.
*
* INPUT PARAMETERS : 	NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : SV_SUCCESS/SV_FAILURE
*
*/

/* PR # 6513 : START 
* 1. Removed the liQuitInfo parameter since m_liProcessState member
* is added to Sentinel class to hold process state 
* 2. Stopping of the replication threads are moved to master thread Protect()
* PR # 6513 : END */

int Sentinel::StopProtection(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    /* PR # 6513 : START 
    * 1. Do Master thread stop here 
    * 2. Log an error if that fails */

    /*Stop the master thread.*/
    if(false == m_MasterThread.Stop())
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to stop master thread. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
    }
    /* PR # 6513 : END */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}


/*
* FUNCTION NAME : StartNewPrismVolume	
*
* DESCRIPTION : NewPrismVolume creates a thread of control for a given prism source lun ID
*
* INPUT PARAMETERS : prism volume info structure
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE 
*
*/
void Sentinel::StartNewPrismVolume(const PRISM_VOLUME_INFO& prismVolumeInfo)
{
    PrismVolume* pPrismVolume = NULL;
    Thread* pThread = NULL;
    ThreadManager& threadMan = ThreadManager::GetInstance();
    pPrismVolume = new PrismVolume(prismVolumeInfo, m_pDeviceFilterFeatures.get(),
                                   m_HostId.c_str(), m_pMirrorError, 
                                   &m_VolumesCache.m_Vs);
    if ( pPrismVolume )
    {
        pThread = threadMan.StartThread(pPrismVolume,NULL,m_liMasterGroupId);
        if ( NULL != pThread )
        {
            std::pair<PrismVolume*,Thread*> pairGroup(pPrismVolume,pThread);
            m_MasterPrismVolumes.insert(pairGroup);
            m_bProtecting = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to start prism thread. @LINE %d in FILE %s for LUN ID %s.\n",LINE_NO,FILE_NAME, 
                                     prismVolumeInfo.sourceLUNID.c_str());
            delete pPrismVolume;
            pPrismVolume = NULL;
        }
    }
    else
    {
        /* TODO: record message here and what to do ? */
    }
}


/*
* FUNCTION NAME : StartNewProtectedGroupVolumes	
*
* DESCRIPTION : StartNewProtectedGroupVolumes creates a thread of control for a given replication pair 
*
* INPUT PARAMETERS :  VolumeGroupSettings object retrieved from the contfigurator, contains details of the volume group,
*						and the replication pair 
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE 
*
*/
void Sentinel::StartNewProtectedGroupVolumes(VolToVgsIter_t &voltovgsiter)
{
    ProtectedGroupVolumes* pProtectedGroupVolume = NULL;
    Thread* pThread = NULL;
    ThreadManager& threadMan = ThreadManager::GetInstance();
    const char *volume = voltovgsiter->first;
    VgsPtrs_t &vgsptrs = voltovgsiter->second;
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t endPointVolGrpSettings;

    if (vgsptrs.size() > 0)
    {
        VOLUME_GROUP_SETTINGS *pvolGroupSettings = *(vgsptrs.begin());
        VOLUME_GROUP_SETTINGS &volGroupSettings = *pvolGroupSettings;

        if ( (volGroupSettings.status == PROTECTED ) && ( SOURCE == volGroupSettings.direction ) )
        {
          VOLUME_GROUP_SETTINGS::volumes_iterator iter;
          for(iter = volGroupSettings.volumes.begin();iter != volGroupSettings.volumes.end(); iter++)
          {
              // FIXME:
              // what if we stay up for long periods of time?
              // a ProtectedGroupVolume may not fully initialize if the volume is a clustered volume that is offline
              // so instead of stopping this whole group we just won't add this one to the list and when it does come
              // online we will pick it up the next time we run
              //pProtectedGroupVolume->Init();
              //if (pProtectedGroupVolume->IsInitialized()) {
              const VOLUME_SETTINGS &vs = iter->second;
              if ((TRANSPORT_PROTOCOL_FILE == vs.transportProtocol) &&
                  m_bLogFileXfer &&
                  (!m_CxTransportXferLogMonitor))
              {
                  SetUpFileXferLog(vs);
              }

              if (TRANSPORT_PROTOOL_MEMORY == vs.transportProtocol)
              {
                  endPointVolGrpSettings = GetEndPointVolumeSettings(m_HostVolumeGroupSettings, vs.endpoints);
                  pProtectedGroupVolume = new ProtectedGroupVolumes(
                                                                    vs,
                                                                    volGroupSettings.direction,
                                                                    volGroupSettings.status, 
                                                                    volGroupSettings.transportSettings,
                                                                    m_pDeviceFilterFeatures.get(),
                                                                    m_HostId.c_str(),
                                                                    endPointVolGrpSettings,
                                                                    &m_VolumesCache,
                                                                    &m_MetadataReadBufLen
                                                                   );
              }
              else
              {
                  pProtectedGroupVolume = new ProtectedGroupVolumes(
                                                                    vs,
                                                                    volGroupSettings.direction,
                                                                    volGroupSettings.status, 
                                                                    volGroupSettings.transportSettings,
                                                                    m_pDeviceFilterFeatures.get(),
                                                                    m_HostId.c_str(),
                                                                    &m_VolumesCache,
                                                                    &m_MetadataReadBufLen
                                                                   );
              }

              //} else {
              //    delete pProtectedGroupVolume;
              //}
   
              if ( pProtectedGroupVolume )
              {
                  pThread = threadMan.StartThread(pProtectedGroupVolume,NULL,m_liMasterGroupId);
                  if ( NULL != pThread )
                  {
                      pair<ProtectedGroupVolumes*,Thread*> pairGroup(pProtectedGroupVolume,pThread);
                      m_MasterProtectedVolumeGroup.insert(pairGroup);
                      m_bProtecting = true;
                  }
                  else
                  {
                      DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to start volume group thread. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
                      delete pProtectedGroupVolume;
                      pProtectedGroupVolume = NULL;
                  }
                }
                else
                {
                  /* TODO: record message here and what to do ? */
                }
          }
       }
    }          
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For volume %s, there are no volume groups. This should not happen\n", volume);
    }
}


void Sentinel::SetUpFileXferLog(const VOLUME_SETTINGS& VolumeSettings)
{
    /* NOTE: This logging should be removed
     *       in case transport server does 
     *       logging itself for file xfer */
    const std::string S2XFERLOGNAME = "s2.xfer.log";
    const int S2XFERLOGMAXSIZE = 10 * 1024 * 1024;
    const int S2XFERLOGRETAINSIZE = 0;
    const int S2XFERLOGROTATECOUNT = 2;
    std::string s2xferlog = TheConfigurator->getVxAgent().getLogPathname();
    s2xferlog += S2XFERLOGNAME;
    /* TODO:  These params come from config file ? */
    cxTransportSetupXferLogging(s2xferlog, S2XFERLOGMAXSIZE, S2XFERLOGROTATECOUNT, S2XFERLOGRETAINSIZE, true);    
    m_CxTransportXferLogMonitor = cxTransportStartXferLogMonitor();
}


/*
* FUNCTION NAME : 	RunProtect
*
* DESCRIPTION : Starts the master thread of control, to commence protection of source volumes
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : The return value of Protect method
*
*/
THREAD_FUNC_RETURN Sentinel::RunProtect(void* arg)
{
    Sentinel* pObject = static_cast<Sentinel*>(arg);
    return (THREAD_FUNC_RETURN) pObject->Protect();
}

/*
* FUNCTION NAME : 	Protect
*
* DESCRIPTION : Protect is responsible for starting protection of volumes. Retrieves the information of protected volumes from 
*				the configurators initial settings. Exits only when quit is signalled or when there is a changes in the volume
*				settings
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : SV_SUCCESS/SV_FAILURE
*
*/

int Sentinel::Protect()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    /* PR # 6513 : START Added return code */
    int iStatus = SV_SUCCESS;	
    /* PR # 6513 : END */

    if ( IsInitialized() )
    {
        VolToVgs_t voltovgs;
        VolumeOps::VolumeOperator vopr;
        VolumeOps::VolToFilterDrvVol_t voltodrvvol;
        bool bstartproactor;

        SetVolumesCache();
        CollectSourcesToReplicate(voltodrvvol, vopr, voltovgs, bstartproactor);
        printVolVgs(voltovgs);

        bool bstartreplicationthreads = (voltovgs.size() > 0);
        bool bstartprismthreads = (m_PrismSettings.size() > 0);

        if (bstartreplicationthreads || bstartprismthreads) {
            std::string filterdrivername = bstartprismthreads ? INMAGE_FILTER_DEVICE_NAME : GetFilterDriverName(voltovgs.begin());
            DebugPrintf(SV_LOG_DEBUG, "Filter driver name is %s\n", filterdrivername.c_str());
            if (!RegisterWithDriver(filterdrivername))
                return SV_FAILURE;
        }

        ThreadManager& threadMan = ThreadManager::GetInstance();
        
        /* PR # 6513 : START 
        * 1. put a while instead of do while 
        * 2. Instead of checking !m_Quit->IsSignalled() for quitting, 
        *    check for m_liProcessState since this is set by main thread 
        * PR # 6513 : END */
        if (bstartproactor)
        {
            StartProactor();
        }
        
        unsigned int pendingchangesupdateinterval = TheConfigurator->getVxAgent().getPendingChangesUpdateInterval();
        
        if (bstartprismthreads)
        {
            m_pMirrorError = new MirrorError();
        }
        if (bstartreplicationthreads || bstartprismthreads)
        {
            m_liMasterGroupId = threadMan.AutoGenerateGroupId();
            m_liMonitoringGroupId = threadMan.AutoGenerateGroupId();
        }

        while ( (PROCESS_QUIT_IMMEDIATELY != m_liProcessState) &&
            (PROCESS_QUIT_TIME_OUT != m_liProcessState) && 
            (PROCESS_QUIT_REQUESTED != m_liProcessState) )
        {
            if ( false == IsProtecting() )
            {
                if (bstartreplicationthreads)
                {
                    for (VolToVgsIter_t cit = voltovgs.begin(); cit != voltovgs.end(); cit++)
                    {
                        StartNewProtectedGroupVolumes(cit);
                    }
                    //For OMS Statistics and DR Metrics Collection
                    //From the MasterThread, get the protectedvolumegroup pointer for each protected device/volume
                    //Populate that in a list and use  the protectedvolumegroup pointer to work on each device/volume 
                    //from inside a dedicated thread which is used for collection of OMS Statistics and DR Metrics.
                    ACE_OS::sleep(10);
                    StartStatsAndMetricsCollThread(m_MasterProtectedVolumeGroup);
                    //end:For OMS Statistics and DR Metrics Collection
                }
                if (bstartprismthreads)
                {
                    std::set<std::string> protectedLUNIDs;
                    for (PRISM_SETTINGS_CONST_ITERATOR prismiter = m_PrismSettings.begin(); prismiter != m_PrismSettings.end(); prismiter++)
                    {
                        bool balreadystarted = (protectedLUNIDs.end() != protectedLUNIDs.find(prismiter->first));
                        if (false == balreadystarted)
                        {
                            protectedLUNIDs.insert(prismiter->first);
                            StartNewPrismVolume(prismiter->second);
                        }
                    }
                }
            }
            if (bstartreplicationthreads && m_pDeviceFilterFeatures->IsDeviceStatsSupported())
            {
                
                DebugPrintf(SV_LOG_DEBUG, "UpdateVolumesPendingChanges done\n") ;
                std::list<std::string> failedvols ;
                int iupdateStatus = vopr.UpdateVolumesPendingChanges(voltodrvvol, TheConfigurator, 
                                                                     *(m_pFilterDriverNotifier->GetDeviceStream()), boost::ref(*m_Quit), failedvols);
                /* TODO: not needed as logging is inside 
                if (SV_SUCCESS != iupdateStatus)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to update pending changes\n");
                }
                */
            }
            m_Quit->Wait(pendingchangesupdateinterval);
        } /* end of while */

        DebugPrintf(SV_LOG_ALWAYS, "Sentinel is stopping, state = %d.\n", m_liProcessState);
        
        /* PR # 6513 : START 
        * 1. Here We need to stop the replication threads
        * 2. For checking m_bProtecting, used IsProtecting() */
        if ( (IsProtecting()) && (!m_bShutdownInProgress) )
        { // TODO: may not be needed but only do this if we were protecting 
            m_bShutdownInProgress = true;

            ThreadManager& threadMan = ThreadManager::GetInstance();
            //Stop the Monitoring ThreadGroup first and then stop the ReplicationPairs ThreadGroup
            if (false == threadMan.StopThreadGroup(m_liMonitoringGroupId,
                m_liProcessState))
            {
                iStatus = SV_FAILURE;
                DebugPrintf(SV_LOG_ERROR, "FAILED: Failed to stop monitoring thread. @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
            }
            else
            {
                if (false == threadMan.StopThreadGroup(m_liMasterGroupId,
                    m_liProcessState))
                {
                    iStatus = SV_FAILURE;
                    m_bProtecting = true;
                    m_bShutdownInProgress = false;
                    DebugPrintf(SV_LOG_ERROR, "FAILED: Failed to stop running threads. @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                }
                else
                {
                    // TODO:
                    // calling Destroy() sometimes seems to cause problems
                    // since we are going to exit and Sentinel::~Sentinel calls Destroy() 
                    // we should be ok for now
                    // sentinel.Destroy();
                    m_liMasterGroupId = 0;
                    m_bProtecting = false;
                    m_bShutdownInProgress = false;
                }
                m_liMonitoringGroupId = 0;
            }

            if (bstartproactor)
            {
                StopProactor();
            }
        } 
        else if ( m_bShutdownInProgress ) 
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_DEBUG,"Sentinel shutdown in progress \n");
        }
        else if ( !(IsProtecting()) )
        {
            DebugPrintf(SV_LOG_DEBUG, "Sentinel has stopped Protection.\n" );
        }
        /* PR # 6513 : END */
    } /* end of if ( IsInitialized() ) */
    else
    {
        /* PR # 6513 : START 
        * If Sentinel is not initialized, log error and return failure */
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"Sentinel is not initialized and Master thread protect is called. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        /* PR # 6513 : END */
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    /* PR # 6513 : START */
    return iStatus;
    /* PR # 6513 : END */
}

/*
* FUNCTION NAME : IsProtecting	
*
* DESCRIPTION : Inspects the status if, sentinel is currently protecting source volumes
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : 
*
*/
const bool Sentinel::IsProtecting() const
{
    return m_bProtecting;
}

/*
* FUNCTION NAME : HasPrismSettingsChanged	
*
* DESCRIPTION : HasPrismSettingsChanged checks if the prism settings from has changed since the last time it was
*				 retrieved from the configurator
*
* INPUT PARAMETERS :  Current prism settings from the configurator
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : returns true if there is a change else false
*
*/
bool Sentinel::HasPrismSettingsChanged(const PRISM_SETTINGS & PrismSettings) 
{
    return !(m_PrismSettings == PrismSettings);
}


/*
* FUNCTION NAME : HasSettingsChanged	
*
* DESCRIPTION : HasSettingsChanged checks if the volume group settings from has changed since the last time it was
*				 retrieved from the configurator
*
* INPUT PARAMETERS :  Current volume group settings from the configurator
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : returns true if there is a change else false
*
*/
bool Sentinel::HasSettingsChanged(const HOST_VOLUME_GROUP_SETTINGS& HostVolGrpSettings) 
{
    bool bSettingsChanged = false;
    bool bFound = false;
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t::const_iterator updVolGrpIter = HostVolGrpSettings.volumeGroups.begin();
    while((updVolGrpIter != HostVolGrpSettings.volumeGroups.end()) && (false == bSettingsChanged))
    {
        if ( updVolGrpIter->direction == SOURCE )
        {
            bFound = false;
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator oldVolGrpIter = m_HostVolumeGroupSettings.volumeGroups.begin();
            while( (oldVolGrpIter != m_HostVolumeGroupSettings.volumeGroups.end()) && (false == bSettingsChanged) && (false == bFound))
            {
                if ( *updVolGrpIter == *oldVolGrpIter) 
                    bFound = true;
                ++oldVolGrpIter;
            }
            if ( !bFound )
                bSettingsChanged = true;
        }
        ++updVolGrpIter;
    }

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator oldVolGrpIter = m_HostVolumeGroupSettings.volumeGroups.begin();
    while((oldVolGrpIter != m_HostVolumeGroupSettings.volumeGroups.end()) && (false == bSettingsChanged))
    {
        if ( oldVolGrpIter->direction == SOURCE )
        {
            bFound = false;
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t::const_iterator updVolGrpIter = HostVolGrpSettings.volumeGroups.begin();
            while( (updVolGrpIter != HostVolGrpSettings.volumeGroups.end()) && (false == bSettingsChanged) && (false == bFound) )
            {
                if ( *updVolGrpIter == *oldVolGrpIter) 
                    bFound = true;
                ++updVolGrpIter;
            }
            if ( !bFound )
                bSettingsChanged = true;
        }
        ++oldVolGrpIter;
    }

    return bSettingsChanged;
}

/*
* FUNCTION NAME : OnChangePrismSettings
*
* DESCRIPTION : This function is a callback. Called by the configurator whenever there is a change in the prism settings.
*
* INPUT PARAMETERS :  Current prism settings from the configurator
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : 
*
*/
void Sentinel::OnChangePrismSettings(PRISM_SETTINGS PrismSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    /* TODO: we do not need to update m_PrismSettings,
     *       and also we want to exit for any
     *       change in prism settings;
    AutoLock PropertyChangeGuard(m_PropertyChangeLock);

    if ( true == HasPrismSettingsChanged(PrismSettings) )
    {
         * since we are quitting 
        m_PrismSettings = PrismSettings;
    */
        DebugPrintf(SV_LOG_DEBUG,"Inside function %s, Calling Signal(PROCESS_QUIT_REQUESTED)\n", FUNCTION_NAME);
        /* TODO: still do we need to lock m_PropertyChangeLock 
         *       since we are not changing any property ? 
         * no since Signal is coming from main thread too 
         * regardless of lock from host volume group settings change */
        Signal(PROCESS_QUIT_REQUESTED);
    /*
    }
    */
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : OnChangeHostVolumeGroup
*
* DESCRIPTION : This function is a callback. Called by the configurator whenever there is a change in the volume settings.
*
* INPUT PARAMETERS :  Current host volume group settings from the configurator
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : 
*
*/
void Sentinel::OnChangeHostVolumeGroup(HOST_VOLUME_GROUP_SETTINGS HostVolGrpSettings)
{
    /* PR # 6254 Added debug print : START */
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    /* PR # 6254 : END */
    AutoLock PropertyChangeGuard(m_PropertyChangeLock);

    if ( true == HasSettingsChanged(HostVolGrpSettings))
    {
        /* NOTE: there is no need to change this 
         *       changing this has side synchronization
         *       problems since m_HostVolumeGroupSettings is 
         *       used by master thread to decide forking of 
         *       threads 
        m_HostVolumeGroupSettings = HostVolGrpSettings; */
        /* PR # 6254 Added debug print : START */
        DebugPrintf(SV_LOG_DEBUG,"Calling Signal(PROCESS_QUIT_REQUESTED)\n");
        /* PR # 6254 : END */
        Signal(PROCESS_QUIT_REQUESTED);
    }
    /* PR # 6254 Added debug print : START */
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    /* PR # 6254 : END */
}


/*
* FUNCTION NAME : IsTimeUp	
*
* DESCRIPTION : Checks if the threshold for sentinel to stay alive is crossed. 
*
* INPUT PARAMETERS :  Start time of sentinel process when it was launched.
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : return true if threshold is crossed else false. 
*
*/
bool Sentinel::IsTimeUp(const ACE_Time_Value& StartTime)
{
    DebugPrintf(SV_LOG_DEBUG, "Sentinel: is alive.\n" );

    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
    DebugPrintf(SV_LOG_DEBUG, "Current Time %lld \n",currentTime.sec() );
    if( (currentTime.sec() - StartTime.sec()) >= m_ExitTime )
    {
        DebugPrintf(SV_LOG_ALWAYS, "Time to exit. StartTime %lld, CurrentTime %lld, duration %d.\n", StartTime.sec(), currentTime.sec(), m_ExitTime );
        return true;
    }
    else if (currentTime.sec() < StartTime.sec())
    {
        DebugPrintf(SV_LOG_ERROR, "Time jump detected. StartTime %lld, CurrentTime %lld, duration %d.\n", StartTime.sec(), currentTime.sec(), m_ExitTime);
        return true;
    }
    return false;
}

/*
* FUNCTION NAME : 	Signal
*
* DESCRIPTION : Signals the quit event indicating to all threads running to exit gracefully enabling the sentinel to quit
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE 
*
*/
void Sentinel::Signal(const long int liSignal)
{
    /* PR # 6254 Added debug print : START */
    DebugPrintf(SV_LOG_ALWAYS,"ENTERED %s with liSignal = %lx\n",FUNCTION_NAME, liSignal);
    /* PR # 6254 : END */
    if ( ( PROCESS_QUIT_IMMEDIATELY == liSignal ) ||
        ( PROCESS_QUIT_REQUESTED   == liSignal ) ||
        ( PROCESS_QUIT_TIME_OUT   == liSignal ) )
    {
        m_Quit->Signal(true);
    }
    /* PR # 6254 Added debug print : START */
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    /* PR # 6254 : END */
}



bool InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
            if(!InitializeConfigurator(&TheConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                rv = false;
                break;
            }

            rv = true;
        }

        catch ( ContextualException& ce )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch( exception const& e )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, e.what());
        }
        catch ( ... )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered unknown exception.\n",
                FUNCTION_NAME);
        }

    } while(0);

    if(!rv)
    {
        DebugPrintf(SV_LOG_INFO, 
            "Replication pair information is not available.\n"
            "s2 cannot run now. please try again\n");
    }

    return rv;
}

/*
* FUNCTION NAME : main	
*
* DESCRIPTION : Entry point for the program. Starts the application. Initializes local logging,validates if the application
*				is launched by svagents,validates if s2 is not already running,initalizes the sentinel object,runs a loop until quit
*				is signalled
*
* INPUT PARAMETERS :  
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : returns 0
*
*/
int main(int argc, char* argv[])
{    
    TerminateOnHeapCorruption();
    // PR#10815: Long Path support
    ACE::init();

#ifdef SV_UNIX
    /* predefining file descriptors 0, 1 and 2 
     * so that children never accidentally write 
     * anything to valid file descriptors of
     * service like driver or svagents.log etc,. */
    const char DEV_NULL[] = "/dev/null";
    int fdin, fdout, fderr;

    fdin = fdout = fderr = -1;
    fdin = open(DEV_NULL, O_RDONLY);
    fdout = open(DEV_NULL, O_WRONLY);
    fderr = open(DEV_NULL, O_WRONLY);

    boost::shared_ptr<void> fdGuardIn(static_cast<void*>(0), boost::bind(fdSafeClose, fdin));
    boost::shared_ptr<void> fdGuardOut(static_cast<void*>(0), boost::bind(fdSafeClose, fdout));
    boost::shared_ptr<void> fdGuardErr(static_cast<void*>(0), boost::bind(fdSafeClose, fderr));

    set_resource_limits();

    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

    tal::GlobalInitialize(); // for curl
    boost::shared_ptr<void> guardTal( static_cast<void*>(0), boost::bind(tal::GlobalShutdown) );

    MaskRequiredSignals();

    {
        // The log cutter must complete before the CloseDebug() is invoked to close the log files.
        Logger::LogCutter logCutter(gLog);
        // __asm int 3;
        try {
            LocalConfigurator localConfigurator;

            //
            // PR # 6337
            // set the curl_verbose option based on settings in 
            // drscout.conf
            //
            tal::set_curl_verbose(localConfigurator.get_curl_verbose());

            boost::filesystem::path logPath(localConfigurator.getLogPathname());
            logPath /= "s2.log";

            const int maxCompletedLogFiles = localConfigurator.getLogMaxCompletedFiles();
            const boost::chrono::seconds cutInterval(localConfigurator.getLogCutInterval());
            const uint32_t logFileSize = localConfigurator.getLogMaxFileSize();
            logCutter.Initialize(logPath, maxCompletedLogFiles, cutInterval);
            logCutter.StartCutting();
            SetLogLevel( localConfigurator.getLogLevel() );
            SetLogInitSize(logFileSize);
            SetLogHostId( localConfigurator.getHostId().c_str() );
            SetLogRemoteLogLevel(SV_LOG_DISABLE);
            SetDaemonMode();

            SetValidateSvdLogCallback(boost::bind(&LogCallback, _1, _2));

            DebugPrintf(SV_LOG_DEBUG,
                    "%s sentinel max files %d cut interval %d max file size %d\n",
                    FUNCTION_NAME,
                    maxCompletedLogFiles,
                    cutInterval.count(),
                    logFileSize);
        }        
        catch(...) {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to initialize logger.\n");
                return -1;
        }

        if(!InitializeConfigurator())
        {
            return -1;
        }

        Sentinel sentinel;

        try
        {
            if ( argc != 2 )
            {
                DebugPrintf(SV_LOG_ERROR,"Incorrect number arguments passed to sentinel. Exiting...\n");
                return 0;
            }
            else if ( 0 != strcmp("svagents",argv[1]) )
            {
                DebugPrintf(SV_LOG_ERROR,"Sentinel launched by unknown process.Exiting...\n");
                //assert(!"Sentinel launched by unknown process.");
                return 0;
            }

            if (IsProcessRunning(basename_r(argv[0])))
            {
                DebugPrintf(SV_LOG_ERROR,"Sentinel already running.Exiting...\n");
                    //assert(!"Sentinel already running.");
                return 0;
            }

            ACE_Time_Value SentineStartTime = ACE_OS::gettimeofday();

            DebugPrintf( SV_LOG_DEBUG,"SV Sentinel started. " );

            string  sVersion = InmageProduct::GetInstance().GetProductVersion();
            //printf( "\t Version %s\n\n", sVersion.c_str() );
            DebugPrintf( SV_LOG_DEBUG,"\t Version %s\n\n", sVersion.c_str() );

            // only call sentinel.Init,  GetConfigurator, and cxConfigurator->getHostVolumeGroupSignal().connect 
            // once as that should be enough
            if ( sentinel.Init() == SV_SUCCESS )
            {
                do
                {
                    /* PR # 6513 : START 
                    * 1. For Keeping s2 process state, instead of g_ProcessState 
                    *    global variable, added a member m_liProcessState in 
                    *    sentinel class. 
                    * 2. Hence to set process state, 
                    *    do sentinel.SetProcessState(state) and to get the process
                    *    state, do sentinel.GetProcessState() */
                    /* PR # 6513 : END */
                    if( WAIT_SUCCESS == sentinel.Wait(SENTINEL_TRANSFER_WAIT_TIME) )
                    {
                        sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
                        sentinel.Signal(PROCESS_QUIT_IMMEDIATELY);
                        DebugPrintf(SV_LOG_DEBUG, "Sentinel: got WM_QUIT\n" );
                    }                
                    else if ( sentinel.IsTimeUp(SentineStartTime) )
                    {
                        sentinel.SetProcessState(PROCESS_QUIT_TIME_OUT);
                        sentinel.Signal(PROCESS_QUIT_TIME_OUT);
                    
                    }

                }while(!sentinel.QuitSignalled());
                /* PR # 6254 : START
                * If sentinel.GetProcessState is still not equal to
                * either PROCESS_QUIT_IMMEDIATELY or PROCESS_QUIT_TIME_OUT
                * then if we are out of above do while, Quit event can come from :
                * 1. Configurator
                * 2. svagents 
                * if this main thread is switched at while loop 
                * So call sentinel.SetProcessState(PROCESS_QUIT_REQUESTED)
                * so that ShouldQuit() return true */
                if ((PROCESS_QUIT_IMMEDIATELY != sentinel.GetProcessState()) && 
                    (PROCESS_QUIT_TIME_OUT != sentinel.GetProcessState()))
                {
                    sentinel.SetProcessState(PROCESS_QUIT_REQUESTED);
                }

                /* PR # 6254 : END */
            } /* End of if ( sentinel.Init() == SV_SUCCESS ) */
            else
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED:Failed to init sentinel.\n");
                /* PR # 6254 init failed; Process should quit immediately : START */
                sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
                /* PR # 6254 : END */
            }

            DebugPrintf( SV_LOG_DEBUG,"Sentinel: Calling StopProtection->Quit. Out of do..while\n" );
            /* PR # 6254 Added debug print : START */
            DebugPrintf( SV_LOG_DEBUG,"with ProcessState = %ld\n", 
                sentinel.GetProcessState() );
            /* PR # 6254 : END */

            /* PR # 6513 : START 
            * Removed liStopInfo parameter since a member m_liProcessState is 
            * added to keep process quit information */
            sentinel.StopProtection();
            /* PR # 6513 : END */
            DebugPrintf(SV_LOG_DEBUG, "Sentinel quitting cleanly.\n" );
        
        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s Sentinel cought exception: %s\n", FUNCTION_NAME, e.what());
            sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
            sentinel.StopProtection();
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Sentinel quitting uncleanly.\n" );
            sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
            /* PR # 6513 : START 
            * Removed liStopInfo parameter since a member m_liProcessState is 
            * added to keep process quit information */
            sentinel.StopProtection();
            /* PR # 6513 : END */
        }
    }
    CloseDebug();
    return( 0 );
}

/*
* FUNCTION NAME : Wait
*
* DESCRIPTION :  waits on teh quit event for the specified duration
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : returns the status of waiting on the event. refer event.cpp::wait for states
*
*/
WAIT_STATE Sentinel::Wait(const int iSeconds)
{
    WAIT_STATE waitRet = WAIT_TIMED_OUT;

    /* PR # 6555 : START 
    * If m_Quit is signalled return WAIT_SUCCESS and should not wait */
    if (m_Quit->IsSignalled())
    {
        waitRet = WAIT_SUCCESS;
    }
    else
    {
        waitRet = m_Quit->Wait(iSeconds);
    }
    /* PR # 6555 : END */
    return waitRet;
}

/*
* FUNCTION NAME : 	InitQuitEvent
*
* DESCRIPTION : Initializes the quit event. The event is a named event which is composed of the literal string "InMageAgent" concatenated with
*				the pid of sentinel process. This event is signalled by svagents when service shutdown request is requested by the user.
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE
*
*/
void Sentinel::InitQuitEvent()
{
    char EventName[256];

    m_ProcessId = ACE_OS::getpid();

    if (ACE_INVALID_PID != m_ProcessId)	
    {
        inm_sprintf_s(EventName, ARRAYSIZE(EventName), "%s%d", AGENT_QUITEVENT_PREFIX, m_ProcessId);
        m_Quit.reset(new Event(false, false, EventName));
    }
}

/*
* FUNCTION NAME : UnInitQuitEvent	
*
* DESCRIPTION : destroys the quit event object
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : 
*
*
void Sentinel::UnInitQuitEvent()
{    
if ( NULL != m_Quit)
{
delete m_Quit;
m_Quit = NULL;
}
}
*/
/*
* FUNCTION NAME : QuitSignalled	
*
* DESCRIPTION : Inspects the quit status
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :  
*
* return value : returns true if signalled or false
*
*/
bool Sentinel::QuitSignalled()
{
    return m_Quit->IsSignalled();
}   



/* PR # 6513 : START */

/*
* FUNCTION NAME : Sentinel::SetProcessState
*
* DESCRIPTION : sets the m_liProcessState member of Sentinel object
*
* INPUT PARAMETERS :  liProcessState is assigned to m_liProcessState
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :  
*
* return value : NONE
*
*/
void Sentinel::SetProcessState(const long int liProcessState)
{
    m_liProcessState = liProcessState;
}


/*
* FUNCTION NAME : Sentinel::GetProcessState
*
* DESCRIPTION : gets the m_liProcessState member value of Sentinel object
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS : NONE
*
* NOTES :  
*
* return value : value of m_liProcessState of Sentinel object 
*
*/
long int Sentinel::GetProcessState(void)
{
    return m_liProcessState;
}
/* PR # 6513 : END */


void Sentinel::CollectSourcesToReplicate(VolumeOps::VolToFilterDrvVol_t &voltodrvvol,
                                         VolumeOps::VolumeOperator &vopr,
                                         VolToVgs_t &voltovgs, 
                                         bool &bstartproactor)
{
    FilterDrvIfMajor fdif;

    /* has to do this */
    bstartproactor = false;
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgsiter;
    for( vgsiter = m_HostVolumeGroupSettings.volumeGroups.begin(); vgsiter != m_HostVolumeGroupSettings.volumeGroups.end(); vgsiter++ )
    {
        VOLUME_GROUP_SETTINGS &vgs = *vgsiter;
        if ((SOURCE == vgs.direction) && (PROTECTED == vgs.status))
        {
            for ( VOLUME_GROUP_SETTINGS::volumes_iterator viter = vgs.volumes.begin(); viter != vgs.volumes.end(); viter++ )
            {
                VOLUME_SETTINGS &vs = viter->second;
                if ((vs.pairState == VOLUME_SETTINGS::PAIR_PROGRESS) ||
                    (vs.pairState == VOLUME_SETTINGS::RESTART_RESYNC_CLEANUP))
                {
                    if ((TRANSPORT_PROTOOL_MEMORY == vs.transportProtocol) &&
                        ((VOLUME_SETTINGS::SYNC_DIFF != vs.syncMode) &&
                         (VOLUME_SETTINGS::SYNC_QUASIDIFF != vs.syncMode)))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Not forking replication thread for source %s as "
                                                  "it is not in differential or quasi differential state\n",
                                                  vs.deviceName.c_str());
                        continue;
                    }

                    if (TRANSPORT_PROTOOL_MEMORY == vs.transportProtocol)
                    {
                        bstartproactor = true;
                    }

                    VolToVgsIter_t it = voltovgs.find(viter->first.c_str());
                    VgsPtrs_t *pvgsptrs;
                    if (voltovgs.end() == it)
                    {
                        std::pair<VolToVgsIter_t, bool> inpair = voltovgs.insert(std::make_pair(viter->first.c_str(), VgsPtrs_t()));
                        VolToVgsIter_t initer = inpair.first;
                        VgsPtrs_t &vgsptrs = initer->second;
                        pvgsptrs = &vgsptrs;
                        VOLUME_SETTINGS &vs = viter->second;
                        std::string devNameForDriverInput = vs.deviceName;
#ifdef SV_UNIX
                        devNameForDriverInput = GetDiskNameForPersistentDeviceName(vs.deviceName, m_VolumesCache.m_Vs);
#endif
                        std::string vol = vs.isFabricVolume() ? vs.sanVolumeInfo.virtualName : devNameForDriverInput;

                        voltodrvvol.insert(std::make_pair(vol, fdif.GetFormattedDeviceNameForDriverInput(vol, vs.devicetype)));
                    }
                    else
                    {
                        VgsPtrs_t &vgsptrs = it->second;
                        pvgsptrs = &vgsptrs;
                    }
                    pvgsptrs->push_back(&vgs);
                }
            }
        }
    }
}


bool Sentinel::StartProactor()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    LocalConfigurator localConfigurator;
    bool asyncops = localConfigurator.AsyncOpEnabled();
    bool useNewApplyAlgorithm = localConfigurator.useNewApplyAlgorithm();
    if( m_ProactorTask == NULL)
    {
        do
        {
            if(!useNewApplyAlgorithm || !asyncops)
            {
                DebugPrintf(SV_LOG_DEBUG,"asyncops is false or useNewApplyAlgorithm is false so not starting the proactor thread\n");
                rv = true;
                break;
            }
    
            if(!m_ProactorTask)
            {
                m_ProactorTask = new (nothrow) SVProactorTask(&m_ThreadManager);
                if(!m_ProactorTask)
                {
                    DebugPrintf(SV_LOG_ERROR, "proactor creation failed.\n");
                    rv = false;
                    break;
                }
                m_ProactorTask ->open();
                m_ProactorTask -> waitready();
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"proactor task creation failed\n");
                rv = false;
                break;
            }
        }while(false);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool Sentinel::StopProactor()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    LocalConfigurator localConfigurator;
    bool asyncops = localConfigurator.AsyncOpEnabled();
    bool useNewApplyAlgorithm = localConfigurator.useNewApplyAlgorithm();
    do
    {
        if(!useNewApplyAlgorithm || !asyncops)
        {
            DebugPrintf(SV_LOG_DEBUG,"asyncops is false or useNewApplyAlgorithm is false so no need to stop the proactor thread\n");
            rv = true;
            break;
        }

        if(m_ProactorTask)
        {
            ACE_Proactor::end_event_loop();
            this->m_ThreadManager.wait_task(m_ProactorTask);
            ACE_Proactor::instance( ( ACE_Proactor* )0 );
            delete m_ProactorTask;
            m_ProactorTask = NULL;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Proactor task is null\n");
            break;
        }
    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


void Sentinel::SetVolumesCache(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    const int WAIT_SECS_FOR_CACHE = 10;

    do
    {
        DataCacher dataCacher;
        if (dataCacher.Init()) {
            DataCacher::CACHE_STATE cs = dataCacher.GetVolumesFromCache(m_VolumesCache);
            if (DataCacher::E_CACHE_VALID == cs) {
                DebugPrintf(SV_LOG_DEBUG, "volume summaries are present in local cache.\n");
                break;
            } else if (DataCacher::E_CACHE_DOES_NOT_EXIST == cs) {
                DebugPrintf(SV_LOG_DEBUG, "volume summaries are not present in local cache. Waiting for it to be created.\n");
            } else
                DebugPrintf(SV_LOG_ERROR, "s2 failed getting volumeinfocollector cache with error: %s.@LINE %d in FILE %s.\n", dataCacher.GetErrMsg().c_str(), LINE_NO, FILE_NAME);
        } else
            DebugPrintf(SV_LOG_ERROR, "s2 failed to initialize data cacher to get volumes cache.@LINE %d in FILE %s.\n", LINE_NO, FILE_NAME);
    } while (WAIT_SUCCESS != Wait(WAIT_SECS_FOR_CACHE));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


std::string Sentinel::GetFilterDriverName(ConstVolToVgsIter_t cit)
{
    std::string drivername;
    const VgsPtrs_t &vgsptrs = cit->second;

    if (vgsptrs.size() > 0)
    {
        const VOLUME_GROUP_SETTINGS *pvolGroupSettings = *(vgsptrs.begin());
        const VOLUME_GROUP_SETTINGS &volGroupSettings = *pvolGroupSettings;
        VOLUME_GROUP_SETTINGS::volumes_constiterator iter;
        for (iter = volGroupSettings.volumes.begin(); iter != volGroupSettings.volumes.end(); iter++)
        {
            const VOLUME_SETTINGS &vs = iter->second;
            FilterDrvIfMajor fdif;
            drivername = fdif.GetDriverName(vs.devicetype);
            break;
        }
    }

    return drivername;
}

//For OMS Statistics and DR Metrics Collection and in future any other statistics required by any consumer
//Create one dedicated thread which will collect the following
//OMS statistics for every one minute  and send this collected statistics to PS every 5 minutes (the interval is configurable)
//DR Metrics like Read Latency, Transmit Latency and Dropped Tags info every one hour (interval is configurable)
void Sentinel::StartStatsAndMetricsCollThread(const MASTER_PROTECTED_VOLUME_GROUP & masterProtectedGroup)
{
    Thread* pThreadForMetricsColl = NULL;
    ThreadManager& threadMan = ThreadManager::GetInstance();

    if (m_MasterProtectedVolumeGroup.size() > 0)
    {
        m_pStatsNMetricsManager.reset(new StatsNMetricsManager(const_cast<MASTER_PROTECTED_VOLUME_GROUP*>(&m_MasterProtectedVolumeGroup)));
        if (m_pStatsNMetricsManager.get())
        {
            pThreadForMetricsColl = threadMan.StartThread(m_pStatsNMetricsManager.get(), NULL, m_liMonitoringGroupId);//Check and Caution, here iGroupId is not passed in third parameter, and so default -1 will be taken.
            if (NULL != pThreadForMetricsColl)
            {
                DebugPrintf(SV_LOG_DEBUG, "Spawned a dedicated thread for collection of OMS Statisitcs and DR Metrics.\n");
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to create a dedicated thread for OMS statistics and DR Metrics collection. @LINE %d in FILE %s.\n", LINE_NO, FILE_NAME);
                m_pStatsNMetricsManager.reset();
            }
        }
    }
}
