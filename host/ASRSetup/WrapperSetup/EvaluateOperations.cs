using System;
using System.Collections.Generic;
using ASRSetupFramework;
using IntegrityCheck = Microsoft.DisasterRecovery.IntegrityCheck;
using Microsoft.DisasterRecovery.SetupFramework;
using DRSetupFramework = Microsoft.DisasterRecovery.SetupFramework;

namespace UnifiedSetup
{
    /// <summary>
    /// Helper class for handling operation scenarios
    /// </summary>
    public class EvaluateOperations
    {
        /// <summary>
        /// Friendly names against the scenarios for Summary file logging.
        /// </summary>
        public static readonly Dictionary<IntegrityCheck.ExecutionScenario, string> ScenarioFriendlyNames =
            new Dictionary<IntegrityCheck.ExecutionScenario, string>()
            {
                { IntegrityCheck.ExecutionScenario.PreInstallation, ASRResources.StringResources.SummaryPrereqScenario },
                { IntegrityCheck.ExecutionScenario.Installation, ASRResources.StringResources.SummaryMidInstallScenario },
                { IntegrityCheck.ExecutionScenario.Configuration, ASRResources.StringResources.SummaryConfigurationScenario },
                { IntegrityCheck.ExecutionScenario.PostInstallation, ASRResources.StringResources.SummaryPostInstallScenario }
            };


