using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Threading;
using System.Threading.Tasks;

using PSCoreSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.Settings;
using PSMonSettings = Microsoft.Azure.SiteRecovery.ProcessServerMonitor.Settings;

namespace ProcessServerMonitor
{
    public partial class ProcessServerMonitor : ServiceBase
    {
        /// <summary>
        /// The cancellation token used to cooperatively cancel running tasks.
        /// </summary>
        private readonly CancellationTokenSource m_cts;

        /// <summary>
        /// The task monitoring the system perf and service status.
        /// </summary>
        private Task m_psTask;

        private SystemMonitor m_sysMonitor;

        private PerfCollector m_perfCollector;

        private ProcessInvoker m_procInvoker;

        private static IEnumerable<TraceSource> GetAllTraceSources() =>
            Constants.GetAllTraceSources().Concat(Tracers.GetAllTraceSources());

        public ProcessServerMonitor()
        {
            m_cts = new CancellationTokenSource();
            InitializeComponent();
        }

        /// <summary>
        /// Implements service continue operation.
        /// </summary>
        protected override void OnContinue()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Implements service OnCustomCommand.
        /// </summary>
        protected override void OnCustomCommand(int command)
        {
            base.OnCustomCommand(command);
        }

        /// <summary>
        /// Implements service pause operation.
        /// </summary>
        protected override void OnPause()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Implements service power event.
        /// </summary>
        protected override bool OnPowerEvent(PowerBroadcastStatus powerStatus)
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Implements service session change event.
        /// </summary>
        protected override void OnSessionChange(SessionChangeDescription changeDescription)
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Implements service shutdown operation.
        /// </summary>
        protected override void OnShutdown()
        {
            base.OnShutdown();
        }

        internal static void UnhandledExceptionHandler(object sender, UnhandledExceptionEventArgs args)
        {
            if (Debugger.IsAttached)
                Debugger.Break();

            Constants.s_psMonMain.TraceAdminLogV2Message(
                TraceEventType.Error,
                "Runtime terminating : {0} due to Unhandled exception : {1}",
                args.IsTerminating, args.ExceptionObject);

            // Indicate to trace listeners to wind down, giving a chance for
            // them to cleanly persist buffered data, etc.

            // NOTE-SanKumar-2004: The taks run in the default thread pool,
            // which consists of background threads, so they simply get aborted
            // when the process quits without blocking the service stop. So,
            // if any task is running at this point, there may be a chance that
            // its logs around this time are missed.
            Tracers.TryFinalizeTraceListeners(GetAllTraceSources());
        }

