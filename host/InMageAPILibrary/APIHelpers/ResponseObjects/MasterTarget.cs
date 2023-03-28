using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class MasterTarget
    {
        public ScoutComponentType ComponentType { get; private set; }
        public string HostId { get; private set; }
        public string HostName { get; private set; }
        public IPAddress IPAddress { get; private set; }
        public OSType OSType { get; private set; }
        public string OSVersion { get; private set; }
        public MachineType ServerType { get; private set; }
        public string Vendor { get; private set; }
        public int ProtectedVolumesCount { get; private set; }
        public string Version { get; private set; }
        public string HeartBeat { get; private set; }
        public Dictionary<string, DateTime?> InstalledUpdates { get; private set; }
        public List<RetentionVolume> RetentionVolumes { get; private set; }

        public MasterTarget(ParameterGroup masterTarget)
        {
            this.ComponentType = Utils.ParseEnum<ScoutComponentType>(masterTarget.GetParameterValue("ComponentType"));
            this.HostId = masterTarget.GetParameterValue("HostId");
            this.HostName = masterTarget.GetParameterValue("HostName");
            this.IPAddress = Utils.ParseIP(masterTarget.GetParameterValue("IPAddress"));
            this.OSType = Utils.ParseEnum<OSType>(masterTarget.GetParameterValue("OSType"));
            this.OSVersion = masterTarget.GetParameterValue("OSVersion");
            this.ServerType = Utils.ParseEnum<MachineType>(masterTarget.GetParameterValue("ServerType"));
            this.Vendor = masterTarget.GetParameterValue("Vendor");
            int value;
            int.TryParse(masterTarget.GetParameterValue("ProtectedVolumes"), out value);
            this.ProtectedVolumesCount = value;
            this.Version = masterTarget.GetParameterValue("Version");
            this.HeartBeat = masterTarget.GetParameterValue("HeartBeat");

            ParameterGroup updates = masterTarget.GetParameterGroup("AgentUpdateHistory");
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

            ParameterGroup retentionVolumes = masterTarget.GetParameterGroup("RetentionVolumes");
            if (retentionVolumes != null && retentionVolumes.ChildList != null && retentionVolumes.ChildList.Count > 0)
            {
                RetentionVolumes = new List<RetentionVolume>();
                foreach (var item in retentionVolumes.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        RetentionVolumes.Add(new RetentionVolume(item as ParameterGroup));
                    }
                }
            }
        }
    }
}
