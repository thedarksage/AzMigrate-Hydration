
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

#ifdef SV_UNIX
#include "persistentdevicenames.h"
#endif

#include "resyncrequestor.h"

#include "portable.h"
#include "boost/shared_ptr.hpp"

#include "svdparse.h"
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
#include "inmsafecapis.h"

extern Configurator* TheConfigurator;

const int MAX_IDLE_WAIT_TIME  = 65 ;
const int MIN_IDLE_WAIT_TIME = 30;// seconds

/* PR # 6254 Constant for waiting for volume to be online : START */
const long int SECS_TO_WAITFOR_VOL = 5;
/* PR # 6254 : END */

const long int SECS_TO_WAITON_ERROR = 120;
const long int SECS_RETRY_MONITOR_DATA = 50;
const long int SECS_RENAME_MON_RETRY = 10;

const long int SECS_TO_WAITON_DRVBUSY = 60;
const long int MSECS_TO_WAITON_GET_DB_RETRY = 300;
const unsigned OCCURENCE_NO_FOR_DRVBUSY_ERRLOG = 10;

const SV_UINT DatapathThrottleLogIntervalSec = 600;

/* TODO: should this be 2 as default ? */
const long int SECS_TO_WAITON_ATLUNSTATS = 2;

#define NOTIFY_DRIVER_ON_NETWORK_FAILURE() \
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


// TODO: these should not be hard coded they should come from configuration or at least some common place
std::string const TimeStampsOnlyTag("tso_");
std::string const TagOnlyTag("tag_");
std::string const DestDir("/home/svsystems/");
std::string const StrMonStat("ChurStat/pre_complete_v1.0_");
std::string const ThrpStat("ThrpStat/pre_complete_v1.0_");
std::string const DatExtension(".dat");
std::string const MetaDataContinuationTag("_MC");
std::string const MetaDataContinuationEndTag("_ME");
std::string const WriteOrderContinuationTag("_WC");
std::string const WriteOrderContinuationEndTag("_WE");
std::string const DiffDirectory("diffs/");
std::string const PreRemoteNamePrefix("pre_completed_diff_");
std::string const FinalRemoteNamePrefix("completed_diff_");

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
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, ProfilerDiskReadOperation, new NormProfilingBuckets((1024*1024), ProfilerDiskReadOperation, true), false, "", false,false)),//for measuring read latency per disk
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, ProfilerNetworkTransmissionOperation, new NormProfilingBuckets((1024 * 1024), ProfilerNetworkTransmissionOperation, true), false, "", false,false)), //for measuring network transmission latency
    m_readFailCnt(0),
    m_lastReadErrLogTime(0),
    m_sendChurnStatFailCnt(0),
    m_lastChurnStatsSendFailTime(0),
    m_sendThroughputStatFailCnt(0),
    m_lastThroughputStatsSendFailTime(0),
    m_diskNotFoundCnt(0),
    m_lastDiskNotFoundErrLogTime(0),
    m_lastNetworkErrLogTime(0),
    m_networkFailCnt(0),
    m_bThrottleDataTransfer(false),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCnt(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    m_Transport[0] = m_Transport[1] = 0;
    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    InitializeMembersForS2Checks();
    /* S2CHKENABLED : END */
    ResetPerDiffProfilerValues();
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
    m_TransportSettings(rhs.m_TransportSettings),
    m_Direction(rhs.m_Direction),
    m_Status(rhs.m_Status),
    m_VolumeChunkSize(rhs.m_VolumeChunkSize),
    m_VolumeSize(rhs.m_VolumeSize),
    m_EndPointVolGrps(rhs.m_EndPointVolGrps),
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),//for measuring read latency per disk
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)), //for measuing network transmission latency
    m_lastReadErrLogTime(0),
    m_readFailCnt(0),
    m_sendChurnStatFailCnt(0),
    m_sendThroughputStatFailCnt(0),
    m_diskNotFoundCnt(0),
    m_lastDiskNotFoundErrLogTime(0),
    m_lastNetworkErrLogTime(0),
    m_networkFailCnt(0),
    m_bThrottleDataTransfer(rhs.m_bThrottleDataTransfer),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCnt(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    m_Transport[0] = m_Transport[1] = 0;
    VOLUME_SETTINGS vs = *(rhs.m_pVolumeSettings);
    m_pVolumeSettings.reset(new VOLUME_SETTINGS((const VOLUME_SETTINGS &)vs));
    SetEndPoints(rhs.GetEndPoints());
    SetRawSourceVolumeName();
    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    InitializeMembersForS2Checks();
    /* S2CHKENABLED : END */
    ResetPerDiffProfilerValues();
    FilterDrvIf drvif;
    m_SrcStartOffset = drvif.GetSrcStartOffset(m_pDeviceFilterFeatures, *m_pVolumeSettings);
    SetTransport();
}


ProtectedGroupVolumes::ProtectedGroupVolumes(
                                             const VOLUME_SETTINGS& VolumeSettings,
                                             CDP_DIRECTION direction,
                                             CDP_STATUS status, 
                                             TRANSPORT_CONNECTION_SETTINGS transportSettings,
                                             const DeviceFilterFeatures *pDeviceFilterFeatures,
                                             const char *pHostId,
                                             HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t endPointVolGrps,
                                             const DataCacher::VolumesCache *pVolumesCache,
                                             const size_t *pMetadataReadBufLen
                                            )
    :  m_sProtectedVolumeName(VolumeSettings.deviceName),
       m_bProtecting(false),
       m_Direction(direction),
       m_Status(status),
       m_ProtectEvent(false, false),
#ifdef SV_WINDOWS
       m_DBNotifyEvent(false,true, NULL, USYNC_PROCESS),
#endif
       m_dataBufferLength(0),
       m_liTotalLength(0),
       m_pDeviceStream(NULL),
       m_pCompress(NULL),
       m_TransportSettings(transportSettings),
       m_liQuitInfo(0),
       m_WaitForDBNotify(true),
       m_VolumeChunkSize(0),
       m_VolumeSize(0),
       m_IdleWaitTime(MAX_IDLE_WAIT_TIME),
       //m_uiCommittedChanges(0),
       m_continuationId(0),
    m_pDeviceFilterFeatures(pDeviceFilterFeatures),
    m_pHostId(pHostId),
    m_pVolumesCache(pVolumesCache),
    m_pMetadataReadBufLen(pMetadataReadBufLen),
    m_fpProfileLog(0),
    m_EndPointVolGrps(endPointVolGrps),
    m_bNeedToRenameDiffs(true),
    m_BytesToDrainWithoutWait(0),
    m_TotalDiffsSize(0),
    m_TotalCompressedDiffsSize(0),
    m_LastDiffsRatePrintTime(0),
    m_fpDiffsRateProfilerLog(0),
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),//for measuring read latency per disk
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)), //for measuing network transmission latency
    m_lastReadErrLogTime(0),
    m_readFailCnt(0),
    m_sendChurnStatFailCnt(0),
    m_sendThroughputStatFailCnt(0),
    m_diskNotFoundCnt(0),
    m_lastDiskNotFoundErrLogTime(0),
    m_lastNetworkErrLogTime(0),
    m_networkFailCnt(0),
    m_bThrottleDataTransfer(false),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCnt(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    m_pVolumeSettings.reset(new VOLUME_SETTINGS(VolumeSettings)) ;
    SetEndPoints(endPointVolGrps);
    /* PrintEndPoints(); */
    SetRawSourceVolumeName();
    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    InitializeMembersForS2Checks();
    /* S2CHKENABLED : END */
    FilterDrvIf drvif;
    m_SrcStartOffset = drvif.GetSrcStartOffset(pDeviceFilterFeatures, VolumeSettings);
    SetTransport();
}




