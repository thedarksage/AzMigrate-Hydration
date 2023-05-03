using System;
using RcmAgentCommonLib.Utilities;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.Service;

namespace MarsAgent.CBEngine.Constants
{
    /// <summary>
    /// Defines the properties for CBEngine service.
    /// </summary>
    public static class CBEngineConstants
    {

        /// <summary>
        /// Gets the service registry path.
        /// </summary>
        private static string ServiceRegistryPath
        {
            get
            {
                return @"Software\Microsoft\Windows Azure Backup\Setup";
            }
        }

        /// <summary>
        /// Gets the service display name.
        /// </summary>
        public static string ServiceDisplayName
        {
            get
            {
                return "Microsoft Azure Recovery Services Agent";
            }
        }

        /// <summary>
        /// Gets the CBEngine installation path.
        /// </summary>
        public static string InstallLocation
        {
            get
            {
                string installPath = RegistryHelper.GetRegistryValue(
                    ServiceRegistryPath,
                    "InstallPath");

                if (string.IsNullOrWhiteSpace(installPath))
                {
                    string defaultInstallPath =
                        @"%programfiles%\\" + ServiceDisplayName;
                    installPath = Environment.ExpandEnvironmentVariables(defaultInstallPath);
                }

                return installPath;
            }
        }

        /// <summary>
        /// Gets the existing cbengine logs directory path .
        /// </summary>
        public static string TempLogDirPath
        {
            get
            {
                return InstallLocation + "\\Temp";
            }
        }

        /// <summary>
        /// Gets the revised cbengine logs directory path for uploading to kusto .
        /// </summary>
        public static string LogDirPath
        {
            get
            {
                ServiceConfigurationSettings config = Configurator.Instance.GetConfig();
                return config.LogFolderPath + "\\CBEngine";
            }
        }

        /// <summary>
        /// Gets the revised cbengine ready for upload logs.
        /// </summary>
        public static string ReadyForUploadDir
        {
            get
            {
                return "ReadyForUpload";
            }
        }

        /// <summary>
        /// Gets the uploaded directory name.
        /// </summary>
        public static string UploadedDir
        {
            get
            {
                return "Uploaded";
            }
        }

        /// <summary>
        /// Gets the delimator in log file name.
        /// </summary>
        public static string LogFileNameSeperator
        {
            get
            {
                return "_";
            }
        }

        /// <summary>
        /// Gets the suffix after parsing cbengine logs.
        /// </summary>
        public static string ParsedLogFileSuffix
        {
            get
            {
                return "parsed";
            }
        }

        /// <summary>
        /// Gets the extension used for cbengine logs .
        /// </summary>
        public static string LogFileExtension
        {
            get
            {
                return ".log";
            }
        }

        /// <summary>
        /// Service name for the Microsoft Recovery Service Agent(obengine).
        /// </summary>
        public static string ServiceName
        {
            get
            {
                return "obengine";
            }
        }
    }
}