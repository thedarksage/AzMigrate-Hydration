//-----------------------------------------------------------------------
// <copyright file="TraceUtil.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Provides tracing.
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    using System;
    using System.Globalization;
    using System.IO;
    using Microsoft.Win32;

    /// <summary>
    /// Values used to determine log level
    /// </summary>
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
        /// Info trace Level
        /// </summary>
        Info = 4,

        /// <summary>
        /// Debug trace Level
        /// </summary>
        Debug = 8
    }

    /// <summary>
    /// Static class, initialized once per process and used by all setup managed code in process
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "All trace classes in one file")]
    [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Naming", "CA1704:IdentifiersShouldBeSpelledCorrectly", Justification = "We needed a short, easy to use class name", MessageId = "Trc")]
    public static class Trc
    {
        #region private members

        /// <summary>
        /// Lock Object
        /// </summary>
        private static readonly object lockObject = new object();

        /// <summary>
        /// Trace Util instance used to do all the real work
        /// </summary>
        private static CommonLogger traceWorker; // is set to null automatically by the runtime.

        /// <summary>
        /// Level to trace at
        /// </summary>
        private static LogLevel traceLevel;

        /// <summary>
        /// Name for the trace log
        /// </summary>
        private static string traceFileName;

        #endregion

        #region Settings

        /// <summary>
        /// Sets the logging level for all associated loggers
        /// </summary>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1044:PropertiesShouldNotBeWriteOnly", Justification = "We currently only write to this property")]
        public static LogLevel Level
        {
            set
            {
                lock (lockObject)
                {
                    if (null != traceWorker)
                    {
                        traceWorker.Level = value;
                    }
                }
            }
        }

        #endregion

        #region public methods

        #region Initializers

        /// <summary>
        /// Initilizes the specified trace level to set.
        /// </summary>
        /// <param name="traceLevelToSet">The trace level to set.</param>
        /// <param name="traceFileNameToSet">The trace file name to set.</param>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1031:DoNotCatchGeneralExceptionTypes", Justification = "If we have an error here, we don't have anywhere to log it")]
        public static void Initialize(LogLevel traceLevelToSet, string traceFileNameToSet)
        {
            lock (lockObject)
            {
                traceLevel = traceLevelToSet;
                traceFileName = traceFileNameToSet;

                string logFolderPath = SetupHelper.SetLogFolderPath();
                string backupLogFolderPath = logFolderPath + @"\Backup";

                try
                {
                    if (!Directory.Exists(backupLogFolderPath))
                    {
                        Directory.CreateDirectory(backupLogFolderPath);
                    }

                    if (File.Exists(traceFileName))
                    {
                        string destFileName = backupLogFolderPath + "\\" + DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss") +
                            traceFileName.Substring(traceFileName.LastIndexOf("\\", StringComparison.OrdinalIgnoreCase) + 1);
                        File.Move(traceFileName, destFileName);
                    }
                }
                catch(Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Failed to create directory " + backupLogFolderPath +
                        " or move file " + traceFileName + 
                        " to backup folder with following exception " + ex);
                }
                
                // See if there is an overriding level in the registry
                RegistryKey baseSetupTraceKey = Registry.LocalMachine.OpenSubKey("Software\\Microsoft\\SystemCenter\\Setup\\Trace");
                if (null != baseSetupTraceKey)
                {
                    RegistryKey applicationSetupTraceKey = baseSetupTraceKey.OpenSubKey(
                        System.Reflection.Assembly.GetExecutingAssembly().GetName().Name);

                    // If there is an Appplication Setup Trace Key, use it
                    if (null != applicationSetupTraceKey)
                    {
                        try
                        {
                            traceLevel = (LogLevel)(int)applicationSetupTraceKey.GetValue(null);
                        }
                        catch (Exception exception)
                        {
                            Trc.Log(
                                LogLevel.Info,
                                string.Format(
                                CultureInfo.InvariantCulture,
                                "Unable to configure the setup trace with the application specific log level" + "\n\nException:\n{0}",
                                exception.Message));
                        }
                    }
                }

                if (string.IsNullOrEmpty(traceFileName))
                {
                    throw new ArgumentException("Trc.Initialize: Invalid argument.  traceFileNameToSet is not set to a FQFN");
                }

                traceWorker = new CommonLogger(traceLevel, traceFileName);
            }
        }

        #endregion

        #region Trace methods

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        public static void Log(LogLevel level, string trace)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.Log(level, trace);
                }
            }
        }

        /// <summary>
        /// Log a new line
        /// </summary>
        /// <param name="level">The level.</param>
        public static void LogLine(LogLevel level)
        {
            Trc.Log(level, "\n");
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="customMessage">The custom message.</param>
        /// <param name="exception">The exception.</param>
        public static void LogErrorException(string customMessage, Exception exception)
        {
            lock (lockObject)
            {
                Trc.Log(LogLevel.Error, "Error:{0}, Exception Type: {1}, Exception Message: {2}", customMessage, exception.GetType().ToString(), exception.Message);
                if (!string.IsNullOrEmpty(exception.StackTrace))
                {
                    Trc.Log(LogLevel.Error, "StackTrace:{0}", exception.StackTrace);
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="logLevel">Log Level.</param>
        /// <param name="customMessage">The custom message.</param>
        /// <param name="exception">The exception.</param>
        public static void LogException(
            LogLevel logLevel,
            string customMessage,
            Exception exception)
        {
            lock (lockObject)
            {
                Trc.Log(
                    logLevel,
                    "{0}: Threw Exception.Type: {1}, Exception.Message: {2}",
                    customMessage,
                    exception.GetType().ToString(),
                    exception.Message);

                Trc.Log(logLevel, "StackTrace:{0}", exception.StackTrace);

                Exception innerException = exception.InnerException;
                while (innerException != null)
                {
                    Trc.Log(
                        logLevel,
                        "InnerException.Type: {1}, InnerException.Message: {2}",
                        customMessage,
                        innerException.GetType().ToString(),
                        innerException.Message);

                    Trc.Log(logLevel, "InnerException.StackTrace:{0}", innerException.StackTrace);

                    innerException = innerException.InnerException;
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="parameter">The parameter.</param>
        public static void Log(LogLevel level, string trace, object parameter)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.Log(level, trace, parameter);
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="parameter1">The parameter1.</param>
        /// <param name="parameter2">The parameter2.</param>
        public static void Log(LogLevel level, string trace, object parameter1, object parameter2)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.Log(level, trace, parameter1, parameter2);
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="parameter1">The parameter1.</param>
        /// <param name="parameter2">The parameter2.</param>
        /// <param name="parameter3">The parameter3.</param>
        public static void Log(LogLevel level, string trace, object parameter1, object parameter2, object parameter3)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.Log(level, trace, parameter1, parameter2, parameter3);
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="parameters">The parameters.</param>
        public static void Log(LogLevel level, string trace, params object[] parameters)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.Log(level, trace, parameters);
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        public static void LogSessionStart(LogLevel level, string trace)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.LogSessionStart(level, trace);
                }
            }
        }

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        public static void LogSessionEnd(LogLevel level, string trace)
        {
            lock (lockObject)
            {
                if (null != traceWorker)
                {
                    traceWorker.LogSessionEnd(level, trace);
                }
            }
        }

        #endregion

        #endregion
    }

    /// <summary>
    /// Class which implements the common logging functions
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "All trace classes in one file")]
    internal class CommonLogger
    {
        /// <summary>
        /// The trace level
        /// </summary>
        private LogLevel traceLevel = LogLevel.Error | LogLevel.Warn;

        /// <summary>
        /// Shows if we are currently in a session
        /// </summary>
        private bool inSession; // is set to false automatically by the runtime.

        /// <summary>
        /// The name of the file to write the log to
        /// </summary>
        private string logFileName;

        #region Public members

        /// <summary>
        /// Initializes a new instance of the CommonLogger class.
        /// </summary>
        /// <param name="traceLevel">The trace level.</param>
        /// <param name="logFileName">Name of the log file.</param>
        public CommonLogger(LogLevel traceLevel, string logFileName)
        {
            // Call this to read the reader specific settings
            this.Level = traceLevel;
            this.LogFileName = logFileName;
            this.LogSessionStart(LogLevel.Always, string.Empty);
        }

        /// <summary>
        /// Finalizes an instance of the CommonLogger class.
        /// <see cref="CommonLogger"/> is reclaimed by garbage collection.
        /// </summary>
        ~CommonLogger()
        {
            this.LogSessionEnd(LogLevel.Always, string.Empty);
        }

        /// <summary>
        /// Gets or sets the name of the log file.
        /// </summary>
        /// <value>The name of the log file.</value>
        public string LogFileName
        {
            get { return this.logFileName; }
            set { this.logFileName = value; }
        }

        /// <summary>
        /// Gets or sets Accessor for trace level
        /// </summary>
        public LogLevel Level
        {
            get { return this.traceLevel; }
            set { this.traceLevel = value; }
        }

        /// <summary>
        /// Determines if the logger will trace on the specified level
        /// </summary>
        /// <param name="level">the log level to check for</param>
        /// <returns>true if the level matches</returns>
        public bool TraceForLevel(LogLevel level)
        {
            return (level == LogLevel.Always) || ((level & this.Level) != 0);
        }

        /// <summary>
        /// Calls the caller implemented Log if the trace level matches
        /// </summary>
        /// <param name="level">The log level to use</param>
        /// <param name="trace">The text to trace</param>
        public void Log(LogLevel level, string trace)
        {
            if (this.TraceForLevel(level))
            {
                this.Log(!this.inSession ? string.Format(CultureInfo.InvariantCulture, "{0}{1}", GetTraceHeader(level), trace) : string.Format(CultureInfo.InvariantCulture, "{0}:{1}", DateTime.Now.ToString("hh:mm:ss", CultureInfo.InvariantCulture), trace));
            }
        }

        /// <summary>
        /// Logs the trace if the level matches
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="param">The param.</param>
        public void Log(LogLevel level, string trace, object param)
        {
            if (this.TraceForLevel(level))
            {
                this.Log(level, string.Format(CultureInfo.InvariantCulture, trace, param));
            }
        }

        /// <summary>
        /// Logs the trace if the level matches
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="param1">The param1.</param>
        /// <param name="param2">The param2.</param>
        public void Log(LogLevel level, string trace, object param1, object param2)
        {
            if (this.TraceForLevel(level))
            {
                this.Log(level, string.Format(CultureInfo.InvariantCulture, trace, param1, param2));
            }
        }

        /// <summary>
        /// Logs the trace if the level matches
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="param1">The param1.</param>
        /// <param name="param2">The param2.</param>
        /// <param name="param3">The param3.</param>
        public void Log(LogLevel level, string trace, object param1, object param2, object param3)
        {
            if (this.TraceForLevel(level))
            {
                this.Log(level, string.Format(CultureInfo.InvariantCulture, trace, param1, param2, param3));
            }
        }

        /// <summary>
        /// Logs the trace if the level matches
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        /// <param name="parameters">The parameters.</param>
        public void Log(LogLevel level, string trace, object[] parameters)
        {
            if (this.TraceForLevel(level))
            {
                this.Log(level, string.Format(CultureInfo.InvariantCulture, trace, parameters));
            }
        }

        /// <summary>
        /// Logs a trace session start, subsequent traces will not have a trace header appended
        /// should be used only when a logging a set of traces in a short amount of time which are related
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        public void LogSessionStart(LogLevel level, string trace)
        {
            this.Log(level, "Trace Session Started");
            this.inSession = true;
            this.Log(level, trace);
        }

        /// <summary>
        /// Logs a trace session end, subsequent traces will have a trace header appended
        /// should be used only when a logging a set of traces in a short amount of time which are related
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        public void LogSessionEnd(LogLevel level, string trace)
        {
            // Calculate total time???
            this.Log(level, trace);
            this.inSession = false;
            this.Log(level, "Trace Session Ended");
        }

        /// <summary>
        /// Writes the trace directly to the specified file
        /// </summary>
        /// <param name="trace">The trace.</param>
        public void Log(string trace)
        {
            using (StreamWriter file = File.AppendText(this.LogFileName))
            {
                file.WriteLine(trace);
                file.Flush();
            }
        }

        #endregion

        #region protected members

        /// <summary>
        /// Returns the string for the trace header given the logging level
        /// </summary>
        /// <param name="level">The level.</param>
        /// <returns>the trace header</returns>
        protected static string GetTraceHeader(LogLevel level)
        {
            return string.Format(CultureInfo.InvariantCulture, "{0}\t{1}:\t", DateTime.Now.ToString("o", CultureInfo.InvariantCulture), Enum.GetName(typeof(LogLevel), level));
        }
        #endregion
    }
}
