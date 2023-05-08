//---------------------------------------------------------------
//  <copyright file="VimDiagnostics.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     05-May-2015   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;

namespace VMware.Client
{
    /// <summary>
    /// Holds various tracing/diagnostics methods.
    /// </summary>
    internal static class VimDiagnostics
    {
        /// <summary>
        /// For StyleCop. Doing "" as optional argument value makes StyleCop error out with:
        /// Error 7859 SA0001: An exception occurred while parsing the file:
        /// System.ArgumentNullException, element
        /// Parameter name: Must not be null
        /// </summary>
        private const string EmptyString = "";

        /// <summary>
        /// Returns the call site information.
        /// </summary>
        /// <param name="sourceName">The source file name.</param>
        /// <param name="sourceFilePath">The source file path.</param>
        /// <param name="sourceLineNumber">The caller line number.</param>
        /// <returns>Call site information.</returns>
        public static VimCallInfo Tracer(
            [CallerMemberName] string sourceName = EmptyString,
            [CallerFilePath] string sourceFilePath = EmptyString,
            [CallerLineNumber] int sourceLineNumber = 0)
        {
            var fileName = Path.GetFileName(sourceFilePath);
            return new VimCallInfo
            {
                MethodName = sourceName,
                FileName = fileName,
                FilePath = sourceFilePath,
                LineNumber = sourceLineNumber,
                ComponentName = fileName,
            };
        }

        /// <summary>
        /// Traces out an exception.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="e">The exception.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceException(
            this VimCallInfo caller,
            Exception e,
            string msg,
            params object[] args)
        {
            string exceptionDetails = string.Format("Exception details:\n{0}", e.ToString());
            caller.TraceError(msg + exceptionDetails, args);
        }

        /// <summary>
        /// Traces out a message.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceCritical(
            this VimCallInfo caller,
            string msg,
            params object[] args)
        {
            caller.Trace(TraceEventType.Critical, msg, args);
        }

        /// <summary>
        /// Traces out a message.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceError(
            this VimCallInfo caller,
            string msg,
            params object[] args)
        {
            caller.Trace(TraceEventType.Error, msg, args);
        }

        /// <summary>
        /// Traces out a message.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceWarning(
            this VimCallInfo caller,
            string msg,
            params object[] args)
        {
            caller.Trace(TraceEventType.Warning, msg, args);
        }

        /// <summary>
        /// Traces out a message.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceInformation(
            this VimCallInfo caller,
            string msg,
            params object[] args)
        {
            caller.Trace(TraceEventType.Information, msg, args);
        }

        /// <summary>
        /// Traces out a message.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void TraceVerbose(
            this VimCallInfo caller,
            string msg,
            params object[] args)
        {
            caller.Trace(TraceEventType.Verbose, msg, args);
        }

        /// <summary>
        /// Asserts a given condition.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="condition">The condition to test.</param>
        /// <param name="msg">The formatted message to trace if condition fails.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void Assert(
            this VimCallInfo caller,
            bool condition,
            string msg = EmptyString,
            params object[] args)
        {
            if (VimTracing.AssertCallback != null)
            {
                VimTracing.AssertCallback(caller, condition, msg, args);
            }
        }

        /// <summary>
        /// Asserts a given condition.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="condition">The condition to test.</param>
        /// <param name="msg">The formatted message to trace if condition fails.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        [Conditional("DEBUG")]
        public static void DebugAssert(
            this VimCallInfo caller,
            bool condition,
            string msg = EmptyString,
            params object[] args)
        {
            if (VimTracing.DebugAssertCallback != null)
            {
                VimTracing.DebugAssertCallback(caller, condition, msg, args);
            }
        }

        /// <summary>
        /// Invokes any tracing callback, if specified.
        /// </summary>
        /// <param name="caller">The call site.</param>
        /// <param name="type">The trace type.</param>
        /// <param name="msg">The formatted message.</param>
        /// <param name="args">Any arguments for the formatted message.</param>
        public static void Trace(
            this VimCallInfo caller,
            TraceEventType type,
            string msg,
            params object[] args)
        {
            try
            {
                if (VimTracing.TraceCallback != null)
                {
                    VimTracing.TraceCallback(caller, type, msg, args);
                }
            }
            catch (Exception e)
            {
                // If a debugger is attached, break in.
                if (Debugger.IsAttached)
                {
                    // This is a debugging aid when we chose to ignore exceptions while logging but
                    // since a debugger is attached it would be a good opportunity to see why this
                    // happened. To ignore this and let the service continue hit F5.
                    Debugger.Break();
                }

                try
                {
                    Console.WriteLine(
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
