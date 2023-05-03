using System;
using System.IO;
using MarsAgent.CBEngine.Constants;
using MarsAgent.CBEngine.FileWatcher;
using MarsAgent.Constants;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service.CBEngine
{
    /// <summary>
    /// Implements functionality to periodically monitor cbengine logs.
    /// </summary>
    public class MonitorCBEngineLogsTask
        : RcmAgentPeriodicTaskBase<LogContext>
    {
        /// <summary>
        /// The client request ID.
        /// </summary>
        private string clientRequestId = Guid.NewGuid().ToString();

        /// <summary>
        /// Initializes a new instance of the <see cref="MonitorCBEngineLogsTask"/> class.
        /// </summary>
        /// <param name="tunables">Task tunables.</param>
        /// <param name="logContext">Task logging context.</param>
        /// <param name="logger">Logger interface.</param>
        public MonitorCBEngineLogsTask(
            IRcmAgentConsumeSettingsTaskTunables tunables,
            LogContext logContext,
            RcmAgentCommonLogger<LogContext> logger)
            : base(tunables, logContext, logger)
        {
            this.Initialize();
        }

        /// <summary>
        /// Gets the type of worker.
        /// </summary>
        public sealed override string TaskType => "MonitorCBEngineLogsTask";

        /// <inheritdoc/>
        public sealed override string Identifier => this.TaskType;

        /// <summary>
        /// Gets the activity ID to be associated with the task.
        /// </summary>
        protected sealed override string ActivityId
            => WellKnownActivityId.CBEngineMonitorLogActivityId;

        /// <inheritdoc/>
        protected override string ServiceActivityId => this.ActivityId;

        /// <summary>
        /// Gets the time interval before next run.
        /// </summary>
        protected sealed override TimeSpan TimeIntervalBetweenRuns
            => ServiceTunables.Instance.ProcessCBEngineLogsInterval;

        /// <inheritdoc/>
        protected sealed override bool IsWorkCompletedInternal => false;

        /// <inheritdoc/>
        protected override string ClientRequestId => this.clientRequestId;

        private LogFileWatcher logProcesser;

        /// <inheritdoc/>
        protected sealed override void PerformWorkOneInvocation()
        {
            // Set client request ID per invocation.
            this.clientRequestId = Guid.NewGuid().ToString();
            this.SetLogContext();

            LogFileWatcher.MoveExistingFiles(Logger.Instance);

            this.CancellationToken.ThrowIfCancellationRequested();

            if (logProcesser is null)
            {
                logProcesser = new LogFileWatcher(() =>
                {
                    this.CancellationToken.ThrowIfCancellationRequested();
                    // Keep the hearbeat alive for this task.
                    this.UpdateTaskHeartbeat();
                }, Logger.Instance, this.CancellationToken);
            }
        }

        /// <summary>
        /// Merges log context for current task.
        /// </summary>
        protected sealed override void MergeLogContext()
        {
            // No-op
        }

        /// <inheritdoc/>
        protected sealed override bool IsReadyToRun()
        {
            return true;
        }

        /// <inheritdoc/>
        protected sealed override void SendStopRequestInternal()
        {
            if (logProcesser != null)
            {
                logProcesser.StopEvents();
            }
        }

        /// <inheritdoc/>
        protected sealed override void CleanupOnTaskCompletion()
        {
            // No-Op.
        }

        /// <summary>
        /// Initialization routine.
        /// </summary>
        private void Initialize()
        {
            if (!Directory.Exists(CBEngineConstants.LogDirPath))
                Directory.CreateDirectory(CBEngineConstants.LogDirPath);
        }
    }
}
