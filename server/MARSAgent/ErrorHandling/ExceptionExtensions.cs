using System;
using RcmAgentCommonLib.ErrorHandling;
using RcmClientLib;
using RcmCommonUtils;
using RcmContract;

namespace MarsAgent.ErrorHandling
{
    /// <summary>
    /// Defines extension methods for exceptions coming from the service.
    /// </summary>
    public static class ExceptionExtensions
    {
        /// <summary>
        /// Converts the given exception to an service exception.
        /// </summary>
        /// <param name="ex">Exception object.</param>
        /// <returns>Service exception.</returns>
        public static MarsAgentException ToMarsAgentException(
            this Exception ex)
        {
            if (ex is MarsAgentException)
            {
                return ex as MarsAgentException;
            }
            else if (ex is RcmException)
            {
                return new MarsAgentException(
                    MarsAgentException.RcmOperationFailed(),
                    ex);
            }
            else if (ex is RcmAgentException)
            {
                return new MarsAgentException(
                    MarsAgentException.RcmAgentOperationFailed(),
                    ex);
            }
            else
            {
                return new MarsAgentException(ex);
            }
        }

        /// <summary>
        /// Converts exception to agent reported error.
        /// </summary>
        /// <param name="ex">The exception object.</param>
        /// <param name="acceptLanguage">The accept language.</param>
        /// <returns>The agent reported error.</returns>
        public static RcmAgentError ToRcmAgentError(this Exception ex, string acceptLanguage)
        {
            RcmAgentError rcmAgentError;

            if (ex is RcmException)
            {
                RcmException rcmException = ex as RcmException;
                RcmError rcmError = rcmException.ToRcmError();

                rcmAgentError = new RcmAgentError(
                    rcmError.ErrorCode,
                    rcmError.Message,
                    rcmError.PossibleCauses,
                    rcmError.RecommendedAction,
                    rcmError.ErrorSeverity,
                    rcmError.ErrorCategory,
                    rcmException.IsRetryableError,
                    rcmError.MessageParameters,
                    rcmError.IsRetryableError,
                    rcmException);
            }
            else if (ex is RcmAgentException)
            {
                RcmAgentException rcmAgentException = ex as RcmAgentException;
                string severity = rcmAgentException.GetErrorSeverity();
                string category = rcmAgentException.GetErrorCategory();
                string errorMessage = rcmAgentException.GetErrorMessage(acceptLanguage);
                string possibleCauses = rcmAgentException.GetPossibleCauses(acceptLanguage);
                string recommendedAction = rcmAgentException.GetRecommendedAction(acceptLanguage);

                rcmAgentError = new RcmAgentError(
                    rcmAgentException.ErrorCode.ToString(),
                    errorMessage,
                    possibleCauses,
                    recommendedAction,
                    severity,
                    category,
                    rcmAgentException.IsRetryableClientException(),
                    rcmAgentException.MessageParameters);
            }
            else
            {
                MarsAgentException marsAgentException = ex as MarsAgentException;
                if (marsAgentException == null)
                {
                    marsAgentException = new MarsAgentException(ex);
                }

                string severity = marsAgentException.GetErrorSeverity();
                string category = marsAgentException.GetErrorCategory();
                string errorMessage = marsAgentException.GetErrorMessage(acceptLanguage);
                string possibleCauses = marsAgentException.GetPossibleCauses(acceptLanguage);
                string recommendedAction =
                    marsAgentException.GetRecommendedAction(acceptLanguage);

                rcmAgentError = new RcmAgentError(
                    marsAgentException.ErrorCode.ToString(),
                    errorMessage,
                    possibleCauses,
                    recommendedAction,
                    severity,
                    category,
                    marsAgentException.IsRetryableClientException(),
                    marsAgentException.MessageParameters);
            }

            return rcmAgentError;
        }
    }
}
