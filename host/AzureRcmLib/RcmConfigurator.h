#ifndef RCM_CONFIGURATOR_H
#define RCM_CONFIGURATOR_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>

#include "sigslot.h"

#include "RcmClient.h"
#include "localconfigurator.h"
#include "protecteddevicedetails.h"
#include "AgentConfig.h"


namespace RcmClientLib {

    enum RcmSettingsSource {
        SETTINGS_SOURCE_UNINITIALIZED,
        SETTINGS_SOURCE_RCM,
        SETTINGS_SOURCE_LOCAL_CACHE_IF_RCM_NOT_AVAILABLE,
        SETTINGS_SOURCE_LOCAL_CACHE,
    };

    class RcmConfigurator;

    typedef RcmConfigurator * RcmConfiguratorPtr;

    class RcmConfigurator : public LocalConfigurator, public sigslot::has_slots<>
    {

    private:
        static RcmConfiguratorPtr s_instancePtr;

        static boost::mutex s_instanceMutex;

        static std::string s_replicationSettingsPath;

        RcmConfigurator();

        /// \brief Initializes the RCM client based on settings
        ///
        /// \throws exception
        void Init();

        void UpdateRcmDetails();

        bool    m_started;

        RcmSettingsSource       m_settingsSource;

        RcmClientSettingsPtr    m_pRcmClientSettings;

        AgentSettings           m_agentSettings;

        AgentSettingsCacherPtr  m_pAgentSettingsCacher;

        RcmProxySettingsCacherPtr m_pRcmProxySettingsCacherPtr;

        mutable boost::mutex    m_agentSettingMutex;

        RcmClientPtr            m_pRcmClient;

        mutable boost::mutex    m_vacpCertMutex;

        mutable boost::shared_mutex    m_protectionStateMutex;

        ACE_Event *                    m_QuitEvent;

        RcmJobs_t                               m_jobs;

        sigslot::signal1<RcmJobs_t&>            m_jobsChangedSignal;

        sigslot::signal0<>                  m_protectionDisabledSignal;

        sigslot::signal0<>                  m_syncSettingsChangedSignal;

        sigslot::signal0<>                  m_drainSettingsChangedSignal;

        sigslot::signal0<>                  m_tunablePropertiesChangedSignal;


        VxProtectedDeviceDetails_t              m_devDetails;

        VxProtectedDeviceDetailPeristentStore   m_devDetailsPersistentStore;

        SVSTATUS GetCurrentPersistentDetails();

        SVSTATUS UpdateAgentSettingsCache(const std::string& serializedSettings);

        bool IsTimeForAutoUpgrade(const MobilityAgentAutoUpgradeSchedule& autoUpgradeSchedule);

        bool IsAgentUpgradeJobValid(const std::vector<RcmJob>& RcmJobs,
            int autoUpgradeRetryCount);

        SVSTATUS ProcessAgentAutoUpgradeSetting(
            const MobilityAgentAutoUpgradeSchedule& autoUpgradeSchedule,
            const std::vector<RcmJob>& RcmJobs);

        SVSTATUS UpdateOnPremToAzureAgentSettingsCache(const std::string& serializedSettings);

        SVSTATUS UpdateAzureToOnPremAgentSettingsCache(const std::string& serializedSettings);

        SVSTATUS UpdateDataProtectionSyncRcmTargetSettings(
            const std::string& serializedSettings);

        void HandleChangeInAgentSettings(const AgentSettings& currSettings);

        bool VerifyRcmDetails(const RcmDetails& rcmDetails);

        RcmDetails GetRcmDetailsFromRcmInfo();

        SVSTATUS UpdateRcmSettingsFromRcmDetails(const RcmDetails& rcmDetails);

        SVSTATUS UpdateRcmInfoFromRcmDetails(const RcmDetails& rcmDetails);

        SVSTATUS InstallCertifcate(const std::string& certificate);
#ifdef SV_WINDOWS
        SVSTATUS InstallCertificateWindows(const std::string& certificate);

        void LogErrorMessage(DWORD& errorMessage);
#else
        SVSTATUS InstallCertificateLinux(const std::string& certificate);
#endif

    protected:

        static ACE_THR_FUNC_RETURN ThreadFunc(void* pArg);

        ACE_THR_FUNC_RETURN run();

        ACE_Thread_Manager m_threadManager;

    public:

        static RcmConfiguratorPtr getInstance();

        /// \brief initialize RCM client library
        static void Initialize(FileConfiguratorMode mode = FILE_CONFIGURATOR_MODE_VX_AGENT,
            const std::string& configFilePath = std::string());

        /// \brief uninitialize RCM client library
        static void Uninitialize();

        /// \brief enable verbose logging in curl
        static void EnableVerboseLogging();

        /// \brief disable verbose logging in curl
        static void DisableVerboseLogging();

        static std::string GetFqdn();

        ~RcmConfigurator();

        void Start(const RcmSettingsSource source);

        void Stop();

        /// \brief returns shared_ptr to RcmLClient
        RcmClientPtr GetRcmClient() const;

        SVSTATUS PollReplicationSettings();

