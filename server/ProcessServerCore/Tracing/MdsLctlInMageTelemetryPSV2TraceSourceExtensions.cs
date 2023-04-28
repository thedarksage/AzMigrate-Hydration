using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System.Diagnostics;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    public static class MdsLctlInMageTelemetryPSV2TraceSourceExtensions
    {
        #region TracePsTelemetryV2Message

        [Conditional("TRACE")]
        public static void TracePsTelemetryV2Message(this TraceSource traceSource, TraceEventType eventType, InMageTelemetryPSV2Row pSV2Row)
        {
            WritePsTelemetryV2Message(traceSource, eventType, pSV2Row, 0);
        }

        private static void WritePsTelemetryV2Message(TraceSource traceSource, TraceEventType eventType, object pSV2Row, int id)
        {
            TaskUtils.RunAndIgnoreException(() =>
            {
                if (traceSource == null)
                {
                    Debug.Assert(false);
                    return;
                }

                // Start, Stop, Suspend, Resume, Transfer are not recognized
                // error levels at the Telemetry and RCM services.
                if (eventType != TraceEventType.Error &&
                    eventType != TraceEventType.Warning &&
                    eventType != TraceEventType.Information &&
                    eventType != TraceEventType.Verbose)
                {
                    Debug.Assert(false);
                    return;
                }

                traceSource.TraceData(eventType, id, (InMageTelemetryPSV2Row)pSV2Row);
            });
        }

        #endregion TracePsTelemetryV2Message
    }
}
