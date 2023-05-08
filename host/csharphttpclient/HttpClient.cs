using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.IO;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Security.Authentication;

namespace HttpCommunication
{
    public class HttpReply
    {       
        public HttpReply()
        {
            // As per the spec HTTP headers are case insensitive, so ignoring case while comparing
            headers = new List<KeyValuePair<string, string>>();
        }
        public string HttpStatusCode;              // holds HTTP status code
        public string status;                      // full reply http status line
        public List<KeyValuePair<string, string>> headers; // array of all response headers
        public string data;                        // holds content data returned by server.
        public string error;                       // empty if no errors otherwis error message        
    }

    public class HttpClient
    {
        public const string HeaderHttpAuthSignature = "HTTP-AUTH-SIGNATURE";
        public const string HeaderHttpAuthRequestId = "HTTP-AUTH-REQUESTID";
        public const string HeaderHttpAuthPHPAPIName = "HTTP-AUTH-PHPAPI-NAME";
        public const string HeaderHttpAuthVersion = "HTTP-AUTH-VERSION";

        public const string HeaderHttpTransferEncoding = "Transfer-Encoding";

        private string ipAddress;
        private int port;
        private bool secure;
        private int timeout;
        private string fingerprintCache;
        private string fingerprintDir;

        // directory that holds passphrase. Set to null if using default location
        public string PassphraseDir { get; set; }

        public HttpClient(string serverIp,       // server ip address
                          int serverPort,        // server port
                          bool useSsl,           // inidcates if secure or non-secure. true: secure, false: non secure
                          int timeOut,           // set socket timeout                          
                          string fingerprintDir) // directory that holds fingerprints. Set to null if using default location
        {
            ipAddress = serverIp;
            port = serverPort;
            secure = useSsl;
            timeout = timeOut;         
            this.fingerprintDir = fingerprintDir;
            this.fingerprintCache = CXSecurityInfo.ReadFingerprint(fingerprintDir, serverIp, serverPort);
        }

