using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Kusto.Cloud.Platform.Data;
using Kusto.Data.Common;
using Kusto.Data.Net.Client;
using System.Collections.Concurrent;
using System.Threading;

namespace Microsoft.Internal.SiteRecovery.ASRDiagnostic
{
    public partial class Progress : Form
    {
        public string SubscriptionID, ObjectName, ClientRequestID, StartTime, EndTime, DirPath;
        public Tuple<string, int> CurrentState;
        public Progress(string SubscriptionID, string ObjectName, string ClientRequestID, string StartTime, string EndTime, string DirPath)
        {
            InitializeComponent();

            this.SubscriptionID = SubscriptionID;
            this.ObjectName = ObjectName;
            this.ClientRequestID = ClientRequestID;
            this.StartTime = StartTime;
            this.EndTime = EndTime;
            this.DirPath = DirPath;
            CurrentState = new Tuple<string, int>("Initializing...", 0);

            backgroundWorker1.RunWorkerAsync();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            DialogResult quit=MessageBox.Show("Do you realy want to exit?", "Exit", MessageBoxButtons.YesNo);

            if (quit == DialogResult.Yes)
            {
                CurrentState = new Tuple<string, int>("Cancelling...", 0);
                backgroundWorker1.ReportProgress(1);
                backgroundWorker1.CancelAsync();
            }
        }

        private void progressBar1_Click(object sender, EventArgs e)
        {

        }

