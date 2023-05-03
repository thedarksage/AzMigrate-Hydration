using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using RcmClientLib;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup
{
    public static class RcmConfigurator
    {
        private static readonly JsonSerializerSettings s_jsonSerSettings =
            JsonUtils.GetStandardSerializerSettings(
                indent: true,
                converters: JsonUtils.GetAllKnownConverters());

        private static void VerifyPSInstallation(RcmOperationContext opContext)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Verifying PS installation");

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Reloading {nameof(PSInstallationInfo)} from registry");

            // NOTE-SanKumar-1910: Eventhough PSInstallationInfo is called out to
            // be one-time loadable per process life time, it's thread-safe here
            // to reload the object with just this one change. Also, this call
            // doesn't occur on the services but in a short-lived powershell.exe
            // invoking the configurator script.
            PSInstallationInfo.Default.Reload();

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Checking if PS is installed on the machine");

            if (!PSInstallationInfo.Default.IsInstalled)
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.NotInstalled,
                    isRetryableError: false,
                    opContext: opContext);
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Checking if the CS mode is Rcm");

            if (PSInstallationInfo.Default.CSMode != CSMode.Rcm)
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.UnexpectedMode,
                    isRetryableError: false,
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                            { "InstalledCSMode", PSInstallationInfo.Default.CSMode.ToString() },
                            { "ExpectedCSMode", CSMode.Rcm.ToString() }
                    });
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Checking the integrity of the PS installation");

            if (!PSInstallationInfo.Default.IsValidInfo(includeFirstTimeConfigure: false))
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.CorruptedInstallation,
                    isRetryableError: false,
                    opContext: opContext);
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Verifying PS installation successful");
        }

        private static void VerifyFirstTimeConfigurationCompleted(
            RcmOperationContext opContext,
            IdempotencyContext idempContext,
            bool expected,
            bool includeCxpsConfig,
            bool validateInstallation)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Verifying if the first time configuration has completed");

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Reloading the {nameof(PSConfiguration)} object");

            idempContext.PSConfiguration.ReloadNow(sharedLockAcquired: false);

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Checking if {nameof(PSConfiguration)} says {nameof(PSConfiguration.IsFirstTimeConfigured)} as {expected}");

            if (idempContext.PSConfiguration.IsFirstTimeConfigured != expected)
            {
                if (expected)
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.NotYetFirstTimeConfigured,
                        isRetryableError: false,
                        opContext: opContext);
                }
                else
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.AlreadyFirstTimeConfigured,
                        isRetryableError: false,
                        opContext: opContext);
                }
            }

            if (includeCxpsConfig)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    $"Reloading the {nameof(CxpsConfig)} object");

                idempContext.CxpsConfig.ReloadNow(sharedLockAcquired: false);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    $"Checking if the {nameof(CxpsConfig)} says {nameof(CxpsConfig.IsRcmPSFirstTimeConfigured)} as {expected}");

                if (idempContext.CxpsConfig.IsRcmPSFirstTimeConfigured.GetValueOrDefault() != expected)
                {
                    if (expected)
                    {
                        throw new ConfiguratorException(
                            PSRcmAgentErrorEnum.NotYetFirstTimeConfigured,
                            isRetryableError: false,
                            opContext: opContext);
                    }
                    else
                    {
                        throw new ConfiguratorException(
                            PSRcmAgentErrorEnum.AlreadyFirstTimeConfigured,
                            isRetryableError: false,
                            opContext: opContext);
                    }
                }
            }

            if (expected && validateInstallation)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Checking if the PS Installation is valid (if all the registry entries are good)");

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    $"Reloading {nameof(PSInstallationInfo)} from registry");

                // NOTE-SanKumar-1910: Eventhough PSInstallationInfo is called out to
                // be one-time loadable per process life time, it's thread-safe here
                // to reload the object with just this one change. Also, this call
                // doesn't occur on the services but in a short-lived powershell.exe
                // invoking the configurator script.
                PSInstallationInfo.Default.Reload();

                // TODO-SanKumar-2004: Ensure the registry values storing folder
                // paths are empty, if expected is set to false.

                if (!PSInstallationInfo.Default.IsValidInfo(includeFirstTimeConfigure: true))
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.CorruptedInstallation,
                        isRetryableError: false,
                        opContext: opContext);
                }
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Successfully verified that the first time configuration has completed");
        }

        private static void VerifyRegisteredWithRcm(RcmOperationContext opContext, IdempotencyContext idempContext)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                $"Verifying if the PS is registered with the Rcm service");

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Reloading the {nameof(PSConfiguration)} object");

            idempContext.PSConfiguration.ReloadNow(sharedLockAcquired: false);

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Checking if the {nameof(PSConfiguration)} contains a Rcm Uri");

            if (!idempContext.PSConfiguration.IsRcmRegistrationCompleted)
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.NotRegistered,
                    isRetryableError: false,
                    opContext: opContext);
            }

            // TODO-SanKumar-2004: Attempt a dummy modify, so that Rcm could
            // return NotFound error. This would be to handle the edge case,
            // where unregister was successful but failed to updated in the
            // PSConfiguration file.

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                $"Successfully verified that the PS is registered with the Rcm service");
        }

        private static void VerifyInputCacheLocation(RcmOperationContext opContext, string cacheLocation)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Validating the input cache location : {0}",
                cacheLocation);

            if (string.IsNullOrWhiteSpace(cacheLocation))
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.BadInputEmptyCacheLocation,
                    isRetryableError: false,
                    opContext: opContext);
            }

            cacheLocation = Environment.ExpandEnvironmentVariables(cacheLocation);

            if (FSUtils.IsPartiallyQualified(cacheLocation))
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.BadInputCacheLocationRelativePath,
                    isRetryableError: false,
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["InvalidCacheLocation"] = cacheLocation
                    });
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Input cache location : {0} is valid",
                cacheLocation);
        }

        private static void CreateCacheLocationFolders(
            RcmOperationContext opContext,
            IdempotencyContext idempContext,
            string cacheLocation,
            out string replicationLogFolderPath,
            out string telemetryFolderPath,
            out string reqDefFolderPath)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Creating the PS folders in the cache location : {0}",
                cacheLocation);

            var foldersToProcess = new[]
            {
                (
                Existing: PSInstallationInfo.Default.GetReplicationLogFolderPath(),
                FolderName: Settings.Default.RcmConf_ReplLogFolderName,
                AlreadyConfErr: PSRcmAgentErrorEnum.ReplicationLogFolderAlreadyConfigured,
                AlreadyPresentErr: PSRcmAgentErrorEnum.ReplicationLogFolderAlreadyPresent
                ),

                (
                Existing: PSInstallationInfo.Default.GetTelemetryFolderPath(),
                FolderName: Settings.Default.RcmConf_TelemetryFolderName,
                AlreadyConfErr: PSRcmAgentErrorEnum.TelemetryFolderAlreadyConfigured,
                AlreadyPresentErr: PSRcmAgentErrorEnum.TelemetryFolderAlreadyPresent
                ),

                (
                Existing: PSInstallationInfo.Default.GetReqDefFolderPath(),
                FolderName: Settings.Default.RcmConf_RequestDefaultFolderName,
                AlreadyConfErr: PSRcmAgentErrorEnum.ReqDefFolderAlreadyConfigured,
                AlreadyPresentErr: PSRcmAgentErrorEnum.ReqDefFolderAlreadyPresent
                )
            };

            cacheLocation = Environment.ExpandEnvironmentVariables(cacheLocation);

            var createdFolders = foldersToProcess.Select(currFolderToProc =>
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Running precheck for the folder : {0}",
                    currFolderToProc);

                if (!string.IsNullOrWhiteSpace(currFolderToProc.Existing))
                {
                    throw new ConfiguratorException(
                        currFolderToProc.AlreadyConfErr,
                        isRetryableError: false,
                        opContext: opContext,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["AlreadyConfiguredFolderPath"] = currFolderToProc.Existing
                        });
                }

                string currFolderPath = Path.Combine(cacheLocation, currFolderToProc.FolderName);
                currFolderPath = Path.GetFullPath(currFolderPath);

                if (Directory.Exists(currFolderPath))
                {
                    throw new ConfiguratorException(
                        currFolderToProc.AlreadyPresentErr,
                        isRetryableError: false,
                        opContext: opContext,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["ExistingFolderPath"] = currFolderPath
                        });
                }

                return currFolderPath;
            }).ToList();

            createdFolders.ForEach(currFolderPath =>
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Creating the folder : {0} with admin and system only access",
                    currFolderPath);

                try
                {
                    idempContext.AddCreatedFolders(currFolderPath, lockFilePath: null);

                    FSUtils.CreateAdminsAndSystemOnlyDir(currFolderPath);
                }
                catch (Exception ex)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        opContext,
                        "Failed to create folder : {0}{1}{2}",
                        currFolderPath, Environment.NewLine, ex);

                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.FolderCreationFailed,
                        isRetryableError: true,
                        opContext: opContext,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["FolderPath"] = currFolderPath,
                            ["Exception"] = ex.ToString()
                        });
                }
            });

            replicationLogFolderPath = createdFolders[0];
            telemetryFolderPath = createdFolders[1];
            reqDefFolderPath = createdFolders[2];

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Updating the created folder paths in the registry");

            PSInstallationInfo.UpdateFolderPaths(
                idempContext,
                logFolderPath: replicationLogFolderPath,
                telemetryFolderPath: telemetryFolderPath,
                reqDefFolderPath: reqDefFolderPath);

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Successfully created the PS folders in the cache location : {0}",
                cacheLocation);
        }

        private static void ClearCacheLocationFolders(
            RcmOperationContext opContext,
            IdempotencyContext idempContext)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Clearing the folders in the cache location");

            var toClearFolders = new[]
            {
                PSInstallationInfo.Default.GetReplicationLogFolderPath(),
                PSInstallationInfo.Default.GetTelemetryFolderPath(),
                PSInstallationInfo.Default.GetReqDefFolderPath()
            };

            foreach (var currFolderToClear in toClearFolders)
            {
                if (string.IsNullOrEmpty(currFolderToClear))
                    continue;

                // NOTE-SanKumar-2006: Explicitly not using the idempotency context
                // to cleanup these folders post committing the operation. Since
                // cleanup of these folders is an important step in Unconfigure
                // operation, we would like to retry these failures by invoking
                // unconfigure operation again.
                // The side effect of this decision is that the few/all folders in the
                // cache location could have been deleted after a failed unconfigure
                // action. The only way to get out of it is by repeatedly calling
                // unconfigure until it succeeds. This can be called as by design.
                try
                {
                    DirectoryUtils.DeleteDirectory(
                        currFolderToClear,
                        recursive: true,
                        useLongPath: true,
                        maxRetryCount: 3, // TODO-SanKumar-2004: Settings or script input
                        retryInterval: TimeSpan.FromSeconds(2), // TODO-SanKumar-2004: Settings or script input
                        traceSource: Tracers.RcmConf);
                }
                catch (Exception ex)
                {
                    // Logging is done by the util above.

                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.FolderDeletionFailed,
                        isRetryableError: true,
                        opContext: opContext,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["FolderPath"] = currFolderToClear,
                            ["Exception"] = ex.ToString()
                        });
                }
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Removing the PS folder paths from registry");

            PSInstallationInfo.ClearFolderPaths(idempContext, Tracers.RcmConf);

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Successfully cleared the folders in the cache location");
        }

        private static void CommitAndReloadLazyObjects(IdempotencyContext idempContext)
        {
            idempContext.Commit();

            // NOTE-SanKumar-1910: Eventhough PSInstallationInfo is called out to
            // be one-time loadable per process life time, it's thread-safe here
            // to reload the object with just this one change. Also, this call
            // doesn't occur on the services but in a short-lived powershell.exe
            // invoking the configurator script.
            PSInstallationInfo.Default.Reload();

            // NOTE-SanKumar-2006: These objects are reloaded lazily. i.e. reset
            // to be loaded on next access. So, there must not be any access of
            // these objects after calling CommitAndReloadObjects() in the current
            // configurator action. Because the idempContext.Commit() may or
            // may not replace the intended files post committing the transactions
            // to the registry. i.e. restore operation is retriable and may not
            // succeed the first time.
            PSConfiguration.Default.ReloadLazy();
            CxpsConfig.Default.ReloadLazy();
        }

        private static ConfiguratorOutput RunConfiguratorAction(
            string applianceJsonPath,
            string clientRequestId,
            string activityId,
            Func<RcmOperationContext, IdempotencyContext, ConfiguratorOutput> impl,
            [CallerMemberName] string callingMemberName = "")
        {
            // TODO-SanKumar-2006: Add logic for only one configurator action to be running across
            // the entire system.

            // TODO-SanKumar-2004: Probably pass -Debug flag for this activity,
            // so that we could achieve the same even for release builds.
#if DEBUG && false // NOTE-SanKumar-2004: Make this true to debug the configurator actions
            while (!Debugger.IsAttached)
                Thread.Sleep(TimeSpan.FromSeconds(1));

            Debugger.Break();
#endif
            RcmOperationContext opContext =
                new RcmOperationContext(activityId: activityId, clientRequestId: clientRequestId);

            ConfiguratorOutput result = null;

            try
            {
                // NOTE-SanKumar-2006: This call uses PSConfiguration.Default for appinsights settings.
                // Not adding a restore operation before this in order to simplify the implementation.
                PrepareLogging(applianceJsonPath);

                try
                {
                    using (var idempContext = new IdempotencyContext())
                    {
                        result = impl(opContext, idempContext);
                    }
                }
                finally
                {
                    (bool status, string failedServices) =
                        ServiceUtils.TryStartProcessServerServices(opContext, Tracers.RcmConf);

                    if (!status)
                    {
                        throw new ConfiguratorException(
                            PSRcmAgentErrorEnum.FailedToStartServices,
                            isRetryableError: true,
                            opContext: opContext,
                            msgParams: new Dictionary<string, string>()
                            {
                                ["Services"] = failedServices
                            });
                    }
                }
            }
            catch (ConfiguratorException cex)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    opContext,
                    $"Hit a configurator error, while running the {{0}} configurator action{{1}}{{2}}",
                    callingMemberName, Environment.NewLine, cex);

                result = cex.Output;
            }
            catch (WebException wex)
            {
                switch (wex.Status)
                {
                    case WebExceptionStatus.ConnectFailure:
                        result = ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.ConfiguratorConnectFailure,
                            isRetryableError: true,
                            opContext: opContext,
                            ex: wex);
                        break;

                    case WebExceptionStatus.NameResolutionFailure:
                        result = ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.ConfiguratorNameResolutionFailure,
                            isRetryableError: true,
                            opContext: opContext,
                            ex: wex);
                        break;

                    case WebExceptionStatus.ProxyNameResolutionFailure:
                        result = ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.ConfiguratorProxyNameResolutionFailure,
                            isRetryableError: true,
                            opContext: opContext,
                            ex: wex);
                        break;

                    case WebExceptionStatus.RequestProhibitedByProxy:
                        result = ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.ConfiguratorRequestProhibitedByProxy,
                            isRetryableError: true,
                            opContext: opContext,
                            ex: wex);
                        break;

                    default:
                        result = ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.ConfiguratorConnectFailure,
                            isRetryableError: true,
                            opContext: opContext,
                            ex: wex);
                        break;
                }
            }
            catch (Exception ex)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    opContext,
                    $"Hit an unknown error, while running the {{0}} configurator action{{1}}{{2}}",
                    callingMemberName, Environment.NewLine, ex);

                result = ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.ConfiguratorActionFailed,
                    isRetryableError: true,
                    opContext: opContext,
                    ex: ex);
            }
            finally
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Returning result:{0}{1}",
                    Environment.NewLine, result);

                Tracers.TryFinalizeTraceListeners(Tracers.GetAllTraceSources());
            }

            return result;
        }

        private static void PrepareLogging(string applianceJsonPath)
        {
            // The configurator is a PowerShell script. Unlike in production,
            // where each configurator invocation is performed in a separate
            // PowerShell process, same PowerShell console could be used
            // to invoke these actions manually. In that case, the same static
            // objects are used across multiple invocations.
            // So, clearing out any lingering TraceListeners from past invocations here.
            Tracers.TryFinalizeTraceListeners(Tracers.GetAllTraceSources());

            // Better performance, since a lock is not used in case of a thread-safe listener
            Trace.UseGlobalLock = false;
            // AutoFlush is turned off in the Process Server services but let's
            // turn it on in the Rcm PS Configurator, since it would be better
            // to send/store each event in various listeners instead of caching
            // them and writing in the end. Better in negative scenarios such as
            // PowerShell.exe crashing in the middle/end, etc.
            Trace.AutoFlush = true;
            Trace.Listeners.Clear();

            SourceSwitch traceSwitch = new SourceSwitch(
                displayName: "RcmConfActionsSwitch", defaultSwitchValue: "Verbose");

            foreach (var currTraceSource in Tracers.GetAllTraceSources())
                currTraceSource.Switch = traceSwitch;

            TraceFilter traceFilter = new EventTypeFilter(SourceLevels.All);

            bool AddTraceListenerToSources(TraceListener traceListener, IEnumerable<TraceSource> traceSources)
            {
                bool addedToAny = false;

                if (traceListener == null || traceSources == null)
                    return addedToAny;

                foreach (var currTraceSource in traceSources)
                {
                    if (currTraceSource == null)
                        continue;

                    addedToAny |= TaskUtils.RunAndIgnoreException(
                        () => currTraceSource.Listeners.Add(traceListener),
                        Tracers.RcmConf);
                }

                return addedToAny;
            }

#if DEBUG
            TaskUtils.RunAndIgnoreException(() =>
            {
                var defaultTraceListener = new DefaultTraceListener();

                if (!AddTraceListenerToSources(defaultTraceListener, Tracers.GetAllTraceSources()))
                    defaultTraceListener.TryDispose(Tracers.RcmConf);
            }, Tracers.RcmConf);
#endif

            // TODO-SanKumar-2004: Enabling this in production by default.
            // Another way would be to enable these logs by setting a flag, while
            // invoking the Configurator (say -Verbose / -Log).
            TaskUtils.RunAndIgnoreException(() =>
            {
                // stderr will contain the logs, while stdout will contain the
                // result of the configurator action.
                var consoleTraceListener = new ConsoleTraceListener(useErrorStream: false)
                {
                    Filter = traceFilter,
                    Name = "DefaultConsoleListener",
                    TraceOutputOptions = TraceOptions.None
                };

                if (!AddTraceListenerToSources(consoleTraceListener, Tracers.GetAllTraceSources()))
                    consoleTraceListener.TryDispose(Tracers.RcmConf);
            }, Tracers.RcmConf);

            TaskUtils.RunAndIgnoreException(() =>
            {
                // Forcing no cuts happen during a configure, allowing large
                // number of completed files and big size for each file. Also,
                // marking for the current file to be marked as completed soon
                // after the action is completed, since actions aren't continuously
                // running operations and only executed on-demand.
                string mdsLogCutterInitData = JsonConvert.SerializeObject(
                    new Dictionary<string, string>()
                    {
                        ["FolderPath"] = Path.Combine(PSInstallationInfo.Default.GetRcmPSConfLogFolderPath(), "Default"),
                        ["MdsTableName"] = "InMageAdminLogV2",
                        ["ComponentId"] = "ProcessServer",
                        ["MaxCompletedFilesCount"] = "10000",
                        ["MaxFileSizeKB"] = "102400",
                        ["CutInterval"] = "01:00:00",
                        ["CompleteOnClose"] = "true"
                    },
                    s_jsonSerSettings);

                var mdsLogCutterTraceListener = new MdsLogCutterTraceListener(mdsLogCutterInitData)
                {
                    Filter = traceFilter,
                    Name = "DefaultMdsListener",
                    TraceOutputOptions = TraceOptions.None
                };

                if (!AddTraceListenerToSources(mdsLogCutterTraceListener, Tracers.GetAllTraceSources()))
                    mdsLogCutterTraceListener.TryDispose(Tracers.RcmConf);
            }, Tracers.RcmConf);

            TaskUtils.RunAndIgnoreException(() =>
            {
                ApplianceSettings applianceSettings =
                    string.IsNullOrEmpty(applianceJsonPath) ?
                    null : ApplianceSettings.LoadFromFile(applianceJsonPath);

                string instrumentationKey = applianceSettings?.AppInsightsInstrumentationKey;
                string machineIdentifier = applianceSettings?.MachineIdentifier;
                string providerId = applianceSettings?.ProviderId;
                string siteSubscriptionId = applianceSettings?.Site?.SubscriptionId;

                TaskUtils.RunAndIgnoreException(() =>
                {
                    // If appliance json path is not provided or if the required
                    // values are empty, we could use the values from PS Configuration.
                    // These values may not present all the time in the PS Configuration
                    // (for example, before first time configure or after unconfigure).
                    // Also, unconfigure is a special action that could even work on a corrupted
                    // PS Configuration file. So igonring any exception occuring
                    // during this attempt.

                    PSConfiguration.Default.ReloadLazy();

                    if (string.IsNullOrEmpty(instrumentationKey))
                        instrumentationKey = PSConfiguration.Default.AppInsightsConfiguration?.InstrumentationKey;

                    if (string.IsNullOrEmpty(machineIdentifier))
                        machineIdentifier = PSInstallationInfo.Default.GetPSHostId();

                    if (string.IsNullOrEmpty(providerId))
                        providerId = PSConfiguration.Default.AppInsightsConfiguration?.ProviderId;

                    if (string.IsNullOrEmpty(siteSubscriptionId))
                        siteSubscriptionId = PSConfiguration.Default.AppInsightsConfiguration?.SiteSubscriptionId;
                }, Tracers.RcmConf);

                if (!string.IsNullOrEmpty(instrumentationKey) &&
                    !string.IsNullOrEmpty(machineIdentifier) &&
                    !string.IsNullOrEmpty(providerId))
                {
                    string appInsightsTraceListenerInitData = JsonConvert.SerializeObject(
                        new Dictionary<string, string>()
                        {
                            ["InstrumentationKey"] = instrumentationKey,
                            ["MachineIdentifier"] = machineIdentifier,
                            ["ProviderId"] = providerId,
                            ["SiteSubscriptionId"] = siteSubscriptionId,
                            ["ComponentId"] = "ProcessServer",
                            ["BackupFolderPath"] = PSInstallationInfo.Default.GetRcmPSConfAppInsightsBackupFolderPath(),
                            ["WaitInTheEndForFlush"] = "true"
                        },
                        s_jsonSerSettings);

                    var appInsightsTraceListener = new AppInsightsTraceListener(appInsightsTraceListenerInitData)
                    {
                        Filter = traceFilter,
                        Name = "DefaultAIListener",
                        TraceOutputOptions = TraceOptions.None
                    };

                    if (!AddTraceListenerToSources(appInsightsTraceListener, Tracers.GetAllTraceSources()))
                        appInsightsTraceListener.TryDispose(Tracers.RcmConf);

                    // TODO-SanKumar-2004: AppInsightsTraceListener sets the
                    // TelemetryConfiguration.Active.TelemetryChannel. Should it
                    // rather not do it? Otherwise, the app insights logic might
                    // be actively running, if a PowerShell console that ran a
                    // configurator action is left active (not in production, as
                    // each configurator action is exectued by Appliance in a
                    // short-lived PowerShell.exe).
                }
                else
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "Not starting app insights trace listener, since required " +
                        "initialize data couldn't be retrieved/provided");
                }
            }, Tracers.RcmConf);
        }

        // TODO-SanKumar-1908: Raise reliability of the configurator.
        // a) On any failure, services could be started back on best effort basis.
        // Return warning on failing to start the services back.
        // b) Since more than one configuration is edited during different
        // configurator actions, we might be needed to reliably undo all the
        // edits made to reflect a transaction.

        public static ConfiguratorOutput FirstTimeConfigure(
            IPAddress natAddress,
            int replicationDataPort,
            bool configureFirewall,
            string cacheLocation,
            string applianceJsonPath,
            string agentRepository,
            string clientRequestId,
            string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    $"Starting the {nameof(FirstTimeConfigure)} action");

                VerifyPSInstallation(opContext);

                try
                {
                    VerifyFirstTimeConfigurationCompleted(
                        opContext,
                        idempContext,
                        expected: false,
                        includeCxpsConfig: true,
                        validateInstallation: false);
                }
                catch (ConfiguratorException ex)
                {
                    if (ex.ErrorCode == PSRcmAgentErrorEnum.AlreadyFirstTimeConfigured)
                    {
                        Tracers.RcmConf.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            $"Process Server was already configured. Returning Success");
                        return ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.Success,
                            isRetryableError: false,
                            message: "First time configuring RCM PS succeeded",
                            opContext: opContext,
                            msgParams: new Dictionary<string, string>()
                            {
                                ["Nat IP"] = natAddress?.ToString(),
                                ["Port"] = replicationDataPort.ToString(CultureInfo.InvariantCulture),
                                ["PreviousPort"] = string.Empty,
                                ["IsFirewallConfigured"] = configureFirewall.ToString(CultureInfo.InvariantCulture),
                                ["CacheLocation"] = cacheLocation,
                                ["ReplicationLogFolderPath"] = string.Empty,
                                ["TelemetryFolderPath"] = string.Empty,
                                ["ReqDefFolderPath"] = string.Empty
                            });
                    }
                    else
                    {
                        throw ex;
                    }
                }

                VerifyInputCacheLocation(opContext, cacheLocation);

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                CreateCacheLocationFolders(
                    opContext,
                    idempContext,
                    cacheLocation,
                    out string replicationLogFolderPath,
                    out string telemetryFolderPath,
                    out string reqDefFolderPath);

                UpdateApplianceSettings(opContext, idempContext, applianceJsonPath, firstTimeConfigure: true);

                ConfigureProcessServerHttpListener(
                        opContext,
                        idempContext,
                        natAddress: natAddress,
                        replicationDataPort: replicationDataPort,
                        configureFirewall: configureFirewall,
                        replLogFolderPath: replicationLogFolderPath,
                        telemetryFolderPath: telemetryFolderPath,
                        reqDefFolderPath: reqDefFolderPath,
                        agentRepositoryPath: agentRepository,
                        firstTimeConfigure: true,
                        prevSslPortInCxpsConf: out string prevSslPortInCxpsConf);

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    $"Successfully completed the {nameof(FirstTimeConfigure)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "First time configuring RCM PS succeeded",
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["Nat IP"] = natAddress?.ToString(),
                        ["Port"] = replicationDataPort.ToString(CultureInfo.InvariantCulture),
                        ["PreviousPort"] = prevSslPortInCxpsConf,
                        ["IsFirewallConfigured"] = configureFirewall.ToString(CultureInfo.InvariantCulture),
                        ["CacheLocation"] = cacheLocation,
                        ["ReplicationLogFolderPath"] = replicationLogFolderPath,
                        ["TelemetryFolderPath"] = telemetryFolderPath,
                        ["ReqDefFolderPath"] = reqDefFolderPath
                    });
            }
        }

        public static ConfiguratorOutput ConfigureProcessServerHttpListener(
            IPAddress natAddress,
            int replicationDataPort,
            bool configureFirewall,
            string applianceJsonPath,
            string clientRequestId,
            string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(ConfigureProcessServerHttpListener)} action");

                VerifyPSInstallation(opContext);

                VerifyFirstTimeConfigurationCompleted(
                    opContext,
                    idempContext,
                    expected: true,
                    includeCxpsConfig: true,
                    validateInstallation: true);

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                ConfigureProcessServerHttpListener(
                    opContext,
                    idempContext,
                    natAddress: natAddress,
                    replicationDataPort: replicationDataPort,
                    configureFirewall: configureFirewall,
                    replLogFolderPath: null,
                    telemetryFolderPath: null,
                    reqDefFolderPath: null,
                    agentRepositoryPath: null,
                    firstTimeConfigure: false,
                    prevSslPortInCxpsConf: out string prevSslPortInCxpsConf);

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(ConfigureProcessServerHttpListener)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "Configuring process server HTTP listener succeeded",
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["Nat IP"] = natAddress?.ToString(),
                        ["Port"] = replicationDataPort.ToString(CultureInfo.InvariantCulture),
                        ["PreviousPort"] = prevSslPortInCxpsConf,
                        ["IsFirewallConfigured"] = configureFirewall.ToString(CultureInfo.InvariantCulture)
                    });
            }
        }

        private static void ConfigureProcessServerHttpListener(
            RcmOperationContext opContext,
            IdempotencyContext idempContext,
            IPAddress natAddress,
            int replicationDataPort,
            bool configureFirewall,
            string replLogFolderPath,
            string telemetryFolderPath,
            string reqDefFolderPath,
            string agentRepositoryPath,
            bool firstTimeConfigure,
            out string prevSslPortInCxpsConf)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Configuring PS Http listener");

            if (replicationDataPort == 0)
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.BadInputReplicationDataPort,
                    isRetryableError: true,
                    opContext: opContext);
            }

            (bool isReplicationDataPortInUse, string processName) =
                SystemUtils.GetProcessListeningOnTcpPort(replicationDataPort, Tracers.RcmConf);

            if (isReplicationDataPortInUse)
            {
                throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.ReplicationDataPortIsInUse,
                    isRetryableError: true,
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["InvalidPort"] = replicationDataPort.ToString(),
                        ["ProcessName"] = processName
                    });
            }

            // NOTE-SanKumar-2003: GenCert creates backup of current files, attempts to generate all
            // the required files and replaces all the current files with the generated files. If
            // there's any problem in these steps, the previous (if any) set of files are restored.
            // Leaving it as is instead of cleaning up the private folder, as anyway the cxps listener
            // wouldn't start, if the first time configure isn't completed. For the same reason, not
            // cleaning up the folder before starting this process as well to help debug any issue.
            // NOTE-SanKumar-2006: For the same reasons mentioned above, we don't include the certs
            // in the idempotency actions as well.
            if (firstTimeConfigure)
            {
                string gencertExePath = PSInstallationInfo.Default.GetGenCertExePath();
                string transportPrivatePath = PSInstallationInfo.Default.GetTransportPrivateFolderPath();

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Executing {0} as part of first-time configure with target path : {1}",
                    gencertExePath, transportPrivatePath);

                // Escape the trailing backslash (if any), as it might confuse the input command line
                // parameter, which is bounded by the double-quotes to ensure the path with spaces
                // is passed correctly.
                if (transportPrivatePath.EndsWith("\\"))
                    transportPrivatePath += "\\";

                string gencertArgs = $"-n ps --dh -C \"US\" --ST \"Washington\" -L \"Redmond\"" +
                        $" -O \"Microsoft\" --OU \"InMage\" --CN \"Scout\"" +
                        $" -d \"{transportPrivatePath}\"";

                int returnVal = ProcessUtils.RunProcess(
                    exePath: gencertExePath,
                    args: gencertArgs,
                    stdOut: out string gencertOutput,
                    stdErr: out string gencertError,
                    pid: out int gencertPid);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    returnVal == 0 ? TraceEventType.Verbose : TraceEventType.Error,
                    opContext,
                    "{0} {1} returned exit code : {2}{3}PID : {4}{5}Output : {6}{7}Error : {8}",
                    gencertExePath, gencertArgs, returnVal, Environment.NewLine,
                    gencertPid, Environment.NewLine,
                    gencertOutput, Environment.NewLine,
                    gencertError);

                if (returnVal != 0)
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.GenCertFailed,
                        isRetryableError: true,
                        opContext: opContext,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["ExecutablePath"] = gencertExePath,
                            ["Arguments"] = gencertArgs,
                            ["Pid"] = gencertPid.ToString(CultureInfo.InvariantCulture),
                            ["ReturnValue"] = returnVal.ToString(CultureInfo.InvariantCulture),
                            ["StdOut"] = gencertOutput,
                            ["StdErr"] = gencertError
                        });
                }
            }

            prevSslPortInCxpsConf = UpdateCxpsConf(
                opContext,
                idempContext,
                replLogFolderPath: replLogFolderPath,
                telemetryFolderPath: telemetryFolderPath,
                reqDefFolderPath: reqDefFolderPath,
                agentRepositoryPath: agentRepositoryPath,
                replicationDataPort: replicationDataPort,
                firstTimeConfigure: firstTimeConfigure);

            // TODO-SanKumar-1907: If not a first time configure operation, then
            // ensure that the BiosId and HostId are consistent to avoid clone
            // (or) copied install folder/configs scenarios.

            var listenerConf = new PSListenerConfiguration(natAddress);

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Updating {nameof(PSConfiguration)} with the listener configuration : {0}",
                JsonConvert.SerializeObject(listenerConf, s_jsonSerSettings));

            // TODO-SanKumar-1904: We could validate if our IP selection
            // heuristic would work and only then save the new settings to conf.
            idempContext.PSConfiguration.SetListenerConfigurationAsync(
                listenerConf,
                validatorAsync: null)
            .GetAwaiter().GetResult();

            // NOTE-SanKumar-2006: Since all the previous firewall rules are cleared
            // up on next reconfigure and the rule only opens up cxps.exe, we don't
            // have to include this in the idempotency logic.
            if (configureFirewall)
            {
                string toAddRuleName = $"{Settings.Default.AsrPSFWRulePrefix} {replicationDataPort}";

                // Not adding the firewall rule, if it was already created.
                if (FirewallUtils.GetFirewallRule(toAddRuleName) == null)
                {
                    // TODO-SanKumar-1907: Even if the firewall rule is present
                    // it might be better to once verify, if all the properties
                    // are set in the rule as expected. If not, we could recreate
                    // the rule.

                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        "Adding firewall rule : {0}",
                        toAddRuleName);

                    // Canonicalizing the path using Path.GetFullPath()
                    FirewallUtils.AddFirewallRuleForTcpListener(
                                toAddRuleName,
                                description: null,
                                groupName: Settings.Default.AsrPSFwGroupName,
                                tcpPort: replicationDataPort,
                                // TODO-SanKumar-1908: Should we instead read the value of install_dir in cxps.conf?
                                applicationName: Path.GetFullPath(PSInstallationInfo.Default.GetCxpsExePath()));

                    // TODO-SanKumar-1907: If adding the firewall rule fails,
                    // raise a warning (should it be error?)
                }
                else
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        "Not adding the firewall rule : {0}, since it is already created",
                        toAddRuleName);
                }

                if (!string.IsNullOrWhiteSpace(prevSslPortInCxpsConf) &&
                    int.TryParse(prevSslPortInCxpsConf, out int portToRemoveRule) &&
                    portToRemoveRule != replicationDataPort)
                {
                    string toRemoveRuleName = $"{Settings.Default.AsrPSFWRulePrefix} {portToRemoveRule}";

                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        "Removing the firewall rule : {0}, corresponding to the previous port : {1}",
                        toRemoveRuleName, portToRemoveRule);

                    FirewallUtils.RemoveFirewallRule(toRemoveRuleName, ignoreIfNotPresent: true);

                    // TODO-SanKumar-1907: If removing the old firewall rule fails,
                    // raise a warning (should it be error?)
                }
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Successfully configured PS Http listener");
        }

        private static string UpdateCxpsConf(
            RcmOperationContext opContext,
            IdempotencyContext idempContext,
            string replLogFolderPath,
            string telemetryFolderPath,
            string reqDefFolderPath,
            string agentRepositoryPath,
            int? replicationDataPort,
            bool firstTimeConfigure, 
            string caCertThumbprint = "",
            string serverCertThumbprint = "")
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Updating the cxps config file");

            string prevSslPortInCxpsConf = null;

            if ((firstTimeConfigure) ==
                (string.IsNullOrWhiteSpace(replLogFolderPath) ||
                 string.IsNullOrWhiteSpace(telemetryFolderPath) ||
                 string.IsNullOrWhiteSpace(reqDefFolderPath) ||
                 string.IsNullOrWhiteSpace(agentRepositoryPath)))
            {
                throw new ArgumentException(
                    "These arguments are expected only during first time configure",
                    string.Join(",", new[] { nameof(replLogFolderPath), nameof(telemetryFolderPath), nameof(reqDefFolderPath), nameof(agentRepositoryPath) }));
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Reloading the {nameof(PSConfiguration)} object");

            idempContext.PSConfiguration.ReloadNow(sharedLockAcquired: false);
            var hostIdInPSConf = PSInstallationInfo.Default.GetPSHostId();

            Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        "Cert based Authentication");

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Reloading the {nameof(CxpsConfig)} object");

            idempContext.CxpsConfig.ReloadNow(sharedLockAcquired: false);

            // TODO-SanKumar-1907: If not a first time configure operation, then
            // ensure that the BiosId and HostId are consistent to avoid clone
            // (or) copied install folder/configs scenarios.

            //if (!string.IsNullOrWhiteSpace(hostIdInCxps) && hostIdInCxps != hostIdInPSConf)
            //{
            //}

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Checking if the {nameof(CxpsConfig)} says {nameof(CxpsConfig.IsRcmPSFirstTimeConfigured)} as {firstTimeConfigure}");

            if (idempContext.CxpsConfig.IsRcmPSFirstTimeConfigured.GetValueOrDefault() == firstTimeConfigure)
            {
                if (firstTimeConfigure)
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.AlreadyFirstTimeConfigured,
                        isRetryableError: false,
                        opContext: opContext);
                }
                else
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.NotYetFirstTimeConfigured,
                        isRetryableError: false,
                        opContext: opContext);
                }
            }

            string cxpsConfPath = PSInstallationInfo.Default.GetCxpsConfigPath();

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Updating the cxps config file : {0}",
                cxpsConfPath);

            using (IniEditor cxpsConfEditor =
                idempContext.CxpsConfig.GetIniEditor(IniParserOptions.RemoveDoubleQuotesInValues))
            {
                if (replicationDataPort.HasValue)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        $"Updating {CxpsConfig.Names.SslPort} in the cxps config to : {0}",
                        replicationDataPort);

                    prevSslPortInCxpsConf = cxpsConfEditor.AddOrEditValue(
                        CxpsConfig.Names.DefaultSectionName, CxpsConfig.Names.SslPort,
                        replicationDataPort.Value.ToString(CultureInfo.InvariantCulture),
                        includeDoubleQuotes: false);
                }

                if (!string.IsNullOrEmpty(caCertThumbprint))
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        $"Updating {CxpsConfig.Names.CaCertThumbprint} in the cxps config to : {caCertThumbprint}");

                    cxpsConfEditor.AddOrEditValue(CxpsConfig.Names.DefaultSectionName, CxpsConfig.Names.CaCertThumbprint, caCertThumbprint, includeDoubleQuotes: false);
                }

                if (!string.IsNullOrEmpty(serverCertThumbprint))
                {
                   
                    Tracers.RcmConf.TraceAdminLogV2Message(
                       TraceEventType.Verbose,
                       opContext,
                       $"Updating {CxpsConfig.Names.PsFingerprint} in the cxps config to : {serverCertThumbprint}");

                    cxpsConfEditor.AddOrEditValue(CxpsConfig.Names.DefaultSectionName, CxpsConfig.Names.PsFingerprint, serverCertThumbprint, includeDoubleQuotes: false);
                }

                if (firstTimeConfigure)
                {
                    string allowedDirs = string.Join(";", new[] { replLogFolderPath, telemetryFolderPath, agentRepositoryPath });
                    string remapPrefix = $"/home/svsystems \"{replLogFolderPath}\"";

                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        $"Updating the following values in the cxps config as part of first-time configure{Environment.NewLine}" +
                        $"{CxpsConfig.Names.Id} : {{0}}{Environment.NewLine}" +
                        $"{CxpsConfig.Names.AllowedDirs} : {{1}}{Environment.NewLine}" +
                        $"{CxpsConfig.Names.RemapPrefix} : {{2}}{Environment.NewLine}" +
                        $"{CxpsConfig.Names.RequestDefaultDir} : {{3}}{Environment.NewLine}",
                        hostIdInPSConf, allowedDirs, remapPrefix, reqDefFolderPath);

                    cxpsConfEditor.AddOrEditValue(
                        CxpsConfig.Names.DefaultSectionName,
                        CxpsConfig.Names.Id,
                        hostIdInPSConf,
                        includeDoubleQuotes: false);

                    cxpsConfEditor.AddOrEditValue(
                        CxpsConfig.Names.DefaultSectionName,
                        CxpsConfig.Names.AllowedDirs,
                        allowedDirs,
                        includeDoubleQuotes: false);

                    cxpsConfEditor.AddOrEditValue(
                        CxpsConfig.Names.DefaultSectionName,
                        CxpsConfig.Names.RemapPrefix,
                        remapPrefix,
                        includeDoubleQuotes: false);

                    cxpsConfEditor.AddOrEditValue(
                        CxpsConfig.Names.DefaultSectionName,
                        CxpsConfig.Names.RequestDefaultDir,
                        reqDefFolderPath,
                        includeDoubleQuotes: false);
                }

                if (firstTimeConfigure)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        $"Setting {CxpsConfig.Names.RcmPSFirstTimeConfigured} in the cxps config as yes");

                    cxpsConfEditor.AddOrEditValue(
                        CxpsConfig.Names.DefaultSectionName, CxpsConfig.Names.RcmPSFirstTimeConfigured,
                        "yes", includeDoubleQuotes: false);
                }

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    $"Saving the changes made to the cxps config file");

                // TODO-SanKumar-1904: Take flush flag as setting
                cxpsConfEditor.SaveIniFileAsync(flush: true, maintainAllBackup: true).GetAwaiter().GetResult();
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                $"Reloading the {nameof(CxpsConfig)} object to refresh newly stored data");

            idempContext.CxpsConfig.ReloadNow(sharedLockAcquired: false);

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                $"Successfully updated the cxps config file. {nameof(prevSslPortInCxpsConf)} : {0}",
                prevSslPortInCxpsConf);

            return prevSslPortInCxpsConf;
        }

        public static ConfiguratorOutput UpdateWebProxy(
            string applianceJsonPath, string clientRequestId, string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(UpdateWebProxy)} action");

                VerifyPSInstallation(opContext);

                VerifyFirstTimeConfigurationCompleted(
                    opContext,
                    idempContext,
                    expected: true,
                    includeCxpsConfig: false,
                    validateInstallation: true);

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                // TODO-SanKumar-1907: Even though this method correctly updates
                // only the proxy in non-first time configure mode, there could
                // be additions to this method in the future. So, create an
                // explicit helper to update only the Proxy settings in the
                // PSConfiguration.
                UpdateApplianceSettings(opContext, idempContext, applianceJsonPath, firstTimeConfigure: false);

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(UpdateWebProxy)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "Updating web proxy succeeded",
                    opContext: opContext);
            }
        }

        public static ConfiguratorOutput OnPassphraseRegenerated(
            string applianceJsonPath, string clientRequestId, string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(OnPassphraseRegenerated)} action");

                VerifyPSInstallation(opContext);

                VerifyFirstTimeConfigurationCompleted(
                    opContext,
                    idempContext,
                    expected: true,
                    includeCxpsConfig: true,
                    validateInstallation: true);

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                UpdateCxpsConf(
                    opContext,
                    idempContext,
                    replLogFolderPath: null,
                    telemetryFolderPath: null,
                    reqDefFolderPath: null,
                    agentRepositoryPath: null,
                    replicationDataPort: null,
                    firstTimeConfigure: false);

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(OnPassphraseRegenerated)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "Updated the passphrase in the Process Server",
                    opContext: opContext);
            }
        }

        private static void UpdateApplianceSettings(
            RcmOperationContext opContext,
            IdempotencyContext idempContext,
            string applianceJsonPath,
            bool firstTimeConfigure)
        {
            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Updating parts of the appliance settings into PS Config. First time configure : {0}",
                firstTimeConfigure);

            if (applianceJsonPath == null)
                throw new ArgumentNullException(nameof(applianceJsonPath));

            if (string.IsNullOrWhiteSpace(applianceJsonPath))
                throw new ArgumentException("Empty path", nameof(applianceJsonPath));

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Loading appliance settings from the file : {0}",
                applianceJsonPath);

            var applSettings = ApplianceSettings.LoadFromFile(applianceJsonPath);

            // TODO-SanKumar-1904: Throw specific configurator error
            if (string.IsNullOrWhiteSpace(applSettings.MachineIdentifier))
                throw new Exception($"Bad {nameof(applSettings.MachineIdentifier)} - {applSettings.MachineIdentifier}");

            // TODO-SanKumar-1908: Validate if the same machine identifier in the
            // PSConfiguration is present in the appliance json.

            Task RcmApplSettingsValidatorAsync(PSConfiguration psConfig)
            {
                // TODO-SanKumar-1904: Get only AAD token, if the RCM Uri is not set.
                // Test Rcm connectivity, if the RCM Uri is set.

                return Task.CompletedTask;
            }

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Verbose,
                opContext,
                "Setting the new data into the PS Config file after validation");

            idempContext.PSConfiguration.SetRcmPSApplianceSettingsAsync(
                applSettings,
                firstTimeConfigure,
                RcmApplSettingsValidatorAsync)
            .GetAwaiter().GetResult();

            Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Information,
                opContext,
                "Successfully updated parts of the appliance settings into PS Config");
        }

        private static string getServerCertificateThumbprint(
            RcmOperationContext opContext) 
        {
            string thumbprintFilePath = "";
            try
            {
                thumbprintFilePath = PSInstallationInfo.Default.GetPSFingerprintFilePath();

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information, opContext, $"Fetching " +
                    $"PS.Fingerprint from {thumbprintFilePath} file");

                if (FSUtils.IsNonEmptyFile(thumbprintFilePath))
                {
                    char[] charsToTrim = { '\n', '\t', ' ' };
                    string[] psFingerPrint = File.ReadAllLines(thumbprintFilePath);
                    foreach (string psFingerPrintLine in psFingerPrint)
                    {
                        string trimmedPsFingerPrintLine = psFingerPrintLine.Trim(charsToTrim);
                        if (string.IsNullOrEmpty(trimmedPsFingerPrintLine))
                            continue;
                        return trimmedPsFingerPrintLine;
                    }  
                }

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information, opContext,
                    $"PS.Fingerprint empty file content");
            }
            catch (Exception ex)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        opContext,
                        $"Exception occurred with PSFingerprint Path {ex.StackTrace}");

                if (ex is UnauthorizedAccessException || ex is NotSupportedException)
                {
                    throw new ConfiguratorException(
                        PSRcmAgentErrorEnum.FingerprintAccessFailure,
                        isRetryableError: false,
                        opContext: opContext,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["PsFingerPrintFilePath"] = thumbprintFilePath
                        });
                }
            }

            throw new ConfiguratorException(
                    PSRcmAgentErrorEnum.FingerprintReadInternalError,
                    isRetryableError: false,
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["PsFingerPrintFilePath"] = thumbprintFilePath
                    });
        }

        public static ConfiguratorOutput RegisterProcessServer(
            Uri rcmUri,
            string applianceJsonPath,
            string clientRequestId,
            string activityId,
            string caCertThumbprint)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(RegisterProcessServer)} action");

                VerifyPSInstallation(opContext);

                VerifyFirstTimeConfigurationCompleted(
                    opContext,
                    idempContext,
                    expected: true,
                    includeCxpsConfig: true,
                    validateInstallation: true);

                // NOTE-SanKumar-1904: No need to check if the PS is already
                // registered, since register PS is an idempotent operation at Rcm.

                // TODO-SanKumar-1904: Move to settings
                TimeSpan timeout = TimeSpan.FromMinutes(2);

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                string serverCertThumbprint = getServerCertificateThumbprint(opContext);

                UpdateCxpsConf(opContext,
                   idempContext,
                   replLogFolderPath: null,
                   telemetryFolderPath: null,
                   reqDefFolderPath: null,
                   agentRepositoryPath: null,
                   replicationDataPort: null,
                   firstTimeConfigure: false,
                   caCertThumbprint: caCertThumbprint,
                   serverCertThumbprint: serverCertThumbprint);

                // TODO-SanKumar-1907: Factory should pass on the timeout, otherwise take from Settings.
                async Task RcmUriValidator(PSConfiguration psConfig, bool register)
                {
                    using (var psRcmApi = RcmApiFactory.GetProcessServerRcmApiStubs(psConfig))
                    using (var cts = new CancellationTokenSource(timeout))
                    {
                        if (register)
                        {
                            await
                                psRcmApi.RegisterProcessServerAsync(
                                    opContext,
                                    SystemUtils.BuildRcmPSRegisterInput(serverCertThumbprint),
                                    cts.Token)
                                .ConfigureAwait(false);
                        }
                        else
                        {
                            await
                                psRcmApi.ModifyProcessServerAsync(
                                    opContext,
                                    SystemUtils.BuildRcmPSModifyInput(serverCertThumbprint),
                                    cts.Token)
                                .ConfigureAwait(false);
                        }
                    }
                }

                bool modified = false;
                try
                {
                    try
                    {
                        Tracers.RcmConf.TraceAdminLogV2Message(
                            TraceEventType.Verbose,
                            opContext,
                            "Registering the PS to Rcm reached via Uri : {0}",
                            rcmUri);

                        idempContext.PSConfiguration.SetRcmUriAsync(
                            rcmUri,
                            async (psConfig) => await RcmUriValidator(psConfig, register: true).ConfigureAwait(false))
                        .GetAwaiter().GetResult();
                    }
                    catch (RcmException rcmEx)
                    when (rcmEx.ErrorCode == nameof(RcmErrorCode.ApplianceComponentAlreadyRegistered))
                    {
                        Tracers.RcmConf.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            opContext,
                            "Failed to register the PS and Rcm returned " +
                            $"{nameof(RcmErrorCode.ApplianceComponentAlreadyRegistered)} error. So, " +
                            "with the assumption that none of the first-time configured settings " +
                            "(previously used info to register the PS), invoking ModifyProcessServer.");

                        modified = true;

                        idempContext.PSConfiguration.SetRcmUriAsync(
                            rcmUri,
                            async (psConfig) => await RcmUriValidator(psConfig, register: false).ConfigureAwait(false))
                        .GetAwaiter().GetResult();
                    }
                }
                catch (SocketException socEx)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        opContext,
                        "Attempting to {0} PS failed at Rcm{1}{2}",
                        modified ? "Modify" : "Register", Environment.NewLine, socEx);

                    return ConfiguratorOutput.Build(
                        PSRcmAgentErrorEnum.DnsFqdnFetchFailed,
                        isRetryableError: true,
                        opContext: opContext);
                }
                catch (RcmException rcmEx)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        opContext,
                        "Attempting to {0} PS failed at Rcm{1}{2}",
                        modified ? "Modify" : "Register", Environment.NewLine, rcmEx);

                    return ConfiguratorOutput.Build(
                        PSRcmAgentErrorEnum.RegisterOrModifyWithRcmFailed,
                        isRetryableError: rcmEx.IsRetryableError,
                        opContext: opContext,
                        rcmEx: rcmEx);
                }

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(RegisterProcessServer)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "RCM PS Registration succeeded",
                    opContext: opContext,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["RcmAction"] = modified ? "ModifyProcessServer" : "RegisterProcessServer"
                    });
            }
        }

        public static ConfiguratorOutput GetProcessServerConfigurationStatus(
            string applianceJsonPath, string clientRequestId, string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                // TODO-SanKumar-1904: TBD

                Tracers.RcmConf.TraceAdminLogV2Message(
                TraceEventType.Warning,
                opContext,
                $"{nameof(GetProcessServerConfigurationStatus)} action is yet to be implemented");

                CommitAndReloadLazyObjects(idempContext);

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    opContext: opContext);
            }
        }

        public static ConfiguratorOutput TestRcmConnection(
            string applianceJsonPath, string clientRequestId, string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(TestRcmConnection)} action");

                VerifyPSInstallation(opContext);

                VerifyFirstTimeConfigurationCompleted(
                    opContext,
                    idempContext,
                    expected: true,
                    includeCxpsConfig: false,
                    validateInstallation: true);

                VerifyRegisteredWithRcm(opContext, idempContext);

                // TODO-SanKumar-1904: Factory should pass on the timeout, otherwise take from Settings.
                TimeSpan timeout = TimeSpan.FromMinutes(1);

                var psConfig = idempContext.PSConfiguration;

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Testing the connection of the PS to Rcm via Uri : {0}",
                    psConfig.RcmUri);

                try
                {
                    using (var psRcmApi = RcmApiFactory.GetProcessServerRcmApiStubs(psConfig))
                    using (var cts = new CancellationTokenSource(timeout))
                    {
                        psRcmApi.TestConnectionAsync(
                            opContext,
                            $"Testing connection from {psConfig.HostId} ({Dns.GetHostEntry("localhost").HostName}) [{PSInstallationInfo.Default.GetPSCurrentVersion()}]",
                            cts.Token).Wait();
                    }
                }
                catch (RcmException rcmEx)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        opContext,
                        "Failed to test connection with Rcm{0}{1}",
                        Environment.NewLine, rcmEx);

                    return ConfiguratorOutput.Build(
                        PSRcmAgentErrorEnum.TestConnectionWithRcmFailed,
                        isRetryableError: rcmEx.IsRetryableError,
                        opContext: opContext,
                        rcmEx: rcmEx);
                }

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(TestRcmConnection)} action");

                // TODO-SanKumar-1903: Should we return RCM uri in the msg params?
                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "Testing RCM connection succeeded",
                    opContext: opContext);
            }
        }

        public static ConfiguratorOutput UnregisterProcessServer(
            string applianceJsonPath, string clientRequestId, string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(UnregisterProcessServer)} action");

                VerifyPSInstallation(opContext);

                // TODO-SanKumar-1907: Should we relax this check here?
                VerifyFirstTimeConfigurationCompleted(
                    opContext,
                    idempContext,
                    expected: true,
                    includeCxpsConfig: false,
                    validateInstallation: true);

                try
                {
                    VerifyRegisteredWithRcm(opContext, idempContext);
                }
                catch (ConfiguratorException cex) when (cex.ErrorCode == PSRcmAgentErrorEnum.NotRegistered)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        opContext,
                        $"Completing the {nameof(UnregisterProcessServer)} action as " +
                        "successful, since the Rcm PS is not yet registered/already unregistered");

                    return ConfiguratorOutput.Build(
                        PSRcmAgentErrorEnum.Success,
                        isRetryableError: false,
                        message: "RCM PS is not yet registered/already unregistered",
                        opContext: opContext);
                }

                // TODO-SanKumar-1907: Should we check, if it's already
                // unregistered and quit? This should actually be idempotent for
                // us. We could also return success, if Rcm throws not found too.

                // TODO-SanKumar-1907: Move to settings and this could also be taken from the
                // command line.
                TimeSpan timeout = TimeSpan.FromMinutes(2);

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                // TODO-SanKumar-1904: Either get it from settings or pass from the script.
                var lockTimeout = TimeSpan.FromMinutes(1);

                string cachedSettingsFilePath = PSInstallationInfo.Default.GetPSCachedSettingsFilePath();

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Deleting the cached settings file : {0}, as it wouldn't be valid " +
                    "after a successful unregistration with the Rcm",
                    cachedSettingsFilePath);

                // The cached settings file wouldn't be valid anymore on successful
                // unregistration, so delete it up ahead to be safe.
                using (var lockFile = FileUtils.AcquireLockFile(cachedSettingsFilePath, exclusive: true, maxWaitTime: lockTimeout))
                {
                    if (File.Exists(cachedSettingsFilePath))
                    {
                        File.Delete(cachedSettingsFilePath);
                    }
                }

                var hostId = PSInstallationInfo.Default.GetPSHostId();

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Unregistering the Process Server : {0} from Rcm via Uri : {1}",
                    hostId, idempContext.PSConfiguration.RcmUri);

                try
                {
                    using (var psRcmApi = RcmApiFactory.GetProcessServerRcmApiStubs(idempContext.PSConfiguration))
                    using (var cts = new CancellationTokenSource(timeout))
                    {
                        // TODO-SanKumar-1904: This id input must be taken from the
                        // PSConfiguration object passed to the stubs factory method.
                        psRcmApi
                            .UnregisterProcessServerAsync(opContext, hostId, cts.Token)
                            .GetAwaiter()
                            .GetResult();
                    }
                }
                catch (RcmException rcmEx)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        opContext,
                        "Failed to Unregister Process Server{0}{1}",
                        Environment.NewLine, rcmEx);

                    return ConfiguratorOutput.Build(
                        PSRcmAgentErrorEnum.UnregisterWithRcmFailed,
                        isRetryableError: rcmEx.IsRetryableError,
                        opContext: opContext,
                        rcmEx: rcmEx);
                }

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Clearing the Rcm Uri from the PS Config file");

                idempContext.PSConfiguration.ClearRcmUriAsync().GetAwaiter().GetResult();

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(UnregisterProcessServer)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "RCM PS unregistration succeeded",
                    opContext: opContext);
            }
        }

        public static ConfiguratorOutput UnconfigureProcessServer(
            bool force, string applianceJsonPath, string clientRequestId, string activityId)
        {
            return RunConfiguratorAction(applianceJsonPath, clientRequestId, activityId, ActionImpl);

            ConfiguratorOutput ActionImpl(RcmOperationContext opContext, IdempotencyContext idempContext)
            {
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Starting the {nameof(UnconfigureProcessServer)} action");

                VerifyPSInstallation(opContext);

                bool isPSRegistered = true, failedRetrievingPSRegInfo = false;
                try
                {
                    VerifyRegisteredWithRcm(opContext, idempContext);
                }
                catch (ConfiguratorException cex) when (cex.ErrorCode == PSRcmAgentErrorEnum.NotRegistered)
                {
                    isPSRegistered = false;
                }
                catch (Exception ex)
                {
                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        opContext,
                        "Failed to verify if the PS is unregistered. Ignoring " +
                        "this check and continuing, since it could be due to " +
                        "a corrupt PSConfiguration file{0}{1}",
                        Environment.NewLine, ex);

                    failedRetrievingPSRegInfo = true;
                }

                if (!failedRetrievingPSRegInfo && isPSRegistered)
                {
                    if (force)
                    {
                        Tracers.RcmConf.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            opContext,
                            $"Continuing with the {nameof(UnconfigureProcessServer)} " +
                            $"action even though the PS is registered to the Rcm service, " +
                            $"since {nameof(force)} flag is set");
                    }
                    else
                    {
                        Tracers.RcmConf.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            opContext,
                            $"Failing the {nameof(UnconfigureProcessServer)} action, " +
                            $"since the PS is still registered to the Rcm service " +
                            $"and {nameof(force)} flag is not set");

                        return ConfiguratorOutput.Build(
                            PSRcmAgentErrorEnum.UnregisterNeeded,
                            isRetryableError: false,
                            opContext: opContext);
                    }
                }

                ServiceUtils.StopProcessServerServices(opContext, Tracers.RcmConf);

                // Since this is a factory reset, we wouldn't trust just the IsFirstTimeConfigured
                // values set to false in the configuration files. So just go ahead and still do the
                // cleanup, even if not yet configured.

                // TODO-SanKumar-2004: Since unconfigure would be probably running
                // in a negative case, being cautious and acquiring the locks before
                // resetting the files. Should we simply do this for all the configurator actions?

                // TODO-SanKumar-1904: Either get it from settings or pass from the script.
                var lockTimeout = TimeSpan.FromMinutes(1);

                string cachedSettingsFilePath = PSInstallationInfo.Default.GetPSCachedSettingsFilePath();
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Deleting the cached PS settings file : {0}",
                    cachedSettingsFilePath);

                if (cachedSettingsFilePath != null)
                {
                    using (var lockFile = FileUtils.AcquireLockFile(cachedSettingsFilePath, exclusive: true, maxWaitTime: lockTimeout))
                    {
                        if (File.Exists(cachedSettingsFilePath))
                        {
                            File.Delete(cachedSettingsFilePath);

                            Tracers.RcmConf.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                opContext,
                                "Successfully deleted the cached PS Settings file : {0}",
                                cachedSettingsFilePath);
                        }
                        else
                        {
                            Tracers.RcmConf.TraceAdminLogV2Message(
                                TraceEventType.Verbose,
                                opContext,
                                "Couldn't find the cached PS Settings file : {0}",
                                cachedSettingsFilePath);
                        }
                    }
                }

                string psConfigurationPath = PSInstallationInfo.Default.GetPSConfigPath();
                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Deleting the PS configuration file : {0}",
                    psConfigurationPath);

                if (psConfigurationPath != null)
                {
                    // TODO-SanKumar-2004: Deleting this file would mean
                    // removing the app insights config as well? Should
                    // we retain at least that setting to let the configurator
                    // logs be attempted to be uploaded via Process Server?
                    // Or should it be a clean sweep with the deletion as below?

                    Tracers.RcmConf.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        opContext,
                        "Marking PS configuration file : {0} to be deleted as part of idempotency context : {1}",
                        psConfigurationPath, idempContext);

                    idempContext.AddFileToBeDeleted(
                        psConfigurationPath,
                        psConfigurationPath + ".lck",
                        Path.Combine(Path.GetDirectoryName(psConfigurationPath), "Backup"));
                }

                string cxpsConfigPath = PSInstallationInfo.Default.GetCxpsConfigPath();
                string cxpsConfigGoldenFilePath = PSInstallationInfo.Default.GetCxpsGoldenConfigPath();

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Marking cxps conf file : {0} to be reset with the contents of the " +
                    "golden cxps conf file : {1}.",
                    cxpsConfigPath, cxpsConfigGoldenFilePath);

                using (var lockFile = FileUtils.AcquireLockFile(cxpsConfigPath, exclusive: true, maxWaitTime: lockTimeout))
                {
                    FSUtils.ParseFilePath(cxpsConfigPath, out string parentDir, out string fileNameWithoutExt, out string fileExt);

                    string lockFilePath = cxpsConfigPath + ".lck";
                    string backupFolder = Path.Combine(Path.GetDirectoryName(cxpsConfigPath), "Backup");
                    string goldenFileCopy = Path.Combine(parentDir, $"{fileNameWithoutExt}_{idempContext.Identifier}{fileExt}");
                    
                    IdempotencyContext.PersistFileToBeDeleted(
                        filePath: goldenFileCopy,
                        lockFilePath: lockFilePath,
                        backupFolderPath: backupFolder);

                    File.Copy(
                        sourceFileName: cxpsConfigGoldenFilePath,
                        destFileName: goldenFileCopy,
                        overwrite: true);

                    // cxps_golden.conf is hidden and read-only in order to be tamperproof.
                    File.SetAttributes(
                        goldenFileCopy,
                        File.GetAttributes(goldenFileCopy) & ~(FileAttributes.ReadOnly | FileAttributes.Hidden));

                    FileUtils.FlushFileBuffers(goldenFileCopy);

                    idempContext.AddFileToBeReplaced(
                        cxpsConfigPath,
                        latestFilePath: goldenFileCopy,
                        lockFilePath: lockFilePath,
                        backupFolderPath: backupFolder,
                        revertLatestFileCleanupGuard: true);
                }

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    opContext,
                    "Succeeded in marking cxps conf file : {0} to be reset to golden file.",
                    cxpsConfigPath);

                ClearCacheLocationFolders(opContext, idempContext);

                CommitAndReloadLazyObjects(idempContext);

                Tracers.RcmConf.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    opContext,
                    $"Successfully completed the {nameof(UnconfigureProcessServer)} action");

                return ConfiguratorOutput.Build(
                    PSRcmAgentErrorEnum.Success,
                    isRetryableError: false,
                    message: "RCM PS unconfigure succeeded",
                    opContext: opContext);
            }
        }
    }
}
