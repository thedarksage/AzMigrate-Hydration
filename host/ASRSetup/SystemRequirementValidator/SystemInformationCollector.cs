using ASRSetupFramework;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Management;
using System.ServiceProcess;
using StorageDeviceLibrary.NativeStructures;
using StorageDeviceLibrary;

using DRIVE_LAYOUT_INFORMATION_EX = StorageDeviceLibrary.NativeStructures.DRIVE_LAYOUT_INFORMATION_EX;

namespace SystemRequirementValidator
{
    /// <summary>
    /// This class defines different methods that figures system properties like system and boot disk.
    /// </summary>
    class SystemInfoCollector
    {
        /// <summary>
        /// Figures system drives from system 
        /// First it figures system volume using SystemDrive Environment variable.
        /// From system volume it figures partition and corresponding disks using WMI.
        /// </summary>
        /// <returns></returns>
        public static ICollection<int> GetSystemDrives()
        {
            ICollection<int> SystemDrives = new HashSet<int>();

            Trc.Log(LogLevel.Debug, "Enterting GetSystemDrives");

            // Get the system logical disk id (drive letter)
            string systemLogicalDiskDeviceId = Environment.GetFolderPath(Environment.SpecialFolder.System).Substring(0, 2);

            if (string.IsNullOrEmpty(systemLogicalDiskDeviceId))
            {
                throw new Exception("Failed to get Environment.SpecialFolder.System");
            }

            string query = string.Format("SELECT * FROM Win32_LogicalDisk WHERE DeviceID='{0}'", systemLogicalDiskDeviceId);
            Trc.Log(LogLevel.Debug, "Query to get system disk: {0}", query);

            // Start by enumerating the logical disks
            using (var searcher = new ManagementObjectSearcher(query))
            {
                foreach (ManagementObject logicalDisk in searcher.Get())
                {
                    foreach (ManagementObject partition in logicalDisk.GetRelated("Win32_DiskPartition"))
                    {
                        foreach (ManagementObject diskDrive in partition.GetRelated("Win32_DiskDrive"))
                        {
                            SystemDrives.Add(int.Parse(diskDrive["Index"].ToString()));
                            Trc.Log(LogLevel.Debug, "Adding disk {0}", diskDrive["Index"].ToString());
                        }
                    }
                }
            }

            Trc.Log(LogLevel.Debug, "Exiting GetSystemDrives");
            return SystemDrives;
        }

        /// <summary>
        /// Method to get boot drives from system.
        /// </summary>
        /// <returns></returns>
        public static ICollection<int> GetBootDrives()
        {
            ICollection<int> BootDrives = new HashSet<int>();

            Trc.Log(LogLevel.Debug, "Enterting GetBootDrives");

            string query = string.Format("SELECT * FROM Win32_DiskPartition WHERE BootPartition='true'");
            Trc.Log(LogLevel.Debug, "Query to get system disk: {0}", query);

            using (var searcher = new ManagementObjectSearcher(query))
            {
                foreach (ManagementObject partition in searcher.Get())
                {
                    foreach (ManagementObject diskDrive in partition.GetRelated("Win32_DiskDrive"))
                    {
                        BootDrives.Add(int.Parse(diskDrive["Index"].ToString()));
                        Trc.Log(LogLevel.Debug, "Adding disk {0}", diskDrive["Index"].ToString());
                    }
                }
            }

            Trc.Log(LogLevel.Debug, "Exiting GetBootDrives");
            return BootDrives;
        }

        public static UInt32 GetNumberOfPartitions(int diskIndex)
        {
            Trc.Log(LogLevel.Debug, "Entering GetNumberOfPartitions");

            string diskName = string.Format(@"\\.\PhysicalDrive{0}", diskIndex);

            DRIVE_LAYOUT_INFORMATION_EX layoutEx = IoControlFunctions.IOCTL_DISK_GET_DRIVE_LAYOUT_EX(diskName);

            Trc.Log(LogLevel.Info, string.Format("Disk {0} has {1} number of partitions.", diskIndex, layoutEx.PartitionCount));
            Trc.Log(LogLevel.Debug, "Exiting GetNumberOfPartitions");
            return layoutEx.PartitionCount;
        }

