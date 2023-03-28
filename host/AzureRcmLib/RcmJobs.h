/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   RcmJobs.h

Description :   RcmJob class is defind as per the RCM RcmJob class. The member names
should match that defined in the RcmJob class. Uses the ESJ JSON formatter to
erialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/

#ifndef _RCM_JOBS_H
#define _RCM_JOBS_H

#include "json_writer.h"
#include "json_reader.h"
#include "CommitTagBookmark.h"
#include "AgentConfig.h"
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

using boost::property_tree::ptree;

namespace RcmClientLib {

    /// \brief RCM job status strings
    namespace RcmJobStatus {
        const char NONE[] = "None";
        const char SUCCEEDED[] = "Finished";
        const char FAILED[] = "Failed";
        const char CANCELLED[] = "Cancelled";
    }

    namespace RcmJobState {
        const char NONE[] = "None";
        const char QUEUED[] = "Queued";
        const char PROCESSING[] = "Processing";
        const char PROCESSED[] = "Processed";
        const char COMPLETING[] = "Completing";
        const char COMPLETED[] = "Completed";
        const char SKIPPED[] = "Skipped";
    }

    /// \brief RCM job types
    namespace RcmJobTypes {

        // Azure to Azure jobs
        const char START_TRACKING[] = "SourceAgentStartTracking";
        const char STOP_TRACKING[] = "SourceAgentStopFilter";
        const char RESYNC_START_TIMESTAMP[] = "SourceAgentNotifyResyncStart";
        const char RESYNC_END_TIMESTAMP[] = "SourceAgentNotifyResyncEnd";
        const char ISSUE_BASELINE_BOOKMARK[] = "SourceAgentIssueBaselineBookmark";
        const char SHARED_DISK_START_TRACKING[] = "SourceAgentStartSharedDisksChangeTracking";

        // OnPrem to Azure jobs
        const char ON_PREM_TO_AZURE_STOP_TRACKING[] = "OnPremToAzureSourceAgentStopFilter";
        const char ON_PREM_TO_AZURE_PREPARE_FOR_SYNC[] = "OnPremToAzureSourceAgentPrepareForSync";
        const char ON_PREM_TO_AZURE_AGENT_AUTO_UPGRADE[] = "OnPremToAzureSourceAgentAutoUpgrade";
        const char ON_PREM_TO_AZURE_AGENT_USER_TRIGGERED_UPGRADE[] = "OnPremToAzureSourceAgentUserTriggeredUpgrade";
        const char ON_PREM_TO_AZURE_AGENT_PREPARE_FOR_SWITCH_APPLIANCE[] = "OnPremToAzureSourceAgentPrepareForSwitchAppliance";
        const char ON_PREM_TO_AZURE_AGENT_SWITCH_APPLIANCE[] = "OnPremToAzureSourceAgentSwitchAppliance";
        const char ON_PREM_TO_AZURE_SWITCH_BACK_TO_CS_STACK[] = "OnPremToAzureSourceAgentSwitchBackToCsStack";

        // OnPrem to Azure Cs stack start draining 
        const char ON_PREM_TO_AZURE_CS_STACK_MACHINE_START_DRAINING[] = "OnPremToAzureCsStackMachineStartDraining";

        // Azure To OnPrem job types
        const char AZURE_TO_ONPREM_AGENT_AUTO_UPGRADE[] = "AzureToOnPremSourceAgentAutoUpgrade";
        const char AZURE_TO_ONPREM_AGENT_USER_TRIGGERED_AUTO_UPGRADE[] = "AzureToOnPremSourceAgentUserTriggeredUpgrade";
        const char AZURE_TO_ONPREM_PREPARE_FOR_SYNC[] = "AzureToOnPremSourceAgentPrepareForSync";
        const char AZURE_TO_ONPREM_STOP_FILTERING[] = "AzureToOnPremSourceAgentStopFilter";

        const char AZURE_TO_ONPREM_FINAL_BOOKMARK[] = "AzureToOnPremSourceAgentIssueFinalReplicationBookmark";

