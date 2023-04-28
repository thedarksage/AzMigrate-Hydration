using MarsAgent.ErrorHandling;
using System;
using System.Collections.Generic;

namespace MarsAgent.Monitoring
{
    /// <summary>
    /// Defines constants for MARS agent API handler.
    /// </summary>
    public static class MarsAgentErrorConstants
    {
        /// <summary>
        /// Gets the map containing error labels for the corresponding error codes.
        /// </summary>
        public static IReadOnlyDictionary<uint, string> ErrorCodeToErrorLabelMap
        {
            get
            {
                return new Dictionary<uint, string>
                {
                    [0x80790101] = "MT_E_SERVICE_COMMUNICATION_ERROR",
                    [0x80790102] = "MT_E_STORAGE_COMMUNICATION_ERROR",
                    [0x80790104] = "MT_E_RPC_DISCONNECTED",
                    [0x80790300] = "MT_E_SERVICE_AUTHENTICATION_ERROR",
                    [0x80790301] = "MT_E_STORAGE_AUTHENTICATION_ERROR",
                    [0x80790303] = "MT_E_SUBSCRIPTION_EXPIRED",
                    [0x80790304] = "MT_E_INITIALIZATION_FAILED",
                    [0x80790305] = "MT_E_REGISTRATION_FAILED",
                    [0x80790308] = "MT_E_GENERIC_ERROR"
                };
            }
        }

        /// <summary>
        /// Gets the map containing component health issue codes corresponding to the
        /// error codes received from MARS.
        /// </summary>
        public static IReadOnlyDictionary<uint, string> ErrorCodeToComponentHealthIssueCodeMap
        {
            get
            {
                return new Dictionary<uint, string>
                {
                    [0x80790101] = nameof(MarsAgentEnum.HealthIssue.MarsServiceCommunicationError),
                    [0x80790102] = nameof(MarsAgentEnum.HealthIssue.MarsStorageCommunicationError),
                    [0x80790104] = nameof(MarsAgentEnum.HealthIssue.MarsRPCServerDisconnected),
                    [0x80790300] = nameof(MarsAgentEnum.HealthIssue.MarsServiceAuthenticationError),
                    [0x80790301] = nameof(MarsAgentEnum.HealthIssue.MarsStorageAuthenticationError),
                    [0x80790303] = nameof(MarsAgentEnum.HealthIssue.MarsSubscriptionExpired),
                    [0x80790304] = nameof(MarsAgentEnum.HealthIssue.MarsInitializationFailed),
                    [0x80790305] = nameof(MarsAgentEnum.HealthIssue.MarsRegistrationFailed),
                    [0x80790308] = nameof(MarsAgentEnum.HealthIssue.MarsGenericError)
                };
            }
        }
    }
}
