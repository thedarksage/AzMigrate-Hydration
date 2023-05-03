using System;
using System.Collections.Generic;
using ClientLibHelpers;
using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Service;
using RcmClientLib;
using RcmContract;
using MarsAgent.Config;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.LoggerInterface;

namespace MarsAgent.Service
{
    /// <summary>
    /// Defines settings for the service.
    /// </summary>
    public sealed class ServiceSettings
        : ServiceSettingsBase<MarsSettings>
    {
        /// <summary>
        /// Singleton instance of <see cref="ServiceSettings"/> used by all callers.
        /// </summary>
        public static readonly ServiceSettings Instance = new ServiceSettings();

        /// <summary>
        /// Prevents a default instance of the <see cref="ServiceSettings"/> class from
        /// being created.
        /// </summary>
        private ServiceSettings()
            : base(ServiceProperties.Instance, Logger.Instance)
        {
        }

        /// <summary>
        /// Method to instantiate agent settings object.
        /// </summary>
        /// <returns>Agent settings object.</returns>
        protected sealed override MarsSettings InstantiateAgentSettings()
        {
            return new MarsSettings();
        }

        /// <summary>
        /// Gets the telemetry EventHub SAS URI.
        /// </summary>
        /// <param name="agentSettings">Agent settings.</param>
        /// <returns>The EventHub SAS URI to upload telemetry data.</returns>
        protected sealed override string GetTelemetryEventHubSasUriInternal(
            MarsSettings agentSettings)
        {
            return agentSettings?.ComponentTelemetrySettings?.KustoEventHubUri;
        }

        /// <summary>
        /// Gets the SAS URI renewal time for source machine.
        /// </summary>
        /// <param name="agentSettings">Agent settings.</param>
        /// <param name="machineId">Machine ID.</param>
        /// <returns>SAS URI renewal time.</returns>
        protected sealed override DateTime GetVmMonitoringSasUriRenewalTimeForSourceMachineInternal(
            MarsSettings agentSettings,
            string machineId)
        {
            string message = $"Possible code bug hit. MsgContext for source machine not " +
                $"applicable for '{this.SvcProperties.ComponentType}'.";

            var callerInfo = CallInfo.Site();
            Logger.Instance.LogError(callerInfo, message);

            AgentMetrics.LogUnexpectedCodePathHit(
                callerInfo.MethodName,
                new AgentMetrics.UnexpectedCodePathHitAnnotationType(
                    callerInfo.FilePath,
                    callerInfo.LineNumber.ToString(),
                    message));

            throw new NotImplementedException(message);
        }

        /// <summary>
        /// Gets the list of protected source machine IDs.
        /// </summary>
        /// <param name="agentSettings">Agent settings.</param>
        /// <returns>List of protected source machine IDs.</returns>
        protected sealed override List<string> EnumerateProtectedSourceMachineIDsInternal(
            MarsSettings agentSettings)
        {
            string message = $"Possible code bug hit. MsgContext for source machine not " +
                $"applicable for '{this.SvcProperties.ComponentType}'.";

            var callerInfo = CallInfo.Site();
            Logger.Instance.LogError(callerInfo, message);

            AgentMetrics.LogUnexpectedCodePathHit(
                callerInfo.MethodName,
                new AgentMetrics.UnexpectedCodePathHitAnnotationType(
                    callerInfo.FilePath,
                    callerInfo.LineNumber.ToString(),
                    message));

            throw new NotImplementedException(message);
        }

        /// <summary>
        /// Gets the list of protected source machine IDs.
        /// </summary>
        /// <param name="agentSettings">Agent settings.</param>
        /// <returns>List of protected source machine IDs.</returns>
        protected sealed override List<string> EnumerateMachineIDsForWhichReplicationSettingsArePresentInternal(
            MarsSettings agentSettings)
        {
            string message = $"Possible code bug hit. MsgContext for source machine not " +
                $"applicable for '{this.SvcProperties.ComponentType}'.";

            var callerInfo = CallInfo.Site();
            Logger.Instance.LogError(callerInfo, message);

            AgentMetrics.LogUnexpectedCodePathHit(
                callerInfo.MethodName,
                new AgentMetrics.UnexpectedCodePathHitAnnotationType(
                    callerInfo.FilePath,
                    callerInfo.LineNumber.ToString(),
                    message));

            throw new NotImplementedException(message);
        }

        /// <summary>
        /// Preserve replication settings for the machines for which latest replication
        /// settings could not be fetched (due to limit on payload size/execution time),
        /// provided that these machines are still protected and associated with the given
        /// RCM replication agent.
        /// </summary>
        /// <param name="existingAgentSettings">Existing agent settings.</param>
        /// <param name="receivedSettings">Settings received from RCM service.</param>
        /// <returns>Agent settings.</returns>
        protected sealed override MarsSettings MergeExistingSettings(
            MarsSettings existingAgentSettings,
            MarsSettings receivedSettings)
        {
            // For Mars service sending the received settings as is.
            MarsSettings settingsCopy =
                RcmDataContractUtils<MarsSettings>.RcmDeepClone(
                    receivedSettings);

            return settingsCopy;
        }

        /// <inheritdoc/>
        public sealed override MarsSettings FetchPartialSettingsFromRcm()
        {
            MarsSettings settings;

            var prevContext = Logger.Instance.GetLogContext();

            try
            {
                string clientRequestId = prevContext.ClientRequestId;
                clientRequestId += "/" + Guid.NewGuid().ToString();

                Logger.Instance.MergeLogContext(
                    new LogContext
                    {
                        ClientRequestId = clientRequestId
                    });

                ServiceConfigurationSettings svcConfig = Configurator.Instance.GetConfig();
                RcmCreds rcmCreds = Configurator.Instance.GetRcmCreds();

                this.LoggerInstance.LogInfo(
                    CallInfo.Site(),
                    $"Fetching replication settings from RCM.");

                string previousSettingsSyncTrackingId = this.GetSettingsSyncTrackingId();

                string sessionTrackingId =
                    string.IsNullOrWhiteSpace(previousSettingsSyncTrackingId) ?
                    null :
                    previousSettingsSyncTrackingId;

                DateTime getSettingsCallTime = DateTime.UtcNow;

                settings =
                    rcmCreds.GetMarsSettings(
                        new ClientOperationContext
                        {
                            ActivityId = this.LoggerInstance.GetCurrentActivityId().ToString(),
                            ClientRequestId = clientRequestId,
                            UserHeaders = new Dictionary<string, string>
                            {
                                {
                                    CustomHttpHeaders.MSAgentMachineId,
                                    svcConfig.MachineIdentifier
                                },
                                {
                                    CustomHttpHeaders.MSAgentComponentId,
                                    this.SvcProperties.ComponentName
                                }
                            }
                        });

                this.LoggerInstance.LogInfo(
                    CallInfo.Site(),
                    "Fetched replication settings from RCM successfully");
            }
            catch (Exception ex)
            {
                Logger.Instance.LogException(
                    CallInfo.Site(),
                    ex,
                    $"RCM API invocation to fetch settings failed.");

                throw;
            }
            finally
            {
                Logger.Instance.SetLogContext(prevContext);
            }

            return settings;
        }

        protected override List<string> EnumerateDisassociatedMachinesInternal(MarsSettings agentSettings)
        {
            // Consume settings makes a call to enumerate disassociated machines for doing cleanup.
            // Returning empty list since disassociated machines list is not applicable for Mars
            // settings.
            // Note : Do not throw exception from this routine.
            return new List<string>();
        }

        protected override List<string> EnumeratePurgedMachinesInternal(MarsSettings agentSettings)
        {
            // Consume settings makes a call to enumerate purged machines for doing cleanup.
            // Returning empty list since purged machines list is not applicable for Mars
            // settings.
            // Note : Do not throw exception from this routine.
            return new List<string>();
        }

        protected override void RemoveMachineSettingsInternal(MarsSettings agentSettings, string machineId)
        {
            // No Op.
            // Consume settings makes a call to remove machine settings after performing cleanup
            // of disassociated or purged machines.
            // This will be no op since machine settings list is not applicable for Mars
            // settings.
        }
    }
}
