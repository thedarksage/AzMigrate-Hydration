using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    /// <summary>
    /// Trace sources used in ProcessServerCore dll.
    /// </summary>
    public static class Tracers
    {
        internal static readonly TraceSource RestWrapper = new TraceSource("PSCore/RestWrapper");
        internal static readonly TraceSource CSApi = new TraceSource("PSCore/CSApi");
        internal static readonly TraceSource Misc = new TraceSource("PSCore/Misc");
        internal static readonly TraceSource MonitorApi = new TraceSource("PSCore/MonitorApi");
        internal static readonly TraceSource RcmApi = new TraceSource("PSCore/RcmApi");
        internal static readonly TraceSource Settings = new TraceSource("PSCore/Settings");
        internal static readonly TraceSource MsgService = new TraceSource("PSCore/MsgService");
        internal static readonly TraceSource RcmJobProc = new TraceSource("PSCore/RcmJobProc");
        internal static readonly TraceSource RcmConf = new TraceSource("PSCore/RcmConf");
        internal static readonly TraceSource TaskMgr = new TraceSource("PSCore/TaskMgr");
        public static readonly TraceSource PSV2Telemetry = new TraceSource("PSCore/PSV2Telemetry");

        public static IEnumerable<TraceSource> GetAllTraceSources()
        {
            return new[]
            {
                RestWrapper,
                CSApi,
                Misc,
                MonitorApi,
                RcmApi,
                Settings,
                MsgService,
                RcmJobProc,
                RcmConf,
                TaskMgr,
                PSV2Telemetry
            };
        }

        public static void TryFinalizeTraceListeners(IEnumerable<TraceSource> traceSources)
        {
            if (traceSources == null)
                return;

            HashSet<TraceListener> traceListenersSet = new HashSet<TraceListener>();

            TaskUtils.RunAndIgnoreException(() =>
            {
                foreach (var currTraceSource in traceSources)
                {
                    traceListenersSet.UnionWith(
                        currTraceSource?.Listeners?.Cast<TraceListener>() ??
                        Enumerable.Empty<TraceListener>());

                    currTraceSource?.Listeners?.Clear();
                }
            }, Tracers.Misc);

            DisposeUtils.TryDisposeAll(traceListenersSet, Tracers.Misc);
        }
    }
}