        SVSTATUS CsLogin(const std::string& ipAddr, const std::string& port, bool bDoNotUsePorxy = false);

        SVSTATUS ParseSourceConfig(const std::string& configFile,
            AgentConfiguration &agentConfig, std::string &serConfig, RCM_CLIENT_STATUS rcmStatus,
            bool validateConfig = true);

        SVSTATUS GetPersistedConfig(AgentConfiguration &persistedConfig,
            std::string &serPersistedConfig, bool validateConfig = true);

        SVSTATUS VerifyVault(const AgentConfiguration &agentConfig,
            const AgentConfiguration &persistedConfig);

        SVSTATUS VerifyClientAuthCommon(const AgentConfiguration &agentConfig,
            const std::string &serConfig);

        SVSTATUS VerifyClientAuth(const std::string& configFile);

        SVSTATUS VerifyClientAuth();

        SVSTATUS GetNonCachedSettings();

        SVSTATUS CompleteAgentRegistration(QuitFunction_t qf = 0, bool bDoNotUseProxy = false, bool update = false);

        SVSTATUS SwitchAppliance(const RcmProxyTransportSetting &rcmProxyTransportSettings);

        SVSTATUS UpdateClientCert();


        SVSTATUS RegisterMachine(QuitFunction_t qf = 0, bool bDoNotUseProxy = false, bool update = false);

        SVSTATUS CheckAccessToAzureServiceUrls(
            const std::string bypasslist = std::string(),
            std::string proxy = std::string());

        template<typename T1, typename T2>
        SVSTATUS ConnectWithProxyOptions(bool bDoNotUsePorxy, T1 arg1, T2 arg2, boost::function<SVSTATUS(T1, T2)> function);

        /// \brief return shared_ptr to RcmClientSettings
        RcmClientSettingsConstPtr GetRcmClientSettings() const;

        /// \brief refresh the RcmClientSettings from persistent store
        SVSTATUS RefreshRcmClientSettings();

        /// \brief returns the sync dataPathTransportSettings for current source VM
        ///
        /// \return
        /// \li SVS_OK on success
        /// \li SVE_FAIL on any failure
        SVSTATUS GetDataPathTransportSettings(std::string& dataTransportType,
            std::string& dataPathTransportSettings) const;

        /// \brief returns the agent settings
        SVSTATUS GetAgentSettings(AgentSettings& agentSettings) const;

        /// \brief returns the sync settings for a given disk-id
        ///
        /// \return
        /// \li SVS_OK on success
        /// \li SVE_FAIL on any failure
        SVSTATUS GetSyncSettings(const std::string& diskid,
            SyncSettings& syncsettings,
            std::string& dataTransportType,
            std::string& dataPathTransportSettings) const;

        /// \brief returns the drain settings
        ///
        /// \return
        /// \li SVS_OK on success
        /// \li SVE_FAIL on any failure
        SVSTATUS GetDrainSettings(
            DrainSettings_t& drainsettings,
            std::string& dataTransportType,
            std::string& dataPathTransportSettings) const;

        /// \brief returns protected deviceIds in the settings
        SVSTATUS GetProtectedDiskIds(std::vector<std::string> &deviceIds) const;

        /// \brief returns consistency settings
        SVSTATUS GetConsistencySettings(ConsistencySettings &consistencySettings) const;

        /// \brief returns the shoebox event settings
        SVSTATUS GetShoeboxEventSettings(ShoeboxEventSettings& settings) const;

        /// \brief returns the monitoring message settings
        SVSTATUS GetMonitoringMessageSettings(AgentMonitoringMsgSettings& settings, const bool& isClusterVirtualNodeContext = false) const;

        /// \brief returns the BIOS id from config file
        std::string GetConfiguredBiosId() const;

        /// \brief returns the Management ID from config file
        std::string GetManagementId() const;

        /// \brief returns the Cluster Manegement ID from config file
        std::string GetClusterManagementId() const;

        /// \brief returns the Client Request ID from config file
        std::string GetClientRequestId() const;

        /// \brief get the RCM service URI
        std::string GetRcmServiceUri() const;

        /// \brief get the AAD URI
        std::string GetAzureADServiceUri() const;

        /// \brief gets the file path of protection state
        std::string GetRcmProtectionStatePath() const;

        /// \brief persist RCM disable protection state
        ///
        /// \throws exception
        void PersistDisableProtectionState();

        /// \brief clear RCM disable protection state
        ///
        /// \throws exception
        void ClearDisableProtectionState();

        /// \brief checks if protection is disabled
        ///
        /// \throws exception
        bool IsProtectionDisabled();

        /// \brief checks if registration is not supported
        ///
        /// \throws exception
        bool IsRegistrationNotSupported();

        /// \brief checks if registration is not allowed
        ///
        /// \throws exception
        bool IsRegistrationNotAllowed();

        /// \brief returns the value of the protection state indicator for a given key
        ///
        /// \throws exception
        std::string GetProtectionStateIndicatorValue(const std::string& indicatorKey);

