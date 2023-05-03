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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

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
#include "devmetrics.h"

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
#include "common/Trace.h"
#include "filterdrvifmajor.h"
#include "securityutils.h"
#include "Monitoring.h"
#include "LogCutter.h"
#include "ShoeboxEventSettings.h"
#include "ChurnAndUploadStatsCollector.h"
#include "resthelper/HttpClient.h"
#include "RcmConfigurator.h"
#include "resthelper/HttpUtil.h"

using namespace std;
using namespace RcmClientLib;
using namespace Logger;
using namespace boost::chrono;

extern Log gLog;

//static bool g_OnChangeHostVolumeGroupFirstTime = true;

LocalConfigurator* TheLocalConfigurator = NULL;

Configurator* TheConfigurator = NULL;

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
* NOTES :  Initializes the the protection status(m_bProtecting),
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


Sentinel::Sentinel(): m_bProtecting(false),
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
* FUNCTION NAME : GetSentinelExitTime
*
* DESCRIPTION : Acquires the default value of time to live from the configurator.
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :The sentinels life cycle is bound by the number of seconds defined by the user. After being spawned by service(svagents)
*       the sentinel is alive of this specified duration in seconds and then exits gracefully on completion of this 
*       time and is respawned again by the service. The default is 15 minutes
*
* return value : Returns the default value of exit time of type integer. Key defined in the configurator is
*               SentinelExitTime
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
* FUNCTION NAME : RegisterWithDriver
*
* DESCRIPTION :The sentinel signals the driver that it has started up by issuing the IOCTL_INMAGE_PROCESS_START_NOTIFY
*               ioctl. This enables the driver to set its required context appropriately. The ioctl issued is asynchronous.
*               In windows this is not completed by the driver. When Sentinel exits the ioctl is cancelled, thereby signalling 
*               the driver that it has exited. In *nix async ioct is not supported
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
*               followed by creating a configuator instance, and lastly starting the master thread(which is responsible for starting up
*               the replication pairs)
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
    const int WAIT_SECS_FOR_SETTINGS = 5;

    if ( IsInitialized() )
    {
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
        return SV_SUCCESS;
    }

    InitQuitEvent();
    m_Quit->Signal(false);

    try {
        TheLocalConfigurator = new LocalConfigurator();

        m_HostId = RcmConfigurator::getInstance()->getHostId();
        m_bLogFileXfer = RcmConfigurator::getInstance()->getLogFileXfer();
        m_MetadataReadBufLen = RcmConfigurator::getInstance()->getMetadataReadBufLen();

        RcmConfigurator::getInstance()->GetDrainSettingsChangeddSignal().connect(this, &Sentinel::OnDrainSettingsChange);
        RcmConfigurator::getInstance()->Start(SETTINGS_SOURCE_LOCAL_CACHE);

        do {
            SVSTATUS ret = SVS_OK;
            {
                boost::mutex::scoped_lock lock(m_mutexDrainSettingsChange);
                ret = RcmConfigurator::getInstance()->GetDrainSettings(m_drainSettings,
                    m_dataTransportType, m_dataTransportSettings);
            }
            if (ret != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings: %d\n",
                    FUNCTION_NAME, ret);
                return SV_FAILURE;
            }

            if (!m_drainSettings.empty())
                break;

            DebugPrintf(SV_LOG_DEBUG, "%s: waiting for drain settings.\n", FUNCTION_NAME);

            if (WAIT_SUCCESS == Wait(WAIT_SECS_FOR_SETTINGS))
                break;

        } while (m_liProcessState == PROCESS_RUN);

    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to instantiate configurator. Quitting sentinel\n");
        throw;
    }

    if (m_liProcessState == PROCESS_RUN)
    {
        m_bInitialized = true;
        m_MasterThread.Start(RunProtect, this);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return SV_SUCCESS;
}


