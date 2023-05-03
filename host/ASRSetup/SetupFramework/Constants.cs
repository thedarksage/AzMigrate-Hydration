namespace ASRSetupFramework
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics.CodeAnalysis;

    /// <summary>
    /// Contains general constants to use in the setup
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1401:FieldsMustBePrivate",
        Justification = "Constants for use in Setup and Registration")]
    public static class UnifiedSetupConstants
    {
        public static bool Is32BitProcess
        {
            get
            {
                return (4 == IntPtr.Size);
            }
        }

        #region Registry Keys

        /// <summary>
        /// InMage product keys.
        /// </summary>
        public static string InMageProductKeys
        {
            get
            {
                return (Is32BitProcess) ? @"SOFTWARE\InMage Systems\Installed Products" :
                    @"SOFTWARE\Wow6432Node\InMage Systems\Installed Products";
            }
        }

        /// <summary>
        /// Platform for which Mobility service is installed.
        /// </summary>
        public const string MachineBackingPlatform = "Platform";

        /// <summary>
        /// InMage registry key to get installation directory.
        /// </summary>
        public const string InMageInstallPathReg = "InstallDirectory";

        /// <summary>
        /// InMage registry to get version.
        /// </summary>
        public const string AgentProductVersion = "Version";

        /// <summary>
        /// Reboot required registry key.
        /// </summary>
        public const string RebootRequiredReg = "Reboot_Required";

        /// <summary>
        /// InMage service config registry key.
        /// </summary>
        public const string InMageDiskFilterServiceSubKey = @"SYSTEM\CurrentControlSet\Services\InDskFlt";

        /// <summary>
        /// Registry path for InMage VX component.
        /// </summary>
        public static string InMageVXRegistryPath
        {
            get
            {
                return InMageProductKeys + @"\5";
            }
        }

        /// <summary>
        /// Last installation action
        /// </summary>
        public static string InMageInstallActionKey = "InstallAction";

        /// <summary>
        /// Post installation actions status.
        /// </summary>
        public static string PostInstallActionsStatusKey = "PostInstallActions";

        /// <summary>
        /// Private endpoint state.
        /// </summary>
        public const string PrivateEndpointState = "PrivateEndpointState";

        #endregion

        /// <summary>
        /// List of inmage agent services.
        /// </summary>
        public static List<string> InMageAgentServices = new List<string>()
        {
            "InMage Scout Application Service",
            "Azure Site Recovery VSS Provider",
            "frsvc",
            "svagents",
            "cxprocessserver"
        };

        /// <summary>
        /// MSI name for MS\MT.
        /// </summary>
        public const string UnifiedAgentMsiName = "UNIFIEDAGENTMSI.MSI";

        /// <summary>
        /// Relative path of CXPS config.
        /// </summary>
        public const string CXPSConfigRelativePath = @"transport\cxps.conf";

        /// <summary>
        /// Relative path of DrScout config.
        /// </summary>
        public const string DrScoutConfigRelativePath = @"Application Data\etc\drscout.conf";

        /// <summary>
        /// Relative path of Source config.
        /// </summary>
        public const string SourceConfigRelativePath = @"Application Data\etc\SourceConfig.json";

        /// <summary>
        /// Relative path of RCM config.
        /// </summary>
        public const string RcmConfRelativePath = @"Microsoft Azure Site Recovery\Config\RCMInfo.conf";

        /// <summary>
        /// Relative path of proxy path.
        /// </summary>
        public const string ProxyConfRelativePath = @"Microsoft Azure Site Recovery\Config\ProxyInfo.conf";

        /// <summary>
        /// Relative path of CXPS config backup.
        /// </summary>
        public const string CXPSConfigBackupRelativePath = @"transport\cxps_backup.conf";

        /// <summary>
        /// Unified agent configurator assembly name.
        /// </summary>
        public const string AgentConfiguratorExeName = "UnifiedAgentConfigurator.exe";

        /// <summary>
        /// cxpsclient exe name.
        /// </summary>
        public const string CxpsclientExeName = "cxpsclient.exe";

        /// <summary>
        /// cxcli exe name.
        /// </summary>
        public const string CxcliExeName = "cxcli.exe";

        /// <summary>
        /// Default installation directory for Agent.
        /// </summary>
        public const string AgentDefaultInstallDir = @"Program Files (x86)\Microsoft Azure Site Recovery\";

        /// <summary>
        /// Suffix to add to agent installation directories.
        /// </summary>
        public const string UnifiedAgentDirectorySuffix = "agent";

        /// <summary>
        /// CS port.
        /// </summary>
        public const string CSPort = "443";

        /// <summary>
        /// Current ASR provider product name.
        /// </summary>
        public const string AzureSiteRecoveryProductName = "Microsoft Azure Site Recovery Provider";

        /// <summary>
        /// MARS agent product name.
        /// </summary>
        public const string MARSAgentProductName = "Microsoft Azure Recovery Services Agent";

        /// <summary>
        /// CspsConfigtool shortcut.
        /// </summary>
        public const string CSPSConfigtoolShortcutName = "Cspsconfigtool.lnk";

        /// <summary>
        /// CspsConfigtool name.
        /// </summary>
        public const string CSPSConfigtoolName = "cspsconfigtool";

        /// <summary>
        /// Non-English UI culture.
        /// </summary>
        public const string NonEnglishUICulture = "NonEnglishUICulture";

        /// <summary>
        /// Non-English OS culture.
        /// </summary>
        public const string NonEnglishOSCulture = "NonEnglishOSCulture";

        /// <summary>
        /// CXTP product name.
        /// </summary>
        public const string CXTPProductName = "Microsoft Azure Site Recovery Configuration/Process Server Dependencies";

        /// <summary>
        /// CS/PS product name.
        /// </summary>
        public const string CSPSProductName = "Microsoft Azure Site Recovery Configuration/Process Server";

        /// <summary>
        /// MS\MT product name.
        /// </summary>
        public const string MS_MTProductName = "Microsoft Azure Site Recovery Mobility Service/Master Target Server";

        /// <summary>
        /// MS\MT product name.
        /// </summary>
        public const string MS_MTProductGUID = "{275197FC-14FD-4560-A5EB-38217F80CBD1}";

        /// <summary>
        /// Unified setup product name.
        /// </summary>
        public const string UnifiedSetupProductName = "Microsoft Azure Site Recovery Unified Setup";

        /// <summary>
        /// Component name of Unified Agent setup logs for telemetry.
        /// </summary>
        public const string UnifiedAgentSetupLogsComponentName = "Microsoft Azure Site Recovery Unified Agent Setup";

        /// <summary>
        /// Component name of Log upload service for telemetry.
        /// </summary>
        public const string LogUploadServiceComponentName = "Microsoft Azure Site Recovery Log Upload Service";

        /// <summary>
        /// MySQL product name.
        /// </summary>
        public const string MySQLProductName = "MySQL";

        /// <summary>
        /// IIS product name.
        /// </summary>
        public const string IisProductName = "Internet Information Services";

        /// <summary>
        /// Unified setup exe name.
        /// </summary>
        public const string UnifiedSetupExeName = "UNIFIEDSETUP.EXE";

        /// <summary>
        /// CXTP exe name.
        /// </summary>
        public const string CXTPExeName = "CX_THIRDPARTY_SETUP.EXE";

        /// <summary>
        /// CS/PS exe name.
        /// </summary>
        public const string CSPSExeName = "UCX_SERVER_SETUP.EXE";

        /// <summary>
        /// MT exe name.
        /// </summary>
        public const string MTExeName = "UNIFIEDAGENT.EXE";

        /// <summary>
        /// UnifiedAgent Installer exe name.
        /// </summary>
        public const string UnifiedAgentInstallerExeName = "UNIFIEDAGENTINSTALLER.EXE";

        /// <summary>
        /// Azure VM agent installer msi name.
        /// </summary>
        public const string AzureVMAgentInstallerMsiName = "WINDOWSAZUREVMAGENT.MSI";

        /// <summary>
        /// Configurator exe name.
        /// </summary>
        public const string ConfiguratorExeName = "UnifiedAgentConfigurator.exe";

        /// <summary>
        /// DRA exe name.
        /// </summary>
        public const string DRAExeName = "AzureSiteRecoveryProvider.exe";

        /// <summary>
        /// SETUPDR exe name.
        /// </summary>
        public const string SETUPDRExeName = "SETUPDR.EXE";

        /// Name of the executable used to register/unregister with RCM service.
        /// </summary>
        public const string AzureRCMCliExeName = "AzureRcmCli.exe";

        /// <summary>
        /// Protocol to connect to Configuration Server.
        /// </summary>
        public const string Get_AcsUrl_API = "GetCsRegistry";

        /// <summary>
        /// API name to get CxPs SSL port.
        /// </summary>
        public const string Get_CxPsConfiguration_API = "GetCxPsConfiguration";

        /// <summary>
        /// Configuration Server communination port.
        /// </summary>
        public const int csSecurePort = 443;

        /// <summary>
        /// Protocol to connect to Configuration Server.
        /// </summary>
        public const string protocol = "https";

        /// <summary>
        /// InMage Profiler GUID
        /// </summary>
        public const string InMageProfilerhostGUID = "5C1DAEF0-9386-44a5-B92A-31FE2C79236A";

        /// <summary>
        /// Azure Site Recovery adapter msi file name.
        public const string AsrAdapterMsiName = "ASRAdapter.msi";

        /// <summary>
        /// Mars agent installer binary name.
        /// </summary>
        public const string MarsAgentExeName = "MARSAgentInstaller.exe";

        /// <summary>
        /// Set Mars agent proxy binary name.
        /// </summary>
        public const string SetMarsProxyExeName = "SetMarsProxy.exe";

        /// <summary>
        /// Configuration Manager MSI name.
        /// </summary>
        public const string ConfigManagerMsiName = "AzureSiteRecoveryConfigurationManager.msi";

        /// <summary>
        /// Configuration Manager product name.
        /// </summary>
        public const string ConfigManagerProductName = "Azure Site Recovery Configuration Manager";

        /// <summary>
        /// Configuration Manager installation status log.
        /// </summary>
        public const string ConfigManagerMsiLog = "ASRConfigurationManager.log";

        /// <summary>
        /// Configuration Manager installation status log.
        /// </summary>
        public const string ConfigManagerStatusLog = @"ProgramData\ASRSetupLogs\ASRConfigurationManager.log";

        /// <summary>
        /// OVF registry.
        /// </summary>
        public const string OVFRegKey = @"SOFTWARE\Microsoft\AzureSiteRecoveryOVF";

        /// <summary>
        /// Installed products registry.
        /// </summary>
        public static string InstalledProductsRegName
        {
            get
            {
                return (Is32BitProcess) ? @"HKEY_LOCAL_MACHINE\SOFTWARE\InMage Systems\Installed Products" :
                                            @"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\InMage Systems\Installed Products";
            }
        }

        /// <summary>
        /// Root key in HKLM for Microsoft products with tag LocalMachine
        /// </summary>
        public const string RootRegKeyLMHive = @"HKEY_LOCAL_MACHINE\";

        /// <summary>
        /// Root key in HKLM for Microsoft products.
        /// </summary>
        public static string RootRegKey
        {
            get
            {
                return (Is32BitProcess) ? @"SOFTWARE\" : @"SOFTWARE\Wow6432Node\";
            }
        }

        /// <summary>
        /// Proxy registry.
        /// </summary>
        public const string ProxyRegistryHive = RootRegKeyLMHive + @"SOFTWARE\Microsoft\Azure Site Recovery\ProxySettings";

        /// <summary>
        /// Registration registry.
        /// </summary>
        public const string RegistrationRegistryHive = RootRegKeyLMHive + @"SOFTWARE\Microsoft\Azure Site Recovery\Registration";

        /// <summary>
        /// SRS URL registry key.
        /// </summary>
        public const string SRSUrlRegKeyName = "SRSUri";

        /// <summary>
        /// AAD URL registry key.
        /// </summary>
        public const string AADUrlRegKeyName = "AADAuthority";

        /// <summary>
        /// ID Management URL registry key.
        /// </summary>
        public const string IdMgmtUrlRegKeyName = "IdmgmtSVCEndpoint";

        /// <summary>
        /// Protection Service URL registry key.
        /// </summary>
        public const string ProtSvcUrlRegKeyName = "ProtectionServiceSVCEndpoint";

        /// <summary>
        /// Telemetry URL registry key.
        /// </summary>
        public const string TelemetryUrlRegKeyName = "TelemetryServiceSVCEndpoint";

        /// <summary>
        /// Temp path.
        /// </summary>
        public const string TempPath = @"Temp\ASRSetup";

        /// <summary>
        /// Root hive for Inmage registry with local machine hive key.
        /// </summary>
        public static string InmageRegistryLMHive
        {
            get
            {
                return RootRegKeyLMHive + RootRegKey + "InMage Systems";
            }
        }

        /// <summary>
        /// Root hive for Inmage registry.
        /// </summary>
        public static string InmageRegistryHive
        {
            get
            {
                return RootRegKey + "InMage Systems";
            }
        }

        /// <summary>
        /// Configuration/process server dependencies registry.
        /// </summary>
        public static string CXTPRegistryName
        {
            get
            {
                return InmageRegistryLMHive + @"\Installed Products\10";
            }
        }

        /// <summary>
        /// Configuration/process server registry.
        /// </summary>
        public static string CSPSRegistryName
        {
            get
            {
                return InmageRegistryLMHive + @"\Installed Products\9";
            }
        }

        /// <summary>
        /// Agent registry.
        /// </summary>
        public static string AgentRegistryName
        {
            get
            {
                return InmageRegistryLMHive + @"\Installed Products\5";
            }
        }

        /// <summary>
        /// Local machine Configuration/process server registry.
        /// </summary>
        public static string LocalMachineCSPSRegName
        {
            get
            {
                return InmageRegistryLMHive + @"\Installed Products\9";
            }
        }

        /// <summary>
        /// Local machine Master target registry.
        /// </summary>
        public static string LocalMachineMSMTRegName
        {
            get
            {
                return InmageRegistryHive + @"\Installed Products\5";
            }
        }

        /// <summary>
        /// .NET Framework registry key.
        /// </summary>
        public const string NETFrameworkRegkey = @"SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full\";

        /// <summary>
        /// Upper filter registry hive.
        /// </summary>
        public const string UpperFilterRegHive = @"System\CurrentControlSet\Control\Class\{4d36e967-e325-11ce-bfc1-08002be10318}";

        /// <summary>
        /// Inmage MT registry hive.
        /// </summary>
        public const string InmageMTregHive = @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Azure Backup\Config\InMageMT";

        /// <summary>
        /// Upper filter registry key.
        /// </summary>
        public const string UpperFilterRegKey = "UpperFilters";

        /// <summary>
        /// InDskFlt name.
        /// </summary>
        public const string InDskFltName = "InDskFlt";

        /// <summary>
        /// InVolFlt name.
        /// </summary>
        public const string InVolFltName = "InVolFlt";

        /// <summary>
        /// Installation directory registry key.
        /// </summary>
        public const string InstDirRegKeyName = @"InstallDirectory";

        /// <summary>
        /// Vault geo registry key.
        /// </summary>
        public const string VaultGeoRegKeyName = "Geo_Name";

        /// <summary>
        /// Proxy type registry key.
        /// </summary>
        public const string ProxyRegKeyName = "Proxy_Type";

        /// <summary>
        /// Cdpcli registerhost status registry key.
        /// </summary>
        public const string CdpcliRegStatusRegKeyName = "Cdpcli_RegisterHost_Status";

        /// <summary>
        /// DRA installation status registry key.
        /// </summary>
        public const string DRAInstStatusRegKeyName = "DRA_Install_Status";

        /// <summary>
        /// DRA registration status registry key.
        /// </summary>
        public const string DRARegStatusRegKeyName = "DRA_Registration_Status";

        /// <summary>
        /// DRA service status registry key.
        /// </summary>
        public const string DRAServiceStatusRegKeyName = "DRA_Service_Status";

        /// <summary>
        /// MARS installation status registry key.
        /// </summary>
        public const string MARSInstStatusRegKeyName = @"MARS_Install_Status";

        /// <summary>
        /// MARS service status registry key.
        /// </summary>
        public const string MARSServiceStatusRegKeyName = @"MARS_Service_Status";

        /// <summary>
        /// MARS proxy status registry key.
        /// </summary>
        public const string MARSProxyStatusRegKeyName = @"MARS_Proxy_Status";

        /// <summary>
        /// MT registration status registry key.
        /// </summary>
        public const string RegisterMTStatusRegKeyName = @"Cdpcli_RegisterMT_Status";

        /// <summary>
        /// Container registration status registry key.
        /// </summary>
        public const string ContainerRegStatusRegKeyName = @"Container_Registration_Status";

        /// <summary>
        /// IIS installation status registry key.
        /// </summary>
        public const string IISInstallationStatusRegKeyName = @"IIS_Installation_Status";

        /// <summary>
        /// Reboot required registry key.
        /// </summary>
        public const string RebootRequiredRegKeyName = @"Reboot_Required";

        /// <summary>
        /// Build Version registry key.
        /// </summary>
        public const string BuildVersionRegKeyName = @"Build_Version";

        /// <summary>
        /// Machine Identifier registry key.
        /// </summary>
        public const string MachineIdentifierRegKeyName = @"MachineIdentifier";

        /// <summary>
        /// Cert issuer name.
        /// </summary>
        public const string CertIssuerName = "scout";

        /// <summary>
        /// Relative path of Pushinstaller config.
        /// </summary>
        public const string PushInstallConfigRelativePath = @"pushinstallsvc\pushinstaller.conf";

        /// <summary>
        /// MySQL file name.
        /// </summary>
        public const string MySQLFileName = @"Temp\ASRSetup\mysql-installer-community-5.7.20.0.msi";

        /// <summary>
        /// MySQL installer path.
        /// </summary>
        public const string MySQLMsiPath = @"ProgramData\MySQL\MySQL Installer for Windows\Product Cache\mysql-5.7.20-win32.msi";

        /// <summary>
        /// Root hive for MySQL registry with local machine hive key.
        /// </summary>
        public static string MySQLRegistryLMHive
        {
            get
            {
                return RootRegKeyLMHive + RootRegKey + @"MySQL AB\MySQL Server 5.7";
            }
        }

        // <summary>
        /// Root hive for SV Systems registry.
        /// </summary>
        public static string SvSystemsRegistryHive
        {
            get
            {
                return RootRegKey + "SV Systems";
            }
        }

        // <summary>
        /// HostId registry key.
        /// </summary>
        public const string HostIdRegKeyName = @"HostId";

        // <summary>
        /// System UUID registry key.
        /// </summary>
        public const string SystemUuidRegKeyName = @"UUID";

        // <summary>
        /// Servername registry key.
        /// </summary>
        public const string ServernameRegKeyName = @"ServerName";

        /// <summary>
        /// Configuration type entry in amethyst.conf.
        /// </summary>
        public const string ConfigurationType = @"CX_TYPE";

        /// <summary>
        /// Host Guid entry in amethyst.conf.
        /// </summary>
        public const string AmethystConfigHostGuid = @"HOST_GUID";

        /// <summary>
        /// Amethyst.conf file path.
        /// </summary>
        public const string AmethystFile = @"etc\amethyst.conf";

        /// <summary>
        /// MSI\MSP logs from msiexec.
        /// </summary>
        public const string UnifiedAgentMSILog = "UnifiedAgentMSIInstall.log";

        /// <summary>
        /// App.conf file path.
        /// </summary>
        public const string AppConfFile = @"ProgramData\Microsoft Azure Site Recovery\Config\App.conf";

        /// <summary>
        /// INSTALLATION_PATH value in App.conf file.
        /// </summary>
        public const string InstPath_AppConf = @"INSTALLATION_PATH";

        /// <summary>
        /// PS_CS_IP value in Amethyst.conf file.
        /// </summary>
        public const string PSCSIP_AmethystConf = @"PS_CS_IP";

        /// <summary>
        /// Folder to extract DRA.
        /// </summary>
        public const string ExtractDRAFolder = @"Temp\DRASETUP";

        /// MySQL link
        /// </summary>
        public const string MySQLLink = @"https://dev.mysql.com/get/Downloads/MySQLInstaller/";

        /// MySQL link
        /// </summary>
        public const string MySQL55Link = @"https://dev.mysql.com/get/archives/mysql-5.5/";

        /// <summary>
        /// MySQL download link
        /// </summary>
        public const string MySQLDownloadLink = @"https://cdn.mysql.com";

        /// <summary>
        /// MARS agent Powershell module.
        /// </summary>
        public const string MarsAgentPSModule = "MSOnlineBackup";

        /// <summary>
        /// MySQL 5.5 version.
        /// </summary>
        public const string MySQL55Version = "5.5.37";

        /// <summary>
        /// MySQL 5.7 version.
        /// </summary>
        public const string MySQL57Version = "5.7.20";

        /// <summary>
        /// Free space in GB.
        /// </summary>
        public const int RequiredFreeSpaceInGB = 2;

        /// <summary>
        /// Msiexec success exit code.
        /// </summary>
        public const int Success = 0;

        /// <summary>
        /// Msiexec success with reboot required exit code.
        /// </summary>
        public const int SuccessWithRebootRequired = 3010;

        /// <summary>
        /// Msiexec uninstall failure exit code.
        /// </summary>
        public const int FailureUninstallInstalledProduct = 1638;

        /// <summary>
        /// Msiexec invalid version number exit code.
        /// </summary>
        public const int InvalidVersionNumber = 1642;

        /// <summary>
        /// CSPS default IP in drscout config.
        /// </summary>
        public const string CSPSDefaultIPInDrscout = "0.0.0.0";

        /// <summary>
        /// DRConfigurator exe path.
        /// </summary>
        public const string DRConfiguratorExePath = @"Program Files\Microsoft Azure Site Recovery Provider\DRConfigurator.exe";

        /// <summary>
        /// DRConfigurator export location.
        /// </summary>
        public const string ExportPath = @"ProgramData\Microsoft Azure Site Recovery\private";

        /// <summary>
        /// Passphrase file path.
        /// </summary>
        public const string PassphrasePath = @"ProgramData\Microsoft Azure Site Recovery\private\connection.passphrase";

        /// <summary>
        /// Cdpcli exe path.
        /// </summary>
        public const string CdpcliExePath = @"agent\cdpcli.exe";

        /// <summary>
        /// Cspsconfigtool exe path.
        /// </summary>
        public const string CspsconfigtoolExePath = @"bin\cspsconfigtool.exe";

        /// <summary>
        /// MARS exe path.
        /// </summary>
        public const string MARSExePath = @"agent\MARSAgentInstaller.exe";

        /// <summary>
        /// Success status string.
        /// </summary>
        public const string SuccessStatus = "Succeeded";

        /// <summary>
        /// Failed status string.
        /// </summary>
        public const string FailedStatus = "Failed";

        /// <summary>
        /// Bypass proxy string.
        /// </summary>
        public const string BypassProxy = "Bypass";

        /// <summary>
        /// Custom proxy string.
        /// </summary>
        public const string CustomProxy = "Custom";

        /// <summary>
        /// Fresh installation string.
        /// </summary>
        public const string FreshInstall = "Fresh";

        /// <summary>
        /// Upgrade string.
        /// </summary>
        public const string Upgrade = "Upgrade";

        /// <summary>
        /// Configuration server mode string.
        /// </summary>
        public const string CSServerMode = "CS";

        /// <summary>
        /// Process server mode string.
        /// </summary>
        public const string PSServerMode = "PS";

        /// <summary>
        /// Yes string.
        /// </summary>
        public const string Yes = "Yes";

        /// <summary>
        /// No string.
        /// </summary>
        public const string No = "No";

        /// <summary>
        /// Name of the C drive.
        /// </summary>
        public const string CDrive = @"C:\";

        /// <summary>
        /// Server manager PS module name.
        /// </summary>
        public const string ServerManagerPowershellModule = "ServerManager";

        /// <summary>
        /// MARS service name.
        /// </summary>
        public const string MARSServiceName = "obengine";

        /// <summary>
        /// Frsvc service name.
        /// </summary>
        public const string FrsvcServiceName = "frsvc";

        /// <summary>
        /// Setup logs upload folder.
        /// </summary>
        public const string LogUploadFolder = @"ASRSetupLogs\UploadedLogs";

        /// <summary>
        /// ASR logging folder name.
        /// </summary>
        public const string LogFolder = @"ASRSetupLogs";

        /// <summary>
        /// DRA logging folder name.
        /// </summary>
        public const string DRALogFolder = @"ProgramData\ASRLogs\";

        /// <summary>
        /// Unified setup logging folder name.
        /// </summary>
        public const string UnifiedSetupLogFolder = @"ProgramData\ASRSetupLogs\";

        /// <summary>
        /// ASR configuration upgrade log.
        /// </summary>
        public const string ASRConfUpgradeLog = @"ProgramData\ASRSetupLogs\ASRConfigurationUpgrade.log";

        /// <summary>
        /// MySQL download log.
        /// </summary>
        public const string MySQLDownloadLog = @"ProgramData\ASRSetupLogs\MySQLDownload.log";

        /// <summary>
        /// CXTP install log.
        /// </summary>
        public const string CXTPInstallLog = @"ProgramData\ASRSetupLogs\CX_TP_InstallLogFile.log";

        /// <summary>
        /// CX install log.
        /// </summary>
        public const string CXInstallLog = @"ProgramData\ASRSetupLogs\CX_InstallLogFile.log";

        /// <summary>
        /// ASR Unified setup log.
        /// </summary>
        public const string ASRUnifiedSetupLog = @"ProgramData\ASRSetupLogs\ASRUnifiedSetup.log";

        /// <summary>
        /// DRA setup log.
        /// </summary>
        public const string DRASetupLog = @"ProgramData\ASRLogs\DRASetupWizard.log";

        /// <summary>
        /// MARS installation status log.
        /// </summary>
        public const string MARSStatusLog = @"ProgramData\ASRSetupLogs\MARSStatus.log";

        /// <summary>
        /// Unified agent install log.
        /// </summary>
        public const string ASRUnifiedAgentInstallLog = @"ProgramData\ASRSetupLogs\ASRUnifiedAgentInstaller.log";

        /// <summary>
        /// Unified agent configurator log.
        /// </summary>
        public const string ASRUnifiedAgentConfiguratorLog = @"ProgramData\ASRSetupLogs\ASRUnifiedAgentConfigurator.log";

        /// <summary>
        /// MSI log absolute path.
        /// </summary>
        public const string ASRUnifiedAgentMSILogFile = @"ProgramData\ASRSetupLogs\UnifiedAgentMSIInstall.log";

        /// <summary>
        /// Proxy settings file name.
        /// </summary>
        public const string ProxySettingsFile = @"Temp\ASRSetup\ProxySettings.txt";

        /// <summary>
        /// Passphrase file name.
        /// </summary>
        public const string PassphraseFile = @"Temp\ASRSetup\Passphrase.txt";

        /// <summary>
        /// Prerequisites status file name.
        /// </summary>
        public const string PrerequisiteStatusFileName = "PrereqStatusFile.txt";

        /// <summary>
        /// MySQL credentials file name.
        /// </summary>
        public const string MySQLCredentialsFile = @"Temp\ASRSetup\MySQLCredentialsFile.txt";

        /// <summary>
        /// Setup wizard log filename.
        /// </summary>
        public const string UnifiedSetupWizardLogFileName = "ASRUnifiedSetup.log";

        /// <summary>
        /// Integrity check log filename.
        /// </summary>
        public const string IntegrityCheckFileName = "IntegrityCheck.log";

        /// <summary>
        /// Unified Agent installer log filename.
        /// </summary>
        public const string UnifiedAgentInstallerLogFileName = "ASRUnifiedAgentInstaller.log";

        /// <summary>
        /// Unified Agent configurator log filename.
        /// </summary>
        public const string UnifiedAgentConfiguratorLogFileName = "ASRUnifiedAgentConfigurator.log";

        /// <summary>
        /// Unified Setup Patch log filename.
        /// </summary>
        public const string UnifiedSetupPatchLogFileName = "ASRUnifiedSetupPatch_9.0.1.0_GA.log";

        /// <summary>
        /// Summary status display filename.
        /// </summary>
        public const string SummaryStatusFileName = "ASRConfiguration.log";

        /// <summary>
        /// Setup wizard log filename.
        /// </summary>
        public const string SetupWizardLogFileNameFormat = "DRASetupWizard_{0}.log";

        /// <summary>
        /// Unified Setup install log.
        /// </summary>
        public const string ASRUnifiedSetupInstallLog = @"ProgramData\ASRSetupLogs\ASRUnifiedSetup.log";

        /// <summary>
        /// World Wide Web service install log path.
        /// </summary>
        public const string WorldWideWebServiceLogPath = @"Windows\System32\LogFiles\HTTPERR";

        /// <summary>
        /// World Wide Web service install log.
        /// </summary>
        public const string WorldWideWebServiceLog = WorldWideWebServiceLogPath + @"\{0}";

        /// <summary>
        /// Tmansvc service log.
        /// </summary>
        public const string TmansvcServiceLog = @"var\tmansvc.log";

        /// <summary>
        /// Push Install service log.
        /// </summary>
        public const string PushInstallServiceLog = @"pushinstallsvc\pushinstallservice.log";

        /// <summary>
        /// Push Install conf file.
        /// </summary>
        public const string PushInstallConfFile = @"pushinstallsvc\pushinstaller.conf";

        /// <summary>
        /// Cx ProcessServer service log.
        /// </summary>
        public const string CxPsErrorServiceLog = @"transport\log\cxps.err.log";

        /// <summary>
        /// Cx ProcessServer service log.
        /// </summary>
        public const string CxPsMonitorServiceLog = @"transport\log\cxps.monitor.log";

        /// <summary>
        /// Cx ProcessServer service log.
        /// </summary>
        public const string CxPsXferServiceLog = @"transport\log\cxps.xfer.log";

        /// <summary>
        /// Azure Recovery Services Agent MSI install log.
        /// </summary>
        public const string MARSMsiLog = @"Windows\Temp\OBMsi.log";

        /// <summary>
        /// Azure Recovery Services Agent Patch install log.
        /// </summary>
        public const string MARSPatchLog = @"Windows\Temp\OBPatch.log";

        /// <summary>
        /// Azure Recovery Services Agent install log.
        /// </summary>
        public const string MARSInstallLog = @"Windows\Temp\OBInstaller0Curr.errlog";

        /// <summary>
        /// Azure Recovery Services Agent managed install log.
        /// </summary>
        public const string MARSManagedInstallLog = @"Windows\Temp\OBManagedlog.LOGCurr.errlog";

        /// <summary>
        /// Azure Recovery Services Agent service log.
        /// </summary>
        public const string MARSServiceLog = @"Program Files\Microsoft Azure Recovery Services Agent\Temp\CBEngineCurr.errlog";

        /// <summary>
        /// DB upgrade log.
        /// </summary>
        public const string DBUpgradeLog = @"var\db_upgrade.log";

        /// <summary>
        /// DB upgrade error log.
        /// </summary>
        public const string DBUpgradeErrorLog = "upgrade_error.log";

        /// <summary>
        /// Upload folder in CSPS machine for Agent setup Logs.
        /// </summary>
        public const string AgentSetupLogsUploadFolder = @"AgentSetupLogs";

        /// <summary>
        /// Agent invoked by Push.
        /// </summary>
        public const string PushInstall = @"PushInstall";

        /// <summary>
        /// Agent invoked by VmWare.
        /// </summary>
        public const string VmWareInstall = @"VmwareInstall";

        /// <summary>
        /// Agent invoked by Command-line.
        /// </summary>
        public const string CommandLineInstall = @"CommandLineInstall";

        /// <summary>
        /// Agent configuration status registry key.
        /// </summary>
        public const string AgentConfigurationStatus = "AgentConfigurationStatus";

        /// <summary>
        /// Azure site recovery windows service name.
        /// </summary>
        public const string DRServiceName = "dra";

        /// <summary>
        /// Microsoft home site (used for Internet connectivity check).
        /// </summary>
        public const string MicrosoftNCSIDotCom = "http://www.msftncsi.com/ncsi.txt";

        /// <summary>
        /// Environment value for test.
        /// </summary>
        public const string TestEnvironment = "TEST";

        /// <summary>
        /// Environment settings.
        /// </summary>
        public const string Environment = "Environment";

        /// <summary>
        /// Azure VM Agent Installed key.
        /// </summary>
        public const string AzureVMAgentInstalled = "AzureVMAgentInstalled";

        /// <summary>
        /// World Wide Web Publishing service name.
        /// </summary>
        public const string WorldWideWebServiceName = "W3SVC";

        /// <summary>
        /// Volsync Thread Manager service name.
        /// </summary>
        public const string TmansvcServiceName = "tmansvc";

        /// <summary>
        /// PushInstaller service name.
        /// </summary>
        public const string PushInstallServiceName = "InMage PushInstall";

        /// <summary>
        /// CX Process Server service name.
        /// </summary>
        public const string CxPsServiceName = "cxprocessserver";

        /// <summary>
        /// Microsoft Visual C++ 2015 Redistributable (x86) name.
        /// </summary>
        public const string MSVC2015Redistributable = "Microsoft Visual C++ 2015 Redistributable (x86)";

        /// <summary>
        /// Microsoft Visual C++ 2013 Redistributable (x86) name.
        /// </summary>
        public const string MSVC2013Redistributable = "Microsoft Visual C++ 2013 Redistributable (x86)";

        /// <summary>
        /// Param to skip Microsoft Visual C++ 2015 Redistributable (x86) installation.
        /// </summary>
        public const string SkipMSVC2015Installation = " /SkipMSVC2015Installation";

        /// <summary>
        /// Param to skip Microsoft Visual C++ 2013 Redistributable (x86) installation.
        /// </summary>
        public const string SkipMSVC2013Installation = " /SkipMSVC2013Installation";

        /// <summary>
        /// Enum for defining if proxy settings need to be cleared or written to registry
        /// </summary>
        public enum ProxyChangeAction
        {
            /// <summary>
            /// Bypass existing system proxy
            /// </summary>
            Bypass,

            /// <summary>
            /// Write new proxy settings
            /// </summary>
            Write,

            /// <summary>
            /// Clear existing proxy settings
            /// </summary>
            Clear,

            /// <summary>
            /// No change in proxy settings
            /// </summary>
            NoChange
        }

        /// <summary>
        /// List of Azure Guest Agent services.
        /// </summary>
        public static List<string> AzureGuestAgentServices = new List<string>()
        {
            "WindowsAzureGuestAgent",
            "WindowsAzureTelemetryService",
            "RdAgent"
        };

        /// <summary>
        /// ProductVersion property name.
        /// </summary>
        public const string ProductVersionProperty = "ProductVersion";

        /// <summary>
        /// Summary file name
        /// </summary>
        public const string SummaryFileNameFormat = "InstallationSummary-{0}.json";

        /// <summary>
        /// Json Files.
        /// </summary>
        public const string JsonFiles = "*.json";

        /// <summary>
        /// Log Files.
        /// </summary>
        public const string LogFiles = "*.log";

        /// <summary>
        /// Absolute path of Upload folder in CSPS machine for Agent Setup Logs .
        /// </summary>
        public const string AgentSetupLogsAbsolutePath = @"/home/svsystems/AgentSetupLogs";

        /// <summary>
        /// Command-line Agent setup logs upload folder.
        /// </summary>
        public const string CommandLineAgentSetupLogsAbsolutePath = AgentSetupLogsAbsolutePath + @"/CommandLineInstall";

        /// <summary>
        /// Absolute path of Upload folder in CSPS machine for Scale out unit Logs .
        /// </summary>
        public const string ScaleOutUnitLogsAbsolutePath = @"/home/svsystems/ScaleOutUnitLogs";

        /// <summary>
        /// Scale out unit Logs folder.
        /// </summary>
        public const string ScaleOutUnitLogsFolderName = @"ScaleOutUnitLogs";

        /// <summary>
        /// Windows OS name
        /// </summary>
        public const string WindowsOs = "Windows";

        /// <summary>
        /// Log upload service log file path.
        /// </summary>
        public const string LogUploadServiceLogPath = @"/ProgramData/LogUploadServiceLogs";

        /// <summary>
        /// Log upload service log file name.
        /// </summary>
        public const string LogUploadServiceLogName = "LogUploadService_{0}.log";

        /// <summary>
        /// Setup wizard log filename.
        /// </summary>
        public const string IntegrityCheckerLogName = "IntegrityCheck_{0}.log";

        /// <summary>
        /// Vault Credentials filename.
        /// </summary>
        public const string VaultCredsFileName = "Vault.VaultCredentials";

        /// <summary>
        /// Log upload service name.
        /// </summary>
        public const string LogUploadServiceName = "LogUpload";

        /// <summary>
        /// Telemetry LogName Component Seperator.
        /// </summary>
        public const string TelemetryLogNameComponentSeperator = "_";

        /// <summary>
        /// Push Install logs folder in CS machine.
        /// </summary>
        public const string PushInstallLogsFolder = "PushInstallLogs";

        /// <summary>
        /// Push Install jobs folder in CS machine.
        /// </summary>
        public const string PushInstallJobsFolder = "PushInstallJobs";

        /// <summary>
        /// Component name of PushInstall logs for telemetry.
        /// </summary>
        public const string PushInstallComponentName = "Push Install Telemetry";

        /// <summary>
        /// Component name of PushInstall jobs for telemetry.
        /// </summary>
        public const string PushInstallJobsName = "Push Install Jobs";

        /// <summary>
        /// Component name of standalone PS for telemetry.
        /// </summary>
        public const string ScaleOutUnitComponentName = "Scaleout Unit Telemetry";

        /// <summary>
        /// Proxy bypass subkey.
        /// </summary>
        public const string ProxyBypass = "BypassDefault";

        /// <summary>
        /// Proxy address subkey.
        /// </summary>
        public const string ProxyAddressRegValue = "ProxyAddress";

        /// <summary>
        /// Proxy port subkey.
        /// </summary>
        public const string ProxyPortRegValue = "ProxyPort";

        /// <summary>
        /// Proxy username subkey.
        /// </summary>
        public const string ProxyUserNameRegValue = "ProxyUserName";

        /// <summary>
        /// Proxy Bypass URLs.
        /// </summary>
        public const string ProxyBypassUrlsRegValue = "ProxyBypassUrls";

        /// <summary>
        /// Proxy password subkey.
        /// </summary>
        public const string ProxyPasswordRegValue = "ProxyPassword";

        /// <summary>
        /// Max Degree Of Parallelism for parallel foreach loop.
        /// </summary>
        public const int MaxDegreeOfParallelism = 4;

        /// <summary>
        /// Logupload Service minimum log file size in MB to upload to telemetry.
        /// </summary>
        public const int MinLogFileSizeInMB = 1;

        /// <summary>
        /// Upload folder.
        /// </summary>
        public const string UploadedLogsFolder = @"UploadedLogs";

        /// <summary>
        /// CXPS SSL Port.
        /// </summary>
        public const string CxPsSSLPort = "9443";

        /// <summary>
        /// Json File Format.
        /// </summary>
        public const string JsonFileFormat = ".json";

        /// <summary>
        /// Log File Format.
        /// </summary>
        public const string LogFileFormat = ".log";

        /// <summary>
        /// Cxps Port for Telemetry.
        /// </summary>
        public const string CxpsPortforTelemetry = "CxpsPortforTelemetry";

        /// <summary>
        /// CSIP for Telemetry.
        /// </summary>
        public const string CSIPforTelemetry = "CSIPforTelemetry";

        /// <summary>
        /// Json File Name.
        /// </summary>
        public const string MetadataJsonFileName = "metadata.json";

        /// <summary>
        /// Client RequestId.
        /// </summary>
        public const string ClientRequestId = "ClientRequestId";

        /// <summary>
        /// ActivityId.
        /// </summary>
        public const string ActivityId = "ActivityId";

        /// <summary>
        /// Service ActivityId.
        /// </summary>
        public const string ServiceActivityId = "ServiceActivityId";

        /// <summary>
        /// App service name.
        /// </summary>
        public const string AppServiceName = "InMage Scout Application Service";

        /// <summary>
        /// Service manual startup type.
        /// </summary>
        public const string ManualStartupType = "Manual";

        /// <summary>
        /// Service automatic startup type.
        /// </summary>
        public const string AutomaticStartupType = "Automatic";

        /// <summary>
        /// HostConfig shortcut.
        /// </summary>
        public const string HostConfigShortcutName = "hostconfigwxcommon.lnk";

        /// <summary>
        /// MARS agent shortcut.
        /// </summary>
        public const string MarsAgentShortcutName = "Microsoft Azure Backup.lnk";

        /// <summary>
        /// Mobility service pre-requisites validator executable name.
        /// </summary>
        public const string MobilityServiceValidatorExe = "MobilityServiceValidator.exe";

        /// <summary>
        /// Mobility service pre-requisites validator V2A config file name.
        /// </summary>
        public const string MobilityServiceValidatorV2AConfigFile = "v2a-mobilityservice-requirements.json";

        /// <summary>
        /// Mobility service pre-requisites validator A2A config file name.
        /// </summary>
        public const string MobilityServiceValidatorA2AConfigFile = "a2a-mobilityservice-requirements.json";

        /// <summary>
        /// Mobility service pre-requisites validations output json file.
        /// </summary>
        public const string MobilityServiceValidatorDefaultOutputJsonFile = "InstallerErrors.json";

        /// <summary>
        /// Mobility service pre-requisites validations log file.
        /// </summary>
        public const string MobilityServiceValidatorLogFile = "MobilityServiceValidations.log";

        /// <summary>
        /// Mobility service pre-requisites validations summary json file.
        /// </summary>
        public const string MobilityServiceValidatorSummaryJsonFile = "MobilityServiceValidationsSummary.json";

        /// <summary>
        /// Mobility service pre-requisites validations -config switch.
        /// </summary>
        public const string MobilityServiceValidatorConfigSwitch = "-config";

        /// <summary>
        /// Mobility service pre-requisites validations -out switch.
        /// </summary>
        public const string MobilityServiceValidatorOutSwitch = "-out";

        /// <summary>
        /// Mobility service pre-requisites validations -log switch.
        /// </summary>
        public const string MobilityServiceValidatorLogSwitch = "-log";

        /// <summary>
        /// Mobility service pre-requisites validations -summary switch.
        /// </summary>
        public const string MobilityServiceValidatorSummaryJsonSwitch = "-summary";

        /// <summary>
        /// Mobility service pre-requisites validations -action switch.
        /// </summary>
        public const string MobilityServiceValidatorActionJsonSwitch = "-action";

        /// <summary>
        /// Upload folder in CSPS machine for CS telemetry Logs.
        /// </summary>
        public const string CSTelemetryLogsFolder = @"cs_telemetry";

        /// <summary>
        /// Component name of CS operation logs for telemetry.
        /// </summary>
        public const string CSOperationLogsComponentName = "Configuration Server Logs";

        /// <summary>
        /// Log upload service status Json file.
        /// </summary>
        public const string UploadStatusJson = "LogUploadStatus.json";

        /// <summary>
        /// var folder name.
        /// </summary>
        public const string VarFolderName = @"var";

        /// <summary>
        /// Svagents service name.
        /// </summary>
        public const string SvagentsServiceName = "svagents";

        /// <summary>
        /// Svagents service display name.
        /// </summary>
        public const string SvagentsServiceDisplayName = "InMage Scout VX Agent - Sentinel/Outpost";

        /// <summary>
        /// Svagents execurable name.
        /// </summary>
        public const string SvagentsExeName = "svagents.exe";

        /// <summary>
        /// Svagents service description.
        /// </summary>
        public const string SvagentsServiceDescription = "Volume Replication Service";

        /// <summary>
        /// Appservice name.
        /// </summary>
        public const string AppserviceName = "InMage Scout Application Service";

        /// <summary>
        /// Appservice display name.
        /// </summary>
        public const string AppserviceDisplayName = "InMage Scout Application Service";

        /// <summary>
        /// Appservice execurable name.
        /// </summary>
        public const string AppserviceExeName = "appservice.exe";

        /// <summary>
        /// Disk Filter Driver name.
        /// </summary>
        public const string IndskFltDriverName = "InDskFlt";

        /// <summary>
        /// Disk Filter Driver Display name.
        /// </summary>
        public const string IndskFltDisplayName = "InDskFlt";

        /// <summary>
        /// Appservice description.
        /// </summary>
        public const string AppserviceDescription = "Helps in the discovery, protection and recovery of applications";

        /// <summary>
        /// Component name of Unified setup service logs for telemetry.
        /// </summary>
        public const string UnifiedSetupServiceLogsComponentName = "Unified Setup Service Logs";

        /// <summary>
        /// Name of Push Install summary Json file starts with.
        /// </summary>
        public const string PISummaryFilePattern = "PISummary*";

        /// <summary>
        /// Name of Push Install verbose file starts with.
        /// </summary>
        public const string PIVerboseFilePattern = "PIVerbose*";

        /// <summary>
        /// Push Install summary Json file name.
        /// </summary>
        public const string PISummaryFileName = "PISummary.json";

        /// <summary>
        /// Push Install verbose file name.
        /// </summary>
        public const string PIVerboseFileName = "PIVerbose.log";

        /// <summary>
        /// Azure Site Recovery VSS service name
        /// </summary>
        public const string ASRVSSServiceName = "Azure Site Recovery VSS Provider";

		/// <summary>
        /// The lowest value of azurercmcli exit code
        /// </summary>
        public const int AzureRcmCliExitCodeLowerLimit = 120;

        /// <summary>
        /// the largest value of azurercmcli exit code
        /// </summary>
        public const int AzureRcmCliExitCodeUpperLimit = 149;
		
        /// <summary>
        /// Process Server Monitor service name
        /// </summary>
        public const string ProcessServerMonitorServiceName = "ProcessServerMonitor";

        /// <summary>
        /// Three minutes timeout value in ms
        /// </summary>
        public const int TimeoutInMs = 3 * 60 * 1000;

        /// <summary>
        /// Microsoft Kb KB4474419 for SHA2 support
        /// </summary>
        public const string KB4474419 = "KB4474419";

        /// <summary>
        /// Microsoft Kb KB4493730 for SHA2 support
        /// </summary>
        public const string KB4493730 = "KB4493730";

        /// <summary>
        /// Microsoft Kb KB4490628 for SHA2 support
        /// </summary>
        public const string KB4490628 = "KB4490628";

        /// <summary>
        /// Error file path parameter for AzureRcmCli
        /// </summary>
        public const string AzureRcmCliErrorFileName = "--errorfilepath";

        /// <summary>
        /// Log file path parameter for AzureRcmCli
        /// </summary>
        public const string AzureRcmCliLogFileName = "--logfile";

        /// <summary>
        /// Agent Config Input parameter for AzureRcmCli
        /// </summary>
        public const string AzureRcmCliAgentConfigInput = "--getagentconfiginput";

        public enum InstallerPrechecksResult
        {
            Success = 0,
            Failure = 1,
            Warning = 2
        };

        public enum InstallationStatus
        {
            Success = 0,
            Failure = 1,
            Warning = 2
        };

        /// <summary>
        /// Kbs required for SHA2 support in Win2k8R2 and windows 7
        /// </summary>
        public static IList<string> SHA2SupportKbwin2k8R2 = new List<string>
        {
            KB4474419, KB4490628
        };

        /// <summary>
        /// Kbs required for SHA2 support in Win2k8 and windows vista
        /// </summary>
        public static IList<string> SHA2SupportKbwin2k8 = new List<string>
        {
            KB4474419, KB4493730
        };

        /// <summary>
        /// If the msi log file size is greater than below size,
        /// do not parse it
        /// </summary>
        public const int MSILogReadMaxFileSize = 20 * 1024 * 1024;
    }
}
