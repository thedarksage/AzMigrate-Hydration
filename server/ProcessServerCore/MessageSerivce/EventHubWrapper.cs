using Newtonsoft.Json;
using System;
using System.Diagnostics;
using System.Text;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.ServiceBus.Messaging;
using static RcmContract.MonitoringMsgEnum;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService
{
    /// <summary>
    /// Posts events to event hub.
    /// </summary>
    class EventHubWrapper : IMessageServiceWrapper
    {
        protected EventHubSender eventHubSender;

        public EventHubWrapper(MiscUtils.EventChannelType eventType)
        {
            var psSettings = CSApi.ProcessServerSettings.GetCachedSettings();
            if (psSettings == null)
            {
                throw new NullReferenceException("PS Settigs not initialized.");
            }

            if (eventType.Equals(MiscUtils.EventChannelType.PSInformation))
            {
                if (psSettings != null && psSettings.InformationalChannelUri != null)
                {
                    eventHubSender = EventHubClientFactory.GetEventHubClient(psSettings.InformationalChannelUri.ToString());
                }
            }
            else if (eventType.Equals(MiscUtils.EventChannelType.PSCritical))
            {
                if (psSettings != null && psSettings.CriticalChannelUri != null)
                {
                    eventHubSender = EventHubClientFactory.GetEventHubClient(psSettings.CriticalChannelUri.ToString());
                }
            }
            else
            {
                throw new NotSupportedException("Event type not supported." + eventType);
            }
        }

        public EventHubWrapper(MiscUtils.EventChannelType eventType, string channelSasUri)
        {
            if (channelSasUri == null)
            {
                throw new NullReferenceException("Channel Uri can't be null.");
            }

            eventHubSender = EventHubClientFactory.GetEventHubClient(channelSasUri);
        }

        public async Task PostMessageSync(EventData payload, CancellationToken cancellationToken)
        {
            int retryCount = Settings.Default.EventHubWrapper_SyncMsgRetryCount;
            while (payload != null && retryCount-- >= 0)
            {
                try
                {
                    if (eventHubSender != null)
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                        await eventHubSender.SendAsync(payload).ConfigureAwait(false);

                        Tracers.MsgService.TraceAdminLogV2Message(TraceEventType.Verbose,
                            "Successfully posted data to event hub");

                        return;
                    }
                    else
                    {
                        Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "Can't post events, as event hub client has been reset as SAS uri is null");
                    }
                }
                catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
                {
                    Tracers.MsgService.TraceAdminLogV2Message(
                                TraceEventType.Error, "Task cancellation exception is thrown in Eventhubwrapper::PostMessageSync.");
                    throw;
                }
                catch (Exception ex)
                {
                    Tracers.MsgService.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Sync Message sending failed with exception {0}{1}",
                        Environment.NewLine,
                        ex);
                }

                await Task.Delay(
                        Settings.Default.EventHubWrapper_SyncMsgRetryInterval, cancellationToken).ConfigureAwait(false);
            }
        }

        public async Task PostMessageAsync(EventData payload, CancellationToken cancellationToken)
        {
            try
            {
                if (eventHubSender != null)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    await eventHubSender.SendAsync(payload).ConfigureAwait(false);
                }
                else
                {
                    Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "Can't post events, as event hub client has been reset as SAS uri is null");
                }
            }
            catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                            TraceEventType.Error, "Task cancellation exception is thrown in Eventhubwrapper::PostMessageAsync.");
                throw;
            }
            catch (Exception ex)
            {
                Tracers.MsgService.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Async Message sending failed with exception {0}{1}",
                    Environment.NewLine,
                    ex);
            }
        }
    }
}