/*
* FUNCTION NAME : Destroy
*
* DESCRIPTION : Cleans up the sentinel object on destruction. Disconnect from the configurator signal, followed by delete the
*                configuator object,followed by deletion of volumemanager,host,threadmanager,inmageprodut,transportfactory singleton
*                objects.
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

    /*if (m_CxTransportXferLogMonitor)
    {
        DebugPrintf(SV_LOG_DEBUG, "Waiting for file xfer log monitor\n");
        cxTransportStopXferLogMonitor();
        m_CxTransportXferLogMonitor->join();
    }*/

    RcmConfigurator::getInstance()->Stop();

    DebugPrintf(SV_LOG_DEBUG,"Quitting sentinel. Cleaning up protected volume objects...\n");
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
* INPUT PARAMETERS : NONE
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

    if (!m_MasterThread.IsRunning())
        return iStatus;

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
* FUNCTION NAME : StartNewProtectedGroupVolumes
*
* DESCRIPTION : StartNewProtectedGroupVolumes creates a thread of control for a given replication pair 
*
* INPUT PARAMETERS :  DrainSettings - contains the location to drain the logs
*                      dataTransportType - type of tranposrt
*                      dataTransportSettings - transport settings
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES : 
*
* return value : NONE 
*
*/
void Sentinel::StartNewProtectedGroupVolumes()
{
    boost::mutex::scoped_lock lock(m_mutexDrainSettingsChange);

    ProtectedGroupVolumes* pProtectedGroupVolume = NULL;
    Thread* pThread = NULL;
    ThreadManager& threadMan = ThreadManager::GetInstance();

    for (DrainSettings_t::const_iterator dsit = m_drainSettings.begin();
        dsit != m_drainSettings.end(); dsit++)
    {
        DATA_PATH_TRANSPORT_SETTINGS datapathTransportSettings;

        try
        {
            datapathTransportSettings.m_diskId = dsit->DiskId;

            if (boost::iequals(TRANSPORT_TYPE_AZURE_BLOB, dsit->DataPathTransportType))
            {
                datapathTransportSettings.m_transportProtocol = TRANSPORT_PROTOCOL_BLOB;

                std::string input;
                if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                {
                     input = securitylib::base64Decode(dsit->DataPathTransportSettings.c_str(),
                        dsit->DataPathTransportSettings.length());
                }
                else
                {
                    input = dsit->DataPathTransportSettings;
                }

                const AzureBlobBasedTransportSettings blobSettings =
                    JSON::consumer<AzureBlobBasedTransportSettings>::convert(input, true);

                datapathTransportSettings.m_AzureBlobContainerSettings.sasUri = blobSettings.BlobContainerSasUri;
                datapathTransportSettings.m_AzureBlobContainerSettings.LogStorageBlobType = blobSettings.LogStorageBlobType;
            }
            else if (boost::iequals(TRANSPORT_TYPE_PROCESS_SERVER, m_dataTransportType))
            {
                datapathTransportSettings.m_transportProtocol = TRANSPORT_PROTOCOL_HTTP;

                const ProcessServerTransportSettings processServerTransportSettings =
                    JSON::consumer<ProcessServerTransportSettings>::convert(m_dataTransportSettings, true);
                datapathTransportSettings.m_ProcessServerSettings.ipAddress = processServerTransportSettings.GetIPAddress();
                datapathTransportSettings.m_ProcessServerSettings.port = boost::lexical_cast<std::string>(processServerTransportSettings.Port);
                
                datapathTransportSettings.m_ProcessServerSettings.logFolder = dsit->LogFolder;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Unsupported transport type %s received for disk %s \n",
                    FUNCTION_NAME,
                    dsit->DataPathTransportType.c_str(),
                    dsit->DiskId.c_str());
                continue;
            }

            pProtectedGroupVolume = new (std::nothrow) ProtectedGroupVolumes(datapathTransportSettings,
                m_pDeviceFilterFeatures.get(),
                m_HostId.c_str(),
                &m_VolumesCache,
                &m_MetadataReadBufLen);

            if (pProtectedGroupVolume)
            {
                pThread = threadMan.StartThread(pProtectedGroupVolume, NULL, m_liMasterGroupId);
                if (NULL != pThread)
                {
                    pair<ProtectedGroupVolumes*, Thread*> pairGroup(pProtectedGroupVolume, pThread);
                    m_MasterProtectedVolumeGroup.insert(pairGroup);
                    m_MasterProtectedVolumes.insert(std::pair<DiskId, ProtectedGroupVolumes*>(datapathTransportSettings.m_diskId, pProtectedGroupVolume));
                    m_MasterTransportSettings[datapathTransportSettings.m_diskId] = datapathTransportSettings;
                    m_bProtecting = true;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to start volume group thread for disk %s.\n", FUNCTION_NAME, dsit->DiskId.c_str());
                    delete pProtectedGroupVolume;
                    pProtectedGroupVolume = NULL;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to start volume group thread for disk %s with memory allocation failure.\n",
                    FUNCTION_NAME, dsit->DiskId.c_str());
            }
        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: For disk %s, fetching dataTransportSettings failed with exception %s\n",
                FUNCTION_NAME, dsit->DiskId.c_str(), e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: For disk %s fetching dataTransportSettings failed.\n",
                FUNCTION_NAME, dsit->DiskId.c_str());
        }
    }

    return;
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
    std::string s2xferlog = TheLocalConfigurator->getLogPathname();
    s2xferlog += S2XFERLOGNAME;
    /* TODO:  These params come from config file ? */
    //cxTransportSetupXferLogging(s2xferlog, S2XFERLOGMAXSIZE, S2XFERLOGROTATECOUNT, S2XFERLOGRETAINSIZE, true);    
    //m_CxTransportXferLogMonitor = cxTransportStartXferLogMonitor();
}


