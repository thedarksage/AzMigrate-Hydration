//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : fastsync.cpp
//
// Description:
//

#include <iostream>
#include <exception>
#include <sstream>
#include <vector>
#include <list>
#include <string>

#include <locale>
#include <iomanip>
#include <functional>

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

#include "hostagenthelpers_ported.h"
#include "fastsync.h"
#include "hashcomparedata.h"
#include "dataprotectionexception.h"
#include "basicio.h"
#include "theconfigurator.h"
#include "localconfigurator.h"
#include "fileconfigurator.h"
#include "volumeinfo.h"
#include "dataprotectionutils.h"
#include "dpdrivercomm.h"
#include "cdpplatform.h"
#include "configurevxagent.h"
#include "configwrapper.h"
#if SV_WINDOWS
#include "hostagenthelpers.h"
#include "cluster.h"
#endif
#include "globs.h"
#include "convertorfactory.h"
#include "svproactor.h"
#include "inm_md5.h"
#include "inmdefines.h"
#include "cdpvolumeutil.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmalertdefs.h"
#include "usecfs.h"
#ifdef SV_UNIX
#include "persistentdevicenames.h"
#endif
#include "HealthCollator.h"
#include "AgentHealthIssueNumbers.h"

#include "azurecanceloperationexception.h"
#include "azureinvalidopexception.h"
#include "replicationpairoperations.h"
#include "error.h"

using namespace std;

#define HEARTBEAT_INTERVAL_IN_SEC 60
#define RESYNC_PROGRESS_COUNTERS_VALIDITY_IN_SEC 30
#define RESYNC_STATUS_IN_PROGRESS "IN_PROGRESS"
#define RESYNC_STATUS_COMPLETED "COMPLETED"
#define HEARTBEAT_INTERVAL_IN_SEC 60

//
// ProcessedBytes/numberOfBlocksSynced = ( fully matched bytes from checksum processor +
//                          applied bytes from sync applier             +
//                          partial matched bytes from sync applier )
// At completion of resync, ProcessedBytes/numberOfBlocksSynced = disk size
//
uint64_t ResyncProgressCounters::GetResyncProcessedBytes() const
{
    return GetFullyMatchedBytesFromProcessHcdTask() +
        GetAppliedBytesFromProcessSyncDataTask() +
        GetPartialMatchedBytesFromProcessSyncDataTask();
}

void FastSync::ResyncProgressInfo::Heartbeat()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();
    if (currentTime > (m_PrevHeartBeatTime + boost::chrono::seconds(HEARTBEAT_INTERVAL_IN_SEC)))
    {
        boost::mutex::scoped_lock guard(m_CxTransportLock);
        if (currentTime > (m_PrevHeartBeatTime + boost::chrono::seconds(HEARTBEAT_INTERVAL_IN_SEC)))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Sending heartbeat.\n", FUNCTION_NAME);
            if (m_CxTransport->heartbeat())
            {
                m_PrevHeartBeatTime = currentTime;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s failed, transport status: %s.\n", FUNCTION_NAME, m_CxTransport->status());
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool FastSync::ResyncProgressInfo::ListFile(const std::string& fileSpec, FileInfosPtr &fileInfos, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    
    boost::mutex::scoped_lock guard(m_CxTransportLock);
    bool retval = true;
    if (!m_CxTransport->listFile(fileSpec, *fileInfos.get()))
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": listFile failed for " << fileSpec << ", transport status: " << m_CxTransport->status() << '.';
        errMsg = sserror.str();
        retval = false;
    }
    else
    {
        m_PrevHeartBeatTime = boost::chrono::steady_clock::now();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

bool FastSync::ResyncProgressInfo::DeleteFile(const std::string& remoteFileName, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    boost::mutex::scoped_lock guard(m_CxTransportLock);
    bool retval = true;
    if (!m_CxTransport->deleteFile(remoteFileName))
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": deleteFile failed for " << remoteFileName << ", transport status: " << m_CxTransport->status() << '.';
        errMsg = sserror.str();
        retval = false;
    }
    else
    {
        m_PrevHeartBeatTime = boost::chrono::steady_clock::now();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

bool FastSync::ResyncProgressInfo::GetResyncProgressCounters(ResyncProgressCounters& resyncProgressCounters, std::string &errmsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    bool retval = true;

    boost::mutex::scoped_lock guard(m_CxTransportLock);

    std::stringstream ss;

    if (!m_FastSync->Source())
    {
        if (!m_FastSync->initSharedBitMapFile(m_ResyncProgressCounters.m_ChecksumProcessedMap,
            m_FastSync->getHcdProcessMapFileName(), m_CxTransport, true))
        {
            ss << "HcdProcess bitMap initialization failed, CxTransport status: " << m_CxTransport->status();
            retval = false;
        }

        if (!m_ResyncProgressCounters.m_SyncDataAppliedMap)
        {
            m_ResyncProgressCounters.m_SyncDataAppliedMap = m_FastSync->m_syncDataAppliedMap;
        }
    }

    if (m_FastSync->Source())
    {
        if (retval && !m_FastSync->initSharedBitMapFile(m_ResyncProgressCounters.m_SyncDataAppliedMap,
            m_FastSync->getSyncApplyMapFileName(), m_CxTransport, true))
        {
            if (!ss.str().empty()) ss << ", ";
            ss << "SyncApplied bitMap initialization failed, CxTransport status: " << m_CxTransport->status();
            retval = false;
        }

        if (!m_ResyncProgressCounters.m_ChecksumProcessedMap)
        {
            m_ResyncProgressCounters.m_ChecksumProcessedMap = m_FastSync->m_checksumProcessedMap;
        }
    }

    if (!ss.str().empty())
    {
        errmsg = ss.str();
    }
    else
    {
        resyncProgressCounters = m_ResyncProgressCounters;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

// --------------------------------------------------------------------------
// constructor
// TODO: jobId should be part of the volumeGroupSettings and configurator
// --------------------------------------------------------------------------
FastSync::FastSync(VOLUME_GROUP_SETTINGS const & volumeGrpSettings, const AGENT_RESTART_STATUS agentRestartStatus)
: DataProtectionSync(volumeGrpSettings, false),
m_HashCompareDataSize(GetConfigurator().getFastSyncHashCompareDataSize()),
m_bDICheck(GetConfigurator().getDICheck()),
m_bIsProcessClusterPipeEnabled(GetConfigurator().IsProcessClusterPipeEnabled()),
m_maxHcdsAllowdAtCx(GetConfigurator().getMaxHcdsAllowdAtCx()),
m_secsToWaitForHcdSend(GetConfigurator().getSecsToWaitForHcdSend()),
m_VolumeSize(0),
m_ResyncStaleFilesCleanupInterval(GetConfigurator().getResyncStaleFilesCleanupInterval()),
m_ShouldCleanupCorruptSyncFile(GetConfigurator().ShouldCleanupCorruptSyncFile()),
m_maxCSComputationThreads(GetConfigurator().getMaxFastSyncProcessThreads()),
m_readBufferSize(GetConfigurator().getFastSyncReadBufferSize()),
m_compareHcd(GetConfigurator().compareHcd()),
m_agentRestartStatus(agentRestartStatus),
m_IsMobilityAgent(GetConfigurator().isMobilityAgent()),
m_LogResyncProgressInterval(GetConfigurator().getLogResyncProgressInterval()),
m_ResyncUpdateInterval(GetConfigurator().getResyncUpdateInterval()),
m_ResyncSlowProgressCheckInterval(GetConfigurator().getResyncSlowProgressThreshold()),
m_ResyncNoProgressCheckInterval(GetConfigurator().getResyncNoProgressThreshold())
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	if (Settings().isAzureDataPlane()) {
        m_MaxSyncChunkSize = GetConfigurator().getFastSyncMaxChunkSizeForE2A();
	}
	else{
        m_MaxSyncChunkSize = GetConfigurator().getFastSyncMaxChunkSize();
	}

    m_startHcdTaskAndWait[0] = &FastSync::startHcdTaskAndWait;
    m_startHcdTaskAndWait[1] = &FastSync::startHcdTaskAndWaitInFsResync;

    m_shouldDoubleBitmap[0] = &FastSync::ShouldDoubleBitmap;
    m_shouldDoubleBitmap[1] = &FastSync::ShouldDoubleBitmapInFsResync;

    m_downloadAndProcessMissingHcd[0] = &FastSync::downloadAndProcessMissingHcd;
    m_downloadAndProcessMissingHcd[1] = &FastSync::downloadAndProcessMissingHcdInFsResync;

    m_areHashesEqual[0] = &FastSync::ReturnHashesAsUnequal;
    m_areHashesEqual[1] = &FastSync::IsHashEqual;

    m_ResyncProgressInfo = boost::make_shared<ResyncProgressInfo>(this);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

// --------------------------------------------------------------------------
// starts protecting
// --------------------------------------------------------------------------
void FastSync::Start()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    //Moved from constructor to start
    std::ostringstream msg;

    bool isnewvirtualvolume = false;
    std::string dev_name = GetDeviceName();
    std::string sparsefile;
#ifdef SV_UNIX
    dev_name = GetDiskNameForPersistentDeviceName(std::string(dev_name), m_VolumesCache.m_Vs);
#endif
    if (!IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(), sparsefile, isnewvirtualvolume)))
    {
        dev_name = sparsefile;
    }
    m_ProtectedData.reset(new cdp_volume_t(dev_name, isnewvirtualvolume, DeviceType(), &m_VolumesCache.m_Vs));

    // Enable Retries on failures (required for fabric multipath device failures)
    m_ProtectedData->EnableRetry(CDPUtil::QuitRequested, GetConfigurator().getVolumeRetries(), GetConfigurator().getVolumeRetryDelay());
    VOLUME_SETTINGS volumeSettings = Settings();
    VOLUME_SETTINGS::TARGET_DATA_PLANE targetDataPlane = volumeSettings.getTargetDataPlane();


    if (!Source()) {

		if (volumeSettings.isUnSupportedDataPlane()){
            DebugPrintf(SV_LOG_ERROR, "Received invalid target data plane for replication; repliation for target device %s cannot proceed.\n", volumeSettings.deviceName.c_str());
            Idle(30);
            return;
        }

        if (volumeSettings.isInMageDataPlane()) {

            //This condition added to check if replication state is not in pogress then exit from the dataprotection

            // Bug #7189 - Hide the volume only if the visibility is false
            if (VOLUME_SETTINGS::PAIR_PROGRESS == volumeSettings.pairState && !(volumeSettings.visibility))
            {
				// for disk, we are not allowing unhide_ro,unhide_rw operations
				// so we can always go ahead and make sure to offline the disk in RW mode.
				if (volumeSettings.GetDeviceType() == VolumeSummary::DISK){

					if (!m_ProtectedData->Hide())
					{
						TargetVolumeUnmountFailedAlert a(volumeSettings.deviceName);
                        SendAlert(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
						Idle(120);
						throw DataProtectionException(a.GetMessage() + "\n");
					}
				}
				else{

					VOLUME_STATE VolumeState = GetVolumeState(volumeSettings.deviceName.c_str());
					if (VolumeState != VOLUME_HIDDEN)
					{
					    if (!m_ProtectedData->Hide())
					    {
					        TargetVolumeUnmountFailedAlert a(volumeSettings.deviceName);
                            SendAlert(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
					        Idle(120);
					        throw DataProtectionException(a.GetMessage() + "\n");
					    }
					    std::string formattedcxname(volumeSettings.deviceName);
					    FormatVolumeNameForCxReporting(formattedcxname);
					    bool status = false;
					    /**
					     * 1 TO N sync: May not need to anything since target
					     */
					    bool rv = updateVolumeAttribute((*TheConfigurator), UPDATE_NOTIFY, formattedcxname, VOLUME_HIDDEN, "", status);
					    if (!rv || !status)
					    {
					        DebugPrintf(SV_LOG_ERROR, "FAILED: Volume hide operation succeeded for %s but failed to update CX with volume status.\n", GetDeviceName().c_str());
					    }
					}
				}
            }
            else
            {
                Idle(30);
                return;
            }
        }

    }

    /* TODO: m_ProtectedData->Open is not needed at all ?
     *       Is this to block other processes from accessing
     *       device ? */

    LocalConfigurator localConfigurator;
	bool useUnBufferedIo = localConfigurator.useUnBufferedIo();
#ifdef SV_SUN
    useUnBufferedIo=false;
#endif

#ifdef SV_AIX
    useUnBufferedIo=true;
#endif

    /* TODO: for hcd generation, open volume in read mode */
    BasicIo::BioOpenMode openMode;
    if (Source())
    {
		openMode = cdp_volume_t::GetSourceVolumeOpenMode(Settings().devicetype);
    }
    else
    {
        openMode = BasicIo::BioRWExisitng | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioSequentialAccess;
        if (useUnBufferedIo)
        {
#ifdef SV_WINDOWS
            openMode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
            openMode |= BasicIo::BioDirect;
#endif
        }
    }

    m_ProtectedData->Open(openMode); // TODO: for dp rcm should this checked at start in main?

    if (!m_ProtectedData->Good())
    {
        msg << "FastSync constructor ProtectedData open " << GetDeviceName() << " failed: " << m_ProtectedData->LastError() << '\n';
        throw DataProtectionException(msg.str(), SV_LOG_WARNING);
    }

    setVolumeSize(Settings().sourceCapacity);

    if (0 == m_VolumeSize)
    {
        msg << "For device name:" << GetDeviceName()
            << ", source capacity:" << Settings().sourceCapacity
            << ", is zero from settings\n";
        throw DataProtectionException(msg.str());
    }

    //Added by BSR for Fast Sybc Target Based Checksum
    if (VOLUME_SETTINGS::SYNC_FAST_TBC != Settings().syncMode)
    {
        std::ostringstream msg;
        msg << "FastSync::FastSync sync mode mismatch: " << Settings().syncMode << '\n';
        throw DataProtectionException(msg.str(), SV_LOG_WARNING);
    }
    //End here
    if (!CreateFullLocalCacheDir(CleanupFilePattern))
    {
        return;
    }

    if (!GetSyncDir() && !IsTransportProtocolAzureBlob())
    {
        return;
    }

    SetShouldCheckForCacheDirSpace();
    /*
     *  Added for fixing overshoot issue with bitmaps
     *  By Suresh
     */
    CreateBitMapDir();
    SyncStart();

    Stop();

    // Bug 2100 [dataprotection should clean the files under application directory on exit.]
    // however on exception, we may still leave behind some files
    CleanupDirectory(CacheDir().c_str(), CleanupFilePattern.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}
/*
 * resyncstart notify will be called when the hcd transfer map contains all zeros.
 * if the hcd transfer map tells it already sent some hcd files it means source already notified
 * resync start time to CX.
 *
 *
 */
bool FastSync::ResyncStartNotifyIfRequired()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = true;
    bool shouldResyncStartNotify = true;

    CxTransport::ptr cxTransport(new CxTransport(Protocol(), TransportSettings(), Secure(), getCfsPsData().m_useCfs, getCfsPsData().m_psId));

    FileInfosPtr fileInfos(new FileInfos_t);

    boost::tribool tb = cxTransport->listFile(getHcdTransferMapFileName(), *fileInfos.get());
    if (!tb)
    {
        std::stringstream msg;
        msg << "Getting file list to know whether hcdtransfermap is present has failed: " << cxTransport->status() << '\n';
        if (m_agentRestartStatus == AGENT_FRESH_START)
        {
            throw DataProtectionNWException(msg.str());
        }
        throw DataProtectionNWException(msg.str(), SV_LOG_DEBUG);
    }
    else if (tb)
    {
        /* File is present */
        DebugPrintf(SV_LOG_DEBUG, "Hcd transfer map is present in CX. Hence not doing resync start notify again\n");
        shouldResyncStartNotify = false;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Hcd transfer map is not present in CX. Hence doing resync start notify\n");
    }

    if (shouldResyncStartNotify)
    {
        DebugPrintf(SV_LOG_DEBUG, "Sending Resync start time to CX\n");
        bRet = ResyncStartNotify();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}
// --------------------------------------------------------------------------
// starts sync'ing
// --------------------------------------------------------------------------
void FastSync::SyncStart()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    // In the target based checksum, both source and target uses this php function call
    // and requires PHP implementation accordingly.
    //jobIdOffsetPair = TheConfigurator->getVxAgent().getVolumeCheckpoint(GetDeviceName());
    /* Added by BSR for Fast Sync TBC
       The functionality moved to DataProtectionSync Class */

    /** Fast Sync TBC - BSR
     *  JobId() == 0 should be checked at both target and source.
     *  Earlier this check is done only at source **/
    // PR #800
    if (boost::iequals(JobId(), "0"))
    {
        DebugPrintf(SV_LOG_ALWAYS,
            "%s for device name %s exiting ... Looks like Pair is undergoing state change. Jobid is zero\n",
            FUNCTION_NAME,
            GetDeviceName().c_str());
        QuitRequested(5);
        return;
    }
    if (Source())
    {
        // send resync start notification
        if (!ResyncStartNotifyIfRequired())
            return;
    }

    if ((GetResyncCopyMode() != VOLUME_SETTINGS::RESYNC_COPYMODE_FILESYSTEM) &&
        (GetResyncCopyMode() != VOLUME_SETTINGS::RESYNC_COPYMODE_FULL))
    {
        DebugPrintf(SV_LOG_ERROR, "resync copy mode %s is incorrect in settings for volume %s\n", StrResyncCopyMode[GetResyncCopyMode()],
            GetDeviceName().c_str());
        return;
    }

    /** Fast Sync TBC - BSR **/
    startTargetBasedChecksumProcess();
    /** End of the Fast Sync TBC - changes **/
    return;
}
/** Fast Sync TBC - BSR
 *  This function would be invoked if both agents are 4.2 only.
 *  ON SOURCE SIDE:
 *  This would be little different from source based checksum implementation.
 *  This class should generate sync files in Target based checksum model against
 *  sdni generation in source based checksum model
 *  ON TARGET SIDE:
 *  Generate Checksum files
 *  Process Sync Files
 * ELIMINATED:
 * SDNI generation and processing
 **/
void FastSync::startTargetBasedChecksumProcess(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    m_clusterParamsPtr.reset(new ClusterParams(this));

    CxTransport::ptr cxTransport(new CxTransport(Protocol(), TransportSettings(), Secure(), getCfsPsData().m_useCfs, getCfsPsData().m_psId));

    if (Source())
    {
        openCheckumsFile();
        
        if (!initSharedBitMapFile(m_checksumProcessedMap, getHcdProcessMapFileName(), cxTransport, true))
        {
            throw DataProtectionException("Process BitMap Initialization failed\n");
        }

        if (IsNoDataTransferSync())
        {
            if (sendResyncEndNotifyAndResyncTransition())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Successfully moved to step 2 by skipping HCD processing\n", FUNCTION_NAME, JobId().c_str());
            } 
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Reconciliation:%s failed to move to step 2 in No Data transfer sync\n", FUNCTION_NAME, JobId().c_str());
            }

        }
        else
        {
            //ProcessHcdTask processHcdTask(this, &m_ThreadManager);
            m_procHCDTask.reset(new ProcessHcdTask(this, m_checksumProcessedMap, &m_ThreadManager));
            if (-1 == m_procHCDTask->open())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "FAILED FastSync::SyncStart processHcdTask.open: %d\n",
                    ACE_OS::last_error());
                return;
            }
            m_generateClusterInfoTask.reset(new GenerateClusterInfoTask(this, &m_ThreadManager));
            if (-1 == m_generateClusterInfoTask->open())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "FAILED FastSync::SyncStart GenerateClusterInfoTask.open: %d\n",
                    ACE_OS::last_error());
                PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
            }
            WaitForSyncTasks(*m_procHCDTask.get(), *m_generateClusterInfoTask.get());
        }
    }
    else
    {
        if (!IsStartTimeStampSetBySource())
        {
            /** Source has not yet updated resync start time stamp.
                Self Close. Service would restart in the next cycle.
                **/
			DebugPrintf(SV_LOG_ALWAYS,
				"%s: Source has not yet updated resync start time stamp. Self Close. Service would restart in the next cycle.\n",
				FUNCTION_NAME);
            QuitRequested(5);
            return;
        }

		try{

			if (Settings().isAzureDataPlane()){

                bool isSignatureCalculationRequired = IsNoDataTransferSync() && IsMarsCompatibleWithNoDataTranfer() ? false : true;

				m_azureagent_ptr.reset(new AzureAgentInterface::AzureAgent(
					Settings().GetEndpointHostName(),
					Settings().GetEndpointHostId(),
					Settings().GetEndpointDeviceName(),
					Settings().GetEndpointRawSize(),
					m_HashCompareDataSize,
					JobId(),
                    (AzureAgentInterface::AzureAgentImplTypes)GetConfigurator().getAzureImplType(),
                    GetConfigurator().getMaxAzureAttempts(),
                    GetConfigurator().getAzureRetryDelayInSecs(),
                    isSignatureCalculationRequired));

				m_azureagent_ptr->StartResync();
			}
		}
		catch (const AzureCancelOpException & ce) {

            std::stringstream ssmsg;
            !m_azureagent_ptr ?
                ssmsg << FUNCTION_NAME << ": Failed to create Azure agent. Error: " << ce.what() :
                ssmsg << FUNCTION_NAME << ": Azure agent failed in StartResync. Error: " << ce.what();
            HandleAzureCancelOpException(ssmsg.str());

			throw DataProtectionException(ssmsg.str(), SV_LOG_ERROR);
		}
		catch (const AzureInvalidOpException & ie) {

			std::stringstream ssmsg;
            !m_azureagent_ptr ?
                ssmsg << FUNCTION_NAME << ": Failed to create Azure agent. Error: " << ie.what() :
                ssmsg << FUNCTION_NAME << ": Azure agent failed in StartResync. Error: " << ie.what();
            HandleAzureInvalidOpException(ssmsg.str(), ie.ErrorCode());

			throw DataProtectionException(ssmsg.str(), SV_LOG_ERROR);
		}
		catch (ErrorException & ee){
            std::stringstream ssmsg;
            !m_azureagent_ptr ?
                ssmsg << FUNCTION_NAME << ": Failed to create Azure agent. Error: " << ee.what() :
                ssmsg << FUNCTION_NAME << ": Azure agent failed in StartResync. Error: " << ee.what();
            throw DataProtectionException(ssmsg.str(), SV_LOG_ERROR);
		}

		

        bool bCleanUp = PerformCleanUpIfRequired();

        if (false == bCleanUp)
        {
            QuitRequested(120);
            std::stringstream ssmsg;
            if (m_agentRestartStatus == AGENT_FRESH_START)
            {
                ssmsg << FUNCTION_NAME << ": Failed to clean up previous state.";
                throw DataProtectionNWException(ssmsg.str());
            }
            ssmsg << FUNCTION_NAME << ": Failed to clean up previous state.";
            throw DataProtectionNWException(ssmsg.str(), SV_LOG_DEBUG);
        }

        if (!initSharedBitMapFile(m_syncDataAppliedMap, getSyncApplyMapFileName(), cxTransport, true))
        {
            std::stringstream ssmsg;
            ssmsg << FUNCTION_NAME << ": Failed to initialize Sync Applied BitMap.";
            throw DataProtectionException(ssmsg.str());
        }
        setChunkSize(m_syncDataAppliedMap->getBitMapGranualarity());
        m_previousProgressBytes = 0;

        RetrieveCdpSettings();

        if (!initSharedBitMapFile(m_checksumTransferMap, getHcdTransferMapFileName(), cxTransport, true))
        {
            std::stringstream ssmsg;
            ssmsg << FUNCTION_NAME << ": Failed to initialize Hcd Transfer BitMap.";
            throw DataProtectionException(ssmsg.str());
        }

        if (!IsNoDataTransferSync() && !(this->*m_startHcdTaskAndWait[IsFsAwareResync()])(m_checksumTransferMap))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to start / wait for hcd tasks\n", FUNCTION_NAME);
        }
    }

    CompleteResync();

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s \n", FUNCTION_NAME);
}

bool FastSync::IsStartTimeStampSetBySource() const
{
    return !(0 == Settings().srcResyncStarttime);
}

void FastSync::RetrieveCdpSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_cdpsettings = TheConfigurator->getCdpSettings(GetDeviceName());
    if (m_cdpsettings.Status() == CDP_ENABLED)
    {
        ACE_stat db_stat = { 0 };
        if (sv_stat(getLongPathName(m_cdpsettings.Catalogue().c_str()).c_str(), &db_stat) == 0)
        {
            m_cdpresynctxn_ptr.reset(new cdp_resync_txn(cdp_resync_txn::getfilepath(m_cdpsettings.CatalogueDir()), JobId()));
            if (!m_cdpresynctxn_ptr->init())
            {
                throw DataProtectionException("Retention resync transaction file initialization failed\n");
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s \n", FUNCTION_NAME);
}

void FastSync::HandleAzureCancelOpException(const std::string & msg) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    //  cancel the current resync and restart fresh session
    //  this is achieved by just sending resync required flag to cs

    const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
    std::stringstream ssmsg(msg);
    ssmsg << ". Marking resync for the device " << GetDeviceName() << " with resyncReasonCode=" << resyncReasonCode;
    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, ssmsg.str().c_str());
    ResyncReasonStamp resyncReasonStamp;
    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

    CDPUtil::store_and_sendcs_resync_required(GetDeviceName(), msg.c_str(), resyncReasonStamp);
    CDPUtil::QuitRequested(600);

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s \n", FUNCTION_NAME);
}

void FastSync::HandleAzureInvalidOpException(const std::string & msg, const long errorCode) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // pause the replication pair
    // example, after a failover operation you cannot upload files, start resync etc operations

    ReplicationPairOperations pairOp;
    stringstream replicationPauseMsg;
    replicationPauseMsg << msg  << " Replication for " << GetDeviceName()
        << "is paused due to unrecoverable error("
        << std::hex << errorCode
        << ").";

    if (pairOp.PauseReplication(TheConfigurator, GetDeviceName(), INMAGE_HOSTTYPE_TARGET, replicationPauseMsg.str().c_str(), errorCode))
    {
        DebugPrintf(SV_LOG_ERROR, replicationPauseMsg.str().c_str());
    }
    else {
        DebugPrintf(SV_LOG_ERROR, "Request '%s' to CS failed", replicationPauseMsg.str().c_str());
    }

    CDPUtil::QuitRequested(600);

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s \n", FUNCTION_NAME);
}

void FastSync::HandleInvalidBitmapError(const std::string & errmsg) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ResyncBitmapsInvalidAlert a(GetDeviceName());
    const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
    std::stringstream ssmsg(a.GetMessage());
    ssmsg << ". Error=" << errmsg;
    ssmsg << ". Marking resync for the target device " << GetDeviceName() << " with resyncReasonCode=" << resyncReasonCode;
    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, ssmsg.str().c_str());
    ResyncReasonStamp resyncReasonStamp;
    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
    
    //  cancel the current resync and restart fresh session
    //  this is achieved by just sending resync required flag to cs
    CDPUtil::store_and_sendcs_resync_required(GetDeviceName(), ssmsg.str().c_str(), resyncReasonStamp);

    SendAlert(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
    CDPUtil::QuitRequested(120);

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s \n", FUNCTION_NAME);

    throw DataProtectionException(ssmsg.str(), SV_LOG_SEVERE);
}


/**Fast Sync TBC - BSR
 *  In older fast sync, volume size would be read by source from file system apis
 *  before generating checksum files. In TBC, checksum are generated at target
 *  side. Target now reads volume size from volume settings.
 **/
void FastSync::setVolumeSize(SV_ULONGLONG volSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    m_VolumeSize = volSize;
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}


