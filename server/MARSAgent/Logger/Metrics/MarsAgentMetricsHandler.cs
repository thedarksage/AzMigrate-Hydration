using System.Collections.Specialized;
using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;

namespace MarsAgent.LoggerInterface
{
    /// <summary>
    /// Class encapsulating configuration and helper methods for emitting metrics.
    /// </summary>
    public class MarsAgentMetricsHandler : IWarmPathMetricsHandler
    {
        /// <summary>
        /// Gets singleton instance of the metrics configuration class.
        /// </summary>
        private static MarsAgentMetricsHandler instance;

        /// <summary>
        /// Lock used to initialize singleton instance of the metrics configuration class.
        /// </summary>
        private static object instanceLock = new object();

        /// <summary>
        /// Prevents a default instance of the <see cref="MarsAgentMetricsHandler"/>
        /// class from being created.
        /// </summary>
        private MarsAgentMetricsHandler()
        {
        }

        /// <summary>
        /// Gets the singleton instance of <see cref="MarsAgentMetricsHandler"/> class.
        /// </summary>
        /// <returns>An object of type <see cref="MarsAgentMetricsHandler"/>.</returns>
        public static MarsAgentMetricsHandler GetInstance()
        {
            if (instance == null)
            {
                lock (instanceLock)
                {
                    if (instance == null)
                    {
                        instance = new MarsAgentMetricsHandler();
                    }
                }
            }

            return instance;
        }

        /// <summary>
        /// Method invoked by metrics helper to trace metrics.
        /// </summary>
        /// <param name="metricName">Metric being emitted.</param>
        /// <param name="metricValue">Value being emitted for the metric.</param>
        /// <param name="dimensions">Ordered list of dimension names and their values.</param>
        /// <param name="annotations">Ordered list of warm path only annotation names and
        /// their values.</param>
        public void TraceMetric(
            string metricName,
            long metricValue,
            OrderedDictionary dimensions,
            OrderedDictionary annotations)
        {
            Logger.Instance.LogMetric(
                CallInfo.Site(),
                new MetricEvent(fetchLogContext: true)
                {
                    MetricName = metricName,
                    MetricValue = metricValue,
                    DimensionsMap = dimensions,
                    AnnotationsMap = annotations,
                });
        }
    }
}