/*
* FUNCTION NAME : RunProtect
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
* FUNCTION NAME : Protect
*
* DESCRIPTION : Protect is responsible for starting protection of volumes. Retrieves the information of protected volumes from 
*                the configurators initial settings. Exits only when quit is signalled or when there is a changes in the volume
*                settings
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
    int iStatus = SV_SUCCESS;

    if ( IsInitialized() )
    {
        VolumeOps::VolumeOperator vopr;
        std::list<std::string> volumesToReplicate;
        std::map<std::string, std::string> mapVolumesToDiskId;

        const bool bstartreplicationthreads = (m_drainSettings.size() > 0);

        if (bstartreplicationthreads) {
            FilterDrvIfMajor fdif;

            std::string filterdrivername = fdif.GetDriverName(VolumeSummary::DISK);
            DebugPrintf(SV_LOG_DEBUG, "Filter driver name is %s\n", filterdrivername.c_str());

            if (!RegisterWithDriver(filterdrivername))
                return SV_FAILURE;
        }

        ThreadManager& threadMan = ThreadManager::GetInstance();
        
        unsigned int pendingchangesupdateinterval = RcmConfigurator::getInstance()->getPendingChangesUpdateInterval();
        unsigned int drprofileinterval = RcmConfigurator::getInstance()->getDRMetricsCollInterval();

        if (bstartreplicationthreads)
        {
            m_liMasterGroupId = threadMan.AutoGenerateGroupId();
            SetVolumesCache();
        }

        while ( (PROCESS_QUIT_IMMEDIATELY != m_liProcessState) &&
            (PROCESS_QUIT_TIME_OUT != m_liProcessState) && 
            (PROCESS_QUIT_REQUESTED != m_liProcessState) )
        {
            if ( false == IsProtecting() )
            {
                if (bstartreplicationthreads)
                {
                    StartNewProtectedGroupVolumes();

                    ACE_OS::sleep(5);

                    MASTER_PROTECTED_VOLUME_GROUP::iterator volIter;
                    for (volIter = m_MasterProtectedVolumeGroup.begin(); volIter != m_MasterProtectedVolumeGroup.end(); ++volIter)
                    {
                        ProtectedGroupVolumes* pgv = volIter->first;
                        std::string volumeName = pgv->GetSourceVolumeName();
                        std::string diskId = pgv->GetSourceDiskId();
                        volumesToReplicate.push_back(volumeName);
                        DebugPrintf(SV_LOG_DEBUG,
                            "mapVolumesToDiskId : volume name %s diskId %s\n",
                            volumeName.c_str(),
                            diskId.c_str());
                        mapVolumesToDiskId.insert(std::make_pair(volumeName, diskId));
                    }
                }
            }
            else
            {
                MASTER_PROTECTED_VOLUME_GROUP::iterator volIter;
                for (volIter = m_MasterProtectedVolumeGroup.begin(); volIter != m_MasterProtectedVolumeGroup.end(); ++volIter)
                {
                    if (volIter->first->IsProtecting() && volIter->first->IsResyncRequired())
                    {
                        ThreadManager& threadMan = ThreadManager::GetInstance();
                        if (threadMan.StopThread((volIter->second)->GetThreadID()))
                        {
                            volIter->first->ResetProtectionState();
                            Thread* pThread = threadMan.StartThread(volIter->first, NULL, m_liMasterGroupId);
                            if (pThread != NULL)
                            {
                                volIter->second = pThread;
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_ERROR, "%s: Failed to start volume group thread for volume %s.\n", FUNCTION_NAME, (volIter->first)->GetSourceVolumeName().c_str());
                                /*delete volIter->first;
                                volIter->second = NULL;
                                m_MasterProtectedVolumeGroup.erase(volIter);*/
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "%s: Failed to stop volume group thread for volume %s on resync required.\n", FUNCTION_NAME, (volIter->first)->GetSourceVolumeName().c_str());
                        }
                    }
                }
            }

            if (m_MasterProtectedVolumeGroup.size() > 0)
            {
                SendChurnAndUploadStats(boost::ref(*m_Quit));
            }

            //Post DeviceStats
            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
            {
                if (bstartreplicationthreads && m_pDeviceFilterFeatures->IsDeviceStatsSupported())
                {
                    static steady_clock::time_point s_lastRunTime = steady_clock::time_point::min();
                    steady_clock::time_point currTime = steady_clock::now();
                    steady_clock::duration retryInterval = seconds(pendingchangesupdateinterval);
                    if (currTime > s_lastRunTime + retryInterval)
                    {
                        s_lastRunTime = currTime;

                        VolumesStats_t vss;
                        std::list<std::string> failedvols;
                        int iupdateStatus = vopr.GetPendingChanges(volumesToReplicate,
                            vss,
                            *(m_pFilterDriverNotifier->GetDeviceStream()),
                            boost::ref(*m_Quit),
                            failedvols);

                        if (SV_SUCCESS != iupdateStatus)
                        {
                            std::string errMsg;
                            if (failedvols.size() != 0)
                            {
                                std::list<std::string>::iterator failedVolIter = failedvols.begin();
                                for (; failedVolIter != failedvols.end(); failedVolIter++)
                                {
                                    errMsg += *failedVolIter;
                                    errMsg += " ";
                                }
                            }

                            DebugPrintf(SV_LOG_ERROR, "Failed to get pending changes %s\n", errMsg.c_str());
                        }
                        else
                        {
                            MonitoringLib::PostStatsToRcm(vss, mapVolumesToDiskId, m_HostId);
                        }
                    }
                }
            }

            if (bstartreplicationthreads)
            {
                static steady_clock::time_point s_lastRunTime = steady_clock::now();
                steady_clock::time_point currTime = steady_clock::now();
                steady_clock::duration retryInterval = seconds(drprofileinterval);
                if (currTime > s_lastRunTime + retryInterval)
                {
                    s_lastRunTime = currTime;
                    try {
                        std::vector<PrBuckets> drSummary;
                        ProfilingSummaryFactory::summaryAsJson(drSummary);
                        std::string strDrSummary;
                        std::vector<PrBuckets>::iterator iterDrSummary = drSummary.begin();

                        for (/*empty*/; iterDrSummary != drSummary.end(); iterDrSummary++)
                        {
                            strDrSummary += JSON::producer<PrBuckets>::convert(*iterDrSummary);
                        }

                        DebugPrintf(SV_LOG_ALWAYS, "DR Profile Summary: %s\n", strDrSummary.c_str());
                    }
                    catch (JSON::json_exception& json)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s : caught exception %s.\n",
                            FUNCTION_NAME,
                            json.what());
                    }
                    catch (std::exception &e)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s : caught exception %s.\n",
                            FUNCTION_NAME,
                            e.what());
                    }
                    catch (...)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s : caught unknown exception.\n",
                            FUNCTION_NAME);
                    }
                }
            }

            m_Quit->Wait(60);
        } /* end of while */

        DebugPrintf(SV_LOG_ALWAYS, "Sentinel is stopping, state = %d.\n", m_liProcessState);

        /* PR # 6513 : START 
        * 1. Here We need to stop the replication threads
        * 2. For checking m_bProtecting, used IsProtecting() */
        if ( (IsProtecting()) && (!m_bShutdownInProgress) )
        { // TODO: may not be needed but only do this if we were protecting 
            m_bShutdownInProgress = true;        


            ThreadManager& threadMan = ThreadManager::GetInstance();
            if ( false == threadMan.StopThreadGroup(m_liMasterGroupId,
                m_liProcessState) )
            {
                iStatus = SV_FAILURE;
                m_bProtecting = true;
                m_bShutdownInProgress = false;
                DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to stop running threads. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
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


void Sentinel::OnDrainSettingsChange()
{
    bool bSettingsChanged = false;

    if (RcmConfigurator::getInstance()->IsAzureToAzureReplication() || RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
    {
        bSettingsChanged = HandleDrainSettingsChangeForAzureVm();
    }
    else
    {
        bSettingsChanged = HandleDrainSettingsChangeForOnPremToAzureAgent();
    }

    if (bSettingsChanged)
    {
        SetProcessState(PROCESS_QUIT_REQUESTED);
        Signal(PROCESS_QUIT_REQUESTED);
    }
    return;
}

bool Sentinel::HandleDrainSettingsChangeForOnPremToAzureAgent()
{
    boost::mutex::scoped_lock lock(m_mutexDrainSettingsChange);

    bool bSettingsChanged = false;
    DrainSettings_t newSettings;
    std::string dataTransportType, dataTransportSettings;
    if (SVS_OK != RcmConfigurator::getInstance()->GetDrainSettings(newSettings, dataTransportType, dataTransportSettings))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get drain settings\n", FUNCTION_NAME);
        return bSettingsChanged;
    }

    ProcessServerTransportSettings processServerTransportSettings;

    try
    {
        if (boost::iequals(TRANSPORT_TYPE_PROCESS_SERVER, dataTransportType))
        {
            processServerTransportSettings = JSON::consumer<ProcessServerTransportSettings>::convert(dataTransportSettings, true);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Unsupported transport type %s received.\n",
                FUNCTION_NAME, dataTransportType.c_str());

            // force a quit if the settings are not as expected
            bSettingsChanged = true;
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: fetching dataTransportSettings failed with exception %s\n",
            FUNCTION_NAME, e.what());
        // force a quit in case of failure to parse settings
        bSettingsChanged = true;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: fetching dataTransportSettings failed with an unknown exception.\n",
            FUNCTION_NAME);
        bSettingsChanged = true;
    }

    if (bSettingsChanged)
        return bSettingsChanged;

    const bool bTransportSettngsChanged = !(dataTransportSettings == m_dataTransportSettings);

    if (newSettings.size() != m_drainSettings.size())
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Settings changed: new %d  old %d\n",
            FUNCTION_NAME, newSettings.size(), m_drainSettings.size());
        bSettingsChanged = true;
        
        return bSettingsChanged;
    }

    DrainSettings_t::const_iterator iterNewDs = newSettings.begin();
    while ((iterNewDs != newSettings.end()) && !bSettingsChanged)
    {
        bool bFound = false;

        DrainSettings_t::iterator iterOldDs = m_drainSettings.begin();
        while ((iterOldDs != m_drainSettings.end()) && !bSettingsChanged && !bFound)
        {
            if (bFound = boost::iequals(iterNewDs->DiskId, iterOldDs->DiskId))
            {
                if (bTransportSettngsChanged || (*iterNewDs != *iterOldDs))
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Data Path Transport Settings changed for %s \n",
                        FUNCTION_NAME, iterNewDs->DiskId.c_str());

                    DATA_PATH_TRANSPORT_SETTINGS datapathTransportSettings;
                    datapathTransportSettings.m_diskId = iterNewDs->DiskId;
                    datapathTransportSettings.m_transportProtocol = TRANSPORT_PROTOCOL_HTTP;
                    datapathTransportSettings.m_ProcessServerSettings.logFolder = iterNewDs->LogFolder;

                    datapathTransportSettings.m_ProcessServerSettings.ipAddress = processServerTransportSettings.GetIPAddress();
                    datapathTransportSettings.m_ProcessServerSettings.port = boost::lexical_cast<std::string>(processServerTransportSettings.Port);

                    if (m_MasterProtectedVolumes[iterOldDs->DiskId]->RefreshTransportSettings(datapathTransportSettings))
                        m_MasterTransportSettings[iterOldDs->DiskId] = datapathTransportSettings;
                }
            }
            iterOldDs++;
        }

        if (!bFound)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Settings changed. New device %s \n", FUNCTION_NAME, iterNewDs->DiskId.c_str());
            bSettingsChanged = true;
        }

        iterNewDs++;
    }

    if (!bSettingsChanged)
    {
        m_drainSettings = newSettings;
        m_dataTransportType = dataTransportType;
        m_dataTransportSettings = dataTransportSettings;
    }

    return bSettingsChanged;
}


