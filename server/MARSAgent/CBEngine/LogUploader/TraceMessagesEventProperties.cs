using MarsAgent.LoggerInterface;

namespace MarsAgent.CBEngine.LogUploader
{
    public sealed class TraceMessagesEventProperties : EventProperties
    {
        /// <summary>
        /// Singleton instance of <see cref="TraceMessagesEventProperties"/> used by all callers.
        /// </summary>
        public static readonly TraceMessagesEventProperties Instance = new TraceMessagesEventProperties();

        /// <summary>
        /// Prevents a default instance of the <see cref="TraceMessagesEventProperties"/> class from
        /// being created.
        /// </summary>
        private TraceMessagesEventProperties()
        {
        }

        /// <summary>
        /// The table ingestion mapping event version.
        /// </summary>
        /// <remarks>
        /// The event version must be changed when there is any change in event properties.
        /// </remarks>
        public override string EventVersion
        {
            get
            {
                return "1.0";
            }
        }

        /// <summary>
        /// Gets the Kusto Table Name
        /// </summary>
        public override string KustoTableName
        {
            get
            {
                return "RcmCBEngineTraceMessages";
            }
        }
    }
}
