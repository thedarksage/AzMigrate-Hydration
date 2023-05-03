using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.ServiceProcess;
using System.Threading;
using System.Threading.Tasks;
using LoggerInterface;
using Microsoft.Azure.HybridServices.Metrics;
using RcmAgentCommonLib.Config.Utilities;
using RcmAgentCommonLib.ErrorHandling;
using RcmAgentCommonLib.Logger.Diagnostics;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using RcmAgentCommonLib.Utilities;
using MarsAgent.Config;
using MarsAgent.Config.CmdLineInputs;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.Constants;
using MarsAgent.ErrorHandling;
using MarsAgent.LoggerInterface;
using MarsAgent.Service;
using MarsAgent.Monitoring;
using Microsoft.ServiceBus;

namespace MarsAgent.Main
{
    public static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        /// <param name="args">Input arguments.</param>
        /// <returns>Returns success or failure based on operation status.</returns>
        public static int Main(string[] args)
        {
            //System.Threading.Thread.Sleep(30000);

            bool launchedAsService = args == null || args.Length == 0;

            Logger.Instance.InitializeLogContext(
                ServiceProperties.Instance.DefaultMachineIdentifier,
                WellKnownActivityId.MarsAgentServiceActivityId);

            ServiceConfigurationSettings config = Configurator.Instance.GetConfig();
            Logger.Instance.MergeLogContext(new LogContext
            {
                ApplianceId = config.MachineIdentifier,
                SubscriptionId = config.SubscriptionId,
                ContainerId = config.ContainerId,
                ResourceId = config.ResourceId,
                ResourceLocation = config.ResourceLocation
            });

            RcmAgentDiagnostics.Initialize(
                ServiceProperties.Instance,
                ServiceTunables.Instance,
                Configurator.Instance,
                Logger.Instance,
                useConsoleWriter: !launchedAsService);

            IMetricsEmitter metricsEmitter = new MarsAgentMetricsEmitter();
            MarsAgentMetrics.Initialize(metricsEmitter);
            Logger.Instance.SetMetricsEmitter(metricsEmitter);

            ServiceControllerHelper.Initialize(
                ServiceTunables.Instance,
                Logger.Instance,
                ServiceProperties.Instance);

            if (ServiceTunables.Instance.SkipServerSslCertValidation)
            {
                ServicePointManager.ServerCertificateValidationCallback +=
                    new RemoteCertificateValidationCallback(ValidateCertificate);
            }

            Logger.Instance.LogInfo(
                                    CallInfo.Site(),
                                    "Default SecurityProtocol : {0}, ConnectivityMode : {1}",
                                    ServicePointManager.SecurityProtocol.ToString(),
                                    ServiceBusEnvironment.SystemConnectivity.Mode.ToString());

            // Setting up the SecurityProtocol and ServiceBusConnectivityMode to be used
            // for network communications.
            ServicePointManager.SecurityProtocol = ServiceTunables.Instance.SecurityProtocolType;
            ServiceBusEnvironment.SystemConnectivity.Mode = ConnectivityMode.Https;

            Logger.Instance.LogInfo(
                                    CallInfo.Site(),
                                    "Modified SecurityProtocol : {0}, ConnectivityMode : {1}",
                                    ServicePointManager.SecurityProtocol.ToString(),
                                    ServiceBusEnvironment.SystemConnectivity.Mode.ToString());

            ServiceBase[] servicesToRun = new ServiceBase[]
            {
                new MarsAgentService()
            };

            if (launchedAsService)
            {
                // Run the services through SCM. This assumes the services have been installed
                // beforehand.
                ServiceBase.Run(servicesToRun);
            }
            else
            {
                try
                {
                    var parser = new CmdLineParser<LogContext>(
                        args,
                        Logger.Instance,
                        ServiceProperties.Instance);

                    switch (parser.ActionName.ToLower())
                    {
                        case nameof(SupportedActions.runinteractive):
                            RunInteractively(servicesToRun);
                            break;

                        case nameof(SupportedActions.install):
                            break;

                        case nameof(SupportedActions.uninstall):
                            break;

                        case nameof(SupportedActions.configure):
                            ConfigureInput configureInput;
                            try
                            {
                                configureInput = parser.CreateInputObject<ConfigureInput>();
                            }
                            catch (Exception)
                            {
                                Logger.Instance.LogError(
                                    CallInfo.Site(),
                                    "Syntax error.\n" +
                                    parser.ShowUsage<ConfigureInput>(
                                        ServiceProperties.Instance.ServiceExecutableName,
                                        parser.ActionName.ToLower()));
                                throw;
                            }

                            Configurator.Instance.Configure(configureInput);
                            break;

                        case nameof(SupportedActions.register):
                            RegisterInput registerInput;
                            try
                            {
                                registerInput = parser.CreateInputObject<RegisterInput>();
                            }
                            catch (Exception)
                            {
                                Logger.Instance.LogError(
                                    CallInfo.Site(),
                                    "Syntax error.\n" +
                                    parser.ShowUsage<RegisterInput>(
                                        ServiceProperties.Instance.ServiceExecutableName,
                                        parser.ActionName.ToLower()));
                                throw;
                            }

                            Configurator.Instance.Register(registerInput);
                            break;

                        case nameof(SupportedActions.updatewebproxy):
                            UpdateWebProxyInput updateWebProxyInput;
                            try
                            {
                                updateWebProxyInput = parser.CreateInputObject<UpdateWebProxyInput>();
                            }
                            catch (Exception)
                            {
                                Logger.Instance.LogError(
                                    CallInfo.Site(),
                                    "Syntax error.\n" +
                                    parser.ShowUsage<UpdateWebProxyInput>(
                                        ServiceProperties.Instance.ServiceExecutableName,
                                        parser.ActionName.ToLower()));
                                throw;
                            }

                            Configurator.Instance.UpdateWebProxy(updateWebProxyInput);
                            break;

                        case nameof(SupportedActions.unregister):
                            UnregisterInput unregisterInput;
                            try
                            {
                                unregisterInput = parser.CreateInputObject<UnregisterInput>();
                            }
                            catch (Exception)
                            {
                                Logger.Instance.LogError(
                                    CallInfo.Site(),
                                    "Syntax error.\n" +
                                    parser.ShowUsage<UnregisterInput>(
                                        ServiceProperties.Instance.ServiceExecutableName,
                                        parser.ActionName.ToLower()));
                                throw;
                            }

                            Configurator.Instance.Unregister(unregisterInput);
                            break;

                        case nameof(SupportedActions.unconfigure):
                            UnconfigureInput unconfigureInput;
                            try
                            {
                                unconfigureInput = parser.CreateInputObject<UnconfigureInput>();
                            }
                            catch (Exception)
                            {
                                Logger.Instance.LogError(
                                    CallInfo.Site(),
                                    "Syntax error.\n" +
                                    parser.ShowUsage<UnconfigureInput>(
                                        ServiceProperties.Instance.ServiceExecutableName,
                                        parser.ActionName.ToLower()));
                                throw;
                            }
                            Configurator.Instance.Unconfigure(unconfigureInput);
                            break;

                        case nameof(SupportedActions.testprotsvcconnection):
                            MarsAgentApi marsAgentApi = new MarsAgentApi();
                            marsAgentApi.CheckAgentHealth();

                            break;

                        default:
                            throw new NotSupportedException();
                    }
                }
                catch (Exception e)
                {
                    Logger.Instance.LogException(CallInfo.Site(), e);

                    RcmAgentError rcmAgentError = e.ToRcmAgentError(
                        Logger.Instance.GetAcceptLanguage());

                    Logger.Instance.LogError(
                        CallInfo.Site(),
                        $"Operation '{string.Join(" ", args)}' failed with error\n" +
                        $"'{rcmAgentError}'.\n");

                    Console.Error.WriteLine(rcmAgentError);

                    return 1;
                }
            }

            return 0;
        }

