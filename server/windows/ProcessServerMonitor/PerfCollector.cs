using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServerMonitor;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace ProcessServerMonitor
{
    /// <summary>
    /// Collects data and creates perf counters.
    /// </summary>
    internal class PerfCollector
    {
        /// <summary>
        /// Collect perfs.
        /// </summary>
        /// <param name="token">Cancellation token.</param>
        /// <returns></returns>
        internal async Task CollectPerfs(CancellationToken token)
        {
            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                TraceEventType.Information, "Starting Perf collection");
            ProcessServerChurnAndThrpCollector psChurnAndThrpCollector = null;

            try
            {
                psChurnAndThrpCollector =
                        new ProcessServerChurnAndThrpCollector();
                await psChurnAndThrpCollector.DeleteStatsFolders(
                    token).ConfigureAwait(false);
            }
            catch (OperationCanceledException) when (token.IsCancellationRequested)
            {
                throw;
            }
            catch (Exception ex)
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                TraceEventType.Error,
                "Failed in creation of churnAndThrpCollector instance with error {0}",
                ex);
            }
            await Task.Delay(
                    Settings.Default.ChurnAndThrpCollectorCreationIterWaitInterval, token)
                    .ConfigureAwait(false);

            // Start stopwatch for cleaning the old churn/throughput stat files
            Stopwatch cleanupStatsSW = new Stopwatch();
            cleanupStatsSW.Start();

            while (true)
            {
                try
                {
                    // Retain this wait in the beginning of the try..catch loop,
                    // so that a tight loop is avoided, if there's a consistent
                    // failure in the code below.

                    await Task.Delay(
                        Settings.Default.PerfColMainIterWaitInterval, token).ConfigureAwait(false);

                    Stopwatch perfCollectorSW = new Stopwatch();
                    perfCollectorSW.Start();
                    psChurnAndThrpCollector.TryCollectPerfData(token);
                    perfCollectorSW.Stop();

                    if (perfCollectorSW.Elapsed >= Settings.Default.PerfCollectionWarnThreshold)
                    {
                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "[PSChurnAndThrpCollector] Time taken for perf collection on PS : {0} ms",
                                    perfCollectorSW.ElapsedMilliseconds);
                    }

                    if (cleanupStatsSW.Elapsed >= Settings.Default.StaleChurnAndThrpStatFilesDelInterval)
                    {
                        psChurnAndThrpCollector.DisposeAllExistingFiles(token);
                        cleanupStatsSW.Restart();
                    }
                }
                catch (OperationCanceledException) when (token.IsCancellationRequested)
                {
                    throw;
                }
                catch (Exception ex)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                TraceEventType.Error, "Perf data collection failed with exception: {0}", ex);
                }

            }
        }

    }
}