        public ConcurrentDictionary<string,List<QueryOutput>> GetDiagnosticData(string SubscriptionID, string ObjectName, string ClientRequestID, string StartTime, string EndTime)
        {
            backgroundWorker1.ReportProgress(1);

            int initialCapacity = 101;
            int numProcs = Environment.ProcessorCount;
            int concurrencyLevel = numProcs * 2;

            ConcurrentDictionary<string, List<QueryOutput>> QryOutput = new ConcurrentDictionary<string, List<QueryOutput>>(concurrencyLevel, initialCapacity);

            if (backgroundWorker1.CancellationPending)
                return QryOutput;

            CurrentState = new Tuple<string, int>("Executing Summary Queries...",1);
            backgroundWorker1.ReportProgress(1);

            string DatabaseName = "ASRKustoDB";

            string qry1, qry2;

            if (ObjectName == "")
            {
                if (ClientRequestID == "")
                {
                    qry1 = string.Format(Query.query6, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"");
                    qry2 = string.Format(Query.query3, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"");
                }
                else
                {
                    qry1 = string.Format(Query.query8, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"", "\"" + ClientRequestID + "\"");
                    qry2 = string.Format(Query.query1, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"", "\"" + ClientRequestID + "\"");
                }

            }
            else
            {
                if (ClientRequestID == "")
                {
                    qry1 = string.Format(Query.query7, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"", "\"" + ObjectName + "\"");
                    qry2 = string.Format(Query.query2, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"", "\"" + ObjectName + "\"");
                }
                else
                {
                    qry1 = string.Format(Query.query5, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"", "\"" + ObjectName + "\"", "\"" + ClientRequestID + "\"");
                    qry2 = string.Format(Query.query4, "\"" + StartTime + "\"", "\"" + EndTime + "\"", "\"" + SubscriptionID + "\"", "\"" + ObjectName + "\"", "\"" + ClientRequestID + "\"");
                }
            }

            HashSet<string> ProtectedVm = new HashSet<string>();
            HashSet<string> EnableDrVm = new HashSet<string>();

            ConcurrentDictionary<string, List<QueryOutput>> FinalOutput = new ConcurrentDictionary<string, List<QueryOutput>>(concurrencyLevel, initialCapacity);

            Parallel.ForEach(Clusters.ClusterNames, (ClusterName, state) =>
            {
                string Cluster = string.Format("https://{0}.kusto.windows.net", ClusterName);

                using (var queryProvider = KustoClientFactory.CreateCslQueryProvider($"{Cluster}/{DatabaseName};Fed=true"))
                {
                    if (backgroundWorker1.CancellationPending)
                        state.Break();
                    
                    using (var reader = queryProvider.ExecuteQuery(qry1))
                    {
                        /* Provider
                         * SourceVmName                          //primary key
                         * HostId
                         * ProtectionState
                         * ReplicationHealth
                         * SourceAgentVersion
                         * OSName
                         * OSType
                         * OSVersion
                         * ErrorCode */

                        if (backgroundWorker1.CancellationPending)
                            state.Break();

                        CurrentState = new Tuple<string, int>("Fetching Summary Records...", 4);
                        backgroundWorker1.ReportProgress(1);

                        while (reader.Read())
                        {
                            if (backgroundWorker1.CancellationPending)
                                state.Break();

                            var rec = new QueryOutput(reader.GetString(0), reader.GetString(1), reader.GetString(2), reader.GetString(3), reader.GetString(4), reader.GetDouble(5), reader.GetString(6), reader.GetString(7), reader.GetString(8), reader.GetString(9));
                            rec.ClusterName = ClusterName;

                            ProtectedVm.Add(rec.ObjectName);
                            QryOutput.TryAdd(rec.ObjectName, new List<QueryOutput>());
                            QryOutput[rec.ObjectName].Add(rec);
                        }

                        CurrentState = new Tuple<string, int>("Fetching Summary Records...", 6);
                        backgroundWorker1.ReportProgress(1);
                    }

                    using (var reader = queryProvider.ExecuteQuery(qry2))
                    {
                        /* Provider
                         * ObjectName                          //primary key
                         * LastClientRequestId
                         * ScenarioName
                         * State
                         * AgentVersion
                         * OSName
                         * OSType
                         * OSVersion
                         * ErrorCodeName */

                        if (backgroundWorker1.CancellationPending)
                            state.Break();


                        CurrentState = new Tuple<string, int>("Fetching Summary Records...",4);
                        backgroundWorker1.ReportProgress(1);

                        while (reader.Read())
                        {
                            if (backgroundWorker1.CancellationPending)
                                state.Break();

                            var rec = new QueryOutput(reader.GetString(0), reader.GetString(1), reader.GetString(2), reader.GetString(3), reader.GetString(4), reader.GetDouble(5), reader.GetString(6), reader.GetString(7), reader.GetString(8), reader.GetString(9));
                            rec.ClusterName = ClusterName;

                            if(!ProtectedVm.Contains(rec.ObjectName))
                            {
                                QryOutput.TryAdd(rec.ObjectName, new List<QueryOutput>());
                                QryOutput[rec.ObjectName].Add(rec);
                                EnableDrVm.Add(rec.ObjectName);
                            }
                        }

                        CurrentState = new Tuple<string, int>("Fetching Summary Records...", 6);
                        backgroundWorker1.ReportProgress(1);
                    }
                }
            });

            CurrentState = new Tuple<string, int>("Fetching Error Logs...", 7);
            backgroundWorker1.ReportProgress(1);

            Parallel.ForEach(QryOutput, (record, state) =>
            {
                Parallel.ForEach(QryOutput[record.Key], (rec) =>
                {
                    string path = this.DirPath + "\\" + record.Key;          //doesn't include file extension
                                                                             //Log files will be named using ObjectName
                    if (backgroundWorker1.CancellationPending)
                        state.Break();

                    if(ProtectedVm.Contains(rec.ObjectName))
                    {
                        if (LogHandlers.ProtectedLogHandlerMethods[rec.Provider].ContainsKey(rec.ErrorCodeName))
                        {
                            path = LogHandlers.ProtectedLogHandlerMethods[rec.Provider][rec.ErrorCodeName](rec.ClusterName, rec.LastClientRequestId, StartTime, EndTime, path);
                        }
                        else if (rec.ErrorCodeName != "")                        //Call default handler
                        {
                            path = "No Handler Found for this ErrorCodeName! Please add one.";
                        }
                    }
                    else if(EnableDrVm.Contains(rec.ObjectName))
                    {
                        if (LogHandlers.ErLogHandlerMethods[rec.Provider].ContainsKey(rec.ErrorCodeName))
                        {
                            path = LogHandlers.ErLogHandlerMethods[rec.Provider][rec.ErrorCodeName](rec.ClusterName, rec.LastClientRequestId, StartTime, EndTime, path);
                        }
                        else if (rec.ErrorCodeName.Contains("MobilityService"))
                        {
                            path = LogHandlers.ErLogHandlerMethods[rec.Provider]["DefaultMobilityServiceHandler"](rec.ClusterName, rec.LastClientRequestId, StartTime, EndTime, path);
                        }
                        else if (rec.ErrorCodeName != "")                        //Call default handler
                        {
                            path = LogHandlers.ErLogHandlerMethods[rec.Provider]["DefaultErHandler"](rec.ClusterName, rec.LastClientRequestId, StartTime, EndTime, path);
                        }
                    }
                    rec.Logs = path;
                });
            });
            CurrentState = new Tuple<string, int>("Fetching Error Logs...", 8);
            backgroundWorker1.ReportProgress(1);

            return QryOutput;
        }

