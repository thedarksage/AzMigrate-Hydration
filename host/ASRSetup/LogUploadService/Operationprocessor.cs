//---------------------------------------------------------------
//  <copyright file="Operationprocessor.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Upload logs to AppInsights database using IntergrityCheck.dll.
//  </summary>
//
//  History:     26-Sep-2017   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Win32;
using ASRResources;
using ASRSetupFramework;
using Microsoft.DisasterRecovery.IntegrityCheck;
using DRSetupFramework = Microsoft.DisasterRecovery.SetupFramework;
using Newtonsoft.Json;

namespace Microsoft.ASRSetup.LogUploadService
{
    /// <summary>
    /// Performs log upload related operations.
    /// </summary>
    internal partial class Operationprocessor
    {
        /// <summary>
        /// Log uploader.
        /// </summary>
        private ICLogUploader logUploader = new ICLogUploader();

        # region internal methods

        /// <summary>
        /// Search agent setup logs to upload.
        /// </summary>
        internal void UploadLogs()
        {
            if (!DRSetupFramework.SetupHelper.IsPrivateEndpointStateEnabled())
            {
                while (true)
                {
                    try
                    {
                        InitializeTrace();
                        Trc.Log(LogLevel.Always, "Begin UploadLogs.");

                        // Fetch installation location.
                        if (string.IsNullOrWhiteSpace(
                            PropertyBagDictionary.Instance.GetProperty<string>(
                                PropertyBagConstants.InstallationLocation)))
                        {
                            PropertyBagDictionary.Instance.SafeAdd(
                                PropertyBagConstants.InstallationLocation,
                                SetupHelper.GetCSInstalledLocation());
                        }

                        UploadServiceLogs();
                        UploadUnifiedSetupLogs();
                        UploadPushInstallLogs();
                        UploadAgentSetupLogs();
                        UploadScaleOutUnitLogs();
                        UploadCSLogs();
                        UploadPushInstallJobs();
                    }
                    catch (Exception ex)
                    {
                        Trc.Log(LogLevel.Error,
                            "Exception at UploadLogs - {0}", ex);
                    }

                    // Sleep.
                    TimeSpan interval = new TimeSpan(0, 30, 0);
                    Trc.Log(LogLevel.Always, "Sleep for {0} mins", interval);
                    Thread.Sleep(interval);
                    Trc.Log(LogLevel.Always, "");
                }
            }
        }

        /// <summary>
        /// Log intilialization.
        /// </summary>
        internal void InitializeTrace()
        {
            try
            {
                string serviceLogDir =
                    Path.GetPathRoot(Environment.SystemDirectory) +
                    UnifiedSetupConstants.LogUploadServiceLogPath;
                string logFilePath =
                    Path.Combine(
                        serviceLogDir,
                        string.Format(
                            UnifiedSetupConstants.LogUploadServiceLogName,
                            DateTime.UtcNow.Ticks));
                string integrityCheckFilePath =
                    Path.Combine(
                        serviceLogDir,
                        string.Format(
                            UnifiedSetupConstants.IntegrityCheckerLogName,
                            DateTime.UtcNow.Ticks));

                if (!Directory.Exists(serviceLogDir))
                {
                    Directory.CreateDirectory(serviceLogDir);
                }

                Trc.Initialize(
                    (LogLevel.Always | LogLevel.Debug | LogLevel.Error | LogLevel.Warn | LogLevel.Info),
                    logFilePath);
                DRSetupFramework.Trc.Initialize(
                    DRSetupFramework.LogLevel.Always,
                    integrityCheckFilePath);

                Trc.Log(LogLevel.Always, "serviceLogDir - {0}", serviceLogDir);
                Trc.Log(LogLevel.Always, "logFilePath - {0}", logFilePath);
                Trc.Log(LogLevel.Always, "integrityCheckFilePath - {0}", integrityCheckFilePath);

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.DefaultLogName,
                    logFilePath);

                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.IntegrityCheckLogName,
                    integrityCheckFilePath);

