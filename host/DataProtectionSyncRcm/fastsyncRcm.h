#ifndef FASTSYNC_RCM_H
#define FASTSYNC_RCM_H

#include <boost/thread/mutex.hpp>

#include "fastsync.h"
#include "resyncStateManager.h"

class FastSyncRcm : public FastSync, public sigslot::has_slots<>
{
public:
    explicit FastSyncRcm(const VOLUME_GROUP_SETTINGS & volumeGrpSettings,
        const boost::shared_ptr<ResyncStateManager> resyncStateManager) :
        FastSync(volumeGrpSettings, /*dummy value*/AGENT_FRESH_START),
        m_ResyncStateManager(resyncStateManager),
        m_ResyncUpdateInterval(GetConfigurator().getResyncUpdateInterval())
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        ResyncRcmSettings::GetInstance(m_ResyncRcmSettings);

        // Attach MatchedBytesRatioSignal in ResyncStateManage with MatchedBytesRatioCallback
        m_ResyncStateManager->GetMatchedBytesRatioSignal().connect(this, &FastSyncRcm::MatchedBytesRatioCallback);

        CacheVolumeGroupSettings();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }

    virtual bool GetSyncDir();

    virtual bool IsResyncStartTimeStampAlreadySent(ResyncTimeSettings &rts);

    virtual bool ResyncStartNotifyIfRequired();

    virtual ClearDiffsState_t CanClearDifferentials();

    virtual bool NotifyResyncStartToControlPath(const ResyncTimeSettings &rts, const bool canClearDiffs);

    virtual bool IsResyncEndTimeStampAlreadySent(ResyncTimeSettings &rts);

    virtual bool NotifyResyncEndToControlPath(const ResyncTimeSettings &rts);

    virtual bool sendResyncEndNotifyAndResyncTransition();

    virtual bool SetFastSyncMatchedBytes(SV_ULONGLONG bytes, int during_resyncTransition = 0);

    virtual bool SetFastSyncFullyUnusedBytes(SV_ULONGLONG bytesunused);

    virtual void ThrottleSpace();

    virtual void CheckResyncState() const;

    virtual bool IsStartTimeStampSetBySource() const;

    virtual void RetrieveCdpSettings();

    virtual bool SetFastSyncFullSyncBytes(SV_LONGLONG fullSyncBytes);

    virtual bool SetFastSyncProgressBytes(const SV_LONGLONG &bytesApplied, const SV_ULONGLONG &bytesMatched, const std::string &resyncStatsJson);
    
    virtual const std::string GetResyncStatsForCSLegacyAsJsonString();

    virtual bool FailResync(const std::string resyncFailReason, const bool restartResync = false) const;

    virtual void ApplyChanges(const std::string &fileToApply) const;

    virtual void HandleAzureCancelOpException(const std::string & msg) const;

    virtual void HandleAzureInvalidOpException(const std::string & msg, const long errorCode) const;

    virtual void HandleInvalidBitmapError(const std::string & msg) const;

    virtual void CompleteResyncToAzureAgent() const;

    virtual void CompleteResyncToRcm() const;

    virtual void CompleteResync() const;

    virtual bool IsNoDataTransferSync() const;

    virtual bool IsMarsCompatibleWithNoDataTranfer() const;

    virtual void getResyncTimeSettingsForNoDataTransfer(ResyncTimeSettings& resyncTimeSettings) const;

    virtual void SendAlert(SV_ALERT_TYPE alertType, SV_ALERT_MODULE alertingModule, const InmAlert &alert) const;

    virtual void UpdateResyncProgress();

    virtual void MonitorTargetToAzureHealth();

    virtual bool IsResyncDataAlreadyApplied(SV_ULONGLONG* offset, SharedBitMap*bm, DiffPtr change_metadata);

    virtual void UpdateResyncStats(const bool forceUpdate = false);

    virtual const ResyncStats GetResyncStats() const;

    virtual void CleanupResyncStaleFilesOnPS();

    virtual std::string GetDeviceName() const;

    void CacheVolumeGroupSettings();

    virtual ~FastSyncRcm()
    {
        m_ResyncStateManager->GetMatchedBytesRatioSignal().disconnect(this);
    }

private:
    bool SerializeOnPremToAzureSyncProgressMsg(RcmClientLib::OnPremToAzureSyncProgressMsg& syncProgressMsg,
        std::string &strSyncProgressMsg,
        std::string &errMsg);

    void MatchedBytesRatioCallback(uint32_t & matchedBytesRatio);

    std::string RetrieveDeviceNameFromVolumeSummaries() const;

private:
    const boost::shared_ptr<ResyncStateManager> m_ResyncStateManager;

    // Used for exclusive access to m_ResyncStateManager.
    // A recursive lock is defined to handle scenario for example, if CompleteResync to AzureAgent fail with
    // AzureCancelOpException then it call HandleAzureCancelOpException while it already acquired m_ResyncStateLock.
    mutable boost::recursive_mutex m_ResyncStateLock;

    boost::shared_ptr<ResyncRcmSettings> m_ResyncRcmSettings;

    const SV_ULONG m_ResyncUpdateInterval;

    std::string m_prevTargetToAzureHeartbeatTimestr;
    boost::chrono::steady_clock::time_point m_prevTargetToAzureHeartbeatSourceTime;
};

#endif // FASTSYNC_RCM_H