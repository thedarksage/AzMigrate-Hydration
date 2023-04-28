using System.Collections.Specialized;
using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;

namespace MarsAgent.LoggerInterface
{
    /// <summary>
    /// Implements TraceMetric to emit metrics.
    /// </summary>
    public class MarsAgentMetricsEmitter : RcmAgentMetricsEmitter
    {
        /// <summary>
        /// Method invoked by metrics helper to trace metrics.
        /// </summary>
        /// <param name="metricName">Metric being emitted.</param>
        /// <param name="metricValue">Value being emitted for the metric.</param>
        /// <param name="dimensions">Ordered list of dimension names and their values.</param>
        /// <param name="annotations">Ordered list of warm path only annotation names and
        /// their values.</param>
        protected override void TraceMetric(
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
