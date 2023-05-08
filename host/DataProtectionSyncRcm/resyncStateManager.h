#ifndef RESYNCSTATEMANAGER_H
#define RESYNCSTATEMANAGER_H

#include <iostream>

#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>

#include "cxtransport.h"
#include "SyncSettings.h"
#include "RcmConfigurator.h"
#include "dpRcmProgramargs.h"
#include "resyncRcmSettings.h"

#include "json_reader.h"
#include "json_writer.h"

enum SYNC_STATE
{
    SYNC_STATE_RESYNC_INIT                         = 0x0001,
    SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE    = 0x0002,
    SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE      = 0x0004,
    SYNC_STATE_RESYNC_FAILED                       = 0x0008,
    SYNC_STATE_TARGET_CONCLUDED_RESYNC             = 0x0010,
    SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM      = 0x0020,
    SYNC_STATE_TARGET_RESTART_RESYNC_INLINE        = 0x0040,
    SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE            = 0x0080
};

enum SYNC_STATE_OWNER
{
    SYNC_STATE_OWNER_UNKNOWN    = 0x0000,
    SYNC_STATE_OWNER_SOURCE     = 0x0001,
    SYNC_STATE_OWNER_TARGET     = 0x0002
};

static const std::string RemoteSourceResyncStateFile = "SourceResyncState";
static const std::string RemoteTargetResyncStateFile = "TargetResyncState";

// ReplicationSessionId is initialized by source only and persist in source resync state.
// Target always use ReplicationSessionId from source state file for initializing AzureAgent.
// Source append inline resync restart count starting 0 to ReplicationSessionId and will increment
// same for each inline restart resync event for current resync session.
static const std::string SourcePropertyReplicationSessionId = "ReplicationSessionId";

static const std::string SourcePropertyHostName = "SourceHostName";
static const std::string SourcePropertyDiskRawSize = "SourceDiskRawSize";
static const std::string TargetPropertyResyncStats = "TargetResyncStats";
static const std::string TargetPropertyTargetToAzureHeartbeat = "TargetToAzureHeartbeat";

//
// ResyncStateInfo is a common resync state information structure used by both source and target
// Few fields are exclusively set by source and target and act as an indicator of a peer event.
// For example, ResyncStartTimestamp is set by source and is an indicator for target to start generating HCDs from Azure blob.
// Though few fields are exclusively set by source and target the structure is defined as common entity for convinience of JSON parsing.
//
class ResyncStateInfo // TODO: rename to SyncStateInfo ? - common for IR and resync
{
public:
    ResyncStateInfo() :
        ResyncStateOwner(SYNC_STATE_OWNER_UNKNOWN),
        CurrentState(SYNC_STATE_RESYNC_INIT),
		ResyncStartTimestamp(0),
		ResyncStartTimestampSeqNo(0),
		ResyncEndTimestamp(0),
		ResyncEndTimestampSeqNo(0),
        RestartResync(false),
        ErrorAlertNumber(0) {}

    // TODO: ResyncStateOwner should be read only - once assigned should not be changed.
    // TODO: nichougu - write a wrapper on ResyncStateInfo which will internally update members and wrapper will be exposed to user of ResyncStateInfo - resync classes here.
    // ResyncStateOwner represents ownership of state either source or tagret defined in RESYNC_STATE_OWNER.
    int ResyncStateOwner;

    // TODO: CurrentState can change for own and entire ResyncStateInfo is const for peer.
    // TODO: nichougu - not a must change - I will take a call in next iteration.
    // CurrentState represents resync state defined in RESYNC_STATE.
    int CurrentState;

    // ResyncStartTimestamp and ResyncStartTimestampSeqNo is exclusively set by source.
    // ResyncStartTimestamp is an indicator for target to start generating HCDs from Azure blob.
    uint64_t ResyncStartTimestamp;
    uint64_t ResyncStartTimestampSeqNo;

    // ResyncEndTimestamp and ResyncEndTimestampSeqNo is exclusively set by source.
    // ResyncEndTimestamp is an indicator for target to conclude resync using Azure agent API CompleteResync.
    uint64_t ResyncEndTimestamp;
    uint64_t ResyncEndTimestampSeqNo;

	// Properties key value map defined by owner for use of peer.
	// For example SourceHostName and SourceDiskRawSize are exclusively set by source and are an input for target to create Azure agent instance.
    // For example ResyncStats is exclusively set by target and is an input for source for OnPremToAzureSyncProgressMsg.
	std::map<std::string, std::string> Properties;

	// FailureReason represents resync fail reason.
	std::string FailureReason;

	// RestartResync if true indicates to mark restart resync in CompleteResync input to RCM.
	bool RestartResync;

