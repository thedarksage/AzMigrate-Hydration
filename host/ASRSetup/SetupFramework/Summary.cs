//---------------------------------------------------------------
//  <copyright file="Summary.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Provides summary on the installation.
//  </summary>
//
//  History:     28-Aug-2017   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace ASRSetupFramework
{
    # region enums
    /// <summary>
    /// Installation scenario type.
    /// </summary>
    public enum InstallType
    {
        /// <summary>
        /// New scenario
        /// </summary>
        New,

        /// <summary>
        /// Upgrade scenario
        /// </summary>
        Upgrade,

        /// <summary>
        /// Configuration.
        /// </summary>
        Configuration
    }

    /// <summary>
    /// Execution scenarios.
    /// </summary>
    public enum Scenario
    {
        /// <summary>
        /// Pre installation
        /// </summary>
        PreInstallation,

        /// <summary>
        /// During installation
        /// </summary>
        Installation,

        /// <summary>
        /// Post installation
        /// </summary>
        PostInstallation,

        /// <summary>
        /// Pre configuration
        /// </summary>
        PreConfiguration,

        /// <summary>
        /// During configuration
        /// </summary>
        Configuration,

        /// <summary>
        /// Post configuration
        /// </summary>
        PostConfiguration,

        /// <summary>
        /// Product pre-requisite checks
        /// </summary>
        ProductPreReqChecks,
    }

    /// <summary>
    /// Operation result.
    /// </summary>
    public enum OpResult
    {
        /// <summary>
        /// For successful operation.
        /// </summary>
        Success,

        /// <summary>
        /// For failed operation.
        /// </summary>
        Failed,

        /// <summary>
        /// Success with warnings.
        /// </summary>
        Warn,

        /// <summary>
        /// For skip operation.
        /// </summary>
        Skip,

        /// <summary>
        /// For aborted (from the user) operations.
        /// </summary>
        Aborted
    }

    /// <summary>
    /// Adapter to be installed during installation.
    /// </summary>
    public enum AdapterType
    {
        /// <summary>
        /// VMM Fabric adapter.
        /// </summary>
        VMMFabricAdapter,

        /// <summary>
        /// Single host adapter.
        /// </summary>
        SingleHostAdapter,

        /// <summary>
        /// InMage adapter.
        /// </summary>
        InMageAdapter,

        /// <summary>
        /// VMware adapter.
        /// </summary>
        VMwareAdapter
    }

    #endregion enums

    /// <summary>
    /// Summary collection definitions.
    /// </summary>
    public class Summary
    {
        /// <summary>
        /// Gets or sets the installation start date and time.
        /// </summary>
        public string StartTime { get; set; }

        /// <summary>
        /// Gets or sets the name of the component being installed.
        /// </summary>
        public string ComponentName { get; set; }

        /// <summary>
        /// Gets or sets the version of the component being installed.
        /// </summary>
        public string ComponentVersion { get; set; }

        /// <summary>
        /// Gets or sets the version of the already installed component version.
        /// </summary>
        public string ExistingVersion { get; set; }

        /// <summary>
        /// Interactive/Silent install.
        /// </summary>
        public bool SilentInstall { get; set; }

        /// <summary>
        /// Final exit code.
        /// </summary>
        public int ExitCode { get; set; }

        /// <summary>
        /// Gets or sets the type of installation.
        /// </summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public InstallType InstallType { get; set; }

        /// <summary>
        /// Gets or sets the fabric adapter type.
        /// </summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public AdapterType FabricAdapter { get; set; }

        /// <summary>
        /// Gets or sets the final installation status.
        /// </summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public OpResult InstallStatus { get; set; }

        /// <summary>
        /// Gets or sets the operating system type (Windows/Linux).
        /// </summary>
        public string OsType { get; set; }

        /// <summary>
        /// Gets or sets the operating system distro details.
        /// </summary>
        public string OsDistro { get; set; }

        /// <summary>
        /// Gets or sets the machine unique identifier.
        /// </summary>
        public Guid MachineIdentifier { get; set; }

        /// <summary>
        /// Gets or sets the machine hostname.
        /// </summary>
        public string HostName { get; set; }

        /// <summary>
        /// Gets or sets the asset tag associated with machine.
        /// </summary>
        public string AssetTag { get; set; }

        /// <summary>
        /// Gets or sets the installation scenario (VmWare/Azure).
        /// </summary>
        public string InstallScenario { get; set; }

        /// <summary>
        /// Gets or sets the installation role.
        /// </summary>
        public string InstallRole { get; set; }

        /// <summary>
        /// Gets or sets the total memory of machine.
        /// </summary>
        public double TotalMemoryInGB { get; set; }

        /// <summary>
        /// Gets or sets the available free memory of machine.
        /// </summary>
        public double FreeMemoryInGB { get; set; }

        /// <summary>
        /// Gets or sets the invoker type.
        /// </summary>
        public string InvokerType { get; set; }

        /// <summary>
        /// Gets or sets the BIOSid.
        /// </summary>
        public string BIOSid { get; set; }

        /// <summary>
        /// Gets or sets the HostId.
        /// </summary>
        public string HostId { get; set; }

        /// <summary>
        /// Gets or sets the RunId.
        /// </summary>
        public string RUNID { get; set; }

        /// <summary>
        /// Gets or sets the installation end date and time.
        /// </summary>
        public string EndTime { get; set; }

        /// <summary>
        /// Gets or sets the list of operations.
        /// </summary>
        public List<OperationDetails> OperationDetails { get; set; }
    }

    /// <summary>
    /// Operation details definition.
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "Summary related classes in one file")]
    public class OperationDetails
    {
        /// <summary>
        /// Gets or sets the scenario name.
        /// </summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public Scenario ScenarioName { get; set; }

        /// <summary>
        /// Gets or sets the operation name.
        /// </summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public OpName OperationName { get; set; }

        /// <summary>
        /// Gets or sets the operation status.
        /// </summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public OpResult Result { get; set; }

        /// <summary>
        /// Gets or sets the error code of the error during the operation.
        /// </summary>
        public int ErrorCode { get; set; }

        /// <summary>
        /// Gets or sets the name of the error that has occured during the operation.
        /// </summary>
        public string ErrorCodeName { get; set; }

        /// <summary>
        /// Gets or sets the error message if failed.
        /// </summary>
        public string Message { get; set; }

        /// <summary>
        /// Gets or sets the possible causes on the operation if failed.
        /// </summary>
        public string Causes { get; set; }

        /// <summary>
        /// Gets or sets the recommended actions on the operation if failed.
        /// </summary>
        public string Recommendation { get; set; }

        /// <summary>
        /// Gets or sets the exception message if failed.
        /// </summary>
        public string Exception { get; set; }
    }

    /// <summary>
    /// Class initialized once per process and used by all setup managed code in process.
    /// </summary>
    internal class OpLogger
    {
        #region Private members

        /// <summary>
        /// Lock Object
        /// </summary>
        private static readonly object lockObject = new object();

        /// <summary>
        /// Summary instance handler
        /// </summary>
        private static OpLogger instance;

        #endregion

        #region Constructors

        /// <summary>
        /// Initializes a new instance of the OpLogger class.
        /// Prevents a default instance from being created.
        /// </summary>
        /// <param name="componentName">Name of the component being installed.</param>
        /// <param name="componentVersion">Component version.</param>
        /// <param name="installationType">type of installation new/upgrade.</param>
        /// <param name="fabricAdapter">Fabric adapter type.</param>
        private OpLogger(
            string componentName,
            Version componentVersion,
            InstallType installationType,
            AdapterType fabricAdapter)
        {
            this.SerializerSettings = new JsonSerializerSettings()
            {
                Formatting = Formatting.Indented,
                NullValueHandling = NullValueHandling.Ignore
            };
            this.Summary = new Summary()
            {
                ComponentName = componentName,
                ComponentVersion = componentVersion.ToString(),
                InstallType = installationType,
                FabricAdapter = fabricAdapter,
                StartTime = DateTime.Now.ToString("o", CultureInfo.InvariantCulture),
                OperationDetails = new List<OperationDetails>()
            };

            // New file for every installation in the common log path.
            string summaryFileName = string.Format(
                UnifiedSetupConstants.SummaryFileNameFormat,
                Guid.NewGuid().ToString());
            this.SummaryFileName = SetupHelper.SetLogFilePath(summaryFileName);
            PropertyBagDictionary.Instance.SafeAdd(
                PropertyBagConstants.SummaryFileName,SummaryFileName);
            this.WriteData();
        }

        #endregion Constructors

        #region Properties

        /// <summary>
        /// Gets the instance.
        /// </summary>
        internal static OpLogger Instance
        {
            get
            {
                Trace.Assert(
                    OpLogger.instance != null,
                    "Summary is incorrectly initialized",
                    "Call Initialize before invoking Instance");
                return OpLogger.instance;
            }
        }

        /// <summary>
        /// Gets the operation summary.
        /// </summary>
        internal Summary Summary
        {
            get; private set;
        }

        /// <summary>
        /// Gets or sets the name of the summary file.
        /// </summary>
        private string SummaryFileName { get; set; }

        /// <summary>
        /// Gets or sets the serializer settings.
        /// </summary>
        private JsonSerializerSettings SerializerSettings { get; set; }

        #endregion Properties

        #region Internal methods

        #region Initializers

        /// <summary>
        /// Initilizes the summary class.
        /// </summary>
        /// <param name="componentName">Name of the component being installed.</param>
        /// <param name="componentVersion">Component version.</param>
        /// <param name="installationType">type of installation new/upgrade.</param>
        /// <param name="fabricAdapter">Fabric adapter type.</param>
        internal static void Initialize(
            string componentName,
            Version componentVersion,
            InstallType installationType,
            AdapterType fabricAdapter)
        {
            Trace.Assert(
                OpLogger.instance == null,
                "Summary already initialized",
                "Dispose summary before invoking again");

            if (OpLogger.instance == null)
            {
                OpLogger.instance = new OpLogger(
                    componentName,
                    componentVersion,
                    installationType,
                    fabricAdapter);

                Trc.Log(
                    LogLevel.Always,
                    "OpLogger intialized with Installation type {0} and Adapter {1}.",
                    installationType,
                    fabricAdapter);
            }
        }

        #endregion

        /// <summary>
        /// Stores the recorded operation summary.
        /// </summary>
        /// <param name="executionScenario">Installation scenario.</param>
        /// <param name="operationName">Name of the operation</param>
        /// <param name="result">Status to be logged.</param>
        /// <param name="errorMessage">Error message.</param>
        /// <param name="causes">Possible causes.</param>
        /// <param name="recommendation">Recommended action</param>
        /// <param name="exception">Exception message if any.</param>
        /// <param name="errorCode">Error code of the operation.</param>
        /// <param name="errorCodeName">Friendly name of the error code of the operation.</param>
        internal void RecordOperationResult(
            Scenario executionScenario,
            OpName operationName,
            OpResult result,
            string errorMessage = null,
            string causes = null,
            string recommendation = null,
            string exception = null,
            int errorCode = 0,
            string errorCodeName = null)
        {
            OperationDetails operationDetails = new OperationDetails()
            {
                ScenarioName = executionScenario,
                OperationName = operationName,
                Result = result,
                Message = errorMessage,
                Causes = causes,
                Recommendation = recommendation,
                Exception = exception,
                ErrorCode = errorCode,
                ErrorCodeName = errorCodeName
            };

            lock (lockObject)
            {
                this.Summary.OperationDetails.Add(operationDetails);
                this.WriteData();
            }
        }

        /// <summary>
        /// Updates summary end date and time with final result.
        /// </summary>
        /// <param name="finalResult">Final result of the installation.</param>
        internal void End(
            OpResult finalResult,
            Guid machineIdentifier,
            string osType,
            string osDistro,
            string hostName,
            string assetTag,
            string installScenario,
            string installRole,
            double totalMemoryInGB,
            double freeMemoryInGB,
            string existingVersion,
            bool silentInstall,
            int exitCode,
            string invokerType,
            string biosId,
            string drscoutHostId,
            string runId)
        {
            this.Summary.InstallStatus = finalResult;
            this.Summary.EndTime = 
                DateTime.Now.ToString("o", CultureInfo.InvariantCulture);
            this.Summary.MachineIdentifier = machineIdentifier;
            this.Summary.OsType = osType;
            this.Summary.OsDistro = osDistro;
            this.Summary.HostName = hostName;
            this.Summary.AssetTag = assetTag;
            this.Summary.InstallScenario = installScenario;
            this.Summary.InstallRole = installRole;
            this.Summary.TotalMemoryInGB = totalMemoryInGB;
            this.Summary.FreeMemoryInGB = freeMemoryInGB;
            this.Summary.ExistingVersion = existingVersion;
            this.Summary.SilentInstall = silentInstall;
            this.Summary.ExitCode = exitCode;
            this.Summary.InvokerType = invokerType;
            this.Summary.BIOSid = biosId;
            this.Summary.HostId = drscoutHostId;
            this.Summary.RUNID = runId;

            this.WriteData();
        }

        /// <summary>
        /// Dispose the current instance.
        /// </summary>
        internal void Dispose()
        {
            OpLogger.instance = null;
        }

        #endregion Internal methods

        #region Private methods

        /// <summary>
        /// Writes summary information to the file in the json format
        /// </summary>
        private void WriteData()
        {
            try
            {
                // Overwrite the file with the new information
                using (StreamWriter sw = new StreamWriter(this.SummaryFileName, false))
                using (JsonWriter writer = new JsonTextWriter(sw))
                {
                    JsonSerializer serializer = JsonSerializer.Create(this.SerializerSettings);
                    serializer.Serialize(writer, this.Summary);
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Write data failed.", ex);
            }
        }

        #endregion Private methods
    }
}
