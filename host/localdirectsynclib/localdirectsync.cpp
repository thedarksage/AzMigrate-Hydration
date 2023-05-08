#include <iostream>
#include <cstdio>
#include <exception>
#include <sstream>
#include <vector>
#include <list>
#include <string>
#include <locale>
#include <iomanip>
#include <ctime>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <ace/config.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Process_Manager.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_stdio.h>
#include <ace/Task.h>


#include "basicio.h"
#include "cdputil.h"
#include "logger.h"
#include "configurator2.h"
#include "localconfigurator.h"
#include "theconfigurator.h"
#include "dataprotectionexception.h"

#include "localdirectsync.h"
#include "cdpdatabase.h"
#include "cdpv1database.h"
#include "portablehelpersmajor.h"
#include "svproactor.h"
#include "cdpresyncapply.h"
#include "volumescopier.h"
#include "cdpvolumeutil.h"
#include "simplevolumeapplier.h"
#include "retentionvolumeapplier.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "usecfs.h"
#include "configwrapper.h"
#include "inmalertdefs.h"

#ifdef SV_UNIX
#include "portablehelpersminor.h"
#endif /* SV_UNIX */

#if SV_WINDOWS
#include "hostagenthelpers.h"
#endif
#include "globs.h"

using namespace std;

static std::string const SyncAgent("ResyncAgent");
static SV_UINT const DefaultPartitions = 128;
static SV_INT const MonitorCopiersInterval = 5;
static SV_INT const CXFailureRetries = 5; // No of retries on communication failure to CX
static unsigned char ApplyChangePercentForUpdate = 1;

#define KB 1024

LocalDirectSync::LocalDirectSync(ACE_Thread_Manager* threadManagerPtr, VOLUME_GROUP_SETTINGS const& volumeGrpSettings, const std::string &sourcename, 
								 const std::string &targetname, const SV_ULONG &blockSizeInKB, const std::string &directoryforinternaloperations)
:ACE_Task<ACE_MT_SYNCH>(threadManagerPtr),
DataProtectionSync(volumeGrpSettings,true),
m_srcVolName(sourcename),
m_tgtVolName(targetname),
m_DirectoryForInternalOperations(directoryforinternaloperations)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    INM_SAFE_ARITHMETIC(m_maxRWSize = InmSafeInt<SV_ULONG>::Type(TheConfigurator->getVxAgent().getDirectSyncBlockSizeInKB())*KB, INMAGE_EX(TheConfigurator->getVxAgent().getDirectSyncBlockSizeInKB()))
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
* FUNCTION NAME :    Constructor  - LocalDirectSync()
*
* DESCRIPTION : constructs the LocalDirectSync object.
*				                  
* INPUT PARAMETERS :  volumegroupsettings and block size in KB
*
* OUTPUT PARAMETERS :  void
*
* NOTES :
*
* return value :none.
*/
LocalDirectSync::LocalDirectSync(VOLUME_GROUP_SETTINGS const& volumeGrpSettings, const std::string &sourcename, 
								 const std::string &targetname, const SV_ULONG &blockSizeInKB, const std::string &directoryforinternaloperations)
: ACE_Task<ACE_MT_SYNCH>(ACE_Thread_Manager::instance()),
DataProtectionSync(volumeGrpSettings),
m_srcVolName(sourcename),
m_tgtVolName(targetname),
m_DirectoryForInternalOperations(directoryforinternaloperations)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    INM_SAFE_ARITHMETIC(m_maxRWSize = InmSafeInt<SV_ULONG>::Type(TheConfigurator->getVxAgent().getDirectSyncBlockSizeInKB())*KB, INMAGE_EX(TheConfigurator->getVxAgent().getDirectSyncBlockSizeInKB()))
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
* FUNCTION NAME :    UpdateProgressToCX()
*
* DESCRIPTION : Update the progress to the CX.
*				                  
* INPUT PARAMETERS :  None
*
* OUTPUT PARAMETERS :  None
*
* NOTES :
*
* return value : 1 if successfully updated , 0 if the update threshold is not reached.
*
*/
int LocalDirectSync::UpdateProgressToCX(const SV_ULONGLONG &offset, const SV_ULONGLONG &filesystemunusedbytes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    setLastResyncOffsetForDirectSync(offset, filesystemunusedbytes);
    return 0;
}


