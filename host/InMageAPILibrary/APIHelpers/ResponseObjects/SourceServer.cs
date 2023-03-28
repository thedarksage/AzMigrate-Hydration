using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class SourceServer
    {
        public string Id { get; private set; }
        // This property is used only for Virtual server
        public string InfrastructureVMId { get; private set; }
        public IPAddress ServerIP { get; private set; }
        public string DisplayName { get; private set; }
        public OSType OS { get; private set; }
        public string PoweredOn { get; private set; }
        public string AgentInstalled { get; private set; }
        public string Protected { get; private set; }

        public SourceServer(ParameterGroup sourceServer)
        {
            this.Id = sourceServer.Id;
            this.InfrastructureVMId = sourceServer.GetParameterValue("InfrastructureVMId");
            this.ServerIP = Utils.ParseIP(sourceServer.GetParameterValue("ServerIP"));
            this.DisplayName = sourceServer.GetParameterValue("DisplayName");
            this.OS = Utils.ParseEnum<OSType>(sourceServer.GetParameterValue("OS"));
            this.PoweredOn = sourceServer.GetParameterValue("PoweredOn");
            this.AgentInstalled = sourceServer.GetParameterValue("AgentInstalled");
            this.Protected = sourceServer.GetParameterValue("Protected");
        }
    }
}
