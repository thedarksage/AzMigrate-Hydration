namespace MarsAgent.ErrorHandling
{
    /// <summary>
    /// Defines enumeration used in the service.
    /// </summary>
    public class MarsAgentEnum
    {
        /// <summary>
        /// To specify error severity.
        /// </summary>
        /// <remarks>
        /// The ToString() value of the following is sent in the contracts so any rename
        /// would be a breaking change.
        /// </remarks>
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
            Information
        }

        /// <summary>
        /// Error category.
        /// </summary>
        /// <remarks>
        /// The ToString() value of the following is sent in the contracts so any rename
        /// would be a breaking change.
        /// </remarks>
        public enum ErrorTagCategory
        {
            /// <summary>
            /// Client error.
            /// </summary>
            ClientError,

            /// <summary>
            /// System error.
            /// </summary>
            SystemError
        }

        /// <summary>
        /// Enumeration to describe the health issue codes.
        /// </summary>
        public enum HealthIssue
        {
            /// <summary>
            /// Represents Network/Proxy issues preventing communication to Azure Services.
            /// </summary>
            MarsServiceCommunicationError,

            /// <summary>
            /// Communication issue with storage.
            /// </summary>
            MarsStorageCommunicationError,

            /// <summary>
            /// RPC server is in disconnected state.
            /// </summary>
            MarsRPCServerDisconnected,

            /// <summary>
            /// Registration issue.
            /// </summary>
            MarsServiceAuthenticationError,

            /// <summary>
            /// Storage authentication issue.
            /// </summary>
            MarsStorageAuthenticationError,

            /// <summary>
            /// Subscription expired, renewal should fix the issue.
            /// </summary>
            MarsSubscriptionExpired,

            /// <summary>
            /// Mars initialization failure.
            /// </summary>
            MarsInitializationFailed,

            /// <summary>
            /// Registration failure
            /// </summary>
            MarsRegistrationFailed,

            /// <summary>
            /// Represents unknown error.
            /// </summary>
            MarsGenericError
        }
    }
}