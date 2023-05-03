namespace MarsAgent.Monitoring
{
    /// <summary>
    /// Class enumerating the categories of MARS agent client errors.
    /// </summary>
    public enum MarsAgentClientErrorCategories
    {
        /// <summary>
        /// Retriable errors for which number of retry attempts have reached the maximum limit for
        /// the current session. In this case, operation can be retried without having to perform
        /// any action.
        /// </summary>
        Retry,

        /// <summary>
        /// Non-retryable fatal errors for which user action is required.
        /// In this case, We will delay next attempt for few minutes.
        /// </summary>
        NonImmediateRetryableError
    }
}