	// ErrorAlertNumber is exclusively set by target to signal source to send particular health event.
	// As of now by target mark health event on resync failure so no reset of ErrorAlertNumber.
	int ErrorAlertNumber;

public:
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "ResyncStateInfo", false);
        JSON_E(adapter, ResyncStateOwner);
        JSON_E(adapter, CurrentState);
        JSON_E(adapter, ResyncStartTimestamp);
        JSON_E(adapter, ResyncStartTimestampSeqNo);
        JSON_E(adapter, ResyncEndTimestamp);
        JSON_E(adapter, ResyncEndTimestampSeqNo);
        JSON_KV_E(adapter, "Properties", Properties);
        JSON_E(adapter, FailureReason);
        JSON_E(adapter, RestartResync);
        JSON_T(adapter, ErrorAlertNumber);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, ResyncStateOwner);
        JSON_P(node, CurrentState);
        JSON_P(node, ResyncStartTimestamp);
        JSON_P(node, ResyncStartTimestampSeqNo);
        JSON_P(node, ResyncEndTimestamp);
        JSON_P(node, ResyncEndTimestampSeqNo);
        JSON_KV_P(node, Properties);
        JSON_P(node, FailureReason);
        JSON_P(node, RestartResync);
        JSON_P(node, ErrorAlertNumber);
    }
};

class ResyncStateManager
{
public:

    static const boost::shared_ptr<ResyncStateManager> GetResyncStateManager(
        const DpRcmProgramArgs &dpProgramArgs);

    ResyncStateInfo GetResyncState() const;

    ResyncStateInfo GetPeerResyncState() const;

    void DumpCurrentResyncState() const;

    bool CanContinueResync() const;

    void MonitorPeerState(const bool isBlocking = false);

    bool UpdateState(ResyncStateInfo & newResyncState);

    sigslot::signal1<uint32_t &> & GetMatchedBytesRatioSignal();

    const std::string GetRemoteResyncStateFileSpec() const;

    bool IsInlineRestartResyncMarked();

    bool CleanupRemoteCache();

    std::string GetReplicationSessionId() const;

    ~ResyncStateManager() {}

protected:

    explicit ResyncStateManager(const DpRcmProgramArgs &args);

    bool InitializeState();

    void InitializeDatapathTransport();

    void NextRemoteResyncStateFileName(boost::filesystem::path& preTarget, boost::filesystem::path& target);

    bool SerializeResyncState(ResyncStateInfo& resyncState, std::string &strResyncState, std::string &errMsg);

    bool DeserializeResyncState(ResyncStateInfo& resyncState, std::string &strResyncState, std::string &errMsg);

    bool ListRemoteState(const std::string& remoteStateFileSpec, bool &isRemoteStatePresent, std::string& remoteStateFileName, std::size_t &remoteFileSize, std::string &errMsg);

    bool PullRemoteState(const std::string& remoteStateFileSpec, std::string& strRemoteState, std::string &errMsg, const std::size_t remoteFileSize = 0);

    bool PushState(const std::string& strResyncState, std::string &errMsg);

    bool CompleteSync(RcmClientLib::SourceAgentCompleteSyncInput & completeSyncInput);

    bool SerializeCompleteSyncInput(RcmClientLib::SourceAgentCompleteSyncInput& completeSyncInput, std::string &CompleteSyncInput, std::string &errMsg);

    void UpdateInMemPeerState(const ResyncStateInfo & newPeerResyncState);

    void UpdateReplicationSessionId();

    bool ValidateNewState(const SYNC_STATE &syncState);

    void ResetSourceResyncState();

    bool ResetTargetResyncState();

protected:
    const DpRcmProgramArgs &m_Args;

    const std::string m_Mode;

    const std::string m_PeerMode;

    RcmClientLib::RcmConfiguratorPtr m_RcmConfigurator;

    boost::shared_ptr<ResyncRcmSettings> m_ResyncRcmSettings;

    const int m_PeerRemoteResyncStateParseRetryCount;

protected:
    std::string m_RemoteResyncStateFileSpec;

    std::string m_PeerRemoteResyncStateFileSpec;

    ResyncStateInfo m_ResyncState;

    ResyncStateInfo m_PeerResyncState;

    std::string m_SerializedPeerResyncState;

    mutable boost::recursive_mutex m_ResyncStateManagerLock;

    CxTransport::ptr m_CxTransport;

    mutable boost::recursive_mutex m_CxTransportLock;

    sigslot::signal1<uint32_t &> m_MatchedBytesRatioSignal;

private:
    mutable boost::mutex m_FileNameLock;

    uint64_t m_RemoteStateFileSeqNo;

    static boost::atomic<bool> m_IsInitialized;

    static boost::mutex m_Lock;

};

#endif // RESYNCSTATEMANAGER_H

