using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.ServiceBus;
using RcmContract;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.ServiceProcess;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities for system info and resources
    /// </summary>
    public static class SystemUtils
    {
        private const string MONITORING_LAST_BOOT_UP_TIME = "LastBootUpTime";
        private const string OS_NAME = "Caption";

        /// <summary>
        /// Get Bios Id of the system
        /// </summary>
        /// <returns>Bios id string</returns>
        public static string GetBiosId()
        {
            const string WmiScope = @"\\.\root\cimv2";
            const string WmiQuery = "SELECT * from Win32_ComputerSystemProduct";

            using (var searcher = new ManagementObjectSearcher(WmiScope, WmiQuery))
            using (var resultObjColl = searcher.Get())
            using (var objEnum = resultObjColl.GetEnumerator())
            {
                if (!objEnum.MoveNext())
                {
                    throw new InvalidDataException("Empty WMI object collection is returned for Win32_ComputerSystemProduct query");
                }

                using (var currWmiObj = objEnum.Current)
                {
                    return (string)currWmiObj.GetPropertyValue("UUID");
                }
            }
        }

        /// <summary>
        /// Get Ipv4 addresses of all NICs attached to the system
        /// </summary>
        /// <returns>Ipv4 addresses of all NICs</returns>
        public static IEnumerable<IPAddress> GetIpv4Addresses()
        {
            return NetworkInterface.GetAllNetworkInterfaces().
                SelectMany(currNic => currNic.GetIPProperties().UnicastAddresses).
                Where(currUnicAddr =>
                    currUnicAddr.DuplicateAddressDetectionState == DuplicateAddressDetectionState.Preferred &&
                    currUnicAddr.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork &&
                    currUnicAddr.Address != IPAddress.None &&
                    !IPAddress.IsLoopback(currUnicAddr.Address)).
                Select(currUnicAddr => currUnicAddr.Address);
        }

        /// <summary>
        /// Get FQDN of the system
        /// </summary>
        /// <returns>FQDN</returns>
        public static string GetFqdn()
        {
            return Dns.GetHostEntry("localhost").HostName;
        }

        public static DateTime? GetSystemBootUpTimeLocal()
        {
            DateTime? currentBootTime = null;
            if (GetSystemProperties(MONITORING_LAST_BOOT_UP_TIME, out string lastBootUpTimeStr))
            {
                currentBootTime = ManagementDateTimeConverter.ToDateTime(lastBootUpTimeStr);
            }

            return currentBootTime;
        }

        /// <summary>
        /// Get last system reboot time of PS
        /// </summary>
        /// <param name="traceSource">TraceSource</param>
        /// <returns>Reboot Time</returns>
        public static long GetLastSystemRebootTime(TraceSource traceSource)
        {
            long lastSystemRebootTime = 0;
            TaskUtils.RunAndIgnoreException(() =>
            {
                DateTime? systemRebootTime = GetSystemBootUpTimeLocal();
                if (systemRebootTime.HasValue)
                {
                    lastSystemRebootTime =
                        DateTimeUtils.GetSecondsSinceEpoch(systemRebootTime.Value);
                }
            }, traceSource);

            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                "LastSystemRebootTime : {0}", lastSystemRebootTime);

            return lastSystemRebootTime;
        }

        internal static bool GetSystemProperties(string property, out string value)
        {
            value = null;
            try
            {
                ManagementClass osClass = new ManagementClass("Win32_OperatingSystem");
                foreach (ManagementObject mo in osClass.GetInstances())
                {
                    value = mo[property].ToString();
                    return true;
                }
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Property {0} not found in operating system info. Ex: {1}{2}",
                    property,
                    Environment.NewLine,
                    ex);
            }

            return false;
        }

        private static ProcessServerAuthDetails BuildProcessServerAuthDetails(string serverCertThumbprint)
        {
            ProcessServerAuthDetails psAuthDetails = new ProcessServerAuthDetails
            {
                ServerCertThumbprint = serverCertThumbprint,
                ServerRolloverCertThumbprint = serverCertThumbprint,
                ServerCertSigningAuthority = CertificateSigningAuthority.SelfSigned.ToString()
            };
            return psAuthDetails;
        }

        internal static RegisterProcessServerInput BuildRcmPSRegisterInput(string serverCertThumbprint)
        {
            string psServiceDisplayName, psServiceName;

            using (var psSvcController =
                new ServiceController(Settings.Default.ServiceName_ProcessServer))
            {
                psServiceDisplayName = psSvcController.DisplayName;
                psServiceName = psSvcController.ServiceName;
            }

            var storedNatIpv4Address = PSConfiguration.Default.HttpListenerConfiguration?.NatIpv4Address;

            return new RegisterProcessServerInput
            {
                AgentVersion = PSInstallationInfo.Default.GetPSCurrentVersion(),
                FriendlyName = GetFqdn(),
                Id = PSInstallationInfo.Default.GetPSHostId(),
                InstallationLocation = PSInstallationInfo.Default.GetInstallationPath(),
                IpAddresses = GetIpv4Addresses().Distinct().Select(ip => ip.ToString()).ToList(),
                NatIpAddress = storedNatIpv4Address != null && storedNatIpv4Address != IPAddress.None ? storedNatIpv4Address.ToString() : null,

                LogRootFolder = PSInstallationInfo.Default.GetReplicationLogFolderPath(),
                TelemetryRootFolder = PSInstallationInfo.Default.GetTelemetryFolderPath(),
                Port = CxpsConfig.Default.SslPort.Value,
                ServiceDisplayName = psServiceDisplayName,
                ServiceName = psServiceName,
                ProcessServerAuthDetails = BuildProcessServerAuthDetails(serverCertThumbprint),
                SupportedJobs = SupportedJobsList(),
                SupportedFeatures = SupportedFeaturesList()
            };
        }

        public static ModifyProcessServerInput BuildRcmPSModifyInput(string serverCertThumbprint)
        {
            var storedNatIpv4Address = PSConfiguration.Default.HttpListenerConfiguration?.NatIpv4Address;

            return new ModifyProcessServerInput
            {
                AgentVersion = PSInstallationInfo.Default.GetPSCurrentVersion(),
                FriendlyName = GetFqdn(),
                Id = PSInstallationInfo.Default.GetPSHostId(),
                IpAddresses = GetIpv4Addresses().Distinct().Select(ip => ip.ToString()).ToList(),
                NatIpAddress = storedNatIpv4Address != null && storedNatIpv4Address != IPAddress.None ? storedNatIpv4Address.ToString() : null,
                Port = CxpsConfig.Default.SslPort.Value,
                ProcessServerAuthDetails = BuildProcessServerAuthDetails(serverCertThumbprint),
                SupportedJobs = SupportedJobsList(),
                SupportedFeatures = SupportedFeaturesList()
            };
        }
        
        public static List<string> SupportedJobsList()
        {
            List<string> supportedJobs = new List<string>();

            supportedJobs.Add(RcmJobEnum.JobType.ProcessServerPrepareForDisableProtection.ToString());
            supportedJobs.Add(RcmJobEnum.JobType.ProcessServerPrepareForSync.ToString());
            supportedJobs.Add(RcmJobEnum.JobType.ProcessServerPrepareForSwitchAppliance.ToString());

            return supportedJobs;
        }

        public static List<string> SupportedFeaturesList()
        {
            List<string> supportedFeatures = new List<string>();

            supportedFeatures.Add(RcmEnum.RcmComponentSupportedFeature.PrivateLink.ToString());

            return supportedFeatures;
        }

        public static bool IsValidIPV4Address(IPAddress ipAddress)
        {
            bool isValidIP = false;

            isValidIP = TaskUtils.RunAndIgnoreException(() =>
            {
                if (ipAddress != null)
                {
                    if (ipAddress == IPAddress.None)
                    {
                        throw new ArgumentException("Provide a valid Ipv4 Address");
                    }
                    else if (ipAddress.AddressFamily != AddressFamily.InterNetwork)
                    {
                        throw new ArgumentException("Not a valid Ipv4 address", nameof(ipAddress));
                    }
                }
            }, Tracers.Misc);

            return isValidIP;
        }

        public static string GetHostName()
        {
            return Dns.GetHostName();
        }

        public static string GetOSName()
        {
            string osName = string.Empty;

            TaskUtils.RunAndIgnoreException(() =>
            {
                if (GetSystemProperties(OS_NAME, out osName))
                {
                    osName = osName.ToString();
                }
            }, Tracers.Misc);

            return osName;
        }

        public static HostInfo GetHostInfo()
        {
            (string psPatchVersion, string psPatchInstallTime) = PSInstallationInfo.Default.GetPSPatchDetails();

            return new HostInfo
            {
                AccountId = PSInstallationInfo.Default.GetAccountId(),
                AccountType = PSInstallationInfo.Default.GetAccountType(),
                CSEnabled = PSInstallationInfo.Default.GetCSType() == CXType.CSAndPS ? 1 : 0,
                HypervisorName = string.Empty,
                HypervisorState = 0,
                HostId = PSInstallationInfo.Default.GetPSHostId(),
                IPAddress = GetPSIPAddressForRegistration(),
                HostName = GetHostName(),
                OSFlag = (int)OSFlag.Windows,
                OSType = SystemUtils.GetOSName(),
                Port = PSInstallationInfo.Default.GetNonSSLPort(),
                ProcessServerEnabled = 1,
                PSInstallTime = PSInstallationInfo.Default.GetPSInstallationDate(),
                PSPatchVersion = psPatchVersion,
                PatchInstallTime = psPatchInstallTime,
                PSVersion = PSInstallationInfo.Default.GetPSCurrentVersion(),
                SslPort = PSInstallationInfo.Default.GetSSLPort()
            };
        }

        public static Dictionary<string, NicInfo> GetNetworkInfo()
        {
            var networkInfo = new Dictionary<string, NicInfo>();

            TaskUtils.RunAndIgnoreException(() =>
            {
                var networkInterfaces = NetworkInterface.GetAllNetworkInterfaces();

                foreach (var nic in networkInterfaces)
                {
                    TaskUtils.RunAndIgnoreException(() =>
                    {
                        if (!nic.Supports(NetworkInterfaceComponent.IPv4) ||
                        nic.NetworkInterfaceType == NetworkInterfaceType.Loopback)
                        {
                            return;
                        }

                        var ipProperties = nic.GetIPProperties();

                        if (ipProperties == null)
                        {
                            return;
                        }

                        string nicName = nic.Description;
                        string dns = (ipProperties.DnsAddresses.Count > 0) ?
                        string.Join(",", ipProperties.DnsAddresses) : string.Empty;

                        var gatewayIPAddresses = new List<string>();
                        foreach (var gatewayAddress in ipProperties.GatewayAddresses)
                        {
                            TaskUtils.RunAndIgnoreException(() =>
                            {
                                var currGatewayIP =
                                RemoveSpecialCharsFromIPV6Address(gatewayAddress.Address.ToString());
                                if (!string.IsNullOrWhiteSpace(currGatewayIP))
                                {
                                    gatewayIPAddresses.Add(currGatewayIP);
                                }
                            }, Tracers.Misc);
                        }

                        var gateway = (gatewayIPAddresses.Count > 0) ?
                        string.Join(",", gatewayIPAddresses.OrderBy(x => x.Length)) : string.Empty;

                        var ipv4Addresses = GetIPAddressofNic(nicName);

                        var netMasks = new List<NetMask>();
                        foreach (var unicIPInfo in ipProperties.UnicastAddresses)
                        {
                            TaskUtils.RunAndIgnoreException(() =>
                            {
                                var currUnicIP =
                                RemoveSpecialCharsFromIPV6Address(unicIPInfo.Address.ToString());

                                var currUnicIPv4Mask = unicIPInfo.IPv4Mask.ToString();

                                if (!string.IsNullOrWhiteSpace(currUnicIP) &&
                                !string.IsNullOrWhiteSpace(currUnicIPv4Mask))
                                {
                                    netMasks.Add(new NetMask
                                    {
                                        NicIPAddress = currUnicIP,
                                        SubNetMask = currUnicIPv4Mask
                                    });
                                }
                            }, Tracers.Misc);
                        }

                        networkInfo[nicName] = new NicInfo
                        {
                            HostId = PSInstallationInfo.Default.GetPSHostId(),
                            DeviceName = nicName,
                            DeviceType = DeviceType.Normal,
                            NicSpeed = string.Empty,
                            Dns = dns,
                            Gateway = gateway,
                            NetMask = netMasks,
                            IpAddresses = ipv4Addresses
                        };
                    }, Tracers.Misc);
                }
            }, Tracers.Misc);

            return networkInfo;
        }

        private static string RemoveSpecialCharsFromIPV6Address(string ipAddress)
        {
            if (string.IsNullOrWhiteSpace(ipAddress))
            {
                return null;
            }

            // Below code is to remove the special characters from ipv6 gateway address
            if (ipAddress.Contains(":") && ipAddress.Contains("%"))
            {
                ipAddress = ipAddress.Split('%')[0];
            }

            return ipAddress;
        }

        public static IPAddress GetPSIPAddressForRegistration()
        {
            IPAddress ipAddress = IPAddress.None;

            TaskUtils.RunAndIgnoreException(() =>
            {
                // Get the NIC details of BPM_NTW_DEVICE defined in amethyst.conf
                var networkInterfaces = NetworkInterface.GetAllNetworkInterfaces().
                Where(nic => nic.Description == PSInstallationInfo.Default.GetNicFromAmethystConf());

                // If there is no result for the given BPM device, get all the NICS details
                if (networkInterfaces.Count() == 0)
                {
                    networkInterfaces = NetworkInterface.GetAllNetworkInterfaces();
                }

                // Get Valid IPV4 Addresses
                var ipAddressList = networkInterfaces.
                    SelectMany(currNic => currNic.GetIPProperties().UnicastAddresses).
                    Where(currUnicAddr =>
                        currUnicAddr.DuplicateAddressDetectionState == DuplicateAddressDetectionState.Preferred &&
                        currUnicAddr.Address.AddressFamily == AddressFamily.InterNetwork &&
                        currUnicAddr.Address != IPAddress.None &&
                        !IPAddress.IsLoopback(currUnicAddr.Address)).
                    Select(currUnicAddr => currUnicAddr.Address);

                ipAddressList = ipAddressList?.Distinct().ToList();

                if (ipAddressList != null && ipAddressList.Count() > 0)
                {
                    // If the NIC IP matches with that of amethyst use the same otherwise pick the first ip
                    ipAddress = ipAddressList.SingleOrDefault(ip => ip.Equals(PSInstallationInfo.Default.GetPSIPAddress()));

                    ipAddress = ipAddress ?? ipAddressList.FirstOrDefault();
                }

            }, Tracers.Misc);

            return ipAddress;
        }

        public static List<IPAddress> GetIPAddressofNic(string NicName)
        {
            List<IPAddress> ipAddressList = new List<IPAddress>();

            TaskUtils.RunAndIgnoreException(() =>
            {
                ipAddressList = NetworkInterface.GetAllNetworkInterfaces().
                    Where(currNic => currNic.Description == NicName).
                    SelectMany(currNic => currNic.GetIPProperties().UnicastAddresses).
                    Where(currUnicAddr =>
                        currUnicAddr.Address.AddressFamily == AddressFamily.InterNetwork &&
                        currUnicAddr.Address != IPAddress.None).
                    Select(currUnicAddr => currUnicAddr.Address).Distinct().ToList();
            }, Tracers.Misc);

            return ipAddressList;
        }

        public static Tuple<bool, string> GetProcessListeningOnTcpPort(int port, TraceSource traceSource)
        {
            int pid = 0;
            bool isUsed = false;
            string processName = string.Empty;
            NetworkApi.TcpData tcpData = null;

            var retVal = TaskUtils.RunAndIgnoreException(() =>
            {
                tcpData = NetworkApi.GetListeningTCPConnections(true).Where(currItem => currItem != null && currItem.LocalEndPoint.Port == port).FirstOrDefault();

                if (tcpData != null)
                {
                    isUsed = true;
                    pid = tcpData.ProcessId;
                    processName = tcpData.ProcessName;
                }
            }, traceSource);

            isUsed = (retVal && !isUsed) ? false : true;

            traceSource?.TraceAdminLogV2Message(TraceEventType.Information,
                                    "Port {0} is {1} {2} {3}",
                                    port,
                                    isUsed ? "used by" : "not in use",
                                    processName,
                                    isUsed ? pid.ToString() : string.Empty);

            return new Tuple<bool, string>(isUsed, processName);
        }

        public static void SetConnectivityModeAndSecurityProtocol(TraceSource traceSource)
        {
            traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Default Azure Service Bus Options, " +
                    "ConnectivityMode : {0}, SecurityProtocol : {1}",
                    ServiceBusEnvironment.SystemConnectivity.Mode.ToString(),
                    ServicePointManager.SecurityProtocol.ToString());

            TaskUtils.RunAndIgnoreException(() =>
            {
                // Setting ServiceBusConnectivityMode - Https as default connecvitiy mode.
                // For RCM Mode, setting SecurityProtocol - TLS12 as default vaule for network communications.
                // if RestWrapper_SetAzureServiceBusOptions flag is set, using the configured SecurityProtocol
                ServiceBusEnvironment.SystemConnectivity.Mode = ConnectivityMode.Https;
                if (PSInstallationInfo.Default.CSMode == CSMode.Rcm ||
                     Settings.Default.RestWrapper_SetAzureServiceBusOptions)
                {
                    SecurityProtocolType securityProtocolType;
                    if (Enum.TryParse(Settings.Default.RestWrapper_SecurityProtocol, out securityProtocolType))
                    {
                        ServicePointManager.SecurityProtocol = securityProtocolType;
                    }
                    else
                    {
                        traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Failed to parse {0}, hence setting {1} as securityprotocol",
                            Settings.Default.RestWrapper_SecurityProtocol, SecurityProtocolType.Tls12);
                    }

                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Modified Azure Service Bus Options," +
                        " ConnectivityMode : {0}, SecurityProtocol : {1}",
                        ServiceBusEnvironment.SystemConnectivity.Mode.ToString(),
                        ServicePointManager.SecurityProtocol.ToString());
                }
            }, traceSource);
        }

        public static bool IsCumulativeThrottled(decimal throttleLimit, string dir, TraceSource traceSource)
        {
            bool isCumulativeThrottled = false;

            TaskUtils.RunAndIgnoreException(() =>
            {
                double diskSpaceThreshold = (double)throttleLimit;
                double freeSpacePerct = FSUtils.GetFreeSpace(
                    dir,
                    out ulong freeBytes,
                    out ulong totalSpace);

                if (freeSpacePerct < 0D || freeSpacePerct > 100D)
                {
                    throw new Exception("Unable to get free disk space");
                }

                double usedSpacePerct = 100 - freeSpacePerct;

                if (usedSpacePerct >= diskSpaceThreshold)
                {
                    isCumulativeThrottled = true;
                    traceSource?.TraceAdminLogV2Message(
                       TraceEventType.Information,
                       "Dir : {0}, Cumulative throttle state : {1}, TotalSpace : {2}" +
                       "Used Space Perct : {3}, CumulativeThrottleLimit : {4}",
                       dir, isCumulativeThrottled, totalSpace, usedSpacePerct, throttleLimit);
                }
            }, traceSource);

            return isCumulativeThrottled;
        }
    }
}
