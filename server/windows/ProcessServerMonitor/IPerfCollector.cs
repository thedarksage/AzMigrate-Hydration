using System.Threading;

namespace ProcessServerMonitor
{
    internal interface IPerfCollector
    {
        void TryCollectPerfData(CancellationToken ct);
    }
}