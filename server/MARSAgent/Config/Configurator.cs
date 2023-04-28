using System;
using System.Collections.Generic;
using ClientLibHelpers;
using LoggerInterface;
using RcmAgentCommonLib.Config;
using RcmAgentCommonLib.Config.SerializedContracts;
using RcmAgentCommonLib.Utilities;
using RcmClientLib;
using RcmCommonUtils;
using RcmContract;
using MarsAgent.Config.CmdLineInputs;
using MarsAgent.Config.SerializedContracts;
using MarsAgent.Constants;
using MarsAgent.LoggerInterface;
using MarsAgent.Service;
using MarsAgent.Utilities;
using MarsAgent.Monitoring;
using MarsAgentException = MarsAgent.ErrorHandling.MarsAgentException;
using MarsAgent.CBEngine.Constants;

namespace MarsAgent.Config
{
    /// <summary>
    /// Defines implementation class for service configuration.
    /// </summary>
    public sealed class Configurator :
        ConfiguratorBase<MarsSettings, ServiceConfigurationSettings, LogContext>
    {
        /// <summary>
        /// Singleton instance of <see cref="Configurator"/> used by all callers.
        /// </summary>
        public static readonly Configurator Instance = new Configurator();

        /// <summary>
        /// Prevents a default instance of the <see cref="Configurator"/> class from
        /// being created.
        /// </summary>
        private Configurator()
            : base(
                  ServiceProperties.Instance,
                  ServiceTunables.Instance,
                  ServiceSettings.Instance,
                  Logger.Instance)
        {
        }

        /// <summary>
        /// Configures the service.
        /// </summary>
        /// <param name="input">Input for configuring the service.</param>
        public void Configure(ConfigureInput input)
        {
            try
            {
                using (new SingletonApp(this.LoggerInstance, this.SvcProperties))
                {
                    this.LoggerInstance.MergeLogContext(new LogContext
                    {
                        ClientRequestId = input.ClientRequestId,
                        ActivityId = Guid.Parse(input.ActivityId)
                    });

                    ServiceConfigurationSettings svcConfig = this.GetConfig();

                    // Validate if service is installed and not registered earlier.
                    this.ValidateServiceReadyForConfiguration(svcConfig);

                    // Get data from appliance config file
                    ApplianceConfig applianceConfig = this.GetApplianceConfigData(
                        input.ApplianceConfigLocation);

                    this.ValidateApplianceConfig(applianceConfig);
                    this.ValidateAgentAuthenticationSpn(applianceConfig);
                    this.ValidateMarsAgentAuthenticationSpn(applianceConfig);

                    this.ConfigureSvcSettings(
                        svcConfig,
                        applianceConfig,
                        this.GetLogFolderToBeUsed(input.LogFolderRoot),
                        this.GetReplicationStateRootFolderToBeUsed(input.LogFolderRoot));

                    ServiceControllerHelper.StopService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Updating '{this.SvcProperties.ConfigFilePath}' with config settings " +
                        $"'{svcConfig.ToString()}'.");

                    this.UpdateConfig(svcConfig);

                    // Updates registry value required after configure
                    this.UpdateConfigureRegistry();

                    ServiceControllerHelper.StartService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogEvent(
                        CallInfo.Site(),
                        RcmAgentEvent.ServiceConfigured());
                }
            }
            catch (Exception e)
            {
                this.LoggerInstance.LogEvent(
                    CallInfo.Site(),
                    RcmAgentEvent.ServiceConfigurationFailed(
                        e.GetType().ToString(),
                        e.GetMessage()));

                throw;
            }
        }

