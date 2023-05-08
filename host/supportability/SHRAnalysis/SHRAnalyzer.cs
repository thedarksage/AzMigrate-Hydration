using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using System.Collections.Concurrent;
using Kusto.Cloud.Platform.Data;
using Kusto.Data.Common;
using Kusto.Data.Net.Client;
using Microsoft.TeamFoundation.WorkItemTracking.WebApi.Models;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    internal sealed class ErrorObject
    {
        public string ClientRequestId;
        public string OSType;
        public string AgentVersion;
        public DateTime PreciseTimeStamp;
        public string ErrorName;
        public string ErrorType;
        public string ErrorGroup;
        public string StampName;
        public string GeoName;
        public string SubscriptionId;
        public string OSName;
        public string OSVersion;
        public string ObjectId;

        public ErrorObject(string clientRequestId, 
                            DateTime preciseTimeStamp,
                            string errorName,
                            string errorType,
                            string errorGroup,
                            string stampName,
                            string subscriptionId,
                            string osType,
                            string agentVersion,
                            string osName,
                            string osVersion,
                            string objectId)
        {
            ClientRequestId = clientRequestId;
            PreciseTimeStamp = preciseTimeStamp.Date;
            ErrorName = errorName;
            ErrorType = errorType;
            ErrorGroup = errorGroup;
            StampName = stampName;
            var index = stampName.IndexOf("-pod01-srs1");
            GeoName = stampName.Remove(index);
            SubscriptionId = subscriptionId;
            OSType = osType;
            AgentVersion = agentVersion;
            OSName = osName;
            OSVersion = osVersion;
            ObjectId = objectId;
        }
    };

    class SHRAnalyzer
    {
        private static IDictionary<string, string> FindWorkItems(IDictionary<string, IList<ErrorObject>> ClientIdMap,
                                                     IDictionary<string, IList<WorkItem>> workItems)
        {
            IDictionary<string, string> applicableWIs = new ConcurrentDictionary<string, string>();
            foreach (var item in ClientIdMap.OrderByDescending(x => x.Value.Count))
            {
                var ErrorCodeName = item.Key;
                var errorObj = item.Value;

                string BugIds = "No Bug";
                if (workItems.ContainsKey(ErrorCodeName) && workItems[ErrorCodeName].Count > 0)
                {
                    // Ideally we should have implemented following logic
                    // If there is a single bug we are done 
                    // If there are multiple bugs
                    //      Pick the highest bug number which is active or,
                    //      Pick the bug which has highest id

                    // But there are complication with above logic as
                    // There can be separate failure for window and linux
                    // Each failure can have different internal error associated
                    //  Like msi error can be either 1601, 1603 or, 1622
                    // If we pick just one bug, we are not going to see which bug has to be worked upon
                    int linuxHitCount = ClientIdMap[ErrorCodeName].Where(x =>
                                                    x.OSType.Equals("linux", StringComparison.OrdinalIgnoreCase)).Count();
                    int windowsHitCount = ClientIdMap[ErrorCodeName].Where(x =>
                                                    x.OSType.Equals("windows", StringComparison.OrdinalIgnoreCase)).Count();
                    int othersHitCount = ClientIdMap[ErrorCodeName].Count() - linuxHitCount - windowsHitCount;

                    WorkItem bugItem = null;
                    var bugs = workItems[ErrorCodeName].Where(x =>
                                    x.Fields["System.State"].ToString().NotEquals("Done") &&
                                    x.Fields["System.State"].ToString().NotEquals("Removed"));

                    if (null == bugs || bugs.Count() == 0)
                    {
                        bugItem = workItems[ErrorCodeName].OrderByDescending(x => int.Parse(x.Fields["System.Id"].ToString())).First();
                    }
                    else
                    {
                        bugItem = bugs.OrderByDescending(x => int.Parse(x.Fields["System.Id"].ToString())).First();
                    }

                    BugIds = string.Format(@"<a href=""https://msazure.visualstudio.com/One/_workitems/edit/{0}"">",
                                bugItem.Fields["System.Id"].ToString()) +
                    string.Join(": ", bugItem.Fields["System.Id"].ToString(),
                                        bugItem.Fields["System.Title"].ToString()) + @"</a>";
                    applicableWIs.Add(ErrorCodeName, BugIds);

                } 
            }
            return applicableWIs;
        }

        public static IDictionary<string, string> GetDataTableForSHR(IDictionary<string, IList<ErrorObject>> ClientAllRequestErrorMap,
            IDictionary<string, IList<ErrorObject>> ClientLastRequestErrorMap)
        {
            var workItems = VSSHelper.GetWorkItems(ClientAllRequestErrorMap.Keys);
            IDictionary<string, string> summaryMap = new Dictionary<string, string>();
            StringBuilder allErrorsSB = new StringBuilder();
            StringBuilder summarySB = new StringBuilder();
            StringBuilder customerJourneySB = new StringBuilder();

            // ErrorCodeName
            // Hit Count
            // Date
            // Geo
            // Client Request Ids
            // BugIds
            Action<StringBuilder> FormTableHeader = header =>
            {
                header.Append("ErrorCodeName,");
                header.Append("Date,");
                header.Append("Geo,");
                header.Append("ClientRequestId,");
                header.Append("SubscriptionId,");
                header.Append("OSType,");
                header.Append("Agent Version,");
                header.Append("OSName,");
                header.Append("OSVersion");
                header.AppendLine("");
            };

            FormTableHeader(customerJourneySB);
            FormTableHeader(allErrorsSB);

            summarySB.Append(@"<h2> Summarized Analysis.</h2>");
            summarySB.Append(@"<table border=""1"">");
            summarySB.Append("<tr>");
            summarySB.Append("<th>ErrorCodeName</th>");
            summarySB.Append("<th>Customer Journey</th>");
            summarySB.Append("<th>windows</th>");
            summarySB.Append("<th>Linux</th>");
            summarySB.Append("<th>ImpactedCustomers</th>");
            summarySB.Append("<th>All Failures</th>");
            summarySB.Append("<th>windows</th>");
            summarySB.Append("<th>Linux</th>");
            summarySB.Append("<th>ImpactedCustomers</th>");
            summarySB.Append("<th>Bug Id</th>");
            summarySB.Append("</tr>");

            ClientAllRequestErrorMap.Keys.Except(ClientLastRequestErrorMap.Keys).ToList().ForEach(k =>
                            ClientLastRequestErrorMap.Add(k, new List<ErrorObject>()));

            Func<IList<ErrorObject>, ulong, string> FormattedFailure = (e, ulFailures) =>
            {
                double failPercent = ((double)e.Count / (double)ulFailures) * 100;
                double failRate = Math.Round(failPercent, 1);

                int linuxFailures = e.Count(x => x.OSType.Equals("linux", StringComparison.OrdinalIgnoreCase));
                double linuxfailRate = Math.Round(((double)linuxFailures / (double)ulFailures) * 100, 1);

                int winFailures = e.Count(x => x.OSType.Equals("windows", StringComparison.OrdinalIgnoreCase));
                double winfailRate = Math.Round(((double)winFailures / (double)ulFailures) * 100, 1);

                StringBuilder stringBuilder = new StringBuilder();
                stringBuilder.Append("<td>" + string.Format("{0}({1}%)", e.Count, failRate) + "</td>");
                stringBuilder.Append("<td>" + string.Format("{0}({1}%)", winFailures, winfailRate) + "</td>");
                stringBuilder.Append("<td>" + string.Format("{0}({1}%)", linuxFailures, linuxfailRate) + "</td>");
                stringBuilder.Append("<td>" + e.Select(x => x.SubscriptionId).Distinct().Count() + "</td>");
                return stringBuilder.ToString();
            };

            int numProcs = Environment.ProcessorCount;
            int concurrencyLevel = numProcs * 2;
            int initialCapacity = 500;

            IDictionary<string, string> ErrorCodeBugMap = new ConcurrentDictionary<string, string>(concurrencyLevel, initialCapacity);

            Parallel.ForEach(
                ClientLastRequestErrorMap.OrderByDescending(x => x.Value.Count), (item) =>
                {
                    var ErrorCodeName = item.Key;
                    var errorObj = item.Value;

                    string bugIds = "No Bug";

                    if (workItems.ContainsKey(ErrorCodeName) && workItems[ErrorCodeName].Count > 0)
                    {
                        bugIds = string.Join("\n", workItems[ErrorCodeName].Select(x =>
                            string.Format(@"<a href=""https://msazure.visualstudio.com/One/_workitems/edit/{0}"">",
                                        x.Fields["System.Id"].ToString()) +
                            string.Join(": ", x.Fields["System.Id"].ToString(),
                                                x.Fields["System.Title"].ToString()) + @"</a>"));
                    }

                    ErrorCodeBugMap.Add(ErrorCodeName, bugIds);
                });

            foreach(var item in ClientLastRequestErrorMap.OrderByDescending(x => x.Value.Count))
            { 
                var ErrorCodeName = item.Key;
                var errorObj = item.Value;

                string bugIds = ErrorCodeBugMap[ErrorCodeName];

                summarySB.Append("<tr>");
                summarySB.Append("<td>" + ErrorCodeName + "</td>");
                summarySB.Append(FormattedFailure(errorObj, (ulong)ClientLastRequestErrorMap.Sum(x => x.Value.Count)));
                summarySB.Append(FormattedFailure(ClientAllRequestErrorMap[ErrorCodeName], (ulong)ClientAllRequestErrorMap.Sum(x => x.Value.Count)));
                summarySB.Append("<td>" + bugIds + "</td>");
                summarySB.Append("</tr>");
            }

            Action<IDictionary<string, IList<ErrorObject>>, StringBuilder> AddContents = (clientMap, contents) =>
            {
                ulong ulFailures = (ulong)clientMap.Sum(x => x.Value.Count);
                foreach (var item in clientMap.OrderByDescending(x=> x.Value.Count))
                {
                    foreach (var obj in item.Value)
                    {
                        contents.Append(obj.ErrorName + ",");
                        contents.Append(obj.PreciseTimeStamp.ToShortDateString() + ",");
                        contents.Append(obj.GeoName + ",");
                        contents.Append(obj.ClientRequestId + ",");
                        contents.Append(obj.SubscriptionId + ",");
                        contents.Append(obj.OSType + ",");
                        contents.Append(obj.AgentVersion + ",");
                        contents.Append(obj.OSName + ",");
                        contents.Append(obj.OSVersion + ",");
                        contents.AppendLine("");
                    }
                }
            };

            AddContents(ClientLastRequestErrorMap, customerJourneySB);
            AddContents(ClientAllRequestErrorMap, allErrorsSB);


            summarySB.Append("</table>");

            summaryMap.Add("summary", summarySB.ToString());
            summaryMap.Add("CustomerJourney", customerJourneySB.ToString());
            summaryMap.Add("AllRequests", allErrorsSB.ToString());
            return summaryMap;
            //summarySB.Append(customerJourneySB);
            //summarySB.Append(allErrorsSB);
            //return summarySB;
        }

        public static IDictionary<string, IList<ErrorObject>> GetSHRData(DateTime start, DateTime end, bool isCustomerJourney)
        {
            int numProcs = Environment.ProcessorCount;
            int concurrencyLevel = numProcs * 2;
            int initialCapacity = 500;

            IDictionary<string, IList<ErrorObject>> ClientRequestErrorMap = new ConcurrentDictionary<string, IList<ErrorObject>>(concurrencyLevel, initialCapacity);

            string endTime = string.Format("datetime({0})", end.ToShortDateString());
            string startTime = string.Format("datetime({0})", start.ToShortDateString());

            Parallel.ForEach(SHRConstants.ASRClusterMap.Keys, (ClusterName) =>
            {
                string Cluster = string.Format("https://{0}.kusto.windows.net", ClusterName);
                string Database = SHRConstants.ASRClusterMap[ClusterName];

                using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{Database};Fed=true"))
                {
                    string query3 = string.Format(SHRQuery.SHRErrorQuery,
                                        "\"" + ClusterName + "\"", "\"ASRKustoDB\"", startTime, endTime);
                    string query4 = query3;

                    if (isCustomerJourney)
                    {
                        query4 += string.Format(SHRQuery.lastRequest, startTime);
                    }

                    Console.WriteLine("Executing Query: {0}", query4);

                    using (var reader = queryProvider.ExecuteQuery(query4))
                    {
                        while (reader.Read())
                        {
                            var ClientRequestId = reader.GetString(0);
                            var PreciseTimeStamp = reader.GetDateTime(1).Date;
                            var ErrorName = reader.GetString(2);
                            var ErrorType = reader.GetString(3);
                            var ErrorGroup = reader.GetString(4);
                            var StampName = reader.GetString(5);
                            var index = StampName.IndexOf("-pod01-srs1");
                            var GeoName = StampName.Remove(index);
                            var SubscriptionId = reader.GetString(6);
                            var objectId = reader.GetString(7);
                            var OSType = reader.GetString(8);
                            var agentVersion = reader.GetString(9);
                            var osName = reader.GetString(10);
                            var osVersion = reader.GetString(11);

                            if (!ClientRequestErrorMap.ContainsKey(ErrorName))
                            {
                                ClientRequestErrorMap.Add(ErrorName, new List<ErrorObject>());
                            }

                            ClientRequestErrorMap[ErrorName].Add(new ErrorObject(ClientRequestId, PreciseTimeStamp, ErrorName,
                                ErrorType, ErrorGroup, StampName, SubscriptionId, OSType, agentVersion, osName, osVersion, objectId));
                        }
                    }
                }
            });

            return ClientRequestErrorMap;
        }

    }
}