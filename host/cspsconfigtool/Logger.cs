using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;


namespace Logging
{
    public class Logger
    {
        private static TraceSwitch guiTraceSwitch = new TraceSwitch("cspsconfigtool", "Switch in config file");

        //public static void SetLogger()
        //{
        //    string logFilePath = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) + "\\cspsconfigtool.log";
        //    FileStream fs = new RollingFileStream(logFilePath, 5242880, 1, FileMode.OpenOrCreate);
        //    TextWriterTraceListener myListener = new FormattedTextWriterTraceListener(fs);
        //    Trace.Listeners.Remove("Default");
        //    Trace.Listeners.Add(myListener);
        //    Trace.AutoFlush = true;
        //}

        /*
         * Info Method
         */
        public static void Info(String msg)
        {
            Trace.WriteLineIf(guiTraceSwitch.TraceInfo, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "INFO", msg));
        }

        /*
         * Warning
         */
        public static void Warn(String msg)
        {
            Trace.WriteLineIf(guiTraceSwitch.TraceWarning, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "WARN", msg));
        }

        /*
         * Error
         */
        public static void Error(String msg)
        {
            Trace.WriteLineIf(guiTraceSwitch.TraceError, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "ERROR", msg));
        }

        /*
         * Debug
         */
        public static void Debug(String msg)
        {
            Trace.WriteLineIf(guiTraceSwitch.TraceVerbose, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "DEBUG", msg));
        }

        /* 
         * Flush and close the Trace Listeners         
         */
        public static void Close()
        {
            Trace.Flush();
            Trace.Close();
        } 
    }
}