// --------------------------------------------------------------------------
// sends the HashCompareData to the server
// --------------------------------------------------------------------------
void FastSync::SendHashCompareData(boost::shared_ptr<HashCompareData> hashCompareData, SV_LONGLONG offset, SV_ULONGLONG bytesRead, CxTransport::ptr cxTransport, Profiler &pHcdFileUpload)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::ostringstream msg;

    if (hashCompareData->Count() > 0 && !QuitRequested())
    {
        std::string preTarget, target;
        nextHcdFileNames(preTarget, target);

        ThrottleSpace();

        pHcdFileUpload->start();
        if (!SendData(cxTransport, preTarget, hashCompareData->Data(), hashCompareData->DataLength(), CX_TRANSPORT_NO_MORE_DATA, COMPRESS_NONE))
        {
            msg << "FAILED FastSync::SendHashCompareData SendData\n";
            throw DataProtectionException(msg.str());
        }
        pHcdFileUpload->stop(hashCompareData->DataLength());

        /*
         *  Fixing overshoot problem with bitmaps
         *  By Suresh
         *  The reanme and updating checksumoffset are now seperate operations..
         */
        boost::tribool tb = cxTransport->renameFile(preTarget, target, COMPRESS_NONE);
        if (!tb || CxTransportNotFound(tb))
        {
            msg << "FAILED FastSync::Rename Failed From " << preTarget << " To " << target << ": " << cxTransport->status() << '\n';
            throw DataProtectionException(msg.str());
        }
        DebugPrintf(SV_LOG_DEBUG, "HCD FileName: %s\n", target.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}


// --------------------------------------------------------------------------
// writes sync data to the volume
// TODO: needs to support any combination of "headers" currently only
// supports the following 2:
//
//                SVD_PREFIX
//                SVD_HEADER1
// these      +-- SVD_PREFIX
// repeating  |   SVD_DIRTY_BLOCK DATA --+ these repeating
// until EOF  +-- DATA                 --+ SVD_PREFIX.count times
// --------------------------------------------------------------------------
/** Fast Sync TBC changes - BSR
 *  Added two additional parameters char* source, SV_UINT sourceLength to handle
 *  inline uncompression for resync
 **/
void FastSync::ApplySyncData(std::string const & syncDataName, long long& totalBytesApplied, std::string const & remoteName, cdp_volume_t::Ptr volume, CDPV2Writer::Ptr cdpv2writer, CxTransport::ptr &cxTransport, char* source, SV_ULONG sourceLenth)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // make sure we delete syncDataName on exit (exception or normal)
    // set this up before BasicIo so that the file will be closed before delete is called
    boost::shared_ptr<void> guard((void*)NULL, boost::bind(&FastSync::RemoveLocalFile, this, syncDataName));
    std::ostringstream msg;
    SV_ULONGLONG offset;
    SharedBitMapPtr  bm;//= NULL;
    bm = m_syncDataAppliedMap;

    if (IsRcmMode() && Settings().isAzureDataPlane())
    {
        DiffPtr change_metadata(new (nothrow) Differential);
        if (!change_metadata)
        {
            std::ostringstream msg;
            msg << FUNCTION_NAME << " failed: memory allocation for " << syncDataName << " failed.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
            throw DataProtectionException(msg.str());
        }

        if (!cdpv2writer->parseOnDiskFile(syncDataName, change_metadata))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to parse sync file %s for volume %s.\n", FUNCTION_NAME, syncDataName.c_str(), volume->GetName().c_str());

            // This Sync is corrupted
            std::stringstream excmsg;
            excmsg << FUNCTION_NAME << " - Sync file: " << syncDataName << " for target volume: " << volume->GetName() << " is corrupted cannot apply.\n";
            throw CorruptResyncFileException(excmsg.str());
        }

        if (!IsResyncDataAlreadyApplied(&offset, bm.get(), change_metadata))
        {
            ApplyChanges(syncDataName);

            totalBytesApplied = change_metadata->DrtdsLength();
        }
    }
    else
    {
        try {
            if (!cdpv2writer->Applychanges(syncDataName,
                totalBytesApplied,
                source,
                sourceLenth,
                &offset, bm.get()))
            {
                std::ostringstream msg;
                msg << "CDPV2Writer::Applychanges for " << syncDataName << " failed\n";
                throw DataProtectionException(msg.str());
            }
        }
        catch (CorruptResyncFileException& dpe) {
            // re-throw CorruptResyncFileException
            throw dpe;
        }
    }

    if (totalBytesApplied != 0)
    {
        stringstream msg;
        SV_ULONG matchedBytes = 0;
        if (m_VolumeSize % getChunkSize() != 0 && (SV_ULONG)(offset / getChunkSize()) == totalChunks() - 1)
        {
            matchedBytes = m_VolumeSize % getChunkSize() - totalBytesApplied;
            msg << "Chunk Size " << m_VolumeSize % getChunkSize() << " Matched Bytes " << matchedBytes;
        }
        else
        {
            matchedBytes = getChunkSize() - totalBytesApplied;
            msg << "Chunk Size " << getChunkSize() << " Matched Bytes " << matchedBytes;
        }
        m_syncDataAppliedMap->markAsApplied(offset, totalBytesApplied, matchedBytes);
        DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
        DebugPrintf(SV_LOG_DEBUG, "Applied Bytes for chunk starting at " ULLSPEC " is " ULLSPEC "\n", offset, totalBytesApplied);
    }

    TransportOp top(TransportOp::DeleteFile, TRANSPORT_RETRY_COUNT,
        TRANSPORT_RETRY_DELAY, remoteName, true);
    /* TODO: no need of checking quit after this call as
     *       anyways we are quitting */
    if (!DoTransportOp(top, cxTransport))
    {
        if (!QuitRequested(0))
        {
            msg << "RemoveRemoteFile " << remoteName << " failed. " << '\n';
            throw DataProtectionException(msg.str());
        }
    }
    else
    {
        if (m_cdpresynctxn_ptr)
        {
            std::string fileBeingApplied = syncDataName;
            std::string::size_type idx = fileBeingApplied.rfind(".gz");
            if (std::string::npos != idx && (fileBeingApplied.length() - idx) == 3)
                fileBeingApplied.erase(idx, fileBeingApplied.length());
            std::string fileBeingAppliedBaseName = basename_r(fileBeingApplied.c_str());
            DebugPrintf(SV_LOG_DEBUG, "removing %s entry to resync_txn file.\n", fileBeingAppliedBaseName.c_str());
            m_cdpresynctxn_ptr->delete_entry(fileBeingAppliedBaseName);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


bool FastSync::SendHcdNoMoreFile(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    HashCompareData hashCompareData(1, m_MaxSyncChunkSize, m_HashCompareDataSize, HashCompareData::MD5, GetEndianTagSize());
    hashCompareData.Clear();

    std::string preName, newName, temp;
    nextHcdFileNames(preName, temp); //temp is not being used
    formatHcdNoMoreFileName(newName);
    newName = RemoteSyncDir() + newName;
    if (IsTransportProtocolAzureBlob())
    {
        preName = newName;
    }

    if (!SendData(cxTransport, preName, hashCompareData.Data(), hashCompareData.DataLength(), CX_TRANSPORT_NO_MORE_DATA, COMPRESS_NONE))
    {
        DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s SendData Returned Fail for %s with status %s\n"
            , FUNCTION_NAME, LINE_NO, FILE_NAME, preName.c_str(), cxTransport->status());
        return false;
    }

    /*
     *  Fixing overshoot problem with bitmaps
     *  By Suresh
     *  The rename and updating checkpoint offset are done independently..
     */
    if (!cxTransport->renameFile(preName, newName, COMPRESS_NONE))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to rename the file %s to %s with error %s\n", preName.c_str(), newName.c_str(), cxTransport->status());
        return false;
    }
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Sent HcdNoMoreFile[%s]\n", FUNCTION_NAME, JobId().c_str(), newName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
    return true;
}


template < class Task1>
void FastSync::waitForSyncTasks(Task1& task, InmPipeline *p)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool quit = false;
    int tasksDone = 0;

    while (!quit)
    {
        if (Quit() || QuitRequested(WindowsWaitTimeInSeconds))
        {
            break;
        }

        PerformResyncMonitoringTasks();

        if (p)
        {
            State_t st = p->GetState();
            if (STATE_NORMAL != st)
            {
                DebugPrintf(SV_LOG_ERROR, "pipeline state is %s, exception message is %s\n", StrState[st], p->GetExceptionMsg().c_str());
                Statuses_t ss;
                p->GetStatusOfStages(ss);
                PrintStatuses(ss, SV_LOG_ERROR);
                PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
            }
        }

        ACE_Message_Block *mb;
        ACE_Time_Value waitTime;
        if (-1 == m_MsgQueue.dequeue_head(mb, &waitTime))
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN)
            {
                continue;
            }
            break;
        }
        switch (mb->msg_type())
        {
        case SYNC_DONE:
            quit = true;
            break;

        case SYNC_EXCEPTION:
            quit = true;
            Stop();
            break;

        default:
            break;
        }
        mb->release();
    }

    if (p)
    {
        p->Destroy();
    }
    m_ThreadManager.wait();
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

//
// Remove all but latest stale pre_sync* files on PS IR cache
//
void FastSync::CleanupResyncStaleFilesOnPS()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    try
    {
        do
        {
            static boost::chrono::steady_clock::time_point s_prevCleanupTime = boost::chrono::steady_clock::time_point::min();
            const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();
            if (currentTime < s_prevCleanupTime + boost::chrono::seconds(m_ResyncStaleFilesCleanupInterval))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as ResyncStaleFilesCleanupInterval is yet to reach.\n", FUNCTION_NAME);
                break;
            }

            FileInfosPtr fileInfos = boost::make_shared<FileInfos_t>();
            std::string strmsg;

            if (Source())
            {
                const std::string preSyncFileSpec(RemoteSyncDir() + PreUnderscore + SyncDataPrefix);
                if (!m_ResyncProgressInfo->ListFile(preSyncFileSpec, fileInfos, strmsg))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, strmsg.c_str());
                    break;
                }
                if (fileInfos->size() > 1)
                {
                    // Sort to make sure latest file is at end.
                    std::sort(fileInfos->begin(), fileInfos->end());

                    // Always exclude a latest pre_completed_sync* file as it can be a current in-process file
                    fileInfos->pop_back();

                    DebugPrintf(SV_LOG_ALWAYS, "%s: %d stale %s%s files identified, attempting to cleanup\n", FUNCTION_NAME,
                        fileInfos->size(), PreUnderscore.c_str(), SyncDataPrefix.c_str());

                    FileInfos_t::iterator iter(fileInfos->begin());
                    for (/*empty*/; (iter != fileInfos->end()) && !QuitRequested(0); iter++)
                    {
                        if (!m_ResyncProgressInfo->DeleteFile(std::string(RemoteSyncDir() + iter->m_name), strmsg))
                        {
                            DebugPrintf(SV_LOG_WARNING, "%s: %s.\n", FUNCTION_NAME, strmsg.c_str());
                        }
                    }
                }
            }

            if (!Source() && m_ShouldCleanupCorruptSyncFile)
            {
                const std::string corruptSyncFileSpec(RemoteSyncDir() + CORRUPT UNDERSCORE WILDCARDCHAR);
                if (!m_ResyncProgressInfo->ListFile(corruptSyncFileSpec, fileInfos, strmsg))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, strmsg.c_str());
                    break;
                }
                if (fileInfos->size())
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: %d stale " CORRUPT UNDERSCORE WILDCARDCHAR " files identified, attempting to cleanup\n", FUNCTION_NAME,
                        fileInfos->size());

                    FileInfos_t::iterator iter(fileInfos->begin());
                    for (/*empty*/; (iter != fileInfos->end()) && !QuitRequested(0); iter++)
                    {
                        if (!m_ResyncProgressInfo->DeleteFile(RemoteSyncDir() + iter->m_name, strmsg))
                        {
                            DebugPrintf(SV_LOG_WARNING, "%s: %s.\n", FUNCTION_NAME, strmsg.c_str());
                        }
                    }
                }
            }

            s_prevCleanupTime = currentTime;

        } while (0);
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception\n", FUNCTION_NAME);
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

bool FastSync::IsTransportProtocolAzureBlob() const
{
    return Settings().transportProtocol == TRANSPORT_PROTOCOL_BLOB;
}

void FastSync::PerformResyncMonitoringTasks()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    try
    {
        UpdateResyncStats(); // Always update resync stats first.
        CheckResyncHealth();
        if (IsRcmMode())
        {
            CheckResyncState();
            UpdateResyncProgress();
            MonitorTargetToAzureHealth();
        }

        DumpResyncProgress();

        if (Quit() || QuitRequested(WindowsWaitTimeInSeconds))
        {
            return;
        }

        CleanupResyncStaleFilesOnPS();

        m_ResyncProgressInfo->Heartbeat();
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

// --------------------------------------------------------------------------
// waits for the sync tasks to complete
// it also checks for service quit requests
// if quit requested tells tasks to quit
// --------------------------------------------------------------------------
template <class Task1, class Task2>
void FastSync::WaitForSyncTasks(Task1& task1, Task2& task2)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool quit = false;

    int tasksDone = 0;
    while (!quit && tasksDone < 2)
    {
        if (Quit() || QuitRequested(WindowsWaitTimeInSeconds))
        {
            break;
        }

        PerformResyncMonitoringTasks();

        ACE_Message_Block *mb;
        ACE_Time_Value waitTime;
        if (-1 == m_MsgQueue.dequeue_head(mb, &waitTime))
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN)
            {
                continue;
            }
            break;
        }
        switch (mb->msg_type())
        {
        case SYNC_DONE:
            ++tasksDone;
            break;

        case SYNC_EXCEPTION:
            quit = true;
            Stop();
            break;

        default:
            break;
        }
        mb->release();
    }

    if (tasksDone < 2)
    {
        Stop(); // force any running tasks to exit
    }
    m_ThreadManager.wait();
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

void FastSync::DumpResyncProgress()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    try
    {
        if (Source())
        {
            DebugPrintf(SV_LOG_INFO, "%s called in source mode - ignoring.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
            return;
        }

        // Check for ResyncProgressInterval default is 15 mins.
        static boost::chrono::steady_clock::time_point prevUpdateTime = boost::chrono::steady_clock::time_point::min();
        const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();
        if (currentTime < (prevUpdateTime + boost::chrono::seconds(m_LogResyncProgressInterval)))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as LogResyncProgressInterval is yet to reach.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
            return;
        }

        std::stringstream ssProgressMsg;
        do
        {
            std::string errmsg;
            ResyncProgressCounters resyncProgressCounters;
            const std::string hcdPrefixName(RemoteSyncDir() + HcdPrefix);
            const std::string syncPrefixName(RemoteSyncDir() + SyncDataPrefix);
            const std::string preHcdPrefixName(RemoteSyncDir() + PreHcd);
            const std::string preSyncPrefixName(RemoteSyncDir() + PreSync);

            // Dump progress from hcd transfer, hcd process, sync applied map
            ssProgressMsg << "HcdTransfer Info:\n" << m_checksumTransferMap->getBitMapInfo() << "\n";

            if (!m_ResyncProgressInfo->GetResyncProgressCounters(resyncProgressCounters, errmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errmsg.c_str());
                errmsg.clear();
            }
            else
            {
                ssProgressMsg << "HCDProcess Info:\n" << resyncProgressCounters.m_ChecksumProcessedMap->getBitMapInfo() << "\n";
                ssProgressMsg << "SyncDataApplied Info:\n" << resyncProgressCounters.m_SyncDataAppliedMap->getBitMapInfo() << "\n";
            }

            // Dump pending files info
            FileInfosPtr fileInfos = boost::make_shared<FileInfos_t>();
            if (!m_ResyncProgressInfo->ListFile(hcdPrefixName, fileInfos, errmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errmsg.c_str());
                errmsg.clear();
            }
            else
            {
                ssProgressMsg << "Pending hcdCount=" << fileInfos->size() << ' ';
            }

            if (!m_ResyncProgressInfo->ListFile(syncPrefixName, fileInfos, errmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errmsg.c_str());
                errmsg.clear();
            }
            else
            {
                ssProgressMsg << "syncCount=" << fileInfos->size() << ' ';
            }

            if (!m_ResyncProgressInfo->ListFile(preHcdPrefixName, fileInfos, errmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errmsg.c_str());
                errmsg.clear();
            }
            else
            {
                ssProgressMsg << "preHcdCount=" << fileInfos->size() << ' ';
            }

            if (!m_ResyncProgressInfo->ListFile(preSyncPrefixName, fileInfos, errmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errmsg.c_str());
                errmsg.clear();
            }
            else
            {
                ssProgressMsg << "preSyncCount=" << fileInfos->size();
            }

        } while (0);

        if (!ssProgressMsg.str().empty())
        {
            std::stringstream sstmpprogmsg;
            sstmpprogmsg << "RESYNC PROGRESS START -----------------------------------------------------------------\n\n";
            sstmpprogmsg << ssProgressMsg.str();
            sstmpprogmsg << "\nRESYNC PROGRESS END -------------------------------------------------------------------\n";
            DebugPrintf(SV_LOG_ALWAYS, "%s:%s\n%s\n", FUNCTION_NAME, JobId().c_str(), sstmpprogmsg.str().c_str());

            prevUpdateTime = currentTime;
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

// --------------------------------------------------------------------------
// posts a message to the message queue
// --------------------------------------------------------------------------
void FastSync::PostMsg(int msgId, int priority)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Message_Block *mb = new ACE_Message_Block(0, msgId);
    mb->msg_priority(priority);
    m_MsgQueue.enqueue_prio(mb);
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}


// ProcessHcdTask
// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::ProcessHcdTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::ProcessHcdTask::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}


// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// for each diff info gets the diff data. if the data is retrieved
// successfull, psosts a message to the apply task
// --------------------------------------------------------------------------
int FastSync::ProcessHcdTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	DebugPrintf(SV_LOG_ALWAYS, "%s:%s ProcessHcd - disk read thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    std::string hcdNoMoreFile = completedHcdPrefix + m_FastSync->JobId() + noMore;
    // process completed_hcd_nomore.dat after processing all hcd files
    std::string hcdMissingFile;
    m_FastSync->formatHcdMissingFileName(hcdMissingFile);
    ACE_Time_Value lastUpdateTime = ACE_OS::gettimeofday(), currentTime = ACE_OS::gettimeofday();

    SendSyncDataWorkerPtr sendsyncdataWorker(new SendSyncDataWorker(
        m_FastSync,
        thr_mgr(),
        m_sendToReadThreadQ,
        m_pcstToSendThreadQ,
        m_pendingFileCount
        )
        );
    sendsyncdataWorker->open();

    PrimaryCSWorkerPtr primarycsWorker(new PrimaryCSWorker(m_FastSync, thr_mgr(), m_readThreadToPcstQ, m_pcstToSendThreadQ));
    primarycsWorker->open();

    MissingSyncProcessStatus status = SYNCMISSINGABSENT; // dummy val
    try
    {
        m_FastSync->setChunkSize(m_checksumProcessedMap->getBitMapGranualarity());
        m_prevMatchedBytesSent = 0;

        std::stringstream excmsg;
        if (!m_FastSync->IsReadBufferLengthValid(excmsg))
        {
            throw DataProtectionException(excmsg.str());
        }

        FileInfosPtr fileInfos(new FileInfos_t);

        std::string remoteDir = m_FastSync->RemoteSyncDir();
        std::string prefixName = remoteDir + HcdPrefix;

        bool bneedtoallocate = true;

        // Step: Read the number of check sums thread value
        /* TODO: Just reusing MaxFastSyncProcessChecksumThreads from drscout.conf
         * Also the max value it can have is just made 4. Need to discuss on this
         * max value as 4 */
        SV_UINT maxCSComputationTasks = m_FastSync->GetMaxCSComputationThreads();
        DebugPrintf(SV_LOG_DEBUG, "The number of HCD process threads configured = %u\n", maxCSComputationTasks);

        if (!allocateHCDBatch())
        {
            excmsg << "memory allocation failed for "
                << "volume data and hcd for volume: "
                << m_FastSync->GetDeviceName()
                << '\n';
            throw DataProtectionException(excmsg.str());
        }

        do
        {
            currentTime = ACE_OS::gettimeofday();
            if ((currentTime - lastUpdateTime).sec() >= m_FastSync->m_ResyncUpdateInterval)
            {
                updateProgress((currentTime - lastUpdateTime).sec());
                lastUpdateTime = ACE_OS::gettimeofday();
            }

            if (isHcdMissingPacketAvailable())
            {
                //m_FastSync->QuitRequested( TaskWaitTimeInSeconds ) ;
                //intervalTime += TaskWaitTimeInSeconds ;
                m_FastSync->Idle(TaskWaitTimeInSeconds);
                continue;
            }

            status = receiveProcessMissingSyncPacket();
            std::stringstream sstatus;
            sstatus << status;
            DebugPrintf(SV_LOG_DEBUG, "status of process sync missing is %s\n", sstatus.str().c_str());
            if (ERRORPROCESSINGSYNCMISSING == status)
            {
                excmsg << "failed to receive and process sync missing file\n";
                throw DataProtectionException(excmsg.str());
            }
            else if (SYNCMISSINGABSENT == status)
            {
                DebugPrintf(SV_LOG_DEBUG, "sync missing file is not present at CX\n");
            }
            else if (CHUNKSPENDING == status)
            {
                DebugPrintf(SV_LOG_DEBUG, "sync missing file is processed; some chunks have to be processed\n");
            }
            else if (TRANSITIONCOMPLETE == status)
            {
                DebugPrintf(SV_LOG_DEBUG, "transition happened to step 2\n");
                m_FastSync->updateProfilingSummary(true);

                // Reset slow/no health issue if set earlier.
                // If HI is resolved since last check and if resync is completed before next check then
                // remove resync related all HIs for this disk before exit.
                std::vector<std::string> vResetHICodes(g_IRissueCodes, g_IRissueCodes + INM_ARRAY_SIZE(g_IRissueCodes));
                // Reset Cummulative throttle issue if set earlier
                vResetHICodes.push_back(AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode);
                do
                {
                    if(!m_FastSync->ResetHealthIssues(vResetHICodes))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Resync is completed but failed to ResetHealthIssues, retrying.\n", FUNCTION_NAME);
                        // If not retried then HI will persist while resync is already completed.
                        // Retry until succeed or quit requested.
                        continue;
                    }
                } while (!m_FastSync->QuitRequested(WindowsWaitTimeInSeconds));

                break;
            }
            else
            {
                excmsg << "failed to receive and process sync missing file with unknown status\n";
                throw DataProtectionException(excmsg.str());
            }

            static long long lastIRMetricsReportTime = ACE_OS::gettimeofday().sec();
            if ((ACE_OS::gettimeofday().sec() - lastIRMetricsReportTime) > m_FastSync->GetConfigurator().getIRMetricsReportInterval())
            {
                m_FastSync->updateProfilingSummary();
                lastIRMetricsReportTime = ACE_OS::gettimeofday().sec();
            }

            if (0 != pendingFileCount())
            {
                // still processing last list of files, send a heartbeat
                // to avoid timing out this connection
                m_cxTransport->heartbeat();
            }
            else
            {
                m_pHcdList->start();
                if (!m_cxTransport->listFile(prefixName, *fileInfos.get()))
                {
                    DebugPrintf(SV_LOG_ERROR, "GetFileList in Function %s failed with: %s\n", FUNCTION_NAME, m_cxTransport->status());
                    m_FastSync->Stop();
                    break;
                }
                m_pHcdList->stop(0);

                if (1 == fileInfos->size())
                {
                    if (std::string::npos != (*fileInfos->begin()).m_name.find(hcdNoMoreFile))
                    {
                        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Processing HcdNoMoreFile[%s] as this is the only file list\n",
                            FUNCTION_NAME, m_FastSync->JobId().c_str(), hcdNoMoreFile.c_str());
                        SendMissingHcdsPacket();
                        continue;
                    }
                }

                setPendingFileCount(fileInfos->size());
                DebugPrintf(SV_LOG_DEBUG, "fileInfos->size() = %d\n", fileInfos->size());

                FileInfos_t::iterator iter(fileInfos->begin());
                FileInfos_t::iterator iterEnd(fileInfos->end());
                for ( /* empty */; (iter != iterEnd) && (!m_FastSync->QuitRequested(0)); ++iter)
                {
                    std::string& fileName = (*iter).m_name;

                    /** Fast Sync TBC - Job Id changes - BSR **/
                    if (!canProcess(fileName, m_FastSync->JobId()))
                    {
                        //Log the error and Delete the file.
                        DebugPrintf(SV_LOG_ERROR,
                            "File received %s from CX seems to be older file. Skipping and Deleting from CX\n",
                            fileName.c_str());
                        m_cxTransport->deleteFile(remoteDir + fileName);
                        --m_pendingFileCount;
                    }
                    else
                    {
                        if (fileName == hcdNoMoreFile ||
                            fileName == HcdNoMoreFileName ||
                            fileName == hcdMissingFile)
                        {
                            --m_pendingFileCount;
                        }
                        else
                        {
                            std::string hcdFile = remoteDir + fileName;
                            char* inBuffer;
                            const std::size_t bufferSize = (*iter).m_size;
                            size_t bytesReturned = 0;
                            m_pHcdDownoad->start();
                            if (m_cxTransport->getFile(hcdFile, bufferSize, &inBuffer, bytesReturned))
                            {
                                m_pHcdDownoad->stop(bufferSize);
                                ChecksumFile_t hcdDetails;
                                hcdDetails.fileName = hcdFile;

                                try
                                {
                                    hcdDetails.hashCompareData.reset(new HashCompareData(inBuffer, bufferSize));

                                    DebugPrintf(SV_LOG_DEBUG, "The hcdfilename is %s\n", hcdDetails.fileName.c_str());

                                    if (bneedtoallocate)
                                    {
                                        if (!allocateReadBufAndMsgBlks(m_FastSync->getReadBufferSize()))
                                        {
                                            excmsg << "memory allocation failed for read buffers, message blocks for reading volume: "
                                                << m_FastSync->GetDeviceName()
                                                << '\n';
                                            throw DataProtectionException(excmsg.str());
                                        }
                                        bneedtoallocate = false;
                                    }

                                    compareAndProcessHCD(&hcdDetails);
                                }
                                catch (CorruptResyncFileException& dpe)
                                {
                                    DebugPrintf(SV_LOG_ERROR, "%s encountered CorruptResyncFileException : %s\n", FUNCTION_NAME, dpe.what());
                                    InvalidResyncFileAlert a(m_FastSync->GetDeviceName(), hcdFile);
                                    m_FastSync->SendAlert(SV_ALERT_ERROR, SV_ALERT_MODULE_RESYNC, a);
                                    std::string corruptfile = remoteDir + CORRUPT UNDERSCORE + fileName;
                                    boost::tribool tb = m_cxTransport->renameFile(hcdFile, corruptfile, COMPRESS_NONE);
                                    if (!tb || CxTransportNotFound(tb))
                                    {
                                        DebugPrintf(SV_LOG_ERROR, "%s: Failed to prefix invalid hcd file %s with " CORRUPT UNDERSCORE ", with error %s\n",
                                            FUNCTION_NAME, hcdFile.c_str(), m_cxTransport->status());
                                    }
                                    else
                                    {
                                        DebugPrintf(SV_LOG_DEBUG, "%s: Prefixed invalid hcd with " CORRUPT UNDERSCORE "\n", FUNCTION_NAME);
                                    }
                                }
                            }
                            else
                            {
                                excmsg << "Retrieving Checksum file from CX failed with transport status " << m_cxTransport->status() << '\n';
                                throw DataProtectionException(excmsg.str());
                            }
                        }
                    }
                    currentTime = ACE_OS::gettimeofday();
                    if ((currentTime - lastUpdateTime).sec() > m_FastSync->m_ResyncUpdateInterval)
                    {
                        updateProgress((currentTime - lastUpdateTime).sec());
                        lastUpdateTime = ACE_OS::gettimeofday();
                    }
                }
            }
        } while (!m_FastSync->QuitRequested(WindowsWaitTimeInSeconds));

        m_pDiskRead->commit();

        thr_mgr()->wait_task(sendsyncdataWorker.get());
        thr_mgr()->wait_task(primarycsWorker.get());

        m_FastSync->PostMsg(SYNC_DONE, SYNC_NORMAL_PRIORITY);

        if (m_checksumProcessedMap->syncToPersistentStore(m_cxTransport) == SVS_OK &&
            m_checksumProcessedMap->getBytesNotProcessed() == 0)
        {
            m_FastSync->SetFastSyncMatchedBytes(m_checksumProcessedMap->getBytesProcessed());
        }
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), "%s: %s\n", FUNCTION_NAME, dpe.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, e.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }

    thr_mgr()->wait_task(sendsyncdataWorker.get());
    thr_mgr()->wait_task(primarycsWorker.get());

    if (TRANSITIONCOMPLETE != status)
    {
        m_FastSync->updateProfilingSummary();
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s ProcessHcd - disk read thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}


