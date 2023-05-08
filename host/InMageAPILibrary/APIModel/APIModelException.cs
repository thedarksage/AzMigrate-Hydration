using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class: APIModelException class for all exceptions occuring in the API Model 
     */
    public class APIModelException : System.Exception
    {
        public string HTTPStatusCode { get; private set; }

        public APIModelException()
            : base()
        {
        }

        /*
         *  Constructor takes message as parameter
         */ 
        public APIModelException(string message)
            : base(message)
        {
        }

        public APIModelException(string message, string httpStatusCode)
            : base(message)
        {
            this.HTTPStatusCode = httpStatusCode;
        }

        public APIModelException(string message, Exception innerException)
            :base(message, innerException)
        {
        }

        public APIModelException(System.Runtime.Serialization.SerializationInfo serializationInfo, System.Runtime.Serialization.StreamingContext streamingContext)
            : base(serializationInfo, streamingContext)
        {
        }

    }
}