        /// \brief returns the host-id for which protection is disabled
        ///
        /// \throws exception
        std::string GetProtectionDisabledHostId();

        /// \brief retrives vacp.cert fingerprint if exists, creates otherwise
        ///
        /// \throws exception
        std::string GetVacpCertFingerprint() const;

        /// \brief returns the CURL timeout to be used with RCM/RCM Proxy web requests
        uint32_t GetRcmClientRequestTimeout() const;

        /// \brief returns the HTTP Proxy settings to be used
        ///
        /// \throws exception
        bool GetProxySettings(std::string &proxyAddress, std::string& proxyPort, std::string& bypassList) const;

        /// \brief returns sigslot reference for job changes signal
        sigslot::signal1<RcmJobs_t&>& GetJobsChangedSignal();

        /// \brief returns sigslot reference for protection disabled signal
        sigslot::signal0<>& GetProtectionDisabledSignal();

        /// \brief returns sigslot reference for protection disabled signal
        sigslot::signal0<>& GetSyncSettingsChangedSignal();

        /// \brief returns sigslot reference for protection disabled signal
        sigslot::signal0<>& GetDrainSettingsChangeddSignal();

        /// \brief returns sigslot reference for properties change signal
        sigslot::signal0<>& GetTunablePropertiesChangeddSignal();

        /// \brief clears the replications settings in the cached settings
        SVSTATUS ClearCachedReplicationSettings();

        /// \brief verifies if cloud pairing is complete in V2A. for A2A always returns true
        bool IsCloudPairingComplete();

        // \brief return serialized SourceAgentReplicationPairMsgContext
        SVSTATUS GetReplicationPairMessageContext(std::map<std::string, std::string>& context) const;

        // \brief return serialized SourceAgentReplicationPairMsgContext for a given disk id
        SVSTATUS GetReplicationPairMessageContext(const std::string& diskid, std::string& context) const;

        // \brief return serialized SourceAgentProtectionPairMsgContext
        SVSTATUS GetProtectionPairContext(std::string& context) const;

        // \brief return serialized ClusterVirtualNodeProtectionContext
        SVSTATUS GetClusterVirtualNodeProtectionContext(std::string& context) const;

        // \brief return serialized ClusterProtectionContext
        SVSTATUS GetClusterProtectionContext(std::string& context) const;

        /// \brief complete sync action with provided status
        SVSTATUS CompleteSyncAction(const std::string& status);

        /// \brief returnes truw if the agent is running in V2A RCM OnPrem to Azure Replication
        bool IsOnPremToAzureReplication() const;

        /// \brief returnes truw if the agent is running in V2A RCM Azure to OnPrem Replication
        bool IsAzureToOnPremReplication() const;

        /// \brief return the telemetry folder path on the appliance for OnPremToAzure replication
        std::string GetTelemetryFolderPathForOnPremToAzure() const;

        /// \brief return the telemetry kusto sas uri for AzureToOnPrem replicaiton
        std::string GetTelemetrySasUriForAzureToOnPrem() const;

        /// \brief return the RcmProxy address from the RCMProxy Transport settings
        //  to be used for connecting
        /// throws exception when address is empty
        SVSTATUS GetRcmProxyAddress(const RcmProxyTransportSetting& proxySetting,
            std::vector<std::string>& rcmProxyAddr, uint32_t& port) const;

        /// \brief return the RcmProxy address to be used for connecting
        /// throws exception when address is empty
        SVSTATUS GetRcmProxyAddress(std::vector<std::string>& rcmProxyAddr, uint32_t& port) const;

        /// \brief logs in appliance to fetch agent configuration
        SVSTATUS ApplianceLogin(const RcmRegistratonMachineInfo& machineInfo, std::string& errMsg);

        ///\brief check for agent resource id and try to create new one if not present
        bool UpdateResourceID();

        /// \brief check if the error is machine does not exists
        bool IsRegistrationFailedDueToMachinedoesNotExistError();

        /// \brief Get Source Agent config file path
        std::string GetSourceAgentConfigPath() const;

        /// \brief Get ALR Machine config file path
        std::string GetALRConfigPath();

        /// \brief Fetch and Update Source Config for ALR machine
        SVSTATUS FetchAndUpdateSourceConfig(const ALRMachineConfig& alrMachineConfig);

        /// \brief Perform ALR recovery steps
        SVSTATUS ALRRecoverySteps();

        /// \brief diagnose and fix issues with respect to agent configuration
        SVSTATUS DiagnoseAndFix();

        /// \brief checks if the disk is virtual node disk
        bool IsClusterSharedDiskProtected(const std::string& diskId) const;

		/// \brief checks if Private Endpoint is enabled
        bool IsPrivateEndpointEnabled() const;
		
		/// \brief get the URLs in settings that need access and Root CA cert status check
        SVSTATUS GetUrlsForAccessCheck(std::set<std::string>& urls);

        /// \brief returns the container type for the specified diskid
        SVSTATUS GetContainerType(const std::string& diskid, std::string& containerType);

        /// \brief checks if it is shared disk replication
        bool IsClusterSharedDiskEnabled() const;
    };
}

#endif
