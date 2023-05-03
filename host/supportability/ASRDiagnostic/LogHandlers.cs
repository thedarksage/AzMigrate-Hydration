using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Kusto.Cloud.Platform.Data;
using Kusto.Data.Common;
using Kusto.Data.Net.Client;
using System.Collections.Concurrent;
using System.IO;
using System.Threading;
using System.Web;

namespace Microsoft.Internal.SiteRecovery.ASRDiagnostic
{
    class LogHandlers
    {
        public static Dictionary<string, Dictionary<string, Func<string, string, string, string, string, string>>> ErLogHandlerMethods = new Dictionary<string,Dictionary<string,Func<string, string, string, string, string, string>>>
            ()
            {
                {"V2A", new Dictionary<string, Func<string, string, string, string, string, string>>()
                    {
                        { "DefaultErHandler", DefaultErLogHandler },
                        { "DefaultMobilityServiceHandler", InstallerConfiguratorHandler }
                    }
                },
                {"A2A", new Dictionary<string, Func<string, string, string, string, string, string>>()
                    {
                        { "DefaultErHandler", DefaultErLogHandler },
                        { "DefaultMobilityServiceHandler", InstallerConfiguratorHandler }
                    }
                }
           };
        public static Dictionary<string, Dictionary<string, Func<string, string, string, string, string, string>>> ProtectedLogHandlerMethods = new Dictionary<string, Dictionary<string, Func<string, string, string, string, string, string>>>
            ()
            {
                {"V2A", new Dictionary<string, Func<string, string, string, string, string, string>>()
                    {
                    }
                },
                {"A2A", new Dictionary<string, Func<string, string, string, string, string, string>>()
                    {
                        { "A2AProtectionServiceSourceChurnRateExceeded", A2AProtectionServiceChurnRateExceededHandler },
                        { "A2AProtectionServiceCrashConsistentReplicationExceeded", A2AProtectionServiceCrashConsistentReplicationExceededHandler }
                    }
                }
           };