        /// <summary>
        /// To validate certificate.
        /// </summary>
        /// <param name="sender">Sender of cert.</param>
        /// <param name="cert">X509 Certificate.</param>
        /// <param name="chain">X509 chain.</param>
        /// <param name="sslPolicyErrors">Policy errors.</param>
        /// <returns>True for validating cert.</returns>
        public static bool ValidateCertificate(
            object sender,
            X509Certificate cert,
            X509Chain chain,
            SslPolicyErrors sslPolicyErrors)
        {
            return true;
        }

        /// <summary>
        /// Runs specified set of services in an interactive mode for development purposes.
        /// </summary>
        /// <param name="servicesToRun">The set of services to be run.</param>
        private static void RunInteractively(ServiceBase[] servicesToRun)
        {
            ManualResetEvent quitEvent = new ManualResetEvent(false);
            Console.CancelKeyPress += (sender, eArgs) =>
            {
                quitEvent.Set();
                eArgs.Cancel = true;
            };

            Logger.Instance.LogInfo(CallInfo.Site(), "Starting all services...");
            List<Task> tasks = new List<Task>();
            foreach (var s in servicesToRun)
            {
                tasks.Add(Task.Run(() =>
                {
                    Logger.Instance.LogInfo(
                        CallInfo.Site(),
                        $"Starting service {s.GetType()}...");
                    ((IServiceControl)s).StartInternal();
                    Logger.Instance.LogInfo(
                        CallInfo.Site(),
                        $"Completed start for service {s.GetType()}.");
                }));
            }

            Logger.Instance.LogInfo(
                CallInfo.Site(),
                "Initiated start for all services.");
            Console.WriteLine("Hit Ctrl+C or Ctrl+Break to stop...");
            quitEvent.WaitOne();

            Logger.Instance.LogInfo(CallInfo.Site(), "Stopping all services...");
            foreach (var s in servicesToRun)
            {
                tasks.Add(Task.Run(() =>
                {
                    Logger.Instance.LogInfo(
                        CallInfo.Site(),
                        $"Stopping service {s.GetType()}...");
                    ((IServiceControl)s).StopInternal();
                    Logger.Instance.LogInfo(
                        CallInfo.Site(),
                        $"Completed stop for service {s.GetType()}.");
                }));
            }

            Logger.Instance.LogInfo(
                CallInfo.Site(),
                "Initiated stop for all services.");
            Logger.Instance.LogInfo(
                CallInfo.Site(),
                "Waiting for all services to stop...");

            try
            {
                Task.WaitAll(tasks.ToArray());
            }
            catch
            {
                // Ignore any wait failures. Its time to stop.
            }

            Console.WriteLine("Press any key to exit...");
            Console.ReadKey(intercept: true);
        }
    }
}
