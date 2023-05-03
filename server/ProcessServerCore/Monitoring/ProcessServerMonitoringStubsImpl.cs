using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Diagnostics;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Monitoring
{
    public class ProcessServerMonitoringStubsImpl : IProcessServerMonitoringStubs
    {
        private const string MarsPerfCategory = "Hyper-V Azure Replication Agent";

        private const string MarsUploadedData = "Uncompressed Bytes";

        private const string MarsTransferTime = "TransferTime";

        /// <summary>
        /// Get Data uploaded using MARS perf counter
        /// </summary>
        /// <returns>Uploaded data and Transfer Time</returns>
        public Tuple<long, double> GetUploadedData()
        {
            long uploadedData = -1;
            double transferTime = -1.0;

            var uploadedDataStats = new Tuple<long, double>(uploadedData, transferTime);

            try
            {
                if (PerformanceCounterCategory.Exists(MarsPerfCategory))
                {
                    var marsPerfCategory = new PerformanceCounterCategory(MarsPerfCategory);
                    string[] instances = marsPerfCategory.GetInstanceNames();

                    if (instances.Length == 0)
                    {
                        Tracers.MonitorApi.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "MARS perf counter doesn't contain any replicating VM instances");

                        return uploadedDataStats;
                    }

                    if (!marsPerfCategory.CounterExists(MarsUploadedData) ||
                        !marsPerfCategory.CounterExists(MarsTransferTime))
                    {
                        return uploadedDataStats;
                    }

                    foreach (var instance in instances)
                    {
                        var sentBytesPC = new PerformanceCounter(
                            MarsPerfCategory, MarsUploadedData, instance);

                        var sentBytes = Convert.ToInt64(sentBytesPC.NextValue());

                        var transferTimePC = new PerformanceCounter(MarsPerfCategory, MarsTransferTime, instance);

                        var transferDuration = Convert.ToDouble(transferTimePC.NextValue());
                        Tracers.MonitorApi.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "VMName: {0}, UncompressedSize: {1}, TransferTime: {2}",
                            instance,
                            sentBytes,
                            transferDuration);

                        uploadedData += sentBytes;
                        transferTime += transferDuration;
                    }
                }
            }
            catch (Exception ex)
            {
                Tracers.MonitorApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Ignoring error, failed to get throughput with error : {0}",
                        ex);
                throw;
            }

            return new Tuple<long, double>(uploadedData, transferTime);
        }
    }
}