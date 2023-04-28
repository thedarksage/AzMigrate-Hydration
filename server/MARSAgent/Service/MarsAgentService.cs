using System;
using System.Diagnostics;
using System.ServiceProcess;
using System.Threading;
using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using RcmAgentCommonLib.Utilities;
using RcmContract;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.Constants;
using MarsAgent.LoggerInterface;
using MarsAgent.Utilities;
using MarsAgent.Service.CBEngine;
using MarsAgent.CBEngine.Constants;

namespace MarsAgent.Service
{
    /// <summary>
    /// RCM replication agent service.
    /// </summary>
    public partial class MarsAgentService : ServiceBase, IServiceControl
    {
        /// <summary>
        /// Service exit code in case an agent task becomes unresponsive.
        /// </summary>
        /// <remarks>
        /// OS will restart the service (if restart on failure configured) if the exit code
        /// is non-zero.
        /// </remarks>
        private const int TaskUnresponsiveExitCode = 101;

        /// <summary>
        /// Max wait timeout to request SCM to wait on service OnStop call.
        /// </summary>
        private static readonly int ServiceStopWaitTimeOut =
            (int)TimeSpan.FromMinutes(5).TotalMilliseconds;

        /// <summary>
        /// Used in signaling stop.
        /// </summary>
        private readonly CancellationTokenSource cts = new CancellationTokenSource();

        /// <summary>
        /// Heartbeat monitor task to monitor the service.
        /// </summary>
        private IRcmAgentHeartbeatMonitor heartbeatMonitor;

        /// <summary>
        /// Initializes a new instance of the <see cref="MarsAgentService" />
        /// class.
        /// </summary>
        public MarsAgentService()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// This method executes when a start command is sent to the service by the
        /// Service Control Manager or when the operating system starts (for a service that
        /// starts automatically).
        /// </summary>
        public void StartInternal()
        {
            if (ServiceTunables.Instance.DebuggerBreakOnStart)
            {
                Stopwatch sw = new Stopwatch();
                sw.Start();

                while (sw.Elapsed < ServiceTunables.Instance.MaxWaitTimeForDebuggerAttachOnStart)
                {
                    Debugger.Launch();

                    if (Debugger.IsAttached)
                    {
                        break;
                    }

                    Thread.Sleep(TimeSpan.FromSeconds(1));
                }

                Debugger.Break();
            }

            Logger.Instance.InitializeLogContext(
                ServiceProperties.Instance.DefaultMachineIdentifier,
                WellKnownActivityId.MarsAgentServiceActivityId);

            ServiceConfigurationSettings svcConfig =
                Configurator.Instance.GetConfig();
            Configurator.Instance.SetDefaultWebProxy();

            Logger.Instance.MergeLogContext(new LogContext
            {
                ApplianceId = svcConfig.MachineIdentifier,
                SubscriptionId = svcConfig.SubscriptionId,
                ContainerId = svcConfig.ContainerId,
                ResourceId = svcConfig.ResourceId,
                ResourceLocation = svcConfig.ResourceLocation
            });

            MarsSettings componentSettings =
                ServiceSettings.Instance.GetSettings();
            Logger.Instance.SetServiceDefaults(
                deploymentName: svcConfig.MachineIdentifier,
                serviceName: ServiceProperties.Instance.ComponentName,
                stampName: componentSettings?.RcmStampName,
                region: string.Empty);

            Logger.Instance.LogEvent(
                CallInfo.Site(),
                RcmAgentEvent.ServiceStarted(ServiceProperties.Instance.ServiceVersion));

            if (!svcConfig.IsConfigured)
            {
                Logger.Instance.LogInfo(
                    CallInfo.Site(),
                    $"{ServiceProperties.Instance.ServiceName} is not yet configured.");

                return; // <---- Early return.
            }

            while (!this.cts.IsCancellationRequested)
            {
                try
                {
                    using (new SingletonApp(
                        Logger.Instance,
                        ServiceProperties.Instance,
                        lockSuffix: "ServiceLock"))
                    {
                        this.Run();
                    }
                }
                catch (Exception e)
                {
                    Logger.Instance.LogException(CallInfo.Site(), e);
                }

                this.cts.Token.WaitHandle.WaitOne(TimeSpan.FromSeconds(1));
            }
        }

