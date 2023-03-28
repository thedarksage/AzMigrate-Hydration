using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;
using System.Threading;
using InMage.APIModel;
using InMage.APIInterfaces;
using InMage.Logging;
using HttpCommunication;

namespace InMage.APIHelpers
{
    public static class CXAPI
    {       
        // Default timeout in milliseconds used for CX calls when no user defined is given.
        const int HTTP_REQUEST_TIMEOUT = 300000;
        const string CX_URL_ABSOLUTE_PATH = "/ScoutAPI/CXAPI.php";
        const int READ_FINGERPRINT_SLEEP = 30000;
        const string SIG_PWD_MISMATCH = "-2";

        public static Response post(ArrayList functionRequests, string cxip, int port, string protocol, string hostID)
        {
            string url = String.Format("{0}://{1}:{2}{3}", protocol, cxip, port, CX_URL_ABSOLUTE_PATH);
            return post(functionRequests, new Uri(url), AuthMethod.ComponentAuth, null, null, null, null, hostID);
        }

        /*
         * Post Request Object and Get Response Object 
         */
        public static Response post(ArrayList functionRequests, string cxip, int port, string protocol)
        {
            string url = String.Format("{0}://{1}:{2}{3}", protocol, cxip, port, CX_URL_ABSOLUTE_PATH);
            return post(functionRequests, new Uri(url), AuthMethod.ComponentAuth, null, null, null, null);
        }

        public static Response post(ArrayList functionRequests, string cxip, int port, string protocol, string activityId, string clientRequestId)
        {
            string url = String.Format("{0}://{1}:{2}{3}", protocol, cxip, port, CX_URL_ABSOLUTE_PATH);
            return post(functionRequests, new Uri(url), AuthMethod.ComponentAuth, null, null, activityId, clientRequestId);
        }

        /*
         * Post Request Object and Get Response Object 
         */
        public static Response post(ArrayList functionRequests, string cxip, int port, string protocol, AuthMethod authMethod, string accessKeyId,
            string passphrase, string fingerprint, string userName, string pwd, string activityId, string clientRequestId)
        {
            DateTime date = DateTime.Now;
            Random random = new Random();
            string requestId = CXHelper.GenerateRequestId();
            string version = CXHelper.RequestVersion;
            string url = String.Format("{0}://{1}:{2}{3}", protocol, cxip, port, CX_URL_ABSOLUTE_PATH);
            return post(requestId, version, functionRequests, new Uri(url), authMethod, accessKeyId, passphrase, fingerprint, null, null, userName, pwd, HTTP_REQUEST_TIMEOUT, activityId, clientRequestId);
        }

        /*
         * Post Request Object and Get Response Object
         */
        public static Response post(ArrayList functionRequests)
        {
            Uri uri = GetCXUri(CX_URL_ABSOLUTE_PATH);
            return post(functionRequests, uri, AuthMethod.ComponentAuth, null, null, null, null);
        }

        /*
         * Post Request Object and Get Response Object
         */
        public static Response post(ArrayList functionRequests, string activityId, string clientRequestId, string serviceActivityId=null)
        {
            Uri uri = GetCXUri(CX_URL_ABSOLUTE_PATH);
            return post(functionRequests, uri, AuthMethod.ComponentAuth, null, null, activityId, clientRequestId, null, serviceActivityId:serviceActivityId);
        }

        /*
         * Post Request Object and Get Response Object
         */
        public static Response post(ArrayList functionRequests, Uri uri)
        {
            return post(functionRequests, uri, AuthMethod.ComponentAuth, null, null, null, null);
        }

        /*
        * Post Request Object and Get Response Object
        */
        public static Response post(ArrayList functionRequests, Uri uri, string activityId)
        {
            return post(functionRequests, uri, AuthMethod.ComponentAuth, null, null, activityId, null);
        }

        /*
        * Post Request Object and Get Response Object
        */
        public static Response post(ArrayList functionRequests, Uri uri, string activityId, string clientRequestId)
        {
            return post(functionRequests, uri, AuthMethod.ComponentAuth, null, null, activityId, clientRequestId);
        }

