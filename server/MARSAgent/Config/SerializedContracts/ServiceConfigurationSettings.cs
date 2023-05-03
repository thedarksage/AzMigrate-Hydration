using RcmAgentCommonLib.Config.SerializedContracts;
using RcmContract;
using MarsAgent.Service;

namespace MarsAgent.Config.SerializedContracts
{
    /// <summary>
    /// Defines class which represents configuration details for the on-premise agent service.
    /// </summary>
    public class ServiceConfigurationSettings : ServiceConfigurationSettingsBase
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ServiceConfigurationSettings"/> class.
        /// </summary>
        public ServiceConfigurationSettings()
            : base(
                  ServiceProperties.Instance.DefaultMachineIdentifier,
                  ServiceProperties.Instance.DefaultInstrumentationKey,
                  ServiceProperties.Instance.LocalPreferencesFilePath,
                  ServiceProperties.Instance.AgentSettingsFilePath,
                  ServiceProperties.Instance.DefaultLogFolder,
                  ServiceProperties.Instance.DefaultReplicationStateRootFolder,
                  new WebProxySettings(),
                  new SpnIdentity(),
                  isConfigured: false,
                  isRegistered: false)
        {
            this.MarsSpnIdentity = new SpnIdentity();
        }

        /// <summary>
        /// Gets or sets the service principal details used by on-premise agent (MARS)
        /// for communication wit protection service.
        /// </summary>
        public SpnIdentity MarsSpnIdentity { get; set; }

        /// <summary>
        /// Overrides the base ToString for debugging/logging purposes.
        /// </summary>
        /// <returns>The display string.</returns>
        public override string ToString()
        {
            return this.RcmSafeToString(
                (objectCopy) =>
                {
                    objectCopy.AppInsightsInstrumentationKey =
                        RcmDataContractUtils.SecretRemovedFromLogging;
                    return objectCopy;
                });
        }
    }
}
