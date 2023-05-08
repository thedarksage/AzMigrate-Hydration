using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using ASRSetupFramework;
using Microsoft.Deployment.WindowsInstaller;

namespace UnifiedAgentCustomActions
{
    /// <summary>
    /// Extension routines.
    /// </summary>
    public static class ExtensionMethods
    {
        /// <summary>
        /// Logs message into session logs.
        /// </summary>
        /// <param name="session">Current MSIEXEC session.</param>
        /// <param name="logLevel">Log level for message.</param>
        /// <param name="message">Format of the message to be logged.</param>
        /// <param name="param">Format params.</param>
        public static void Log(
            this Session session,
            LogLevel logLevel, 
            string messageFormat, 
            params object[] param)
        {
            LogInternal(
                GetCallerMethodName(), 
                session, 
                logLevel, 
                string.Format(messageFormat, param));
        }

        /// <summary>
        /// Logs message into session logs.
        /// </summary>
        /// <param name="session">Current MSIEXEC session.</param>
        /// <param name="logLevel">Log level for message.</param>
        /// <param name="message">Format of the message to be logged.</param>
        public static void Log(
            this Session session,
            LogLevel logLevel,
            string message)
        {
            LogInternal(GetCallerMethodName(), session, logLevel, message);
        }

        /// <summary>
        /// Gets the name of the method that called this routine.
        /// </summary>
        private static string GetCallerMethodName()
        {
            try
            {
                StackTrace trace = new StackTrace();
                int caller = 2;
                StackFrame frame = trace.GetFrame(caller);
                return frame.GetMethod().Name;
            }
            catch
            {
                return string.Empty;
            }
        }

        /// <summary>
        /// Logs message into session logs.
        /// </summary>
        /// <param name="callerName">Calling method name.</param>
        /// <param name="session">Current MSIEXEC session.</param>
        /// <param name="logLevel">Log level for message.</param>
        /// <param name="message">Message to be logged.</param>
        private static void LogInternal(
            string callerName,
            Session session,
            LogLevel logLevel, 
            string message)
        {
            session.Log("[{0}][{1}]{2}", callerName, logLevel.ToString(), message);
        }
    }
}
