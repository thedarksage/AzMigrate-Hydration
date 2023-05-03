using RcmAgentCommonLib.Service;
using RcmContract;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.LoggerInterface;
using LoggerInterface;
using MarsAgent.Monitoring;
using RcmAgentCommonLib.Utilities;
using MarsAgent.CBEngine.Constants;
using System.ServiceProcess;
using System;
using MarsAgent.ErrorHandling;
using MarsAgentException = MarsAgent.ErrorHandling.MarsAgentException;
using MarsAgent.Utilities;

namespace MarsAgent.Service
{
    /// <summary>
    /// Defines class for the service health.
    /// </summary>
    public sealed class ServiceHealth
        : ServiceHealthBase<ServiceConfigurationSettings, MarsSettings, LogContext>
    {
        /// <summary>
        /// Gets singleton instance of the service health class.
        /// </summary>
        public static readonly ServiceHealth Instance = new ServiceHealth();

        /// <summary>
        /// Prevents a default instance of the <see cref="ServiceHealth"/> class from
        /// being created.
        /// </summary>
        private ServiceHealth()
            : base(
                  ServiceProperties.Instance,
                  ServiceSettings.Instance,
                  Configurator.Instance,
                  Logger.Instance)
        {
        }


        /// <summary>
        /// Detects the health of the component.
        /// </summary>
        protected override void DetectComponentHealth()
        {
            string errorCode = string.Empty;

            try
            {
                errorCode = MonitorCBEngineServiceStatus();

                if (string.IsNullOrWhiteSpace(errorCode))
                {
                    MarsAgentApi marsAgentApi = new MarsAgentApi();
                    marsAgentApi.CheckAgentHealth();
                }
            }
            catch (MarsAgentClientException marsEx)
            {
                this.LoggerInstance.LogError(
                        CallInfo.Site(),
                        $"Caught Health issue '{marsEx.ErrorCode}' Mars Agent.");

                MarsAgentErrorConstants.ErrorCodeToComponentHealthIssueCodeMap.TryGetValue(
                    marsEx.ErrorCode, out errorCode);
            }

            if (!string.IsNullOrWhiteSpace(errorCode))
            {
                this.LoggerInstance.LogError(
                    CallInfo.Site(),
                    $"Health issue '{errorCode}' detected for Mars Agent.");

                this.RecordHealthIssue(errorCode);

                this.LoggerInstance.LogError(
                    CallInfo.Site(),
                    $"Reported health issue '{errorCode}' for Mars Agent.");
            }
        }

        /// <summary>
        /// Raise health issue when cbengine service is not running
        /// </summary>
        private string MonitorCBEngineServiceStatus()
        {
            string errorCode = string.Empty;
            try
            {
                var serviceName = CBEngineConstants.ServiceName;

                this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Verifying Service '{serviceName}' status");

                if (!ServiceControllerHelper.IsServiceInstalled(serviceName))
                {
                    errorCode = nameof(MarsAgentErrorCode.ServicesNotInstalled);

                    this.LoggerInstance.LogError(
                        CallInfo.Site(),
                        $"Service '{serviceName}' is not installed." +
                        $" Raising '{errorCode}' health issue");

                    return errorCode;
                }

                if (ServiceUtils.IsServiceStartupTypeDisabled(serviceName))
                {
                    errorCode = nameof(MarsAgentErrorCode.ServiceIsInDisabledState);

                    this.LoggerInstance.LogError(
                        CallInfo.Site(),
                        $"Service '{serviceName}' is in disabled state." +
                        $" Raising '{errorCode}' health issue");

                    return errorCode;
                }

                var status = ServiceControllerHelper.GetServiceStatus(serviceName);

                this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Service '{serviceName}' status is '{status}'");

                if (status != ServiceControllerStatus.Running)
                {
                    errorCode = nameof(MarsAgentErrorCode.ServicesNotRunning);

                    this.LoggerInstance.LogVerbose(
                        CallInfo.Site(),
                        $"Service '{serviceName}' is not running." +
                        $" Raising '{errorCode}' health issue");

                    return errorCode;
                }
            }
            catch (Exception ex)
            {
                // For any other exceptions, we log the exception
                // TODO : Check if we need to raise a new health issue in case of unknown exception.
                this.LoggerInstance.LogException(
                                        CallInfo.Site(),
                                        ex,
                                        $"Failed to get '{CBEngineConstants.ServiceName}'" +
                                        $" service status.");
            }

            return errorCode;
        }
    }
}