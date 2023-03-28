namespace ExchangeValidation
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Linq;
    using System.Threading;

    internal class ExchangeMain
    {
        private static int EXCH_NOT_INSTALLED = 2;
        private static int EXIT_ABNORMALLY = -1;
        private static int EXIT_GRACEFULLY = 0;
        private static List<KeyValuePair<string, KeyValuePair<string, KeyValuePair<DateTime, DateTime>>>> exitStatus = new List<KeyValuePair<string, KeyValuePair<string, KeyValuePair<DateTime, DateTime>>>>();
        private static int INVALID_ARGS = 1;
        private static bool mainExitStatus = true;
        private static int numDB;
        private static int thread_exitcode;
        private static string threadOutputFolder;
        private static object workerLocker = new object();

        public static string GetAssemblyCopyrightMessage()
        {
            string copyRights = "";
            object[] copyRightsAttrib = System.Reflection.Assembly.GetExecutingAssembly().GetCustomAttributes(typeof(System.Reflection.AssemblyCopyrightAttribute), false);
            if (copyRightsAttrib.Length != 0)
            {
                copyRights = ((System.Reflection.AssemblyCopyrightAttribute)copyRightsAttrib[0]).Copyright;
            }
            return copyRights;
        }

        private static int Main(string[] args)
        {
            Console.WriteLine(GetAssemblyCopyrightMessage());
            TextWriterTraceListener listener = new TextWriterTraceListener();
            try
            {
                listener = new TextWriterTraceListener(File.CreateText("ExchangeValidation.log"));
                Trace.Listeners.Add(listener);
                Trace.WriteLine("   DEBUG   ENTERED Main", DateTime.Now.ToString());
            }
            catch (Exception exception)
            {
                Console.WriteLine("[Warning] : Unable to create ExchangeValidation.log file");
                Console.WriteLine(exception.Message);
            }
            Exchange exchange = new Exchange();
            ExchangeVsnap vsnap = new ExchangeVsnap();
            ExchangeLogFlush flush = new ExchangeLogFlush();
            ExchangeConsistency consistency = new ExchangeConsistency();
            new ExchangeLogFlush();
            HashSet<string> commandList = new HashSet<string>();
            DateTime now = DateTime.Now;
            List<IntegrityResult> resultList = new List<IntegrityResult>();
            try
            {
                Console.Write("\nCommand Line : ");
                Trace.Write("Command Line        :    ");
                foreach (string str in Environment.GetCommandLineArgs())
                {
                    Console.Write("{0} ", str);
                    Trace.Write(" " + str);
                }
                Console.Write("\n");
                exchange.GetLogonInfo();
                Console.WriteLine("\nValidating Command Line arguments.....");
                if (!exchange.ValidateCmdLineArgs(args))
                {
                    exchange.ValidationUsage();
                    Trace.WriteLine("[Error] Please check the command line arguments");
                    Trace.Flush();
                    Trace.Close();
                    listener.Flush();
                    listener.Close();
                    return INVALID_ARGS;
                }
                Console.WriteLine("\nExchange Validation Start at : {0}", now);
                Trace.WriteLine("   " + exchange.inMageInstallPath, "InMage Agent Path   ");
                string applicationName = exchange.applicationName;
                string appType = "ExchangeIS";
                if ((string.Compare(exchange.applicationName, "exchange", true) == 0) || (string.Compare(exchange.applicationName, "exchange2003", true) == 0))
                {
                    applicationName = "exchange";
                    appType = "Exchange";
                }
                string exchEseutilPath = "";
                if (!exchange.IsExchangeServer(applicationName, exchange.bEseutil, exchange.eseutilPath, ref exchEseutilPath))
                {
                    Trace.Flush();
                    Trace.Close();
                    listener.Flush();
                    listener.Close();
                    return EXCH_NOT_INSTALLED;
                }
                Trace.WriteLine("   " + exchEseutilPath, "Exchange eseutil path");
                Trace.WriteLine("   INFO    Read Data from Batch script file", DateTime.Now.ToString());
                string batFilePath = exchange.inMageConsistencyPath + @"\";
                if ((string.Compare(exchange.applicationName, "exchange", true) == 0) || (string.Compare(exchange.applicationName, "exchange2003", true) == 0))
                {
                    batFilePath = batFilePath + "exchange";
                }
                else
                {
                    batFilePath = batFilePath + exchange.applicationName.ToLower();
                }
                batFilePath = batFilePath + "_consistency_validation.bat";
                commandList = exchange.ReadDataFromBatchFile(batFilePath);
                if (commandList.Count == 0)
                {
                    Console.WriteLine("[Error] Found no exchange configuration details in the file \"{0}\"", batFilePath);
                    Trace.Flush();
                    Trace.Close();
                    listener.Flush();
                    listener.Close();
                    return EXIT_ABNORMALLY;
                }
                if (!exchange.ParseBatchScript(commandList))
                {
                    Console.WriteLine("[Error] : Failed to parse Batch Script file");
                    Trace.Flush();
                    Trace.Close();
                    listener.Flush();
                    listener.Close();
                    return EXIT_ABNORMALLY;
                }
                Trace.WriteLine("   INFO    Persist MaxLogValidationThreadCount=2 in drscout.conf", DateTime.Now.ToString());
                exchange.WriteIniThreadCount("application", "MaxLogValidationThreadCount", "2");
                if (!consistency.CreateVsnapAndValidate(exchange.virtualServerName, batFilePath, exchange.sgdbPathDrive, exchange.eventTag, exchange.seqNum, exchange.bUseExistingVsnap, exchange.srcHostName, commandList, exchEseutilPath))
                {
                    Console.WriteLine("[Error] Failed to perform exchange consistency validation");
                    mainExitStatus = false;
                    goto Label_06CD;
                }
                ExchangeMain.threadOutputFolder = exchange.inMageInstallPath;
                ExchangeMain.threadOutputFolder = ExchangeMain.threadOutputFolder + @"\Application Data\ExchangeValidation\";
                ExchangeMain.threadOutputFolder = ExchangeMain.threadOutputFolder + exchange.seqNum;
                ExchangeMain.threadOutputFolder = ExchangeMain.threadOutputFolder + @"\";
                string threadOutputFolder = ExchangeMain.threadOutputFolder;
                if (exchange.CreateDir(ExchangeMain.threadOutputFolder))
                {
                    if (Directory.Exists(ExchangeMain.threadOutputFolder))
                    {
                        string text2 = ExchangeMain.threadOutputFolder;
                    }
                    else
                    {
                        ExchangeMain.threadOutputFolder = "./";
                    }
                }
                Trace.WriteLine("   INFO    Spawn Threads", DateTime.Now.ToString());
                int index = 0;
                int numThreads = exchange.GetNumThreads();
                numDB = consistency.commandList.Count<KeyValuePair<string, string>>();
                Thread[] threadArray = new Thread[consistency.commandList.Count<KeyValuePair<string, string>>()];
            Label_04BC:
                while (index < numThreads)
                {
                    if (index == consistency.commandList.Count<KeyValuePair<string, string>>())
                    {
                        break;
                    }
                    threadArray[index] = new Thread(new ParameterizedThreadStart(ExchangeMain.WorkerThread));
                    threadArray[index].Start(consistency.commandList[index]);
                    KeyValuePair<string, string> pair = consistency.commandList[index];
                    Trace.WriteLine("  " + pair.Value, "Thread Argument     ");
                    index++;
                }
                lock (workerLocker)
                {
                    while (numDB > 0)
                    {
                        Monitor.Wait(workerLocker);
                        if (index != consistency.commandList.Count<KeyValuePair<string, string>>())
                        {
                            numThreads = index + 1;
                            goto Label_04BC;
                        }
                    }
                }
                for (int i = 0; i < consistency.commandList.Count<KeyValuePair<string, string>>(); i++)
                {
                    threadArray[i].Join();
                }
                int num4 = 0;
                if (exitStatus.Count != 0)
                {
                    foreach (KeyValuePair<string, KeyValuePair<string, KeyValuePair<DateTime, DateTime>>> pair2 in exitStatus)
                    {
                        if (string.Compare(pair2.Value.Key, "5") == 0)
                        {
                            num4++;
                        }
                    }
                    if (num4 == consistency.commandList.Count<KeyValuePair<string, string>>())
                    {
                        Console.WriteLine("\n[WARNING] : Previous Tag and Current Tag are same for Exchange Validation");
                        Console.WriteLine("Please issue another tag and perform Exchange validation");
                        mainExitStatus = false;
                        goto Label_06CD;
                    }
                }
                Console.WriteLine("\n***** Checking for Exchange Logs and Databases Integirty results");
                string eventTag = exchange.eventTag;
                if (exchange.bUseExistingVsnap)
                {
                    eventTag = consistency.consistentTag;
                }
                exchange.ReadResultFromFile(exchange.seqNum, out resultList);
                if (resultList.Count != 0)
                {
                    flush.PersistLogFilesForFlush(resultList, consistency.proVolVsnapPath, applicationName, exchange.srcHostName, exchange.virtualServerName, exchange.seqNum, exchange.sgdbPathDrive, exchange.srcLOGPath, exchange.sgPathChkPrefix, exchEseutilPath);
                    if (exchange.bUseExistingVsnap)
                    {
                        vsnap.FindValidatedTagAndMarkAsVerified(resultList, consistency.validSgDbMap, consistency.volumeTagPairs, flush.storageGpForFlush, appType);
                    }
                    else
                    {
                        vsnap.MarkCommonTagAsVerified(flush.storageGpForFlush, consistency.sgdbmap, appType, eventTag);
                    }
                }
                else
                {
                    Console.WriteLine("      No Storage group logs are eligible for flushing");
                }
            }
            catch (Exception exception2)
            {
                Console.WriteLine("\n[Exception] Main \"{0}\"", exception2.Message);
                Trace.WriteLine("Exception in Main = " + exception2.Message);
                Trace.Flush();
                Trace.Close();
                listener.Flush();
                listener.Close();
                return EXIT_ABNORMALLY;
            }
        Label_06CD:
            if (!exchange.bUseExistingVsnap)
            {
                bool flag = true;
                Console.WriteLine("\n***** Started unmounting vsnap");
                if (consistency.proVolVsnapPath.Count != 0)
                {
                    foreach (KeyValuePair<string, string> pair3 in consistency.proVolVsnapPath)
                    {
                        Console.WriteLine("\nVolume = {0}\n", pair3.Value);
                        if (!vsnap.UnMountVsnap(exchange.inMageCdpCliPath, pair3.Value))
                        {
                            Console.WriteLine("Failed to unmount vsnap from the location \"{0}\"", pair3.Key);
                            flag = false;
                        }
                    }
                }
                if (flag)
                {
                    try
                    {
                        if (Directory.Exists(consistency.workingDirectory))
                        {
                            Directory.Delete(consistency.workingDirectory, true);
                        }
                    }
                    catch (Exception)
                    {
                    }
                }
            }
            Console.WriteLine("\nREPORT\n======\n");
            int num5 = 1;
            foreach (IntegrityResult result in resultList)
            {
                Console.WriteLine("\n***** EXCHANGE DATABASE : {0}\n", num5);
                Console.WriteLine("DBName  : {0} \nDBIntegrity  : {1}", result.dbFile, result.dbIntegrity);
                Console.WriteLine("LOGPath : {0} \nLOGIntegrity : {1}", result.logFile, result.logIntegrity);
                num5++;
            }
            DateTime dateAndTime = exchange.GetDateAndTime();
            TimeSpan span = dateAndTime.Subtract(now);
            Console.WriteLine("\nExchange Validation Started at : {0}", now);
            Console.WriteLine("Exchange Validation Ended   at : {0}", dateAndTime);
            Console.WriteLine("Total Time Taken               : {0} Days,{1} Hrs,{2} Mins,{3} Secs,{4} MilliSecs", new object[] { span.Days, span.Hours, span.Minutes, span.Seconds, span.Milliseconds });
            Console.WriteLine("\n***************End of Exchange Validation***************\n");
            if (!mainExitStatus)
            {
                Trace.WriteLine("   DEBUG   EXITED Main with status  = -1", DateTime.Now.ToString());
                Trace.Flush();
                Trace.Close();
                listener.Flush();
                listener.Close();
                return EXIT_ABNORMALLY;
            }
            Trace.WriteLine("   DEBUG   EXITED Main with status  = 0", DateTime.Now.ToString());
            Trace.Flush();
            Trace.Close();
            listener.Flush();
            listener.Close();
            return EXIT_GRACEFULLY;
        }

        private static void WorkerThread(object argument)
        {
            ProcessStartInfo info = new ProcessStartInfo();
            KeyValuePair<string, string> pair = (KeyValuePair<string, string>) argument;
            try
            {
                Process process;
                string threadOutputFolder;
                info.EnvironmentVariables.Add("TempPath", @"%SystemRoot%\System32");
                info.FileName = "cscript.exe";
                info.Arguments = pair.Value;
                info.UseShellExecute = false;
                info.RedirectStandardOutput = true;
                info.CreateNoWindow = true;
                Console.WriteLine("\n***** Started consistency validation for the database file : {0}", pair.Key);
                lock (workerLocker)
                {
                    threadOutputFolder = ExchangeMain.threadOutputFolder;
                    threadOutputFolder = threadOutputFolder + Path.GetFileNameWithoutExtension(pair.Key);
                    threadOutputFolder = threadOutputFolder + ".txt";
                    FileStream stream = new FileStream(threadOutputFolder, FileMode.Create, FileAccess.ReadWrite);
                    StreamWriter writer = new StreamWriter(stream);
                    process = new Process();
                    process.StartInfo = info;
                    process.Start();
                    writer.AutoFlush = true;
                    StreamReader standardOutput = process.StandardOutput;
                    writer.Write(standardOutput.ReadToEnd());
                    standardOutput.Close();
                    writer.Close();
                    stream.Close();
                }
                process.WaitForExit();
                thread_exitcode = process.ExitCode;
                Console.WriteLine("      Completed Validation for DBName : {0}, Return Code : {1}", pair.Key, thread_exitcode);
                Console.WriteLine("      Start Time : {0}, End Time : {1}", process.StartTime, process.ExitTime);
                Console.WriteLine("      Successfully persisted the results in the file \"{0}\"\n", threadOutputFolder);
                lock (workerLocker)
                {
                    exitStatus.Add(new KeyValuePair<string, KeyValuePair<string, KeyValuePair<DateTime, DateTime>>>(pair.Key, new KeyValuePair<string, KeyValuePair<DateTime, DateTime>>(thread_exitcode.ToString(), new KeyValuePair<DateTime, DateTime>(process.StartTime, process.ExitTime))));
                    numDB--;
                    Monitor.Pulse(workerLocker);
                }
                process.Close();
            }
            catch (Exception exception)
            {
                Console.WriteLine("[Error] : Error while spawning a thread");
                Console.WriteLine("[Exception] : {0}", exception.Message);
                Trace.WriteLine("Exception in thread = " + exception.Message);
                lock (workerLocker)
                {
                    numDB--;
                    Monitor.Pulse(workerLocker);
                }
            }
        }
    }
}

