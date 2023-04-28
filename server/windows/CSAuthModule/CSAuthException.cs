using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CSAuthModuleNS
{
    [Serializable]
    public class CSAuthException : System.Exception
    {
        public CSAuthException(int statusCode)
            : base()
        {
            responseCode = statusCode;
        }


        public CSAuthException(int statusCode, string message)
            :  base(message)
        {
            responseCode = statusCode;
        }

        public CSAuthException(int statusCode, string message, Exception innerException)
            :base(message, innerException)
        {
            responseCode = statusCode;
        }


        public CSAuthException(int statusCode, System.Runtime.Serialization.SerializationInfo serializationInfo, System.Runtime.Serialization.StreamingContext streamingContext)
            : base(serializationInfo, streamingContext)
        {
            responseCode = statusCode;
        }

        public int  responseCode { get; set; }

    }
}