        public HttpReply sendHttpRequest(string method,
                                         string url,
                                         string content,
                                         string contentType,
                                         Dictionary<string, string> httpHeaders)
        {
            TcpClient client = new TcpClient();
            client.ReceiveTimeout = timeout;
            client.SendTimeout = timeout;

            client.Connect(ipAddress, port);
            TextWriter writer;
            BinaryReader reader;
            if (secure)
            {
                SslStream ssl = new SslStream(client.GetStream(),
                                              false,
                                              new RemoteCertificateValidationCallback(CertificateValidationCallback));

                X509CertificateCollection collection = new X509CertificateCollection();

                ssl.AuthenticateAsClient(ipAddress, collection, SslProtocols.Tls | SslProtocols.Ssl3 | SslProtocols.Tls11 | SslProtocols.Tls12, false);

                writer = new StreamWriter(ssl);
                reader = new BinaryReader(ssl);
            }
            else
            {
                NetworkStream stream = client.GetStream();
                writer = new StreamWriter(stream);
                reader = new BinaryReader(stream);
            }

            string httpRequest = method + " " + url + " HTTP/1.1 \r\n"
                + "Host: " + ipAddress + "\r\n"
                + "Connection: close \r\n";            
            string requestId = null;
            string version = null;
            string[] apiNames = null;
            string accessSignature = null;
            Uri uri = new Uri(String.Format("{0}://{1}:{2}{3}", secure ? "https" : "http", ipAddress, port, url));
            if (httpHeaders != null)
            {
                foreach (string key in httpHeaders.Keys)
                {
                    httpRequest += key + ":" + httpHeaders[key] + "\r\n";
                    if(String.Compare(key, HeaderHttpAuthRequestId, StringComparison.OrdinalIgnoreCase) == 0)
                    {
                        requestId = httpHeaders[key];
                    }
                    else if (String.Compare(key, HeaderHttpAuthVersion, StringComparison.OrdinalIgnoreCase) == 0)
                    {
                        version = httpHeaders[key];
                    }
                    else if (String.Compare(key, HeaderHttpAuthPHPAPIName, StringComparison.OrdinalIgnoreCase) == 0)
                    {
                        apiNames = httpHeaders[key].Split(new char[]{','});
                    }
                    else if (String.Compare(key, HeaderHttpAuthSignature, StringComparison.OrdinalIgnoreCase) == 0)
                    {
                        accessSignature = httpHeaders[key];
                    }
                }
            }
            if(String.IsNullOrEmpty(requestId))
            {
                requestId = CXHelper.GenerateRequestId();
                httpRequest += HeaderHttpAuthRequestId + ":" + requestId + "\r\n";
            }
            if(String.IsNullOrEmpty(version))
            {
                version = CXHelper.RequestVersion;
                httpRequest += HeaderHttpAuthVersion + ":" + version + "\r\n";
            }
            if(apiNames == null || apiNames.Length == 0)
            {
                apiNames = new string[] { uri.AbsolutePath };                
                httpRequest += HeaderHttpAuthPHPAPIName + ":" + uri.AbsolutePath + "\r\n";
            }
            if (String.IsNullOrEmpty(accessSignature))
            {
                CXSignature signature = new CXSignature(
                    CXSecurityInfo.GetAccessKeyID(),
                    CXSecurityInfo.ReadPassphrase(PassphraseDir),
                    version, 
                    CXSecurityInfo.ReadFingerprint(fingerprintDir, uri.Host, uri.Port));
                httpRequest += HeaderHttpAuthSignature + ":" + signature.ComputeSignatureComponentAuth(requestId, apiNames, uri, method) + "\r\n";
            }
            if (!String.IsNullOrEmpty(content))
            {
                byte[] contentBytes = Encoding.UTF8.GetBytes(content);
                httpRequest += "Content-Length:" + contentBytes.Length + "\r\nContent-Type:" + contentType + "\r\n\r\n" + content;
            }
            else
            {
                httpRequest += "\r\n";
            }
            writer.WriteLine(httpRequest);
            writer.Flush();
            HttpReply reply = processReply(reader);
            client.Close();
            return reply;
        }

        private string removeAll(string s, char c)
        {
            string dest = "";
            foreach (char ch in s)
            {
                if (c != ch)
                {
                    dest += ch;
                }
            }
            return dest;
        }

        private bool fingerprintMatch(string fingerprint, string certFingerprint)
        {
            fingerprint = removeAll(fingerprint, ':'); // make sure to remove any ':'
            certFingerprint = removeAll(certFingerprint, ':'); // make sure to remove any ':'
            return (fingerprint.ToLower() == certFingerprint.ToLower());
        }

        private HttpReply processReply(BinaryReader reader)
        {
            HttpReply reply = new HttpReply();
            const int BufferSize = 8192;
            byte[] buffer = new byte[BufferSize];
            string line = ReadLine(reader, ref buffer);
            string[] status = line.Split(' ');
            reply.HttpStatusCode = status[1];
            if ("200" != status[1])
            {                
                reply.error = line;
                return reply;
            }
            string currentHeaderName = null;
            string currentHeaderValue = null;
            do
            {
                // read headers
                line = ReadLine(reader, ref buffer);
                if (String.IsNullOrEmpty(line))
                {
                    break;
                }
                if (' ' == line[0])
                {
                    // Enters this block when the current header is spanned with multi lines.
                    if (!String.IsNullOrEmpty(currentHeaderName))
                    {
                        currentHeaderValue += line.Trim();
                    }
                }
                else
                {
                    if (currentHeaderName != null)
                    {
                        // Found new header so add the current header to the reply
                        reply.headers.Add(new KeyValuePair<string, string>(currentHeaderName, currentHeaderValue));
                        currentHeaderName = currentHeaderValue = null;
                    }

                    int idx = line.IndexOf(":");
                    if (-1 == idx)
                    {
                        currentHeaderName = currentHeaderValue = null;
                    }
                    else
                    {
                        currentHeaderName = line.Substring(0, idx);
                        currentHeaderValue = line.Substring(idx + 1).Trim();
                    }
                }
            } while (true);

            if (currentHeaderName != null)
            {
                // Adding the last header to the reply
                reply.headers.Add(new KeyValuePair<string, string>(currentHeaderName, currentHeaderValue));
            }

            string encodingHeaderValue = reply.headers.Find(key => key.Key.Equals(HeaderHttpTransferEncoding, StringComparison.InvariantCultureIgnoreCase)).Value;
            if (!String.IsNullOrEmpty(encodingHeaderValue) && String.Equals(encodingHeaderValue, "chunked", StringComparison.InvariantCultureIgnoreCase))
            {
                // If the header has Transfer-encoding: chunked then decode the chunked response.
                reply.data = DecodeChunkedResponse(reader);
            }
            else
            {                
                reply.data = ReadToEnd(reader);
            }
            return reply;
        }

