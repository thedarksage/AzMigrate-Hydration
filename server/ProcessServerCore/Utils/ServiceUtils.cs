using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Threading;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Service related utilities
    /// </summary>
    public class ServiceUtils
    {
        /// <summary>
        /// Starts the service with default max retry count and poll timeout
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        /// <param name="token">Cancellation Token</param>
        public static void StartService(string serviceName, CancellationToken token)
        {
            StartServiceWithRetries(serviceName: serviceName,
                maxRetryCount: Settings.Default.PSMonitor_DefaultServicesMaxRetryCount,
                pollTimeout: Settings.Default.PSMonitor_DefaultServicesPollChangeTimeout,
                ct: token);
        }

        /// <summary>
        /// Starts the service with given retry count and poll timeout
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        /// <param name="maxRetryCount">Max number of times service start will be attempted</param>
        /// <param name="pollTimeout">Polling interval to check desired service status</param>
        /// <param name="ct">Cancellation token</param>
        public static void StartServiceWithRetries(
            string serviceName, int maxRetryCount, TimeSpan pollTimeout, CancellationToken ct)
        {
            using (var controller = new ServiceController(serviceName))
            {
                if (controller.Status != ServiceControllerStatus.StartPending &&
                    controller.Status != ServiceControllerStatus.Running)
                {
                    controller.Start();
                    controller.Refresh();
                }

                int loopCount = 0;
                while (controller.Status != ServiceControllerStatus.Running)
                {
                    ct.ThrowIfCancellationRequested();

                    try
                    {
                        if (loopCount >= maxRetryCount)
                        {
                            break;
                        }

                        loopCount++;
                        controller.Refresh();
                        controller.WaitForStatus(ServiceControllerStatus.Running, pollTimeout);
                    }
                    catch (System.ServiceProcess.TimeoutException)
                    {

                    }
                }

                if (controller.Status != ServiceControllerStatus.Running)
                {
                    throw new System.TimeoutException(
                        $"Failed to start {serviceName} service after {maxRetryCount} retries. " +
                        $"Current state is {controller.Status}.");
                }
            }
        }

        /// <summary>
        /// Start the PS services
        /// </summary>
        public static Tuple<bool, string> TryStartProcessServerServices(
            RcmOperationContext opContext,
            TraceSource traceSource)
        {
            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Attempting to start the Process Server services");

            bool successfullyStartedAll = true;
            var overallFailedServices = new List<string>();

            // TODO-SanKumar-1904: Move these services list to Settings.
            // PSMonitor must always be the last to start, otherwise it might
            // end up reporting that services are not running.
            var servicesToStartInOrder = new[] { "cxprocessserver", "ProcessServer", "ProcessServerMonitor" };

            var startTimeout = Settings.Default.Setup_DefaultServicesStartTimeout;
            var pollTimeout = Settings.Default.Setup_DefaultServicesPollChangeTimeout;
            var maxRetryCount = Settings.Default.Setup_ServiceStartMaxRetryCnt;
            var retryInterval = Settings.Default.Setup_ServiceStartRetryInterval;

            foreach (var currSvcName in servicesToStartInOrder)
            {
                for (int retryCnt = 0; ; retryCnt++)
                {
                    try
                    {
                        traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            opContext,
                            "Attempting to start the service : {0}",
                            currSvcName);

                        var sw = Stopwatch.StartNew();

                        using (var currSvcCtrl = new ServiceController(currSvcName))
                        {
                            if (currSvcCtrl.Status != ServiceControllerStatus.StartPending &&
                                currSvcCtrl.Status != ServiceControllerStatus.Running)
                            {
                                currSvcCtrl.Start();
                                currSvcCtrl.Refresh();
                            }

                            do
                            {
                                try
                                {
                                    currSvcCtrl.WaitForStatus(ServiceControllerStatus.Running, pollTimeout);
                                }
                                catch (System.ServiceProcess.TimeoutException)
                                {
                                }

                                currSvcCtrl.Refresh();
                            } while (currSvcCtrl.Status != ServiceControllerStatus.Running && sw.Elapsed < startTimeout);

                            if (currSvcCtrl.Status != ServiceControllerStatus.Running)
                            {
                                traceSource?.TraceAdminLogV2Message(
                                    TraceEventType.Error,
                                    opContext,
                                    "Failed to start the service : {0} within {1}. Current state : {2}.",
                                    currSvcName, startTimeout, currSvcCtrl.Status);

                                successfullyStartedAll = false;
                            }
                        }
                        break;
                    }
                    catch(Exception ex)
                    {
                        if (retryCnt >= maxRetryCount)
                        {
                            traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            opContext,
                            "Failed to start the service : {0} even retrying for {1} times with exeption {2}{3}",
                            currSvcName, maxRetryCount, Environment.NewLine, ex);

                            overallFailedServices.Add(currSvcName);
                            successfullyStartedAll = false;

                            break;
                        }

                        Thread.Sleep(retryInterval);
                    }

                    // Continuing to attempt to start the next service.
                }
            }

            var failedServices = string.Empty;
            if (overallFailedServices != null && overallFailedServices.Count > 0)
            {
                failedServices = string.Join(",", overallFailedServices);
            }

            traceSource?.TraceAdminLogV2Message(
                successfullyStartedAll? TraceEventType.Information : TraceEventType.Error,
                opContext,
                "{0} in starting the Process Server services {1}",
                successfullyStartedAll ? "Succeeded" : "Failed", failedServices);

            return new Tuple<bool, string>(successfullyStartedAll, failedServices);
        }

        /// <summary>
        /// Stop the PS services
        /// </summary>
        public static void StopProcessServerServices(RcmOperationContext opContext, TraceSource traceSource)
        {
            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Stopping Process Server services");

            // TODO-SanKumar-1904: Add retries on any failure.

            // TODO-SanKumar-1904: Move these services list to Settings.
            // PSMonitor must always be the first to stop, otherwise it might
            // end up reporting that services are not running.
            var servicesToStopInOrder = new[] { "ProcessServerMonitor", "ProcessServer", "cxprocessserver" };

            var stopTimeout = Settings.Default.Setup_DefaultServicesStopTimeout;
            var pollTimeout = Settings.Default.Setup_DefaultServicesPollChangeTimeout;

            foreach (var currSvcName in servicesToStopInOrder)
            {
                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    "Stopping the service : {0}",
                    currSvcName);

                var sw = Stopwatch.StartNew();

                using (var currSvcCtrl = new ServiceController(currSvcName))
                {
                    if (currSvcCtrl.Status != ServiceControllerStatus.Stopped &&
                        currSvcCtrl.Status != ServiceControllerStatus.StopPending)
                    {
                        currSvcCtrl.Stop();
                        currSvcCtrl.Refresh();
                    }

                    do
                    {
                        try
                        {
                            currSvcCtrl.WaitForStatus(ServiceControllerStatus.Stopped, pollTimeout);
                        }
                        catch (System.ServiceProcess.TimeoutException)
                        {
                        }

                        currSvcCtrl.Refresh();
                    } while (currSvcCtrl.Status != ServiceControllerStatus.Stopped && sw.Elapsed < stopTimeout);

                    if (currSvcCtrl.Status != ServiceControllerStatus.Stopped)
                    {
                        traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            opContext,
                            "Failed to stop {0} service within {1}. Current state: {2}.",
                            currSvcName, stopTimeout, currSvcCtrl.Status);

                        throw new System.TimeoutException(
                            $"Failed to stop {currSvcName} service within {stopTimeout}. " +
                            $"Current state: {currSvcCtrl.Status}.");
                    }

                    // TODO-SanKumar-1904: Kill the process hosting the service on timeout and after
                    // retries. This mustn't be done for hosted services, which could be sharing the
                    // process with multiple services.
                }

                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    "Stopped the service : {0}",
                    currSvcName);
            }

            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Successfully stopped all the Process Server services");
        }

        /// <summary>
        /// Stop a service
        /// </summary>
        /// <param name="serviceName">Service to be stopped</param>
        /// <param name="token">Cancellation token</param>
        public static void StopService(string serviceName, CancellationToken token)
        {
            StopServiceWithRetries(serviceName: serviceName,
                maxRetryCount: Settings.Default.PSMonitor_DefaultServicesMaxRetryCount,
                pollTimeout: Settings.Default.PSMonitor_DefaultServicesPollChangeTimeout,
                ct: token);
        }

        /// <summary>
        /// Stop a service
        /// </summary>
        /// <param name="serviceName">Service to be stopped</param>
        /// <param name="maxRetryCount">Max number of times service stop will be attempted</param>
        /// <param name="pollTimeout">Polling interval to check service status</param>
        /// <param name="ct">Cancellation token</param>
        public static void StopServiceWithRetries(
            string serviceName, int maxRetryCount, TimeSpan pollTimeout, CancellationToken ct)
        {
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
                    ct.ThrowIfCancellationRequested();

                    try
                    {
                        if (loopCount >= maxRetryCount)
                        {
                            break;
                        }

                        loopCount++;
                        controller.Refresh();
                        controller.WaitForStatus(ServiceControllerStatus.Stopped, pollTimeout);
                    }
                    catch (System.ServiceProcess.TimeoutException)
                    {

                    }
                }

                if (controller.Status != ServiceControllerStatus.Stopped)
                {
                    throw new System.TimeoutException(
                        $"Failed to stop {serviceName} service after {maxRetryCount} retries. " +
                        $"Current state is {controller.Status}.");
                }
            }
        }

        /// <summary>
        /// Checks if a service is installed on the system
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        /// <returns>True if the service is installed, false otherwise</returns>
        public static bool IsServiceInstalled(string serviceName)
        {
            return ServiceController.GetServices().Any(
                service => service.ServiceName.Equals(
                    serviceName, StringComparison.OrdinalIgnoreCase));
        }

        /// <summary>
        /// Check if the given service is in disired state or not
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        /// <returns>True if the service is in expected state,
        /// false otherwise</returns>
        public static bool IsServiceInExpectedState(
            string serviceName,
            ServiceControllerStatus expectedState,
            TraceSource traceSource)
        {
            bool status = false;

            TaskUtils.RunAndIgnoreException(() =>
            {
                using (var controller = new ServiceController(serviceName))
                {
                    var state = controller.Status;

                    if (state == expectedState)
                    {
                        status = true;
                    }
                }
            }, traceSource);

            return status;
        }
    }
}