        public static string DefaultErLogHandler(string ClusterName,string ClientRequestId, string StartTime, string EndTime, string FilePath)
        {
            FilePath += ".txt";                                         //adding file extension to FilePath

            StringBuilder RecordLog = new StringBuilder();

            string DatabaseName = Clusters.ASRClusterMap[ClusterName];

            string Cluster = string.Format("https://{0}.kusto.windows.net", ClusterName);

            using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{DatabaseName};Fed=true"))
            {
                string qrylog = Query.GetOtherLogs;

                qrylog = string.Format(qrylog, "\"" + StartTime + "\"", "\"" + EndTime + "\"","\"" + ClientRequestId + "\"");
                System.Console.WriteLine("Fetch Logs Query:", qrylog);

                using (var reader = queryProvider.ExecuteQuery(qrylog))
                {
                    while (reader.Read())
                    {
                        RecordLog.Append("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                        RecordLog.Append(reader.GetString(0));
                    }
                }
            }

            while (true)
            {
                try
                {
                    using (StreamWriter fs = new StreamWriter(FilePath, true))
                    {
                        fs.Write(RecordLog.ToString());
                    }
                    break;
                }
                catch
                {
                    Thread.Sleep(100);
                }
            }

            return FilePath;
        }
        public static string InstallerConfiguratorHandler(string ClusterName, string ClientRequestId, string StartTime, string EndTime, string FilePath)
        {
            FilePath += ".txt";                                         //adding file extension to FilePath

            StringBuilder RecordLog = new StringBuilder();

            string DatabaseName = Clusters.ASRClusterMap[ClusterName];

            string Cluster = string.Format("https://{0}.kusto.windows.net", ClusterName);

            using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{DatabaseName};Fed=true"))
            {
                string qrylog = Query.InstallerConfiguratorLogs;

                qrylog = string.Format(qrylog, "\"" + ClientRequestId + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"");
                System.Console.WriteLine("Fetch Logs Query:", qrylog);

                using (var reader = queryProvider.ExecuteQuery(qrylog))
                {
                    while (reader.Read())
                    {
                        RecordLog.Append("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                        RecordLog.Append(reader.GetString(0));
                    }
                }
            }

            while (true)
            {
                try
                {
                    using (StreamWriter fs = new StreamWriter(FilePath, true))
                    {
                        fs.Write(RecordLog.ToString());
                    }
                    break;
                }
                catch
                {
                    Thread.Sleep(100);
                }
            }

            return FilePath;
        }
        public static string A2AProtectionServiceChurnRateExceededHandler(string ClusterName, string ClientRequestId, string StartTime, string EndTime, string FilePath)
        {
            FilePath += ".htm";

            StringBuilder RecordLog = new StringBuilder();

            RecordLog.Append("<h2> Analyzing health issue A2AProtectionServiceSourceChurnRateExceeded </h2>\n");
            RecordLog.Append("<h4>HostId : " + ClientRequestId + "</h4>\n");

            string DatabaseName = Clusters.ASRClusterMap[ClusterName];
            string Cluster = string.Format("https://{0}.kusto.windows.net", ClusterName);

            using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{DatabaseName};Fed=true"))
            {
                string qryDataSourceId = Query.GetDataSourceId;

                qryDataSourceId = string.Format(qryDataSourceId, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + ClientRequestId + "\"");

                string DataSourceId = "", DsUniqueName = "";

                using (var reader = queryProvider.ExecuteQuery(qryDataSourceId))
                {
                    while (reader.Read())
                    {
                        DataSourceId = reader.GetString(0);
                        DsUniqueName = reader.GetString(1);
                    }
                }
                RecordLog.Append("<h4>DataSourceId : " + DataSourceId + "</h4>\n");
                RecordLog.Append("<h4>DsUniqueName : " + DsUniqueName + "\n</h4>\n");
                RecordLog.Append("<TABLE border = \"2\">\n");
                RecordLog.Append("<TR>\n");
                RecordLog.Append("<TH>DiskId</TH>\n");
                RecordLog.Append("<TH>Source Churn Rate (MBps)</TH>\n");
                RecordLog.Append("<TH>Service Processing Rate (MBps)</TH>\n");
                RecordLog.Append("</TR>\n");

                string qryDataSourceHealth = Query.GetDsHealth;

                qryDataSourceHealth = string.Format(qryDataSourceHealth, "\"" + DataSourceId + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"");

                List<string> HighChurnDiskId = new List<string>();

                using (var reader = queryProvider.ExecuteQuery(qryDataSourceHealth))
                {
                    while (reader.Read())
                    {
                        RecordLog.Append("<TR>\n");
                        RecordLog.Append("<TD>" + reader.GetString(0) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(1) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(2) + "</TD>");
                        RecordLog.Append("</TR>\n");

                        if (reader.GetDouble(1) >= reader.GetDouble(2))                   //Source Churn Rate > Service Processing Rate
                            HighChurnDiskId.Add(reader.GetString(0));
                    }
                }
                RecordLog.Append("</TABLE>\n");

                var DiskHealthReport = new ConcurrentDictionary<string, List<Tuple<string,double,double,double,double>>>();

                Parallel.ForEach(HighChurnDiskId, (DiskId) =>
                {
                    string qryDiskHealth = Query.GetDiskHealth;

                    qryDiskHealth = string.Format(qryDiskHealth, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + DataSourceId + "\"", "\"" + DiskId + "\"");

                    using (var reader = queryProvider.ExecuteQuery(qryDiskHealth))
                    {
                        while (reader.Read())
                        {
                            var CurrentDiskHealthReport = new Tuple<string, double, double, double, double>(reader.GetDateTime(0).ToString(), reader.GetDouble(1), reader.GetDouble(2), reader.GetDouble(3), reader.GetDouble(4));

                            DiskHealthReport.TryAdd(DiskId, new List<Tuple<string, double, double, double, double>>());
                            DiskHealthReport[DiskId].Add(CurrentDiskHealthReport);
                        }
                    }
                });

                foreach(var rep in DiskHealthReport)
                {
                    RecordLog.Append("<h4>DiskId : " + rep.Key + "</h4>\n");
                    RecordLog.Append("<TABLE border = \"2\">\n");
                    RecordLog.Append("<TR>\n");
                    RecordLog.Append("<TH>Hourly Time Stamp</TH>\n");
                    RecordLog.Append("<TH>Incoming Rate (MBps)</TH>\n");
                    RecordLog.Append("<TH>Apply Rate (MBps)</TH>\n");
                    RecordLog.Append("<TH>Processing Rate (MBps)</TH>\n");
                    RecordLog.Append("<TH>Last RPO (hours)</TH>\n");
                    RecordLog.Append("</TR>\n");

                    foreach(var row in rep.Value)
                    {
                        RecordLog.Append("<TR>\n");
                        RecordLog.Append("<TD>" + row.Item1 + "</TD>");
                        RecordLog.Append("<TD>" + row.Item2 + "</TD>");
                        RecordLog.Append("<TD>" + row.Item3 + "</TD>");
                        RecordLog.Append("<TD>" + row.Item4 + "</TD>");
                        RecordLog.Append("<TD>" + row.Item5 + "</TD>");
                        RecordLog.Append("</TR>\n");
                    }
                    RecordLog.Append("</TABLE>\n");
                }
            }

            while(true)
            {
                try
                {
                    using (StreamWriter fs = new StreamWriter(FilePath, true))
                    {
                        fs.Write(RecordLog.ToString());
                    }
                    break;
                }
                catch
                {
                    Thread.Sleep(100);
                }
            }
            return FilePath;
        }
        public static string A2AProtectionServiceCrashConsistentReplicationExceededHandler(string ClusterName, string HostId, string StartTime, string EndTime, string FilePath)
        {
            FilePath += ".htm";

            StringBuilder RecordLog = new StringBuilder();

            RecordLog.Append("<h2> Analyzing health issue A2AProtectionServiceCrashConsistentReplicationExceeded </h2>\n");
            RecordLog.Append("<h4>HostId : " + HostId + "</h4>\n");

            string DatabaseName = Clusters.ASRClusterMap[ClusterName];
            string Cluster = string.Format("https://{0}.kusto.windows.net", ClusterName);

            using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{DatabaseName};Fed=true"))
            {
////////////////////////////////////////////////////////////////////GetDataSourceId///////////////////////////////////////////////////////////////////////////////////////////////
                string qryDataSourceId = Query.GetDataSourceId;

                qryDataSourceId = string.Format(qryDataSourceId, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + HostId + "\"");

                string DataSourceId = "", DsUniqueName = "";

                using (var reader = queryProvider.ExecuteQuery(qryDataSourceId))
                {
                    while (reader.Read())
                    {
                        DataSourceId = reader.GetString(0);
                        DsUniqueName = reader.GetString(1);
                    }
                }
                RecordLog.Append("<h4>DataSourceId : " + DataSourceId + "</h4>\n");
                RecordLog.Append("<h4>DsUniqueName : " + DsUniqueName + "\n</h4>\n");

/////////////////////////////////////////////////////////////////////GetDataSourceHealth///////////////////////////////////////////////////////////////////////////////////////////////

                RecordLog.Append("<h2>Data Source Health</h2>\n");

                RecordLog.Append("<TABLE border = \"2\">\n");
                RecordLog.Append("<TR>\n");
                RecordLog.Append("<TH>VhdStorageAccountType</TH>\n");
                RecordLog.Append("<TH>VhdStorageAccountName</TH>\n");
                RecordLog.Append("<TH>SourceType</TH>\n");
                RecordLog.Append("<TH>DataSourceId</TH>\n");
                RecordLog.Append("<TH>DiskId</TH>\n");
                RecordLog.Append("<TH>Last RPO</TH>\n");
                RecordLog.Append("<TH>Source Churn Rate (MBps)</TH>\n");
                RecordLog.Append("<TH>Service Processing Rate (MBps)</TH>\n");
                RecordLog.Append("<TH>Average Write Size (KB)</TH>\n");
                RecordLog.Append("<TH>Average Overlap Rate</TH>\n");
                RecordLog.Append("<TH>Service Processing Rate (MBps)</TH>\n");
                RecordLog.Append("<TH>Average Source IOPS</TH>\n");
                RecordLog.Append("<TH>Compression Ratio</TH>\n");
                RecordLog.Append("<TH>Source Upload Duration</TH>\n");
                RecordLog.Append("<TH>Total Data Size uploaded (MB)</TH>\n");
                RecordLog.Append("</TR>\n");

                string qryDataSourceHealth = Query.GetDataSourceHealth;
                qryDataSourceHealth = string.Format(qryDataSourceHealth, "\"" + DataSourceId + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"");

                int TotalDisks = 0;

                using (var reader = queryProvider.ExecuteQuery(qryDataSourceHealth))
                {
                    while (reader.Read())
                    {
                        TotalDisks++;
                        RecordLog.Append("<TR>\n");
                        RecordLog.Append("<TD>" + reader.GetString(0) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetString(1) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetString(2) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetString(3) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetString(4) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetValue(5).ToString() + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(6) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(7) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(8) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(9) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(10) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(11) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(12) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetValue(13).ToString() + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDouble(14) + "</TD>");
                        RecordLog.Append("</TR>\n");
                    }
                }
                RecordLog.Append("</TABLE>\n");
                RecordLog.Append("<h2>Total No. of Disks :" + TotalDisks + "</h2>\n");

////////////////////////////////////////////////////////////////////GetMarkerArrivalInfo///////////////////////////////////////////////////////////////////////////////////////////////

                RecordLog.Append("<h2>InMage Marker Arrival Info</h2>\n");

                RecordLog.Append("<TABLE border = \"2\">\n");
                RecordLog.Append("<TR>\n");
                RecordLog.Append("<TH>Data Source Id</TH>\n");
                RecordLog.Append("<TH>Marker Id</TH>\n");
                RecordLog.Append("<TH>First Arrival</TH>\n");
                RecordLog.Append("<TH>Disk Count</TH>\n");
                RecordLog.Append("<TH>Arrival Difference</TH>\n");
                RecordLog.Append("<TH>Disk List</TH>\n");
                RecordLog.Append("</TR>\n");

                string qryMarkerArrivalInfo = Query.GetInMageMarkerArrivalInfo;

                var StartTimeInMageMarkerArrvialInfo = DateTime.Parse(EndTime).AddHours(-1).ToString();

                qryMarkerArrivalInfo = string.Format(qryMarkerArrivalInfo, "\"" + DataSourceId + "\"", "\"" + StartTimeInMageMarkerArrvialInfo + "\"", "\"" + EndTime + "\"");

                bool hasData = false;
                using (var reader = queryProvider.ExecuteQuery(qryMarkerArrivalInfo))
                {
                    while (reader.Read())
                    {
                        hasData = true;
                        RecordLog.Append("<TR>\n");
                        RecordLog.Append("<TD>" + reader.GetString(0) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetString(1) + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetDateTime(2).ToString() + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetValue(3).ToString() + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetValue(4).ToString() + "</TD>");
                        RecordLog.Append("<TD>" + reader.GetValue(5).ToString() + "</TD>");
                        RecordLog.Append("</TR>\n");
                    }
                }
                RecordLog.Append("</TABLE>\n");

                /*if(!hasData)
                {*/
////////////////////////////////////////////////////////////////////GetHvrApplicationSyncSessionStats///////////////////////////////////////////////////////////////////////////////////////////////
                    RecordLog.Append("<h2>HvrApplySyncSessionStats</h2>\n");

                    RecordLog.Append("<TABLE border = \"2\">\n");
                    RecordLog.Append("<TR>\n");
                    RecordLog.Append("<TH>Precise Time Stamp</TH>\n");
                    RecordLog.Append("<TH>Marker Id</TH>\n");
                    RecordLog.Append("<TH>Disk Id</TH>\n");
                    RecordLog.Append("</TR>\n");


                    string qryHvrApplySyncSessionStats = Query.HvrApplicationSyncStats;
                    qryHvrApplySyncSessionStats = string.Format(qryHvrApplySyncSessionStats, "\"" + DataSourceId + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"");

                    using (var reader = queryProvider.ExecuteQuery(qryMarkerArrivalInfo))
                    {
                        while (reader.Read())
                        {
                            hasData = true;
                            RecordLog.Append("<TR>\n");
                            RecordLog.Append("<TD>" + reader.GetValue(0).ToString() + "</TD>");
                            RecordLog.Append("<TD>" + reader.GetString(1) + "</TD>");
                            RecordLog.Append("<TD>" + reader.GetValue(2).ToString() + "</TD>");
                            RecordLog.Append("</TR>\n");
                        }
                    }
                    RecordLog.Append("</TABLE>\n");

                    /*if (!hasData)
                    {*/
/////////////////////////////////section 6: Get VACP Table//////////////////////////////////////////////////////////////////////////////////////
                        RecordLog.Append("<h2>VACP Table</h2>\n");

                        RecordLog.Append("<TABLE border = \"2\">\n");
                        RecordLog.Append("<TR>\n");
                        RecordLog.Append("<TH>Precise Time Stamp</TH>\n");
                        RecordLog.Append("<TH>Src Logger Time</TH>\n");
                        RecordLog.Append("<TH>No. of Disks in policy</TH>\n");
                        RecordLog.Append("<TH>No. of Disks with tag</TH>\n");
                        RecordLog.Append("<TH>Tag Type Conf</TH>\n");
                        RecordLog.Append("<TH>Tag Type Insert</TH>\n");
                        RecordLog.Append("<TH>Final Status</TH>\n");
                        RecordLog.Append("<TH>Reason</TH>\n");
                        RecordLog.Append("<TH>Message</TH>\n");
                        RecordLog.Append("</TR>\n");

                        string qryGetVacpTable = Query.GetVacpTable;
                        qryGetVacpTable = string.Format(qryGetVacpTable, "\"" + HostId + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"");

                        using (var reader = queryProvider.ExecuteQuery(qryGetVacpTable))
                        {
                            while (reader.Read())
                            {
                                RecordLog.Append("<TR>\n");
                                RecordLog.Append("<TD>" + reader.GetValue(0).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(1).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(2).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(3).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(4).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(5).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(6).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(7).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(8).ToString() + "</TD>");
                                RecordLog.Append("</TR>\n");
                            }
                        }
                        RecordLog.Append("</TABLE>\n");
                    //}
                    /*else
                    {*/
/////////////////////////////////section 5: Latest Marker(which has all disks) Info//////////////////////////////////////////////////////////////////////////
                        string qryLastMarkerId = Query.GetLastMarkerId;
                        qryLastMarkerId = string.Format(qryLastMarkerId, "\"" + DataSourceId + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"", TotalDisks );

                        string LastMarkerIdWhichHasAllDisks = "";
                        using (var reader = queryProvider.ExecuteQuery(qryLastMarkerId))
                        {
                            while (reader.Read())
                            {
                                LastMarkerIdWhichHasAllDisks = reader.GetString(0);
                            }
                        }


                        RecordLog.Append("<h2>Last Marker Info</h2>\n");

                        RecordLog.Append("<TABLE border = \"2\">\n");
                        RecordLog.Append("<TR>\n");
                        RecordLog.Append("<TH>Precise Time Stamp</TH>\n");
                        RecordLog.Append("<TH>Disk Id</TH>\n");
                        RecordLog.Append("<TH>Session Id</TH>\n");
                        RecordLog.Append("<TH>Session Type</TH>\n");
                        RecordLog.Append("<TH>Is Recoverable?</TH>\n");
                        RecordLog.Append("<TH>DsType</TH>\n");
                        RecordLog.Append("<TH>Marker Id</TH>\n");
                        RecordLog.Append("<TH>Marker Type</TH>\n");
                        RecordLog.Append("</TR>\n");

                        string qryLastMarkerInfo = Query.GetLastMarkerInfo;
                        qryLastMarkerInfo = string.Format(qryLastMarkerInfo, "\"" + LastMarkerIdWhichHasAllDisks + "\"", "\"" + StartTime + "\"", "\"" + EndTime + "\"");

                        using (var reader = queryProvider.ExecuteQuery(qryLastMarkerInfo))
                        {
                            while (reader.Read())
                            {
                                RecordLog.Append("<TR>\n");
                                RecordLog.Append("<TD>" + reader.GetDateTime(0).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetString(1) + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetString(2) + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetString(3) + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(4).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(5).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetValue(6).ToString() + "</TD>");
                                RecordLog.Append("<TD>" + reader.GetString(7) + "</TD>");
                                RecordLog.Append("</TR>\n");
                            }
                        }
                        RecordLog.Append("</TABLE>\n");
                    //}
                //}
            }
            while (true)
            {
                try
                {
                    using (StreamWriter fs = new StreamWriter(FilePath, true))
                    {
                        fs.Write(RecordLog.ToString());
                    }
                    break;
                }
                catch
                {
                    Thread.Sleep(100);
                }
            }
            return FilePath;
        }
    }
}
