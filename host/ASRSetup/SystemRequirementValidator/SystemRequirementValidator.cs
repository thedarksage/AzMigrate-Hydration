using ASRSetupFramework;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Reflection;
using System.ServiceProcess;

namespace SystemRequirementValidator
{
    public delegate void Handler(IDictionary<string, string> Params, string logFile);

    /// <summary>
    /// Class for system requirement handling.
    /// This class defines different methods that check for a given system requirement.
    /// </summary>
    public class SourceRequirementHandler
    {
        private static IDictionary<string, Handler> s_handlerFunc;

        public SourceRequirementHandler()
        {
            s_handlerFunc = new Dictionary<string, Handler>()
            {
                {SystemRequirementNames.BootDriversCheck, BootDriversCheck},
                {SystemRequirementNames.CriticalServicesCheck, CriticalServicesCheck},
                {SystemRequirementNames.BootAndSystemDiskCheck, BootAndSystemDiskCheck},
                {SystemRequirementNames.ActivePartitonsCheck, ActivePartitionsCheck},
                {SystemRequirementNames.BootUEFICheck, BootUEFICheck},
                {SystemRequirementNames.VSSProviderInstallationCheck, VSSProviderInstallationCheck},
                {SystemRequirementNames.FileValidationCheck, FileValidationCheck},
                {SystemRequirementNames.CommandExecutionCheck, CommandExecutionCheck},
                {SystemRequirementNames.SHA2CompatibilityCheck, SHA2CompatibilityCheck},
                {SystemRequirementNames.AgentMandatoryFilesCheck, AgentMandatoryFilesCheck},
                {SystemRequirementNames.MachineMemoryCheck, MachineMemoryCheck},
                {SystemRequirementNames.InvolFltDriverCheck, InvolFltDriverCheck}
            };
        }

        /// <summary>
        /// Returns the handler for validating given requirement.
        /// </summary>
        /// <param name="functionname">handler for given requirement.</param>
        /// <returns></returns>
        public Handler GetHandler(string functionname)
        {
            if (!s_handlerFunc.ContainsKey(functionname))
            {
                throw new Exception(string.Format("Failed to get handler for check {0}", functionname));
            }

            Trc.Log(LogLevel.Debug, "Handler for check {0} is {1}", functionname, s_handlerFunc[functionname].ToString());
            return s_handlerFunc[functionname];
        }

        /// <summary>
        /// Return all control sets from system.
        /// </summary>
        static IList<string> QueryControlSets()
        {
            IList<string> controlSets = new List<string>();

            RegistryKey controlKey = Registry.LocalMachine.OpenSubKey("System");
            foreach (var subkey in controlKey.GetSubKeyNames())
            {
                if (subkey.StartsWith("ControlSet", StringComparison.OrdinalIgnoreCase))
                {
                    Trc.Log(LogLevel.Debug, "Adding control set {0}", subkey);
                    controlSets.Add(subkey);
                }
            }
            controlKey.Close();

            return controlSets;
        }

        /// <summary>
        /// Method for validating Critical Services.
        /// It opens registry and checks if all values inside it is as expected.
        /// </summary>
        /// <param name="attribMap">Parameters for service validation like service names.</param>
        /// <param name="logFile">Log file that capture errors.</param>
        static void CriticalServicesCheck(IDictionary<string, string> attribMap, string logFile)
        {
            IList<string> controlsets = new List<string>();
            ICollection<string> inCompatibleServices = new HashSet<string>();

            Trc.Log(LogLevel.Debug, "Entering CriticalServicesCheck");

            if (!attribMap.ContainsKey(CriticalServicesCheckParams.ServicesList))
            {
                Trc.Log(LogLevel.Error, "Key {0} doesn't exist in parameters for CriticalServicesCheck", CriticalServicesCheckParams.ServicesList);
                throw new Exception(string.Format("invalid checker config. {0} key is missing.", CriticalServicesCheckParams.ServicesList));
            }

            if (!attribMap.ContainsKey(CommonAttributeValues.ControlSets))
            {
                Trc.Log(LogLevel.Error, "Key {0} doesn't exist in parameters for CriticalServicesCheck", CommonAttributeValues.ControlSets);
                throw new Exception("invalid checker config.");
            }

            IList<string> controlSets = new List<string>();

            if (attribMap[CommonAttributeValues.ControlSets].Equals(CommonValues.AllControlSets, StringComparison.OrdinalIgnoreCase))
            {
                controlSets = QueryControlSets();
            }
            else
            {
                controlSets.Add(attribMap[CommonAttributeValues.ControlSets]);
            }

            string serviceslist = attribMap[CriticalServicesCheckParams.ServicesList];

            Trc.Log(LogLevel.Debug, "Services: {0}", serviceslist);

            char[] separators = { ',' };
            var services = serviceslist.Split(separators);

            // Validate services in given control set.
            foreach (var controlSet in controlSets)
            {
                string root = string.Format("SYSTEM\\{0}\\Services", controlSet);

                foreach (var service in services)
                {
                    try
                    {
                        Trc.Log(LogLevel.Debug, "Validating service {0} in {1}", service, root);
                        SystemInfoCollector.ValidateServiceKeys(root, service, false, (int)ServiceStartMode.Manual);
                        Trc.Log(LogLevel.Info, "Validated Service {0} in {1}", service, root);
                    }
                    catch (Exception ex)
                    {
                        Trc.Log(LogLevel.Error, ex.ToString());
                        inCompatibleServices.Add(service);
                    }
                }

                if (inCompatibleServices.Count != 0)
                {
                    string servicesStr = string.Join(",", inCompatibleServices.Select(d => d).ToArray());
                    IDictionary<string, string> ErrorParams = new Dictionary<string, string>()
                {
                    {CriticalServicesMissingMsgArgs.Services, servicesStr}
                };

                    Trc.Log(LogLevel.Error, "Services {0} are incompatible", servicesStr);
                    throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceCriticalServicesMissing.ToString(),
                                                        ErrorParams,
                                                        string.Format(ErrorResources.ServicesNotReady, servicesStr));
                }
            }
            Trc.Log(LogLevel.Debug, "Exiting CriticalServicesCheck");
        }

