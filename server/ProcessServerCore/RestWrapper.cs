using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Diagnostics;
using System.Globalization;
using System.Net;
using System.Net.Cache;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Security;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core
{
    internal class RestWrapper
    {
        // Event acting as a rundown barrier for pending requests. By
        // initializing with one additional count initially, we can chose to
        // initiate the rundown of this class at our will.
        private readonly CountdownEvent m_cntdownEvt = new CountdownEvent(1);

        private readonly CancellationTokenSource m_pendingRequestsCts = new CancellationTokenSource();
        private readonly HttpClient m_httpClient;

        public RestWrapper(
            Uri serverUri, TimeSpan timeout, IWebProxy proxy,
            RemoteCertificateValidationCallback remoteCertValidationCallback)
        {
            m_httpClient = CreateHttpClient(serverUri, timeout, proxy, remoteCertValidationCallback);
        }

        public static WebRequestHandler CreateWebRequestHandler(
            TimeSpan timeout, IWebProxy proxy,
            RemoteCertificateValidationCallback remoteCertValidationCallback)
        {
            return new WebRequestHandler
            {
                // AllowAutoRedirect = Default (true)
                // Note: .Net Framework will redirect from HTTPS to HTTP but not .Net core.

                // AllowPipelining = Default (true) https://en.wikipedia.org/wiki/HTTP_pipelining
                // Note: It may not be good idea to pipeline non-idempotent methods,
                // such as POST (but in CS, the atomicity is guaranteed by the SQL
                // locks and conditions in the queries, so we could make use of this
                // to optimally consume resources).

                // Server authentication is done by the application through equating
                // the stored server certificate. Client authentication is done by
                // the server by validating the HMAC signed using its passphrase.
                AuthenticationLevel = AuthenticationLevel.None,

                // TODO-SanKumar-1903: Take it as a setting?
                // AutomaticDecompression = Default (None)

                // BypassCache is anyway the default property retrieved based on the
                // setting in the machine.config. Just in case that entry is modified
                // to different policy, we are still enforcing bypass.
                CachePolicy = new RequestCachePolicy(RequestCacheLevel.BypassCache),

                // CheckCertificateRevocationList >= 4.7.1

                // ClientCertificateOptions = default(ClientCertificateOption.Manual) for WebRequestHandler
                // ClientCertificates = default(Empty)

                // ContinueTimeout = default (350ms)
                // Note: Time spent in waiting for 100-continue from the server, before sending further data

                // CookieContainer = default (stores server cookies; none added by the library)
                // Note: Even though, the server cookies (if any) would be stored,
                // the library never (yet) uses them in its request.

                // Credentials = Default (null)
                // DefaultProxyCredentials >= 4.7.1
                // ImpersonationLevel = Default (Delegation)
                // MaxAutomaticRedirections = Default (50)
                // MaxConnectionsPerServer >= 4.7.1

                // TODO-SanKumar-1903: Make this a setting?
                // MaxRequestContentBufferSize = Default (2GB)

                // MaxResponseHeadersLength = Default (DefaultMaximumResponseHeadersLength = Default (64KB))
                // PreAuthenticate = Default (false)
                // Properties >= 4.7.1

                // Note: Override priority order
                // a) handler.Proxy b) defaultproxy in Configuration file c) system WinInet proxy
                Proxy = proxy,

                ReadWriteTimeout = checked((int)(timeout.TotalMilliseconds)),

                // ServerCertificateCustomValidationCallback >= 4.7.1

                ServerCertificateValidationCallback = remoteCertValidationCallback

                // SslProtocols >= 4.7.1
                // SupportsAutomaticDecompression = Default (true)
                // SupportsProxy = Default (true)
                // SupportsRedirectConfiguration = Default (true)
                // UnsafeAuthenticatedConnectionSharing = Default (false)
                // UseCookies = Default (true)
                // UseDefaultCredentials = Default (false)
                // UseProxy = Default (true)
            };
        }

        public static HttpClient CreateHttpClient(
            Uri serverUri, TimeSpan timeout, IWebProxy proxy,
            RemoteCertificateValidationCallback remoteCertValidationCallback)
        {
            // TODO: Dispose handler, if the client couldn't be created.
            // TODO: Common handler for same parameters?

            var handler = CreateWebRequestHandler(timeout, proxy, remoteCertValidationCallback);
            return CreateHttpClient(handler, serverUri, timeout);
        }

        public static HttpClient CreateHttpClient(
            WebRequestHandler handler, Uri serverUri, TimeSpan timeout)
        {
            HttpClient httpClient = new HttpClient(handler, disposeHandler: true)
            {
                BaseAddress = serverUri,

                // TODO-SanKumar-1903: Take this as a setting?
                // MaxResponseContentBufferSize = Default (2GB)

                Timeout = timeout
                // TODO-SanKumar-1903: Do we need to set the following:
                // Connect timeout, HTTP protocol version, TLS version
            };

            // TODO: Dispose httpClient, if any exception occurs after this point.

            httpClient.DefaultRequestHeaders.ConnectionClose = false; // Keep alive

            // TODO-SanKumar-1903: Set ServicePointManager.FindServicePoint(new Uri(url)).ConnectionLeaseTimeout
            // Helps close the connections that are kept alive in time, which
            // otherwise might take very long to be detected as disconnected.
            // https://blogs.msdn.microsoft.com/shacorn/2016/10/21/best-practices-for-using-httpclient-on-services/

            return httpClient;
        }

        public delegate Task<HttpContent> RequestBuilder(CancellationToken cancellationToken);

        public delegate Task RequestHeadersEditor(
            HttpRequestHeaders httpRequestHeaders,
            HttpContentHeaders httpContentHeaders,
            CancellationToken cancellationToken);

        public delegate Task<T> ResponseParser<T>(
            HttpResponseMessage httpResponseMessage, CancellationToken cancellationToken);

        public delegate Task<Exception> ExceptionBuilder(HttpResponseMessage httpResponseMessage);

        public Task PostAsync(
            string method,
            CancellationToken cancellationToken,
            RequestBuilder requestBuilder,
            RequestHeadersEditor requestHeadersEditor,
            ExceptionBuilder exceptionBuilder)
        {
            return PostAsync(
                requestUri: method,
                cancellationToken: cancellationToken,
                requestBuilder: requestBuilder,
                requestHeadersEditor: requestHeadersEditor,
                responseParser: (ResponseParser<int>)null,
                exceptionBuilder: exceptionBuilder);
        }

        public Task<T> PostAsync<T>(
            string requestUri,
            CancellationToken cancellationToken,
            RequestBuilder requestBuilder,
            RequestHeadersEditor requestHeadersEditor,
            ResponseParser<T> responseParser)
        {
            return PostAsync(
                requestUri: requestUri,
                cancellationToken: cancellationToken,
                requestBuilder: requestBuilder,
                requestHeadersEditor: requestHeadersEditor,
                responseParser: responseParser,
                exceptionBuilder: null);
        }

        public async Task<T> PostAsync<T>(
            string requestUri,
            CancellationToken cancellationToken,
            RequestBuilder requestBuilder,
            RequestHeadersEditor requestHeadersEditor,
            ResponseParser<T> responseParser,
            ExceptionBuilder exceptionBuilder)
        {
            ThrowIfDisposed();

            if (!m_cntdownEvt.TryAddCount())
            {
                Debug.Assert(false);
                throw new InvalidOperationException();
            }

            try
            {
                CancellationTokenSource combinedCts = null;
                if (cancellationToken.CanBeCanceled)
                {
                    combinedCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, m_pendingRequestsCts.Token);
                    cancellationToken = combinedCts.Token;
                }
                else
                {
                    cancellationToken = m_pendingRequestsCts.Token;
                }

                using (combinedCts)
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    using (var request = new HttpRequestMessage(HttpMethod.Post, requestUri))
                    {
                        if (requestBuilder != null)
                        {
                            request.Content =
                                await requestBuilder(cancellationToken).ConfigureAwait(false);

#if DEBUG
                            if (request.Content != null)
                            {
                                // TODO: Include Base URI, request URI, request headers and content headers.
                                Tracers.RestWrapper.DebugAdminLogV2Message(
                                    await request.Content.ReadAsStringAsync().ConfigureAwait(false));
                            }
#endif
                        }

                        cancellationToken.ThrowIfCancellationRequested();

                        //request.Properties
                        //request.Version
                        // The default in the .NET Framework and earlier versions of .NET Core
                        // is 1.1. In .NET Core 2.1 and later, it is 2.0.

                        if (requestHeadersEditor != null)
                        {
                            await requestHeadersEditor(
                                request.Headers, request.Content.Headers, cancellationToken).
                                ConfigureAwait(false);

#if DEBUG
                            Tracers.RestWrapper.DebugAdminLogV2Message("Request Headers: {0}{1}", 
                                Environment.NewLine, request.Headers.ToString());
                            Tracers.RestWrapper.DebugAdminLogV2Message("Request content headers: {0}{1}", 
                                Environment.NewLine, request.Content.Headers.ToString());
#endif
                        }

                        // TODO-SanKumar-1903: Rather use SendAsync() with new HttpRequestMessage().Method = HttpMethod.Post?

                        // TODO: Move to config
                        // Return the call right after the 
                        const HttpCompletionOption completionOption = HttpCompletionOption.ResponseHeadersRead;

                        using (var response =
                            await m_httpClient.SendAsync(request, completionOption, cancellationToken).
                            ConfigureAwait(false))
                        {
                            if (response.Content != null)
                            {
                                // TODO: Include Base URI, request URI, response headers, HTTP code.

                                // TODO-SanKumar-1904: Handle the case, where reading the content
                                // could fail
                                Tracers.RestWrapper.DebugAdminLogV2Message(
                                    "Response Content: {0}{1}",
                                    Environment.NewLine,
                                    await response.Content.ReadAsStringAsync().
                                    ConfigureAwait(false));
                            }
                            else
                            {
                                Tracers.RestWrapper.DebugAdminLogV2Message(
                                    "Content is not present is response data");
                            }

                            if (exceptionBuilder != null &&
                                !response.IsSuccessStatusCode &&
                                response.Content != null)
                            {
                                Exception ex = await exceptionBuilder(response).ConfigureAwait(false);
                                if (ex != null)
                                    throw ex;
                            }

                            try
                            {
                                // TODO-SanKumar-1903: Should we only check for 200, since this checks for 20x?
                                response.EnsureSuccessStatusCode();
                            }
                            finally
                            {
                                if (Settings.Default.RestWrapper_LogPostCallResponse)
                                {
                                    Tracers.RestWrapper.TraceAdminLogV2Message(TraceEventType.Information,
                                        "Post request {0}, status code : {1}, response content : {2}{3}",
                                        response.IsSuccessStatusCode ? "succeeded" : "failed",
                                        (int)response.StatusCode,
                                        Environment.NewLine,
                                        (response.Content != null) ? await response.Content.ReadAsStringAsync().
                                        ConfigureAwait(false) : string.Empty);
                                }
                            }

                            if (responseParser != null)
                                return await responseParser(response, cancellationToken).ConfigureAwait(false);
                            else
                                return default(T);
                        }
                    }
                }
            }
            finally
            {
                m_cntdownEvt.Signal();
            }
        }

        public override string ToString()
        {
            if (m_httpClient != null && m_httpClient.BaseAddress != null)
            {
                return string.Format(
                    CultureInfo.InvariantCulture,
                    "RestWrapper - {0}",
                    m_httpClient.BaseAddress);
            }

            return "RestWrapper";
        }

        public void Close()
        {
            Dispose();
        }

        #region Dispose Pattern

        ~RestWrapper()
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
                try
                {
                    m_pendingRequestsCts.Cancel();

                    // Redundant but extra safety
                    m_httpClient.CancelPendingRequests();

                    // Signalling the one additional hold-up from the init count.
                    m_cntdownEvt.Signal();

                    m_cntdownEvt.Wait();
                }
                finally
                {
                    m_httpClient.Dispose();

                    m_cntdownEvt.Dispose();

                    m_pendingRequestsCts.Dispose();
                }
            }

            // Dispose unmanaged resources
        }

        #endregion Dispose Pattern
    }
}
