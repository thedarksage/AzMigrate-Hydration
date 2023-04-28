using System;
using System.Collections.Generic;

namespace MarsAgent.CBEngine.Constants
{
    /// <summary>
    /// CBEngine logger enum used.
    /// </summary>
    public class LoggerEnum
    {
        /// <summary>
        /// CBEngine logger severity.
        /// </summary>
        public enum ErrorSeverity
        {
            /// <summary>
            /// Severity level is error.
            /// </summary>
            Error,

            /// <summary>
            /// Severity level is warning.
            /// </summary>
            Warning,

            /// <summary>
            /// Severity level is information.
            /// </summary>
            Information,

            /// <summary>
            /// Severity level is verbose.
            /// </summary>
            Verbose

        }

        /// <summary>
        /// CBEngine log level to ErrorSeverity mapping.
        /// </summary>
        public static Dictionary<String, ErrorSeverity> ErrorSeverityMapping = new Dictionary<String, ErrorSeverity>
        {
            { "NORMAL", ErrorSeverity.Information},
            { "VERBOSE", ErrorSeverity.Verbose},
            { "WARNING", ErrorSeverity.Warning},
            { "FATAL", ErrorSeverity.Error}
        };
    }
}