        /// <summary>
        /// Register the service.
        /// </summary>
        /// <param name="input">Input for registering the service.</param>
        public void Register(RegisterInput input)
        {
            try
            {
                using (new SingletonApp(this.LoggerInstance, this.SvcProperties))
                {
                    this.LoggerInstance.MergeLogContext(new LogContext
                    {
                        ClientRequestId = input.ClientRequestId,
                        ActivityId = Guid.Parse(input.ActivityId)
                    });

                    ServiceConfigurationSettings svcConfig = this.GetConfig();
                    this.ValidateServiceReadyForPostConfiguration(svcConfig);
                    this.ValidateRcmServiceUri(svcConfig, input.RcmServiceUri);

                    svcConfig.RcmServiceUrl = input.RcmServiceUri;

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Updating '{this.SvcProperties.ConfigFilePath}' with config settings " +
                        $"'{svcConfig.ToString()}'.");

                    this.UpdateConfig(svcConfig);

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Registering '{this.SvcProperties.ServiceName}' service details with " +
                        $"RCM at '{input.RcmServiceUri}'.");

                    RcmCreds rcmCreds = this.GetRcmCreds(input.RcmServiceUri);

                    try
                    {
                        this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Registering '{this.SvcProperties.ServiceName}' service details with " +
                        $"HostId : '{svcConfig.MachineIdentifier}'" +
                        $"FriendlyName : '{this.SvcProperties.AgentFriendlyName}'" +
                        $"AgentVersion : '{this.SvcProperties.ServiceVersion}'" +
                        $"InstallationLocation : '{this.SvcProperties.InstallLocation}'" +
                        $"ServiceName : '{this.SvcProperties.ServiceName}'" +
                        $"ServiceDisplayName : '{this.SvcProperties.ServiceDisplayName}'");

                        rcmCreds.RegisterMars(
                            new RegisterMarsInput
                            {
                                Id = svcConfig.MachineIdentifier,
                                FriendlyName = this.SvcProperties.AgentFriendlyName,
                                AgentVersion = this.SvcProperties.ServiceVersion,
                                InstallationLocation = this.SvcProperties.InstallLocation,
                                ServiceName = this.SvcProperties.ServiceName,
                                ServiceDisplayName = this.SvcProperties.ServiceDisplayName,
                                SupportedFeatures = new List<string>()
                                {
                                    nameof(RcmEnum.RcmComponentSupportedFeature.PrivateLink)
                                }
                            },
                            new ClientOperationContext
                            {
                                ActivityId = input.ActivityId,
                                ClientRequestId = input.ClientRequestId,
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
                            $"'{this.SvcProperties.ServiceName}' registered with RCM at " +
                            $"'{input.RcmServiceUri}'.");
                    }
                    catch (RcmException ex)
                    when (ex.ErrorCode == nameof(RcmErrorCode.ApplianceComponentAlreadyRegistered))
                    {
                        this.LoggerInstance.LogInfo(
                            CallInfo.Site(),
                            $"'{this.SvcProperties.ServiceName}' is already registered with RCM. " +
                            $"Calling Modify API.");

                        rcmCreds.ModifyMars(
                            new ModifyMarsInput
                            {
                                Id = svcConfig.MachineIdentifier,
                                FriendlyName = this.SvcProperties.AgentFriendlyName,
                                AgentVersion = this.SvcProperties.ServiceVersion,
                                SupportedFeatures = new List<string>()
                                {
                                    nameof(RcmEnum.RcmComponentSupportedFeature.PrivateLink)
                                }
                            },
                            new ClientOperationContext
                            {
                                ActivityId = input.ActivityId,
                                ClientRequestId = input.ClientRequestId,
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
                            $"'{this.SvcProperties.ServiceName}' registration updated with RCM " +
                            $"at '{input.RcmServiceUri}'.");
                    }

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Fetching replication settings and instrumentation key from RCM.");

                    MarsSettings settings =
                        rcmCreds.GetMarsSettings(
                            new ClientOperationContext
                            {
                                ActivityId = input.ActivityId,
                                ClientRequestId = input.ClientRequestId,
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
                        "Fetched settings and instrumentation key from RCM successfully.");

                    svcConfig.AppInsightsInstrumentationKey =
                        settings.ApplicationInsightsInstrumentationKey;
                    svcConfig.IsRegistered = true;
                    svcConfig.IsPrivateEndpointEnabled = settings.IsPrivateEndpointEnabled;

                    ServiceControllerHelper.StopService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Updating '{this.SvcProperties.ConfigFilePath}' with config settings " +
                        $"'{svcConfig.ToString()}'.");

                    this.SvcSettingsWriter.UpdateAgentSettings(settings, persistOnDisk: true);
                    this.UpdateConfig(svcConfig);
                    ServiceControllerHelper.StartService(this.SvcProperties.ServiceName);

                    // Configure MARS agent.
                    this.ConfigureMarsAgent(svcConfig, input);

                    this.LoggerInstance.LogEvent(
                        CallInfo.Site(),
                        RcmAgentEvent.ServiceRegistered());
                }
            }
            catch (Exception e)
            {
                this.LoggerInstance.LogEvent(
                    CallInfo.Site(),
                    RcmAgentEvent.ServiceRegisterationFailed(
                        e.GetType().ToString(),
                        e.GetMessage()));

                throw;
            }
        }

        /// <summary>
        /// Update web proxy.
        /// </summary>
        /// <param name="input">Input for updating web proxy.</param>
        public void UpdateWebProxy(UpdateWebProxyInput input)
        {
            try
            {
                using (new SingletonApp(this.LoggerInstance, this.SvcProperties))
                {
                    this.LoggerInstance.MergeLogContext(new LogContext
                    {
                        ClientRequestId = input.ClientRequestId,
                        ActivityId = Guid.Parse(input.ActivityId)
                    });
                    ServiceConfigurationSettings svcConfig = this.GetConfig();

                    this.ValidateServiceReadyForPostConfiguration(svcConfig);

                    ApplianceConfig applianceConfig = this.GetApplianceConfigData(
                        input.ApplianceConfigLocation);

                    this.ValidateApplianceConfig(applianceConfig);

                    ApplianceProxyConfig proxyConfig = applianceConfig.Proxy;
                    if (proxyConfig != null && !string.IsNullOrEmpty(proxyConfig.IPAddress))
                    {
                        List<string> proxyBypassList = new List<string>();
                        if (!string.IsNullOrWhiteSpace(proxyConfig.BypassList))
                        {
                            proxyBypassList.AddRange(proxyConfig.BypassList.Split(';'));
                        }

                        svcConfig.WebProxySettings = new WebProxySettings
                        {
                            IPAddress = proxyConfig.IPAddress,
                            PortNumber = ushort.Parse(proxyConfig.PortNumber),
                            UserName = proxyConfig.UserName,
                            EncryptedPassword = proxyConfig.Password,
                            BypassProxyOnLocal = Convert.ToBoolean(proxyConfig.BypassProxyOnLocal),
                            AddressesToBypass = proxyBypassList
                        };
                    }

                    ServiceControllerHelper.StopService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Updating '{this.SvcProperties.ConfigFilePath}' with config settings " +
                        $"'{svcConfig.ToString()}'.");

                    this.UpdateConfig(svcConfig);
                    ServiceControllerHelper.StartService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogEvent(
                        CallInfo.Site(),
                        RcmAgentEvent.WebProxyUpdated());
                }
            }
            catch (Exception e)
            {
                this.LoggerInstance.LogEvent(
                    CallInfo.Site(),
                    RcmAgentEvent.WebProxyUpdationFailed(
                        e.GetType().ToString(),
                        e.GetMessage()));

                throw;
            }
        }

        /// <summary>
        /// Unregister the service.
        /// </summary>
        /// <param name="input">Input for unregistering the service.</param>
        public void Unregister(UnregisterInput input)
        {
            try
            {
                using (new SingletonApp(this.LoggerInstance, this.SvcProperties))
                {
                    this.LoggerInstance.MergeLogContext(new LogContext
                    {
                        ClientRequestId = input.ClientRequestId,
                        ActivityId = Guid.Parse(input.ActivityId)
                    });

                    ServiceConfigurationSettings svcConfig = this.GetConfig();

                    if (!ServiceControllerHelper.IsServiceInstalled(this.SvcProperties.ServiceName))
                    {
                        this.LoggerInstance.LogWarning(
                            CallInfo.Site(),
                            $"Service '{this.SvcProperties.ServiceName}' is not installed.");

                        return;
                    }
                    else if (!svcConfig.IsConfigured)
                    {
                        this.LoggerInstance.LogWarning(
                            CallInfo.Site(),
                            $"Service '{this.SvcProperties.ServiceName}' is not configured.");

                        return;
                    }
                    else if (!svcConfig.IsRegistered &&
                        string.IsNullOrWhiteSpace(svcConfig.RcmServiceUrl))
                    {
                        this.LoggerInstance.LogWarning(
                            CallInfo.Site(),
                            $"Service '{this.SvcProperties.ServiceName}' is not registered.");

                        return;
                    }
                    else
                    {
                        this.LoggerInstance.LogInfo(
                            CallInfo.Site(),
                            $"Unregistering '{this.SvcProperties.ServiceName}' service details with " +
                            $"RCM at '{svcConfig.RcmServiceUrl}'.");

                        RcmCreds rcmCreds = this.GetRcmCreds(svcConfig.RcmServiceUrl);
                        rcmCreds.UnregisterMars(
                            svcConfig.MachineIdentifier,
                            new ClientOperationContext
                            {
                                ActivityId = input.ActivityId,
                                ClientRequestId = input.ClientRequestId,
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
                            $"'{this.SvcProperties.ServiceName}' unregistered with RCM at " +
                            $"'{svcConfig.RcmServiceUrl}'.");
                    }

                    svcConfig.RcmServiceUrl = string.Empty;
                    svcConfig.IsRegistered = false;

                    ServiceControllerHelper.StopService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Updating '{this.SvcProperties.ConfigFilePath}' with config settings " +
                        $"'{svcConfig.ToString()}'.");

                    this.SvcSettingsWriter.UpdateAgentSettings(
                        new MarsSettings(),
                        persistOnDisk: true);
                    this.UpdateConfig(svcConfig);

                    ServiceControllerHelper.StartService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogEvent(
                        CallInfo.Site(),
                        RcmAgentEvent.ServiceUnregistered());
                }
            }
            catch (Exception e)
            {
                this.LoggerInstance.LogEvent(
                    CallInfo.Site(),
                    RcmAgentEvent.ServiceUnregisterationFailed(
                        e.GetType().ToString(),
                        e.GetMessage()));

                throw;
            }
        }

        /// <summary>
        /// Un-configures the service.
        /// </summary>
        /// <param name="input">Input for un-configuring the service.</param>
        public void Unconfigure(UnconfigureInput input)
        {
            try
            {
                using (new SingletonApp(this.LoggerInstance, this.SvcProperties))
                {
                    this.LoggerInstance.MergeLogContext(new LogContext
                    {
                        ClientRequestId = input.ClientRequestId,
                        ActivityId = Guid.Parse(input.ActivityId)
                    });

                    ServiceConfigurationSettings svcConfig = this.GetConfig();
                    this.ValidateServiceReadyForUnconfiguration(svcConfig);

                    svcConfig = new ServiceConfigurationSettings();

                    ServiceControllerHelper.StopService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogInfo(
                        CallInfo.Site(),
                        $"Updating '{this.SvcProperties.ConfigFilePath}' with config settings " +
                        $"'{svcConfig.ToString()}'.");

                    this.UpdateConfig(svcConfig);
                    ServiceControllerHelper.StartService(this.SvcProperties.ServiceName);

                    this.LoggerInstance.LogEvent(
                        CallInfo.Site(),
                        RcmAgentEvent.ServiceUnconfigured());
                }
            }
            catch (Exception e)
            {
                this.LoggerInstance.LogEvent(
                    CallInfo.Site(),
                    RcmAgentEvent.ServiceUnconfigurationFailed(
                        e.GetType().ToString(),
                        e.GetMessage()));

                throw;
            }
        }

        /// <summary>
        /// Gets the modify agent input.
        /// </summary>
        /// <returns>Modify agent input.</returns>
        protected override ModifyAgentInputBase GetModifyAgentInput()
        {
            return new ModifyMarsInput
            {
                Id = this.GetConfig().MachineIdentifier,
                FriendlyName = this.SvcProperties.AgentFriendlyName,
                AgentVersion = this.SvcProperties.ServiceVersion,
                SupportedFeatures = new List<string>()
                {
                    nameof(RcmEnum.RcmComponentSupportedFeature.PrivateLink)
                }
            };
        }

        /// <summary>
        /// Invokes the modify agent API.
        /// </summary>
        /// <param name="modifyAgentInput">Modify agent input.</param>
        /// <param name="activityId">Activity ID for the operation.</param>
        /// <param name="clientRequestId">Client request ID for the operation.</param>
        protected override void CallModifyApi(RcmContract.ModifyAgentInputBase modifyAgentInput, string activityId, string clientRequestId)
        {
            RcmCreds rcmCreds = this.GetRcmCreds();

            ModifyMarsInput modifyMarsInput =
                modifyAgentInput as ModifyMarsInput;

            rcmCreds.ModifyMars(
                modifyMarsInput,
                new ClientOperationContext
                {
                    ActivityId = activityId,
                    ClientRequestId = clientRequestId,
                    UserHeaders = new Dictionary<string, string>
                    {
                        {
                            CustomHttpHeaders.MSAgentMachineId,
                            this.GetConfig().MachineIdentifier
                        },
                        {
                            CustomHttpHeaders.MSAgentComponentId,
                            this.SvcProperties.ComponentName
                        }
                    }
                });
        }

        /// <summary>
        /// Update the configuration settings for service using appliance configuration.
        /// </summary>
        /// <param name="svcConfig">Service configuration.</param>
        /// <param name="applianceConfig">Appliance configuration settings.</param>
        /// <param name="logFolderPath">Log folder path.</param>
        protected sealed override void ConfigureSvcSettingsInternal(
            ServiceConfigurationSettings svcConfig,
            ApplianceConfig applianceConfig,
            string logFolderPath)
        {
            svcConfig.MarsSpnIdentity = new SpnIdentity
            {
                AadAuthority = applianceConfig.MarsAgentAuthenticationSpn.AadAuthority,
                Audience = applianceConfig.MarsAgentAuthenticationSpn.Audience,
                ApplicationId = applianceConfig.MarsAgentAuthenticationSpn.ApplicationId,
                CertificateThumbprint =
                    applianceConfig.MarsAgentAuthenticationSpn.CertificateThumbprint,
                TenantId = applianceConfig.MarsAgentAuthenticationSpn.TenantId,
                ObjectId = applianceConfig.MarsAgentAuthenticationSpn.ObjectId
            };
        }

        /// <summary>
        /// Configure MARS agent.
        /// </summary>
        /// <param name="svcConfig">Service configuration.</param>
        /// <param name="input">Input for registration.</param>
        private void ConfigureMarsAgent(
            ServiceConfigurationSettings svcConfig,
            RegisterInput input)
        {
            try
            {
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.SubscriptionIdRegValue,
                    svcConfig.SubscriptionId);
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.ResourceIdRegValue,
                    svcConfig.ResourceId);
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.ResourceLocationRegValue,
                    svcConfig.ResourceLocation);

                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.ContainerUniqueNameRegValue,
                    svcConfig.ContainerUniqueName);

                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.ProtectionServiceEndpointRegValue,
                    input.ProtectionServiceUri);

                // Update Mars registry with SPN details.
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.SpnTenantIdRegValueName,
                    svcConfig.MarsSpnIdentity.TenantId);
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.SpnApplicationIdRegValueName,
                    svcConfig.MarsSpnIdentity.ApplicationId);
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.SpnAudienceRegValueName,
                    svcConfig.MarsSpnIdentity.Audience);
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.SpnAadAuthorityRegValueName,
                    svcConfig.MarsSpnIdentity.AadAuthority);
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.RegistrationHive,
                    MarsRegistrySettingNames.SpnCertificateThumbprintRegValueName,
                    svcConfig.MarsSpnIdentity.CertificateThumbprint);

                // Restart MARS service.
                ServiceUtils.StopServiceWithRetries(
                    CBEngineConstants.ServiceName,
                    CBEngineConstants.ServiceDisplayName,
                    this.SvcProperties.AgentFriendlyName);
                ServiceControllerHelper.StartService(CBEngineConstants.ServiceName);

                MarsAgentApi marsAgentApi = new MarsAgentApi();
                marsAgentApi.CheckAgentHealth();
            }
            catch (MarsAgentClientException marsEx)
            {
                throw MarsAgentException.MarsAgentConfigurationValidationFailed(
                    ServiceProperties.Instance.MarsUserFriendlyServiceName,
                    marsEx.ErrorLabel,
                    marsEx.ErrorCode.ToString("X"));
            }
        }

        /// <summary>
        /// Update MARS Registry Value after Configure
        /// </summary>
        private void UpdateConfigureRegistry()
        {
            try
            {
                RegistryHelper.SetRegistryValue(
                    this.SvcProperties,
                    MarsRegistrySettingNames.SetupHive,
                    MarsRegistrySettingNames.Mode,
                    ReplicationMode.RCM);
            } 
            catch(Exception ex)
            {
                throw ex;
            }
        }

        protected override void OnReregisterPostAction(RcmContract.ModifyAgentInputBase modifyAgentInput)
        {
            // No op
        }
    }
}