        /*
         * Description: Reads all bytes from the binary stream and converts it to string
         * 
         * Returns: string
         */
        public static string ReadToEnd(BinaryReader reader)
        {
            const int BufferSize = 4096;
            using (MemoryStream memoryStream = new MemoryStream())
            {
                byte[] buffer = new byte[BufferSize];
                int bytesRead = 0;
                while (0 != (bytesRead = reader.Read(buffer, 0, buffer.Length)))
                {
                    memoryStream.Write(buffer, 0, bytesRead);
                }

                return memoryStream.Length > 0 ? Encoding.UTF8.GetString(memoryStream.ToArray()) : null;
            }
        }

        /*
         * Description: Reads a line from the binary stream
         * 
         * Returns: string if new line is found
         *          null if new line is not found
         */
        private string ReadLine(BinaryReader reader, ref byte[] buffer)
        {
            byte value1 = 0;
            byte value2 = 0;
            bool foundNewLine = false;
            int byteCount = 0;
            byte[] newLine = Encoding.UTF8.GetBytes("\r\n");
            if (null != buffer)
            {
                int bufferSize = buffer.Length;
                try
                {
                    do
                    {
                        value1 = reader.ReadByte();
                        if (value1 == newLine[0])
                        {
                            value2 = reader.ReadByte();
                            if (value2 == newLine[1])
                            {
                                foundNewLine = true;
                                break;
                            }
                            buffer[byteCount++] = (byte)value1;
                            if (byteCount >= bufferSize)
                            {
                                break;
                            }
                            buffer[byteCount++] = (byte)value2;
                        }
                        else
                        {
                            buffer[byteCount++] = (byte)value1;
                        }
                    } while (byteCount < bufferSize);
                }
                catch (EndOfStreamException)
                {
                }

                for (int i = byteCount; i < bufferSize; i++)
                {
                    buffer[i] = 0;
                }
            }
            string line = null;
            if (foundNewLine)
            {
                line = byteCount > 0 ? Encoding.UTF8.GetString(buffer, 0, byteCount) : String.Empty;
            }

            return line;
        }

