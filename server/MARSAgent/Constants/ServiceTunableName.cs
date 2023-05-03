namespace MarsAgent.Constants
{
    /// <summary>
    /// Defines enumeration specifying names of configuration variables used by the service.
    /// </summary>
    public enum ServiceTunableName
    {
        /// <summary>
        /// The name of the key whose value is used as maximum wait time for debugger attach on
        /// start.
        /// </summary>
        MaxWaitTimeInSecondsForDebuggerAttachOnStart,

        /// <summary>
        /// The name of the key whose value is used for indicating whether debug logging is enabled
        /// or not.
        /// </summary>
        DebugLoggingEnabled,

        /// <summary>
        /// The name of the key whose value is used for indicating whether to break in debugger on
        /// start.
        /// </summary>
        DebuggerBreakOnStart,

        /// <summary>
        /// The name of the key whose value is used as maximum wait time for service status change.
        /// </summary>
        MaxWaitTimeInSecondsForServiceStatusChange,

        /// <summary>
        /// The name of the key whose value is used as maximum wait time for RCM AMT response.
        /// </summary>
        MaxWaitTimeInSecondsForRcmApiResponse,

        /// <summary>
        /// The name of the key whose value is used as time delay between reattempts on failure
        /// in RCM API invocation.
        /// </summary>
        RcmApiFailureRetryDelayInSeconds,

        /// <summary>
        /// The name of the key  whose value is used as log cleanup interval.
        /// </summary>
        LogCleanupIntervalInMinutes,

        /// <summary>
        /// The name of the key  whose value is used as log process interval.
        /// </summary>
        LogProcessIntervalInMinutes,

        /// <summary>
        /// The name of the key whose value is used as max number of log directories to process.
        /// </summary>
        MaxNumberOfLogDirectoriesToProcess,

        /// <summary>
        /// The name of the key whose value is used as the time interval between successive runs
        /// for log upload.
        /// </summary>
        LogUploadIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as log retention duration.
        /// </summary>
        LogRetentionDurationInDays,

        /// <summary>
        /// The name of the key whose value is used as interval between successive fetch settings
        /// invocation.
        /// </summary>
        FetchSettingsIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as interval between successive consume settings
        /// invocation.
        /// </summary>
        ConsumeSettingsIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as interval between persisting of settings
        /// fetched from RCM.
        /// </summary>
        PersistSettingsIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as the interval for job monitoring.
        /// </summary>
        JobMonitorIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as time delay between reattempts on failure
        /// in starting job execution.
        /// </summary>
        StartJobExecutionFailureRetryDelayInSeconds,

        /// <summary>
        /// The name of the key whose value is used as time delay between reattempts on failure
        /// in tracking of the job execution.
        /// </summary>
        TrackJobExecutionFailureRetryDelayInSeconds,

        /// <summary>
        /// The name of the key whose value is used as time delay between reattempts on failure
        /// in job cleanup.
        /// </summary>
        JobCleanupFailureRetryDelayInSeconds,

        /// <summary>
        /// The name of the key whose value is used as max time interval to wait for the job
        /// to terminate.
        /// </summary>
        MaxWaitTimeForJobTerminationInSeconds,

        /// <summary>
        /// The name of the key whose value is used as maximum wait time for process
        /// to handle quit request.
        /// </summary>
        MaxTimeIntervalForProcessToHonourQuitRequest,

        /// <summary>
        /// The name of the key whose value is used as the count of maximum retries allowed while
        /// invoking mars API for retriable errors.
        /// </summary>
        MaxRetryCountForInvokingMarsApi,

        /// <summary>
        /// The name of the key whose value is used as minimum upload size in bytes.
        /// </summary>
        MinUploadSizeInBytes,

        /// <summary>
        /// The name of the key whose value is used as minimum time to wait (in seconds)
        /// before next upload.
        /// </summary>
        MinTimeToWaitBeforeNextUploadInSec,

        /// <summary>
        /// The name of the key whose value is used as minimum time to wait (in seconds) before
        /// checking if a new file has arrived.
        /// </summary>
        MinTimeToWaitBeforeCheckingNewFileArrivalInSec,

        /// <summary>
        /// The name of the key whose value is used as frequency (in seconds) to drain logs.
        /// </summary>
        DrainLogsFrequencyInSeconds,

        /// <summary>
        /// The name of the key whose value indicates whether mars upload session is
        /// to be logged or not.
        /// </summary>
        ShouldMarsUploadSessionBeLogged,

        /// <summary>
        /// The name of the key whose value is used as back-off time in case of errors
        /// which have to be retried with exponential delay.
        /// </summary>
        DeltaBackoffTimeForRetriableErrorsInSeconds,

        /// <summary>
        /// The name of the key whose value is used as upper limit for
        /// back-off time for retriable errors.
        /// </summary>
        MaxBackoffTimeForRetriableErrorsInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the interval for which the heartbeat
        /// monitor will sleep between runs.
        /// </summary>
        HeartbeatMonitorIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as the interval (in minutes) for
        /// heartbeat monitor to stop.
        /// </summary>
        HeartbeatMonitorStopTimeoutInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the interval (in minutes) for which the
        /// Stop() handler will wait for task to stop.
        /// </summary>
        TaskStopTimeoutInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the threshold value (in minutes) after which
        /// worker is deemed unresponsive if the last heartbeat time is not getting updated.
        /// </summary>
        TaskUnresponsiveThresholdInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the threshold value (in minutes) after which
        /// worker is deemed continuously failing if the last successful run is not getting updated.
        /// </summary>
        TaskFailureThresholdInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the time delay between task execution
        /// attempts on exception.
        /// </summary>
        TaskExecutionRetryDelayInSeconds,

        /// <summary>
        /// The name of the key whose value is used as the threshold value (in minutes) after which
        /// drain acion handler is deemed unresponsive if the last heartbeat time is not getting
        /// updated.
        /// </summary>
        DrainActionHandlerUnresponsiveThresholdInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the interval (in seconds) for which the loop
        /// will sleep between retries for posting quit signal.
        /// </summary>
        PostQuitSignalLoopSleepIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as the max retries for posting quit signal.
        /// </summary>
        MaxRetryCountForPostingQuitSignal,

        /// <summary>
        /// The name of the key whose value is used as the time interval (in minutes) between
        /// periodic registrations.
        /// </summary>
        MinWaitTimeBetweenPeriodicRegistrationInMinutes,

        /// <summary>
        /// The name of the key whose value is used to decide whether to skip server side SSL
        /// certificate validation.
        /// </summary>
        SkipServerSslCertValidation,

        /// <summary>
        /// The name of the key whose value is used to decide whether to enable compression for
        /// uploaded log folders or not.
        /// </summary>
        IsCompressionEnabledForUploadedLogs,

        /// <summary>
        /// The name of the key whose value is used as the maximum count of differential files
        /// to process from process server cache.
        /// </summary>
        MaxCountOfDiffFilesToProcessFromCache,

        /// <summary>
        /// The name of the key whose value is used as the delta backoff time (in seconds)
        /// to retry process creation.
        /// </summary>
        DeltaBackoffTimeToRetryProcessCreationInSeconds,

        /// <summary>
        /// The name of the key whose value is used as the maximum backoff time (in minutes)
        /// to retry process creation.
        /// </summary>
        MaxBackoffTimeToRetryProcessCreationInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the minimum duration (in seconds) to
        /// classify a process run as valid.
        /// </summary>
        ProcessValidRunMinDurationInSeconds,

        /// <summary>
        /// The name of the key whose value is used to decide whether to fail fast on
        /// encountering an unresponsive task or not.
        /// </summary>
        FailFastOnUnresponsiveTask,

        /// <summary>
        /// The name of the key whose value is used to decide whether to limit log retention
        /// on disk based on size or not.
        /// </summary>
        LimitLogRetentionBasedOnSize,

        /// <summary>
        /// The name of the key whose value is used as the maximum size (in bytes) of log
        /// retention allowed on disk if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        LogRetentionMaximumSizeInBytes,

        /// <summary>
        /// The name of the key whose value is used as the threshold value after which health
        /// issue is raised.
        /// </summary>
        HealthIssueOccurrenceCountThreshold,

        /// <summary>
        /// The name of the key whose value is used as the threshold duration after which health
        /// issue is raised.
        /// </summary>
        HealthIssueDurationThresholdInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the time interval between successive runs for
        /// reporting component health.
        /// </summary>
        ReportComponentHealthIntervalInMinutes,

        /// <summary>
        /// The name of the key whose value is used as the threshold duration after which health
        /// issue is discarded.
        /// </summary>
        HealthIssueDiscardDurationThresholdInMinutes,

        /// <summary>
        /// The name of the key whose value is used as interval between successive event hub SAS
        /// URI renewal invocation.
        /// </summary>
        RenewVmMonitoringEventHubSasUriIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value is used as number of threads for SAS URI renewal.
        /// </summary>
        MaxThreadsForVmMonitoringEventHubSasUriRenewal,

        /// <summary>
        /// The name of the key whose value is used to decide whether or not to renew EventHub SAS
        /// URI for VM level monitoring.
        /// </summary>
        IsPeriodicVmMonitoringSasUriRenewalNeeded,

        /// <summary>
        /// The name of the key whose value is used as the interval (in seconds) for which the loop
        /// will sleep between retries for processing cbengine logs.
        /// </summary>
        ProcessCBEngineLogsIntervalInSeconds,

        /// <summary>
        /// The name of the key whose value indicates the security protocol to be used.
        /// </summary>
        SecurityProtocolType,

        /// <summary>
        /// The name of the key whose value indicates the available disk percentage
        /// below which log pruning based on size will be performed.
        /// Applicable if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        LogPruningAvailableDiskSizePercentageThreshold,

        /// <summary>
        /// The name of the key whose value indicates the available disk size in bytes
        /// below which log pruning based on size will be performed.
        /// Applicable if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        LogPruningAvailableDiskSizeInBytesThreshold
    }
}
