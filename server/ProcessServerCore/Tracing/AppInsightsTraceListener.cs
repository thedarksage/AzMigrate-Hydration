using Microsoft.ApplicationInsights;
using Microsoft.ApplicationInsights.Channel;
using Microsoft.ApplicationInsights.DataContracts;
using Microsoft.ApplicationInsights.Extensibility;
using Microsoft.ApplicationInsights.WindowsServer.TelemetryChannel;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    public sealed class AppInsightsTraceListener : PSTraceListenerBase
    {
        private readonly ServerTelemetryChannel m_serverTelemetryChannel;

        private readonly TelemetryClient m_telemetryClient;

        private readonly string m_machineIdentifier, m_providerId, m_siteSubscriptionId;

        private readonly bool m_waitInTheEndForFlush;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="initializeData">JSON containing the following info:
        /// 1. InstrumentationKey - Application insights instrumentation key
        /// 2. MachineIdentifier - PS Host Id
        /// 3. ProviderId - Value filled in the Account Id column in Kusto
        /// 4. SiteSubscriptionId (optional) - Subscription Id of the site
        /// 5. ComponentId - ComponentId passed to MDS (Ex: ProcessServer)
        /// 6. BackupFolderPath (optional) - Backup storage folder for overflowing
        /// or failed logs. These will be retried by the client.
        /// 7. WaitInTheEndForFlush (optional) - Wait for the asynchronous flush
        /// to complete on ending the trace listener.
        /// </param>
        /// <remarks>Not implementing the default constructor, this class can't
        /// operate without valid initializeData</remarks>
        public AppInsightsTraceListener(string initializeData)
        {
            const string InstrumentationKeyKey = "InstrumentationKey";
            const string MachineIdentifierKey = "MachineIdentifier";
            const string ProviderIdKey = "ProviderId";
            const string SiteSubscriptionIdKey = "SiteSubscriptionId";
            const string ComponentIdKey = "ComponentId";
            const string BackupFolderPathKey = "BackupFolderPath";
            const string WaitInTheEndForFlushKey = "WaitInTheEndForFlush";

            // TODO-SanKumar-2004: Support well-known folder path for backup folder path,
            // similar to MDS TraceListener?

            var initDataKVPairs =
                JsonConvert.DeserializeObject<Dictionary<string, string>>(
                    initializeData, JsonUtils.GetStandardSerializerSettings(indent: false));

            initDataKVPairs.TryGetValue(InstrumentationKeyKey, out string instrumentationKey);
            if (string.IsNullOrWhiteSpace(instrumentationKey))
                throw new ArgumentNullException(InstrumentationKeyKey);

            initDataKVPairs.TryGetValue(ProviderIdKey, out m_providerId);
            if (string.IsNullOrWhiteSpace(m_providerId))
                throw new ArgumentNullException(ProviderIdKey);

            initDataKVPairs.TryGetValue(MachineIdentifierKey, out m_machineIdentifier);
            if (string.IsNullOrWhiteSpace(m_machineIdentifier))
                throw new ArgumentNullException(MachineIdentifierKey);

            initDataKVPairs.TryGetValue(ComponentIdKey, out string componentId);
            if (string.IsNullOrWhiteSpace(componentId))
                throw new ArgumentNullException(ComponentIdKey);

            initDataKVPairs.TryGetValue(SiteSubscriptionIdKey, out m_siteSubscriptionId);

            if (!initDataKVPairs.TryGetValue(WaitInTheEndForFlushKey, out string waitInTheEndForFlushVal) ||
                !bool.TryParse(waitInTheEndForFlushVal, out m_waitInTheEndForFlush))
            {
                m_waitInTheEndForFlush = false;
            }

            // App insights trace listener is only meant to emulate InMageAdminLogV2 table
            const MdsTable mdsTable = MdsTable.InMageAdminLogV2;
            base.Initialize(mdsTable, componentId);

            // Initialize the telemetry processor. TelemetryClients created after this point will use your processors.
            // disabling sampling in this processor to ensure all telemetry logs are pushed.
            var builder = TelemetryConfiguration.Active.TelemetryProcessorChainBuilder.Use(
                (next) => new SamplingTelemetryProcessor(next));
            builder.UseSampling(0 /*disable sampling*/); // percentage
            builder.Build();

            var telIniter = new TelemetryInitializer(
                machineIdentifier: m_machineIdentifier,
                subscriptionId: m_siteSubscriptionId,
                componentId: componentId);

            TelemetryConfiguration.Active.InstrumentationKey = instrumentationKey;
            TelemetryConfiguration.Active.TelemetryInitializers.Add(telIniter);

            m_serverTelemetryChannel = new ServerTelemetryChannel();

            if (initDataKVPairs.TryGetValue(BackupFolderPathKey, out string backupFolderPath) &&
                !string.IsNullOrEmpty(backupFolderPath))
            {
                if (!Directory.Exists(backupFolderPath))
                    FSUtils.CreateAdminsAndSystemOnlyDir(backupFolderPath);

                m_serverTelemetryChannel.StorageFolder = backupFolderPath;
                // TODO-SanKumar-2004: Tweak the buffer sizes and timeouts to ensure
                // that most/all of the in-memory logs are guaranteed to be at the
                // least persistent locally in the backup folder.
            }

#if DEBUG
            m_serverTelemetryChannel.DeveloperMode = true;
#endif

            m_serverTelemetryChannel.Initialize(TelemetryConfiguration.Active);
            // TODO-SanKumar-2004: Is this needed here, since we anyway have the
            // reference to the channel until this class is disposed?
            TelemetryConfiguration.Active.TelemetryChannel = m_serverTelemetryChannel;

            var telemetryConfig = new TelemetryConfiguration(instrumentationKey, m_serverTelemetryChannel);
            telemetryConfig.TelemetryInitializers.Add(telIniter);

            m_telemetryClient = new TelemetryClient(telemetryConfig);

            // Mapping field in Kusto "AccountId" represents provider for diagnostics
            // (i.e. ASR migration appliance).
            m_telemetryClient.Context.User.AccountId = m_providerId;

            // Mapping field in Kusto "AnonId" represents unique machine identifier.
            m_telemetryClient.Context.User.Id = m_machineIdentifier;
        }

        /// <inheritdoc/>
        public override bool IsThreadSafe => true; // Underlying TelemetryClient is thread-safe

        /// <inheritdoc/>
        public override string ToString()
        {
            return $"{nameof(AppInsightsTraceListener)}: {this.Name}";
        }

        /// <inheritdoc/>
        public override void Flush()
        {
            if (m_isDisposed != 0)
                return;

            m_telemetryClient.Flush(); // This is asynchronous
        }

        /// <inheritdoc/>
        protected override void OnTraceData(TraceEventCache eventCache, string source, TraceEventType eventType, int id, object data1)
        {
            if (m_isDisposed != 0)
                return;

            switch (this.MdsTable)
            {
                case MdsTable.InMageAdminLogV2:
                    var adminLogV2TracePkt = data1 as InMageAdminLogV2TracePacket;

                    var adminV2RowObj = InMageAdminLogV2Row.Build(
                        eventCache, this.SystemProperties, source,
                        eventType, adminLogV2TracePkt?.OpContext,
                        id, adminLogV2TracePkt?.FormatString, adminLogV2TracePkt?.FormatArgs);

                    if (adminLogV2TracePkt != null)
                    {
                        adminV2RowObj.DiskId = adminLogV2TracePkt.DiskId;
                        adminV2RowObj.DiskNumber = adminLogV2TracePkt.DiskNumber;
                        adminV2RowObj.ErrorCode = adminLogV2TracePkt.ErrorCode;
                    }

                    // TODO-SanKumar-1903: this.TraceOutputOptions.HasFlag(TraceOptions.Callstack)
                    // Support logging the stack trace in the extended properties for
                    // debugging critical issues without debugger.

                    WriteToAppInsights(adminV2RowObj, adminLogV2TracePkt?.OpContext);

                    break;

                default:
                    throw new NotImplementedException(this.MdsTable.ToString());
            }
        }

        /// <inheritdoc/>
        protected override void OnTraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args)
        {
            if (m_isDisposed != 0)
                return;

            switch (this.MdsTable)
            {
                case MdsTable.InMageAdminLogV2:
                    var rowObj = InMageAdminLogV2Row.Build(
                        eventCache, this.SystemProperties, source, eventType, null, id, format, args);

                    // TODO-SanKumar-2004: this.TraceOutputOptions.HasFlag(TraceOptions.Callstack)
                    // Support logging the stack trace in the extended properties for
                    // debugging critical issues without debugger.

                    WriteToAppInsights(rowObj, null);

                    break;

                default:
                    throw new NotImplementedException(this.MdsTable.ToString());
            }
        }

        private static SeverityLevel GetSeverityLevel(TraceEventType traceEventType)
        {
            switch (traceEventType)
            {
                case TraceEventType.Critical:
                    return SeverityLevel.Critical;
                case TraceEventType.Error:
                    return SeverityLevel.Error;
                case TraceEventType.Warning:
                    return SeverityLevel.Warning;
                case TraceEventType.Information:
                    return SeverityLevel.Information;
                case TraceEventType.Verbose:
                    return SeverityLevel.Verbose;
                case TraceEventType.Start:
                case TraceEventType.Stop:
                case TraceEventType.Suspend:
                case TraceEventType.Resume:
                case TraceEventType.Transfer:
                default:
                    Debug.Assert(false);
                    return SeverityLevel.Information;
            }
        }

        private void WriteToAppInsights(InMageAdminLogV2Row rowObj, RcmOperationContext opContext)
        {
            if (rowObj == null)
                throw new ArgumentNullException(nameof(rowObj));

            TraceTelemetry telemetry = new TraceTelemetry(
                message: rowObj.Message,
                severityLevel: GetSeverityLevel(rowObj.AgentLogLevel));

            telemetry.Context.Operation.Id = opContext?.ActivityId;
            telemetry.Context.Operation.ParentId = opContext?.ClientRequestId;

            telemetry.Properties[nameof(rowObj.AgentTid)] = rowObj.AgentTid;

            telemetry.Timestamp = rowObj.AgentTimeStamp;

            if (rowObj.BiosId != null)
                telemetry.Properties[nameof(rowObj.BiosId)] = rowObj.BiosId;

            if (rowObj.DiskId != null)
                telemetry.Properties[nameof(rowObj.DiskId)] = rowObj.DiskId;

            if (rowObj.DiskNumber != null)
                telemetry.Properties[nameof(rowObj.DiskNumber)] = rowObj.DiskNumber;

            if (rowObj.ErrorCode != null)
                telemetry.Properties[nameof(rowObj.ErrorCode)] = rowObj.ErrorCode;

            if (rowObj.ExtendedPropertiesDict != null)
            {
                foreach (var currKVPair in rowObj.ExtendedPropertiesDict)
                    telemetry.Properties.Add(currKVPair);
            }

            if (rowObj.FabType != null)
                telemetry.Properties[nameof(rowObj.FabType)] = rowObj.FabType;

            if (rowObj.MemberName != null)
                telemetry.Properties[nameof(rowObj.MemberName)] = rowObj.MemberName;

            telemetry.Sequence = rowObj.SequenceNumber.ToString(CultureInfo.InvariantCulture);

            if (rowObj.SourceEventId != null)
                telemetry.Properties[nameof(rowObj.SourceEventId)] = rowObj.SourceEventId;

            if (rowObj.SourceFilePath != null)
                telemetry.Properties[nameof(rowObj.SourceFilePath)] = rowObj.SourceFilePath;

            if (rowObj.SourceLineNumber != null)
                telemetry.Properties[nameof(rowObj.SourceLineNumber)] = rowObj.SourceLineNumber;

            if (rowObj.SubComponent != null)
                telemetry.Properties[nameof(rowObj.SubComponent)] = rowObj.SubComponent;

            m_telemetryClient.TrackTrace(telemetry);

            // TODO-SanKumar-2004: Should we add this here for additional guarantee? Anyway, if the
            // consumer needs this behavior, it could be achieved by setting Trace.AutoFlush = true
            // in code or in App.config.
            //m_telemetryClient.Flush();
        }

        #region Dispose Pattern

        private int m_isDisposed = 0;

        //private void ThrowIfDisposed()
        //{
        //    if (m_isDisposed != 0)
        //        throw new ObjectDisposedException(this.ToString());
        //}

        /// <inheritdoc/>
        protected override void Dispose(bool disposing)
        {
            // Do nothing and return, if already dispose started / completed.
            if (0 != Interlocked.CompareExchange(ref m_isDisposed, 1, 0))
                return;

            if (disposing)
            {
                if (m_telemetryClient != null &&
                    TaskUtils.RunAndIgnoreException(m_telemetryClient.Flush, Tracers.Misc) &&
                    m_waitInTheEndForFlush)
                {
                    // Flush() of ServerTelemetryChannel is asynchronous. The documentation 
                    // suggests that ample time must be given before closing the appliation.
                    Task.Delay(Settings.Default.AITraceListener_WaitTimeForFlush).Wait();
                }

                m_serverTelemetryChannel.TryDispose(Tracers.Misc);
            }

            base.Dispose(disposing);
        }

        #endregion Dispose Pattern
    }

    public class TelemetryInitializer : ITelemetryInitializer
    {
        private static readonly string s_pid = Process.GetCurrentProcess().Id.ToString();
        private static readonly string s_roleName = Process.GetCurrentProcess().ProcessName;

        private readonly string m_machineIdentifier, m_subscriptionId, m_componentId, m_version, m_fqdn;

        public TelemetryInitializer(string machineIdentifier, string subscriptionId, string componentId)
        {
            if (string.IsNullOrWhiteSpace(machineIdentifier))
                throw new ArgumentNullException(nameof(machineIdentifier));

            m_machineIdentifier = machineIdentifier;
            m_subscriptionId = subscriptionId;
            m_componentId = componentId;
            m_version = PSInstallationInfo.Default.GetPSCurrentVersion();
            m_fqdn = SystemUtils.GetFqdn();
        }

        public void Initialize(ITelemetry telemetry)
        {
            telemetry.Context.Device.Id = m_machineIdentifier;
            telemetry.Context.Component.Version = m_version;
            telemetry.Context.Cloud.RoleName = s_roleName;

            telemetry.Context.GlobalProperties[nameof(InMageAdminLogV2Row.AgentPid)] = s_pid;
            telemetry.Context.GlobalProperties[nameof(InMageAdminLogV2Row.MachineId)] = m_machineIdentifier;
            // Reusing the property names similar to Appliance's app insights logs
            telemetry.Context.GlobalProperties["HostName"] = m_fqdn;
            telemetry.Context.GlobalProperties["ComponentType"] = m_componentId;
            telemetry.Context.GlobalProperties["ComponentVersion"] = m_version;
            telemetry.Context.GlobalProperties["SubscriptionId"] = m_subscriptionId;
            telemetry.Context.GlobalProperties["ResourceId"] = m_machineIdentifier;
        }
    }
}