/*
* FUNCTION NAME :    Start()
*
* DESCRIPTION : Start the direct sync process.
*				                  
* INPUT PARAMETERS :  void.
*
* OUTPUT PARAMETERS :  void
*
* NOTES :
*
* return value :none.
*/
void LocalDirectSync::Start(void) 
{
    ACE_Thread_Manager proactorThreadManager;
    SVProactorTask *proactorTask = NULL;

    try 
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        do
        {
            if(JobId() == "0")
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "%s for device name %s exiting ... Looks like Pair is undergoing state change. Jobid is zero\n", 
                    FUNCTION_NAME, 
                    Settings().deviceName.c_str());
                Idle(5);
                return;
            }
            LocalConfigurator localConfigurator;
           m_cdpsettings = TheConfigurator->getCdpSettings(m_tgtVolName);

            // During Initial sync, retention is disabled. On subsequent resync it will be enabled.
            // For now our direct sync will not have retention awareness, so we will prune all the 
            // logs for for subsequent resyncs.
            bool dbExists = false;
            if( m_cdpsettings.Status() == static_cast<CDP_STATE>(CDP_ENABLED) ) 
            {
                CDPDatabase db(m_cdpsettings.Catalogue());
                dbExists = db.exists();
            }

            if ((GetResyncCopyMode() != VOLUME_SETTINGS::RESYNC_COPYMODE_FILESYSTEM) &&
                (GetResyncCopyMode() != VOLUME_SETTINGS::RESYNC_COPYMODE_FULL))
            {
                DebugPrintf(SV_LOG_ERROR, "resync copy mode %s is incorrect in settings for volume %s\n", StrResyncCopyMode[GetResyncCopyMode()],
                                          Settings().deviceName.c_str());
                break;
            }

            if (!DoPreCopyOperations())
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to do precopy operations for volume %s\n", Settings().deviceName.c_str());
                break;
            }

            if(Protocol() != TRANSPORT_PROTOOL_MEMORY)
            {
                StartProactor(proactorTask, proactorThreadManager);
            }

            cdp_volume_util u;
            VolumesCopier::VolumeInstantiator_t vi = std::bind1st(std::mem_fun(&cdp_volume_util::CreateVolume), &u);
            VolumesCopier::VolumeDestroyer_t vd = std::bind1st(std::mem_fun(&cdp_volume_util::DestroyVolume), &u);
            E_FS_SUPPORT_ACTIONS fssa = (VOLUME_SETTINGS::RESYNC_COPYMODE_FILESYSTEM == GetResyncCopyMode()) ?
                                        E_SET_NOFS_CAPABILITIES_IFNOSUPPORT : E_SET_NOFS_CAPABILITIES;
            VolumesCopier::ActionOnBytesCovered_t sa = std::bind1st(std::mem_fun(&LocalDirectSync::ActionOnNoBytesCovered),
                                                                    this);
            VolumesCopier::ActionOnBytesCovered_t ea = std::bind1st(std::mem_fun(&LocalDirectSync::ActionOnAllBytesCovered),
                                                                    this);
            VolumesCopier::ActionOnBytesCovered_t ma = std::bind1st(std::mem_fun(&LocalDirectSync::ActionOnBytesCovered),
                                                                    this);
            VolumesCopier::VolumeApplierInstantiator_t vai;
            VolumesCopier::VolumeApplierDestroyer_t vad;
			if (dbExists)
			{
				vai = std::bind1st(std::mem_fun(&LocalDirectSync::CreateRetentionVolumeApplier), this);
				vad = std::bind1st(std::mem_fun(&LocalDirectSync::DestroyRetentionVolumeApplier), this);
				m_cdpresynctxn_ptr.reset(new cdp_resync_txn(cdp_resync_txn::getfilepath(m_cdpsettings.CatalogueDir()), JobId() ));
				if(!m_cdpresynctxn_ptr ->init())
				{
					DebugPrintf(SV_LOG_ERROR, "Initialization of Retention resync transaction file failed for source %s, target %s\n", 
						m_srcVolName.c_str(), m_tgtVolName.c_str());
					break;
				}
			}
            else
            {
                vai = std::bind1st(std::mem_fun(&cdp_volume_util::CreateSimpleVolumeApplier), &u);
                vad = std::bind1st(std::mem_fun(&cdp_volume_util::DestroySimpleVolumeApplier), &u);
            }

            QuitFunction_t qf = std::bind1st(std::mem_fun(&LocalDirectSync::QuitRequested), this); 
            ReadInfo::ReadRetryInfo_t rri(ZerosForSourceReadFailures(), SourceReadRetries(), SourceReadRetriesInterval());
           
            bool useunbufferedio = localConfigurator.useUnBufferedIo();
