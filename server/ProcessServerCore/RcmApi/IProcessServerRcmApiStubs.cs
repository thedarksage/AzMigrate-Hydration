using RcmContract;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi
{
    /// <summary>
    /// Context for requests executed by Rcm and its components
    /// </summary>
    public class RcmOperationContext
    {
        /// <summary>
        ///  Generates an RCM operation context
        /// </summary>
        public RcmOperationContext()
            : this(null, null)
        {
        }

        /// <summary>
        /// Sets the RcmOperationContext
        /// </summary>
        /// <param name="activityId">Activity Id</param>
        /// <param name="clientRequestId">Client request Id</param>
        public RcmOperationContext(string activityId, string clientRequestId)
        {
            if (string.IsNullOrWhiteSpace(activityId) &&
                string.IsNullOrWhiteSpace(ClientRequestId))
            {
                // Generate activity id and client request id, if not passed

                this.ActivityId = Guid.NewGuid().ToString();
                this.ClientRequestId = Guid.NewGuid().ToString();
            }
            else
            {
                this.ActivityId = activityId;
                this.ClientRequestId = clientRequestId;
            }
        }

        /// <summary>
        /// Convert object to string
        /// </summary>
        /// <returns>String representation of the object</returns>
        public override string ToString()
        {
            return $"ActivityId : {ActivityId}, ClientRequestId : {ClientRequestId}";
        }

        /// <summary>
        /// Activity Id
        /// </summary>
        public string ActivityId { get; }

        /// <summary>
        /// Client request Id
        /// </summary>
        public string ClientRequestId { get; }
    }

    public interface IProcessServerRcmApiStubs : IDisposable
    {
        /// <summary>
        /// Asynchronous API to register process server component with RCM service.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="input">
        /// Information about the process server being registered.
        /// </param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        Task RegisterProcessServerAsync(
            RcmOperationContext context,
            RegisterProcessServerInput input,
            CancellationToken cancellationToken);

        /// <summary>
        /// Synchronous API to modify process server component registered with RCM service.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="input">
        /// Details to be modified and information about the process server being modified.
        /// </param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        Task ModifyProcessServerAsync(
            RcmOperationContext context,
            ModifyProcessServerInput input,
            CancellationToken cancellationToken);

        /// <summary>
        /// Asynchronous API to unregister process server component with RCM service.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="processServerId">Identifier of the process server to unregister.
        /// </param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        Task UnregisterProcessServerAsync(
            RcmOperationContext context,
            string processServerId,
            CancellationToken cancellationToken);

        /// <summary>
        /// Asynchronous API to validate connection to the service.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="message">Input for the API.</param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        Task TestConnectionAsync(
            RcmOperationContext context,
            string message,
            CancellationToken cancellationToken);

        /// <summary>
        /// Asynchronous API to fetch replication settings.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="input">Information about the settings already cached
        /// with the process server.</param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        /// <returns>Replication settings for the process server.</returns>
        Task<ProcessServerSettings> GetProcessServerSettingsAsync(
            RcmOperationContext context,
            GetProcessServerSettingsInput input,
            CancellationToken cancellationToken);

        /// <summary>
        /// Asynchronous API to update job status from agent to RCM.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="job">The job status from agent.</param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        Task UpdateAgentJobStatusAsync(
            RcmOperationContext context,
            RcmJob job,
            CancellationToken cancellationToken);

        /// <summary>
        /// Asynchronous API to post monitoring messages from agent to RCM.
        /// </summary>
        /// <param name="context">Context of the Rcm operation.</param>
        /// <param name="input">Message input.</param>
        /// <param name="cancellationToken">
        /// Token to request cancellation of the request.
        /// </param>
        Task SendMonitoringMessageAsync(
            RcmOperationContext context,
            SendMonitoringMessageInput input,
            CancellationToken cancellationToken);

        void Close();
    }
}
