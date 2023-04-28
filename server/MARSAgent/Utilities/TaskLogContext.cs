using System;
using MarsAgent.LoggerInterface;
using MarsAgent.Constants;
using MarsAgent.Service;
using RcmAgentCommonLib.LoggerInterface;

namespace MarsAgent.Utilities
{
    public class TaskLogContext
    {
        public static RcmAgentCommonLogger<LogContext>  GetLogContext()
        {
            // set client request ID per invocation.
            string clientRequestId = Guid.NewGuid().ToString();
            RcmAgentTaskLogContext logContext = new RcmAgentTaskLogContext();
            RcmAgentCommonLogger<LogContext> LoggerInstance = Logger.Instance;
            LoggerInstance.SetLogContext(new LogContext
            {
                ApplianceId = logContext.ApplianceId,
                ClientRequestId = clientRequestId,
                ActivityId = Guid.Parse(WellKnownActivityId.CBEngineServiceActivityId),
                SubscriptionId = logContext.SubscriptionId,
                ContainerId = logContext.ContainerId,
                ResourceId = logContext.ResourceId,
                ResourceLocation = logContext.ResourceLocation,
                RcmStampName = logContext.RcmStampName
            });

            return LoggerInstance;
        }
    }
}
