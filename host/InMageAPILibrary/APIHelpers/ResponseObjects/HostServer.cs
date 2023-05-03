using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class HostServer
    {
        public string Id { get; private set; }
        public InfrastructureType DiscoveryType { get; private set; }
        public IPAddress ServerIP { get; private set; }
        public string ServerName { get; private set; }
        public string Organization { get; private set; }
        public string LastDiscoveryTime { get; private set; }
        public string DiscoveryErrorLog { get; private set; }
        public List<SourceServer> Servers { get; private set; }

        public HostServer(ParameterGroup discoveredServer)
        {
            this.Id = discoveredServer.Id;
            this.DiscoveryType = Utils.ParseEnum<InfrastructureType>(discoveredServer.GetParameterValue("DiscoveryType"));
            this.ServerIP = Utils.ParseIP(discoveredServer.GetParameterValue("ServerIP"));
            this.ServerName = discoveredServer.GetParameterValue("ServerName");
            this.Organization = discoveredServer.GetParameterValue("Organization");
            this.LastDiscoveryTime = discoveredServer.GetParameterValue("LastDiscoveryTime");
            this.DiscoveryErrorLog = discoveredServer.GetParameterValue("DiscoveryErrorLog");

            if (discoveredServer.ChildList != null)
            {
                this.Servers = new List<SourceServer>();
                foreach (var item in discoveredServer.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        this.Servers.Add(new SourceServer(item as ParameterGroup));
                    }
                }
            }
        }
    }
}
