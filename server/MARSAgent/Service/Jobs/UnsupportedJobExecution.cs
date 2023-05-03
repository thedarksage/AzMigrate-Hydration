using System;
using System.Collections.Generic;
using RcmAgentCommonLib.ErrorHandling;
using RcmAgentCommonLib.Utilities.JobExecution;
using RcmContract;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.ErrorHandling;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service
{
    /// <summary>
    /// Defines class for the UnsupportedJob job execution.
    /// </summary>
    public sealed class UnsupportedJobExecution
        : UnsupportedJobExecutionBase<MarsSettings, ServiceConfigurationSettings, LogContext>
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="UnsupportedJobExecution" /> class.
        /// </summary>
        /// <param name="rcmJob">Job to execute.</param>
        public UnsupportedJobExecution(RcmJob rcmJob)
            : base(
                  rcmJob,
                  ServiceTunables.Instance,
                  new RcmAgentTaskLogContext(),
                  Logger.Instance,
                  Configurator.Instance,
                  ServiceSettings.Instance,
                  ServiceProperties.Instance)
        {
        }

        /// <summary>
        /// Convert the exception to job specific errors.
        /// </summary>
        /// <param name="ex">Exception to convert.</param>
        /// <returns>A list of job specific errors.</returns>
        protected override List<RcmJobError> ConvertExceptionToErrorsInternal(Exception ex)
        {
            RcmAgentError rcmAgentError = ex.ToRcmAgentError(
                Logger.Instance.GetAcceptLanguage());

            RcmJobError jobError = rcmAgentError.ToRcmJobError();
            return new List<RcmJobError> { jobError };
        }
    }
}
