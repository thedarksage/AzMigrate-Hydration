using System;
using System.Collections.Generic;
using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using RcmAgentCommonLib.Utilities.JobExecution;
using RcmContract;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service
{
    /// <summary>
    /// Implements functionality to periodically consume settings from RCM.
    /// </summary>
    public class ConsumeSettingsTask
        : RcmAgentConsumeSettingsTaskBase<ServiceConfigurationSettings, MarsSettings, LogContext>
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ConsumeSettingsTask"/> class.
        /// </summary>
        /// <param name="tunables">Task tunables.</param>
        /// <param name="logContext">Task logging context.</param>
        /// <param name="logger">Logger interface.</param>
        /// <param name="serviceConfig">Service configuration.</param>
        /// <param name="serviceSettings">Service settings.</param>
        /// <param name="heartbeatMonitor">Heartbeat monitor.</param>
        public ConsumeSettingsTask(
            IRcmAgentConsumeSettingsTaskTunables tunables,
            LogContext logContext,
            RcmAgentCommonLogger<LogContext> logger,
            IServiceConfiguration<ServiceConfigurationSettings> serviceConfig,
            IServiceSettings<MarsSettings> serviceSettings,
            IRcmAgentHeartbeatMonitor heartbeatMonitor)
            : base(
                 tunables,
                 logContext,
                 logger,
                 serviceConfig,
                 serviceSettings,
                 heartbeatMonitor)
        {
        }

        /// <summary>
        /// Fetch jobs for the component.
        /// </summary>
        /// <param name="settings">Service settings.</param>
        /// <returns>Component level jobs.</returns>
        protected override List<RcmJob> FetchComponentLevelJobs(
            MarsSettings settings)
        {
            return settings.RcmJobs;
        }

        /// <summary>
        /// Fetch actions for the protected machines to be performed by this component.
        /// </summary>
        /// <param name="settings">Service settings.</param>
        /// <returns>Actions for the protected machines to be performed by this component.
        /// </returns>
        protected override Dictionary<string, List<RcmActionSignature>>
            FetchProtectedMachineLevelActions(MarsSettings settings)
        {
            return new Dictionary<string, List<RcmActionSignature>>();
        }

        /// <summary>
        /// Fetch jobs for the protected machines to be served by this component.
        /// </summary>
        /// <param name="settings">Service settings.</param>
        /// <returns>Protected machine level jobs to be served by this component.</returns>
        protected override Dictionary<string, List<RcmJob>> FetchProtectedMachineLevelJobs(
            MarsSettings settings)
        {
            return new Dictionary<string, List<RcmJob>>();
        }

        /// <inheritdoc/>
        protected sealed override IRcmJobExecution CreateJobExecutionInstance(
            RcmJob job,
            MarsSettings settings)
        {
            IRcmJobExecution jobExecution = null;
            switch (job.JobType)
            {
                default:
                    jobExecution = new UnsupportedJobExecution(job);

                    string message = $"Forgot to add handler for '{job.JobType}'.";
                    Logger.Instance.DebugFail(CallInfo.Site(), message);
                    Logger.Instance.LogError(CallInfo.Site(), message);
                    break;
            }

            return jobExecution;
        }

        /// <inheritdoc/>
        protected sealed override bool TryCreateActionHandlerInstance(
            RcmActionSignature action,
            MarsSettings settings,
            out IRcmActionExecution<MarsSettings> actionHandler)
        {
            bool rv = true;
            actionHandler = null;
            switch (action.ActionName)
            {
                default:
                    string message = $"Forgot to add handler for '{action.ActionName}'.";
                    Logger.Instance.DebugFail(CallInfo.Site(), message);
                    Logger.Instance.LogError(CallInfo.Site(), message);
                    rv = false;
                    break;
            }

            return rv;
        }
    }
}