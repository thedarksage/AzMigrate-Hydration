#ifndef RCM_LIB_CONSTANTS_H
#define RCM_LIB_CONSTANTS_H

namespace RcmClientLib {

    /// \brief RCM URI to append to the service URI for source agent
    const std::string RCM_BASE_URI = "rcmagentapi";

    /// \brief the RCM data plance API's URI extentions for the source agnet 
    const std::string REGISTER_MACHINE_URI = "registermachine";
    const std::string MODIFY_MACHINE_URI = "modifymachine";
    const std::string UNREGISTER_MACHINE_URI = "unregistermachine";
    const std::string REGISTER_SOURCE_AGENT_URI = "registersourceagent";

    const std::string UNREGISTER_SOURCE_AGENT_URI = "unregistersourceagent";
    const std::string GET_AGENT_SETTINGS_URI = "getreplicationsettings";
    const std::string GET_AZURE_TO_AZURE_SOURCE_AGENT_SETTINGS_URI = "getazuretoazuresourceagentsettingsv2";

    const std::string GET_ON_PREM_TO_AZURE_SOURCE_AGENT_SETTINGS_URI =
        "GetOnPremToAzureSourceAgentSettings";
    const std::string GET_AZURE_TO_ON_PREM_SOURCE_AGENT_SETTINGS_URI =
        "GetSourceAgentAzureToOnPremSettings";

    const std::string INITIATE_ON_PREM_TO_AZURE_AGENT_AUTO_UPGRADE_URI =
        "InitiateOnPremToAzureAgentAutoUpgrade";
    const std::string POST_AGENT_JOB_STATUS_URI = "updateagentjobstatus";
    const std::string RENEW_LOG_CONTAINER_SAS_URI = "renewlogcontainersasuri";

    const std::string ONPREM_TO_AZURE_COMPLETE_SYNC_URI = "OnPremToAzureCompleteSync";
    const std::string AZURE_TO_ONPREM_COMPLETE_SYNC_URI = "AzureToOnPremCompleteSync";

    const std::string GET_RCM_DETAILS = "GetRcmDetails";
    const std::string TEST_CONNECTION = "TestConnection";

    const std::string CREATE_MOBILITY_AGENT_CONFIG = "CreateMobilityAgentConfig";

    const std::string CREATE_MOBILITY_AGENT_CONFIG_MIGRATION_TO_RCM_STACK = "CreateMobilityAgentConfigForMigrationToRcmStack";
    const std::string CREATE_MOBILITY_AGENT_CONFIG_FOR_ON_PREM_ALR_MACHINE = "CreateMobilityAgentConfigForOnPremAlrMachine";

    const std::string RENEW_LOG_CONTAINER_SAS_URI_AZURE_TO_ONPREM_RESYNC = "AzureToOnPremRenewResyncLogCtrSasUri";
    const std::string RENEW_LOG_CONTAINER_SAS_URI_AZURE_TO_ONPREM_DIFFSYNC = "AzureToOnPremRenewSourceAgentDiffSyncLogCtrSasUri";


    const std::string POST_MONITORING_MESSAGE_URI = "SendMonitoringMessage";
    const std::string POST_DIAGNOSTIC_MESSAGE_URI = "SendDiagnosticLog";

    /// \brief OS arch types
    const char OsArchitectureTypex86[] = "x86";
    const char OsArchitectureTypex64[] = "x64";

    /// \brief Hypervisor types
    const char HypervisorVendorVmware[] = "Vmware";
    const char HypervisorVendorHyperV[] = "HyperV";
    const char PhysicalMachine[] = "Physical";

    const char ContentType[] = "Content-type";
    const char AppJson[] = "application/json";
    const char FormUrlEncoded[] = "application/x-www-form-urlencoded";

    const char X_MS_CLIENT_REQUEST_ID[] = "x-ms-client-request-id";
    const char X_MS_CORRELATION_REQUEST_ID[] = "x-ms-correlation-request-id";
    const char X_MS_AGENT_MACHINE_ID[] = "x-ms-agent-machine-id";
    const char X_MS_AGENT_COMPONENT_ID[] = "x-ms-agent-component-id";
    
    const char X_MS_AGENT_TENANT_ID[] = "x-ms-agent-tenant-id";
    const char X_MS_AGENT_OBJECT_ID[] = "x-ms-agent-object-id";
    const char X_MS_AUTH_TYPE[] = "x-ms-authtype";
    const char X_MS_AGENT_DISK_ID[] = "x-ms-agent-disk-id";

    // RCM Proxy CS Login contract headers
    const char X_MS_AGENT_AUTH_ID[] = "x-ms-agent-auth-id";
    const char X_MS_AGENT_RCM_ACTION[] = "x-ms-agent-rcm-action";
    const char X_MS_AGENT_RCM_ACTION_TYPE[] = "x-ms-agent-rcm-action-type";

    const char X_MS_AGENT_AUTH_NONCE[] = "x-ms-agent-auth-nonce";
    const char X_MS_AGENT_AUTH_ACTION[] = "x-ms-agent-auth-action";
    const char X_MS_AGENT_AUTH_VERSION[] = "x-ms-agent-auth-version";
    const char X_MS_AGENT_AUTH_SIGNATURE[] = "x-ms-agent-auth-signature";
    const char X_MS_AGENT_DO_NOT_USE_CACHED_SETTINGS[] = "x-ms-agent-donotusecachedsettings";

