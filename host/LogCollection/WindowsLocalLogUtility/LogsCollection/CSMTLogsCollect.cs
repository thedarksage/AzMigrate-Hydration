using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Diagnostics;

namespace LogsCollection
{
    class CSMTLogsCollect
    {
        static string serverType;
        static bool sflag= false;

        public static int Main(string[] args)
        {
            int flag=0;
            try
            {
                Console.Title = "Azure CS/PS/Source/MT Logs Collector";
                
                flag = ValidateArgs(args);
                string hostName = Environment.MachineName;
                string zipName = hostName + "-" + serverType.ToUpper() + "-" + DateTime.Now.ToString("dd-MM-HH-mm-ss").ToString();
              
                if (flag == 0)
                {
                    string folder = Path.Combine(Directory.GetCurrentDirectory(), zipName);
                    if (!Directory.Exists(folder))
                    {
                        Directory.CreateDirectory(folder);
                    }
                    
                    string errorLogFileName = hostName + "-" + serverType.ToUpper() + "-" + DateTime.Now.ToString("dd-MM-HH-mm-ss").ToString() + "-" + "utility.log";
                    string errorLogFilePath = Directory.GetCurrentDirectory() + @"\" + errorLogFileName;
                    TextWriterTraceListener m_traceListener = new TextWriterTraceListener(File.AppendText(errorLogFilePath));
                    Trace.Listeners.Add(m_traceListener);
                    Trace.AutoFlush = true;
                    ServerAgentLogs sa = new ServerAgentLogs();
                    flag = sa.CollectLogs(folder,serverType);
                    
               
                    Trace.Flush();
                    Trace.Close();
                } 
            }
            catch (Exception ex)
            {
                Trace.WriteLine(ex.Message);
               
            }
            return flag;
        }

        private static int ValidateArgs(string[] args)
        {
            try
            {
                if (args.Length == 0)
                    return Constants.NoArguments;

                HashSet<string> validFlags = new HashSet<string>();
                validFlags.Add("-stype");

                HashSet<string> validServerType = new HashSet<string>();
                validServerType.Add("cs");
                validServerType.Add("ps");
                validServerType.Add("source");
                validServerType.Add("mt");


                for (int i = 0; i < args.Length; i++)
                {

                    if (!validFlags.Contains(args[i].ToLower()))
                    {
                        return Constants.InvalidFlag;
                    }
                    
                    if (string.Compare(args[i].ToLower(), "-stype", true) == 0)
                    {
                        sflag = true;
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedServerType;
                        }
                        serverType = args[i].ToLower();
                        if (!validServerType.Contains(serverType))
                        {
                            return Constants.InvalidServerTpe;
                        }
                    }

                }
                if (!sflag)
                {
                    return Constants.UndefinedServerTypeFlag;
                   
                }

            }
            catch (Exception e)
            {
                Trace.Write("Exception in ValidateArgs()");
                Trace.Write(String.Format("Exception Details : {0}", e.Message));
                //return false;
            }
            return Constants.SuccessReturnCode;
        }
    }
}