using System;

namespace DMSUploader
{
    /// <summary>
    /// Invalid request exception.
    /// </summary>
    public class InvalidRequestException : Exception
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="InvalidRequestException"/> class.
        /// </summary>
        public InvalidRequestException()
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="InvalidRequestException"/> class.
        /// </summary>
        /// <param name="errorMessage">Error message.</param>
        public InvalidRequestException(string errorMessage)
            : base(errorMessage)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="InvalidRequestException"/> class.
        /// </summary>
        /// <param name="errorMessage">Error message.</param>
        /// <param name="exception">Exception.</param>
        public InvalidRequestException(string errorMessage, Exception exception)
            : base(errorMessage, exception)
        {
        }
    }
}