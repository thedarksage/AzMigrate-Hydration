using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.Identity.Client;
using Newtonsoft.Json;
using RcmClientLib;
using RcmContract;
using System;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi
{
    internal class ProcessServerRcmApiStubsImpl : IProcessServerRcmApiStubs
    {
        private readonly AzureADSpn m_aadDetails;
        private readonly ProxySettings m_proxy;
        private readonly RestWrapper m_restWrapper;
        private IConfidentialClientApplication m_clientApp;
        private readonly MsalHttpClientFactory m_httpClient;
        private readonly string m_machineId;

        private static readonly JsonSerializerSettings s_rcmContractsJsonSerSettings
            = JsonUtils.GetStandardSerializerSettings(indent: false);

        private class MsalHttpClientFactory : IMsalHttpClientFactory
        {
            public HttpClient HttpClient { get; }

            public MsalHttpClientFactory(TimeSpan timeout, IWebProxy webProxy)
            {
                HttpClient = RestWrapper.CreateHttpClient(
                    serverUri: null,
                    timeout: timeout,
                    proxy: webProxy,
                    remoteCertValidationCallback: null);
            }

            public HttpClient GetHttpClient() => this.HttpClient;
        }

        public ProcessServerRcmApiStubsImpl(
            Uri rcmUri, AzureADSpn aadDetails, ProxySettings proxy, string machineId, bool disableRcmSslCertCheck)
        {
            if (rcmUri == null)
                throw new ArgumentNullException(nameof(rcmUri));

            if (aadDetails == null)
                throw new ArgumentNullException(nameof(aadDetails));

            if (!aadDetails.ContainsValidProperties())
                throw new ArgumentException($"{nameof(aadDetails)} passed contains invalid data");

            if (proxy != null && !proxy.ContainsValidProperties())
                throw new ArgumentException($"{nameof(proxy)} passed contains invalid data");

            if (string.IsNullOrWhiteSpace(machineId))
                throw new ArgumentNullException(nameof(machineId));

            m_aadDetails = aadDetails.DeepCopy();
            m_proxy = proxy?.DeepCopy();
            m_machineId = machineId;

            // TODO-SanKumar-1905: Give different timeouts for AAD client and the RCM client
            // TODO-SanKumar-1905: Move the timeout value to Settings
            TimeSpan timeout = TimeSpan.FromSeconds(180);

            WebProxy webProxy = null;

            // TODO-SanKumar-1904: Instead of checking here, the object construction must handle validation
            if (m_proxy != null &&
                !string.IsNullOrWhiteSpace(m_proxy.IpAddress?.ToString()))
            {
                var uriBuilder = new UriBuilder(m_proxy.IpAddress)
                {
                    Port = m_proxy.PortNumber
                };

                var bypassList = m_proxy.BypassList?.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries);

                ICredentials credential = null;
                if (!string.IsNullOrEmpty(m_proxy.UserName))
                {
                    credential = new NetworkCredential(
                        m_proxy.UserName,
                        DataProtectionApi.UnprotectData(m_proxy.Password, Encoding.Unicode));
                }

                webProxy = new WebProxy(
                    uriBuilder.Uri, m_proxy.BypassProxyOnLocal,
                    bypassList.Length == 0 ? null : bypassList, credential);
            }

            m_httpClient = new MsalHttpClientFactory(timeout, webProxy);
            // When null, standard .Net SSL cert validation implementation is used.
            RemoteCertificateValidationCallback remCertValCallback = null;
            if (disableRcmSslCertCheck)
            {
                // TODO-SanKumar-1907: Since this is only needed for in-house
                // testing, should we #if it under DEBUG?
                remCertValCallback = (_, __, ___, ____) => true;
            }

            m_restWrapper = new RestWrapper(
                serverUri: rcmUri, timeout: timeout, proxy: webProxy, remoteCertValidationCallback: remCertValCallback);
        }

        private Task<HttpContent> RequestBuilderAsync(object input, CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();

            string jsonSerializedInput =
                JsonConvert.SerializeObject(input, s_rcmContractsJsonSerSettings);

            // TODO-SanKumar-1904: Pass the Encoding and ContentType here?!!!

            return Task.FromResult(
                (HttpContent)new StringContent(jsonSerializedInput));
        }

        private static readonly MediaTypeHeaderValue ApplicationJson =
            MediaTypeHeaderValue.Parse("application/json; charset=utf-8");

        private async Task CommonRequestHeadersEditor(
            RcmOperationContext context,
            HttpRequestHeaders httpRequestHeaders,
            HttpContentHeaders httpContentHeaders,
            CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            X509Certificate2 aadCertificate;

            // TODO-SanKumar-1904: How could we cache this cert and update only on change?!
            using (var certStore = new X509Store(StoreName.My, StoreLocation.LocalMachine))
            {
                certStore.Open(OpenFlags.OpenExistingOnly | OpenFlags.ReadOnly);

                // TODO-SanKumar-1904: Should we only request valid certs?
                // Currently left to the AAD to handle this.
                var certCollection = certStore.Certificates.Find(
                    X509FindType.FindByThumbprint,
                    m_aadDetails.CertificateThumbprint,
                    validOnly: false);

                // TODO-SanKumar-1904: First fails. We should rather log better with First or default

                aadCertificate = certCollection.
                    Cast<X509Certificate2>().
                    //Where(cert => cert.NotBefore >= DateTime.Now).
                    OrderByDescending(cert => cert.Verify()).
                    ThenByDescending(cert => cert.NotAfter).
                    First();

                // TODO-SanKumar-1904: Add exception handling with proper messages
            }

            var authority = m_aadDetails.AadAuthority.ToString();
            var authoritySegs = authority.Split('/');
            bool validateAuthority = authoritySegs.Length < 4 || !string.Equals(
                    "adfs",
                    authoritySegs[3],
                    StringComparison.OrdinalIgnoreCase);

            if (m_clientApp == null)
            {
                m_clientApp = ConfidentialClientApplicationBuilder.Create(
                        m_aadDetails.ApplicationId).
                        WithCertificate(aadCertificate).
                        WithAuthority(authority, validateAuthority).
                        WithHttpClientFactory(m_httpClient).
                        Build();
            }

            var scopes = new string[] { $"{m_aadDetails.Audience}/.default" };

            AuthenticationResult authResult =
                await m_clientApp.AcquireTokenForClient(scopes)
                .WithSendX5C(true)
                .ExecuteAsync().ConfigureAwait(false);

            // TODO-SanKumar-1904: Instead of simply failing at this point, we
            // should add more context to the token acquiring failure
            // https://docs.microsoft.com/en-us/azure/active-directory/develop/msal-error-handling-dotnet

            httpRequestHeaders.Authorization = AuthenticationHeaderValue.Parse(authResult.CreateAuthorizationHeader());
            httpRequestHeaders.Add("AuthTokenType", authResult.TokenType);

            httpRequestHeaders.Add(RcmHttpHeaders.Custom.MSActivityId, context.ActivityId);
            httpRequestHeaders.Add(RcmHttpHeaders.Custom.MSAgentComponentId, "ProcessServer");
            // RcmHttpHeaders.Custom.MSAgentDiskId
            httpRequestHeaders.Add(RcmHttpHeaders.Custom.MSAgentMachineId, m_machineId);
            // RcmHttpHeaders.Custom.MSAgentProxyId
            httpRequestHeaders.Add(RcmHttpHeaders.Custom.MSAuthType, "AadOAuth2");
            httpRequestHeaders.Add(RcmHttpHeaders.Custom.MSClientRequestId, context.ClientRequestId);
            // RcmHttpHeaders.Custom.MSContainerId
            // RcmHttpHeaders.Custom.MSPrincipalId
            // RcmHttpHeaders.Custom.MSPrincipalLiveId
            // RcmHttpHeaders.Custom.MSRequestId
            // RcmHttpHeaders.Custom.MSResourceId
            // RcmHttpHeaders.Custom.MSResourceLocation
            // RcmHttpHeaders.Custom.MSRole
            // RcmHttpHeaders.Custom.MSSrsActivityId
            // RcmHttpHeaders.Custom.MSSubscriptionId

            httpContentHeaders.ContentType = ApplicationJson;
        }

        private static async Task<Exception> RcmExceptionBuilderAsync(HttpResponseMessage httpResponseMessage)
        {
            try
            {
                var responseString = await httpResponseMessage.Content.ReadAsStringAsync();
                var error = RcmDataContractUtils<RcmError>.Deserialize(responseString);
                return new RcmException(error, httpResponseMessage.StatusCode);
            }
            catch
            {
                // TODO-SanKumar-2002: Reformat the response content into a custom
                // RcmException along with the client request id and activity id.
                return null;
            }
        }

        private Task PostAsync(
            RcmOperationContext context,
            string method,
            object input,
            CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            Task<HttpContent> RequestBuilderAdapter(CancellationToken ct)
            {
                return RequestBuilderAsync(input, ct);
            }

            Task RequestHeadersEditor(
                HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders,
                CancellationToken ct)
            {
                return CommonRequestHeadersEditor(
                    context, httpRequestHeaders, httpContentHeaders, ct);
            }

            return m_restWrapper.PostAsync(
                method,
                cancellationToken,
                input == null ? (RestWrapper.RequestBuilder)null : RequestBuilderAdapter,
                RequestHeadersEditor,
                RcmExceptionBuilderAsync);
        }

        private Task<OutputT> PostAsync<OutputT>(
            RcmOperationContext context,
            string method,
            object input,
            CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            Task<HttpContent> RequestBuilderAdapter(CancellationToken ct)
            {
                return RequestBuilderAsync(input, ct);
            }

            Task RequestHeadersEditor(
                HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders,
                CancellationToken ct)
            {
                return CommonRequestHeadersEditor(
                    context, httpRequestHeaders, httpContentHeaders, ct);
            }

            async Task<OutputT> ResponseParser(
                HttpResponseMessage httpResponseMessage, CancellationToken ct)
            {
                ct.ThrowIfCancellationRequested();

                return JsonConvert.DeserializeObject<OutputT>(
                    await httpResponseMessage.Content.ReadAsStringAsync().ConfigureAwait(false),
                    s_rcmContractsJsonSerSettings);
            }

            // TODO-SanKumar-1905: On failures, format the HTTP output into RcmException!

            return m_restWrapper.PostAsync(
                method, cancellationToken,
                input == null ? (RestWrapper.RequestBuilder)null : RequestBuilderAdapter,
                RequestHeadersEditor, ResponseParser, RcmExceptionBuilderAsync);
        }

        private static class RcmAgentApi
        {
            public static readonly string TestConnection = $"{nameof(RcmAgentApi)}/{nameof(TestConnection)}";
            public static readonly string UpdateAgentJobStatus = $"{nameof(RcmAgentApi)}/{nameof(UpdateAgentJobStatus)}";
            public static readonly string RegisterProcessServer = $"{nameof(RcmAgentApi)}/{nameof(RegisterProcessServer)}";
            public static readonly string ModifyProcessServer = $"{nameof(RcmAgentApi)}/{nameof(ModifyProcessServer)}";
            public static readonly string GetProcessServerSettings = $"{nameof(RcmAgentApi)}/{nameof(GetProcessServerSettings)}";
            public static readonly string UnregisterProcessServer = $"{nameof(RcmAgentApi)}/{nameof(UnregisterProcessServer)}";
            public static readonly string SendMonitoringMessage = $"{nameof(RcmAgentApi)}/{nameof(SendMonitoringMessage)}";
        }

        public Task<ProcessServerSettings> GetProcessServerSettingsAsync(
            RcmOperationContext context,
            GetProcessServerSettingsInput input,
            CancellationToken cancellationToken)
        {
            if (input == null)
                throw new ArgumentNullException(nameof(input));

            return PostAsync<ProcessServerSettings>(
                context, RcmAgentApi.GetProcessServerSettings, input, cancellationToken);
        }

        public Task RegisterProcessServerAsync(
            RcmOperationContext context,
            RegisterProcessServerInput input,
            CancellationToken cancellationToken)
        {
            if (input == null)
                throw new ArgumentNullException(nameof(input));

            return PostAsync(context, RcmAgentApi.RegisterProcessServer, input, cancellationToken);
        }

        public Task ModifyProcessServerAsync(
            RcmOperationContext context,
            ModifyProcessServerInput input,
            CancellationToken cancellationToken)
        {
            if (input == null)
                throw new ArgumentNullException(nameof(input));

            return PostAsync(context, RcmAgentApi.ModifyProcessServer, input, cancellationToken);
        }

        public Task TestConnectionAsync(
            RcmOperationContext context, string message, CancellationToken cancellationToken)
        {
            if (message == null)
                throw new ArgumentNullException(nameof(message));

            return PostAsync(context, RcmAgentApi.TestConnection, message, cancellationToken);
        }

        public Task UnregisterProcessServerAsync(
            RcmOperationContext context, string processServerId, CancellationToken cancellationToken)
        {
            if (string.IsNullOrWhiteSpace(processServerId))
                throw new ArgumentNullException(nameof(processServerId));

            return PostAsync(
                context, RcmAgentApi.UnregisterProcessServer, processServerId, cancellationToken);
        }

        public Task UpdateAgentJobStatusAsync(
            RcmOperationContext context, RcmJob job, CancellationToken cancellationToken)
        {
            if (job == null)
                throw new ArgumentNullException(nameof(job));

            return PostAsync(
                context, RcmAgentApi.UpdateAgentJobStatus, job, cancellationToken);
        }

        public Task SendMonitoringMessageAsync(
            RcmOperationContext context,
            SendMonitoringMessageInput input,
            CancellationToken cancellationToken)
        {
            if (input == null)
                throw new ArgumentNullException(nameof(input));

            return PostAsync(context, RcmAgentApi.SendMonitoringMessage, input, cancellationToken);
        }

        public override string ToString()
        {
            return (m_restWrapper == null) ?
                nameof(ProcessServerRcmApiStubsImpl) :
                $"{nameof(ProcessServerRcmApiStubsImpl)} - {m_restWrapper}";
        }

        public void Close()
        {
            Dispose();
        }

        #region Dispose Pattern

        ~ProcessServerRcmApiStubsImpl()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private int m_isDisposed = 0;

        private void ThrowIfDisposed()
        {
            if (m_isDisposed != 0)
                throw new ObjectDisposedException(this.ToString());
        }

        protected virtual void Dispose(bool disposing)
        {
            if (Interlocked.CompareExchange(ref m_isDisposed, 1, 0) == 1)
            {
                return;
            }

            if (disposing)
            {
                m_restWrapper?.Dispose();
            }

            // Dispose unmanaged resources
        }

        #endregion Dispose Pattern
    }
}