        /// <summary>
        /// Method that validates where service key inside registry has proper values or, not.
        /// </summary>
        /// <param name="root">Root registry control set</param>
        /// <param name="szDriverName">Driver or, service name to be validated</param>
        /// <param name="pathValidation">For driver we validate path.</param>
        /// <param name="startType">Start type as expected. like for VSS it should be Manual or, less</param>
        public static void ValidateServiceKeys(string root, string szDriverName, bool pathValidation, int startType)
        {
            Trc.Log(LogLevel.Debug, string.Format(
                    @"Enterting ValidateServiceKeys root: {0} szDriverName: {1} PathValidation: {2} StartType: {3}",
                                        root,
                                        szDriverName,
                                        pathValidation.ToString(),
                                        startType.ToString()));

            string drvRegKeyPath = string.Format("{0}\\{1}", root, szDriverName);

            Trc.Log(LogLevel.Debug, "Validating registry key {0}", drvRegKeyPath);

            RegistryKey drvKey = Registry.LocalMachine.OpenSubKey(drvRegKeyPath, false);
            if (null == drvKey)
            {
                throw new Exception(string.Format("Failed to open registry {0}", drvRegKeyPath));
            }

            string sysDrive = Environment.SystemDirectory;
            if (null == sysDrive)
            {
                throw new Exception(string.Format("Failed to get Environment.SystemDirectory"));
            }

            Trc.Log(LogLevel.Debug, "System Drive: {0}", sysDrive);

            var regStartTypeValue = drvKey.GetValue("Start");
            if (null == regStartTypeValue)
            {
                throw new Exception(string.Format("{0}\\Start doesnt exist", drvRegKeyPath));
            }

            Trc.Log(LogLevel.Debug, "Start: {0}", regStartTypeValue.ToString());

            int iStartType = int.Parse(regStartTypeValue.ToString());
            if ((startType != -1) && (iStartType > startType))
            {
                string serviceStartType = ((ServiceStartMode)iStartType).ToString();
                throw new Exception(string.Format(ErrorResources.ServiceInvalidStartState, serviceStartType));
            }

            // Path validation is applicable for driver (.sys) files and not for services.
            if (!pathValidation)
            {
                Trc.Log(LogLevel.Debug, "No Path Validation.\nExiting ValidateServiceKeys");
                return;
            }

            string regPath = (string)drvKey.GetValue("ImagePath");

            // Check for path existence.
            if (String.IsNullOrEmpty(regPath))
            {
                throw new Exception(string.Format("{0}\\ImagePath doesnt exist", drvRegKeyPath));
            }

            Trc.Log(LogLevel.Debug, "ImagePath: {0}", regPath);

            string windowsPath = Environment.GetEnvironmentVariable("SystemRoot");

            if (string.IsNullOrEmpty(windowsPath))
            {
                throw new Exception(string.Format(ErrorResources.EnvironmentVariableNotFoundMsg, "SystemRoot"));
            }

            Trc.Log(LogLevel.Debug, "windowsPath: {0}", windowsPath);

            string path = string.Format("{0}\\{1}", windowsPath, regPath);
            Trc.Log(LogLevel.Debug, "driver path: {0}", path);

            if (regPath.StartsWith("\\", StringComparison.CurrentCultureIgnoreCase))
            {
                string envVar = regPath.Substring(regPath.IndexOf("\\") + 1, regPath.IndexOf("\\", regPath.IndexOf("\\") + 1) - 1);

                string rootPath = Environment.GetEnvironmentVariable(envVar);

                if (string.IsNullOrEmpty(rootPath))
                {
                    throw new Exception(string.Format(ErrorResources.EnvironmentVariableNotFoundMsg, envVar));
                }

                path = string.Format("{0}\\{1}", rootPath, regPath.Substring(regPath.IndexOf("\\", regPath.IndexOf("\\") + 1) + 1));
                Trc.Log(LogLevel.Debug, "absolute driver path: {0}", path);
            }

            // Check for path existence.
            if (!File.Exists(path))
            {
                throw new Exception(string.Format(ErrorResources.FileNotFoundMsg, path));
            }

            Trc.Log(LogLevel.Debug, "Exiting ValidateServiceKeys");
        }

