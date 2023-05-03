using System.Collections.Generic;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using RcmContract;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service
{
    /// <summary>
    /// Implements functionality to periodically report component health.
    /// </summary>
    public class ReportComponentHealthTask
        : RcmAgentReportComponentHealthTaskBase<ServiceConfigurationSettings, MarsSettings, LogContext>
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ReportComponentHealthTask"/> class.
        /// </summary>
        /// <param name="tunables">Task tunables.</param>
        /// <param name="logContext">Task logging context.</param>
        /// <param name="logger">Logger interface.</param>
        /// <param name="serviceConfig">Service configuration.</param>
        /// <param name="serviceSettings">Service settings.</param>
        /// <param name="serviceProperties">Service properties.</param>
        /// <param name="serviceHealth">Service health.</param>
        public ReportComponentHealthTask(
            IRcmAgentReportComponentHealthTaskTunables tunables,
            LogContext logContext,
            RcmAgentCommonLogger<LogContext> logger,
            IServiceConfiguration<ServiceConfigurationSettings> serviceConfig,
            IServiceSettings<MarsSettings> serviceSettings,
            IServiceProperties serviceProperties,
            IServiceHealth serviceHealth)
            : base(
                 tunables,
                 logContext,
                 logger,
                 serviceConfig,
                 serviceSettings,
                 serviceProperties,
                 serviceHealth)
        {
        }

        /// <summary>
        /// Generate component health message.
        /// </summary>
        /// <param name="healthIssues">Component health issues.</param>
        /// <returns>Appliance component health message.</returns>
        protected override ApplianceComponentHealthMsgBase GenerateComponentHealthMessage(
            List<HealthIssue> healthIssues)
        {
            return new MarsComponentHealthMsg
            {
                HealthIssues = healthIssues
            };
        }
    }
}
