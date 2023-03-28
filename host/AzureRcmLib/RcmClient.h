/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	RcmClient.h

Description	:   RcmClient class provides APIs to invoke RCM dataplane APIs for
    RegisterMachine
    RegisterSourceAgent
    UnregisterSourceAgent

+------------------------------------------------------------------------------------+
*/
#ifndef _RCM_CLIENT_H
#define _RCM_CLIENT_H

#include <vector>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

#include "volumegroupsettings.h"
#include "RcmClientSettings.h"
#include "RcmJobs.h"
#include "AgentSettings.h"
#include "RcmClientErrorCodes.h"
#include "MigrationPolicies.h"

namespace RcmClientLib
{

    typedef enum rcmClientImplTypes { RCM_AZURE_IMPL, RCM_PROXY_IMPL} RcmClientImplTypes;

    const char ONPREM_TO_AZUREAVS_FAILOVER_TARGET_TYPE[] = "AzureAvs";
    const char AZUREAVS_TO_ONPREM_FAILOVER_TARGET_TYPE[] = "AvsOnPrem";
    const char RCM_AZURE_CONTROL_PLANE[] = "Rcm";
    const char RCM_ONPREM_CONTROL_PLANE[] = "RcmProxy";

    class RcmClientImpl;

    class RcmConfigurator;

    /// \brief a common class that provides APIs to get/post requests to RCM
    /// \exception throws no exception. caller should use GetErrorMessage() to fetch 
    /// error conditions
    class RcmClient
    {
        /// \brief guard to serialize concurrent register machine calls
        boost::mutex m_registerMachineMutex;

#ifdef WIN32
        /// \brief error code as set by the this class
        static __declspec(thread) RCM_CLIENT_STATUS m_errorCode;
#else
        static __thread RCM_CLIENT_STATUS m_errorCode;
#endif

        /// \brief implementation
        boost::shared_ptr<RcmClientImpl>    m_pimpl;

        RcmClient() {}

        RcmClient(RcmClientImplTypes impl_type);

        SVSTATUS GetInstallerError(RcmJob& job, int exitcode, const std::string& jsonErrorFile);

        RcmJob CreateRcmJobDetails(int exitCode,
            const std::string& jobDetails, const std::string& jsonErrorFile);

        friend class RcmConfigurator;

    public:
        
        ~RcmClient();

        /// \brief registers local machine as a machine entity in RCM
        SVSTATUS RegisterMachine(QuitFunction_t qf = 0, bool update = false);

        /// \brief unregisters local machine with RCM
        SVSTATUS UnregisterMachine();

        /// \brief registers a source agent with RCM
        SVSTATUS RegisterSourceAgent();

        /// \brief unregisters a source agent with RCM
        SVSTATUS UnregisterSourceAgent();

        /// \brief sends a request to RCM to create an autoupgrade job
        SVSTATUS InitiateOnPremToAzureAgentAutoUpgrade();

        /// \brief API to retrieve error code in case of failures
        int32_t GetErrorCode();

        /// \brief fetch jobs from RCM service
        SVSTATUS GetReplicationSettings(std::string& serializedSettings,
            const std::map<std::string, std::string>& headers);

        /// \brief fetch jobs from RCM service
        SVSTATUS GetReplicationSettings(std::string& serializedSettings);

        /// \brief fetch Rcm Details from RCM service
        SVSTATUS GetRcmDetails(std::string& serializedSettings);

        /// \brief post a job's status back to RCM
        SVSTATUS UpdateAgentJobStatus(RcmJob& job);

        /// \brief Complete the auto upgrade job for agent
        SVSTATUS CompleteAgentUpgradeJob(int exitCode, const std::string& jobDetails, const std::string& jsonErrorFile);

        /// \brief renew an Azure blob conatiner SAS URI
        SVSTATUS RenewBlobContainerSasUri(
            const std::string& diskId,
            const std::string& containerType,
            std::string& response,
            const bool& isClusterVirtualNodeContext = false,
            const std::string &clientRequestId = std::string());

        /// \brief does a CS login in RCM Proxy mode
        SVSTATUS CsLogin(const std::string& ipaddr,
                    uint32_t uiPort);

        /// \brief does an Appliance login in RCM Proxy mode for migration
        SVSTATUS ApplianceLogin(const RcmRegistratonMachineInfo& machineInfo, 
            std::string& response) const;
            
        /// \brief does an Appliance login in RCM Proxy mode for ALR
        SVSTATUS ALRApplianceLogin(const ALRMachineConfig& alrMachineConfig,
            std::string& response) const;

        /// \brief Verify client authentication
        SVSTATUS VerifyClientAuth(const RcmProxyTransportSetting &rcmProxyTransportSettings,
            bool notUsed);

        /// \brief fetch Cert Details from RCM service
        SVSTATUS RenewClientCertDetails(std::string& serializedSettings);

        /// \brief API to post completion of IR or resync to RCM Proxy
        SVSTATUS CompleteSync(const std::string& serializedInput);

        /// \brief posts the health, pending changes stats & OMS stats messages to RCM Proxy 
        SVSTATUS PostAgentMessage(const std::string& endPoint,
            const std::string &serializedMessageInput,
            const std::map<std::string, std::string>& propertyMap);
    };

    typedef boost::shared_ptr<RcmClient> RcmClientPtr;
}
#endif
