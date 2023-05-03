namespace MarsAgent.LoggerInterface
{
    public abstract class EventProperties
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="EventProperties" /> class.
        /// </summary>
        public EventProperties()
        {
        }

        /// <summary>
        /// The table ingestion mapping event version.
        /// </summary>
        /// <remarks>
        /// The event version must be changed when there is any change in event properties.
        /// </remarks>
        public abstract string EventVersion { get; }

        /// <summary>
        /// Gets the Kusto Table Name
        /// </summary>
        public abstract string KustoTableName { get; }
    }
}
