namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi
{
    internal static class RcmHttpHeaders
    {
        public static class Standard
        {
            /// <summary>
            /// String representing the accept language in http request header.
            /// </summary>
            public const string AcceptLanguage = "Accept-Language";

            /// <summary>
            /// String representing the authorization in http request header.
            /// </summary>
            public const string Authorization = "Authorization";

            /// <summary>
            /// String representing the integrity key token in http request header.
            /// </summary>
            public const string ChannelIntegrityKeyToken = "Agent-Authentication";

            /// <summary>
            /// String representing retry after value in http request header.
            /// </summary>
            public const string RetryAfter = "Retry-After";

            /// <summary>
            /// String representing the location in http request header.
            /// </summary>
            public const string Location = "Location";

            /// <summary>
            /// String representing the azure async operation in http request header.
            /// </summary>
            public const string AzureAsyncOperation = "Azure-AsyncOperation";
        }

        public static class Custom
        {
            /// <summary>
            /// String representing the client request id in http request header.
            /// </summary>
            public const string MSClientRequestId = "x-ms-client-request-id";

            /// <summary>
            /// String representing the activity id in http request header.
            /// </summary>
            public const string MSActivityId = "x-ms-correlation-request-id";

            /// <summary>
            /// String representing the principal id in http request header.
            /// </summary>
            public const string MSPrincipalId = "x-ms-principal-id";

            /// <summary>
            /// String representing the principal live id in http request header.
            /// </summary>
            public const string MSPrincipalLiveId = "x-ms-principal-liveid";

            /// <summary>
            /// String representing the request id in http request header.
            /// </summary>
            public const string MSRequestId = "x-ms-request-id";

            /// <summary>
            /// String representing the SRS activity id in http request header.
            /// </summary>
            public const string MSSrsActivityId = "x-ms-srs-activity-id";

            /// <summary>
            /// String representing the container id in http request header.
            /// </summary>
            public const string MSContainerId = "x-ms-container-id";

            /// <summary>
            /// String representing the resource id in http request header.
            /// </summary>
            public const string MSResourceId = "x-ms-resource-id";

            /// <summary>
            /// String representing the resource location in http request header.
            /// </summary>
            public const string MSResourceLocation = "x-ms-resource-location";

            /// <summary>
            /// String representing the subscription ID of the user in http request header.
            /// </summary>
            public const string MSSubscriptionId = "x-ms-subscription-id";

            /// <summary>
            /// String representing the component id in http request header.
            /// </summary>
            public const string MSAgentComponentId = "x-ms-agent-component-id";

            /// <summary>
            /// String representing the machine id where the agent component resides 
            /// in http request header.
            /// </summary>
            public const string MSAgentMachineId = "x-ms-agent-machine-id";

            /// <summary>
            /// String representing the disk id on agent machine in http request header.
            /// </summary>
            public const string MSAgentDiskId = "x-ms-agent-disk-id";

            /// <summary>
            /// String representing the id for the agent proxy in http request header.
            /// </summary>
            public const string MSAgentProxyId = "x-ms-agent-proxy-id";

            /// <summary>
            /// String representing the authentication type in http request header.
            /// </summary>
            public const string MSAuthType = "x-ms-authtype";

            /// <summary>
            /// String representing the role in http request header.
            /// </summary>
            public const string MSRole = "x-ms-role";
        }
    }
}
