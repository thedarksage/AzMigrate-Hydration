/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. All rights reserved.
+------------------------------------------------------------------------------------+
File        :    SyncSettings.h

Description    :   Contains IR or resync actions, completion input and progress message
related classes
+------------------------------------------------------------------------------------+
*/
#ifndef RCM_SYNC_SETTINGS_H
#define RCM_SYNC_SETTINGS_H

#include <string>
#include <vector>

#include "json_reader.h"
#include "json_writer.h"

#include "RcmJobs.h"
#include "RcmContracts.h"

namespace RcmClientLib
{

    /// \brief Defines settings for SourceAgent Sync action.
    class OnPremToAzureSourceAgentProcessServerBasedSyncSettings
    {
    public:
        /// \brief the replication pair ID.
        std::string ReplicationPairId;

        /// \brief the replication session ID.
        std::string ReplicationSessionId;

        std::string     ResyncProgressType;

        // Resync copy initiated time expressed as the number of 100 - nanosecond
        // intervals that have elapsed since 12:00:00 midnight, January 1, 0001.
        uint64_t        ResyncCopyInitiatedTimeTicks;

        std::string LogFolder;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureSourceAgentProcessServerBasedSyncSettings", false);

            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_E(adapter, ResyncProgressType);
            JSON_E(adapter, ResyncCopyInitiatedTimeTicks);
            JSON_T(adapter, LogFolder);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, ReplicationPairId);
            JSON_P(node, ReplicationSessionId);
            JSON_P(node, ResyncProgressType);
            JSON_P(node, ResyncCopyInitiatedTimeTicks);
            JSON_P(node, LogFolder);
        }
    };

    /// \brief Defines settings for SourceAgent Zero-Copy resync
    class OnPremToAzureSourceAgentNoDataTransferSyncSettings : public OnPremToAzureSourceAgentProcessServerBasedSyncSettings
    {
    public:
        /// \brief the latest time in hundred nano seconds since windows epoch when
        /// the bookmark issued
        uint64_t  LatestReplicationCycleBookmarkTime;

        /// \brief latest bookmark IO sequence identifier
        uint64_t  LatestReplicationCycleBookmarkSequenceId;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureSourceAgentNoDataTransferSyncSettings", false);

            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_E(adapter, ResyncProgressType);
            JSON_E(adapter, ResyncCopyInitiatedTimeTicks);
            JSON_E(adapter, LogFolder);
            JSON_E(adapter, LatestReplicationCycleBookmarkTime);
            JSON_T(adapter, LatestReplicationCycleBookmarkSequenceId);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, ReplicationPairId);
            JSON_P(node, ReplicationSessionId);
            JSON_P(node, ResyncProgressType);
            JSON_P(node, ResyncCopyInitiatedTimeTicks);
            JSON_P(node, LogFolder);
            JSON_P(node, LatestReplicationCycleBookmarkTime);
            JSON_P(node, LatestReplicationCycleBookmarkSequenceId);
        }

    };

    class SyncSettings
    {
    public:
        std::string     DiskId;

        std::string     ReplicationPairId;

        std::string     ReplicationSessionId;

        std::string     LogFolder;

        std::string     TargetDiskId;

        AzureBlobBasedTransportSettings TransportSettings;

        std::string     ResyncProgressType;

        /// \brief zero copy resync settings for brownfield migration
        std::string     SyncType;

        /// \brief the latest time in hundred nano seconds since windows epoch when
        /// the bookmark issued
        uint64_t  LatestReplicationCycleBookmarkTime;

        /// \brief latest bookmark IO sequence identifier
        uint64_t  LatestReplicationCycleBookmarkSequenceId;

        // Resync copy initiated time expressed as the number of 100 - nanosecond
        // intervals that have elapsed since 12:00:00 midnight, January 1, 0001.
        uint64_t        ResyncCopyInitiatedTimeTicks;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SyncSettings", false);

            JSON_E(adapter, DiskId);
            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_E(adapter, LogFolder);
            JSON_E(adapter, TargetDiskId);
            JSON_E(adapter, TransportSettings);
            JSON_E(adapter, ResyncProgressType);
            JSON_E(adapter, SyncType);
            JSON_E(adapter, LatestReplicationCycleBookmarkTime);
            JSON_E(adapter, LatestReplicationCycleBookmarkSequenceId);
            JSON_T(adapter, ResyncCopyInitiatedTimeTicks);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, DiskId);
            JSON_P(node, ReplicationPairId);
            JSON_P(node, ReplicationSessionId);
            JSON_P(node, LogFolder);
            JSON_P(node, TargetDiskId);
            JSON_CL(node, TransportSettings);
            JSON_P(node, ResyncProgressType);
            JSON_P(node, SyncType);
            JSON_P(node, LatestReplicationCycleBookmarkTime);
            JSON_P(node, LatestReplicationCycleBookmarkSequenceId);
            JSON_P(node, ResyncCopyInitiatedTimeTicks);
        }

        bool operator==(const SyncSettings& rhs) const
        {
            return boost::iequals(DiskId, rhs.DiskId) &&
                boost::iequals(ReplicationPairId, rhs.ReplicationPairId) &&
                boost::iequals(ReplicationSessionId, rhs.ReplicationSessionId) &&
                boost::iequals(LogFolder, rhs.LogFolder) &&
                boost::iequals(TargetDiskId, rhs.TargetDiskId) &&
                boost::iequals(SyncType, rhs.SyncType) &&
                (TransportSettings == rhs.TransportSettings);
        }
    };


    /// \brief sync settings for AzureToOnPrem
    class AzureToOnPremSyncSettings {

    public:
        std::string     ReplicationPairId;

        std::string     ReplicationSessionId;

        std::string     TargetDiskId;

        AzureBlobBasedTransportSettings     TransportSettings;

        std::string     ResyncProgressType;

        // Resync copy initiated time expressed as the number of 100 - nanosecond
        // intervals that have elapsed since 12:00:00 midnight, January 1, 0001.
        uint64_t        ResyncCopyInitiatedTimeTicks;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToOnPremSyncSettings", false);

            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_E(adapter, TargetDiskId);
            JSON_E(adapter, TransportSettings);
            JSON_E(adapter, ResyncProgressType);
            JSON_T(adapter, ResyncCopyInitiatedTimeTicks);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, ReplicationPairId);
            JSON_P(node, ReplicationSessionId);
            JSON_P(node, TargetDiskId);
            JSON_CL(node, TransportSettings);
            JSON_P(node, ResyncProgressType);
            JSON_P(node, ResyncCopyInitiatedTimeTicks);
        }
    };

    typedef std::vector<SyncSettings>    SyncSettings_t;

    /// \brief Defines initial replication, resync copy progress message for a replication pair.
    /// The replication pair information is sent as part of the message context.
    class OnPremToAzureSyncProgressMsg
    {
    public:
        /// \brief the replication pair ID.
        std::string ReplicationPairId;
            
        /// \brief the replication session ID.
        std::string ReplicationSessionId;
            
        /// \brief the number of blocks synced.
        uint64_t NumberOfBlocksSynced;
            
        /// \brief the total number of blocks.
        uint64_t TotalNumberOfBlocks;
            
        /// \brief ratio of the matched bytes.
        uint32_t MatchedBytesRatio;
            
        /// \brief estimated time remaining for initial replication/resync completion.
        /// in 100-nanosecond units
        uint64_t EstimatedTimeRemaining;

        /// \brief bytes transferred to target Azure blob in last 15 min sliding window.
        uint64_t ResyncLast15MinutesTransferredBytes;
        
        /// \brief time in UTC of last data transferred to target Azure blob.
        std::string ResyncLastDataTransferTimeUtc;

        /// \brief overall processed bytes by resync for the disk size.
        /// For example for a 1GB disk if 256MB is transferred and 256MB is matched then ResyncProcessedBytes is 536870912.
        uint64_t ResyncProcessedBytes;

        /// \brief bytes transferred to target Azure blob by resync for disk.
        uint64_t ResyncTransferredBytes;

        /// \brief time in UTC when resync copy has started.
        std::string ResyncCopyStartTimeUtc;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureSyncProgressMsg", false);

            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_E(adapter, NumberOfBlocksSynced);
            JSON_E(adapter, TotalNumberOfBlocks);
            JSON_E(adapter, MatchedBytesRatio);
            JSON_E(adapter, EstimatedTimeRemaining);
            JSON_E(adapter, ResyncLast15MinutesTransferredBytes);
            JSON_E(adapter, ResyncLastDataTransferTimeUtc);
            JSON_E(adapter, ResyncProcessedBytes);
            JSON_E(adapter, ResyncTransferredBytes);
            JSON_T(adapter, ResyncCopyStartTimeUtc);
        }
     };


    /// \brief RCM resync status strings
    namespace ResyncCompletionStatus {
        const char SUCCESS[] = "Success";
        const char FAILED[] = "Failed";
        const char CANCELLED[] = "Cancelled";
    }

    /// \brief Defines input for initialreplication for CompleteSync API.
    class SourceAgentCompleteSyncInput
    {
    public:
        /// \briefthe replication pair context.
        /// This is the serialized std::string representation of "SourceAgentReplicationPairMsgContext".
        std::string ReplicationContext;
            
        /// \brief the replication pair ID.
        std::string ReplicationPairId;
            
        /// \brief the replication sync session ID.
        std::string ReplicationSessionId;
            
        /// \brief the resync completion status. see ResyncCompletionStatus
        std::string Status;
            
        /// \brief the sync session start time expressed as 100-nanoseconds since Windows epoch.
        uint64_t SyncSessionStartTime;
            
        /// \brief the sync session start sequence ID.
        uint64_t SyncSessionStartSequenceId;
            
        /// \brief the sync session end time expressed as 100-nanoseconds since Windows epoch.
        uint64_t SyncSessionEndTime;
            
        /// \brief the sync session end sequence ID.
        uint64_t SyncSessionEndSequenceId;
            
        /// \brief ratio of the matched bytes.
        uint32_t MatchedBytesRatio;
            
        /// \brief time taken for resync copy job execution in 100-nanosecond units
        uint64_t ElapsedTime;
            
        /// \brief set to true if the sync operation has to be retried
        bool IsFailureRetriable;

        /// \brief errors for resync copy job.
        std::vector<RcmJobError> Errors;

        /// \brief name of the component reporting errors (if any)
        std::string ErrorReportingComponent;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentCompleteSyncInput", false);

            JSON_E(adapter, ReplicationContext);
            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_E(adapter, Status);
            JSON_E(adapter, SyncSessionStartTime);
            JSON_E(adapter, SyncSessionStartSequenceId);
            JSON_E(adapter, SyncSessionEndTime);
            JSON_E(adapter, SyncSessionEndSequenceId);
            JSON_E(adapter, MatchedBytesRatio);
            JSON_E(adapter, ElapsedTime);
            JSON_E(adapter, IsFailureRetriable);
            JSON_E(adapter, Errors);
            JSON_T(adapter, ErrorReportingComponent);
        }
    };
}

class ResyncBatch
{
public:
    std::map<std::string, int> InprogressResyncDiskIDsMap;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "ResyncBatch", false);

        JSON_T(adapter, InprogressResyncDiskIDsMap);
    }
};

#endif