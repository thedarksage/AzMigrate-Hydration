using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using InMage.Logging;
using HttpCommunication;

namespace InMage.APIInterfaces
{
    /*
     * Class: CXAPIInterface provides concerete implementation of executing API on CX Server
     */
    public class CXAPIInterface : APIInterface
    {
        Dictionary<String, String> dictionary;

        /*
         *  Initialize the CXAPIInterface
         */
        public void init(Dictionary<string, string> dictionary)
        {
            this.dictionary = dictionary;
        }

        /*
         *  Post the RequestXML to CX URL and Return the ResponseXML to caller program
         */
        public string post(string requestXML)
        {
            // Extract CX URL to Post
            string serverIP = dictionary["ServerIP"];
            int serverPort = int.Parse(dictionary["ServerPort"]);
            string protocol = dictionary["Protocol"];
            string absoluteUrlPath = dictionary["AbsoluteURLPath"];
            String requestMethod = dictionary["RequestMethod"];
            bool useSsl = (String.Compare(protocol, Uri.UriSchemeHttps, true) == 0) ? true : false;
            int timeOut = int.Parse(dictionary["TimeOut"]);
            Dictionary<string, string> httpHeaders = new Dictionary<string, string>();
            if (dictionary.ContainsKey("ActivityId"))
            {
                httpHeaders.Add("ActivityId", dictionary["ActivityId"]);
            }
            if (dictionary.ContainsKey("ClientRequestId"))
            {
                httpHeaders.Add("ClientRequestId", dictionary["ClientRequestId"]);
            }
            if (dictionary.ContainsKey("ServiceActivityId"))
            {
                httpHeaders.Add("ServiceActivityId", dictionary["ServiceActivityId"]);
            }
            httpHeaders.Add(HttpClient.HeaderHttpAuthRequestId, dictionary["RequestId"]);
            httpHeaders.Add(HttpClient.HeaderHttpAuthVersion, dictionary["Version"]);
            if (dictionary.ContainsKey("AccessSignature"))
            {
                httpHeaders.Add(HttpClient.HeaderHttpAuthSignature, dictionary["AccessSignature"]);
            }
            httpHeaders.Add(HttpClient.HeaderHttpAuthPHPAPIName, dictionary["FunctionNames"]);

            // Send HTTP request
            HttpClient client = new HttpClient(serverIP, serverPort, useSsl, timeOut, null);
            Logger.Info(String.Format("Posting the Request to CX with the URL: {0}://{1}:{2}{3}", protocol, serverIP, serverPort, absoluteUrlPath));
            HttpReply reply = client.sendHttpRequest(requestMethod, absoluteUrlPath, requestXML, "application/xml", httpHeaders);
            String responseXML = String.Empty;

            // Extract response XML
            if (String.IsNullOrEmpty(reply.error))
            {                
                if (!String.IsNullOrEmpty(reply.data))
                {
                    responseXML = reply.data;
                }
            }
            else
            {
                Logger.Error("HTTP request failed. Error: " + reply.error);
                throw new APIModel.APIModelException("HTTP request failed: " + reply.error, reply.HttpStatusCode);
            }
            return responseXML;
        }
    }
}
