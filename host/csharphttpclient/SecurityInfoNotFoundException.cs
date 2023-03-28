using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HttpCommunication
{
    public class SecurityInfoNotFoundException : Exception
    {
        public SecurityInfoNotFoundException()
            : base()
        {
        }
        
        public SecurityInfoNotFoundException(string message)
            : base(message)
        {
        }

        public SecurityInfoNotFoundException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        public SecurityInfoNotFoundException(System.Runtime.Serialization.SerializationInfo serializationInfo, System.Runtime.Serialization.StreamingContext streamingContext)
            : base(serializationInfo, streamingContext)
        {
        }
    }
}
