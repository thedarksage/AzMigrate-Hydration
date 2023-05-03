using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnifiedSetup
{
    // SetupParameters is a singleton class which gets instanciated once and used in entire flow.
    class SetupParameters
    {
        private static SetupParameters _instance;
        private static Object objLock = new Object();

        // The constructor is protected to avoid any creations
        protected SetupParameters()
        {
        }

        /// <summary>
        /// Returns the Instance of the SetupController
        /// </summary>
        /// <returns></returns>
        public static SetupParameters Instance()
        {
            // Uses lazy initialization.
            if (_instance == null)
            {
                lock (objLock)
                {
                    if (_instance == null)
                    {
                        _instance = new SetupParameters();
                    }
                }
            }
            return _instance;
        }

        //Common Parameters
        private string commonParamters = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
        public string CommonParameters
        {
            get { return commonParamters; }
        }

        //Server Mode
        private string serverMode;
        public string ServerMode
        {
            get { return serverMode; }
            set { serverMode = value; }
        }

        //CX Installation Directory
        private string cxInstallDir;
        public string CXInstallDir
        {
            get { return cxInstallDir; }
            set { cxInstallDir = value; }
        }

        //MySQL Credentials File Path
        private string mysqlCredsFilePath;
        public string MysqlCredsFilePath
        {
            get { return mysqlCredsFilePath; }
            set { mysqlCredsFilePath = value; }
        }

        //Vault Credentials File Path
        private string valutCredsFilePath;
        public string ValutCredsFilePath
        {
            get { return valutCredsFilePath; }
            set { valutCredsFilePath = value; }
        }

        //Environment Type
        private string envType;
        public string EnvType
        {
            get { return envType; }
            set { envType = value; }
        }

        //Passphrase File Path
        private string passphraseFilePath;
        public string PassphraseFilePath
        {
            get { return passphraseFilePath; }
            set { passphraseFilePath = value; }
        }

        //Source Config File Path
        private string sourceConfigFilePath;
        public string SourceConfigFilePath
        {
            get { return sourceConfigFilePath; }
            set { sourceConfigFilePath = value; }
        }

        //ProxySettings File Path
        private string proxysettingsFilePath;
        public string ProxySettingsFilePath
        {
            get { return proxysettingsFilePath; }
            set { proxysettingsFilePath = value; }
        }

        //Agent Role
        private string agentRole = "MT";
        public string AgentRole
        {
            get { return agentRole; }
        }

        //Agent Installation Directory
        private string agentInstallDir;
        public string AgentInstallDir
        {
            get { return agentInstallDir; }
            set { agentInstallDir = value; }
        }

        //CS IP
        private string csIP;
        public string CSIP
        {
            get { return csIP; }
            set { csIP = value; }
        }

        //CS Port
        private string csPort = "443";
        public string CSPort
        {
            get { return csPort; }
            set { csPort = value; }
        }

        //Communication Mode 
        private string commMode = "Https";
        public string CommMode
        {
            get { return commMode; }
        }

        //PS IP
        private string psIP;
        public string PSIP
        {
            get { return psIP; }
            set { psIP = value; }
        }

        //AZURE IP
        private string azureIP;
        public string AZUREIP
        {
            get { return azureIP; }
            set { azureIP = value; }
        }

        //Data Transfer Secure Port
        private string dataTransferSecurePort = "9443";
        public string DataTransferSecurePort
        {
            get { return dataTransferSecurePort; }
            set { dataTransferSecurePort = value; }
        }

        //Skip Space Check
        private string skipSpaceCheck = "false";
        public string SkipSpaceCheck
        {
            get { return skipSpaceCheck; }
            set { skipSpaceCheck = value; }
        }

        //Passphrase validation
        private string ispassphrasevalid;
        public string IsPassphraseValid
        {
            get { return ispassphrasevalid; }
            set { ispassphrasevalid = value; }
        }

        //Proxy Type
        private string proxytype;
        public string ProxyType
        {
            get { return proxytype; }
            set { proxytype = value; }
        }

        //Prerequisite Checks Failed
        private string prerequisiteChecksFailed;
        public string PrerequisiteChecksFailed
        {
            get { return prerequisiteChecksFailed; }
            set { prerequisiteChecksFailed = value; }
        }

        //Product is already installed
        private string productISAlreadyInstalled;
        public string ProductISAlreadyInstalled
        {
            get { return productISAlreadyInstalled; }
            set { productISAlreadyInstalled = value; }
        }

        // Fresh/Upgrade installation.
        private string installType;
        public string InstallType
        {
            get { return installType; }
            set { installType = value; }
        }

        // Installation status.
        private string installStatus;
        public string InstallStatus
        {
            get { return installStatus; }
            set { installStatus = value; }
        }

        // Reinstallation status.
        private String reInstallationStatus;
        public String ReInstallationStatus
        {
            get { return reInstallationStatus; }
            set { reInstallationStatus = value; }
        }

        // Reboot required status.
        private string rebootRequired;
        public string RebootRequired
        {
            get { return rebootRequired; }
            set { rebootRequired = value; }
        }

        // MySQL version.
        private string mysqlVersion;
        public string MysqlVersion
        {
            get { return mysqlVersion; }
            set { mysqlVersion = value; }
        }

        /// <summary>
        /// Is VM a PS gallery image.
        /// </summary>
        public bool PSGalleryImage { get; set; }

        /// <summary>
        /// Is VM a OVF image.
        /// </summary>
        public bool OVFImage { get; set; }

        /// <summary>
        /// To skip all pre-requisites check.
        /// </summary>
        public bool SkipPrereqsCheck { get; set; }

        /// <summary>
        /// To skip registration.
        /// </summary>
        public bool SkipRegistration { get; set; }

        /// <summary>
        /// To skip TLS check.
        /// </summary>
        public bool SkipTLSCheck { get; set; }

        /// <summary>
        /// CX TP installation status.
        /// </summary>
        public bool IsCXTPInstalled { get; set; }

        /// <summary>
        /// CS installation status.
        /// </summary>
        public bool IsCSInstalled { get; set; }

        /// <summary>
        /// MT installation status.
        /// </summary>
        public bool IsMTInstalled { get; set; }

        /// <summary>
        /// MT configuration status.
        /// </summary>
        public bool IsMTConfigured { get; set; }

        /// <summary>
        /// DRA installation status.
        /// </summary>
        public bool IsDRAInstalled { get; set; }

        /// <summary>
        /// DRA registration status.
        /// </summary>
        public bool IsDRARegistered { get; set; }

        /// <summary>
        /// IIS installation status.
        /// </summary>
        public bool IsIISInstall { get; set; }

        /// <summary>
        /// MARS installation status.
        /// </summary>
        public bool IsMARSInstalled { get; set; }

        /// <summary>
        /// Configuration Manager installation status.
        /// </summary>
        public bool IsConfigManagerInstalled { get; set; }

        /// <summary>
        /// MT registration status.
        /// </summary>
        public bool IsMTRegistered { get; set; }

        /// <summary>
        /// MySQL download status.
        /// </summary>
        public bool IsMySQLDownloaded { get; set; }

        /// <summary>
        /// Installation status.
        /// </summary>
        public bool IsInstallationSuccess { get; set; }

        /// <summary>
        /// Current build version.
        /// </summary>
        public Version CurrentBuildVersion { get; set; }
    }
}
