using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using LoggerInterface;
using MarsAgent.CBEngine;
using MarsAgent.CBEngine.Constants;
using MarsAgent.CBEngine.LogUploader;
using MarsAgent.CBEngine.Parser;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.Constants;
using MarsAgent.LoggerInterface;
using MarsAgent.Utilities;
using Microsoft.ServiceBus.Messaging;
using RcmAgentCommonLib.Config.SerializedContracts;
using RcmAgentCommonLib.ErrorHandling;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using RcmAgentCommonLib.Utilities;
using RcmAgentCommonLib.Utilities.Win32NativeRoutines;
using RcmContract;

namespace MarsAgent.Service.CBEngine
{
    /// <summary>
    /// Defines class implementing a periodic task to upload on-premise agent log to eventhub or
    /// ApplicationInsight based on the component registration state.
    /// </summary>
    /// <typeparam name="Tconfig">The type parameter name for the service configuration.
    /// </typeparam>
    /// <typeparam name="Tsettings">The type parameter name for the agent settings.</typeparam>
    /// <typeparam name="TlogContext">The type parameter name for the on-premise service logging
    /// context.</typeparam>
    public class CBEngineLogUploadTask<Tconfig, Tsettings, TlogContext>
        : RcmAgentPeriodicTaskBase<TlogContext>
        where Tconfig : ServiceConfigurationSettingsBase, new()
        where Tsettings : AgentSettingsBase, new()
        where TlogContext : RcmAgentCommonLogContext, new()
    {
        /// <summary>
        /// Map with log directory names as keys and corresponding size (in bytes) as values.
        /// </summary>
        private Dictionary<string, long> logDirNameToDirSizeMap =
            new Dictionary<string, long>();

        /// <summary>
        /// The client request ID.
        /// </summary>
        private string clientRequestId = Guid.NewGuid().ToString();

        /// <summary>
        /// The last log cleanup time.
        /// </summary>
        private DateTime lastLogCleanupTime = DateTime.UtcNow;

        /// <summary>
        /// Size of a cluster (in bytes) on disk.
        /// </summary>
        private uint diskClusterSize = 0;

        /// <summary>
        /// The max error log file size.
        /// </summary>
        private const int MaxErrorLogFileSizeInBytes = 1024 * 1024;

        /// <summary>
        /// The max error log file count.
        /// </summary>
        private const int MaxErrorLogFileCount = 3;

        /// <summary>
        /// The error log file name.
        /// </summary>
        private const string ErrorLogFileName = "UploadError.log";

        /// <summary>
        /// Initializes a new instance of the
        /// <see cref="CBEngineLogUploadTask{Tconfig, Tsettings, TlogContext}"/> class.
        /// </summary>
        /// <param name="tunables">Task tunables.</param>
        /// <param name="logContext">Task logging context.</param>
        /// <param name="logger">Logger interface.</param>
        /// <param name="serviceConfig">Service configuration.</param>
        /// <param name="serviceSettings">Service settings.</param>
        public CBEngineLogUploadTask(
            ServiceTunables tunables,
            LogTunables logTunables,
            TlogContext logContext,
            RcmAgentCommonLogger<TlogContext> logger,
            IServiceConfiguration<Tconfig> serviceConfig,
            IServiceSettings<Tsettings> serviceSettings,
            IServiceProperties serviceProperties)
            : base(
                  tunables,
                  logContext,
                  logger)
        {
            this.LogUploadTaskTunables = tunables;
            this.ServiceConfig = serviceConfig;
            this.ServiceSettings = serviceSettings;
            this.ServiceProperties = serviceProperties;
        }

        /// <inheritdoc/>
        public sealed override string TaskType => "CBEngineLogUploadTask";

        /// <inheritdoc/>
        public sealed override string Identifier => this.TaskType;

        /// <inheritdoc/>
        protected sealed override string ClientRequestId => this.clientRequestId;

        /// <inheritdoc/>
        protected sealed override string ActivityId => Constants.WellKnownActivityId.CBEngineLogUploadActivityId;

        /// <inheritdoc/>
        protected sealed override string ServiceActivityId => this.ActivityId;

        /// <inheritdoc/>
        protected sealed override bool IsWorkCompletedInternal => false;

        /// <summary>
        /// Gets the task tunables.
        /// </summary>
        private ServiceTunables LogUploadTaskTunables { get; }

        /// <summary>
        /// Gets the service configuration.
        /// </summary>
        private IServiceConfiguration<Tconfig> ServiceConfig { get; }

        /// <summary>
        /// Gets the service settings.
        /// </summary>
        private IServiceSettings<Tsettings> ServiceSettings { get; }

        /// <summary>
        /// Gets the service properties.
        /// </summary>
        private IServiceProperties ServiceProperties { get; }

        /// <inheritdoc/>
        protected override TimeSpan TimeIntervalBetweenRuns =>
            LogUploadTaskTunables.LogUploadInterval;

        /// <inheritdoc/>
        protected sealed override bool IsReadyToRun()
        {
            Tconfig svcConfig = this.ServiceConfig.GetConfig();
            return svcConfig.IsConfigured;
        }

        /// <inheritdoc/>
        protected sealed override void MergeLogContext()
        {
            // No-Op.
        }

        /// <inheritdoc/>
        protected sealed override void SendStopRequestInternal()
        {
            // No-Op.
        }

        /// <inheritdoc/>
        protected sealed override void CleanupOnTaskCompletion()
        {
            // No-Op.
        }

        /// <inheritdoc/>
        protected sealed override void PerformWorkOneInvocation()
        {
            //starting stop watch to log time taken for process
            var stopwatch = Stopwatch.StartNew();

            // Set client request ID per invocation.
            this.clientRequestId = Guid.NewGuid().ToString();
            this.SetLogContext();

            Tconfig config = this.ServiceConfig.GetConfig();

            var logFolderPath = Path.Combine(config.LogFolderPath, "CBEngine");
            this.InitializeLogRetentionInfo(logFolderPath);

            LogUploaderBase logUploader;
            if (!this.TryGetLogUploader(logFolderPath, config, out logUploader))
            {
                return;  // <---- Early return.
            }

            this.CancellationToken.ThrowIfCancellationRequested();

            var biosId = SystemUtils.GetBiosId();

            List<string> readyForUploadDirectories = LogUploadHelper.GetReadyForUploadDirectories(
                logFolderPath,
                LogUploadTaskTunables.MaxNumberOfLogDirectoriesToProcess);

            var logParser = new TsvToCsvLogParser();
            // TODO : Move to settings
            TimeSpan timeout = TimeSpan.FromMinutes(5);

            foreach (string readyForUploadDirectory in readyForUploadDirectories)
            {
                try
                {
                    var taskCts = new CancellationTokenSource(timeout);
                    CancellationTokenSource combinedCts = null;
                    CancellationToken combinedCt = taskCts.Token;
                    if (this.CancellationToken.CanBeCanceled)
                    {
                        combinedCts = CancellationTokenSource.CreateLinkedTokenSource(
                            this.CancellationToken, taskCts.Token);
                        combinedCt = combinedCts.Token;
                    }

                    combinedCt.ThrowIfCancellationRequested();

                    using (combinedCts)
                    {
                        List<string> logFiles = LogUploadHelper.GetFilesReadyForUpload(
                            readyForUploadDirectory);
                        if (logFiles.Any())
                        {
                            foreach (string logFile in logFiles)
                            {
                                try
                                {
                                    combinedCt.ThrowIfCancellationRequested();
                                    var parsedLogFile = logParser.FileParser(
                                        logFile, biosId, ServiceProperties.ServiceVersion, combinedCts.Token);

                                    if (!string.IsNullOrWhiteSpace(parsedLogFile))
                                    {
                                        logUploader.ProcessLog(
                                            parsedLogFile,
                                            LogUploadTaskTunables.IsCompressionEnabledForUploadedLogs);
                                    }

                                    // Keep the hearbeat alive for this task.
                                    this.UpdateTaskHeartbeat();
                                }
                                catch (OperationCanceledException) when (taskCts.IsCancellationRequested)
                                {
                                    // Task time-out exception
                                    this.LoggerInstance.LogWarning(
                                        CallInfo.Site(),
                                        "Cancelling the stuck file " + $"'{logFile}'" +
                                        "upload task due to timeout");
                                }
                                catch (OperationCanceledException) when (this.CancellationToken.IsCancellationRequested)
                                {
                                    // Rethrow operation canceled exception if the service is stopped.
                                    this.LoggerInstance.LogVerbose(
                                        CallInfo.Site(),
                                        "Task cancellation exception is while uploading" +
                                        " the files" + $" '{logFile}'");
                                    throw;
                                }
                                catch (ServerBusyException e) when (ExceptionExtensions<TlogContext>.IsEventHubThrottled(e))
                                {
                                    // EventHubUploader server busy exception
                                    this.LoggerInstance.LogWarning(
                                        CallInfo.Site(),
                                        "CBEngineLogUploadTask EventHub uploader server busy. Unable to upload log for : " + 
                                        $"'{logFile}'");

                                    this.LogEventHubUploadFailed(config, e.ToString(), logFile);

                                    throw;
                                }
                                catch (Exception ex)
                                {
                                    this.LoggerInstance.LogException(
                                        CallInfo.Site(),
                                        ex,
                                        $"Error occurred while uploading the file" +
                                        $" '{logFile}'");

                                    this.LogEventHubUploadFailed(config, ex.ToString(), logFile);
                                }
                            }
                        }
                    }
                }
                catch (OperationCanceledException) when (this.CancellationToken.IsCancellationRequested)
                {
                    // Rethrow operation canceled exception if the service is stopped.
                    this.LoggerInstance.LogVerbose(
                        CallInfo.Site(),
                        "Task cancellation exception is throwed");
                    throw;
                }
                catch (Exception ex)
                {
                    this.LoggerInstance.LogException(
                        CallInfo.Site(),
                        ex,
                        $"Error occurred while uploading the files in directory" +
                        $" '{readyForUploadDirectory}'");
                }
            }

            // Cleanup unused log folders.
            if (DateTime.UtcNow.Subtract(this.lastLogCleanupTime).TotalMinutes >
                LogUploadTaskTunables.LogCleanupInterval.TotalMinutes)
            {
                this.RemoveStaleLogDirectories(logFolderPath);
                this.lastLogCleanupTime = DateTime.UtcNow;
            }
            
            stopwatch.Stop();
            this.LoggerInstance.LogInfo(
                CallInfo.Site(),
                $"CBEngineLogUploadTask time taken for one processing for logs : '{stopwatch.ElapsedMilliseconds}' ms");

            logUploader.Flush();
        }

        /// <summary>
        /// Get version number of cbengine exe
        /// </summary>
        /// <returns>Version number of cbengine exe</returns>
        private static string GetCBEngineVersion()
        {
            string cbEngineVersion = string.Empty;
            string cbEnginePath = string.Empty;

            try
            {
                cbEnginePath = Path.Combine(
                    CBEngineConstants.InstallLocation, @"bin\cbengine.exe");
                if (File.Exists(cbEnginePath))
                {
                    cbEngineVersion = FileVersionInfo.GetVersionInfo(
                        cbEnginePath).ProductVersion;
                }

                Logger.Instance.LogVerbose(
                    CallInfo.Site(),
                    $"CBEngine Version : '{cbEngineVersion}'");
            }
            catch (Exception ex)
            {
                Logger.Instance.LogException(
                    CallInfo.Site(), ex, $"Failed to fetch '{cbEnginePath}' version.");
            }

            return cbEngineVersion;
        }

        /// <summary>
        /// Update error log with log upload failure message.
        /// </summary>
        /// <param name="errorMessage">The upload failure error message.</param>
        /// <param name="logFile">The log file that's failed to upload.</param>
        private void LogEventHubUploadFailed(Tconfig config, string errorMessage, string logFile)
        {
            if (!config.IsPrivateEndpointEnabled)
            {
                string errorFilepath = Path.Combine(CBEngineConstants.LogDirPath, ErrorLogFileName);

                if (File.Exists(errorFilepath))
                {
                    long errorFileSize = new FileInfo(errorFilepath).Length;

                    bool isErrorFileReadyForRotation =
                        (errorFileSize + errorMessage.Length) > MaxErrorLogFileSizeInBytes;

                    if (isErrorFileReadyForRotation)
                    {
                        LogUploadHelper.RotateFile(errorFilepath, MaxErrorLogFileCount);
                    }
                }

                File.AppendAllText(errorFilepath, $"{logFile}: {errorMessage}\n");
            }
        }

        /// <summary>
        /// Initializes log retention information.
        /// </summary>
        /// <param name="readyForUploadLogDirPath">Log folder path.</param>
        private void InitializeLogRetentionInfo(string readyForUploadLogDirPath)
        {
            string uploadedLogsDirPath = Path.Combine(
                readyForUploadLogDirPath,
                LogUploadHelper.UploadedDirectoryName);

            if (!Directory.Exists(uploadedLogsDirPath))
            {
                Directory.CreateDirectory(uploadedLogsDirPath);
            }

            if (LogUploadTaskTunables.IsCompressionEnabledForUploadedLogs)
            {
                try
                {
                    using (FileIo fileIo = new FileIo(uploadedLogsDirPath))
                    {
                        fileIo.Open(
                            Win32Native.CreateFileConstants.CreateFileGenericRead |
                            Win32Native.CreateFileConstants.CreateFileGenericWrite,
                            Win32Native.CreateFileConstants.CreateFileShareRead |
                            Win32Native.CreateFileConstants.CreateFileShareWrite,
                            IntPtr.Zero,
                            Win32Native.CreateFileConstants.CreateFileOpenExisting,
                            Win32Native.CreateFileConstants.CreateFileBackupSemantics,
                            IntPtr.Zero);
                        fileIo.EnableCompression();
                        fileIo.Close();
                    }
                }
                catch (Exception ex)
                {
                    this.LoggerInstance.LogException(
                        CallInfo.Site(),
                        ex,
                        $"Error occurred while enabling compression for directory" +
                        $" '{uploadedLogsDirPath}'");

                    //AgentMetrics.LogEnablingFileCompressionFailed(uploadedLogsDirPath);
                }

                string rootPath = new DirectoryInfo(readyForUploadLogDirPath).Root.FullName;
                this.diskClusterSize = DiskUtilities.GetClusterSizeForDisk(rootPath);
            }
        }

        /// <summary>
        /// Removes stale log directories based on limits on log retention size/duration.
        /// </summary>
        /// <param name="readyForUploadLogDirPath">Log folder path.</param>
        private void RemoveStaleLogDirectories(string readyForUploadLogDirPath)
        {
            this.RemoveOlderDirectories(readyForUploadLogDirPath, this.CancellationToken);

            if (LogUploadTaskTunables.LimitLogRetentionBasedOnSize)
            {
                // Limit log retention based on size if the available disk size in
                // percentage and in GB is less then minimum required threshold.
                string rootPath = new DirectoryInfo(readyForUploadLogDirPath).Root.FullName;
                double availableDiskSpacePercentage =
                    DiskUtilities.GetAvailableDiskSpacePercentage(rootPath);
                ulong availableDiskSizeInBytes =
                    DiskUtilities.GetAvailableDiskSizeInBytes(rootPath);

                if (availableDiskSpacePercentage <
                        this.LogUploadTaskTunables.LogPruningAvailableDiskSizePercentageThreshold &&
                    availableDiskSizeInBytes <
                        this.LogUploadTaskTunables.LogPruningAvailableDiskSizeInBytesThreshold)
                {
                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Available disk space percentage '{availableDiskSpacePercentage}'" +
                        $" and size in bytes '{availableDiskSizeInBytes}'.");

                    string uploadedLogDirPath = Path.Combine(
                        readyForUploadLogDirPath,
                        LogUploadHelper.UploadedDirectoryName);

                    var uploadedLogDirInfo = new DirectoryInfo(uploadedLogDirPath);

                    var uploadedLogDirectories = uploadedLogDirInfo.EnumerateDirectories(
                        $"{LogUploadHelper.UploadedSubDirectoryPrefix}*")
                        .OrderBy(dir => dir.CreationTime)
                        .ToList();

                    // Remove the latest logs directory from size based pruning process.
                    if (uploadedLogDirectories.Count > 0)
                    {
                        uploadedLogDirectories.RemoveAt(uploadedLogDirectories.Count - 1);
                    }

                    long logRetentionCurrentSizeInBytes = 0;

                    foreach (var dir in uploadedLogDirectories)
                    {
                        logRetentionCurrentSizeInBytes += this.AddorUpdateLogDirectoryToMap(dir);

                        // Keep the hearbeat alive for this task.
                        this.UpdateTaskHeartbeat();
                    }

                    // Delete uploaded log directories until log retention current size is less
                    // than the maximum allowed size (delete from oldest to newest).
                    foreach (var dir in uploadedLogDirectories)
                    {
                        // Keep the hearbeat alive for this task.
                        this.UpdateTaskHeartbeat();

                        if (logRetentionCurrentSizeInBytes >
                                this.LogUploadTaskTunables.LogRetentionMaximumSizeInBytes)
                        {
                            this.CancellationToken.ThrowIfCancellationRequested();
                            dir.Delete(recursive: true);
                            logRetentionCurrentSizeInBytes -= this.RemoveLogDirectoryFromMap(
                                dir.Name);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Calculates the size of files (in bytes) in a given directory.
        /// </summary>
        /// <param name="directory">Directory info object.</param>
        /// <returns>Size of files in the given directory in bytes.</returns>
        /// <remarks>This helper routine is kept in this class so as to keep updating task
        /// heartbeat while size calculation.</remarks>
        private long CalculateDirectoryFilesSize(
            DirectoryInfo directory)
        {
            long directorySize = 0;

            if (directory.Exists)
            {
                foreach (var filePath in Directory.GetFiles(directory.FullName))
                {
                    var fileIo = new FileIo(filePath);
                    directorySize += fileIo.GetFileSizeOnDisk(this.diskClusterSize);

                    this.CancellationToken.ThrowIfCancellationRequested();

                    // Keep the hearbeat alive for this task.
                    this.UpdateTaskHeartbeat();
                }
            }

            return directorySize;
        }

        /// <summary>
        /// Adds or updates the name of the given log directory and its corresponding size
        /// to the map.
        /// </summary>
        /// <param name="dir">Directory to be added.</param>
        /// <returns>The size of the directory being added.</returns>
        private long AddorUpdateLogDirectoryToMap(DirectoryInfo dir)
        {
            long directorySize = 0;

            // Return the existing directory size if already computed and stored previously in the map.
            // Otherwise for new directory, compute the size and add to the map.
            if (!this.logDirNameToDirSizeMap.TryGetValue(dir.Name, out directorySize))
            {
                directorySize = this.CalculateDirectoryFilesSize(dir);

                if (directorySize > 0)
                {
                    this.logDirNameToDirSizeMap.Add(dir.Name, directorySize);
                }
            }

            return directorySize;
        }

        /// <summary>
        /// Removes the entry corresponding to the given log directory from the map.
        /// </summary>
        /// <param name="directoryName">Name of the log directory to be removed.</param>
        /// <returns>Size of the directory being removed from the map.</returns>
        private long RemoveLogDirectoryFromMap(string directoryName)
        {
            long dirSize = 0;
            if (this.logDirNameToDirSizeMap.TryGetValue(directoryName, out dirSize))
            {
                this.logDirNameToDirSizeMap.Remove(directoryName);
            }

            return dirSize;
        }

        /// <summary>
        /// Gets the log uploader instance.
        /// </summary>
        /// <param name="logFolderPath">CBEngine Log folder path.</param>
        /// <param name="config">The service configuration.</param>
        /// <param name="logUploader">Log uploader.</param>
        /// <returns>True is log uploader is initialized, false otherwise.</returns>
        private bool TryGetLogUploader(string logFolderPath, Tconfig config, out LogUploaderBase logUploader)
        {
            logUploader = null;
            bool returnValue = true;

            if (config.IsRegistered)
            {
                if (config.IsPrivateEndpointEnabled)
                {
                    logUploader = new PrivateEndpointLogUploader(logFolderPath);
                }
                else
                {
                    string telemetryEventHubSasUri;
                    if (this.ServiceSettings.TryGetTelemetryEventHubSasUri(
                        out telemetryEventHubSasUri))
                    {
                        logUploader = new EventHubUploader<LogContext>(
                            telemetryEventHubSasUri,
                            TraceMessagesEventProperties.Instance,
                            Logger.Instance,
                            this.CancellationToken);
                    }
                    else
                    {
                        logUploader = null;
                        returnValue = false;
                    }
                }
            }

            return returnValue;
        }

        /// <summary>
        /// Remove log directories older than backup duration.
        /// </summary>
        /// <param name="logDirPath">The log directory path.</param>
        /// <param name="cancellationToken">Cancellation token.</param>
        private void RemoveOlderDirectories(
            string logDirPath,
            CancellationToken cancellationToken)
        {
            var logDirs = LogUploadHelper.FetchOlderDirectories(
                logDirPath,
                this.LogUploadTaskTunables.LogRetentionDuration);

            foreach (var logDir in logDirs)
            {
                cancellationToken.ThrowIfCancellationRequested();
                logDir.Delete(recursive: true);
                this.RemoveLogDirectoryFromMap(logDir.Name);

                this.UpdateTaskHeartbeat();
            }
        }
    }
}