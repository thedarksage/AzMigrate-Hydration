using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace InMage.Logging
{
    public class Logger
    {
        private static TraceSwitch inmageAPILibrarySwitch = new TraceSwitch("InMageAPILibrary", "Switch in config file");

        /*
         * Info Method
         */
        public static void Info(String msg)
        {
            Trace.WriteLineIf(inmageAPILibrarySwitch.TraceInfo, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "INFO", msg));
        }

        /*
         * Warning
         */
        public static void Warn(String msg)
        {
            Trace.WriteLineIf(inmageAPILibrarySwitch.TraceWarning, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "WARN", msg));
        }

        /*
         * Error
         */
        public static void Error(String msg)
        {
            Trace.WriteLineIf(inmageAPILibrarySwitch.TraceError, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "ERROR", msg));
        }

        /*
         * Debug
         */
        public static void Debug(String msg)
        {
            Trace.WriteLineIf(inmageAPILibrarySwitch.TraceVerbose, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "DEBUG", msg));
        }
 
    }
}
