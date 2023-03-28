#include <cstring>
#include <iostream>
#include <string>
#include <list>
#include <sstream>
#include <exception>
#include <stdlib.h>
#include <cmath>
#include <sstream>
#include <fstream>
#include <new>
#include <iomanip>

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#endif
#include "devicefilter.h"

#include "entity.h"
#include "ace/ACE.h"

#include "ace/Process_Manager.h"
#include "portableheaders.h"
#include "error.h"
#include "portable.h"
#include "svtypes.h"
#include "globs.h"
#include "s2libsthread.h"

#include "runnable.h"
#include "volumemanager.h"
#include "event.h"

#include "genericfile.h"
#include "file.h"
#include "volume.h"
#include "device.h"
#include "host.h"

#include "cxtransport.h"
#include "basetransportstream.h"
#include "transportstream.h"
#include "memorystream.h"
#include "volumestream.h"
#include "devicestream.h"


#include "volumemanager.h"

#include "genericdata.h"
#include "volumechangedata.h"

#include "volumegroupsettings.h"

#include "transport_settings.h"
#include "proxysettings.h"

#include "resyncrequestor.h"

#include "portable.h"
#include "boost/shared_ptr.hpp"

#include "svdparse.h"
#include "devmetrics.h"
//#include "hostagenthelpers.h"
#include "protectedgroupvolumes.h"
#include "configurator2.h"
#include "configurevxagent.h"
#include "StreamRecords.h" // for Tag related structures and macros.
////////////////////
//
/** Added by BSR for source io time stamps **/
#include "boost/shared_array.hpp"
/** End of the change **/

#include "validatesvdfile.h"
#include "portablehelpersmajor.h"
#include "convertorfactory.h"
#include "sharedalignedbuffer.h"
#include "volumeop.h"
#include "fileconfiguratordefs.h"
#include "dummytransportstream.h"
#include "inmstrcmp.h"

#include "decompress.h"

#include "inmsafecapis.h"
#include "configwrapper.h"
#include "inmalertdefs.h"
#include "filterdrvif.h"
#include "filterdrvifmajor.h"
#include "stopvolumegroups.h"
#include "Monitoring.h"
#include "RcmConfigurator.h"
#include "azurepageblobstream.h"
#include "azureblockblobstream.h"

using namespace RcmClientLib;

const int MAX_IDLE_WAIT_TIME  = 65 ;
const int MIN_IDLE_WAIT_TIME = 30;// seconds

/* PR # 6254 Constant for waiting for volume to be online : START */
const long int SECS_TO_WAITFOR_VOL = 5;
/* PR # 6254 : END */

const long int SECS_TO_WAITON_ERROR = 120;

const long int NETWORK_TIMEOUT_MULTIPLIER = 2;
const long int SECS_TO_WAITON_NETWORK_ERROR = 3;
const long int SECS_TO_WAITON_DRVBUSY = 60;
const long int MSECS_TO_WAITON_GET_DB_RETRY = 300;
const unsigned OCCURENCE_NO_FOR_DRVBUSY_ERRLOG = 10;

// the factor at which the error are retried with a 2 ^ N delay interval
const uint64_t  NETWORK_ERROR_RETRY_RATE_FACTOR = 4;
const uint64_t  DISK_ERROR_RETRY_RATE_FACTOR = 4;
const uint64_t  DRIVER_ERROR_RETRY_RATE_FACTOR = 4;
const uint64_t  MALLOC_ERROR_RETRY_RATE_FACTOR = 1;

// Network timeout failure default configurables

const SV_UINT DefaultAzureBlobOperationTimeoutResetInterval = 4;
const SV_ULONGLONG DefaultAzureBlobOperationMaximumTimeout = 30;
const SV_ULONGLONG DefaultAzureBlobOperationMinimumTimeout = 3;

const SV_UINT DatapathThrottleLogIntervalSec = 600;

#define NOTIFY_DRIVER_ON_NETWORK_FAILURE(Changes) \
            if (m_networkFailureCode) \
            {   \
                int iCxNotifyStatus = SV_SUCCESS; \
                unsigned long long ullFlags = COMMITDB_NETWORK_FAILURE; \
                if (SV_SUCCESS == (iCxNotifyStatus = Changes.CommitFailNotify(*m_pDeviceStream, \
                    m_SourceNameForDriverInput, \
                    ullFlags,   \
                    m_networkFailureCode))) \
                {   \
                    DebugPrintf(SV_LOG_DEBUG, "%s: CommitFailNotify success\n", FUNCTION_NAME); \
                }   \
                else \
                { \
                    DebugPrintf(SV_LOG_ERROR, "%s: CommitFailNotify failed for network error 0x%x.\n", FUNCTION_NAME, m_networkFailureCode); \
                } \
            }

#define WAIT_ON_RETRY_NETWORK_ERROR(Changes) \
    WaitOnNetworkErrorRetry(NETWORK_ERROR_RETRY_RATE_FACTOR, m_networkFailureCount, Changes);

#define WAIT_ON_RETRY_DISK_READ_ERROR() \
    WaitOnErrorRetry(DISK_ERROR_RETRY_RATE_FACTOR, m_diskReadFailureCount);

#define WAIT_ON_RETRY_DRIVER_ERROR() \
    WaitOnErrorRetry(DRIVER_ERROR_RETRY_RATE_FACTOR, m_driverFailureCount);

#define WAIT_ON_RETRY_MALLOC_ERROR() \
    WaitOnErrorRetry(MALLOC_ERROR_RETRY_RATE_FACTOR, m_memAllocFailureCount);

/* TODO: should this be 2 as default ? */
const long int SECS_TO_WAITON_ATLUNSTATS = 2;

// TODO: these should not be hard coded they should come from configuration or at least some common place
std::string const TimeStampsOnlyTag("tso_");
std::string const TagsOnlyTag("tag_");
std::string const DestDir("/home/svsystems/");
std::string const DatExtension(".dat");
std::string const MetaDataContinuationTag("_MC");
std::string const MetaDataContinuationEndTag("_ME");
std::string const WriteOrderContinuationTag("_WC");
std::string const WriteOrderContinuationEndTag("_WE");
std::string const DiffDirectory("diffs/");
std::string const PreRemoteNamePrefix("pre_completed_diff_");
std::string const FinalRemoteNamePrefix("completed_diff_");

std::string const PreRemoteNameExPrefix("pre_completed_ediffcompleted_diff_");
std::string const FinalRemoteNameExPrefix("completed_ediffcompleted_diff_");

std::string const ProfilerDiskReadOperation("DiskRead");
std::string const ProfilerNetworkTransmissionOperation("DiffUpload");
std::string const Slash("/");

bool const ContinuationEnd = true;

int const TsoContinuationId = 1;


/*
 * FUNCTION NAME :      ProtectedGroupVolumes
 *
 * DESCRIPTION :    Default Constructor
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
ProtectedGroupVolumes::ProtectedGroupVolumes(void)
    : m_bProtecting(false),
    m_Direction(TARGET),
    m_Status(UNPROTECTED),
    m_ProtectEvent(false, false),
#ifdef SV_WINDOWS
    m_DBNotifyEvent(false,true, NULL, USYNC_PROCESS),
#endif
    m_pDeviceStream(NULL),
    m_pCompress(NULL),
    m_liQuitInfo(0),
    m_WaitForDBNotify(true),
    m_dataBufferLength(0),
    m_liTotalLength(0),
    m_IdleWaitTime(MAX_IDLE_WAIT_TIME),
    //m_uiCommittedChanges(0),
    m_continuationId(0),
    m_pDeviceFilterFeatures(NULL),
    m_pHostId(NULL),
    m_SrcStartOffset(0),
    m_pVolumesCache(NULL),
    m_pMetadataReadBufLen(NULL),
    m_fpProfileLog(0),
    m_bNeedToRenameDiffs(true),
    m_BytesToDrainWithoutWait(0),
    m_TotalDiffsSize(0),
    m_TotalCompressedDiffsSize(0),
    m_LastDiffsRatePrintTime(0),
    m_fpDiffsRateProfilerLog(0),
    m_resyncRequired(false),
    m_compressMode(COMPRESS_NONE),
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, ProfilerDiskReadOperation, new NormProfilingBuckets((1024 * 1024), ProfilerDiskReadOperation, true), false, "", false, false)),
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, ProfilerNetworkTransmissionOperation, new NormProfilingBuckets((1024 * 1024), ProfilerNetworkTransmissionOperation, true), false, "", false, false)),
    m_driverFailureCount(0),
    m_diskReadFailureCount(0),
    m_networkFailureCount(0),
    m_memAllocFailureCount(0),
    m_drainLogTransferedBytes(0),
    m_networkFailureCode(0),
    m_networkTimeoutFailureCount(0),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCount(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    m_Transport[0] = m_Transport[1] = 0;
    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    InitializeMembersForS2Checks();
    /* S2CHKENABLED : END */
    ResetPerDiffProfilerValues();
    SetDefaultNetworkTimeoutValues();
}
/*
 * FUNCTION NAME :      ProtectedGroupVolumes
 *
 * DESCRIPTION :    Copy Constructor
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
ProtectedGroupVolumes::ProtectedGroupVolumes(const ProtectedGroupVolumes& rhs)
    : m_sProtectedVolumeName(rhs.GetSourceVolumeName()),
      m_sRawProtectedVolumeName(rhs.GetRawSourceVolumeName()),
      m_bProtecting(false),
      m_pDeviceStream(NULL),
      m_pCompress(NULL),
#ifdef SV_WINDOWS
      m_DBNotifyEvent(rhs.m_DBNotifyEvent),
#endif
      m_ProtectEvent(rhs.m_ProtectEvent),
      /* TODO: what about m_pVolumeSettings ? 
       * should we do new of rhs' volume settings ? */
      m_liQuitInfo(0),
      m_WaitForDBNotify(true),
      m_dataBufferLength(0),
      m_liTotalLength(0),
      m_IdleWaitTime(MAX_IDLE_WAIT_TIME),
      //m_uiCommittedChanges(0),
      m_continuationId(0),
    m_pDeviceFilterFeatures(rhs.m_pDeviceFilterFeatures),
    m_pHostId(rhs.m_pHostId),
    m_SrcStartOffset(rhs.m_SrcStartOffset),
    m_pVolumesCache(rhs.m_pVolumesCache),
    m_pMetadataReadBufLen(rhs.m_pMetadataReadBufLen),
    m_fpProfileLog(0),
    m_FinalPaths(rhs.m_FinalPaths),
    m_bNeedToRenameDiffs(true),
    m_BytesToDrainWithoutWait(0),
    m_TotalDiffsSize(0),
    m_TotalCompressedDiffsSize(0),
    m_LastDiffsRatePrintTime(0),
    m_fpDiffsRateProfilerLog(0),
    m_resyncRequired(rhs.m_resyncRequired),
    m_compressMode(rhs.m_compressMode),
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),
    m_driverFailureCount(0),
    m_diskReadFailureCount(0),
    m_networkFailureCount(0),
    m_memAllocFailureCount(0),
    m_drainLogTransferedBytes(0),
    m_networkFailureCode(0),
    m_networkTimeoutFailureCount(0),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCount(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    m_Transport[0] = m_Transport[1] = 0;
    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    InitializeMembersForS2Checks();
    /* S2CHKENABLED : END */
    ResetPerDiffProfilerValues();
	SetDefaultNetworkTimeoutValues();
}

ProtectedGroupVolumes::ProtectedGroupVolumes(const DATA_PATH_TRANSPORT_SETTINGS &settings,
    const DeviceFilterFeatures *pDeviceFilterFeatures, 
    const char *pHostId,
    const DataCacher::VolumesCache *pVolumesCache,
    const size_t *pMetadataReadBufLen)
    : m_sProtectedVolumeName(settings.m_diskId),
    m_datapathTransportSettings(settings),
    m_Direction(SOURCE),
    m_SrcStartOffset(0),
    m_bProtecting(false),
    m_ProtectEvent(false, false),
#ifdef SV_WINDOWS
    m_DBNotifyEvent(false, true, NULL, USYNC_PROCESS),
#endif
    m_dataBufferLength(0),
    m_liTotalLength(0),
    m_pDeviceStream(NULL),
    m_pCompress(NULL),
    m_liQuitInfo(0),
    m_WaitForDBNotify(true),
    m_VolumeChunkSize(0),
    m_VolumeSize(0),
    m_IdleWaitTime(MAX_IDLE_WAIT_TIME),
    m_continuationId(0),
    m_pDeviceFilterFeatures(pDeviceFilterFeatures),
    m_pHostId(pHostId),
    m_pVolumesCache(pVolumesCache),
    m_pMetadataReadBufLen(pMetadataReadBufLen),
    m_fpProfileLog(0),
    m_bNeedToRenameDiffs(true),
    m_BytesToDrainWithoutWait(0),
    m_TotalDiffsSize(0),
    m_TotalCompressedDiffsSize(0),
    m_LastDiffsRatePrintTime(0),
    m_fpDiffsRateProfilerLog(0),
    m_transportProtocol(settings.m_transportProtocol),
    m_resyncRequired(false),
    m_rpoThreshold(0),
    m_compressMode(COMPRESS_NONE),
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),
    m_driverFailureCount(0),
    m_diskReadFailureCount(0),
    m_networkFailureCount(0),
    m_memAllocFailureCount(0),
    m_drainLogTransferedBytes(0),
    m_networkFailureCode(0),
    m_networkTimeoutFailureCount(0),
    m_bTransportSettingsChanged(false),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCount(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    SetRawSourceVolumeName();
    InitializeMembersForS2Checks();
    ResetPerDiffProfilerValues();
    SetTransport();
    m_TransportSettings.ipAddress = m_datapathTransportSettings.m_ProcessServerSettings.ipAddress;
    m_TransportSettings.sslPort = m_datapathTransportSettings.m_ProcessServerSettings.port;
    SetDefaultNetworkTimeoutValues();
}

/*
 * FUNCTION NAME :  WaitOnQuitEvent
 *
 * DESCRIPTION :    Waits on the protect event for specified number of seconds. This is in case
 *                  quit is signalled during wait,the protection thread can exit gracefully
 *
 * INPUT PARAMETERS :  Number of seconds, milli seconds to wait
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the wait state. refer the wait_state enum data type
 *
 */
WAIT_STATE ProtectedGroupVolumes::WaitOnQuitEvent(long int seconds, long int milli_seconds)
{
    WAIT_STATE waitRet = m_ProtectEvent.Wait(seconds, milli_seconds);
    return waitRet;
}

/*
 * FUNCTION NAME :  CreateVolume
 *
 * DESCRIPTION :    Creates the Volume object, through the volume manager. The volume manager
 *                  returns an instance of Volume class. NULL is returned in case it could not create 
 *                  the instance.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if volume object is created successfully else false
 *
 */
bool ProtectedGroupVolumes::CreateVolume()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    VolumeManager& VolMan = VolumeManager::GetInstance();
    m_pProtectedVolume = VolMan.CreateVolume(GetRawSourceVolumeName(), VolumeSummary::DISK, &m_pVolumesCache->m_Vs);
    bool bRetVal = (NULL != m_pProtectedVolume);
    if (false == bRetVal)
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED : CreateVolume() failed @LINE %d in File %s for volume %s\n", LINE_NO, FILE_NAME,
                    GetSourceVolumeName().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (bRetVal);
}


/*
* FUNCTION NAME :  OpenVolume
*
* DESCRIPTION :  Opens volume in read mode.
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :
*
* return value : returns true if volume was opened else false
*
*/
bool ProtectedGroupVolumes::OpenVolume(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    BasicIo::BioOpenMode mode = cdp_volume_t::GetSourceVolumeOpenMode(VolumeSummary::DISK);
    std::stringstream ssmode;
    ssmode << std::hex << std::showbase << mode;
    DebugPrintf(SV_LOG_DEBUG, "Opening in mode %s.\n", ssmode.str().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (SV_SUCCESS == m_pProtectedVolume->Open(mode));
}

/*
 * FUNCTION NAME :  IsCompressionEnabled
 *
 * DESCRIPTION :    Inspects if source side compression is enabled. If so in-memory compression
 *                  is performed before data is uploaded to cx
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return true if source side compression is enabled else false
 *
 */
const bool ProtectedGroupVolumes::IsCompressionEnabled() const
{
    return (COMPRESS_SOURCESIDE == m_compressMode);
}
/*
 * FUNCTION NAME :  Stop
 *
 * DESCRIPTION :    This interface is defined in runnable class and is implemented for thread
 *                  stop operation.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the thread stop status
 *
 */
THREAD_FUNC_RETURN ProtectedGroupVolumes::Stop(const long int PARAM)
{
    DebugPrintf(SV_LOG_ALWAYS,
        "Stopping protection for volume %s. Thread Shutting down..\n",
        GetSourceVolumeName().c_str());

    return (THREAD_FUNC_RETURN) StopProtection(PARAM);
}
/*
 * FUNCTION NAME :
 *
 * DESCRIPTION :
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
THREAD_FUNC_RETURN ProtectedGroupVolumes::Run()
{
    return (THREAD_FUNC_RETURN) Protect();
}
/*
 * FUNCTION NAME :  StopProtection
 *
 * DESCRIPTION :    Sets the stop protection information to m_liQuitInfo, wakes up the thread in
 *                   case it is waiting on the driver to signal arrival of data
 *
 * INPUT PARAMETERS :  long int liParam stores quit information,signalling whether quit is called for immediately
 *                      or quit is called as timout has occurred,or a quit request is called
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS/SV_FAILURE
 *
 */
int ProtectedGroupVolumes::StopProtection(const long int liParam)
{
    /* BUG # 6684 : START */
    int iStatus = SV_SUCCESS;
    /* BUG # 6684 : END */
    m_liQuitInfo = liParam;
    boost::unique_lock<boost::mutex> lock(m_DBNotifyMutex);
#ifdef SV_WINDOWS
    if ( IsProtecting() )
    {
        m_DBNotifyEvent.Signal(true);
    }
#endif
    m_ProtectEvent.Signal(true);
#ifdef SV_UNIX
    /* BUG # 6684 : START
     * Call m_pDeviceStream->IsOpen only if m_pDeviceStream is not NULL */
    if (m_pDeviceStream)
    {
        /* BUG # 6684 : END */
        //calling stop on the replication threads will set the quit event. so just wakeup all those threads which are sleeping.
        if ( m_pDeviceStream->IsOpen() )
        {
            DebugPrintf(SV_LOG_DEBUG,"Sentinel is notifying driver to wake up all threads\n.");
            if ( (iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_WAKEUP_ALL_THREADS) ) != SV_SUCCESS )
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Sentinel notification to driver to wake up all threads failed for volume %s\n.", 
                            GetSourceVolumeName().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"SUCCEEDED: Sentinel notification to driver to wake up all threads succeeded: \n.");
            }
        }
        /* BUG # 6684 : START */
    } /* End of if (m_pDeviceStream) */
    /* BUG # 6684 : END */
#endif

    /* BUG # 6684 : START */
    return iStatus;
    /* BUG # 6684 : END */
}

/*
 * FUNCTION NAME :      ProtectedGroupVolumes
 *
 * DESCRIPTION :    Destructor
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
ProtectedGroupVolumes::~ProtectedGroupVolumes()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    Destroy();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}
/*
 * FUNCTION NAME :  IsStopRequested
 *
 * DESCRIPTION :    Inspects the stop event state to determine if stop protection is requested
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if stop protection is signalled else false
 *
 */
const bool ProtectedGroupVolumes::IsStopRequested() const
{
    return m_ProtectEvent.IsSignalled();
}
/*
 * FUNCTION NAME :  Destroy
 *
 * DESCRIPTION :    Destroy cleans up the protectedgroupvolume. Deletes the volume instance,
 *                  volume stream instance,device stream instance,transport stream instance,
 *                  compress stream instance,volume settings instance, and the buffer used to
 *                  store the data while in datastream or mode.
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
void ProtectedGroupVolumes::Destroy()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    CloseFilesForChecks();

    if (m_fpProfileLog)
    {
        fclose(m_fpProfileLog);
        m_fpProfileLog = 0;
    }

    if (m_fpDiffsRateProfilerLog)
    {
        PrintDiffsRateEntry(time(NULL));
        fclose(m_fpDiffsRateProfilerLog);
        m_fpDiffsRateProfilerLog = 0;
    }

    if ( NULL != m_pDeviceStream )
    {
        m_pDeviceStream->Close();
        delete m_pDeviceStream;
        m_pDeviceStream = NULL;
    }

    if ( NULL != m_pCompress )
    {
        delete m_pCompress;
        m_pCompress = NULL;
    }

    m_dataBufferLength = 0;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}

/*
void ProtectedGroupVolumes::DestroyDiffSyncObj(void)
{
    if (m_DiffsyncObj)
    {
        delete m_DiffsyncObj;
        m_DiffsyncObj = 0;
    }
}
*/

/*
 * FUNCTION NAME :  InitTransport
 *
 * DESCRIPTION :    Creates and initializes the transport stream
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if transport stream is created successfully
 *
 */
