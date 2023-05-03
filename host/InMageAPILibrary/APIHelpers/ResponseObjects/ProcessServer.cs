using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class ProcessServer 
    {
        public ScoutComponentType ComponentType { get; private set; }
        public string HostId { get; private set; }
        public string HostName { get; private set; }
        public IPAddress IPAddress { get; private set; }
        public OSType OSType { get; private set; }
        public IPAddress PSNatIP { get; private set; }
        public string Version { get; private set; }
        public string HeartBeat { get; private set; }
        public Dictionary<string, DateTime?> InstalledUpdates { get; private set; }

        public ProcessServer(ParameterGroup processServer)
        {
            this.ComponentType = Utils.ParseEnum<ScoutComponentType>(processServer.GetParameterValue("ComponentType"));
            this.HostId = processServer.GetParameterValue("HostId");
            this.HostName = processServer.GetParameterValue("HostName");
            this.IPAddress = Utils.ParseIP(processServer.GetParameterValue("IPAddress"));
            this.OSType = Utils.ParseEnum<OSType>(processServer.GetParameterValue("OSType"));
            this.PSNatIP = Utils.ParseIP(processServer.GetParameterValue("PSNatIP"));
            this.Version = processServer.GetParameterValue("Version");
            this.HeartBeat = processServer.GetParameterValue("HeartBeat");

            ParameterGroup updates = processServer.GetParameterGroup("PSUpdateHistory");
            ParameterGroup updateDetails;
            string updateVersion;
            if(updates != null)
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
        }
    }
}