        /// <summary>
        /// Service execution logic.
        /// </summary>
        private void Run()
        {
            bool tasksInitialized = false;
            while (!this.cts.IsCancellationRequested)
            {
                try
                {
                    if (this.heartbeatMonitor == null)
                    {
                        this.heartbeatMonitor = new RcmAgentHeartbeatMonitor<LogContext>(
                            ServiceTunables.Instance,
                            new RcmAgentTaskLogContext(),
                            Logger.Instance);
                    }

                    if (this.heartbeatMonitor.IsStartRequested())
                    {
                        Logger.Instance.LogInfo(
                            CallInfo.Site(),
                            $"Sending start request to task " +
                            $"'{this.heartbeatMonitor.Identifier}'.");

                        this.heartbeatMonitor.SendStartRequest();
                    }

                    if (this.heartbeatMonitor.IsUnResponsive)
                    {
                        Logger.Instance.LogError(
                            CallInfo.Site(),
                            $" task '{this.heartbeatMonitor.Identifier}' is unresponsive !!!");

                        AgentMetrics.LogUnresponsiveTask(this.heartbeatMonitor.Identifier);

                        if (ServiceTunables.Instance.FailFastOnUnresponsiveTask)
                        {
                            Environment.FailFast(
                                $"Unresponsive tasks: '{this.heartbeatMonitor.Identifier}'.");
                        }
                        else
                        {
                            Environment.Exit(TaskUnresponsiveExitCode);
                        }
                    }

                    if (!tasksInitialized)
                    {
                        this.InitializeTasks();
                        tasksInitialized = true;
                    }
                }
                catch (Exception e)
                {
                    Logger.Instance.LogException(
                        CallInfo.Site(),
                        e,
                        $"StartInternal threw exception.");
                }

                this.cts.Token.WaitHandle.WaitOne(TimeSpan.FromSeconds(1));
            }
        }

        /// <summary>
        /// This method executes when a stop command is sent to the service by the
        /// Service Control Manager.
        /// </summary>
        public void StopInternal()
        {
            if (this.heartbeatMonitor != null && this.heartbeatMonitor.IsAlive)
            {
                this.heartbeatMonitor.Shutdown();
            }

            this.cts.Cancel();
        }

        /// <summary>
        /// This method executes when a start command is sent to the service by the
        /// Service Control Manager or when the operating system starts (for a service that
        /// starts automatically).
        /// </summary>
        /// <param name="args">Data passed by the start command.</param>
        protected override void OnStart(string[] args)
        {
            ThreadPool.QueueUserWorkItem(unused => this.StartInternal());
        }

        /// <summary>
        /// This method executes when a stop command is sent to the service by the
        /// Service Control Manager.
        /// </summary>
        protected override void OnStop()
        {
            this.RequestAdditionalTime(ServiceStopWaitTimeOut);
            this.StopInternal();

            Logger.Instance.LogEvent(
                CallInfo.Site(),
                RcmAgentEvent.ServiceStopped(ServiceProperties.Instance.ServiceVersion));
        }
        
        // <summary>
        // Initializes the tasks to be monitored.
        // </summary>
        private void InitializeTasks()
        {
            RcmAgentTaskLogContext logContext = new RcmAgentTaskLogContext();
            this.heartbeatMonitor.AddTask(
                new RcmAgentFetchSettingsTask<ServiceConfigurationSettings, MarsSettings, LogContext>(
                    ServiceTunables.Instance,
                    logContext,
                    Logger.Instance,
                    Configurator.Instance,
                    Configurator.Instance,
                    ServiceSettings.Instance,
                    ServiceHealth.Instance));

            this.heartbeatMonitor.AddTask(new ConsumeSettingsTask(
                ServiceTunables.Instance,
                logContext,
                Logger.Instance,
                Configurator.Instance,
                ServiceSettings.Instance,
                this.heartbeatMonitor));

            this.heartbeatMonitor.AddTask(
                new RegisterComponentTask<ServiceConfigurationSettings, MarsSettings, LogContext>(
                    ServiceTunables.Instance,
                    logContext,
                    Logger.Instance,
                    Configurator.Instance,
                    Configurator.Instance,
                    ServiceSettings.Instance));

            this.heartbeatMonitor.AddTask(
                new RcmAgentLogUploadTask<ServiceConfigurationSettings, MarsSettings, LogContext>(
                    ServiceTunables.Instance,
                    logContext,
                    Logger.Instance,
                    Configurator.Instance,
                    ServiceSettings.Instance,
                    ServiceProperties.Instance));

            this.heartbeatMonitor.AddTask(new ReportComponentHealthTask(
                ServiceTunables.Instance,
                logContext,
                Logger.Instance,
                Configurator.Instance,
                ServiceSettings.Instance,
                ServiceProperties.Instance,
                ServiceHealth.Instance));

            this.heartbeatMonitor.AddTask(new MonitorCBEngineLogsTask(
                ServiceTunables.Instance,
                logContext,
                Logger.Instance));

            this.heartbeatMonitor.AddTask(
                new CBEngineLogUploadTask<ServiceConfigurationSettings, MarsSettings, LogContext>(
                    ServiceTunables.Instance,
                    LogTunables.Instance,
                    logContext,
                    Logger.Instance,
                    Configurator.Instance,
                    ServiceSettings.Instance,
                    ServiceProperties.Instance));
        }
    }
}