int ProtectedGroupVolumes::InitTransport()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;

    if ( SV_SUCCESS == CreateTransportStream() )
    {
        if ( SV_SUCCESS == OpenTransportStream() )
        {
            DebugPrintf(SV_LOG_DEBUG,"SV_SUCCESS: Initialized transport stream to send data. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        }
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to open transport stream to send data. Attempting to create a new stream. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to create transport stream to send data. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  GetVolumeChunkSize
 *
 * DESCRIPTION :    Determines the size of data to be uploaded in a single file. It is a
 *                  configurable value manually stored in the config file and retrieved thru
 *                  the configurator.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :          Default value is 64MB
 *
 * return value : returns the size of
 *
 */
unsigned long int ProtectedGroupVolumes::GetVolumeChunkSize()
{
    unsigned long int uliVolumeChunkSize = 0;
    //Get Volume Chunk Size to be read at a time
    try {
        uliVolumeChunkSize = RcmConfigurator::getInstance()->getVolumeChunkSize();
    }
    catch(...) {
        uliVolumeChunkSize = DEFAULT_VOLUME_CHUNK_SIZE;
    }
    return uliVolumeChunkSize;
}
/*
 * FUNCTION NAME :  GetThresholdDirtyblockSize
 *
 * DESCRIPTION :    Retrieves the size of data which specifies the threshold to upload data to cx.
 *                  If data size is less than the threshold, this replication thread waits for more
 *                  data to be available before continuing upload. This is to avoid sending large
 *                  number of files of of small size.
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
unsigned long int ProtectedGroupVolumes::GetThresholdDirtyblockSize()
{
    SV_ULONG uliThresholdDirtyBlockSize = 0;
#ifdef SV_WINDOWS
    CRegKey cregkey;

    //Get Volume Chunk Size to be read at a time
    DWORD dwResult;
    std::string globalKey;
    std::string perDeviceKey;
    std::string paramName;

    if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == m_pDeviceStream->GetName()) {
        //Disk parameter
        globalKey = SV_INDSKFLT_PARAMETER;
        
        perDeviceKey = globalKey;
        perDeviceKey += '\\';
        perDeviceKey += std::string(m_SourceNameForDriverInput.begin(), m_SourceNameForDriverInput.end());
        
        paramName = SV_DISK_DATA_NOTIFY_LIMIT;
    }
    else {
        //Volume parameter
        globalKey = SV_INVOLFLT_PARAMETER;
        
        perDeviceKey = globalKey;
        perDeviceKey += '\\';
        perDeviceKey += "Volume";
        perDeviceKey += '{';
        perDeviceKey += std::string(m_SourceNameForDriverInput.begin(), m_SourceNameForDriverInput.end());
        perDeviceKey += '}';

        paramName = SV_VOLUME_DATA_NOTIFY_LIMIT;
    }

    dwResult = cregkey.Open(HKEY_LOCAL_MACHINE,
        perDeviceKey.c_str());
    if (ERROR_SUCCESS != dwResult)
    {
        dwResult = cregkey.Open(HKEY_LOCAL_MACHINE,
            globalKey.c_str());
        if (ERROR_SUCCESS != dwResult)
        {
            std::stringstream sserr;
            sserr << dwResult;
            DebugPrintf(SV_LOG_ERROR, "FAILED cregkey.Open() of %s with error %s...\n", globalKey.c_str(), sserr.str().c_str());
        }
    }

    if (dwResult == ERROR_SUCCESS)
    {
        dwResult = cregkey.QueryDWORDValue(paramName.c_str(),
            uliThresholdDirtyBlockSize);
    }
    
    if( ( ERROR_SUCCESS != dwResult ) || ( 0 == uliThresholdDirtyBlockSize ) )
    {
        uliThresholdDirtyBlockSize = DEFAULT_VOLUME_DATA_NOTIFY_LIMIT * 1024;
    } else
        uliThresholdDirtyBlockSize *= 1024;
#elif SV_UNIX
    get_db_thres_t volumeThreshold;
    memset((void*) &volumeThreshold, 0, sizeof(get_db_thres_t));
    inm_strcpy_s((char*)volumeThreshold.VolumeGUID, NELEMS(volumeThreshold.VolumeGUID), m_SourceNameForDriverInput.c_str());
    if(SV_SUCCESS ==  m_pDeviceStream->IoControl(IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD, &volumeThreshold))
    {
        uliThresholdDirtyBlockSize = volumeThreshold.threshold;
        //DebugPrintf(SV_LOG_DEBUG, "Threshold dirtyblock size for volume %s, is %u .", this->GetSourceVolumeName().c_str(),uliThresholdDirtyBlockSize);
        assert(volumeThreshold.threshold > 0);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Unable to get the threshold dirtyblock size for volume %s, using the default size.", this->GetSourceVolumeName().c_str());
        uliThresholdDirtyBlockSize = DEFAULT_VOLUME_DATA_NOTIFY_LIMIT * 1024;
    }
#endif

    DebugPrintf(SV_LOG_DEBUG, "The dirty block notify threshold is %lu for volume %s\n", uliThresholdDirtyBlockSize, GetSourceVolumeName().c_str());
    return (uliThresholdDirtyBlockSize );
}


/*
 * FUNCTION NAME :  CreateDeviceStream
 *
 * DESCRIPTION :    Creates the device stream used to interact with the inmage filter driver
 *
 * INPUT PARAMETERS :  driver name
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS/SV_FAILURE
 *
 */
int ProtectedGroupVolumes::CreateDeviceStream(const std::string &driverName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    if ( NULL != m_pDeviceStream )
        delete m_pDeviceStream;

    m_pDeviceStream = NULL;
    Device dev(driverName);
    m_pDeviceStream = new DeviceStream(dev);
    iStatus = m_pDeviceStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;

}


bool ProtectedGroupVolumes::CheckVolumeStatus(int &error, std::string &errorMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    VolumeManager& VolMan = VolumeManager::GetInstance();
    cdp_volume_t::Ptr volume = VolMan.CreateVolume(GetRawSourceVolumeName(), VolumeSummary::DISK, &m_pVolumesCache->m_Vs);
    std::stringstream errMsg;
    if (NULL == volume.get())
    {
        errMsg << FUNCTION_NAME << ": CreateVolume() failed for " << GetRawSourceVolumeName();
        errorMsg = errMsg.str();
        error = -1;
        return false;
    }

    BasicIo::BioOpenMode mode = cdp_volume_t::GetSourceVolumeOpenMode(VolumeSummary::DISK);
    volume->Open(mode);
    if (!volume->Good())
    {   
        errMsg << FUNCTION_NAME << ": volume open failed for " << GetRawSourceVolumeName() << ", Error: " << volume->LastError();
        errorMsg = errMsg.str();
        error = volume->LastError();
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}


/*
 * FUNCTION NAME :  CreateVolumeStream
 *
 * DESCRIPTION :    Instantiates the volume stream.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : SV_SUCCESS if the stream was created else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::CreateVolumeStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    bool isVolumeOpened = false;
    const long RETRY_TIME = 60;
    do {
        //Create the volume object
        if (CreateVolume()) {
            DebugPrintf(SV_LOG_DEBUG, "Created source device object.\n");
            // Open the volume
            isVolumeOpened = OpenVolume();
            if (isVolumeOpened) {
                DebugPrintf(SV_LOG_DEBUG, "Opened source device for reading.\n");
                break;
            } else {
                //Retry open after getting latest vic cache
                DebugPrintf(SV_LOG_ERROR, "Failed to open source device %s for draining diffs with error %lu. Retrying after %ld seconds.@LINE %d in FILE %s\n",
                    GetSourceVolumeName().c_str(), m_pProtectedVolume->LastError(), RETRY_TIME, LINE_NO, FILE_NAME);
                WaitOnQuitEvent(RETRY_TIME);
                if (!IsStopRequested())
                    GetLatestVolumesCacheIfPresent();
            }
        } else {
            // Failure creating object: No need to get latest vic cache
            DebugPrintf(SV_LOG_ERROR, "Failed to create device object for source device %s for draining diffs.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
            break;
        }
    } while (!IsStopRequested());

    int iStatus = SV_FAILURE;
    if (IsStopRequested())
        DebugPrintf(SV_LOG_DEBUG, "Quit is requested when trying to create volume stream.\n");
    else {
        if (isVolumeOpened) {
            //Volume opened. Now create volume stream
            iStatus = AllocateVolumeStream();
            if (SV_SUCCESS == iStatus) {
                DebugPrintf(SV_LOG_DEBUG, "Created volume stream.\n");
                m_pVolumeStream->DisableRetry();
                if (m_fpProfileLog)
                    m_pVolumeStream->ProfileRead();
            } else
                DebugPrintf(SV_LOG_ERROR, "Failed to set volume stream object for source device %s for draining diffs.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    return iStatus;
}


/*
* FUNCTION NAME :  AllocateVolumeStream
*
* DESCRIPTION :    Allocates the volume stream object
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :
*
* return value : SV_SUCCESS if the stream was created else SV_FAILURE
*
*/
int ProtectedGroupVolumes::AllocateVolumeStream(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    int iStatus = SV_FAILURE;
    try {
        VolumeStream *p = new VolumeStream(m_pProtectedVolume->GetHandle());
        m_pVolumeStream.reset(p);
        iStatus = SV_SUCCESS;
    } catch (std::bad_alloc &e) {
        DebugPrintf(SV_LOG_ERROR, "FAILED: Memory allocation for volume stream object failed for source device %s with error %s.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), e.what(), LINE_NO, FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    return iStatus;
}


/*
* FUNCTION NAME :  GetLatestVolumesCacheIfPresent
*
* DESCRIPTION :  Gets the latest vic cache
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :
*
* return value : None
*
*/
void ProtectedGroupVolumes::GetLatestVolumesCacheIfPresent(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    DataCacher dataCacher;
    if (dataCacher.Init()) {
        time_t t = dataCacher.GetVolumesCacheMtime();
        if (0 == t) {
            DebugPrintf(SV_LOG_ERROR, "FAILED: s2 failed to find volumes cache modified time. Source device %s.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
        } else if (t != m_pVolumesCache->m_MTime) {
            DebugPrintf(SV_LOG_DEBUG, "Volumes cache updated. Getting latest one.\n");
            DataCacher::CACHE_STATE cs = dataCacher.GetVolumesFromCache(m_VolumesCache);
            if (DataCacher::E_CACHE_VALID == cs) {
                DebugPrintf(SV_LOG_DEBUG, "Got latest cache.\n");
                m_pVolumesCache = &m_VolumesCache;
            } else if (DataCacher::E_CACHE_DOES_NOT_EXIST == cs) {
                DebugPrintf(SV_LOG_ERROR, "FAILED: s2 found volumes cache not existing. Source device %s.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
            } else {
                DebugPrintf(SV_LOG_ERROR, "FAILED: s2 failed to get volumes cache with error %s. Source device %s.@LINE %d in FILE %s\n",
                    dataCacher.GetErrMsg().c_str(), GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
            }
        } else
            DebugPrintf(SV_LOG_DEBUG, "Volumes cache is already latest.\n");
    } else
        DebugPrintf(SV_LOG_ERROR, "FAILED: DataCacher initialization failed for source device %s.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
}


/*
 * FUNCTION NAME :  Init
 *
 * DESCRIPTION :    Initializes ProtectedGroupVolume. Create the Volume instance,initialize the idle wait time,
 *                  initialize the volume chunk size,initialize the formatted volume size,initialize the
 *                  transport,volume,device streams and for windows initialize the db notify event
 *
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if initialization completes successfully.
 *
 */
int ProtectedGroupVolumes::Init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_FAILURE;
    std::string errMsg;

    if ( !IsInitialized() )
    {
        m_sProtectedDiskId = GetSourceVolumeName();

        GetLatestVolumesCacheIfPresent();

        VolumeReporterImp vrimp;
        m_VolumeReport.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
        vrimp.GetVolumeReport(m_sProtectedDiskId, m_pVolumesCache->m_Vs, m_VolumeReport);

        if (!m_VolumeReport.m_bIsReported)
        {
            errMsg += "Failed to find disk " + m_sProtectedDiskId + " in volume cache file";
            DebugPrintf(SV_LOG_ERROR, "%s %s\n", FUNCTION_NAME, errMsg.c_str());
            return iStatus;
        }

        vrimp.PrintVolumeReport(m_VolumeReport);

#ifdef SV_UNIX
        m_sProtectedVolumeName = m_VolumeReport.m_DeviceName;
        SetRawSourceVolumeName();
#endif

        m_sProtectedDiskDeviceName = m_VolumeReport.m_DeviceName;

        DebugPrintf(SV_LOG_DEBUG,
            "Source volume name %s Source Disk Id %s Device Name %s\n",
            GetSourceVolumeName().c_str(),
            GetSourceDiskId().c_str(),
            GetSourceDeviceName().c_str());

        FilterDrvIfMajor fdif;
        m_SourceNameForDriverInput = fdif.GetFormattedDeviceNameForDriverInput(
                                                m_sProtectedVolumeName,
                                                VolumeSummary::DISK);

        DebugPrintf(SV_LOG_ALWAYS, "Source name for driver input is %S\n",
            std::wstring(m_SourceNameForDriverInput.begin(), m_SourceNameForDriverInput.end()).c_str());
        
        /* because CreateVolumeStream uses profile option for read profiling */
        SetUpProfilerLog();
        SetUpDiffsRateProfilerLog();
        m_bProfileDiffsSize = m_fpProfileLog || m_fpDiffsRateProfilerLog;

        iStatus = CreateVolumeStream();
        if (SV_SUCCESS == iStatus)
        {
            m_bS2ChecksEnabled = RcmConfigurator::getInstance()->getS2StrictMode() ;
            m_bDICheckEnabled = RcmConfigurator::getInstance()->getDICheck() ;
            m_bSVDCheckEnabled = RcmConfigurator::getInstance()->getSVDCheck();
            m_bHasToCheckTS = RcmConfigurator::getInstance()->getTSCheck() ;
            SetUpLogForAllRenamedFiles();
            setupChecksumStore( GetSourceVolumeName() ) ;

            m_IdleWaitTime = GetIdleWaitTime();
            m_VolumeChunkSize   = GetVolumeChunkSize();
            m_VolumeSize = m_VolumeReport.m_Size;


           if ( IsCompressionEnabled() && (0 == m_pCompress ) )
           {
                m_pCompress = CompressionFactory::CreateCompressor(ZLIB);
                m_pCompress->m_bCompressionChecksEnabled = (m_bS2ChecksEnabled || m_bDICheckEnabled);
                m_pCompress->m_bProfile = m_fpProfileLog;
            }

            if ( (iStatus = InitTransport()) == SV_SUCCESS)
            {
                iStatus = CreateDeviceStream(fdif.GetDriverName(VolumeSummary::DISK));
                if ( iStatus == SV_SUCCESS )
                {
#ifdef SV_WINDOWS
                    VOLUME_DB_EVENT_INFO    EventInfo;
                    memset(&EventInfo, 0, sizeof EventInfo);
                    inm_wmemcpy_s(EventInfo.VolumeGUID, NELEMS(EventInfo.VolumeGUID), m_SourceNameForDriverInput.c_str(), m_SourceNameForDriverInput.length());
                    EventInfo.hEvent = (HANDLE)m_DBNotifyEvent.Self();
                    if ( (iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT,&EventInfo,sizeof(VOLUME_DB_EVENT_INFO),NULL,0) ) != SV_SUCCESS )
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to initialize volume with SET DIRTY BLOCK NOTIFY EVENT.\n");
                    }
#elif SV_UNIX
                    //for linux we need not do anything here
#endif
                }
            }

            if (iStatus == SV_SUCCESS)
            {
                if (!IsTransportProtocolMemory())
                {
                    long transportflushlimit = RcmConfigurator::getInstance()->getTransportFlushThresholdForDiff();
                    
                    if (m_transportProtocol == TRANSPORT_PROTOCOL_BLOB)
                    {
                        transportflushlimit = boost::iequals(m_datapathTransportSettings.m_AzureBlobContainerSettings.LogStorageBlobType, AZURE_BLOB_TYPE_BLOK) ? 
                            RcmConfigurator::getInstance()->getAzureBlockBlobMaxWriteSize() :
                            RcmConfigurator::getInstance()->getAzureBlobFlushThresholdForDiff();
                    }

                    if (m_pTransportStream->NeedAlignedBuffers())
                    {
                        long alignLen = m_pTransportStream->AlignedBufferSize();
                        transportflushlimit = (transportflushlimit / alignLen) * alignLen;
                    }

                    m_dataPtr.reset(new (std::nothrow) char[transportflushlimit]);
                    if (m_dataPtr.get() != NULL)
                    {
                        memset(m_dataPtr.get(), 0, transportflushlimit);
                        m_dataBufferLength = transportflushlimit;
                        m_bInitialized = true;
                    }
                }
                else
                {
                    m_bInitialized = true;
                }
            }
        } /* End of if (SV_SUCCESS == iStatus) */
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  IsProtecting
 *
 * DESCRIPTION :    Inspects protection status
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true is volume is protected else false
 *
 */
const bool ProtectedGroupVolumes::IsProtecting() const
{
    return m_bProtecting;
}
/*
 * FUNCTION NAME :      GetProtectionDirection
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the protection direction SOURCE/TARGET. refer CDP_DIRECTION enum
 *
 */
CDP_DIRECTION ProtectedGroupVolumes::GetProtectionDirection() const
{
    return m_Direction;
}

/*
 * FUNCTION NAME :  Protect
 *
 * DESCRIPTION :    Commences protection of volume. Checks if initialization is complete in a loop.
 *                  Issues a start filtering ioctl,such that differentials will be drained in profiling mode too.
 *                  Starts replication when cdp direction is SOURCE.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCESS/SV_FAILURE
 *
 */
int ProtectedGroupVolumes::Protect()
{
    int iStatus = SV_FAILURE;
    if (m_bNeedToRenameDiffs = RcmConfigurator::getInstance()->getShouldS2RenameDiffs())
    {
        DebugPrintf(SV_LOG_DEBUG, "s2 has to rename differential files\n");
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: unsupported scenario to rename diffs.\n", FUNCTION_NAME);
        return iStatus;
    }

    while( !IsStopRequested() && (!IsInitialized()))
    {
        if(SV_SUCCESS != Init())
        {
            DebugPrintf(SV_LOG_WARNING, "The volume: %s is not initialized. The protected volume could be offline, waiting for 30 seconds.", m_sProtectedVolumeName.c_str());
        }
        if ( !IsInitialized() )
        {
            WaitOnQuitEvent(30);
        }
        else
        {
            break;
        }
    }

    if (IsInitialized() && IsResyncRequired())
    {
        iStatus = StopFilteringOnResync();

        if (SV_SUCCESS == iStatus)
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s stopped filtering for volume %s on resync\n",
                FUNCTION_NAME,
                GetSourceVolumeName().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,
                "Failed to stop filtering for volume %s, status %d\n",
                GetSourceVolumeName().c_str(),
                iStatus);
            return iStatus;
        }
    }

    if(IsInitialized() && !IsStopRequested())
    {
        iStatus = StartFiltering();
    }

    if (SV_SUCCESS == iStatus)
    {
        //Instantiate HealthCollator for this thread
        InitHealthCollator();
        //Get this after the start filtering as the function issues an ioctl which will work only if the volume is being filtered.
        m_ThresholdDirtyblockSize = GetThresholdDirtyblockSize();

        DebugPrintf(SV_LOG_ALWAYS, "INFO: Starting Protection on volume %s.\n", GetSourceVolumeName().c_str());
        if ( !IsStopRequested() && (true == IsInitialized()) && (false == IsProtecting() ) && (SOURCE == GetProtectionDirection()) )
        {
            m_bProtecting = true;
            //
            // Call Replicate to send the header and one drtd tag with N records
            //
            return this->Replicate();
        }
    }

    return iStatus;
}

/*
 * FUNCTION NAME :      CloseTransportStream
 *
 * DESCRIPTION :    Delete the transport stream instance
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
// S2 Version
/*
int ProtectedGroupVolumes::CloseTransportStream()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    if ( NULL != m_pTransportStream )
    {
        //m_pTransportStream->Close();
        delete m_pTransportStream;
        m_pTransportStream =  NULL;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return SV_SUCCESS;

}*/	
/*
 * FUNCTION NAME :  FinishSendingData
 *
 * DESCRIPTION :    Sets the name of the diff file, and renames the uploaded file from pre_completed* to completed_*
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if rename succeeds else SV_FAILURE
 *
 */
// S2 Version
int ProtectedGroupVolumes::FinishSendingData(std::string const & preName, std::string const & finalName, VolumeChangeData & changeData, 
                                             unsigned m_continuationId, bool isContinuationEnd)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;

    boost::shared_ptr<Checksums_t> checksums;
    if( m_bDICheckEnabled )
        checksums.reset( new Checksums_t() ) ;
    else
        checksums.reset() ;

    /* S2CHKENABLED : Checks that need to be done in FinishSendingData if m_bS2ChecksEnabled is true : START */
    iStatus = ChecksInFinishSendingData(checksums, changeData);
    /* S2CHKENABLED : END */

    if (SV_SUCCESS == iStatus)
    {
        /* If RenameRemoteFile succeeds, record the sent name in previousSentName*/
        iStatus = RenameRemoteFile(preName, finalName);
        if (  iStatus == SV_FAILURE)
        {
            DebugPrintf(SV_LOG_DEBUG,"FAILED: Could not rename remote file. Second attempt failed.@LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
        }
        else
        {
           PersistFileNamesAndCheckSums(finalName, checksums);
        }
    } /* end of if (iStatus = SV_SUCCESS) */
    else
    {
        WaitOnQuitEvent( SECS_TO_WAITFOR_VOL );
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  CreateTransportStream
 *
 * DESCRIPTION :    Creates the transport stream based on the transport protocol.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS if the transport stream is created successfully else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::CreateTransportStream()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    
    int iStatus = SV_SUCCESS;
    try
    {
        if (m_fpDiffsRateProfilerLog)
        {
            m_pTransportStream.reset(new DummyTransportStream());
        }
        else if(m_transportProtocol == TRANSPORT_PROTOOL_MEMORY)
        {
            CreateDiffSyncObject();
            m_pTransportStream.reset(new MemoryStream(TRANSPORT_PROTOOL_MEMORY,m_TransportSettings,m_TgtVolume, m_SrcCapacity,m_CDPSettings,m_DiffsyncObj.get()));
        }
        else if (m_transportProtocol == TRANSPORT_PROTOCOL_BLOB)
        {
            const uint32_t timeout = RcmConfigurator::getInstance()->getAzureBlobOperationsTimeout();
            const bool bEnableDiffFileChecksums = RcmConfigurator::getInstance()->getEnableDiffFileChecksums();
            
            if(boost::iequals(m_datapathTransportSettings.m_AzureBlobContainerSettings.LogStorageBlobType, AZURE_BLOB_TYPE_BLOK))
            {
                m_pTransportStream.reset(new AzureBlockBlobStream(m_datapathTransportSettings.m_AzureBlobContainerSettings.sasUri,
                    timeout,
                    RcmConfigurator::getInstance()->getAzureBlockBlobParallelUploadChunkSize(),
                    RcmConfigurator::getInstance()->getAzureBlockBlobMaxWriteSize(),
                    RcmConfigurator::getInstance()->getAzureBlockBlobMaxParallelUploadThreads(),
                    bEnableDiffFileChecksums));
            }
            else
            {
                m_pTransportStream.reset(new AzurePageBlobStream(m_datapathTransportSettings.m_AzureBlobContainerSettings.sasUri, timeout, bEnableDiffFileChecksums));
            }

            std::string proxySettingsPath = RcmConfigurator::getInstance()->getProxySettingsPath();
            if (!boost::filesystem::exists(proxySettingsPath))
            {
                DebugPrintf(SV_LOG_INFO, "Proxy settings not found for transport stream.\n");
            }
            else
            {
                ProxySettings proxy(proxySettingsPath);
                if (proxy.m_Address.length() && proxy.m_Port.length())
                {
                    m_pTransportStream->SetHttpProxy(proxy.m_Address, proxy.m_Port, proxy.m_Bypasslist);
                    DebugPrintf(SV_LOG_INFO, "Using proxy settings %s %s for transport stream.\n",
                        proxy.m_Address.c_str(),
                        proxy.m_Port.c_str());
                }
                else
                    DebugPrintf(SV_LOG_INFO, "Proxy settings not set for transport stream.\n");
            }

        }
        else if (m_transportProtocol == TRANSPORT_PROTOCOL_HTTP)
        {
            boost::unique_lock<boost::mutex> lock(m_mutexTransportSettingsChange);
            m_pTransportStream.reset(new TransportStream(m_transportProtocol, m_TransportSettings));
        }
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "%s: unsupported transport protocol.\n", FUNCTION_NAME);
        }
        
        if (m_pTransportStream)
        {
            m_pTransportStream->SetDiskId(GetSourceDiskId());
            m_pTransportStream->SetFileType(CxpsTelemetry::FileType_DiffSync);
        }

        if (m_pTransportStream)
        {
            m_pTransportStream->SetAppendSystemTimeUtcOnRename(true);
        }
    }
    catch(DataProtectionException& dpe) {
        iStatus = SV_FAILURE ;
        DebugPrintf(dpe.LogLevel(), dpe.what());
    }
    catch( std::string const& e )
    {
        iStatus = SV_FAILURE ;
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception %s\n", e.c_str() );
    }
    catch( char const* e ) 
    {
        iStatus = SV_FAILURE ;
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception %s\n", e );
    }
    catch( exception const& e ) 
    {
        iStatus = SV_FAILURE ;
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception %s\n", e.what() );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    return iStatus;
}

/*
 * FUNCTION NAME :  OpenTransportStream
 *
 * DESCRIPTION :    Opens the transport stream for read/write operations. If secure mode is
 *                  specified then additionally the stream is opened in secure mode. ie. data
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS if stream is opened successfully
 *
 */int ProtectedGroupVolumes::OpenTransportStream()
 {
     DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

     int iStatus = SV_SUCCESS;

     if (NULL == m_pTransportStream) // && ( ( 0 == (iStatus = CreateTransportStream() ) ) ) )
     {
         iStatus = SV_FAILURE;
         DebugPrintf(SV_LOG_ERROR,"FAILED: Transport stream is NULL. Create a Transport Stream first. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
     }
     else
     {
         //BUGBUG: Instead of specifying VolumeStream:: use GenericStream::
         STREAM_MODE mode = VolumeStream::Mode_Open | VolumeStream::Mode_RW;

         /// make it always secure
         mode |= VolumeStream::Mode_Secure;

         if ( SV_FAILURE == m_pTransportStream->Open(mode) )
         {
             iStatus = SV_FAILURE;
             DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to open transport stream. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
         }
     }
     DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

     return iStatus;
 }

/*
 * FUNCTION NAME :  TransferDataToCX
 *
 * DESCRIPTION :    Uploads data to the cx. The data is accumulated in a buffer till it exceeds
 *                  transport flush limit threshold after which data is then uploaded to the cx and the
 *                  counters reset.
 *
 *
 * INPUT PARAMETERS :   sData is the binary data to be uploaded
 *                      uliDataLen is the length of the data to tbe uploaded
 *                      sDestination specifies the absolute directory path inclusive of filename for upload
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS on upload of data else SV_FAILURE
 *
 */
//BUGBUG: Review logic thoroughly
int ProtectedGroupVolumes::TransferDataToCX(const char* sData,const long int uliDataLen,const string& sDestination, bool bMoreData, bool bCreateMissingDirs, bool bIsFull)
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    if ( (NULL != sData ) && ( uliDataLen > 0 ) )
    {
        bool bCanProceed =  WriteToChkFileInTransferDataToCX(sData, uliDataLen);
        if (bCanProceed == false)
        {
            iStatus = SV_FAILURE;
            m_pTransportStream->SetNeedToDeletePreviousFile(true);
            WaitOnQuitEvent ( SECS_TO_WAITFOR_VOL );
        }
        if (SV_SUCCESS == iStatus)
        {
            Transport_t p = m_Transport[IsTransportProtocolMemory()];
            iStatus = (this->*p)(sData, uliDataLen, sDestination, bMoreData, bCreateMissingDirs, bIsFull);
        } /* endif of iStatus == SV_SUCCESS */
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING,"Either length of data is zero or buffer is null.\n");
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}


int ProtectedGroupVolumes::TransferThroughAccmulation(const char* sData,const long int uliDataLen,const string& sDestination, bool bMoreData, 
                                                      bool bCreateMissingDirs, bool bIsFull)
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;
    bool bNeedAlignedBuffers = m_pTransportStream->NeedAlignedBuffers();

    if (m_liTotalLength == 0 && !bMoreData)
    {
        /// this is the only buffer in the diff file  -or-
        /// last buffer in diff file while all prev buffers were already sent
        /// send the entire buffer
        return FlushDataToCx(sData, uliDataLen, sDestination, bMoreData, bCreateMissingDirs, bIsFull);
    }
    else if (m_liTotalLength != 0 && !bMoreData)
    {
        /// this is the last buffer in a series of buffers in the diff file
        /// fill the residual buffer and send the buffer -and-
        /// then send rest of the buffer
        if ( (uliDataLen + m_liTotalLength) <= m_dataBufferLength)
        {
            inm_memcpy_s(m_dataPtr.get() + m_liTotalLength,
                m_dataBufferLength - m_liTotalLength,
                sData,
                uliDataLen);
            
            m_liTotalLength += uliDataLen;

            iStatus = FlushDataToCx(m_dataPtr.get(),
                m_liTotalLength,
                sDestination,
                bMoreData,
                bCreateMissingDirs,
                bIsFull);
            
            m_liTotalLength = 0;
            
            return iStatus;
        }
        else if ( (uliDataLen + m_liTotalLength) > m_dataBufferLength)
        {
            long residualBufLen = m_dataBufferLength - m_liTotalLength;
            inm_memcpy_s(m_dataPtr.get() + m_liTotalLength, residualBufLen, sData, residualBufLen);

            iStatus = FlushDataToCx(m_dataPtr.get(),
                m_dataBufferLength,
                sDestination,
                true,
                bCreateMissingDirs,
                bIsFull);

            m_liTotalLength = 0;

            if (iStatus != SV_SUCCESS)
                return iStatus;

            iStatus = FlushDataToCx(sData + residualBufLen,
                uliDataLen - residualBufLen,
                sDestination,
                bMoreData,
                bCreateMissingDirs,
                bIsFull);

            return iStatus;
        }
    }
    else if ((m_liTotalLength == 0) && bMoreData)
    {
        /// this is a first buffer in a series of buffers
        /// buffer it if small -or- 
        /// send it if can't buffer
        /// check for buffer alignment and buffer unaligned bytes
        /// unaligned bytes will be merged with next incoming buffer
        if (uliDataLen < m_dataBufferLength)
        {
            inm_memcpy_s(m_dataPtr.get(), m_dataBufferLength, sData, uliDataLen);
            m_liTotalLength = uliDataLen;
            return iStatus;
        }
        else if (uliDataLen >= m_dataBufferLength)
        {
            if (bNeedAlignedBuffers)
            {
                long alignLen = m_pTransportStream->AlignedBufferSize();

                long unalignedLen = uliDataLen % alignLen;

                iStatus = FlushDataToCx(sData, uliDataLen - unalignedLen, sDestination, bMoreData, bCreateMissingDirs, bIsFull);
                if (iStatus != SV_SUCCESS)
                    return iStatus;

                inm_memcpy_s(m_dataPtr.get(), m_dataBufferLength, sData + uliDataLen - unalignedLen, unalignedLen);

                m_liTotalLength = unalignedLen;

                return iStatus;
            }
            else
            {
                return FlushDataToCx(sData, uliDataLen, sDestination, bMoreData, bCreateMissingDirs, bIsFull);
            }
        }
    }
    else if ((m_liTotalLength != 0) && bMoreData)
    {
        /// this is a middle buffer in a series of buffers
        /// merge with existing buffer it if small -or-
        /// merge and send if it more than what we can buffer
        /// check if alignment required and buffer any unaligned bytes
        /// unaligned bytes will be merged with next incoming buffer
        if (uliDataLen + m_liTotalLength < m_dataBufferLength)
        {
            inm_memcpy_s(m_dataPtr.get() + m_liTotalLength,
                m_dataBufferLength - m_liTotalLength,
                sData,
                uliDataLen);
            m_liTotalLength += uliDataLen;

            return iStatus;
        }
        else if (uliDataLen + m_liTotalLength >= m_dataBufferLength)
        {
            long residualBufLen = m_dataBufferLength - m_liTotalLength;
            inm_memcpy_s(m_dataPtr.get() + m_liTotalLength, residualBufLen, sData, residualBufLen);

            iStatus = FlushDataToCx(m_dataPtr.get(),
                m_dataBufferLength,
                sDestination,
                bMoreData,
                bCreateMissingDirs,
                bIsFull);

            m_liTotalLength = 0;

            if (iStatus != SV_SUCCESS)
                return iStatus;

            long remainingDataLen = uliDataLen - residualBufLen;

            if (remainingDataLen < m_dataBufferLength)
            {
                inm_memcpy_s(m_dataPtr.get(), m_dataBufferLength, sData + residualBufLen, remainingDataLen);
                m_liTotalLength = remainingDataLen;
                return iStatus;
            }
            else if (remainingDataLen >= m_dataBufferLength)
            {
                if (bNeedAlignedBuffers)
                {
                    long alignLen = m_pTransportStream->AlignedBufferSize();

                    long unalignedLen = remainingDataLen % alignLen;

                    iStatus = FlushDataToCx(sData + residualBufLen,
                        remainingDataLen - unalignedLen,
                        sDestination,
                        bMoreData,
                        bCreateMissingDirs,
                        bIsFull);

                    if (iStatus != SV_SUCCESS)
                        return iStatus;

                    inm_memcpy_s(m_dataPtr.get(),
                        m_dataBufferLength,
                        sData + uliDataLen - unalignedLen,
                        unalignedLen);

                    m_liTotalLength = unalignedLen;
                    
                    return iStatus;
                }
                else
                {
                    return FlushDataToCx(sData + residualBufLen,
                        remainingDataLen,
                        sDestination,
                        bMoreData,
                        bCreateMissingDirs,
                        bIsFull);
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}


int ProtectedGroupVolumes::TransferDirectly(const char* sData,const long int uliDataLen,const string& sDestination, bool bMoreData, 
                                            bool bCreateMissingDirs, bool bIsFull)
{
    return FlushDataToCx(sData, uliDataLen, sDestination, bMoreData, bCreateMissingDirs, bIsFull);
}


/*
 * FUNCTION NAME :  FlushDataToCx
 *
 * DESCRIPTION :    Performs transport upload operation
 *
 * INPUT PARAMETERS :   sData is the binary data to be uploaded
 *                      uliDataLen is the length of the data to tbe uploaded
 *                      sDestination specifies the absolute directory path inclusive of filename for upload
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS on upload of data else SV_FAILURE
 *
 */int ProtectedGroupVolumes::FlushDataToCx(const char* sDataToSend,const long int liLength,const string& sDestination, bool bMoreData,bool bCreateMissingDirs, bool bIsFull)
 {
     DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
     DebugPrintf(SV_LOG_DEBUG, "%s: length is %lu and drainedbytes are %llu\n", FUNCTION_NAME, liLength, m_drainLogTransferedBytes);
     int iStatus = SV_SUCCESS;

    if (m_pTransportStream->Good())
    {
        ACE_Time_Value TimeBeforeTransport, TimeAfterTransport;
        m_fpProfileLog && (TimeBeforeTransport = ACE_OS::gettimeofday());
        m_DiffFileUploadProfiler->start();
        iStatus = m_pTransportStream->Write(sDataToSend, liLength, sDestination, bMoreData, m_compressMode, bIsFull);
        if (SV_SUCCESS == iStatus)
        {
            m_DiffFileUploadProfiler->stop(liLength);
            m_drainLogTransferedBytes += liLength;
        }
        m_fpProfileLog && (TimeAfterTransport = ACE_OS::gettimeofday()) && (m_TimeForTransport += (TimeAfterTransport - TimeBeforeTransport));
        if (iStatus == SV_FAILURE)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Volume: %s. Failed to send data.@LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
            m_networkFailureCode = m_pTransportStream->GetLastError();
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED: Volume: %s. Failed to send data.Corrupted Transport stream. @LINE %d in FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
        iStatus = SV_FAILURE;
    }

    if (SV_SUCCESS == iStatus && !bMoreData)
    {
        if (m_bThrottled)
            DebugPrintf(SV_LOG_ALWAYS, "%s: throttle reset on network transfer success.\n", FUNCTION_NAME);

        m_bThrottled = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
 }

/*
 * FUNCTION NAME :  RenameRemoteFile
 *
 * DESCRIPTION :    Renames the remote diff file from pre_completed* to completed*
 *
 * INPUT PARAMETERS :   preName is the name of the file while transport upload operation is in progress
 *                          it is pre_completed* and specified as an absolute directory path.
 *                      finalName is the name of the file after upload of file is completed
 *                          it is completed_*
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS if rename is successfull else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::RenameRemoteFile(std::string const & preName, std::string const & finalName)
 {
     DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
     DebugPrintf(SV_LOG_DEBUG, "Name of the differential file sent: %s\n", finalName.c_str());

     /* Initialize iStatus with SV_FAILURE */
     int iStatus = SV_FAILURE;

     if ( (NULL != m_pTransportStream) && m_pTransportStream->Good())
     {
         iStatus = m_pTransportStream->Rename(preName,finalName,m_compressMode, m_FinalPaths);
     }
     if ( iStatus == SV_FAILURE)
     {
         DebugPrintf(SV_LOG_DEBUG, "FAILED Couldn't rename file (%s => %s) in final paths (%c separated) %s\n", preName.c_str(), finalName.c_str(), 
                                   FINALPATHS_SEP, m_FinalPaths.c_str());
     }
     else
     {
         DebugPrintf( SV_LOG_DEBUG, "RenameFile: %s => %s in final paths (%c separated) %s\n", preName.c_str(), finalName.c_str(), 
                                    FINALPATHS_SEP, m_FinalPaths.c_str());
     }

     DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

     return( iStatus );
 }


/*
 * FUNCTION NAME :  SendSizeOfChangesInfo
 *
 * DESCRIPTION :    Uploads the size of changes to the destination file
 *
 * INPUT PARAMETERS :  SV_LONGLONG& ChangeSize is the total size of the change in bytes
 *                      string& sDestination is the upload file name
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES : The size is the sum of the size of data and the respective headers as computed by ComputeChangesToSend function
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::SendSizeOfChangesInfo(const SV_LONGLONG& ChangeSize,const string& sDestination)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    const int iBufferSize = sizeof(SVD_PREFIX) + sizeof(SV_ULARGE_INTEGER);
    char pszBuffer[sizeof(SVD_PREFIX) + sizeof(SV_ULARGE_INTEGER)] = { 0 };
    SVD_PREFIX* pPrefixChangeSize = (SVD_PREFIX*)pszBuffer;

    pPrefixChangeSize->count  = 1;
    pPrefixChangeSize->Flags  = 0;
    pPrefixChangeSize->tag    = SVD_TAG_LENGTH_OF_DRTD_CHANGES;

    SV_ULARGE_INTEGER sv_ulargeint;
    sv_ulargeint.QuadPart = ChangeSize;

    inm_memcpy_s(pszBuffer+sizeof(SVD_PREFIX), (sizeof(pszBuffer) - sizeof(SVD_PREFIX)), &sv_ulargeint, sizeof (SV_ULARGE_INTEGER));

    if(IsCompressionEnabled())
    {
        iStatus = m_pCompress->CompressStream((unsigned char*) pszBuffer, iBufferSize);
        if(0 == iStatus)
        {
            iStatus = SV_SUCCESS;
        }
        else
        {
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        iStatus = TransferDataToCX(  (char*)pszBuffer,
                                     iBufferSize,
                                     sDestination,
                                     true);
        m_bProfileDiffsSize && (m_DiffSize+=iBufferSize);
    }

    if (iStatus == SV_FAILURE)
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED %s.@ LINE %d in FILE %s \n",
                     FUNCTION_NAME, LINE_NO, FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}

int ProtectedGroupVolumes::SendTimeStampInfoV2( const TIME_STAMP_TAG_V2& TimeStampOfChange,
                                                CHANGE_TIME_STAMP_POS pos,
                                                const string& sDestination )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str() ) ;

    int iStatus = SV_SUCCESS ;
    const int iBufferSize = sizeof( SVD_PREFIX ) + sizeof( SVD_TIME_STAMP_V2 ) ;
    char pszBuffer[ iBufferSize ] = { 0 } ;
    SVD_PREFIX* pPrefixTimeStampOfChange = ( SVD_PREFIX*) pszBuffer ;
    pPrefixTimeStampOfChange->count = 1 ;
    pPrefixTimeStampOfChange->Flags = 0 ;

    bool bMoreData ;
    switch( pos )
    {
        case FIRST_CHANGE:
            pPrefixTimeStampOfChange->tag = SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2 ;
            bMoreData = true ;
            break ;
        case LAST_CHANGE :
            pPrefixTimeStampOfChange->tag = SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2 ;
            bMoreData = false ;
            break ;
    }

    SVD_TIME_STAMP_V2 *pTimeStamp = (SVD_TIME_STAMP_V2*) ( pszBuffer + sizeof( SVD_PREFIX ) ) ;
    pTimeStamp->Header.ucFlags = TimeStampOfChange.Header.ucFlags;
    pTimeStamp->Header.ucLength= TimeStampOfChange.Header.ucLength;
    pTimeStamp->Header.usStreamRecType = TimeStampOfChange.Header.usStreamRecType;

    pTimeStamp->TimeInHundNanoSecondsFromJan1601 = TimeStampOfChange.TimeInHundNanoSecondsFromJan1601;
    pTimeStamp->SequenceNumber = TimeStampOfChange.ullSequenceNumber;

    if(IsCompressionEnabled())
    {
        iStatus = m_pCompress->CompressStream((unsigned char*) pszBuffer, iBufferSize);
        if(0 == iStatus)
        {
            iStatus = SV_SUCCESS;
            if(false == bMoreData)
            {
                unsigned long ulSize = 0;
                unsigned long *pul = m_bProfileDiffsSize ? &m_DiffSize : 0;
                const unsigned char * const pBuffer = m_pCompress->CompressStreamFinish(&ulSize, pul);
                if (NULL != pBuffer)
                {
                    m_bProfileDiffsSize && (m_CompressedDiffSize=ulSize);
                    DebugPrintf(SV_LOG_DEBUG, "In function %s, @LINE %d in FILE %s, pBuffer = %p, ulSize = %lu. After compress stream finish.\n", FUNCTION_NAME, LINE_NO, FILE_NAME, pBuffer, ulSize);
                    iStatus = TransferDataToCX((char*)pBuffer, (const long)ulSize, sDestination, bMoreData);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "CompressStreamFinish returned NULL @ LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                    iStatus = SV_FAILURE;
                }
            }
        }
        else
        {
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        iStatus = TransferDataToCX(  (char*)pszBuffer,
                                     iBufferSize,
                                     sDestination,
                                     bMoreData);
        m_bProfileDiffsSize && (m_DiffSize+=iBufferSize);
    }

    if (iStatus == SV_FAILURE)
    {
        DebugPrintf(m_bThrottled? SV_LOG_DEBUG : SV_LOG_ERROR,
                     "FAILED %s... %s.@ LINE %d in FILE %s\n",
                     FUNCTION_NAME,
                     sDestination.c_str(),LINE_NO, FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}

/*
 * FUNCTION NAME :  SendTimeStampInfo
 *
 * DESCRIPTION :    Uploads the times stamp of first change and last changes
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::SendTimeStampInfo(const TIME_STAMP_TAG& TimeStampOfChange,CHANGE_TIME_STAMP_POS pos,const string& sDestination)//SendTimeStampInfo(const PDIRTY_BLOCKS pDirtyBlock) //
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    const int iBufferSize = sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP);
    char pszBuffer[sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP)] = { 0 };
    SVD_PREFIX* pPrefixTimeStampOfChange = (SVD_PREFIX*)pszBuffer;

    pPrefixTimeStampOfChange->count  = 1;
    pPrefixTimeStampOfChange->Flags  = 0;
    switch(pos)
    {
        case FIRST_CHANGE:
            pPrefixTimeStampOfChange->tag    = SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE;
            break;
        case LAST_CHANGE:
            pPrefixTimeStampOfChange->tag    = SVD_TAG_TIME_STAMP_OF_LAST_CHANGE;
            break;
    }

    SVD_TIME_STAMP* pTimeStamp = (SVD_TIME_STAMP*)(pszBuffer + sizeof(SVD_PREFIX));

    pTimeStamp->Header.ucFlags = TimeStampOfChange.Header.ucFlags;
    pTimeStamp->Header.ucLength= TimeStampOfChange.Header.ucLength;
    pTimeStamp->Header.usStreamRecType = TimeStampOfChange.Header.usStreamRecType;

    pTimeStamp->TimeInHundNanoSecondsFromJan1601 = TimeStampOfChange.TimeInHundNanoSecondsFromJan1601;
    pTimeStamp->ulSequenceNumber = TimeStampOfChange.ulSequenceNumber;

    bool bMoreData;
    switch(pos)
    {
        case FIRST_CHANGE:
            bMoreData = true;
            break;
        case LAST_CHANGE:
            bMoreData = false;
            break;
    }

    if(IsCompressionEnabled())
    {
        iStatus = m_pCompress->CompressStream((unsigned char*) pszBuffer, iBufferSize);
        if(0 == iStatus)
        {
            iStatus = SV_SUCCESS;
            if(false == bMoreData)
            {
                unsigned long ulSize = 0;
                unsigned long *pul = m_bProfileDiffsSize ? &m_DiffSize : 0;
                const unsigned char * const pBuffer = m_pCompress->CompressStreamFinish(&ulSize, pul);
                if (NULL != pBuffer)
                {
                    m_bProfileDiffsSize && (m_CompressedDiffSize=ulSize);
                    DebugPrintf(SV_LOG_DEBUG, "In function %s, @LINE %d in FILE %s, pBuffer = %p, ulSize = %lu. After compress stream finish.\n", FUNCTION_NAME, LINE_NO, FILE_NAME, pBuffer, ulSize);
                    iStatus = TransferDataToCX((char*)pBuffer, (const long)ulSize, sDestination, bMoreData);

                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "CompressStreamFinish returned NULL @ LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                    iStatus = SV_FAILURE;
                }
            }
        }
        else
        {
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        iStatus = TransferDataToCX(  (char*)pszBuffer,
                                     iBufferSize,
                                     sDestination,
                                     bMoreData);
        m_bProfileDiffsSize && (m_DiffSize+=iBufferSize);
    }

    if (iStatus == SV_FAILURE)
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED %s... %s.@ LINE %d in FILE %s\n",
                     FUNCTION_NAME, sDestination.c_str(),LINE_NO, FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/** Added by BSR for source IO time stamps **/
int ProtectedGroupVolumes::SendDirtyBlockInfoV2( const SV_LONGLONG& Offset,
                                                 const SV_ULONG& Length,
                                                 const SV_ULONG& timeDelta,
                                                 const SV_ULONG& sequenceDelta,
                                                 string& sDestination )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str() ) ;
    int iStatus = SV_SUCCESS ;

    SV_UINT sizeHeaderBuffer = sizeof( SVD_DIRTY_BLOCK_V2 ) + sizeof( SVD_PREFIX ) ;
    char pszHeaderBuffer[sizeof(SVD_DIRTY_BLOCK_V2)+ sizeof(SVD_PREFIX)] = {0};

    SVD_PREFIX *drtdPrefix = (SVD_PREFIX*)( pszHeaderBuffer );
    drtdPrefix->tag = SVD_TAG_DIRTY_BLOCK_DATA_V2 ;
    //drtdPrefix->count = Changes.GetCurrentChangeIndex();
    drtdPrefix->count = 1;
    drtdPrefix->Flags = 0;

    SVD_DIRTY_BLOCK_V2 *DirtyBlock = (SVD_DIRTY_BLOCK_V2*)(pszHeaderBuffer + sizeof(SVD_PREFIX));
    DirtyBlock->Length = Length;
    DirtyBlock->ByteOffset = Offset;
    DirtyBlock->uiTimeDelta = timeDelta ;
    DirtyBlock->uiSequenceNumberDelta = sequenceDelta ;

    if(IsCompressionEnabled())
    {
        iStatus = m_pCompress->CompressStream((unsigned char *)pszHeaderBuffer, sizeHeaderBuffer);
        if(0 == iStatus)
        {
            iStatus = SV_SUCCESS;
        }
        else
        {
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        // Send the svd header
        iStatus = TransferDataToCX(  pszHeaderBuffer,
                                     sizeHeaderBuffer,
                                     sDestination,
                                     true);
        m_bProfileDiffsSize && (m_DiffSize+=sizeHeaderBuffer);
    }

    if (iStatus == SV_FAILURE)
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED %s.@ LINE %d in FILE %s \n",
                     FUNCTION_NAME, LINE_NO, FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  SendDirtyBlockHeaderOffsetAndLength
 *
 * DESCRIPTION :    Uploads the DRTD header with offset and length of change
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
// V2.0 Sends a Dirty Block Header, Offset and Length
int ProtectedGroupVolumes::SendDirtyBlockHeaderOffsetAndLength(const SV_LONGLONG& Offset,const SV_ULONG& Length,const string& sDestination)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;

    // Construct the svd header buffer
    //
    SV_UINT sizeHeaderBuffer = sizeof(SVD_DIRTY_BLOCK) + sizeof(SVD_PREFIX);
    char pszHeaderBuffer[sizeof(SVD_DIRTY_BLOCK)+ sizeof(SVD_PREFIX)] = {0};

    SVD_PREFIX *drtdPrefix = (SVD_PREFIX*)(pszHeaderBuffer);
    drtdPrefix->tag = SVD_TAG_DIRTY_BLOCK_DATA;
    //drtdPrefix->count = Changes.GetCurrentChangeIndex();
    drtdPrefix->count = 1;
    drtdPrefix->Flags = 0;

    SVD_DIRTY_BLOCK *DirtyBlock = (SVD_DIRTY_BLOCK*)(pszHeaderBuffer+sizeof(SVD_PREFIX));
    DirtyBlock->Length = Length;
    DirtyBlock->ByteOffset = Offset;

    if(IsCompressionEnabled())
    {
        iStatus = m_pCompress->CompressStream((unsigned char *)pszHeaderBuffer, sizeHeaderBuffer);
        if(0 == iStatus)
        {
            iStatus = SV_SUCCESS;
        }
        else
        {
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        // Send the svd header
        iStatus = TransferDataToCX(  pszHeaderBuffer,
                                     sizeHeaderBuffer,
                                     sDestination,
                                     true);
        m_bProfileDiffsSize && (m_DiffSize+=sizeHeaderBuffer);
    }

    if (iStatus == SV_FAILURE)
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED %s.@ LINE %d in FILE %s \n",FUNCTION_NAME, LINE_NO, FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    return iStatus;
}
/*
 * FUNCTION NAME :  SendDirtyBlockInfo
 *
 * DESCRIPTION :    Uploads the only the offset and length of change
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
// V1.0 Replica of S1. Sends Offset and Length only
int ProtectedGroupVolumes::SendDirtyBlockInfo(const SV_LONGLONG& Offset,const SV_ULONG& Length,const std::string& sDestination)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    // Construct the svd header buffer

    char pszHeaderBuffer[sizeof(SVD_DIRTY_BLOCK)] = {0};
    SVD_DIRTY_BLOCK *DirtyBlock = (SVD_DIRTY_BLOCK*)pszHeaderBuffer;

    DirtyBlock->Length = Length;
    DirtyBlock->ByteOffset = Offset;

    // Send the svd header

    iStatus = TransferDataToCX(  pszHeaderBuffer,
                                 sizeof(SVD_DIRTY_BLOCK),
                                 sDestination,
                                 true);

    if ( iStatus == SV_FAILURE )
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED SendDirtyBlockInfo().@ LINE %d in FILE %s \n",LINE_NO, FILE_NAME );
        iStatus = SV_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iStatus;
}
/*
 * FUNCTION NAME :  SendHeaderInfo
 *
 * DESCRIPTION :    Uploads the header information of the file
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
// V2.0 Sends only Header Info
int ProtectedGroupVolumes::SendHeaderInfo(const unsigned long int& cRecords,const string& sDestination)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    // Construct the svd header buffer

    const SV_UINT sizeHeaderBuffer = sizeof( SV_UINT ) + sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1);

    char pszHeaderBuffer[sizeHeaderBuffer] = {0};

    SV_UINT* dataFormatFlags = reinterpret_cast<SV_UINT*>(pszHeaderBuffer) ;
    ConvertorFactory::setDataFormatFlags( *dataFormatFlags ) ;

    // Write svd header into the header buffer
    //
    SVD_PREFIX *prefix = (SVD_PREFIX*)(pszHeaderBuffer + sizeof( SV_UINT ));
    prefix->tag = SVD_TAG_HEADER1;
    prefix->count = 1;
    prefix->Flags = 0;

    SVD_HEADER1 *header1 = (SVD_HEADER1*)(pszHeaderBuffer + sizeof( SV_UINT ) + sizeof(SVD_PREFIX));
    memset(header1, 0, sizeof(SVD_HEADER1));

    if(IsCompressionEnabled())
    {
        //We send the data to CX only after entire compression is complete. because we get the pointer
        // to the out buffer when we do the CompressStreamFinish.
        iStatus = m_pCompress->CompressStream((unsigned char*) pszHeaderBuffer, sizeHeaderBuffer);
        if(0 == iStatus)
        {
            iStatus = SV_SUCCESS;
        }
        else
        {
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        // Send the svd header
        iStatus = TransferDataToCX(  (char*)pszHeaderBuffer,
                                     sizeHeaderBuffer,
                                     sDestination,
                                     true);
        m_bProfileDiffsSize && (m_DiffSize+=sizeHeaderBuffer);
    }

    if (iStatus == SV_FAILURE)
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED SendHeaderInfo().@ LINE %d in FILE %s \n",LINE_NO, FILE_NAME );
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iStatus;
}
/*
 * FUNCTION NAME :  SendHeaderAndDirtyBlockInfo
 *
 * DESCRIPTION :    Uploads the header information of the file and the number of disk changes in the file
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
// V1.0 Replica of S1 Sends headers and Dirty Block info together
int ProtectedGroupVolumes::SendHeaderAndDirtyBlockInfo(const unsigned long int& cRecords,const string& sDestination)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;
    // Construct the svd header buffer
    SV_UINT sizeHeaderBuffer = sizeof(SVD_PREFIX)*2+ sizeof(SVD_HEADER1);

    char pszHeaderBuffer[sizeof(SVD_PREFIX)*2+ sizeof(SVD_HEADER1)] = {0};


    // Write svd header into the header buffer
    //
    SVD_PREFIX *prefix = (SVD_PREFIX*)pszHeaderBuffer;
    prefix->tag = SVD_TAG_HEADER1;
    prefix->count = 1;
    prefix->Flags = 0;

    SVD_HEADER1 *header1 = (SVD_HEADER1*)(pszHeaderBuffer + sizeof(SVD_PREFIX));
    memset(header1, 0, sizeof(SVD_HEADER1));

    SVD_PREFIX *drtdPrefix = (SVD_PREFIX*)(pszHeaderBuffer + sizeof(SVD_PREFIX) +sizeof(SVD_HEADER1));
    drtdPrefix->tag = SVD_TAG_DIRTY_BLOCK_DATA;
    drtdPrefix->count = cRecords;
    drtdPrefix->Flags = 0;

    // Send the svd header
    iStatus = TransferDataToCX(  (char*)pszHeaderBuffer,
                                 sizeHeaderBuffer,
                                 sDestination,
                                 true,
                                 true);

    if ( iStatus == SV_FAILURE)
    {
        DebugPrintf( SV_LOG_ERROR,"@ LINE %d in FILE %s \n", LINE_NO, FILE_NAME );
        DebugPrintf( SV_LOG_ERROR, "FAILED SendHeaderInfo()... \n");
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iStatus;
}
/*
 * FUNCTION NAME :
 *
 * DESCRIPTION :
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
/** Since Already available in common
    static bool IsColon( std::string::value_type ch ) { return ':' == ch; }
*/

/*
 * FUNCTION NAME :  SetRemoteName
 *
 * DESCRIPTION :
 // creates name in following format
 // "<destination directory>/<hostId>/<volume>/[pre_]completed_diff_[tso_]S<startTime>_<n>_E<endTime>_<n>_<MC|ME|WC|WE><continuation number>.dat
 // where:
 //   <destination directory>: cx directory to place the data in
 //   <hostid>: is the hostid of the agent sending the data
 //   <volume>: is the source volume the data is from
 //   [pre_]: optional - inserted if asked to create a pre name. this is the intial name used for sending the data
 //                      the pre_ is removed via a rename operation once all the data has been successfully sent
 //   [tso_]: optional -  inserted if this is for a time stamp only file (i.e. no actual changes in the change data)
 //   S<startTime>: timestamp from the dirty block for the start timestamp
 //   <n>: the sequence number from the dirty block for start timestamp
 //   E<startTime>: timestamp from the dirty block for the end timestamp
 //   <n>: the sequence number from the dirty block for end timestamp
 //   <MC|ME|WC|WE>: is the continuation indicator
 //      MC: meta data mode continuation more data coming with the same time stamps
 //      ME: meta data mode continuation end
 //      WC: write order mode continuation more data coming with the same time stamps
 //      WE: write order mode continuation end
 //   <continuation number>: specifies the continuation number.
 //
 // e.g. initial name (assuming continutaion id 1 and continucation end)
 //   /home/svsystems/9F623FAA-A20B-4446-B3E323FFB361C2C2/E/diffs/pre_completed_diff_S127794935171250000_1_E127794935212187500_1_ME1.dat
 // on successfully sending all the data it is renamed to
 //   /home/svsystems/9F623FAA-A20B-4446-B3E323FFB361C2C2/E/diffs/completed_diff_S127794935171250000_1_E127794935212187500_1_ME1.dat
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
bool ProtectedGroupVolumes::SetRemoteName(std::string & name,
                                          std::string & finalname,
                                          VolumeChangeData & changeData,
                                          unsigned continuationId,
                                          bool isContinuationEnd)
{

    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    string sVolName = this->GetSourceVolumeName();
    SV_ULONGLONG ullTSFC = 0;
    SV_ULONGLONG ullSeqFC = 0;
    SV_ULONGLONG ullTSLC = 0;
    SV_ULONGLONG ullSeqLC = 0;
    SV_ULONGLONG ullPrevEndTS = 0;
    SV_ULONGLONG ullPrevEndSeq = 0;
    SV_ULONG ulSeqIDForSplitIO = 0;
    SV_ULONG ulPrevSeqIDForSplitIO = 0;

#ifdef SV_WINDOWS
    if ( IsDrive(sVolName) )
    {
        UnformatVolumeNameToDriveLetter(sVolName);
    }
    else if ( IsMountPoint(sVolName))
    {
        sVolName = GetVolumeDirName(sVolName);
    }
#endif

    ullTSFC = changeData.GetFirstChangeTimeStamp();
    ullSeqFC = m_pDeviceFilterFeatures->IsPerIOTimestampSupported()?
               changeData.GetFirstChangeSequenceNumberV2():
               changeData.GetFirstChangeSequenceNumber();
    DebugPrintf(SV_LOG_DEBUG, "TSFC: " ULLSPEC ", SEQNOFC: " ULLSPEC "\n", ullTSFC, ullSeqFC);

    ullTSLC = changeData.GetLastChangeTimeStamp();
    ullSeqLC = m_pDeviceFilterFeatures->IsPerIOTimestampSupported()?
               changeData.GetLastChangeSequenceNumberV2():
               changeData.GetLastChangeSequenceNumber();  
    ulSeqIDForSplitIO = changeData.GetSequenceIdForSplitIO();

    std::stringstream timestamp;
    if( !m_pDeviceFilterFeatures->IsDiffOrderSupported() )
    {
        timestamp << "S" << ullTSFC
              << "_" << ullSeqFC
              << "_E" << ullTSLC
              << "_" << ullSeqLC;
    }
    else
    {
        ullPrevEndTS = changeData.GetPrevEndTS();
        ullPrevEndSeq = changeData.GetPrevEndSeqNo();
        ulPrevSeqIDForSplitIO = changeData.GetPrevSeqIDForSplitIO();
        timestamp << "P" << ullPrevEndTS
            << "_" << ullPrevEndSeq
            << "_" << ulPrevSeqIDForSplitIO
            << "_E" << ullTSLC
            << "_" << ullSeqLC;
    }
    DebugPrintf( SV_LOG_DEBUG, "TimeStamp String: %s\n", timestamp.str().c_str() ) ;
    if (m_transportProtocol == TRANSPORT_PROTOCOL_BLOB)
    {
        if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            name = GetSourceDiskId();
            name += Slash;
            name += FinalRemoteNamePrefix;
        }
        else
        {
            name = FinalRemoteNamePrefix;
        }
    }
    else if (m_transportProtocol == TRANSPORT_PROTOCOL_HTTP)
    {
        name = m_datapathTransportSettings.m_ProcessServerSettings.logFolder;
        name += Slash;
        name += PreRemoteNameExPrefix;
    }
    else
    {
        name = DestDir;
        name += m_pHostId;
        name += Slash;
        name += sVolName;
        name += Slash;
        name += DiffDirectory;
        name += PreRemoteNamePrefix;
    }
    if ( changeData.IsTimeStampsOnly() )
    {
        name += TimeStampsOnlyTag;
    }
    else if (changeData.IsTagsOnly())
    {
        name += TagsOnlyTag;
    }
    name += timestamp.str();
    name += WriteOrderTag(isContinuationEnd, changeData);
    std::stringstream strseqidforsplitIO;
    strseqidforsplitIO << ulSeqIDForSplitIO;
    // TODO: need to use continuation info from the changeData.GetDirtyBlock()
    // if in write order, either have it passed in or just check here and use it
    name += strseqidforsplitIO.str();
    if ((m_transportProtocol == TRANSPORT_PROTOOL_MEMORY) ||
        (m_transportProtocol == TRANSPORT_PROTOCOL_BLOB))
    {
        time_t currTime = time(NULL);
        std::string timeStr = boost::lexical_cast<std::string>(currTime);
        name += "_";
        name += timeStr;
    }
    name += DatExtension;
    if( IsCompressionEnabled() && (SOURCE_FILE != changeData.GetDataSource()) )
    {
        name += ".gz";
    }

    finalname = GetFinalNameFromPreName(name);

    DebugPrintf( SV_LOG_DEBUG, "Remote File Name: %s\n", name.c_str() ) ;
    DebugPrintf( SV_LOG_DEBUG, "Remote Final File Name: %s\n", finalname.c_str() ) ;
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return true;
}

const int ProtectedGroupVolumes::ThrottleNotifyToDriver()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceDiskId().c_str());

    int iStatus = SV_SUCCESS;
#ifdef SV_WINDOWS
    REPLICATION_STATE   replicationState = { 0 };
    inm_wmemcpy_s(replicationState.DeviceId, NELEMS(replicationState.DeviceId),
        GetSourceNameForDriverInput().c_str(),
        GetSourceNameForDriverInput().length());
    replicationState.ulFlags = REPLICATION_STATE_DIFF_SYNC_THROTTLED;
    iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_SET_REPLICATION_STATE, &replicationState, sizeof(replicationState), NULL, 0);
    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to set driver replication status as diff sync throttled for device %s with error %d\n",
            FUNCTION_NAME,
            GetSourceDiskId().c_str(),
            m_pDeviceStream->GetErrorCode());
    }
    else {
        DebugPrintf(SV_LOG_DEBUG, "%s: Sent throttle to driver for device %s\n", FUNCTION_NAME, GetSourceDiskId().c_str());
    }
#endif
    DebugPrintf(SV_LOG_DEBUG, "%s: Throttling Protected Drive %s...\n", FUNCTION_NAME, GetSourceDiskId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return iStatus;

}

/*
 * FUNCTION NAME :  IsThrottled
 *
 * DESCRIPTION :    Checks if source is throttled from sending differentials
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if source is throttled else false
 *
 */
const bool ProtectedGroupVolumes::IsThrottled()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    bool bThrottleDataTransfer = false;
    do
    {
        if( bThrottleDataTransfer )
        {
            DebugPrintf( SV_LOG_WARNING,"Sentinel:Throttling Protected Drive %s...\n",GetSourceVolumeName().c_str() );
            if( IsStopRequested() )
            {
                break;
            }
            WaitOnQuitEvent(30);
        }
        else
        {
            break;
        }
    }while ( true );

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bThrottleDataTransfer;

}
/*
 * FUNCTION NAME :  SendUserTags
 *
 * DESCRIPTION :    Uploads the user tags in the dirty block.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
// BugBug: May not work correctly for unicode character set.
int ProtectedGroupVolumes::SendUserTags(const void* pDirtyBlock,const string& sTransferPath)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    unsigned char* BufferForTags ;
    STREAM_REC_HDR_4B *TagEndOfList ;
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
        UDIRTY_BLOCK_V2* DirtyBlock = ( UDIRTY_BLOCK_V2* ) pDirtyBlock ;
        BufferForTags = DirtyBlock->uTagList.BufferForTags ;
        TagEndOfList = &( DirtyBlock->uTagList.TagList.TagEndOfList ) ;
    }
    else
    {
        UDIRTY_BLOCK* DirtyBlock = ( UDIRTY_BLOCK* ) pDirtyBlock ;
        BufferForTags = DirtyBlock->uTagList.BufferForTags ;
        TagEndOfList = &( DirtyBlock->uTagList.TagList.TagEndOfList ) ;
    }
    int iStatus = SV_SUCCESS;
    if ( !IsTagSent())
    {
        SVD_PREFIX *puserTagPrefix = NULL;

        unsigned long ulBytes = (SV_ULONG) ((SV_PUCHAR) TagEndOfList - (SV_PUCHAR) BufferForTags);
        PSTREAM_GENERIC_REC_HDR pTag = NULL;

        pTag = (PSTREAM_GENERIC_REC_HDR) (BufferForTags + ulBytes);
        SV_PUCHAR pUCharData = NULL;
        boost::shared_ptr<char> pSendData;
        int iSizeOfUserTagChange = 0;
        while( (pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) && ( iStatus == SV_SUCCESS ) )
        {
            iSizeOfUserTagChange = 0;
            SV_ULONG ulLength = GET_STREAM_LENGTH(pTag);
            SV_ULONG ulLengthOfTag = 0;

            ulLengthOfTag = *(SV_PUSHORT)((SV_PUCHAR)pTag + GET_STREAM_HEADER_LENGTH(pTag));

            iSizeOfUserTagChange = sizeof(SVD_PREFIX) + ulLengthOfTag;
            pSendData.reset(new char[iSizeOfUserTagChange]);

            memset(pSendData.get(),'\000',iSizeOfUserTagChange);

            puserTagPrefix          = (SVD_PREFIX*)pSendData.get();
            puserTagPrefix->tag     = SVD_TAG_USER;
            puserTagPrefix->count   = 1;
            puserTagPrefix->Flags   = ulLengthOfTag;

            pUCharData  = (SV_PUCHAR) ((SV_PUCHAR)pTag + GET_STREAM_HEADER_LENGTH(pTag) + sizeof(SV_USHORT)); //(ulLength - ulLengthOfTag) ;

            /*************************************************
             * This pUCharData is directly copied from driver *
             * This has structure(4B or 8B). driver has to put*
             * endian ness code here. There is tricky         *
             * multiplexing here between either a 4B or 8B    *
             * structure. This has to be handled by driver and*
             * target. From s2 side only prefix has to be     *
             * swapped.                                       *
             *************************************************/
            inm_memcpy_s(pSendData.get() + sizeof(SVD_PREFIX), (iSizeOfUserTagChange - sizeof(SVD_PREFIX)), pUCharData,ulLengthOfTag);

            DebugPrintf(SV_LOG_DEBUG,"INFO: Received user tag data of length %lu .\n",ulLengthOfTag);
            for(unsigned int uiIndex = 0; uiIndex < ulLengthOfTag;uiIndex++)
            {
                DebugPrintf(SV_LOG_DEBUG,"%x ",pUCharData[uiIndex]);
            }
            DebugPrintf(SV_LOG_DEBUG,"\n");

            if(IsCompressionEnabled())
            {
                iStatus = m_pCompress->CompressStream((unsigned char*) pSendData.get(), iSizeOfUserTagChange);
                if(0 == iStatus)
                {
                    iStatus = SV_SUCCESS;
                }
                else
                {
                    iStatus = SV_FAILURE;
                }
            }
            else
            {
                iStatus = TransferDataToCX(pSendData.get(),iSizeOfUserTagChange,sTransferPath,true);
                m_bProfileDiffsSize && (m_DiffSize+=iSizeOfUserTagChange);
            }
            if ( iStatus == SV_FAILURE)
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to send user tag data of length %lu. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME,ulLengthOfTag);
            }
            else
            {
                m_bTagSent = true;
            }

            ulBytes += ulLength;
            pTag = (PSTREAM_GENERIC_REC_HDR)(BufferForTags + ulBytes);
            puserTagPrefix = NULL;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  ReportIfResyncIsRequired
 *
 * DESCRIPTION :    Reports to the cx if a resync required flag is set in the dirty block being processed.
 *                  Reports the err code and the err msg along with the volume name, if resync required flag is set.
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
bool ProtectedGroupVolumes::ReportIfResyncIsRequired(VolumeChangeData& VolumeChanges)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    if(VolumeChanges.IsResyncRequired())
    {
        ResyncRequestor resyncRequestor;
        SV_ULONG ulOutOfSyncErrorCode ;
        char* ErrorStringForResync ;
        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {
            UDIRTY_BLOCK_V2* DirtyBlock = ( UDIRTY_BLOCK_V2* ) ( VolumeChanges.GetDirtyBlock() ) ;
            ulOutOfSyncErrorCode = DirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode ;
            ErrorStringForResync = (char*) DirtyBlock->uHdr.Hdr.ErrorStringForResync ;
        }
        else
        {
            UDIRTY_BLOCK* DirtyBlock = ( UDIRTY_BLOCK* ) ( VolumeChanges.GetDirtyBlock() ) ;
            ulOutOfSyncErrorCode = DirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode ;
            ErrorStringForResync = (char*) DirtyBlock->uHdr.Hdr.ErrorStringForResync ;
        }

        m_resyncRequired = true;
        
        if (!resyncRequestor.ReportResyncRequired(GetSourceVolumeName(),
            VolumeChanges.GetOutOfSyncTimeStamp(),
            ulOutOfSyncErrorCode,
            ErrorStringForResync,
            m_pDeviceStream->GetName()))
        {

            DebugPrintf(SV_LOG_ERROR,
                "%s: Resync required for Volume: %s. ErrorCode %d ErrorString %s TimeStamp %lld.\n",
                FUNCTION_NAME,
                GetSourceVolumeName().c_str(),
                ulOutOfSyncErrorCode,
                ErrorStringForResync,
                VolumeChanges.GetOutOfSyncTimeStamp());
        }
     
        if (ulOutOfSyncErrorCode == E_RESYNC_REQUIRED_REASON_FOR_RESIZE)
        {
            MonitoringLib::SendAlertToRcm(SV_ALERT_CRITICAL,
                SV_ALERT_MODULE_DIFFS_DRAINING,
                DiskResizeAlert(GetSourceVolumeName()),
                std::string(m_pHostId),
                GetSourceDiskId());
        }
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return m_resyncRequired;
}
/*
 * FUNCTION NAME :  ProcessDataFile
 *
 * DESCRIPTION :    Uploads the file created by the driver in data file mode.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::ProcessDataFile(VolumeChangeData& Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    /* PR # 5113 : START */
    int iStatus = SV_SUCCESS;
    const std::string LocalFileName = Changes.GetFileName();
    if (LocalFileName.empty())
    {
        iStatus = SV_FAILURE;
    }

    if (SV_SUCCESS == iStatus)
    {
        /* PR # 5113 : END */
        /* PR # 6387 : START
         * Pre TargetFile is formed here so that if connectivity to CX fails,
         * DeleteInconsistentFileFromCX(TargetFile) is done */
        std ::string TargetFile;
        std ::string RenameFile;
        iStatus = SetPreTargetDataFileName(Changes, LocalFileName, TargetFile, RenameFile);

        if (SV_SUCCESS == iStatus)
        {
            iStatus = deletePrevFileIfReq(TargetFile);

            if (SV_SUCCESS == iStatus)
            {
                /* Check if previouse sent name is got again from driver */
                if( (Changes.m_bCheckDB) &&
                    (!previousSentName.empty()) &&
                    ( (previousSentName.find( TargetFile ) != string::npos) ||
                      ( TargetFile.find( previousSentName )!= string::npos) ) )
                {
                    DebugPrintf( SV_LOG_ERROR,
                                 "Dirty Block committed. But received the same file name from the driver. @LINE %d in FILE %s\n" ,
                                 LINE_NO ,FILE_NAME) ;
                    DebugPrintf( SV_LOG_ERROR, "previousSentName = %s\n", previousSentName.c_str() ) ;
                    DebugPrintf( SV_LOG_ERROR, "TargetFile = %s\n" , TargetFile.c_str()) ;
                    iStatus = SV_FAILURE ;
                    WaitOnQuitEvent(SECS_TO_WAITFOR_VOL);
                }
                else
                {
                    /* PR # 5926 : END */
                    iStatus = TransferFileToCX(LocalFileName, TargetFile, RenameFile, Changes) ;
                    if ( SV_SUCCESS == iStatus )
                    {
                        /* PR # 6387 : END */
                        DebugPrintf(SV_LOG_DEBUG,"INFO: Volume: %s. Committing file mode changes in dirty block. %s\n",GetSourceVolumeName().c_str(),
                                    LocalFileName.c_str());
                        iStatus = Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput);
                        if (SV_SUCCESS == iStatus)
                        {
                            ResetErrorCounters();
                            if (m_bProfileDiffsSize) {
                                m_TotalDiffsSize += m_DiffSize;
                                m_TotalCompressedDiffsSize += m_CompressedDiffSize;
                                PrintDiffsRate();
                            }
                            if (m_fpProfileLog)
                            {
                                m_TimeAfterCommit = ACE_OS::gettimeofday();
                                Profile(TargetFile, Changes);
                            }
                            if (m_bHasToCheckTS)
                            {
                                int recstatus = RecordPrevDirtyBlockDataAfterRename(Changes);
                                if (SV_SUCCESS != recstatus)
                                {
                                    /** FIXME: Just record error since commit happend, cannot do anything */
                                    DebugPrintf(SV_LOG_ERROR, "RecordPrevDirtyBlockDataAfterRename failed for volume %s @ LINE %d in FILE %s\n",
                                        GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                                }
                            }
                            previousSentName = TargetFile;
                            if (ShouldWaitForData(Changes))
                            {
                                WaitForData();
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "INFO: Volume: %s. Failed to commit file mode changes in dirty block for file %s\n",
                                GetSourceVolumeName().c_str(),
                                LocalFileName.c_str());
                            DeleteSentDiffFile(RenameFile);
                        }
                    }
                    else
                    {
                        iStatus = SV_FAILURE;
                    }
                }
                /* PR # 5926 : START */
            } /* if (SV_SUCCESS == iStatus) */
            /* PR # 5926 : END */
        } /* end of if (SV_SUCCESS == iStatus) */
        else
        {
            /* TargetFile did not form. Just log it */
            DebugPrintf (SV_LOG_ERROR,
                         "FAILED: Setting PreTargetDataFileName failed. @LINE %d in FILE %s\n",
                         LINE_NO, FILE_NAME);
        }
        /* PR # 6387 : END */
        /* PR # 5113 : START */
    } /* End of if (SV_SUCCESS == iStatus) for LocalFileName */
    /* PR # 5113 : END */
    else
    {
        DebugPrintf (SV_LOG_ERROR, "FAILED: Volume %s. Changes.GetFileName() failed . @LINE %d in FILE %s\n", GetSourceVolumeName().c_str (), LINE_NO,
                     FILE_NAME);

    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    return iStatus;
}
/*
 * FUNCTION NAME :  SendFileHeaders
 *
 * DESCRIPTION :    Uploads the File header,Timestamp of first change,user tags and size of change.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::SendFileHeaders(VolumeChangeData& Changes,
                                           unsigned long int uliChanges,
                                           const SV_LONGLONG& SizeOfChangesToSend,
                                           const std::string& fileName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_FAILURE;

    //HeaderInfo modified to implement the compression changes.
    if ( SV_SUCCESS == (iStatus = SendHeaderInfo( uliChanges,fileName)) )
    {
        // Send the start timestamp
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {
            UDIRTY_BLOCK_V2* DirtyBlock = ( UDIRTY_BLOCK_V2* ) ( Changes.GetDirtyBlock() ) ;
            iStatus = SendTimeStampInfoV2( DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange,
                                           FIRST_CHANGE,
                                           fileName ) ;
        }
        else
        {
            UDIRTY_BLOCK* DirtyBlock = ( UDIRTY_BLOCK* ) ( Changes.GetDirtyBlock() ) ;
            iStatus = SendTimeStampInfo(DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange,
                                        FIRST_CHANGE,
                                        fileName) ;
        }
        if ( SV_SUCCESS == iStatus )
        {
            //SendUserTags modifed to implement the compression changes.
            if ( SV_SUCCESS == (iStatus = SendUserTags(Changes.GetDirtyBlock(),fileName) ) )
            {
                // Send the size of the changes only if size is greater than zero
                if ( 0 < SizeOfChangesToSend)
                    //SendSizeChangesInfo modified to implement the compression changes.
                    iStatus = SendSizeOfChangesInfo(SizeOfChangesToSend,fileName);
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;

}
/*
 * FUNCTION NAME :  ProcessDataStream
 *
 * DESCRIPTION :    Uploads data to file when  in data stream mode
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::ProcessDataStream(VolumeStream& volStream,VolumeChangeData& Changes,const std::string& fileName, 
                                             const std::string &finalName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    /* PR # 5926 : START */
    int iStatus = SV_SUCCESS;
    SV_ULONG ulSize = 0;
    unsigned const char* pBuffer = NULL;

    iStatus = deletePrevFileIfReq(fileName);

    if (SV_SUCCESS == iStatus)
    {
        /* PR # 5926 : END */
        SV_INT numbufs = Changes.GetNumberOfDataStreamBuffers();
        DebugPrintf(SV_LOG_DEBUG, "number of data stream buffers = %d\n", numbufs);

        if (numbufs != 1)
        {
            SV_ULONG sizeOfChanges = GetStreamDataSize(Changes);
            m_pTransportStream->SetSizeOfStream(sizeOfChanges);
        }

        do
        {
            volStream >> Changes;
            if (volStream.Good () )
            {
                DebugPrintf(SV_LOG_DEBUG, "In function %s, @LINE %d in FILE %s, Changes.GetData() = %p, Changes.GetDataLength() = %lu for volume %s.\n", FUNCTION_NAME, LINE_NO, FILE_NAME, Changes.GetData(), Changes.GetDataLength(), GetSourceVolumeName().c_str());

#ifdef SV_UNIX
                boost::shared_ptr<Checksums_t> checksums;
                if (m_bSVDCheckEnabled)
                {
                    DebugPrintf(SV_LOG_DEBUG,
                        "calling SvdCheckOnDataStream with Changes.GetData() = %p, Changes.GetDataLength() = %lu, MoreData = %u, numbufs = %u for volume %s.\n",
                        Changes.GetData(), Changes.GetDataLength(), volStream.MoreData(), numbufs, GetSourceVolumeName().c_str());

                    SvdCheckOnDataStream(Changes.GetData(), Changes.GetDataLength(), fileName, checksums, Changes);
                }
#endif

                if(IsCompressionEnabled())
                {
                    iStatus = m_pCompress->CompressStream((unsigned char *)Changes.GetData(), Changes.GetDataLength());

                    if(0 != iStatus)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Unable to compress the data buffer @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                        iStatus = SV_FAILURE;
                    }
                    else
                    {
                        //Need too set this as the compress stream will return 0 or negative value.
                        iStatus = SV_SUCCESS;
                        if(Changes.IsTransactionComplete())
                        {
                            //No more data, finish the compression and send the data to cx.
                            ulSize = 0;
                            unsigned long *pul = m_bProfileDiffsSize ? &m_DiffSize : 0;
                            pBuffer = m_pCompress->CompressStreamFinish(&ulSize, pul);
                            if (NULL != pBuffer)
                            {
                                m_bProfileDiffsSize && (m_CompressedDiffSize=ulSize);
                                DebugPrintf(SV_LOG_DEBUG, "In function %s, @LINE %d in FILE %s, pBuffer = %p, ulSize = %lu for volume %s. After compress stream finish.\n", FUNCTION_NAME, LINE_NO, FILE_NAME, pBuffer, ulSize, GetSourceVolumeName().c_str());
                                iStatus = TransferDataToCX ((const char *)pBuffer, ulSize, fileName, false);
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_ERROR, "CompressStreamFinish returned NULL @ LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                                iStatus = SV_FAILURE;
                            }
                        }
                    }
                }
                else
                {
                    iStatus = TransferDataToCX (    Changes.GetData (),
                                                    Changes.Size (), fileName,
                                                    volStream.MoreData(), 
                                                    false, (1 == numbufs));
                    m_bProfileDiffsSize && (m_DiffSize+=Changes.Size());
                }
            }
        }while ((Changes.IsTransactionComplete () == false) &&
                volStream.MoreData() &&
                SV_SUCCESS == iStatus);

        if (Changes.IsTransactionComplete ()
            && SV_SUCCESS == iStatus
            && volStream.Good ())
        {
            if (SV_SUCCESS ==
                (iStatus = FinishSendingData (fileName, finalName, Changes, m_continuationId,
                                              Changes.IsContinuationEnd ())))
            {

#ifdef SV_UNIX
                boost::shared_ptr<Checksums_t> checksums;
                if (m_bSVDCheckEnabled)
                {
                    DebugPrintf(SV_LOG_DEBUG,
                        "calling SvdCheckOnDataStream with Changes.GetData() = %p, Changes.GetDataLength() = %lu, MoreData = %u, numbufs = %u for volume %s.\n",
                        Changes.GetData(), Changes.GetDataLength(), volStream.MoreData(), numbufs, GetSourceVolumeName().c_str());

                    SvdCheckOnDataStream(Changes.GetData(), Changes.GetDataLength(), fileName, checksums, Changes);
                }
#endif

                if (SV_SUCCESS == (iStatus = Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput)))
                {
                    ResetErrorCounters();
                    if (m_bProfileDiffsSize) {
                        m_TotalDiffsSize += m_DiffSize;
                        m_TotalCompressedDiffsSize += m_CompressedDiffSize;
                        PrintDiffsRate();
                    }
                    if (m_fpProfileLog)
                    {
                        m_TimeAfterCommit = ACE_OS::gettimeofday();
                        Profile(fileName, Changes);
                    }
                    if (m_bHasToCheckTS)
                    {
                        int recstatus = RecordPrevDirtyBlockDataAfterRename(Changes);
                        if (SV_SUCCESS != recstatus)
                        {
                            DebugPrintf(SV_LOG_ERROR, "RecordPrevDirtyBlockDataAfterRename failed for volume %s @ LINE %d in FILE %s\n",
                                GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                        }
                    }
                    previousSentName = fileName;
                    if (ShouldWaitForData(Changes) && (!ShouldQuit()))
                    {
                        WaitForData();
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "volume: %s, in process data stream, for file %s, commit failed\n",
                        GetSourceVolumeName().c_str(),
                        fileName.c_str());
                    DeleteSentDiffFile(finalName);
                }
            }
            else
            {
                DebugPrintf (SV_LOG_ERROR,
                             "FAILED: Volume %s. Unable to complete dirty block transfer. Starting over again. @LINE %d in FILE %s\n",
                             GetSourceVolumeName().c_str (), LINE_NO,
                             FILE_NAME);
            }
        }
        else
        {
            /* Added iStatus = SV_FAILURE */
            iStatus = SV_FAILURE;
            DebugPrintf (SV_LOG_DEBUG,
                         "FAILED: Volume %s. Unable to complete dirty block transfer. Starting over again. @LINE %d in FILE %s\n",
                         GetSourceVolumeName().c_str (), LINE_NO, FILE_NAME);
        }
        /* PR # 5926 : START */
    } /* end of if (SV_SUCCESS == iStatus) */
    /* PR # 5926 : END */
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  ShouldQuit
 *
 * DESCRIPTION :    Checks if sentinel can exit. In case it is throttled then it returns
 *                  true if exit is signalled.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if sentinel can exit else false
 *
 */
bool ProtectedGroupVolumes::ShouldQuit() const
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    /*
     * The check for m_uiCommittedChanges and m_bCompletedSendingDirtyBlock has been removed because we dont want s2 to go into an infinite loop of trying to send the dirty block when the trasport stream is unable to do so.
     */
    if ( IsStopRequested() && (PROCESS_QUIT_IMMEDIATELY == m_liQuitInfo) )
    {
        return true;
    }
    // BUGBUG: This should be
    /*
          else if ( IsStopRequested() && (PROCESS_QUIT_TIME_OUT == m_liQuitInfo) )
          {
          if ( !IsThrottled() && ( m_uiCommittedChanges > 0 ))
          return true;
          else if ( IsThrottled() )
          return true;
          }
    */
    else if ( IsStopRequested() && (PROCESS_QUIT_TIME_OUT == m_liQuitInfo) )
    {
        return true;
    }
    // BUGBUG: This should be
    /*
          else if ( IsStopRequested() && (PROCESS_QUIT_REQUESTED == m_liQuitInfo) )
          {
          if ( !IsThrottled() && ( m_uiCommittedChanges > 0 ))
          return true;
          else if ( IsThrottled() )
          return true;
          }
    */

    else if ( IsStopRequested() && (PROCESS_QUIT_REQUESTED == m_liQuitInfo) )
    {
        return true;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return false;
}
/*
 * FUNCTION NAME :  ProcessTagsOnly
 *
 * DESCRIPTION :    Uploads a dirty block which has no disk changes but has a user tag
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::ProcessTagsOnly(VolumeChangeData& Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    /* PR # 5926 : START */
    int iStatus = SV_SUCCESS;
    SV_ULONG sizeOfHeaders = 0;
    std::string preName;
    std::string finalName;


    if(!m_pTransportStream->Good())
    {
        /*If the transport stream is bad then reset the connection.*/
        iStatus = InitTransport();
    }

    if (SV_SUCCESS == iStatus)
    {
        /* PR # 5926 : END */

        // No disk changes. But there are tags. So send timestamps and tags
        //TODO:Calculate sizeof Timestamps and Tags Headers
        sizeOfHeaders = GetDiffFileSize(Changes,0); // For Headers putting sizeofchanges argument as 0
        m_pTransportStream->SetSizeOfStream(sizeOfHeaders);
        if ( SendTimestampsAndTags(Changes, preName, finalName) )
        {

            assert(IsTagSent()); //If it came here without tag then there is something wrong

            if (SV_SUCCESS == (iStatus = Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput)))
            {
                ResetErrorCounters();
                if (m_bProfileDiffsSize) {
                    m_TotalDiffsSize += m_DiffSize;
                    m_TotalCompressedDiffsSize += m_CompressedDiffSize;
                    PrintDiffsRate();
                }
                if (m_fpProfileLog)
                {
                    m_TimeAfterCommit = ACE_OS::gettimeofday();
                    Profile(preName, Changes);
                }
                if (m_bHasToCheckTS)
                {
                    int recstatus = RecordPrevDirtyBlockDataAfterRename(Changes);
                    if (SV_SUCCESS != recstatus)
                    {
                        DebugPrintf(SV_LOG_ERROR, "RecordPrevDirtyBlockDataAfterRename failed for volume %s @ LINE %d in FILE %s\n",
                            GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                    }
                }
                previousSentName = preName;

                DebugPrintf(SV_LOG_DEBUG, "Sending tags only succeeded.\n");
                if (Changes.GetTotalChangesPending() < m_ThresholdDirtyblockSize)
                {
                    WaitForData();
                }
                m_BytesToDrainWithoutWait = 0;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "volume: %s, in process tags only, for file %s, commit failed\n",
                    GetSourceVolumeName().c_str(),
                    preName.c_str());
                DeleteSentDiffFile(finalName);
            }
        }
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to send timestamp and user tags data %s. @LINE %d in FILE %s.\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
        }
        /* PR # 5926 : START */
    } /* if (SV_SUCCESS == iStatus) */

    if (SV_FAILURE == iStatus)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: ProcessTagsOnly Failed . @LINE %d in FILE %s.\n", LINE_NO, FILE_NAME);

        WAIT_ON_RETRY_NETWORK_ERROR(Changes);
    }

    /* PR # 5926 : END */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  ProcessTimeStampsOnly
 *
 * DESCRIPTION :    Uploads a dirty block which has only timestamps but no tags and disk changes
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::ProcessTimeStampsOnly(VolumeChangeData& Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    /* PR # 5926 : START */
    int iStatus = SV_SUCCESS;
    SV_ULONG sizeOfHeaders = 0;
    std::string preName;
    std::string finalName;

    if( IsCompressionEnabled() )
    {
        m_pCompress->CompressStreamInit();
    }

    if(!m_pTransportStream->Good())
    {
        /*If the transport stream is bad then reset the connection.*/
        iStatus = InitTransport();
    }

    if (SV_SUCCESS == iStatus)
    {
        /* PR # 5926 : END */

       /* if (!ReportIfResyncIsRequired(Changes) )
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to report resync required to CX. It will be retried for volume %s\n",
                        GetSourceVolumeName().c_str());
        }*/
        //TODO:Calculate Total size of Timestamp and Tags
        sizeOfHeaders = GetDiffFileSize(Changes,0); // For Headers putting sizeofchanges argument as 0
        m_pTransportStream->SetSizeOfStream(sizeOfHeaders);
        if ( SendTimestampsAndTags(Changes, preName, finalName) )
        {
            if (SV_SUCCESS == Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput))
            {
                ResetErrorCounters();
                if (m_bProfileDiffsSize) {
                    m_TotalDiffsSize += m_DiffSize;
                    m_TotalCompressedDiffsSize += m_CompressedDiffSize;
                    PrintDiffsRate();
                }
                if (m_fpProfileLog)
                {
                    m_TimeAfterCommit = ACE_OS::gettimeofday();
                    Profile(preName, Changes);
                }
                if (m_bHasToCheckTS)
                {
                    int recstatus = RecordPrevDirtyBlockDataAfterRename(Changes);
                    if (SV_SUCCESS != recstatus)
                    {
                        DebugPrintf(SV_LOG_ERROR, "RecordPrevDirtyBlockDataAfterRename failed for volume %s @ LINE %d in FILE %s\n",
                            GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                    }
                }
                previousSentName = preName;
                iStatus = SV_SUCCESS;
                WaitForData();
                m_BytesToDrainWithoutWait = 0;
            }
            else
            {
                iStatus = SV_FAILURE;
                DebugPrintf(SV_LOG_ERROR, "volume: %s, in process tso only, for file %s, commit failed\n",
                    GetSourceVolumeName().c_str(),
                    preName.c_str());
                DeleteSentDiffFile(finalName);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to Send Timestams and Tags in the tso file. Volume: %s. Line: %d File: %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
            iStatus = SV_FAILURE;
        }
        /* PR # 5926 : START */
    } /* if (SV_SUCCESS == iStatus) */

    if (SV_FAILURE == iStatus)
    {
        DebugPrintf(m_bThrottled?SV_LOG_DEBUG:SV_LOG_ERROR, "FAILED: ProcessTimeStampsOnly Failed . @LINE %d in FILE %s.\n", LINE_NO, FILE_NAME);
    }

    /* PR # 5926 : END */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  Replicate
 *
 * DESCRIPTION :    1) Acquires a dirty block by issuing an IOCTL
 *                  2) If resync is required it reports it to the cx
 *                  3) Gets the source of data
 *                  4) Checks if source of data is a file and uploads the same
 *                  5) Else Checks if source of data is a buffer and uploads the same
 *                  6) Else Checks if source of data is a stream and uploads the same
 *                  7) Else Checks if source of data is a bitmap or meta data and uploads the same
 *                  8) If there are no disk changes then uploads only timestamps
 *                  9) If there is a an error in acquiring dirty block, the it retries every 5 seconds.
 Failure to acquire a dirty block can occur if volume is offline
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
// 2.0 sends multiple dirty blocks in a single file limited by volume chunk size
int ProtectedGroupVolumes::Replicate()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());


    int iStatus = SV_SUCCESS;

    if (IsInitialized()) {
        unsigned long int uliTotalChanges =  0;
        unsigned long int uliChangesToSend =  0;
        unsigned long int uliChangeIndex =  0;
        unsigned long int uliChangesAlreadySent = 0;
        int TRANSPORT_OK  = 1;
        SV_LONGLONG Offset  = 0;
        SV_ULONG       Length  = 0;
        SV_LONGLONG     SizeOfChangesToSend=0;
        SV_ULONG           ulSizeOfChangesSent=0;
        SV_LONGLONG     uliTotalChangesPending=0;
        SV_LONGLONG TotalSizeOfBuffer =0;
        unsigned DrvBusyRollingCount = 0;
        
        std::string preName;
        std::string finalName;

        DB_DATA_STATUS dbStatus;
        GetWaitForDBNotify();

        /**
        * TODO: Need to ask
        * OOD: make the volumechangedata with the driver version
        *      Make the isSVGSupported and isDirtyBlockNumSupported inline
        *      define bool variables for SVG supported and isDirtyBlockNumSupported
        *      (or) access the bools directly to avoid function calls
        *      lets access it directly even though functions are added and made inline
        *      because of fastness
        */
        SV_ULONGLONG srcstartoffset = GetSrcStartOffset();
        VolumeChangeData Changes( m_pDeviceFilterFeatures , srcstartoffset);
        while (!IsStopRequested() && ((iStatus = Changes.Init(m_pMetadataReadBufLen) ) != SV_SUCCESS))
        {
            DebugPrintf(SV_LOG_FATAL, "Failed to Initialize the VolumeChangeData structure. Retrying...\n") ;
            WAIT_ON_RETRY_MALLOC_ERROR();
        }

        Changes.SetProfiler(m_DiskReadProfiler);//set Disk Read Profiler to measure disk read latencies

        std::stringstream sssrcstartoffset;
        sssrcstartoffset << srcstartoffset;
        DebugPrintf(SV_LOG_DEBUG, "SVG supported: %s for volume %s\n", m_pDeviceFilterFeatures->IsPerIOTimestampSupported()?YES:NO, GetSourceVolumeName().c_str());
        DebugPrintf(SV_LOG_DEBUG, "diffential ordering supported: %s for volume %s\n", m_pDeviceFilterFeatures->IsDiffOrderSupported()?YES:NO,
                                  GetSourceVolumeName().c_str());
        DebugPrintf(SV_LOG_DEBUG, "mirroring supported: %s for volume %s\n", m_pDeviceFilterFeatures->IsMirrorSupported()?YES:NO,
                                  GetSourceVolumeName().c_str());
        DebugPrintf(SV_LOG_DEBUG, "source start offset: %s for source %s\n", sssrcstartoffset.str().c_str(), GetSourceVolumeName().c_str());

        //m_uiCommittedChanges = 0;
        // create tal handle and handler need to worry about timeout if syncing as we don't send timestamps while syncing
        while( !IsStopRequested() )
        {
            Changes.Reset(0);
            m_liTotalLength = 0;
            uliTotalChanges = 0;
            m_bTagSent = false;
            //m_bCompletedSendingDirtyBlock = false;
            m_continuationId = 0;
            bool bCheckPassed = true;

            /* PR # 6254 : START
             * Added this if to eliminate call to GetSourceVolume() */
            if (( NULL == m_pVolumeStream ) && (SV_SUCCESS != CreateVolumeStream()))
            {
                DebugPrintf(SV_LOG_ERROR, "CreateVolumeStream failed for volume %s @LINE %d in File %s \n", GetSourceVolumeName().c_str(), LINE_NO,
                            FILE_NAME);
            }
            else
            {
                m_pVolumeStream->Clear();

                if (m_bProfileDiffsSize)
                    ResetPerDiffProfilerValues();
                if (m_fpProfileLog)
                    m_TimeBeforeGetDB = ACE_OS::gettimeofday(); 

                /* PR # 6254 : END */

                // change trasport settings only at the begining of transaction with driver
                if (m_bTransportSettingsChanged)
                    UpdateTransportSettings();

                if ( ( (dbStatus = Changes.BeginTransaction(*m_pDeviceStream, m_SourceNameForDriverInput)) == DATA_AVAILABLE) &&
                     (!IsStopRequested() ) )
                {
                    if (ReportIfResyncIsRequired(Changes) )
                    {
                        iStatus = Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput);
                        if (SV_SUCCESS == iStatus)
                        {
                            ResetErrorCounters();
                            DebugPrintf(SV_LOG_ERROR, "volume: %s, for ResyncRequired, commit succeeded\n",
                                GetSourceVolumeName().c_str());
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "volume: %s, for ResyncRequired, commit failed\n",
                                GetSourceVolumeName().c_str());
                        }
                        break;
                    }

                    DATA_SOURCE SourceOfData = Changes.GetDataSource();

                    bCheckPassed = m_bHasToCheckTS?VerifyPreviousEndTSAndSeq(Changes):true;

                    if (bCheckPassed)
                    {
                        if( IsCompressionEnabled() &&
                            ( SOURCE_FILE != SourceOfData ) )
                        {
                            m_pCompress->CompressStreamInit();
                        }

                        if(SOURCE_FILE == SourceOfData)
                        {
                            iStatus = ProcessDataFile(Changes);

                            if(SV_SUCCESS != iStatus)
                            {
                                HandleTransportFailure(Changes, SourceOfData);
                            }
                        }
                        else if ( SOURCE_STREAM == SourceOfData)
                        {
                            SetRemoteName (preName, finalName, Changes, m_continuationId,Changes.IsContinuationEnd());
                            if(m_transportProtocol == TRANSPORT_PROTOOL_MEMORY)
                            {
                                DebugPrintf(SV_LOG_DEBUG,"Protocol is memory stream so preName %s is final name %s\n",preName.c_str(),finalName.c_str());
                                preName = finalName;

                            }
                            if( (Changes.m_bCheckDB) && 
                                (!previousSentName.empty()) &&
                                (( previousSentName.find( preName ) != string::npos) ||
                                 (preName.find( previousSentName )!= string::npos) ) )
                            {
                                DebugPrintf( SV_LOG_ERROR,
                                             "Dirty Block committed. But received the same file name from the driver. @LINE %d in FILE %s\n",
                                             LINE_NO ,FILE_NAME) ;
                                DebugPrintf( SV_LOG_ERROR, "previousSentName = %s\n preName = %s\n", previousSentName.c_str(), preName.c_str() ) ;
                                WaitOnQuitEvent(SECS_TO_WAITFOR_VOL);
                            }
                            else
                            {
                                bool bCanProcessDataStream = SetUpFileNamesForChecks(preName);
                                if (bCanProcessDataStream)
                                {
                                    if(SV_SUCCESS != ProcessDataStream(*m_pVolumeStream,Changes,preName, finalName))
                                    {
                                        HandleTransportFailure(Changes, SourceOfData);
                                    }
                                } /* end of if (bCanProcessDataStream) */
                                else
                                {
                                    WaitOnQuitEvent(2);/*Waiting for some time before restarting the send.*/
                                }
                            } /* end of else */
                        }
                        else if ( (SOURCE_BITMAP == SourceOfData ) || ( SOURCE_META_DATA == SourceOfData ))
                        {
                            uliTotalChanges = Changes.GetTotalChanges();
                            uliTotalChangesPending =
                                Changes.GetTotalChangesPending();
                            uliChangesAlreadySent   = 0;
                            if ( Changes.Good() &&  !Changes.IsTimeStampsOnly() && !Changes.IsTagsOnly() )
                            {
                                while( (uliTotalChanges > 0 ) && ( !ShouldQuit()))
                                {
                                    m_pVolumeStream->Clear();
                                    ++m_continuationId;
                                    uliChangesToSend                = 0;
                                    SizeOfChangesToSend                             = 0;
                                    ulSizeOfChangesSent             = 0;

                                    TRANSPORT_OK = SV_SUCCESS;
                                    ComputeChangesToSend(Changes,
                                                         uliChangesAlreadySent,
                                                         uliChangesToSend,
                                                         SizeOfChangesToSend);

                                    if ( uliChangesToSend > uliChangesAlreadySent)
                                    {
                                        /* PR # 5926 : START */
                                        int iTranspStatus = SV_SUCCESS;
                                        /* PR # 5926 : END */
                                        SetRemoteName(preName, finalName, Changes, m_continuationId, Changes.IsContinuationEnd());
                                        if(m_transportProtocol == TRANSPORT_PROTOOL_MEMORY)
                                        {
                                            DebugPrintf(SV_LOG_DEBUG,"Protocol is memory stream so preName %s is final name %s\n",preName.c_str(),finalName.c_str());
                                            preName = finalName;
                                        }
                                        iTranspStatus = deletePrevFileIfReq(preName);

                                        /* PR # 5926 : START */
                                        bool bCanProcessMetaData = SetUpFileNamesForChecks(preName);
                                        if (bCanProcessMetaData == false)
                                        {
                                            iTranspStatus = SV_FAILURE;
                                            WaitOnQuitEvent( SECS_TO_WAITFOR_VOL ) ;
                                        }

                                        /* Check if got same name from driver */
                                        if( (Changes.m_bCheckDB) &&
                                            (!previousSentName.empty()) &&
                                            ( ( previousSentName.find( preName ) != string::npos) ||
                                              (preName.find( previousSentName )!= string::npos) ) )
                                        {
                                            DebugPrintf( SV_LOG_ERROR, "Dirty Block committed. But received the same file name from the driver. @LINE %d in FILE %s\n" , LINE_NO ,FILE_NAME) ;
                                            DebugPrintf( SV_LOG_ERROR, "previousSentName = %s\n", previousSentName.c_str() ) ;
                                            DebugPrintf( SV_LOG_ERROR, "preName = %s\n" , preName.c_str()) ;
                                            iTranspStatus = SV_FAILURE ;
                                            /* Wait for some time */
                                            WaitOnQuitEvent(SECS_TO_WAITFOR_VOL);
                                        }


                                        if (SV_SUCCESS == iTranspStatus)
                                        {
                                            /* PR # 5926 : END */
                                            uliChangeIndex = uliChangesAlreadySent;
                                            Changes.Reset(uliChangeIndex);
                                            // Send the offset / length and data
                                            TotalSizeOfBuffer = GetDiffFileSize(Changes,SizeOfChangesToSend);
                                            m_pTransportStream->SetSizeOfStream(TotalSizeOfBuffer);
                                            TRANSPORT_OK = SendFileHeaders(Changes,
                                                                           (uliChangesToSend-uliChangesAlreadySent),
                                                                           SizeOfChangesToSend,
                                                                           preName) ;
                                            if ( SV_SUCCESS == TRANSPORT_OK )
                                            {
                                                /** Added by BSR for source IO time stamps **/
                                                SV_ULONG timeDelta ;
                                                SV_ULONG sequenceDelta ;

                                                do
                                                {
                                                    Offset =    Changes.GetCurrentChangeOffset();
                                                    Length =    Changes.GetCurrentChangeLength();
                                                    DebugPrintf(SV_LOG_DEBUG,
                                                                "Volume: %s; Total: %lu; Current: %lu; Offset: " ULLSPEC"; Length: %lu\n",
                                                                GetSourceVolumeName().c_str(),
                                                                uliChangesToSend,
                                                                uliChangeIndex,
                                                                Offset,
                                                                Length);
                                                    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
                                                    {
                                                        timeDelta = Changes.GetCurrentChangeTimeDelta() ;
                                                        sequenceDelta = Changes.GetCurrentChangeSequenceDelta() ;
                                                        DebugPrintf( SV_LOG_DEBUG,
                                                                     "timeDelta: %lu, sequenceDelta:%lu\n",
                                                                     timeDelta,
                                                                     sequenceDelta ) ;
                                                    }
                                                    *m_pVolumeStream >> Changes;
                                                    if ( Length > 0 )
                                                    {
                                                        if ( ( !Changes.IsEmpty() ) && ( (*m_pVolumeStream).Good()) )
                                                        {
                                                            if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
                                                            {
                                                                ulSizeOfChangesSent += PREFIX_SIZE_V2 ;
                                                                TRANSPORT_OK = SendDirtyBlockInfoV2( Offset,
                                                                                                     Length,
                                                                                                     timeDelta,
                                                                                                     sequenceDelta,
                                                                                                     preName ) ;
                                                            }
                                                            else
                                                            {
                                                                ulSizeOfChangesSent += PREFIX_SIZE;
                                                                TRANSPORT_OK = SendDirtyBlockHeaderOffsetAndLength(
                                                                    Offset,
                                                                    Length,
                                                                    preName ) ;
                                                            }
                                                            if ( SV_SUCCESS == TRANSPORT_OK )
                                                            {
                                                                bool bMoreData = (*m_pVolumeStream).MoreData();
                                                                ulSizeOfChangesSent += Changes.Size();
                                                                if(IsCompressionEnabled())
                                                                {
                                                                    TRANSPORT_OK = m_pCompress->CompressStream(
                                                                        (unsigned char*) Changes.GetData(),
                                                                        Changes.Size());
                                                                    if(0 == TRANSPORT_OK)
                                                                    {
                                                                        m_pTransportStream->heartbeat();
                                                                        TRANSPORT_OK = SV_SUCCESS;
                                                                    }
                                                                }
                                                                else
                                                                {
                                                                    TRANSPORT_OK = TransferDataToCX(Changes.GetData(),
                                                                                                    Changes.Size(),
                                                                                                    preName,
                                                                                                    true);
                                                                    m_bProfileDiffsSize && (m_DiffSize+=Changes.Size());
                                                                }

                                                                while( (TRANSPORT_OK == SV_SUCCESS )
                                                                       && ( m_pVolumeStream->MoreData() )
                                                                       && ( m_pVolumeStream->Good()))
                                                                    // Watch out for the stop. If we are at the penultimate end of the changesToSend
                                                                    // and we exit then make sure uliChangeIndex does not get incremented.
                                                                    // Commented to Fix Bug #578
                                                                    //&& (false == m_ProtectEvent.IsSignalled() )

                                                                {
                                                                    *m_pVolumeStream >> Changes;
                                                                    if ( !Changes.IsEmpty() && m_pVolumeStream->Good())
                                                                    {
                                                                        ulSizeOfChangesSent += Changes.Size();
                                                                        if(IsCompressionEnabled())
                                                                        {
                                                                            TRANSPORT_OK = m_pCompress->CompressStream((unsigned char *) Changes.GetData(),
                                                                                                                       Changes.Size());
                                                                            if(0 == TRANSPORT_OK)
                                                                            {
                                                                                m_pTransportStream->heartbeat();
                                                                                TRANSPORT_OK = SV_SUCCESS;
                                                                            }
                                                                        }
                                                                        else
                                                                        {
                                                                            TRANSPORT_OK = TransferDataToCX(
                                                                                Changes.GetData(),
                                                                                Changes.Size(),
                                                                                preName,
                                                                                true);
                                                                            m_bProfileDiffsSize && (m_DiffSize+=Changes.Size());
                                                                        }
                                                                    }
                                                                    else
                                                                    {
                                                                        DebugPrintf(SV_LOG_ERROR,
                                                                                    "An Error occurred while getting dirty block disk change. @LINE %d in FILE %s.\n",
                                                                                    LINE_NO,
                                                                                    FILE_NAME);
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        Offset =    0;
                                                        Length =    0;

                                                        if (m_pVolumeStream->Bad() || m_pVolumeStream->Fail())
                                                        {
                                                            DebugPrintf(SV_LOG_ERROR,
                                                                        "Volume: %s Replicate() stream error. volume offline\?\n",
                                                                        GetSourceVolumeName().c_str());
                                                            int volOpenErr = -1;
                                                            std::string errmsg;
                                                            if (!CheckVolumeStatus(volOpenErr, errmsg) && (volOpenErr != ENOENT))
                                                            {
                                                                DebugPrintf(SV_LOG_ERROR, "%s: sending VolumeReadFailureAlert, Error: %s.\n", FUNCTION_NAME, errmsg.c_str());
                                                                MonitoringLib::SendAlertToRcm(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFS_DRAINING, VolumeReadFailureAlert(GetSourceVolumeName()), std::string(m_pHostId), GetSourceDiskId());
                                                            }
                                                            WAIT_ON_RETRY_DISK_READ_ERROR();/*Waiting for some time before restarting the send.*/
                                                            // read error most likely volume is offline
                                                            // either way stop processing these changes
                                                            // FIXME:
                                                            // probably want something better then this but this should work for now
                                                            // we don't want to continue with these changes,
                                                            uliTotalChanges = 0;

                                                            // we don't want to call FinishSendingData,
                                                            uliChangeIndex = 0;    // these 2 will prevent us from calling FinishSendingData
                                                            uliChangesToSend = 1;

                                                            // we don't want to commit these changes
                                                            uliChangesAlreadySent = 0; // this should prevent committing these changes

                                                            break;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        TRANSPORT_OK = SV_SUCCESS;
                                                    }

                                                    // Make sure the last send is fine.
                                                    // In addition the stream state reflects
                                                    // all changes are sent. if stream state is bad or
                                                    // failed we break in the above else.
                                                    if ( (TRANSPORT_OK == SV_SUCCESS ) &&
                                                         ( !m_pVolumeStream->MoreData() ) &&
                                                         (true == m_pVolumeStream->Good()))
                                                    {
                                                        ++uliChangeIndex;
                                                    }
                                                    else
                                                    {
                                                        break;
                                                    }

                                                }while ( ( uliChangeIndex < uliChangesToSend)                                   // more changes to process
                                                         && ( TRANSPORT_OK == SV_SUCCESS )
                                                         && ((*m_pVolumeStream).Good() || m_pVolumeStream->Eof())); // stream is good or EOF
                                                // Commented to Fix Bug #578
                                                //&& (false == m_ProtectEvent.IsSignalled() )                             // not signalled to quit
                                            }

                                            if ( (SV_SUCCESS == TRANSPORT_OK) &&
                                                 (uliChangeIndex >= uliChangesToSend ))
                                            {
                                                // succesfully sent all changes
                                        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
                                                {
                                                    TIME_STAMP_TAG_V2 timeStampInfo = ( ( UDIRTY_BLOCK_V2* ) Changes.GetDirtyBlock() )->uTagList.TagList.TagTimeStampOfLastChange ;
                                                    TRANSPORT_OK = SendTimeStampInfoV2( timeStampInfo,
                                                                                        LAST_CHANGE,
                                                                                        preName ) ;
                                                }
                                                else
                                                {
                                                    TIME_STAMP_TAG timeStampInfo = ( ( UDIRTY_BLOCK* ) Changes.GetDirtyBlock() )->uTagList.TagList.TagTimeStampOfLastChange ;
                                                    TRANSPORT_OK = SendTimeStampInfo( timeStampInfo,
                                                                                      LAST_CHANGE,
                                                                                      preName ) ;
                                                }

                                                if ( SV_SUCCESS == TRANSPORT_OK )
                                                {
                                                    if ( ulSizeOfChangesSent != SizeOfChangesToSend)
                                                    {
                                                        DebugPrintf(SV_LOG_SEVERE,"FAILED: Volume: %s. Size of Changes sent is not equal to Computed changes to send. Sent: %lu; Computed: %I64u. @LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),ulSizeOfChangesSent,SizeOfChangesToSend,LINE_NO,FILE_NAME);

                                                    }

                                                    if ( SV_SUCCESS == (TRANSPORT_OK = FinishSendingData(preName, finalName, Changes, m_continuationId, Changes.IsContinuationEnd())))//IsContinuationEnd(uliTotalChanges, uliChangesToSend, uliChangesAlreadySent)) )
                                                    {
                                                        uliTotalChanges -= (uliChangesToSend-uliChangesAlreadySent);
                                                        uliChangesAlreadySent = uliChangesToSend;

                                                        DebugPrintf(SV_LOG_DEBUG,"INFO: Volume: %s. Size of Data Transferred: %I64u and Volume Chunk Size: %ul bytes.\n",GetSourceVolumeName().c_str(),SizeOfChangesToSend,m_VolumeChunkSize);
                                                    }
                                                    else
                                                    {
                                                        DebugPrintf(SV_LOG_ERROR,"FAILED: Volume %s. Unable to complete dirty block transfer. Starting over again. @LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);

                                                    }
                                                }
                                                else
                                                {
                                                    DebugPrintf(SV_LOG_DEBUG,"FAILED: Volume: %s. Unable to send last timestamp change. Starting over again. @LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);

                                                }
                                            }
                                            else
                                            {
                                                DebugPrintf(SV_LOG_ERROR,"INFO:  Volume: %s. Change Index = %ld; Changes To Send = %ld. Transport failure, did not send all changes.@LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),uliChangeIndex,uliChangesToSend,LINE_NO,FILE_NAME);
                                            }
                                            /* PR # 5926 : START */
                                        } /* End of if (SV_SUCCESS == iTranspStatus) */
                                        /* PR # 5926 : END */
                                    } /* End of if ( uliChangesToSend > uliChangesAlreadySent) */

                                    if(uliChangesAlreadySent != uliChangesToSend)
                                    {
                                        --m_continuationId;
                                    }

                                    if ( !m_pVolumeStream->Good() )
                                    {
                                        /* PR # 6254 : START
                                         * Wait for success of CreateVolumeStream */
                                        do
                                        {
                                            /* Wait for some time */
                                            WaitOnQuitEvent(SECS_TO_WAITFOR_VOL);
                                            if (SV_SUCCESS == CreateVolumeStream())
                                            {
                                                break;
                                            }
                                        } while(!ShouldQuit());
                                        /* PR # 6254 : END */

                                        m_pTransportStream->SetNeedToDeletePreviousFile(true);
                                        break;
                                    }
                                    if ( TRANSPORT_OK == SV_FAILURE || !m_pTransportStream->Good())
                                    {
                                        HandleTransportFailure(Changes, SourceOfData);
                                        break; //Break the while loop so that we do a get db again.
                                    }
                                } // while

                                if ( ( 0 == uliTotalChanges )                                                                           // processed all changes
                                     && (uliChangesAlreadySent == Changes.GetTotalChanges() )                // sent all changes
                                     && ( Changes.IsTransactionComplete() )                                                  // all changes read from stream
                                     )
                                {
                                    DebugPrintf(SV_LOG_DEBUG,"INFO: Volume: %s. Committing %ld changes in dirty block\n",GetSourceVolumeName().c_str(),uliChangesAlreadySent);
                                    if (SV_SUCCESS == Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput))
                                    {
                                        ResetErrorCounters();
                                        if (m_bProfileDiffsSize) {
                                            m_TotalDiffsSize += m_DiffSize;
                                            m_TotalCompressedDiffsSize += m_CompressedDiffSize;
                                            PrintDiffsRate();
                                        }
                                        if (m_fpProfileLog)
                                        {
                                            m_TimeAfterCommit = ACE_OS::gettimeofday();
                                            Profile(preName, Changes);
                                        }
                                        if (m_bHasToCheckTS)
                                        {
                                            int recstatus = RecordPrevDirtyBlockDataAfterRename(Changes);
                                            if (SV_SUCCESS != recstatus)
                                            {
                                                DebugPrintf(SV_LOG_ERROR, "RecordPrevDirtyBlockDataAfterRename failed for volume %s @ LINE %d in FILE %s\n",
                                                    GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                                            }
                                        }
                                        previousSentName = preName;
                                        if (ShouldWaitForData(Changes))
                                        {
                                            WaitForData();
                                        }
                                    }
                                    else
                                    {
                                        DebugPrintf(SV_LOG_ERROR, "volume: %s, for bitmap/metadata mode, for file %s, commit failed\n",
                                            GetSourceVolumeName().c_str(),
                                            preName.c_str());
                                        DeleteSentDiffFile(finalName);
                                    }
                                }
                            }
                            else if ( Changes.Good() && Changes.IsTagsOnly() )
                            {
                                ProcessTagsOnly(Changes);
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_SEVERE,"FAILED: Unknown source of data. Cannot process dirty block.\n");
                            WAIT_ON_RETRY_DRIVER_ERROR();
                        }
                    } /* end of if (true == bCheckPassed) */
                }
                else if ( Changes.Good() && Changes.IsTimeStampsOnly() && (DATA_0 == dbStatus) )
                {

                     if (ReportIfResyncIsRequired(Changes) )
                    {
                        iStatus = Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput);
                        if (SV_SUCCESS == iStatus)
                        {
                            ResetErrorCounters();
                            DebugPrintf(SV_LOG_ERROR, "volume: %s, for ResyncRequired, commit succeeded\n",
                                GetSourceVolumeName().c_str());
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "volume: %s, for ResyncRequired, commit failed\n",
                                GetSourceVolumeName().c_str());
                        }
                        break;
                    }

                    bCheckPassed = m_bHasToCheckTS?VerifyPreviousEndTSAndSeq(Changes):true;
                    // No disk changes or tags. So just send timestamps wait for a while
                    // make sure to wait before sending so that if we just sent some changes
                    // and then got here, we don't generate this file potential in the same minute
                    // as the last change. files with the same time up to the minute cause a very
                    // slow sort to happen on the outpost side
                    if (bCheckPassed)
                    {
                        iStatus = ProcessTimeStampsOnly(Changes);
                        if(SV_SUCCESS != iStatus)
                        {
                            HandleTransportFailure(Changes, SOURCE_DATA);
                        }
                    }
                }
                else if ( !Changes.Good() && (DATA_NOTAVAILABLE == dbStatus ))
                {
                    DrvBusyRollingCount++;
                    DrvBusyRollingCount %= OCCURENCE_NO_FOR_DRVBUSY_ERRLOG;
                    SV_LOG_LEVEL sv = SV_LOG_WARNING;
                    if (1 == DrvBusyRollingCount) {
                        sv = SV_LOG_ERROR;
                    }
                    DebugPrintf(sv, "Involflt has returned ERR_BUSY for GetDB ioctl for volume %s. "
                                "Waiting on DB Notify event. This error repeated %d times.\n",
                                GetSourceVolumeName().c_str(), OCCURENCE_NO_FOR_DRVBUSY_ERRLOG);
                    
                    WaitForData();
                }
                else if ( !Changes.Good() && (DATA_RETRY == dbStatus ))
                {
                    DebugPrintf(SV_LOG_DEBUG, "For device %s filter driver requested get db retry. Waiting on DB Notifyevent.\n", GetSourceVolumeName().c_str());
                    WaitForData();
                }
                else if ( !Changes.Good() && (DATA_ERROR == dbStatus ))
                {
                    m_pVolumeStream->Seek(srcstartoffset, POS_BEGIN);
                    /* PR # 6254 : START
                     * 1. Corrected the flow for creating volume stream if bad
                     * 2. Passing correct number of bytes to read to Read */
                    const unsigned int MAX_SECTOR_SIZE = 4096;
                    SharedAlignedBuffer buffer(MAX_SECTOR_SIZE, MAX_SECTOR_SIZE);
                    do
                    {
                        if( m_pVolumeStream && (m_pVolumeStream->Good()) && (m_pVolumeStream->Read(buffer.Get(), MAX_SECTOR_SIZE) > 0))
                        {
                            DebugPrintf(SV_LOG_ERROR,
                                "Failed to retrieve dirty blocks from driver while volume %s is online.\n",
                                GetSourceVolumeName().c_str());

                            WAIT_ON_RETRY_DRIVER_ERROR();
                            break;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to retrieve dirty blocks from volume. Volume is offline: Waiting for 5 seconds before trying again. %s. @LINE %d in FILE %s.\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
                            int volOpenErr = -1;
                            std::string errmsg;
                            if (!CheckVolumeStatus(volOpenErr, errmsg) && (volOpenErr != ENOENT))
                            {
                                DebugPrintf(SV_LOG_ERROR, "%s: sending VolumeReadFailureAlert, Error: %s.\n", FUNCTION_NAME, errmsg.c_str());
                                MonitoringLib::SendAlertToRcm(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFS_DRAINING, VolumeReadFailureAlert(GetSourceVolumeName()), std::string(m_pHostId), GetSourceDiskId());
                            }
                            WAIT_ON_RETRY_DISK_READ_ERROR();
                            if (SV_SUCCESS == CreateVolumeStream())
                            {
                                /* Since this is in dowhile loop,
                                 * next time it will break from above if */
                                DebugPrintf(SV_LOG_DEBUG,"INFO: CreateVolumeStream succeeded. and will break out of while in next iteration @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                            }
                            else
                            {
                                /* Has to be in loop. */
                                DebugPrintf(SV_LOG_DEBUG,"INFO: CreateVolumeStream failed. @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
                            }
                        } /* End of else */
                        /* PR # 6254 : END */
                    }while(!ShouldQuit()); //dont try to begintrans. since the volume is offline, you have to create the volume.
                }
                /* PR # 6254 : START */
            } /* End of else */
            //Manage throttle health issue setting and resetting
            if (!m_bThrottled)
            {
                ResetThrottleHealthIssue();
            }
            /* PR # 6254 : END */
        } /* End of while( !IsStopRequested() ) */
    } /* End of if (IsInitialized()) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}
/*
 * FUNCTION NAME :  WaitForData
 *
 * DESCRIPTION :    If there is no data available for processing then the thread waits on the event
 *                  The driver signal this even when a dirty block is available for processing
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : Refer WAIT_STATE enum for possible return values
 *
 */
WAIT_STATE ProtectedGroupVolumes::WaitForData()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    if( m_WaitForDBNotify )
    {
        WAIT_STATE waitStatus;
        DebugPrintf(SV_LOG_DEBUG, "INFO: Waiting for data. Sleeping for %d seconds. @LINE %d in FILE %s\n", m_IdleWaitTime, LINE_NO, FILE_NAME);

        {
            // need a scoped lock as otherwise the lock is held across wait
            boost::unique_lock<boost::mutex> lock(m_DBNotifyMutex);

            if (IsStopRequested())
                return WAIT_SUCCESS;
        }
        // this is the window where the quit can be singalled and drain thread still
        // miss the quit as it auto-resets the DB Notify event before waiting
#ifdef SV_WINDOWS
        waitStatus = m_DBNotifyEvent.Wait(m_IdleWaitTime);
        if ( WAIT_TIMED_OUT == waitStatus)
        {
            DebugPrintf(SV_LOG_DEBUG,"INFO: Volume: %s. Wait Timed out.@LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
        }
        else if (WAIT_SUCCESS == waitStatus)
        {
            DebugPrintf(SV_LOG_DEBUG,"INFO: Volume: %s. Data has arrived. Continuing processing. @LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
        }
        else if ( WAIT_FAILURE == waitStatus)
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Volume %s. Wait on driver notification for data failed. @LINE %d in FILE %s.\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
        }
#elif SV_UNIX
        WAIT_FOR_DB_NOTIFY EventInfo;
        memset((void*)&EventInfo, 0, sizeof(WAIT_FOR_DB_NOTIFY));
        inm_strcpy_s((char *)EventInfo.VolumeGUID, NELEMS(EventInfo.VolumeGUID), m_SourceNameForDriverInput.c_str());
        EventInfo.Seconds = m_IdleWaitTime; //Seconds to wait.
        int iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_WAIT_FOR_DB, &EventInfo);
        if(SV_SUCCESS == iStatus)
        {
            if(ETIMEDOUT == m_pDeviceStream->GetErrorCode())
            {
                DebugPrintf(SV_LOG_DEBUG, "INFO: Volume %s. Wait timed out. @LINE %d in FILE %s \n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                waitStatus = WAIT_TIMED_OUT;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "INFO: Volume %s. Data has arrived. Continuing processing. @Line %d FILE %s\n", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                waitStatus = WAIT_SUCCESS;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Volume %s. Wait on driver notification for data failed. @LINE %d in FILE %s.\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
            waitStatus = WAIT_FAILURE;
        }
#endif
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
        return waitStatus;
    }

    assert(m_IdleWaitTime >= MIN_IDLE_WAIT_TIME);
    WaitOnQuitEvent(m_IdleWaitTime);
    return WAIT_TIMED_OUT;
}
/*
 * FUNCTION NAME :  DeleteInconsistentFileFromCX
 *
 * DESCRIPTION :    Deletes a stale / inconsistent file from the cx. First the onging upload is aborted and the file then deleted.
 *                  This can happen when tranport failures occur during the upload process.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS if succesfully deleted the inconsistent
 *                file from CX else returns SV_FAILURE
 *
 */
/* PR # 5926 : START
 * Added return type in this function for more error checking
 * PR # 5926 : END
 */
int ProtectedGroupVolumes::DeleteInconsistentFileFromCX(std::string const & fileName)
{
    /* PR # 5926 : START */
    int iStatus = SV_FAILURE;
    /* PR # 5926 : END */

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    if ( (m_pTransportStream ) && (m_pTransportStream->Good() ) )
    {
        if ( SV_SUCCESS == m_pTransportStream->Abort(fileName.c_str()) )
        {
            if ( ( m_pTransportStream->Good() ) && ( SV_SUCCESS == m_pTransportStream->DeleteFile(fileName) ) )
            {
                DebugPrintf(SV_LOG_WARNING,"Volume: %s. Deleted incomplete file %s.\n",GetSourceVolumeName().c_str(),fileName.c_str());
                iStatus = SV_SUCCESS ;
            } 
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Volume: %s. Failed to delete incomplete file %s.\n", GetSourceVolumeName().c_str(), fileName.c_str());
            }
        }
    }

    /* PR # 5926 : START */
    if (SV_FAILURE == iStatus)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to delete incomplete file %s on cx\n",fileName.c_str());
    }
    /* PR # 5926 : END */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  GetSourceVolumeName
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the name of the volume being protected as a std::string
 *
 */
const string& ProtectedGroupVolumes::GetSourceVolumeName() const
{
    return m_sProtectedVolumeName;
}

/*
* FUNCTION NAME :  GetSourceDiskId
*
* DESCRIPTION :
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :
*
* return value : returns the name of the volume being protected as a std::string
*
*/
const std::string& ProtectedGroupVolumes::GetSourceDiskId() const
{
    return m_sProtectedDiskId;
}


/*
* FUNCTION NAME :  GetSourceDeviceName
*
* DESCRIPTION :
*
* INPUT PARAMETERS :  NONE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :
*
* return value : returns the device name of the disk being protected as a std::string
*
*/
const std::string& ProtectedGroupVolumes::GetSourceDeviceName() const
{
    return m_sProtectedDiskDeviceName;
}


/*
 * FUNCTION NAME :  GetRawSourceVolumeName
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the name of the raw volume being protected as a std::string
 *
 */
const string& ProtectedGroupVolumes::GetRawSourceVolumeName() const
{
    return m_sRawProtectedVolumeName;
}


/*
 * FUNCTION NAME :  SendTimestampsAndTags
 *
 * DESCRIPTION :    Uploads on the timestamps and tags in the dirty block
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if upload is successful else false
 *
 */
bool ProtectedGroupVolumes::SendTimestampsAndTags(VolumeChangeData & changes, std::string &preName, std::string &finalName)
{
    DebugPrintf(SV_LOG_DEBUG, "ProtectedGroupVolumes::SendTimestampsOnly\n");

    // TSO is always continuation end
    SetRemoteName(preName, finalName, changes, TsoContinuationId, ContinuationEnd);
    if(m_transportProtocol == TRANSPORT_PROTOOL_MEMORY)
    {
        DebugPrintf(SV_LOG_DEBUG,"Protocol is memory stream so preName %s is final name %s\n",preName.c_str(),finalName.c_str());
        preName = finalName;
    }
    bool bCanProcessTSO = SetUpFileNamesForChecks(preName);
    if (!bCanProcessTSO)
    {
        DebugPrintf(SV_LOG_ERROR, "SetUpFileNamesForChecks failed for volume %s\n", GetSourceVolumeName().c_str());
        return false;
    }

    if( (changes.m_bCheckDB) &&
        (!previousSentName.empty()) &&
        (( previousSentName.find( preName ) != string::npos) ||
        (preName.find( previousSentName )!= string::npos) ) )
    {
        DebugPrintf( SV_LOG_ERROR,
        "Dirty Block committed. But received the same file name from the driver. @LINE %d in FILE %s\n",
        LINE_NO ,FILE_NAME) ;
        DebugPrintf( SV_LOG_ERROR, "previousSentName = %s\n preName = %s\n", previousSentName.c_str(), preName.c_str() ) ;
        WaitOnQuitEvent(SECS_TO_WAITFOR_VOL);
        return false;
    }

    // There are no disk changes to send. Therefore hardcode the second parameter as ZERO
    // Size of changes to send is zero therefore third parameter is also zero
    if ( SV_FAILURE == SendFileHeaders(changes,0,0,preName) )
        return false;

    // Send the timestamp of last change
    int iStatus ;
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
        TIME_STAMP_TAG_V2 timeStampInfo = ( ( UDIRTY_BLOCK_V2* ) changes.GetDirtyBlock() )->uTagList.TagList.TagTimeStampOfLastChange ;
        iStatus = SendTimeStampInfoV2( timeStampInfo,
                                       LAST_CHANGE,
                                       preName ) ;
    }
    else
    {
        TIME_STAMP_TAG timeStampInfo = ( ( UDIRTY_BLOCK* ) changes.GetDirtyBlock() )->uTagList.TagList.TagTimeStampOfLastChange ;
        iStatus = SendTimeStampInfo( timeStampInfo,
                                     LAST_CHANGE,
                                     preName ) ;
    }
    if (SV_FAILURE == iStatus )
    {
        return false;
    }

    if (SV_SUCCESS != FinishSendingData(preName, finalName, changes, TsoContinuationId, ContinuationEnd)) 
    {
        return false;
    }

    return true;
}

/*
 * FUNCTION NAME :  GetWaitForDBNotify
 *
 * DESCRIPTION :    Checks if sentinel should wait for data to arrive if there are no more dirty blocks
 *                  for processing
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
void ProtectedGroupVolumes::GetWaitForDBNotify()
{

    try {
        m_WaitForDBNotify = (1 == RcmConfigurator::getInstance()->getWaitForDBNotify());
    }
    catch(...) {
        m_WaitForDBNotify = true;
        DebugPrintf(SV_LOG_ERROR,"FAILED:Unable to get wait for driver notification event status. Setting it to false\n");
    }
}
/*
 * FUNCTION NAME :  TransferFileToCX
 *
 * DESCRIPTION :    Uploads the file to cx when in data file mode
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */

/* PR # 6387 : START
 * 1. Changed TransferFileToCX to take TargetFile as arguement
 * 2. Code to form TargetFile is moved in SetPreTargetDataFileName
 * PR # 6387 : END */

int
ProtectedGroupVolumes::TransferFileToCX(const std ::string & LocalFilename,
                                        const std ::string & TargetFile, 
                                        const std ::string & RenameFile, 
                                        const VolumeChangeData &Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    SVERROR se = SVS_OK;
    int iStatus = SV_SUCCESS;

    m_bProfileDiffsSize && (m_DiffSize=File::GetSizeOnDisk(LocalFilename));

    boost::shared_ptr<Checksums_t> checksums ;
    if( m_bDICheckEnabled )
        checksums.reset( new Checksums_t() ) ;
    else
        checksums.reset( ) ;

    bool bSvdCheckOnDataFile = SvdCheckOnDataFile(LocalFilename, checksums, Changes);

    if (true == bSvdCheckOnDataFile)
    {
        if ( m_pTransportStream->Good() )
        {
            ACE_Time_Value TimeBeforeTransport, TimeAfterTransport;
            m_fpProfileLog && (TimeBeforeTransport=ACE_OS::gettimeofday());
            iStatus = m_pTransportStream->Write(LocalFilename,TargetFile,m_compressMode, RenameFile, m_FinalPaths);
            m_fpProfileLog && (TimeAfterTransport=ACE_OS::gettimeofday()) && (m_TimeForTransport+=(TimeAfterTransport-TimeBeforeTransport));
            if (iStatus == SV_FAILURE)
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to send data file %s to CX. Its pre name: %s, final name: %s in "
                                         "final paths (%c separated) %s\n", LocalFilename.c_str(), TargetFile.c_str(),
                                         RenameFile.c_str(), FINALPATHS_SEP, m_FinalPaths.c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"sent data file %s to CX. Its pre name: %s, final name: %s in "
                                         "final paths (%c separated) %s\n", LocalFilename.c_str(), TargetFile.c_str(),
                                         RenameFile.c_str(), FINALPATHS_SEP, m_FinalPaths.c_str());
                /* Record the sent file */
                LogRenamedFilesInDataFileMode(RenameFile, checksums);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to send file.Corrupted Transport stream. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        iStatus = SV_FAILURE;
        WaitOnQuitEvent (SECS_TO_WAITFOR_VOL);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
/*
 * FUNCTION NAME :  GetIdleWaitTime
 *
 * DESCRIPTION :    Acquires the time to wait before requesting driver for dirty block,
 *                  If there are no dirty blocks for processings.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the number of seconds to wait as a on unsingned long
 *
 */
unsigned long ProtectedGroupVolumes::GetIdleWaitTime()
{
    //__asm int 3;
    unsigned long idleWaitTime = MAX_IDLE_WAIT_TIME;

    if(m_rpoThreshold != 0)
    {
        assert(m_rpoThreshold >= 0);
        idleWaitTime = (MIN_IDLE_WAIT_TIME + 2* m_rpoThreshold);
        if(idleWaitTime > MAX_IDLE_WAIT_TIME) idleWaitTime = MAX_IDLE_WAIT_TIME;
    }
    return idleWaitTime;
}
/*
 * FUNCTION NAME :  SvdCheck
 *
 * DESCRIPTION :    Invokes the svscheck utility to validate the file generated in data file mode.
 *                  This function is called only in debug builds and not in release builds
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
bool ProtectedGroupVolumes::SvdCheck(
                                     std::string const & fileName, 
                                     Checksums_t* checksums 
                                    )
{
    int result;
    bool bCheckSucceeded = true;

    /* second arguement verbose is set to false */
    result = ValidateSVFile (fileName.c_str(), NULL, NULL, checksums ) ;
    if (result != 1 )
    {
        bCheckSucceeded = false;
        DebugPrintf(SV_LOG_ERROR, "FAILED ProtectedGroupVolumes::SvdCheck %s did not pass check @ LINE %d in FILE %s\n", fileName.c_str(),LINE_NO,FILE_NAME);
    }
    return bCheckSucceeded;
}

/*
* FUNCTION NAME :  SvdCheck
*
* DESCRIPTION :    Invokes the svscheck utility to validate the file generated in data file mode.
*                  This function is called only in debug builds and not in release builds
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
bool ProtectedGroupVolumes::SvdCheck(
    char *buffer,
    const size_t    bufferlen,
    std::string const & fileName,
    Checksums_t* checksums
    )
{
    int result;
    bool bCheckSucceeded = true;
    stCmdLineAndPrintOpts_t clinepropts;
    clinepropts.m_bShouldPrint = true;
    clinepropts.m_bvboseEnabled = true;

    /* second arguement verbose is set to false */
    result = ValidateSVBuffer(buffer, bufferlen, fileName.c_str(), NULL, NULL, checksums);
    if (result != 1)
    {
        // rerun with verbose mode logging on failure
        result = ValidateSVBuffer(buffer, bufferlen, fileName.c_str(), &clinepropts, NULL, checksums);
        bCheckSucceeded = false;
    }
    return bCheckSucceeded;
}

/*
 * FUNCTION NAME :  IsTagSent
 *
 * DESCRIPTION :    Inspects if the tag if any in the current dirty block is sent.
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
const bool ProtectedGroupVolumes::IsTagSent() const
{
    return m_bTagSent;
}


/* PR # 6387 : START
 * Added this function to form the Pre TargetFileName in Data File mode */
/*
 * FUNCTION NAME :  SetPreTargetDataFileName
 *
 * DESCRIPTION :    Set the pre target data file name in data file mode.
 *
 * INPUT PARAMETERS :  LocalFilename is the local data filename
 *
 * OUTPUT PARAMETERS :  PreTargetDataFileName is set to the name of pre target
 *                      data file name.
 *
 * NOTES :
 *
 * return value : returns SV_SUCCESS if successfully created the pre target data
 *                file name else returns SV_FAILURE
 *
 */
int ProtectedGroupVolumes::SetPreTargetDataFileName(
                                                    VolumeChangeData& Changes,
                                                    const std::string &LocalFilename, 
                                                    std::string & PreTargetDataFileName,
                                                    std::string & RenameFile
                                                   )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,
                GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;
    
    string sVolName = this->GetSourceVolumeName();
    if ( IsDrive(sVolName))
        UnformatVolumeNameToDriveLetter(sVolName);
    else if ( IsMountPoint(sVolName))        
        sVolName = GetVolumeDirName(sVolName);
    PreTargetDataFileName = DestDir;
    PreTargetDataFileName += m_pHostId;
    PreTargetDataFileName += Slash;
    PreTargetDataFileName += sVolName;
    PreTargetDataFileName += Slash;
    PreTargetDataFileName += DiffDirectory;
    PreTargetDataFileName += SupportedFileName(Changes, LocalFilename);

    RenameFile = GetFinalNameFromPreName(PreTargetDataFileName);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}

/* PR # 6387 : END */

bool ProtectedGroupVolumes::UncompressIfNeeded(std::string& filePath)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"Inside %s %s\n",FUNCTION_NAME, filePath.c_str());
    do
    {
        GzipUncompress uncompresser;
        std::string uncompressedFileName = filePath;

        std::string::size_type idx = filePath.rfind(".gz");
        if (std::string::npos == idx || (filePath.length() - idx) > 3) 
        {
            rv = true;
            break; // uncompress not needed;
        }

        uncompressedFileName.erase(idx, uncompressedFileName.length());
        if(!uncompresser.UnCompress(filePath,uncompressedFileName))
        {
            std::ostringstream msg;
            msg << "Uncompress failed for "
                << filePath
                << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());

            // remove partial uncompressed file
            ACE_OS::unlink(getLongPathName(uncompressedFileName.c_str()).c_str());
            rv = false;
            break;
        } else
        {
            // remove the .gz
            ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str());
            filePath.erase(idx, filePath.length());
            rv = true;
            break;
        }

    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


void ProtectedGroupVolumes::InitializeMembersForS2Checks(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    CurrDiffFileName = "";
    prevSentTSFC = 0;
    prevSentTSLC = 0;
    prevSentSEQNOFC = 0;
    prevSentSEQNOLC = 0;
    prevSentSeqIDForSplitIO = 0;
    fpRenamedSuccessfully = NULL;
    m_fpChecksumFile = NULL;
    m_bS2ChecksEnabled = false;
    m_bDICheckEnabled = false;
    m_bHasToCheckTS = false;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}


int ProtectedGroupVolumes::ChecksInFinishSendingData(boost::shared_ptr<Checksums_t> &checksums, const VolumeChangeData &Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;

    /* Checks in Finish Sending data : START */
    if (m_bS2ChecksEnabled || m_bDICheckEnabled )
    {
        if (!CurrDiffFileName.empty())
        {
            std::string finalfiletorunsvdcheck = CurrDiffFileName;
            std::string copyofuncompressedfile;
            /* If compression is enabled, do gunzip -tv of compressed file */
            if (IsCompressionEnabled())
            {
                copyofuncompressedfile = m_pCompress->m_UnCompressedFileName;
                copyofuncompressedfile += ".copy";
                /* Check for the svdcheck of the uncompressed buffer lying with the compress stream */
                if (SvdCheck(m_pCompress->m_UnCompressedFileName, NULL))
                {
                    DebugPrintf(SV_LOG_DEBUG,
                                "svdcheck succeeded for %s before compression for volume %s @LINE %d in FILE %s.\n",
                                m_pCompress->m_UnCompressedFileName.c_str(), GetSourceVolumeName().c_str(),
                                LINE_NO, FILE_NAME);
                    // PR#10815: Long Path support
                    if(ACE_OS::rename(getLongPathName(m_pCompress->m_UnCompressedFileName.c_str()).c_str(),
                        getLongPathName(copyofuncompressedfile.c_str()).c_str()) < 0)
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to rename %s to %s, error no: %d\n",
                            m_pCompress->m_UnCompressedFileName.c_str(), copyofuncompressedfile.c_str(),
                            ACE_OS::last_error());
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "svdcheck failed for %s before compression for volume %s @ LINE %d in FILE %s\n"
                                "sleeping the process for file \n",
                                m_pCompress->m_UnCompressedFileName.c_str(), GetSourceVolumeName().c_str(),
                                LINE_NO, FILE_NAME );
                    iStatus = SV_FAILURE;
                    while (1)
                    {
                        ACE_OS::sleep(1000);
                    }
                }
                if(UncompressIfNeeded(CurrDiffFileName))
                {
                    DebugPrintf(SV_LOG_DEBUG,
                                "Uncompressing succeeded for %s for volume %s @ LINE %d in FILE %s.\n",
                                CurrDiffFileName.c_str(),
                                GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                    finalfiletorunsvdcheck = m_pCompress->m_UnCompressedFileName;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "Uncompressing failed for %s for volume %s @ LINE %d in FILE %s\n"
                                "sleeping the process for file \n",
                                CurrDiffFileName.c_str(),
                                GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME );
                    iStatus = SV_FAILURE;
                    while (1)
                    {
                        ACE_OS::sleep(1000);
                    }
                }
            }
            if (SvdCheck(finalfiletorunsvdcheck, checksums.get()))
            {
                DebugPrintf(SV_LOG_DEBUG,
                            "svdcheck succeeded for %s %s for volume %s @ LINE %d in FILE %s.\n",
                            finalfiletorunsvdcheck.c_str(),
                            (IsCompressionEnabled()? "after uncompression" : "when no compression enabled"),
                            GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
                /* returns success : Remove the file */
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(finalfiletorunsvdcheck.c_str()).c_str());
                CurrDiffFileName = "";
                if (IsCompressionEnabled())
                {
                    /* Remove the uncompressed buffer if compression enabled */
                    // PR#10815: Long Path support
                    ACE_OS::unlink(getLongPathName(copyofuncompressedfile.c_str()).c_str());
                    m_pCompress->m_UnCompressedFileName = "";
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                            "svdcheck failed for %s %s for volume %s @ LINE %d in FILE %s\n sleeping for the file \n",
                            finalfiletorunsvdcheck.c_str(),
                            (IsCompressionEnabled()? "after uncompression" : "when no compression enabled"),
                            GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME );
                iStatus = SV_FAILURE;
                while (1)
                {
                    ACE_OS::sleep(1000);
                }
            }
        }
    } /* End of if (m_bS2ChecksEnabled) || (m_bDICheckEnabled) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}


int ProtectedGroupVolumes::RecordPrevDirtyBlockDataAfterRename( const VolumeChangeData & changeData )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;

    prevSentSeqIDForSplitIO = changeData.GetSequenceIdForSplitIO();
    prevSentTSLC = tsAndSeqnosFile.lastTimeStamp;
    prevSentSEQNOLC = tsAndSeqnosFile.lastSequenceNumber;
    prevSentTSFC = tsAndSeqnosFile.startTimeStamp;
    prevSentSEQNOFC = tsAndSeqnosFile.startSequenceNumber;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iStatus;
}
 

bool ProtectedGroupVolumes::VerifyPreviousEndTSAndSeq( const VolumeChangeData & Changes )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    bool bCheckPassed = true;
    SV_ULONGLONG EndTSOfPrevDiff = 0;
    SV_ULONGLONG EndSeqOfPrevDiff = 0;
    SV_ULONG SeqIDForSplitIOOfPrevDiff = 0; 
    const SV_ULONGLONG ullMax = MAX_ULL_VALUE;
    /**
    * For sequence ID for split IO, the issue is that it is a ulong
    * in windows and 32 bit quantity in unix, we are trying to take
    * every thing in terms of ulong, and assumption is that max ulong
    * value in windows is 2^32 - 1
    */
    const SV_ULONG ulMax32BitUVal = MAX_FOURBYTE_VALUE;
    SV_ULONGLONG ullSeqMaxValToCmp = ullMax;

    if (!m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
    {
        ullSeqMaxValToCmp = MAX_FOURBYTE_VALUE;
    }

    if (m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
        EndTSOfPrevDiff = Changes.GetPrevEndTS();
        EndSeqOfPrevDiff = Changes.GetPrevEndSeqNo();
        SeqIDForSplitIOOfPrevDiff = Changes.GetPrevSeqIDForSplitIO();

        if( Changes.m_bCheckDB )
        {
            if (EndTSOfPrevDiff != prevSentTSLC)
            {
                if (ullMax != EndTSOfPrevDiff)
                {
                    DebugPrintf(SV_LOG_ERROR, "end timestamp of previous diff from dirty block = " ULLSPEC 
                                              ", end timestamp of previous sent diff to CX = " ULLSPEC 
                                              ", are not equal for volume %s. Cannot process further\n", 
                                              EndTSOfPrevDiff, prevSentTSLC, GetSourceVolumeName().c_str());
                    bCheckPassed = false;
                }
            }
            if (EndSeqOfPrevDiff != prevSentSEQNOLC)
            {
                if (ullSeqMaxValToCmp != EndSeqOfPrevDiff)
                {
                    DebugPrintf(SV_LOG_ERROR, "end sequence of previous diff from dirty block = " ULLSPEC 
                                              ", end sequence of previous sent diff to CX = " ULLSPEC 
                                              ", are not equal for volume %s. Cannot process further\n", 
                                              EndSeqOfPrevDiff, prevSentSEQNOLC, GetSourceVolumeName().c_str());
                    bCheckPassed = false;
                }
            }
            if (SeqIDForSplitIOOfPrevDiff != prevSentSeqIDForSplitIO)
            { 
                if (ulMax32BitUVal != SeqIDForSplitIOOfPrevDiff)
                {
                    DebugPrintf(SV_LOG_ERROR, "sequence ID for split IO of previous diff from dirty block = %lu"
                                              ", sequence ID for split IO of previous sent diff to CX = %lu"
                                              ", are not equal for volume %s. Cannot process further\n", 
                                              SeqIDForSplitIOOfPrevDiff, prevSentSeqIDForSplitIO, GetSourceVolumeName().c_str());
                    bCheckPassed = false;
                }
            }
        }
    }

    if (bCheckPassed)
    {
        bCheckPassed = OutOfOrderAndSameFileNameChecks(Changes);
    }

    if (!bCheckPassed)
    {
        WAIT_ON_RETRY_DRIVER_ERROR();
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return bCheckPassed;
}


bool ProtectedGroupVolumes::OutOfOrderAndSameFileNameChecks( const VolumeChangeData & Changes )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    bool bCheckPassed = true;

    std::stringstream logTsSeq;

    tsAndSeqnosFile.zeroAll();
    if ((SOURCE_FILE == Changes.GetDataSource()) && (!Changes.IsTimeStampsOnly()))
    {
        const std::string LocalFile = Changes.GetFileName();
        if (E_DIFF != ParseFileName(LocalFile.c_str(), &tsAndSeqnosFile))
        {
            DebugPrintf(SV_LOG_ERROR, "data file %s for volume %s is not in differential file format\n", LocalFile.c_str(),
                                      GetSourceVolumeName().c_str());
            bCheckPassed = false;
        }
    }
    else
    {
        tsAndSeqnosFile.startTimeStamp = Changes.GetFirstChangeTimeStamp() ;
        tsAndSeqnosFile.lastTimeStamp = Changes.GetLastChangeTimeStamp() ;
        if( !m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {
            tsAndSeqnosFile.startSequenceNumber = Changes.GetFirstChangeSequenceNumber() ;
            tsAndSeqnosFile.lastSequenceNumber = Changes.GetLastChangeSequenceNumber() ;        
        }
        else
        {
            tsAndSeqnosFile.startSequenceNumber = Changes.GetFirstChangeSequenceNumberV2() ;
            tsAndSeqnosFile.lastSequenceNumber = Changes.GetLastChangeSequenceNumberV2() ;
        }

        logTsSeq << "DeviceId: " << GetSourceVolumeName()
            << ", CheckDB: " << Changes.m_bCheckDB
            << ", Num changes: " << Changes.GetTotalChanges()
            << ", Pending changes: " << Changes.GetTotalChangesPending()
            << ", Type: " << (Changes.IsTagsOnly() ? "Tag" : (Changes.IsTimeStampsOnly() ? "TSO" : "IO"))
            << ", Data Source: " << STRDATASOURCE[Changes.GetDataSource()]
            << ", Write Order State: " << Changes.IsWriteOrderStateData()
            << "\n, Previous File Start Time: " << prevSentTSFC
            << ", Previous File First Seq No: " << prevSentSEQNOFC
            << ", Previous File End Time: " << prevSentTSLC
            << ", Previous File End SeqNo: " << prevSentSEQNOLC
            << "\n, Current Start Time: " << tsAndSeqnosFile.startTimeStamp
            << ", Current First Seq No: " << tsAndSeqnosFile.startSequenceNumber
            << ", Current End Time: " << tsAndSeqnosFile.lastTimeStamp
            << ", Current End Seq No: " << tsAndSeqnosFile.lastSequenceNumber
            << ", Current Split IO Sequence Id: " << Changes.GetSequenceIdForSplitIO()
            << "\n, Previous End Time: " << Changes.GetPrevEndTS()
            << ", Previous End SeqNo: " << Changes.GetPrevEndSeqNo()
            << ", Previous Split IO Sequence Id:" << Changes.GetPrevSeqIDForSplitIO();

        bCheckPassed = (tsAndSeqnosFile.lastTimeStamp >= tsAndSeqnosFile.startTimeStamp) &&
            (tsAndSeqnosFile.lastSequenceNumber >= tsAndSeqnosFile.startSequenceNumber);

        if (!bCheckPassed)
        {
            DebugPrintf(SV_LOG_ERROR, "Differential file has decreasing timestamps. Not processing.\n%s\n",
                logTsSeq.str().c_str());
        }
        else
        {
            if (!Changes.IsTimeStampsOnly() && !Changes.IsTagsOnly())
            {
                if (Changes.IsWriteOrderStateData() && (Changes.GetDataSource() != SOURCE_STREAM))
                {
                    bCheckPassed = false;
                    DebugPrintf(SV_LOG_ERROR, "Differential file in write order state has incorrect data source. Not processing.\n%s\n",
                        logTsSeq.str().c_str());
                }
            }
        }
    }
  
    if( bCheckPassed && (prevSentTSLC || Changes.m_bCheckDB) )
    {
        if( ( Changes.IsContinuationEnd() == false  && 
              Changes.GetSequenceIdForSplitIO() > 1 ) 
            || 
            ( Changes.IsContinuationEnd() == true && 
              Changes.GetSequenceIdForSplitIO() > 1 ) 
          )
        {
            //for split io
            if( tsAndSeqnosFile.startTimeStamp != prevSentTSFC ||
                tsAndSeqnosFile.lastTimeStamp != prevSentTSLC ||
                tsAndSeqnosFile.startSequenceNumber != prevSentSEQNOFC ||
                tsAndSeqnosFile.lastSequenceNumber != prevSentSEQNOLC )
            {
                DebugPrintf( SV_LOG_ERROR, "In Split IO Timestamps are not same as previous values for volume %s\n", 
                             GetSourceVolumeName().c_str());
                bCheckPassed = false ;
            }
        }	
        else if ( Changes.IsContinuationEnd() == true || 
                  ( Changes.IsContinuationEnd() == false && 
                    Changes.GetSequenceIdForSplitIO() == 1 )
                )
        {
            if( tsAndSeqnosFile.startTimeStamp < prevSentTSLC ||
                tsAndSeqnosFile.lastTimeStamp < tsAndSeqnosFile.startTimeStamp )
            {
                bCheckPassed = false;
            }

            /* Compare sequence number only if it is dirty block Structure V2 */
            if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
            { 
                if ( tsAndSeqnosFile.startSequenceNumber <= prevSentSEQNOLC ||
                      tsAndSeqnosFile.lastSequenceNumber < tsAndSeqnosFile.startSequenceNumber )
                {
                    bCheckPassed = false ;
                }
            }
        }

        if( bCheckPassed == false )
        {
            DebugPrintf( SV_LOG_ERROR, "Out of Order Differential file is being observed for volume %s. Not processing.\n"
                                       "Values as Follows\n"
                                       "Previous Start Time:" ULLSPEC "\t Current Start Time:" ULLSPEC "\n"
                                       "Previous First Seq No:" ULLSPEC "\t Current First Seq No:" ULLSPEC "\n"
                                       "Previous End Time:" ULLSPEC "\tCurrent End Time:" ULLSPEC "\n"
                                       "Previous End SeqNo:" ULLSPEC "\t Current End Seq No:" ULLSPEC "\n",
                                       GetSourceVolumeName().c_str(),
                                       prevSentTSFC, tsAndSeqnosFile.startTimeStamp,
                                       prevSentSEQNOFC, tsAndSeqnosFile.startSequenceNumber,
                                       prevSentTSLC, tsAndSeqnosFile.lastTimeStamp,
                                       prevSentSEQNOLC, tsAndSeqnosFile.lastSequenceNumber ) ;
        }
    } /* end of if (Changes.m_bCheckDB) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return bCheckPassed;
}


bool ProtectedGroupVolumes::SetUpFileNamesForChecks( const std::string & preName )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    bool bCanProceed = true;

    if (m_bS2ChecksEnabled || m_bDICheckEnabled)
    {
        string sVolName = this->GetSourceVolumeName();

        if ( IsDrive(sVolName))
            UnformatVolumeNameToDriveLetter(sVolName);
        else if ( IsMountPoint(sVolName))
            sVolName = GetVolumeDirName(sVolName);

#ifdef SV_UNIX
        sVolName += "/";
#else
        std::replace( sVolName.begin(), sVolName.end(), '/',  '\\' );
        sVolName += "\\";
#endif

#ifdef SV_UNIX
        CurrDiffFileName = "/tmp";
        if ('/' != sVolName[0])
        {
            CurrDiffFileName += '/';
        }
#else
        CurrDiffFileName = RcmConfigurator::getInstance()->getInstallPath();
        if ('\\' != CurrDiffFileName[CurrDiffFileName.length() - 1])
        {
            CurrDiffFileName += '\\';
        }
        CurrDiffFileName += "tmp\\";
#endif

        CurrDiffFileName += sVolName;

        SVMakeSureDirectoryPathExists( CurrDiffFileName.c_str() ) ;
        CurrDiffFileName += basename_r(preName.c_str(), '/');

        std::ios::openmode mode = std::ios :: out | std::ios :: binary | std::ios :: trunc;
        // PR#10815: Long Path support
        std::ofstream localofstream(getLongPathName(CurrDiffFileName.c_str()).c_str(),mode);
        if(!localofstream.is_open())
        {
            DebugPrintf( SV_LOG_ERROR, "open for %s failed. @LINE %d in FILE %s\n", CurrDiffFileName.c_str(), LINE_NO ,FILE_NAME);
            bCanProceed = false;
        }

        if (bCanProceed)
        {
            localofstream.close();
            /* If compression is enabled, assign the removed *.gz to compress stream */
            if( IsCompressionEnabled())
            {
                std::string PreFileBaseName = basename_r(preName.c_str(), '/');
#ifdef SV_UNIX
                m_pCompress->m_UnCompressedFileName = "/tmp";
                if ('/' != sVolName[0])
                {
                    m_pCompress->m_UnCompressedFileName += '/';
                }
#else
                m_pCompress->m_UnCompressedFileName = RcmConfigurator::getInstance()->getInstallPath();
                if ('\\' != m_pCompress->m_UnCompressedFileName[m_pCompress->m_UnCompressedFileName.length() - 1])
                {
                    m_pCompress->m_UnCompressedFileName += "\\";
                }
                m_pCompress->m_UnCompressedFileName += "tmp\\";
#endif

                m_pCompress->m_UnCompressedFileName += sVolName;

                SVMakeSureDirectoryPathExists( m_pCompress->m_UnCompressedFileName.c_str() ) ;
                std::string::size_type idx = PreFileBaseName.rfind(".gz");
                m_pCompress->m_UnCompressedFileName.append(PreFileBaseName, 0, idx);

                std::ios ::openmode mode = std::ios :: out | std::ios :: binary | std::ios :: trunc;
                // PR#10815: Long Path support
                std::ofstream localuncompressofstream(getLongPathName(m_pCompress->m_UnCompressedFileName.c_str()).c_str(),mode);
                if(!localuncompressofstream.is_open())
                {
                    DebugPrintf( SV_LOG_ERROR, "open for %s failed. @LINE %d in FILE %s\n", m_pCompress->m_UnCompressedFileName.c_str(), LINE_NO ,
                                 FILE_NAME);
                    bCanProceed = false;
                }

                if (bCanProceed == true)
                {
                    localuncompressofstream.close();
                }
            }
        }
    } /* End of if (m_bS2ChecksEnabled) || m_bDICheckEnabled */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return bCanProceed;
}


void ProtectedGroupVolumes::CloseFilesForChecks(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    if (m_bS2ChecksEnabled || m_bDICheckEnabled)
    {
        if (NULL != fpRenamedSuccessfully)
        {
            fclose(fpRenamedSuccessfully);
            fpRenamedSuccessfully = NULL;
        }

        if (NULL != m_fpChecksumFile)
        {
            fclose(m_fpChecksumFile);
            m_fpChecksumFile = NULL;
        }

        if (!CurrDiffFileName.empty())
        {
            CurrDiffFileName = "";
        }
    } /* End of if (m_bS2ChecksEnabled) || m_bDICheckEnabled */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}


bool ProtectedGroupVolumes::WriteToChkFileInTransferDataToCX(const char* sData,const long int uliDataLen)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    bool bRetVal = true;

    if (m_bS2ChecksEnabled || m_bDICheckEnabled)
    {
        if ( !CurrDiffFileName.empty() )
        {
            //Added ios :: ate for tellp file comparion checks
            std::ios::openmode mode = std::ios::out | std::ios::binary | std::ios::app | std::ios::ate;
            // PR#10815: Long Path support
            std::ofstream localofstream(getLongPathName(CurrDiffFileName.c_str()).c_str(),mode);

            if(!localofstream.is_open())
            {
                DebugPrintf(SV_LOG_ERROR, "In Function %s @ LINE %d in FILE %s, failed to open localofstream\n", FUNCTION_NAME, LINE_NO, FILE_NAME );
                bRetVal = false;
            }
            if (true == bRetVal)
            {
                std::streamsize firstPointer = localofstream.tellp() ;

                if (IsCompressionEnabled())
                {
                    if ( 0 != firstPointer )
                    {
                        DebugPrintf(SV_LOG_ERROR,
                                    "In Function %s @ LINE %d in FILE %s, firstPointer = %d which is not 0 for volume %s before write."
                                    "going ahead and doing write for file %s with stream state = %d\n",
                                    FUNCTION_NAME, LINE_NO, FILE_NAME, firstPointer,
                                    GetSourceVolumeName().c_str(),
                                    CurrDiffFileName.c_str(), localofstream.good());
                    }
                }

                /* DebugPrintf(SV_LOG_ERROR,
                   "In function %s, @ LINE %d in FILE %s, sData = %p, uliDataLen = %ld, firstPointer = %d "
                   "when %s before call to write for volume %s. This is not an error.\n",
                   FUNCTION_NAME, LINE_NO, FILE_NAME, sData, uliDataLen, firstPointer,
                   (IsCompressionEnabled()? "compression enabled" : "no compression"), GetSourceVolumeName().c_str());*/

                localofstream.write( sData,(std::streamsize) uliDataLen );
                if(!localofstream)
                {
                    DebugPrintf(SV_LOG_ERROR, "localofstream.write failed @ LINE %d in FILE %s \n", LINE_NO, FILE_NAME );
                    localofstream.close();
                    bRetVal = false;
                }
                if (true == bRetVal)
                {
                    std::streamsize secondPointer = localofstream.tellp() ;
                    bool bStreamState = localofstream.good();

                    if( (secondPointer - firstPointer) != ( std::streamsize ) uliDataLen )
                    {
                        DebugPrintf( SV_LOG_ERROR, "In function %s @ LINE %d in FILE %s, (secondPointer = %d - firstPointer = %d) != uliDataLen = %ld."
                                     "The state of localofstream is %d\n", FUNCTION_NAME, LINE_NO, FILE_NAME, secondPointer, firstPointer,
                                     uliDataLen, bStreamState) ;
                    }
                    localofstream.close();

                    if (IsCompressionEnabled())
                    {
                        if ( uliDataLen != secondPointer )
                        {
                            DebugPrintf(SV_LOG_ERROR,
                                        "In Function %s @ LINE %d in FILE %s, secondPointer = %d which is not equal to uliDataLen = %ld "
                                        "for volume %s with stream state = %d. going ahead with the process \n",
                                        FUNCTION_NAME, LINE_NO, FILE_NAME, secondPointer, uliDataLen,
                                        GetSourceVolumeName().c_str(), bStreamState);
                        }
                    }

                    /* DebugPrintf(SV_LOG_ERROR,
                       "In function %s, @ LINE %d in FILE %s, sData = %p, uliDataLen = %ld, firstPointer = %d"
                       "secondPointer = %d, secondPointer - firstPointer = %d when %s after write for volume %s. This is not an error.\n",
                       FUNCTION_NAME, LINE_NO, FILE_NAME, sData, uliDataLen, firstPointer,
                       secondPointer, (secondPointer - firstPointer),
                       (IsCompressionEnabled()? "compression enabled" : "no compression"), GetSourceVolumeName().c_str());*/
                }
            }
        } /* End of if (!CurrDiffFileName) */
    } /* End of if (m_bS2ChecksEnabled) || m_bDICheckEnabled */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return bRetVal;
}


void ProtectedGroupVolumes::SetUpLogForAllRenamedFiles(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    if (m_bS2ChecksEnabled || m_bDICheckEnabled)
    {
        std::string FilePreAndCompDiffFiles;
        std::stringstream ss_pid;
        ss_pid << ACE_OS::getpid();
#ifdef SV_WINDOWS
        FilePreAndCompDiffFiles = RcmConfigurator::getInstance()->getInstallPath();
        if ('\\' != FilePreAndCompDiffFiles[FilePreAndCompDiffFiles.length() - 1])
        {
            FilePreAndCompDiffFiles += '\\';
        }
        FilePreAndCompDiffFiles += "tmp\\";
#else
        FilePreAndCompDiffFiles = "/tmp/";
#endif
        FilePreAndCompDiffFiles += "drained_files";
        FilePreAndCompDiffFiles += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        FilePreAndCompDiffFiles += ss_pid.str();
        FilePreAndCompDiffFiles += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        SVMakeSureDirectoryPathExists( FilePreAndCompDiffFiles.c_str() ) ;

        std::string srcvolnameexcludingslash = GetSourceVolumeName();
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ':' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '/' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '\\' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ' ' , '_');

        FilePreAndCompDiffFiles += srcvolnameexcludingslash;
        FilePreAndCompDiffFiles += ".dat";
        DebugPrintf(SV_LOG_DEBUG, "The drained file full path formed is %s.\n", FilePreAndCompDiffFiles.c_str());

#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)
        // PR#10815: Long Path support
        fpRenamedSuccessfully = _wfopen(getLongPathName(FilePreAndCompDiffFiles.c_str()).c_str(), ACE_TEXT("w"));
#else
        fpRenamedSuccessfully = fopen(FilePreAndCompDiffFiles.c_str(), "w");
#endif

        if (NULL == fpRenamedSuccessfully)
        {
            DebugPrintf(SV_LOG_ERROR, "Could not fopen the file %s @LINE %d in FILE %s\n", FilePreAndCompDiffFiles.c_str(),
                        LINE_NO, FILE_NAME);
        }
        else
        {
            fprintf(fpRenamedSuccessfully, "Final paths (%c separated): %s\n", FINALPATHS_SEP, m_FinalPaths.c_str());
        }
    } /* End of if (m_bS2ChecksEnabled) || m_bDICheckEnabled */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}

