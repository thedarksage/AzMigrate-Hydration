using Microsoft.ApplicationInsights.Extensibility;
using Microsoft.ApplicationInsights.WindowsServer.TelemetryChannel;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.TaskHandlers;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Threading;
using System.Threading.Tasks;

using ProcessServerSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.ProcessServerSettings;
using ProcServSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Settings;
using PSCore = Microsoft.Azure.SiteRecovery.ProcessServer.Core;
using PSCoreSettings = Microsoft.Azure.SiteRecovery.ProcessServer.Core.Settings;

namespace ProcessServer
{
    public partial class ProcessServer : ServiceBase
    {
        /// <summary>
        /// The cancellation token used to cooperatively cancel running tasks.
        /// </summary>
        private readonly CancellationTokenSource m_cts = new CancellationTokenSource();

        /// <summary>
        /// The main process server task.
        /// </summary>
        private Task m_psTask;

        // <summary>
        /// The task to monitor health service.
        /// </summary>
        private MonitorHealthService m_monitorHealthService;

        /// <summary>
        /// Volsync object to move files in legacy stack
        /// </summary>
        private Volsync m_volsync;

        private static IEnumerable<TraceSource> GetAllTraceSources() =>
            Tracers.GetAllTraceSources().Concat(PSCore.Tracing.Tracers.GetAllTraceSources());

        /// <summary>
        /// Constructor.
        /// </summary>
        public ProcessServer()
        {
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

            Tracers.PSMain.TraceAdminLogV2Message(
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
            PSCore.Tracing.Tracers.TryFinalizeTraceListeners(GetAllTraceSources());
        }

        /// <summary>
        /// Implements service start operation.
        /// </summary>
        protected override void OnStart(string[] args)
        {
#if DEBUG
            Stopwatch sw = new Stopwatch();
            sw.Start();

            while (sw.Elapsed < ProcServSettings.Default.Debugger_MaxWaitTimeForAttachOnStart)
            {
                Debugger.Launch();

                if (Debugger.IsAttached)
                    break;

                Thread.Sleep(TimeSpan.FromSeconds(1));
            }

            if (ProcServSettings.Default.Debugger_BreakOnStart)
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

                Tracers.PSMain.TraceAdminLogV2Message(TraceEventType.Information, "Service starting");

                SystemUtils.SetConnectivityModeAndSecurityProtocol(Tracers.PSMain);

                ProcessServerSettings.TunablesChanged += (sender, tunablesChangedEventArgs) =>
                {
                    TunablesBasedSettingsProvider.OnTunablesChanged(
                        tunablesChangedEventArgs?.NewTunables, ProcServSettings.Default, PSCoreSettings.Default);
                };

                if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                {
                    IdempotencyContext.RestoreTransactedSavedFilesAsync(exclLockAcquired: false, ct: CancellationToken.None)
                        .GetAwaiter().GetResult();
                }

                m_psTask = TaskUtils.RunTaskForAsyncOperation(
                    Tracers.PSMain,
                    "Process Server main",
                    m_cts.Token,
                    async (mainCancToken) =>
                    {
                        if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                            TryStartAppInsightsUploadFromBackup();

                        await PSInstallationInfo.Default.
                            BlockUntilValidInstallationDetected(includeFirstTimeConfigure: true, ct: mainCancToken).
                            ConfigureAwait(false);

                        Tracers.PSMain.TraceAdminLogV2Message(
                            TraceEventType.Information, $"CS Mode : {PSInstallationInfo.Default.CSMode}");

                        var taskList = new List<Task>();
                        bool spawnMasterPoller;
                        object masterPollerAssistObj;

                        if (ProcServSettings.Default.MonitorHealthService)
                        {
                            m_monitorHealthService = new MonitorHealthService();

                            Task monitorHealthServiceTask = TaskUtils.RunTaskForAsyncOperation(
                                traceSource: Tracers.Monitoring,
                                name: nameof(MonitorHealthService),
                                ct: mainCancToken,
                                operation: m_monitorHealthService.MonitorHealth);

                            taskList.Add(monitorHealthServiceTask);
                        }

                        if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                        {
                            // No need to start any tasks unless the registration with Rcm is done.
                            await PSConfiguration.Default.BlockUntilRcmRegistrationIsCompleted(mainCancToken);

                            RcmJobProcessor rcmJobProcessor = new RcmJobProcessor();

                            Task rcmJobProcessorTask = TaskUtils.RunTaskForAsyncOperation(
                                traceSource: Tracers.PSMain,
                                name: nameof(RcmJobProcessor),
                                ct: mainCancToken,
                                operation: rcmJobProcessor.Run);

                            taskList.Add(rcmJobProcessorTask);

                            Task recurringModPSTask = TaskUtils.RunTaskForAsyncOperation(
                                traceSource: Tracers.PSMain,
                                name: "Recurring ModifyProcessServer",
                                ct: mainCancToken,
                                operation: RecurringModifyProcessServerAsync);

                            taskList.Add(recurringModPSTask);

                            spawnMasterPoller = true;
                            masterPollerAssistObj = rcmJobProcessor;
                        }
                        else if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
                        {
                            if (ProcServSettings.Default.StartVolsync)
                            {
                                m_volsync = new Volsync();

                                Task volsyncTask = TaskUtils.RunTaskForAsyncOperation(
                                    traceSource: Tracers.PSMain,
                                    name: nameof(Volsync),
                                    ct: mainCancToken,
                                    operation: m_volsync.RunVolsyncParent
                                    );

                                taskList.Add(volsyncTask);
                            }

                            if (ProcServSettings.Default.RunLegacyPSTasks)
                            {
                                PSTaskManager psTaskManager =
                                new PSTaskManager(ProcServSettings.Default.LegacyPS_TaskSettings);

                                Task taskManagerTask = TaskUtils.RunTaskForAsyncOperation(
                                    traceSource: Tracers.PSMain,
                                    name: nameof(PSTaskManager),
                                    ct: mainCancToken,
                                    operation: psTaskManager.RunTasks);

                                taskList.Add(taskManagerTask);
                            }

                            spawnMasterPoller = true;
                            masterPollerAssistObj = null;
                        }
                        else
                        {
                            spawnMasterPoller = false;
                            masterPollerAssistObj = null;

                            taskList.Add(Task.Delay(Timeout.Infinite, mainCancToken));
                        }

                        if (spawnMasterPoller)
                        {
                            Task settingsMasterPollerTask = TaskUtils.RunTaskForAsyncOperation(
                                            traceSource: Tracers.PSMain,
                                            name: "PS Settings - Master Poller",
                                            ct: mainCancToken,
                                            operation: async (mastPollerCancToken) =>
                                            await ProcessServerSettings.RunMasterPoller(
                                                PSInstallationInfo.Default.GetPSCachedSettingsFilePath(),
                                                masterPollerAssistObj,
                                                mastPollerCancToken)
                                            .ConfigureAwait(false));

                            taskList.Add(settingsMasterPollerTask);
                        }

                        await Task.WhenAll(taskList).ConfigureAwait(false);
                    });

                base.OnStart(args);

                Tracers.PSMain.TraceAdminLogV2Message(TraceEventType.Information, "Service started");

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
                    PSCore.Tracing.Tracers.TryFinalizeTraceListeners(GetAllTraceSources());
            }
        }

