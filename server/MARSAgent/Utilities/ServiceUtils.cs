using System;
using System.ComponentModel;
using System.Management;
using System.ServiceProcess;
using LoggerInterface;
using MarsAgent.ErrorHandling;
using MarsAgent.LoggerInterface;
using RcmAgentCommonLib.ErrorHandling;
using RcmAgentCommonLib.Utilities;

namespace MarsAgent.Utilities
{
    public class ServiceUtils
    {
        /// <summary>
        /// Max number of times service related operations like start/stop will be attempted
        /// </summary>
        private static readonly int MaxServiceOpRetryCount = 5;

        /// <summary>
        /// Polling interval to check service status
        /// </summary>
        private static readonly TimeSpan ServiceStatusPollTimeout = new TimeSpan(0, 0, 30);

        /// <summary>
        /// Check the service startup type.
        /// </summary>
        /// <returns>false if startup type is set to disabled, true otherwise</returns>
        public static bool IsServiceStartupTypeDisabled(string serviceName)
        {
            bool isServiceStartupTypeDisabled = false;

            try
            {
                Logger.Instance.LogInfo(
                        CallInfo.Site(),
                        $"Verifying Service '{serviceName}' startup type");

                string query = String.Format("SELECT * FROM Win32_Service WHERE Name = '{0}'", serviceName);
                ManagementObjectSearcher searcher = new ManagementObjectSearcher(query);
                if (searcher != null)
                {
                    ManagementObjectCollection services = searcher.Get();
                    foreach (ManagementObject service in services)
                    {
                        string startupType = service.GetPropertyValue("StartMode").ToString();
                        Logger.Instance.LogInfo(
                            CallInfo.Site(),
                            "The {0} service startup type is : {1}", serviceName, startupType);
                        if (startupType == "Disabled")
                        {
                            isServiceStartupTypeDisabled = true;
                        }
                    }
                }

                return isServiceStartupTypeDisabled;
            }
            catch (Exception ex)
            {
                Logger.Instance.LogException(
                        CallInfo.Site(),
                        ex,
                        $"Error occurred while trying to check if {serviceName} is enabled or not");

                return isServiceStartupTypeDisabled;
            }
        }

        /// <summary>
        /// Stop a service
        /// </summary>
        /// <param name="serviceName">Service to be stopped</param>
        /// <param name="serviceFriendlyName">Service display name</param>
        /// <param name="hostName">HostName of the appliance</param>
        public static void StopServiceWithRetries(
            string serviceName, string serviceFriendlyName, string hostName)
        {
            if (!ServiceControllerHelper.IsServiceInstalled(serviceName))
            {
                Logger.Instance.LogError(
                    CallInfo.Site(),
                    $"Service '{serviceName}' is not installed");

                throw RcmAgentException.ServiceIsNotInstalled(
                    hostName,
                    serviceFriendlyName);
            }

            string errorCode = string.Empty, errCodeEnum = string.Empty, errMsg = string.Empty;

            using (var controller = new ServiceController(serviceName))
            {
                if (controller.Status != ServiceControllerStatus.StopPending &&
                    controller.Status != ServiceControllerStatus.Stopped)
                {
                    controller.Stop();
                    controller.Refresh();
                }

                int loopCount = 0;
                while (controller.Status != ServiceControllerStatus.Stopped)
                {
                    try
                    {
                        if (loopCount >= MaxServiceOpRetryCount)
                        {
                            break;
                        }
                        loopCount++;
                        Logger.Instance.LogInfo(CallInfo.Site(),
                                $"Current state is {controller.Status}." +
                                $"Attempting to stop {serviceName} service, {loopCount}/{MaxServiceOpRetryCount}.");

                        controller.Refresh();
                        controller.WaitForStatus(ServiceControllerStatus.Stopped, ServiceStatusPollTimeout);
                    }
                    catch (System.ServiceProcess.TimeoutException e)
                    {
                        Logger.Instance.LogException(
                            CallInfo.Site(),
                            e,
                            $"{serviceName} stop timedout.");

                        errorCode = nameof(RcmAgentErrorCode.ServiceStopTimedout);
                        errCodeEnum = e.HResult.ToString();
                        errMsg = e.Message;
                    }
                    catch (Win32Exception e)
                    {
                        Logger.Instance.LogException(
                            CallInfo.Site(),
                            e,
                            $"{serviceName} stop failed.");

                        errorCode = nameof(RcmAgentErrorCode.ServiceStopFailed);
                        errCodeEnum = e.HResult.ToString();
                        errMsg = e.Message;
                    }
                }

                if (controller.Status != ServiceControllerStatus.Stopped)
                {
                    Logger.Instance.LogError(CallInfo.Site(),
                        $"Failed to stop {serviceName} service after {MaxServiceOpRetryCount} " +
                        $"retries with error {errorCode} Current state is {controller.Status}.");

                    if (!string.Equals(errorCode, nameof(RcmAgentErrorCode.ServiceStopTimedout), StringComparison.OrdinalIgnoreCase))
                    {
                        throw RcmAgentException.ServiceStopTimedout(
                                serviceFriendlyName,
                                serviceName,
                                errCodeEnum,
                                errMsg);
                    }
                    else
                    {
                        throw RcmAgentException.ServiceStopFailed(
                            serviceFriendlyName,
                            serviceName,
                            errCodeEnum,
                            errMsg);
                    }
                }
                else
                {
                    Logger.Instance.LogInfo(
                                CallInfo.Site(),
                                $"{serviceName} has been stopped.");
                }
            }
        }
    }
}