    const char X_MS_AGENT_PROTECTION_SCENARIO[] = "x-ms-agent-protection-scenario";
    const char X_MS_AGENT_FORCERENEWSASURI[] = "x-ms-agent-forcerenewsasuri";

    const char SourceComponent[] = "SourceAgent";
    const char ServiceBusAuth[] = "ServiceBusAuthorization";
    const char AadOAuth2[] = "AadOAuth2";

    const char Auth[] = "Authorization";
    const char HTTPS_PREFIX[] = "https://";
    const char LOCALHOSTURI[] = "localhost";

    const char OAUTH_TYPE2[] = "oauth2";
    const char OAUTH_TOKEN[] = "token";
    const char OAUTH_GRANT_TYPE[] = "grant_type";
    const char OAUTH_CLIENT_CREDENTIALS[] = "client_credentials";
    
    const char OAUTH_CLIENT_ID[] = "client_id";
    const char OAUTH_CLIENT_ASSERTION_TYPE[] = "client_assertion_type";
    const char OAUTH_ASSERTION_JWT_BEARER[] = "urn:ietf:params:oauth:client-assertion-type:jwt-bearer";
    const char OAUTH_CLIENT_ASSERTION[] = "client_assertion";
    const char OAUTH_RESOURCE[] = "resource";

    const char OAUTH_TOKEN_TYPE[] = "token_type";
    const char OAUTH_BEARER_TOKEN[] = "Bearer";
    const char OAUTH_ACCESS_TOKEN[] = "access_token";
    const char OAUTH_ACCESS_TOKEN_EXPIRY_DURATION[] = "expires_in";

    const char JWT_HEADER_TOKEN_TYPE[] = "typ";
    const char JWT_HEADER_ALGORITHM[] = "alg";
    const char JWT_HEADER_X509THUMBPRINT[] = "x5t";

    const char JWT_TOKEN_TYPE[] = "JWT";
    const char ALGORITH_RS256[] = "RS256";

    const char JWT_PAYLOAD_AUDIENCE[] = "aud";
    const char JWT_PAYLOAD_ISSUER[] = "iss";
    const char JWT_PAYLOAD_SUBJECT[] = "sub";
    const char JWT_PAYLOAD_JWTID[] = "jti";
    
    const char JWT_PAYLOAD_NOT_BEFORE[] = "nbf";
    const char JWT_PAYLOAD_EXPIRATION[] = "exp";
    const char JWT_PAYLOAD_ISSUED_AT[] = "iat";

    const char RCM_ACTION_CS_LOGIN[] = "Login";
    const char RCM_ACTION_CALL_RCM_API[] = "CallRcmApi";
    const char RCM_ACTION_POST_MESSAGE[] = "PostMessage";
    const char RCM_ACTION_GET_SETTING[] = "getreplicationsettings";

#ifdef _DEBUG
    const std::string TEST_CONNECTION_URI = "testconnection";
#endif

    const std::string OsTypes[] = { "Unknown", "Windows", "Linux" };
    const std::string OsEndiannessTypes[] = { "Unknown", "LittleEndian", "BigEndian", "MiddleEndian" };

    namespace RcmLibFirmwareTypes {
        const char RCMLIB_FIRMWARE_TYPE_EFI[] = "EFI";
        const char RCMLIB_FIRMWARE_TYPE_BIOS[] = "BIOS";
        const char RCMLIB_FIRMWARE_TYPE_UEFI[] = "UEFI";
    }

    /// \brief consistency types
    namespace ConsistencyBookmarkTypes {
        const char CrashConsistent[] = "CrashConsistent";
        const char SingleVmCrashConsistent[] = "SingleVmCrashConsistent";
        const char SingleVmApplicationConsistent[] = "SingleVmApplicationConsistent";
        const char MultiVmCrashConsistent[] = "MultiVmCrashConsistent";
        const char MultiVmApplicationConsistent[] = "MultiVmApplicationConsistent";
    }

    namespace ServiceConnectionModes {
        const std::string DIRECT = "direct";
        const std::string RELAY = "relay";
    }

    namespace RenewRequestTypes {
		const std::string ResyncContainer = "ResyncContainer";
		const std::string DiffSyncContainer = "DiffSyncContainer";
    }

    namespace NetworkEndPointType
    {
        const std::string PUBLIC_ENDPOINT = "Public";
        const std::string PRIVATE_ENDPOINT = "Private";
        const std::string MIXED_ENDPOINT = "Mixed";
    }

    namespace ResyncProgressType
    {
        const std::string None = "None";
        const std::string InitialReplication = "InitialReplication";
        const std::string AutoResync = "AutoResync";
        const std::string ManualResync = "ManualResync";

        // Define the ResyncTypePriority whenever new resync type is added, 0 being highest.
        // ResyncTypePriority is used in resync batching logic to start resync copy for DiskId
        // which has highest ResyncTypePriority.
        enum ResyncTypePriority
        {
            PriorityManualResyncType = 0,
            PriorityInitialReplicationType = 1,
            PriorityAutoResyncType = 2
        };

        const std::string ResyncTypes[] = {"ManualResync", "InitialReplication", "AutoResync"};
    }

    namespace FailvoerClusterMemberType
    {
        const char IsClusterMember[] = "ClusterMember";
        const char IsClusterVirtualNode[] = "ClusterVirtualNode";
    }
}

#endif