bool FastSync::ProcessHcdTask::allocateHCDBatch(void)
{
    setClusterSize();
    unsigned int nhcdsinbatch;
    INM_SAFE_ARITHMETIC(nhcdsinbatch = InmSafeInt<SV_UINT>::Type(m_FastSync->getReadBufferSize()) / m_clusterSize, INMAGE_EX(m_FastSync->getReadBufferSize())(m_clusterSize))
        /* by 2 since worst case */
        INM_SAFE_ARITHMETIC(nhcdsinbatch /= InmSafeInt<int>::Type(2), INMAGE_EX(nhcdsinbatch))
        return m_volReadBuffers.allocateHCDBatch(nhcdsinbatch);
}


void FastSync::ProcessHcdTask::setClusterSize(void)
{
    m_clusterSize = m_FastSync->GetClusterSizeFromVolume(m_volume);
}


void FastSync::ProcessHcdTask::updateProgress(const time_t& intervalTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (m_checksumProcessedMap->syncToPersistentStore(m_cxTransport) == SVS_OK &&
        m_prevMatchedBytesSent.value() != m_checksumProcessedMap->getBytesProcessed() &&
        m_checksumProcessedMap->getBytesNotProcessed() == 0 &&
        m_FastSync->SetFastSyncMatchedBytes(m_checksumProcessedMap->getBytesProcessed()))
    {
        DebugPrintf(SV_LOG_DEBUG, "Updating Matched Bytes Information after " ULLSPEC" seconds:"
            "\n Matched Bytes:" ULLSPEC"\n",
            intervalTime,
            m_checksumProcessedMap->getBytesProcessed());
        m_prevMatchedBytesSent = m_checksumProcessedMap->getBytesProcessed();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


string FastSync::ProcessHcdTask::getHcdProcessMapFileName()
{
    return m_FastSync->RemoteSyncDir() + mapHcdsProcessed + "_" + m_FastSync->JobId() + ".Map";
}

bool FastSync::ProcessHcdTask::isHcdMissingPacketAvailable()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bReturnValue;
    Files_t files;
    string missingHcdsPkt = m_FastSync->RemoteSyncDir() + completedHcdPrefix + m_FastSync->JobId() + missingSuffix;

    if (m_FastSync->GetFileList(m_cxTransport, missingHcdsPkt, files)) //If Missing Hcds Packet exist do not process anything
    {
        bReturnValue = files.size() == 1 ? true : false;
    }
    else
    {
        throw DataProtectionException("Get File List Failed\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bReturnValue;
}


FastSync::ProcessHcdTask::MissingSyncProcessStatus
FastSync::ProcessHcdTask::receiveProcessMissingSyncPacket()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    string missingSyncsPkt = m_FastSync->RemoteSyncDir() + completedSyncPrefix + m_FastSync->JobId() + missingSuffix;
    FileInfosPtr MissingSyncInfo(new FileInfos_t);
    MissingSyncProcessStatus status = ERRORPROCESSINGSYNCMISSING;

    std::stringstream msg;
    boost::tribool tbms = m_cxTransport->listFile(missingSyncsPkt, *MissingSyncInfo.get());
    if (!tbms)
    {
        msg << "FAILED: unable to find presence of sync missing file "
            << missingSyncsPkt << '\n';
        throw  DataProtectionException(msg.str());
    }
    else if (tbms)
    {
        /* File is present */
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s MissingSyncFile present in CX[%s]\n", FUNCTION_NAME, m_FastSync->JobId().c_str(), missingSyncsPkt.c_str());
        status = ProcessMissingSyncsPacket();
    }
    else
    {
        /* File is not present */
        DebugPrintf(SV_LOG_DEBUG, "File %s is not present in CX.\n", missingSyncsPkt.c_str());
        status = SYNCMISSINGABSENT;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}


void FastSync::ProcessHcdTask::SendMissingHcdsPacket()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    std::ostringstream msg;
    char * buffer = NULL;
    Files_t files;
    SV_ULONG fileSize = 0;
    string preMissingHcdsDatName = m_FastSync->RemoteSyncDir() + PreHcd + m_FastSync->JobId() + missingSuffix;
    string completedMissingHcdsDatName = m_FastSync->RemoteSyncDir() + completedHcdPrefix + m_FastSync->JobId() + missingSuffix;

    if (m_FastSync->IsTransportProtocolAzureBlob())
    {
        preMissingHcdsDatName = completedMissingHcdsDatName;
    }

    if (m_checksumProcessedMap->syncToPersistentStore(m_cxTransport) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync:Unable to persist Hcd Process Map\n");
    }

    if (!m_FastSync->GetFileList(m_cxTransport, completedMissingHcdsDatName, files))
    {
        msg << "FAILED FastSync::Failed GetFileList for file spec " << completedMissingHcdsDatName << " tal status : " << m_cxTransport->status() << endl;
        throw DataProtectionException(msg.str());
    }

    if (files.size() == 0)  //Send the missing hcds dat file if it doesnt already exist at cx..
    {
        if (m_checksumProcessedMap->syncToPersistentStore(m_cxTransport, preMissingHcdsDatName) != SVS_OK)
        {
            msg << "FAILED FastSync::SendMissingHcdsPacket, unable to persist pre missing hcd file: "
                << preMissingHcdsDatName << '\n';
            throw  DataProtectionException(msg.str());
        }

        /* This is a must because due to above, hcd process bitmap file name becomes the precompleted hcd
         * and every thing breaks */
        if (m_checksumProcessedMap->syncToPersistentStore(m_cxTransport, m_FastSync->getHcdProcessMapFileName()) != SVS_OK)
        {
            msg << "FAILED FastSync::SendMissingHcdsPacket, Unable to persist Hcd Process Map: "
                << m_FastSync->getHcdProcessMapFileName() << '\n';
            throw DataProtectionException(msg.str());
        }

        if (!m_FastSync->IsTransportProtocolAzureBlob())
        {
            boost::tribool tb = m_cxTransport->renameFile(preMissingHcdsDatName, completedMissingHcdsDatName, COMPRESS_NONE);
            if (!tb || CxTransportNotFound(tb))
            {
                msg << "FAILED FastSync::Failed rename missing hcds packet From " << preMissingHcdsDatName << " To " \
                    << completedMissingHcdsDatName << " Tal Status : " << m_cxTransport->status() << endl;
                m_cxTransport->deleteFile(preMissingHcdsDatName);
                throw DataProtectionException(msg.str());
            }
        }
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Sent MissingHcdFile[%s] (copy of HcdProcessMap[%s])\n", FUNCTION_NAME, m_FastSync->JobId().c_str(), completedMissingHcdsDatName.c_str(), m_FastSync->getHcdProcessMapFileName().c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


int FastSync::ProcessHcdTask::ChunksToBeProcessed(SharedBitMapPtr& missingSyncsMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int index = 0;
    int pendingChunks = 0;
    missingSyncsMap->dumpBitMapInfo();
    bool ProcessHcdsIdxVal, ProcessHcdsIndxPlus1Val;

    do
    {
        ProcessHcdsIdxVal = missingSyncsMap->isBitSet(index);
        ProcessHcdsIndxPlus1Val = missingSyncsMap->isBitSet(index + 1);

        if (!ProcessHcdsIdxVal && !ProcessHcdsIndxPlus1Val)
        {
            pendingChunks++;
        }

        index += 2;
    } while (index <= (m_FastSync->totalChunks() * 2) - 2);

    DebugPrintf(SV_LOG_DEBUG, "There are %d Pending chunks\n", pendingChunks);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return pendingChunks;
}


FastSync::ProcessHcdTask::MissingSyncProcessStatus
FastSync::ProcessHcdTask::ProcessMissingSyncsPacket()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    Files_t files;
    std::ostringstream msg;
    std::string completedMissingSyncsDatFile = m_FastSync->RemoteSyncDir() + completedSyncPrefix + m_FastSync->JobId() + missingSuffix;
    MissingSyncProcessStatus status = ERRORPROCESSINGSYNCMISSING;

    SharedBitMapPtr missingSyncsMap;
    if (!m_FastSync->initSharedBitMapFile(missingSyncsMap, completedMissingSyncsDatFile, m_cxTransport, true))
    {
        msg << "FAILED FastSync::Failed to initialize chunks missing packet. " << completedMissingSyncsDatFile << '\n';
        throw DataProtectionException(msg.str());
    }

    int pendingChunks = ChunksToBeProcessed(missingSyncsMap);
    if (missingSyncsMap->syncToPersistentStore(m_cxTransport, m_FastSync->getHcdProcessMapFileName()) != SVS_OK)
    {
        msg << "Failed to persist the missing syncs map as "
            << m_FastSync->getHcdProcessMapFileName() << '\n';
        throw DataProtectionException(msg.str());
    }

    if (!m_FastSync->initSharedBitMapFile(m_checksumProcessedMap, m_FastSync->getHcdProcessMapFileName(), m_cxTransport, true))
    {
        msg << "FAILED FastSync::Failed to initialize checksums Process map.." << m_FastSync->getHcdProcessMapFileName() << '\n';
        throw DataProtectionException(msg.str());
    }

    if (pendingChunks == 0)
    {
        stringstream msg;
        msg << "Processed Bytes: " << m_checksumProcessedMap->getBytesProcessed();
        msg << ", Not Processed Bytes: " << m_checksumProcessedMap->getBytesNotProcessed() << endl;
        DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());

        /*
          SV_ULONGLONG fullyunusedbytes = GetFullyUnUsedBytes();
          DebugPrintf(SV_LOG_DEBUG, "fullyunusedbytes = " ULLSPEC "\n", fullyunusedbytes);
          */

        if (m_FastSync->SetFastSyncMatchedBytes(m_checksumProcessedMap->getBytesProcessed() + m_checksumProcessedMap->getBytesNotProcessed() /* +
                                                                                                                                                 fullyunusedbytes */, 1) &&
                                                                                                                                                 m_FastSync->sendResyncEndNotifyAndResyncTransition())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Successfully moved to step 2\n", FUNCTION_NAME, m_FastSync->JobId().c_str());
            status = TRANSITIONCOMPLETE;
        }
        else
        {
            msg << "FAILED: Last update of Matched bytes to CX is not successful\n";
            throw DataProtectionException(msg.str());
        }
    }
    else
    {
        msg << "Post reconciliation: chunks to be processed=["
            << pendingChunks
            << "], processing MissingSyncFile[" << completedMissingSyncsDatFile << "] in HcdProcessMap[" << m_FastSync->getHcdProcessMapFileName() << "]";
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s %s\n", FUNCTION_NAME, m_FastSync->JobId().c_str(), msg.str().c_str());
        status = CHUNKSPENDING;
    }

    if ((TRANSITIONCOMPLETE == status) || (CHUNKSPENDING == status))
    {
        if (!m_cxTransport->deleteFile(completedMissingSyncsDatFile))
        {
            DebugPrintf(SV_LOG_WARNING, "Unable to remove %s from CX.. tal status %s\n", completedMissingSyncsDatFile.c_str(), m_cxTransport->status());
        }
    }

    std::string hcdNoMore;
    m_FastSync->formatHcdNoMoreFileName(hcdNoMore);
    hcdNoMore = m_FastSync->RemoteSyncDir() + hcdNoMore;
    if (m_FastSync->GetFileList(m_cxTransport, hcdNoMore, files) && files.size() == 1)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Post reconciliation: removing hcdNoMoreFile[%s]\n", FUNCTION_NAME, m_FastSync->JobId().c_str(), hcdNoMore.c_str());
        m_cxTransport->deleteFile(hcdNoMore);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}


SV_ULONGLONG FastSync::ProcessHcdTask::GetFullyUnUsedBytes(void)
{
    SharedBitMapPtr checksumTransferMap;
    if (!m_FastSync->initSharedBitMapFile(checksumTransferMap, m_FastSync->getHcdTransferMapFileName(), m_cxTransport, true))
    {
        throw DataProtectionException("Hcd TransferBitmap initialization failed at source to get fully unused bytes\n");
    }

    return checksumTransferMap->getBytesNotProcessed();
}


bool FastSync::ProcessHcdTask::isHcdProcessed(SV_ULONGLONG offset, CxTransport::ptr cxTransport, string deleteName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERING %s with offset " ULLSPEC "\n", FUNCTION_NAME, offset);
    bool retVal = false;

    size_t chunkIndex = (size_t)(offset / m_FastSync->m_MaxSyncChunkSize);

    std::stringstream msg;
    msg << "For source volume "
        << m_FastSync->GetDeviceName()
        << ", ";
    m_checksumProcessedMap->dumpBitMapInfo();

    bool indxPlus1 = m_checksumProcessedMap->isBitSet((chunkIndex * 2) + 1);
    bool indx = m_checksumProcessedMap->isBitSet(chunkIndex * 2);

    if (indxPlus1 || indx)
    {
        msg << "chunk " << chunkIndex << " is already processed from bitmap.";
        retVal = true;
    }
    else
    {
        ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_OffsetsInProcessMutex);
        if (guard.locked())
        {
            Offsets_t::const_iterator o = m_OffsetsInProcess.find(offset);
            if (o != m_OffsetsInProcess.end())
            {
                retVal = true;
                msg << "offset " << offset
                    << " is already under process.";
            }
            else
            {
                m_OffsetsInProcess.insert(offset);
            }
        }
        else
        {
            throw DataProtectionException("failed to lock for checking offsets in process\n");
        }
    }

    if (retVal)
    {
        DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
        if (!cxTransport->deleteFile(deleteName))
        {
            std::string excmsg;
            excmsg = "removal of ";
            excmsg += deleteName;
            excmsg += " for which checksums already processed, failed: ";
            excmsg += cxTransport->status();
            excmsg += "\n";
            throw DataProtectionException(excmsg);
        }
        --m_pendingFileCount;
        DebugPrintf(SV_LOG_DEBUG, "Removed the %s for which checksums already processed\n", deleteName.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", FUNCTION_NAME);
    return retVal;
}


void FastSync::ProcessHcdTask::deleteFromOffsetsInProcess(SV_ULONGLONG offset)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_OffsetsInProcessMutex);
    if (guard.locked())
    {
        m_OffsetsInProcess.erase(offset);
    }
    else
    {
        throw DataProtectionException("failed to lock for adding offsets in process\n");
    }
}


void FastSync::assignFileListToThreads(ACE_Shared_MQ& sharedListQueue, FileInfosPtr fileInfos, int taskCount, int maxEntries, int basePoint)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int stride = (maxEntries > taskCount) ? taskCount : maxEntries;
    for (int i = 0; i < stride; i++)
    {
        FastSyncMsg_t * msg = new FastSyncMsg_t();
        msg->type = MSG_PROCESS;
        msg->fileInfos = fileInfos;
        msg->index = basePoint + i;
        msg->skip = stride;
        msg->maxIndex = maxEntries + basePoint;

        ACE_Message_Block * mb = new
            ACE_Message_Block((char *)msg, sizeof(FastSyncMsg_t));
        mb->msg_priority(SYNC_NORMAL_PRIORITY);
        sharedListQueue->enqueue_prio(mb);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

bool FastSync::createVolume(cdp_volume_t::Ptr &volume, std::string &err)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string nameToOpen(GetDeviceName());
    std::string mountPoint(Settings().mountPoint);

    if (Source())
    {
        std::string dskname = nameToOpen;

#ifdef SV_UNIX
        dskname = GetDiskNameForPersistentDeviceName(nameToOpen, m_VolumesCache.m_Vs);
#endif

        nameToOpen = GetRawVolumeName(dskname);
    }
#ifdef SV_AIX
    else
    {

        std::string dskname = nameToOpen;
        nameToOpen = GetRawVolumeName(dskname);
    }
#endif

    std::string dev_name(nameToOpen);
    std::string mntPt(mountPoint);
    std::string sparsefile;
    bool isnewvirtualvolume = false;
    LocalConfigurator localConfigurator;
    if (!GetConfigurator().IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(), sparsefile, isnewvirtualvolume)))
    {
        dev_name = sparsefile;
        mntPt = sparsefile;
    }

    volume = cdp_volume_t::Ptr(new cdp_volume_t(dev_name, isnewvirtualvolume, DeviceType(), &m_VolumesCache.m_Vs));

    // Enable Retries on failures (required for fabric multipath device failures)
    volume->EnableRetry(CDPUtil::QuitRequested, GetConfigurator().getVolumeRetries(), GetConfigurator().getVolumeRetryDelay());

    bool useUnBufferedIo = localConfigurator.useUnBufferedIo();
#ifdef SV_SUN
    useUnBufferedIo=false;
#endif

#ifdef SV_AIX
    useUnBufferedIo=true;
#endif

    /* TODO: for hcd generation, open volume in read mode */
    BasicIo::BioOpenMode openMode;
    if (Source())
    {
		openMode = cdp_volume_t::GetSourceVolumeOpenMode(Settings().devicetype);
    }
    else
    {
        openMode = BasicIo::BioRWExisitng | BasicIo::BioShareAll | BasicIo::BioBinary | BasicIo::BioSequentialAccess;
        if (useUnBufferedIo)
        {
#ifdef SV_WINDOWS
            openMode |= (BasicIo::BioNoBuffer | BasicIo::BioWriteThrough);
#else
            openMode |= BasicIo::BioDirect;
#endif
        }
    }

    volume->Open(openMode);
    if (!volume->Good())
    {
        std::stringstream errMsg;
        errMsg << FUNCTION_NAME << ": volume open failed for " << GetDeviceName() << ", Error: " << volume->LastError();
        err = errMsg.str();
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

cdp_volume_t::Ptr FastSync::createVolume()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	cdp_volume_t::Ptr volume;
    std::string err;
    if (!createVolume(volume, err))
    {
        throw DataProtectionException(err);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volume;
}

ACE_Message_Block * FastSync::retrieveMessage(ACE_Shared_MQ& sharedQueue)
{
    ACE_Message_Block *mb = NULL;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Time_Value wait = ACE_OS::gettimeofday();
    int waitSeconds = TaskWaitTimeInSeconds;
    wait.sec(wait.sec() + waitSeconds);
    if (-1 == sharedQueue->dequeue_head(mb, &wait))
    {
        if (errno != EWOULDBLOCK && errno != ESHUTDOWN)
        {
            throw DataProtectionException("Message could not retrieved from the shared Queue\n");
        }
        DebugPrintf(SV_LOG_DEBUG, "dequeue_head failed with errno = %d\n", errno);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return mb;
}

// ProcessSyncDataTask
// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::ProcessSyncDataTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::ProcessSyncDataTask::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// for each diff info gets the diff data. if the data is retrieved
// successfull, psosts a message to the apply task
// --------------------------------------------------------------------------
int FastSync::ProcessSyncDataTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s ProcessSyncDataTask - sync file applier primary thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    FastSyncApplyTasks_t applyTasks;
    SVProactorTask* proactorTask = NULL;
    std::string jobId;

    std::string syncMissing = completedSyncPrefix + m_FastSync->JobId() + missingSuffix;
    Files_t files;
    CxTransport::ptr cxTransport;
    try
    {
        cxTransport.reset(new CxTransport(m_FastSync->Protocol(),
            m_FastSync->TransportSettings(),
            m_FastSync->Secure(),
            m_FastSync->getCfsPsData().m_useCfs,
            m_FastSync->getCfsPsData().m_psId));

        SV_UINT MaxApplyTasks = m_FastSync->GetConfigurator().getMaxFastSyncApplyThreads();

        // prefix for the sync files to fetch
        std::string prefixName(m_FastSync->RemoteSyncDir());
        prefixName += SyncDataPrefix;

        ACE_Shared_MQ sharedmq(new ACE_Message_Queue<ACE_MT_SYNCH>());

        // list of pending sync files
        //boost::shared_ptr<Files_t> files(new Files_t);
        FileInfosPtr fileInfos(new FileInfos_t);

        // Atomic Thread Safe Variable  for maintaining pending file count
        m_FastSync->SetPendingSyncCount(0);

        VolumeList vList;
        vList.push_back(m_FastSync->GetDeviceName());

        m_FastSync->StartProactor(proactorTask, m_FastSync->m_ThreadManager);

        SV_ULONG syncApplyInterval = 0;

        if (PurgeRetentionIfRequired())
        {
            Profiler psl = NEWPROFILER(SYNCLIST, m_FastSync->ProfileVerbose(), m_FastSync->ProfilingPath(), false);
            do
            {
                syncApplyInterval += TaskWaitTimeInSeconds;

                if (syncApplyInterval >= m_FastSync->m_ResyncUpdateInterval &&
                    m_FastSync->updateAndPersistApplyInformation(cxTransport) == true)
                {
                    syncApplyInterval = 0;
                }

                static long long lastIRMetricsReportTime = ACE_OS::gettimeofday().sec();
                long long currentTime = ACE_OS::gettimeofday().sec();
                if ((currentTime - lastIRMetricsReportTime) > m_FastSync->GetConfigurator().getIRMetricsReportInterval())
                {
                    m_FastSync->updateProfilingSummary();
                    lastIRMetricsReportTime = ACE_OS::gettimeofday().sec();
                }

                if (0 != m_FastSync->PendingSyncCount())
                {
                    cxTransport->heartbeat();
                }
                else
                {
                    psl->start();
                    if (!cxTransport->listFile(prefixName, *fileInfos.get()))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s failed to list %s with transport status %s.\n",
                            FUNCTION_NAME, prefixName.c_str(), cxTransport->status());
                        m_FastSync->Stop();
                        break;
                    }
                    psl->stop(0);

                    // if there are no pending files idle for few seconds and try again
                    if (fileInfos->empty())
                    {
                        m_FastSync->downloadAndProcessMissingHcdsPacket(cxTransport);
                        m_FastSync->Idle(WindowsWaitTimeInSeconds);
                        continue;
                    }

                    bool canupdateAppliedBytesToCx = false;

                    // create the apply threads only once
                    // i.e first time we get a non empty pending file list
                    if (!applyTasks.size())
                    {

                        for (SV_UINT i = 0; i < MaxApplyTasks; ++i)
                        {
                            FastSyncApplyTaskPtr applyTask(new
                                FastSyncApplyTask(m_FastSync, thr_mgr(), sharedmq));
                            applyTask->open();
                            applyTasks.push_back(applyTask);
                        }
                    }

                    // initialize the pending file count
                    m_FastSync->SetPendingSyncCount(fileInfos->size());
                    m_FastSync->assignFileListToThreads(sharedmq, fileInfos, MaxApplyTasks, fileInfos->size()); // pass the index and skips to each worker thread
                }
            } while (!m_FastSync->QuitRequested(TaskWaitTimeInSeconds));
        }


        FastSyncApplyTasks_t::iterator iter(applyTasks.begin());
        FastSyncApplyTasks_t::iterator end(applyTasks.end());
        for (; iter != end; ++iter)
        {
            FastSyncApplyTaskPtr taskptr = (*iter);
            thr_mgr()->wait_task(taskptr.get());
        }

        m_FastSync->PostMsg(SYNC_DONE, SYNC_NORMAL_PRIORITY);

    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "FastSync::ProcessSyncDataTask::svc caught an unnknown exception\n");
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }

    m_FastSync->Stop();

    m_FastSync->updateProfilingSummary();

    //wait for apply tasks
    FastSyncApplyTasks_t::iterator iter(applyTasks.begin());
    FastSyncApplyTasks_t::iterator end(applyTasks.end());
    for (; iter != end; ++iter)
    {
        FastSyncApplyTaskPtr taskptr = (*iter);
        thr_mgr()->wait_task(taskptr.get());
    }
    if (!IsRcmMode())
    {
        m_FastSync->UpdateResyncStats(true);
    }
    bool bUpdate = m_FastSync->updateAndPersistApplyInformation(cxTransport);
    if (bUpdate == false)
    {
        DebugPrintf(SV_LOG_ERROR, "Updating Applied Bytes information to CX Failed.. It will be retried after %d seconds..\n", m_FastSync->m_ResyncUpdateInterval);
    }
    m_FastSync->StopProactor(proactorTask, m_FastSync->m_ThreadManager);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s ProcessSyncDataTask - sync file applier primary thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    return 0;
}