        private void Progress_Load(object sender, EventArgs e)
        {
            this.CancelButton = button1;
        }

        private void Progress_FormClosing(object sender, FormClosingEventArgs e)
        {
            backgroundWorker1.CancelAsync();
        }

        private void CurrentAction_Click(object sender, EventArgs e)
        {

        }

        public void CreateHtmlTable(ConcurrentDictionary<string, List<QueryOutput> > QryOutput,string FileName)
        {
            StringBuilder HtmlMarkup = new StringBuilder();

            HtmlMarkup.Append("<H1> Health Report for Customer Subscription ID : " + FileName + "\n\n\n</H1>\n");

            HtmlMarkup.Append("<TABLE border = \"2\">\n");

            HtmlMarkup.Append("<TR>\n");

            HtmlMarkup.Append("<TH>Provider</TH>\n");
            HtmlMarkup.Append("<TH>Object Name</TH>\n");
            HtmlMarkup.Append("<TH>Last Client Request ID</TH>\n");
            HtmlMarkup.Append("<TH>Scenario Name</TH>\n");
            HtmlMarkup.Append("<TH>State</TH>\n");
            HtmlMarkup.Append("<TH>Agent Version</TH>\n");
            HtmlMarkup.Append("<TH>OS Name</TH>\n");
            HtmlMarkup.Append("<TH>OS Type</TH>\n");
            HtmlMarkup.Append("<TH>OS Version</TH>\n");
            HtmlMarkup.Append("<TH>Error Code Name</TH>\n");
            HtmlMarkup.Append("<TH>Error Logs</TH>\n");

            HtmlMarkup.Append("</TR>\n");


            foreach(var records in QryOutput)
            {
                foreach(var rec in QryOutput[records.Key])
                {
                        HtmlMarkup.Append("<TR>\n");
                        HtmlMarkup.Append("<TD>" + rec.Provider + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.ObjectName + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.LastClientRequestId + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.ScenarioName + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.State + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.AgentVersion + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.OSName + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.OSType + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.OSVersion + "</TD>");
                        HtmlMarkup.Append("<TD>" + rec.ErrorCodeName + "</TD>");
                        if(rec.ErrorCodeName != "" && rec.Logs != "" && rec.Logs == "No Handler Found for this ErrorCodeName! Please add one.")
                            HtmlMarkup.Append("<TD>" + rec.Logs  + "</TD>");
                        else
                            HtmlMarkup.Append("<TD> <A href=\"" + rec.Logs + "\">" + rec.ErrorCodeName + "</A>" + "</TD>");
                        HtmlMarkup.Append("</TR>\n");
                }
            }

            HtmlMarkup.Append("</TABLE>\n");

            string FilePath = this.DirPath + "\\" + FileName + ".htm";

            System.IO.File.WriteAllText(FilePath, HtmlMarkup.ToString());

            CurrentState = new Tuple<string, int>("Creating Summary Table...", 50);
            backgroundWorker1.ReportProgress(1);

            Thread.Sleep(1000);
        }

        private void backgroundWorker1_DoWork(object sender, DoWorkEventArgs e)
        {
            backgroundWorker1.ReportProgress(1);

            CurrentState = new Tuple<string, int>("Sending Customer Information to Kusto...", 7);

            var QryOutput = this.GetDiagnosticData(SubscriptionID, ObjectName, ClientRequestID, StartTime, EndTime);
            
            if (backgroundWorker1.CancellationPending)
                e.Cancel = true;
            else
            {
                CurrentState = new Tuple<string, int>("Creating Summary Table...", 0);
                backgroundWorker1.ReportProgress(1);

                this.CreateHtmlTable(QryOutput, SubscriptionID);
            }
        }

        private void backgroundWorker1_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            this.CurrentAction.Text = CurrentState.Item1;
            this.progressBar1.Increment(CurrentState.Item2);
        }

        private void backgroundWorker1_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            this.Close();
        }
        
    }
}
