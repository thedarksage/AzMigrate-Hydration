#include "inmageex.h"
#include "Monitoring.h"
#include "fastsyncRcm.h"
#include "inmsafecapis.h"
#include "SyncSettings.h"
#include "theconfigurator.h"
#include "RcmConfigurator.h"
#include "portablehelpers.h"
#include "inmerroralertdefs.h"
#include "volumereporterimp.h"
#include "resyncStateManager.h"
#include "inmerroralertnumbers.h"
#include "azurebadfileexception.h"
#include "azureinvalidopexception.h"
#include "azurecanceloperationexception.h"
#include "globalHealthCollator.h"

#include <boost/chrono.hpp>

int const HeartbeatIntervalInSec = 60;

bool FastSyncRcm::GetSyncDir()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!Settings().resyncDirectory.empty())
    {
        boost::filesystem::path resyncDirectory(Settings().resyncDirectory);
        resyncDirectory += boost::filesystem::path::preferred_separator;
        SetRemoteSyncDir(resyncDirectory.string());
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool FastSyncRcm::IsResyncStartTimeStampAlreadySent(ResyncTimeSettings &rts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bRet = false;
    ResyncStateInfo resyncStateInfo(m_ResyncStateManager->GetResyncState());
    if (resyncStateInfo.ResyncStartTimestamp)
    {
        rts.time = resyncStateInfo.ResyncStartTimestamp;
        rts.seqNo = resyncStateInfo.ResyncStartTimestampSeqNo;

        DebugPrintf(SV_LOG_DEBUG, "%s: Resync Start Timestamp for %s has already been issued as %llu %llu.\n", FUNCTION_NAME,
            Settings().deviceName.c_str(), resyncStateInfo.ResyncStartTimestamp, resyncStateInfo.ResyncStartTimestampSeqNo);
        
        bRet = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

// check resync state for start time
bool FastSyncRcm::ResyncStartNotifyIfRequired()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = true;

    ResyncTimeSettings rts;
    if (IsResyncStartTimeStampAlreadySent(rts))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Resync start already issued hence not doing resync start again.\n", FUNCTION_NAME);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Sending Resync start time to CX\n");
        if (!ResyncStartNotify()) // TODO: nichougu - add out error string param for failure case - same for rest similar functions
        {
            DebugPrintf(SV_LOG_ERROR, "%s: ResyncStartNotify failed.\n", FUNCTION_NAME);
            bRet = false;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

ClearDiffsState_t FastSyncRcm::CanClearDifferentials()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ClearDiffsState_t st = ClearDiffs;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return st;
}

bool FastSyncRcm::NotifyResyncStartToControlPath(const ResyncTimeSettings &rts, const bool canClearDiffs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = true;

    ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
    currentResyncStateInfo.ResyncStartTimestamp = rts.time;
    currentResyncStateInfo.ResyncStartTimestampSeqNo = rts.seqNo;
    currentResyncStateInfo.CurrentState = SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE;

    if (!m_ResyncStateManager->UpdateState(currentResyncStateInfo))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to update Resync Start Timestamp in remote resync state.\n", FUNCTION_NAME);
        bRet = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool FastSyncRcm::IsResyncEndTimeStampAlreadySent(ResyncTimeSettings &rts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bRet = false;
    ResyncStateInfo resyncStateInfo(m_ResyncStateManager->GetResyncState());
    if (resyncStateInfo.ResyncEndTimestamp)
    {
        rts.time = resyncStateInfo.ResyncEndTimestamp;
        rts.seqNo = resyncStateInfo.ResyncEndTimestampSeqNo;

        DebugPrintf(SV_LOG_DEBUG, "%s: Resync End Timestamp for %s has already been issued as %llu %llu.\n", FUNCTION_NAME,
            Settings().deviceName.c_str(), resyncStateInfo.ResyncStartTimestamp, resyncStateInfo.ResyncStartTimestampSeqNo);
        
        bRet = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool FastSyncRcm::NotifyResyncEndToControlPath(const ResyncTimeSettings &rts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

    bool bRet = true;

    ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
    currentResyncStateInfo.ResyncEndTimestamp = rts.time;
    currentResyncStateInfo.ResyncEndTimestampSeqNo = rts.seqNo;
    currentResyncStateInfo.CurrentState = SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE;

    if (!m_ResyncStateManager->UpdateState(currentResyncStateInfo))
    {
        bRet = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool FastSyncRcm::sendResyncEndNotifyAndResyncTransition()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    bool bRet = true;
    
    if (!ResyncEndNotify())
    {
        DebugPrintf(SV_LOG_ERROR, "ResyncEndNotify Failed\n");
        bRet = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool FastSyncRcm::SetFastSyncMatchedBytes(SV_ULONGLONG bytes, int during_resyncTransition)
{
    // A dummy override - Same in dataprotectionsync is used in V2A CS to report resync progress
    return true;
}

bool FastSyncRcm::SetFastSyncFullyUnusedBytes(SV_ULONGLONG bytesunused)
{
    // A dummy override - Same in dataprotectionsync is used in V2A CS to report resync progress
    return true;
}

void FastSyncRcm::ThrottleSpace()
{
    // A dummy override - Same in dataprotectionsync is used in V2A CS to pause resync if throttle is flagged in settings by CS.
    return;
}

//
// CheckResyncState calls MonitorPeerState of ResyncStateManager for default 1 min interval
//
void FastSyncRcm::CheckResyncState() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        static boost::chrono::steady_clock::time_point prevCheckTime = boost::chrono::steady_clock::time_point::min();
        static const int peerRemoteResyncStateMonitorInterval = GetConfigurator().getPeerRemoteResyncStateMonitorInterval();

        if (boost::chrono::steady_clock::now() >= (prevCheckTime + boost::chrono::seconds(peerRemoteResyncStateMonitorInterval)))
        {
            m_ResyncStateManager->MonitorPeerState();
            prevCheckTime = boost::chrono::steady_clock::now();
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

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Returns true if start timestamp is arived in remote source resync state file false otherwise
//
bool FastSyncRcm::IsStartTimeStampSetBySource() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool IsStartTimeStampArrived = false;
    const ResyncStateInfo sourceResyncStateInfo(m_ResyncStateManager->GetPeerResyncState());
    if (sourceResyncStateInfo.ResyncStartTimestamp)
    {
        IsStartTimeStampArrived = true;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return IsStartTimeStampArrived;
}

void FastSyncRcm::RetrieveCdpSettings()
{
    // A dummy override
    return;
}

bool FastSyncRcm::SetFastSyncFullSyncBytes(SV_LONGLONG fullSyncBytes)
{
    // A dummy override - Same in DataProtectionSync is used in V2A CS to report resync progress
    return true;
}

bool FastSyncRcm::SetFastSyncProgressBytes(const SV_LONGLONG &bytesApplied, const SV_ULONGLONG &bytesMatched, const std::string &resyncStatsJson)
{
    // A dummy override - Same in DataProtectionSync is used in V2A CS to report resync progress
    return true;
}

const std::string FastSyncRcm::GetResyncStatsForCSLegacyAsJsonString()
{
    // A dummy override - Same in FastSync is used in V2A CS to report resync progress
    return std::string();
}

bool FastSyncRcm::FailResync(const std::string resyncFailReason, const bool restartResync) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

    bool bRet = true;

    ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
    currentResyncStateInfo.CurrentState = SYNC_STATE_RESYNC_FAILED;
    currentResyncStateInfo.RestartResync = restartResync;
    currentResyncStateInfo.FailureReason = resyncFailReason;

    if (!m_ResyncStateManager->UpdateState(currentResyncStateInfo))
    {
        bRet = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

//
// Uploads sync file to Azure blob. Delets sync file on successful upload.
//
// In param fileToApply contains sync file path
//
// Throws if upload fail.
//
void FastSyncRcm::ApplyChanges(const std::string &fileToApply) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

#ifdef SV_WINDOWS
    const std::string fileToApplyLongPath(convertWstringToUtf8(getLongPathName(fileToApply.c_str())));
#else
    const std::string fileToApplyLongPath(getLongPathName(fileToApply.c_str()));
#endif

    try {

        if (!m_azureagent_ptr)
        {
            throw INMAGE_EX("Internal error: resync azure agent is not initialized")(Settings().deviceName);
        }

        DebugPrintf(SV_LOG_DEBUG, "%s: Sync file to apply=%s.\n", FUNCTION_NAME, fileToApplyLongPath.c_str());
        Profiler syncApplyProfiler(NEWPROFILER_WITHNORMANDTSBKT(SYNCUPLOADTOAZURE, ProfileVerbose(), ProfilingPath(), true));
        boost::system::error_code ec;
        SV_ULONGLONG fsize = boost::filesystem::file_size(fileToApplyLongPath, ec);
        syncApplyProfiler->start();
        m_azureagent_ptr->ApplyLog(fileToApplyLongPath);
        syncApplyProfiler->stop(fsize);
    }
    catch (const AzureCancelOpException & ce)
    {
        //      cancel the current resync and restart fresh session
        //      this is achieved by just sending resync required flag to RCM

        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " Azure agent failed in ApplyLog for " << fileToApply << " with AzureCancelOpException: " << ce.what();
        HandleAzureCancelOpException(ssmsg.str());
        throw DataProtectionException(ce.what(), SV_LOG_ERROR);
    }
    catch (const AzureInvalidOpException & ie)
    {
        // V2A CS stack action:
        //      pause the replication pair
        //      example, after a failover operation you cannot upload files, start resync etc operations

        // Fail resync in V2A RCM stack

        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " Azure agent failed in ApplyLog for " << fileToApply << " with AzureInvalidOpException: " << ie.what();
        HandleAzureInvalidOpException(ssmsg.str(), ie.ErrorCode());
        throw DataProtectionException(ie.what(), SV_LOG_ERROR);
    }
    catch (const AzureBadFileException & be)
    {
        // re-throw in case of bad resync file
        //
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " Azure agent failed in ApplyLog for " << fileToApply << " with AzureBadFileException - corrupt sync file: " << be.what();
        throw CorruptResyncFileException(ssmsg.str());
    }
    catch (const std::exception & e)
    {
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " Azure agent failed in ApplyLog for " << fileToApply << " with exception: " << e.what();
        throw DataProtectionException(ssmsg.str());
    }
    catch (...)
    {
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " Azure agent failed in ApplyLog for " << fileToApply << " with an unknown exception.";
        throw DataProtectionException(ssmsg.str());
    }

    // ApplyLog succeeded, delete sync file.
    if (-1 == ACE_OS::unlink(getLongPathName(fileToApply.c_str()).c_str()))
    {
        const int errcode = ACE_OS::last_error();
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << ": unlink failed for " << fileToApply << ". Error: " << errcode;
        DebugPrintf(SV_LOG_ERROR, "%s\n", ssmsg.str().c_str());
        throw DataProtectionException(ssmsg.str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::HandleAzureCancelOpException(const std::string & msg) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

    // Mark restart resync inline.
    DebugPrintf(SV_LOG_ERROR, "%s: Marking for inline restart resync, error=%s.\n", FUNCTION_NAME, msg.c_str());
    
    ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
    currentResyncStateInfo.CurrentState = SYNC_STATE_TARGET_RESTART_RESYNC_INLINE;
    currentResyncStateInfo.ErrorAlertNumber = E_AZURE_CANCEL_OPERATION_ERROR_ALERT;
    currentResyncStateInfo.FailureReason = msg;

    if (m_ResyncStateManager->UpdateState(currentResyncStateInfo))
    {
        // Pause and quit to avoid respawn as current resync session is failed.
        CDPUtil::QuitRequested(600);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::HandleAzureInvalidOpException(const std::string & msg, const long errorCode) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

    DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, msg.c_str());

    ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
    currentResyncStateInfo.CurrentState = SYNC_STATE_RESYNC_FAILED;
    currentResyncStateInfo.FailureReason = msg;
    currentResyncStateInfo.RestartResync = false;

    if (m_ResyncStateManager->UpdateState(currentResyncStateInfo))
    {
        // Pause and quit to avoid respawn as current resync session is failed.
        CDPUtil::QuitRequested(600);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::CompleteResyncToAzureAgent() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        m_azureagent_ptr->CompleteResync(true);
    }
    catch (const AzureCancelOpException & ce)
    {
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " failed: Azure agent failed in CompleteResync with exception: " << ce.what();
        HandleAzureCancelOpException(ssmsg.str());

        throw DataProtectionException(ce.what(), SV_LOG_ERROR);
    }
    catch (const AzureInvalidOpException & ie)
    {
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " failed: Azure agent failed in CompleteResync with exception: " << ie.what();
        HandleAzureInvalidOpException(ssmsg.str(), ie.ErrorCode());

        throw DataProtectionException(ie.what(), SV_LOG_ERROR);
    }
    catch (const std::exception & e)
    {
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " failed: Azure agent failed in CompleteResync with exception: " << e.what();
        throw DataProtectionException(ssmsg.str());
    }
    catch (...)
    {
        std::stringstream ssmsg;
        ssmsg << FUNCTION_NAME << " failed: Azure agent failed in CompleteResync with an unknown exception";
        throw DataProtectionException(ssmsg.str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::CompleteResyncToRcm() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

    const ResyncStateInfo targetResyncStateInfo(m_ResyncStateManager->GetPeerResyncState());
    if (targetResyncStateInfo.CurrentState & SYNC_STATE_TARGET_CONCLUDED_RESYNC)
    {
        // Update source resync state to SOURCE_COMPLETED_RESYNC_TO_RCM.
        ResyncStateInfo newResyncStateInfo(m_ResyncStateManager->GetResyncState());
        newResyncStateInfo.CurrentState = SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM;

        if (m_ResyncStateManager->UpdateState(newResyncStateInfo))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Source successfully completed resync to RCM.\n", FUNCTION_NAME);
            
            // Wait till agents settings fetch interval to avoid respawn
            CDPUtil::QuitRequested(GetConfigurator().getReplicationSettingsFetchInterval());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Target has not completed resync to Azure Agent so not completing resync with RCM. Target state = 0x%04x.\n", FUNCTION_NAME, targetResyncStateInfo.CurrentState);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::CompleteResync() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (GetConfigurator().isMobilityAgent())
    {
        CompleteResyncToRcm();
    }
    else
    {
        boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

        const ResyncStateInfo sourceResyncStateInfo(m_ResyncStateManager->GetPeerResyncState());
        if ((sourceResyncStateInfo.CurrentState & SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE) &&
            sourceResyncStateInfo.ResyncEndTimestamp)
        {
            if (!m_ResyncRcmSettings->IsOnpremReprotectTargetSide())
            {
                CompleteResyncToAzureAgent();
            }

            try
            {
                // Update target resync state to SYNC_STATE_TARGET_CONCLUDED_RESYNC.
                ResyncStateInfo newResyncStateInfo(m_ResyncStateManager->GetResyncState());
                newResyncStateInfo.CurrentState = SYNC_STATE_TARGET_CONCLUDED_RESYNC;

                if (!m_ResyncStateManager->UpdateState(newResyncStateInfo))
                {
                    const std::string err("CompleteResync with Azure Agent succeeded but target failed to update own resync state to SYNC_STATE_TARGET_CONCLUDED_RESYNC.");
                    throw DataProtectionException(err.c_str(), SV_LOG_ERROR);
                }

                DebugPrintf(SV_LOG_ALWAYS, "%s: Target successfully concluded resync.\n", FUNCTION_NAME);

                // Resync transition happened to step 2 so mark complete in IR profiling symmary.
                updateProfilingSummary(true);
            }
            catch (const std::exception & e)
            {
                std::stringstream ssmsg;
                ssmsg << FUNCTION_NAME << " failed to conclude resync with exception: " << e.what();
                throw DataProtectionException(ssmsg.str());
            }
            catch (...)
            {
                std::stringstream ssmsg;
                ssmsg << FUNCTION_NAME << " failed to conclude resync with an unknown exception.";
                throw DataProtectionException(ssmsg.str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Source has not set resync end timestamp so not completing resync to Azure Agent. Source state = 0x%04x.\n", FUNCTION_NAME, sourceResyncStateInfo.CurrentState);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


bool FastSyncRcm::IsNoDataTransferSync() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool isnodatatransfer = boost::iequals(m_ResyncRcmSettings->GetSourceAgentSyncSettings().SyncType,
        RcmClientLib::SYNC_TYPE_NO_DATA_TRANSFER);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, IsNoDataTransferSync %s\n", FUNCTION_NAME, isnodatatransfer ? "true" : "false");

    return isnodatatransfer;
}

bool FastSyncRcm::IsMarsCompatibleWithNoDataTranfer() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isMarsCompatibleWithNoDataTranfer = false;
#ifdef SV_WINDOWS
    do
    {
        LocalConfigurator lc;
        std::string errMsg;
        std::list<InstalledProduct> marsProduct;
        std::string minMARSVersionForMigration = lc.getMigrationMinMARSVersion();
        if (GetMarsProductDetails(marsProduct))
        {
            DebugPrintf(SV_LOG_DEBUG, "Number of MARS products received : %d\n", marsProduct.size());
            marsProduct.sort(CompareProudctVersion);
            marsProduct.unique();
            if (1 == marsProduct.size())
            {
                DebugPrintf(SV_LOG_DEBUG, "Mars Agent Version: %s\n", marsProduct.begin()->version.c_str());
                if (CompareVersion(marsProduct.begin()->version.c_str(), minMARSVersionForMigration) >= 0)
                {
                    DebugPrintf(SV_LOG_ALWAYS, "Mars Agent is supported for Migration\n");
                    isMarsCompatibleWithNoDataTranfer = true;
                    break;
                }
                else
                {
                    errMsg += "Current Mars agent Version is not supported for migration. Please make sure Mars agent version is minimum=";
                    errMsg += minMARSVersionForMigration;
                }
            }
            else
            {
                if (marsProduct.size())
                {
                    errMsg += "Multiple Mars agent versions are detected. Kindly re-install Mars agent";
                    DumpProductDetails(marsProduct);
                }
                else
                {
                    errMsg += "Mars agent is not installed on the machine. Please install MARS";
                }
            }
        }
        else
        {
            errMsg += "Failed to detect Mars agent version. Please re-install Mars agent";
        }

        DebugPrintf(SV_LOG_ERROR, 
            "Mars version incompatible for No Data Transfer sync.Error Msg=%s. Proceeding with Target Checksum resync ", 
            errMsg);
        isMarsCompatibleWithNoDataTranfer = false;
    } while (false);
#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isMarsCompatibleWithNoDataTranfer;
}

void FastSyncRcm::getResyncTimeSettingsForNoDataTransfer(ResyncTimeSettings& resyncTimeSettings) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    resyncTimeSettings.seqNo = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LatestReplicationCycleBookmarkSequenceId;
    resyncTimeSettings.time = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LatestReplicationCycleBookmarkTime;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::HandleInvalidBitmapError(const std::string & msg) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateLock);

    // Mark restart resync inline.
    DebugPrintf(SV_LOG_ERROR, "%s: Marking for inline restart resync, error=%s.\n", FUNCTION_NAME, msg.c_str());

    ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
    currentResyncStateInfo.CurrentState = SYNC_STATE_TARGET_RESTART_RESYNC_INLINE;
    currentResyncStateInfo.ErrorAlertNumber = E_RESYNC_BITMAPS_INVALID_ALERT;
    currentResyncStateInfo.FailureReason = msg;

    if (m_ResyncStateManager->UpdateState(currentResyncStateInfo))
    {
        // Pause till agents settings fetch interval and quit to avoid respawn as current resync session is failed
        CDPUtil::QuitRequested(GetConfigurator().getReplicationSettingsFetchInterval());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    throw DataProtectionException(msg, SV_LOG_SEVERE);
}

void FastSyncRcm::SendAlert(SV_ALERT_TYPE alertType, SV_ALERT_MODULE alertingModule, const InmAlert &alert) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    if (m_IsMobilityAgent)
    {
        RcmClientLib::MonitoringLib::SendAlertToRcm(alertType, alertingModule, alert, Settings().hostId);
        DebugPrintf(SV_LOG_ALWAYS, "%s: Sent alert %s.\n", FUNCTION_NAME, alert.GetID().c_str()); // TODO: include alert.GetMessage()
    }
    else
    {
        //
        // Ignoring alert in target.
        //          Current RcmConfiguration design has limitation of only source can raise alert.
        //          So for V2A RCM alerts from target are handeled via remote persistent resync state only when target fail resync.
        //              => In this case there is no question of source to acknowledge alert sent successful back to target.
        //          So if target want to raise without failing resync then we need to incorporate a feature - source to signal back to target about said alert is successfully raised.
        //              => This way target will clear the alert from state file. Otherwise source will keep on sending same alert in each monitor peer interval which will be false alert.
        //          Other solution is RcmConfiguration to allow target to raise alert.
        //
        //          Taking this feature in next release.
        //
        DebugPrintf(SV_LOG_ALWAYS, "%s: Target ignoring alert %s.\n", FUNCTION_NAME, alert.GetID().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

bool FastSyncRcm::SerializeOnPremToAzureSyncProgressMsg(RcmClientLib::OnPremToAzureSyncProgressMsg& syncProgressMsg,
    std::string &strSyncProgressMsg,
    std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        strSyncProgressMsg = JSON::producer<RcmClientLib::OnPremToAzureSyncProgressMsg>::convert(syncProgressMsg);
        strSyncProgressMsg.erase(std::remove(strSyncProgressMsg.begin(), strSyncProgressMsg.end(), '\n'),
            strSyncProgressMsg.end());
    }
    catch (const JSON::json_exception &jsone)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << "failed - JSON parser failed with exception " << jsone.what();
        errMsg = sserror.str();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    catch (const std::exception &e)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << "failed with exception " << e.what();
        errMsg = sserror.str();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    catch (...)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << "failed with an unknown exception.";
        errMsg = sserror.str();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

void FastSyncRcm::UpdateResyncProgress()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    try
    {
        static boost::chrono::steady_clock::time_point s_prevUpdateTime = boost::chrono::steady_clock::time_point::min();
        const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();

        std::stringstream ss;
        
        if (!m_IsMobilityAgent)
        {
            ss << "resync progress update is supported only on source so ignoring";
        }
        else if (0 == m_ResyncStateManager->GetResyncState().ResyncStartTimestamp)
        {
            ss << "Skipping resync progress update as resync start time is not yet updated";
        }
        else
        {
            // Check for ResyncUpdateInterval. We can skip first update call.
            if (currentTime < (s_prevUpdateTime + boost::chrono::seconds(m_ResyncUpdateInterval)))
            {
                ss << "Skipping resync progress update as progress update interval is yet to reach";
            }
        }

        if(!ss.str().empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: %s\n", FUNCTION_NAME, ss.str().c_str());
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        const ResyncStats resyncStats = GetResyncStats();
        
        // Prepare VMwareToAzureSyncProgressMsg
        RcmClientLib::OnPremToAzureSyncProgressMsg syncProgressMsg;

        syncProgressMsg.ReplicationPairId = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationPairId;

        syncProgressMsg.ReplicationSessionId = JobId();

        const uint64_t &numberOfBytesSynced = resyncStats.m_ResyncProcessedBytes;

        if (0 == m_HashCompareDataSize)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed: HashCompareDataSize is set to zero which is not expected to be zero. This can be because of wrong config entry.\n",
                FUNCTION_NAME);
            return;
        }

        // If disk size is not alligned with m_HashCompareDataSize add one extra block
        syncProgressMsg.NumberOfBlocksSynced = (numberOfBytesSynced == -1) ? 0 :
            ((numberOfBytesSynced % m_HashCompareDataSize) ?
             (numberOfBytesSynced / m_HashCompareDataSize) + 1 :
                (numberOfBytesSynced / m_HashCompareDataSize));

        syncProgressMsg.TotalNumberOfBlocks = (getVolumeSize() % m_HashCompareDataSize) ?
            (getVolumeSize() / m_HashCompareDataSize) + 1 :
            (getVolumeSize() / m_HashCompareDataSize);

        syncProgressMsg.MatchedBytesRatio = (numberOfBytesSynced == -1) ? 0 : resyncStats.m_MatchedBytesRatio;

        syncProgressMsg.EstimatedTimeRemaining = 0; // TODO: post PP2

        syncProgressMsg.ResyncLast15MinutesTransferredBytes = (numberOfBytesSynced == -1) ? 0 : resyncStats.m_ResyncLast15MinutesTransferredBytes;
        if (!resyncStats.m_ResyncLastDataTransferTimeUtcPtimeIsoStr.empty())
        {
            const boost::posix_time::ptime resyncLastDataTransferTimeUtcPtime =
                boost::posix_time::from_iso_string(resyncStats.m_ResyncLastDataTransferTimeUtcPtimeIsoStr);
            if (!resyncLastDataTransferTimeUtcPtime.is_not_a_date_time())
            {
                syncProgressMsg.ResyncLastDataTransferTimeUtc = boost::posix_time::to_simple_string(resyncLastDataTransferTimeUtcPtime);
            }
        }
        syncProgressMsg.ResyncProcessedBytes = (numberOfBytesSynced == -1) ? 0 : resyncStats.m_ResyncProcessedBytes;
        syncProgressMsg.ResyncTransferredBytes = (numberOfBytesSynced == -1) ? 0 : resyncStats.m_ResyncTotalTransferredBytes;
        
        // Convert in sec as srcResyncStarttime is TimeInHundNanoSeconds
        const int IntervalsPerSecond = 10000000;
		const SV_ULONGLONG srcResyncStartTimeInSec = m_ResyncStateManager->GetResyncState().ResyncStartTimestamp / IntervalsPerSecond;
        const SV_ULONGLONG srcResyncStartTimeInSecInUnixEpoch = (Settings().sourceOSVal == SV_WIN_OS) ?
            (srcResyncStartTimeInSec - GetSecsBetweenEpoch1970AndEpoch1601()) : srcResyncStartTimeInSec;
        syncProgressMsg.ResyncCopyStartTimeUtc = boost::posix_time::to_simple_string(
            boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1)) + boost::posix_time::seconds(srcResyncStartTimeInSecInUnixEpoch));

        std::string strSyncProgressMsg;
        std::string errMsg;
        if (!SerializeOnPremToAzureSyncProgressMsg(syncProgressMsg, strSyncProgressMsg, errMsg))
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to serialize OnPremToAzureSyncProgressMsg with error %s.\n", FUNCTION_NAME, errMsg.c_str());
            return;
        }

        if (!RcmClientLib::MonitoringLib::PostSyncProgressMessageToRcm(strSyncProgressMsg, Settings().deviceName))
        {
            const int errorCode = RcmClientLib::RcmConfigurator::getInstance()->GetRcmClient()->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s failed as PostSyncProgressMessageToRcm failed with error code %d.\n", FUNCTION_NAME, errorCode);
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: OnPremToAzureSyncProgressMsg=%s.\n", FUNCTION_NAME, strSyncProgressMsg.c_str());
            s_prevUpdateTime = currentTime;
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

void FastSyncRcm::MatchedBytesRatioCallback(uint32_t & matchedBytesRatio)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    if (!m_IsMobilityAgent)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s called in target mode, this function is supposed to be called only on source so ignoring.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    ResyncProgressCounters resyncProgressCounters;
    std::string errmsg;
    if (!m_ResyncProgressInfo->GetResyncProgressCounters(resyncProgressCounters, errmsg))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errmsg.c_str());
        return;
    }

    const uint64_t numberOfBytesSynced = resyncProgressCounters.GetResyncProcessedBytes();
    matchedBytesRatio = numberOfBytesSynced ? ((resyncProgressCounters.GetFullyMatchedBytesFromProcessHcdTask() +
        resyncProgressCounters.GetPartialMatchedBytesFromProcessSyncDataTask()) * 100 / numberOfBytesSynced) : 0;

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

bool FastSyncRcm::IsResyncDataAlreadyApplied(SV_ULONGLONG* offset, SharedBitMap*bm, DiffPtr change_metadata)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    const SV_UINT resync_chunk_size = GetConfigurator().getFastSyncMaxChunkSizeForE2A();

    if (offset == NULL)
    {
        throw INMAGE_EX("Out param offset is NULL.")(Settings().deviceName);
    }
    
    cdp_drtdv2s_iter_t drtdsBegin = change_metadata->DrtdsBegin();
    cdp_drtdv2s_iter_t drtdsIterEnd = change_metadata->DrtdsEnd();
    if (drtdsBegin != drtdsIterEnd)
    {
        cdp_drtdv2_t drtd = *drtdsBegin;
        *offset = drtd.get_volume_offset();

    }
    if (bm != NULL)
    {
        int chunkIndex = *offset / resync_chunk_size;
        if (bm->isBitSet(chunkIndex * 2) && bm->isBitSet((chunkIndex * 2) + 1))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: The Sync data already applied for chunk %d.\n", FUNCTION_NAME, chunkIndex);
            rv = true;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: About to apply sync data starting at offset %llu.\n", FUNCTION_NAME, *offset);
        }
    }
    /* End of Change */

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}

void FastSyncRcm::UpdateResyncStats(const bool forceUpdate)
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

        FastSync::UpdateResyncStats();
        ResyncStats resyncStats = FastSync::GetResyncStats();
        std::string strResyncStats = JSON::producer<ResyncStats>::convert(resyncStats);
        strResyncStats.erase(std::remove(strResyncStats.begin(), strResyncStats.end(), '\n'), strResyncStats.end());

        ResyncStateInfo currentResyncStateInfo(m_ResyncStateManager->GetResyncState());
        currentResyncStateInfo.Properties[TargetPropertyResyncStats] = strResyncStats;

        const boost::posix_time::ptime currentHeartBeatTime = boost::posix_time::second_clock::universal_time();
        currentResyncStateInfo.Properties[TargetPropertyTargetToAzureHeartbeat] = boost::posix_time::to_iso_string(currentHeartBeatTime);

        if (m_ResyncStateManager->UpdateState(currentResyncStateInfo))
        {
            s_prevUpdateTime = currentTime;
            DebugPrintf(SV_LOG_ALWAYS, "%s: strResyncStats=%s.\n", FUNCTION_NAME, strResyncStats.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to update ResyncStats in remote resync state.\n", FUNCTION_NAME);
        }
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
}

const ResyncStats FastSyncRcm::GetResyncStats() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ResyncStats resyncStats;

    if (!Source())
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s called in target mode, this function is supposed to be called only on source so ignoring.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return resyncStats;
    }

    const ResyncStateInfo targetResyncStateInfo(m_ResyncStateManager->GetPeerResyncState());
    const std::map<std::string, std::string>::const_iterator citr = targetResyncStateInfo.Properties.find(TargetPropertyResyncStats);
    if (citr != targetResyncStateInfo.Properties.end())
    {
        try
        {
            resyncStats = JSON::consumer<ResyncStats>::convert(citr->second, true);
        }
        catch (const boost::property_tree::json_parser::json_parser_error &jsonparsere)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s failed to deserialize ResyncStats with exception &s.\n", FUNCTION_NAME, jsonparsere.what());
        }
        catch (const JSON::json_exception &jsone)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s failed to deserialize ResyncStats with exception &s.\n", FUNCTION_NAME, jsone.what());
        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s failed to deserialize ResyncStats with exception &s.\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s failed to deserialize ResyncStats with an unknown exception.\n", FUNCTION_NAME);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return resyncStats;
}

void FastSyncRcm::CleanupResyncStaleFilesOnPS()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    do
    {
        static boost::chrono::steady_clock::time_point s_prevCleanupTime = boost::chrono::steady_clock::time_point::min();
        const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();
        if (currentTime < s_prevCleanupTime + boost::chrono::seconds(m_ResyncStaleFilesCleanupInterval))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as ResyncStaleFilesCleanupInterval is yet to reach.\n", FUNCTION_NAME);
            break;
        }

        if (!Source())
        {
            // IMPORTANT TODO NOTE: We should enable target DP to post to RCM/RCM proxy to avoid this kind of non recommended stuff.
            
            // Cleanup stale TargetResyncState_*.json files on PS resync cache.
            const std::string targetResyncStateFileSpec(m_ResyncStateManager->GetRemoteResyncStateFileSpec());
            FileInfosPtr fileInfos = boost::make_shared<FileInfos_t>();
            std::string strmsg;
            
            if (!m_ResyncProgressInfo->ListFile(targetResyncStateFileSpec, fileInfos, strmsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, strmsg.c_str());
            }
            else if (fileInfos->size() > 1)
            {
                // Sort to make sure latest file is at end.
                std::sort(fileInfos->begin(), fileInfos->end());

                // Always exclude a latest TargetResyncState_*.json file as it has current target resync state.
                fileInfos->pop_back(); // NOTE: If this is missd then result IR/resync will stuck untill
                                       // service rcmreplicationagent is manually restarted on target.
                
                DebugPrintf(SV_LOG_DEBUG, "%s: %d stale %s files identified, attempting to cleanup\n", FUNCTION_NAME,
                    fileInfos->size(), RemoteTargetResyncStateFile.c_str());

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

    if (!IsTransportProtocolAzureBlob())
        FastSync::CleanupResyncStaleFilesOnPS();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string FastSyncRcm::RetrieveDeviceNameFromVolumeSummaries() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string deviceName;

#ifdef SV_WINDOWS
    bool isAzureStack = IsAzureStackVirtualMachine();
    ConstVolumeSummariesIter itrVs(m_VolumesCache.m_Vs.begin());
    DebugPrintf(SV_LOG_ALWAYS, "%s: TargetDiskId=%s.\n", FUNCTION_NAME, (m_ResyncRcmSettings->GetSourceAgentSyncSettings().TargetDiskId).c_str());
    for (/*empty*/; itrVs != m_VolumesCache.m_Vs.end(); ++itrVs)
    {
        if (!isAzureStack)
        {
            Attributes_t::const_iterator citrSerialNumber = itrVs->attributes.find(NsVolumeAttributes::SERIAL_NUMBER);
            if (citrSerialNumber != itrVs->attributes.end() &&
                boost::iequals(citrSerialNumber->second, m_ResyncRcmSettings->GetSourceAgentSyncSettings().TargetDiskId))
            {
                deviceName = itrVs->name;

                DebugPrintf(SV_LOG_ALWAYS, "%s: deviceName=%s.\n", FUNCTION_NAME, deviceName.c_str());

                break;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Running on Azure Stack Hub virtual machine.\n", FUNCTION_NAME);
            // There's only one SCSI interface and only all of the data disks are attached to this SCSI interface in case of Azure stack
            // Use LUN as target disk ID to find the device on SCSI interface should work, if the above assumption changes, need to relook
            Attributes_t::const_iterator citrInterfaceType = itrVs->attributes.find(NsVolumeAttributes::INTERFACE_TYPE);
            Attributes_t::const_iterator citrSCSILUN = itrVs->attributes.find(NsVolumeAttributes::SCSI_LOGICAL_UNIT);
            if (citrInterfaceType != itrVs->attributes.end() && citrSCSILUN != itrVs->attributes.end())
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: current deviceName=%s, INTERFACE_TYPE=%s, SCSI_LOGICAL_UNIT=%s.\n",
                    FUNCTION_NAME, itrVs->name, (citrInterfaceType->second).c_str(), (citrSCSILUN->second).c_str());
                if (boost::iequals(citrSCSILUN->second, m_ResyncRcmSettings->GetSourceAgentSyncSettings().TargetDiskId) &&
                    boost::iequals(citrInterfaceType->second, "scsi"))
                {
                    deviceName = itrVs->name;

                    DebugPrintf(SV_LOG_ALWAYS, "%s: deviceName=%s.\n", FUNCTION_NAME, deviceName.c_str());

                    break;
                }
            }
        }
    }


#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return deviceName;
}

std::string FastSyncRcm::GetDeviceName() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string deviceName;

    deviceName = m_ResyncRcmSettings->IsOnpremReprotectTargetSide() ?
        RetrieveDeviceNameFromVolumeSummaries() : Settings().deviceName;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return deviceName;
}

void FastSyncRcm::CacheVolumeGroupSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

#ifdef SV_WINDOWS
    if (m_ResyncRcmSettings->IsOnpremReprotectTargetSide())
    {
        VOLUME_GROUP_SETTINGS volGroupSettings;
        volGroupSettings.direction = SOURCE;
        volGroupSettings.status = PROTECTED;

        VOLUME_SETTINGS vs = Settings();
        vs.deviceName = GetDeviceName();

        volGroupSettings.volumes[vs.deviceName] = vs;

        m_ResyncRcmSettings->CacheVolumeGroupSettings(volGroupSettings);
    }
#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FastSyncRcm::MonitorTargetToAzureHealth() {

    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    try {
        if (!RcmClientLib::RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s called in OnPrem to Azure direction, this monitoring is supposed to be called only in reprotect direction so ignoring.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        if (!m_IsMobilityAgent)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s called in target mode, this function is supposed to be called only on source so ignoring.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        static boost::chrono::steady_clock::time_point s_prevUpdateTime = boost::chrono::steady_clock::time_point::min();
        const boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();

        if (currentTime <= (s_prevUpdateTime + boost::chrono::seconds(RESYNC_SOURCE_MONITOR_TARGET_TO_AZURE_HEARTBEAT_INTERVAL_IN_SEC)))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as monitoring interval is yet to reach.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        ResyncStateInfo resyncStateInfo = m_ResyncStateManager->GetPeerResyncState();
        const std::string lastTargetToAzureHeartBeatTimestr = resyncStateInfo.Properties[TargetPropertyTargetToAzureHeartbeat];

        if (lastTargetToAzureHeartBeatTimestr.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as the lastTargetToAzureHeartBeatTime is empty.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        const std::string healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::HealthCode;

        if (m_prevTargetToAzureHeartbeatTimestr.empty() || (m_prevTargetToAzureHeartbeatTimestr != lastTargetToAzureHeartBeatTimestr)) {
            m_prevTargetToAzureHeartbeatTimestr = lastTargetToAzureHeartBeatTimestr;
            m_prevTargetToAzureHeartbeatSourceTime = currentTime;
            if (m_healthCollatorPtr->IsDiskHealthIssuePresent(healthIssueCode, GetDeviceName())) 
            {
                if (!m_healthCollatorPtr->ResetDiskHealthIssue(healthIssueCode, GetDeviceName()))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to reset the health issue %s for the disk %s.\n", FUNCTION_NAME, healthIssueCode.c_str(), GetDeviceName().c_str());
                }
            }
            DebugPrintf(SV_LOG_DEBUG, "%s: Skipping as in memory time is not yet set.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        if(currentTime >= (m_prevTargetToAzureHeartbeatSourceTime + boost::chrono::seconds(RESYNC_TARGET_TO_AZURE_NO_HEARTBEAT_RAISE_HEALTH_ISSUE_INTERVAL_IN_SEC)))
        {
            //raise a health issue
            std::string blobContainerSasUrl;
            std::string cacheStorageAccountName;

            if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB)) {
                 blobContainerSasUrl = m_ResyncRcmSettings->GetSourceAgentSyncSettings().TransportSettings.BlobContainerSasUri; 
            }
            if (!blobContainerSasUrl.empty()) {
                ExtractCacheStorageNameFromBlobContainerSasUrl(blobContainerSasUrl, cacheStorageAccountName);
            }
            if (cacheStorageAccountName.empty())
            {
                std::string blobSasUrlToBeLogged = blobContainerSasUrl.substr(
                    0, blobContainerSasUrl.find("?"));
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to raise the health issue as cache storage account name is empty. blob SAS is %s\n", 
                    FUNCTION_NAME, blobSasUrlToBeLogged.c_str());
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return;
            }
            std::map<std::string, std::string> msgParams;
            msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::CacheStorageAccountName,
                cacheStorageAccountName.c_str()));
            msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::ObservationTime,
                m_healthCollatorPtr->GetCurrentObservationTime()));
            AgentDiskLevelHealthIssue diskHI(GetDeviceName(), healthIssueCode, msgParams);
            if (!m_healthCollatorPtr->SetDiskHealthIssue(diskHI))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to raise the health issue %s for the disk %s.\n", FUNCTION_NAME, healthIssueCode.c_str(), GetDeviceName().c_str());
            }
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        
        //reset the health issue if set earlier else do nothing
        if (m_healthCollatorPtr->IsDiskHealthIssuePresent(healthIssueCode, GetDeviceName())) {
            if (!m_healthCollatorPtr->ResetDiskHealthIssue(healthIssueCode, GetDeviceName()))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to reset the health issue %s for the disk %s.\n", FUNCTION_NAME, healthIssueCode.c_str(), GetDeviceName().c_str());
            }
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