void ProtectedGroupVolumes::SetUpProfilerLog(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    if (RcmConfigurator::getInstance()->getProfileDiffs())
    {
        std::string ProfilerLog;
#ifdef SV_WINDOWS
        ProfilerLog = RcmConfigurator::getInstance()->getInstallPath();
        if ('\\' != ProfilerLog[ProfilerLog.length() - 1])
        {
            ProfilerLog += '\\';
        }
        ProfilerLog += "tmp\\";
#else
        ProfilerLog = "/tmp/";
#endif
        ProfilerLog += "diffs_profile";
        ProfilerLog += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        SVMakeSureDirectoryPathExists( ProfilerLog.c_str() ) ;

        std::string srcvolnameexcludingslash = GetSourceVolumeName();
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ':' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '/' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '\\' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ' ' , '_');

        ProfilerLog += srcvolnameexcludingslash;
        ProfilerLog += ".dat";
        DebugPrintf(SV_LOG_DEBUG, "The profiler file full path formed is %s.\n", ProfilerLog.c_str());

#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)
        // PR#10815: Long Path support
        m_fpProfileLog = _wfopen(getLongPathName(ProfilerLog.c_str()).c_str(), ACE_TEXT("a"));
#else
        m_fpProfileLog = fopen(ProfilerLog.c_str(), "a");
#endif

        if (NULL == m_fpProfileLog)
        {
            DebugPrintf(SV_LOG_ERROR, "Could not fopen the file %s @LINE %d in FILE %s\n", ProfilerLog.c_str(),
                        LINE_NO, FILE_NAME);
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}


void ProtectedGroupVolumes::SetUpDiffsRateProfilerLog(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    std::string profiledifferentialrateoption = RcmConfigurator::getInstance()->ProfileDifferentialRate();
    if ((InmStrCmp<NoCaseCharCmp>(PROFILE_DIFFERENTIAL_RATE_OPTION_YES, profiledifferentialrateoption) == 0) || 
        (InmStrCmp<NoCaseCharCmp>(PROFILE_DIFFERENTIAL_RATE_OPTION_WITH_COMPRESSION, profiledifferentialrateoption) == 0))
    {
        std::string DiffsRateProfilerLog;
#ifdef SV_WINDOWS
        DiffsRateProfilerLog = RcmConfigurator::getInstance()->getInstallPath();
        if ('\\' != DiffsRateProfilerLog[DiffsRateProfilerLog.length() - 1])
        {
            DiffsRateProfilerLog += '\\';
        }
        DiffsRateProfilerLog += "tmp\\";
#else
        DiffsRateProfilerLog = "/tmp/";
#endif
        DiffsRateProfilerLog += "differential_rate";
        DiffsRateProfilerLog += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        SVMakeSureDirectoryPathExists( DiffsRateProfilerLog.c_str() ) ;

        std::string srcvolnameexcludingslash = GetSourceVolumeName();
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ':' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '/' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '\\' , '_');
        replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ' ' , '_');

        DiffsRateProfilerLog += srcvolnameexcludingslash;
        DiffsRateProfilerLog += ".dat";
        DebugPrintf(SV_LOG_DEBUG, "The diffs rate profiler file full path formed is %s.\n", DiffsRateProfilerLog.c_str());

#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)
        // PR#10815: Long Path support
        m_fpDiffsRateProfilerLog = _wfopen(getLongPathName(DiffsRateProfilerLog.c_str()).c_str(), ACE_TEXT("a"));
#else
        m_fpDiffsRateProfilerLog = fopen(DiffsRateProfilerLog.c_str(), "a");
#endif

        if (NULL == m_fpDiffsRateProfilerLog)
        {
            DebugPrintf(SV_LOG_ERROR, "Could not fopen the file %s @LINE %d in FILE %s\n", DiffsRateProfilerLog.c_str(),
                        LINE_NO, FILE_NAME);
        }
        else 
        {
            fprintf(m_fpDiffsRateProfilerLog, "*************************** DRAIN SESSION START *******************************\n");
            fprintf(m_fpDiffsRateProfilerLog, "Time,Total differentials in bytes,Total compressed differentials in bytes\n");
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}

void ProtectedGroupVolumes::LogRenamedFilesInDataFileMode(const std::string &RenameFile,
                                                          boost::shared_ptr<Checksums_t> &checksums )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    if (m_bS2ChecksEnabled || m_bDICheckEnabled)
    {
        /* Log the preName and completed name */
        if (fpRenamedSuccessfully)
        {
            fprintf(fpRenamedSuccessfully, "In DATA FILE MODE : START *************\n");
            fprintf(fpRenamedSuccessfully, "%s\n", RenameFile.c_str());
            fprintf(fpRenamedSuccessfully, "In DATA FILE MODE : END *************\n");
            fflush(fpRenamedSuccessfully);
        }

        if ((NULL != checksums.get()) && m_fpChecksumFile && m_bDICheckEnabled)
        {
            persistChecksums( *(checksums.get()), RenameFile ) ;
        }

    } /* End of if (m_bS2ChecksEnabled) || m_bDICheckEnabled */
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}