bool Sentinel::HandleDrainSettingsChangeForAzureVm()
{
    boost::mutex::scoped_lock lock(m_mutexDrainSettingsChange);

    bool bSettingsChanged = false;
    DrainSettings_t newSettings;
    std::string dataTransportType, dataTransportSettings;
    SVSTATUS ret = RcmConfigurator::getInstance()->GetDrainSettings(newSettings, dataTransportType, dataTransportSettings);
    if (ret != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings: %d\n",
            FUNCTION_NAME, ret);
        return bSettingsChanged;
    }

    if (newSettings.size() != m_drainSettings.size())
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Settings changed: new %d  old %d\n", FUNCTION_NAME,
            newSettings.size(), m_drainSettings.size());
        bSettingsChanged = true;
        return bSettingsChanged;
    }

    DrainSettings_t::const_iterator iterNewDs = newSettings.begin();
    while ((iterNewDs != newSettings.end()) && !bSettingsChanged)
    {
        bool bFound = false;

        DrainSettings_t::iterator iterOldDs = m_drainSettings.begin();
        while ((iterOldDs != m_drainSettings.end()) && !bSettingsChanged && !bFound)
        {
            if (bFound = boost::iequals(iterNewDs->DiskId, iterOldDs->DiskId))
            {

                if (*iterNewDs != *iterOldDs)
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Data Path Transport Settings changed for %s \n",
                        FUNCTION_NAME, iterNewDs->DiskId.c_str());

                    try {
                        DATA_PATH_TRANSPORT_SETTINGS datapathTransportSettings;
                        datapathTransportSettings.m_diskId = iterNewDs->DiskId;
                        datapathTransportSettings.m_transportProtocol = TRANSPORT_PROTOCOL_BLOB;

                        if (boost::iequals(TRANSPORT_TYPE_AZURE_BLOB, iterNewDs->DataPathTransportType))
                        {
                            std::string input;
                            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                            {
                                input = securitylib::base64Decode(
                                    iterNewDs->DataPathTransportSettings.c_str(),
                                    iterNewDs->DataPathTransportSettings.length());
                            }
                            else
                            {
                                input = iterNewDs->DataPathTransportSettings;
                            }

                            const AzureBlobBasedTransportSettings blobSettings =
                                JSON::consumer<AzureBlobBasedTransportSettings>::convert(input, true);

                            datapathTransportSettings.m_AzureBlobContainerSettings.sasUri = blobSettings.BlobContainerSasUri;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "%s: Unsupported transport type %s received for disk %s \n",
                                FUNCTION_NAME, iterNewDs->DataPathTransportType.c_str(),
                                iterNewDs->DiskId.c_str());

                            // force a quit if the settings are not as expected
                            bSettingsChanged = true;
                            continue;
                        }

                        if (m_MasterProtectedVolumes[iterOldDs->DiskId]->RefreshTransportSettings(datapathTransportSettings))
                            m_MasterTransportSettings[iterOldDs->DiskId] = datapathTransportSettings;
                    }
                    catch (const std::exception &e)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s: For disk %s, fetching dataTransportSettings failed with exception %s\n",
                            FUNCTION_NAME, iterNewDs->DiskId.c_str(), e.what());
                        // force a quit in case of failure to parse settings
                        bSettingsChanged = true;
                    }
                    catch (...)
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: For disk %s fetching dataTransportSettings failed with unknown exception.\n",
                            FUNCTION_NAME, iterNewDs->DiskId.c_str());
                        bSettingsChanged = true;
                    }
                }
            }
            iterOldDs++;
        }

        if (!bFound)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Settings changed. New device %s \n", FUNCTION_NAME, iterNewDs->DiskId.c_str());
            bSettingsChanged = true;
        }

        iterNewDs++;
    }

    if (!bSettingsChanged)
    {
        m_drainSettings = newSettings;
    }

    return bSettingsChanged;
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
        DebugPrintf(SV_LOG_ALWAYS, "Time to exit. StartTime %lld, CurrentTime %lld, duration %d.\n", StartTime.sec(), currentTime.sec(), m_ExitTime);
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
* FUNCTION NAME : Signal
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