/*
 * FUNCTION NAME :      ProtectedGroupVolumes
 *
 * DESCRIPTION :    Constructor
 *
 * INPUT PARAMETERS :   Volume Settings to secure the volume details
 *                      CDP  directon specifies the direction of protection ie. SOURCE/TARGET
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
ProtectedGroupVolumes::ProtectedGroupVolumes(
                                             const VOLUME_SETTINGS& VolumeSettings,
                                             CDP_DIRECTION direction,
                                             CDP_STATUS status, 
                                             TRANSPORT_CONNECTION_SETTINGS transportSettings,
                                             const DeviceFilterFeatures *pDeviceFilterFeatures,
                                             const char *pHostId,
                                             const DataCacher::VolumesCache *pVolumesCache,
                                             const size_t *pMetadataReadBufLen
                                            )
    :  m_sProtectedVolumeName(VolumeSettings.deviceName),
       m_bProtecting(false),
       m_Direction(direction),
       m_Status(status),
       m_ProtectEvent(false, false),
#ifdef SV_WINDOWS
       m_DBNotifyEvent(false,true, NULL, USYNC_PROCESS),
#endif
       m_dataBufferLength(0),
       m_liTotalLength(0),
       m_pDeviceStream(NULL),
       m_pCompress(NULL),
       m_TransportSettings(transportSettings),
       m_liQuitInfo(0),
       m_WaitForDBNotify(true),
       m_VolumeChunkSize(0),
       m_VolumeSize(0),
       m_IdleWaitTime(MAX_IDLE_WAIT_TIME),
       //m_uiCommittedChanges(0),
       m_continuationId(0),
    m_pDeviceFilterFeatures(pDeviceFilterFeatures),
    m_pHostId(pHostId),
    m_pVolumesCache(pVolumesCache),
    m_pMetadataReadBufLen(pMetadataReadBufLen),
    m_fpProfileLog(0),
    //m_EndPointVolGrps(endPointVolGrps),
    m_bNeedToRenameDiffs(true),
    m_BytesToDrainWithoutWait(0),
    m_TotalDiffsSize(0),
    m_TotalCompressedDiffsSize(0),
    m_LastDiffsRatePrintTime(0),
    m_fpDiffsRateProfilerLog(0),
    m_DiskReadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerDiskReadOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)),//for measuring read latency per disk
    m_DiffFileUploadProfiler(ProfilerFactory::getProfiler(GENERIC_PROFILER, std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), new NormProfilingBuckets((1024 * 1024), std::string(ProfilerNetworkTransmissionOperation + m_sProtectedVolumeName), true, m_sProtectedVolumeName), false, "", false, false)), //for measuing network transmission latency
    m_lastReadErrLogTime(0),
    m_readFailCnt(0),
    m_sendChurnStatFailCnt(0),
    m_sendThroughputStatFailCnt(0),
    m_diskNotFoundCnt(0),
    m_lastDiskNotFoundErrLogTime(0),
    m_lastNetworkErrLogTime(0),
    m_networkFailCnt(0),
    m_bThrottleDataTransfer(false),
    m_transferredBytesToPS(0),
    m_prevDatapathThrottleLogTime(0),
    m_datapathThrottleCnt(0),
    m_cumulativeTimeSpentInThrottleInMsec(0),
    m_bThrottled(false)
{
    m_pVolumeSettings.reset(new VOLUME_SETTINGS(VolumeSettings)) ;
    SetEndPoints(VolumeSettings.endpoints);
    SetRawSourceVolumeName();
    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    InitializeMembersForS2Checks();
    /* S2CHKENABLED : END */
    ResetPerDiffProfilerValues();
    FilterDrvIf drvif;
    m_SrcStartOffset = drvif.GetSrcStartOffset(pDeviceFilterFeatures, VolumeSettings);
    SetTransport();
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
    m_pProtectedVolume = VolMan.CreateVolume(GetRawSourceVolumeName(), GetVolumeSettings().devicetype, &m_pVolumesCache->m_Vs);
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

    BasicIo::BioOpenMode mode = cdp_volume_t::GetSourceVolumeOpenMode(m_pVolumeSettings->devicetype);
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
    DebugPrintf(SV_LOG_ALWAYS,"Stopping protection for volume %s. Thread Shutting down..\n",GetSourceVolumeName().c_str());

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
* FUNCTION NAME :  InitMonitoringTransport
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
int ProtectedGroupVolumes::InitMonitoringTransport()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;
    m_pMonitorTransportStream.reset(new TransportStream(m_pVolumeSettings->transportProtocol, m_TransportSettings));
    iStatus = OpenMonitoringTransportStream();
    if (SV_SUCCESS == iStatus)
    {
        DebugPrintf(SV_LOG_DEBUG, "SV_SUCCESS: Initialized transport stream to send monitoring data.%s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to open transport stream to send monitoring data. Attempting to create a new stream.%s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
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
        uliVolumeChunkSize = TheConfigurator->getVxAgent().getVolumeChunkSize();
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
    cdp_volume_t::Ptr volume = VolMan.CreateVolume(GetRawSourceVolumeName(), GetVolumeSettings().devicetype, &m_pVolumesCache->m_Vs);
    std::stringstream errMsg;
    if (NULL == volume.get())
    {
        errMsg << FUNCTION_NAME << ": CreateVolume() failed for " << GetRawSourceVolumeName();
        errorMsg = errMsg.str();
        error = -1;
        return false;
    }

    BasicIo::BioOpenMode mode = cdp_volume_t::GetSourceVolumeOpenMode(m_pVolumeSettings->devicetype);
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
                m_diskNotFoundCnt++;
                ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                if ((currentTime.sec() - m_lastDiskNotFoundErrLogTime) > TheConfigurator->getVxAgent().getDiskNotFoundErrorLogInterval())
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to open source device %s for draining diffs with error %lu. Failures since %dsec=%d. Retrying after %ld seconds. @LINE %d in FILE %s\n",
                        GetSourceVolumeName().c_str(), m_pProtectedVolume->LastError(), TheConfigurator->getVxAgent().getDiskNotFoundErrorLogInterval(), m_diskNotFoundCnt, RETRY_TIME, LINE_NO, FILE_NAME);
                    m_lastDiskNotFoundErrLogTime = ACE_OS::gettimeofday().sec();
                    m_diskNotFoundCnt = 0;
                }
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

    if ( !IsInitialized() )
    {
        std::string devNameForDriverInput = m_pVolumeSettings->deviceName;
        m_sProtectedDiskId = m_pVolumeSettings->deviceName;

#ifdef SV_UNIX
        GetLatestVolumesCacheIfPresent();
        devNameForDriverInput = GetDiskNameForPersistentDeviceName(m_pVolumeSettings->deviceName, m_pVolumesCache->m_Vs);
        m_sProtectedVolumeName = devNameForDriverInput;
        SetRawSourceVolumeName();
#endif

        FilterDrvIfMajor fdif;
        m_SourceNameForDriverInput = fdif.GetFormattedDeviceNameForDriverInput(m_pVolumeSettings->isFabricVolume() ? m_pVolumeSettings->sanVolumeInfo.virtualName : devNameForDriverInput,
                                                                               m_pVolumeSettings->devicetype
                                                                               );
        DebugPrintf(SV_LOG_DEBUG, "Source name for driver input is %S\n", std::wstring(m_SourceNameForDriverInput.begin(), m_SourceNameForDriverInput.end()).c_str());


        /* initialization order has to be maintained. For eg: compress mode is used subsequently */
        std::string profiledifferentialrateoption = TheConfigurator->getVxAgent().ProfileDifferentialRate();
        m_compressMode = (InmStrCmp<NoCaseCharCmp>(PROFILE_DIFFERENTIAL_RATE_OPTION_WITH_COMPRESSION, profiledifferentialrateoption) == 0) ? 
                         COMPRESS_SOURCESIDE : m_pVolumeSettings->compressionEnable;


        /* because CreateVolumeStream uses profile option for read profiling */
        SetUpProfilerLog();
        SetUpDiffsRateProfilerLog();
        m_bProfileDiffsSize = m_fpProfileLog || m_fpDiffsRateProfilerLog;
        iStatus = CreateVolumeStream();

        if (SV_SUCCESS == iStatus)
        {
            if (!m_pVolumeSettings->isFabricVolume())
            {
                std::string messageifinvalid;
                VolumeOps::VolumeOperator vopr;
                if (!vopr.IsSourceDeviceCapacityValidInSettings(m_pVolumeSettings->deviceName, m_pProtectedVolume->GetSize(), m_pVolumeSettings->sourceCapacity,
                    m_pVolumeSettings->GetProtectionDirection(), messageifinvalid))
                {
                    DebugPrintf(SV_LOG_ERROR, "Cannot start protection on volume %s because %s. It will be retried.\n",
                        m_pVolumeSettings->deviceName.c_str(), messageifinvalid.c_str());
                    return SV_FAILURE;
                }
            }

            m_bS2ChecksEnabled = TheConfigurator->getVxAgent().getS2StrictMode() ;
            m_bDICheckEnabled = TheConfigurator->getVxAgent().getDICheck() ;
            m_bSVDCheckEnabled = TheConfigurator->getVxAgent().getSVDCheck();
            m_bForceDirectTrnasfer = TheConfigurator->getVxAgent().getDirectTransfer();
            m_bHasToCheckTS = TheConfigurator->getVxAgent().getTSCheck() ;
            SetUpLogForAllRenamedFiles();
            setupChecksumStore( GetSourceVolumeName() ) ;

            m_IdleWaitTime = GetIdleWaitTime();
            m_VolumeChunkSize   = GetVolumeChunkSize();
            m_VolumeSize        = m_pVolumeSettings->sourceCapacity;

            if (!IsTransportProtocolMemory())
            {
                long transportflushlimit = TheConfigurator->getVxAgent().getTransportFlushThresholdForDiff();
                m_dataPtr.reset(new char[transportflushlimit]);
                if(m_dataPtr.get() != NULL)
                {
                    memset(m_dataPtr.get(),0,transportflushlimit);
                    m_dataBufferLength = transportflushlimit;
                }
           }

           if ( IsCompressionEnabled() && (0 == m_pCompress ) )
           {
                m_pCompress = CompressionFactory::CreateCompressor(ZLIB);
                m_pCompress->m_bCompressionChecksEnabled = (m_bS2ChecksEnabled || m_bDICheckEnabled);
                m_pCompress->m_bProfile = m_fpProfileLog;
            }
            if ( (iStatus = InitTransport()) == SV_SUCCESS)
            {
                iStatus = CreateDeviceStream(fdif.GetDriverName(m_pVolumeSettings->devicetype));
                if ( iStatus == SV_SUCCESS )
                {
#ifdef SV_WINDOWS
                    VOLUME_DB_EVENT_INFO    EventInfo;
                    memset(&EventInfo, 0, sizeof EventInfo);
                    inm_wmemcpy_s(EventInfo.VolumeGUID, NELEMS(EventInfo.VolumeGUID), m_SourceNameForDriverInput.c_str(), m_SourceNameForDriverInput.length());
                    EventInfo.hEvent = (HANDLE)m_DBNotifyEvent.Self();
                    if ( (iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT,&EventInfo,sizeof(VOLUME_DB_EVENT_INFO),NULL,0) ) == SV_SUCCESS )
                    {
                        m_bInitialized = true;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to initialize volume with SET DIRTY BLOCK NOTIFY EVENT.\n");
                    }
#elif SV_UNIX
                    //for linux we need not do anything here except setting m_bInitialized
                    m_bInitialized = true;
#endif
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
 * FUNCTION NAME :  GetVolumeSettings
 *
 * DESCRIPTION :    stores the VOLUME_SETTINGS instance
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the instance of volume_settings
 *
 */
const VOLUME_SETTINGS& ProtectedGroupVolumes::GetVolumeSettings() const
{
    return *(m_pVolumeSettings.get());
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
    if (m_bNeedToRenameDiffs = TheConfigurator->getVxAgent().getShouldS2RenameDiffs())
    {
        DebugPrintf(SV_LOG_DEBUG, "s2 has to rename differential files\n");
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "s2 does not have to rename differential files as it is done in differential write operation\n");
        SetFinalPaths();
    }

    while( !IsStopRequested() && (!IsInitialized()))
    {
        if (SV_SUCCESS != Init())
        {
            DebugPrintf(SV_LOG_WARNING, "The volume: %s is not initialized. The protected volume could be offline, waiting for 30 seconds.", m_sProtectedVolumeName.c_str());
        }
        if (!IsInitialized())
        {
            WaitOnQuitEvent(30);
        }
        else
        {
            break;
        }
    }

    if (IsInitialized() && !IsStopRequested())
    {
        iStatus = StartFiltering();
    }

    if (SV_SUCCESS == iStatus)
    {
        InitHealthCollator();

        //Get this after the start filtering as the function issues an ioctl which will work only if the volume is being filtered.
        m_ThresholdDirtyblockSize = GetThresholdDirtyblockSize();

        DebugPrintf(SV_LOG_ALWAYS, "INFO: Starting Protection on volume %s.\n", GetSourceVolumeName().c_str());
        if (!IsStopRequested() && (true == IsInitialized()) && (false == IsProtecting()) && (SOURCE == GetProtectionDirection()))
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
void ProtectedGroupVolumes::SetEndPoints(const ENDPOINTS& EndPoints)
{
    m_pEndPoints.reset(new ENDPOINTS(EndPoints));
}

void ProtectedGroupVolumes::SetEndPoints(HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t &endPointVolGrps)
{
    m_pEndPoints.reset(new (std::nothrow) ENDPOINTS());
    if (m_pEndPoints.get())
    {
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t::iterator vgiter = endPointVolGrps.begin();
        for ( /* empty */; vgiter != endPointVolGrps.end(); vgiter++)
        {
            VOLUME_GROUP_SETTINGS &vgs = *vgiter;
            VOLUME_GROUP_SETTINGS::volumes_iterator iter;
            for(iter = vgs.volumes.begin();iter != vgs.volumes.end(); iter++)
            {
                VOLUME_SETTINGS &vs = iter->second;
                m_pEndPoints->insert(m_pEndPoints->end(), vs.endpoints.begin(), vs.endpoints.end());
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "for volume %s, could not allocate memory to hold endpoints\n", GetSourceVolumeName().c_str());
    }
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
void ProtectedGroupVolumes::SetEndPoints(VgsPtrs_t &vgsptrs)
{
    m_pEndPoints.reset(new (std::nothrow) ENDPOINTS());
    if (m_pEndPoints.get())
    {
        for (VgsPtrsIter_t vit = vgsptrs.begin(); vit != vgsptrs.end(); vit++)
        {
            VOLUME_GROUP_SETTINGS *pvgs = *vit;
            VOLUME_GROUP_SETTINGS &vgs = *pvgs;
            VOLUME_GROUP_SETTINGS::volumes_iterator iter;
            for(iter = vgs.volumes.begin();iter != vgs.volumes.end(); iter++)
            {
                VOLUME_SETTINGS &vs = iter->second;
                m_pEndPoints->insert(m_pEndPoints->end(), vs.endpoints.begin(), vs.endpoints.end());
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "for volume %s, could not allocate memory to hold endpoints\n", GetSourceVolumeName().c_str());
    }
}

void ProtectedGroupVolumes::PrintEndPoints(void)
{
    if (m_pEndPoints)
    {
        DebugPrintf(SV_LOG_DEBUG, "endpoints for %s:\n", GetSourceVolumeName().c_str());
        for (ENDPOINTSITER it = m_pEndPoints->begin(); it != m_pEndPoints->end(); it++)
        {
            DebugPrintf(SV_LOG_DEBUG, "----\n");
            VOLUME_SETTINGS &evs = *it;
            DebugPrintf(SV_LOG_DEBUG, "device name = %s\n", evs.deviceName.c_str());
            DebugPrintf(SV_LOG_DEBUG, "host id = %s\n", evs.hostId.c_str());
            DebugPrintf(SV_LOG_DEBUG, "----\n");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "for volume %s, there are no endpoints. This should not happen for sent diff file\n", GetSourceVolumeName().c_str());
    }
}


void ProtectedGroupVolumes::SetFinalPaths(void)
{
    if (m_pEndPoints)
    {
        for (ENDPOINTSITER it = m_pEndPoints->begin(); it != m_pEndPoints->end(); it++)
        {
            VOLUME_SETTINGS &evs = *it;
            /*
             -rwxrwxrwx 1 root root       66 Jan 18 23:33 completed_ediffcompleted_diff_tso_P12954376096213402_5_1_E12954376746355682_7_ME1_1295411627.dat
             drwxrwxrwx 2 root root    77824 Jan 19 18:18 resync
             [root@ccos5u5cx fslv02]# pwd
             /home/svsystems/8fd5ce3c-23e4-11e0-b6bf-bf0efe96427c/dev/fslv02
             [root@ccos5u5cx fslv02]#

             Target code that builds folder for diff path 
                m_cxLocation = m_configurator->getVxAgent().getDiffTargetSourceDirectoryPrefix();
                m_cxLocation += "/";
                m_cxLocation +=  m_configurator->getVxAgent().getHostId();
                m_cxLocation += '/';
                m_cxLocation += GetVolumeDirName(m_volname);
            */
            std::string path;
            path = TheConfigurator->getVxAgent().getDiffTargetSourceDirectoryPrefix();
            path += "/";
            path += evs.hostId;
            path += '/';
            path += GetVolumeDirName(evs.deviceName);
 
            m_FinalPaths += path;
            m_FinalPaths += FINALPATHS_SEP;
        }
        DebugPrintf(SV_LOG_DEBUG, "final paths = %s\n", m_FinalPaths.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "for volume %s, there are no endpoints. This should not happen for sent diff file\n", GetSourceVolumeName().c_str());
    }
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
int ProtectedGroupVolumes::ReturnHome()
{
    // PENDING: Maybe when return home is implemented this might be used.
    assert(!"Not Implemented as yet.");
    return SV_FAILURE;
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
            DebugPrintf(SV_LOG_ERROR,"FAILED: Could not rename remote file. Second attempt failed.@LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
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
        Configurator* configurator;
        if (!GetConfigurator(&configurator)) {
            return SV_FAILURE;
        }
        if (m_fpDiffsRateProfilerLog)
        {
            m_pTransportStream.reset(new DummyTransportStream());
        }
        else if(m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY)
        {
            CreateDiffSyncObject();
            m_pTransportStream.reset(new MemoryStream(TRANSPORT_PROTOOL_MEMORY,m_TransportSettings,m_TgtVolume, m_SrcCapacity,m_CDPSettings,m_DiffsyncObj.get()));
        }
        else
        {
            m_pTransportStream.reset(new TransportStream(m_pVolumeSettings->transportProtocol,m_TransportSettings));
        }

        if (m_pTransportStream)
        {
            m_pTransportStream->SetDiskId(GetSourceDiskId());
            m_pTransportStream->SetFileType(CxpsTelemetry::FileType_DiffSync);
        }
    }
    catch (DataProtectionException& dpe) {
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

    return SV_SUCCESS;
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
 */
int ProtectedGroupVolumes::OpenTransportStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    if (NULL == m_pTransportStream)
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR, "FAILED: Transport stream is NULL. Create a Transport Stream first. @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
    }
    else
    {
        //BUGBUG: Instead of specifying VolumeStream:: use GenericStream::
        STREAM_MODE mode = VolumeStream::Mode_Open | VolumeStream::Mode_RW;
        if (VOLUME_SETTINGS::SECURE_MODE_SECURE == m_pVolumeSettings->sourceToCXSecureMode)
            mode |= VolumeStream::Mode_Secure;

        if (SV_FAILURE == m_pTransportStream->Open(mode))
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to open transport stream. @LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    return iStatus;
}


/*
* FUNCTION NAME :  OpenMonitoringTransportStream
*
* DESCRIPTION :    Opens the transport stream for sending monitoring data to PS. If secure mode is
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
*/
int ProtectedGroupVolumes::OpenMonitoringTransportStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    int iStatus = SV_SUCCESS;
    if (NULL == m_pMonitorTransportStream)
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR, "FAILED: monitoring Transport stream is NULL. Create a Transport Stream first. %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    }
    else
    {
        //BUGBUG: Instead of specifying VolumeStream:: use GenericStream::
        STREAM_MODE mode = VolumeStream::Mode_Open | VolumeStream::Mode_RW;
        if (VOLUME_SETTINGS::SECURE_MODE_SECURE == m_pVolumeSettings->sourceToCXSecureMode)
            mode |= VolumeStream::Mode_Secure;

        if (SV_FAILURE == m_pMonitorTransportStream->Open(mode))
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "FAILED: Unable to open monitoring transport stream. %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

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
            Transport_t p = m_Transport[m_bForceDirectTrnasfer? 1 : IsTransportProtocolMemory()];
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

    if ( (uliDataLen + m_liTotalLength) > m_dataBufferLength  && 0 != m_liTotalLength)
    {
        // not enough space in buffer for data and there is data in buffer, send it
        iStatus = FlushDataToCx(m_dataPtr.get(), m_liTotalLength, sDestination, true, bCreateMissingDirs, bIsFull);
        m_liTotalLength = 0; // buffer is now "empty"
        if (SV_SUCCESS != iStatus)
        {
            return iStatus;
        }
    }
    if ( uliDataLen >= m_dataBufferLength || ( 0 == m_liTotalLength && !bMoreData ) )
    {
        // buffer is not large enough to hold the data or buffer empty an no more data, so just send the data
        return FlushDataToCx(sData, uliDataLen, sDestination, bMoreData, bCreateMissingDirs, bIsFull);
    }

    // enough space in buffer for data, copy the data to the buffer

    inm_memcpy_s(m_dataPtr.get() + m_liTotalLength, (m_dataBufferLength - m_liTotalLength), const_cast<char*>(sData), uliDataLen);
    m_liTotalLength += uliDataLen;

    if ( !bMoreData )
    {
        // no more data, but we have data in the buffer so send it
        iStatus = FlushDataToCx(m_dataPtr.get(), m_liTotalLength, sDestination, false, bCreateMissingDirs, bIsFull);
        m_liTotalLength = 0; // buffer is now "empty"
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

     int iStatus = SV_SUCCESS;

     if ( m_pTransportStream->Good() )
     {
         ACE_Time_Value TimeBeforeTransport, TimeAfterTransport;
         m_fpProfileLog && (TimeBeforeTransport=ACE_OS::gettimeofday());
         m_DiffFileUploadProfiler->start();
         iStatus = m_pTransportStream->Write(sDataToSend,liLength,sDestination,bMoreData, m_compressMode, bIsFull);
         if (SV_SUCCESS == iStatus)
         {
           m_DiffFileUploadProfiler->stop(liLength);
           m_transferredBytesToPS += liLength;
         }
         else
         {
             // though this may not be the exact error, marking it as no-connection
             // once the transport stream can emit a correct error code, this should reflect the exact error
             m_networkFailureCode = ENOTCONN;
         }
         m_fpProfileLog && (TimeAfterTransport=ACE_OS::gettimeofday()) && (m_TimeForTransport+=(TimeAfterTransport-TimeBeforeTransport));
         LogNetworkFailures(iStatus);

     }
     else
     {
         DebugPrintf(SV_LOG_ERROR,"FAILED: Volume: %s. Failed to send data.Corrupted Transport stream. @LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
         iStatus = SV_FAILURE;
         m_networkFailureCode = ENOTCONN;
     }

     if (SV_SUCCESS == iStatus && !bMoreData)
     {
         if (m_bThrottled)
             DebugPrintf(SV_LOG_ALWAYS, "%s: throttle reset on network transfer success.\n", FUNCTION_NAME);

         m_bThrottled = false;
     }

     DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
     return iStatus;
}

int ProtectedGroupVolumes::TransferMonData(const char* sDataToSend, const long int liLength, const string& sDestination, bool bMoreData, bool bCreateMissingDirs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    int iStatus = SV_SUCCESS;
    int renStatus = SV_SUCCESS;
    int noOfRetryAttempts;
    std::string repName;
    if ((m_pMonitorTransportStream == NULL) || (!m_pMonitorTransportStream->Good()))
    {
        DebugPrintf(((m_pMonitorTransportStream == NULL) ? SV_LOG_DEBUG : SV_LOG_ERROR), "Resetting bad monitoring Transport stream. %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
        iStatus = InitMonitoringTransport();
        if (SV_SUCCESS != iStatus)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to initialize monitoring transport . %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
            return iStatus;
        }
    }
    if (m_pMonitorTransportStream->Good())
    {
        for (noOfRetryAttempts = 0; !IsStopRequested() && noOfRetryAttempts < 3; noOfRetryAttempts++)
        {
            if (noOfRetryAttempts > 0)
            {
                if (WAIT_SUCCESS == WaitOnQuitEvent((2 * noOfRetryAttempts - 1) * SECS_RETRY_MONITOR_DATA))
                {
                    // send success so that there are no attemps to log the failure
                    // in the caller as we know this is in a processs quit path
                    return SV_SUCCESS;
                }
            }
            iStatus = m_pMonitorTransportStream->Write(sDataToSend, liLength, sDestination, bMoreData, COMPRESS_NONE, bCreateMissingDirs);
            if (iStatus == SV_SUCCESS)
            {
                break;
            }
        }
        if (SV_SUCCESS == iStatus)
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully transferred file %s\n", sDestination.c_str());
            // m_DiffFileUploadProfiler->stop(liLength);
            repName = sDestination;
            repName.replace(repName.find("pre_complete_"), sizeof("pre_complete_") - 1, "completed_");
            renStatus = RenameRemoteStatsFile(sDestination, repName);
            if (renStatus != SV_SUCCESS)
            {
                int rename_retry = 0;
                while ((renStatus != SV_SUCCESS) && rename_retry < 5)
                {
                    WaitOnQuitEvent(SECS_RENAME_MON_RETRY);
                    renStatus = RenameRemoteStatsFile(sDestination, repName);
                    rename_retry++;
                }
                if (renStatus != SV_SUCCESS)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to rename file pre_complete_%s. %s: %s\n", sDestination.c_str(), FUNCTION_NAME, GetSourceVolumeName().c_str());
                }
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Volume: %s. Failed to send data.Corrupted Transport stream. @FUNC: %s\n", GetSourceVolumeName().c_str(), FUNCTION_NAME);
        iStatus = SV_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

    return iStatus;
}

bool ProtectedGroupVolumes::SendHeartbeat()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bStatus = false;
    if ((NULL != m_pMonitorTransportStream) && m_pMonitorTransportStream->Good())
    {
        bStatus = m_pMonitorTransportStream->heartbeat(false);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Monitoring stream is not open for device: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bStatus;
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
 */int ProtectedGroupVolumes::RenameRemoteFile(std::string const & preName, std::string const & finalName)
 {
     DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
     DebugPrintf(SV_LOG_DEBUG, "Name of the differential file sent: %s\n", finalName.c_str());

     /* Initialize iStatus with SV_FAILURE */
     int iStatus = SV_FAILURE;

     if ( (NULL != m_pTransportStream) && m_pTransportStream->Good())
     {
         iStatus =  m_pTransportStream->Rename(preName,finalName,m_compressMode, m_FinalPaths);
     }
     if ( iStatus == SV_FAILURE)
     {
         DebugPrintf(SV_LOG_ERROR, "FAILED Couldn't rename file (%s => %s) in final paths (%c separated) %s\n", preName.c_str(), finalName.c_str(), 
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
 *FUNCTION NAME : RenameRemoteStatsFile
     *
     * DESCRIPTION : Renames the churn data file from pre_completed* to completed*
     *
     * INPUT PARAMETERS : preName is the name of the file while transport upload operation is in progress
     *                          it is pre_completed* and specified as an absolute directory path.
     *                      finalName is the name of the file after upload of file is completed
     *                          it is completed_*
     *
     * OUTPUT PARAMETERS : NONE
     *
     * NOTES :
     *
     * return value : returns SV_SUCCESS if rename is successfull else SV_FAILURE
     *
     */ 
 int ProtectedGroupVolumes::RenameRemoteStatsFile(std::string const & preName, std::string const & finalName)
 {
     DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());
     DebugPrintf(SV_LOG_DEBUG, "Name of the monitoring file sent: %s\n", finalName.c_str());

     int iStatus = SV_FAILURE;

     if ((NULL != m_pMonitorTransportStream) && m_pMonitorTransportStream->Good())
     {
         iStatus = m_pMonitorTransportStream->Rename(preName, finalName, m_compressMode, m_FinalPaths);
     }
     if (iStatus == SV_FAILURE)
     {
         DebugPrintf(SV_LOG_ERROR, "FAILED Couldn't rename file (%s => %s) in final paths (%c separated) %s\n", preName.c_str(), finalName.c_str(),
             FINALPATHS_SEP, m_FinalPaths.c_str());
     }
     else
     {
         DebugPrintf(SV_LOG_DEBUG, "RenameFile: %s => %s in final paths (%c separated) %s\n", preName.c_str(), finalName.c_str(),
             FINALPATHS_SEP, m_FinalPaths.c_str());
     }

     DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %s\n", FUNCTION_NAME, GetSourceVolumeName().c_str());

     return(iStatus);
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
    LogNetworkFailures(iStatus);

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
    if (*(dataFormatFlags) != LitttleEndianDataFormatFlags)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Invalid endian data format flags [0x%x], device=%s\n", FUNCTION_NAME, *(dataFormatFlags), GetSourceVolumeName().c_str());
    }

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
    string sVolName = this->GetSourceDiskId();
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
    name = DestDir;
    name += m_pHostId;
    name += Slash;
    name += sVolName;
    name += Slash;
    name += DiffDirectory;
    name += PreRemoteNamePrefix;
    if ( changeData.IsTimeStampsOnly() )
    {
        name += TimeStampsOnlyTag;
    }
    else if (changeData.IsTagsOnly())
    {
        name += TagOnlyTag;
    }
    name += timestamp.str();
    name += WriteOrderTag(isContinuationEnd, changeData);
    std::stringstream strseqidforsplitIO;
    strseqidforsplitIO << ulSeqIDForSplitIO;
    // TODO: need to use continuation info from the changeData.GetDirtyBlock()
    // if in write order, either have it passed in or just check here and use it
    name += strseqidforsplitIO.str();
    if(m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY)
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
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n", FUNCTION_NAME, GetSourceDiskId().c_str());
    
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
    DebugPrintf( SV_LOG_DEBUG,"%s: Throttling Protected Drive %s...\n", FUNCTION_NAME, GetSourceDiskId().c_str());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
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
        bThrottleDataTransfer = TheConfigurator->getVxAgent().shouldThrottleSource(GetSourceDiskId());

        if (m_bThrottleDataTransfer != bThrottleDataTransfer)
        {
            DebugPrintf( SV_LOG_ALWAYS,
            "Sentinel: %s data upload for Protected device %s.\n",
            bThrottleDataTransfer ? "Blocking" : "Unblocking",
            GetSourceVolumeName().c_str() );
            m_bThrottleDataTransfer = bThrottleDataTransfer;
        }

        if( bThrottleDataTransfer )
        {
            ThrottleNotifyToDriver();
            if (IsStopRequested())
            {
                break;
            }
            WaitOnQuitEvent(30);
        }
        else
        {
            break;
        }
    } while( true );

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
    bool bReportedSuccessfully = false;
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
        if(resyncRequestor.ReportResyncRequired(TheConfigurator,
                                                GetSourceVolumeName(),
                                                VolumeChanges.GetOutOfSyncTimeStamp(),
                                                ulOutOfSyncErrorCode,
                                                ErrorStringForResync,
                                                m_pDeviceStream->GetName()))
        {
            bReportedSuccessfully = true;
            VolumeChanges.SetResyncRequiredProcessed();
        }
        else
        {
            bReportedSuccessfully = false;
            DebugPrintf(SV_LOG_ERROR,"FAILED: Volume: %s. Unable to report resync required to CX. @LINE %d in FILE %s\n",GetSourceVolumeName().c_str(),LINE_NO,FILE_NAME);
        }
    }
    else
    {
        bReportedSuccessfully = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    return bReportedSuccessfully;
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

        int endiannessChk = 1;
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
                    Changes.GetData(), Changes.GetDataLength(), volStream.MoreData(), numbufs , GetSourceVolumeName().c_str());

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
                    if (1 == endiannessChk++)
                    {
                        unsigned int dataFormatFlags;
                        if (Changes.Size() >= sizeof(dataFormatFlags))
                        {
                            inm_memcpy_s((void*)&dataFormatFlags, sizeof(dataFormatFlags), Changes.GetData(), sizeof(dataFormatFlags));
                            DebugPrintf(SV_LOG_DEBUG, "%s, data Format Flags=[0x%x], device=%s, LitttleEndianDataFormatFlags=[0x%x]\n", FUNCTION_NAME, dataFormatFlags, GetSourceVolumeName().c_str(), LitttleEndianDataFormatFlags);
                            if (dataFormatFlags != LitttleEndianDataFormatFlags)
                            {
                                DebugPrintf(SV_LOG_ERROR, "%s, Invalid endian data format flags [0x%x], device=%s\n", FUNCTION_NAME, dataFormatFlags, GetSourceVolumeName().c_str());
                            }
                        }
                    }
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
        }

        if (LogNetworkFailures(iStatus))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to send timestamp and user tags data for %s.\n",
                GetSourceVolumeName().c_str());
        }
        /* PR # 5926 : START */
    } /* if (SV_SUCCESS == iStatus) */

    if (SV_FAILURE == iStatus)
    {
        // DebugPrintf(SV_LOG_ERROR,"FAILED: ProcessTagsOnly Failed . @LINE %d in FILE %s.\n", LINE_NO, FILE_NAME);
        NOTIFY_DRIVER_ON_NETWORK_FAILURE();

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

        if (!ReportIfResyncIsRequired(Changes) )
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to report resync required to CX. It will be retried for volume %s\n",
                        GetSourceVolumeName().c_str());
        }
        //TODO:Calculate Total size of Timestamp and Tags
        sizeOfHeaders = GetDiffFileSize(Changes,0); // For Headers putting sizeofchanges argument as 0
        m_pTransportStream->SetSizeOfStream(sizeOfHeaders);
        if ( SendTimestampsAndTags(Changes, preName, finalName) )
        {
            if (SV_SUCCESS == Changes.Commit(*m_pDeviceStream, m_SourceNameForDriverInput))
            {
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
            DebugPrintf(SV_LOG_DEBUG, "Failed to Send Timestams and Tags in the tso file. Volume: %s. Line: %d File: %s", GetSourceVolumeName().c_str(), LINE_NO, FILE_NAME);
            iStatus = SV_FAILURE;
        }
        /* PR # 5926 : START */
    } /* if (SV_SUCCESS == iStatus) */

    if (SV_FAILURE == iStatus)
    {
        // DebugPrintf(SV_LOG_ERROR,"FAILED: ProcessTimeStampsOnly Failed . @LINE %d in FILE %s.\n", LINE_NO, FILE_NAME);

        NOTIFY_DRIVER_ON_NETWORK_FAILURE();
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
        while (!IsStopRequested() && ((iStatus = Changes.Init(m_pMetadataReadBufLen)) != SV_SUCCESS))
        {
            DebugPrintf(SV_LOG_FATAL, "Failed to Initialize the VolumeChangeData structure. Retrying ...\n") ;
            WaitOnQuitEvent(SECS_TO_WAITON_ERROR);
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

                if (ATLUN_STATS_REQUEST::ATLUN_STATS_NOREQUEST != m_pVolumeSettings->atLunStatsRequest.type)
                {
                    if (m_pDeviceFilterFeatures->IsMirrorSupported())
                    {
                        if (SV_SUCCESS != IsATLUNStatsReqProcessed())
                        {
                            /* There are two reasons for failure:
                             * 1. ioctl fail
                             * 2. CX not up 
                             * In both the cases, wait for 2 seconds reasonable
                            */
                            DebugPrintf(SV_LOG_ERROR, "For volume %s, could not process at lun stats request. Retrying later\n", 
                                        GetSourceVolumeName().c_str());
                            WaitOnQuitEvent(SECS_TO_WAITON_ATLUNSTATS);
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "For volume %s, could not process " 
                                                  "AT lun stats request as mirroring "
                                                  "is not supported by driver\n", GetSourceVolumeName().c_str());
                    }
                }

                if (m_bProfileDiffsSize)
                    ResetPerDiffProfilerValues();
                if (m_fpProfileLog)
                    m_TimeBeforeGetDB = ACE_OS::gettimeofday(); 

                /* PR # 6254 : END */
                if ( ( (dbStatus = Changes.BeginTransaction(*m_pDeviceStream, m_SourceNameForDriverInput)) == DATA_AVAILABLE) &&
                     ( false == IsThrottled() ) && (!IsStopRequested() ) )
                {
                    if (!ReportIfResyncIsRequired(Changes) )
                    {
                        DebugPrintf(SV_LOG_WARNING, "Failed to report resync required to CX. It will be retried for volume %s\n",
                                    GetSourceVolumeName().c_str());
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
                            if(m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY)
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
                                    SizeOfChangesToSend             = 0;
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
                                        if(m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY)
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
                                                            m_readFailCnt++;
                                                            ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                                                            if ((currentTime.sec() - m_lastReadErrLogTime) > TheConfigurator->getVxAgent().getDiskReadErrorLogInterval())
                                                            {
                                                                DebugPrintf(SV_LOG_ERROR,
                                                                    "Volume: %s Replicate() stream error. Read failures since %dsec=%d. %s\n",
                                                                    GetSourceVolumeName().c_str(), TheConfigurator->getVxAgent().getDiskReadErrorLogInterval(), m_readFailCnt, m_pVolumeStream->GetLastErrMsg().c_str());
                                                                m_lastReadErrLogTime = ACE_OS::gettimeofday().sec();
                                                                m_readFailCnt = 0;
                                                            }
                                                            int volOpenErr = -1;
                                                            std::string errmsg;
                                                            if (!CheckVolumeStatus(volOpenErr, errmsg) && (volOpenErr != ENOENT))
                                                            {
                                                                DebugPrintf(SV_LOG_ERROR, "%s: sending VolumeReadFailureAlert, Error: %s.\n", FUNCTION_NAME, errmsg.c_str());
                                                                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFS_DRAINING, VolumeReadFailureAlert(GetSourceVolumeName()));
                                                            }
                                                            WaitOnQuitEvent(SECS_TO_WAITON_ERROR);/*Waiting for some time before restarting the send.*/
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
                            WaitOnQuitEvent(SECS_TO_WAITON_ERROR);
                        }
                    } /* end of if (true == bCheckPassed) */
                }
                else if ( Changes.Good() && Changes.IsTimeStampsOnly() && (DATA_0 == dbStatus) )
                {
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
                    DebugPrintf(sv,
                                "Involflt has returned ERR_BUSY for GetDB ioctl for volume %s. "
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
                                "Failed to retrieve dirty blocks from driver while volume %s is online. Waiting for %d seconds before retrying again.\n",
                                GetSourceVolumeName().c_str(),
                                SECS_TO_WAITON_ERROR);

                            WaitOnQuitEvent(SECS_TO_WAITON_ERROR);
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
                                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFS_DRAINING, VolumeReadFailureAlert(GetSourceVolumeName()));
                            }
                            WaitOnQuitEvent(SECS_TO_WAITON_ERROR);
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
                DebugPrintf(SV_LOG_WARNING,"WARNING: Volume: %s. Deleted incomplete file %s on cx.\n",GetSourceVolumeName().c_str(),fileName.c_str());
                iStatus = SV_SUCCESS ;
            } 
            LogNetworkFailures(iStatus);
        }
    }

    /* PR # 5926 : START */
    if (SV_FAILURE == iStatus)
    {
        // DebugPrintf(SV_LOG_ERROR, "FAILED: Failed to delete incomplete file %s on cx\n", fileName.c_str());
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

const std::string& ProtectedGroupVolumes::GetSourceDiskId() const
{
    return m_sProtectedDiskId;
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
const ENDPOINTS& ProtectedGroupVolumes::GetEndPoints() const
{
    return *(m_pEndPoints.get());
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
    if(m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY)
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
        m_WaitForDBNotify = (1 == TheConfigurator->getVxAgent().getWaitForDBNotify());
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
            
            if (!LogNetworkFailures(iStatus))
            {
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
    unsigned int rpoThreshold = 0;
    unsigned long idleWaitTime = MAX_IDLE_WAIT_TIME;

    rpoThreshold = TheConfigurator->getRpoThreshold( GetSourceVolumeName() );
    if(rpoThreshold != 0)
    {
        assert(rpoThreshold >= 0);
        idleWaitTime = (MIN_IDLE_WAIT_TIME + 2* rpoThreshold);
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
    
    string sVolName = this->GetSourceDiskId();
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
        WaitOnQuitEvent( SECS_TO_WAITON_ERROR ) ;
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
            DebugPrintf( SV_LOG_ERROR, "Out of Order Differential file is being observed for volume %s. Not processing.\n",
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
        CurrDiffFileName = TheConfigurator->getVxAgent().getInstallPath();
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
                m_pCompress->m_UnCompressedFileName = TheConfigurator->getVxAgent().getInstallPath();
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
        FilePreAndCompDiffFiles = TheConfigurator->getVxAgent().getInstallPath();
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
    if (TheConfigurator->getVxAgent().getProfileDiffs())
    {
        std::string ProfilerLog;
#ifdef SV_WINDOWS
        ProfilerLog = TheConfigurator->getVxAgent().getInstallPath();
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
    std::string profiledifferentialrateoption = TheConfigurator->getVxAgent().ProfileDifferentialRate();
    if ((InmStrCmp<NoCaseCharCmp>(PROFILE_DIFFERENTIAL_RATE_OPTION_YES, profiledifferentialrateoption) == 0) || 
        (InmStrCmp<NoCaseCharCmp>(PROFILE_DIFFERENTIAL_RATE_OPTION_WITH_COMPRESSION, profiledifferentialrateoption) == 0))
    {
        std::string DiffsRateProfilerLog;
#ifdef SV_WINDOWS
        DiffsRateProfilerLog = TheConfigurator->getVxAgent().getInstallPath();
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
    if((j = finalName.find( PreRemoteNamePrefix )) != string::npos)
    {
        finalName.replace( j, PreRemoteNamePrefix.length(), FinalRemoteNamePrefix );
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
        std::string checksumDirectory = TheConfigurator->getVxAgent().getTargetChecksumsDir() ;

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
    
    if(m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY)
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


int ProtectedGroupVolumes::IsATLUNStatsReqProcessed(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());
    int iStatus = SV_FAILURE;

    if (ATLUN_STATS_REQUEST::ATLUN_STATS_LASTIO_TS == m_pVolumeSettings->atLunStatsRequest.type)
    {
        DebugPrintf(SV_LOG_DEBUG, "The request came for at lun statistics is ATLUN_STATS_LASTIO_TS\n");
        iStatus = ProcessLastIOATLUNRequest();
    }
     
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME, GetSourceVolumeName().c_str());
    return iStatus;
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
        std::string hostId = TheConfigurator->getVxAgent().getHostId();
        std::string diffLocation = TheConfigurator->getVxAgent().getDiffTargetSourceDirectoryPrefix() +
                                    ACE_DIRECTORY_SEPARATOR_STR_A +  hostId;
        std::string cacheLocation = TheConfigurator->getVxAgent().getDiffTargetCacheDirectoryPrefix() +
                                    ACE_DIRECTORY_SEPARATOR_STR_A + hostId;
        std::string searchPattern = TheConfigurator->getVxAgent().getDiffTargetFilenamePrefix();
        volumeIter = (*volumeGroupIt).volumes.begin();
        volumeEnd = (*volumeGroupIt).volumes.end();

        std::string deviceName;
        for (/* empty*/; volumeIter != volumeEnd; ++volumeIter) 
        {
            m_TgtVolume = (*volumeIter).second.deviceName;
            VOLUME_SETTINGS::endpoints_iterator endPointsIter = volumeIter->second.endpoints.begin();
            m_SrcCapacity = volumeIter->second.sourceCapacity;
            deviceName = endPointsIter->deviceName;
            m_CDPSettings = TheConfigurator->getCdpSettings(m_TgtVolume);
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
    return (m_pVolumeSettings->transportProtocol == TRANSPORT_PROTOOL_MEMORY);
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
    if ((now-m_LastDiffsRatePrintTime) >= TheConfigurator->getVxAgent().ProfileDifferentialRateInterval())
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

    m_datapathThrottleCnt++;
    m_cumulativeTimeSpentInThrottleInMsec += ttlmsec;
    if ((currentTimeInSec.count() - m_prevDatapathThrottleLogTime) >= DatapathThrottleLogIntervalSec)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n", FUNCTION_NAME, msg.c_str());

        if (m_prevDatapathThrottleLogTime)
        {
            std::stringstream ss;
            ss << "device " << GetSourceDiskId() << " throttled " << m_datapathThrottleCnt << " times in last "
                << currentTimeInSec.count() - m_prevDatapathThrottleLogTime
                << " sec total throttle time=" << m_cumulativeTimeSpentInThrottleInMsec << " milisec";
            DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n", FUNCTION_NAME, ss.str().c_str());
        }

        m_prevDatapathThrottleLogTime = currentTimeInSec.count();
        m_datapathThrottleCnt = 0;
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
        DebugPrintf(SV_LOG_ERROR, "%s: Health IssueCode is empty.\n ", FUNCTION_NAME);
        return;
    }

    // recurring health issue
    if (healthIssueCode == m_strThrottleHealthCode)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Skipping recurring Health IssueCode %s.\n ", FUNCTION_NAME, healthIssueCode.c_str());
        return;
    }

    //Raise a Health Issue here
    if (m_healthCollatorPtr.get() != NULL)
    {
        //fill in params for the disk level Health Issue object
        std::map < std::string, std::string>params;

        params.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::ObservationTime,
            m_healthCollatorPtr->GetCurrentObservationTime()));//throttled time

        AgentDiskLevelHealthIssue diskHealthIssue(GetSourceDiskId(), healthIssueCode, params);

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
        DebugPrintf(SV_LOG_ERROR, "Failed to get HealthCollator object and hence not able to raise Health Issue %s for the disk %s\n",
            healthIssueCode.c_str(), GetSourceDiskId().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
void ProtectedGroupVolumes::HandleDatapathThrottle(const ResponseData &rd, const DATA_SOURCE &SourceOfData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::stringstream sserr;
    const int defaultttl = 1000 * SECS_TO_WAITON_ERROR;
    SV_ULONGLONG ttlmsec = 0;

    m_bThrottled = true;

    ThrottleNotifyToDriver();
    sserr << FUNCTION_NAME << ": device " << GetSourceDiskId() << " data source " << STRDATASOURCE[SourceOfData];

    //Note: If Cumulatvie Throttle Health Issue is being raised by PS, then no need to raise ResyncThrottle Health Issue
    if (rd.headers.find(CxpsTelemetry::CUMULATIVE_THROTTLE) != rd.headers.end())
    {
        sserr << " is cumulative throttled";
        //For V2A Legacy, cumulative throttle health issue is raised by PS
    }
    else
    {
        if (rd.headers.find(CxpsTelemetry::DIFF_THROTTLE) != rd.headers.end())
        {
            sserr << " is diff throttled";
        }
        else
        {
            sserr << " is throttled but throttle info is missing in ResponseData.";
        }
        SetHealthIssue(AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode);
    }

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

    if (rd.responseCode == CLIENT_RESULT_NOSPACE)
    {
        HandleDatapathThrottle(rd, SourceOfData);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: differential upload failed source stream=%s, transport status=%u\n",
            FUNCTION_NAME, STRDATASOURCE[SourceOfData], m_pTransportStream->GetLastError());

        NOTIFY_DRIVER_ON_NETWORK_FAILURE();
        WaitOnQuitEvent(SECS_TO_WAITON_ERROR);/*Waiting for some time before restarting the send.*/
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

/*
*FUNCTION NAME : LogNetworkFailures
*
* DESCRIPTION : Rate control network failure logging.
*
* INPUT PARAMETERS : SV_SUCCESS or SV_FAILURE
*
* OUTPUT PARAMETERS : NONE
*
* NOTES :
*
* return value : return true if logged else false
*
*/
bool ProtectedGroupVolumes::LogNetworkFailures(int iStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool status = false;

    if (iStatus == SV_FAILURE)
    {
        m_networkFailCnt++;
        long long currentTime = ACE_OS::gettimeofday().sec();
        long long elapsedTime = currentTime - m_lastNetworkErrLogTime;

        if ((m_lastNetworkErrLogTime) &&
            (elapsedTime > TheConfigurator->getVxAgent().getTransportErrorLogInterval()))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: encountered %d network errors in last %lld sec.\n",
                FUNCTION_NAME,
                m_networkFailCnt,
                elapsedTime);

            m_lastNetworkErrLogTime = currentTime;
            m_networkFailCnt = 0;
            status = true;
        }
        else if (!m_lastNetworkErrLogTime)
        {
            m_lastNetworkErrLogTime = currentTime;
            status = true;
        }
    }
    else
    {
        m_lastNetworkErrLogTime = 0;
        m_networkFailCnt = 0;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

//For Churn Metrics collection
const string ProtectedGroupVolumes::GetHostId()
{
    return std::string(m_pHostId);
}

const std::string& ProtectedGroupVolumes::GetDriverName()
{
    FilterDrvIfMajor fdif;
    m_strDriverName = fdif.GetDriverName(m_pVolumeSettings->devicetype);
    return m_strDriverName;
}

const DeviceFilterFeatures* ProtectedGroupVolumes::GetDeviceFilterFeatures()
{
    return m_pDeviceFilterFeatures;
}
DeviceStream* ProtectedGroupVolumes::GetDeviceStream() const
{
    return m_pDeviceStream;
}
int ProtectedGroupVolumes::GetDeviceType()
{
    return m_pVolumeSettings->GetDeviceType();
}
FilterDrvVol_t ProtectedGroupVolumes::GetSourceNameForDriverInput()
{
    return m_SourceNameForDriverInput;
}
SV_ULONGLONG ProtectedGroupVolumes::GetTransferredBytesToPS()
{
    return m_transferredBytesToPS;
}

bool ProtectedGroupVolumes::ResetThrottleHealthIssue()
{
    if (m_strThrottleHealthCode.empty()) return true;

    bool bStatus = false;
    if (m_healthCollatorPtr.get() != NULL)
    {
        bStatus = m_healthCollatorPtr->ResetDiskHealthIssue(AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode, GetSourceDiskId());

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
    //Compute the Json Health File path
    std::stringstream ssHealthPath;
    ssHealthPath << TheConfigurator->getVxAgent().getHealthCollatorDirPath();
    ssHealthPath << ACE_DIRECTORY_SEPARATOR_CHAR_A;
    ssHealthPath << NSHealthCollator::S2Prefix;
    ssHealthPath << NSHealthCollator::UnderScore;
    ssHealthPath << TheConfigurator->getVxAgent().getHostId();
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
            //check if throttle health issue is already there or not
            if (m_healthCollatorPtr->IsDiskHealthIssuePresent(AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode, GetSourceDiskId()))
            {
                m_strThrottleHealthCode = AgentHealthIssueCodes::DiskLevelHealthIssues::DRThrottle::HealthCode;
            }

            m_bThrottled = !m_strThrottleHealthCode.empty();

            DebugPrintf(m_bThrottled ? SV_LOG_ALWAYS : SV_LOG_DEBUG, "%s: throttle health issue %s.\n", FUNCTION_NAME, m_bThrottled ? "found" : "not found");
        }
    }
    catch (const std::exception &ex)
    {
        m_healthCollatorPtr.reset();

        DebugPrintf(SV_LOG_ERROR, "%s:for health file %s caught exception %s.\n",
            FUNCTION_NAME, ssHealthPath.str().c_str(), ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception.\n", FUNCTION_NAME);
    }
}
DevMetrics::DevMetrics( std::string hostId,
    std::string devId,
    std::string hostName,
    std::string biosId,
    SV_ULONGLONG prevCumBytesTracked,
    SV_ULONGLONG cumTagsDropped,
    SV_ULONGLONG prevCumSentBytes) :m_hostId(hostId),
    m_deviceId(devId),
    m_hostName(hostName),
    m_biosId(biosId),
    m_prevCumulativeTrackedBytes(prevCumBytesTracked),
    m_cumulativeDroppedTagsCount(cumTagsDropped),
    m_prevCumulativeSentBytes(prevCumSentBytes),
    m_timeStamp(std::string("0000-00-00 00:00:00")),
    m_TrackedBytes(0),
    m_TrackedBytesRate(0),
    m_prevDroppdTagsCount(0),
    m_diskReadLatency(0),
    m_networkTransmitLatency(0),
    m_collectionInterval(0),
    m_bIsProtected(false),
    m_devType(0),
    m_diskName(std::string(""))
{
}

FilterDrvVol_t DevMetrics::GetFilterDrvVolName()
{
    return m_fltDrvVolName;
}
void DevMetrics::SetFilterDrvVolName(FilterDrvVol_t drvVolName)
{
    m_fltDrvVolName = drvVolName;
}

void DevMetrics::SetDeviceType(int devType)
{
    m_devType = devType;
}
std::string DevMetrics::GetDeviceId()
{
    return m_deviceId;
}
void DevMetrics::SetDeviceId(std::string strDevName)
{
    m_deviceId = strDevName;
}

void DevMetrics::SetCumulativeTrackedBytes(SV_ULONGLONG cumTrackedBytes)
{
    m_prevCumulativeTrackedBytes = cumTrackedBytes;
}
void DevMetrics::SetCumulativeSentBytes(SV_ULONGLONG cumSentBytes)
{
    m_prevCumulativeSentBytes = cumSentBytes;
}
SV_ULONGLONG DevMetrics::GetPrevCumulativeTrackedBytesCount()
{
    return m_prevCumulativeTrackedBytes;
}
SV_ULONGLONG DevMetrics::GetPrevCumulativeSentBytesCount()
{
    return m_prevCumulativeSentBytes;
}
void DevMetrics::SetTrackedBytes(SV_ULONGLONG trackedBytes, ACE_Time_Value elapsedTime)
{
    //We have to send Tracked Bytes/sec data rate to PS
    m_TrackedBytes = trackedBytes;
    m_TrackedBytesRate = (float)(trackedBytes / elapsedTime.sec());//Tracked bytes per second
}
void DevMetrics::SetSentBytes(SV_ULONGLONG sentBytes, ACE_Time_Value elapsedTime)
{
    //We have to send Sent Bytes/sec data rate to PS
    m_SentBytes = sentBytes;
    m_SentBytesRate = (float)(sentBytes / elapsedTime.sec());//Tracked bytes per second
}

SV_ULONGLONG DevMetrics::GetTrackedBytes()
{
    return m_TrackedBytes;
}
float DevMetrics::GetTrackedBytesRate()
{
    return m_TrackedBytesRate;
}

bool DevMetrics::IsThisDeviceBeingProtected()
{
    return m_bIsProtected;
}

FilterDrvVol_t DevMetrics::GetDriverDevName()
{
    return m_fltDrvVolName;
}

std::string DevMetrics::GetHostId()
{
    return m_hostId;
}

std::string DevMetrics::GetDiskName()
{
    m_diskName = DataCacher::GetDeviceNameForDeviceSignature(m_deviceId);
    boost::ireplace_all(m_diskName, "\\\\.\\PHYSICALDRIVE", "Disk");
    return m_diskName;
}

// HostId=<hostid>,DeviceId=<devid>,TimeStamp=<ts>,TrackedBytesCount=<no of bytes tracked>,TrackedBytesRate-<Rate of tracked bytes per second>
// Note: This contract of Names and sequence is enforced between Agent and PS.
// Any new field should be appended to the last and any change in the existing fields
// should also be propagated to PS.
void DevMetrics::AddChurnRecord()
{
    static SV_ULONGLONG earlierCumulativeBytesTracked;
    SV_ULONGLONG ulBytesTracked = 0;
    std::string strRecord;
    //Machine Name
    strRecord += "MachineName=";
    strRecord += std::string(m_hostName);
    strRecord += ",";
    
    //Host Id
    strRecord += "HostId=";
    strRecord += std::string(m_hostId);
    strRecord += ",";

    //Device Id
    strRecord += "DeviceId=";
    strRecord += GetDiskName();
    strRecord += ",";

    //Bios Id
    strRecord += "BiosId=";
    strRecord += m_biosId;
    strRecord += ",";

    //Time Stamp
    strRecord += "TimeStamp=";
    std::string strTimeStamp = GetTimeStampAsString();
    strRecord += strTimeStamp;
    strRecord += ",";

    //Number of Tracked bytes = Total Churned bytes - skipped bytes (the writes that correspond to page file and bitmap file should be skipped for tracking)
    strRecord += "TrackedBytes=";
    std::string strTrackedBytes = boost::lexical_cast<std::string>(m_TrackedBytes);//std::to_string(m_TrackedBytes);
    strRecord += strTrackedBytes;
    strRecord += ",";

    strRecord += "TrackedBytesRate=";
    std::string strTrackedBytesRate = boost::lexical_cast<std::string>(m_TrackedBytesRate);//std::to_string(m_TrackedBytesRate);
    strRecord += strTrackedBytesRate;
    //printing as Churn record added
    DebugPrintf(SV_LOG_DEBUG, "Churn record %s added\n", strRecord.c_str());
    //Insert this record to the list;
    m_vChurnRecords.push_back(strRecord);
}

// Note: This contract of Names and sequence is enforced between Agent and PS.
// Any new field should be appended to the last and any change in the existing fields
// should also be propagated to PS.
void DevMetrics::AddThroughputRecord()
{
    static SV_ULONGLONG earlierCumulativeBytesSent;
    SV_ULONGLONG ulBytesSent = 0;
    std::string strRecord;
    //Machine Name
    strRecord += "MachineName=";
    strRecord += std::string(m_hostName);
    strRecord += ",";

    //Host Id
    strRecord += "HostId=";
    strRecord += std::string(m_hostId);
    strRecord += ",";

    //Device Id
    strRecord += "DeviceId=";
    strRecord += GetDiskName();
    strRecord += ",";

    //Bios Id
    strRecord += "BiosId=";
    strRecord += m_biosId;
    strRecord += ",";

    //Time Stamp
    strRecord += "TimeStamp=";
    std::string strTimeStamp = GetTimeStampAsString();
    strRecord += strTimeStamp;
    strRecord += ",";

    //Number of Tracked bytes = Total Churned bytes - skipped bytes (the writes that correspond to page file and bitmap file should be skipped for tracking)
    strRecord += "SentBytes=";
    std::string strTrackedBytes = boost::lexical_cast<std::string>(m_SentBytes);//std::to_string(m_TrackedBytes);
    strRecord += strTrackedBytes;
    strRecord += ",";

    strRecord += "SentBytesRate=";
    std::string strTrackedBytesRate = boost::lexical_cast<std::string>(m_SentBytesRate);//std::to_string(m_TrackedBytesRate);
    strRecord += strTrackedBytesRate;
    //printing as Churn record added
    DebugPrintf(SV_LOG_DEBUG, "Throughput record %s added\n", strRecord.c_str());
    //Insert this record to the list;
    m_vThroughputRecords.push_back(strRecord);
}

std::string DevMetrics::GetTimeStampAsString()
{
    time_t t;
    char szTime[128];
    t = time(NULL);
    if (t ==0)
    {
        return std::string("0000-00-00 00:00:00");
    }
    strftime(szTime, 128, "%Y - %m - %d %H:%M:%S", localtime(&t));
    return std::string(szTime);
}

std::string DevMetrics::GetChurnStats()
{
    std::vector<std::string>::iterator iter = m_vChurnRecords.begin();
    for (size_t n = 0; (n < m_vChurnRecords.size()) && (iter != m_vChurnRecords.end());n++)
    {
        m_ChurnStatsAsString += (*iter);
        if ((n + 1) < m_vChurnRecords.size())
        {
            m_ChurnStatsAsString += "\n";
        }
        iter++;
    }
    return m_ChurnStatsAsString;
}
std::string DevMetrics::GetThroughputStats()
{
    std::vector<std::string>::iterator iter = m_vThroughputRecords.begin();
    for (size_t n = 0; (n < m_vThroughputRecords.size()) && (iter != m_vThroughputRecords.end()); n++)
    {
        m_ThroughputStatsAsString += (*iter);
        if ((n + 1) < m_vThroughputRecords.size())
        {
            m_ThroughputStatsAsString += "\n";
        }
        iter++;
    }
    return m_ThroughputStatsAsString;
}
void DevMetrics::SetProtectionState(bool bProtState)
{
    m_bIsProtected = bProtState;
}
void DevMetrics::ClearChurnRecords()
{
    m_ChurnStatsAsString = "";
    m_vChurnRecords.clear();
    m_ChurnRecord = "";
}
void DevMetrics::ClearThroughputRecords()
{
    m_ThroughputStatsAsString = "";
    m_vThroughputRecords.clear();
}
SV_ULONGLONG DevMetrics::GetCumulativeTrackedBytes()
{
    return m_prevCumulativeTrackedBytes;
}
SV_ULONGLONG DevMetrics::GetCumulativeDroppedTagsCount()
{
    return m_cumulativeDroppedTagsCount;
}
void DevMetrics::SetDroppedTagsCount(SV_ULONGLONG droppedTagsCount)
{
    m_cumulativeDroppedTagsCount = droppedTagsCount;
}
SV_ULONGLONG DevMetrics::GetPrevCumulativeDroppedTagsCount()
{
    return m_prevDroppdTagsCount;
}
StatsNMetricsManager::StatsNMetricsManager() : m_bStopMonitoring(false), m_QuitEvent(false,false) {}

StatsNMetricsManager::StatsNMetricsManager(const MAP_PROTECTED_GROUPS* pMapMasterProtectedPairs) : m_QuitEvent(false,false)
{
    m_pMapProtectedPairs = const_cast<MAP_PROTECTED_GROUPS*>(pMapMasterProtectedPairs);
    m_bStopMonitoring = false;
}

int StatsNMetricsManager::Init()
{
    int nStatus = SV_SUCCESS;

    m_earlierChurnCollTime = ACE_OS::gettimeofday();
    m_earlierChurnSendTime = m_earlierChurnCollTime;
    m_earlierThroughputCollTime = m_earlierChurnCollTime;
    m_earlierThroughputSendTime = m_earlierChurnSendTime;
    m_earlierDRTime = ACE_OS::gettimeofday();// earlierChurnCollTime;

    //Get the statistics and metrics collection intervals.Read Churn Stats Collection Interval from drscout.conf
    LocalConfigurator localConf;

    //Get Churn Statsistics collection interval
    m_nChurnNThroughputStatsCollInterval = localConf.getOMSStatsCollInterval();

    //Get Churn Statistics sending interval
    m_nChurnNThroughputStatsSendingIntervalToPs = localConf.getOMSStatsSendingIntervalToPS();

    //Get DR Metrics collection interval
    m_nDrMetricsCollInterval = localConf.getDRMetricsCollInterval();

    std::string hostName = Host::GetInstance().GetHostName(); 
    if (hostName.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Could not get the host name of this machine.\n", FUNCTION_NAME);
        return SV_FAILURE;
    }

    m_strHostId = localConf.getHostId();
    if (m_strHostId.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Could not get the HostId of this machine.\n", FUNCTION_NAME);
        return SV_FAILURE;
    }

    std::string biosId;
    bool bRet = GetBiosId(biosId);
    if (bRet != true)
    {
        DebugPrintf(SV_LOG_ERROR, "Could not get BiosId for this machine.\n");
        biosId = "NULL";
    }
    boost::erase_all(biosId, " ");

    if (m_pMapProtectedPairs)
    {
        std::map<ProtectedGroupVolumes*, Thread*>::iterator iter = m_pMapProtectedPairs->begin();
        for (/*empty*/; !IsStopRequested() && iter != m_pMapProtectedPairs->end(); iter++)
        {
            ProtectedGroupVolumes *pgv = iter->first;
            if (pgv)
            {
                boost::shared_ptr<DevMetrics> devMetrics(new DevMetrics(m_strHostId, pgv->GetSourceVolumeName(), &hostName[0], biosId, 0, 0, 0));
                if (devMetrics.get())
                {
                    devMetrics->SetFilterDrvVolName(pgv->GetSourceNameForDriverInput());
                    devMetrics->SetDeviceType(pgv->GetDeviceType());
                    devMetrics->SetProtectionState(pgv->IsProtecting());
                    devMetrics->SetDeviceId(pgv->GetSourceVolumeName());
                    m_mapProtGrpDevMetrics.insert(std::pair<ProtectedGroupVolumes*, boost::shared_ptr<DevMetrics> >(pgv, devMetrics));
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "\nFailed to create DevMetrics object for the corresponding ProtectedVolumeGroup object.\n");
                    return SV_FAILURE;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: ProtectedGroupVolume is null.\n", FUNCTION_NAME);
                return SV_FAILURE;
            }
        } 
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: No Protected Volumes.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", FUNCTION_NAME);
    return nStatus;
}

//From all devices collect the Churn records and keep it in memory
void StatsNMetricsManager::CollectChurnStats()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    MAP_PROTGRPS_DEVMETRICS::const_iterator iter = m_mapProtGrpDevMetrics.begin();
    for (/*empty*/; !IsStopRequested() && iter != m_mapProtGrpDevMetrics.end(); iter++)
    {
        ProtectedGroupVolumes *pg = iter->first;

        if (!pg->IsInitialized())
            continue;

        if (!pg->GetDeviceStream())
            continue;

        // reset the device name for driver input now that the device stream is created.
        if (iter->second->GetFilterDrvVolName().empty())
            iter->second->SetFilterDrvVolName(pg->GetSourceNameForDriverInput());

        SV_ULONGLONG currentCumulativeTrackedBytes;
        VolumeOps::VolumeOperator VolOp;		
        int nRet = VolOp.GetTotalTrackedBytes(
            *(pg->GetDeviceStream()), 
            (iter->second)->GetFilterDrvVolName(),
            currentCumulativeTrackedBytes);
        if (SV_SUCCESS == nRet)
        {
            SV_ULONGLONG prevCumTrackedBytes = 
                (iter->second)->GetPrevCumulativeTrackedBytesCount();
            
            SV_ULONGLONG trackedBytes = 0;

            if (currentCumulativeTrackedBytes < prevCumTrackedBytes)
            {
                DebugPrintf(SV_LOG_ERROR, "Driver returned less bytes tracked than what previously reported. \
                Mostly, \"clear differentials\" is called on driver.So, ignoring this value.\n");
            }
            else
            {
                (iter->second)->SetCumulativeTrackedBytes(currentCumulativeTrackedBytes);
                trackedBytes = currentCumulativeTrackedBytes - prevCumTrackedBytes;
                (iter->second)->SetTrackedBytes(trackedBytes, m_elapsedChurnCollTime);

                if ((0 == prevCumTrackedBytes) && (trackedBytes != 0))
                {
                    DebugPrintf(SV_LOG_DEBUG, "s2 agent is freshly launched and so the \
                    previous cumulative tracked data will be zero. This Churn stats \
                    collection interval's  data will not be sent to PS.\n");
                }
                else
                {
                    (iter->second)->AddChurnRecord();//Add this as a Churn record
                }
            }
        }
        else //ioctl failed as the driver has not returned the tracked bytes count
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to get the TotalTrackedBytes \
            count from the driver for the device %s.Hence not sending this data to PS \
            .\n", (iter->second)->GetDeviceId().c_str());
        }
    }
}
void StatsNMetricsManager::CollectThroughputStats()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    MAP_PROTGRPS_DEVMETRICS::const_iterator iter = m_mapProtGrpDevMetrics.begin();
    for (/*empty*/; !IsStopRequested() && iter != m_mapProtGrpDevMetrics.end(); iter++)
    {
        if (!iter->first->IsInitialized())
            continue;

        SV_ULONGLONG prevCumSentBytes = (iter->second)->GetPrevCumulativeSentBytesCount();
        SV_ULONGLONG currentCumulativeSentBytes = (iter->first)->GetTransferredBytesToPS();
        SV_ULONGLONG sentBytes = 0;
        if (currentCumulativeSentBytes < prevCumSentBytes)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Current cumulative sent bytes is less than previous value for device %s .So, ignoring this value.\n", FUNCTION_NAME, (iter->second)->GetDeviceId().c_str());
        }
        else
        {
            (iter->second)->SetCumulativeSentBytes(currentCumulativeSentBytes);
            sentBytes = currentCumulativeSentBytes - prevCumSentBytes;
            (iter->second)->SetSentBytes(sentBytes, m_elapsedThroughputCollTime);
            (iter->second)->AddThroughputRecord();//Add this as a Throughput record
        }
    }
}
int StatsNMetricsManager::SendChurnStatsToPS()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    //For every protected device,send the Churn records to PS
    int nStatus = SV_SUCCESS;
    std::string strChurnDest = "";
    MAP_PROTGRPS_DEVMETRICS::const_iterator iter = m_mapProtGrpDevMetrics.begin();
    for (/*empty*/; !IsStopRequested() && iter != m_mapProtGrpDevMetrics.end(); iter++)
    { 	
        //Churn data will be sent to CS in the following file name format...
        //home/svsystems/ChurStat/<hostid>_<timestamp>.churstat

        if (!iter->first->IsInitialized())
            continue;

        strChurnDest = "";
        strChurnDest += DestDir;
        strChurnDest += StrMonStat;
        strChurnDest += (iter->first)->GetHostId();
        strChurnDest += std::string("_");
        std::string device_id = (iter->second)->GetDeviceId();
        std::replace(device_id.begin(), device_id.end(), '/', '_');
        strChurnDest += device_id;
        strChurnDest += std::string("_");
        time_t currTime = time(NULL);
        std::string timeStr = boost::lexical_cast<std::string>(currTime);
        strChurnDest += timeStr;
        strChurnDest += ".churstat";
        std::string strData = (iter->second)->GetChurnStats();
        if (strData.length() == 0)
            continue;

        try
        {
            //Send this data to PS
            nStatus = (iter->first)->TransferMonData(strData.c_str(), strData.length(), strChurnDest, false, true);
            if (SV_SUCCESS != nStatus)
            {
                (iter->first)->m_sendChurnStatFailCnt++;
                ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                if ((currentTime.sec() - (iter->first)->m_lastChurnStatsSendFailTime) > TheConfigurator->getVxAgent().getDiskReadErrorLogInterval())
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Failed to send Churn statistics for device %s for %d times for last %d seconds.\n",
                        (iter->second)->GetDeviceId().c_str(), (iter->first)->m_sendChurnStatFailCnt, TheConfigurator->getVxAgent().getDiskReadErrorLogInterval());
                    (iter->first)->m_lastChurnStatsSendFailTime = ACE_OS::gettimeofday().sec();
                    (iter->first)->m_sendChurnStatFailCnt = 0;
                }
                nStatus = SV_FAILURE;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Successfully sent churn stats information to PS.\n");
                (iter->second)->ClearChurnRecords();//after successful sending, clear the in-memory Churn records
            }
        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s has thrown exception, error: %s\n", FUNCTION_NAME, e.what());
        }
    }
    return nStatus;
} 

int StatsNMetricsManager::SendThroughputStatsToPS()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    //For every protected device,send the Churn records to PS
    int nStatus = SV_SUCCESS;
    std::string strThrpDest = "";
    MAP_PROTGRPS_DEVMETRICS::const_iterator iter = m_mapProtGrpDevMetrics.begin();
    for (/*empty*/; !IsStopRequested() && iter != m_mapProtGrpDevMetrics.end(); iter++)
    {

        if (!iter->first->IsInitialized())
            continue;

        //Churn data will be sent to CS in the following file name format...
        //home/svsystems/ChurStat/<hostid>_<timestamp>.churstat
        strThrpDest = "";
        strThrpDest += DestDir;
        strThrpDest += ThrpStat;
        strThrpDest += (iter->first)->GetHostId();
        strThrpDest += std::string("_");
        std::string device_id = (iter->second)->GetDeviceId();
        std::replace(device_id.begin(), device_id.end(), '/', '_');
        strThrpDest += device_id;
        strThrpDest += std::string("_");
        time_t currTime = time(NULL);
        std::string timeStr = boost::lexical_cast<std::string>(currTime);
        strThrpDest += timeStr;
        strThrpDest += ".thrpstat";
        std::string strData = (iter->second)->GetThroughputStats();
        if (strData.length() == 0)
            continue;

        try 
        {
            //Send this data to PS
            nStatus = (iter->first)->TransferMonData(strData.c_str(), strData.length(), strThrpDest, false, true);
            if (SV_FAILURE == nStatus)
            {
                (iter->first)->m_sendThroughputStatFailCnt++;
                ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                if ((currentTime.sec() - (iter->first)->m_lastChurnStatsSendFailTime) > TheConfigurator->getVxAgent().getDiskReadErrorLogInterval())
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to send Throughput statistics for device %s for %d times for last %d seconds.\n",
                        (iter->second)->GetDeviceId().c_str(), (iter->first)->m_sendThroughputStatFailCnt, TheConfigurator->getVxAgent().getDiskReadErrorLogInterval());
                    (iter->first)->m_lastThroughputStatsSendFailTime = ACE_OS::gettimeofday().sec();
                    (iter->first)->m_sendThroughputStatFailCnt = 0;
                }
                nStatus = SV_FAILURE;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Successfully sent throughput stats information to PS.\n");
                (iter->second)->ClearThroughputRecords();//after successful sending, clear the in-memory Churn records
            }
        }
        catch (std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s has thrown exception, error: %s\n", FUNCTION_NAME, e.what());
        }
    }
    return nStatus;
}

void StatsNMetricsManager::SendHeartbeatToPS()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bStatus;
    MAP_PROTGRPS_DEVMETRICS::const_iterator iter = m_mapProtGrpDevMetrics.begin();
    for (/*empty*/; !IsStopRequested() && iter != m_mapProtGrpDevMetrics.end(); iter++)
    {
        if (!iter->first->IsInitialized())
            continue;

        bStatus = (iter->first)->SendHeartbeat();
        if (!bStatus)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Failed to send Heartbeat to PS for device %s.\n", FUNCTION_NAME, (iter->first)->GetSourceVolumeName().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Successfully sent heartbeat to PS for device %s.\n", FUNCTION_NAME, (iter->first)->GetSourceVolumeName().c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string StatsNMetricsManager::GetHostId()
{
    return m_strHostId;
}

void StatsNMetricsManager::DRProfilingSummary() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    try {
        std::vector<PrBuckets> drSmry;
        ProfilingSummaryFactory::summaryAsJson(drSmry);
        DRSummary jdrs;
        jdrs.Host = m_strHostId;
        jdrs.DrpdTagsSmry = m_drpdTagsSmry;
        jdrs.CumBytsTrkdSmry = m_cumTrkdBytesSmry;
        jdrs.DRSmry = drSmry;
        
        std::string jsmry = JSON::producer<DRSummary>::convert(jdrs);
        jsmry.erase(std::remove(jsmry.begin(), jsmry.end(), '\n'), jsmry.end());

        std::string smry = ";DRSS;" + jsmry + ";DRSE;";
        DebugPrintf(SV_LOG_ERROR, "%s\n", smry.c_str());
    }
    catch (JSON::json_exception& json)
    {
        errMsg += json.what();
        status = SVE_ABORT;
    }
    catch (std::exception &e)
    {
        errMsg += e.what();
        status = SVE_ABORT;
    }
    catch (...)
    {
        errMsg += "unknown error";
        status = SVE_ABORT;
    }
    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed, error: %s\n", FUNCTION_NAME, errMsg.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//********Get Dropped Tags count information by calling the ioctl IOCTL_INMAGE_GET_MONITORING_STATS*********
void StatsNMetricsManager::PrintDroppedTags(ProtectedGroupVolumes* pg, DevMetrics* dm)
{
    assert(NULL != pg);

    SV_ULONGLONG prevDroppedTagsCount = 0;
    SV_ULONGLONG currentCumulativeDroppedTagsCount = 0;

    VolumeOps::VolumeOperator VolOp;

    currentCumulativeDroppedTagsCount = 0;
    if (!pg->GetDeviceStream())
        return;
    
    // reset the device name for driver input now that the device stream is created.
    if (dm->GetFilterDrvVolName().empty())
        dm->SetFilterDrvVolName(pg->GetSourceNameForDriverInput());

    int nRet = VolOp.GetDroppedTagsCount(*(pg->GetDeviceStream()),
        dm->GetFilterDrvVolName(),
        currentCumulativeDroppedTagsCount);
    if (SV_SUCCESS == nRet)
    {
        prevDroppedTagsCount = dm->GetPrevCumulativeDroppedTagsCount();
        if (currentCumulativeDroppedTagsCount < prevDroppedTagsCount)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Got less dropped tags (%llu) count than previous(%llu) for device %s.\n",
                FUNCTION_NAME,
                currentCumulativeDroppedTagsCount,
                prevDroppedTagsCount,
                dm->GetDeviceId().c_str());
        }
        else
        {
            dm->SetDroppedTagsCount(currentCumulativeDroppedTagsCount);
            DebugPrintf(SV_LOG_ALWAYS, "Total number of tags dropped for the device %s is:%llu.\n",
                (dm->GetDeviceId()).c_str(),
                currentCumulativeDroppedTagsCount);
        }
        
        m_drpdTagsSmry[dm->GetDeviceId()] = boost::lexical_cast<std::string>(currentCumulativeDroppedTagsCount);
    }
    else //ioctl failed as the driver has not returned the tracked bytes count
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get dropped tags count for the device %s.\n", FUNCTION_NAME, dm->GetDeviceId().c_str());
        m_drpdTagsSmry[dm->GetDeviceId()] = "Failed";
    }
}

////********Get Cumulative Tracked Bytes information by calling the ioctl IOCTL_INMAGE_GET_MONITORING_STATS*********
void StatsNMetricsManager::PrintCumulativeTrackedBytesCount(ProtectedGroupVolumes *pg, DevMetrics* dm)
{
    SV_ULONGLONG currentCumulativeTrackedBytes = 0;

    VolumeOps::VolumeOperator VolOp;

    if (!pg->GetDeviceStream())
        return;
    
    // reset the device name for driver input now that the device stream is created.
    if (dm->GetFilterDrvVolName().empty())
        dm->SetFilterDrvVolName(pg->GetSourceNameForDriverInput());

    int nRet = VolOp.GetTotalTrackedBytes(*(pg->GetDeviceStream()),dm->GetFilterDrvVolName(),currentCumulativeTrackedBytes);

    if (SV_SUCCESS == nRet)
    {
        SV_ULONGLONG prevCumTrackedBytes =
            dm->GetPrevCumulativeTrackedBytesCount();

        SV_ULONGLONG trackedBytes = 0;

        if (currentCumulativeTrackedBytes < prevCumTrackedBytes)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Driver returned less bytes tracked: %llu than what previously reported:%llu for device %s.\n",
                FUNCTION_NAME,
                currentCumulativeTrackedBytes,
                prevCumTrackedBytes,
                dm->GetDeviceId().c_str());

            m_cumTrkdBytesSmry[dm->GetDeviceId()] = "less Bytes recvd than prev";
        }
        else
        {
            dm->SetCumulativeTrackedBytes(currentCumulativeTrackedBytes);
            prevCumTrackedBytes = dm->GetPrevCumulativeTrackedBytesCount();

            if (0 == prevCumTrackedBytes)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: s2 start, previous cumulative tracked data for device %s is zero.\n", FUNCTION_NAME, dm->GetDeviceId().c_str());
                m_cumTrkdBytesSmry[dm->GetDeviceId()] = "0 on startup";
            }
        }
        DebugPrintf(SV_LOG_ALWAYS, "Cumulative Tracked Bytes for device %s is %llu.\n",
            (dm->GetDeviceId()).c_str(),
            currentCumulativeTrackedBytes);
        
        m_cumTrkdBytesSmry[dm->GetDeviceId()] = boost::lexical_cast<std::string>(currentCumulativeTrackedBytes);
    }
    else //ioctl failed as the driver has not returned the tracked bytes count
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get the Cumulative TotalTrackedBytes count from the driver for the device %s.\n",
            FUNCTION_NAME, dm->GetDeviceId().c_str());

        m_cumTrkdBytesSmry[dm->GetDeviceId()] = "Failed to get TrckdByts";
    }
}
void StatsNMetricsManager::CollectDRMetrics()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    MAP_PROTGRPS_DEVMETRICS::const_iterator iter = m_mapProtGrpDevMetrics.begin();
    ProtectedGroupVolumes *pg = NULL;	
    for (/*empty*/; iter != m_mapProtGrpDevMetrics.end() && !IsStopRequested(); iter++)
    {
        pg = iter->first;
        if (pg)
        {  
            PrintDroppedTags(pg, iter->second.get());
            PrintCumulativeTrackedBytesCount(pg, iter->second.get());
        }
    }

    DRProfilingSummary();
    m_drpdTagsSmry.clear();
    m_cumTrkdBytesSmry.clear();
}

