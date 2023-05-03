//---------------------------------------------------------------
//  <copyright file="VimTracing.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     05-May-2015   GSinha     Created
//----------------------------------------------------------------

using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Text;

namespace VMware.Client
{
    /// <summary>
    /// Holds properties that are initialized once.
    /// </summary>
    public class VimTracing
    {
        /// <summary>
        /// Traces the specified message.
        /// </summary>
        /// <param name="callerInfo">Information about the caller tracing the message.</param>
        /// <param name="type">The type of event that has caused the trace.</param>
        /// <param name="msg">The message to be traced.</param>
        /// <param name="args">Arguments for the trace message.</param>
        public delegate void TraceDelegate(
            VimCallInfo callerInfo,
            TraceEventType type,
            string msg,
            params object[] args);

        /// <summary>
        /// Asserts that the specified condition has been met. If not, traces the specified message
        /// and raises an exception.
        /// </summary>
        /// <param name="callerInfo">Information about the caller asserting the condition.</param>
        /// <param name="condition">The condition to be asserted.</param>
        /// <param name="msg">The message to be traced.</param>
        /// <param name="args">Arguments for the trace message.</param>
        public delegate void AssertDelegate(
            VimCallInfo callerInfo,
            bool condition,
            string msg = StyleCop.EmptyString,
            params object[] args);

        /// <summary>
        /// In debug builds, asserts that the specified condition has been met. If not, traces the
        /// specified message and raises an exception.
        /// </summary>
        /// <param name="callerInfo">Information about the caller asserting the condition.</param>
        /// <param name="condition">The condition to be asserted.</param>
        /// <param name="msg">The message to be traced.</param>
        /// <param name="args">Arguments for the trace message.</param>
        public delegate void DebugAssertDelegate(
            VimCallInfo callerInfo,
            bool condition,
            string msg = StyleCop.EmptyString,
            params object[] args);

        /// <summary>
        /// Gets or sets the callback to invoke for tracing.
        /// </summary>
        internal static TraceDelegate TraceCallback { get; set; }

        /// <summary>
        /// Gets or sets the callback to invoke for asserting.
        /// </summary>
        internal static AssertDelegate AssertCallback { get; set; }

        /// <summary>
        /// Gets or sets the callback to invoke for debug asserting.
        /// </summary>
        internal static DebugAssertDelegate DebugAssertCallback { get; set; }

        /// <summary>
        /// One time initialization for the logging callbacks to use for tracing.
        /// </summary>
        /// <param name="traceCallback">The tracing callback.</param>
        /// <param name="assertCallback">The assert callback.</param>
        /// <param name="debugAssertCallback">The debug assert callback.</param>
        public static void Initialize(
            TraceDelegate traceCallback,
            AssertDelegate assertCallback,
            DebugAssertDelegate debugAssertCallback)
        {
            TraceCallback = traceCallback;
            AssertCallback = assertCallback;
            DebugAssertCallback = debugAssertCallback;
        }
    }

    /// <summary>
    /// Holds information such as file name, line number, and method name of the caller.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.MaintainabilityRules",
        "SA1402:FileMayOnlyContainASingleClass",
        Justification = "Keeping an tracing related classes together.")]
    public class VimCallInfo
    {
        /// <summary>
        /// Gets or sets the component name.
        /// </summary>
        public string ComponentName { get; set; }

        /// <summary>
        /// Gets or sets the file name.
        /// </summary>
        public string FileName { get; set; }

        /// <summary>
        /// Gets or sets the file path.
        /// </summary>
        public string FilePath { get; set; }

        /// <summary>
        /// Gets or sets the line number.
        /// </summary>
        public int LineNumber { get; set; }

        /// <summary>
        /// Gets or sets the method name.
        /// </summary>
        public string MethodName { get; set; }

        /// <summary>
        /// Returns a formatted string.
        /// </summary>
        /// <returns>A formatted string.</returns>
        public override string ToString()
        {
            // Creates a string of the form: "C:\foo\Tracer.cs:36 LogInfo".
            StringBuilder sb = new StringBuilder();
            sb.Append(this.FilePath + ":");
            sb.Append(this.LineNumber + " ");
            sb.Append(this.MethodName);
            return sb.ToString();
        }
    }

    /// <summary>
    /// Style cop issues.
    /// </summary>
    internal class StyleCop
    {
        /// <summary>
        /// For StyleCop. Doing "" as optional argument value makes StyleCop error out with:
        /// Error 7859 SA0001: An exception occurred while parsing the file:
        /// System.ArgumentNullException, element
        /// Parameter name: Must not be null
        /// </summary>
        public const string EmptyString = "";
    }
}

