using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class ScoutAgent
    {
        public string InfrastructureVMId { get; private set; }
        public string ResourceId { get; private set; }
        public ScoutComponentType ComponentType { get; private set; }
        public string HostId { get; private set; }
        public string HostName { get; private set; }
        public IPAddress IPAddress { get; private set; }
        public OSType OSType { get; private set; }
        public string OSVersion { get; private set; }
        public MachineType ServerType { get; private set; }
        public string Vendor { get; private set; }
        public string Version { get; private set; }
        public string HeartBeat { get; private set; }
        public bool IsProtected { get; private set; }
        public List<DiskDetail> DiskDetails { get; private set; }
        public List<NetworkConfiguration> NetworkConfigurations { get; private set; }
        public HardwareConfiguration HardwareConfiguration { get; private set; }
        public Dictionary<string, DateTime?> InstalledUpdates { get; private set; }

        public ScoutAgent(ParameterGroup scoutAgent)
        {
            this.InfrastructureVMId = scoutAgent.GetParameterValue("InfrastructureVMId");
            this.ResourceId = scoutAgent.GetParameterValue("ResourceId");
            this.ComponentType = Utils.ParseEnum<ScoutComponentType>(scoutAgent.GetParameterValue("ComponentType"));
            this.HostId = scoutAgent.GetParameterValue("HostId");
            this.HostName = scoutAgent.GetParameterValue("HostName");
            this.IPAddress = Utils.ParseIP(scoutAgent.GetParameterValue("IPAddress"));
            this.OSType = Utils.ParseEnum<OSType>(scoutAgent.GetParameterValue("OSType"));
            this.OSVersion = scoutAgent.GetParameterValue("OSVersion");
            this.ServerType = Utils.ParseEnum<MachineType>(scoutAgent.GetParameterValue("ServerType"));
            this.Vendor = scoutAgent.GetParameterValue("Vendor");
            this.Version = scoutAgent.GetParameterValue("Version");

            ParameterGroup updates = scoutAgent.GetParameterGroup("AgentUpdateHistory");
            ParameterGroup updateDetails;
            string updateVersion;
            if (updates != null)
            {
                InstalledUpdates = new Dictionary<string, DateTime?>();
                foreach (var item in updates.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        updateDetails = item as ParameterGroup;
                        updateVersion = updateDetails.GetParameterValue("Version");
                        if (!String.IsNullOrEmpty(updateVersion))
                        {
                            InstalledUpdates[updateVersion] = Utils.UnixTimeStampToDateTime(updateDetails.GetParameterValue("InstallationTimeStamp"));
                        }
                    }
                }
            }
            this.HeartBeat = scoutAgent.GetParameterValue("HeartBeat");
            this.IsProtected = (scoutAgent.GetParameterValue("isProtected") == "Yes") ? true : false;
            ParameterGroup diskDetails = scoutAgent.GetParameterGroup("DiskDetails");
            if (diskDetails != null && diskDetails.ChildList != null && diskDetails.ChildList.Count > 0)
            {
                DiskDetails = new List<DiskDetail>();
                foreach (var item in diskDetails.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        DiskDetails.Add(new DiskDetail(item as ParameterGroup));
                    }
                }
            }

            ParameterGroup networkConfigurations = scoutAgent.GetParameterGroup("NetworkConfiguration");
            if (networkConfigurations != null && networkConfigurations.ChildList != null && networkConfigurations.ChildList.Count > 0)
            {
                NetworkConfigurations = new List<NetworkConfiguration>();
                foreach (var item in networkConfigurations.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        NetworkConfigurations.Add(new NetworkConfiguration(item as ParameterGroup));
                    }
                }
            }

            ParameterGroup hardwareConfiguration = scoutAgent.GetParameterGroup("HardwareConfiguration");
            if (hardwareConfiguration != null)
            {
                HardwareConfiguration = new HardwareConfiguration(hardwareConfiguration);
            }
        }
    }
}
