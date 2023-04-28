using RcmAgentCommonLib.Utilities.ProcessMgmt;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service
{
    /// <summary>
    /// Defines process collection for the service.
    /// </summary>
    public sealed class ProcessCollectionSingleton : ProcessCollection<LogContext>
    {
        /// <summary>
        /// Singleton instance of <see cref="ProcessCollectionSingleton"/> used by all callers.
        /// </summary>
        public static readonly ProcessCollectionSingleton Instance = new ProcessCollectionSingleton();

        /// <summary>
        /// Prevents a default instance of the <see cref="ProcessCollectionSingleton"/> class from
        /// being created.
        /// </summary>
        private ProcessCollectionSingleton()
            : base(
                ServiceTunables.Instance,
                ServiceProperties.Instance.ProcessJobGroupName,
                Logger.Instance)
        {
        }
    }
}