void Sentinel::SendChurnAndUploadStats(QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "%s: ENTERED\n", FUNCTION_NAME);

    const int WaitTimeForDiskInitCheckInSec = 1;
    const int MaxWatiTimeToLogDiskInitErrorInSec = 900; // 15 mins

    int index = 0;
    uint64_t CumulativeDiskInitWaitTime = 0;

    try {

        if (!m_statsCollectorPtr.get())
            if (SVS_OK != InitStatsCollectorPtr())
                return;

        MASTER_PROTECTED_VOLUME_GROUP::iterator pgvIter = m_MasterProtectedVolumeGroup.begin();
        for (/*empty*/; pgvIter != m_MasterProtectedVolumeGroup.end() && !qf(0); pgvIter++)
        {
            std::string diskId = (pgvIter->first)->GetSourceDiskId();
            std::string devName = (pgvIter->first)->GetSourceDeviceName();

#ifdef SV_WINDOWS
            // remove leading slashes and dot in \\.\PhysicalDrive0
            boost::erase_all(devName, ".");
            boost::erase_all(devName, "\\");
#endif

            if (!m_statsCollectorPtr->IsDiskIdRegistered(diskId))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Adding devmetrics for device id %s\n", FUNCTION_NAME, diskId.c_str());

                while (!pgvIter->first->IsInitialized())
                {
                    // Rate controlling error log to MaxWatiTimeToLogDiskInitErrorInSec
                    if (CumulativeDiskInitWaitTime % MaxWatiTimeToLogDiskInitErrorInSec == 0) {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s: waited for %lld sec for pgv of disk %s to intialize.\n",
                            FUNCTION_NAME,
                            CumulativeDiskInitWaitTime,
                            diskId.c_str());
                    }

                    if (WAIT_SUCCESS == Wait(WaitTimeForDiskInitCheckInSec))
                    {
                        break;
                    }

                    CumulativeDiskInitWaitTime += WaitTimeForDiskInitCheckInSec;
                }
                m_statsCollectorPtr->RegisterDiskId(diskId, devName);
            }
        }

        if (m_MasterProtectedVolumeGroup.size() > 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Starting to collect churn and throughput stats\n", FUNCTION_NAME);

            m_statsCollectorPtr->SetCollectionTime();

            pgvIter = m_MasterProtectedVolumeGroup.begin();
            for (/*empty*/; pgvIter != m_MasterProtectedVolumeGroup.end() && !qf(0); pgvIter++)
            {
                std::string diskId = (pgvIter->first)->GetSourceDiskId();

                if (!m_statsCollectorPtr->IsDiskIdRegistered(diskId))
                    continue;

                SV_ULONGLONG currentCumulativeTrackedBytes = 0;

                VolumeOps::VolumeOperator VolOp;
                int nRet = VolOp.GetTotalTrackedBytes(
                    *m_pFilterDriverNotifier->GetDeviceStream(),
                    (const FilterDrvVol_t)(pgvIter->first)->GetSourceNameForDriverInput(),
                    currentCumulativeTrackedBytes);
                if (SV_SUCCESS == nRet)
                {
                    m_statsCollectorPtr->SetCumulativeTrackedBytes(diskId, currentCumulativeTrackedBytes);
                }
                else //ioctl failed as the driver has not returned the tracked bytes count
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to get the TotalTrackedBytes "
                        "count from the driver for the diskId %s. Hence not sending this data.\n",
                        diskId.c_str());
                }

                //Get Throughput Stats
                SV_ULONGLONG currentCumulativeTransferredBytes = (pgvIter->first)->GetDrainLogTransferedBytes();
                m_statsCollectorPtr->SetCumulativeTransferredBytes(diskId, currentCumulativeTransferredBytes);
            }
        }

        m_statsCollectorPtr->SendChurnAndThroughputStats(boost::ref(*m_Quit));
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "%s: EXITED\n", FUNCTION_NAME);
    return;
}