        //jobs supporting AVS DR
        const char ONPREM_TO_AZUREAVS_FAILOVER_EXTENDED_LOCATION[] = "SourceAgentSetOnPremToAzureAvsFailoverExtendedLocationJob";
        const char  AZUREAVS_TO_ONPREM_FAILOVER_EXTENDED_LOCATION[] = "SourceAgentSetAzureAvsToOnPremFailoverExtendedLocationJob";

        const std::string AzureToAzureSupportedJobs[] =
        {
            RcmJobTypes::START_TRACKING,
            RcmJobTypes::STOP_TRACKING,
            RcmJobTypes::RESYNC_START_TIMESTAMP,
            RcmJobTypes::RESYNC_END_TIMESTAMP,
            RcmJobTypes::ISSUE_BASELINE_BOOKMARK, 
            RcmJobTypes::SHARED_DISK_START_TRACKING
        };

        const std::string OnPremToAzureSupportedJobs[] =
        {
            RcmJobTypes::ON_PREM_TO_AZURE_STOP_TRACKING,
            RcmJobTypes::ON_PREM_TO_AZURE_PREPARE_FOR_SYNC,
            RcmJobTypes::ON_PREM_TO_AZURE_AGENT_AUTO_UPGRADE,
            RcmJobTypes::ON_PREM_TO_AZURE_AGENT_USER_TRIGGERED_UPGRADE,
            RcmJobTypes::ON_PREM_TO_AZURE_AGENT_PREPARE_FOR_SWITCH_APPLIANCE,
            RcmJobTypes::ON_PREM_TO_AZURE_AGENT_SWITCH_APPLIANCE
        };

        const std::string AzureToOnPremSupportedJobs[] =
        {
            RcmJobTypes::AZURE_TO_ONPREM_FINAL_BOOKMARK,
            RcmJobTypes::AZURE_TO_ONPREM_STOP_FILTERING,
            RcmJobTypes::AZURE_TO_ONPREM_PREPARE_FOR_SYNC,
            RcmJobTypes::AZURE_TO_ONPREM_AGENT_AUTO_UPGRADE,
            RcmJobTypes::AZURE_TO_ONPREM_AGENT_USER_TRIGGERED_AUTO_UPGRADE
        };
    }
    /// \brief RCM action types
    namespace RcmActionTypes {
        const char DRAIN_LOGS[] = "DrainLogs";
        const char ON_PREM_TO_AZURE_DRAIN_LOGS[] = "OnPremToAzureDrainLogs";
        const char CONSISTENCY[] = "IssueBookmarks";
        const char ON_PREM_TO_AZURE_CONSISTENCY[] = "OnPremToAzureIssueBookmarks";
        const char ON_PREM_TO_AZURE_SYNC[] = "OnPremToAzureSync";
        const char ON_PREM_TO_AZURE_NO_DATA_TRNSFER_SYNC[] = "OnPremToAzureNoDataTransferSync";

        // AzureToOnPrem action types
        const char AZURE_TO_ONPREM_CONSISTENCY[] = "AzureToOnPremIssueBookmarks";
        const char AZURE_TO_ONPREM_DRAIN_LOGS[] = "AzureToOnPremDrainLogs";
        const char AZURE_TO_ONPREM_SYNC[] = "AzureToOnPremSync";
        const char AZURE_TO_ONPREM_REPROTECT_SYNC_APPLIANCE[] = "ReprotectSync";

    }

    /// \brief RCM error severity 
    namespace RcmJobSeverity {
        const char RCM_ERROR[] = "Error";
        const char RCM_WARNING[] = "Warning";
    }

    class ComponentError
    {
    public:
        std::string ComponentId;
        std::string ErrorCode;
        std::string Message;
        std::string Severity;
        std::string PossibleCauses;
        std::string RecommendedAction;
        std::map<std::string, std::string> MessageParameters;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ComponentError", false);

