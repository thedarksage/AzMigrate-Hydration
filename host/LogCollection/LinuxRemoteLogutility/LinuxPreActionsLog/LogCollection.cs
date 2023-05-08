using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;

namespace LinuxPreActionLog
{
    class LogCollection
    {
        static int flag = 0,logLevel = 0;
        static string ipAddress = "", userName = "", password = "",action;
        static uint port = 0;
        static int Main(string[] args)
        {
            try
            {
                string currentDir = Directory.GetCurrentDirectory();
                string errorLogFilePath = currentDir + @"\loglevel.log";
                if (File.Exists(errorLogFilePath)) File.Delete(errorLogFilePath);

                TextWriterTraceListener m_traceListener = new TextWriterTraceListener(File.AppendText(errorLogFilePath));
                Trace.Listeners.Add(m_traceListener);
                Trace.AutoFlush = true;
                Console.Title = "Source/MT Logs Collector";

                flag = ValidateArgs(args);
                if (flag == 0)
                {
                    LogLevel ll = new LogLevel();
                    if (logLevel != 0)
                    {
                        flag = ll.LogLevelChange(ipAddress, userName, password, port, logLevel);
                        if (flag != 0)
                            return flag;
                    }

                    if (action != null)
                    {
                        flag = ll.ServiceUpdate(ipAddress, userName, password, port, action);
                        if (flag != 0)
                            return flag;
                    }

                }

                Trace.Flush();
                Trace.Close();

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
                if (args.Length == 0) return Constants.NoArguments;

                HashSet<string> validFlags = new HashSet<string>();
                validFlags.Add("-ip");
                validFlags.Add("-username");
                validFlags.Add("-password");
                validFlags.Add("-port");
                validFlags.Add("-loglevel");
                validFlags.Add("-action");


                for (int i = 0; i < args.Length; i++)
                {
                    if (!validFlags.Contains(args[i].ToLower()))
                    {
                        return Constants.InvalidFlag;
                    }
                    if (string.Compare(args[i].ToLower(), "-ip", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedIp;
                        }
                        ipAddress = args[i];

                    }
                    else if (string.Compare(args[i].ToLower(), "-username", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedUserName;
                        }
                        userName = args[i];
                    }
                    else if (string.Compare(args[i].ToLower(), "-password", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedPassword;
                        }
                        password = args[i];
                    }
                    else if (string.Compare(args[i].ToLower(), "-port", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedPort;
                        }
                        port = Convert.ToUInt32(args[i]);
                    }
                    else if (string.Compare(args[i].ToLower(), "-loglevel", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedLogLevel;
                        }
                        logLevel = Convert.ToInt32(args[i]);
                    }
                    else if (string.Compare(args[i].ToLower(), "-action", true) == 0)
                    {
                        i++;
                        if ((i == args.Length) || (args[i][0] == '-'))
                        {
                            return Constants.UndefinedAction;
                        }
                        action = args[i];
                    }

                }
                if (String.IsNullOrEmpty(ipAddress))
                {
                    return Constants.UndefinedIpFlag;
                }
                if (String.IsNullOrEmpty(userName))
                {
                    return Constants.UndefinedUserNameFlag;
                }
                if (String.IsNullOrEmpty(password))
                {
                    return Constants.UndefinedPasswordFlag;
                }
                if (port == 0)
                {
                    return Constants.UndefinedPort;
                }
                
            }
            catch (Exception e)
            {
                Trace.Write("Exception in ValidateArgs()");
                Trace.Write(String.Format("Exception Details : {0}", e.Message));
            }
            return Constants.SuccessReturnCode;

        }
    }
}


