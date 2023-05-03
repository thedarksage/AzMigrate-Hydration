using System;
using System.Text;

namespace MarsAgent.Monitoring
{
    /// <summary>
    /// Class representing the exception that wraps MARS agent error.
    /// </summary>
    public class MarsAgentException : Exception
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentException"/> class.
        /// </summary>
        /// <param name="endpointException">Exception thrown by the MARS endpoint.</param>
        public MarsAgentException(Exception endpointException)
            : base()
        {
            this.ErrorCode = unchecked((uint)endpointException.HResult);
            this.EndpointException = endpointException;
        }

        /// <summary>
        /// Gets or sets the error code for the MARS agent error.
        /// </summary>
        public uint ErrorCode { get; set; }

        /// <summary>
        /// Gets or sets the exception thrown by the MARS endpoint.
        /// </summary>
        public Exception EndpointException { get; set; }

        /// <summary>
        /// Checks whether the given MARS agent error is retriable or not.
        /// </summary>
        /// <returns>True if it is a retriable error, false otherwise.</returns>
        public bool IsRetriable()
        {
            return !(this.ErrorCode == 0x80790002 ||
                this.ErrorCode == 0x80790003 ||
                ((this.ErrorCode >= 0x80790400) && (this.ErrorCode <= 0x807906FF)));
        }

        /// <summary>
        /// Checks whether the given MARS agent error is retriable with exponentially increasing
        /// delay or not.
        /// </summary>
        /// <returns>True if the error is retriable with exponentially increasing delay,
        /// false otherwise.</returns>
        public bool IsRetriableWithExponentialDelay()
        {
            return (this.ErrorCode >= 0x80790300) && (this.ErrorCode <= 0x807903FF);
        }

        /// <summary>
        /// Checks whether the given MARS agent error is caused due to unavailability of RPC server.
        /// </summary>
        /// <returns>True if the error is caused due to unavailability of RPC server, false
        /// otherwise.</returns>
        public bool IsRpcServerUnavailable()
        {
            uint hresultCode = this.ErrorCode & 0xFFFF;
            return hresultCode == 1722L;
        }

        /// <summary>
        /// Overrides the base ToString for debugging/logging purposes.
        /// </summary>
        /// <returns>The string representation for MARS agent exception.</returns>
        public override string ToString()
        {
            return new StringBuilder()
                .Append("Error code: ").AppendFormat("{0:X}", this.ErrorCode).AppendLine()
                .AppendLine("MARS endpoint exception: ")
                .AppendLine(this.EndpointException.ToString())
                .ToString();
        }
    }
}