        /// <summary>
        /// Friendly names against the validations/operations for the Summary file logging.
        /// </summary>
        public static readonly Dictionary<IntegrityCheck.OperationName, string> OperationFriendlyNames =
            new Dictionary<IntegrityCheck.OperationName, string>()
            {
                { IntegrityCheck.OperationName.IsFreeSpaceAvailable, ASRResources.StringResources.SummaryFreeSpaceCheck },
                { IntegrityCheck.OperationName.IsSystemRestartPending, ASRResources.StringResources.SummarySystemRestartCheck },
                { IntegrityCheck.OperationName.IsWinGreaterThan2012R2, ASRResources.StringResources.SummaryWin2k12R2Check },
                { IntegrityCheck.OperationName.IsTimeInSync, ASRResources.StringResources.SummaryTimeSyncCheck },
                { IntegrityCheck.OperationName.IsCsServiceRunning, ASRResources.StringResources.SummaryCsServiceCheck },
                { IntegrityCheck.OperationName.IsPsServiceRunning, ASRResources.StringResources.SummaryPsServiceCheck },
                { IntegrityCheck.OperationName.IsPushInstallServiceRunning, ASRResources.StringResources.SummaryPushInstallServiceCheck },
                { IntegrityCheck.OperationName.IsCxPsServiceRunning, ASRResources.StringResources.SummaryCxPsServiceCheck },
                { IntegrityCheck.OperationName.IsMarsServiceRuning, ASRResources.StringResources.SummaryMarsServiceCheck },
                { IntegrityCheck.OperationName.IsVxAgentServiceRunning, ASRResources.StringResources.SummaryVxAgentServiceCheck },
                { IntegrityCheck.OperationName.IsScoutApplicationServiceRunning, ASRResources.StringResources.SummaryScoutApplicationServiceCheck },
                { IntegrityCheck.OperationName.IsProcessServerServiceRunning, ASRResources.StringResources.SummaryProcessServerServiceCheck },
                { IntegrityCheck.OperationName.IsProcessServerMonitorServiceRunning, ASRResources.StringResources.SummaryProcessServerMonitorServiceCheck },
                { IntegrityCheck.OperationName.IsDraServiceRunning, ASRResources.StringResources.SummaryDraServiceCheck },
                { IntegrityCheck.OperationName.CheckEndpointsConnectivity, ASRResources.StringResources.SummaryCheckEndpoints},
                { IntegrityCheck.OperationName.InstallTp, ASRResources.StringResources.SummaryTpInstallCheck },
                { IntegrityCheck.OperationName.InstallCs, ASRResources.StringResources.SummaryCsInstallCheck },
                { IntegrityCheck.OperationName.InstallPs, ASRResources.StringResources.SummaryPsInstallCheck },
                { IntegrityCheck.OperationName.InstallDra, ASRResources.StringResources.SummaryDRAInstallCheck },
                { IntegrityCheck.OperationName.InstallMt, ASRResources.StringResources.SummaryMTInstallCheck },
                { IntegrityCheck.OperationName.ConfigureMt, ASRResources.StringResources.SummaryMTConfigurationCheck },
                { IntegrityCheck.OperationName.InstallMars, ASRResources.StringResources.SummaryMARSInstallCheck },
                { IntegrityCheck.OperationName.InstallConfigManager, ASRResources.StringResources.SummaryConfigManagerInstallCheck },
                { IntegrityCheck.OperationName.DownloadMySql, ASRResources.StringResources.SummaryMySQLDownloadCheck },
                { IntegrityCheck.OperationName.RegisterDra, ASRResources.StringResources.SummaryDRARegCheck },
                { IntegrityCheck.OperationName.RegisterCloudContainer, ASRResources.StringResources.SummaryContainerRegCheck },
                { IntegrityCheck.OperationName.RegisterMt, ASRResources.StringResources.SummaryMTRegCheck },
                { IntegrityCheck.OperationName.SetMarsProxy, ASRResources.StringResources.SummarySetMARSProxyCheck },
                { IntegrityCheck.OperationName.IsMachineMemoryAvailable, ASRResources.StringResources.SummaryMachineMemoryCheck },
                { IntegrityCheck.OperationName.AreCoresAvailable, ASRResources.StringResources.SummaryProcessorCoresCheck },
                { IntegrityCheck.OperationName.CheckRegistryAccessPolicy, ASRResources.StringResources.SummaryRegistryAccessPolicyCheck },
                { IntegrityCheck.OperationName.CheckCommandPromptPolicy, ASRResources.StringResources.SummaryCommandPromprtAccessPolicyCheck },
                { IntegrityCheck.OperationName.CheckTrustLogicAttachmentsPolicy, ASRResources.StringResources.SummaryTrustLogicFileAttachmentsPolicyCheck },
                { IntegrityCheck.OperationName.PowershellExecutionPolicyStatus, ASRResources.StringResources.SummaryPowerShellExecutionPolicyCheck },
                { IntegrityCheck.OperationName.WebsiteConfigurationCheck, ASRResources.StringResources.SummaryWebsiteConfigurationCheck },
                { IntegrityCheck.OperationName.IsAnonymousAuthEnabled, ASRResources.StringResources.SummaryAnonymousauthenticationCheck },
                { IntegrityCheck.OperationName.IsFastCgiDisabled, ASRResources.StringResources.SummaryFastCgiConfigurationCheck },
                { IntegrityCheck.OperationName.IsIISPortConfigured, ASRResources.StringResources.SummaryPortBindingsCheck },
                { IntegrityCheck.OperationName.InstallIIS, ASRResources.StringResources.SummaryIISInstallCheck },
                { IntegrityCheck.OperationName.CheckDhcpStatus, ASRResources.StringResources.PrereqNetworkInterfaceText },
                { IntegrityCheck.OperationName.IsRightPerlVersionInstalled, ASRResources.StringResources.PrereqPerlCheckText },
                { IntegrityCheck.OperationName.CheckFIPSStatus, ASRResources.StringResources.PrereqFipsCheckText },
                { IntegrityCheck.OperationName.CheckTLS10Status, ASRResources.StringResources.PrereqTls10CheckText },
            };