        public static bool IsDiskGPT(int diskIndex)
        {
            Trc.Log(LogLevel.Debug, "Entering IsDiskGPT");

            string diskName = string.Format(@"\\.\PhysicalDrive{0}", diskIndex);

            DRIVE_LAYOUT_INFORMATION_EX layoutEx = IoControlFunctions.IOCTL_DISK_GET_DRIVE_LAYOUT_EX(diskName);
            Trc.Log(LogLevel.Debug, "Disk Layout is {0}", layoutEx.PartitionStyle);

            Trc.Log(LogLevel.Debug, "Exiting IsDiskGPT");
            return (layoutEx.PartitionStyle == PARTITION_STYLE.PARTITION_STYLE_GPT);
        }

        public static uint GetDiskSectorSize(int diskIndex)
        {
            Trc.Log(LogLevel.Debug, "Entering GetDiskSectorSize");

            string  diskName = string.Format(@"\\.\PhysicalDrive{0}", diskIndex);

            DISK_GEOMETRY geometry = IoControlFunctions.IOCTL_DISK_GET_DRIVE_GEOMETRY(diskName);

            Trc.Log(LogLevel.Debug, "Disk {0} BytesPerSector {1}", diskIndex, geometry.BytesPerSector);

            Trc.Log(LogLevel.Debug, "Exiting GetDiskSectorSize");
            return geometry.BytesPerSector;
        }

        // https://social.technet.microsoft.com/wiki/contents/articles/942.hyper-v-how-to-detect-if-a-computer-is-a-vm-using-script.aspx
        public static string GetHypervisor()
        {
            Trc.Log(LogLevel.Debug, "Entering GetHypervisor");

            string query = string.Format("SELECT Manufacturer, Model from Win32_ComputerSystem");
            Trc.Log(LogLevel.Debug, "Query is {0}", query);

            var searcher = new ManagementObjectSearcher(query);
            var queryResult = searcher.Get();
            if (queryResult.Count == 0)
            {
                Trc.Log(LogLevel.Error, "Failed to find a result for Query {0}", query);
                return string.Empty;
            }

            ManagementObject mobject = queryResult.OfType<ManagementObject>().FirstOrDefault();
            string hypervisor = Hypervisor.PHYSICAL;

            if (null != mobject["Model"])
            {
                string model = mobject["Model"].ToString();
                Trc.Log(LogLevel.Debug, "Model: {0}", model);
                if (string.IsNullOrEmpty(model))
                {
                    // Physical
                }
                else if (model.IndexOf(ComputerManufacturers.VMWAREMODEL, StringComparison.InvariantCultureIgnoreCase) >= 0)
                {
                    hypervisor = Hypervisor.VMWARE;
                }
                else if (model.IndexOf(ComputerManufacturers.HYPERVMODEL, StringComparison.InvariantCultureIgnoreCase) >= 0)
                {
                    if (null != mobject["Manufacturer"])
                    {
                        string manufacturer = mobject["Manufacturer"].ToString();
                        Trc.Log(LogLevel.Debug, "Manufacturer: {0}", manufacturer);
                        if (!string.IsNullOrEmpty(manufacturer) &&
                            (manufacturer.IndexOf(ComputerManufacturers.HYPERVMANUFACTURER, 
                                                  StringComparison.InvariantCultureIgnoreCase) >= 0))
                        {
                            hypervisor = Hypervisor.HYPERV;
                        }
                    }
                }
            }
            Trc.Log(LogLevel.Debug, "Exiting GetHypervisor Hypervisor: {0}", hypervisor);
            return hypervisor;
        }
    }
}