using System;
using System.Collections.Generic;
using System.IO;
using System.Collections;
using System.Diagnostics;
using InMage.APIHelpers;
using InMage.APIModel;
using InMage.Test.API;

namespace CXAPITestAutomation
{
    class Program
    {
        static void Main(string[] args)
        {
            TraceListener traceListener = null;         
            try
            {
                Trace.Listeners.Clear();
                string logFilePath = String.Format("{0}\\TestAPI_{1}.log", Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location), DateTime.Now.ToString("yyyyMMddHHmmssffff"));
                traceListener = new System.Diagnostics.TextWriterTraceListener(logFilePath);
                Trace.Listeners.Add(traceListener);
                Trace.AutoFlush = true;

                Logger.Debug("================== BEGIN ==================");
                Console.WriteLine();
                string configFile = null;
                if (args.Length > 0)
                {
                    for (int index = 0; index < args.Length; index++)
                    {
                        if (String.Compare(args[index], "--ConfigFile", StringComparison.OrdinalIgnoreCase) == 0)
                        {
                            index++;
                            if (index == args.Length || String.IsNullOrEmpty(args[index]))
                            {
                                PrintUsage();
                                return;
                            }
                            configFile = args[index];
                        }
                        else
                        {
                            PrintUsage();
                            return;
                        }
                    }
                }
                
                if(String.IsNullOrEmpty(configFile))
                {
                    string executingAssemblyDir = System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
                    configFile = System.IO.Path.Combine(executingAssemblyDir, "CXAPITestAutomation.config");
                }
                Logger.Debug("Loading the configuration file " + configFile);                

                APITestExecutor executor = new APITestExecutor(new CXAPITest(), configFile);
                Dictionary<string, string> executionStatusOfAPIs = executor.Execute();

                Console.WriteLine("\r\n");
                Logger.Debug("\r\n***************************** Summary *****************************", true);
                Console.WriteLine();
                Logger.Debug("-----------------------------------------------", true);
                Logger.Debug(String.Format("{0, -30} {1, -10}", "API Name", "Execution Status"), true);
                Logger.Debug("-----------------------------------------------", true);
                foreach(KeyValuePair<string,string> status in executionStatusOfAPIs)
                {
                    Logger.Debug(String.Format("{0, -30} {1, -10}", status.Key, status.Value), true);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Exiting with the error: {0}", ex.Message);
                Logger.Error("Caught Exception. Stack: " + ex);
            }
            Logger.Debug("================== END ==================");
            if (traceListener != null)
            {
                traceListener.Close();
            }
        }

        private static void PrintUsage()
        {
            Console.WriteLine("Usage:");
            Console.WriteLine("=======");
            Console.WriteLine("CXAPITest.exe --ConfigFile <configuration file path>");
            Logger.Debug("Usage: CXAPITest.exe --ConfigFile <configuration file path>");
        }       
        
    }
}