            JSON_E(adapter, ComponentId);
            JSON_E(adapter, ErrorCode);
            JSON_E(adapter, Message);
            JSON_E(adapter, Severity);
            JSON_E(adapter, PossibleCauses);
            JSON_E(adapter, RecommendedAction);
            JSON_KV_T(adapter, "MessageParameters", MessageParameters);
        }

        /// \brief a serialize function that can handle members in any order
        void serialize(ptree& node)
        {
            JSON_P(node, ComponentId);
            JSON_P(node, ErrorCode);
            JSON_P(node, Message);
            JSON_P(node, Severity);
            JSON_P(node, PossibleCauses);
            JSON_P(node, RecommendedAction);
            JSON_KV_P(node, MessageParameters);
            return;
        }
    };

    namespace AutoUpgradeAgentDownloadLinkType {
        const std::string DownloadCenter = "DownloadCenter";
        const std::string Appliance = "Appliance";
        const std::string StorageAccount = "StorageAccount";
    }

    /// \brief RCM job error as defined in RCM - Agent contract
    class RcmJobError{
    public:
        /// \brief unique code for the error
        std::string ErrorCode;

        /// \brief error message
        std::string Message;

        /// \brief possible causes for the error
        std::string PossibleCauses;

        /// \brief any recommended actions to mitigate the error
        std::string RecommendedAction;

        /// \brief error severity
        std::string Severity;

        /// \brief Component Error
        ComponentError ComponentErrorObj;

        /// \brief Message parameters
        std::map<std::string, std::string> MessageParameters;

        /// \brief agent error message
        std::string AgentErrorMessage;

        /// \brief default constructor
        RcmJobError(){}

        RcmJobError(
            std::string errorCode,
            std::string message,
            std::string possibleCauses,
            std::string recommendedAction,
            std::string severity)
        {
            ErrorCode = errorCode;
            Message = message;
            PossibleCauses = possibleCauses;
            RecommendedAction = recommendedAction;
            Severity = severity;
        }

        RcmJobError(
            std::string errorCode,
            std::string message,
            std::string possibleCauses,
            std::string recommendedAction,
            std::string severity,
            ComponentError componentError,
            std::map<std::string, std::string> messageParams)
        {
            ErrorCode = errorCode;
            Message = message;
            PossibleCauses = possibleCauses;
            RecommendedAction = recommendedAction;
            Severity = severity;
            ComponentErrorObj = componentError;
            MessageParameters = messageParams;
        }

        RcmJobError(
            std::string errorCode,
            std::string message,
            std::string possibleCauses,
            std::string recommendedAction,
            std::string severity,
            std::string agentErrorMessage)
        {
            ErrorCode = errorCode;
            Message = message;
            PossibleCauses = possibleCauses;
            RecommendedAction = recommendedAction;
            Severity = severity;
            AgentErrorMessage = agentErrorMessage;
        }

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmJobError", false);

            JSON_E(adapter, ErrorCode);
            JSON_E(adapter, Message);
            JSON_E(adapter, PossibleCauses);
            JSON_E(adapter, RecommendedAction);
            JSON_E(adapter, Severity);
            JSON_E(adapter, ComponentErrorObj);
            JSON_KV_E(adapter, "MessageParameters", MessageParameters);
            JSON_T(adapter, AgentErrorMessage);
        }

        /// \brief a serialize function that can handle members in any order
        void serialize(ptree& node)
        {
            JSON_P(node, ErrorCode);
            JSON_P(node, Message);
            JSON_P(node, PossibleCauses);
            JSON_P(node, RecommendedAction);
            JSON_P(node, Severity);
            JSON_CL(node, ComponentErrorObj);
            JSON_KV_P(node, MessageParameters);
            JSON_P(node, AgentErrorMessage);
            return;
        }
    };

    /// \brief RCM job context that is retrieved from job and posted back in response
    class RcmJobContext
    {
    public:
        /// \brief Rcm workflow id
        std::string WorkflowId;

        /// \brief Rcm activity id
        std::string ActivityId;

        /// \brief Rcm client reuqest id
        std::string ClientRequestId;

        /// \brief container id
        std::string ContainerId;

        /// \brief resource id
        std::string ResourceId;

        /// \brief Resoure location
        std::string ResourceLocation;

        /// \brief language
        std::string AcceptLanguage;

        /// \brief service activity id
        std::string ServiceActivityId;

        /// \brief srs activity id
        std::string SrsActivityId;

        /// \brief subscription id
        std::string SubscriptionId;


        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "Context", true);

            JSON_E(adapter, WorkflowId);
            JSON_E(adapter, ActivityId);
            JSON_E(adapter, ClientRequestId);
            JSON_E(adapter, ContainerId);
            JSON_E(adapter, ResourceId);
            JSON_E(adapter, ResourceLocation);
            JSON_E(adapter, AcceptLanguage);
            JSON_E(adapter, ServiceActivityId);
            JSON_E(adapter, SrsActivityId);
            JSON_T(adapter, SubscriptionId);
        }

        /// \brief a serialize function that can handle members in any order
        void serialize(ptree& node)
        {
            JSON_P(node, WorkflowId);
            JSON_P(node, ActivityId);
            JSON_P(node, ClientRequestId);
            JSON_P(node, ContainerId);
            JSON_P(node, ResourceId);
            JSON_P(node, ResourceLocation);
            JSON_P(node, AcceptLanguage);
            JSON_P(node, ServiceActivityId);
            JSON_P(node, SrsActivityId);
            JSON_P(node, SubscriptionId);
            return;
        }
    };


    /// \brief RCM job for source agent as defined in contract
    class RcmJob {
    
    public:

        /// \brief a unique Id for the job
        std::string Id;
        
        /// \brief consumer id for the job
        std::string ConsumerId;

        /// \brief component id for the job
        std::string ComponentId;

        /// \brief type of the job
        std::string JobType;

        /// \brief session id
        std::string SessionId;

        /// \brief status of the job
        std::string JobStatus;

        /// \brief RCM service URI
        std::string RcmServiceUri;

        /// \brief serialized string of input data
        std::string InputPayload;

        /// \brief serialized string of output data
        std::string OutputPayload;

        /// \brief errors if any
        std::vector<RcmJobError> Errors;

        /// \brief Rcm job context
        RcmJobContext Context;

        /// \brief Rcm Job State internal to the agent
        std::string JobState;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmJob", false);

            JSON_E(adapter, Id);
            JSON_E(adapter, ConsumerId);
            JSON_E(adapter, ComponentId);
            JSON_E(adapter, JobType);
            JSON_E(adapter, SessionId);
            JSON_E(adapter, JobStatus);
            JSON_E(adapter, RcmServiceUri);
            JSON_E(adapter, InputPayload);
            JSON_E(adapter, OutputPayload);
            JSON_E(adapter, Errors);
            JSON_T(adapter, Context);
        }

        /// \brief a serialize function that can handle members in any order
        void serialize(ptree& node)
        {
            JSON_P(node, Id);
            JSON_P(node, ConsumerId);
            JSON_P(node, ComponentId);
            JSON_P(node, JobType);
            JSON_P(node, SessionId);
            JSON_P(node, JobStatus);
            JSON_P(node, RcmServiceUri);
            JSON_P(node, InputPayload);
            JSON_P(node, OutputPayload);
            JSON_VCL(node, Errors);
            JSON_CL(node, Context);
            return;
        }

        bool operator==(const RcmJob& rhs) const
        {
            return boost::iequals(Id, rhs.Id);
        }
    };

    typedef std::vector<RcmJob> RcmJobs_t;

    /// \brief Resync start notify input
    class ResyncStartNotifyInput {
    public:

        /// \brief source disk ID for which the resync start is issued
        std::string SourceDiskId;

        /// \brief a GUID for the resysn session
        std::string ResyncSessionId;

        /// \brief flag indicating clear differentials with driver
        /// only set when there one target as opposed to 1-N targets
        bool DeleteAccumulatedChanges;

        /// \brief a serializer for the JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ResyncStartNotifyInput", false);

            JSON_E(adapter, SourceDiskId);
            JSON_E(adapter, ResyncSessionId);
            JSON_T(adapter, DeleteAccumulatedChanges);
        }
    };

    /// \brief resync end notify input
    class ResyncEndNotifyInput {
    public:

        /// \brief source disk ID for which the resync end is issued
        std::string SourceDiskId;

        /// \brief a GUID for the resysn session
        std::string ResyncSessionId;

        /// \brief a serializer for the JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ResyncEndNotifyInput", false);

            JSON_E(adapter, SourceDiskId);
            JSON_T(adapter, ResyncSessionId);
        }
    };

    /// \brief resync start notify output the source agent sends to RCM
    class ResyncStartNotifyOutput {
    public:

        /// \brief resync start time as given by driver
        uint64_t ResyncStartTime;

        /// \brief resync start sequence id given by driver
        uint64_t ResyncStartSequenceId;

        /// \brief a serializer for the JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ResyncStartNotifyOutput", false);

            JSON_E(adapter, ResyncStartTime);
            JSON_T(adapter, ResyncStartSequenceId);
        }
    };

    /// \brief resync end notify output the source agent sends to RCM
    class ResyncEndNotifyOutput {
    public:

        /// \brief resync end time as given by driver
        uint64_t ResyncEndTime;

        /// \brief resync end sequence id given by driver
        uint64_t ResyncEndSequenceId;

        /// \brief a serializer for the JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ResyncEndNotifyOutput", false);

            JSON_E(adapter, ResyncEndTime);
            JSON_T(adapter, ResyncEndSequenceId);
        }

        /// \brief a serializer for the JSON
        void serialize(ptree& adapter)
        {
            JSON_P(adapter, ResyncEndTime);
            JSON_P(adapter, ResyncEndSequenceId);
        }
    };

    /// \brief stop filtering job input
    class StopFilteringInput {
    public:

        /// \brief source disk ID for which the stop filtering is issued
        std::string SourceDiskId;

        /// \brief a serializer for the JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "StopFilteringInput", false);

            JSON_T(adapter, SourceDiskId);
        }
    };

    class IssueBaselineBookmarkInput {
    public:

        /// \brief source disk ID for which the baseline bookmark is to be issued
        std::string SourceDiskId;

        /// \brief a book mark for the disk id
        std::string BaselineBookmarkId;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "IssueBaselineBookmarkInput", false);

            JSON_E(adapter, SourceDiskId);
            JSON_T(adapter, BaselineBookmarkId);
        }
    };

    class SourceAgentPrepareForSyncInput {
    public:
        std::string SourceDiskId;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentPrepareForSyncInput", false);

            JSON_T(adapter, SourceDiskId);
        }
    };

    ///  \brief represents input for start change tracking for 
    ///  shared disk
    class SourceAgentStartSharedDisksChangeTrackingInput
    {
    public:

        /// \brief ProtectedSharedDiskIds for start draining
        std::vector<std::string> ProtectedSharedDiskIds;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentStartSharedDisksChangeTrackingInput", false);

            JSON_T(adapter, ProtectedSharedDiskIds);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree& node)
        {
            JSON_VP(node, ProtectedSharedDiskIds);
        }

    };

    class SourceAgentDownloadDlcDetailsInput {
    public:
        std::string DownloadLink;

        std::string Sha256Checksum;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentDownloadDlcDetailsInput", false);

            JSON_E(adapter, DownloadLink);
            JSON_T(adapter, Sha256Checksum);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, DownloadLink);
            JSON_P(node, Sha256Checksum);
        }
    };

    class SourceAgentDownloadApplianceDetailsInput {
    public:
        std::string DownloadFilePathOnAppliance;

        std::string Sha256Checksum;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentDownloadApplianceDetailsInput", false);

            JSON_E(adapter, DownloadFilePathOnAppliance);
            JSON_T(adapter, Sha256Checksum);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, DownloadFilePathOnAppliance);
            JSON_P(node, Sha256Checksum);
        }
    };

    class SourceAgentDownloadSasDetailsInput {
    public:
        std::string DownloadLinkSasUri;

        std::string Sha256Checksum;

        uint64_t SasUriExpirationTime;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentDownloadSasDetailsInput", false);

            JSON_E(adapter, DownloadLinkSasUri);
            JSON_E(adapter, Sha256Checksum);
            JSON_T(adapter, SasUriExpirationTime);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, DownloadLinkSasUri);
            JSON_P(node, Sha256Checksum);
            JSON_P(node, SasUriExpirationTime);
        }
    };

    class SourceAgentUpgradeInput {
    public:
        std::string UpgradeableVersion;

        std::string DownloadLinkType;

        std::string DownloadLinkDetails;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentUpgradeInput", false);

            JSON_E(adapter, UpgradeableVersion);
            JSON_E(adapter, DownloadLinkType);
            JSON_T(adapter, DownloadLinkDetails);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, UpgradeableVersion);
            JSON_P(node, DownloadLinkType);
            JSON_P(node, DownloadLinkDetails);
        }
    };

    class AzureToOnPremSourceAgentIssueFinalReplicationBookmark : public ReplicationBookmarkInput, public ALRMachineConfig {
    public:

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToOnPremSourceAgentIssueFinalReplicationBookmark", false);

            JSON_E(adapter, BookmarkType);
            JSON_E(adapter, BookmarkId);
            JSON_E(adapter, OnPremMachineBiosId);
            JSON_T(adapter, Passphrase);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, BookmarkType);
            JSON_P(node, BookmarkId);
            JSON_P(node, OnPremMachineBiosId);
            JSON_P(node, Passphrase);
        }
    };

    class SourceAgentAzureToOnPremIssueFinalReplicationBookmarkOutput : public ReplicationBookmarkOutput {
    public:

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentAzureToOnPremIssueFinalReplicationBookmarkOutput", false);

            JSON_E(adapter, BookmarkType);
            JSON_E(adapter, BookmarkId);
            JSON_KV_T(adapter, "SourceDiskIdToBookmarkDetailsMap", SourceDiskIdToBookmarkDetailsMap);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, BookmarkType);
            JSON_P(node, BookmarkId);
            JSON_KV_CL(node, SourceDiskIdToBookmarkDetailsMap);
        }

        void operator=(ReplicationBookmarkOutput& rhs)
        {
            BookmarkType = rhs.BookmarkType;
            BookmarkId = rhs.BookmarkId;
            SourceDiskIdToBookmarkDetailsMap = rhs.SourceDiskIdToBookmarkDetailsMap;
        }
    };

    /// \brief SourceAgentSetOnPremToAzureAvsFailoverExtendedLocation Job input
    class SourceAgentFailoverExtendedLocationJobInputBase {
    public:

        /// \brief  BIOS ID of the failover VM
        std::string FailoverVmBiosId;

        /// \brief failover Target Type
        std::string FailoverTargetType;

        /// \brief a serializer for the JSON
        void serialize(JSON::Adapter& adapter)
        {   
            JSON::Class root(adapter, "SourceAgentFailoverExtendedLocationJobInputBase", false);

            JSON_E(adapter, FailoverVmBiosId);
            JSON_E(adapter, FailoverTargetType);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, FailoverVmBiosId);
            JSON_P(node, FailoverTargetType);
        }
    };

    /// \brief SourceAgentSetOnPremToAzureAvsFailoverExtendedLocation Job input
    class SourceAgentSetAzureAvsToOnPremFailoverExtendedLocationJobInput : public SourceAgentFailoverExtendedLocationJobInputBase {
    };

    /// \brief SourceAgentSetOnPremToAzureAvsFailoverExtendedLocation Job input
    class SourceAgentSetOnPremToAzureAvsFailoverExtendedLocationJobInput : public SourceAgentFailoverExtendedLocationJobInputBase {
    };
}
#endif
