using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace InMage.Test.API
{
    public class Logger
    {
        private static TraceSwitch cxapiTestSwitch = new TraceSwitch("APITestLib", "Switch in config file");

        /*
         * Info Method
         */
        public static void Info(String msg)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceInfo, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "INFO", msg));
        }

        /*
         * Info Method
         */
        public static void Info(String msg, bool displayOnConsole)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceInfo, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "INFO", msg));
            if (displayOnConsole)
            {
                Console.WriteLine(msg);
            }
        }

        /*
         * Warning
         */
        public static void Warn(String msg)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceWarning, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "WARN", msg));
        }

        /*
         * Warning
         */
        public static void Warn(String msg, bool displayOnConsole)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceWarning, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "WARN", msg));
            if (displayOnConsole)
            {
                Console.WriteLine(msg);
            }
        }

        /*
         * Error
         */
        public static void Error(String msg)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceError, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "ERROR", msg));
        }

        /*
        * Error
        */
        public static void Error(String msg, bool displayOnConsole)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceError, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "ERROR", msg));
            if (displayOnConsole)
            {
                Console.WriteLine(msg);
            }
        }

        /*
         * Debug
         */
        public static void Debug(String msg)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceVerbose, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "DEBUG", msg));
        }

        /*
         * Debug
         */
        public static void Debug(String msg, bool displayOnConsole)
        {
            Trace.WriteLineIf(cxapiTestSwitch.TraceVerbose, String.Format("{0,-23}{1,-8}{2}", DateTime.Now, "DEBUG", msg));
            if (displayOnConsole)
            {
                Console.WriteLine(msg);
            }
        } 
    }
}