        /// <summary>
        /// Handler to validate boot drivers.
        /// </summary>
        /// <param name="attribMap">contains driver names and other parameters to be validated.</param>
        /// <param name="logFile">Log file that capture errors.</param>
        static void BootDriversCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Debug, "Entering BootDriversCheck");

            ICollection<string> inCompatDrivers = new HashSet<string>();

            if (!attribMap.ContainsKey(BootDriverCheckParams.DriversList))
            {
                Trc.Log(LogLevel.Error, "Key {0} doesn't exist in parameters for BootDriverCheckParams", BootDriverCheckParams.DriversList);
                throw new Exception("invalid checker config.");
            }

            if (!attribMap.ContainsKey(CommonAttributeValues.ControlSets))
            {
                Trc.Log(LogLevel.Error, "Key {0} doesn't exist in parameters for BootDriverCheckParams", CommonAttributeValues.ControlSets);
                throw new Exception("invalid checker config.");
            }

            IList<string> controlSets = new List<string>();

            if (attribMap[CommonAttributeValues.ControlSets].Equals(CommonValues.AllControlSets, StringComparison.OrdinalIgnoreCase))
            {
                controlSets = QueryControlSets();
            }
            else
            {
                controlSets.Add(attribMap[CommonAttributeValues.ControlSets]);
            }

            string driverlist = attribMap[BootDriverCheckParams.DriversList];
            char[] separators = { ',' };
            var drivers = driverlist.Split(separators);

            foreach (var controlSet in controlSets)
            {
                string root = string.Format("SYSTEM\\{0}\\Services", controlSet);
                foreach (var driver in drivers)
                {
                    try
                    {
                        Trc.Log(LogLevel.Debug, "Validating driver {0} in {1}", driver, root);
                        SystemInfoCollector.ValidateServiceKeys(root, driver, false, -1);
                        Trc.Log(LogLevel.Info, "Validated driver {0} in {1}", driver, root);
                    }
                    catch (Exception ex)
                    {
                        Trc.Log(LogLevel.Error, ex.ToString());
                        inCompatDrivers.Add(driver);
                    }
                }
            }

            if (inCompatDrivers.Count != 0)
            {
                string driversStr = string.Join(",", inCompatDrivers.Select(d => d).ToArray());
                IDictionary<string, string> ErrorParams = new Dictionary<string, string>()
                {
                    {BootDriversMissingMsgArgs.Drivers, driversStr}
                };

                Trc.Log(LogLevel.Error, "Drivers {0} are incompatible", driversStr);

                throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceBootDriversMissing.ToString(),
                                                    ErrorParams,
                                                    string.Format(ErrorResources.BootDriverNotPresent, driversStr));
            }

            Trc.Log(LogLevel.Debug, "Exiting BootDriversCheck");
        }

        /// <summary>
        /// Handler to validate active partitions count.
        /// </summary>
        /// <param name="attribMap"></param>
        /// <param name="logFile">Log file that capture errors.</param>
        static void ActivePartitionsCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Debug, "Entering ActivePartitionsCheck");

            if ((1 != attribMap.Count) && !attribMap.ContainsKey(ActivePartitonsCheckParams.NumberOfActivePartitions))
            {
                Trc.Log(LogLevel.Error, "Key {0} doesn't exist in parameters", ActivePartitonsCheckParams.NumberOfActivePartitions);
                throw new Exception("invalid checker config.");
            }

            int activePartitionsCount = 0;
            if (!int.TryParse(attribMap[ActivePartitonsCheckParams.NumberOfActivePartitions], out activePartitionsCount))
            {
                Trc.Log(LogLevel.Error, "{0} attribute contains unsupported '{1}' value",
                                            ActivePartitonsCheckParams.NumberOfActivePartitions,
                                            attribMap[ActivePartitonsCheckParams.NumberOfActivePartitions]);

                throw new Exception(string.Format("{0} attribute contains unsupported '{1}' value",
                    ActivePartitonsCheckParams.NumberOfActivePartitions,
                    attribMap[ActivePartitonsCheckParams.NumberOfActivePartitions]));
            }

            ICollection<int> bootDrives = SystemInfoCollector.GetBootDrives();
            if (bootDrives.Count != activePartitionsCount)
            {
                string bootDrivesStr = string.Join(",", bootDrives.Select(n => n.ToString()).ToArray());

                Trc.Log(LogLevel.Error, string.Format(ErrorResources.MultipleActivePartitions, bootDrivesStr));

                throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceMultipleActivePartitions.ToString(),
                    new Dictionary<string, string>()
                    {
                        {MultipleActivePartitionsMsgArgs.Partitions, bootDrivesStr}
                    },
                    string.Format(ErrorResources.MultipleActivePartitions, bootDrivesStr));
            }

            Trc.Log(LogLevel.Info, "Validated ActivePartitionsCheck");
            Trc.Log(LogLevel.Debug, "Exiting ActivePartitionsCheck");
        }

        /// <summary>
        /// Handler to validate boot and system disk check
        /// </summary>
        /// <param name="attribMap">Contains attributes of this check.</param>
        /// <param name="logFile">Log file that capture errors.</param>
        static void BootAndSystemDiskCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Debug, "Entering BootAndSystemDiskCheck");

            if ((1 != attribMap.Count) && !attribMap.ContainsKey(BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk))
            {
                Trc.Log(LogLevel.Error, "Key {0} doesn't exist in parameters", BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk);
                throw new Exception("invalid checker config.");
            }

            bool isBootAndSystemPartitionOnSameDisk = false;
            if (!bool.TryParse(attribMap[BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk], out isBootAndSystemPartitionOnSameDisk))
            {
                Trc.Log(LogLevel.Error, "{0} attribute contains unsupported '{1}' value",
                                            BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk,
                                            attribMap[BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk]);

                throw new Exception(string.Format("{0} attribute contains unsupported '{1}' value",
                    BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk,
                    attribMap[BootAndSystemDiskCheckParams.BootAndSystemPartitionsOnSameDisk]));
            }

            if (!isBootAndSystemPartitionOnSameDisk)
            {
                Trc.Log(LogLevel.Debug, "Exiting BootAndSystemDiskCheck");
                return;
            }

            ICollection<int> systemDrives = SystemInfoCollector.GetSystemDrives();
            if (1 != systemDrives.Count)
            {
                if (0 == systemDrives.Count)
                {
                    Trc.Log(LogLevel.Error, ErrorResources.NoSystemDisk);

                    throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceNoSystemDiskFound.ToString(),
                        null,
                        ErrorResources.NoSystemDisk);
                }

                Trc.Log(LogLevel.Error, string.Format(ErrorResources.MultipleSystemDisks,
                            string.Join(",", systemDrives.Select(n => n.ToString()).ToArray())));

                throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceMultipleSystemDisks.ToString(),
                    new Dictionary<string, string>()
                    {
                        {MultipleSystemDisksMsgArgs.SystemDisk, string.Join(",", systemDrives.Select(n => n.ToString()).ToArray())}
                    },
                    string.Format(ErrorResources.MultipleSystemDisks,
                            string.Join(",", systemDrives.Select(n => n.ToString()).ToArray())));
            }

            ICollection<int> bootDrives = SystemInfoCollector.GetBootDrives();
            if (0 == bootDrives.Count)
            {
                Trc.Log(LogLevel.Error, ErrorResources.NoBootDisk);

                throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceNoBootDiskFound.ToString(),
                        null,
                        ErrorResources.NoBootDisk);
            }

            string systemDisk = systemDrives.First().ToString();

            if (!bootDrives.Contains(systemDrives.First()))
            {
                string bootDisks = string.Join(",", bootDrives.Select(n => n.ToString()).ToArray());
                IDictionary<string, string> ErrorParams = new Dictionary<string, string>() 
                {
                    {BootAndSystemDiskOnSeparateDisksMsgArgs.SystemDisk, systemDrives.First().ToString()},
                    {BootAndSystemDiskOnSeparateDisksMsgArgs.BootDisk, bootDisks},
                };

                Trc.Log(LogLevel.Error, string.Format(ErrorResources.BootAndSystemDiskOnSeparateDisk, bootDisks, systemDisk));

                throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceBootAndSystemDiskOnSeparateDisks.ToString(),
                                                    ErrorParams,
                                                    string.Format(ErrorResources.BootAndSystemDiskOnSeparateDisk, bootDisks, systemDisk));
            }


            Trc.Log(LogLevel.Info, "Validated Requirement BootAndSystemDiskCheck");
            Trc.Log(LogLevel.Debug, "Exiting BootAndSystemDiskCheck");
        }

        static void BootUEFICheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Debug, "Entering BootUefiCheck");

            if (0 == attribMap.Count)
            {
                Trc.Log(LogLevel.Debug, "No Arguments. Exiting BootUefiCheck");
                return;
            }

            ICollection<int> systemDrives = SystemInfoCollector.GetSystemDrives();
            if (1 != systemDrives.Count)
            {
                if (0 == systemDrives.Count)
                {
                    Trc.Log(LogLevel.Error, ErrorResources.NoBootDisk);

                    throw new SystemRequirementNotMet(SystemRequirementFailure.ASRMobilityServiceNoBootDiskFound.ToString(),
                        null,
                        ErrorResources.NoBootDisk);
                }

                string errorMsg = string.Format(ErrorResources.MultipleSystemDisks,
                                                string.Join(",", systemDrives.Select(n => n.ToString()).ToArray()));

                Trc.Log(LogLevel.Error, errorMsg);

                throw new SystemRequirementNotMet(
                    SystemRequirementFailure.ASRMobilityServiceMultipleBootDisks.ToString(),
                    new Dictionary<string, string>()
                    {
                        {MultipleBootDisksMsgArgs.BootDisk, string.Join(",", systemDrives.Select(n => n.ToString()).ToArray())}
                    },
                    errorMsg);
            }

            if (!SystemInfoCollector.IsDiskGPT(systemDrives.First()))
            {
                Trc.Log(LogLevel.Info, string.Format("System Disk {0} is not GPT. Skipping uefi check.", systemDrives.First()));
                Trc.Log(LogLevel.Debug, "Exiting BootUefiCheck");
                return;
            }

            if (attribMap.ContainsKey(BootUEFIParams.Hypervisor))
            {
                string supportedHypervisor = attribMap[BootUEFIParams.Hypervisor];
                string currentHypervisor = SystemInfoCollector.GetHypervisor();

                if (!supportedHypervisor.Equals(currentHypervisor, StringComparison.InvariantCultureIgnoreCase))
                {
                    string errorMsg = string.Format(ErrorResources.BootUEFIUnsupportedHypervisor,
                                                    currentHypervisor);
                    Trc.Log(LogLevel.Error, errorMsg);

                    throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceBootUEFIUnsupportedHypervisor.ToString(),
                        new Dictionary<string, string>()
                    {
                        {HypervisorNameArg.HypervisorName, currentHypervisor}
                    },
                    errorMsg);
                }
            }

            if (!attribMap.ContainsKey(BootUEFIParams.MinOSMajorVersion) &&
                attribMap.ContainsKey(BootUEFIParams.MinOSMinorVersion))
            {
                string exceptionmsg = string.Format("Invalid config: Contains attribute {0} without attribute {1}",
                                                                                        BootUEFIParams.MinOSMinorVersion,
                                                                                        BootUEFIParams.MinOSMajorVersion);
                Trc.Log(LogLevel.Error, exceptionmsg);
                throw new Exception(exceptionmsg);
            }

            if (attribMap.ContainsKey(BootUEFIParams.MinOSMajorVersion))
            {
                int minOsMinorVersion, minOsMajorVersion;
                Trc.Log(LogLevel.Debug, "Current VM OS Version Major: {0} Minor: {1}",
                                                    Environment.OSVersion.Version.Major,
                                                    Environment.OSVersion.Version.Minor);

                if (!int.TryParse(attribMap[BootUEFIParams.MinOSMajorVersion], out minOsMajorVersion))
                {
                    string exceptionmsg = string.Format("Invalid config: Attribute {0} contains invalid value {1}",
                                                                                            BootUEFIParams.MinOSMajorVersion,
                                                                                            attribMap[BootUEFIParams.MinOSMajorVersion]);
                    Trc.Log(LogLevel.Error, exceptionmsg);
                    throw new Exception(exceptionmsg);
                }

                int osMajorVersion = Environment.OSVersion.Version.Major;

                if (osMajorVersion < minOsMajorVersion)
                {
                    throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceBootUEFIUnsupportedOSMajor.ToString(),
                         new Dictionary<string, string>() 
                         {
                             {BootUEFIUnsupportedOSMajorMsgArgs.MinOsMajorVersion, minOsMajorVersion.ToString()},
                             {BootUEFIUnsupportedOSMajorMsgArgs.currentOsMajorVersion, osMajorVersion.ToString()}
                         },
                         string.Format(ErrorResources.BootUEFIUnsupportedOSMajor, minOsMajorVersion, osMajorVersion)
                        );
                }

                if (osMajorVersion == minOsMajorVersion)
                {
                    if (!int.TryParse(attribMap[BootUEFIParams.MinOSMinorVersion], out minOsMinorVersion))
                    {
                        string exceptionmsg = string.Format("Invalid config: Attribute {0} contains invalid value {1}",
                                                                                                BootUEFIParams.MinOSMinorVersion,
                                                                                                attribMap[BootUEFIParams.MinOSMinorVersion]);
                        Trc.Log(LogLevel.Error, exceptionmsg);
                        throw new Exception(exceptionmsg);
                    }

                    int osMinorVersion = Environment.OSVersion.Version.Minor;

                    if (osMinorVersion < minOsMinorVersion)
                    {
                        throw new SystemRequirementNotMet(
                            SystemRequirementFailure.ASRMobilityServiceBootUEFIUnsupportedOS.ToString(),
                             new Dictionary<string, string>() 
                         {
                             {BootUEFIUnsupportedOSMsgArgs.MinOsMajorVersion, minOsMajorVersion.ToString()},
                             {BootUEFIUnsupportedOSMsgArgs.MinOsMinorVersion, minOsMinorVersion.ToString()},
                             {BootUEFIUnsupportedOSMsgArgs.CurrentOsMajorVersion, osMajorVersion.ToString()},
                             {BootUEFIUnsupportedOSMsgArgs.CurrentOsMinorVersion, osMinorVersion.ToString()}
                         },
                             string.Format(ErrorResources.BootUEFIUnsupportedOS, minOsMajorVersion, minOsMinorVersion, osMajorVersion, osMinorVersion)
                            );
                    }
                }
            }

            if (attribMap.ContainsKey(BootUEFIParams.BytesPerSector))
            {
                uint uiSupportedBytesPerSector;
                if (!uint.TryParse(attribMap[BootUEFIParams.BytesPerSector], out uiSupportedBytesPerSector))
                {
                    string exceptionmsg = string.Format("Invalid config: Attribute {0} contains invalid value {1}",
                                                                                            BootUEFIParams.BytesPerSector,
                                                                                            attribMap[BootUEFIParams.BytesPerSector]);
                    Trc.Log(LogLevel.Error, exceptionmsg);
                    throw new Exception(exceptionmsg);
                }

                uint uiBootDiskSectorSize = SystemInfoCollector.GetDiskSectorSize(systemDrives.First());

                if (uiSupportedBytesPerSector != uiBootDiskSectorSize)
                {
                    throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceBootUEFIUnsupportedDiskSectorSize.ToString(),
                         new Dictionary<string, string>() 
                         {
                             {BootUEFIUnsupportedDiskSectorSizeMsgArgs.SupportedBytesPerSector, uiSupportedBytesPerSector.ToString()},
                             {BootUEFIUnsupportedDiskSectorSizeMsgArgs.BootDiskBytesPerSector, uiBootDiskSectorSize.ToString()},
                         },
                         string.Format(ErrorResources.BootUEFIUnsupportedDiskSectorSize, uiSupportedBytesPerSector, uiBootDiskSectorSize)
                        );
                }
            }

            if (attribMap.ContainsKey(BootUEFIParams.MaxNumPartititons))
            {
                UInt32 uiMaxNumPartitionsSupported;
                if (!uint.TryParse(attribMap[BootUEFIParams.MaxNumPartititons], out uiMaxNumPartitionsSupported))
                {
                    string exceptionmsg = string.Format("Invalid config: Attribute {0} contains invalid value {1}",
                                                                                            BootUEFIParams.MaxNumPartititons,
                                                                                            attribMap[BootUEFIParams.MaxNumPartititons]);
                    Trc.Log(LogLevel.Error, exceptionmsg);
                    throw new Exception(exceptionmsg);
                }

                UInt32 uiNumPartitions = SystemInfoCollector.GetNumberOfPartitions(systemDrives.First());
                if (uiNumPartitions > uiMaxNumPartitionsSupported)
                {
                    string exceptionmsg = string.Format(ErrorResources.BootUEFIUnsupportedNumberOfPartitions, uiMaxNumPartitionsSupported, uiNumPartitions);
                    Trc.Log(LogLevel.Error, exceptionmsg);
                    throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceBootUEFIUnsupportedNumberOfPartitions.ToString(),
                         new Dictionary<string, string>() 
                         {
                             {BootUEFIUnsupportedNumberOfPartitionsMsgArgs.MaxNumPartitionsSupported, uiMaxNumPartitionsSupported.ToString()},
                             {BootUEFIUnsupportedNumberOfPartitionsMsgArgs.NumPartitions, uiNumPartitions.ToString()},
                         },
                         exceptionmsg
                        );
                }
            }

            if (attribMap.ContainsKey(BootUEFIParams.SecureBoot))
            {
                Trc.Log(LogLevel.Debug, "Validating UEFI Secure Boot");

                bool isSecureBootReq = false;

                if (!bool.TryParse(attribMap[BootUEFIParams.SecureBoot], out isSecureBootReq))
                {
                    Trc.Log(LogLevel.Error, "{0} attribute contains unsupported '{1}' value",
                                                BootUEFIParams.SecureBoot,
                                                attribMap[BootUEFIParams.SecureBoot]);

                    throw new Exception(string.Format("{0} attribute contains unsupported '{1}' value",
                        BootUEFIParams.SecureBoot,
                        attribMap[BootUEFIParams.SecureBoot]));
                }

                bool isSecureBoot = false;

                try
                {
                    RegistryKey controlKey = Registry.LocalMachine.OpenSubKey(UEFISecureBoot.RegKey);

                    if (null != controlKey)
                    {
                        var value = controlKey.GetValue(UEFISecureBoot.ValueName);
                        if (null != value)
                        {
                            isSecureBoot = (0 != (int)value);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Debug, "Registry Value {0} doesnt exist", UEFISecureBoot.ValueName);
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Debug, "Registry Key {0} doesnt exist", UEFISecureBoot.RegKey);
                    }
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Failed to open registry {0} erro: {1}", UEFISecureBoot.RegKey, ex);
                }

                if (isSecureBoot != isSecureBootReq)
                {
                    string exceptionmsg = string.Format(ErrorResources.BootUEFISecureBootNotSupported);
                    Trc.Log(LogLevel.Error, exceptionmsg);
                    throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceBootUEFISecureBootNotSupported.ToString(),
                         new Dictionary<string, string>()
                         {
                         },
                         exceptionmsg
                        );
                }
            }
                
            Trc.Log(LogLevel.Info, string.Format("Validated UEFI Boot Disk {0}.", systemDrives.First()));
            Trc.Log(LogLevel.Debug, "Exiting BootUefiCheck");
        }

        static void VSSProviderInstallationCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Debug, "Entering VSSInstallation Check");

            int osMajorVersion = Environment.OSVersion.Version.Major,
            osMinorVersion = Environment.OSVersion.Version.Minor,
            minOsMajorVersion, minOsMinorVersion;

            if (0 == attribMap.Count)
            {
                Trc.Log(LogLevel.Debug,"No parameters found for VSS Provider Installatipon check. Exiting");
                throw new Exception("Mandatory parameters missing for VSS Provider Installation check");
            }
            else
            {
                if (attribMap.ContainsKey(VSSInstallationCheckParams.MinOSMajorVersion))
                {
                    if (!int.TryParse(attribMap[VSSInstallationCheckParams.MinOSMajorVersion], out minOsMajorVersion))
                    {
                        Trc.Log(LogLevel.Info, string.Format("Unable to parse parameter {0}. Exiting VSS Provider installation check.",
                            VSSInstallationCheckParams.MinOSMajorVersion));
                        throw new Exception(string.Format("Invalid parameter values. Argument {0} contains value {1}",
                            VSSInstallationCheckParams.MinOSMajorVersion, attribMap[VSSInstallationCheckParams.MinOSMajorVersion]));
                    }
                    else
                    {
                        if(osMajorVersion < minOsMajorVersion)
                        {
                            Trc.Log(LogLevel.Always,string.Format("Os major version {0} is less than minimum supported os major version {1}. Thus exiting VSS Provider Installation.",
                                osMajorVersion,minOsMajorVersion));
                            return;
                        }

                        if(osMajorVersion == minOsMajorVersion)
                        {
                            if(!attribMap.ContainsKey(VSSInstallationCheckParams.MinOSMinorVersion))
                            {
                                Trc.Log(LogLevel.Info, string.Format("Parameter {0} not found. Exiting VSS provider installation",
                                    VSSInstallationCheckParams.MinOSMinorVersion));
                                throw new Exception(string.Format("Mandatory argument {0} missing in parameters",
                                    VSSInstallationCheckParams.MinOSMinorVersion));
                            }
                            else if (!int.TryParse(attribMap[VSSInstallationCheckParams.MinOSMinorVersion], out minOsMinorVersion))
                            {
                                Trc.Log(LogLevel.Info, string.Format("Unable to parse parameter {0}. Exiting VSS Provider Installation check.",
                                    VSSInstallationCheckParams.MinOSMinorVersion));
                                throw new Exception(string.Format("Invalid parameter values. Argument {0} contains value {1}",
                                    VSSInstallationCheckParams.MinOSMinorVersion, attribMap[VSSInstallationCheckParams.MinOSMinorVersion]));
                            }
                            else if (osMinorVersion < minOsMinorVersion)
                            {
                                Trc.Log(LogLevel.Always, string.Format("Os version {0}.{1} is less than minimum supported os version {2}.{3} .Thus skipping VSS Provider Installation.",
                                    osMajorVersion, osMinorVersion, minOsMajorVersion,minOsMinorVersion));
                                return;
                            }
                        }                      
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Info, string.Format("Parameter {0} not found. Exiting VSS provider installation",
                        VSSInstallationCheckParams.MinOSMajorVersion));
                    throw new Exception(string.Format("Mandatory argument {0} missing in parameters",
                        VSSInstallationCheckParams.MinOSMajorVersion));
                }
            }

            int exitCode;
            string VssInstallationTargetDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            Trc.Log(LogLevel.Always, string.Format("VSS Provider Installation location is {0}", VssInstallationTargetDirectory));

            Trc.Log(LogLevel.Info, "Starting VSS Installation");
            exitCode = SetupHelper.InstallVSSProvider(VssInstallationTargetDirectory);
            Trc.Log(LogLevel.Always, string.Format("Vss Installation exit code {0}",exitCode.ToString()));

            if(exitCode != 0)
            {
                Trc.Log(LogLevel.Error,string.Format("Vss installation failed with exit code : {0}",exitCode.ToString()));

                string exitcode = (exitCode == -1) ? "internal error" : exitCode.ToString();
                string exceptionMsg = string.Format(ErrorResources.VSSInstallationFailure, exitcode);
                string errorCode;

                if(VSSProviderInstallErrors.ErrorCodeMap.ContainsKey(exitCode))
                {
                    errorCode = VSSProviderInstallErrors.ErrorCodeMap[exitCode].ToString();
                }
                else
                {
                    //default error code
                    errorCode = SystemRequirementFailure.ASRMobilityServiceVSSProviderInstallationFailed.ToString();
                }

                throw new SystemRequirementNotMet(
                    errorCode,
                    new Dictionary<string, string>()
                    {
                        {VSSInstallationFailedInPrecheckMsgArgs.ErrorCode, exitcode}
                    },
                    exceptionMsg
                );
            }

            Trc.Log(LogLevel.Always, "Completed VSS Installation");
            Trc.Log(LogLevel.Debug, "Exiting Vss Installation check");
        }

        static void FileValidationCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Always, "Entered File Validation Check");
            if(attribMap.Count == 0)
            {
                Trc.Log(LogLevel.Error, "No arguments found in File Validation Check");
                throw new Exception("No arguments found in File Validation Check");
            }

            if(!attribMap.ContainsKey(FileValidationCheckParams.Filename))
            {
                Trc.Log(LogLevel.Error, string.Format("Parameter {0} not found in params",FileValidationCheckParams.Filename));
                throw new Exception(string.Format("Parameter {0} not found in params", FileValidationCheckParams.Filename));
            }
            if (!attribMap.ContainsKey(FileValidationCheckParams.Path))
            {
                Trc.Log(LogLevel.Error, string.Format("Parameter {0} not found in params", FileValidationCheckParams.Path));
                throw new Exception(string.Format("Parameter {0} not found in params", FileValidationCheckParams.Path));
            }

            string fileList = attribMap[FileValidationCheckParams.Filename], pathlist = attribMap[FileValidationCheckParams.Path];
            char[] separators = { ',' };
            var files = fileList.Split(separators);
            var paths = pathlist.Split(separators);
            IList<string> filesNotPresent = new List<string>();

            if(files.Length != paths.Length)
            {
                Trc.Log(LogLevel.Error, string.Format("Param {0} and Param {1} have different no of values",
                    FileValidationCheckParams.Filename, FileValidationCheckParams.Path));
                throw new Exception(string.Format("Param {0} and Param {1} have different no of values",
                    FileValidationCheckParams.Filename, FileValidationCheckParams.Path));
            }

            for (var counter = 0; counter < files.Length; counter ++ )
            {
                if (!FileValidationCheckHelper.FilePathMap.ContainsKey(files[counter]))
                {
                    Trc.Log(LogLevel.Error, "File path not found for the file " + files[counter]);
                    throw new Exception("File path not found for the file " + files[counter]);
                }
                string filepath = Environment.ExpandEnvironmentVariables(paths[counter]) + FileValidationCheckHelper.FilePathMap[files[counter]];
                Trc.Log(LogLevel.Debug, string.Format("Path of file {0} is {1}", files[counter], filepath));
                if (File.Exists(filepath))
                {
                    Trc.Log(LogLevel.Always, "The file " + filepath + " is present in the system");
                }
                else
                {
                    Trc.Log(LogLevel.Debug, "The file " + filepath + " is not present in the system");
                    filesNotPresent.Add(files[counter]);
                }
            }
            
            if(filesNotPresent.Count > 0)
            {
                string fileNotPresentList = string.Join(",", filesNotPresent.ToArray());
                Trc.Log(LogLevel.Error, "The following files are not present in the system : " + fileNotPresentList);
                throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceInstallerMandatoryFilesMissing.ToString(),
                        new Dictionary<string, string>()
                    {
                        { FileValidationCheckPrecheckMsgArgs.MissingExe, fileNotPresentList}
                    },
                        string.Format(ErrorResources.MandatoryMissingFiles, fileNotPresentList)
                    );
            }
            else
            {
                Trc.Log(LogLevel.Debug, "All required files are present in the system");
            }
            Trc.Log(LogLevel.Always, "Exiting File Validation Check");
        }

        static void CommandExecutionCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Always, "Entered Command Execution Check");
            if (attribMap.Count == 0)
            {
                Trc.Log(LogLevel.Error, "No arguments found in Command Execution Check");
                throw new Exception("No arguments found in Command Execution Check");
            }

            if (!attribMap.ContainsKey(CommandExecutionCheckParams.CommandName))
            {
                Trc.Log(LogLevel.Error, string.Format("Parameter {0} not found in params", CommandExecutionCheckParams.CommandName));
                throw new Exception(string.Format("Parameter {0} not found in params", CommandExecutionCheckParams.CommandName));
            }
            if (!attribMap.ContainsKey(CommandExecutionCheckParams.Result))
            {
                Trc.Log(LogLevel.Error, string.Format("Parameter {0} not found in params", CommandExecutionCheckParams.Result));
                throw new Exception(string.Format("Parameter {0} not found in params", CommandExecutionCheckParams.Result));
            }
            if (!attribMap.ContainsKey(CommandExecutionCheckParams.Arguments))
            {
                Trc.Log(LogLevel.Error, string.Format("Parameter {0} not found in params", CommandExecutionCheckParams.Arguments));
                throw new Exception(string.Format("Parameter {0} not found in params", CommandExecutionCheckParams.Arguments));
            }

            string commandList = attribMap[CommandExecutionCheckParams.CommandName], resultList = attribMap[CommandExecutionCheckParams.Result],
                argsList = attribMap[CommandExecutionCheckParams.Arguments];
            char[] separators = { ',' };
            var commands = commandList.Split(separators);
            var results = resultList.Split(separators);
            var args = argsList.Split(separators);
            IList<string> commandsNotRunning = new List<string>();

            if (commands.Length != results.Length || commands.Length != args.Length)
            {
                Trc.Log(LogLevel.Error, string.Format("Param {0}, Param {1} and Param {2} have different no. of values",
                    CommandExecutionCheckParams.CommandName, CommandExecutionCheckParams.Result, CommandExecutionCheckParams.Arguments));
                throw new Exception(string.Format("Param {0}, Param {1} and Param {2} have different no. of values",
                    CommandExecutionCheckParams.CommandName, CommandExecutionCheckParams.Result, CommandExecutionCheckParams.Arguments));
            }

            int[] expectedResult = new int [commands.Length];
            for(int counter = 0; counter < commands.Length; counter++)
            {
                if (!Int32.TryParse(results[counter], out expectedResult[counter]))
                {
                    Trc.Log(LogLevel.Error, "Failed to parse result {0}", results[counter]);
                    throw new Exception(string.Format("Failed to parse result {0}", results[counter]));
                }
            }

            for (int counter = 0; counter < commands.Length; counter++ )
            {
                string output = String.Empty, error = String.Empty;
                int exitCode = 0;
                string commandLine = args[counter].IsNullOrWhiteSpace() ? commands[counter] : commands[counter] + " " + args[counter];
                try
                {
                    exitCode = CommandExecutor.ExecuteCommand(commands[counter], out output, out error, args[counter]);
                }
                catch(Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Failed to execute command {0} with exception {1}", commandLine, ex);
                    commandsNotRunning.Add(commandLine);
                    //if there is any exception in running a command, then assume that the command failed
                    // so log the error and continue with next command
                    continue;
                }
                if(exitCode != expectedResult[counter])
                {
                    commandsNotRunning.Add(commandLine);
                }
                Trc.Log(LogLevel.Debug, "Exit code of the command {0} is {1}", commandLine, exitCode);
            }

            if(commandsNotRunning.Count > 0 )
            {
                string commandsNotRunningList = string.Join(",", commandsNotRunning.ToArray());
                Trc.Log(LogLevel.Error, "The following commands are not running in the system : " + commandsNotRunningList);
                throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceInstallerCommandExecutionFailure.ToString(),
                        new Dictionary<string, string>()
                    {
                        { CommandExecutionCheckPrecheckMsgArgs.FailedCommands, commandsNotRunningList}
                    },
                        string.Format(ErrorResources.CommandsNotRunning, commandsNotRunningList)
                    );
            }
            else
            {
                Trc.Log(LogLevel.Debug, "Successfully executed all the commands");
            }

            Trc.Log(LogLevel.Always, "Exiting Command Execution Check");
        }

        /// <summary>
        /// Checks for SHA2 support on selected os
        /// Kb required for SHA2 support https://support.microsoft.com/en-us/help/4472027/2019-sha-2-code-signing-support-requirement-for-windows-and-wsus
        /// </summary>
        /// <returns>true: if SHA2 support is present, false otherwise</returns>
        static void SHA2CompatibilityCheck(IDictionary<string, string> attribMap, string logFile)
        {
            int OSMajorVersion = Environment.OSVersion.Version.Major,
                OSMinorVersion = Environment.OSVersion.Version.Minor;

            IList<string> SupportedKbList, HotFixIDList, KbNotPresentList;
            HotFixIDList = new List<string>();
            KbNotPresentList = new List<string>();
            if (OSMajorVersion == 6 && OSMinorVersion == 0)
            {
                SupportedKbList = UnifiedSetupConstants.SHA2SupportKbwin2k8;
            }
            else if(OSMajorVersion == 6 && OSMinorVersion == 1)
            {
                SupportedKbList = UnifiedSetupConstants.SHA2SupportKbwin2k8R2;
            }
            else
            {
                SupportedKbList = new List<string>();
            }

            if (OSMajorVersion == 6 && (OSMinorVersion == 0 || OSMinorVersion == 1))
            {
                const string query = "SELECT HotFixID FROM Win32_QuickFixEngineering";
                ManagementObjectSearcher searcher = new ManagementObjectSearcher(query);

                foreach (ManagementObject quickFix in searcher.Get())
                    HotFixIDList.Add(quickFix["HotFixID"].ToString());

                foreach (var Kb in SupportedKbList)
                {
                    if (!HotFixIDList.Contains(Kb))
                    {
                        KbNotPresentList.Add(Kb);
                    }
                }

                if (KbNotPresentList.Count > 0)
                {
                    string KbNotPresent = String.Join(",", KbNotPresentList.ToArray());
                    SystemRequirementFailure errorCode = (OSMinorVersion == 0) ? 
                        SystemRequirementFailure.ASRMobilityServiceInstallerUnsupportedSHA2SigningFailureWindows2008 :
                        SystemRequirementFailure.ASRMobilityServiceInstallerUnsupportedSHA2SigningFailureWindows2008R2;
                    Trc.Log(LogLevel.Error, string.Format(ErrorResources.SHA2SigningSupportFailure, KbNotPresent));
                    throw new SystemRequirementNotMet(
                                errorCode.ToString(),
                                new Dictionary<string, string>()
                                {
                                    {SHA2CompatibilityCheckPrecheckMsgArgs.OSVersion, SetupHelper.GetOSNameFromOSVersion()}
                                },
                                string.Format(ErrorResources.SHA2SigningSupportFailure, KbNotPresent)
                                );
                }
            }
        }

        static void AgentMandatoryFilesCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Always, "Entered Agent Mandatory Files Check");
            if (attribMap.Count == 0)
            {
                Trc.Log(LogLevel.Error, "No arguments found in Agent Mandatory Files Check");
                throw new Exception("No arguments found in Agent Mandatory Files Check");
            }

            if (!attribMap.ContainsKey(AgentMandatoryFilesCheckParams.FileName))
            {
                Trc.Log(LogLevel.Error, string.Format("Parameter {0} not found in params", AgentMandatoryFilesCheckParams.FileName));
                throw new Exception(string.Format("Parameter {0} not found in params", AgentMandatoryFilesCheckParams.FileName));
            }

            string installationLocation = SetupHelper.GetAgentInstalledLocation();

            if (string.IsNullOrEmpty(installationLocation) || installationLocation.Trim().Length == 0)
            {
                Trc.Log(LogLevel.Error, string.Format("Agent installation directory returned empty value."));
                throw new Exception(string.Format("Agent installation directory returned empty value."));
            }

            string fileList = attribMap[AgentMandatoryFilesCheckParams.FileName];
            char[] separators = { ',' };
            var files = fileList.Split(separators);
            IList<string> filesNotPresent = new List<string>();

            for (var counter = 0; counter < files.Length; counter++)
            {
                string filepath = Path.Combine(
                    installationLocation, files[counter]);
                Trc.Log(LogLevel.Always, string.Format("Path of file {0} is {1}", files[counter], filepath));
                if (File.Exists(filepath))
                {
                    Trc.Log(LogLevel.Always, "The file " + filepath + " is present in the system");
                }
                else
                {
                    Trc.Log(LogLevel.Error, "The file " + filepath + " is not present in the system");
                    filesNotPresent.Add(filepath);
                }
            }

            if (filesNotPresent.Count > 0)
            {
                string fileNotPresentList = string.Join(",", filesNotPresent.ToArray());
                Trc.Log(LogLevel.Error, "The following files are not present in the {0} directory {1} {2}: ", installationLocation, Environment.NewLine, fileNotPresentList);
                throw new SystemRequirementNotMet(
                        SystemRequirementFailure.ASRMobilityServiceInstallerUpgradeMandatoryFilesMissing.ToString(),
                        null,
                        string.Format(ErrorResources.MandatoryMissingFiles, fileNotPresentList)
                    );
            }
            else
            {
                Trc.Log(LogLevel.Always, "All required files are present in the {0} directory", installationLocation);
            }
            Trc.Log(LogLevel.Always, "Exiting Agent Mandatory Files Check");
        }

        static void MachineMemoryCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Always, "Entered Machine Memory Check");
            if (attribMap.Count == 0)
            {
                Trc.Log(LogLevel.Error, "No arguments found in Machine Memory Check");
                throw new Exception("No arguments found in Machine Memory Check");
            }

            ulong totalMemoryInMB = 0;
            bool isRequiredMemoryNotAvailable = false;

            totalMemoryInMB = SetupHelper.GetMachineMemoryBytes()/(1024*1024);

            ulong minimumMemoryRequiredInMB = ulong.Parse(attribMap[MemoryCheckParams.RequiredMemoryInMB]);
            Trc.Log(LogLevel.Debug, "Minimum memory requirement is {0}MB", minimumMemoryRequiredInMB.ToString());

            try
            {
                if (totalMemoryInMB >= minimumMemoryRequiredInMB)
                {
                    Trc.Log(LogLevel.Always, "The memory on the machine is {0} MB.", totalMemoryInMB.ToString());
                }
                else
                {
                    Trc.Log(LogLevel.Error, "Machine has only {0} GB memory. Minimum memory of {1} GB required.", totalMemoryInMB, minimumMemoryRequiredInMB);
                    isRequiredMemoryNotAvailable = true;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to validate machine memory with error: {0}", ex);
            }

            if(isRequiredMemoryNotAvailable)
            {
                throw new SystemRequirementNotMet(
                    SystemRequirementFailure.ASRMobilityServiceInstallerRequiredMemoryError.ToString(),
                    null,
                    string.Format(ErrorResources.MemoryError)
                    );
            }

            Trc.Log(LogLevel.Always, "Exiting Machine Memory Check");
        }

        static void InvolFltDriverCheck(IDictionary<string, string> attribMap, string logFile)
        {
            Trc.Log(LogLevel.Always, "Entered InvolFlt Driver Check");

            if (ServiceControlFunctions.IsInstalled(UnifiedSetupConstants.InVolFltName))
            {
                throw new SystemRequirementNotMet(
                    SystemRequirementFailure.ASRMobilityServiceInstallerInvolFltDriverError.ToString(),
                    null,
                    string.Format(ErrorResources.InvolFltDriverError)
                    );
            }

            Trc.Log(LogLevel.Always, "Exiting InvolFlt Driver Check");
        }
    };
}