                Trc.Log(LogLevel.Always, "Application Started");
            }
            catch (Exception ex)
            {
                Console.WriteLine("Exception occurred - {0}", ex);
            }
        }

        # endregion internal methods

        # region private methods

        /// <summary>
        /// Upload available Unified setup logs.
        /// </summary>
        private void UploadUnifiedSetupLogs()
        {
            try
            {
                string unifiedSetupLogsFolder =
                    Path.GetPathRoot(Environment.SystemDirectory) +
                    UnifiedSetupConstants.UnifiedSetupLogFolder;
                string unifiedSummaryLogsFolder =
                    Path.GetPathRoot(Environment.SystemDirectory) +
                    UnifiedSetupConstants.DRALogFolder;
                Trc.Log(LogLevel.Always, "unifiedSetupLogsFolder - {0}",
                    unifiedSetupLogsFolder);
                Trc.Log(LogLevel.Always, "unifiedSummaryLogsFolder - {0}",
                    unifiedSummaryLogsFolder);

                // All PSMT setup related folders.
                List<string> PsMtLogDirs = new List<string>()
            {
                unifiedSetupLogsFolder,
                unifiedSummaryLogsFolder
            };

                foreach (string dir in PsMtLogDirs)
                {
                    SetupProperties setupProperties;
                    GetSetupProperties(out setupProperties);

                    Trc.Log(LogLevel.Always, "Product name - {0}",
                        setupProperties.ProductName);
                    Trc.Log(LogLevel.Always, "MachineIdentifier - {0}",
                        setupProperties.MachineIdentifier);
                    Trc.Log(LogLevel.Always, "HostName - {0}",
                        setupProperties.HostName);

                    if (Directory.Exists(dir))
                    {
                        UploadLogsFromDirectory(
                            setupProperties,
                            dir,
                            false);
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadUnifiedSetupLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Upload available PushInstall logs.
        /// </summary>
        private void UploadPushInstallLogs()
        {
            try
            {
                string pushInstallLogsUploadDir = Path.Combine(
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation),
                    UnifiedSetupConstants.PushInstallLogsFolder);
                Trc.Log(LogLevel.Always,
                    "pushInstallLogsUploadDir - {0}",
                    pushInstallLogsUploadDir);

                if (Directory.Exists(pushInstallLogsUploadDir))
                {
                    string productName =
                        UnifiedSetupConstants.PushInstallComponentName;
                    Trc.Log(LogLevel.Always, "PushInstallComponentName - {0}",
                        productName);
                    ParseAndUploadLogsFromDirectory(
                        pushInstallLogsUploadDir,
                        productName);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadPushInstallLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Upload available PushInstall job logs.
        /// </summary>
        private void UploadPushInstallJobs()
        {
            try
            {
                string pushInstallJobsUploadDir = Path.Combine(
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation),
                    UnifiedSetupConstants.PushInstallJobsFolder);
                Trc.Log(LogLevel.Always,
                    "Push insatll jobs directory - {0}",
                    pushInstallJobsUploadDir);

                if (Directory.Exists(pushInstallJobsUploadDir))
                {
                    SetupProperties setupProperties;
                    GetSetupProperties(out setupProperties);
                    setupProperties.ProductName = UnifiedSetupConstants.PushInstallJobsName;
                    Trc.Log(LogLevel.Always, "Product name - {0}", setupProperties.ProductName);

                    // Parse the metadata file content and assign the key/value to setupproperties dictionary.
                    string metadataFilePath = Path.Combine(
                        pushInstallJobsUploadDir,
                        UnifiedSetupConstants.MetadataJsonFileName);
                    if (File.Exists(metadataFilePath))
                    {
                        var values = ParseMetadataFile(metadataFilePath);
                        foreach (KeyValuePair<string, string> kvp in values)
                        {
                            Trc.Log(LogLevel.Always, "{0}: {1}", kvp.Key, kvp.Value);
                            setupProperties.SetupPropertiesDictionary.Add(kvp.Key, kvp.Value);
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Error, "metadata file '{0}' is not available.", metadataFilePath);
                    }

                    UploadSpecifiedLogsFromDirectory(
                        setupProperties,
                        pushInstallJobsUploadDir,
                        UnifiedSetupConstants.LogFiles);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadPushInstallJobs - {0}", ex);
            }
        }

        /// <summary>
        /// Upload available Scaleout unit logs.
        /// </summary>
        private void UploadScaleOutUnitLogs()
        {
            try
            {
                string scaleOutUnitLogsUploadDir = Path.Combine(
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation),
                    UnifiedSetupConstants.ScaleOutUnitLogsFolderName);
                Trc.Log(LogLevel.Always,
                    "scaleOutUnitLogsUploadDir - {0}",
                    scaleOutUnitLogsUploadDir);

                if (Directory.Exists(scaleOutUnitLogsUploadDir))
                {
                    string productName =
                        UnifiedSetupConstants.ScaleOutUnitComponentName;
                    Trc.Log(LogLevel.Always, "productName - {0}",
                        productName);
                    ParseAndUploadLogsFromDirectory(
                        scaleOutUnitLogsUploadDir,
                        productName);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadScaleOutUnitLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Upload available agent setup logs.
        /// </summary>
        private void UploadAgentSetupLogs()
        {
            try
            {
                string agentLogsUploadDir = Path.Combine(
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation),
                    UnifiedSetupConstants.AgentSetupLogsUploadFolder);
                Trc.Log(LogLevel.Always, "agentLogsUploadDir - {0}",
                    agentLogsUploadDir);

                if (Directory.Exists(agentLogsUploadDir))
                {
                    List<string> PossibleInstallTypes = new List<string>()
                {
                    UnifiedSetupConstants.PushInstall,
                    UnifiedSetupConstants.CommandLineInstall
                };

                    foreach (string installType in PossibleInstallTypes)
                    {
                        string agentLogsDir = Path.Combine(
                            agentLogsUploadDir,
                            installType);
                        Trc.Log(LogLevel.Always,
                            "Agent logs directory - {0}", agentLogsDir);

                        string productName =
                            UnifiedSetupConstants.UnifiedAgentSetupLogsComponentName;
                        string invocationType = installType;
                        ParseAndUploadLogsFromDirectory(
                            agentLogsDir,
                            productName,
                            invocationType);
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadAgentSetupLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Uploads CS operation logs.
        /// </summary>
        private void UploadCSLogs()
        {
            try
            {
                string csOpLogsDir = Path.Combine(
                    PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation),
                    UnifiedSetupConstants.VarFolderName,
                    UnifiedSetupConstants.CSTelemetryLogsFolder);
                string logUploadStatusFile = Path.Combine(
                    csOpLogsDir,
                    UnifiedSetupConstants.UploadStatusJson);
                Trc.Log(LogLevel.Always, "CS Operation Logs Dir - {0}", csOpLogsDir);

                ParseAndUploadLogsFromDirectory(
                    csOpLogsDir,
                    UnifiedSetupConstants.CSOperationLogsComponentName,
                    uploadStatusFile: logUploadStatusFile);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadCSLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Search for available logs and upload them to AppInsights.
        /// </summary>
        /// <param name="LogsDir">Logs available folder.</param>
        /// <param name="productName">Product name</param>
        /// <param name="invocationType">Invocation type</param>
        /// <param name="uploadStatusFile">upload status file</param>
        private void ParseAndUploadLogsFromDirectory(
            string LogsDir,
            string productName = null,
            string invocationType = null,
            string uploadStatusFile = null)
        {
            try
            {
                Trc.Log(LogLevel.Always, "Begin ParseAndUploadLogsFromDirectory.");

                if (Directory.Exists(LogsDir))
                {
                    string[] Dirs = Directory.GetDirectories(LogsDir);
                    Parallel.ForEach(
                        Dirs,
                        new ParallelOptions { 
                            MaxDegreeOfParallelism = UnifiedSetupConstants.MaxDegreeOfParallelism },
                        dir =>
                        {
                            SetupProperties setupProperties;
                            GetSetupProperties(out setupProperties);
                            setupProperties.ProductName = productName;
                            setupProperties.InvocationType = invocationType;

                            Trc.Log(LogLevel.Always,
                                "Parsing directory {0}",
                                dir);

                            string dirName = Path.GetFileNameWithoutExtension(dir);
                            Trc.Log(LogLevel.Always,
                                "dirName - {0}", dirName);

                            bool uploadLogs = true;

                            if (productName == UnifiedSetupConstants.CSOperationLogsComponentName)
                            {
                                // Parse the metadata file content and assign the key/value to setupproperties dictionary.
                                string metadataFilePath = Path.Combine(
                                    dir,
                                    UnifiedSetupConstants.MetadataJsonFileName);
                                if (File.Exists(metadataFilePath))
                                {
                                    var values = ParseMetadataFile(metadataFilePath);
                                    foreach (KeyValuePair<string, string> kvp in values)
                                    {
                                        Trc.Log(LogLevel.Always, "{0}: {1}", kvp.Key, kvp.Value);
                                        setupProperties.SetupPropertiesDictionary.Add(kvp.Key, kvp.Value);
                                    }
                                }
                                else
                                {
                                    Trc.Log(LogLevel.Error, "metadata file '{0}' is not available.", metadataFilePath);
                                    uploadLogs = false;
                                }
                            }
                            else if ((invocationType == UnifiedSetupConstants.CommandLineInstall) ||
                                (productName == UnifiedSetupConstants.ScaleOutUnitComponentName))
                            {
                                Guid machineIdentifier = Guid.Empty;
                                string hostName = string.Empty;
                                int MinComponentCount = 2;
                                string[] filePathComponents = Regex.Split(
                                    dirName, 
                                    UnifiedSetupConstants.TelemetryLogNameComponentSeperator);
                                Trc.Log(LogLevel.Always, "Word count - {0}", filePathComponents.Count());
                                if (filePathComponents.Count() >= MinComponentCount)
                                {
                                    if (!Guid.TryParse(filePathComponents[0], out machineIdentifier))
                                    {
                                        Trc.Log(LogLevel.Always, "Guid tryparse failed.");
                                    }

                                    hostName = filePathComponents[1];
                                }
                                else
                                {
                                    Trc.Log(LogLevel.Info,
                                        "Directory - {0} is not created in appropriate format.",
                                        dirName);
                                }

                                Trc.Log(LogLevel.Always,
                                    "Machine identifier - {0}",
                                    machineIdentifier);
                                Trc.Log(LogLevel.Always,
                                    "HostName - {0}",
                                    hostName);

                                if (invocationType == UnifiedSetupConstants.CommandLineInstall)
                                {
                                    setupProperties.AgentMachineIdentifier = machineIdentifier;
                                    setupProperties.AgentHostName = hostName;
                                }
                                else
                                {
                                    setupProperties.MachineIdentifier = machineIdentifier;
                                    setupProperties.HostName = hostName;
                                }
                            }
                            else if (productName == UnifiedSetupConstants.PushInstallComponentName ||
                                invocationType == UnifiedSetupConstants.PushInstall)
                            {
                                string emptyGuid = Guid.Empty.ToString();
                                setupProperties.ClientRequestId = emptyGuid;
                                setupProperties.ActivityId = emptyGuid;
                                setupProperties.ServiceActivityId = emptyGuid;
                                string metadataFilePath = Path.Combine(
                                    dir, 
                                    UnifiedSetupConstants.MetadataJsonFileName);

                                var values = ParseMetadataFile(metadataFilePath);
                                foreach (KeyValuePair<string, string> kvp in values)
                                {
                                    Trc.Log(LogLevel.Always, "{0}: {1}", kvp.Key, kvp.Value);
                                    setupProperties.SetupPropertiesDictionary.Add(kvp.Key, kvp.Value);
                                }

                                // Create PushInstall summary Json if it doesn't exist in the directory.
                                if (!CheckForFileExistsInDir(dir, UnifiedSetupConstants.PISummaryFilePattern))
                                {
                                    Trc.Log(LogLevel.Always, "Creating summary Json file as it doesnt exist in the directory.");
                                    string summaryFileName = Path.Combine(dir, UnifiedSetupConstants.PISummaryFileName);
                                    PushInstallJsonProperties piJsonProps = new PushInstallJsonProperties()
                                    {
                                        Message = StringResources.PushInstallSummaryFileNotAvailable
                                    };

                                    JsonHelper jsonHelper = JsonHelper.Instance();
                                    jsonHelper.AddContent(summaryFileName, piJsonProps);
                                }

                                // Create PushInstall verbose log if it doesn't exist in the directory.
                                if (!CheckForFileExistsInDir(dir, UnifiedSetupConstants.PIVerboseFilePattern))
                                {
                                    Trc.Log(LogLevel.Always, "Creating verbose file as it doesnt exist in the directory.");
                                    string verboseFileName = Path.Combine(dir, UnifiedSetupConstants.PIVerboseFileName);
                                    CreateFile(verboseFileName, StringResources.PushInstallVerboseFileNotAvailable);
                                }
                            }
                            else
                            {
                                setupProperties.ActivityId = dirName;
                                Trc.Log(LogLevel.Always,
                                    "ActivityId - {0}",
                                    setupProperties.ActivityId);
                            }

                            // Upload logs from directory.
                            if (uploadLogs)
                            {
                                UploadLogsFromDirectory(
                                    setupProperties,
                                    dir,
                                    statusFile: uploadStatusFile);
                            }

                            // Delete empty folders.
                            if (Directory.GetFiles(dir).Length == 0)
                            {
                                Trc.Log(
                                    LogLevel.Always,
                                    "Deleting directory - {0} as it is empty.",
                                    dir);
                                Directory.Delete(dir);
                            }
                        });
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at ParseAndUploadLogsFromDirectory - {0}", ex);
            }
        }

        /// <summary>
        /// Upload files to telemetry/app insights.
        /// </summary>
        /// <param name="setupProperties">Setup properties to upload</param>
        /// <param name="directoryName">Upload logs under specified directory name</param>
        /// <param name="deleteFile">true to delete file</param>
        /// <param name="statusFile">upload status file</param>
        private void UploadLogsFromDirectory(
            SetupProperties setupProperties,
            string directoryName,
            bool deleteFile = true,
            string statusFile = null)
        {
            Trc.Log(LogLevel.Always, "Uploading logs from directory - {0}", directoryName);
            Trc.Log(LogLevel.Always, setupProperties.ToString());
            string[] LogFiles = Directory.GetFiles(directoryName);
            string uploadedLogsFolder = Path.Combine(
                directoryName, 
                UnifiedSetupConstants.UploadedLogsFolder);
            if (LogFiles.Length > 0)
            {
                foreach (string fileName in LogFiles)
                {
                    bool isFileuploaded = false;
                    bool isFileTypeJson = 
                        fileName.EndsWith(".json");

                    isFileuploaded = (isFileTypeJson) ?
                        logUploader.LogUpload(
                            setupProperties,
                            fileName,
                            LogType.Summary) :
                        logUploader.LogUpload(
                            setupProperties,
                            fileName,
                            LogType.Verbose);

                    if (isFileuploaded)
                    {
                        Trc.Log(
                            LogLevel.Always, 
                            "Upload succeeded for file - {0}", 
                            fileName);
                        if (deleteFile)
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Deleting file {0}.",
                                fileName);
                            File.Delete(fileName);
                        }
                        else
                        {
                            if(!Directory.Exists(uploadedLogsFolder))
                            {
                                Directory.CreateDirectory(uploadedLogsFolder);
                            }
                            string destFile = Path.Combine(
                                uploadedLogsFolder, 
                                string.Concat(
                                    SetupHelper.CompressDate(DateTime.UtcNow),
                                    Path.GetFileName(fileName)));
                            Trc.Log(
                                LogLevel.Always,
                                "Moving file {0} to {1} .",
                                fileName,
                                destFile);
                            File.Move(
                                fileName,
                                destFile);
                        }

                        // Write timestamp to status file.
                        if (!string.IsNullOrWhiteSpace(statusFile))
                        {
                            WriteTimeStampToFile(statusFile);
                        }
                    }
                    else
                    {
                        if (setupProperties.ProductName == UnifiedSetupConstants.PushInstallComponentName ||
                            setupProperties.InvocationType == UnifiedSetupConstants.PushInstall)
                        {
                            Trc.Log(
                                LogLevel.Always, 
                                string.Format("Failed to upload file {0} with ClientRequestId : {1}, ActivityId : {2},  ServiceActivityId : {3}.", 
                                    fileName,
                                    setupProperties.ClientRequestId,
                                    setupProperties.ActivityId,
                                    setupProperties.ServiceActivityId));
                        }
                        else
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Failed to upload file {0}.",
                                fileName);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Upload specific file format files to app insights.
        /// </summary>
        /// <param name="setupProperties">Setup properties to upload</param>
        /// <param name="directoryName">Upload logs under specified directory name</param>
        /// <param name="searchPattern">file format to search</param>
        private void UploadSpecifiedLogsFromDirectory(
            SetupProperties setupProperties,
            string directoryName,
            string searchPattern)
        {
            try
            {
                Trc.Log(LogLevel.Always,
                    "Uploading {0} files from directory - {1}",
                    searchPattern,
                    directoryName);
                Trc.Log(LogLevel.Always, setupProperties.ToString());
                string[] LogFiles = Directory.GetFiles(directoryName, searchPattern);
                if (LogFiles.Length > 0)
                {
                    foreach (string fileName in LogFiles)
                    {
                        bool isFileuploaded = logUploader.LogUpload(
                            setupProperties,
                            fileName,
                            (fileName.EndsWith(UnifiedSetupConstants.JsonFiles)) ?
                            LogType.Summary :
                            LogType.Verbose);

                        if (isFileuploaded)
                        {
                            Trc.Log(LogLevel.Always, "Upload succeeded for file - {0}", fileName);
                            File.Delete(fileName);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "Failed to upload file {0}.", fileName);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadSpecifiedLogsFromDirectory - {0}", ex);
            }
        }

        /// <summary>
        /// Upload service related logs to app insights.
        /// </summary>
        private void UploadServiceLogs()
        {
            try
            {
                string serviceLogName = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.DefaultLogName);
                string icLogName = PropertyBagDictionary.Instance.GetProperty<string>(
                    PropertyBagConstants.IntegrityCheckLogName);
                string serviceLogDir = 
                    Path.GetPathRoot(Environment.SystemDirectory) +
                    UnifiedSetupConstants.LogUploadServiceLogPath;
                string vaultFilePath = Path.Combine(
                    serviceLogDir,
                    UnifiedSetupConstants.VaultCredsFileName);
                this.CopyRequiredServiceLogs();
                string[] ServiceLogFiles = Directory.GetFiles(serviceLogDir);
                
                Trc.Log(LogLevel.Always, "serviceLogName - {0}", serviceLogName);
                Trc.Log(LogLevel.Always, "icLogName - {0}", icLogName);
                Trc.Log(LogLevel.Always, "serviceLogDir - {0}", serviceLogDir);

                // Upload service related logs.
                foreach (string logfile in ServiceLogFiles)
                {
                    SetupProperties setupProperties;
                    GetSetupProperties(out setupProperties);
                    setupProperties.ProductName =
                        UnifiedSetupConstants.LogUploadServiceComponentName;

                    Trc.Log(LogLevel.Always, setupProperties.ToString());

                    if (logfile != serviceLogName &&
                        logfile != icLogName &&
                        logfile != vaultFilePath)
                    {
                        Trc.Log(LogLevel.Always, "Uploading service log - {0}", logfile);
                        if (logUploader.LogUpload(
                                setupProperties,
                                logfile,
                                LogType.Verbose))
                        {
                            Trc.Log(
                                LogLevel.Always,
                                "Deleting file - {0} as upload succeeded.",
                                logfile);
                            File.Delete(logfile);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadServiceLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Upload service logs to app insights.
        /// </summary>
        private void CopyRequiredServiceLogs()
        {
            try
            {
                string csInstallLocation = PropertyBagDictionary.Instance.GetProperty<string>(
                        PropertyBagConstants.InstallationLocation);
                string agentInstallLocation = SetupHelper.GetAgentInstalledLocation();
                string serviceLogDir =
                    Path.GetPathRoot(Environment.SystemDirectory) +
                    UnifiedSetupConstants.LogUploadServiceLogPath;
                bool isTmansvcServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.TmansvcServiceName);
                bool isPushInstallServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.PushInstallServiceName);
                bool isCxPsServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.CxPsServiceName);
                bool isSvagentsServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.SvagentsServiceName);
                bool isAppServiceRunning = ServiceControlFunctions.IsEnabledAndRunning(
                    UnifiedSetupConstants.AppServiceName);

                List<string> FilesList = new List<string>();

                if (!isTmansvcServiceRunning)
                {
                    FilesList.Add(Path.Combine(csInstallLocation, UnifiedSetupConstants.TmansvcServiceLog));
                }

                if (!isPushInstallServiceRunning)
                {
                    FilesList.Add(Path.Combine(csInstallLocation, UnifiedSetupConstants.PushInstallServiceLog));
                }

                if (!isCxPsServiceRunning)
                {
                    FilesList.Add(Path.Combine(csInstallLocation, UnifiedSetupConstants.CxPsErrorServiceLog));
                    FilesList.Add(Path.Combine(csInstallLocation, UnifiedSetupConstants.CxPsMonitorServiceLog));
                    FilesList.Add(Path.Combine(csInstallLocation, UnifiedSetupConstants.CxPsXferServiceLog));
                }

                if (!isSvagentsServiceRunning)
                {
                    // Add svagents logs.
                    string[] svagentsLogs = Directory.GetFiles(agentInstallLocation, "svagents*.log");
                    foreach (string logFile in svagentsLogs)
                    {
                        FilesList.Add(logFile);
                    }
                }

                if (!isAppServiceRunning)
                {
                    // Add appservice logs.
                    string[] appserviceLogs = Directory.GetFiles(agentInstallLocation, "appservice*.log");
                    foreach (string logFile in appserviceLogs)
                    {
                        FilesList.Add(logFile);
                    }
                }

                SetupProperties setupProperties;
                GetSetupProperties(out setupProperties);
                foreach (string fileName in FilesList)
                {
                    string destFile = Path.Combine(serviceLogDir, Path.GetFileName(fileName));
                    Trc.Log(LogLevel.Always, "Copying file - {0} to {1}", fileName, destFile);
                    File.Copy(fileName, destFile, true);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at UploadRequiredAdditionalLogs - {0}", ex);
            }
        }

        /// <summary>
        /// Get setup properties.
        /// </summary>
        /// <param name="out setupProperties">Setup properties to upload</param>
        private void GetSetupProperties(
            out SetupProperties setupProperties)
        {
            setupProperties = new SetupProperties();
            Guid machineidentifier = Guid.Empty;
            Guid.TryParse(
                SetupHelper.GetMachineIdentifier(),
                out machineidentifier);
            setupProperties.VaultLocation = (string)Registry.GetValue(
                UnifiedSetupConstants.CSPSRegistryName,
                UnifiedSetupConstants.VaultGeoRegKeyName,
                string.Empty);
            setupProperties.CSPSMTVersion = (string)Registry.GetValue(
                UnifiedSetupConstants.CSPSRegistryName,
                UnifiedSetupConstants.BuildVersionRegKeyName,
                string.Empty);
            setupProperties.MachineIdentifier = machineidentifier;
            setupProperties.HostName = Dns.GetHostName();
            setupProperties.BiosHardwareId = SetupHelper.GetBiosHardwareId();
            setupProperties.SetupPropertiesDictionary = new Dictionary<string, string>();
        }

        /// <summary>
        /// Parse the metadata json file.
        /// </summary>
        /// <param name="metadataFilePath"></param>
        /// <returns>dictionary of json contents</returns>
        private Dictionary<string, string> ParseMetadataFile(
            string metadataFilePath)
        {
            Dictionary<string, string> values = new Dictionary<string, string>();

            Trc.Log(LogLevel.Always, "metadata file name - {0}", metadataFilePath);
            if (File.Exists(metadataFilePath))
            {
                using (StreamReader sr = new StreamReader(metadataFilePath))
                {
                    string jsonContent = sr.ReadToEnd();
                    if (!string.IsNullOrWhiteSpace(jsonContent))
                    {
                        values = JsonConvert.DeserializeObject<Dictionary<string, string>>(jsonContent);
                    }
                }
            }
            else
            {
                Trc.Log(LogLevel.Error, "metadata file '{0}' is not available.", metadataFilePath);
            }

            return values;
        }

        /// <summary>
        /// Writes the time stamp to file.
        /// </summary>
        /// <param name="FilePath">full path of file name</param>
        private void WriteTimeStampToFile(string filePath)
        {
            try
            {
                Trc.Log(LogLevel.Always, "Writing time stamp to file - {0}", filePath);

                JsonProperties jsonProps = new JsonProperties();
                jsonProps.LastUploadTime = DateTime.Now.ToString();

                JsonHelper createJson = JsonHelper.Instance();
                createJson.AddContent(filePath, jsonProps);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at WriteTimeStampToFile - {0}", ex);
            }
        }

        /// <summary>
        /// Check for required file format files existence in the directory.
        /// </summary>
        /// <param name="dirName">full path name of the directory</param>
        /// <param name="searchPattern">required search pattern</param>
        /// <returns>true if file exists, else false</returns>
        private bool CheckForFileExistsInDir(
            string dirName, 
            string searchPattern)
        {
            try
            {
                return (Directory.GetFiles(dirName, searchPattern).Length > 0);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at CheckForFileExistsInDir - {0}", ex);

                return false;
            }
        }

        /// <summary>
        /// Create log file.
        /// </summary>
        /// <param name="filePath">full path of file name</param>
        /// <param name="message">message to write</param>
        private void CreateFile(string filePath, string message)
        {
            try
            {
                using (StreamWriter sw = new StreamWriter(filePath, false))
                {
                    sw.Write(message);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error,
                    "Exception at CreateFile - {0}", ex);
            }
        }

        #endregion private methods

    }
}
