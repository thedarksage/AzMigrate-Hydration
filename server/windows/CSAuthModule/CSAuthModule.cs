using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web;

namespace CSAuthModuleNS
{
    

    public class CSAuthModule : IHttpModule
    {

        protected static CSConfigData m_csConfigData = new CSConfigData();
        protected static bool m_CSConfigloaded = false;
        private static string csInstallationPath = CSConfig.GetCSInstallationPath();

        private String WebServerLog = csInstallationPath+"\\home\\svsystems\\var\\authentication.log";

        public CSAuthModule()
        {

        }

        public void Dispose()
        {

        }

        public void Init(HttpApplication context)
        {
            context.PostAuthenticateRequest += new EventHandler(CSAuthPostAuthenticateRequestHandler);
            context.Error += new EventHandler(CSAuthErrorHandler);
            //context.EndRequest += new EventHandler(CSAuthEndRequestHandler);
        }

        private void loadConfiguration()
        {
            //  algorithm:
            //  acquire a lock to prevent simultaneous access to shared data (use autogaurd?)
            //    get the list of access blocked  folders/urls
            //    get the list of allowed folders/urls without needing any authentication
            //    is authentication to be performed
            //    get passphrase
            //    get fingerprint
            //  release lock
            // note: if any of the operation fails, throw exception which would result in
            //       client recieving server internal error (500?)


            if (!m_CSConfigloaded)
            {

                lock (m_csConfigData)
                {
                    if (!m_CSConfigloaded)
                    {
                        CSConfig.ReadCSConfigData(ref m_csConfigData);
                    }
                    m_CSConfigloaded = true;
                }
            }

        }


        private void VerifySignature(HttpRequest httpRequest)
        {

            String computedSignature = "";
            String requestId = httpRequest.Headers["HTTP-AUTH-REQUESTID"];
            String requestSignature = httpRequest.Headers["HTTP-AUTH-SIGNATURE"];
            String requestPhpName = httpRequest.Headers["HTTP-AUTH-PHPAPI-NAME"];
            String requestVersion = httpRequest.Headers["HTTP-AUTH-VERSION"];
            String requestMethod = httpRequest.HttpMethod;
            Uri requestUri = httpRequest.Url;

            computedSignature = Signature.ComputeSignatureComponentAuth(requestId, requestPhpName, requestUri, requestMethod, requestVersion);
            if (String.Compare(computedSignature, requestSignature, false) != 0)
            {
                throw new CSAuthException(403, "Access to " + httpRequest.FilePath + " is not allowed using provided credentials"
                    + "<br>" + "HTTP-AUTH-REQUESTID: " + requestId
                    + "<br>" + "HTTP-AUTH-SIGNATURE: " + requestSignature
                    + "<br>" + "HTTP-AUTH-PHPAPI-NAME: " + requestPhpName
                    + "<br>" + "HTTP-AUTH-VERSION: " + requestVersion
                    + "<br>" + "HTTP-METHOD: " + requestMethod
                    + "<br>" + "Expected HTTP-AUTH-SIGNATURE: " + computedSignature);
            }
        }

        private void CSAuthPostAuthenticateRequestHandler(object sender, EventArgs e)
        {
            try
            {
                loadConfiguration();
                HttpApplication app = (HttpApplication)sender;
                HttpRequest request = app.Context.Request;

                // if the specified url is under blockedFolders, throw forbidden access error
                foreach (String blkFldr in m_csConfigData.blockedFolders)
                {
                    if (request.FilePath.StartsWith(blkFldr))
                        throw new CSAuthException(403, "Access to " + request.FilePath + " is not allowed.");
                }

                // if the specified url matches with any of blockedUrls, throw forbidden access error
                foreach (String blkUrl in m_csConfigData.blockedUrls)
                {
                    if (blkUrl == request.FilePath)
                        throw new CSAuthException(403, "Access to " + request.FilePath + " is not allowed.");
                }

                // if the specified url is under folders allowing access without any checks, return
                foreach (String fldrWoCheck in m_csConfigData.foldersAllowingAccessWithoutAnyChecks)
                {
                    if (request.FilePath.StartsWith(fldrWoCheck))
                        return;
                }

                // if the specified url matches with any of urls allowing access without any checks, return
                foreach (String urlWoCheck in m_csConfigData.urlsAllowingAccessWithoutAnyChecks)
                {
                    if (urlWoCheck == request.FilePath)
                        return;
                }

                // if the specified url ends with any of extension allowing access without any checks, return
                foreach (String extnWoCheck in m_csConfigData.extensionsAllowingAccessWithoutAnyChecks)
                {
                    if (request.FilePath.EndsWith(extnWoCheck))
                        return;
                }

                // if verificationRequired is false, return
                if (m_csConfigData.client_authentication == "1")
                {
                    VerifySignature(request);
                }
            }
            catch (Exception ex)
            {
                if (ex is CSAuthException) 
                { 
                    throw; 
                }
                else
                {
                    HttpRequest request = ((HttpApplication)sender).Context.Request;
                    throw new CSAuthException(500, String.Format("<br>identity:{0} Url access:{1}<br>", request.LogonUserIdentity.Name, request.FilePath), ex);
                }
            }
        }

        private void CSAuthErrorHandler(object sender, EventArgs e)
        {
            HttpApplication app = (HttpApplication)sender;
            HttpRequest request = app.Context.Request;
            HttpResponse resp = app.Context.Response;

            Exception objErr = app.Context.Server.GetLastError();//.GetBaseException();
            
            if (objErr is CSAuthException)
            {
                resp.StatusCode = ((CSAuthException)objErr).responseCode;
            } else {
                resp.StatusCode = 500;
            }
            
            string errMsg = "<!DOCTYPE html><html><body><span style=\"font-size: 12pt; color: red\"> <br />";
            string errMsgToLog;
            errMsg += "<b>Exception: </b>" + resp.StatusCode +
                       "<hr><br>" +
                         "<br><b>Error in: </b>" + HttpUtility.HtmlEncode(request.Url.ToString()) +
                         "<br><b>Sender:</b>" + HttpUtility.HtmlEncode(request.UserHostAddress);
            errMsgToLog = DateTime.UtcNow.ToString() + " (UTC): " +errMsg + "<br><b>Error Message: </b>" + objErr.Message.ToString();

            //if (objErr is CSAuthException == false)
            {
                errMsgToLog += "<br><b>Error Message: </b>" + objErr.GetBaseException().ToString() +
                         "<br><b>Stack Trace:</b><br>" +
                          objErr.StackTrace.ToString();
            }


            errMsg += "<br/></span></body></html>";
            errMsgToLog += "<br/></span></body></html>";

            
            //resp.SuppressContent = true;


            File.AppendAllText(WebServerLog, errMsgToLog + Environment.NewLine);

            resp.Write(errMsg);
            resp.End();
            resp.TrySkipIisCustomErrors = true;
            resp.Flush();
            app.Context.Server.ClearError();
        }
    }
}


