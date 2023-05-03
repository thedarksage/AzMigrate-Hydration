using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;

namespace WrapperUnifiedAgent
{
    class UnifiedAgentLogs
    {

        /// <summary>
        /// Log files path.
        /// </summary>
        private static string logsPath;

        /// <summary>
        /// Initializes a new instance of the UnifiedAgentLogs class.
        /// </summary>
        /// <param name="fileName"> log file name.</param>
        public UnifiedAgentLogs(string fileName)
        {
            this.SetLogPath();
            this.CreateLogFile(fileName);
        }

        /// <summary>
        /// Gets the debug log writer.
        /// </summary>
        public static StreamWriter LogWriter { get; private set; }

        /// <summary>
        /// Write into log file.
        /// </summary>
        public static void LogMessage(TraceEventType type, string message, params object[] args)
        {
            string timeStamp = DateTime.Now.ToString();
            message = string.Format(message, args);
            string logMessage;
            switch (type)
            {
                case TraceEventType.Critical:
                    logMessage = timeStamp + ": CRITICAL :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Error:
                    logMessage = timeStamp + ": ERROR :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Warning:
                    logMessage = timeStamp + ": WARNING :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Information:
                    logMessage = timeStamp + ": INFO :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                case TraceEventType.Verbose:
                    logMessage = timeStamp + ": DEBUG :: " + message;
                    LogWriter.WriteLine(logMessage);
                    break;

                default:
                    logMessage = timeStamp + ": EXCEPTION :: " + message;
                    LogWriter.WriteLine(logMessage);
                    LogWriter.WriteLine(
                        string.Format("Unexpected error of type {0}", type));
                    break;
            }
        }

        /// <summary>
        /// Creates log file
        /// </summary>
        /// <param name="fileName">log file name to be created</param>
        private void CreateLogFile(string fileName)
        {
            string timeStamp = DateTime.Now.ToString();
            if (!Directory.Exists(logsPath))
            {
                Directory.CreateDirectory(logsPath);
            }

            string logFile = logsPath + "\\" + fileName;
            LogWriter = File.AppendText(logFile);
            LogWriter.AutoFlush = true;
        }

        /// <summary>
        /// Sets log file path
        /// </summary>
        private void SetLogPath()
        {
            string strProgramDataPath =
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            logsPath = strProgramDataPath + "\\ASRSetupLogs";
        }
    }
}
