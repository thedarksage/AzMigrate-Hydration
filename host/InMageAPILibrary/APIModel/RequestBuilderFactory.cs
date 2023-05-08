using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{
    /*
     * Class: RequestBuilderFactory - Constructs and passes the reference of RequestBuilder Object based on protocol version
     */
    public class RequestBuilderFactory
    {

        /*
         * Returns Default RequestBuilder Object for InMage 
         */
        public static RequestBuilder getRequestBuilder(String version)
        {
            String accessKeyId = "DDC9525FF275C104EFA1DFFD528BD0145F903CB1";
            String passphrase = "4DCC775C9CFBB9F291AE2DBA64AF7CE54D10720A";
            RequestBuilder requestBuilder = getRequestBuilder(accessKeyId, passphrase, version);
            return requestBuilder;
        }


        /*
         *  Returns RequestBuilder Object used for Partners
         */
        public static RequestBuilder getRequestBuilder(String accessKeyId, String passphrase, String version)
        {
            RequestBuilder requestBuilder = new RequestBuilder(accessKeyId, passphrase, version);
            return requestBuilder;
        }

        /*
        *  Returns RequestBuilder Object used for Partners
        */
        public static RequestBuilder getRequestBuilder(String accessKeyId, String passphrase, String version, String fingerprint)
        {
            RequestBuilder requestBuilder = new RequestBuilder(accessKeyId, passphrase, version, fingerprint);
            return requestBuilder;
        }



    }
}
