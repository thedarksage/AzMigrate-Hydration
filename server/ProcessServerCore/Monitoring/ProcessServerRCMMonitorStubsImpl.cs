using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.ServiceBus.Messaging;
using Newtonsoft.Json;
using RcmContract;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using static Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils.MiscUtils;
using static RcmContract.MonitoringMsgEnum;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    class ProcessServerRCMMonitorStubsImpl : IProcessServerCSApiStubs
    {
        ProcessServerSettings psSettings;
        ProcessServerComponentHealthMsg psComponentHealthMsg;
        volatile IMessageServiceWrapper psMessageServiceWrapperInformation;
        volatile IMessageServiceWrapper psMessageServiceWrapperCritical;
        ConcurrentDictionary<string, IMessageServiceWrapper> hostIdInfoMessageWrapperMap;
        ConcurrentDictionary<string, IMessageServiceWrapper> hostIdCriticalMessageWrapperMap;

        private const long unknownValLong = -1;
        private const double unknownValDouble = -1;

        private static readonly SemaphoreSlim semaphoreSlim = new SemaphoreSlim(1);

        private string psMessageContext;
        Stopwatch psHealthSW = new Stopwatch();

        bool isFirstTime = true;

        public ProcessServerRCMMonitorStubsImpl()
        {
            psComponentHealthMsg = new ProcessServerComponentHealthMsg
            {
                HealthIssues = new List<HealthIssue>()
            };

            psSettings = ProcessServerSettings.GetCachedSettings();

            psMessageServiceWrapperInformation = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.PSInformation);
            psMessageServiceWrapperCritical = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.PSCritical);
            // Maintains a map of HostId vs the messageWrapper, as each host different event hub channel to post to.
            hostIdInfoMessageWrapperMap = new ConcurrentDictionary<string, IMessageServiceWrapper>();
            hostIdCriticalMessageWrapperMap = new ConcurrentDictionary<string, IMessageServiceWrapper>();

            // To listen and update to the Event hub uri changes in PSSettings
            ProcessServerSettings.HostInfoChannelUriChanged += ProcessServerSettings_HostMonitoringInfoUriChanged;
            ProcessServerSettings.HostCriticalChannelUriChanged += ProcessServerSettings_HostMonitoringCriticalUriChanged;
            ProcessServerSettings.PSInfoChannelUriChanged += ProcessServerSettings_PSMonitoringInfoUriChanged;
            ProcessServerSettings.PSCriticalChannelUriChanged += ProcessServerSettings_PSMonitoringCriticalUriChanged;
        }

        private static double GetPercentage(double actual, double total)
        {
            if (total == 0)
                throw new InvalidDataException("Total can't be 0");
            return 100 * (actual  / total);
        }

        public void Close()
        {
            Dispose();
        }


        public void Dispose()
        {
            try
            {
                ProcessServerSettings.HostInfoChannelUriChanged -= ProcessServerSettings_HostMonitoringInfoUriChanged;
                ProcessServerSettings.HostCriticalChannelUriChanged -= ProcessServerSettings_HostMonitoringCriticalUriChanged;
                ProcessServerSettings.PSInfoChannelUriChanged -= ProcessServerSettings_PSMonitoringInfoUriChanged;
                ProcessServerSettings.PSCriticalChannelUriChanged -= ProcessServerSettings_PSMonitoringCriticalUriChanged;
            }
            catch(Exception ex)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Exception while disposing RCMMonitorStubsImpl: {0}{1}",
                    Environment.NewLine,
                    ex);
            }
        }

        public Task<ProcessServerSettings> GetPSSettings()
        {
            return GetPSSettings(CancellationToken.None);
        }

        public Task<ProcessServerSettings> GetPSSettings(CancellationToken cancellationToken)
        {
            throw new NotImplementedException();
        }

        public Task ReportInfraHealthFactorsAsync(IEnumerable<InfraHealthFactorItem> infraHealthFactors)
        {
            return ReportInfraHealthFactorsAsync(infraHealthFactors, CancellationToken.None);
        }

        public async Task ReportInfraHealthFactorsAsync(IEnumerable<InfraHealthFactorItem> infraHealthFactors, CancellationToken cancellationToken)
        {
            // Lightweight alternative for lock()
            await semaphoreSlim.WaitAsync(cancellationToken).ConfigureAwait(false);
            try
            {
                foreach (InfraHealthFactorItem currItem in infraHealthFactors)
                {
                    //Adding only infraHealthFactors which are set, whichever is absent from the PSComponentHealthMessage
                    // will automatically be removed.
                    if (!currItem.Set)
                    {
                        continue;
                    }

                    if (!HealthUtils.RCMInfraHealthFactorStrings.ContainsKey(currItem.InfraHealthFactor))
                        throw new NotSupportedException(currItem.InfraHealthFactor + " is not supported");

                    var currInfraHFCode = HealthUtils.RCMInfraHealthFactorStrings[currItem.InfraHealthFactor].Item1;
                    var currInfraHFLevel = HealthUtils.RCMInfraHealthFactorStrings[currItem.InfraHealthFactor].Item2;
                    var messageParams = new Dictionary<string, string>();
                    if (currItem.Placeholders != null)
                    {
                        messageParams = PlaceholderToDictionary(currItem.Placeholders);
                    }

                    psComponentHealthMsg.HealthIssues.Add(
                                            new HealthIssue(currInfraHFCode,
                                                            currInfraHFLevel.ToString(),
                                                            MonitoringMessageSource.ProcessServer.ToString(),
                                                            DateTime.UtcNow,
                                                            messageParams,
                                                            MonitoringMessageSource.ProcessServer.ToString()));
                }

                FlushMessages(cancellationToken);
            }
            finally
            {
                semaphoreSlim.Release();
            }
        }

        public Task ReportStatisticsAsync(ProcessServerStatistics stats)
        {
            return ReportStatisticsAsync(stats, CancellationToken.None);
        }

        public async Task ReportStatisticsAsync(ProcessServerStatistics stats, CancellationToken cancellationToken)
        {
            await semaphoreSlim.WaitAsync(cancellationToken).ConfigureAwait(false);
            try
            {
                psComponentHealthMsg.SystemLoad = stats.SystemLoad.ProcessorQueueLength;
                psComponentHealthMsg.SystemLoadStatus = stats.SystemLoad.Status.ToString();

                psComponentHealthMsg.ProcessorUsagePercentage = stats.CpuLoad.Percentage != -1 ?
                                                    (double)stats.CpuLoad.Percentage : unknownValDouble;
                psComponentHealthMsg.ProcessorUsageStatus = stats.CpuLoad.Status.ToString();

                // Setting Total to -1 for scrubbing, if Used is -1
                psComponentHealthMsg.TotalMemoryInBytes = stats.MemoryUsage.Used != -1 ?
                                                    stats.MemoryUsage.Total : unknownValLong;
                // Setting Used to -1 for scrubbing, if Total is -1
                psComponentHealthMsg.UsedMemoryInBytes = stats.MemoryUsage.Total != -1 ?
                                                    stats.MemoryUsage.Used : unknownValLong;
                // Setting available memory to -1, if any one of the value in the MemoryUsage set is -1
                psComponentHealthMsg.AvailableMemoryInBytes = (stats.MemoryUsage.Total != -1 && stats.MemoryUsage.Used != -1) ?
                                                    stats.MemoryUsage.Total - stats.MemoryUsage.Used : unknownValLong;
                // Setting memory percentage to -1, if any one of the value in the MemoryUsage set is -1 or total memory is 0.
                psComponentHealthMsg.MemoryUsagePercentage = (stats.MemoryUsage.Total != -1 && stats.MemoryUsage.Used != -1) ?
                                                    GetPercentage(stats.MemoryUsage.Used, stats.MemoryUsage.Total) : unknownValDouble;
                psComponentHealthMsg.MemoryUsageStatus = stats.MemoryUsage.Status.ToString();

                // if Used is -1 in the InstallVolumeSpace set, setting Total to -1 for scrubbing
                psComponentHealthMsg.TotalSpaceInBytes = (stats.InstallVolumeSpace.Used != -1) ?
                                                    stats.InstallVolumeSpace.Total : unknownValLong;
                // if Total is -1 in the InstallVolumeSpace set, setting Used to -1 for scrubbing
                psComponentHealthMsg.UsedSpaceInBytes = (stats.InstallVolumeSpace.Total != -1) ?
                                                    stats.InstallVolumeSpace.Used : -1;
                // Setting available space to -1, if any one of the value in the InstallVolumeSpace set is -1
                psComponentHealthMsg.AvailableSpaceInBytes = (stats.InstallVolumeSpace.Total != -1 && stats.InstallVolumeSpace.Used != -1) ?
                                                    stats.InstallVolumeSpace.Total - stats.InstallVolumeSpace.Used : unknownValLong;
                // Setting FreeSpace percentage to -1, if any one of the value in the InstallVolumeSpace set is -1
                psComponentHealthMsg.FreeSpacePercentage = (stats.InstallVolumeSpace.Total != -1 && stats.InstallVolumeSpace.Used != -1) ?
                                                    GetPercentage(stats.InstallVolumeSpace.Total - stats.InstallVolumeSpace.Used, stats.InstallVolumeSpace.Total) : unknownValLong;
                psComponentHealthMsg.DiskUsageStatus = stats.InstallVolumeSpace.Status.ToString();

                psComponentHealthMsg.ThroughputUploadPendingDataInBytes = stats.Throughput.UploadPendingData;
                // Setting Throughput to -1, if uploadpendingdata=-1 for scrubbing
                psComponentHealthMsg.ThroughputInBytes = stats.Throughput.UploadPendingData != -1 ?
                                                    stats.Throughput.ThroughputBytesPerSec : unknownValLong;
                psComponentHealthMsg.ThroughputStatus = stats.Throughput.Status.ToString();

                // Setting delay to 0, if the Elapsed time for psHealthSW is already more than posting interval
                psComponentHealthMsg.DelayInPostingStats = psHealthSW.IsRunning ?
                    Settings.Default.RCM_WaitIntervalPSHealthMsg - psHealthSW.Elapsed : Settings.Default.RCM_WaitIntervalPSHealthMsg;
                psComponentHealthMsg.DelayInPostingStats = psComponentHealthMsg.DelayInPostingStats > TimeSpan.FromSeconds(0) ?
                       psComponentHealthMsg.DelayInPostingStats : TimeSpan.FromSeconds(0);
                FlushMessages(cancellationToken);
            }
            finally
            {
                semaphoreSlim.Release();
            }
        }

        public Task ReportPSEventsAsync(ApplianceHealthEventMsg psEvents)
        {
            return ReportPSEventsAsync(psEvents, CancellationToken.None);
        }

        public Task ReportPSEventsAsync(ApplianceHealthEventMsg psEvents, CancellationToken cancellationToken)
        {
            RefreshProcessServerSettings();

            if (!psSettings.IsPrivateEndpointEnabled)
            {
                EventData payload = ConstructEventHubMessageForProcessServer(psEvents);
                return PostEventsSync(payload, cancellationToken);
            }
            else
            {
                var payload = ConstructSendMonitoringMsgInputForPS(psEvents, cancellationToken);
                return SendMonitoringMessageAsync(
                    input: payload,
                    cancellationToken: cancellationToken);
            }
        }

        private void RefreshProcessServerSettings()
        {
            TaskUtils.RunAndIgnoreException(() =>
            {
                if (psSettings == null)
                {
                    psSettings = ProcessServerSettings.GetCachedSettings();
                    if (psSettings == null)
                    {
                        throw new Exception("Unable to get the ps settings");
                    }
                }
            }, Tracers.MsgService);
        }

        public Task UpdateJobIdAsync(JobIdUpdateItem jobIdUpdateItem)
        {
            throw new NotImplementedException(nameof(UpdateJobIdAsync));
        }

        public Task UpdateJobIdAsync(JobIdUpdateItem jobIdUpdateItem, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(UpdateJobIdAsync));
        }

        public Task<string> GetPsConfigurationAsync(string pairid)
        {
            throw new NotImplementedException(nameof(GetPsConfigurationAsync));
        }

        public Task<string> GetPsConfigurationAsync(string pairid, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(GetPsConfigurationAsync));
        }

        public Task<string> GetRollbackStatusAsync(string sourceid)
        {
            throw new NotImplementedException(nameof(GetRollbackStatusAsync));
        }

        public Task<string> GetRollbackStatusAsync(string sourceid, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(GetRollbackStatusAsync));
        }

        public Task SetResyncFlagAsync(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile)
        {
            throw new NotImplementedException(nameof(SetResyncFlagAsync));
        }

        public Task SetResyncFlagAsync(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(SetResyncFlagAsync));
        }

        /// <summary>
        /// Flushes the PS health to event hub periodically.
        /// Post statistics will be calling flush every 1 min,
        /// Post infraHealth willl be calling every 5 min.
        /// So in a posting window of 5 min, PSComponenthealth will be populated with health
        /// and max 1 min old stats. This handles the scenario if either one of health and stats
        /// is to be sent. As they will be posted irrrespective of other.
        /// </summary>
        private void FlushMessages(CancellationToken cancellationToken)
        {
            //Todo:[Himanshu] Go back to PR and recheck if Reset on restart will work properly or not.
            if (isFirstTime)
            {
                psHealthSW.Start();
                isFirstTime = false;
                return;
            }

            if (psHealthSW.Elapsed > Settings.Default.RCM_WaitIntervalPSHealthMsg)
            {
                SendHealthMessagesSync(psComponentHealthMsg, cancellationToken);
                psComponentHealthMsg = new ProcessServerComponentHealthMsg();
                psHealthSW.Restart();
            }
        }

        private Task SendHealthMessagesSync(object content, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            RefreshProcessServerSettings();

            if (!psSettings.IsPrivateEndpointEnabled)
            {
                return PostHealthMessagesSync(
                    payload: ConstructEventHubMessageForProcessServer(content),
                    cancellationToken: ct);
            }
            else
            {
                return SendMonitoringMessageAsync(
                    input: ConstructSendMonitoringMsgInputForPS(content, ct),
                    cancellationToken: ct);
            }
        }

        private Task PostHealthMessagesSync(EventData payload, CancellationToken cancellationToken)
        {
            if (psMessageServiceWrapperInformation == null)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Message service wrapper for informational queue is null, trying to recreate");
                psMessageServiceWrapperInformation = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.PSInformation);
                if (psMessageServiceWrapperInformation == null)
                {
                    return Task.FromException(new Exception("EventHubWrapper is for info queue Not yet initialised."));
                }
            }

            Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "Posting the message data to information queue : {0}",
                    payload.ToString());

            //Todo: Handle the exception from post sync.
            return psMessageServiceWrapperInformation.PostMessageSync(payload, cancellationToken);
        }

        public Task ReportHostHealthAsync(bool IsPrivateEndpointEnabled, IProcessServerHostSettings host,
            ProcessServerProtectionPairHealthMsg hostHealth,
            CancellationToken cancellationToken)
        {
            if (!IsPrivateEndpointEnabled)
            {
                EventData payload = ConstructEventHubMessageForHost(hostHealth, host);
                GetInfoMessageWrappperForHostId(host, out IMessageServiceWrapper hostMessageWrapper);
                return PostHostMessagesAsync(hostMessageWrapper, payload, cancellationToken);
            }
            else
            {
                return SendMonitoringMessageAsync(
                    input: ConstructSendMonitoringMsgInputForHost(hostHealth, host, cancellationToken),
                    cancellationToken: cancellationToken);
            }
        }

        private void GetInfoMessageWrappperForHostId(IProcessServerHostSettings host, out IMessageServiceWrapper hostMessageWrapper)
        {
            if (hostIdInfoMessageWrapperMap.TryGetValue(host.HostId, out hostMessageWrapper))
            {
                return;
            }
            else
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Message service wrapper for host {0} informational queue is null, trying to recreate",
                    host.HostId);
                hostMessageWrapper = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.HostInformation, host.InformationalChannelUri);
                if (hostMessageWrapper != null)
                {
                    hostIdInfoMessageWrapperMap.TryAdd(host.HostId, hostMessageWrapper);
                }
            }
        }

        private Task PostHostMessagesAsync(IMessageServiceWrapper hostMessageWrapper, EventData payload, CancellationToken cancellationToken)
        {
            Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "Posting the message data to information queue : {0} {1}",
                    payload.ToString(),
                    hostMessageWrapper);

            return hostMessageWrapper?.PostMessageAsync(payload, cancellationToken);
        }

        private Task PostEventsSync(EventData payload, CancellationToken cancellationToken)
        {
            if (psMessageServiceWrapperCritical == null)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Message service wrapper for critical is null, trying to recreate");
                psMessageServiceWrapperCritical = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.PSCritical);
                if (psMessageServiceWrapperCritical == null)
                {
                    return Task.FromException(new Exception("EventHubWrapper for critical queue is Not yet initialised."));
                }
            }

            Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "Posting the event data to critical queue: {0}",
                    payload.ToString());

            //Todo: Handle the exception from post sync.
            return psMessageServiceWrapperCritical.PostMessageSync(payload, cancellationToken);
        }

        private EventData ConstructEventHubMessageForProcessServer(object content)
        {
            EventData payload = PopulateCommonHeaders(content);

            if (GetPSMessageContext() == null)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Message sending failed for event {0} as PS message context is not set",
                        content.ToString());
                return null;
            }

            payload.Properties.Add(
                MonitoringMessageProperty.MessageContext.ToString(),
                GetPSMessageContext());
            payload.Properties.Add(
                MonitoringMessageProperty.MessageType.ToString(),
                MessageType.ProcessServerComponentHealth.ToString());

            Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Posting PS Statistics and health issues to RCM : {0}",
                        content.ToString());

            return payload;
        }

        private EventData ConstructEventHubMessageForHost(object content, IProcessServerHostSettings host)
        {
            EventData payload = PopulateCommonHeaders(content);
            payload.Properties.Add(
                MonitoringMessageProperty.MessageType.ToString(),
                MessageType.ProcessServerProtectionPairHealth.ToString());

            payload.Properties.Add(
                MonitoringMessageProperty.MessageContext.ToString(),
                host.MessageContext);

            return payload;
        }

        private static EventData PopulateCommonHeaders(object content)
        {
            string clientReqId = Guid.NewGuid().ToString();
            string activityId = Guid.NewGuid().ToString();
            EventData payload = new EventData(Encoding.UTF8.GetBytes(JsonConvert.SerializeObject(content)));

            payload.Properties.Add(
                 MonitoringMessageProperty.MessageSource.ToString(),
                 nameof(MonitoringMessageSource.ProcessServer));
            payload.Properties.Add(
                MonitoringMessageProperty.ClientRequestId.ToString(),
               clientReqId);
            payload.Properties.Add(
                MonitoringMessageProperty.ActivityId.ToString(),
                activityId);
            return payload;
        }

        /// <summary>
        /// Tries to fetch and populate psMessageContext if empty.
        /// </summary>
        /// <returns></returns>
        private string GetPSMessageContext()
        {
            if (psMessageContext == null)
            {
                var psSettings = CSApi.ProcessServerSettings.GetCachedSettings();
                psMessageContext = psSettings?.MessageContext;
            }

            return psMessageContext;
        }

        private void ProcessServerSettings_HostMonitoringInfoUriChanged(object sender, CSApi.ProcessServerSettings.HostChannelUriChangedEventArgs e)
        {
            try
            {
                // only care about the uri change notification for hosts which are posting healths currently, ignore rest.
                hostIdInfoMessageWrapperMap.TryGetValue(e.HostId, out IMessageServiceWrapper hostMessageWrapper);
                if (hostMessageWrapper != null)
                {
                    hostMessageWrapper = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.HostInformation, e.NewChannelUri);
                    if (hostMessageWrapper != null)
                    {
                        hostIdInfoMessageWrapperMap.AddOrUpdate(e.HostId, hostMessageWrapper, (key, oldValue) => hostMessageWrapper);
                    }
                }
            }
            catch (Exception ex)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception while refreshing info the info sas uri for host: {0}{1}{2}",
                        e.HostId,
                        Environment.NewLine,
                        ex);
            }
        }

        private void ProcessServerSettings_HostMonitoringCriticalUriChanged(object sender, CSApi.ProcessServerSettings.HostChannelUriChangedEventArgs e)
        {
            try
            {
                // only care about the uri change notification for hosts which are posting healths currently, ignore rest.
                hostIdCriticalMessageWrapperMap.TryGetValue(e.HostId, out IMessageServiceWrapper hostMessageWrapper);
                if (hostMessageWrapper != null)
                {
                    hostMessageWrapper = MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.HostCritical, e.NewChannelUri);
                    if (hostMessageWrapper != null)
                    {
                        hostIdCriticalMessageWrapperMap.AddOrUpdate(e.HostId, hostMessageWrapper, (key, oldValue) => hostMessageWrapper);
                    }
                }
            }
            catch (Exception ex)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception while refreshing critical sas uri for host: {0}{1}{2}",
                        e.HostId,
                        Environment.NewLine,
                        ex);
            }
        }

        private void ProcessServerSettings_PSMonitoringInfoUriChanged(object sender, CSApi.ProcessServerSettings.PSChannelUriChangedEventArgs e)
        {
            try
            {
                psMessageServiceWrapperInformation =
                    MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.PSInformation, e.NewChannelUri);
            }
            catch (Exception ex)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception while refreshing the PS info sas uri: {0}{1}",
                        Environment.NewLine,
                        ex);
            }
        }

        private void ProcessServerSettings_PSMonitoringCriticalUriChanged(object sender, CSApi.ProcessServerSettings.PSChannelUriChangedEventArgs e)
        {
            try
            {
                psMessageServiceWrapperCritical =
                    MessageServiceFactory.GetMessageService().GetMessageWrapper(EventChannelType.PSCritical, e.NewChannelUri);
            }
            catch (Exception ex)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Exception while refreshing the PS critical sas uri: {0}{1}",
                        Environment.NewLine,
                        ex);
            }
        }

        private SendMonitoringMessageInput PopulateCommonSendMonitoringMsgInput(object content)
        {
            SendMonitoringMessageInput sendMonitoringMessageInput = null;

            if (GetPSMessageContext() == null)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Message sending failed for event {0} as PS Message Context is not set",
                        content.ToString());
                return sendMonitoringMessageInput;
            }

            TaskUtils.RunAndIgnoreException(() =>
            {
                string payload = JsonConvert.SerializeObject(content);

                sendMonitoringMessageInput = new SendMonitoringMessageInput
                {
                    MessageContent = payload,
                    MessageContext = GetPSMessageContext(),
                    MessageSource = nameof(MonitoringMessageSource.ProcessServer)
                };

                Tracers.MsgService.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Posting Monitoring Messages {0} to RCM API",
                            sendMonitoringMessageInput.ToString());
            }, Tracers.MsgService);

            return sendMonitoringMessageInput;
        }

        private SendMonitoringMessageInput ConstructSendMonitoringMsgInputForPS(object content, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            SendMonitoringMessageInput sendMonitoringMessageInput = PopulateCommonSendMonitoringMsgInput(content);

            if (sendMonitoringMessageInput == null)
            {
                return sendMonitoringMessageInput;
            }

            sendMonitoringMessageInput.MessageType = MessageType.ProcessServerComponentHealth.ToString();

            Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Posting the PS message data to RCM API : {0}",
                    content.ToString());

            return sendMonitoringMessageInput;
        }

        private SendMonitoringMessageInput ConstructSendMonitoringMsgInputForHost(
            object content, IProcessServerHostSettings host, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            SendMonitoringMessageInput sendMonitoringMessageInput = PopulateCommonSendMonitoringMsgInput(content);

            if (sendMonitoringMessageInput == null)
            {
                return sendMonitoringMessageInput;
            }

            sendMonitoringMessageInput.MessageType = MessageType.ProcessServerProtectionPairHealth.ToString();
            sendMonitoringMessageInput.MessageContext = host.MessageContext;

            Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Posting the host message data to RCM API : {0}",
                    content.ToString());

            return sendMonitoringMessageInput;
        }

        public async Task SendMonitoringMessageAsync(SendMonitoringMessageInput input,
            CancellationToken cancellationToken)
        {
            using (var stubs = RcmApiFactory.GetProcessServerRcmApiStubs(PSConfiguration.Default))
            {
                RcmOperationContext opContext = new RcmOperationContext();
                await stubs.SendMonitoringMessageAsync(opContext, input, cancellationToken).ConfigureAwait(false);
            }
        }

        public Task RegisterPSAsync(HostInfo hostInfo, Dictionary<string, NicInfo> networkInfo)
        {
            throw new NotImplementedException(nameof(RegisterPSAsync));
        }

        public Task RegisterPSAsync(HostInfo hostInfo, Dictionary<string, NicInfo> networkInfo, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(RegisterPSAsync));
        }

        public Task StopReplicationAtPSAsync(IStopReplicationInput stopReplicationInput)
        {
            throw new NotImplementedException(nameof(StopReplicationAtPSAsync));
        }

        public Task StopReplicationAtPSAsync(IStopReplicationInput stopReplicationInput, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(StopReplicationAtPSAsync));
        }

        public Task ReportPSEventsAsync(IPSHealthEvent psHealthEvent)
        {
            throw new NotImplementedException(nameof(ReportPSEventsAsync));
        }

        public Task ReportPSEventsAsync(IPSHealthEvent psHealthEvent, CancellationToken cancellationToken)
        {
            throw new NotImplementedException(nameof(ReportPSEventsAsync));
        }

        public Task UpdateReplicationStatusForDeleteReplicationAsync(string pairId)
        {
            throw new NotImplementedException(nameof(UpdateReplicationStatusForDeleteReplicationAsync));
        }

        public Task UpdateReplicationStatusForDeleteReplicationAsync(string pairId, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateReplicationStatusForDeleteReplicationAsync));
        }

        public Task UpdateCertExpiryInfoAsync(Dictionary<string, long> certExpiryInfo)
        {
            throw new NotImplementedException(nameof(UpdateCertExpiryInfoAsync));
        }

        public Task UpdateCertExpiryInfoAsync(Dictionary<string, long> certExpiryInfo, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateCertExpiryInfoAsync));
        }

        public Task ReportDataUploadBlockedAsync(DataUploadBlockedUpdateItem dataUploadBlockedUpdateItem)
        {
            throw new NotImplementedException(nameof(ReportDataUploadBlockedAsync));
        }

        public Task ReportDataUploadBlockedAsync(DataUploadBlockedUpdateItem dataUploadBlockedUpdateItem, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(ReportDataUploadBlockedAsync));
        }

        public Task UpdateCumulativeThrottleAsync(bool isThrottled, SystemHealth systemHealth)
        {
            throw new NotImplementedException(nameof(UpdateCumulativeThrottleAsync));
        }

        public Task UpdateCumulativeThrottleAsync(bool isThrottled, SystemHealth systemHealth, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateCumulativeThrottleAsync));
        }

        public Task UpdateIRDRStuckHealthAsync(List<IRDRStuckHealthEvent> iRDRStuckHealthEventList)
        {
            throw new NotImplementedException(nameof(UpdateIRDRStuckHealthAsync));
        }

        public Task UpdateIRDRStuckHealthAsync(List<IRDRStuckHealthEvent> iRDRStuckHealthEventList, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateIRDRStuckHealthAsync));
        }

        public Task UpdateRPOAsync(Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo)
        {
            throw new NotImplementedException(nameof(UpdateRPOAsync));
        }

        public Task UpdateRPOAsync(Dictionary<IProcessServerPairSettings, TimeSpan> RPOInfo, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateRPOAsync));
        }

        public Task UpdateExecutionStateAsync(string pairId, ExecutionState executionState)
        {
            throw new NotImplementedException(nameof(UpdateExecutionStateAsync));
        }

        public Task UpdateExecutionStateAsync(string pairId, ExecutionState executionState, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateExecutionStateAsync));
        }

        public Task PauseReplicationAsync(string pairId)
        {
            throw new NotImplementedException(nameof(PauseReplicationAsync));
        }

        public Task PauseReplicationAsync(string pairId, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(PauseReplicationAsync));
        }

        public Task UpdatePolicyInfoForRenewCertsAsync(RenewCertsPolicyInfo renewCertsPolicyInfo)
        {
            throw new NotImplementedException(nameof(UpdatePolicyInfoForRenewCertsAsync));
        }

        public Task UpdatePolicyInfoForRenewCertsAsync(RenewCertsPolicyInfo renewCertsPolicyInfo, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdatePolicyInfoForRenewCertsAsync));
        }

        public Task UpdateLogRotationInfoAsync(string logName, long logRotTriggerTime)
        {
            throw new NotImplementedException(nameof(UpdateLogRotationInfoAsync));
        }

        public Task UpdateLogRotationInfoAsync(string logName, long logRotTriggerTime, CancellationToken ct)
        {
            throw new NotImplementedException(nameof(UpdateLogRotationInfoAsync));
        }

        public Task UpdateResyncThrottleAsync(Dictionary<int, Tuple<ThrottleState, long >> resyncThrottleInfo)
        {
            throw new NotImplementedException(nameof(UpdateResyncThrottleAsync));
        }

        public Task UpdateResyncThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> resyncThrottleInfo, CancellationToken token)
        {
            throw new NotImplementedException(nameof(UpdateResyncThrottleAsync));
        }

        public Task UpdateDiffThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> diffThrottleInfo)
        {
            throw new NotImplementedException(nameof(UpdateDiffThrottleAsync));
        }

        public Task UpdateDiffThrottleAsync(Dictionary<int, Tuple<ThrottleState, long>> diffThrottleInfo, CancellationToken token)
        {
            throw new NotImplementedException(nameof(UpdateDiffThrottleAsync));
        }

        public Task UploadFilesAsync(List<Tuple<string, string>> uploadFileInputItems)
        {
            throw new NotImplementedException(nameof(UploadFilesAsync));
        }

        public Task UploadFilesAsync(List<Tuple<string, string>> uploadFileInputItems, CancellationToken token)
        {
            throw new NotImplementedException(nameof(UploadFilesAsync));
        }
    }
}