bool ProtectedGroupVolumes::SvdCheckOnDataFile(const std::string &LocalFilename,
                                               boost::shared_ptr<Checksums_t> &checksums, 
                                               const VolumeChangeData &Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    bool bRetVal = true;

    if ( m_bS2ChecksEnabled || m_bDICheckEnabled )
    {
        /* Check for the svdcheck on the local file name */
        bool bDoOutOfOrderChecks = true;
        if (SvdCheck(LocalFilename, checksums.get()) )
        {
            /* Never do a remove of localfile here since this file only will go to CX */
            DebugPrintf(SV_LOG_DEBUG,
                        "svdcheck succeeded for local file %s in data file mode for volume name: %s @LINE %d in FILE %s"
                        "\n",
                        LocalFilename.c_str(), GetSourceVolumeName().c_str() , LINE_NO, FILE_NAME);
        }
        else
        {
            FILE *fp_localfile;

#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)
            // PR#10815: Long Path support
            fp_localfile = _wfopen(getLongPathName(LocalFilename.c_str()).c_str(), ACE_TEXT("rb"));
#else
            fp_localfile = fopen(LocalFilename.c_str(), "rb");
#endif

            if (NULL == fp_localfile)
            {
                DebugPrintf(SV_LOG_ERROR,
                            "@ LINE %d in FILE %s, a local file in data file mode cannot be opened for svdcheck\n",
                            LINE_NO, FILE_NAME);
            }
            else
            {
                fclose(fp_localfile);
                DebugPrintf(SV_LOG_ERROR,
                            "svdcheck failed for local file %s in data file mode for volume name: %s @LINE %d in FILE %s\n"
                            "sleeping for the file\n",
                            LocalFilename.c_str(), GetSourceVolumeName().c_str() , LINE_NO, FILE_NAME );
                bRetVal = false;
                while (1)
                {
                    ACE_OS::sleep(1000);
                }
            }
        }
    } /* End if of m_bS2ChecksEnabled || m_bDICheckEnabled */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return bRetVal;
}

