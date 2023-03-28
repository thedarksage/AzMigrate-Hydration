using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;
using System.Diagnostics;
using InMage.APIModel;
using InMage.APIInterfaces;
using InMage.Logging;

namespace InMage.APIHelpers
{
    public static class LocalAPI
    {
        private static object lockObj = new object();
        /*
         *  Post the request object and gets the valid response object
         */

        public static Response post(String requestId, String version, ArrayList functionRequests, bool isFile)
        {
            // Create RequestBuilder to Create RequestObject

            RequestBuilder requestBuilder = RequestBuilderFactory.getRequestBuilder(version);
            Request req = requestBuilder.createRequestObject(requestId, functionRequests);
            String requestXML = requestBuilder.createRequestXML(req);
            Logger.Info("********************* Request ************************"
                + "\r\n" + requestXML
                + "\r\n************************************************************************************");


            /* Post the XML to Local   */

            // Construct the Dictionary Object to initialize the APIInterface
            Dictionary<String, String> dictionary = new Dictionary<string, string>();
            dictionary.Add("Type", "Local");
            if(isFile)
                dictionary.Add("RequestIs", "File");
            else
                dictionary.Add("RequestIs", "Memory");

            // Get API Interface
            APIInterface apiInterface = APIInterfaceFactory.getAPIInterface(dictionary);
            String responseXML = "";
            lock (lockObj)
            {
                responseXML = apiInterface.post(requestXML);
            }
            Logger.Info("********************* Response ***********************"
                + "\r\n" + responseXML
                + "\r\n************************************************************************************");

            /* Create Object from XML */
            ResponseBuilder responseBuilder = new ResponseBuilder();
            Response response = responseBuilder.build(responseXML);
            return response;
        }
       
        /*
         * Post Request Object and Get Response Object 
         */
        
        public static Response post(ArrayList functionRequests, bool isFile)
        {
            DateTime date = DateTime.Now;
            String requestId = date.ToString("yyyyMMddHHmmssffff");
            //  String requestId = "XXXX";
            String version = "1.0";
            return post(requestId, version, functionRequests, isFile);
        }
    }
}