bool FastSync::ProcessSyncDataTask::PurgeRetentionIfRequired()
{
    bool rv = true;
    if (IsRcmMode())
    {
        // Placeholder for RCM Prime.
        return true;
    }
    do
    {
        CDP_SETTINGS cdpSettings = TheConfigurator->getCdpSettings(m_FastSync->GetDeviceName());
        //Check if we are doing initial sync.
        ACE_stat db_stat = { 0 };
        if ((cdpSettings.cdp_status != CDP_DISABLED) && (sv_stat(getLongPathName(cdpSettings.Catalogue().c_str()).c_str(), &db_stat) == 0))
        {
            //Check if cdp_pruning file is present, if so exit.
            std::string cdpprunefile = cdpSettings.CatalogueDir();
            cdpprunefile += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            cdpprunefile += "cdp_pruning";

            if (sv_stat(getLongPathName(cdpprunefile.c_str()).c_str(), &db_stat) == 0)
            {
                CDPDatabase db(cdpSettings.Catalogue());
                if (!db.purge_retention(m_FastSync->GetDeviceName()))
                {
                    rv = false;
                    DebugPrintf(SV_LOG_ERROR, "Failed to purge retention directory %s, target volume %s. Exiting Dataprotection\n", cdpSettings.CatalogueDir().c_str(), m_FastSync->GetDeviceName().c_str());
                    break;
                }
                ACE_OS::unlink(cdpprunefile.c_str());

                RetentionDirFreedUpAlert a(cdpSettings.CatalogueDir());
                DebugPrintf(SV_LOG_DEBUG, "%s", a.GetMessage().c_str());
                m_FastSync->SendAlert(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
            }
        }
    } while (0);
    return rv;
}

bool FastSync::updateAndPersistApplyInformation(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string syncMissing = RemoteSyncDir() + completedSyncPrefix + JobId() + missingSuffix;
    bool bApplied = true;
    Files_t files;
    if (GetFileList(cxTransport, syncMissing, files) && files.size() == 0)
    {
        if (m_syncDataAppliedMap->getBytesProcessed() + m_syncDataAppliedMap->getBytesNotProcessed()
            != m_previousProgressBytes.value())
        {
            if (m_syncDataAppliedMap->syncToPersistentStore(cxTransport) == SVS_OK)
            {
                m_previousProgressBytes = m_syncDataAppliedMap->getBytesProcessed() +
                    m_syncDataAppliedMap->getBytesNotProcessed();
            }
            else
                bApplied = false;
        }
        SetFastSyncProgressBytes(m_syncDataAppliedMap->getBytesProcessed(),
            m_syncDataAppliedMap->getBytesNotProcessed(), GetResyncStatsForCSLegacyAsJsonString());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bApplied;
}


// FastSyncApplyTask
// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::FastSyncApplyTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::FastSyncApplyTask::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// for each sync data get it and apply to volume + retention
// --------------------------------------------------------------------------
int FastSync::FastSyncApplyTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	DebugPrintf(SV_LOG_ALWAYS, "%s:%s FastSyncApplyTask - sync file applier worker thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    try {
        CDPV2Writer::Ptr cdpv2writer;
        // Bypass hide disk before apply on target for rcm prime.
        // In legacy CS cdpmgr do unhide post recovery unlike rcm reprotect agent of RCM prime.
        const bool hideBeforeApplyingSyncData = (IsRcmMode() && m_FastSync->Settings().isInMageDataPlane()) ? false : true;
        cdpv2writer.reset(new (nothrow)CDPV2Writer(m_FastSync->GetDeviceName(), m_FastSync->Settings().sourceCapacity, m_FastSync->CdpSettings(), false, 
			m_FastSync->CdpResyncTxnPtr(),
			m_FastSync ->AzureAgentPtr(),
            &m_FastSync ->m_VolumesCache.m_Vs, m_pSyncApply, hideBeforeApplyingSyncData));
        if (cdpv2writer.get() == 0)
        {
            std::ostringstream msg;
            msg << "In FastSync::FastSyncApplyTask::svc, CDPV2Writer instantiation failed.\n";
            throw DataProtectionException(msg.str());
        }

        CxTransport::ptr cxTransport(new CxTransport(m_FastSync->Protocol(),
            m_FastSync->TransportSettings(),
            m_FastSync->Secure(),
            m_FastSync->getCfsPsData().m_useCfs,
            m_FastSync->getCfsPsData().m_psId));
        cdp_volume_t::Ptr volume = m_FastSync->createVolume();

        while (!m_FastSync->QuitRequested(0))
        {
            ACE_Message_Block * mb = m_FastSync->retrieveMessage(m_getmq);
            if (mb != NULL)
            {
                FastSyncMsg_t * msg = (FastSyncMsg_t*)mb->base();
                processFiles(cxTransport, volume, cdpv2writer, msg);
                delete msg;
                mb->release();
            }
            else
            {
                cxTransport->heartbeat();
            }
        }
        m_pSyncDownload->commit();
        m_pSyncUncompress->commit();
        m_pSyncApply->commit();
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->Stop();
    }
    catch (std::exception& e) {

        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->Stop();
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "FastSync::FastSyncApplyTask::svc caught an unnknown exception\n");
        m_FastSync->Stop();
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s FastSyncApplyTask - sync file applier worker thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}


// --------------------------------------------------------------------------
//  loop through the pending file list starting at the specified index
//  and call worker routine to fetch and apply the file
// --------------------------------------------------------------------------
void FastSync::FastSyncApplyTask::processFiles(CxTransport::ptr &cxTransport, cdp_volume_t::Ptr volptr, CDPV2Writer::Ptr cdpv2writer,
    FastSyncMsg_t * msg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    size_t index = msg->index;
    size_t skip = msg->skip;
    //boost::shared_ptr<Files_t> files = msg ->files;
    FileInfosPtr fileInfos = msg->fileInfos;

    while (!m_FastSync->QuitRequested(0))
    {
        if (index >= fileInfos->size())
            break;

        std::string fileName = fileInfos.get()->at(index).m_name;

        /** Fast sync TBC - BSR **/
        if (!canProcess(fileName, m_FastSync->JobId()))
        {
            DebugPrintf(SV_LOG_ERROR,
                "File Received %s from CX seems to be old file. Skipping and Deleting from CX\n",
                fileName.c_str());
            if (!cxTransport->deleteFile(m_FastSync->RemoteSyncDir() + fileName))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "Delete older file %s received from CX failed: %s\n",
                    fileName.c_str(),
                    cxTransport->status());
            }
            --m_FastSync->m_pendingSyncFiles;
        }
        else
        {
            std::string syncNoMore = completedSyncPrefix + m_FastSync->JobId() + noMore;
            std::string syncMissing = completedSyncPrefix + m_FastSync->JobId() + missingSuffix;

            if (std::string::npos != fileName.find(syncNoMore))
            {
                //m_FastSync->bSyncNoMore = true ;
                DebugPrintf(SV_LOG_DEBUG, "Encountered a Sync No More, would not be processed. Skipping to next file\n");
                --m_FastSync->m_pendingSyncFiles;
            }
            else if (std::string::npos != fileName.find(syncMissing))
            {
                DebugPrintf(SV_LOG_DEBUG, "Encountered a Sync missing packet %s would not be processed. Skipping to next file\n", syncMissing.c_str());
                --m_FastSync->m_pendingSyncFiles;
            }
            else
            {
                SV_ULONG size = fileInfos.get()->at(index).m_size;
                m_FastSync->worker(cxTransport, volptr, cdpv2writer, fileName, size, m_pSyncDownload, m_pSyncUncompress);
            }
        }
        /** End of the change - TBC **/
        index += skip;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


void FastSync::worker(CxTransport::ptr &cxTransport, cdp_volume_t::Ptr volptr, CDPV2Writer::Ptr cdpv2writer, const string& fileName, SV_ULONG size, Profiler &pSyncDownload, Profiler &pSyncUncompress)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SV_LONGLONG totalBytesApplied = 0;
    std::stringstream msg;
    //CheckCacheDirSpace();

    string fullFileName = RemoteSyncDir() + fileName;
    string localFile = (IsRcmMode() && !IsTransportProtocolAzureBlob()) ? fullFileName : CacheDir() + fileName;

    do
    {
        try
        {
            if (!size)
            {
                // This Sync is corrupted
                std::stringstream excmsg;
                excmsg << FUNCTION_NAME << ": Sync file " << fullFileName << " is 0 bytes so cannot apply.";
                throw CorruptResyncFileException(excmsg.str());
            }
            if (!tryMemoryApply(cxTransport, volptr, cdpv2writer, fullFileName, localFile, size, pSyncDownload, pSyncUncompress))
            {
                if (IsRcmMode() && !IsTransportProtocolAzureBlob())
                {
                    ApplySyncData(fullFileName, totalBytesApplied, fullFileName, volptr, cdpv2writer, cxTransport);
                }
                else
                {
                    // Note: make sure to keep guard and ApplySyncData in same block
                    boost::shared_ptr<void> guard((void*)NULL, boost::bind(&FastSync::RemoveLocalFile, this, localFile));

                    pSyncDownload->start();
                    boost::tribool tb = cxTransport->getFile(fullFileName, localFile);
                    pSyncDownload->stop(size);

                    if (!tb || CxTransportNotFound(tb))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: transport download %s failed error: %s\n",
                            FUNCTION_NAME, fullFileName.c_str(), cxTransport->status());
                        break;
                    }
                    pSyncUncompress->start();
                    if (!UncompressIfNeeded(localFile))
                    {
                        RemoveLocalFile(localFile);
                        break;
                    }
                    pSyncUncompress->stop(size);
                    
                    ApplySyncData(localFile, totalBytesApplied, fullFileName, volptr, cdpv2writer, cxTransport);
                }
            }
        }
        catch (CorruptResyncFileException& dpe) {
            // corrupt sync file identified
            // rename corrupt sync file, send InvalidResyncFileAlert and return
            // Note: ApplySyncData is designed to delete sync file on exit on success or failure (exception or normal).
            //    In V2A CS legacy scenario ApplySyncData do copy sync file to local cache and then pass local file to AzureApply.
            //    If AzureApply fail with AzureBadFileException then the local file is guaranteed to be deleted in scoped binded function
            //    but skip to delete remote file as it delete remote file only on successful Apply.
            //    So the corrupt file is retried causing resync stuck forever.
            //    Here we are following a fail safe case and renaming such file with CORRUPT UNDERSCORE and later delete all such files
            //    in CleanupResyncStaleFilesOnPS in main thread.
            //    For V2A RCM PRIME this fail safe case will hit only if ApplySyncData fail to delete corrupt file for any reason where there is no retry.
            DebugPrintf(SV_LOG_ERROR, "%s encountered CorruptResyncFileException : %s\n", FUNCTION_NAME, dpe.what());
            InvalidResyncFileAlert a(GetDeviceName(), fullFileName);
            const std::string corruptfile = RemoteSyncDir() + CORRUPT UNDERSCORE + fileName;
            boost::tribool tb = cxTransport->renameFile(fullFileName, corruptfile, COMPRESS_NONE);
            if (!tb || CxTransportNotFound(tb))
            {
                // Safe to ignore rename fail messgae
                DebugPrintf(SV_LOG_WARNING, "%s: Failed to prefix invalid sync %s with " CORRUPT UNDERSCORE ", with error %s. This could be because file is already deleted.\n",
                    FUNCTION_NAME, (RemoteSyncDir() + fileName).c_str(), cxTransport->status());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Prefixed invalid sync with " CORRUPT UNDERSCORE " %s\n",
                    FUNCTION_NAME, corruptfile.c_str());
            }
            //
            // TODO: nichougu - V2A RCM mode - handle CorruptResyncFileException and raise alert.
            // V2A RCM mode:
            //          Ignoring CorruptResyncFileException in V2A RCM mode.
            //          Current RcmConfiguration design has limitation of only source can raise alert.
            //          So for V2A RCM alerts from target are handeled via remote persistent resync state only when target fail resync.
            //              => In this case there is no question of source to acknowledge alert sent successful back to target.
            //          So if target want to raise without failing resync then we need to incorporate a feature - source to signal back to target about said alert is successfully raised.
            //              => This way target will clear the alert from state file. Otherwise source will keep on sending same alert in each monitor peer interval which will be false alert.
            //          Other solution is RcmConfiguration to allow target to raise alert.
            //          Taking this feature in next release.
            //
            SendAlert(SV_ALERT_ERROR, SV_ALERT_MODULE_RESYNC, a);
        }
        catch (DataProtectionException& dpe) {
            DebugPrintf(dpe.LogLevel(), "%s encountered DataProtectionException : %s - Exiting", FUNCTION_NAME, dpe.what());
            //if ApplyChanges failes, dataprotection should exit.
            PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
        }
        catch (std::exception& e) {
            DebugPrintf(SV_LOG_ERROR, "%s encountered an exception : %s", FUNCTION_NAME, e.what());
            PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
        }
        catch (...) {
            DebugPrintf(SV_LOG_ERROR, "%s encountered an unnknown exception\n", FUNCTION_NAME);
            PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
        }

    } while (0);

    // decrement the pending file count irrespective of success or failure
    // if the apply failed, the file would be sent as part of pending list
    // again in next round

    --m_pendingSyncFiles;


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

/*Added by BSR for Fast Sync TBC */
/** Fast Sync TBC - BSR
 * This function would be used to do inline uncompression for resyncs also.
 * It reads the required constraints from drscout.conf and proceeds if met.
 * It uncompression to memory fails, it tries to uncompress to disk.
 * On failure of above, it returns false which will be treated to use traditional
 * file downloading would be used.
 **/
bool FastSync::tryMemoryApply(CxTransport::ptr &cxTransport,
    cdp_volume_t::Ptr volptr,
    CDPV2Writer::Ptr cdpv2writer,
    string &fullFileName,
    string &localFile,
    SV_ULONG& fileSize,
    Profiler &pSyncDownload,
    Profiler &pSyncUncompress)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED FUNCTION %s\n", FUNCTION_NAME);

    bool status = true;
    bool bMemoryApply = false;

	if (Settings().isAzureDataPlane()){

		bMemoryApply = false;
	}
	else {

		std::string::size_type index = fullFileName.rfind(".gz");
		if (std::string::npos == index || (fullFileName.length() - index) > 3)
		{
			SV_ULONG cbMaxInMemoryUnCompressedFileSize = GetConfigurator().getMaxInMemoryUnCompressedFileSize();
			if (fileSize <= cbMaxInMemoryUnCompressedFileSize)
			{
				bMemoryApply = true;
			}
		}
		else
		{
			SV_ULONG cbMaxInMemoryCompressedFileSize = GetConfigurator().getMaxInMemoryCompressedFileSize();
			if (fileSize <= cbMaxInMemoryCompressedFileSize)
			{
				bMemoryApply = true;
			}
		}
	}

    if (bMemoryApply == true)
    {
        char* inBuffer = NULL;
        SV_ULONG bufferLength = fileSize; //fastSyncChunkSize;
        size_t bytesReturned = 0;
        BoostArray_t buffer;
        SV_LONGLONG totalBytesApplied = 0;

        pSyncDownload->start();
        if (cxTransport->getFile(fullFileName, bufferLength, &inBuffer, bytesReturned))
        {
            pSyncDownload->stop(bufferLength);

            buffer.reset(inBuffer, free);
            std::string::size_type idx = localFile.rfind(".gz");
            if (std::string::npos != idx && (localFile.length() - idx) <= 3)
            {
                pSyncUncompress->start();
                if (UncompressToMemory(buffer, bufferLength))
                {
                    pSyncUncompress->stop(bufferLength);

                    ApplySyncData(localFile, totalBytesApplied, fullFileName, volptr, cdpv2writer, cxTransport, buffer.get(), bufferLength);
                }
                else
                {
                    pSyncUncompress->start();
                    if (UncompressToDisk(buffer, bufferLength, localFile))
                    {
                        pSyncUncompress->stop(bufferLength);

                        ApplySyncData(localFile, totalBytesApplied, fullFileName, volptr, cdpv2writer, cxTransport);
                    }
                    else
                    {
                        RemoveLocalFile(localFile);
                        status = false;
                    }
                }
            }
            else
            {
                ApplySyncData(localFile, totalBytesApplied, fullFileName, volptr, cdpv2writer, cxTransport, buffer.get(), bufferLength);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,
                "transport download %s failed with error: %s\n",
                fullFileName.c_str(), cxTransport->status());
            status = false;
        }
    }
    else
    {
        status = false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

/** Fast Sync TBC - BSR
 *  Sends the SVD Header file. If Compression is enabled, it would compresses
 *  the data and returns back. Compressed buffer would be sent along with first
 *  Sync Data.
 **/
bool FastSync::sendSVDHeader(CxTransport::ptr cxTransport,
    const string& preTarget,
    COMPRESS_MODE compressMode,
    boost::shared_ptr<Compress> compressor)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    char mainHeader[sizeof(SVD_PREFIX)+sizeof(SVD_HEADER1)+sizeof(SV_UINT)];
    memset(mainHeader, 0, sizeof(mainHeader));

    SV_UINT* dataFormatFlags = reinterpret_cast<SV_UINT*>(mainHeader);
    ConvertorFactory::setDataFormatFlags(*dataFormatFlags);
    if (*(dataFormatFlags) != LitttleEndianDataFormatFlags)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, Invalid endian data format flags [0x%x], device=%s\n", FUNCTION_NAME, *(dataFormatFlags), GetDeviceName().c_str());
    }
    SVD_PREFIX* tempPrefix = reinterpret_cast<SVD_PREFIX*>(mainHeader + sizeof(SV_UINT));

    tempPrefix->tag = SVD_TAG_HEADER1;
    tempPrefix->count = 1;

    int returnValue = 0;
    if (COMPRESS_SOURCESIDE == compressMode)
    {
        if (compressor->CompressStream((unsigned char*)mainHeader, sizeof mainHeader) == 0)
        {
            returnValue = 1;
        }
        else
        {
            compressMode = COMPRESS_NONE;
        }
    }
    //Use non compressed mode transfer, we should use further
    if (returnValue != 1)
    {
        if (!SendData(cxTransport, preTarget, mainHeader, sizeof(mainHeader), CX_TRANSPORT_MORE_DATA, compressMode))
        {
            std::ostringstream msg;
            msg << "FAILED FastSync::SendSyncData SendData\n";
            DebugPrintf(SV_LOG_ERROR, "%s\nTAL error", msg.str().c_str(), cxTransport->status());
            throw DataProtectionException(msg.str().c_str());
        }
        returnValue = 1;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return returnValue;
}

/** Fast Sync TBC - BSR
 *  This function send the sync data to CX.
 *  If Compression is enabled, it would compress and send each chunk.
 **/
