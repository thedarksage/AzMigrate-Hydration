using System.Collections.Generic;
using System.Diagnostics;

namespace ProcessServer
{
    internal static class Tracers
    {
        public static readonly TraceSource PSMain = new TraceSource("PS/Main");
        public static readonly TraceSource Monitoring = new TraceSource("PS/Monitoring");
        public static readonly TraceSource PSVolsyncMain = new TraceSource("PS/VolsyncMain");
        public static readonly TraceSource PSVolsyncChild = new TraceSource("PS/VolsyncChild");
        public static readonly TraceSource PSDiffDataSort = new TraceSource("PS/DiffDataSort");

        public static IEnumerable<TraceSource> GetAllTraceSources()
        {
            return new[]
            {
                PSMain,
                Monitoring,
                PSVolsyncMain,
                PSVolsyncChild,
                PSDiffDataSort
            };
        }
    }
}