        /// <summary>
        /// Validations to Operations mapping.
        /// </summary>
        public static readonly Dictionary<IntegrityCheck.Validations, IntegrityCheck.OperationName> ValidationMappings =
            new Dictionary<IntegrityCheck.Validations, IntegrityCheck.OperationName>()
            {
                { IntegrityCheck.Validations.IsFreeSpaceAvailable, IntegrityCheck.OperationName.IsFreeSpaceAvailable },
                { IntegrityCheck.Validations.IsSystemRestartPending, IntegrityCheck.OperationName.IsSystemRestartPending },
                { IntegrityCheck.Validations.IsWinGreaterThan2012R2, IntegrityCheck.OperationName.IsWinGreaterThan2012R2 },
                { IntegrityCheck.Validations.IsTimeInSync, IntegrityCheck.OperationName.IsTimeInSync },
                { IntegrityCheck.Validations.IsCsServiceRunning, IntegrityCheck.OperationName.IsCsServiceRunning },
                { IntegrityCheck.Validations.IsPsServiceRunning, IntegrityCheck.OperationName.IsPsServiceRunning },
                { IntegrityCheck.Validations.IsPushInstallServiceRunning, IntegrityCheck.OperationName.IsPushInstallServiceRunning },
                { IntegrityCheck.Validations.IsCxPsServiceRunning, IntegrityCheck.OperationName.IsCxPsServiceRunning },
                { IntegrityCheck.Validations.IsDraServiceRunning, IntegrityCheck.OperationName.IsDraServiceRunning },
                { IntegrityCheck.Validations.IsVxAgentServiceRunning, IntegrityCheck.OperationName.IsVxAgentServiceRunning },
                { IntegrityCheck.Validations.IsScoutApplicationServiceRunning, IntegrityCheck.OperationName.IsScoutApplicationServiceRunning },
                { IntegrityCheck.Validations.IsProcessServerServiceRunning, IntegrityCheck.OperationName.IsProcessServerServiceRunning },
                { IntegrityCheck.Validations.IsProcessServerMonitorServiceRunning, IntegrityCheck.OperationName.IsProcessServerMonitorServiceRunning },
                { IntegrityCheck.Validations.IsMarsServiceRuning, IntegrityCheck.OperationName.IsMarsServiceRuning },
                { IntegrityCheck.Validations.CheckEndpointsConnectivity, IntegrityCheck.OperationName.CheckEndpointsConnectivity },
                { IntegrityCheck.Validations.IsMachineMemoryAvailable, IntegrityCheck.OperationName.IsMachineMemoryAvailable },
                { IntegrityCheck.Validations.AreCoresAvailable, IntegrityCheck.OperationName.AreCoresAvailable },
                { IntegrityCheck.Validations.CheckRegistryAccessPolicy, IntegrityCheck.OperationName.CheckRegistryAccessPolicy },
                { IntegrityCheck.Validations.CheckCommandPromptPolicy,  IntegrityCheck.OperationName.CheckCommandPromptPolicy },
                { IntegrityCheck.Validations.CheckTrustLogicAttachmentsPolicy, IntegrityCheck.OperationName.CheckTrustLogicAttachmentsPolicy },
                { IntegrityCheck.Validations.PowershellExecutionPolicyStatus, IntegrityCheck.OperationName.PowershellExecutionPolicyStatus },
                { IntegrityCheck.Validations.WebsiteConfigurationCheck, IntegrityCheck.OperationName.WebsiteConfigurationCheck },
                { IntegrityCheck.Validations.IsAnonymousAuthEnabled, IntegrityCheck.OperationName.IsAnonymousAuthEnabled },
                { IntegrityCheck.Validations.IsFastCgiDisabled, IntegrityCheck.OperationName.IsFastCgiDisabled },
                { IntegrityCheck.Validations.IsIISPortConfigured, IntegrityCheck.OperationName.IsIISPortConfigured },
                { IntegrityCheck.Validations.CheckDhcpStatus, IntegrityCheck.OperationName.CheckDhcpStatus },
                { IntegrityCheck.Validations.IsRightPerlVersionInstalled, IntegrityCheck.OperationName.IsRightPerlVersionInstalled },
                { IntegrityCheck.Validations.CheckFIPSStatus, IntegrityCheck.OperationName.CheckFIPSStatus },
                { IntegrityCheck.Validations.CheckTLS10Status, IntegrityCheck.OperationName.CheckTLS10Status },
            };


        /// <summary>
        /// Service to respective component install mapping.
        /// </summary>
        public static readonly Dictionary<IntegrityCheck.Validations, IntegrityCheck.OperationName> ServiceToOperationMappings =
            new Dictionary<IntegrityCheck.Validations, IntegrityCheck.OperationName>()
            {
                { IntegrityCheck.Validations.IsCsServiceRunning, IntegrityCheck.OperationName.InstallCs },
                { IntegrityCheck.Validations.IsPsServiceRunning, IntegrityCheck.OperationName.InstallPs },
                { IntegrityCheck.Validations.IsPushInstallServiceRunning, IntegrityCheck.OperationName.InstallPs },
                { IntegrityCheck.Validations.IsCxPsServiceRunning, IntegrityCheck.OperationName.InstallPs },
                { IntegrityCheck.Validations.IsProcessServerServiceRunning, IntegrityCheck.OperationName.InstallPs },
                { IntegrityCheck.Validations.IsProcessServerMonitorServiceRunning, IntegrityCheck.OperationName.InstallPs },
                { IntegrityCheck.Validations.IsDraServiceRunning, IntegrityCheck.OperationName.InstallDra },
                { IntegrityCheck.Validations.IsVxAgentServiceRunning, IntegrityCheck.OperationName.InstallMt },
                { IntegrityCheck.Validations.IsScoutApplicationServiceRunning, IntegrityCheck.OperationName.InstallMt },
                { IntegrityCheck.Validations.IsMarsServiceRuning, IntegrityCheck.OperationName.InstallMars }
            };

