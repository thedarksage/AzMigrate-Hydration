using System.Collections.Generic;
using System.Diagnostics;

namespace ProcessServerMonitor
{
    internal static class Constants
    {
        public static readonly TraceSource s_psSysMonTraceSource = new TraceSource("PSMonitor/SystemMonitor");
        public static readonly TraceSource s_psMonMain = new TraceSource("PSMonitor/Main");

        public static IEnumerable<TraceSource> GetAllTraceSources()
        {
            return new[]
            {
                s_psSysMonTraceSource,
                s_psMonMain
            };
        }
    }
}