void StatsNMetricsManager::StopMonitoring()
{
   m_bStopMonitoring = true;
   m_QuitEvent.Signal(true);
}

WAIT_STATE StatsNMetricsManager::WaitOnQuitEvent(long int seconds, long int milli_seconds)
{
    WAIT_STATE waitRet = m_QuitEvent.Wait(seconds, milli_seconds);
    return waitRet;
}
bool StatsNMetricsManager::IsStopRequested()
{
    if (m_bStopMonitoring || m_QuitEvent.IsSignalled())
        return true;

    MAP_PROTECTED_GROUPS::const_iterator iter = m_pMapProtectedPairs->begin();
    while (iter != m_pMapProtectedPairs->end())
    {
        if ((iter->first)->IsStopRequested())
        {
            m_bStopMonitoring = true;
            break;
        }
        iter++;
    }
    return m_bStopMonitoring;
}
int StatsNMetricsManager::ManageLogCollectionForStatsAndMetrics()
{
    DebugPrintf(SV_LOG_ALWAYS, "Starting dedicated Statistics and Metrics collection thread in s2.\n");

    int iStatus = Init();
    if (iStatus != SV_SUCCESS)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to initialize the Churn, Throughput stats and DR Metrics log collection module.\n");
        return SV_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG, "Value of collection const is %d and sending constant is %d\n",
                m_nChurnNThroughputStatsCollInterval,
                m_nChurnNThroughputStatsSendingIntervalToPs);
    bool sendHeartbeat = true;
    while ((m_pMapProtectedPairs->size() > 0) && (!IsStopRequested()))
    {
        ACE_Time_Value curTime = ACE_OS::gettimeofday();
        sendHeartbeat = true;
        DebugPrintf(SV_LOG_DEBUG,
                    "Value of (curTime - m_earlierChurnCollTime).sec() const is %d\n",
                    (int)(curTime - m_earlierChurnCollTime).sec());
        if ((curTime - m_earlierChurnCollTime).sec() >= m_nChurnNThroughputStatsCollInterval)
        {
            m_elapsedChurnCollTime = (curTime - m_earlierChurnCollTime);
            CollectChurnStats();
            m_earlierChurnCollTime = curTime;
        }

        if ((curTime - m_earlierChurnSendTime).sec() >= m_nChurnNThroughputStatsSendingIntervalToPs)
        {
            iStatus = SendChurnStatsToPS();
            m_earlierChurnSendTime = curTime;
            sendHeartbeat = false;
        }

        if ((curTime - m_earlierThroughputCollTime).sec() >= m_nChurnNThroughputStatsCollInterval)
        {
            m_elapsedThroughputCollTime = (curTime - m_earlierThroughputCollTime);
            CollectThroughputStats();
            m_earlierThroughputCollTime = curTime;
        }

        if ((curTime - m_earlierThroughputSendTime).sec() >= m_nChurnNThroughputStatsSendingIntervalToPs)
        {
            iStatus = SendThroughputStatsToPS();
            m_earlierThroughputSendTime = curTime;
            sendHeartbeat = false;
        }

        if (sendHeartbeat)
        {
            SendHeartbeatToPS();
        }

        if ((curTime - m_earlierDRTime).sec() > m_nDrMetricsCollInterval)
        {
            CollectDRMetrics();
            m_earlierDRTime = curTime;
        }
        WaitOnQuitEvent(60,0);
        
    };

    return iStatus;
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
THREAD_FUNC_RETURN StatsNMetricsManager::Stop(const long int PARAM)
{
    DebugPrintf(SV_LOG_ALWAYS, "Stopping the dedicated Statistics and Metrics collection thread in s2.\n");
    return (THREAD_FUNC_RETURN)StopDedicatedStatsNMetricsCollThread(PARAM);
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
THREAD_FUNC_RETURN StatsNMetricsManager::Run()
{
    return (THREAD_FUNC_RETURN)ManageLogCollectionForStatsAndMetrics();
}

int StatsNMetricsManager::StopDedicatedStatsNMetricsCollThread(const long int liParam)
{
    int iStatus = SV_SUCCESS;
    StopMonitoring();
    return iStatus;
}