bool ProtectedGroupVolumes::SvdCheckOnDataStream(
    char *buffer,
    const size_t bufferlen,
    const std::string &LocalFilename,
    boost::shared_ptr<Checksums_t> &checksums,
    const VolumeChangeData &Changes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    bool bRetVal = true;

    if (m_bSVDCheckEnabled)
    {
        /* Check for the svdcheck on the local file name */
        if (SvdCheck(buffer, bufferlen, LocalFilename, checksums.get()))
        {
            /* Never do a remove of localfile here since this file only will go to CX */
            DebugPrintf(SV_LOG_DEBUG,
                "svdcheck succeeded for file %s in data mode for volume name\n",
                LocalFilename.c_str(), GetSourceVolumeName().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,
                "svdcheck failed for file %s in data mode for volume name: %s.\n",
                LocalFilename.c_str(), GetSourceVolumeName().c_str());
            bRetVal = false;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    return bRetVal;
}


void ProtectedGroupVolumes::SetTransport(void)
{
    m_Transport[0] = &ProtectedGroupVolumes::TransferThroughAccmulation;
    m_Transport[1] = &ProtectedGroupVolumes::TransferDirectly;
}

/*
 * FUNCTION NAME :  SetRawSourceVolumeName
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : sets the name of the raw volume being protected as a std::string
 *
 */
void ProtectedGroupVolumes::SetRawSourceVolumeName(void)
{
    m_sRawProtectedVolumeName = GetRawVolumeName(m_sProtectedVolumeName);
}

std::string ProtectedGroupVolumes::GetFinalNameFromPreName(std::string const & preName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with preName = %s for volume %s\n",FUNCTION_NAME, preName.c_str(),GetSourceVolumeName().c_str());
    std::string finalName = preName;
    size_t j;
    if (m_transportProtocol == TRANSPORT_PROTOCOL_HTTP)
    {
        if ((j = finalName.find(PreRemoteNameExPrefix)) != string::npos)
        {
            finalName.replace(j, PreRemoteNameExPrefix.length(), FinalRemoteNameExPrefix);
        }
    }
    else
    {
        if ((j = finalName.find(PreRemoteNamePrefix)) != string::npos)
        {
            finalName.replace(j, PreRemoteNamePrefix.length(), FinalRemoteNamePrefix);
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with finalName = %s for volume %s\n",FUNCTION_NAME, finalName.c_str(), GetSourceVolumeName().c_str());
    return finalName;
}


std::string ProtectedGroupVolumes::WriteOrderTag(bool isContinuationEnd, VolumeChangeData &changeData)
{
    std::string writeordertag;
    unsigned long int cChanges = changeData.GetTotalChanges() ;
    DebugPrintf( SV_LOG_DEBUG,
                 "isContinuationEnd: %d  changeData.IsContinuationEnd():%d cChanges: %lu\n",
                 isContinuationEnd,
                 changeData.IsContinuationEnd(),
                 cChanges )  ;


    if ((isContinuationEnd) || (0 == cChanges))
    {
        if ( changeData.IsWriteOrderStateData() )
        {
            writeordertag = WriteOrderContinuationEndTag ;
        }
        else
        {
            writeordertag = MetaDataContinuationEndTag;
        }
    }
    else
    {
        if ( changeData.IsWriteOrderStateData() )
        {
            writeordertag = WriteOrderContinuationTag ;
        }
        else
        {
            writeordertag = MetaDataContinuationTag;
        }
    }

    return writeordertag;
}

/*1. Create the directory at source installation directory. The path would be <installation directory>/diffsync/checksums/ - same as second point */
/*2. create the filename with source volume, if it is not already available - this is done as part of initialization only DICheck = 1 */
/*3. Append the differential file name and checksums to the end of file */
/*4. This file would be deleted by source service when issued stop filtering or s2 can check the file creation time
  File creation is not provided by linux. Hence serivce would have to handle this.*/
int ProtectedGroupVolumes::persistChecksums(
    Checksums_t& checksums,
    std::string differentialFileName )
{
    int nReturnCode = SV_SUCCESS ;

    if( m_fpChecksumFile )
    {
        fprintf(m_fpChecksumFile, "%s\n", differentialFileName.c_str());
        fflush(m_fpChecksumFile);
        Checksums_t::iterator beginIterator = checksums.begin() ;
        for( ; beginIterator != checksums.end(); beginIterator++ )
        {
            std::string checksumDetails = *(beginIterator) ;
            fprintf(m_fpChecksumFile, "%s", checksumDetails.c_str());
            fflush(m_fpChecksumFile);
        }
    }

    return nReturnCode ;
}

int ProtectedGroupVolumes::setupChecksumStore( const std::string& sourceVolume )
{
    const std::string CHECKSUMS = "checksums";
    const std::string SOURCE = "source";
    int nReturnCode = SV_FAILURE ;
    if( m_bDICheckEnabled )
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,sourceVolume.c_str());
        std::string checksumDirectory = RcmConfigurator::getInstance()->getTargetChecksumsDir() ;

        if (checksumDirectory[checksumDirectory.size() - 1] != ACE_DIRECTORY_SEPARATOR_CHAR_A)
        {
            checksumDirectory += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        }
        checksumDirectory += CHECKSUMS;
        checksumDirectory += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        checksumDirectory += SOURCE;
        checksumDirectory += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        SVERROR svError = SVMakeSureDirectoryPathExists( checksumDirectory.c_str() ) ;

        if( svError.succeeded() )
        {
            std::string srcvolnameexcludingslash = sourceVolume;
            replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ':' , '_');
            replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '/' , '_');
            replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), '\\' , '_');
            replace(srcvolnameexcludingslash.begin(), srcvolnameexcludingslash.end(), ' ' , '_');
            std::string checksumFileName = checksumDirectory;
            checksumFileName += srcvolnameexcludingslash;
            checksumFileName += ".dat" ;

            DebugPrintf(SV_LOG_DEBUG, "The check sums log file name formed is %s for volume %s\n", checksumFileName.c_str(), sourceVolume.c_str());

#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)
            // PR#10815: Long Path support
            m_fpChecksumFile = _wfopen(getLongPathName(checksumFileName.c_str()).c_str(), ACE_TEXT("a"));
#else
            m_fpChecksumFile = fopen(checksumFileName.c_str(), "a");
#endif

            if (m_fpChecksumFile)
            {
                nReturnCode = SV_SUCCESS;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s, fopen of %s failed for volume %s\n",
                            __LINE__, __FILE__, checksumFileName.c_str(), sourceVolume.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s, SVMakeSureDirectoryPathExists for %s failed for volume %s\n",
                        __LINE__, __FILE__, checksumDirectory.c_str(), sourceVolume.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,sourceVolume.c_str());
    }
    else
    {
        nReturnCode = SV_SUCCESS ;
    }
    return nReturnCode ;
}