#ifdef SV_SUN
            useunbufferedio=false;
#endif

#ifdef SV_AIX
            useunbufferedio=true;
#endif

            DebugPrintf(SV_LOG_DEBUG, "io buffer size in local direct sync = %u\n", m_maxRWSize);
			bool profile = localConfigurator.ShouldProfileDirectSync();
            VolumesCopier::CopyInfo ci(m_srcVolName, m_tgtVolName, vi, vd,
                                       Settings().fstype, Settings().sourceCapacity, 
                                       Settings().srcStartOffset, dbExists?true:TheConfigurator->getVxAgent().CompareInInitialDirectSync(), JobId(), 
                                       TheConfigurator->getVxAgent().pipelineReadWriteInDirectSync(),
									   TheConfigurator->getVxAgent().DirectSyncIOBufferCount(),
                                       m_maxRWSize, TheConfigurator->getVxAgent().getVxAlignmentSize(), fssa, sa, ea, ma,
                                       MonitorCopiersInterval, vai, vad, qf, rri, useunbufferedio, ApplyChangePercentForUpdate,
									   GetClusterBitmapCustomizationInfos(), TheConfigurator->getVxAgent().getLengthForFileSystemClustersQuery(), 
									   m_DirectoryForInternalOperations, profile);
            VolumesCopier vc(ci);
            if (!vc.Init())
            {
                DebugPrintf(SV_LOG_ERROR, "Initialization of VolumesCopier failed with error message: %s for source %s, target %s\n", 
                                          vc.GetErrorMessage().c_str(), m_srcVolName.c_str(), m_tgtVolName.c_str());
                break;
            }
        
            if (!vc.Copy())
            {
              DebugPrintf(SV_LOG_ERROR, "Copy method of VolumesCopier failed with error message: %s for source %s, target %s\n", 
                          vc.GetErrorMessage().c_str(), m_srcVolName.c_str(), m_tgtVolName.c_str());
              break;
            }

			if (profile)
			{
				std::stringstream prfmsg;
				prfmsg << "For source " << m_srcVolName << ", target " << m_tgtVolName << ", job id: " << JobId() << ", direct sync profile data:\n"
					   << "Time for reading source: " << vc.GetTimeForSourceRead().sec() << " seconds" << '\n'
		               << "Time for reading target: " << vc.GetTimeForTargetRead().sec() << " seconds" << '\n'
		               << "Time for applying: " << vc.GetTimeForApply().sec() << " seconds" << '\n'
					   << "Time for apply overhead: " << vc.GetTimeForApplyOverhead().sec() << " seconds" << '\n'
					   << "Wait time by writer stage for buffer: " << vc.GetWriterWaitTimeForBuffer().sec() << " seconds" << '\n'
					   << "Wait time by reader stage for buffer: " << vc.GetReaderWaitTimeForBuffer().sec() << " seconds" << '\n'
		               << "Time for checksums compute and compare: " << vc.GetTimeForChecksumsComputeAndCompare().sec() << " seconds" << '\n';
				RecordProfileData(prfmsg.str().c_str());
			}


        } while (0);
    }
    catch (DataProtectionException& dpe) 
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
    } 
    catch (std::exception& e) 
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
    }
    catch (...) 
    {
        DebugPrintf(SV_LOG_ERROR, "LocalDirectSync::start caught an unnknown exception\n");
    }
    Stop();
    if(Protocol() != TRANSPORT_PROTOOL_MEMORY)
    {
        StopProactor(proactorTask, proactorThreadManager);
    }
}