        /// <summary>
        /// Implements service stop operation.
        /// </summary>
        protected override void OnStop()
        {
            try
            {
                Tracers.PSMain.TraceAdminLogV2Message(TraceEventType.Information, "Service stopping");

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
                            Tracers.PSMain.TraceAdminLogV2Message(
                                TraceEventType.Verbose, "Process Server main task is cancelled");

                            setError = false;
                        }
                    }

                    if (setError)
                    {
                        Tracers.PSMain.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Exception occured while trying to stop service : {0}",
                            ae.Flatten());
                    }
                }
                catch (Exception ex)
                {
                    Tracers.PSMain.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception occured while trying to stop service : {0}", ex);
                }

                base.OnStop();

                Tracers.PSMain.TraceAdminLogV2Message(TraceEventType.Information, "Service stopped");
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
                PSCore.Tracing.Tracers.TryFinalizeTraceListeners(GetAllTraceSources());
            }
        }

        private static readonly SemaphoreSlim s_modifyPSSemaphore = new SemaphoreSlim(1);

        private static async Task RecurringModifyProcessServerAsync(CancellationToken token)
        {
            // Lightweight lock replacement in Async pattern
            await s_modifyPSSemaphore.WaitAsync(token).ConfigureAwait(false);

            try
            {
                // TODO-SanKumar-2001: The StopWatch object would be resilient to system time changes,
                // only if it is using QPC (StopWatch.IsHighResolution == true). Otherwise, it uses
                // System clock for getting elapsed time. In such cases, we should add mecanism to
                // handle System clock adjustments.
                Stopwatch timeElapsedSW = new Stopwatch();

                ModifyProcessServerInput lastUpdatedPSInput = null;

                for (; ; )
                {
                    bool modRcmPSOpFailed = false;
                    RcmOperationContext opContext = null;

                    timeElapsedSW.Restart();

                    try
                    {
                        PSConfiguration.Default.ReloadNow(sharedLockAcquired: false);
                        CxpsConfig.Default.ReloadNow(sharedLockAcquired: false);

                        if (!PSConfiguration.Default.IsFirstTimeConfigured ||
                            !PSConfiguration.Default.IsRcmRegistrationCompleted ||
                            !CxpsConfig.Default.IsRcmPSFirstTimeConfigured.GetValueOrDefault())
                        {
                            Tracers.PSMain.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                "First time configuration and/or registration isn't yet completed");
                        }
                        else
                        {
                            ModifyProcessServerInput populatedPSInput = SystemUtils.BuildRcmPSModifyInput(CxpsConfig.Default.PsFingerprint);

                            // Invoke modify call only if there's any change in the populated input
                            // from the previously updated input.
                            if (!string.Equals(
                                lastUpdatedPSInput?.ToString(),
                                populatedPSInput.ToString(),
                                StringComparison.Ordinal)) // Intentional case-sensitive comparison
                            {
                                opContext = new RcmOperationContext();

                                Tracers.PSMain.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    opContext,
                                    "Updating latest PS info with RCM (Modify PS){0}{1}",
                                    Environment.NewLine, populatedPSInput);

                                // TODO-SanKumar-2001: Factory should pass on the timeout, otherwise take from Settings.
                                using (var psRcmApi = RcmApiFactory.GetProcessServerRcmApiStubs(PSConfiguration.Default))
                                {
                                    await
                                        psRcmApi.ModifyProcessServerAsync(
                                            opContext,
                                            populatedPSInput,
                                            token)
                                        .ConfigureAwait(false);
                                }

                                Tracers.PSMain.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    opContext,
                                    "Succesfully updated the latest PS info with RCM (Modify PS)");

                                // On successful call, update the last registered info
                                Interlocked.Exchange(ref lastUpdatedPSInput, populatedPSInput);
                            }
                        }
                    }
                    catch (OperationCanceledException) when (token.IsCancellationRequested)
                    {
                        // NOTE-SanKumar-1912: We have seen that the API calls fail with
                        // OperationCanceledException due to other errors too, such as
                        // timeout. So we are checking if the token that we passed has
                        // been cancelled to quit this loop.

                        throw;
                    }
                    catch (Exception ex)
                    {
                        Tracers.PSMain.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            opContext,
                            "Updating latest PS info with RCM (Modify PS) failed{0}{1}",
                            Environment.NewLine, ex);

                        modRcmPSOpFailed = true;
                    }

                    TimeSpan toWaitTimeSpan = ProcServSettings.Default.ModifyRcmPS_PollInterval;

                    if (modRcmPSOpFailed &&
                        toWaitTimeSpan > ProcServSettings.Default.ModifyRcmPS_FailureRetryInterval)
                    {
                        toWaitTimeSpan = ProcServSettings.Default.ModifyRcmPS_FailureRetryInterval;
                    }

                    toWaitTimeSpan -= timeElapsedSW.Elapsed;
                    if (toWaitTimeSpan < TimeSpan.Zero)
                    {
                        toWaitTimeSpan = TimeSpan.Zero;
                    }

                    await Task.Delay(toWaitTimeSpan, token).ConfigureAwait(false);
                }
            }
            finally
            {
                s_modifyPSSemaphore.Release();
            }
        }

        /// <summary>
        /// App insights trace listener opts-in for storing the failed uploads in
        /// a local folder during the Rcm PS Configurator execution. Since it's
        /// not a long running process, the trace listener is initialized in the
        /// ProcessServer.exe to attempt uploads of pending logs with backoff logic.
        /// </summary>
        private void TryStartAppInsightsUploadFromBackup()
        {
            TaskUtils.RunAndIgnoreException(() =>
            {
                var instrumentationKey = PSConfiguration.Default.AppInsightsConfiguration?.InstrumentationKey;
                var storageBackupFolderPath = PSInstallationInfo.Default.GetRcmPSConfAppInsightsBackupFolderPath();

                if (string.IsNullOrWhiteSpace(instrumentationKey) ||
                    string.IsNullOrWhiteSpace(storageBackupFolderPath))
                {
                    Tracers.PSMain.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "Not starting app insights upload from backup, since {0} is invalid",
                        string.IsNullOrWhiteSpace(instrumentationKey) ? "Instrumentation Key" : "Storage Backup Folder Path");

                    return;
                }

                TelemetryConfiguration.Active.InstrumentationKey = instrumentationKey;
                ServerTelemetryChannel appInsightsChannel = new ServerTelemetryChannel
                {
                    StorageFolder = storageBackupFolderPath
                };

#if DEBUG
                appInsightsChannel.DeveloperMode = true;
#endif

                appInsightsChannel.Initialize(TelemetryConfiguration.Active);
                TelemetryConfiguration.Active.TelemetryChannel = appInsightsChannel;

                appInsightsChannel.Flush(); // Asynchronous
            }, Tracers.PSMain);
        }
    }
}