int ProtectedGroupVolumes::deletePrevFileIfReq( const std::string& preName )
{
    //DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,sourceVolume.c_str());
    int iTranspStatus = SV_SUCCESS;

    if((!m_pTransportStream->Good()) ||
       (m_pTransportStream->NeedToDeletePreviousFile()))
    {
        if(!m_pTransportStream->Good())
        {
            /*If the transport stream is bad
             * reset the connection.*/
            iTranspStatus = InitTransport();
        }
        if (SV_SUCCESS == iTranspStatus)
        {
            /* It just tries to create a file
             * with 0 bytes and delete it */
            iTranspStatus = DeleteInconsistentFileFromCX(preName);
        }
        if ((SV_SUCCESS == iTranspStatus)
            && m_pTransportStream->NeedToDeletePreviousFile())
        {
            m_pTransportStream->SetNeedToDeletePreviousFile(false);
        }
    }

    //DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,sourceVolume.c_str());
    return iTranspStatus;
}


void ProtectedGroupVolumes::PersistFileNamesAndCheckSums( const std::string & finalName,
                                                          boost::shared_ptr<Checksums_t> &checksums )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    if (m_bS2ChecksEnabled || m_bDICheckEnabled)
    {
        /* Log the preName and completed name to FilePreAndCompDiffFile */
        if (fpRenamedSuccessfully)
        {
            fprintf(fpRenamedSuccessfully, "%s\n",finalName.c_str());
            fflush(fpRenamedSuccessfully);
        }

        if ((NULL != checksums.get()) && m_fpChecksumFile && m_bDICheckEnabled)
        {
            persistChecksums( *(checksums.get()), finalName ) ;
        }

    } /* End of if (m_bS2ChecksEnabled) || m_bDICheckEnabled */
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
}


