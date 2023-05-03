//---------------------------------------------------------------
//  <copyright file="Diagnostics.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Tool used for periodic discovery triggered by Process Server in InMage.
//  </summary>
//
//  History:     30-Oct-2015   Prmyaka   Created
// -----------------------------------------------------------------------

using System;
using System.Diagnostics;
using VMware.Client;

namespace InMageDiscovery
{
    /// <summary>
    /// Diagnostics logging.
    /// </summary>
    public static class Diagnostics
    {
        /// <summary>
        /// Debug assert delegate.
        /// </summary>
        /// <param name="callInfo">Information about the caller asserting the condition.
        /// </param>
        /// <param name="condition">The condition to be asserted.</param>
        /// <param name="msg">The message to be traced.</param>
        /// <param name="args">Arguments for the trace message.</param>
        public static void DebugAssert(
            this VimCallInfo callInfo,
            bool condition,
            string msg = "",
            params object[] args)
        {
            Assert(callInfo, condition, msg, args);
        }

        /// <summary>
        /// Assert delegate method.
        /// </summary>
        /// <param name="callInfo">Information about the caller asserting the condition.
        /// </param>
        /// <param name="condition">The condition to be asserted.</param>
        /// <param name="msg">The message to be traced.</param>
        /// <param name="args">Arguments for the trace message.</param>
        public static void Assert(
            this VimCallInfo callInfo,
            bool condition,
            string msg = "",
            params object[] args)
        {
            if (!condition)
            {
                string assertMessage =
                    string.Format("Assertion Failure!\n{0}", string.Format(msg, args));
                DiscoveryLogs.LogWriter.WriteLine(assertMessage);
                throw new Exception(assertMessage);
            }
        }

        /// <summary>
        /// Trace delegate method.
        /// </summary>
        /// <param name="callInfo">The call site.</param>
        /// <param name="type">The trace type.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void Trace(
            this VimCallInfo callInfo,
            TraceEventType type,
            string msg,
            params object[] args)
        {
            TraceInternal(callInfo, type, msg, args);
        }

        /// <summary>
        /// Trace logging.
        /// </summary>
        /// <param name="callInfo">The call site.</param>
        /// <param name="type">The trace type.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceInternal(
            this VimCallInfo callInfo,
            TraceEventType type,
            string msg,
            params object[] args)
        {
            try
            {
                string source =
                    string.Format(
                        "[{0}:{1} {2}] ",
                        callInfo.FilePath,
                        callInfo.LineNumber,
                        callInfo.MethodName);
                string message = string.Format(msg, args);
                string timeStamp = DateTime.Now.ToString();
                string logMessage;

                switch (type)
                {
                    case TraceEventType.Critical:
                        logMessage = timeStamp + ": CRITICAL :: " + message;
                        DiscoveryLogs.LogWriter.WriteLine(logMessage);
                        break;

                    case TraceEventType.Error:
                        logMessage = timeStamp + ": ERROR :: " + message;
                        DiscoveryLogs.LogWriter.WriteLine(logMessage);
                        break;

                    case TraceEventType.Warning:
                        logMessage = timeStamp + ": WARNING :: " + message;
                        DiscoveryLogs.LogWriter.WriteLine(logMessage);
                        break;

                    case TraceEventType.Information:
                        logMessage = timeStamp + ": INFO :: " + message;
                        DiscoveryLogs.LogWriter.WriteLine(logMessage);
                        break;

                    case TraceEventType.Verbose:
                        logMessage = timeStamp + ": DEBUG :: " + message;
                        DiscoveryLogs.LogWriter.WriteLine(logMessage);
                        break;

                    default:
                        logMessage = timeStamp + ": EXCEPTION :: " + message
                            + " at " + source;
                        DiscoveryLogs.LogWriter.WriteLine(logMessage);
                        DiscoveryLogs.LogWriter.WriteLine(
                            string.Format("Unexpected error of type {0}", type));
                        break;
                }
            }
            catch (Exception e)
            {
                // If a debugger is attached, break in.
                if (Debugger.IsAttached)
                {
                    // This is a debugging aid when we chose to ignore exceptions
                    // while logging but since a debugger is attached it would be a
                    // good opportunity to see why this happened.
                    // To ignore this and let the service continue hit F5.
                    Debugger.Break();
                }

                try
                {
                    DiscoveryLogs.LogWriter.WriteLine(
                        string.Format(
                            "Ignoring exception {0} hit during tracing of message {1}.",
                            e.ToString(),
                            msg));
                }
                catch (Exception)
                {
                }
            }
        }
    }
}
