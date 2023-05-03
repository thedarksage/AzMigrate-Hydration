using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HttpCommunication
{
    public class HttpClientException : Exception
    {
        public HttpClientException()
            : base()
        {
        }

        public HttpClientException(string message)
            : base(message)
        {
        }

        public HttpClientException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        public HttpClientException(System.Runtime.Serialization.SerializationInfo serializationInfo, System.Runtime.Serialization.StreamingContext streamingContext)
            : base(serializationInfo, streamingContext)
        {
        }
    }
}