bool FastSync::sendSyncData(CxTransport::ptr cxTransport,
    const string& preTarget,
    const SV_UINT dataLength,
    const char* syncBuffer,
    const SV_LONGLONG offset,
    COMPRESS_MODE compressMode,
    const bool bMoreData,
    boost::shared_ptr<Compress> compressor,
    std::ostringstream& msg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int dbhSize = sizeof(SVD_PREFIX)+sizeof(SVD_DIRTY_BLOCK);

    std::vector<char> buffer(dbhSize);

    SVD_PREFIX* dbPrefix = reinterpret_cast<SVD_PREFIX*>(&buffer[0]);
    dbPrefix->tag = SVD_TAG_DIRTY_BLOCK_DATA;
    dbPrefix->count = 1;
    dbPrefix->Flags = 0;

    SVD_DIRTY_BLOCK* dbHeader = reinterpret_cast<SVD_DIRTY_BLOCK*>(&buffer[0] + sizeof(SVD_PREFIX));

    dbHeader->ByteOffset = offset;
    dbHeader->Length = dataLength;

    DebugPrintf(SV_LOG_DEBUG, "Offset: " ULLSPEC" Length: %d\n", offset, dataLength);
    if (COMPRESS_SOURCESIDE == compressMode)
    {
        if (compressor->CompressStream((unsigned char*)&buffer[0], dbhSize) != 0)
        {
            msg << "FAILED FastSync::SendSyncData, Compress->CompressStream" << '\n';
            return false;
        }
    }
    else
    {
        if (!SendData(cxTransport, preTarget, &buffer[0], dbhSize, CX_TRANSPORT_MORE_DATA, compressMode))
        {
            msg << "FAILED FastSync::SendSyncData SendData 1: " << cxTransport->status() << '\n';
            return false; // don't throw here, just break so that SendAbort is done
        }
    }

    SV_ULONG sendBufferSize = 0;
    char* sendBuffer = NULL;
    if (COMPRESS_SOURCESIDE == compressMode)
    {
        if (compressor->CompressStream((unsigned char*)syncBuffer, dataLength) != 0)
        {
            msg << "FAILED FastSync::SendSyncData, Compress->CompressStream" << '\n';
            return false;
        }
        //compresser->CompressStreamInit would clear this buffer or Compresser destructor cleans the buffer
        unsigned const char* temp = compressor->CompressStreamFinish(&sendBufferSize, NULL);
        sendBuffer = (char*)temp;
    }
    else
    {
        sendBuffer = (char*)syncBuffer;
        sendBufferSize = dataLength;
    }

    if (!SendData(cxTransport, preTarget, sendBuffer, sendBufferSize, bMoreData, compressMode))
    {
        msg << "FAILED FastSync::SendSyncData SendData: " << cxTransport->status() << '\n';
        return false; // don't throw here, just break so that SendAbort is done
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return true;
}

/** Fast Sync TBC - BSR
 *  Creates preSync File Name and tmpSync File Name. if Souce based
 *  compression is enabled it would append .gz to the file name.
 **/
void FastSync::nextPresyncAndSyncFileNames(std::string& preTarget,
    std::string& target,
    COMPRESS_MODE compressMode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::ostringstream name;
    // TODO: better unique name
    ACE_Time_Value aceTv = ACE_OS::gettimeofday();
    name << PreSync;
    name << JobId() << "_";

    name << std::hex << std::setw(16) << std::setfill('0') <<
        ACE_TIME_VALUE_AS_USEC(aceTv) << DatExtension;

    if (COMPRESS_SOURCESIDE == compressMode)
        name << gzipExtension;
    preTarget = RemoteSyncDir() + name.str();
    target = RemoteSyncDir();
    if (Settings().compressionEnable == COMPRESS_CXSIDE)
    {
        target += TmpUnderscore;
    }
    target += name.str().substr(PreUnderscore.length());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

/** Fast Sync TBC - BSR **/
void FastSync::nextHcdFileNames(std::string& preTarget,
    std::string& target)
{
    AutoGuard lock(m_FileNameLock);
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::ostringstream name;
    // TODO: better unique name
    ACE_Time_Value aceTv = ACE_OS::gettimeofday();

    name << PreHcd;
    name << JobId() << "_";
    name << std::hex << std::setw(16) << std::setfill('0') << ACE_TIME_VALUE_AS_USEC(aceTv) << DatExtension;

    preTarget = RemoteSyncDir() + name.str();
    target = RemoteSyncDir() + name.str().substr(4);
    ACE_Time_Value wait;
    wait.msec(FileNameWaitInMilli);
    ACE_OS::sleep(wait);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


void FastSync::formatHcdNoMoreFileName(std::string& hcdNoMore)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    hcdNoMore = completedHcdPrefix + JobId() + noMore;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}
void FastSync::formatHcdMissingFileName(std::string& hcdMissing)
{
    hcdMissing = completedHcdPrefix + JobId() + missingSuffix;
    return;
}


/* End of the change */

/*
 *  Fixing the overshoot problem with bitmaps..
 *  This creates a directory to store the bitmap files..
 *  By Suresh
 */
bool FastSync::CreateBitMapDir()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    string str;
    m_bitMapDir = GetConfigurator().getInstallPath();
    m_bitMapDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    m_bitMapDir += Resync_Folder;
    m_bitMapDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    DebugPrintf(SV_LOG_DEBUG, "The BitMapDirectory is %s\n", m_bitMapDir.c_str());
    SVERROR rc = SVMakeSureDirectoryPathExists(m_bitMapDir.c_str());
    if (rc.failed()) {
        DebugPrintf(SV_LOG_ERROR, "FAILED FastSync::m_bitMapDir SVMakeSureDirectoryPathExists %s: %s\n",
            m_bitMapDir.c_str(), rc.toString());
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}


bool FastSync::initSharedBitMapFile(boost::shared_ptr<SharedBitMap>&  bitmap, const string& fileName, CxTransport::ptr cxTransport,
    const bool & bShouldCreateIfNotPresent)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SV_ULONG bitMapSize = totalChunks();

    if (0 == bitMapSize)
    {
        DebugPrintf(SV_LOG_ERROR, "For file %s, totalChunks returned bitmap size zero\n", fileName.c_str());
    }

    if ((this->*m_shouldDoubleBitmap[IsFsAwareResync()])(fileName))
    {
        bitMapSize *= 2;
    }
    DebugPrintf(SV_LOG_DEBUG, "For file %s, bitmap size locally calculated is %lu\n", fileName.c_str(), bitMapSize);

	bitmap.reset(new SharedBitMap(fileName, bitMapSize, IsTransportProtocolAzureBlob(), m_MaxSyncChunkSize, Secure()));
    if (bitmap->initialize(cxTransport, bShouldCreateIfNotPresent) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to initialize bitmap file %s\n", fileName.c_str());
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}


void FastSync::downloadAndProcessMissingHcdInFsResync(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string missingHcdFile = RemoteSyncDir() + completedHcdPrefix + JobId() + missingSuffix;
    std::string tmpSyncFiles = RemoteSyncDir() + TmpUnderscore + SyncDataPrefix;
    Files_t files;
    if (GetFileList(cxTransport, tmpSyncFiles, files)
        && files.size() == 0)
    {
        if (GetFileList(cxTransport, missingHcdFile, files) &&
            files.size() != 0)
        {
            if (GetFileList(cxTransport, getUnProcessedClusterBitMapFileName(), files) &&
                files.size() != 0)
            {
                ProcessMissingHcdsPacketInFsResync(cxTransport);
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void FastSync::downloadAndProcessMissingHcd(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string missingHcdFile = RemoteSyncDir() + completedHcdPrefix + JobId() + missingSuffix;
    std::string tmpSyncFiles = RemoteSyncDir() + TmpUnderscore + SyncDataPrefix;
    Files_t files;
    if (GetFileList(cxTransport, tmpSyncFiles, files)
        && files.size() == 0)
    {
        if (GetFileList(cxTransport, missingHcdFile, files) &&
            files.size() != 0)
        {
            ProcessMissingHcdsPacket(cxTransport);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

/*
	Reconciliation check function to check progress map files has synchronized bits
	corresponding to each other and take appropriate action if discrepancy is identified

	checksumTransferMap
		00 – Not processed
		01 – partially used
		10 - Fully unused
		11 - Fully used

	m_missingHcdsMap - hcd process map from source
		00 - Not processed
		01 - unmatched
		10 – fully unused
		11 – fully matched

	m_syncDataAppliedMap
		00 - not processed
		11 - If sync file was for a chunk
		01 - If sync file was for less than a chunk
*/
void FastSync::ProcessMissingHcdsPacket(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream ssReconInfo;
    std::ostringstream msg;
    string completedMissingHcdsDatName = RemoteSyncDir() + completedHcdPrefix + JobId() + missingSuffix;;

    if (!initSharedBitMapFile(m_missingHcdsMap, completedMissingHcdsDatName, cxTransport, true))
    {
        SendHcdNoMoreFile(cxTransport);
        if (!cxTransport->deleteFile(completedMissingHcdsDatName))
        {
            DebugPrintf(SV_LOG_ERROR, "delete file %s failed: %s\n", completedMissingHcdsDatName.c_str(), cxTransport->status());
        }
        throw DataProtectionException("FAILED FastSync::Failed to Initialize the missing hcds bitmap\n");
    }

    int index = 0;
    int missingHcds = 0;
    int missingSyncs = 0;

    SharedBitMap* checksumTransferMap = m_genHCDTask->checksumTransferMap();

    ssReconInfo << "Reconciliation check: " << JobId() << " ----------------------------------------------------------------------\n\n";
    ssReconInfo << "HcdTransfer map before check:\n" << checksumTransferMap->getBitMapInfo() << "\n";
    ssReconInfo << "MissingHcds (HCDProcess on source) map before check:\n" << m_missingHcdsMap->getBitMapInfo() << "\n";
    ssReconInfo << "SyncApplied map before check:\n" << m_syncDataAppliedMap->getBitMapInfo() << "\n\n";

    bool baremapsvalid = AreBitMapsValid(checksumTransferMap, m_missingHcdsMap, m_syncDataAppliedMap, msg);
    if (!baremapsvalid)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed: AreBitMapsValid failed with error %s\n", FUNCTION_NAME, msg.str().c_str());
        HandleInvalidBitmapError(msg.str());
    }

    ssReconInfo << "Reconciliation check START\n\n";

    SV_ULONGLONG lostHcdBytesSent = 0;
    SV_ULONG chunkSize = 0;
    do
    {
        bool SyncIdxVal, MissingHcdsIdxVal, SyncIdxPlus1Val, MissingHcdsIndxPlus1Val;
        SyncIdxVal = m_syncDataAppliedMap->isBitSet(index);
        SyncIdxPlus1Val = m_syncDataAppliedMap->isBitSet(index + 1);

        MissingHcdsIdxVal = m_missingHcdsMap->isBitSet(index);
        MissingHcdsIndxPlus1Val = m_missingHcdsMap->isBitSet(index + 1);

        int chunkNo = index / 2;

        if (m_VolumeSize % getChunkSize() != 0 && chunkNo == totalChunks() - 1) //chunkNo starts from 0
        {
            chunkSize = m_VolumeSize % m_MaxSyncChunkSize;
        }
        else
            chunkSize = m_MaxSyncChunkSize;

        if ((!SyncIdxPlus1Val && SyncIdxVal) || (!MissingHcdsIndxPlus1Val && MissingHcdsIdxVal))
        {
            std::stringstream msg;
            msg << "bitmap files corrupted For chunk[" << chunkNo << "] [" << m_syncDataAppliedMap->GetBitMapFileName() << "] [" << m_missingHcdsMap->GetBitMapFileName() << "]..\n";
            throw DataProtectionException(msg.str().c_str());
        }

        if (SyncIdxPlus1Val && SyncIdxVal)
        {
            /* applied */
            if (MissingHcdsIndxPlus1Val && MissingHcdsIdxVal)
            {
                /* applied but process map says matched, mark as unmatched */
                /*  could this occur? */

                ssReconInfo << "For chunk[" << chunkNo << "], syncAppliedMap says applied " << SyncIdxVal << SyncIdxPlus1Val
                    << " but hcdProcessMap says matched " << MissingHcdsIdxVal << MissingHcdsIndxPlus1Val << "(no sync file sent)"
                    << " - marking as unmatched in missingHcdsMap (hcdProcessMap)" << "\n";

                m_missingHcdsMap->markAsUnMatched((SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize, chunkSize);
                MissingHcdsIdxVal = false;
            }
            else if (!MissingHcdsIndxPlus1Val && !MissingHcdsIdxVal)
            {
                /* applied but process map says not processed, should have marked as not processed
                 *  not processed is already there. Is markAsUnMatched done to get sync file ? */
                /*  could this occur? */

                ssReconInfo << "For chunk[" << chunkNo << "], syncAppliedMap says applied " << SyncIdxVal << SyncIdxPlus1Val
                    << " but hcdProcessMap says not processed " << MissingHcdsIdxVal << MissingHcdsIndxPlus1Val << "(no sync file sent)"
                    << ", hcdProcessMap should have marked as not processed - not processed is already there" << "\n";

                m_missingHcdsMap->markAsUnMatched((SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize);
                MissingHcdsIndxPlus1Val = true;
            }
        }

        if (!SyncIdxPlus1Val && !SyncIdxVal && MissingHcdsIndxPlus1Val && !MissingHcdsIdxVal)
        {
            /* sync maps says did not apply but process map says unmatched and sync file sent.
               here marking not processed */
            ssReconInfo << "Sync file lost for chunk[" << chunkNo << "], marking as not processed in missingHcdsMap (hcdProcessMap)" << "\n";
            missingSyncs++;
            m_missingHcdsMap->markAsNotProcessed((SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize);
            MissingHcdsIndxPlus1Val = false;
        }

        if (MissingHcdsIndxPlus1Val && MissingHcdsIdxVal)
        {
            /* If matched, mark as not applied
             */
            m_syncDataAppliedMap->markAsNotApplied((SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize);
            DebugPrintf(SV_LOG_DEBUG, "Marking Matched Chunk in Sync Applied Map for chunk %d\n", chunkNo);
        }

        if (MissingHcdsIndxPlus1Val && !checksumTransferMap->isBitSet(chunkNo))
        {
            /* process map says processed but transfer map says not transferred then mark as transferred */
            /* ? */

            ssReconInfo << "For chunk[" << chunkNo << "], hcdProcessMap says processed " << MissingHcdsIdxVal << MissingHcdsIndxPlus1Val
                << " but checksumTransferMap map says not transferred " << checksumTransferMap->isBitSet(chunkNo)
                << " - marking as transferred in checksumTransferMap" << "\n";

            checksumTransferMap->markAsTransferred((SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize, chunkSize);
        }

        if (!MissingHcdsIndxPlus1Val)
        {
            if (checksumTransferMap->isBitSet(chunkNo))
            {
                /* process map says not processed, and transfer map says sent, mark as not transferred */

                ssReconInfo << "For chunk[" << chunkNo << "], hcdProcessMap says not processed " << MissingHcdsIdxVal << MissingHcdsIndxPlus1Val
                    << " but checksumTransferMap map says sent " << checksumTransferMap->isBitSet(chunkNo)
                    << " - Marking as not transferred in checksumTransferMap" << "\n";

                checksumTransferMap->markAsNotTransferred((SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize, chunkSize);
            }
            missingHcds++;
        }

        index += 2;
    } while (index <= (totalChunks() * 2) - 2);

    ssReconInfo << "\nMissedHcds=" << missingHcds << ", MissedSyncFiles=" << missingSyncs << "\n\n";


    if (m_syncDataAppliedMap->syncToPersistentStore(cxTransport) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync::Failed to sync sync data  transfer map to persistent store\n");
    }

    if (missingHcds + missingSyncs == 0)
    {
        m_missingHcdsMap->setPartialMatchedBytes(m_syncDataAppliedMap->getBytesNotProcessed());
    }

    if (!IsRcmMode())
    {
        UpdateResyncStats(true);
    }
    if (!SetFastSyncProgressBytes(m_syncDataAppliedMap->getBytesProcessed(), m_syncDataAppliedMap->getBytesNotProcessed(),
        GetResyncStatsForCSLegacyAsJsonString()))
    {
        throw DataProtectionException("FAILED FastSync: Failed to Update the FastSyncProgressBytes\n");
    }
    else
    {
        m_previousProgressBytes = m_syncDataAppliedMap->getBytesProcessed() + m_syncDataAppliedMap->getBytesNotProcessed();
    }

    if (!SetFastSyncFullSyncBytes(checksumTransferMap->getBytesProcessed()))
    {
        throw DataProtectionException("FAILED FastSync: Failed to Update the Full SyncBytes...\n");
    }
    else
    {
        m_genHCDTask->setprevFullSyncBytesSent(checksumTransferMap->getBytesProcessed());//m_prevFullSyncBytesSent = m_checksumTransferMap->getBytesProcessed() ;
    }
    SendMissingSyncsPacket(cxTransport);

    if (checksumTransferMap->syncToPersistentStore(cxTransport) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync::Failed to sync checksum transfer map to persistent store\n");
    }

    if (!cxTransport->deleteFile(completedMissingHcdsDatName))
    {
        DebugPrintf(SV_LOG_ERROR, "delete file %s failed: %s\n", completedMissingHcdsDatName.c_str(), cxTransport->status());
        throw DataProtectionException("FAILED FastSync::Failed to remove the missing hcd file from cx \n");
    }

    ssReconInfo << "HcdTransfer map after check:\n" << checksumTransferMap->getBitMapInfo() << "\n";
    ssReconInfo << "MissingHcds (HCDProcess on source) map after check:\n" << m_missingHcdsMap->getBitMapInfo() << "\n";
    ssReconInfo << "SyncApplied map after check:\n" << m_syncDataAppliedMap->getBitMapInfo() << "\n\n";
    ssReconInfo << "Reconciliation check END ----------------------------------------------------------------------\n\n";
    DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n\n", FUNCTION_NAME, ssReconInfo.str().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


void FastSync::ProcessMissingHcdsPacketInFsResync(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s reconciliation check START\n", FUNCTION_NAME, JobId().c_str());
    std::ostringstream msg;
    string completedMissingHcdsDatName = RemoteSyncDir() + completedHcdPrefix + JobId() + missingSuffix;

    if (!initSharedBitMapFile(m_missingHcdsMap, completedMissingHcdsDatName, cxTransport, true))
    {
        /* TODO: review this code */
        if (!cxTransport->deleteFile(completedMissingHcdsDatName))
        {
            DebugPrintf(SV_LOG_ERROR, "delete file %s failed: %s\n", completedMissingHcdsDatName.c_str(), cxTransport->status());
        }
        throw DataProtectionException("FAILED FastSync::Failed to Initialize the missing hcds bitmap\n");
    }

    SharedBitMapPtr unprocessedClusterMap;
    if (!GetClusterParams()->initSharedBitMapFile(unprocessedClusterMap, getUnProcessedClusterBitMapFileName(), cxTransport, true))
    {
        /* TODO: review this code */
        if (!cxTransport->deleteFile(getUnProcessedClusterBitMapFileName()))
        {
            DebugPrintf(SV_LOG_ERROR, "delete file %s failed: %s\n", getUnProcessedClusterBitMapFileName().c_str(), cxTransport->status());
        }
        throw DataProtectionException("FAILED FastSync::Failed to initialize the unprocessed cluster map\n");
    }

    int index = 0;
    int missingClusters = 0;
    int missingSyncs = 0;

    SharedBitMap* checksumTransferMap = m_procClusterStage->checksumTransferMap();

    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s HcdTransfer Info\n", FUNCTION_NAME, JobId().c_str());
    checksumTransferMap->dumpBitMapInfo(SV_LOG_ALWAYS);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s MissingHcds Info\n", FUNCTION_NAME, JobId().c_str());
    m_missingHcdsMap->dumpBitMapInfo(SV_LOG_ALWAYS);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s SyncApplied Info\n", FUNCTION_NAME, JobId().c_str());
    m_syncDataAppliedMap->dumpBitMapInfo(SV_LOG_ALWAYS);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s UnProcessedCluster Info\n", FUNCTION_NAME, JobId().c_str());
    unprocessedClusterMap->dumpBitMapInfo(SV_LOG_ALWAYS);

    bool baremapsvalid = AreBitMapsValidInFsResync(checksumTransferMap, m_missingHcdsMap, m_syncDataAppliedMap, unprocessedClusterMap, msg);
    if (!baremapsvalid)
    {
        ResyncBitmapsInvalidAlert a(GetDeviceName());
        SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
        Idle(120);
        throw DataProtectionException(msg.str() + a.GetMessage(), SV_LOG_SEVERE);
    }

    SV_ULONG chunkSize = 0;
    bool SyncIdxVal, MissingHcdsIdxVal, SyncIdxPlus1Val, MissingHcdsIndxPlus1Val, HcdTransferIdxVal, HcdTransferIdxPlus1Val;
    SV_ULONGLONG offset;

    do
    {
        SyncIdxVal = m_syncDataAppliedMap->isBitSet(index);
        SyncIdxPlus1Val = m_syncDataAppliedMap->isBitSet(index + 1);

        MissingHcdsIdxVal = m_missingHcdsMap->isBitSet(index);
        MissingHcdsIndxPlus1Val = m_missingHcdsMap->isBitSet(index + 1);

        HcdTransferIdxVal = checksumTransferMap->isBitSet(index);
        HcdTransferIdxPlus1Val = checksumTransferMap->isBitSet(index + 1);

        int chunkNo = index / 2;

        if (m_VolumeSize % getChunkSize() != 0 && chunkNo == totalChunks() - 1) //chunkNo starts from 0
        {
            chunkSize = m_VolumeSize % m_MaxSyncChunkSize;
        }
        else
            chunkSize = m_MaxSyncChunkSize;

        if (!SyncIdxPlus1Val && SyncIdxVal)
        {
            std::stringstream msg;
            msg << "Sync Applied BitMap is corrupted for chunk[" << chunkNo << "] " << m_syncDataAppliedMap->GetBitMapFileName() << "..\n";
            throw DataProtectionException(msg.str().c_str());
        }

        offset = (SV_ULONGLONG)chunkNo * m_MaxSyncChunkSize;
        if (SyncIdxPlus1Val && SyncIdxVal)
        {
            /* applied */
            if (MissingHcdsIndxPlus1Val && MissingHcdsIdxVal)
            {
                /* applied but process map says matched, mark as unmatched */
                /*  could this occur? */
                DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s For chunk[%d], syncApplied map says applied but hcdProcess map says matched. "
                    "Hence marking it as unmatched and reducing %lu from matched bytes.\n",
                    FUNCTION_NAME, JobId().c_str(), chunkNo, chunkSize);
                m_missingHcdsMap->markAsUnMatched(offset, chunkSize);
                MissingHcdsIdxVal = false;
            }
            else if (!MissingHcdsIndxPlus1Val && !MissingHcdsIdxVal)
            {
                /* applied but process map says not processed, should have marked as not processed
                 *  not processed is already there. Is markAsUnMatched done to get sync file ? */
                /*  could this occur? */
                DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s For chunk[%d], syncApplied map says applied but hcdProcess map says unprocessed. "
                    "Hence marking it as unmatched.\n", FUNCTION_NAME, JobId().c_str(), chunkNo);
                m_missingHcdsMap->markAsUnMatched(offset);
                MissingHcdsIndxPlus1Val = true;
            }
        }

        if (!SyncIdxPlus1Val && !SyncIdxVal && MissingHcdsIndxPlus1Val && !MissingHcdsIdxVal)
        {
            /* sync maps says did not apply but process map says unmatched and sync file sent.
               here marking not processed */
            DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Sync file lost for chunk[%d]. Hence marking in hcdProcess map as not processed\n",
                FUNCTION_NAME, JobId().c_str(), chunkNo);
            missingSyncs++;
            m_missingHcdsMap->markAsNotProcessed(offset);
            MissingHcdsIndxPlus1Val = false;
        }

        if (MissingHcdsIndxPlus1Val && MissingHcdsIdxVal)
        {
            /* If matched, mark as not applied
             */
            m_syncDataAppliedMap->markAsNotApplied(offset);
            DebugPrintf(SV_LOG_DEBUG, "Marking Matched Chunk in Sync Applied Map for chunk %d\n", chunkNo);
            SyncIdxPlus1Val = true;
            SyncIdxVal = false;
        }

        if (MissingHcdsIndxPlus1Val && (!HcdTransferIdxPlus1Val && !HcdTransferIdxVal))
        {
            /* process map says hcd came but transfer map says not processed, mark as either
             * partially or fully used */
            DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s For chunk[%d], hcdProcess map says hcd processed, but hcdTransfer map says unprocessed."
                "Hence marking as fully used (can be either fully or partially used)\n", FUNCTION_NAME, JobId().c_str(), chunkNo);
            checksumTransferMap->markAsFullyUsed(offset);
            HcdTransferIdxPlus1Val = HcdTransferIdxVal = true;
        }

        if ((!MissingHcdsIndxPlus1Val && !MissingHcdsIdxVal) &&
            (!HcdTransferIdxPlus1Val && HcdTransferIdxVal))
        {
            /* process map says not processed, and transfer map says fully unused ,
             * mark as fully unused in process map */
            DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s For chunk[%d], hcdProcess map says not processed and hcdTransfer map says fully unused. Marking chunk as unused in hcdProcess map.\n",
                FUNCTION_NAME, JobId().c_str(), chunkNo);
            m_missingHcdsMap->markAsFullyUnUsed(offset);
            MissingHcdsIdxVal = true;
        }

        if (!MissingHcdsIndxPlus1Val && MissingHcdsIdxVal)
        {
            /* If fully unused, mark as not applied
             */
            m_syncDataAppliedMap->markAsNotApplied(offset);
            DebugPrintf(SV_LOG_DEBUG, "Marking unused Chunk in Sync Applied Map for chunk %d\n", chunkNo);
            SyncIdxPlus1Val = true;
            SyncIdxVal = false;
        }

        if ((!MissingHcdsIndxPlus1Val && !MissingHcdsIdxVal) &&
            HcdTransferIdxPlus1Val)
        {
            /* process map says not processed, and transfer map says sent, mark as not transferred */
            DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s For chunk[%d], hcdProcess map says not processed but "
                "hcdTransfer map says hcd sent. Hence marking as "
                "not processed in hcdTransfer map.\n", FUNCTION_NAME, JobId().c_str(), chunkNo);
            checksumTransferMap->markAsNotProcessed(offset);
            HcdTransferIdxPlus1Val = HcdTransferIdxVal = false;
        }

        size_t clusterBitPos = offset / unprocessedClusterMap->getBitMapGranualarity();

        if ((!HcdTransferIdxPlus1Val && !HcdTransferIdxVal) &&
            unprocessedClusterMap->isBitSet(clusterBitPos))
        {
            SV_ULONG clusterchunksize;
            if ((getVolumeSize() % unprocessedClusterMap->getBitMapGranualarity()) &&
                (clusterBitPos == GetClusterParams()->totalChunks() - 1))
            {
                clusterchunksize = getVolumeSize() % unprocessedClusterMap->getBitMapGranualarity();
            }
            else
            {
                clusterchunksize = unprocessedClusterMap->getBitMapGranualarity();
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s For chunk[%d], volume offset " ULLSPEC
                ", hcdTransfer map says not processed, "
                "and clusterTransfer map says cluster info sent, hence "
                "marking in clusterTransfer map as not transferred for "
                "bit position %u and reducing "
                "%lu from processed bytes\n", FUNCTION_NAME, JobId().c_str(), chunkNo, offset,
                clusterBitPos, clusterchunksize);
            unprocessedClusterMap->markAsNotTransferred(offset, clusterchunksize);
            missingClusters++;
        }

        index += 2;
    } while (index <= (totalChunks() * 2) - 2);

    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s Missed clusters=%d, Missed Sync files=%d\n", FUNCTION_NAME, JobId().c_str(), missingClusters, missingSyncs);

    if (m_syncDataAppliedMap->syncToPersistentStore(cxTransport) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync::Failed to sync sync data  transfer map to persistent store\n");
    }

    if (missingClusters + missingSyncs == 0)
    {
        m_missingHcdsMap->setPartialMatchedBytes(m_syncDataAppliedMap->getBytesNotProcessed());
    }

    if (!IsRcmMode())
    {
        UpdateResyncStats(true);
    }
    
    if (!SetFastSyncProgressBytes(m_syncDataAppliedMap->getBytesProcessed(), m_syncDataAppliedMap->getBytesNotProcessed(),
        GetResyncStatsForCSLegacyAsJsonString()))
    {
        throw DataProtectionException("FAILED FastSync: Failed to Update the FastSyncProgressBytes\n");
    }
    else
    {
        m_previousProgressBytes = m_syncDataAppliedMap->getBytesProcessed() + m_syncDataAppliedMap->getBytesNotProcessed();
    }

    if (checksumTransferMap->syncToPersistentStore(cxTransport) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync::Failed to sync checksum transfer map to persistent store\n");
    }

    if (!SetFastSyncFullyUnusedBytes(checksumTransferMap->getBytesNotProcessed()))
    {
        throw DataProtectionException("FAILED FastSync: Failed to Update the SetFastSyncUnusedBytes\n");
    }
    else
    {
        m_procClusterStage->setPrevUnUsedBytes(checksumTransferMap->getBytesNotProcessed());
    }

    SendMissingSyncsPacket(cxTransport);

    if (!cxTransport->deleteFile(completedMissingHcdsDatName))
    {
        DebugPrintf(SV_LOG_ERROR, "delete file %s failed: %s\n", completedMissingHcdsDatName.c_str(), cxTransport->status());
        throw DataProtectionException("FAILED FastSync::Failed to remove the missing hcd file from cx \n");
    }

    SendProcessedClusterMap(unprocessedClusterMap, cxTransport);

    if (!cxTransport->deleteFile(getUnProcessedClusterBitMapFileName()))
    {
        DebugPrintf(SV_LOG_ERROR, "delete file %s failed: %s\n", getUnProcessedClusterBitMapFileName().c_str(), cxTransport->status());
        throw DataProtectionException("FAILED FastSync::Failed to remove the unprocessed cluster map from CX\n");
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s reconciliation check END\n", FUNCTION_NAME, JobId().c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


void FastSync::SendProcessedClusterMap(SharedBitMapPtr unprocessedClusterMap, CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string completedProcessedClusterMap = getProcessedClusterBitMapFileName();
    std::string preProcessedClusterMap = getProcessedPreClusterBitMapFileName();
    if (IsTransportProtocolAzureBlob())
    {
        preProcessedClusterMap = completedProcessedClusterMap;
    }
    stringstream msg;

    if (unprocessedClusterMap->syncToPersistentStore(cxTransport, preProcessedClusterMap) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync::Failed to sync preProcessedClusterMap\n");
    }

    if (!IsTransportProtocolAzureBlob())
    {
        if (!cxTransport->renameFile(preProcessedClusterMap, completedProcessedClusterMap, COMPRESS_NONE))
        {
            cxTransport->deleteFile(preProcessedClusterMap);
            msg << "FAILED FastSync::Failed to rename the processed cluster packet From " << preProcessedClusterMap << "To " \
                << completedProcessedClusterMap << " Tal Status : " << cxTransport->status() << endl;
            throw DataProtectionException(msg.str());
        }
    }
    DebugPrintf(SV_LOG_ALWAYS, "%s: Reconciliation:%s sent ProcessedClusterMap[%s] (copy of unprocessedCluster post reconciliation, processed by source in ClusterTransfer map)\n",
        FUNCTION_NAME, JobId().c_str(), preProcessedClusterMap.c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


void FastSync::SendMissingSyncsPacket(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string preMissingSyncsDatFile = RemoteSyncDir() + PreSync + JobId() + missingSuffix;
    std::string completedMissingSyncsDatFile = RemoteSyncDir() + completedSyncPrefix + JobId() + missingSuffix;
    if (IsTransportProtocolAzureBlob())
    {
        preMissingSyncsDatFile = completedMissingSyncsDatFile;
    }
    stringstream msg;

    if (m_missingHcdsMap->syncToPersistentStore(cxTransport, preMissingSyncsDatFile) != SVS_OK)
    {
        throw DataProtectionException("FAILED FastSync::Failed to sync missing hcds map to persistent store\n");
    }
    if (!IsTransportProtocolAzureBlob())
    {
        if (!cxTransport->renameFile(preMissingSyncsDatFile, completedMissingSyncsDatFile, COMPRESS_NONE))
        {
            cxTransport->deleteFile(preMissingSyncsDatFile);
            msg << "FAILED FastSync::Failed to rename the missing chunks packet From " << preMissingSyncsDatFile << "To " \
                << completedMissingSyncsDatFile << " Tal Status : " << cxTransport->status() << endl;
            throw DataProtectionException(msg.str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

bool FastSync::sendResyncEndNotifyAndResyncTransition()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool status = false;

    if (!ResyncEndNotify())
    {
        DebugPrintf(SV_LOG_ERROR, "ResyncEndNotify Failed\n");
        return false;
    }

    int nReturn = this->setResyncTransitionStepOneToTwo("");
    std::stringstream errMsg;
    switch (nReturn)
    {
    case 4:
        errMsg << "setResyncTransitionStepOnetoTwo has failed to update resync transition state\n";
        break;
    case 3:
        errMsg << "setResyncTransitionStepOnetoTwo has failed to delete the sync No More file\n";
        break;
    case 0:
        break;
    default:
        errMsg << "setResyncTransitionStepOnetoTwo has failed with return value = ";
        errMsg << nReturn;
        errMsg << ".\n";
        break;
    }

    if (!errMsg.str().empty())
        DebugPrintf(SV_LOG_ERROR, errMsg.str().c_str());

    status = (nReturn == 0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SV_ULONG FastSync::totalChunks() const
{
    SV_ULONG rval = (SV_ULONG)((m_VolumeSize / (SV_LONGLONG)m_MaxSyncChunkSize) + (m_VolumeSize % (SV_LONGLONG)m_MaxSyncChunkSize ? 1 : 0));
    if (0 == rval)
    {
        std::stringstream msg;
        msg << "m_VolumeSize = " << m_VolumeSize
            << ", m_MaxSyncChunkSize = " << m_MaxSyncChunkSize;
        DebugPrintf(SV_LOG_ERROR, "In function totalChunks, with %s, the total chunks calculated are zero\n", msg.str().c_str());
    }
    /* DebugPrintf(SV_LOG_DEBUG, "In function totalChunks, with %s, total chunks are %lu\n", msg.str().c_str(), rval); */

    return rval;
}

string FastSync::getHcdProcessMapFileName() const
{
    return RemoteSyncDir() + mapHcdsProcessed + "_" + JobId() + ".Map";
}

string FastSync::getHcdTransferMapFileName() const
{
    return RemoteSyncDir() + mapHcdsTransferred + "_" + JobId() + ".Map";
}


string FastSync::getSyncApplyMapFileName() const
{
    return RemoteSyncDir() + mapSyncApplied + "_" + JobId() + ".Map";
}

string FastSync::getMissingHcdsMapFileName()
{
    return RemoteSyncDir() + mapMissingHcds + "_" + JobId() + ".Map";
}

string FastSync::getMissingSyncsMapFileName()
{
    return RemoteSyncDir() + mapMissingSyncs + "_" + JobId() + ".Map";
}


void FastSync::ProcessHcdTask::getHcdCountToProcess(
    const unsigned int &HCDToProcess,
    const HashCompareData::iterator &hcdIter,
    unsigned int &countofhcd,
    unsigned int &lengthtoread
    )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with HCDToProcess = %d\n", FUNCTION_NAME, HCDToProcess);
    countofhcd = 0;
    lengthtoread = 0;
    unsigned int acclen = 0;
    SV_LONGLONG prevoff = hcdIter->m_Offset;
    SV_UINT prevlen = 0;
    unsigned int incr = 0;
    unsigned int i = 0;
    SV_LONGLONG prevendoffset = 0;

    for ( /* empty */; i < HCDToProcess; i++)
    {
        incr = (hcdIter + i)->m_Length;
        prevendoffset = prevoff + prevlen;
        if (prevendoffset != (hcdIter + i)->m_Offset)
        {
            incr += ((hcdIter + i)->m_Offset - prevendoffset);
        }
        if ((acclen + incr) > m_FastSync->getReadBufferSize())
        {
            break;
        }
        prevoff = (hcdIter + i)->m_Offset;
        prevlen = (hcdIter + i)->m_Length;
        acclen += incr;
    }

    countofhcd = i;
    lengthtoread = acclen;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with count of hcd = %u, length to read = %u\n", FUNCTION_NAME, countofhcd, lengthtoread);
}

bool FastSync::ProcessHcdTask::readFromVolume(SV_LONGLONG offset, unsigned int length, char *pbuffer)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with offset = " LLSPEC
        ", length = %u, volume = %s\n", FUNCTION_NAME,
        offset, length, m_FastSync->GetDeviceName().c_str());
    bool bretval;
    SV_ULONG count;
    unsigned int nretry = 0;
    do
    {
        m_pDiskRead->start();
        m_volume->Seek(offset, BasicIo::BioBeg);
        count = m_volume->FullRead(pbuffer, length);
        bretval = (count == length);
        if (bretval)
        {
            m_pDiskRead->stop(count, offset);
            break;
        }
        std::stringstream msg;
        if (m_volume->Eof())
        {
            // ASSUMPTION: don't use an assert here as we want this test in the field
            // we should never see this as the targets should be aware of the sources size
            // and never send offsets outside their range. sources should always be
            // less then or equal to the size of their targets so sources would never send
            // out of range offsets to a target
            m_volume->Seek(0, BasicIo::BioEnd);
            VolumeReadFailureEOFAlert a(m_FastSync->GetDeviceName(), offset, m_volume->Tell());
            m_FastSync->SendAlert(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
            m_FastSync->Idle(120);
            if (IsRcmMode())
            {
                // Mark resync fail as disk size shrink is encountered.
                // TODO: Should we mark for resart resync?
                bretval = false;
                if (m_FastSync->FailResync(a.GetMessage()))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Marking resync fail as disk size shrink is encountered. Error: %s", FUNCTION_NAME, a.GetMessage().c_str());
                    return bretval;
                }
            }
            throw DataProtectionException(std::string(FUNCTION_NAME) + ": " + a.GetMessage() + "\n");
        }

        VolumeReadFailureAlertAtOffset a(m_FastSync->GetDeviceName(), offset, count, length, m_volume->LastError());
        msg << FUNCTION_NAME << ": " << a.GetMessage();
        if (nretry == m_FastSync->SourceReadRetries())
        {
            if (!m_FastSync->ZerosForSourceReadFailures())
            {
                cdp_volume_t::Ptr volume;
                std::string errmsg;
                if (!m_FastSync->createVolume(volume, errmsg))
                {
                    msg << "\nAn attempt to create volume also failed with error " << errmsg;
                }
                DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
                //SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
                // TODO - nichougu - 9.32 - Instead sending event fail resync and mark for restart resync - TASK 5749840
                m_FastSync->Idle(120);
                if (IsRcmMode())
                {
                    bretval = false;
                    // Mark resync fail as disk read attempts failed for last 30 mins.
                    if (m_FastSync->FailResync(a.GetMessage()))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Marking resync fail as disk read attempts failed for last 30 mins. Error: %s", FUNCTION_NAME, a.GetMessage().c_str());
                        return bretval;
                    }
                }
                throw DataProtectionException(msg.str(), SV_LOG_ERROR);
            }
            msg << "\nNOTE: Considering all zeros as contents for above, since same is asked for.";
            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
            memset(pbuffer, 0, length);
            bretval = true;
        }
        else
        {
            msg << " Retrying read after " << m_FastSync->SourceReadRetriesInterval() << " seconds. Read retry count = " << nretry;
            DiskReadErrorLogger::LogDiskReadError(msg.str(), boost::chrono::steady_clock::now());
        }
        nretry++;
    } while ((nretry <= m_FastSync->SourceReadRetries()) && !m_FastSync->QuitRequested(m_FastSync->SourceReadRetriesInterval()));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bretval;
}



void FastSync::SetMaxCSComputationThreads(SV_UINT maxCSComputationThreads)
{
    m_maxCSComputationThreads = maxCSComputationThreads;
}


SV_UINT FastSync::GetMaxCSComputationThreads(void)
{
    return m_maxCSComputationThreads;
}


bool FastSync::ProcessHcdTask::compareAndProcessHCD(ChecksumFile_t* csfile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::ostringstream msg;
    bool returnValue = true;
    bool balreadyprocessed = false;
    boost::shared_ptr<HashCompareData> &hashCompareData = csfile->hashCompareData;
    std::string const &deleteName = csfile->fileName;

    HashCompareData::iterator hcdIter = hashCompareData->Begin();
    HashCompareData::iterator end = hashCompareData->End();
    if (hcdIter != end)
    {
        m_FastSync->ThrottleSpace();
        /* This will work even if start of 64 MB offset is unused cluster */
        if (isHcdProcessed((*hcdIter).m_Offset, m_cxTransport, deleteName))
        {
            balreadyprocessed = true;
        }
        unsigned int HCDToProcess = hashCompareData->Count();

        if (!balreadyprocessed)
        {
            if (!m_volReadBuffers.breuse)
            {
                for (unsigned int i = 0; (i < m_volReadBuffers.numreadbufs) && HCDToProcess && (!m_FastSync->QuitRequested(0)); i++)
                {
                    if (!processHCDBatch(HCDToProcess,
                        hcdIter,
                        hashCompareData,
                        m_volReadBuffers.hcdbatch + i,
                        m_volReadBuffers.mb[i],
                        deleteName))
                    {
                        //TODO: what has to done here since exception is thrown by this
                        returnValue = false;
                        break;
                    }
                }

                if (returnValue)
                {
                    m_volReadBuffers.breuse = true;
                }
            }

            while (returnValue && HCDToProcess)
            {
                if (m_FastSync->QuitRequested(0))
                {
                    //TODO: Should we do send abort? How to deal with failures here
                    // Read volume fails, then do we keep the preTarget file lying?
                    // Should the return value be false
                    DebugPrintf(SV_LOG_DEBUG, "quit is requested in function compareAndProcessHCD\n");
                    returnValue = false;
                    break;
                }
                ACE_Message_Block* mbret = m_FastSync->retrieveMessage(m_sendToReadThreadQ);
                if (mbret)
                {
                    VolumeDataAndHCDBatch_t *pvDataAndHcdBatch = (VolumeDataAndHCDBatch_t *)mbret->base();
                    if (!processHCDBatch(HCDToProcess,
                        hcdIter,
                        hashCompareData,
                        pvDataAndHcdBatch,
                        mbret,
                        deleteName))
                    {
                        //TODO: what has to done here since exception is thrown by this
                        returnValue = false;
                        break;
                    }
                }
                else
                {
                    m_cxTransport->heartbeat();
                }
            }
        } /* if (!balreadyprocessed) */
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with return value = %s\n", FUNCTION_NAME, returnValue ? "true" : "false");
    return returnValue;
}

int FastSync::SendSyncDataWorker::open(void* args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}


int FastSync::SendSyncDataWorker::close(SV_ULONG flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}


int FastSync::SendSyncDataWorker::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s SendSyncDataWorker - sync data sender thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::QuitRequested), m_FastSync);
    try
    {
        while (!m_FastSync->QuitRequested(0))
        {
            VolumeDataAndHCDBatch_t *pvDataAndHcdBatch = NULL;
            ACE_Message_Block* mbret = m_FastSync->retrieveMessage(m_pcstQ);
            if (mbret)
            {
                VolumeDataAndHCDBatch_t *pvDataAndHcdBatch = (VolumeDataAndHCDBatch_t *)mbret->base();

                if (!SendUnMatchedSyncData(pvDataAndHcdBatch))
                {
                    //TODO: what has to be done since exception inside the SendUnMatchedSyncData
                    break;
                }

                if (!EnQ(m_volReadQ.get(), mbret, TaskWaitTimeInSeconds, qf))
                {
                    throw DataProtectionException("Enqueuing failed...\n");
                }
            }
            else
            {
                m_cxTransport->heartbeat();
            }
        }
        m_pSyncUpload->commit();

        if (E_SENT == m_eSyncFileHeaderStatus)
        {
            m_FastSync->SendAbort(m_cxTransport, m_preTarget);
        }
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->Stop();
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->Stop();
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s SendSyncDataWorker - sync data sender thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}


bool FastSync::SendSyncDataWorker::SendUnMatchedSyncData(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bretval = true;
    std::stringstream msg;

    if (0 == pvDataAndHcdBatch->volDataAndHcd->hcdIndex)
    {
        m_FastSync->nextPresyncAndSyncFileNames(m_preTarget, m_target, CompressMode());

        if (m_FastSync->IsTransportProtocolAzureBlob())
        {
            m_preTarget = m_target;
        }

        if (m_FastSync->IsDICheckEnabled())
        {
            m_CheckSumsToLog = m_target;
            m_CheckSumsToLog += '\n';
        }
        m_StartOffsetOfHCD = pvDataAndHcdBatch->volDataAndHcd->hcNode.m_Offset;
        DebugPrintf(SV_LOG_DEBUG, "The m_preTarget = %s, m_target = %s\n", m_preTarget.c_str(), m_target.c_str());
    }

    for (unsigned int i = 0; i < pvDataAndHcdBatch->countOfHCD; i++)
    {
        SendSyncData(pvDataAndHcdBatch, i);
        if (m_FastSync->IsDICheckEnabled())
        {
            /* TODO: will do the combined check sums hcd later */
            std::stringstream csrec;
            csrec << "compare: "
                << (pvDataAndHcdBatch->volDataAndHcd + (i))->hcNode.m_Offset << ','
                << (pvDataAndHcdBatch->volDataAndHcd + (i))->hcNode.m_Length << ','
                << GetPrintableCheckSum((pvDataAndHcdBatch->volDataAndHcd + (i))->hash) << ','
                << GetPrintableCheckSum((pvDataAndHcdBatch->volDataAndHcd + (i))->hcNode.m_Hash)
                << '\n';
            m_CheckSumsToLog += csrec.str();
        }
    }

    unsigned int hcdsinbatch = pvDataAndHcdBatch->countOfHCD;
    if ((pvDataAndHcdBatch->totalHCD - 1) == (pvDataAndHcdBatch->volDataAndHcd + (hcdsinbatch - 1))->hcdIndex)
    {
        char temp[1] = { 0 };
        if (E_SENT == m_eSyncFileHeaderStatus)
        {
            m_FastSync->SendData(m_cxTransport, m_preTarget, temp, 0, CX_TRANSPORT_NO_MORE_DATA, CompressMode());
        }

        SV_ULONG unusedBytes = 0;
        if (m_FastSync->getVolumeSize() % m_FastSync->getChunkSize() != 0 && (SV_ULONG)(m_StartOffsetOfHCD / m_FastSync->getChunkSize()) == m_FastSync->totalChunks() - 1)
        {
            unusedBytes = m_FastSync->getVolumeSize() % m_FastSync->getChunkSize() - m_bytesInHcd;
        }
        else
        {
            unusedBytes = m_FastSync->getChunkSize() - m_bytesInHcd;
        }

        DebugPrintf(SV_LOG_DEBUG, "For offset: " LLSPEC ", unused bytes: %lu\n", m_StartOffsetOfHCD, unusedBytes);
        m_bytesMatched += unusedBytes;

        if (E_SENT == m_eSyncFileHeaderStatus)
        {
            if (!m_cxTransport->renameFile(m_preTarget, m_target, CompressMode()))
            {
                msg << "Rename Failed From " << m_preTarget << "To " << m_target << " Failed: " << m_cxTransport->status() << endl;
                m_FastSync->SendAbort(m_cxTransport, m_preTarget);
                throw DataProtectionException(msg.str(), SV_LOG_ERROR);
            }
            m_FastSync->m_procHCDTask->markAsUnMatched(m_StartOffsetOfHCD);
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Marking Chunk starting at offset " ULLSPEC " as matched\n", m_StartOffsetOfHCD);
            m_FastSync->m_procHCDTask->markAsMatched(m_StartOffsetOfHCD, m_bytesMatched);
        }

        if (!m_cxTransport->deleteFile(pvDataAndHcdBatch->hcdName))
        {
            throw DataProtectionException("Remove Remote File Failed \n");
        }

        m_FastSync->m_procHCDTask->deleteFromOffsetsInProcess(m_StartOffsetOfHCD);
        --m_pendingFileCount;

        m_eSyncFileHeaderStatus = E_NEEDTOSEND;
        m_bytesMatched = 0;
        m_bytesInHcd = 0;
        if (m_FastSync->IsDICheckEnabled())
        {
            m_FastSync->dumpHashesToFile(m_FastSync->m_dumpHcdsHandle, m_CheckSumsToLog);
            m_CheckSumsToLog.clear();
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bretval;
}


void FastSync::SendSyncDataWorker::SendSyncData(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch, const unsigned int &i)
{
    m_bytesInHcd += (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Length;

    if (E_NOTMATCHED == (pvDataAndHcdBatch->volDataAndHcd + i)->eHcdMatchStatus)
    {
        if (-1 == m_PrevOffset)
        {
            RecordPrev(pvDataAndHcdBatch, i);
        }
        else if (IsContinuousWithinLimit(pvDataAndHcdBatch, i, m_FastSync->getReadBufferSize()))
        {
            m_PrevLength += (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Length;
        }
        else
        {
            SendPrevIfReq();
            RecordPrev(pvDataAndHcdBatch, i);
        }
    }
    else if (E_MATCHED == (pvDataAndHcdBatch->volDataAndHcd + i)->eHcdMatchStatus)
    {
        m_bytesMatched += (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Length;
        SendPrevIfReq();
    }
    else
    {
        if (E_SENT == m_eSyncFileHeaderStatus)
        {
            m_FastSync->SendAbort(m_cxTransport, m_preTarget);
        }
        std::stringstream msg;
        msg << "For sync file " << m_preTarget << " checksum match status is still pending before send\n";
        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
    }

    if (i + 1 == pvDataAndHcdBatch->countOfHCD)
    {
        SendPrevIfReq();
    }
}


bool FastSync::SendSyncDataWorker::IsContinuousWithinLimit(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch, const unsigned int &i,
    unsigned long lim)
{
    bool biscont = false;

    SV_LONGLONG prevendoff = m_PrevOffset + m_PrevLength;
    if (prevendoff == (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Offset)
    {
        biscont = ((m_PrevLength + (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Length) <= lim);
    }

    return biscont;
}


void FastSync::SendSyncDataWorker::RecordPrev(VolumeDataAndHCDBatch_t *pvDataAndHcdBatch, const unsigned int &i)
{
    m_PrevOffset = (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Offset;
    m_PrevVolData = (pvDataAndHcdBatch->volDataAndHcd + i)->volData;
    m_PrevLength = (pvDataAndHcdBatch->volDataAndHcd + i)->hcNode.m_Length;
}


void FastSync::SendSyncDataWorker::SendPrevIfReq(void)
{
    if (m_PrevLength)
    {
        bool bsent = SendSyncData(m_PrevOffset, m_PrevLength, m_PrevVolData);
        if (bsent)
        {
            ResetPrevHcd();
        }
        else
        {
            /* TODO: what todo as exception is already thrown */
        }
    }
}


bool FastSync::SendSyncDataWorker::SendSyncData(
    SV_LONGLONG offset,
    SV_UINT length,
    char *voldatatosend
    )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    //TODO: Error handling and deleting inconsistent file at CX from all places(Send Abort)
    bool bretval = true;

    //Need to send the sync file from here...
    if (!m_compressor->IsInitialized())
    {
        m_compressor->CompressStreamInit();
    }

    std::ostringstream msg;
    msg << "For file " << m_preTarget
        << " For volume " << m_FastSync->GetDeviceName()
        << ",";

    //Step: set the name of file member if 0 == hcdindex, at the same time, send svd1 header
    if (E_NEEDTOSEND == m_eSyncFileHeaderStatus)
    {
        int status = m_FastSync->sendSVDHeader(m_cxTransport,
            m_preTarget,
            CompressMode(),
            m_compressor);

        if (status == 0)
        {
            msg << "Sending SVD Header for sync file is failed." << '\n';
            m_FastSync->SendAbort(m_cxTransport, m_preTarget);
            throw DataProtectionException(msg.str());
        }

        m_eSyncFileHeaderStatus = E_SENT;
    }

    m_pSyncUpload->start();
    bretval = m_FastSync->sendSyncData(m_cxTransport,
        m_preTarget,
        length,
        voldatatosend,
        offset,
        CompressMode(),
        CX_TRANSPORT_MORE_DATA,
        m_compressor,
        msg);
    m_pSyncUpload->stop(length);
    if (!bretval)
    {
        m_FastSync->SendAbort(m_cxTransport, m_preTarget);
        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
    }

    if (m_FastSync->IsDICheckEnabled())
    {
        m_CheckSumsToLog += "clubbed: ";
        m_CheckSumsToLog += GetCSRecord(voldatatosend, offset, length);
        m_CheckSumsToLog += '\n';
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bretval;
}

int FastSync::PrimaryCSWorker::open(void* args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}


int FastSync::PrimaryCSWorker::close(SV_ULONG flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}


int FastSync::PrimaryCSWorker::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s PrimaryCSWorker - checksum calc thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    CSTasks_t tasks;
    MessageBlocks_t msgblks;

    try
    {
        ACE_Shared_MQ hcQ;
        ACE_Shared_MQ ackQ;
        std::vector<ACE_Shared_MQ> hcQs;
        std::vector<ACE_Shared_MQ> ackQs;

        /* Fork the remaining check sum thread */
        for (unsigned int i = 1; i < m_FastSync->GetMaxCSComputationThreads(); i++)
        {
            hcQ.reset(new ACE_Message_Queue<ACE_MT_SYNCH>());
            ackQ.reset(new ACE_Message_Queue<ACE_MT_SYNCH>());

            hcQs.push_back(hcQ);
            ackQs.push_back(ackQ);

            CSWorkerPtr csWorker(new CSWorker(m_FastSync, thr_mgr(), hcQs[i - 1], ackQs[i - 1], i));
            csWorker->open();
            tasks.push_back(csWorker);

            ACE_Message_Block *mb = new ACE_Message_Block();
            mb->msg_priority(SYNC_NORMAL_PRIORITY);
            msgblks.push_back(mb);
        }

        QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::QuitRequested), m_FastSync);
        while (!m_FastSync->QuitRequested(0))
        {
            ACE_Message_Block* mbret = m_FastSync->retrieveMessage(m_volReadQ);
            if (mbret)
            {
                VolumeDataAndHCDBatch_t *pvDataAndHcdBatch = (VolumeDataAndHCDBatch_t *)mbret->base();

                DebugPrintf(SV_LOG_DEBUG, "countOfHcd = %u, totalHCD = %u, volDataAndHCD = %p, hcdName = %s\n",
                    pvDataAndHcdBatch->countOfHCD, pvDataAndHcdBatch->totalHCD,
                    pvDataAndHcdBatch->volDataAndHcd, pvDataAndHcdBatch->hcdName.c_str());

                for (unsigned int i = 1; i < m_FastSync->GetMaxCSComputationThreads(); i++)
                {
                    //Step: Give this to checksums threads
                    msgblks[i - 1]->base((char *)(pvDataAndHcdBatch), sizeof (VolumeDataAndHCDBatch_t));

                    if (!EnQ(hcQs[i - 1].get(), msgblks[i - 1], TaskWaitTimeInSeconds, qf))
                    {
                        throw DataProtectionException("Enqueuing failed...\n");
                    }
                }

                DebugPrintf(SV_LOG_DEBUG, "volData = %p, hcdIndex = %u,"
                    "hcd offset = " LLSPEC ", hcd length = %u\n",
                    pvDataAndHcdBatch->volDataAndHcd->volData,
                    pvDataAndHcdBatch->volDataAndHcd->hcdIndex,
                    pvDataAndHcdBatch->volDataAndHcd->hcNode.m_Offset,
                    pvDataAndHcdBatch->volDataAndHcd->hcNode.m_Length);

                unsigned int nIndex = m_threadID;
                while ((nIndex < pvDataAndHcdBatch->countOfHCD) && (!m_FastSync->QuitRequested()))
                {
                    m_pHcdCalc->start();
                    m_FastSync->UpdateHCDMatchState(pvDataAndHcdBatch->volDataAndHcd + nIndex);
                    m_pHcdCalc->stop(pvDataAndHcdBatch->volDataAndHcd->hcNode.m_Length);
                    nIndex += m_FastSync->GetMaxCSComputationThreads();
                }

                for (unsigned int i = 1; (i < m_FastSync->GetMaxCSComputationThreads()) && (!m_FastSync->QuitRequested(0)); i++)
                {
                    while (!m_FastSync->QuitRequested(0))
                    {
                        ACE_Message_Block* csdonemb = m_FastSync->retrieveMessage(ackQs[i - 1]);
                        if (csdonemb)
                        {
                            break;
                        }
                    }
                }

                if (!m_FastSync->QuitRequested(0))
                {
                    if (!EnQ(m_sendQ.get(), mbret, TaskWaitTimeInSeconds, qf))
                    {
                        throw DataProtectionException("Enqueuing failed...\n");
                    }
                }
            }
        }
        m_pHcdCalc->commit();
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->Stop();
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->Stop();
    }

    /* Wait for check sums task */
    CSTasks_t::iterator iter(tasks.begin());
    CSTasks_t::iterator end(tasks.end());
    for (; iter != end; ++iter)
    {
        CSWorkerPtr taskptr = (*iter);
        thr_mgr()->wait_task(taskptr.get());
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s PrimaryCSWorker - checksum calc thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}


void FastSync::ProcessHcdTask::FillHcdBatchToProcess(
    const unsigned int &countofhcd,
    const std::string &deletename,
    const unsigned int &numHCDs,
    const HashCompareData::iterator &hcdbegin,
    char *volData,
    VolumeDataAndHCDBatch_t *pvDataAndHcdFirstBatch,
    HashCompareData::iterator &hcdIter
    )
{
    pvDataAndHcdFirstBatch->countOfHCD = countofhcd;
    pvDataAndHcdFirstBatch->hcdName = deletename;
    pvDataAndHcdFirstBatch->totalHCD = numHCDs;
    SV_LONGLONG startoffset = hcdIter->m_Offset;

    for (unsigned int i = 0; i < countofhcd; i++)
    {
        (pvDataAndHcdFirstBatch->volDataAndHcd + i)->volData = volData + (hcdIter->m_Offset - startoffset);
        (pvDataAndHcdFirstBatch->volDataAndHcd + i)->hcNode = *hcdIter;
        (pvDataAndHcdFirstBatch->volDataAndHcd + i)->hcdIndex = (hcdIter - hcdbegin);
        hcdIter++;
    }
}


/**
 * Lets keep here since PrimaryCSWorker and CSWorker use this
 * Ideally, there has to be a class that just compares hcds
 */
void FastSync::UpdateHCDMatchState(VolumeDataAndHCD_t *pvolDataAndHcd)
{
    /* DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with hcd length = %u, hcd offset = " LLSPEC
       " ,volData = %p\n", FUNCTION_NAME, pvolDataAndHcd->hcNode.m_Length,
       pvolDataAndHcd->hcNode.m_Offset, pvolDataAndHcd->volData); */
	bool bhashesequal = AreHashesEqual(pvolDataAndHcd->hcNode, pvolDataAndHcd->volData, IsDICheckEnabled(), pvolDataAndHcd->hash, pvolDataAndHcd->getHashsize());
    if (bhashesequal)
    {
        /* DebugPrintf(SV_LOG_DEBUG, "offset: " LLSPEC ", length: %u matched. hcdIndex = %u\n",
           pvolDataAndHcd->hcNode.m_Offset, pvolDataAndHcd->hcNode.m_Length, pvolDataAndHcd->hcdIndex); */
        pvolDataAndHcd->eHcdMatchStatus = E_MATCHED;
    }
    else
    {
        /* DebugPrintf(SV_LOG_DEBUG, "offset: " LLSPEC ", length: %u unmatched. hcdIndex = %u\n",
           pvolDataAndHcd->hcNode.m_Offset, pvolDataAndHcd->hcNode.m_Length, pvolDataAndHcd->hcdIndex); */
        pvolDataAndHcd->eHcdMatchStatus = E_NOTMATCHED;
    }
    /* DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME); */
}


int FastSync::CSWorker::open(void* args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}


int FastSync::CSWorker::close(SV_ULONG flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}


int FastSync::CSWorker::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s CSWorker - checksum calc thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::QuitRequested), m_FastSync);
    while (!m_FastSync->QuitRequested(0))
    {
        ACE_Message_Block* mbret = m_FastSync->retrieveMessage(m_hcQ);
        if (mbret)
        {
            VolumeDataAndHCDBatch_t *pvHcdBatch = (VolumeDataAndHCDBatch_t*)mbret->base();
            VolumeDataAndHCD_t *pvDataAndHcd = pvHcdBatch->volDataAndHcd;

            SV_UINT nIndex = m_threadID;
            while ((nIndex < pvHcdBatch->countOfHCD) && (!m_FastSync->QuitRequested(0)))
            {
                m_pHcdCalc->start();
                m_FastSync->UpdateHCDMatchState(pvDataAndHcd + nIndex);
                m_pHcdCalc->stop(pvDataAndHcd->hcNode.m_Length);

                nIndex += m_FastSync->GetMaxCSComputationThreads();
            }
            if (!EnQ(m_ackQ.get(), mbret, TaskWaitTimeInSeconds, qf))
            {
                throw DataProtectionException("Enqueuing failed...\n");
            }
        }
    }
    m_pHcdCalc->commit();

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s CSWorker - checksum calc thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}


bool FastSync::IsDICheckEnabled(void)
{
    return m_bDICheck;
}

bool FastSync::IsProcessClusterPipeEnabled(void)
{
    return m_bIsProcessClusterPipeEnabled;
}

SV_UINT FastSync::GetMaxHcdsAllowdAtCx(void)
{
    return m_maxHcdsAllowdAtCx;
}

SV_UINT FastSync::GetSecsToWaitForHcdSend(void)
{
    return m_secsToWaitForHcdSend;
}

/**
 * Lets keep here since PrimaryCSWorker and CSWorker use this
 */
bool FastSync::AreHashesEqual(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash,int phashSize)
{
	return (this->*m_areHashesEqual[m_compareHcd])(node, buffer, brecordchecksum, phash, phashSize);
}


bool FastSync::IsHashEqual(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash, int phashSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    /* DebugPrintf(SV_LOG_DEBUG, "node.m_Offset = " LLSPEC ", node.m_Length = %u, buffer = %p\n",
       node.m_Offset, node.m_Length, buffer); */
    unsigned char hash[HASH_LEN];
    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)buffer, node.m_Length);
    INM_MD5Final(hash, &ctx);

    /* TODO: remove this check of brecordchecksum and decide using function pointer */
    if (brecordchecksum)
    {
		inm_memcpy_s(phash, phashSize, hash, HASH_LEN);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (0 == memcmp(node.m_Hash, hash, sizeof(hash)));
}


bool FastSync::ReturnHashesAsUnequal(HashCompareNode const & node, char* buffer, bool brecordchecksum, unsigned char *phash,int phashSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (brecordchecksum)
    {
        memset(phash, 0, HASH_LEN);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return false;
}


std::string FastSync::SendSyncDataWorker::GetCSRecord(char *buffer, SV_LONGLONG offset, SV_UINT length)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with buffer = %p, offset = " LLSPEC ", length = %u\n", FUNCTION_NAME, buffer, offset, length);
    unsigned char hash[16];
    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)buffer, length);
    INM_MD5Final(hash, &ctx);

    std::string checksumData;
    std::stringstream cstolog;
    cstolog << offset << ','
        << length << ','
        << GetPrintableCheckSum(hash);
    checksumData = cstolog.str();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return checksumData;
}


bool FastSync::ProcessHcdTask::processHCDBatch(
    unsigned int &HCDToProcess,
    HashCompareData::iterator &hcdIter,
    boost::shared_ptr<HashCompareData> &hashCompareData,
    VolumeDataAndHCDBatch_t *pVolDataAndHCDBatch,
    ACE_Message_Block *mb,
    const std::string &deleteName
    )
{
    bool returnValue = true;
    unsigned int countofhcd;
    unsigned int length;
    SV_LONGLONG readoffset = (*hcdIter).m_Offset + m_FastSync->GetSrcStartOffset();
    QuitFunction_t qf = std::bind1st(std::mem_fun(&FastSync::QuitRequested), m_FastSync);

    getHcdCountToProcess(HCDToProcess, hcdIter, countofhcd, length);

    if (length > pVolDataAndHCDBatch->allocatedReadLen)
    {
        std::stringstream msg;
        msg << "For Volume: " << m_FastSync->GetDeviceName()
            << ", offset in hcd(including source start offset): " << readoffset
            << ", hcd file name: " << deleteName
            << ", length to read in batch: " << length
            << " is greater than allocated batch length: " << pVolDataAndHCDBatch->allocatedReadLen
            << ". This is due to mismatch in hash compare sizes at source and target or internal logic error\n";
        throw DataProtectionException(msg.str());
    }

    if (!readFromVolume(readoffset, length, pVolDataAndHCDBatch->volDataAndHcd->volData))
    {
        //TODO: what has to be done here since exception is thrown by readFromVolume
        returnValue = false;
    }
    else
    {
        FillHcdBatchToProcess(
            countofhcd,
            deleteName,
            hashCompareData->Count(),
            hashCompareData->Begin(),
            pVolDataAndHCDBatch->volDataAndHcd->volData,
            pVolDataAndHCDBatch, hcdIter
            );
        //Step: If send fails, there is exception thrown. Hence can
        //      assume that HCDProcessed
        HCDToProcess -= countofhcd;
        //Step: Give this to primary checksums thread
        if (!EnQ(m_readThreadToPcstQ.get(), mb, TaskWaitTimeInSeconds, qf))
        {
            //TODO: what has to be done here since exception is thrown by EnQ
            //      Now that threads have been created, we should wait for them
            returnValue = false;
            throw DataProtectionException("Enqueuing failed...\n");
        }
    } /* else */

    return returnValue;
}


bool FastSync::AreBitMapsValid(
    SharedBitMap *checksumTransferMap,
    SharedBitMapPtr &missingHcdsMap,
    SharedBitMapPtr &syncDataAppliedMap,
    std::ostringstream &msg
    )
{
    bool bvalid = true;

    SV_ULONG HcdTransferChunkSize = checksumTransferMap->getBitMapGranualarity();
    SV_ULONG HcdMissingChunkSize = missingHcdsMap->getBitMapGranualarity();
    SV_ULONG SyncAppliedChunkSize = syncDataAppliedMap->getBitMapGranualarity();

    if (HcdTransferChunkSize != HcdMissingChunkSize)
    {
        msg << "Hcd Transfer bit map granularity " << HcdTransferChunkSize
            << " does not match Hcd Missing bit map granularity " << HcdMissingChunkSize
            << '.';
        bvalid = false;
    }

    if (bvalid && (HcdTransferChunkSize != SyncAppliedChunkSize))
    {
        msg << "Hcd Transfer bit map granularity " << HcdTransferChunkSize
            << " does not match Sync Applied bit map granularity " << SyncAppliedChunkSize
            << '.';
        bvalid = false;
    }


    if (bvalid)
    {
        SV_ULONG HcdTransferMapSize = checksumTransferMap->getBitMapSize();
        SV_ULONG HcdMissingMapSize = missingHcdsMap->getBitMapSize();
        SV_ULONG SyncAppliedMapSize = syncDataAppliedMap->getBitMapSize();
        if (HcdMissingMapSize != SyncAppliedMapSize)
        {
            msg << "Hcd Missing map size " << HcdMissingMapSize
                << " does not match sync applied map size " << SyncAppliedMapSize
                << '.';
            bvalid = false;
        }
        if (bvalid && (HcdTransferMapSize != (SyncAppliedMapSize / 2)))
        {
            msg << "Hcd Transfer map size " << HcdTransferMapSize
                << " does not equal half of sync applied map size " << SyncAppliedMapSize
                << '.';
            bvalid = false;
        }
    }

    return bvalid;
}


bool FastSync::AreBitMapsValidInFsResync(
    SharedBitMap *checksumTransferMap,
    SharedBitMapPtr &missingHcdsMap,
    SharedBitMapPtr &syncDataAppliedMap,
    SharedBitMapPtr &unprocessedClusterMap,
    std::ostringstream &msg
    )
{
    bool bvalid = true;

    SV_ULONG HcdTransferChunkSize = checksumTransferMap->getBitMapGranualarity();
    SV_ULONG HcdMissingChunkSize = missingHcdsMap->getBitMapGranualarity();
    SV_ULONG SyncAppliedChunkSize = syncDataAppliedMap->getBitMapGranualarity();

    if (HcdTransferChunkSize != HcdMissingChunkSize)
    {
        msg << "Hcd Transfer bit map granularity " << HcdTransferChunkSize
            << " does not match Hcd Missing bit map granularity " << HcdMissingChunkSize
            << '.';
        bvalid = false;
    }

    if (bvalid && (HcdTransferChunkSize != SyncAppliedChunkSize))
    {
        msg << "Hcd Transfer bit map granularity " << HcdTransferChunkSize
            << " does not match Sync Applied bit map granularity " << SyncAppliedChunkSize
            << '.';
        bvalid = false;
    }


    if (bvalid)
    {
        SV_ULONG HcdTransferMapSize = checksumTransferMap->getBitMapSize();
        SV_ULONG HcdMissingMapSize = missingHcdsMap->getBitMapSize();
        SV_ULONG SyncAppliedMapSize = syncDataAppliedMap->getBitMapSize();
        if (HcdMissingMapSize != SyncAppliedMapSize)
        {
            msg << "Hcd Missing map size " << HcdMissingMapSize
                << " does not match sync applied map size " << SyncAppliedMapSize
                << '.';
            bvalid = false;
        }
        if (bvalid && (HcdTransferMapSize != SyncAppliedMapSize))
        {
            msg << "Hcd Transfer map size " << HcdTransferMapSize
                << " does not equal sync applied map size " << SyncAppliedMapSize
                << '.';
            bvalid = false;
        }
    }

    if (bvalid)
    {
        bvalid = IsClusterTransferMapValid(unprocessedClusterMap, msg);
    }

    return bvalid;
}


bool FastSync::IsClusterTransferMapValid(SharedBitMapPtr &unprocessedClusterMap,
    std::ostringstream &msg)
{
    return m_clusterParamsPtr->IsClusterTransferMapValid(unprocessedClusterMap, msg);
}


bool FastSync::PerformCleanUpIfRequired(void)
{
    bool bcleaned = false;
    CxTransport::ptr cxTransport;
    try
    {
        cxTransport.reset(new CxTransport(Protocol(), TransportSettings(), Secure(), getCfsPsData().m_useCfs, getCfsPsData().m_psId));
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "FastSync::PerformCleanUpIfRequired create CxTransport failed: %s\n", e.what());
        return false;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "FastSync::PerformCleanUpIfRequired create CxTransport failed: unknonw error\n");
        return false;
    }

    std::string completedMissingSyncsDatFile = RemoteSyncDir() + completedSyncPrefix + JobId() + missingSuffix;
    FileInfosPtr MissingSyncInfo(new FileInfos_t);
    boost::tribool tbms = cxTransport->listFile(completedMissingSyncsDatFile, *MissingSyncInfo.get());
    if (!tbms)
    {
        if (m_agentRestartStatus == AGENT_FRESH_START)
        {
            DebugPrintf(SV_LOG_ERROR, "Getting file list to know whether %s is present has failed: %s\n", completedMissingSyncsDatFile.c_str(), cxTransport->status());
        }
        // MT and PS are remote in failback scenario where transport can fail.
        // So safe to ignore VxToPSCommunicationFailAlert in RCM mode.
        if (Settings().isInMageDataPlane())
        {
            VxToPSCommunicationFailAlert a(TransportSettings().ipAddress, TransportSettings().port, TransportSettings().sslPort,
                GetStrTransportProtocol(Protocol()), Secure());
            SendAlert(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a);
        }
    }
    else if (tbms)
    {
        /* File is present */
        DebugPrintf(SV_LOG_DEBUG, "File %s is present at CX. Checking further if cleanup is required\n", completedMissingSyncsDatFile.c_str());
        std::string completedMissingHcdsDatName = RemoteSyncDir() + completedHcdPrefix + JobId() + missingSuffix;;
        FileInfosPtr MissingHcdInfo(new FileInfos_t);

        boost::tribool tbmh = cxTransport->listFile(completedMissingHcdsDatName, *MissingHcdInfo.get());
        if (!tbmh)
        {
            DebugPrintf(SV_LOG_ERROR, "Getting file list to know whether %s is present has failed: %s\n", completedMissingHcdsDatName.c_str(), cxTransport->status());
        }
        else if (tbmh)
        {
            /* File is present */
            DebugPrintf(SV_LOG_DEBUG, "File %s is present at CX. Need to clean this file\n", completedMissingHcdsDatName.c_str());
            SharedBitMapPtr missingSyncsMap;
            SharedBitMapPtr missingHcdsMap;
            bool bShouldCreateIfNotPresent = false;

            bool bIsMissingSyncMapProper = initSharedBitMapFile(missingSyncsMap, completedMissingSyncsDatFile, cxTransport, bShouldCreateIfNotPresent);
            bool bIsMissingHcdMapProper = initSharedBitMapFile(missingHcdsMap, completedMissingHcdsDatName, cxTransport, bShouldCreateIfNotPresent);

            if (bIsMissingSyncMapProper && bIsMissingHcdMapProper)
            {
                boost::tribool tbd = cxTransport->deleteFile(completedMissingHcdsDatName);
                if (tbd)
                {
                    DebugPrintf(SV_LOG_DEBUG, "cleaned up stale file %s at CX\n", completedMissingHcdsDatName.c_str());
                    bcleaned = true;
                }
                if (!bcleaned)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to delete stale file %s at CX: %s\n", completedMissingHcdsDatName.c_str(), cxTransport->status());
                }
            }
        }
        else
        {
            /* File is not present */
            DebugPrintf(SV_LOG_DEBUG, "File %s is not present in CX. Hence clean up is not required\n", completedMissingHcdsDatName.c_str());
            bcleaned = true;
        }
    }
    else
    {
        /* File is not present */
        DebugPrintf(SV_LOG_DEBUG, "File %s is not present in CX. Hence clean up is not required\n", completedMissingSyncsDatFile.c_str());
        bcleaned = true;
    }

    if (false == bcleaned)
    {
        if (m_agentRestartStatus == AGENT_FRESH_START)
        {
            DebugPrintf(SV_LOG_ERROR, "cleanup did not succeed\n");
        }
    }

    return bcleaned;
}


bool FastSync::AreClusterParamsValid(const unsigned long &clustersyncchunksize,
    const SV_UINT &clustersize,
    std::ostream &msg)
{
    bool brvalid = true;

    if (getVolumeSize() % clustersize)
    {
        msg << "volume size: " << getVolumeSize()
            << " is not exact multiple of "
            << "cluster size: " << clustersize
            << ". For volume " << GetDeviceName() << '\n';
        brvalid = false;
    }

    /* TODO: should we auto set and continue below
     * all parameters if they are wrong ?
     * In which case, we have to call set
     * proper parameters first */

    /* cluster sync chunk size to be multiple of cluster size: so that
     * last bit in cluster bitmap indicates a full cluster and not the
     * incomplete one */
    if (clustersyncchunksize % clustersize)
    {
        msg << "cluster sync chunk size: " << clustersyncchunksize
            << " is not exact multiple of "
            << "cluster size: " << clustersize
            << ". For volume " << GetDeviceName() << '\n';
        brvalid = false;
    }

    /* TODO: should this be only specific for ntfs ?
     * For now add the check, but it has to be more
     * user friendly and check itself might have to be
     * removed if we adjust ? */

    /* second cluster number to query which is the cluster number got
     * from cluster sync chunk size divided by cluster size,
     * should be multipe of 8, because it is mandated by ntfs and
     * also by our algorithm where in bitmap bytes are sent in cluster
     * bitmap file and starting cluster number has to be the first bit
     * in the first byte. */
    unsigned long q = clustersyncchunksize / clustersize;
    if (q % NBITSINBYTE)
    {
        msg << "For cluster sync chunk size: " << clustersyncchunksize
            << ", for cluster size: " << clustersize
            << ", second cluster number to query " << q
            << " (first cluster number being zero) "
            << ", is not exact multiple of " << NBITSINBYTE
            << ". This is mandated by system"
            << ". Volume " << GetDeviceName() << '\n';
        brvalid = false;
    }

    /* so that one hcd should cover complete cluster */
    if (m_MaxSyncChunkSize % clustersize)
    {
        msg << "max sync chunk size: " << m_MaxSyncChunkSize
            << " is not exact multiple of "
            << "cluster size: " << clustersize
            << ". For volume " << GetDeviceName() << '\n';
        brvalid = false;
    }

    /* so that in read buffer covers complete cluster */
    if (m_readBufferSize % clustersize)
    {
        msg << "read buffer size: " << m_readBufferSize
            << " is not exact multiple of "
            << "cluster size: " << clustersize
            << ". For volume " << GetDeviceName() << '\n';
        brvalid = false;
    }

    return brvalid;
}


bool FastSync::IsReadBufferLengthValid(std::ostream &msg)
{
    bool brval = true;

    if (0 == m_readBufferSize)
    {
        msg << "read buffer size is zero";
        brval = false;
    }

    if (m_readBufferSize % m_HashCompareDataSize)
    {
        msg << "read buffer size: "
            << m_readBufferSize
            << " is not exact multiple of "
            << "HashCompareDataSize: "
            << m_HashCompareDataSize;
        brval = false;
    }

    return brval;
}


int FastSync::ProcessClusterTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}


int FastSync::ProcessClusterTask::close(SV_ULONG flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}


int FastSync::ProcessClusterTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int rval = -1;

    try
    {
        std::stringstream excmsg;
        excmsg << "For volume " << m_FastSync->GetDeviceName() << ", ";
        if (!m_pGenerateHcdStage->Init(excmsg))
        {
            throw DataProtectionException(excmsg.str());
        }

        m_pProcessClusterStage->SetGenAndSendHcdStage(m_pGenerateHcdStage, m_pSendHcdStage);
        rval = m_pProcessClusterStage->svc();
        m_pGenerateHcdStage->DeInit();
        m_FastSync->PostMsg(SYNC_DONE, SYNC_NORMAL_PRIORITY);
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->Stop();
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->Stop();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rval;
}


unsigned long FastSync::GetHashCompareDataSize(void)
{
    /* TODO: remove hash compare data size
     * completely as from file system aware
     * resync, this has no meaning at all */
    return m_HashCompareDataSize;
}


SV_UINT FastSync::GetClusterSizeFromVolume(cdp_volume_t::Ptr& volume)
{
    cdp_volume_util u;
    std::string errmsg;
    E_FS_SUPPORT_ACTIONS fssa = (VOLUME_SETTINGS::RESYNC_COPYMODE_FILESYSTEM == GetResyncCopyMode()) ?
    E_SET_NOFS_CAPABILITIES_IFNOSUPPORT : E_SET_NOFS_CAPABILITIES;

    SV_UINT c = u.GetClusterSize(*volume.get(), Settings().fstype, getVolumeSize(), GetSrcStartOffset(), fssa, errmsg);
    if (0 == c)
    {
        std::stringstream msg;
        msg << "For volume " << volume->GetName() << ", file system cluster size got is zero";
        throw DataProtectionException(msg.str());
    }

    return c;
}


FastSync::ClusterParams *FastSync::GetClusterParams(void)
{
    return m_clusterParamsPtr.get();
}


std::string FastSync::getClusterBitMapFileName(void)
{
    return RemoteSyncDir() + mapClusterTransferred + "_" + JobId() + ".Map";
}


std::string FastSync::getUnProcessedClusterBitMapFileName(void)
{
    std::string s = RemoteSyncDir();

    s += Completed;
    s += UNDERSCORE;
    s += UnProcessed;
    s += UNDERSCORE;
    s += Cluster;
    s += UNDERSCORE;
    s += JobId();
    s += DatExtension;

    return s;
}


std::string FastSync::getProcessedClusterBitMapFileName(void)
{
    std::string s = RemoteSyncDir();

    s += Completed;
    s += UNDERSCORE;
    s += Processed;
    s += UNDERSCORE;
    s += Cluster;
    s += UNDERSCORE;
    s += JobId();
    s += DatExtension;

    return s;
}


std::string FastSync::getUnProcessedPreClusterBitMapFileName(void)
{
    std::string s = RemoteSyncDir();

    s += PRE;
    s += UNDERSCORE;
    s += Completed;
    s += UNDERSCORE;
    s += UnProcessed;
    s += UNDERSCORE;
    s += Cluster;
    s += UNDERSCORE;
    s += JobId();
    s += DatExtension;

    return s;
}


std::string FastSync::getProcessedPreClusterBitMapFileName(void)
{
    std::string s = RemoteSyncDir();

    s += PRE;
    s += UNDERSCORE;
    s += Completed;
    s += UNDERSCORE;
    s += Processed;
    s += UNDERSCORE;
    s += Cluster;
    s += UNDERSCORE;
    s += JobId();
    s += DatExtension;

    return s;
}


// GenerateHcdTask
// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::GenerateHcdTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int FastSync::GenerateHcdTask::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// for each diff info gets the diff data. if the data is retrieved
// successfull, psosts a message to the apply task
// --------------------------------------------------------------------------
int FastSync::GenerateHcdTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s GenerateHcdTask - hcd generator primary thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    try
    {
        GenerateHcdFiles();
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }

    GenerateHCDWorkerTasks_t::iterator beginIter(m_genTasks.begin());
    GenerateHCDWorkerTasks_t::iterator endIter(m_genTasks.end());
    for (; beginIter != endIter; beginIter++)
    {
        GenerateHCDWorkerPtr taskptr = (*beginIter);
        thr_mgr()->wait_task(taskptr.get());
    }

    if (m_checksumTransferMap->syncToPersistentStore(m_cxTransport) == SVS_OK)
    {
        m_FastSync->SetFastSyncFullSyncBytes(m_checksumTransferMap->getBytesProcessed());
    }
    m_FastSync->PostMsg(SYNC_DONE, SYNC_NORMAL_PRIORITY);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s GenerateHcdTask - hcd generator primary thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
    return 0;
}

bool FastSync::GenerateHcdTask::canSendHCDs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    bool bRetValue = false;
    size_t index1 = m_checksumTransferMap->getFirstUnSetBit();
    if (index1 != boost::dynamic_bitset<>::npos)
    {
        const std::string& remoteDir = m_FastSync->RemoteSyncDir();
        const std::string& jobId = m_FastSync->JobId();
        std::string completedMissingSyncsPkt = remoteDir + completedSyncPrefix + jobId + missingSuffix;
        std::string completedMissingHCDsPkt = remoteDir + completedHcdPrefix + jobId + missingSuffix;
        Files_t files;
        if (m_FastSync->GetFileList(m_cxTransport, completedMissingHCDsPkt, files) && files.size() == 0 &&
            m_FastSync->GetFileList(m_cxTransport, completedMissingSyncsPkt, files) && files.size() == 0)
        {
            bRetValue = true;
        }
    }
    else
    {
        m_FastSync->sendHCDNoMoreIfRequired(m_cxTransport);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
    return bRetValue;
}

bool FastSync::sendHCDNoMoreIfRequired(CxTransport::ptr cxTransport)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    Files_t files;
    string hcdNoMoreFile = RemoteSyncDir() + completedHcdPrefix + JobId() + noMore;
    bool sent = false;

    DebugPrintf(SV_LOG_DEBUG, "hcd no more file name is %s\n", hcdNoMoreFile.c_str());
    if (GetFileList(cxTransport, hcdNoMoreFile, files))
    {
        if (files.size())
        {
            sent = true;
            DebugPrintf(SV_LOG_DEBUG, "file %s is already present at cx. "
                "Hence not sending.\n",
                hcdNoMoreFile.c_str());
        }
        else
            sent = SendHcdNoMoreFile(cxTransport);
    }
    else
        DebugPrintf(SV_LOG_ERROR, "Failed to get the file list for file spec %s.\n", hcdNoMoreFile.c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
    return sent;
}

void FastSync::GenerateHcdTask::GenerateHcdFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    setPendingChunkCount(0);
    ACE_Shared_MQ chunkQueue(new ACE_Message_Queue<ACE_MT_SYNCH>());
    SV_UINT taskCount = m_FastSync->GetConfigurator().getMaxFastSyncGenerateHCDThreads();
    SV_ULONG intervalTime = 0;
    m_prevFullSyncBytesSent = 0;
    SV_UINT TotalNoOfChunks = m_FastSync->totalChunks();


    while (!m_FastSync->QuitRequested(WindowsWaitTimeInSeconds))
    {
        m_cxTransport->heartbeat();

        intervalTime += WindowsWaitTimeInSeconds;
        if (intervalTime >= m_FastSync->m_ResyncUpdateInterval)
        {
            if (m_checksumTransferMap->syncToPersistentStore(m_cxTransport) == SVS_OK &&
                m_prevFullSyncBytesSent.value() != m_checksumTransferMap->getBytesProcessed() &&
                m_FastSync->SetFastSyncFullSyncBytes(m_checksumTransferMap->getBytesProcessed()))
            {
                DebugPrintf(SV_LOG_DEBUG, "Updating FullSync Bytes Information after %u seconds:"
                    "\nFullSync Sent Bytes:" ULLSPEC"\n",
                    intervalTime,
                    m_checksumTransferMap->getBytesProcessed());
                m_prevFullSyncBytesSent = m_checksumTransferMap->getBytesProcessed();
            }
            intervalTime = 0;
        }

        if (canSendHCDs() == true && pendingChunkCount() == 0)
        {
            size_t index1 = m_checksumTransferMap->getFirstUnSetBit();
            if (index1 != boost::dynamic_bitset<>::npos)
            {
                DebugPrintf(SV_LOG_DEBUG, "There are some chunks for which the hcds needs to be sent\n");
                m_checksumTransferMap->dumpBitMapInfo();

                size_t index2 = m_checksumTransferMap->getNextSetBit(index1);
                SV_UINT chunksToBeProcessed = 0;
                if (index2 == boost::dynamic_bitset<>::npos) //reached end of the bitmap
                {
                    chunksToBeProcessed = TotalNoOfChunks - index1;
                }
                else
                {
                    chunksToBeProcessed = index2 - index1;
                }

                SV_UINT numMaxHcds = m_FastSync->GetMaxHcdsAllowdAtCx();
                std::string remoteDir = m_FastSync->RemoteSyncDir();
                std::string prefixName = remoteDir + HcdPrefix;
                Files_t files;
                if (m_FastSync->GetFileList(m_cxTransport, prefixName, files))
                {
                    size_t numPresent = files.size();

                    SV_LONGLONG llNNeedToSend = chunksToBeProcessed;
                    if ((numPresent + chunksToBeProcessed) > numMaxHcds)
                    {
                        SV_LONGLONG llNExisting = numPresent;
                        SV_LONGLONG llLimit = numMaxHcds;
                        llNNeedToSend = llLimit - llNExisting;
                    }

                    if (llNNeedToSend > 0)
                    {
                        chunksToBeProcessed = llNNeedToSend;
                        if (m_genTasks.size() == 0)
                        {
                            for (int index = 0; index < taskCount; index++)
                            {
                                GenerateHCDWorkerPtr genTask(new GenerateHCDWorker(m_FastSync, thr_mgr(), chunkQueue, m_pendingChunkCount));
                                genTask->open();
                                m_genTasks.push_back(genTask);
                            }
                        }
                        setPendingChunkCount(chunksToBeProcessed);
                        FileInfosPtr fileInfos;
                        m_FastSync->assignFileListToThreads(chunkQueue, fileInfos, taskCount, chunksToBeProcessed, index1);
                    }
                    else
                    {
                        std::stringstream msg;
                        msg << "number of hcds to send = " << chunksToBeProcessed << ','
                            << "number of hcds already present at CX = " << numPresent << ','
                            << "limit on number of hcds present = " << numMaxHcds << '\n';
                        DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
                        SV_UINT nsecstowaitforhcdsend = m_FastSync->GetSecsToWaitForHcdSend();
                        DebugPrintf(SV_LOG_DEBUG, "hence waiting for %u seconds\n", nsecstowaitforhcdsend);
                        m_FastSync->Idle(nsecstowaitforhcdsend);
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_WARNING, "Failed to get the file list for hcd file spec %s. Retry After some time.\n", prefixName.c_str());
                }
            }
        }
    }

    if (m_checksumTransferMap->syncToPersistentStore(m_cxTransport) == SVS_OK)
    {
        m_FastSync->SetFastSyncFullSyncBytes(m_checksumTransferMap->getBytesProcessed());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}


int FastSync::GenerateHCDWorker::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

int FastSync::GenerateHCDWorker::close(SV_ULONG flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    return 0;
}

int FastSync::GenerateHCDWorker::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s GenerateHCDWorker - hcd generator worker thread START\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    try
    {
        while (!m_FastSync->QuitRequested(TaskWaitTimeInSeconds))
        {
            ACE_Message_Block * mb = m_FastSync->retrieveMessage(m_chunkQueue);
            if (mb != NULL)
            {
                FastSyncMsg_t * msg = (FastSyncMsg_t*)mb->base();
                GenerateHashCompareData(msg);
                delete msg;
                mb->release();
            }
            else
            {
                m_cxTransport->heartbeat();
            }
        }
        if (!m_FastSync->Settings().isAzureDataPlane())
        {
            m_pDiskRead->commit();
            m_pHcdCalc->commit();
        }
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_FastSync->Stop();
    }
    catch (std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_FastSync->Stop();
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "FastSync::GenerateHCDWorker::svc caught an unnknown exception\n");
        m_FastSync->Stop();
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s:%s GenerateHCDWorker - hcd generator worker thread END\n", FUNCTION_NAME, m_FastSync->JobId().c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}

void FastSync::GenerateHCDWorker::GenerateHashCompareData(FastSyncMsg_t* msg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    size_t chunkIndex = msg->index;
    while (!m_FastSync->QuitRequested(0) && chunkIndex < msg->maxIndex)
    {
        SV_LONGLONG offset = chunkIndex * (SV_ULONGLONG)m_FastSync->m_MaxSyncChunkSize;
        SV_LONGLONG tempOffset = offset;
        if (m_FastSync->Settings().isAzureDataPlane()){
            m_FastSync->GenerateHashCompareDataFromAzure(&offset, 1, m_cxTransport, m_pHcdFileUpload, m_pGetHcdAzureAgent);
        }
        else {
            m_FastSync->GenerateHashCompareData(&offset, 1, m_volume, m_cxTransport, m_pHcdFileUpload, m_pDiskRead, m_pHcdCalc);
        }

        DebugPrintf(SV_LOG_DEBUG, "ChunkIndex: %d, Offset to be generated: " ULLSPEC ",Offset After CS generated: " ULLSPEC " The difference: " ULLSPEC "\n", chunkIndex, tempOffset, offset, offset - tempOffset);
        --m_pendingChunkCount;
        chunkIndex += msg->skip;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}



// generate hashes for the volume starting at offset. it restricts the number
// HashCompareDatas genreated to try to limit the number sent to the server
// This routine was added as part of TFS 2166919 to fetch hash from 
// Azure instead of locally attached volume
// --------------------------------------------------------------------------
int FastSync::GenerateHashCompareDataFromAzure(SV_LONGLONG* offset, int processLimit, CxTransport::ptr cxTransport, Profiler &pHcdFileUpload, Profiler &pGetHcdAzureAgent)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::ostringstream msg;

    unsigned long chunkRemainder = m_MaxSyncChunkSize % m_HashCompareDataSize;
    if (chunkRemainder)
    {
        msg << "MaxChunkSize: "
            << m_MaxSyncChunkSize
            << " is not exact multiple of "
            << "HashCompareDataSize: "
            << m_HashCompareDataSize
            << '\n';
        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
    }

    // TODO: is there any constant that we can use instead of hard coding?
    unsigned int digestSize = (INM_MD5TEXTSIGLEN / 2);

    // chunkSize represents the max Sync Chunk size (64mb) for all but the last chunk
    // in case of last chunk, it will be till we reached eof disk
    size_t chunkSize = m_MaxSyncChunkSize;

    // chunkRemaining is to keep track of when we are last block within chunk
    // in case of last block, last chunk the remaining read length would be less
    // than the checksum computation block size  (1mb)
    size_t chunkRemaining = m_MaxSyncChunkSize;

    // we will compute checksum for m_HashCompareDataSize (1mb) , but for last block, last chunk
    // it can be less
    size_t readLength = m_HashCompareDataSize;

    int hashCount = 0;
	size_t digestLen = 0;
    
    int generated = 0;
    while ((NO_PROCESS_LIMIT == processLimit || generated < processLimit) && !QuitRequested())
    {
        if (m_VolumeSize == *offset)
        {
            break;
        }

        // if the chunk already processed skipping it..
        if (isHcdTransferred(*offset))
        {
            DebugPrintf(SV_LOG_DEBUG, "The hcd file sent for this chunk %d already\n", (*offset) / m_MaxSyncChunkSize);
            if (m_VolumeSize - *offset <= m_MaxSyncChunkSize)
            {
                DebugPrintf(SV_LOG_DEBUG, "The %d is last chunk\n", (*offset) / m_MaxSyncChunkSize);
                break;
            }
            else
            {
                *offset = *offset + m_MaxSyncChunkSize;
                DebugPrintf(SV_LOG_DEBUG, "The hcd file sent for this chunk %d starting at offset" ULLSPEC "\n",
                    (*offset) / m_MaxSyncChunkSize, *offset);
                continue;
            }
        }


        if (m_VolumeSize - *offset <= m_MaxSyncChunkSize) {
            chunkSize = (m_VolumeSize - *offset);
            chunkRemaining = (m_VolumeSize - *offset);
        }
        else {
            chunkSize = m_MaxSyncChunkSize;
            chunkRemaining = m_MaxSyncChunkSize;
        }

		hashCount = chunkRemaining / m_HashCompareDataSize;
		if (chunkRemaining % m_HashCompareDataSize) {
			hashCount += 1;
		}

		boost::shared_ptr<HashCompareData> hashCompareData(new HashCompareData(hashCount,
			m_MaxSyncChunkSize,
			m_HashCompareDataSize,
			HashCompareData::MD5,
			GetEndianTagSize()));

		//  GetChecksums will fill up the checksum containing (hashCount * digestSize) bytes
		std::vector<unsigned char> digests;
		digestLen = (hashCount * digestSize);

		try
		{
            pGetHcdAzureAgent->start();
			m_azureagent_ptr->GetChecksums(*offset, chunkRemaining, digests, digestLen);
            pGetHcdAzureAgent->stop(chunkRemaining);
		}
		catch (const AzureCancelOpException & ce) {
            // TODO: nichougu - either use a dummy base class for exception that wraps logs or, a macro.

            std::stringstream ssmsg;
            ssmsg << FUNCTION_NAME << ": Azure agent failed in GetChecksums. Error: " << ce.what();
            HandleAzureCancelOpException(ssmsg.str());
			throw DataProtectionException(ce.what(), SV_LOG_ERROR);
		}
		catch (const AzureInvalidOpException & ie) {

            std::stringstream ssmsg;
            ssmsg << FUNCTION_NAME << ": Azure agent failed in GetChecksums. Error: " << ie.what();
            HandleAzureInvalidOpException(ssmsg.str(), ie.ErrorCode());
			throw DataProtectionException(ie.what(), SV_LOG_ERROR);
		}
		catch (ErrorException & ee)
		{
            std::stringstream ssmsg;
            ssmsg << FUNCTION_NAME << ": Azure agent failed in GetChecksums. Error: " << ee.what();
			throw DataProtectionException(ssmsg.str());
		}
		
        for (int i = 0; i < hashCount; ++i)
        {
            if (QuitRequested())
            {
                return 0;
            }

            if (chunkRemaining < m_HashCompareDataSize)	{
                readLength = chunkRemaining;
            }
            else {
                readLength = m_HashCompareDataSize;
            }

            if (!hashCompareData->UpdateHash((*offset), readLength, &digests[i*digestSize], GetConfigurator().getDICheck()))
            {
                msg << "FastSync::GenerateHashCompareData failed computing hash ran out of buffer space\n";
                throw DataProtectionException(msg.str());
            }

            (*offset) += readLength;
            chunkRemaining -= readLength;
        }


        if (0 == hashCompareData->Count())
        {
            return generated; // nothing to send
        }

        SendHashCompareData(hashCompareData, (*offset), chunkSize, cxTransport, pHcdFileUpload);
        m_genHCDTask->checksumTransferMap()->markAsTransferred(*offset - chunkSize, chunkSize);
        ++generated;
    }
    pGetHcdAzureAgent->commit();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return generated;
}


// --------------------------------------------------------------------------
// generate hashes for the volume starting at offset. it restricts the number
// HashCompareDatas genreated to try to limit the number sent to the server
// --------------------------------------------------------------------------
int FastSync::GenerateHashCompareData(SV_LONGLONG* offset, int processLimit, cdp_volume_t::Ptr volume, CxTransport::ptr cxTransport, Profiler &pHcdFileUpload, Profiler &pDiskRead, Profiler &pChecksumCalc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int hashCount;
    INM_SAFE_ARITHMETIC(hashCount = InmSafeInt<unsigned long>::Type(m_MaxSyncChunkSize) / m_HashCompareDataSize, INMAGE_EX(m_MaxSyncChunkSize)(m_HashCompareDataSize))

        SV_ULONG count;

    SV_ULONGLONG bytesRead = 0;

    std::ostringstream msg;

    unsigned long chunkRemainder = m_MaxSyncChunkSize % m_HashCompareDataSize;
    if (chunkRemainder)
    {
        msg << "MaxChunkSize: "
            << m_MaxSyncChunkSize
            << " is not exact multiple of "
            << "HashCompareDataSize: "
            << m_HashCompareDataSize
            << '\n';
        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
    }

    //std::vector<char> buffer(m_HashCompareDataSize);

#ifdef SV_WINDOWS
    boost::shared_array<char> pbuffer(new (nothrow) char[m_HashCompareDataSize]);
#else
    boost::shared_array<char> pbuffer((char *)valloc(m_HashCompareDataSize), free);
#endif

    if (!pbuffer)
    {
        msg << "Error detected  " << "in Function:" << FUNCTION_NAME
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Error during memory allocation.\n"
            << " Error Message: " << Error::Msg() << "\n";

        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
    }

    volume->Seek((*offset), BasicIo::BioBeg);

    if (volume->Eof()) {
        return 0;
    }

    if (!volume->Good())
    {
        msg << "FastSync::GenerateHashCompareData open failed: " << volume->LastError() << '\n';
#ifdef SV_WINDOWS
        //In windows ERROR_NOT_READY is returned if volume is offline or unmounted
        // CORRECT ME: Should this be treated as warning??
        throw DataProtectionException(msg.str(), (ERROR_NOT_READY == volume->LastError() ? SV_LOG_WARNING : SV_LOG_ERROR));
#else
        throw DataProtectionException(msg.str(), SV_LOG_ERROR);
#endif

    }

    /** Fast Sync TBC - BSR **/
    // int jobId;


    int generated = 0; // TODO: if we run this in a separate thread no need to limit the number generated can just keep going
    while ((NO_PROCESS_LIMIT == processLimit || generated < processLimit) && !QuitRequested())
    {
        // TODO: for now always ask the cx which offset to get
        // this is needed because the cx does not report failures of update status so this will make sure
        // fast sync doesn't move on to the next chunk if an update failed.
        bytesRead = 0;
        if (m_VolumeSize == *offset)
        {
            break;
        }

        if (volume->Tell() != *offset) {
            // need to adjust to new offset because either the update status failed
            // or resync was restarted
            volume->Seek(*offset, BasicIo::BioBeg);
        }
        /*
         *  Fixing overshoot problem by bitmaps..
         *  If the chunk already processed skipping it..
         *  By Suresh
         */
        if (isHcdTransferred(*offset))
        {
            DebugPrintf(SV_LOG_DEBUG, "The hcd file sent for this chunk %d already\n", (*offset) / m_MaxSyncChunkSize);
            if (m_VolumeSize - *offset <= m_MaxSyncChunkSize)
            {
                DebugPrintf(SV_LOG_DEBUG, "The %d is last chunk\n", (*offset) / m_MaxSyncChunkSize);
                break;
            }
            else
            {
                *offset = *offset + m_MaxSyncChunkSize;
                DebugPrintf(SV_LOG_DEBUG, "The hcd file sent for this chunk %d starting at offset" ULLSPEC "\n",
                    (*offset) / m_MaxSyncChunkSize, *offset);
                continue;
            }
        }
        /* End of Change*/
        boost::shared_ptr<HashCompareData> hashCompareData(new HashCompareData(hashCount,
            m_MaxSyncChunkSize,
            m_HashCompareDataSize,
            HashCompareData::MD5,
            GetEndianTagSize()));
        for (int i = 0; i < hashCount; ++i)
        {
            if (QuitRequested())
            {
                return 0;
            }

            SV_ULONGLONG pendingRead = m_VolumeSize - *offset;
            SV_ULONG readsize =
                ((pendingRead < m_HashCompareDataSize) ? (SV_ULONG)pendingRead : m_HashCompareDataSize);

            pDiskRead->start();
            count = volume->FullRead(pbuffer.get(), readsize);
            if (!volume->Good() && !volume->Eof()) {
                msg << "FastSync::GenerateHashCompareData FullRead failed: " << volume->LastError() << '\n';

#ifdef SV_WINDOWS
                throw DataProtectionException(msg.str(),
                    (ERROR_NOT_READY == volume->LastError() ? SV_LOG_WARNING : SV_LOG_ERROR));
#else
                throw DataProtectionException(msg.str(), SV_LOG_ERROR);
#endif
            }
            pDiskRead->stop(readsize);

            if (0 == count)
            {
                // nothing read must have been at EOF
                msg << "Bytes read is zero at offset " << *offset
                    << " for volume " << GetDeviceName()
                    << " . Requested length " << readsize
                    << ", when generating hcd.";
                DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
                break;
            }

            pChecksumCalc->start();
            if (!hashCompareData->ComputeHash((*offset), count, pbuffer.get(), GetConfigurator().getDICheck()))
            {
                msg << "FastSync::GenerateHashCompareData failed computing hash ran out of buffer space\n";
                throw DataProtectionException(msg.str());
            }
            pChecksumCalc->stop(count);

            (*offset) += count;
            bytesRead += count;

            if (m_VolumeSize == *offset)
            {
                break;
            }

            //This would not be the case in Fast Sync TBC...
            if (volume->Eof())
            {
                // nothing more to read
                break;
            }
        }

        if ((*offset) > (SV_LONGLONG)m_VolumeSize)
        {
            DebugPrintf(SV_LOG_INFO, "INFO: fast sync - file system reported volume size " LLSPEC " < read volume size " LLSPEC "\n", m_VolumeSize, (*offset));
        }

        if (0 == hashCompareData->Count())
        {
            return generated; // nothing to send
        }

        SendHashCompareData(hashCompareData, (*offset), bytesRead, cxTransport, pHcdFileUpload);

        m_genHCDTask->checksumTransferMap()->markAsTransferred(*offset - bytesRead, bytesRead);
        ++generated;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return generated;
}


bool FastSync::isHcdTransferred(SV_ULONGLONG offset)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_genHCDTask->checksumTransferMap()->dumpBitMapInfo();
    bool bReturnValue = m_genHCDTask->checksumTransferMap()->isBitSet(offset / m_MaxSyncChunkSize);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bReturnValue;
}


bool FastSync::startHcdTaskAndWaitInFsResync(SharedBitMapPtr checksumTransferMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    m_procClusterStage.reset(new ProcessClusterStage(this, checksumTransferMap));
    GenerateHcdStage hcs(this);
    SendHcdStage shs(this, m_procClusterStage.get());

    boost::shared_ptr<InmPipeline> processClusterPipe;
    boost::shared_ptr<ProcessClusterTask> processClusterTask;
    if (IsProcessClusterPipeEnabled())
    {
        InmPipeline::Stages_t s;
        s.push_back(m_procClusterStage.get());
        s.push_back(&hcs);
        s.push_back(&shs);
        processClusterPipe.reset(new InmPipelineImp());
        if (!processClusterPipe->Create(s, InmPipeline::MULTIRINGS))
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED FastSync::SyncStart to start process cluster pipeline\n");
            return false;
        }
    }
    else
    {
        processClusterTask.reset(new ProcessClusterTask(this, &m_ThreadManager, m_procClusterStage.get(), &hcs, &shs));
        if (-1 == processClusterTask->open())
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED FastSync::SyncStart to start process cluster task\n");
            return false;
        }
    }

    ProcessSyncDataTask processSyncDataTask(this, &m_ThreadManager);
    if (-1 == processSyncDataTask.open())
    {
        DebugPrintf(SV_LOG_ERROR,
            "FAILED FastSync::SyncStart processSyncDataTask.open: %d\n",
            ACE_OS::last_error());
        PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }

    /* TODO: write a generic wait function that takes
     *       vector of tasks and vector of pipelines
     *       for which to wait for */
    if (IsProcessClusterPipeEnabled())
    {
        waitForSyncTasks(processSyncDataTask, processClusterPipe.get());
    }
    else
    {
        WaitForSyncTasks(processSyncDataTask, *processClusterTask.get());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}


bool FastSync::startHcdTaskAndWait(SharedBitMapPtr checksumTransferMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    m_genHCDTask.reset(new GenerateHcdTask(this, checksumTransferMap, &m_ThreadManager));
    if (-1 == m_genHCDTask->open())
    {
        DebugPrintf(SV_LOG_ERROR,
            "FAILED FastSync::SyncStart generateHcdTask.open: %d\n",
            ACE_OS::last_error());
        return false;
    }

    ProcessSyncDataTask processSyncDataTask(this, &m_ThreadManager);
    if (-1 == processSyncDataTask.open())
    {
        DebugPrintf(SV_LOG_ERROR,
            "FAILED FastSync::SyncStart processSyncDataTask.open: %d\n",
            ACE_OS::last_error());
        PostMsg(SYNC_EXCEPTION, SYNC_HIGH_PRIORITY);
    }

    WaitForSyncTasks(*m_genHCDTask.get(), processSyncDataTask);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}


bool FastSync::ShouldDoubleBitmap(const std::string &filename) const
{
    return (filename != getHcdTransferMapFileName());
}


bool FastSync::ShouldDoubleBitmapInFsResync(const std::string &filename) const
{
    return true;
}


void FastSync::downloadAndProcessMissingHcdsPacket(CxTransport::ptr cxTransport)
{
    return (this->*m_downloadAndProcessMissingHcd[IsFsAwareResync()])(cxTransport);
}

void FastSync::CheckResyncHealth()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    try
    {
        if (!m_IsMobilityAgent)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s called in target mode, this function is supposed to be called only on source so ignoring.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        
        if (!m_healthCollatorPtr)
        {
            throw INMAGE_EX("HealthCollator instance not initialized.")(GetDeviceName());
        }

        // Reusing ResyncUpdateInterval as being 5 min default.
        static boost::chrono::steady_clock::time_point s_prevCheckTime = boost::chrono::steady_clock::time_point::min();
        const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();

        if (currentTime <= (s_prevCheckTime + boost::chrono::seconds(m_ResyncUpdateInterval)))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as check interval is yet to reach.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        DebugPrintf(SV_LOG_DEBUG, "%s: Checking resync health.\n", FUNCTION_NAME); // TODO: test make debug

        // numberOfBytesSynced = ( fully matched bytes from checksum processor +
        //                          applied bytes from sync applier             +
        //                          partial matched bytes from sync applier )
        // At completion of resync numberOfBytesSynced = disk size
        uint64_t fullyMatchedBytesFromProcessHcdTask = 0;
        uint64_t appliedBytesFromProcessSyncDataTask = 0;
        uint64_t partialMatchedBytesFromProcessSyncDataTask = 0;

        ResyncProgressCounters resyncProgressCounters;
        std::string errmsg;
        if (!m_ResyncProgressInfo->GetResyncProgressCounters(resyncProgressCounters, errmsg))
        {
            // If init failed for checksumProcessedMap or syncDataAppliedMap then this can be datapath issue.
            // Proceed as this is accounted as no progress since last check.
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errmsg.c_str());
        }
        else
        {
            fullyMatchedBytesFromProcessHcdTask = resyncProgressCounters.m_ChecksumProcessedMap->getBytesProcessed();
            appliedBytesFromProcessSyncDataTask = resyncProgressCounters.m_SyncDataAppliedMap->getBytesProcessed();
            partialMatchedBytesFromProcessSyncDataTask = resyncProgressCounters.m_SyncDataAppliedMap->getBytesNotProcessed();
        }

        static uint64_t s_prevResyncProcessedBytes = fullyMatchedBytesFromProcessHcdTask +
            appliedBytesFromProcessSyncDataTask +
            partialMatchedBytesFromProcessSyncDataTask;
        static boost::chrono::steady_clock::time_point s_prevResyncProcessedBytesCheckTime = boost::chrono::steady_clock::now();

        const uint64_t currResyncProcessedBytes = fullyMatchedBytesFromProcessHcdTask +
            appliedBytesFromProcessSyncDataTask +
            partialMatchedBytesFromProcessSyncDataTask;

        if (s_prevResyncProcessedBytes == currResyncProcessedBytes)
        {
            std::string healthIssueCode;
            if (currentTime >= (s_prevResyncProcessedBytesCheckTime + boost::chrono::seconds(m_ResyncNoProgressCheckInterval)))
            {
                // Reset slow resync health issue if set earlier
                std::vector<std::string> vResetHICodes;
                vResetHICodes.push_back(Settings().isAzureDataPlane() ?
                    AgentHealthIssueCodes::DiskLevelHealthIssues::SlowResyncProgressOnPremToAzure::HealthCode :
                    AgentHealthIssueCodes::DiskLevelHealthIssues::SlowResyncProgressAzureToOnPrem::HealthCode);
                if (!ResetHealthIssues(vResetHICodes))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to ResetHealthIssues, retrying in next attempt.\n", FUNCTION_NAME);
                    // Slow resync and No resync are mutual exclusive so retry in next attempt.
                    return;
                }

                healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::NoResyncProgress::HealthCode;
                DebugPrintf(SV_LOG_ALWAYS, "%s: No resync identified - resync is not progressing for more than %d sec, raising health issue %s.\n",
                    FUNCTION_NAME, m_ResyncNoProgressCheckInterval, healthIssueCode.c_str());
            }
            else if (currentTime >= (s_prevResyncProcessedBytesCheckTime + boost::chrono::seconds(m_ResyncSlowProgressCheckInterval)))
            {
                // If no resync HI is already present then skip slow resync HI - service/VM restart case.
                if (!m_healthCollatorPtr->IsDiskHealthIssuePresent(AgentHealthIssueCodes::DiskLevelHealthIssues::NoResyncProgress::HealthCode,
                    GetDeviceName()))
                {
                    healthIssueCode = Settings().isAzureDataPlane() ?
                        AgentHealthIssueCodes::DiskLevelHealthIssues::SlowResyncProgressOnPremToAzure::HealthCode :
                        AgentHealthIssueCodes::DiskLevelHealthIssues::SlowResyncProgressAzureToOnPrem::HealthCode;
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Slow resync identified, raising health issue %s.\n", FUNCTION_NAME, healthIssueCode.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: No resync health issue is already set, skipping slow resync health issue.\n", FUNCTION_NAME);
                }
            }

            if (!healthIssueCode.empty())
            {
                std::map<std::string, std::string> msgParams;
                msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::ObservationTime,
                    m_healthCollatorPtr->GetCurrentObservationTime()));
                AgentDiskLevelHealthIssue diskHI(GetDeviceName(), healthIssueCode, msgParams);
                if (!m_healthCollatorPtr->SetDiskHealthIssue(diskHI))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to raise the health issue %s.\n", FUNCTION_NAME, healthIssueCode.c_str());
                }
            }
        }
        else
        {
            s_prevResyncProcessedBytes = currResyncProcessedBytes;
            s_prevResyncProcessedBytesCheckTime = currentTime;

            // Reset health issue if set earlier
            std::vector<std::string> vResetHICodes;
            vResetHICodes.push_back(AgentHealthIssueCodes::DiskLevelHealthIssues::SlowResyncProgressOnPremToAzure::HealthCode);
            vResetHICodes.push_back(AgentHealthIssueCodes::DiskLevelHealthIssues::SlowResyncProgressAzureToOnPrem::HealthCode);
            vResetHICodes.push_back(AgentHealthIssueCodes::DiskLevelHealthIssues::NoResyncProgress::HealthCode);
            if (!ResetHealthIssues(vResetHICodes))
            {
                // Retry in next attempt.
                DebugPrintf(SV_LOG_ERROR, "%s: failed to reset resync progress health issues, retrying in next attempt.\n", FUNCTION_NAME);
                return;
            }
        }
        s_prevCheckTime = currentTime;
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

bool FastSync::ResetHealthIssues(const std::vector<std::string> &vResetHICodes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool retval = false;

    if (!m_healthCollatorPtr)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed: HealthCollator instance not initialized.", FUNCTION_NAME);
    }
    else if (m_healthCollatorPtr->IsThereAnyDiskLevelHealthIssue())
    {
        retval = m_healthCollatorPtr->ResetDiskHealthIssues(vResetHICodes, GetDeviceName());
    }
    else
    {
        retval = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

void FastSync::updateProfilingSummary(const bool resyncCompleted) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    try {
        std::vector<PrBuckets> IRSmry;
        ProfilingSummaryFactory::summaryAsJson(IRSmry);
        if (!IRSmry.empty())
        {
            IRSummary jirs;
            jirs.Job = Settings().jobId;
            jirs.Dev = GetDeviceName();
            jirs.DevSize_B = boost::lexical_cast<std::string>(m_VolumeSize);
            jirs.IsSrc = boost::lexical_cast<std::string>(Source());
            jirs.Status = resyncCompleted ? RESYNC_STATUS_COMPLETED : RESYNC_STATUS_IN_PROGRESS;
            jirs.IRSmry = IRSmry;

            std::string jsmry = JSON::producer<IRSummary>::convert(jirs);
            jsmry.erase(std::remove(jsmry.begin(), jsmry.end(), '\n'), jsmry.end());

            DebugPrintf(SV_LOG_ALWAYS, "%s\n", jsmry.c_str());
        }
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

void FastSync::UpdateResyncStats(const bool forceUpdate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    try
    {
        if (Source())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s called in source mode - ignoring.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
            return;
        }

        static boost::chrono::steady_clock::time_point s_prevUpdateTime = boost::chrono::steady_clock::time_point::min();
        const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();

        if (!forceUpdate && (currentTime <= (s_prevUpdateTime + boost::chrono::seconds(RESYNC_STATS_UPDATE_INTERVAL_IN_SEC))))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as update interval is yet to reach.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        DebugPrintf(SV_LOG_DEBUG, "%s: Updating stats.\n", FUNCTION_NAME);

        ResyncProgressCounters resyncProgressCounters;
        std::string errmsg;
        if (!m_ResyncProgressInfo->GetResyncProgressCounters(resyncProgressCounters, errmsg))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errmsg.c_str());
            return; // throw or return both has implication incorrect ResyncLast15MinutesTransferredBytes for next 10 mins.
        }

        ResyncStats resyncStats;

        const uint64_t &numberOfBytesSynced = resyncStats.m_ResyncProcessedBytes = resyncProgressCounters.GetResyncProcessedBytes();

        resyncStats.m_ResyncTotalTransferredBytes = resyncProgressCounters.GetAppliedBytesFromProcessSyncDataTask();

        resyncStats.m_MatchedBytesRatio = numberOfBytesSynced ? ((resyncProgressCounters.GetFullyMatchedBytesFromProcessHcdTask() +
                resyncProgressCounters.GetPartialMatchedBytesFromProcessSyncDataTask()) * 100 / numberOfBytesSynced) : 0 ;

        std::vector<boost::shared_ptr<ProfilingBuckets> > vProfilingBuckets;
        uint64_t currentTotalTransferredBytes = 0;
        const std::string operationName = Settings().isAzureDataPlane() ? SYNCUPLOADTOAZURE : SYNCDOWNLOAD;
        ProfilingSummaryFactory::summaryAsData(operationName, vProfilingBuckets);
        if (!vProfilingBuckets.empty())
        {
            const boost::shared_ptr<ProfilingBuckets> profilingBuckets = *(vProfilingBuckets.begin());
            if (currentTotalTransferredBytes = profilingBuckets->GetTotalSize())
            {
                const boost::posix_time::ptime lastUpdateTS(profilingBuckets->GetLastUpdateTS());
                if (!lastUpdateTS.is_not_a_date_time())
                {
                    resyncStats.m_ResyncLastDataTransferTimeUtcPtimeIsoStr = boost::posix_time::to_iso_string(lastUpdateTS);
                }
            }
        }

        static uint64_t s_lastTotalTransferredBytes = 0;

        const uint64_t currentCallTransferredBytesDelta = (currentTotalTransferredBytes > s_lastTotalTransferredBytes) ?
            (currentTotalTransferredBytes - s_lastTotalTransferredBytes) : 0;
        static uint64_t s_lastCallTransferredBytesDelta = 0;
        static uint64_t s_lastToLastCallTransferredBytesDelta = 0;

        resyncStats.m_ResyncLast15MinutesTransferredBytes = s_lastTotalTransferredBytes =
            currentCallTransferredBytesDelta +
            s_lastCallTransferredBytesDelta +
            s_lastToLastCallTransferredBytesDelta;

        s_lastToLastCallTransferredBytesDelta = s_lastCallTransferredBytesDelta;
        s_lastCallTransferredBytesDelta = currentCallTransferredBytesDelta;

        {
            boost::mutex::scoped_lock guard(m_ResyncStatsLock);
            m_ResyncStats = resyncStats;
        }
        s_prevUpdateTime = currentTime;
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

const ResyncStats FastSync::GetResyncStats() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::mutex::scoped_lock guard(m_ResyncStatsLock);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_ResyncStats;
}

const std::string FastSync::GetResyncStatsForCSLegacyAsJsonString()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string strResyncStatsForCSLegacyJson;

    try
    {
        const ResyncStats resyncStats = GetResyncStats();
        ResyncStatsForCSLegacy resyncStatsForCSLegacy;
        resyncStatsForCSLegacy.ResyncLast15MinutesTransferredBytes = resyncStats.m_ResyncLast15MinutesTransferredBytes;
        
        if (!resyncStats.m_ResyncLastDataTransferTimeUtcPtimeIsoStr.empty())
        {
            const boost::posix_time::ptime resyncLastDataTransferTimeUtcPtime =
                boost::posix_time::from_iso_string(resyncStats.m_ResyncLastDataTransferTimeUtcPtimeIsoStr);
            if (!resyncLastDataTransferTimeUtcPtime.is_not_a_date_time())
            {
                resyncStatsForCSLegacy.ResyncLastDataTransferTimeUTC = boost::lexical_cast<std::string>(
                    boost::posix_time::time_duration(resyncLastDataTransferTimeUtcPtime -
                        boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds());
            }
        }

        if (!resyncStats.m_ResyncProcessedBytes)
        {
            resyncStatsForCSLegacy.ResyncPercentage = "0";
        }
        else
        {
            if (0 == m_VolumeSize)
            {
                std::stringstream msg;
                msg << FUNCTION_NAME << ": VolumeSize identified as zero, sourceCapacity in settings=" << Settings().sourceCapacity;
                throw DataProtectionException(msg.str());
            }
            const float resyncPercentage = resyncStats.m_ResyncProcessedBytes * (float)(100) / m_VolumeSize;
            std::vector<char> vResyncPercentage(10, '\0');
            inm_sprintf_s(&vResyncPercentage[0], vResyncPercentage.size(), "%.2f", resyncPercentage);
            resyncStatsForCSLegacy.ResyncPercentage = std::string(&vResyncPercentage[0]);
        }
        
        resyncStatsForCSLegacy.ResyncProcessedBytes = resyncStats.m_ResyncProcessedBytes;

        // Convert in sec as srcResyncStarttime is TimeInHundNanoSeconds
        const int IntervalsPerSecond = 10000000;
        if (Settings().srcResyncStarttime)
        {
            const SV_ULONGLONG srcResyncStartTimeInSec = Settings().srcResyncStarttime / IntervalsPerSecond;
            resyncStatsForCSLegacy.ResyncStartTimeUTC = boost::lexical_cast<std::string>(
                (Settings().endpoints.begin()->sourceOSVal == SV_WIN_OS) ? (srcResyncStartTimeInSec - GetSecsBetweenEpoch1970AndEpoch1601()) :
                srcResyncStartTimeInSec);
        }
        
        resyncStatsForCSLegacy.ResyncTotalTransferredBytes = resyncStats.m_ResyncTotalTransferredBytes;

        strResyncStatsForCSLegacyJson = JSON::producer<ResyncStatsForCSLegacy>::convert(resyncStatsForCSLegacy);
        strResyncStatsForCSLegacyJson.erase(std::remove(strResyncStatsForCSLegacyJson.begin(), strResyncStatsForCSLegacyJson.end(), '\n'),
            strResyncStatsForCSLegacyJson.end());

        DebugPrintf(SV_LOG_ALWAYS, "%s: strResyncStatsForCSLegacyJson=%s.\n", FUNCTION_NAME, strResyncStatsForCSLegacyJson.c_str());
    }
    catch (const JSON::json_exception& json)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to serialize resyncStats with exception: %s.\n", FUNCTION_NAME, json.what());
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return strResyncStatsForCSLegacyJson;
}
