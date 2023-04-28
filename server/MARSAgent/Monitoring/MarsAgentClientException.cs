using System;
using System.Text;

namespace MarsAgent.Monitoring
{
    /// <summary>
    /// Class representing the exception thrown by MARS client.
    /// </summary>
    public class MarsAgentClientException : Exception
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentClientException"/> class.
        /// </summary>
        /// <param name="e">MARS agent exception.</param>
        public MarsAgentClientException(MarsAgentException e)
            : base()
        {
            this.ErrorCode = e.ErrorCode;

            string label;
            this.ErrorLabel = MarsAgentErrorConstants.ErrorCodeToErrorLabelMap.TryGetValue(
                e.ErrorCode, out label) ? label : MarsAgentErrorConstants.ErrorCodeToErrorLabelMap[DefaultErrorCode];

            this.EndpointException = e.EndpointException;
        }

        /// <summary>
        /// Default error label MT_E_GENERIC_ERROR for CBEngine error codes
        /// </summary>
        private const uint DefaultErrorCode = unchecked((uint)0x80790308);

        /// <summary>
        /// Gets or sets the error code for the MARS agent error.
        /// </summary>
        public uint ErrorCode { get; set; }

        /// <summary>
        /// Gets or sets the error label for the MARS agent error.
        /// </summary>
        public string ErrorLabel { get; set; }

        /// <summary>
        /// Gets the error category for the MARS agent error.
        /// This field is a string representation of <see cref="MarsAgentClientErrorCategories"/>
        /// enum.
        /// </summary>
        public string ErrorCategory
        {
            get
            {
                MarsAgentClientErrorCategories category;

                if (this.ErrorCode >= 0x80790500 && this.ErrorCode <= 0x80790504)
                {
                    category = MarsAgentClientErrorCategories.NonImmediateRetryableError;
                }
                else
                {
                    category = MarsAgentClientErrorCategories.Retry;
                }

                return category.ToString();
            }
        }

        /// <summary>
        /// Gets or sets the exception thrown by the MARS endpoint.
        /// </summary>
        public Exception EndpointException { get; set; }

        /// <summary>
        /// Checks if the exception is a non immediate retryable exception.
        /// </summary>
        /// <returns>True if this is non retryable exception, false otherwise.</returns>
        public bool IsNonImmediateRetryableError()
        {
            return this.ErrorCategory == nameof(MarsAgentClientErrorCategories.NonImmediateRetryableError);
        }

        /// <summary>
        /// Overrides the base ToString for debugging/logging purposes.
        /// </summary>
        /// <returns>The string representation for MARS agent exception.</returns>
        public override string ToString()
        {
            return new StringBuilder()
                .Append("Error code: ").AppendFormat("{0:X}", this.ErrorCode).AppendLine()
                .AppendLine($"Error label: {this.ErrorLabel}")
                .AppendLine($"Error category: {this.ErrorCategory}")
                .AppendLine("MARS endpoint exception: ")
                .AppendLine(this.EndpointException.ToString())
                .ToString();
        }
    }
}