std::string ProtectedGroupVolumes::SupportedFileName(
                                                     VolumeChangeData& Changes,
                                                     const std::string &LocalFilename 
                                                    )
{
    std::string localfilebasename = basename_r(LocalFilename.c_str());
    std::string supportedfilename = localfilebasename;

    if ( m_pDeviceFilterFeatures->IsDiffOrderSupported() )
    {
        std::stringstream prevendtsstr;
        prevendtsstr << PREVENDTS_MARKER
                     << Changes.GetPrevEndTS()
                     << TOKEN_SEP_FILE
                     << Changes.GetPrevEndSeqNo()
                     << TOKEN_SEP_FILE
                     << Changes.GetPrevSeqIDForSplitIO(); 
        size_t startpos = supportedfilename.find(STARTTS_MARKER);
        startpos++;
        size_t endpos = supportedfilename.find(ENDTS_MARKER);
        size_t counttoreplace = endpos - startpos;
        supportedfilename.replace(startpos, counttoreplace, prevendtsstr.str());
    }
    
    if(m_transportProtocol == TRANSPORT_PROTOOL_MEMORY)
    {
        time_t currTime = time(NULL);
        std::string timeStr = boost::lexical_cast<std::string>(currTime);
        size_t dotPosition = supportedfilename.rfind( '.' ) ;
        if(  dotPosition != std::string::npos )
        {
            std::string tempString = supportedfilename.substr( dotPosition ) ;
            supportedfilename = supportedfilename.substr( 0, dotPosition ) ;
            supportedfilename += "_" ;
            supportedfilename += timeStr;
            supportedfilename += tempString ;
        }
    }
    /* DebugPrintf( SV_LOG_DEBUG, "After conveting to newer format: %s\n", supportedfilename.c_str() ) ; */
    return supportedfilename;
}

SV_ULONGLONG ProtectedGroupVolumes::GetSrcStartOffset(void) const
{
    return m_SrcStartOffset;
}


SV_ULONG ProtectedGroupVolumes::GetDiffFileSize(VolumeChangeData& Changes,const SV_LONGLONG& SizeOfChangesToSend)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_FAILURE;
    SV_ULONG iTotalFileHeaderSize = 0 ;
    //HeaderInfo modified to implement the compression changes.
    iTotalFileHeaderSize = GetHeaderInfoSize();
    // Send the start timestamp
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {

        iTotalFileHeaderSize = iTotalFileHeaderSize + (2* GetTimeStampInfoV2Size() );
    }
    else
    {
        iTotalFileHeaderSize = iTotalFileHeaderSize + (2* GetTimeStampInfoSize() );
    }
    //SendUserTags modifed to implement the compression changes.
    iTotalFileHeaderSize += GetUserTagsSize(Changes.GetDirtyBlock());
        // Send the size of the changes only if size is greater than zero
    iTotalFileHeaderSize += GetSizeOfChangesInfoSize();
    iTotalFileHeaderSize += SizeOfChangesToSend;
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iTotalFileHeaderSize;

}

int ProtectedGroupVolumes::GetHeaderInfoSize(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    int sizeHeaderBuffer = sizeof( SV_UINT ) + sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return sizeHeaderBuffer;
}

int ProtectedGroupVolumes::GetTimeStampInfoV2Size(void)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str() ) ;

    int iStatus = SV_SUCCESS ;
    const int iBufferSize = sizeof( SVD_PREFIX ) + sizeof( SVD_TIME_STAMP_V2 ) ;
  
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iBufferSize;
}

/*
 * FUNCTION NAME :  SendTimeStampInfo
 *
 * DESCRIPTION :    Uploads the times stamp of first change and last changes
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if upload succeeds else SV_FAILURE
 *
 */
int ProtectedGroupVolumes::GetTimeStampInfoSize(void)//SendTimeStampInfo(const PDIRTY_BLOCKS pDirtyBlock) //
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    const int iBufferSize = sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iBufferSize;
}

int ProtectedGroupVolumes::GetUserTagsSize(const void* pDirtyBlock)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iTotalSizOfUserTagChange = 0;
    unsigned char* BufferForTags ;
    STREAM_REC_HDR_4B *TagEndOfList ;
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
        UDIRTY_BLOCK_V2* DirtyBlock = ( UDIRTY_BLOCK_V2* ) pDirtyBlock ;
        BufferForTags = DirtyBlock->uTagList.BufferForTags ;
        TagEndOfList = &( DirtyBlock->uTagList.TagList.TagEndOfList ) ;
    }
    else
    {
        UDIRTY_BLOCK* DirtyBlock = ( UDIRTY_BLOCK* ) pDirtyBlock ;
        BufferForTags = DirtyBlock->uTagList.BufferForTags ;
        TagEndOfList = &( DirtyBlock->uTagList.TagList.TagEndOfList ) ;
    }
    if ( !IsTagSent())
    {
        unsigned long ulBytes = (SV_ULONG) ((SV_PUCHAR) TagEndOfList - (SV_PUCHAR) BufferForTags);
        PSTREAM_GENERIC_REC_HDR pTag = NULL;

        pTag = (PSTREAM_GENERIC_REC_HDR) (BufferForTags + ulBytes);
        int iSizeOfUserTagChange = 0;
        while( pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG)
        {
            iSizeOfUserTagChange = 0;
            SV_ULONG ulLength = GET_STREAM_LENGTH(pTag);
            SV_ULONG ulLengthOfTag = 0;

            ulLengthOfTag = *(SV_PUSHORT)((SV_PUCHAR)pTag + GET_STREAM_HEADER_LENGTH(pTag));

            iSizeOfUserTagChange = sizeof(SVD_PREFIX) + ulLengthOfTag;
            iTotalSizOfUserTagChange += iSizeOfUserTagChange;
            ulBytes += ulLength;
            pTag = (PSTREAM_GENERIC_REC_HDR)(BufferForTags + ulBytes);  
            DebugPrintf(SV_LOG_DEBUG, "Tag length = %lu \n",ulLengthOfTag);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "TotalSizOfUserTagChange = %d \n",iTotalSizOfUserTagChange);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iTotalSizOfUserTagChange;
}