        /**
         * Description: Decodes chunks of response received using HTTP 1.1 version (this parsing is not required for HTTP 1.0 version)
         * Returns: string
         */
        private string DecodeChunkedResponse(BinaryReader reader)
        {
            string actualContent = null;
            string chunkLengthHex = "";
            int chunkLength = 0;
            string[] tokens;
            byte[] newLine = Encoding.UTF8.GetBytes("\r\n");
            int currentIndex = 0;

            using (MemoryStream memoryStream = new MemoryStream())
            {
                const int BufferSize = 4096;
                byte[] buffer = new byte[BufferSize];
                do
                {
                    // a. To extract the Chunk length (in hexadecimal) read bytes until new line is reached.
                    chunkLengthHex = ReadLine(reader, ref buffer);
                    if (String.IsNullOrEmpty(chunkLengthHex))
                    {
                        throw new HttpClientException("Failed to decode chunked response due to invalid chunk length format");
                    }
                    currentIndex += (chunkLengthHex.Length + 2); // including new line

                    // b. Separate chunk length and chunk extension (Format: <chunk-length>[;<chunk-extension>]\r\n)
                    // In this format <chunk-extension> is optional and has name value fields separated using '='(Ex: name1=value1;name2=value2) 
                    tokens = chunkLengthHex.Split(new char[] { ';' });
                    if (null != tokens && tokens.Length > 0)
                    {
                        chunkLengthHex = tokens[0];
                    }

                    try
                    {
                        // c. Convert length of chunk in hexa decimal to integer value
                        chunkLength = Convert.ToInt32(chunkLengthHex, 16);
                    }
                    catch (ArgumentOutOfRangeException e)
                    {
                        throw new HttpClientException(String.Format("Failed to decode chunked response: chunk length {0} is not a hexadecimal value", chunkLengthHex), e);
                    }
                    catch (FormatException e)
                    {
                        throw new HttpClientException(String.Format("Failed to decode chunked response: chunk length {0} is not a hexadecimal value", chunkLengthHex), e);
                    }

                    // d. If chunk length is zero then come out of the loop as it indicates the end of the response.
                    if (chunkLength == 0)
                    {
                        break;
                    }

                    // e. If chunk length is non-zero, read bytes of chunk of length 'chunkLength'                                       
                    int countOfBytesRead = 0;
                    int bytesToRead = chunkLength < BufferSize ? chunkLength : BufferSize;
                    int totalBytesRead = 0;
                    int remainingBytes = 0;
                    do
                    {
                        countOfBytesRead = reader.Read(buffer, 0, bytesToRead);
                        if(0 == countOfBytesRead)
                        {
                            throw new HttpClientException("Failed to decode chunked response: expecting more bytes to read, but got EOF");
                        }
                        memoryStream.Write(buffer, 0, countOfBytesRead);
                        totalBytesRead += countOfBytesRead;
                        currentIndex += countOfBytesRead;
                        remainingBytes = (chunkLength - totalBytesRead);
                        bytesToRead = remainingBytes < BufferSize ? remainingBytes : BufferSize;
                    } while (totalBytesRead < chunkLength);
                    
                    try
                    {
                        // f. Read and skip the new line at the end of each chunk.
                        if (reader.ReadByte() != newLine[0] || reader.ReadByte() != newLine[1])
                        {
                            throw new HttpClientException("Failed to decode chunk response: expected '\r\n' not found at index " + currentIndex);
                        }
                        currentIndex += 2;
                    }
                    catch (EndOfStreamException e)
                    {
                        throw new HttpClientException("Failed to decode chunk response: expected '\r\n' not found at index " + currentIndex, e);
                    }

                } while (true);

                actualContent = Encoding.UTF8.GetString(memoryStream.ToArray());                
            }            
            return actualContent;
        }

        private bool CertificateValidationCallback(object sender,
                                                   X509Certificate certificate,
                                                   X509Chain chain,
                                                   SslPolicyErrors sslPolicyErrors)
        {
            if (sslPolicyErrors != SslPolicyErrors.None)
            {
                if ((sslPolicyErrors & SslPolicyErrors.RemoteCertificateChainErrors) != 0)
                {
                    if (chain != null && chain.ChainStatus != null)
                    {
                        foreach (X509ChainStatus status in chain.ChainStatus)
                        {
                            if ((certificate.Subject == certificate.Issuer) && (status.Status == X509ChainStatusFlags.UntrustedRoot))
                            {
                                continue;
                            }
                            else if (status.Status != X509ChainStatusFlags.NoError)
                            {
                                return false;
                            }
                        }
                    }
                }
            }
            // either no errors or only found unstrusted self-signed certificates
            // fingerprint check will cover that
            X509Certificate2 cert = new X509Certificate2(certificate);
            if (fingerprintMatch(this.fingerprintCache, cert.Thumbprint))
            {
                return true;
            }

            return false;
        }
    }
}
