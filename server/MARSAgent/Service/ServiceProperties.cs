using MarsAgent.CBEngine.Constants;
using MarsAgent.Utilities;
using RcmAgentCommonLib.Service;
using RcmAgentCommonLib.Utilities;
using RcmContract;
using System;
using System.IO;

namespace MarsAgent.Service
{
    /// <summary>
    /// Defines the properties for the on-premise agent service.
    /// </summary>
    public sealed class ServiceProperties : MarsServicePropertiesBase
    {
        /// <summary>
        /// Singleton instance of <see cref="ServiceProperties"/> used by all callers.
        /// </summary>
        public static readonly ServiceProperties Instance = new ServiceProperties();

        /// <summary>
        /// Prevents a default instance of the <see cref="ServiceProperties"/> class from
        /// being created.
        /// </summary>
        private ServiceProperties()
        {
        }

        /// <summary>
        /// Gets the service registry path.
        /// </summary>
        public override string ServiceRegistryPath
        {
            get
            {
                return @"Software\Microsoft\Recovery Services Agent";
            }
        }

        /// <summary>
        /// Gets the config file path.
        /// </summary>
        public override string ConfigFilePath
        {
            get
            {
                string configPath = RegistryHelper.GetRegistryValue(
                    this.ServiceRegistryPath,
                    "ConfigPath");

                if (string.IsNullOrWhiteSpace(configPath))
                {
                    configPath = Environment.ExpandEnvironmentVariables(
                        @"%programdata%\\Microsoft Azure\\Config\\MarsAgent.json");
                }

                return configPath;
            }
        }

        /// <summary>
        /// Gets the file path for local preferences.
        /// </summary>
        public override string LocalPreferencesFilePath
        {
            get
            {
                string configFolderPath = Directory.GetParent(this.ConfigFilePath).FullName;
                return configFolderPath + Path.DirectorySeparatorChar +
                    "MarsAgentlocalpreferences.json";
            }
        }

        /// <summary>
        /// Gets the file path for persisting settings received from RCM.
        /// </summary>
        public string AgentSettingsFolderPath
        {
            get
            {
                string configFolderPath = Directory.GetParent(this.ConfigFilePath).FullName;
                return configFolderPath + Path.DirectorySeparatorChar;
            }
        }

        /// <summary>
        /// Gets the file path for persisting settings received from RCM.
        /// </summary>
        public override string AgentSettingsFilePath
        {
            get
            {
                return this.AgentSettingsFolderPath + "MarsSettings.json";
            }
        }

        /// <summary>
        /// Gets the file path for persisting health data.
        /// </summary>
        public override string AgentHealthFilePath
        {
            get
            {
                string configFolderPath = Directory.GetParent(this.ConfigFilePath).FullName;
                return configFolderPath + Path.DirectorySeparatorChar +
                    "MarsAgenthealth.json";
            }
        }

        /// <summary>
        /// Gets the installation path.
        /// </summary>
        public override string InstallLocation
        {
            get
            {
                string installPath = RegistryHelper.GetRegistryValue(
                    this.ServiceRegistryPath,
                    "InstallDirectory");

                if (string.IsNullOrWhiteSpace(installPath))
                {
                    string defaultInstallPath =
                        @"%programfiles%\\" + this.ServiceDisplayName;
                    installPath = Environment.ExpandEnvironmentVariables(defaultInstallPath);
                }

                return installPath;
            }
        }

        /// <summary>
        /// Gets the default log folder path.
        /// </summary>
        public override string DefaultLogFolder
        {
            get
            {
                return this.InstallLocation + "\\logs";
            }
        }

        /// <summary>
        /// Gets the default instrumentation key to use until the service is configured.
        /// </summary>
        public override string DefaultInstrumentationKey
        {
            get
            {
                return "90b46585-1513-4a55-814c-bd57e9b24709";
            }
        }

        /// <summary>
        /// Gets the service GUID.
        /// </summary>
        public override string ServiceGuid
        {
            get
            {
                return "97771B14-09C5-41C9-BB28-33B838685056";
            }
        }

        /// <summary>
        /// Gets the service name.
        /// </summary>
        public override string ServiceName
        {
            get
            {
                return "MarsAgent";
            }
        }

        /// <summary>
        /// Gets the user friendly service name as displayed on azure portal.
        /// </summary>
        public override string UserFriendlyServiceName
        {
            get
            {
                return "Microsoft Azure Recovery Services On-Premise Agent";
            }
        }

        /// <summary>
        /// Gets the service display name.
        /// </summary>
        public override string ServiceDisplayName
        {
            get
            {
                return "Microsoft Azure Recovery Services On-Premise Agent";
            }
        }

        /// <summary>
        /// Gets the service description.
        /// </summary>
        public override string ServiceDescription
        {
            get
            {
                return "Microsoft Azure Recovery Services On-Premise Agent";
            }
        }

        /// <summary>
        /// Gets the component name.
        /// </summary>
        public override string ComponentName
        {
            get
            {
                return nameof(RcmEnum.RcmComponentIdentifier.Mars);
            }
        }

        /// <summary>
        /// Gets the component type.
        /// </summary>
        public override RcmEnum.RcmComponentIdentifier ComponentType
        {
            get
            {
                return RcmEnum.RcmComponentIdentifier.Mars;
            }
        }

        /// <summary>
        /// Gets the logging event source name.
        /// </summary>
        public override string EventSourceName
        {
            get
            {
                return "MarsAgent";
            }
        }

        /// <summary>
        /// Gets the user friendly service name for MARS.
        /// </summary>
        public string MarsUserFriendlyServiceName
        {
            get
            {
                return "Microsoft Azure Recovery Services Agent";
            }
        }

        /// <summary>
        /// Gets the default replication state root folder path.
        /// </summary>
        public override string DefaultReplicationStateRootFolder
        {
            get
            {
                return this.InstallLocation + "\\AppliedInfo";
            }
        }
    }
}