        /// <summary>
        /// List of prerequisites.
        /// </summary>
        public enum Prerequisites
        {
            CheckMachineConfiguration,
            IsSystemRestartPending,
            IsWinGreaterThanOrEqualTo2012R2,
            IsIISNotConfigured,
            GroupPoliciesAbsent,
            IsTimeInSync,
            IsFreeSpaceAvailable,
            CheckDhcpStatus,
            IsRightPerlVersionInstalled
        };

        /// <summary>
        /// Prerequisites to sub validations mapping.
        /// </summary>
        public static readonly Dictionary<Prerequisites, List<IntegrityCheck.Validations>> UiPrereqMappings = new Dictionary<Prerequisites, List<IntegrityCheck.Validations>>()
        {
            { Prerequisites.CheckMachineConfiguration, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.IsMachineMemoryAvailable,
                IntegrityCheck.Validations.AreCoresAvailable
            }
            },
            { Prerequisites.IsSystemRestartPending, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.IsSystemRestartPending
            }
            },
            { Prerequisites.IsWinGreaterThanOrEqualTo2012R2, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.IsWinGreaterThan2012R2
            }
            },
            { Prerequisites.IsIISNotConfigured, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.WebsiteConfigurationCheck,
                IntegrityCheck.Validations.IsAnonymousAuthEnabled,
                IntegrityCheck.Validations.IsFastCgiDisabled,
                IntegrityCheck.Validations.IsIISPortConfigured
            }
            },
            { Prerequisites.GroupPoliciesAbsent, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.CheckRegistryAccessPolicy,
                IntegrityCheck.Validations.CheckCommandPromptPolicy,
                IntegrityCheck.Validations.CheckTrustLogicAttachmentsPolicy,
                IntegrityCheck.Validations.PowershellExecutionPolicyStatus,
                IntegrityCheck.Validations.CheckFIPSStatus
            }
            },
            { Prerequisites.IsTimeInSync, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.IsTimeInSync
            }
            },
            { Prerequisites.IsFreeSpaceAvailable, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.IsFreeSpaceAvailable
            }
            },
            { Prerequisites.CheckDhcpStatus, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.CheckDhcpStatus
            }
            },
            { Prerequisites.IsRightPerlVersionInstalled, new List<IntegrityCheck.Validations>()
            {
                IntegrityCheck.Validations.IsRightPerlVersionInstalled
            }
            }
        };

        /// <summary>
        /// Returns list of prereqs.
        /// </summary>
        /// <returns>Fabric specific Prereqs.</returns>
        public static List<Prerequisites> GetPrereqs()
        {
            List<Prerequisites> fabricPrereqs = new List<Prerequisites>();
            fabricPrereqs.Add(Prerequisites.CheckMachineConfiguration);
            fabricPrereqs.Add(Prerequisites.IsSystemRestartPending);
            fabricPrereqs.Add(Prerequisites.IsWinGreaterThanOrEqualTo2012R2);
            fabricPrereqs.Add(Prerequisites.IsIISNotConfigured);
            fabricPrereqs.Add(Prerequisites.GroupPoliciesAbsent);
            fabricPrereqs.Add(Prerequisites.IsTimeInSync);
            fabricPrereqs.Add(Prerequisites.IsFreeSpaceAvailable);
            fabricPrereqs.Add(Prerequisites.CheckDhcpStatus);
            fabricPrereqs.Add(Prerequisites.IsRightPerlVersionInstalled);

            return fabricPrereqs;
        }

        /// <summary>
        /// Returns server specific service checks to be done.
        /// </summary>
        /// <param name="serverMode">server mode</param>
        /// <returns>Returns validation result</returns>
        public static List<IntegrityCheck.Validations> GetServiceChecks(string serverMode)
        {
            List<IntegrityCheck.Validations> serviceValidations = new List<IntegrityCheck.Validations>();

            if (serverMode == UnifiedSetupConstants.CSServerMode)
            {
                serviceValidations.Add(IntegrityCheck.Validations.IsCsServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsPsServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsPushInstallServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsCxPsServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsProcessServerServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsProcessServerMonitorServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsDraServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsMarsServiceRuning);
                serviceValidations.Add(IntegrityCheck.Validations.IsVxAgentServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsScoutApplicationServiceRunning);
            }
            else
            {
                // Mode will be the PS
                serviceValidations.Add(IntegrityCheck.Validations.IsPsServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsPushInstallServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsCxPsServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsProcessServerServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsProcessServerMonitorServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsMarsServiceRuning);
                serviceValidations.Add(IntegrityCheck.Validations.IsVxAgentServiceRunning);
                serviceValidations.Add(IntegrityCheck.Validations.IsScoutApplicationServiceRunning);
            }

            return serviceValidations;
        }

        /// <summary>
        /// Evaluate service checks and returns the result
        /// </summary>
        /// <param name="validationName">validation to be evaluated.</param>
        /// <returns>Returns validation result</returns>
        public static IntegrityCheck.Response EvaluateServiceChecks(IntegrityCheck.Validations validationName)
        {
            IntegrityCheck.Response response;

            switch (validationName)
            {
                case IntegrityCheck.Validations.IsDraServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.DRA
                        });

                    if (response.Result == IntegrityCheck.OperationResult.Success)
                    {
                        IntegrityCheck.DraServiceInput input = new IntegrityCheck.DraServiceInput()
                        {
                            FabricAdapterType = DRSetupConstants.FabricAdapterType.InMageAdapter
                        };

                        return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsDraServiceRunning, input);
                    }
                    else
                    {
                        return response;
                    }
                case IntegrityCheck.Validations.IsCsServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.CS
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsCsServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsPsServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.PS
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsPsServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsPushInstallServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.PI
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsPushInstallServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsCxPsServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.CXPS
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsCxPsServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsMarsServiceRuning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.MARS
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsMarsServiceRuning) :
                        response;
                case IntegrityCheck.Validations.IsVxAgentServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.VxAgent
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsVxAgentServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsScoutApplicationServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.ScoutAgent
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsScoutApplicationServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsProcessServerServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.ProcessServer
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsProcessServerServiceRunning) :
                        response;
                case IntegrityCheck.Validations.IsProcessServerMonitorServiceRunning:
                    response = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsServicePresent,
                        new IntegrityCheck.IsServicePresentInput()
                        {
                            ServiceType = IntegrityCheck.DRServiceType.ProcessServerMonitor
                        });

                    return response.Result == IntegrityCheck.OperationResult.Success ?
                        IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsProcessServerMonitorServiceRunning) :
                        response;
                default:
                    throw new Exception("Not supported service check.");
            }
        }

        /// <summary>
        /// Evaluates Prereq and returns the result.
        /// </summary>
        /// <param name="validationName">validation to be evaluated.</param>
        /// <returns>Returns validation result</returns>
        public static IntegrityCheck.Response EvaluatePrereq(IntegrityCheck.Validations validationName)
        {
            switch (validationName)
            {
                case IntegrityCheck.Validations.IsFreeSpaceAvailable:
                    IntegrityCheck.Response freespaceResult = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsFreeSpaceAvailable,
                        new IntegrityCheck.CheckFreespaceAvailableInput()
                        {
                            MinimumFreeSpaceRequired = 5,
                            FreeSpaceRequiredInGB = 600
                        });

                    if (freespaceResult.Result != IntegrityCheck.OperationResult.Failed)
                    {
                        IntegrityCheck.Response freespaceDriveResult = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                            IntegrityCheck.Validations.IsFreeSpaceAvailable,
                            new IntegrityCheck.CheckFreespaceAvailableInput()
                            {
                                MinimumFreeSpaceRequired = 2,
                                FreeSpaceRequiredInGB = 2,
                                TargetDrive = UnifiedSetupConstants.CDrive
                            });

                        if (freespaceDriveResult.Result == IntegrityCheck.OperationResult.Failed)
                        {
                            return freespaceDriveResult;
                        }
                    }

                    return freespaceResult;
                case IntegrityCheck.Validations.IsMachineMemoryAvailable:
                    IntegrityCheck.Response machineMemoryResult = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.IsMachineMemoryAvailable,
                        new IntegrityCheck.CheckMachineMemoryInput()
                        {
                            MinimumMemoryRequiredInGB = 16
                        });

                    return machineMemoryResult;
                case IntegrityCheck.Validations.AreCoresAvailable:
                    IntegrityCheck.Response coresResult = IntegrityCheck.IntegrityCheckWrapper.Evaluate(
                        IntegrityCheck.Validations.AreCoresAvailable,
                        new IntegrityCheck.CheckProcessorCoresInput()
                        {
                            MinimumProcessorCoresRequired = 8
                        });

                    return coresResult;

                case IntegrityCheck.Validations.IsSystemRestartPending:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsSystemRestartPending);
                case IntegrityCheck.Validations.IsTimeInSync:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsTimeInSync);
                case IntegrityCheck.Validations.IsWinGreaterThan2012R2:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsWinGreaterThan2012R2);
                case IntegrityCheck.Validations.CheckRegistryAccessPolicy:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.CheckRegistryAccessPolicy);
                case IntegrityCheck.Validations.CheckCommandPromptPolicy:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.CheckCommandPromptPolicy);
                case IntegrityCheck.Validations.CheckTrustLogicAttachmentsPolicy:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.CheckTrustLogicAttachmentsPolicy);
                case IntegrityCheck.Validations.PowershellExecutionPolicyStatus:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.PowershellExecutionPolicyStatus);
                case IntegrityCheck.Validations.WebsiteConfigurationCheck:
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        IntegrityCheck.Response websiteResponse = new IntegrityCheck.Response();
                        websiteResponse.Message = ASRResources.StringResources.SkipIISConfigCheckText;
                        websiteResponse.Result = IntegrityCheck.OperationResult.Skip;
                        return websiteResponse;
                    }
                    else
                    {
                        return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.WebsiteConfigurationCheck);
                    }
                case IntegrityCheck.Validations.IsAnonymousAuthEnabled:
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        IntegrityCheck.Response anonymousAuthenticationResponse = new IntegrityCheck.Response();
                        anonymousAuthenticationResponse.Message = ASRResources.StringResources.SkipIISConfigCheckText;
                        anonymousAuthenticationResponse.Result = IntegrityCheck.OperationResult.Skip;
                        return anonymousAuthenticationResponse;
                    }
                    else
                    {
                        return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsAnonymousAuthEnabled);
                    }
                case IntegrityCheck.Validations.IsFastCgiDisabled:
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        IntegrityCheck.Response fastCGIResponse = new IntegrityCheck.Response();
                        fastCGIResponse.Message = ASRResources.StringResources.SkipIISConfigCheckText;
                        fastCGIResponse.Result = IntegrityCheck.OperationResult.Skip;
                        return fastCGIResponse;
                    }
                    else
                    {
                        return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsFastCgiDisabled);
                    }
                case IntegrityCheck.Validations.IsIISPortConfigured:
                    if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                    {
                        IntegrityCheck.Response portResponse = new IntegrityCheck.Response();
                        portResponse.Message = ASRResources.StringResources.SkipIISConfigCheckText;
                        portResponse.Result = IntegrityCheck.OperationResult.Skip;
                        return portResponse;
                    }
                    else
                    {
                        return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsIISPortConfigured);
                    }
                case IntegrityCheck.Validations.CheckDhcpStatus:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.CheckDhcpStatus);
                case IntegrityCheck.Validations.IsRightPerlVersionInstalled:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.IsRightPerlVersionInstalled);
                case IntegrityCheck.Validations.CheckFIPSStatus:
                    return IntegrityCheck.IntegrityCheckWrapper.Evaluate(IntegrityCheck.Validations.CheckFIPSStatus);
                default:
                    throw new Exception("Un-supported Prerequisite check.");
            }
        }
    }
}
