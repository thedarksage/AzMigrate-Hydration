using System;
using System.Net;
using Microsoft.ServiceBus;
using MarsAgent.Constants;
using RcmAgentCommonLib.Service;

namespace MarsAgent.Service
{
    /// <summary>
    /// Implements interface for fetching configuration variable values.
    /// </summary>
    public sealed class ServiceTunables : IServiceTunables
    {
        /// <summary>
        /// Singleton instance of <see cref="ServiceTunables"/> used by all callers.
        /// </summary>
        public static readonly ServiceTunables Instance = new ServiceTunables();

        /// <summary>
        /// Prevents a default instance of the <see cref="ServiceTunables"/> class from
        /// being created.
        /// </summary>
        private ServiceTunables()
        {
        }

        /// <summary>
        /// Gets the max wait time for debugger attach on start.
        /// </summary>
        public TimeSpan MaxWaitTimeForDebuggerAttachOnStart
        {
            get
            {
                int waitTimeInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxWaitTimeInSecondsForDebuggerAttachOnStart),
                    300);

                return TimeSpan.FromSeconds(waitTimeInSeconds);
            }
        }

        /// <summary>
        /// Gets the time interval to wait for process to handle quit request.
        /// </summary>
        public TimeSpan MaxTimeIntervalForProcessToHonourQuitRequest
        {
            get
            {
                int waitTimeInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxTimeIntervalForProcessToHonourQuitRequest),
                    90);

                return TimeSpan.FromSeconds(waitTimeInSeconds);
            }
        }

        /// <summary>
        /// Gets a value indicating whether debug logging is enabled or not.
        /// </summary>
        public bool DebugLoggingEnabled
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.DebugLoggingEnabled),
                    false);
            }
        }

        /// <summary>
        /// Gets a value indicating whether to break in debugger on start.
        /// </summary>
        public bool DebuggerBreakOnStart
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.DebuggerBreakOnStart),
                    false);
            }
        }

        /// <summary>
        /// Gets the maximum wait time for service status change.
        /// </summary>
        public TimeSpan MaxWaitTimeForServiceStatusChange
        {
            get
            {
                int waitTimeInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxWaitTimeInSecondsForServiceStatusChange),
                    300);

                return TimeSpan.FromSeconds(waitTimeInSeconds);
            }
        }

        /// <summary>
        /// Gets the maximum wait time for RCM API response.
        /// </summary>
        public TimeSpan MaxWaitTimeForRcmApiResponse
        {
            get
            {
                int waitTimeInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxWaitTimeInSecondsForRcmApiResponse),
                    120);

                return TimeSpan.FromSeconds(waitTimeInSeconds);
            }
        }

        /// <summary>
        /// Gets the time interval between reattempts on failures encountered in RCM API
        /// invocation.
        /// </summary>
        public TimeSpan RcmApiFailureRetryDelay
        {
            get
            {
                int delayInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.RcmApiFailureRetryDelayInSeconds),
                    30);

                return TimeSpan.FromSeconds(delayInSeconds);
            }
        }

        /// <summary>
        /// Gets the log cleanup interval.
        /// </summary>
        public TimeSpan LogCleanupInterval
        {
            get
            {
                int cleanuMTntervalInMinutes = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogCleanupIntervalInMinutes),
                    60);

                return TimeSpan.FromMinutes(cleanuMTntervalInMinutes);
            }
        }

        /// <summary>
        /// Gets the log process interval.
        /// </summary>
        public TimeSpan LogProcessInterval
        {
            get
            {
                int logProcessInterval = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogProcessIntervalInMinutes),
                    5);

                return TimeSpan.FromMinutes(logProcessInterval);
            }
        }

        /// <summary>
        /// Gets the max number of log directories to process.
        /// </summary>
        public int MaxNumberOfLogDirectoriesToProcess
        {
            get
            {
                int maxNumberOfLogDirectoriesToProcess = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxNumberOfLogDirectoriesToProcess),
                    5);

                return maxNumberOfLogDirectoriesToProcess;
            }
        }

        /// <summary>
        /// Gets the interval (in seconds) between periodic log upload.
        /// </summary>
        public TimeSpan LogUploadInterval
        {
            get
            {
                int logUploadInterval = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogUploadIntervalInSeconds),
                    60);

                return TimeSpan.FromSeconds(logUploadInterval);
            }
        }

        /// <summary>
        /// Gets the log retention duration.
        /// </summary>
        public TimeSpan LogRetentionDuration
        {
            get
            {
                int retentionDurationInDays = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogRetentionDurationInDays),
                    7);

                return TimeSpan.FromDays(retentionDurationInDays);
            }
        }

        /// <summary>
        /// Gets the time interval between successive runs for fetching settings from RCM.
        /// </summary>
        public TimeSpan FetchSettingsInterval
        {
            get
            {
                int fetchSettingsIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.FetchSettingsIntervalInSeconds),
                    90);

                return TimeSpan.FromSeconds(fetchSettingsIntervalInSeconds);
            }
        }

        /// <summary>
        /// Gets the time interval between successive runs for consuming settings from RCM.
        /// </summary>
        public TimeSpan ConsumeSettingsInterval
        {
            get
            {
                int consumeSettingsIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.ConsumeSettingsIntervalInSeconds),
                    30);

                return TimeSpan.FromSeconds(consumeSettingsIntervalInSeconds);
            }
        }

        /// <summary>
        /// Gets the time interval between persisting settings from RCM on disk.
        /// </summary>
        public TimeSpan PersistSettingsInterval
        {
            get
            {
                int persistSettingsIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.PersistSettingsIntervalInSeconds),
                    300);

                return TimeSpan.FromSeconds(persistSettingsIntervalInSeconds);
            }
        }

        /// <summary>
        /// Gets the count of maximum retries allowed while invoking mars API for retriable errors.
        /// </summary>
        public int MaxRetryCountForInvokingMarsApi
        {
            get
            {
                int maxRetryCountForInvokingMarsApi = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxRetryCountForInvokingMarsApi),
                    3);

                return maxRetryCountForInvokingMarsApi;
            }
        }

        /// <summary>
        /// Gets the interval (in seconds) for which the main worker loop will sleep between
        /// runs. Keeping it below a minute as normally we want any faulted activity to resume
        /// within a minute of it stopping so the main loop needs to run around that time interval.
        /// </summary>
        public TimeSpan HeartbeatMonitorInterval
        {
            get
            {
                int heartbeatMonitorIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.HeartbeatMonitorIntervalInSeconds),
                    50);

                return TimeSpan.FromSeconds(heartbeatMonitorIntervalInSeconds);
            }
        }

        /// <summary>
        /// Gets the interval (in minutes) for which the Stop() handler will wait for Start() handler
        /// to exit. Service stops are expected to complete within 5 minutes - we will give
        /// ourselves 4 minutes for this.
        /// </summary>
        public TimeSpan HeartbeatMonitorStopTimeout
        {
            get
            {
                int heartbeatMonitorStopTimeoutInMinutes = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.HeartbeatMonitorStopTimeoutInMinutes),
                    4);

                return TimeSpan.FromMinutes(heartbeatMonitorStopTimeoutInMinutes);
            }
        }

        /// <summary>
        /// Gets the interval for task to stop on service shutdown.
        /// Service stops are expected to complete within 5 minutes - we will give
        /// ourselves 3 minutes.
        /// </summary>
        public TimeSpan TaskStopTimeout
        {
            get
            {
                int taskStopTimeoutInMinutes = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.TaskStopTimeoutInMinutes),
                    3);

                return TimeSpan.FromMinutes(taskStopTimeoutInMinutes);
            }
        }

        /// <summary>
        /// Gets the threshold value (in minutes) after which worker is deemed
        /// unresponsive if the last heartbeat time is not getting updated.
        /// </summary>
        public TimeSpan TaskUnresponsiveThreshold
        {
            get
            {
                int taskUnresponsiveThresholdInMinutes = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.TaskUnresponsiveThresholdInMinutes),
                    60);

                return TimeSpan.FromMinutes(taskUnresponsiveThresholdInMinutes);
            }
        }

        /// <summary>
        /// Gets the threshold value after which task is deemed continuously failing if the
        /// last sucessful run time is not getting updated.
        /// </summary>
        public TimeSpan TaskFailureThreshold
        {
            get
            {
                int taskFailureThresholdInMinutes = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.TaskFailureThresholdInMinutes),
                    180);

                return TimeSpan.FromMinutes(taskFailureThresholdInMinutes);
            }
        }

        /// <summary>
        /// Gets the time delay between retries on exception.
        /// </summary>
        public TimeSpan TaskExecutionRetryDelay
        {
            get
            {
                int taskExecutionRetryDelayInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.TaskExecutionRetryDelayInSeconds),
                    60);

                return TimeSpan.FromSeconds(taskExecutionRetryDelayInSeconds);
            }
        }

        /// <summary>
        /// Gets the interval (in seconds) for which the loop will sleep between retries for posting
        /// quit event.
        /// </summary>
        public TimeSpan PostQuitSignalLoopSleepInterval
        {
            get
            {
                int postQuitSignalLoopSleepIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.PostQuitSignalLoopSleepIntervalInSeconds),
                    1);

                return TimeSpan.FromSeconds(postQuitSignalLoopSleepIntervalInSeconds);
            }
        }

        /// <summary>
        /// Gets the max retries for posting quit signal.
        /// </summary>
        public int MaxRetryCountForPostingQuitSignal
        {
            get
            {
                int maxRetryCountForPostingQuitSignal = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxRetryCountForPostingQuitSignal),
                    3);

                return maxRetryCountForPostingQuitSignal;
            }
        }

        /// <summary>
        /// Gets the time interval (in minutes) between periodic registrations.
        /// </summary>
        public TimeSpan MinWaitTimeBetweenPeriodicRegistration
        {
            get
            {
                int minWaitTimeBetweenPeriodicRegistration = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MinWaitTimeBetweenPeriodicRegistrationInMinutes),
                    60);

                return TimeSpan.FromMinutes(minWaitTimeBetweenPeriodicRegistration);
            }
        }

        /// <summary>
        /// Gets a value indicating whether to skip server SSL certificate validation or not.
        /// </summary>
        public bool SkipServerSslCertValidation
        {
            get
            {
                bool skipCertValidation = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.SkipServerSslCertValidation),
                    false);

                return skipCertValidation;
            }
        }

        /// <summary>
        /// Gets a value indicating whether to enable compression for uploaded log folders or not.
        /// </summary>
        public bool IsCompressionEnabledForUploadedLogs
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.IsCompressionEnabledForUploadedLogs),
                    true);
            }
        }

        /// <summary>
        /// Gets the delta backoff time to retry process creation.
        /// </summary>
        public TimeSpan DeltaBackoffTimeToRetryProcessCreation
        {
            get
            {
                int deltaBackoffTimeInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.DeltaBackoffTimeToRetryProcessCreationInSeconds),
                    60);

                return TimeSpan.FromSeconds(deltaBackoffTimeInSeconds);
            }
        }

        /// <summary>
        /// Gets the maximum backoff time to retry process creation.
        /// </summary>
        public TimeSpan MaxBackoffTimeToRetryProcessCreation
        {
            get
            {
                int maxBackoffTimeInMinutes = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxBackoffTimeToRetryProcessCreationInMinutes),
                    10);

                return TimeSpan.FromMinutes(maxBackoffTimeInMinutes);
            }
        }

        /// <summary>
        /// Gets the minimum duration to classify a process run as valid.
        /// </summary>
        public TimeSpan ProcessValidRunMinDuration
        {
            get
            {
                int minDurationInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.ProcessValidRunMinDurationInSeconds),
                    180);

                return TimeSpan.FromSeconds(minDurationInSeconds);
            }
        }

        /// <summary>
        /// Gets a value indicating whether to fail fast on encountering an unresponsive task
        /// or not.
        /// </summary>
        public bool FailFastOnUnresponsiveTask
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.FailFastOnUnresponsiveTask),
                    false);
            }
        }

        /// <summary>
        /// Gets a value indicating whether to limit log retention on disk based on size or not.
        /// </summary>
        public bool LimitLogRetentionBasedOnSize
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LimitLogRetentionBasedOnSize),
                    false);
            }
        }

        /// <summary>
        /// Gets the maximum size (in bytes) of log retention allowed on disk
        /// if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        public long LogRetentionMaximumSizeInBytes
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogRetentionMaximumSizeInBytes),
                    (long)int.MaxValue);
            }
        }

        /// <summary>
        /// Gets the time interval between reattempts on failures encountered in starting job
        /// execution.
        /// </summary>
        public TimeSpan StartJobExecutionFailureRetryDelay
        {
            get
            {
                int delayInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.StartJobExecutionFailureRetryDelayInSeconds),
                    30);

                return TimeSpan.FromSeconds(delayInSeconds);
            }
        }

        /// <summary>
        /// Gets the time interval between reattempts on failures encountered in tracking job
        /// execution.
        /// </summary>
        public TimeSpan TrackJobExecutionFailureRetryDelay
        {
            get
            {
                int delayInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.TrackJobExecutionFailureRetryDelayInSeconds),
                    30);

                return TimeSpan.FromSeconds(delayInSeconds);
            }
        }

        /// <inheritdoc/>
        public TimeSpan JobMonitoringinterval
        {
            get
            {
                int jobMonitorIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.JobMonitorIntervalInSeconds),
                    50);

                return TimeSpan.FromSeconds(jobMonitorIntervalInSeconds);
            }
        }

        /// <summary>
        /// Gets the threshold value after which health issue is raised.
        /// </summary>
        public int HealthIssueOccurrenceCountThreshold
        {
            get
            {
                int healthIssueOccurrenceCountThreshold = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.HealthIssueOccurrenceCountThreshold),
                    10);

                return healthIssueOccurrenceCountThreshold;
            }
        }

        /// <summary>
        /// Gets the threshold value (in minutes) after which health issue is raised.
        /// </summary>
        public TimeSpan HealthIssueDurationThreshold
        {
            get
            {
                int healthIssueDurationThreshold = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.HealthIssueDurationThresholdInMinutes),
                    30);

                return TimeSpan.FromMinutes(healthIssueDurationThreshold);
            }
        }

        /// <summary>
        /// Gets the time interval between successive runs for reporting component health.
        /// </summary>
        public TimeSpan ReportComponentHealthInterval
        {
            get
            {
                int trackHealthInterval = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.ReportComponentHealthIntervalInMinutes),
                    15);

                return TimeSpan.FromMinutes(trackHealthInterval);
            }
        }

        /// <summary>
        /// Gets the threshold value (in minutes) after which health issue is discarded.
        /// </summary>
        public TimeSpan HealthIssueDiscardDurationThreshold
        {
            get
            {
                int healthIssueDiscardDurationThreshold =
                    ServiceSettings.Instance.GetTunableValue(
                        nameof(ServiceTunableName.HealthIssueDurationThresholdInMinutes),
                        60);

                return TimeSpan.FromMinutes(healthIssueDiscardDurationThreshold);
            }
        }

        /// <summary>
        /// Gets the time interval between successive runs for processing cbengine logs.
        /// </summary>
        public TimeSpan ProcessCBEngineLogsInterval
        {
            get
            {
                int processLogsIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.ProcessCBEngineLogsIntervalInSeconds),
                    60);

                return TimeSpan.FromSeconds(processLogsIntervalInSeconds);
            }
        }

        /// <inheritdoc/>
        public TimeSpan RenewVmMonitoringEventHubSasUriInterval
        {
            get
            {
                int renewSasIntervalInSeconds = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.RenewVmMonitoringEventHubSasUriIntervalInSeconds),
                    600);

                return TimeSpan.FromSeconds(renewSasIntervalInSeconds);
            }
        }

        /// <inheritdoc/>
        public int MaxThreadsForVmMonitoringEventHubSasUriRenewal
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.MaxThreadsForVmMonitoringEventHubSasUriRenewal),
                    8);
            }
        }

        /// <inheritdoc/>
        public bool IsPeriodicVmMonitoringSasUriRenewalNeeded
        {
            get
            {
                bool isPeriodicSourceAgentSasUriRenewalNeeded = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.IsPeriodicVmMonitoringSasUriRenewalNeeded),
                    false);

                return isPeriodicSourceAgentSasUriRenewalNeeded;
            }
        }

        /// <summary>
        /// Gets a value indicating the security protocol to be used.
        /// </summary>
        public SecurityProtocolType SecurityProtocolType
        {
            get
            {
                string securityProtocolTypeString = ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.SecurityProtocolType),
                    string.Empty);

                SecurityProtocolType securityProtocolType;
                if (!Enum.TryParse(securityProtocolTypeString, out securityProtocolType))
                {
                    securityProtocolType = SecurityProtocolType.Tls12;
                }

                return securityProtocolType;
            }
        }

        /// <summary>
        /// Gets a value indicating the underlying wire-level protocol used to
        /// communicate with Service Bus.
        /// </summary>
        public ConnectivityMode ServiceBusConnectivityMode
        {
            get
            {
                return ConnectivityMode.Https; ;
            }
        }

        /// <summary>
        /// Gets the available disk percentage below which
        /// log pruning based on size will be performed.
        /// Applicable if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        public int LogPruningAvailableDiskSizePercentageThreshold
        {
            get
            {
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogPruningAvailableDiskSizePercentageThreshold),
                    20);
            }
        }

        /// <summary>
        /// Gets the available disk size in bytes below which
        /// log pruning based on size will be performed.
        /// Applicable if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        public ulong LogPruningAvailableDiskSizeInBytesThreshold
        {
            get
            {
                // By default 20 GB in bytes.
                return ServiceSettings.Instance.GetTunableValue(
                    nameof(ServiceTunableName.LogPruningAvailableDiskSizeInBytesThreshold),
                    21474836480UL);
            }
        }
    }
}