        /*
         *  Post the request object and gets the valid response object
         */
        public static Response post(ArrayList functionRequests, Uri uri, AuthMethod authMethod, string passphraseDir, string fingerprintDir, string activityId, string clientRequestId, string accesskeyId = null, string serviceActivityId = null)
        {
            string passphrase = null;
            string fingerprint = null;
            Response response = null;
            bool readNewFingerPrint = false;
            bool readNewPassphrase = false;
            int retryCount = 3;

            while (true)
            {
                try
                {
                    if (authMethod == AuthMethod.ComponentAuth)
                    {
                        if (string.IsNullOrEmpty(accesskeyId))
                        {
                            accesskeyId = CXSecurityInfo.GetAccessKeyID();
                        }
                        passphrase = CXSecurityInfo.ReadPassphrase(passphraseDir, readNewPassphrase);

                        if (uri.Scheme == Uri.UriSchemeHttps)
                        {
                            fingerprint = CXSecurityInfo.ReadFingerprint(fingerprintDir, uri.Host, uri.Port, readNewFingerPrint);
                        }
                    }
                    response = post(functionRequests, uri, authMethod, accesskeyId, passphrase, fingerprint, passphraseDir, fingerprintDir, activityId, clientRequestId, serviceActivityId);
                    if (response.Returncode == SIG_PWD_MISMATCH)
                    {
                        --retryCount;
                        if (retryCount > 0)
                        {
                            readNewPassphrase = true;
                            Thread.Sleep(READ_FINGERPRINT_SLEEP);
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                catch (System.Security.Authentication.AuthenticationException ex)
                {
                    --retryCount;
                    if (retryCount > 0)
                    {
                        readNewFingerPrint = true;
                        Thread.Sleep(READ_FINGERPRINT_SLEEP);
                    }
                    else
                    {
                        throw;
                    }
                }
            }
            return response;
        }

        /*
         * Post Request Object and Get Response Object
         */
        public static Response post(ArrayList functionRequests, Uri uri, AuthMethod authMethod, string accessKeyId,
            string passphrase, string fingerprint, string userName, string pwd, string activityId, string clientRequestId, string serviceActivityId = null)
        {
            DateTime date = DateTime.Now;
            Random random = new Random();
            string requestId = CXHelper.GenerateRequestId();
            string version = CXHelper.RequestVersion;
            return post(requestId, version, functionRequests, uri, authMethod, accessKeyId, passphrase, fingerprint, null, null, userName, pwd, HTTP_REQUEST_TIMEOUT, activityId, clientRequestId, serviceActivityId);
        }

        /*
         * Post the request object and gets the valid response object
         */
        public static Response post(string requestId, string version, ArrayList functionRequests, Uri uri,
            AuthMethod authMethod, string accessKeyId, string passphrase, string fingerprint, string passphraseDir, string fingerprintDir,
            string userName, string pwd, int timeout, string activityId, string clientRequestId, string serviceActivityId = null)
        {
            // Create RequestBuilder to Create RequestObject
            RequestBuilder requestBuilder = null;
            Request req = null;
            string reqMethod = "POST";
            Response response = null;
            String requestXML = null;
            String responseXML = null;

            if (String.IsNullOrEmpty(accessKeyId) || String.IsNullOrEmpty(passphrase))
            {
                requestBuilder = RequestBuilderFactory.getRequestBuilder(version);
            }
            else
            {
                requestBuilder = RequestBuilderFactory.getRequestBuilder(accessKeyId, passphrase, version, fingerprint);
            }
            if (authMethod == AuthMethod.ComponentAuth)
            {
                req = requestBuilder.createRequestObject(requestId, authMethod, functionRequests, uri, reqMethod, "V2015_01");
            }
            else if (authMethod == AuthMethod.MessageAuth)
            {
                req = requestBuilder.createRequestObject(requestId, functionRequests);
            }
            else
            {
                req = requestBuilder.createRequestObject(requestId, authMethod, functionRequests, userName, pwd);
            }
            requestXML = requestBuilder.createRequestXML(req);

            // bug: 5922423  - Do not log credentials even in Debug mode in CS
            // Commented the below lines to address  the bug 5922423
            //Logger.Info("********************** Request ***********************"
            //    + "\r\n" + requestXML + "\r\n************************************************************************************");

            /* Post the XML to CX   */

            // Construct the Dictionary Object to initialize the APIInterface
            Dictionary<String, String> dictionary = new Dictionary<string, string>();
            dictionary.Add("Type", "CX");
            dictionary.Add("ServerIP", uri.Host);
            dictionary.Add("ServerPort", uri.Port.ToString());
            dictionary.Add("Protocol", uri.Scheme);
            dictionary.Add("AbsoluteURLPath", uri.AbsolutePath);
            dictionary.Add("RequestMethod", reqMethod);
            dictionary.Add("TimeOut", timeout.ToString());
            dictionary.Add("RequestId", req.Id);
            dictionary.Add("Version", req.Version);
            if (!String.IsNullOrEmpty(req.Header.Authentication.AccessSignature))
            {
                dictionary.Add("AccessSignature", req.Header.Authentication.AccessSignature);
            }
            string functionNames = "";
            int count = 0;
            foreach (FunctionRequest funcRequest in functionRequests)
            {
                ++count;
                functionNames += funcRequest.Name;
                if (count == functionRequests.Count)
                {
                    functionNames += ",";
                }
            }
            dictionary.Add("FunctionNames", functionNames);
            if (!string.IsNullOrEmpty(activityId))
            {
                dictionary.Add("ActivityId", activityId);
            }
            if (!string.IsNullOrEmpty(clientRequestId))
            {
                dictionary.Add("ClientRequestId", clientRequestId);
            }
            if (!string.IsNullOrEmpty(serviceActivityId))
            {
                dictionary.Add("ServiceActivityId", serviceActivityId);
            }

            // Get API Interface
            APIInterface apiInterface = APIInterfaceFactory.getAPIInterface(dictionary);
            responseXML = apiInterface.post(requestXML);

            // bug: 5922423  - Do not log credentials even in Debug mode in CS
            // Commented the below lines to address  the bug 5922423
            //Logger.Info("******************** Response ************************"
            //+ "\r\n" + responseXML + "\r\n************************************************************************************");

            /* Create Object from XML */
            ResponseBuilder responseBuilder = new ResponseBuilder();
            response = responseBuilder.build(responseXML);

            return response;
        }

        public static Uri GetCXUri(string absolutePath)
        {
            Uri uri = null;
            InMageComponent installedComponent = CXSecurityInfo.GetInstalledComponent();
            string regKey;
            string cxIPRegKeyName;
            string cxPortRegKeyName;
            string cxProtocolRegKeyName;
            if (installedComponent == InMageComponent.CX)
            {
                regKey = "SOFTWARE{0}\\InMage Systems\\Installed Products\\9";
                cxIPRegKeyName = "CsIpAddress";
                cxPortRegKeyName = "CsPort";
                cxProtocolRegKeyName = "CsProtocol";
            }
            else if (installedComponent == InMageComponent.Agent)
            {
                regKey = "SOFTWARE{0}\\SV Systems";
                cxIPRegKeyName = "ServerName";
                cxPortRegKeyName = "ServerHttpPort";
                cxProtocolRegKeyName = "Https";
            }
            else
            {
                throw new APIModelException("Neither InMage Agent nor CX are installed.");
            }
            Microsoft.Win32.RegistryKey hklm = null;
            try
            {
                hklm = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(String.Format(regKey, "\\Wow6432Node"));
                if (hklm == null)
                {
                    hklm = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(regKey);
                    if (hklm == null)
                    {
                        throw new APIModelException("Unable to find CX URI, not able to open regKey:" + regKey + ",installedComponent:" + installedComponent);
                    }
                }                
                string cxIp = hklm.GetValue(cxIPRegKeyName, "").ToString();
                string port = hklm.GetValue(cxPortRegKeyName, "").ToString();
                CXProtocol protocol = Utils.ParseEnum<CXProtocol>(hklm.GetValue(cxProtocolRegKeyName, "http").ToString());
                if(String.IsNullOrEmpty(cxIp) || String.IsNullOrEmpty(port))
                {
                    throw new APIModelException("CX URI not found,cxIp:" + cxIp + ",port:" + port + ",installedComponent:" + installedComponent);
                }
                if (String.IsNullOrEmpty(absolutePath))
                {
                    uri = new Uri(String.Format("{0}://{1}:{2}", protocol, cxIp, port));
                }
                else
                {
                    uri = new Uri(String.Format("{0}://{1}:{2}{3}", protocol, cxIp, port, absolutePath));
                }
            }
            catch (APIModelException)
            {
                throw;
            }
            catch (Exception ex)
            {
                throw new APIModelException("Unable to find CX URI", ex);
            }
            return uri;
        }
    }
}