bool LocalDirectSync::DoPreCopyOperations(void)
{
    bool bdone = true;

    if (!File::IsExisting(m_srcVolName))
    {
        bdone = false;
        DebugPrintf(SV_LOG_ERROR, "source %s does not exist\n", m_srcVolName.c_str());
    }

    if (bdone)
    {
        std::string dev_name(m_tgtVolName);
        std::string sparsefile;
        LocalConfigurator localConfigurator;
        bool isnewvirtualvolume = false;
        if(!localConfigurator.IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(),sparsefile,isnewvirtualvolume)))
        {
            dev_name = sparsefile;
        }

        if(!isnewvirtualvolume)
        {
            if(!File::IsExisting(m_tgtVolName))
            {
                DebugPrintf(SV_LOG_ERROR, "target %s does not exist\n", m_tgtVolName.c_str());
                bdone = false;
            }
        }

        if (bdone)
        {
            cdp_volume_t tgtvol(dev_name, isnewvirtualvolume);
            VOLUME_STATE VolumeState    = GetVolumeState(m_tgtVolName.c_str());
            if( VolumeState != VOLUME_HIDDEN )
            {
                if(!tgtvol.Hide() || !tgtvol.Good())
                {
                    std::stringstream msg;
                    msg << "failed to hide target volume :" 
                        << tgtvol.GetName()<< " error: " << tgtvol.LastError();
                    DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
                    bdone = false;
                }
            }
        }
    }

    return bdone;
}


void LocalDirectSync::Run()
{
    if ( -1 == open() ) 
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED LocalDirectSync::init failed to open: %d\n", ACE_OS::last_error());
        throw "Failed to start localdirect sync thread\n" ;
    }
}


int LocalDirectSync::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
}


void LocalDirectSync::unprotect()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    Stop() ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


int LocalDirectSync::close(u_long flags)
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return 0 ;
}


int LocalDirectSync::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME) ;
    Start();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0 ;
}


std::string LocalDirectSync::RetentionApplyFileName(const VolumeWithOffsetToApply volumewithoffsettoapply)
{
   std::stringstream filename;
 
    filename << "LocalDirectSync for "
			 << JobId() << " "
			 << volumewithoffsettoapply.m_Name << " at offset " 
             << volumewithoffsettoapply.m_Offset;
    return filename.str();
}


CDPApply * LocalDirectSync::CreateCDPApplier(const std::string deviceName)
{
    CDP_SETTINGS cdpSettings = TheConfigurator->getCdpSettings(m_tgtVolName);
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = TheConfigurator->getInitialSettings().hostVolumeSettings.volumeGroups;
    CDPApply *p = new (std::nothrow) CDPResyncApply(deviceName,
                                                    Settings().sourceCapacity,
                                                    CdpSettings(),
                                                    false,
                                                    CdpResyncTxnPtr());
    return p;
}


void LocalDirectSync::DestroyCDPApplier(CDPApply *pcdpapply)
{
    if (pcdpapply)
    {
        delete pcdpapply;
    }
}


bool LocalDirectSync::UseRetentionBytesApplied(const SV_LONGLONG bytesapplied)
{
    return true;
}


bool LocalDirectSync::ShouldApplyRetention(const std::string deviceName)
{
    bool bRetVal = true;
    CDP_SETTINGS cdpSettings = TheConfigurator->getCdpSettings(m_tgtVolName);
    //Check if cdp_pruning file is present, if so exit.

    std::string cdpprunefile = cdpSettings.CatalogueDir();
    cdpprunefile += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cdpprunefile += "cdp_pruning";
    ACE_stat db_stat ={0};

    if(sv_stat(getLongPathName(cdpprunefile.c_str()).c_str(),&db_stat) == 0)
    {
		CDPDatabase db(cdpSettings.Catalogue());
		if(db.purge_retention(m_tgtVolName))
		{
			ACE_OS::unlink(cdpprunefile.c_str());
            RetentionDirFreedUpAlert a(cdpSettings.CatalogueDir());
			DebugPrintf(SV_LOG_DEBUG, "%s", a.GetMessage().c_str());
			SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to purge retention for %s,target volume %s. Exiting Dataprotection\n",cdpSettings.CatalogueDir().c_str(),m_tgtVolName.c_str()); 
			bRetVal = false;
		}
    }

    return bRetVal;
}


