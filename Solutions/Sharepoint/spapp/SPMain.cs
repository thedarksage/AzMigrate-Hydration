using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Collections;
using System.Diagnostics;
using System.IO;

namespace spapp
{	 
    class SPMain
    {        
        public const int EXIT_CODE_EXECUTION_FAILED = 1;
        public const int EXIT_CODE_EXECUTION_SUCCESSFUL = 0;
        //public const int EXIT_CODE_EXECUTION_STATUS_UNKNOWN = 2;
 
        //Entry point to the program.
        static int Main(string[] args)
        {            
            int exitCode = EXIT_CODE_EXECUTION_SUCCESSFUL;
            try
            {
                Console.WriteLine();
                Console.WriteLine(Configuration.getCopyrightFromAssembly());

                //Creating a TraceLog for debug purpose                 
                String SpappDebugLOGpath = Configuration.vxAgentPath() + Configuration.SPAPP_TRACE_DIR + "SpappTrace.log";
                if (!Directory.Exists(Configuration.vxAgentPath() + Configuration.SPAPP_TRACE_DIR))
                {
                    try
                    {
                        Directory.CreateDirectory(Configuration.vxAgentPath() + Configuration.SPAPP_TRACE_DIR);
                    }
                    catch (Exception exp)
                    {
                        Console.WriteLine("Failed to create Directory\t" + exp.Message);
                    }
                }
                TextWriterTraceListener TraceLog2 = new TextWriterTraceListener(File.AppendText(SpappDebugLOGpath));
                Trace.Listeners.Add(TraceLog2);
                Trace.AutoFlush = true;
                Trace.WriteLine("\n==================== SPAPP TRACE INFORMATION LOG ====================\n");

                if (args.Length != 1)
                {
                    PrintUsage();
                    return EXIT_CODE_EXECUTION_FAILED; 
                }
                for (int index = 0; index < args.Length; index++)
                {
                    if (String.Compare(args[index], "--discover", true) == 0)
                    {
                        SharepointDiscovery spDiscovery = new SharepointDiscovery();
                        if (!spDiscovery.Discover(true))
                        {
                            exitCode = EXIT_CODE_EXECUTION_FAILED;
                        }
                    }
                    else
                    {
                        PrintUsage();
                        return EXIT_CODE_EXECUTION_FAILED;
                    }
                }
                //Close the log file
                try
                {
                    Trace.Flush();
                    Trace.Close();
                }
                catch (Exception exp)
                {
                    Trace.WriteLine("Trace listerners are flushed and closed already" + exp.Message);
                }
            }
            catch (Exception ex)
            {
                exitCode = EXIT_CODE_EXECUTION_FAILED;
                Trace.WriteLine(String.Format("{0}\tCaught exception in main. Exception:{1}", DateTime.Now.ToString(), ex.ToString()));
                Console.WriteLine("Exiting spapp with error: {0}", ex.Message);
            }
            return exitCode;
        }

        static void PrintUsage()
        {
            Console.WriteLine("\nDescription: This tool is used to perform SharePoint discovery in a local farm.");
            Console.WriteLine("\nUsage: spapp --discover\n");              
        }
       
    }//end of spmain
}//end of namespace
