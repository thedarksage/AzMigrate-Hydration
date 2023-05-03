using RcmContract;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service
{
    /// <summary>
    /// Defines class implementing the logging context for a task.
    /// </summary>
    public class RcmAgentTaskLogContext : LogContext
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="RcmAgentTaskLogContext"/> class.
        /// </summary>
        public RcmAgentTaskLogContext()
        {
            ServiceConfigurationSettings svcConfig = Configurator.Instance.GetConfig();
            MarsSettings componentSettings =
                ServiceSettings.Instance.GetSettings();
            this.ApplianceId = svcConfig.MachineIdentifier;
            this.RcmStampName = componentSettings?.RcmStampName;
            this.SubscriptionId = svcConfig.SubscriptionId;
            this.ResourceId = svcConfig.ResourceId;
            this.ResourceLocation = svcConfig.ResourceLocation;
        }
    }
}