bool LocalDirectSync::ActionOnNoBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered)
{
    int retry = CXFailureRetries;
    DebugPrintf(SV_LOG_DEBUG,"Issuing ResyncStartNotify for volume %s %s\n", m_srcVolName.c_str(), m_tgtVolName.c_str());
    while(retry)
    {
        if(!ResyncStartNotify())
        {
            --retry;
            std::stringstream msg;
            msg << "Try - " << CXFailureRetries - retry
                << " ResyncStartNotify failed For pair source "
                <<  m_srcVolName 
                << " And target is "
                << m_tgtVolName;
            DebugPrintf(SV_LOG_WARNING,"%s\n", msg.str().c_str());
        }
        else
        {
            break;
        }
    }

    bool bdone = (0!=retry);
    if(false == bdone)
    {
        std::stringstream msg;
        msg << "Failed to notify the Resync start for pair source "
            << m_srcVolName
            << " And target "
            << m_tgtVolName;
        DebugPrintf(SV_LOG_ERROR,"%s\n", msg.str().c_str());
    }

    return bdone;
}


bool LocalDirectSync::ActionOnAllBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered)
{
    DebugPrintf(SV_LOG_DEBUG, "Issuing ResyncEndNotify for volume %s %s\n", m_srcVolName.c_str(), m_tgtVolName.c_str());
    int retry = CXFailureRetries;
    while(retry)
    {
        if(!ResyncEndNotify())
        {
            --retry;
            stringstream msg;
            msg << "Try -" << CXFailureRetries - retry
                << "ResyncEndNotify failed For pair source "
                <<  m_srcVolName 
                << " And target is "
                << m_tgtVolName;
            DebugPrintf(SV_LOG_WARNING,"%s\n", msg.str().c_str());
        }
        else
            break;
    }

    bool bdone = (0!=retry);
    if(false == bdone)
    {
        stringstream msg;
        msg << "Failed to notify the Resync end for pair : source "
            << m_srcVolName
            << " And target "
            << m_tgtVolName;
        DebugPrintf(SV_LOG_ERROR,"%s\n", msg.str().c_str());
    }
    else
    {
        retry = CXFailureRetries;
        while(retry)
        {
            if(setResyncTransitionStepOneToTwo(""))
            {
                --retry;
                stringstream msg;
                msg << "Try - " << CXFailureRetries - retry
                    << " setResyncTransitionStepOneToTwo failed For pair source "
                    <<  m_srcVolName 
                    << " And target is "
                    << m_tgtVolName;
                DebugPrintf(SV_LOG_WARNING,"%s\n", msg.str().c_str());
            }
            else
            {
                stringstream msg;
                msg << " setResyncTransitionStepOneToTwo returned success for pair : source "
                    << m_srcVolName
                    << " And target "
                    << m_tgtVolName;
                DebugPrintf(SV_LOG_DEBUG,"%s\n", msg.str().c_str());
                break;
            }
        }

        bdone = (0!=retry);
        if(false == bdone)
        {
            stringstream msg;
            msg << "Failed to setResyncTransitionStepOneToTwo for pair : source "
                << m_srcVolName
                << " And target "
                << m_tgtVolName;
            DebugPrintf(SV_LOG_ERROR,"%s\n", msg.str().c_str());
        }
    }
    
    return bdone;
}


bool LocalDirectSync::ActionOnBytesCovered(const VolumesCopier::CoveredBytesInfo_t bytescovered)
{
    /* TODO: fail if update was not successful as 
     * cx will not be able to transition */
	UpdateProgressToCX(bytescovered.m_BytesCovered, bytescovered.m_FilesystemUnusedBytes);
    return true;
}