        /// <summary>
        /// Implements service start operation.
        /// </summary>
        protected override void OnStart(string[] args)
        {
#if DEBUG
            Stopwatch sw = new Stopwatch();
            sw.Start();

            while (sw.Elapsed < PSMonSettings.Default.Debugger_MaxWaitTimeForAttachOnStart)
            {
                Debugger.Launch();

                if (Debugger.IsAttached)
                    break;

                Thread.Sleep(TimeSpan.FromSeconds(1));
            }

            if (PSMonSettings.Default.Debugger_BreakOnStart)
                Debugger.Break();
#endif

            bool successfullyStarted = false;

            try
            {
                // NOTE-SanKumar-1908: Enable this by using the PSCore DLL setting
                // MiscUtils_AllDllsLoadTest_CSMode and run once privately before
                // committing; in order to validate
                // 1. The right DLL is packaged in the MSI
                // 2. The right DLL version is referred by the project, since we
                // mark Specific Version = true
                // 3. The right DLL version is provided in assembly manifest's
                // DLL redirection.

                MiscUtils.AllDllsLoadTest();

                Constants.s_psMonMain.TraceAdminLogV2Message(
                    TraceEventType.Information, "Service starting");

                SystemUtils.SetConnectivityModeAndSecurityProtocol(Constants.s_psMonMain);

                ProcessServerSettings.TunablesChanged += (sender, tunablesChangedEventArgs) =>
                {
                    TunablesBasedSettingsProvider.OnTunablesChanged(
                        tunablesChangedEventArgs?.NewTunables, PSMonSettings.Default, PSCoreSettings.Default);
                };

                if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                {
                    IdempotencyContext.RestoreTransactedSavedFilesAsync(exclLockAcquired: false, ct: CancellationToken.None)
                        .GetAwaiter().GetResult();
                }

                m_psTask = TaskUtils.RunTaskForAsyncOperation(
                    Constants.s_psMonMain,
                    "Process Server Monitor main",
                    m_cts.Token,
                    async (mainCancToken) =>
                    {
                        await PSInstallationInfo.Default.
                            BlockUntilValidInstallationDetected(includeFirstTimeConfigure: true, ct: mainCancToken).
                            ConfigureAwait(false);

                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information, $"CS Mode : {PSInstallationInfo.Default.CSMode}");

                        var taskList = new List<Task>();
                        bool spawnSecondaryPoller = true;

                        // TODO-SiSunkar-1908:  Use Lazy<SystemMonitor> object
                        // to be resilient to any initialization failures that
                        // would lead to crash.
                        m_sysMonitor = new SystemMonitor();

                        if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
                        {
                            Task upgradeTask = TaskUtils.RunTaskForAsyncOperation(
                                traceSource: Constants.s_psMonMain,
                                name: $"{nameof(SystemMonitor)} - Upgrade Task",
                                ct: mainCancToken,
                                operation: m_sysMonitor.UpgradeTasks);

                            await upgradeTask.ConfigureAwait(false);
                        }

                        Task systemMonitorTask = TaskUtils.RunTaskForAsyncOperation(
                            traceSource: Constants.s_psMonMain,
                            name: nameof(SystemMonitor),
                            ct: mainCancToken,
                            operation: m_sysMonitor.MonitorSystem);

                        taskList.Add(systemMonitorTask);

                        m_perfCollector = new PerfCollector();

                        // The task collecting and pushing the custom perfs to windows perfs.
                        Task perfCollectorTask = TaskUtils.RunTaskForAsyncOperation(
                                traceSource: Constants.s_psMonMain,
                                name: nameof(PerfCollector),
                                ct: mainCancToken,
                                operation: m_perfCollector.CollectPerfs);

                        taskList.Add(perfCollectorTask);

                        if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                        {
                            m_procInvoker = new ProcessInvoker(
                                PSMonSettings.Default.EvtCollForwPath,
                                PSMonSettings.Default.EvtCollForwArgs,
                                PSMonSettings.Default.EvtCollForwProcName);

                            Task monitorProcessTask = TaskUtils.RunTaskForAsyncOperation(
                                traceSource: Constants.s_psMonMain,
                                name: nameof(ProcessInvoker),
                                ct: mainCancToken,
                                operation: m_procInvoker.MonitorProcess);

                            taskList.Add(monitorProcessTask);
                        }
                        else if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
                        {
                        }
                        else
                        {
                            taskList.Add(Task.Delay(Timeout.Infinite, mainCancToken));

                            spawnSecondaryPoller = false;
                        }

                        if (spawnSecondaryPoller)
                        {
                            Task settingsSecondaryPollerTask = TaskUtils.RunTaskForAsyncOperation(
                                            traceSource: Constants.s_psMonMain,
                                            name: "PS Settings - Secondary Poller",
                                            ct: mainCancToken,
                                            operation: async (mastPollerCancToken) =>
                                            await ProcessServerSettings.RunSecondaryPoller(
                                                PSInstallationInfo.Default.GetPSCachedSettingsFilePath(),
                                                ct: mastPollerCancToken)
                                            .ConfigureAwait(false)); ;

                            taskList.Add(settingsSecondaryPollerTask);
                        }

                        await Task.WhenAll(taskList).ConfigureAwait(false);
                    });

                base.OnStart(args);

                Constants.s_psMonMain.TraceAdminLogV2Message(
                    TraceEventType.Information, "Service started");

                successfullyStarted = true;
            }
            finally
            {
                // Indicate to trace listeners to wind down, giving a chance for
                // them to cleanly persist buffered data, etc.

                // NOTE-SanKumar-2004: The taks run in the default thread pool,
                // which consists of background threads, so they simply get aborted
                // when the process quits without blocking the service stop. So,
                // if any task triggerred above is running at this point, there
                // may be a chance that its logs around this time are missed.
                if (!successfullyStarted)
                    Tracers.TryFinalizeTraceListeners(GetAllTraceSources());
            }
        }

        /// <summary>
        /// Implements service stop operation.
        /// </summary>
        protected override void OnStop()
        {
            Constants.s_psMonMain.TraceAdminLogV2Message(
                TraceEventType.Information, "Service stopping");

            try
            {
                try
                {
                    if (m_cts != null)
                    {
                        m_cts.Cancel();
                    }

                    if (m_psTask != null)
                    {
                        m_psTask.Wait();
                    }
                }
                catch (AggregateException ae)
                {
                    bool setError = true;

                    foreach (Exception e in ae.Flatten().InnerExceptions)
                    {
                        // Setting error if the current cancelled exception is not related to system monitor
                        if (e is TaskCanceledException tcEx && tcEx.Task == m_psTask)
                        {
                            Constants.s_psMonMain.TraceAdminLogV2Message(
                                TraceEventType.Verbose, "Process Server main task is cancelled");
                            setError = false;
                        }
                    }

                    if (setError)
                    {
                        Constants.s_psMonMain.TraceAdminLogV2Message(
                                                            TraceEventType.Error,
                                                            "Exception occured while trying to stop service : {0}",
                                                            ae.Flatten());
                    }
                }
                catch (Exception e)
                {
                    Constants.s_psMonMain.TraceAdminLogV2Message(
                                TraceEventType.Error,
                                "Exception occured while trying to stop service : {0}", e);
                }

                base.OnStop();

                Constants.s_psMonMain.TraceAdminLogV2Message(
                    TraceEventType.Information, "Service stopped");
            }
            finally
            {
                // Indicate to trace listeners to wind down, giving a chance for
                // them to cleanly persist buffered data, etc.

                // NOTE-SanKumar-2004: The taks run in the default thread pool,
                // which consists of background threads, so they simply get aborted
                // when the process quits without blocking the service stop. So,
                // if any task isn't properly waited to be completed above, there
                // may be a chance that its logs around this time are missed.
                Tracers.TryFinalizeTraceListeners(GetAllTraceSources());
            }
        }
    }
}
