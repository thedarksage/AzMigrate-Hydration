namespace ASRSetupFramework
{
    using System;
    using System.Globalization;
    using System.IO;
    using Microsoft.Win32;

    public enum LogLevel
    {
        /// <summary>
        /// Always trace Level
        /// </summary>
        Always = -1,

        /// <summary>
        /// None trace Level
        /// </summary>
        None = 0,

        /// <summary>
        /// Error trace Level
        /// </summary>
        Error = 1,

        /// <summary>
        /// Warn trace Level
        /// </summary>
        Warn = 2,

        /// <summary>
        /// Debug trace Level
        /// </summary>
        Debug = 4,

        /// <summary>
        /// Info trace Level
        /// </summary>
        Info = 8
    };

    public static class Trc
    {
        private static StreamWriter s_logFile = new StreamWriter(@"c:\test\test.log");

        //LogException
        public static void Log(LogLevel level, string trace, params object[] parameters)
        {
            s_logFile.WriteLine(string.Format(CultureInfo.InvariantCulture, trace, parameters));
            s_logFile.Flush();
        }

        public static void Log(LogLevel level, params object[] parameters)
        {
            for (int i = 0; i < parameters.Length; i++)
            {
                s_logFile.Write(parameters[i]);
            }
            s_logFile.WriteLine("");
            s_logFile.Flush();
        }

        public static void LogException(LogLevel level, string trace, params object[] parameters)
        {
            s_logFile.WriteLine(string.Format(CultureInfo.InvariantCulture, trace, parameters));
            s_logFile.Flush();
        }

        public static void LogException(LogLevel level, params object[] parameters)
        {
            for (int i = 0; i < parameters.Length; i++)
            {
                s_logFile.Write(parameters[i]);
            }
            s_logFile.WriteLine("");
            s_logFile.Flush();
        }

    }
}