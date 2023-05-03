using LoggerInterface;
using MarsAgent.LoggerInterface;
using MarsAgent.Service;
using MTReplicationProviderLib;
using System;
using System.Threading;

namespace MarsAgent.Monitoring
{
    /// <summary>
    /// Implements MARS agent APIs.
    /// </summary>
    public class MarsAgentApi
    {
        /// <summary>
        /// MARS endpoint COM provider.
        /// </summary>
        private IMTReplicationEndpointProvider provider;

        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentApi"/> class.
        /// </summary>
        public MarsAgentApi()
        {
            this.provider = this.GetEndpointProvider();
        }

        /// <summary>
        /// Check for any configuration issues.
        /// </summary>
        public void CheckAgentHealth()
        {
            try
            {
                this.CheckAgentHealthInternal();
            }
            catch (MarsAgentException marsException)
            {
                throw new MarsAgentClientException(marsException);
            }

            Logger.Instance.LogInfo(
                CallInfo.Site(),
                "Successfully verified MARS endpoint health.");
        }

        /// <summary>
        /// Check for any configuration issues.
        /// </summary>
        private void CheckAgentHealthInternal()
        {
            int retryCount = 0;
            bool operationCompleted = false;
            TimeSpan delay = TimeSpan.FromSeconds(1);

            do
            {
                try
                {
                    this.provider.CheckAgentHealth();
                    operationCompleted = true;
                }
                catch (Exception e)
                {
                    retryCount++;

                    MarsAgentException marsException = new MarsAgentException(e);

                    if (marsException.IsRpcServerUnavailable())
                    {
                        // Re-initialize
                        this.provider = this.GetEndpointProvider();
                    }

                    if (marsException.IsRetriable() &&
                        retryCount < ServiceTunables.Instance.MaxRetryCountForInvokingMarsApi)
                    {
                        Logger.Instance.LogWarning(
                            CallInfo.Site(),
                            $"Retriable error occurred while trying to check MARS agent health " +
                            $"endpoint; retrying in {delay.TotalSeconds} seconds.\n" +
                            $"Error: \n{marsException.ToString()}");

                        Thread.Sleep(delay);
                    }
                    else
                    {
                        Logger.Instance.LogError(
                            CallInfo.Site(),
                            $"Error occurred while trying to check MARS agent health." +
                            $"\nError: \n{marsException.ToString()}");

                        throw marsException;
                    }
                }
            }
            while (!operationCompleted);
        }

        /// <summary>
        /// Gets the MARS agent endpoint provider.
        /// </summary>
        /// <returns>MARS agent endpoint provider.</returns>
        private IMTReplicationEndpointProvider GetEndpointProvider()
        {
            IMTReplicationEndpointProvider provider = null;

            int retryCount = 0;
            bool initialized = false;

            do
            {
                try
                {
                    provider = new MTReplicationEndpointProviderClass();
                    initialized = true;
                }
                catch (Exception e)
                {
                    retryCount++;

                    if (retryCount < ServiceTunables.Instance.MaxRetryCountForInvokingMarsApi)
                    {
                        TimeSpan delay = TimeSpan.FromSeconds(1);
                        Logger.Instance.LogWarning(
                            CallInfo.Site(),
                            $"Error occurred while creating an instance of MT replication " +
                            $"endpoint provider: {e.Message}. Retrying in '{delay.TotalSeconds}'" +
                            $" seconds.");

                        Thread.Sleep(delay);
                    }
                    else
                    {
                        Logger.Instance.LogError(
                            CallInfo.Site(),
                            $"Error occurred while creating an instance of MT replication " +
                            $"endpoint provider: {e.Message}.");

                        throw new MarsAgentException(e);
                    }
                }
            }
            while (!initialized);

            return provider;
        }
    }
}
