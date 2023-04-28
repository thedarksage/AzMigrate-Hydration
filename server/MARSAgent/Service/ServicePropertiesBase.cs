using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Reflection;
using MarsAgent.CBEngine.Constants;
using MarsAgent.Utilities;
using RcmAgentCommonLib.Utilities;
using RcmContract;

namespace RcmAgentCommonLib.Service
{
    /// <summary>
    /// Defines abstract base class for service properties.
    /// </summary>
    public abstract class MarsServicePropertiesBase : IServiceProperties
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="MarsServicePropertiesBase" /> class.
        /// </summary>
        public MarsServicePropertiesBase()
        {
        }

        /// <summary>
        /// Gets the config file path.
        /// </summary>
        public abstract string ConfigFilePath { get; }

        /// <summary>
        /// Gets the file path for local preferences.
        /// </summary>
        public abstract string LocalPreferencesFilePath { get; }

        /// <summary>
        /// Gets the file path for persisting settings received from RCM.
        /// </summary>
        public abstract string AgentSettingsFilePath { get; }

        /// <summary>
        /// Gets the file path for persisting health data.
        /// </summary>
        public abstract string AgentHealthFilePath { get; }

        /// <summary>
        /// Gets the installation path.
        /// </summary>
        public abstract string InstallLocation { get; }

        /// <summary>
        /// Gets the default log folder path.
        /// </summary>
        public abstract string DefaultLogFolder { get; }

        /// <summary>
        /// Gets the default replication state folder path.
        /// </summary>
        public abstract string DefaultReplicationStateRootFolder { get; }

        /// <summary>
        /// Gets the default instrumentation key to use until the service is configured.
        /// </summary>
        public abstract string DefaultInstrumentationKey { get; }

        /// <summary>
        /// Gets the service GUID.
        /// </summary>
        public abstract string ServiceGuid { get; }

        /// <summary>
        /// Gets the service executable name.
        /// </summary>
        public abstract string ServiceName { get; }

        /// <summary>
        /// Gets the user friendly service name as displayed on azure portal.
        /// </summary>
        public abstract string UserFriendlyServiceName { get; }

        /// <summary>
        /// Gets the service description.
        /// </summary>
        public abstract string ServiceDescription { get; }

        /// <summary>
        /// Gets the service display name.
        /// </summary>
        public abstract string ServiceDisplayName { get; }

        /// <summary>
        /// Gets the service registry path.
        /// </summary>
        public abstract string ServiceRegistryPath { get; }

        /// <summary>
        /// Gets the component name.
        /// </summary>
        public abstract string ComponentName { get; }

        /// <summary>
        /// Gets the component type.
        /// </summary>
        public abstract RcmEnum.RcmComponentIdentifier ComponentType { get; }

        /// <summary>
        /// Gets the logging event source name.
        /// </summary>
        public abstract string EventSourceName { get; }

        /// <summary>
        /// Gets the service executable path.
        /// </summary>
        public string ServiceExecutablePath
        {
            get
            {
                return Assembly.GetEntryAssembly().Location;
            }
        }

        /// <summary>
        /// Gets the service executable name.
        /// </summary>
        public string ServiceExecutableName
        {
            get
            {
                return Path.GetFileName(this.ServiceExecutablePath);
            }
        }

        /// <summary>
        /// Gets the default machine identifier to use until the service is configured.
        /// </summary>
        public string DefaultMachineIdentifier
        {
            get
            {
                return this.AgentFriendlyName;
            }
        }

        /// <summary>
        /// Gets the default machine identifier to use until the service is configured.
        /// </summary>
        public string AgentFriendlyName
        {
            get
            {
                string domainName = IPGlobalProperties.GetIPGlobalProperties().DomainName;
                string hostName = Dns.GetHostName();

                if (!string.IsNullOrWhiteSpace(domainName))
                {
                    domainName = "." + domainName;
                    if (!hostName.EndsWith(domainName))
                    {
                        hostName += domainName;
                    }
                }

                return hostName;
            }
        }

        /// <summary>
        /// Gets the service version.
        /// </summary>
        public string ServiceVersion
        {
            get
            {
                return FileUtils.GetFileVersion(Path.Combine(
                    CBEngineConstants.InstallLocation, @"bin\cbengine.exe"));
            }
        }

        /// <summary>
        /// Gets the name of the process job group being for processes launched by the service.
        /// </summary>
        public string ProcessJobGroupName
        {
            get
            {
                return this.ServiceName + this.ServiceGuid;
            }
        }
    }
}