SVSTATUS Sentinel::InitStatsCollectorPtr()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    static steady_clock::time_point s_lastErrorLogTime = steady_clock::time_point::min();
    const steady_clock::duration retryInterval = seconds(1800);
    
    steady_clock::time_point currTime = steady_clock::now();

    SVSTATUS status = SVS_OK;
    std::stringstream strErrorMsg;
    if (RcmConfigurator::getInstance()->IsAzureToAzureReplication() || IsAgentRunningOnAzureVm())
    {
        ShoeboxEventSettings shoeboxSettings;

        status = RcmConfigurator::getInstance()->GetShoeboxEventSettings(shoeboxSettings);
        if (status == SVS_OK)
        {
            if (!shoeboxSettings.VaultResourceArmId.empty() && !shoeboxSettings.DiskIdToDiskNameMap.empty())
            {
                m_statsCollectorPtr.reset(new ChurnAndUploadStatsCollectorAzure(shoeboxSettings.VaultResourceArmId,
                    shoeboxSettings.DiskIdToDiskNameMap,
                    RcmConfigurator::getInstance()->getOMSStatsSendingIntervalToPS()));
            }
            else
            {
                strErrorMsg << "Not initializing churn and throughtput stats collection as ";
                if (shoeboxSettings.VaultResourceArmId.empty())
                {
                    strErrorMsg << "vault resource armid is empty. ";
                }
                if (shoeboxSettings.DiskIdToDiskNameMap.empty())
                {
                    strErrorMsg << "disk-id to disk-name mapping is empty.";
                }

                status = SVE_FAIL;
            }
        }
        else
        {
            strErrorMsg << "Not initializing churn and throughtput stats collection as GetShoeboxEventSettings() failed withe error ";
        }
    }
    else//V2A-RCM OnPremToAzure
    {
        m_statsCollectorPtr.reset(new ChurnAndUploadStatsCollectorOnPrem(RcmConfigurator::getInstance()->getOMSStatsSendingIntervalToPS()));
    }

    if (status != SVS_OK)
    {
        if (currTime > s_lastErrorLogTime + retryInterval)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s %d\n", FUNCTION_NAME, strErrorMsg.str().c_str(), status);
            s_lastErrorLogTime = currTime;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

bool InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
            Configurator *pConfigurator;
            if (!InitializeConfigurator(&pConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
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

static void LogCallback(unsigned int logLevel, const char *msg)
{
    DebugPrintf((SV_LOG_LEVEL)logLevel, "%s", msg);
}

/*
* FUNCTION NAME : main
*
* DESCRIPTION : Entry point for the program. Starts the application. Initializes local logging,validates if the application
*                is launched by svagents,validates if s2 is not already running,initalizes the sentinel object,runs a loop until quit
*                is signalled
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

    MaskRequiredSignals();

    {
        // The log cutter must complete before the CloseDebug() is invoked to close the log files.
        Logger::LogCutter logCutter(gLog);

        // __asm int 3;
        try {
            LocalConfigurator localConfigurator;

            boost::filesystem::path logPath(localConfigurator.getLogPathname());
            logPath /= "s2.log";

            // TODO-SanKumar-1612: Move this to configuration.
            const int maxCompletedLogFiles = 3;
            const boost::chrono::seconds cutInterval(300); // 5 mins.

            if (localConfigurator.getIsAzureStackHubVm())
            {
                // Auzre Stack blob storage supports a lower version of API.
                // https://learn.microsoft.com/en-us/azure-stack/user/azure-stack-acs-differences?view=azs-2206
                AzureStorageRest::HttpRestUtil::Set_X_MS_Version(
                        AzureStorageRest::HttpRestUtil::AZURE_STACK_X_MS_VERSION);
            }

            logCutter.Initialize(logPath, maxCompletedLogFiles, cutInterval);
            logCutter.StartCutting();

            SetLogLevel(localConfigurator.getLogLevel());
            SetLogHttpIpAddress(localConfigurator.getHttp().ipAddress);
            SetLogHttpPort(localConfigurator.getHttp().port);
            SetLogHostId(localConfigurator.getHostId().c_str());
            SetLogRemoteLogLevel(SV_LOG_DISABLE);
            SetSerializeType(localConfigurator.getSerializerType());
            SetLogHttpsOption(localConfigurator.IsHttps());
            SetDaemonMode();

            AzureStorageRest::HttpClient::SetRetryPolicy(0);

            Trace::Init("",
                (LogLevel)localConfigurator.getLogLevel(),
                boost::bind(&LogCallback, _1, _2));

            SetValidateSvdLogCallback(boost::bind(&LogCallback, _1, _2));

        }
        catch (...) {
            DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to initialize logger.\n");
        }



        Sentinel sentinel;

        try
        {
            if (argc != 2)
            {
                DebugPrintf(SV_LOG_ERROR, "Incorrect number arguments passed to sentinel. Exiting...\n");
                return 0;
            }
            else if (0 != strcmp("svagents", argv[1]))
            {
                DebugPrintf(SV_LOG_ERROR, "Sentinel launched by unknown process.Exiting...\n");
                //assert(!"Sentinel launched by unknown process.");
                return 0;
            }

            if (IsProcessRunning(basename_r(argv[0])))
            {
                DebugPrintf(SV_LOG_ERROR, "Sentinel already running.Exiting...\n");
                //assert(!"Sentinel already running.");
                return 0;
            }

            ACE_Time_Value SentineStartTime = ACE_OS::gettimeofday();

            DebugPrintf(SV_LOG_DEBUG, "SV Sentinel started. ");

            string  sVersion = InmageProduct::GetInstance().GetProductVersion();
            DebugPrintf(SV_LOG_DEBUG, "\t Version %s\n\n", sVersion.c_str());

            // only call sentinel.Init once as that should be enough
            if (sentinel.Init() == SV_SUCCESS)
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
                    if (WAIT_SUCCESS == sentinel.Wait(SENTINEL_TRANSFER_WAIT_TIME))
                    {
                        sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
                        sentinel.Signal(PROCESS_QUIT_IMMEDIATELY);
                        DebugPrintf(SV_LOG_DEBUG, "Sentinel: got WM_QUIT\n");
                    }
                    else if (sentinel.IsTimeUp(SentineStartTime))
                    {
                        sentinel.SetProcessState(PROCESS_QUIT_TIME_OUT);
                        sentinel.Signal(PROCESS_QUIT_TIME_OUT);

                    }

                } while (!sentinel.QuitSignalled());
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
                DebugPrintf(SV_LOG_ERROR, "FAILED:Failed to init sentinel.\n");
                /* PR # 6254 init failed; Process should quit immediately : START */
                sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
                /* PR # 6254 : END */
            }

            DebugPrintf(SV_LOG_DEBUG, "Sentinel: Calling StopProtection->Quit. Out of do..while\n");
            /* PR # 6254 Added debug print : START */
            DebugPrintf(SV_LOG_DEBUG, "with ProcessState = %ld\n",
                sentinel.GetProcessState());
            /* PR # 6254 : END */

            /* PR # 6513 : START
            * Removed liStopInfo parameter since a member m_liProcessState is
            * added to keep process quit information */
            sentinel.StopProtection();
            /* PR # 6513 : END */
            DebugPrintf(SV_LOG_DEBUG, "Sentinel quitting cleanly.\n");
        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s Sentinel cought exception: %s\n", FUNCTION_NAME, e.what());
            sentinel.SetProcessState(PROCESS_QUIT_IMMEDIATELY);
            sentinel.StopProtection();
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "Sentinel quitting uncleanly.\n");
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
* FUNCTION NAME : InitQuitEvent
*
* DESCRIPTION : Initializes the quit event. The event is a named event which is composed of the literal string "InMageAgent" concatenated with
*                the pid of sentinel process. This event is signalled by svagents when service shutdown request is requested by the user.
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

