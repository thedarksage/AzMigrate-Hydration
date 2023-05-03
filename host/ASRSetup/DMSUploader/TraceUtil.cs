// +--------------------------------------------------------------
//  <copyright file="TraceUtil.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  Description: Logger Utility.
//
//  History:     10-Aug-2016   Sopriya   Created
//
// ---------------------------------------------------------------
using System;
using System.Globalization;
using System.IO;
using Microsoft.Win32;

namespace Logger
{
    /// <summary>
    /// Values used to determine log level
    /// </summary>
    public enum LogLevel
    {
        /// <summary>
        /// Always trace Level; even console if flag is set.
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
        Info = 8,

        /// <summary>
        /// Headline tracer for demarcating different portions of code
        /// </summary>
        Headline = 10,

        /// <summary>
        /// Error trace for user; even on console if flag is set.
        /// </summary>
        ErrorToUser = 16
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
        private static readonly object LockObject = new object();

        /// <summary>
        /// Trace Utility instance used to do all the real work
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

        #endregion private members

        #region Settings

        /// <summary>
        /// Sets the logging level for all associated loggers
        /// </summary>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1044:PropertiesShouldNotBeWriteOnly", Justification = "We currently only write to this property")]
        public static LogLevel Level
        {
            set
            {
                lock (LockObject)
                {
                    if (traceWorker != null)
                    {
                        traceWorker.Level = value;
                    }
                }
            }
        }

        #endregion Settings

        #region public methods

        #region Initializers

        /// <summary>
        /// Initializes the specified trace level to set.
        /// </summary>
        /// <param name="traceLevelToSet">The trace level to set.</param>
        /// <param name="traceFileNameToSet">The trace file name to set.</param>
        /// <param name="consoleTraceRequired">Is console tracing required.</param>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1031:DoNotCatchGeneralExceptionTypes", Justification = "If we have an error here, we don't have anywhere to log it")]
        public static void Initialize(LogLevel traceLevelToSet, string traceFileNameToSet, bool consoleTraceRequired = false)
        {
            lock (LockObject)
            {
                traceLevel = traceLevelToSet;
                traceFileName = traceFileNameToSet;

                // See if there is an overriding level in the registry
                RegistryKey baseSetupTraceKey = Registry.LocalMachine.OpenSubKey("Software\\Microsoft\\SystemCenter\\Setup\\Trace");
                if (baseSetupTraceKey != null)
                {
                    RegistryKey applicationSetupTraceKey = baseSetupTraceKey.OpenSubKey(
                        System.Reflection.Assembly.GetExecutingAssembly().GetName().Name);

                    // If there is an Application Setup Trace Key, use it
                    if (applicationSetupTraceKey != null)
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

                if (String.IsNullOrEmpty(traceFileName))
                {
                    throw new ArgumentException("Trc.Initialize: Invalid argument.  traceFileNameToSet is not set to a FQFN");
                }

                traceWorker = new CommonLogger(traceLevel, traceFileName, consoleTraceRequired);
            }
        }

        #endregion Initializers

        #region Trace methods

        /// <summary>
        /// Logs trace to all loggers
        /// </summary>
        /// <param name="level">The level.</param>
        /// <param name="trace">The trace.</param>
        public static void Log(LogLevel level, string trace)
        {
            if (traceWorker != null)
            {
                traceWorker.Log(level, trace);
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
            lock (LockObject)
            {
                Trc.Log(
                    logLevel,
                    string.Format(
                    "{0}: Threw Exception.Type: {1}, Exception.Message: {2}",
                    customMessage,
                    exception.GetType().ToString(),
                    exception.Message));

                Trc.Log(logLevel, string.Format("StackTrace:{0}", exception.StackTrace));

                Exception innerException = exception.InnerException;
                while (innerException != null)
                {
                    Trc.Log(
                        logLevel,
                        string.Format(
                        "InnerException.Type: {1}, InnerException.Message: {2}",
                        customMessage,
                        innerException.GetType().ToString(),
                        innerException.Message));

                    Trc.Log(logLevel, string.Format("InnerException.StackTrace:{0}", innerException.StackTrace));

                    innerException = innerException.InnerException;
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
            lock (LockObject)
            {
                if (traceWorker != null)
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
            lock (LockObject)
            {
                if (traceWorker != null)
                {
                    traceWorker.LogSessionEnd(level, trace);
                }
            }
        }

        #endregion Trace methods

        #endregion public methods
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

        /// <summary>
        /// Is console tracing required flag.
        /// </summary>
        private bool consoleTraceRequired;

        /// <summary>
        /// Filestream Writer
        /// </summary>
        private StreamWriter streamWriter;

        /// <summary>
        /// Write Lock
        /// </summary>
        private Object writeLock;

        #region Public members

        /// <summary>
        /// Initializes a new instance of the CommonLogger class.
        /// </summary>
        /// <param name="traceLevel">The trace level.</param>
        /// <param name="logFileName">Name of the log file.</param>
        /// <param name="consoleTraceRequired">Is console tracing required.</param>
        public CommonLogger(LogLevel traceLevel, string logFileName, bool consoleTraceRequired)
        {
            // Call this to read the reader specific settings
            this.Level = traceLevel;
            this.LogFileName = logFileName;
            this.LogSessionStart(LogLevel.Info, String.Empty);
            this.consoleTraceRequired = consoleTraceRequired;
        }

        /// <summary>
        /// Finalizes an instance of the CommonLogger class.
        /// <see cref="CommonLogger"/> is reclaimed by garbage collection.
        /// </summary>
        ~CommonLogger()
        {
            ////this.LogSessionEnd(LogLevel.Info, String.Empty);
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
        /// Gets or sets Accessors for trace level
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
                this.Log(!this.inSession ? string.Format(CultureInfo.InvariantCulture, "{0}{1}", GetTraceHeader(level), trace) : string.Format(CultureInfo.InvariantCulture, "{0}:{1}", DateTime.Now.ToString(CultureInfo.InvariantCulture), trace));
            }

            if ((level == LogLevel.Always
                || level == LogLevel.ErrorToUser
                || level == LogLevel.Warn)
                && this.consoleTraceRequired)
            {
                ConsoleColor foregroundColor = Console.ForegroundColor;
                ConsoleColor displayColor = foregroundColor;
                if (level == LogLevel.ErrorToUser)
                {
                    displayColor = ConsoleColor.Red;
                }
                else if (level == LogLevel.Warn)
                {
                    displayColor = ConsoleColor.Yellow;
                }

                Console.ForegroundColor = displayColor;
                Console.WriteLine(trace);
                Console.ForegroundColor = foregroundColor;
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
            this.streamWriter = File.AppendText(this.LogFileName);
            this.writeLock = new object();
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
            this.streamWriter.Close();
            this.streamWriter = File.AppendText(this.LogFileName);
            //// Calculate total time???
            this.Log(level, trace);
            this.inSession = false;
            this.Log(level, "Trace Session Ended");
            this.streamWriter.Close();
        }

        /// <summary>
        /// Writes the trace directly to the specified file
        /// </summary>
        /// <param name="trace">The trace.</param>
        public void Log(string trace)
        {
            lock (this.writeLock)
            {
                this.streamWriter.WriteLine(trace);
                this.streamWriter.Flush();
            }
        }

        #endregion Public members

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

        #endregion protected members
    }
}