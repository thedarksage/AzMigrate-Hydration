using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class Infrastructure
    {
        public string InfrastructureId { get; private set; }
        public InfrastructureType DiscoveryType { get; private set; }
        public IPAddress Reference { get; private set; }
        public OperationStatus DiscoveryStatus { get; private set; }
        public DateTime? LastDiscoveryTime { get; private set; }
        public List<HostServer> PhysicalServers { get; private set; }
        public List<HostServer> VirtualizedHosts { get; private set; }
        public string DiscoveryErrorLog { get; private set; }
        public ErrorDetails DiscoveryErrorDetails { get; private set; }

        public Infrastructure(ParameterGroup infrastructure)
        {
            this.InfrastructureId = infrastructure.GetParameterValue("InfrastructureId");
            this.DiscoveryType = Utils.ParseEnum<InfrastructureType>(infrastructure.GetParameterValue("DiscoveryType"));
            this.Reference = Utils.ParseIP(infrastructure.GetParameterValue("Reference"));
            this.DiscoveryStatus = Utils.ParseEnum<OperationStatus>(infrastructure.GetParameterValue("DiscoveryStatus"));
            this.LastDiscoveryTime = Utils.UnixTimeStampToDateTime(infrastructure.GetParameterValue("LastDiscoveryTime"));
            this.DiscoveryErrorLog = infrastructure.GetParameterValue("DiscoveryErrorLog");

            ParameterGroup discoveryErrorDetailsParamGroup = infrastructure.GetParameterGroup("ErrorDetails");
            if(discoveryErrorDetailsParamGroup != null && discoveryErrorDetailsParamGroup.ChildList != null)
            {
                DiscoveryErrorDetails = new ErrorDetails(discoveryErrorDetailsParamGroup);
            }

            ParameterGroup discoveredServers = infrastructure.GetParameterGroup("DiscoveredServers");
            if (DiscoveryType == InfrastructureType.Physical)
            {
                if (discoveredServers != null && discoveredServers.ChildList != null)
                {
                    this.PhysicalServers = new List<HostServer>();
                    foreach (var item in discoveredServers.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            this.PhysicalServers.Add(new HostServer(item as ParameterGroup));
                        }
                    }
                }
            }
            else
            {
                if (discoveredServers != null && discoveredServers.ChildList != null)
                {
                    this.VirtualizedHosts = new List<HostServer>();
                    foreach (var item in discoveredServers.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            this.VirtualizedHosts.Add(new HostServer(item as ParameterGroup));
                        }
                    }
                }
            }
        }
    }
}
