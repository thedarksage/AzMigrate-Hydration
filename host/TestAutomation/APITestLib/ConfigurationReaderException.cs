using System;
using System.Collections.Generic;
using System.Text;

namespace InMage.Test.API
{
    public class ConfigurationReaderException : System.Exception
    {
        public ConfigurationReaderException()
            : base()
        {
        }

        public ConfigurationReaderException(string message)
            : base(message)
        {
        }

        public ConfigurationReaderException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        public ConfigurationReaderException(System.Runtime.Serialization.SerializationInfo serializationInfo, System.Runtime.Serialization.StreamingContext streamingContext)
            : base(serializationInfo, streamingContext)
        {
        }
    }
}
