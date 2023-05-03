using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Utilities
{
    /// <summary>
    /// Implements extension methods for the logging interface used by the service.
    /// </summary>
    public static class LoggerExtensions
    {
        /// <summary>
        /// Helper routine to map an <see cref="RcmAgentEvent" /> to the generic LogEvent method.
        /// </summary>
        /// <param name="logger">Logger instance.</param>
        /// <param name="callerInfo">The caller info.</param>
        /// <param name="rcmAgentEvent">The specific event.</param>
        public static void LogEvent(
            this RcmAgentCommonLogger<LogContext> logger,
            CallInfo callerInfo,
            RcmAgentEvent rcmAgentEvent)
        {
            int eventId = (int)rcmAgentEvent.Id;
            string message = rcmAgentEvent.Message;

            logger.LogEvent(callerInfo, eventId, message);
        }
    }
}