VolumeApplier *LocalDirectSync::CreateRetentionVolumeApplier(VolumeApplier::VolumeApplierFormationInputs_t inputs)
{
    RetentionVolumeApplier::FileNameProvider_t fnp = std::bind1st(std::mem_fun(&LocalDirectSync::RetentionApplyFileName),
                                                                  this);
    RetentionVolumeApplier::CDPApplierInstantiator_t ai = std::bind1st(std::mem_fun(&LocalDirectSync::CreateCDPApplier),
                                                                    this);
    RetentionVolumeApplier::CDPApplierDestroyer_t ad = std::bind1st(std::mem_fun(&LocalDirectSync::DestroyCDPApplier),
                                                                    this);
    RetentionVolumeApplier::AppliedBytesUser_t  au = std::bind1st(std::mem_fun(&LocalDirectSync::UseRetentionBytesApplied),
                                                                  this);
    RetentionVolumeApplier::ShouldApply_t sa = std::bind1st(std::mem_fun(&LocalDirectSync::ShouldApplyRetention),
                                                             this);
    LocalConfigurator localConfigurator;
    SV_UINT maxDirectSyncChanges = localConfigurator.getMaxDirectSyncFlushToRetnSize();
    //Default - Collect 32 changes of max read write buffer size before writing to retention.
    if(maxDirectSyncChanges == 0)
    {
        maxDirectSyncChanges = 32;
    }
 
    RetentionVolumeApplier *prva = new (std::nothrow) RetentionVolumeApplier(inputs, fnp, ai, ad, au, sa);
    if (prva)
    {
        if (!prva->Init(maxDirectSyncChanges, m_maxRWSize))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to initialize RetentionVolumeApplier for target %s with erro %s\n",
				inputs.m_Name.c_str(), prva->GetErrorMessage().c_str());
            delete prva;
            prva = 0;
        }
    }
    else
    {
		DebugPrintf(SV_LOG_ERROR, "Failed to instantiate RetentionVolumeApplier for target %s\n", inputs.m_Name.c_str());
    }

    return prva;
}


void LocalDirectSync::DestroyRetentionVolumeApplier(VolumeApplier *pvolumeapplier)
{
    if (pvolumeapplier)
    {
        delete pvolumeapplier;
    }
}


void LocalDirectSync::RecordProfileData(const char *data)
{
	DebugPrintf(SV_LOG_DEBUG, "%s", data);
	std::string profilelog;

#ifdef SV_WINDOWS
	profilelog = TheConfigurator->getVxAgent().getInstallPath();
	if ('\\' != profilelog[profilelog.length() - 1])
	{
		profilelog += '\\';
	}
	profilelog += "tmp\\";
#endif

#ifdef SV_UNIX
	profilelog = "/tmp/";
#endif

	profilelog += "directsync_profile";

#ifdef SV_WINDOWS
	profilelog += "\\";
#endif
#ifdef SV_UNIX
	profilelog += "/";
#endif

	SVMakeSureDirectoryPathExists( profilelog.c_str() ) ;

	std::string tgtvolnameexcludingslash = m_tgtVolName;
	replace(tgtvolnameexcludingslash.begin(), tgtvolnameexcludingslash.end(), ':' , '_');
	replace(tgtvolnameexcludingslash.begin(), tgtvolnameexcludingslash.end(), '/' , '_');
	replace(tgtvolnameexcludingslash.begin(), tgtvolnameexcludingslash.end(), '\\' , '_');
	replace(tgtvolnameexcludingslash.begin(), tgtvolnameexcludingslash.end(), ' ' , '_');
	profilelog += tgtvolnameexcludingslash;
	profilelog += ".log";
	DebugPrintf(SV_LOG_DEBUG, "The profile data full path formed is %s.\n", profilelog.c_str());

	FILE *fp = fopen(profilelog.c_str(), "a");
	if (fp)
	{
		fprintf(fp, "==========\n");
		time_t t = time(NULL);
		fprintf(fp, "Time: %s\n", ctime(&t));
		fprintf(fp, "%s", data);
		fprintf(fp, "==========\n");
		fprintf(fp, "\n");
		fclose(fp);
	}
}