int ProtectedGroupVolumes::GetSizeOfChangesInfoSize()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;
    const int iBufferSize = sizeof(SVD_PREFIX) + sizeof(SV_ULARGE_INTEGER);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return iBufferSize;
}

SV_ULONG ProtectedGroupVolumes::GetStreamDataSize(VolumeChangeData& Data)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_ULONG ulicbLength = 0; 
    SV_ULONG ulBufferSize  = 0;
    if ( Data.IsInitialized() )
    {
        Data.SetDataLength(0);
        
        if(m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            UDIRTY_BLOCK_V2* DirtyBlock = ( ( UDIRTY_BLOCK_V2* ) Data.GetDirtyBlock() ) ;
            ulicbLength = DirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
            ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
        }
        else
        {
            UDIRTY_BLOCK* DirtyBlock = ( ( UDIRTY_BLOCK* ) Data.GetDirtyBlock()) ;
            ulicbLength = DirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
            ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return ulicbLength;
}

void ProtectedGroupVolumes::CreateDiffSyncObject()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
    VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd;
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEndIt(m_EndPointVolGrps.end());
    //TODO::This for loop executes only once as of now there is no support of 1-N sync;
    //For 1-N case need to add each diffsync object to a list of diffsync objects
    for (HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIt = m_EndPointVolGrps.begin();
        volumeGroupIt != volumeGroupEndIt;
        ++volumeGroupIt)
    {
        DebugPrintf(SV_LOG_DEBUG,"Creating Diff sync object\n");
        std::vector<std::string> visibleVolumes;
        int id = (*volumeGroupIt).id;
        std::string hostId = RcmConfigurator::getInstance()->getHostId();
        std::string diffLocation = RcmConfigurator::getInstance()->getDiffTargetSourceDirectoryPrefix() +
                                    ACE_DIRECTORY_SEPARATOR_STR_A +  hostId;
        std::string cacheLocation = RcmConfigurator::getInstance()->getDiffTargetCacheDirectoryPrefix() +
                                    ACE_DIRECTORY_SEPARATOR_STR_A + hostId;
        std::string searchPattern = RcmConfigurator::getInstance()->getDiffTargetFilenamePrefix();
        volumeIter = (*volumeGroupIt).volumes.begin();
        volumeEnd = (*volumeGroupIt).volumes.end();

        std::string deviceName;
        for (/* empty*/; volumeIter != volumeEnd; ++volumeIter) 
        {
            m_TgtVolume = (*volumeIter).second.deviceName;
            VOLUME_SETTINGS::endpoints_iterator endPointsIter = volumeIter->second.endpoints.begin();
            m_SrcCapacity = volumeIter->second.sourceCapacity;
            deviceName = endPointsIter->deviceName;
            /// m_CDPSettings = TheConfigurator->getCdpSettings(m_TgtVolume);
        }
        m_DiffsyncObj.reset(new DifferentialSync(*volumeGroupIt,
                                                id,
                                                RUN_OUTPOST_SNAPSHOT_AGENT,
                                                visibleVolumes,
                                                "dataprotection",
                                                "target",
                                                "diff",
                                                diffLocation,
                                                cacheLocation,
                                                searchPattern,
                                                true));
        DebugPrintf(SV_LOG_DEBUG,"Target Volume = %s \n",m_TgtVolume.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool ProtectedGroupVolumes::IsTransportProtocolMemory(void) const
{
    return (m_transportProtocol == TRANSPORT_PROTOOL_MEMORY);
}

void ProtectedGroupVolumes::Profile(const std::string &diff, VolumeChangeData &Changes)
{
    std::stringstream ss;
    ACE_Time_Value committime = m_TimeAfterCommit-m_TimeBeforeGetDB;
    ACE_Time_Value compresstime = m_pCompress ? m_pCompress->GetTimeForCompression() : ACE_Time_Value();
    ACE_Time_Value readtime = m_pVolumeStream ? m_pVolumeStream->GetTimeForRead() : ACE_Time_Value();
    
    /* TODO: print the capture and write order modes */
    ss << diff << ", "
       << STRDATASOURCE[Changes.GetDataSource()] << ", "
       << committime.sec() << '-'
       << committime.usec() << ", "
       << compresstime.sec() << '-'
       << compresstime.usec() << ", "
       << readtime.sec() << '-'
       << readtime.usec() << ", "
       << m_TimeForTransport.sec() << '-'
       << m_TimeForTransport.usec() << ", "
       << m_DiffSize << ", "
       << m_CompressedDiffSize;
    fprintf(m_fpProfileLog, "%s\n", ss.str().c_str());
    fflush(m_fpProfileLog);
}


void ProtectedGroupVolumes::PrintDiffsRate(void)
{
    std::stringstream ss;
    time_t now = time(NULL);
    if ((now-m_LastDiffsRatePrintTime) >= RcmConfigurator::getInstance()->ProfileDifferentialRateInterval())
    {
        PrintDiffsRateEntry(now);
        m_LastDiffsRatePrintTime = now;
    }
}


void ProtectedGroupVolumes::PrintDiffsRateEntry(const time_t &now)
{
    std::string strnow = ctime(&now);
    strnow.erase(strnow.end()-1);
    fprintf(m_fpDiffsRateProfilerLog, "%s," ULLSPEC "," ULLSPEC "\n", strnow.c_str(), m_TotalDiffsSize, m_TotalCompressedDiffsSize);
    fflush(m_fpDiffsRateProfilerLog);
}


void ProtectedGroupVolumes::ResetPerDiffProfilerValues(void)
{
    m_TimeBeforeGetDB = m_TimeAfterCommit = 0;
    m_DiffSize = m_CompressedDiffSize = 0;
    m_TimeForTransport = 0;
}


int ProtectedGroupVolumes::DeleteSentDiffFile(const std::string &sentfile)
{
    int iDelStatus;
    const char *pdelfile;
    if (m_bNeedToRenameDiffs)
    {
        /* TODO: what happens if ps has already renamed to .gz ? */
        iDelStatus = m_pTransportStream->DeleteFile(sentfile);
        pdelfile = sentfile.c_str();
    }
    else
    {
        /* basename because the DeleteFile api does not want '/' or 
         * '\\' in file spec (second arguement) */
        std::string deleteFileName = basename_r(sentfile.c_str(), Slash[0]);
        deleteFileName += (COMPRESS_CXSIDE == m_compressMode) ? ".gz" : "";
        iDelStatus = m_pTransportStream->DeleteFile(m_FinalPaths, deleteFileName);
        pdelfile = deleteFileName.c_str();
    }

    if (SV_SUCCESS != iDelStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "volume: %s, deletion unsuccessful for \"already sent but commit failed\" file %s"
                                  " in directories (%c separated) %s\n", GetSourceVolumeName().c_str(), 
                                  pdelfile, FINALPATHS_SEP, m_FinalPaths.c_str());
    }

    return iDelStatus;
}


bool ProtectedGroupVolumes::ShouldWaitForData(VolumeChangeData &Changes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool needtowait;
    SV_LONGLONG totalchangespending = Changes.GetTotalChangesPending();
    
    if (totalchangespending < m_ThresholdDirtyblockSize) {
        DebugPrintf(SV_LOG_DEBUG, "totalchangespending " LLSPEC " is less than threshold. Checking if we need to wait for data.\n", totalchangespending);
        needtowait = ShouldWaitForDataOnLessChanges(Changes);
    } else {
        DebugPrintf(SV_LOG_DEBUG, "totalchangespending " LLSPEC " is greater than or equal to threshold. Hence not waiting for data.\n", totalchangespending);
        needtowait = false;
        m_BytesToDrainWithoutWait = 0;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with needtowait = %s\n",FUNCTION_NAME, STRBOOL(needtowait));
    return needtowait;
}

void ProtectedGroupVolumes::LogDatapathThrottleInfo(const std::string& msg, const SV_ULONGLONG &ttlmsec)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    const boost::chrono::seconds currentTimeInSec =
        boost::chrono::duration_cast<boost::chrono::seconds>(boost::chrono::steady_clock::now().time_since_epoch());

    m_datapathThrottleCount++;
    m_cumulativeTimeSpentInThrottleInMsec += ttlmsec;
    if ((currentTimeInSec.count() - m_prevDatapathThrottleLogTime) >= DatapathThrottleLogIntervalSec)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n", FUNCTION_NAME, msg.c_str());

        if (m_prevDatapathThrottleLogTime)
        {
            std::stringstream ss;
            ss << "device " << GetSourceDiskId() << " throttled " << m_datapathThrottleCount << " times in last "
                << currentTimeInSec.count() - m_prevDatapathThrottleLogTime
                << " sec total throttle time=" << m_cumulativeTimeSpentInThrottleInMsec << " milisec";
            DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n", FUNCTION_NAME, ss.str().c_str());
        }

        m_prevDatapathThrottleLogTime = currentTimeInSec.count();
        m_datapathThrottleCount = 0;
        m_cumulativeTimeSpentInThrottleInMsec = 0;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void ProtectedGroupVolumes::SetHealthIssue(const std::string& healthIssueCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (healthIssueCode.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Health IssueCode is empty.\n", FUNCTION_NAME);
        return;
    }

    // recurring health issue
    if (healthIssueCode == m_strThrottleHealthCode)
        return;

    // a different health code
    if (!m_strThrottleHealthCode.empty())
    {
        // here there could be two scenarios:
        // diff to cumulative : this is most likely scenario. this doesn't mean the diff throttle is cleared.
        //      since cumulative throttle is received, it has higher precedence. Do not clear diff throttle
        //      but just add cumulative throttle also.
        // cumulative to diff: this can happen when the cumulative is cleared but the diff throttle is either
        //      a contuation of already set diff throttle or a new diff throttle. clear the cumulative throttle
        //      and keep only diff throttle
        // ?? does cxps guarantee that there will be no diff throttle sent when cumulative throttle is present.

        if (healthIssueCode == AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode)
        {
            if (!ResetThrottleHealthIssue())
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: cumulative throttle reset failed for disk %s as diff throttle is being set.\n",
                    FUNCTION_NAME, GetSourceDiskId().c_str());
                return;
            }
        }
    }

    //Raise a Health Issue here
    if (m_healthCollatorPtr.get() != NULL)
    {
        //fill in params for the disk level Health Issue object
        std::map < std::string, std::string>params;

        params.insert(std::make_pair(AgentHealthIssueCodes::ObservationTime,
            m_healthCollatorPtr->GetCurrentObservationTime()));//throttled time

        AgentDiskLevelHealthIssue diskHealthIssue(GetSourceDiskId(),
            healthIssueCode, params);

        //Handover this health issue to the Health Collator to generate/persist in the health .json file
        if (!m_healthCollatorPtr->SetDiskHealthIssue(diskHealthIssue))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to raise the Health Issue %s for the disk %s\n",
                diskHealthIssue.IssueCode.c_str(), diskHealthIssue.DiskContext.c_str());
        }
        else
        {
            m_strThrottleHealthCode = healthIssueCode;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get HealthCollator object and hence not able to raise Health Issue %s for the disk %s\n",
            FUNCTION_NAME, healthIssueCode.c_str(), GetSourceDiskId().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ProtectedGroupVolumes::HandleDatapathThrottle(const ResponseData &rd, const DATA_SOURCE &SourceOfData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::stringstream sserr;
    const int defaultttl = 1000 * SECS_TO_WAITON_ERROR;
    SV_ULONGLONG ttlmsec = 0;
    std::string healthIssueCode;

    m_bThrottled = true;

    ThrottleNotifyToDriver();

    //Note: If Cumulative Throttle Health Issue is found then no need to raise Differnetial Throttle Health Isue
    // eventhough the cumulative throttle is a VM level health issue, it is being raise at disk level and SRS
    // consolidating and showing only one health issue.
    sserr << FUNCTION_NAME << ": device " << GetSourceDiskId() << " data source " << STRDATASOURCE[SourceOfData];
    if (rd.headers.find(CxpsTelemetry::CUMULATIVE_THROTTLE) != rd.headers.end())
    {
        sserr << " is cumulative throttled";
        healthIssueCode = AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode;
    }
    else if (rd.headers.find(CxpsTelemetry::DIFF_THROTTLE) != rd.headers.end())
    {
        sserr << " is diff throttled";
        healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode;
    }
    else
    {
        sserr << " is throttled but throttle info is missing in ResponseData.";
        healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode;//safely assuming as DR Throttle in this case
    }

    SetHealthIssue(healthIssueCode);

    try
    {
        responseHeaders_t::const_iterator itr = rd.headers.find(CxpsTelemetry::THROTTLE_TTL);
        if (itr != rd.headers.end())
        {
            ttlmsec = boost::lexical_cast<SV_ULONGLONG>(itr->second);
        }
        else
        {
            sserr << " TTL is missing in ResponseData using default TTL=" << defaultttl << " milisec";
        }
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s caught exception %s\n", FUNCTION_NAME, ex.what());
    }

    if (!ttlmsec)
    {
        ttlmsec = defaultttl;
    }

    sserr << " Current TTL=" << ttlmsec << " milisec";

    LogDatapathThrottleInfo(sserr.str(), ttlmsec);

    DebugPrintf(SV_LOG_DEBUG, "%s honoring current TTL %lu milisec before next data upload attempt\n", FUNCTION_NAME, ttlmsec);
    WaitOnQuitEvent(0, ttlmsec);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void ProtectedGroupVolumes::HandleTransportFailure(VolumeChangeData& Changes, const DATA_SOURCE &SourceOfData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    const ResponseData rd = m_pTransportStream->GetResponseData();

    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication()
        && (rd.responseCode == CLIENT_RESULT_NOSPACE))
    {
        HandleDatapathThrottle(rd, SourceOfData);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: differential upload failed source stream=%s, transport status=%u\n",
            FUNCTION_NAME, STRDATASOURCE[SourceOfData], m_pTransportStream->GetLastError());

        WAIT_ON_RETRY_NETWORK_ERROR(Changes);/*Waiting for some time before restarting the send.*/
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

int ProtectedGroupVolumes::StopFilteringOnResync()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    PauseTrackingStatus_t status;
    PauseTrackingUpdateInfo update;
    std::string nameForDriverInput = std::string(m_SourceNameForDriverInput.begin(),
        m_SourceNameForDriverInput.end());

    status.insert(std::make_pair(nameForDriverInput, update));
    if (stopFiltering(m_pDeviceStream, status))
    {
        m_resyncRequired = false;
        DebugPrintf(SV_LOG_DEBUG,
            "Volume: %s. Stop filtering as resync required.\n",
            GetSourceVolumeName().c_str());
    }
    else
    {
        iStatus = SV_FAILURE;

        DebugPrintf(SV_LOG_ERROR,
            "FAILED: Volume: %s. Unable to stop filtering. Status %d Reason %s\n",
            GetSourceVolumeName().c_str(),
            update.m_statusCode,
            update.m_statusMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return iStatus;
}

bool ProtectedGroupVolumes::IsResyncRequired()
{
    return m_resyncRequired;
}

//
// This function should be called only when the PGV thread is in stopped state.
// A restart of the PGV is supported only for resync required case.
// 
void ProtectedGroupVolumes::ResetProtectionState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    assert(IsResyncRequired());

    DebugPrintf(SV_LOG_ALWAYS, 
        "%s : resetting protection state for %s.\n", 
        FUNCTION_NAME,
        GetSourceVolumeName().c_str());

    m_bProtecting = false;
    m_ProtectEvent.Signal(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

//
// This function is called when there is a network error
// Contains the timeout logic and wait logic for network errors
//
void ProtectedGroupVolumes::WaitOnNetworkErrorRetry(int rateFactor, uint64_t &counter, VolumeChangeData &changes)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "Network error code is %d\n", m_networkFailureCode);

    if (m_networkFailureCode == CURLE_OPERATION_TIMEDOUT && m_networkTimeoutFailureCurrentTimeout < m_networkTimeoutFailureMaximumTimeout)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s:Timeout error encountered\n", FUNCTION_NAME);
        m_networkTimeoutFailureCount++;
        DebugPrintf(SV_LOG_ERROR, "%s:Timeout failure counter value is %lld and Network failure counter value is %lld \n", FUNCTION_NAME, m_networkTimeoutFailureCount, counter);
        m_networkTimeoutResetTimestamp = boost::chrono::steady_clock::now();
        DebugPrintf(SV_LOG_DEBUG, "%s: Updated timeout reset timestamp to current time %s\n", FUNCTION_NAME, (boost::lexical_cast<string>(m_networkTimeoutResetTimestamp)).c_str());

        m_networkTimeoutFailureCurrentTimeout = std::min(NETWORK_TIMEOUT_MULTIPLIER * m_networkTimeoutFailureCurrentTimeout, m_networkTimeoutFailureMaximumTimeout);
        DebugPrintf(SV_LOG_DEBUG, "%s: Updated current timeout %lld, Updated maximum timeout %lld\n", FUNCTION_NAME, m_networkTimeoutFailureCurrentTimeout, m_networkTimeoutFailureMaximumTimeout);
        m_pTransportStream->SetTransportTimeout(m_networkTimeoutFailureCurrentTimeout);
        DebugPrintf(SV_LOG_DEBUG, "%s: Updated blob timeout to %lld sec\n", FUNCTION_NAME, m_networkTimeoutFailureCurrentTimeout);
        counter++;
        DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME);
        return;
    }

    if (m_networkFailureCode == CURLE_OPERATION_TIMEDOUT)
    {
        m_networkTimeoutFailureCount++;
        DebugPrintf(SV_LOG_ERROR, "%s:Timeout failure counter value is %lld and Network failure counter value is %lld \n", FUNCTION_NAME, m_networkTimeoutFailureCount, counter);
        m_networkTimeoutResetTimestamp = boost::chrono::steady_clock::now();
        DebugPrintf(SV_LOG_DEBUG, "%s: Updated timeout reset timestamp to current time %s\n", FUNCTION_NAME, (boost::lexical_cast<string>(m_networkTimeoutResetTimestamp)).c_str());
        WaitOnErrorRetry(rateFactor, (uint64_t)m_networkTimeoutFailureCount);
        NOTIFY_DRIVER_ON_NETWORK_FAILURE(changes);
        counter++;
        DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME);
        return;
    }
    else
    {
        if (counter > rateFactor)
        {
            return WaitOnErrorRetry(rateFactor, counter);
        }

        uint64_t waitTime = SECS_TO_WAITON_NETWORK_ERROR;
        DebugPrintf(SV_LOG_ERROR, "%s: waiting for %lld secs.\n", FUNCTION_NAME, waitTime);
        WaitOnQuitEvent(waitTime);
        NOTIFY_DRIVER_ON_NETWORK_FAILURE(changes);
        counter++;
        DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME);
        return;
    }
}

void ProtectedGroupVolumes::WaitOnErrorRetry(int rateFactor, uint64_t counter)
{
    uint64_t waitMultiplier = 1 << (counter / rateFactor); 
    
    if (waitMultiplier > rateFactor)
        waitMultiplier = rateFactor;

    DebugPrintf(SV_LOG_ERROR, "%s: waiting for %d sec\n", FUNCTION_NAME, SECS_TO_WAITON_ERROR * waitMultiplier);
    WaitOnQuitEvent(SECS_TO_WAITON_ERROR * waitMultiplier); 
    counter++;

    return;
}

void ProtectedGroupVolumes::ResetErrorCounters()
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "Resetting counters to 0\n");
    m_networkFailureCount = 0;
    m_diskReadFailureCount = 0;
    m_driverFailureCount = 0;
    m_networkTimeoutFailureCount = 0;
    m_memAllocFailureCount = 0;
    m_networkFailureCode = 0;
    DecreaseNetworkTimeout();
    DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME);
}

//For Churn Metrics collection
FilterDrvVol_t ProtectedGroupVolumes::GetSourceNameForDriverInput()
{
    return m_SourceNameForDriverInput;
}
SV_ULONGLONG ProtectedGroupVolumes::GetDrainLogTransferedBytes()
{
    return m_drainLogTransferedBytes;
}

bool ProtectedGroupVolumes::RefreshTransportSettings(DATA_PATH_TRANSPORT_SETTINGS& settings)
{
    bool bRefreshed = true;
    boost::unique_lock<boost::mutex> lock(m_mutexTransportSettingsChange);
    m_datapathTransportSettings = settings;
    m_bTransportSettingsChanged = true;
    return bRefreshed;
}

bool ProtectedGroupVolumes::UpdateTransportSettings()
{
    bool bUpdated = true;
    if (m_transportProtocol == TRANSPORT_PROTOCOL_BLOB)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: updating transport settings for %s\n", FUNCTION_NAME, GetSourceDiskId().c_str());
        boost::unique_lock<boost::mutex> lock(m_mutexTransportSettingsChange);
        m_pTransportStream->UpdateTransportSettings(m_datapathTransportSettings.m_AzureBlobContainerSettings.sasUri);
        m_bTransportSettingsChanged = false;
    }
    else if (m_transportProtocol == TRANSPORT_PROTOCOL_HTTP)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: updating transport settings for %s\n", FUNCTION_NAME, GetSourceDiskId().c_str());
        boost::unique_lock<boost::mutex> lock(m_mutexTransportSettingsChange);
        m_TransportSettings.ipAddress = m_datapathTransportSettings.m_ProcessServerSettings.ipAddress;
        m_TransportSettings.sslPort = m_datapathTransportSettings.m_ProcessServerSettings.port;
        m_bTransportSettingsChanged = false;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Unsupported transport protocol %d to update transport settings.\n",
            m_transportProtocol);
        bUpdated = false;
    }
    
    return bUpdated;
}

void ProtectedGroupVolumes::DecreaseNetworkTimeout()
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", FUNCTION_NAME);
    try
    {
        if (m_networkTimeoutFailureCurrentTimeout <= m_networkTimeoutFailureMinimumTimeout)
        {
            return;
        }
        DebugPrintf(SV_LOG_DEBUG, "Time duration value is %d hrs\n", m_networkTimeoutResetInterval);
        if (boost::chrono::steady_clock::now() > m_networkTimeoutResetTimestamp + boost::chrono::hours(m_networkTimeoutResetInterval))
        {
            DebugPrintf(SV_LOG_DEBUG, "Timeout reset interval %d has elasped since last error timestamp %s\n",
                m_networkTimeoutResetInterval,
                (boost::lexical_cast<string>(m_networkTimeoutResetTimestamp)).c_str());
            SV_ULONGLONG newTimeout = std::max(m_networkTimeoutFailureMinimumTimeout, m_networkTimeoutFailureCurrentTimeout / NETWORK_TIMEOUT_MULTIPLIER);;
            if (newTimeout != m_networkTimeoutFailureCurrentTimeout)
            {
                DebugPrintf(SV_LOG_ALWAYS, "Timeout failure timeout value updated to %lld s.\n", newTimeout);
                m_networkTimeoutFailureCurrentTimeout = newTimeout;
                m_pTransportStream->SetTransportTimeout(m_networkTimeoutFailureCurrentTimeout);
                DebugPrintf(SV_LOG_ALWAYS, "Azure blob timeout updated to %lld s.\n", m_networkTimeoutFailureCurrentTimeout);
            }
        }
    }
    catch (std::exception & ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception %s occured at decrease timeout function\n", ex.what());
    }
    DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME);
}

void ProtectedGroupVolumes::SetDefaultNetworkTimeoutValues()
{
    m_networkTimeoutResetTimestamp = boost::chrono::steady_clock::now();
    try
    {
        m_networkTimeoutResetInterval = RcmConfigurator::getInstance()->getAzureBlobOperationTimeoutResetInterval();
        m_networkTimeoutFailureMaximumTimeout = RcmConfigurator::getInstance()->getAzureBlobOperationMaximumTimeout();
        m_networkTimeoutFailureMinimumTimeout = RcmConfigurator::getInstance()->getAzureBlobOperationMinimumTimeout();
        m_networkTimeoutFailureCurrentTimeout = m_networkTimeoutFailureMinimumTimeout;
        DebugPrintf(SV_LOG_DEBUG, "Azure blob timeout values are\nAzure Blob Operation Timeout Reset Interval : %d, AzureBlobOperationMaximumTimeout : %lld, AzureBlobOperationMinimumTimeout : %lld", m_networkTimeoutResetInterval, m_networkTimeoutFailureMaximumTimeout, m_networkTimeoutFailureMinimumTimeout);
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Could not fetch timeout values from descout.conf. Setting default values\n");
        m_networkTimeoutResetInterval = DefaultAzureBlobOperationTimeoutResetInterval;
        m_networkTimeoutFailureMaximumTimeout = DefaultAzureBlobOperationMaximumTimeout;
        m_networkTimeoutFailureMinimumTimeout = DefaultAzureBlobOperationMinimumTimeout;
        DebugPrintf(SV_LOG_DEBUG, "Azure blob timeout values are\nAzure Blob Operation Timeout Reset Interval : %d, AzureBlobOperationMaximumTimeout : %lld, AzureBlobOperationMinimumTimeout : %lld", m_networkTimeoutResetInterval, m_networkTimeoutFailureMaximumTimeout, m_networkTimeoutFailureMinimumTimeout);
    }
}

bool ProtectedGroupVolumes::ResetThrottleHealthIssue()
{
    if (m_strThrottleHealthCode.empty()) return true;

    bool bStatus = false;

    if (m_healthCollatorPtr.get() != NULL)
    {
        // if not throttled, reset all health issues
        bStatus = true;
        if (!m_bThrottled || 
            (m_strThrottleHealthCode == AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode))
        {
            bStatus = m_healthCollatorPtr->ResetDiskHealthIssue(AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode, GetSourceDiskId());
        }

        if (bStatus && 
            (!m_bThrottled ||
            (m_strThrottleHealthCode == AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode)))
        {
            bStatus = m_healthCollatorPtr->ResetDiskHealthIssue(AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode, GetSourceDiskId());
        }

        if (bStatus)
        {
            m_strThrottleHealthCode.clear();

            DebugPrintf(SV_LOG_DEBUG, "%s: The Throttle Health Issues %s for the disk %s is reset at time %s\n",
                FUNCTION_NAME,
                m_strThrottleHealthCode.c_str(),
                GetSourceDiskId().c_str(),
                m_healthCollatorPtr->GetCurrentObservationTime().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to remove the Health Issue: %s for the disk %s\n",
                FUNCTION_NAME,
                m_strThrottleHealthCode.c_str(),
                GetSourceDiskId().c_str());
        }
    }
    return bStatus;
}

void ProtectedGroupVolumes::InitHealthCollator()
{
    if (m_healthCollatorPtr.get() == NULL)
    {
        //Compute the Json Health File path
        std::stringstream ssHealthPath;
        ssHealthPath << RcmConfigurator::getInstance()->getHealthCollatorDirPath();
        ssHealthPath << ACE_DIRECTORY_SEPARATOR_CHAR_A;
        ssHealthPath << NSHealthCollator::S2Prefix;
        ssHealthPath << NSHealthCollator::UnderScore;
        ssHealthPath << RcmConfigurator::getInstance()->getHostId();
        ssHealthPath << NSHealthCollator::UnderScore;
        std::string strDiskId = GetSourceDiskId();
        boost::replace_all(strDiskId, "/", "_");
        ssHealthPath << strDiskId;
        ssHealthPath << NSHealthCollator::ExtJson;
        try
        {
            m_healthCollatorPtr.reset(new HealthCollator(ssHealthPath.str()));
            if (m_healthCollatorPtr.get() == NULL)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to create Health Collator instance for the file %s", \
                    FUNCTION_NAME, (ssHealthPath.str()).c_str());
            }
            else
            {
                // load any perviously set health issues. the oder is important
                // cumulative throttle has precedence over the diff throttle
                if (m_healthCollatorPtr->IsDiskHealthIssuePresent(AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode, GetSourceDiskId()))
                {
                    m_strThrottleHealthCode = AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode;
                }

                if (m_healthCollatorPtr->IsDiskHealthIssuePresent(AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode, GetSourceDiskId()))
                {
                    m_strThrottleHealthCode = AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode;
                }

                m_bThrottled = !m_strThrottleHealthCode.empty();

                DebugPrintf(m_bThrottled ? SV_LOG_ALWAYS : SV_LOG_DEBUG, "%s: throttle health issue %s.\n", FUNCTION_NAME, m_bThrottled ? "found" : "not found");
            }
        }
        catch (std::exception &ex)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:Caught exception while creating Health Collator object for the file %s.\n",
                FUNCTION_NAME, (ssHealthPath.str()).c_str());
            m_healthCollatorPtr.reset();
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s,Caught generic exception while initializing Health Collator.\n", FUNCTION_NAME);
        }
    }
}
