using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.XmlElements;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

// TODO: This should go to another common dll - Microsoft.Azure.SiteRecovery.CSApi
namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    /// <summary>
    /// Implementation of IProcessServerCSApiStubs
    /// </summary>
    internal class ProcessServerCSApiStubsImpl : IProcessServerCSApiStubs
    {
        private readonly PhpFormatter m_phpFormatter = new PhpFormatter(serializeObjectAsArray: true);
        private readonly string m_hostId;
        private readonly string m_csFingerprintFileName, m_csFingerprintFilePath, m_csPassphrase;
        private readonly bool m_isHttps;
        private readonly FileSystemWatcher m_csFingerprintFileWatcher;

        private volatile Lazy<string> m_csFingerprint;

        private readonly Lazy<string> m_psBuildVersionString;

        private readonly Uri m_csUri;
        private readonly RestWrapper m_restWrapper;

        // TODO-SanKumar-1903: Provide override override that takes cs ip, port
        // and http scheme, which would help developing m x n test clients.
        // Even same WebRequestHandler could be used by those test clients, since
        // they would mostly be talking to one or very few CS.
        public ProcessServerCSApiStubsImpl()
        {
            m_hostId = PSInstallationInfo.Default.GetPSHostId();

            m_csFingerprintFileName = LegacyCSUtils.GetCSFingerprintFileName();
            m_csFingerprintFilePath = Path.Combine(
                Settings.Default.LegacyCS_FullPath_Fingerprints, m_csFingerprintFileName);

            ResetCachedCSFingerprint();

            m_csFingerprintFileWatcher = new FileSystemWatcher(
                Settings.Default.LegacyCS_FullPath_Fingerprints, m_csFingerprintFileName)
            {
                IncludeSubdirectories = false,
                NotifyFilter =
                    NotifyFilters.FileName | NotifyFilters.LastWrite |
                    NotifyFilters.Size | NotifyFilters.CreationTime |
                    NotifyFilters.Attributes | NotifyFilters.Security |
                    NotifyFilters.DirectoryName
            };
            // Not adding NotifyFilters.LastAccess out of all, since unlike others
            // this doesn't indicate modification to the file.

            m_csFingerprintFileWatcher.Changed += OnCSFingerprintFileChanged;
            m_csFingerprintFileWatcher.Created += OnCSFingerprintFileChanged;
            m_csFingerprintFileWatcher.Deleted += OnCSFingerprintFileChanged;
            m_csFingerprintFileWatcher.Error += OnCSFingerprintFileWatcherError;
            m_csFingerprintFileWatcher.Renamed += OnCSFingerprintFileRenamed;

            m_csFingerprintFileWatcher.EnableRaisingEvents = true;

            m_csPassphrase = LegacyCSUtils.GetCSPassphrase();

            // This member is set to be lazily initialized from the file. So,
            // when the next time version string is needed, the file would be read
            // and the value would be cached. This is thread safe but if multiple
            // threads access this lazy object, it is set to race by reading file
            // in each of those threads and whomever succeeds will set the value
            // (PublicationOnly option). Also, any error in retrieving the version
            // string is localized, so each request beyond that point would attempt
            // to read the version string from the file until succeeded.
            m_psBuildVersionString = new Lazy<string>(
                PSInstallationInfo.Default.GetPSCurrentVersion, LazyThreadSafetyMode.PublicationOnly);

            m_isHttps = AmethystConfig.Default.GetConfigurationServerScheme() == HttpScheme.Https;

            m_csUri = new UriBuilder(
                scheme: AmethystConfig.Default.GetConfigurationServerScheme().ToString(),
                host: AmethystConfig.Default.GetConfigurationServerIP().ToString(),
                portNumber: AmethystConfig.Default.GetConfigurationServerPort()
                ).Uri;

            // TODO-SanKumar-1904: Read this from the new Process Server config.
            IWebProxy webProxy = null;

            m_restWrapper = new RestWrapper(
                m_csUri, Settings.Default.PSCSApiImpl_HttpTimeout, webProxy,
                this.RemoteCertificateValidationCallback);
        }

        // TODO-SanKumar-1904: Currently the CS certificate is renewed by the PS,
        // when the register request fails with SSL error after stopping the services.
        // When all the calls come into this implementation, we could detect
        // the fingerprint mismatch immediately and trigger cert renewal,
        // while temporarily pausing new http requests without having to restart
        // the .Net PS services.
        private bool RemoteCertificateValidationCallback(object sender, X509Certificate certificate, X509Chain chain, SslPolicyErrors sslPolicyErrors)
        {
            // Example: https://docs.microsoft.com/en-us/previous-versions/office/developer/exchange-server-2010/dd633677(v=exchg.80)

            // If the server presents a valid and signed certificate, then proceed
            if (sslPolicyErrors == SslPolicyErrors.None)
                return true;

            // Note-SanKumar-1902: As per my understanding, this would be set,
            // if the client doesn't pass certificate when expected by the server.
            Debug.Assert(!sslPolicyErrors.HasFlag(SslPolicyErrors.RemoteCertificateNotAvailable));

            if ((sslPolicyErrors & ~(SslPolicyErrors.RemoteCertificateNameMismatch | SslPolicyErrors.RemoteCertificateChainErrors)) != 0)
            {
                Tracers.CSApi.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failing CS server certificate validation due to one or more unknown policy errors : {0} for certificate with fingerprint : {1}",
                    sslPolicyErrors, certificate.GetCertHashString());

                return false;
            }

            if (sslPolicyErrors.HasFlag(SslPolicyErrors.RemoteCertificateNameMismatch))
            {
                // No handling required, since we know that CS cert isn't
                // generated with CS host name or IP as the Subject, (which is
                // the standard validation used by the web browsers).

                if (sslPolicyErrors == SslPolicyErrors.RemoteCertificateNameMismatch)
                {
                    // TODO-SanKumar-2004: Should we ensure that only the self-signed
                    // cert of InMage is allowed? i.e. check that if subject == issuer
                    // and if subject == "C=US, ST=Washington, L=Redmond, O=Microsoft, OU=InMage, CN=Scout"?
                    // Otherwise, this lets any server whose certificate is trusted
                    // in the machine to respond to the PS calls. Drawback is that
                    // we will tie down this validation to just the particular
                    // implementation of the CS certificate and it might break,
                    // if any change is made to the subject name in the certificate
                    // generation in the future.


                    // If it's the only policy error, proceed to succeed the validation.
                    // This situation is expected, if the self-signed CS cert is indeed
                    // added to the Trusted Root CA manually by the customer.
                    return true;
                }
            }

            if (sslPolicyErrors.HasFlag(SslPolicyErrors.RemoteCertificateChainErrors))
            {
                // TODO-SanKumar-1903: It could be possible that the CS could
                // chain certificates ending with self-signed certificate. So,
                // should we actually skip through valid intermediate certificates
                // till we reach the topmost self-signed CS certificate?
                if (chain == null || chain.ChainStatus == null || chain.ChainStatus.Length != 1)
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failing CS server certificate validation for certificate with fingerprint : {0}, " +
                        "due to unexpected chaining. Chain length : {1}.",
                        certificate.GetCertHashString(), (chain?.ChainStatus?.Length).GetValueOrDefault());

                    return false;
                }

                // TODO-SanKumar-1903: We could use the raw data from the
                // distinguished names - SubjectName and IssuerName for
                // stricter self-signed comparison.
                if ((certificate.Subject != certificate.Issuer) ||
                    (chain.ChainStatus[0].Status != X509ChainStatusFlags.UntrustedRoot))
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failing CS server certificate validation for certificate with fingerprint : {0}{1}" +
                        "1. Server certificate is not self-signed. Subject: {2}. Issuer : {3}.{4}" +
                        "2. or, Default .Net validation returned unrecognized status for the self-signed certifiate : {5}.",
                        certificate.GetCertHashString(), Environment.NewLine,
                        certificate.Subject, certificate.Issuer, Environment.NewLine,
                        chain.ChainStatus[0].Status);

                    return false;
                }

                string fingerprintInFile;
                try
                {
                    fingerprintInFile = m_csFingerprint.Value;
                }
                catch (Exception ex)
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failing CS server certificate validation due to failure in reading the CS fingerprint from file : {0}{1}{2}",
                        m_csFingerprintFilePath, Environment.NewLine, ex);

                    return false;
                }

                // TODO-SanKumar-1903: Would be better to compute stronger hash of complete
                // RawData of stored CS cert, then equate it against server certificate,
                // instead of comparing the fingerprints. This might have a higher performance
                // impact though.
                if (!string.Equals(certificate.GetCertHashString(), fingerprintInFile, StringComparison.OrdinalIgnoreCase))
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failing CS server certificate validation, as the CS fingerprint in file : {0} doesn't match the actual server certificate's fingerprint : {1}",
                        fingerprintInFile, certificate.GetCertHashString());

                    var csNewFingerPrint = certificate.GetCertHashString().ToLower();
                    if (!string.IsNullOrWhiteSpace(csNewFingerPrint))
                    {
                        Tracers.CSApi.TraceAdminLogV2Message(TraceEventType.Information,
                            "Updating new fingerprint {0} in file {1}", csNewFingerPrint, m_csFingerprintFilePath);

                        File.WriteAllText(m_csFingerprintFilePath, csNewFingerPrint);

                        Tracers.CSApi.TraceAdminLogV2Message(TraceEventType.Information,
                            "Successfully updated fingerprint");

                        ResetCachedCSFingerprint();
                    }

                    return true;
                }

                // If it's a self-signed certificate, whose fingerprint
                // matches the stored CS fingerprint, go ahead with it.
                return true;
            }

            Tracers.CSApi.TraceAdminLogV2Message(
                TraceEventType.Error,
                "Failing CS server certificate validation due to unhandled error from default .Net validation of certificate with fingerprint : {0}",
                certificate.GetCertHashString());

            return false;
        }

        private void ResetCachedCSFingerprint()
        {
            // This method resets the cached fingerprint string and sets it to be
            // lazily initialized from the file. So, when the next time fingerprint
            // is needed, the file would be read and the value would be cached.
            // This is thread safe but if multiple threads access this lazy object,
            // it is set to race by reading file in each of those threads and
            // whomever succeeds will set the value (PublicationOnly option).
            // Also, any error in retrieving the fingerprint is localized, so
            // each request beyond that point would attempt to read the fingerprint
            // until succeeded.

            m_csFingerprint = new Lazy<string>(
                () => File.ReadAllText(m_csFingerprintFilePath, Encoding.UTF8).TrimEnd(),
                LazyThreadSafetyMode.PublicationOnly);
        }

        private void OnCSFingerprintFileRenamed(object sender, RenamedEventArgs args)
        {
            Tracers.CSApi.TraceAdminLogV2MsgWithExtProps(TraceEventType.Information,
                new Dictionary<string, string>() {
                    {"OldFilePath", args.OldFullPath},
                    {"NewFilePath", args.FullPath}
                },
                "CS Fingerprint file rename detected from {0} to {1}.",
                args.OldName, args.Name);

            this.ResetCachedCSFingerprint();
        }

        private void OnCSFingerprintFileChanged(object sender, FileSystemEventArgs args)
        {
            Tracers.CSApi.TraceAdminLogV2MsgWithExtProps(TraceEventType.Information,
                new Dictionary<string, object>() {
                    {"FilePath", args.FullPath},
                    {"ChangeType", args.ChangeType}
                },
                "{0} change detected on CS fingerprint file - {1}.",
                args.ChangeType, args.Name);

            this.ResetCachedCSFingerprint();
        }

        private void OnCSFingerprintFileWatcherError(object sender, ErrorEventArgs args)
        {
            var ex = args.GetException();

            Tracers.CSApi.TraceAdminLogV2MsgWithExtProps(TraceEventType.Error,
                new Dictionary<string, string>()
                {
                    {"FilePath", m_csFingerprintFilePath},
                    {"Exception", ex.GetType().Name},
                    {"Message", ex.Message}
                },
                "Error detected on CS fingerprint file - {0} change tracking{1}{2}",
                m_csFingerprintFileName, Environment.NewLine, ex);

            // Generally the file watcher error occurs due to buffer overflow,
            // mainly due to the file path being very long along with large
            // number of actions. Since some activity has been going on with this
            // file, Reset the lazy object to be best effort.

            this.ResetCachedCSFingerprint();
        }

        private delegate (HttpContent RequestContent, string AccessSignature) RequestBuilder(
            string requestId, CancellationToken cancellationToken);

        private delegate void RequestHeadersEditor(
            string requestId,
            string accessSignature,
            HttpRequestHeaders httpRequestHeaders,
            HttpContentHeaders httpContentHeaders,
            CancellationToken cancellationToken);

        private delegate Task<T> ResponseParser<T>(
            string requestId,
            HttpResponseMessage httpResponseMessage,
            CancellationToken cancellationToken);

        private async Task<T> PostAsync<T>(
            string accessFileName, CancellationToken cancellationToken, RequestBuilder requestBuilder,
            RequestHeadersEditor requestHeadersEditor, ResponseParser<T> responseParser)
        {
            ThrowIfDisposed();

            string requestId = string.Empty;

            try
            {
                // TODO-SanKumar-1903: Move to Settings
                const int NonceCharCount = 32;

                string accessSignature = string.Empty;

                async Task<HttpContent> RequestBuilder(CancellationToken ct)
                {
                    // NOTE-SanKumar-1902: Perl invocation passes "false", which is a string
                    // taken as true for includeTS. Continuing the legacy implmentation here.
                    Interlocked.Exchange(
                        ref requestId,
                        await RandUtils.GenerateRandomNonce(
                            count: NonceCharCount, includeTimestamp: true, ct: ct)
                            .ConfigureAwait(false));

                    Tracers.CSApi.TraceAdminLogV2Message(TraceEventType.Verbose, "Starting request : {0}", requestId);

                    var (builtRequestContent, generatedAccSignature) = requestBuilder(requestId, ct);

                    Interlocked.Exchange(ref accessSignature, generatedAccSignature);

                    return builtRequestContent;
                }

                Task RequestHeadersEditor(
                    HttpRequestHeaders httpRequestHeaders,
                    HttpContentHeaders httpContentHeaders,
                    CancellationToken ct)
                {
                    requestHeadersEditor(requestId, accessSignature, httpRequestHeaders, httpContentHeaders, ct);
                    return Task.CompletedTask;
                }

                Task<T> ResponseParser(HttpResponseMessage httpResponseMessage, CancellationToken ct)
                {
                    return responseParser(requestId, httpResponseMessage, ct);
                }

                return await m_restWrapper.PostAsync(
                    accessFileName, cancellationToken, RequestBuilder,
                    RequestHeadersEditor, responseParser == null ? (RestWrapper.ResponseParser<T>)null : ResponseParser).ConfigureAwait(false);
            }
            finally
            {
                Tracers.CSApi.TraceAdminLogV2Message(TraceEventType.Verbose, "Ending request : {0}", requestId);
            }
        }

        internal const string CS_AUTH_METHOD = "ComponentAuth_V2015_01";
        internal const string REQUEST_METHOD_POST = "POST";

        internal static class CSApiVersion
        {
            public const string V1_0 = "1.0";

            public const string CURRENT = V1_0;
        }

        internal static class AccessFileName
        {
            public const string CXAPI_PHP = "/ScoutAPI/CXAPI.php";
            public const string UPDATE_PS_STATISTICS_PHP = "/update_ps_statics.php";
            public const string GET_DB_DATA_PHP = "get_db_data.php";
            public const string STOP_REPLICATION_PS_PHP = "/stop_replication_ps.php";
            public const string UPLOAD_FILE_PHP = "/upload_file.php";
        }

        internal static class FunctionName
        {
            public const string UPDATE_DB = "UpdateDB";
            public const string GET_PS_SETTINGS = "GetPSSettings";
            public const string UPDATE_PS_STATISTICS = "update_ps_statics";
            public const string REGISTER_HOST = "RegisterHost";
            public const string STOP_REPLICATION_PS = "stop_replication_ps";
            public const string ADD_ERROR_MESSAGE = "addErrorMessage";
            public const string MARK_RESYNC_REQUIRED = "MarkResyncRequiredToPairWithReasonCode";
            public const string GET_DB_DATA = "GetDBData";
            public const string IS_ROLLBACK_IN_PROGRESS = "IsRollbackInProgress";
            public const string UPLOAD_FILE = "upload_file";
        }

        internal static class MediaType
        {
            public static readonly MediaTypeHeaderValue MEDIA_TYPE_XML_UTF8 =
                MediaTypeHeaderValue.Parse("text/xml; charset=utf-8");

            public static readonly MediaTypeHeaderValue MEDIA_TYPE_FORM_URL_ENC =
                MediaTypeHeaderValue.Parse("application/x-www-form-urlencoded");
        }

        /// <summary>
        /// Query types for updatedb query
        /// </summary>
        private static class UpdateDbQueryType
        {
            public const string Update = "UPDATE";
            public const string Insert = "INSERT";
            public const string Replace = "REPLACE"; 
            public const string Delete = "DELETE";
        }

        /// <summary>
        /// Table names for update db queries
        /// </summary>
        private static class UpdateDbTableNames
        {
            public const string SrcLogicalVolumeDestLogicalVolume = "srclogicalvolumedestlogicalvolume";
            public const string SystemHealth = "systemhealth";
            public const string ProcessServer = "processserver";
            public const string Hosts = "hosts";
            public const string HealthFactors = "healthfactors";
            public const string PolicyInstance = "policyinstance";
            public const string LogRotationList = "logrotationlist";
        }

        private
        (StreamContent RequestContent, string AccessSignature)
            XmlRequestBuilder(
                ICSApiReqBuilder_TextWriter request,
                string requestId,
                string requestMethod,
                string functionName,
                string accessFileName,
                CancellationToken cancellationToken
            )
        {
            ThrowIfDisposed();

            var fingerprint = m_isHttps ? m_csFingerprint.Value : string.Empty;

            var accessSignature = LegacyCSUtils.ComputeSignatureComponentAuth(
                passphrase: m_csPassphrase,
                requestMethod: requestMethod,
                resource: accessFileName,
                function: functionName,
                requestId: requestId,
                version: CSApiVersion.CURRENT,
                fingerprint: fingerprint);

            // TODO-SanKumar-1906: Move to settings
            const int DefBufSize = 1024;

            MemoryStream ms = null;
            bool disposeMS = true;

            try
            {
                ms = new MemoryStream();

                using (var sw = new FormattingStreamWriter(ms, CultureInfo.InvariantCulture, Encoding.UTF8, DefBufSize, leaveOpen: true))
                {
                    request.GenerateXml(tw: sw, requestId: requestId, hostId: m_hostId, hmacSignature: accessSignature);
                }

                ms.Seek(0, SeekOrigin.Begin);

                var content = new StreamContent(ms);
                disposeMS = false;
                // The stream will be used by the content object further, which
                // now owns the responsibility of disposing it.
                return (content, accessSignature);
            }
            finally
            {
                if (disposeMS)
                    ms?.Dispose();
            }
        }

        private
        (FormUrlEncodedContent RequestContent, string AccessSignature)
            FormUrlEncodedRequestBuilder(
                IReadOnlyDictionary<string, ICSApiReqBuilder_TextWriter> requests,
                string requestId,
                string requestMethod,
                string functionName,
                string accessFileName,
                CancellationToken cancellationToken
            )
        {
            ThrowIfDisposed();

            var fingerprint = m_isHttps ? m_csFingerprint.Value : string.Empty;

            var accessSignature = LegacyCSUtils.ComputeSignatureComponentAuth(
                passphrase: m_csPassphrase,
                requestMethod: requestMethod,
                resource: accessFileName,
                function: functionName,
                requestId: requestId,
                version: CSApiVersion.CURRENT,
                fingerprint: fingerprint);

            var requestsDict = new Dictionary<string, string>();

            // TODO-SanKumar-1906: Move to settings
            const int DefBufSize = 1024;

            foreach (var currKVPair in requests)
            {
                using (MemoryStream ms = new MemoryStream())
                {
                    using (var sw = new FormattingStreamWriter(
                        ms, CultureInfo.InvariantCulture, Encoding.UTF8, DefBufSize, leaveOpen: true))
                    {
                        currKVPair.Value.GenerateXml(
                            tw: sw, requestId: requestId, hostId: m_hostId,
                            hmacSignature: accessSignature);
                    }

#if DEBUG
                    ms.Seek(0, SeekOrigin.Begin);

                    using (var sr = new StreamReader(
                        ms, Encoding.UTF8, detectEncodingFromByteOrderMarks: false,
                        bufferSize: DefBufSize, leaveOpen: true))
                    {
                        Tracers.RestWrapper.DebugAdminLogV2Message(currKVPair.Key);
                        Tracers.RestWrapper.DebugAdminLogV2Message(sr.ReadToEnd());
                    }
#endif

                    ms.Seek(0, SeekOrigin.Begin);

                    using (var sr = new StreamReader(
                        ms, Encoding.UTF8, detectEncodingFromByteOrderMarks: false,
                        bufferSize: DefBufSize, leaveOpen: true))
                    {
                        requestsDict[currKVPair.Key] = sr.ReadToEnd();
                    }
                }
            }

            return (new FormUrlEncodedContent(requestsDict), accessSignature);
        }

        private
        (FormUrlEncodedContent RequestContent, string AccessSignature)
            FormUrlEncodedRequestBuilder(
                IReadOnlyDictionary<string, string> requestDict,
                string requestId,
                string requestMethod,
                string functionName,
                string accessFileName,
                CancellationToken cancellationToken
            )
        {
            ThrowIfDisposed();

            var accessSignature = LegacyCSUtils.ComputeSignatureComponentAuth(
                passphrase: m_csPassphrase,
                requestMethod: requestMethod,
                resource: accessFileName,
                function: functionName,
                requestId: requestId,
                version: CSApiVersion.CURRENT,
                fingerprint: m_isHttps ? m_csFingerprint.Value : string.Empty);

            return (new FormUrlEncodedContent(requestDict), accessSignature);
        }

        private
            (MultipartFormDataContent RequestContent, string AccessSignature)
            MultipartFormDataRequestBuilder(
                IReadOnlyList<Tuple<string, string>> fileList,
                string requestId,
                string requestMethod,
                string functionName,
                string accessFileName,
                CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            var accessSignature = LegacyCSUtils.ComputeSignatureComponentAuth(
                passphrase: m_csPassphrase,
                requestMethod: requestMethod,
                resource: accessFileName,
                function: functionName,
                requestId: requestId,
                version: CSApiVersion.CURRENT,
                fingerprint: m_isHttps ? m_csFingerprint.Value : string.Empty);

            MultipartFormDataContent formContent = new MultipartFormDataContent();
            int count = 0;

            foreach (var fileData in fileList)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    string fileName = fileData.Item1;
                    string filePath = fileData.Item2;

                    if(!File.Exists(fileName))
                    {
                        throw new FileNotFoundException(fileName);
                    }

                    // Always keep this at the top, since it can throw exceptions.
                    formContent.Add(new StreamContent(File.OpenRead(fileName)),
                        "file_" + count, Path.GetFileName(fileName));
                    formContent.Add(new StringContent(fileName), "file_name_" + count);
                    formContent.Add(new StringContent(filePath), "file_path_" + count);
                    count++;
                });
            }
            return (formContent, accessSignature);
        }

        private static void 
            CommonAuthHeadersEditor(
                string phpApiName,
                string requestId,
                string accessSignature,
                HttpRequestHeaders httpRequestHeaders)
        {
            httpRequestHeaders.Add("HTTP-AUTH-VERSION", CSApiVersion.CURRENT);
            httpRequestHeaders.Add("HTTP-AUTH-SIGNATURE", accessSignature);
            httpRequestHeaders.Add("HTTP-AUTH-REQUESTID", requestId);
            httpRequestHeaders.Add("HTTP-AUTH-PHPAPI-NAME", phpApiName);
        }

        private static void
            CommonRequestHeadersEditor(
                string phpApiName,
                string requestId,
                string accessSignature,
                MediaTypeHeaderValue mediaType,
                HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders)
        {
            CommonAuthHeadersEditor(
                phpApiName: phpApiName,
                requestId: requestId,
                accessSignature: accessSignature,
                httpRequestHeaders: httpRequestHeaders);

            httpContentHeaders.ContentType = mediaType;
        }

        private static void
            MultipartFormRequestHeadersEditor(
                string phpApiName,
                string requestId,
                string accessSignature,
                HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders)
        {
            CommonAuthHeadersEditor(
                phpApiName: phpApiName,
                requestId: requestId,
                accessSignature: accessSignature,
                httpRequestHeaders: httpRequestHeaders);
        }

        private async Task<T> ParseXMLResponseAsync<T>(
            string requestId,
            HttpResponseMessage response)
        {
            ThrowIfDisposed();

            var responseElement = ResponseElement.Parse(
                await response.Content.ReadAsStreamAsync().ConfigureAwait(false));

            Tracers.CSApi.DebugAdminLogV2Message(
                "RequestId: {0} | Response: {1}",
                requestId, responseElement.ToString());

            responseElement.EnsureSuccess();

            T retVal = default(T);

            if (typeof(T) == typeof(int))
            {
                retVal = (T)(object)1;
            }
            else if (typeof(T) == typeof(FunctionResponseElement))
            {
                if (responseElement?.Body?.FunctionList?[0]?.FunctionResponse == null)
                    throw new InvalidDataException("Invalid response from CS");

                retVal = (T)(object)responseElement.Body.FunctionList[0].FunctionResponse;
            }

            return retVal;
        }

        private Task UpdateDBAsync(UpdateDBRequest updateDBRequest, CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                return XmlRequestBuilder(
                    updateDBRequest,
                    requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.UPDATE_DB,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.UPDATE_DB,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<int> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                return await ParseXMLResponseAsync<int>(
                    requestId: requestId,
                    response: response);
            };

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        private static bool ValidateRegisterPSHostInput(HostInfo hostInfo)
        {
            bool status = true;
            StringBuilder inputError = new StringBuilder();

            if (string.IsNullOrWhiteSpace(hostInfo.HostId))
            {
                inputError.AppendFormat(
                    "{0} is null or empty {1}", nameof(hostInfo.HostId), Environment.NewLine);
            }

            if (string.IsNullOrWhiteSpace(hostInfo.HostName))
            {
                inputError.AppendFormat(
                    "{0} is null or empty {1}", nameof(hostInfo.HostName), Environment.NewLine);
            }

            if(!SystemUtils.IsValidIPV4Address(hostInfo.IPAddress))
            {
                inputError.AppendFormat(
                    "{0} is not valid {1}", nameof(hostInfo.IPAddress), Environment.NewLine);
            }

            if (inputError.Length > 0)
            {
                Tracers.CSApi.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Register PS Host input validation failed with error : {0}{1}",
                                Environment.NewLine,
                                inputError);

                status = false;
            }

            return status;
        }

        private static bool ValidateRegisterPSNetworkInput(NicInfo nicInfo)
        {
            bool status = true;
            StringBuilder inputError = new StringBuilder();

            if (string.IsNullOrWhiteSpace(nicInfo.HostId))
            {
                inputError.AppendFormat(
                    "{0} is null or empty {1}", nameof(nicInfo.HostId), Environment.NewLine);
            }

            var validNicIPAddresses =
                    nicInfo.IpAddresses.Where(curr => SystemUtils.IsValidIPV4Address(curr)).ToList();

            // TODO : Check if we need to throw even if any of the nic ipaddress is invalid?
            if (validNicIPAddresses.Count <= 0)
            {
                inputError.AppendFormat(
                    "{0} is not valid {1}", nameof(nicInfo.IpAddresses), Environment.NewLine);
            }

            foreach (var netmask in nicInfo.NetMask)
            {
                if (string.IsNullOrWhiteSpace(netmask.NicIPAddress))
                {
                    inputError.AppendFormat(
                        "{0} is not valid {1}", nameof(netmask.NicIPAddress), Environment.NewLine);
                }

                if (string.IsNullOrWhiteSpace(netmask.SubNetMask))
                {
                    inputError.AppendFormat(
                        "{0} is not valid {1}", nameof(netmask.SubNetMask), Environment.NewLine);
                }
            }

            if (string.IsNullOrWhiteSpace(nicInfo.DeviceName))
            {
                inputError.AppendFormat(
                    "{0} is null or empty {1}", nameof(nicInfo.DeviceName), Environment.NewLine);
            }

            // TODO : Add/Remove validations for network inputs if required.

            if (inputError.Length > 0)
            {
                Tracers.CSApi.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Register PS Network input validation failed with error : {0}{1}",
                                Environment.NewLine,
                                inputError);

                status = false;
            }

            return status;
        }

        private (string HostInput, string NetworkInput) BuildV2APSRegisterInput(
            HostInfo hostInfo,
            Dictionary<string, NicInfo> networkInfo)
        {
            ThrowIfDisposed();

            if (hostInfo == null)
                throw new ArgumentNullException(nameof(hostInfo));

            if (networkInfo == null || !networkInfo.Any())
                throw new ArgumentNullException(nameof(networkInfo));

            if (!ValidateRegisterPSHostInput(hostInfo))
            {
                throw new ArgumentException(
                    $"{nameof(hostInfo)} contains invalid properties");
            }

            // Below keys are defined as per CS contract
            var hostDetails = new Dictionary<string, object>()
            {
                ["accountId"] = hostInfo.AccountId,
                ["psInstallTime"] = hostInfo.PSInstallTime,
                ["csEnabled"] = hostInfo.CSEnabled,
                ["hypervisorName"] = hostInfo.HypervisorName,
                ["sslPort"] = hostInfo.SslPort,
                ["psPatchVersion"] = hostInfo.PSPatchVersion,
                ["processServerEnabled"] = hostInfo.ProcessServerEnabled,
                ["patchInstallTime"] = hostInfo.PatchInstallTime,
                ["osType"] = hostInfo.OSType,
                ["id"] = hostInfo.HostId,
                ["hypervisorState"] = hostInfo.HypervisorState,
                ["name"] = hostInfo.HostName,
                ["psVersion"] = hostInfo.PSVersion,
                ["port"] = hostInfo.Port,
                ["ipaddress"] = hostInfo.IPAddress.ToString(),
                ["accountType"] = hostInfo.AccountType,
                ["osFlag"] = hostInfo.OSFlag
            };

            var networkDetails = new Dictionary<string, Dictionary<string, object>>();
            foreach (var item in networkInfo)
            {
                var nicInfo = item.Value;

                if (nicInfo == null)
                    throw new ArgumentNullException(nameof(nicInfo));

                if (!ValidateRegisterPSNetworkInput(nicInfo))
                {
                    throw new ArgumentException(
                        $"{nameof(nicInfo)} contains invalid properties");
                }

                // Below keys are defined as per CS contract
                var nicDict = new Dictionary<string, object>()
                {
                    ["dns"] = nicInfo.Dns,
                    ["nicSpeed"] =
                    !string.IsNullOrWhiteSpace(nicInfo.NicSpeed) ? nicInfo.NicSpeed : string.Empty,
                    ["gateway"] = nicInfo.Gateway,
                    ["ipAddress"] = nicInfo.IpAddresses.Select(currIP => currIP.ToString()).ToList(),
                    ["deviceType"] = (int)nicInfo.DeviceType,
                    ["deviceName"] = nicInfo.DeviceName,
                    ["hostId"] = nicInfo.HostId,
                    ["netmask"] = nicInfo.NetMask.ToDictionary(curr => curr.NicIPAddress, curr => curr.SubNetMask)
                };
                networkDetails.Add(item.Key, nicDict);
            }

            string hostInput = MiscUtils.SerializeObjectToPHPFormat(hostDetails);
            string networkInput = MiscUtils.SerializeObjectToPHPFormat(networkDetails);

            Tracers.CSApi.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "RegisterPS hostInput : {0}, networkInput : {1}",
                    hostInput, networkInput);

            return (hostInput, networkInput);
        }

        // TODO-SanKumar-1903: This should go into Common, since other components can report this as well.
        public Task ReportInfraHealthFactorsAsync(IEnumerable<InfraHealthFactorItem> infraHealthFactors)
        {
            return ReportInfraHealthFactorsAsync(infraHealthFactors, CancellationToken.None);
        }
        public Task ReportInfraHealthFactorsAsync(IEnumerable<InfraHealthFactorItem> infraHealthFactors, CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            var updateDBItems = infraHealthFactors.Select(currItem =>
                {
                    if (!HealthUtils.InfraHealthFactorStrings.ContainsKey(currItem.InfraHealthFactor))
                        throw new NotSupportedException(currItem.InfraHealthFactor + " is not supported");

                    var currInfraHFCode = HealthUtils.InfraHealthFactorStrings[currItem.InfraHealthFactor].Item1;
                    var currInfraHFLevel = HealthUtils.InfraHealthFactorStrings[currItem.InfraHealthFactor].Item2;

                    if (currItem.Set)
                    {
                        const int priority = 0;
                        const string component = "Ps";
                        const string healthDescription = "";

                        string serializedPlaceholders;
                        using (MemoryStream ms = new MemoryStream())
                        {
                            m_phpFormatter.Serialize(ms, currItem.Placeholders);
                            ms.Seek(0, SeekOrigin.Begin);
                            using (StreamReader sr = new StreamReader(ms))
                            {
                                serializedPlaceholders = sr.ReadToEnd();
                            }
                        }

                        return new UpdateDBItem()
                        {
                            QueryType = "INSERT",
                            TableName = "infrastructurehealthfactors",
                            FieldNames = "hostId, healthFactorCode, healthFactor, priority, component, healthDescription, healthCreationTime, healthUpdateTime, placeHolders",
                            Condition = string.Format(
                                CultureInfo.InvariantCulture,
                                "'{0}', '{1}', '{2}', '{3}','{4}', '{5}', now(), now(), '{6}'",
                                m_hostId, currInfraHFCode, currInfraHFLevel, priority, component, healthDescription, serializedPlaceholders),
                            AdditionalInfo = string.Format(
                                CultureInfo.InvariantCulture,
                                "on duplicate key update healthUpdateTime= now(), placeHolders= '{0}'",
                                serializedPlaceholders)
                        };
                    }
                    else
                    {
                        return new UpdateDBItem()
                        {
                            QueryType = "DELETE",
                            TableName = "infrastructurehealthfactors",
                            Condition = string.Format(
                                CultureInfo.InvariantCulture,
                                "hostId= '{0}' AND healthFactorCode= '{1}'",
                                m_hostId, currInfraHFCode)
                        };
                    }
                });

            return UpdateDBAsync(new UpdateDBRequest(updateDBItems), cancellationToken);
        }

        /// <summary>
        /// Updates the job id in the db for given parameters
        /// </summary>
        /// <param name="jobIdUpdateItem">
        /// Object of JobIdUpdateItem, contains the required parameters to make the query
        /// </param>
        /// <returns>Task to update db</returns>
        public Task UpdateJobIdAsync(JobIdUpdateItem jobIdUpdateItem)
        {
            return UpdateJobIdAsync(jobIdUpdateItem, CancellationToken.None);
        }

        /// <summary>
        /// Updates the job id in the db for given parameters
        /// </summary>
        /// <param name="jobIdUpdateItem">
        /// Object of JobIdUpdateItem, contains the required parameters to make the query
        /// </param>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns>Task to update db</returns>
        public Task UpdateJobIdAsync(JobIdUpdateItem jobIdUpdateItem, CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            if(!jobIdUpdateItem.IsValid())
            {
                throw new ArgumentException(nameof(jobIdUpdateItem));
            }

            long GetNewJobId(long tmid)
            {
                const int randomNunberUpperLimit = 1000;
                return (DateTimeUtils.GetSecondsAfterEpoch()
                    + tmid + RandUtils.GetNextIntAsync(randomNunberUpperLimit, cancellationToken).GetAwaiter().GetResult());
            }

            (string ReplicationCleanupFlags, string WhereClause)
                GetUpdateJobIdQueryParams(long restartResyncCounter)
            {
                string replication_cleanup_flags = string.Empty;
                string where_clause = string.Empty;

                if (restartResyncCounter > 0)
                {
                    replication_cleanup_flags = @",replicationCleanupOptions = 0,replication_status = 0";
                    where_clause = @"and replication_status = 42 and replicationCleanUpOptions = 1 and restartResyncCounter > 0";
                }
                else
                {
                    where_clause = @"and replication_status = 0 and replicationCleanUpOptions = 0 and restartResyncCounter = 0";
                }

                return (replication_cleanup_flags, where_clause);
            }

            string replicationCleanupFlags = string.Empty;
            string whereClause = string.Empty;

            (replicationCleanupFlags, whereClause) = GetUpdateJobIdQueryParams(jobIdUpdateItem.restartResyncCounter);

            var updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                FieldNames = string.Format(CultureInfo.InvariantCulture, "jobId='{0}',lastResyncOffset=0,resyncStartTagtime=0 {1} ",
                GetNewJobId(jobIdUpdateItem.tmid), replicationCleanupFlags),
                Condition = string.Format(CultureInfo.InvariantCulture, "sourceHostId = '{0}' and sourceDeviceName='{1}' and " +
                    "destinationHostId = '{2}' and destinationDeviceName = '{3}' and jobId = '0' {4}", jobIdUpdateItem.srcHostId, jobIdUpdateItem.srcHostVolume,
                    jobIdUpdateItem.destHostId, jobIdUpdateItem.destHostVolume, whereClause)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), cancellationToken);
        }

        public Task SetResyncFlagAsync(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile)
        {
            return SetResyncFlagAsync(hostId, deviceName, destHostId, destDeviceName, resyncReasonCode, detectionTime,
                hardlinkFromFile, hardlinkToFile, CancellationToken.None);
        }

        public Task SetResyncFlagAsync(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile, CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                return XmlRequestBuilder(
                    request: new SetResyncFlagRequest(hostId, deviceName, destHostId, destDeviceName, resyncReasonCode, detectionTime,
                    hardlinkFromFile, hardlinkToFile),
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.MARK_RESYNC_REQUIRED,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.MARK_RESYNC_REQUIRED,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<int> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                return await ParseXMLResponseAsync<int>(
                    requestId: requestId,
                    response: response);
            };

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        public Task<string> GetRollbackStatusAsync(string sourceid)
        {
            return GetRollbackStatusAsync(sourceid, CancellationToken.None);
        }

        public Task<string> GetRollbackStatusAsync(string sourceid, CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                return XmlRequestBuilder(
                    request: new GetRollbackStatusRequest(sourceid),
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.IS_ROLLBACK_IN_PROGRESS,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.IS_ROLLBACK_IN_PROGRESS,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<string> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                var functionResponse = await ParseXMLResponseAsync<FunctionResponseElement>(
                    requestId: requestId,
                    response: response);

                //Todo: should we check for parameter name = rollbackStatus?
                return functionResponse.ParameterList.Single(
                    param => param.Name.Equals("rollbackStatus", StringComparison.OrdinalIgnoreCase)).Value;
            };

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        public Task<string> GetPsConfigurationAsync(string pairid)
        {
            return GetPsConfigurationAsync(pairid, CancellationToken.None);
        }

        public Task<string> GetPsConfigurationAsync(string pairid, CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                return XmlRequestBuilder(
                    request: new GetPSConfigurationRequest(pairid),
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.GET_DB_DATA,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.GET_DB_DATA,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<string> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                var functionResponse =  await ParseXMLResponseAsync<FunctionResponseElement>(
                    requestId: requestId,
                    response: response);

                // this is more strict than php implementation, which accepts multiple parameter elements
                // in the response, and takes the last one
                return functionResponse.ParameterList.Single(
                    param => param.Name.Equals("jobId", StringComparison.OrdinalIgnoreCase)).Value;
            };

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        public Task<ProcessServerSettings> GetPSSettings()
        {
            return GetPSSettings(CancellationToken.None);
        }

        public Task<ProcessServerSettings> GetPSSettings(CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                return XmlRequestBuilder(
                    new GetPSSettingsRequest(),
                    requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.GET_PS_SETTINGS,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.GET_PS_SETTINGS,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<ProcessServerSettings> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                var functionResponse =
                    await ParseXMLResponseAsync<FunctionResponseElement>(
                    requestId: requestId,
                    response: response);

                return new CSProcessServerSettings(
                    functionResponse,
                    m_csUri);
            }

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        public Task ReportStatisticsAsync(ProcessServerStatistics stats)
        {
            return ReportStatisticsAsync(stats, CancellationToken.None);
        }

        public Task ReportStatisticsAsync(ProcessServerStatistics stats, CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                var psBuildVersionString = m_psBuildVersionString.Value;

                return FormUrlEncodedRequestBuilder(
                    new Dictionary<string, ICSApiReqBuilder_TextWriter>
                    {
                        ["perf_params"] =
                            new UpdatePSStatisticsPerformanceRequest(psBuildVersionString, stats),

                        ["serv_params"] =
                            new UpdatePSStatisticsServicesRequest(psBuildVersionString, stats)
                    },
                    requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.UPDATE_PS_STATISTICS,
                    accessFileName: AccessFileName.UPDATE_PS_STATISTICS_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.UPDATE_PS_STATISTICS,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_FORM_URL_ENC,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            return PostAsync<int>(
                AccessFileName.UPDATE_PS_STATISTICS_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, responseParser: null);
        }

        public Task RegisterPSAsync(HostInfo hostInfo, Dictionary<string, NicInfo> networkInfo)
        {
            return RegisterPSAsync(hostInfo, networkInfo, CancellationToken.None);
        }

        public Task RegisterPSAsync(
            HostInfo hostInfo,
            Dictionary<string, NicInfo> networkInfo,
            CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                // TODO : Move the input validations and input serialization logic to a new class
                var (hostInput, networkInput) = BuildV2APSRegisterInput(hostInfo, networkInfo);

                return XmlRequestBuilder(
                    request: new RegisterProcessServerRequest(hostInput, networkInput),
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.REGISTER_HOST,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.REGISTER_HOST,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<int> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                return await ParseXMLResponseAsync<int>(
                    requestId: requestId,
                    response: response);
            };

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        public Task StopReplicationAtPSAsync(IStopReplicationInput stopReplicationInput)
        {
            return StopReplicationAtPSAsync(stopReplicationInput, CancellationToken.None);
        }

        public Task StopReplicationAtPSAsync(
            IStopReplicationInput stopReplicationInput,
            CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                var input = (StopReplicationInput)stopReplicationInput;
                return FormUrlEncodedRequestBuilder(
                    requestDict: input.StopReplicationPHPInput,
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.STOP_REPLICATION_PS,
                    accessFileName: AccessFileName.STOP_REPLICATION_PS_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.STOP_REPLICATION_PS,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_FORM_URL_ENC,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            return PostAsync<int>(
                AccessFileName.STOP_REPLICATION_PS_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, responseParser: null);
        }

        public Task ReportPSEventsAsync(IPSHealthEvent psHealthEvent)
        {
            return ReportPSEventsAsync(psHealthEvent, CancellationToken.None);
        }

        public Task ReportPSEventsAsync(IPSHealthEvent psHealthEvent, CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                var eventInput = (PSHealthEvent)psHealthEvent;

                return XmlRequestBuilder(
                    request: new ReportPSEventsRequest(eventInput.PHPSerializedEventInput),
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.ADD_ERROR_MESSAGE,
                    accessFileName: AccessFileName.CXAPI_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                CommonRequestHeadersEditor(
                    phpApiName: FunctionName.ADD_ERROR_MESSAGE,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    mediaType: MediaType.MEDIA_TYPE_XML_UTF8,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            async Task<int> ResponseParser(
                string requestId, HttpResponseMessage response, CancellationToken ct)
            {
                return await ParseXMLResponseAsync<int>(
                    requestId: requestId,
                    response: response);
            };

            return PostAsync(
                AccessFileName.CXAPI_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, ResponseParser);
        }

        public Task UploadFilesAsync(List<Tuple<string, string>> uploadFileInputItems)
        {
            return UploadFilesAsync(uploadFileInputItems, CancellationToken.None);
        }

        public Task UploadFilesAsync(List<Tuple<string, string>> uploadFileInputItems, CancellationToken cancellationToken)
        {
            (HttpContent RequestContent, string AccessSignature) RequestBuilder(
                string requestId, CancellationToken ct)
            {
                return MultipartFormDataRequestBuilder(
                    fileList: uploadFileInputItems,
                    requestId: requestId,
                    requestMethod: REQUEST_METHOD_POST,
                    functionName: FunctionName.UPLOAD_FILE,
                    accessFileName: AccessFileName.UPLOAD_FILE_PHP,
                    cancellationToken: ct);
            }

            void RequestHeadersEditor(
                string requestId, string accessSignature, HttpRequestHeaders httpRequestHeaders,
                HttpContentHeaders httpContentHeaders, CancellationToken ct)
            {
                MultipartFormRequestHeadersEditor(
                    phpApiName: FunctionName.UPLOAD_FILE,
                    requestId: requestId,
                    accessSignature: accessSignature,
                    httpRequestHeaders: httpRequestHeaders,
                    httpContentHeaders: httpContentHeaders);
            }

            return PostAsync<bool>(
                AccessFileName.UPLOAD_FILE_PHP,
                cancellationToken, RequestBuilder, RequestHeadersEditor, responseParser: null);
        }

        public Task UpdateReplicationStatusForDeleteReplicationAsync(string pairId)
        {
            return UpdateReplicationStatusForDeleteReplicationAsync(pairId, CancellationToken.None);
        }

        public Task UpdateReplicationStatusForDeleteReplicationAsync(string pairId, CancellationToken cancellationToken)
        {
            ThrowIfDisposed();

            if (string.IsNullOrWhiteSpace(pairId))
                throw new ArgumentNullException(nameof(pairId));

            const int SRC_DEL_DONE = 30;
            const int PS_DEL_DONE = 31;

            var updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                FieldNames = string.Format(
                    CultureInfo.InvariantCulture, "replication_status='{0}'", PS_DEL_DONE),
                Condition = string.Format(
                                CultureInfo.InvariantCulture,
                                "replication_status = '{0}' AND processServerId = '{1}' AND pairId = '{2}'",
                                SRC_DEL_DONE, m_hostId, pairId)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), cancellationToken);
        }

        public Task UpdateCumulativeThrottleAsync(bool isThrottled, SystemHealth systemHealth)
        {
            return UpdateCumulativeThrottleAsync(isThrottled, systemHealth, CancellationToken.None);
        }

        public Task UpdateCumulativeThrottleAsync(bool isThrottled, SystemHealth systemHealth, CancellationToken ct)
        {
            ThrowIfDisposed();
            List<UpdateDBItem> updateDBItemsList = new List<UpdateDBItem>();
            UpdateDBItem updateDBItem = null;

            if (isThrottled && systemHealth == SystemHealth.healthy)
            {
                updateDBItem = new UpdateDBItem()
                {
                    QueryType = UpdateDbQueryType.Update,
                    TableName = UpdateDbTableNames.SystemHealth,
                    FieldNames = string.Format(
                        CultureInfo.InvariantCulture,
                        "healthFlag='degraded', startTime=now()"),
                    Condition = string.Format(
                        CultureInfo.InvariantCulture,
                        "component='Space Constraint'")
                };
                updateDBItemsList.Add(updateDBItem);
            }
            else if(!isThrottled && systemHealth == SystemHealth.degraded)
            {
                updateDBItem = new UpdateDBItem()
                {
                    QueryType = UpdateDbQueryType.Update,
                    TableName = UpdateDbTableNames.SystemHealth,
                    FieldNames = string.Format(
                        CultureInfo.InvariantCulture,
                        "healthFlag='healthy', startTime=now()"),
                    Condition = string.Format(
                        CultureInfo.InvariantCulture,
                        "component='Space Constraint'")
                };
                updateDBItemsList.Add(updateDBItem);
            }

            int throttleCount = isThrottled ? 1 : 0;
            updateDBItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.ProcessServer,
                FieldNames = string.Format(
                        CultureInfo.InvariantCulture,
                        "cummulativeThrottle={0}",
                        throttleCount),
                Condition = string.Format(
                        CultureInfo.InvariantCulture,
                        "processServerId='{0}'",
                        AmethystConfig.Default.HostGuid)
            };
            updateDBItemsList.Add(updateDBItem);

            return UpdateDBAsync(new UpdateDBRequest(updateDBItemsList), ct);
        }

        public Task UpdateCertExpiryInfoAsync(Dictionary<string, long> certExpiryInfo)
        {
            return UpdateCertExpiryInfoAsync(certExpiryInfo, CancellationToken.None);
        }

        public Task UpdateCertExpiryInfoAsync(Dictionary<string, long> certExpiryInfo, CancellationToken ct)
        {
            ThrowIfDisposed();

            if (certExpiryInfo == null || !certExpiryInfo.Any())
                throw new ArgumentNullException(nameof(certExpiryInfo));

            var serializedCertExpiryInfo = MiscUtils.SerializeObjectToPHPFormat(certExpiryInfo);

            if (string.IsNullOrWhiteSpace(serializedCertExpiryInfo))
                throw new ArgumentNullException(nameof(serializedCertExpiryInfo));

            var updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.ProcessServer,
                FieldNames = string.Format(
                    CultureInfo.InvariantCulture, "psCertExpiryDetails='{0}'", serializedCertExpiryInfo),
                Condition = string.Format(
                                CultureInfo.InvariantCulture, "processServerId = '{0}'", m_hostId)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), ct);
        }

        public Task ReportDataUploadBlockedAsync(DataUploadBlockedUpdateItem dataUploadBlockedUpdateItem)
        {
            return ReportDataUploadBlockedAsync(dataUploadBlockedUpdateItem, CancellationToken.None);
        }

        public Task ReportDataUploadBlockedAsync(DataUploadBlockedUpdateItem dataUploadBlockedUpdateItem, CancellationToken ct)
        {
            ThrowIfDisposed();

            if (!dataUploadBlockedUpdateItem.IsValid())
                throw new ArgumentException($"The argument {nameof(dataUploadBlockedUpdateItem)} is not valid");

            var updateDbItemList = dataUploadBlockedUpdateItem.UploadStatusForRepItems.Select(item =>
            {
                var updateDBItem = new UpdateDBItem()
                {
                    QueryType = UpdateDbQueryType.Update,
                    TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                    FieldNames = string.Format(CultureInfo.InvariantCulture,
                                            "isCommunicationfromSource = '{0}'",
                                            item.Value),
                    Condition = string.Format(CultureInfo.InvariantCulture,
                                            "pairId = '{0}'",
                                            item.Key)
                };
                return updateDBItem;
            });

            return UpdateDBAsync(new UpdateDBRequest(updateDbItemList), ct);
        }

        public Task UpdateDiffThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> diffThrottleInfo)
        {
            return UpdateDiffThrottleAsync(diffThrottleInfo, CancellationToken.None);
        }

        public Task UpdateDiffThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> diffThrottleInfo, CancellationToken token)
        {
            ThrowIfDisposed();

            if (!diffThrottleInfo.Any())
                throw new ArgumentNullException(nameof(diffThrottleInfo));

            List<UpdateDBItem> updateDBItemList = new List<UpdateDBItem>();
            foreach(var item in diffThrottleInfo)
            {
                try
                {
                    if (item.Key <= 0)
                    {
                        throw new ArgumentException($"PairId {item.Key} should be greater than zero");
                    }

                    if (item.Value.Item1 != ThrottleState.NotThrottled &&
                    item.Value.Item1 != ThrottleState.Throttled)
                    {
                        throw new ArgumentException($"The throttle state {item.Value.Item1} should be either " +
                        $"{ThrottleState.NotThrottled} or {ThrottleState.Throttled}");
                    }

                    if (item.Value.Item2 < 0)
                    {
                        throw new ArgumentException($"Pending diff data {item.Value.Item2}" +
                            $"should be greater than or equal to 0");
                    }

                    updateDBItemList.Add(new UpdateDBItem()
                    {
                        QueryType = UpdateDbQueryType.Update,
                        TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                        FieldNames = string.Format(CultureInfo.InvariantCulture,
                                            "throttleDifferentials = {0}, remainingDifferentialBytes = {1}, remainingQuasiDiffBytes = {2}",
                                            (int)item.Value.Item1, item.Value.Item2, item.Value.Item2),
                        Condition = string.Format(CultureInfo.InvariantCulture,
                                            "pairId = '{0}'",
                                            item.Key)
                    });
                }
                catch(Exception ex)
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to validate diff throttle information for pairid {0} with exception{1}{2}",
                        item.Key, Environment.NewLine, ex);
                }
            }

            if (updateDBItemList.Count > 0)
            {
                return UpdateDBAsync(new UpdateDBRequest(updateDBItemList), token);
            }
            else return Task.CompletedTask;
        }

        public Task UpdateResyncThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> resyncThrottleInfo)
        {
            return UpdateResyncThrottleAsync(resyncThrottleInfo, CancellationToken.None);
        }

        public Task UpdateResyncThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> resyncThrottleInfo, CancellationToken token)
        {
            ThrowIfDisposed();

            if (!resyncThrottleInfo.Any())
                throw new ArgumentNullException(nameof(resyncThrottleInfo));

            var updateDBItemList = new List<UpdateDBItem>();
            foreach(var item in resyncThrottleInfo)
            {
                try
                {
                    if (item.Key <= 0)
                    {
                        throw new ArgumentException($"PairId {item.Key} should be greater than zero");
                    }

                    if (item.Value.Item1 != ThrottleState.NotThrottled &&
                    item.Value.Item1 != ThrottleState.Throttled)
                    {
                        throw new ArgumentException($"The throttle state {item.Value.Item1} should be either " +
                        $"{ThrottleState.NotThrottled} or {ThrottleState.Throttled}");
                    }

                    if (item.Value.Item2 < 0)
                    {
                        throw new ArgumentException($"Pending resync data {item.Value.Item2}" +
                            $"should be greater than or equal to 0");
                    }

                    updateDBItemList.Add(new UpdateDBItem()
                    {
                        QueryType = UpdateDbQueryType.Update,
                        TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                        FieldNames = string.Format(CultureInfo.InvariantCulture,
                                        "throttleresync = {0}, remainingResyncBytes = {1}",
                                        (int)item.Value.Item1, item.Value.Item2),
                        Condition = string.Format(CultureInfo.InvariantCulture,
                                        "pairId = '{0}'",
                                        item.Key)
                    });
                }
                catch(Exception ex)
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to validate resync throttle information for pairid {0} with exception{1}{2}",
                        item.Key, Environment.NewLine, ex);
                }
            }

            if (updateDBItemList.Count > 0)
            {
                return UpdateDBAsync(new UpdateDBRequest(updateDBItemList), token);
            }
            else return Task.CompletedTask;
        }

        public Task UpdateIRDRStuckHealthAsync(List<IRDRStuckHealthEvent> iRDRStuckHealthEventList)
        {
            return UpdateIRDRStuckHealthAsync(iRDRStuckHealthEventList, CancellationToken.None);
        }

        public Task UpdateIRDRStuckHealthAsync(List<IRDRStuckHealthEvent> iRDRStuckHealthEventList, CancellationToken ct)
        {
            ThrowIfDisposed();

            const string COMPONENT = "PS";
            const string HEALTH_FACTOR_TYPE = "DISK";

            List<UpdateDBItem> updateDBItemList = new List<UpdateDBItem>();
            foreach (var iRDRStuckHealthEvent in iRDRStuckHealthEventList)
            {
                var pair = iRDRStuckHealthEvent.Pair;
                try
                {
                    if (!iRDRStuckHealthEvent.IsValid())
                    {
                        throw new ArgumentException(nameof(iRDRStuckHealthEvent));
                    }

                    if (iRDRStuckHealthEvent.Health == IRDRStuckHealthFactor.Warning ||
                        iRDRStuckHealthEvent.Health == IRDRStuckHealthFactor.Critical)
                    {
                        updateDBItemList.Add(new UpdateDBItem()
                        {
                            QueryType = UpdateDbQueryType.Replace,
                            TableName = UpdateDbTableNames.HealthFactors,
                            FieldNames = string.Format(CultureInfo.InvariantCulture,
                                                    "sourceHostId, destinationHostId, pairId, errCode, healthFactorCode," +
                                                    " healthFactor, priority, component, healthDescription, healthTime, healthFactorType"),
                            Condition = string.Format(
                                CultureInfo.InvariantCulture,
                                "'{0}', '{1}', '{2}', '{3}', '{4}', '{5}', '{6}', '{7}', '{8}', now(), '{9}'",
                                pair.HostId,
                                pair.TargetHostId,
                                pair.PairId,
                                iRDRStuckHealthEvent.ErrorCode,
                                iRDRStuckHealthEvent.HealthFactorCode,
                                iRDRStuckHealthEvent.HealthFactor,
                                iRDRStuckHealthEvent.Priority,
                                COMPONENT,
                                iRDRStuckHealthEvent.HealthDescription,
                                HEALTH_FACTOR_TYPE)
                        });
                    }
                    else if(iRDRStuckHealthEvent.Health == IRDRStuckHealthFactor.Healthy)
                    {
                        updateDBItemList.Add(new UpdateDBItem()
                        {
                            QueryType = UpdateDbQueryType.Delete,
                            TableName = UpdateDbTableNames.HealthFactors,
                            FieldNames = string.Format(CultureInfo.InvariantCulture,
                            string.Empty),
                            Condition = string.Format(CultureInfo.InvariantCulture,
                            "sourceHostId= '{0}' AND pairId= '{1}' AND errCode IN ('{2}', '{3}')",
                            pair.HostId,
                            pair.PairId,
                            IRDRFileStuckHealthErrorCodes.IRDRStuckWarningCode,
                            IRDRFileStuckHealthErrorCodes.IRDRStuckCriticalCode)
                        });
                    }
                }
                catch (Exception ex)
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to check IR and DR stuck files for pairid {0} with exception{1}{2}",
                        pair.PairId, Environment.NewLine, ex);
                }
            }

            if (updateDBItemList.Count > 0)
            {
                return UpdateDBAsync(new UpdateDBRequest(updateDBItemList), ct);
            }
            else return Task.CompletedTask;
        }

        public Task UpdateRPOAsync(Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo)
        {
            return UpdateRPOAsync(RPOInfo, CancellationToken.None);
        }

        public Task UpdateRPOAsync(Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo, CancellationToken ct)
        {
            ThrowIfDisposed();

            if (!RPOInfo.Any())
            {
                throw new ArgumentNullException(nameof(UpdateRPOAsync));
            }

            var updateDBItemList = new List<UpdateDBItem>();
            foreach (var item in RPOInfo)
            {
                try
                {
                    if (item.Key == null)
                        throw new ArgumentNullException(nameof(item.Key), "ProcessServerPairSettings is null");

                    if (item.Value == TimeSpan.MinValue || item.Value == TimeSpan.MaxValue)
                        throw new ArgumentException("RPO value is invalid");

                    IProcessServerPairSettings pair = item.Key;
                    string rpoTime = String.Empty;

                    if (pair.JobId == 0 && pair.ReplicationStatus == 0)
                    {
                        // Initial status when the pair is newly added
                        rpoTime = "pairConfigureTime";
                    }
                    else
                    {

                        // CS currently expects RPO as datetime rather than duration
                        // So send the RPO time as now() - actualRPO for db update
                        rpoTime = $"FROM_UNIXTIME(UNIX_TIMESTAMP(now()) - {(long)item.Value.TotalSeconds})";
                    }

                    updateDBItemList.Add(new UpdateDBItem()
                    {
                        QueryType = UpdateDbQueryType.Update,
                        TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                        FieldNames = string.Format(
                        CultureInfo.InvariantCulture,
                        "currentRPOTime = {0}, statusUpdateTime = now()",
                        rpoTime),
                        Condition = string.Format(
                        CultureInfo.InvariantCulture,
                        "pairId = '{0}'",
                        pair.PairId)
                    });
                }
                catch (Exception ex)
                {
                    Tracers.CSApi.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to validate RPO information for pairid {0} with exception{1}{2}",
                        item.Key.PairId, Environment.NewLine, ex);
                }
            }

            if (updateDBItemList.Count > 0)
            {
                return UpdateDBAsync(new UpdateDBRequest(updateDBItemList), ct);
            }
            else return Task.CompletedTask;
        }

         public Task UpdateExecutionStateAsync(string pairId, ExecutionState executionState)
        {
            return UpdateExecutionStateAsync(pairId, executionState, CancellationToken.None);
        }

        public Task UpdateExecutionStateAsync(string pairId, ExecutionState executionState, CancellationToken ct)
        {
            ThrowIfDisposed();

            if (string.IsNullOrWhiteSpace(pairId))
                throw new ArgumentNullException(nameof(pairId));

            // Delay Auto Resync for pairs whose resync Order is not 1.
            // Update executionState to 4(Delay Resync)
            var updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                FieldNames = string.Format(
                    CultureInfo.InvariantCulture, "executionState={0}", (int)executionState),
                Condition = string.Format(CultureInfo.InvariantCulture, "pairId = '{0}'", pairId)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), ct);
        }

        public Task PauseReplicationAsync(string pairId)
        {
            return PauseReplicationAsync(pairId, CancellationToken.None);
        }

        public Task PauseReplicationAsync(string pairId, CancellationToken ct)
        {
            ThrowIfDisposed();

            // TODO: Revisit later to define enum with all possible replication_status values for the pair
            const int PAIR_PAUSE_PENDING = 41;
            const string PAIR_PAUSE_BY_USER = "user";

            if (string.IsNullOrWhiteSpace(pairId))
                throw new ArgumentNullException(nameof(pairId));

            // For the given pair,
            // If target replication status and source replication status is in non paused state
            // and the pair is not deleted then
            //  1. Mark the pair to pause pending state
            //  2. Set the target replication status to paused
            //  3. Mark the pair as paused by 'user'
            //  4. Mark autoResme as not set
            //  5. Mark restart pause which finally goes to restart resync
            UpdateDBItem updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.SrcLogicalVolumeDestLogicalVolume,
                FieldNames = string.Format(
                    CultureInfo.InvariantCulture,
                    "replication_status = {0}, tar_replication_status = {1}, pauseActivity = '{2}', autoResume = {3}, restartPause = {4}",
                    PAIR_PAUSE_PENDING,
                    (int)TargetReplicationStatus.Paused,
                    PAIR_PAUSE_BY_USER,
                    (int)PairAutoResumeStatus.NotSet,
                    (int)PairPauseStatus.Restart),
                Condition = string.Format(
                    CultureInfo.InvariantCulture,
                    "pairId = '{0}' and tar_replication_status = {1} and src_replication_status = {2} and deleted = {3}",
                    pairId,
                    (int)TargetReplicationStatus.NonPaused,
                    (int)TargetReplicationStatus.NonPaused,
                    (int)PairDeletionStatus.NotDeleted)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), ct);
        }

        public Task UpdatePolicyInfoForRenewCertsAsync(RenewCertsPolicyInfo renewCertsPolicyInfo)
        {
            return UpdatePolicyInfoForRenewCertsAsync(renewCertsPolicyInfo, CancellationToken.None);
        }

        public Task UpdatePolicyInfoForRenewCertsAsync(RenewCertsPolicyInfo renewCertsPolicyInfo, CancellationToken ct)
        {
            ThrowIfDisposed();

            if (!renewCertsPolicyInfo.IsValid())
            {
                throw new ArgumentException(
                        $"{nameof(renewCertsPolicyInfo)} contains invalid properties");
            }

            var policyState = (int)renewCertsPolicyInfo.PolicyState;
            var certRenewStatus = renewCertsPolicyInfo.renewCertsStatus;

            var serCertRenewalError = string.Empty;
            if (certRenewStatus != null)
            {
                serCertRenewalError = MiscUtils.SerializeObjectToPHPFormat(
                    new Dictionary<string, object>
                    {
                        ["ErrorDescription"] = certRenewStatus.ErrorDescription,
                        ["ErrorCode"] = certRenewStatus.ErrorCode,
                        ["PlaceHolders"] = certRenewStatus.PlaceHolders,
                        ["LogMessage"] = certRenewStatus.LogMessage
                    });
            }

            var updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.PolicyInstance,
                FieldNames = string.Format(
                    CultureInfo.InvariantCulture,
                    "policyState = '{0}', executionLog = '{1}'",
                    policyState, serCertRenewalError),
                Condition = string.Format(
                                CultureInfo.InvariantCulture,
                                "hostId = '{0}' AND policyId = '{1}' AND policyState = '{2}'",
                                m_hostId, renewCertsPolicyInfo.PolicyId, (int)PolicyState.Pending)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), ct);
        }

        public Task UpdateLogRotationInfoAsync(string logName, long logRotTriggerTime)
        {
            return UpdateLogRotationInfoAsync(logName, logRotTriggerTime, CancellationToken.None);
        }

        public Task UpdateLogRotationInfoAsync(string logName, long logRotTriggerTime, CancellationToken ct)
        {
            ThrowIfDisposed();

            if (string.IsNullOrWhiteSpace(logName))
                throw new ArgumentNullException(nameof(logName));

            if (logRotTriggerTime <= 0)
                throw new ArgumentNullException(nameof(logRotTriggerTime));

            UpdateDBItem updateDbItem = new UpdateDBItem()
            {
                QueryType = UpdateDbQueryType.Update,
                TableName = UpdateDbTableNames.LogRotationList,
                FieldNames = string.Format(
                        CultureInfo.InvariantCulture,
                        "startTime = ADDDATE(now(), {0})", logRotTriggerTime),
                Condition = string.Format(
                    CultureInfo.InvariantCulture,
                    "logName = '{0}'",
                    logName)
            };

            return UpdateDBAsync(new UpdateDBRequest(new List<UpdateDBItem>() { updateDbItem }), ct);
        }

        public override string ToString()
        {
            if (m_restWrapper != null)
            {
                return string.Format(
                    CultureInfo.InvariantCulture,
                    "ProcessServerCSApiStubsImpl - ({0})",
                    m_restWrapper);
            }

            return "ProcessServerCSApiStubsImpl";
        }

        public void Close()
        {
            Dispose();
        }

        #region Dispose Pattern

        ~ProcessServerCSApiStubsImpl()
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
                m_restWrapper.Dispose();

                m_csFingerprintFileWatcher.Dispose();
            }

            // Dispose unmanaged resources
        }

        public Task ReportPSEventsAsync(ApplianceHealthEventMsg psEvents, CancellationToken token)
        {
            throw new NotImplementedException();
        }

        public Task ReportHostHealthAsync(bool IsPrivateEndpointEnabled, IProcessServerHostSettings host,
            ProcessServerProtectionPairHealthMsg hostHealth,
            CancellationToken cancellationToken)
        {
            throw new NotImplementedException();
        }

        public Task SendMonitoringMessageAsync(SendMonitoringMessageInput input, CancellationToken cancellationToken)
        {
            throw new NotImplementedException();
        }

        #endregion Dispose Pattern
    }
}
