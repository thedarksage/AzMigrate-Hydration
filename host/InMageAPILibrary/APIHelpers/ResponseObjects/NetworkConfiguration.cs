using InMage.APIModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;

namespace InMage.APIHelpers
{
    public class NetworkConfiguration
    {
        public string Id { get; private set; }
        public string InterfaceName { get; private set; }
        public IPAddress IPAddress { get; private set; }
        public string MACAddress { get; private set; }
        public IPAddress Subnet { get; private set; }
        public IPAddress Gateway { get; private set; }
        public IPAddress[] DNS { get; private set; }
        public bool IsDHCPEnabled { get; private set; }

        public NetworkConfiguration(ParameterGroup networkConfiguration)
        {
            this.Id = networkConfiguration.Id;            
            this.InterfaceName = networkConfiguration.GetParameterValue("InterfaceName");
            this.IPAddress = Utils.ParseIP(networkConfiguration.GetParameterValue("IPAddress"));
            this.MACAddress = networkConfiguration.GetParameterValue("MACAddress");
            this.Subnet = Utils.ParseIP(networkConfiguration.GetParameterValue("SubnetMask"));
            this.Gateway = Utils.ParseIP(networkConfiguration.GetParameterValue("Gateway"));
            string value = networkConfiguration.GetParameterValue("DNS");
            if (!String.IsNullOrEmpty(value))
            {
                string[] dnsIPs = value.Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
                if (dnsIPs != null)
                {
                    DNS = new IPAddress[dnsIPs.Length];
                    for (int index = 0; index < dnsIPs.Length; index++ )
                    {
                        this.DNS[index] = Utils.ParseIP(dnsIPs[index]);
                    }
                }
            }
            this.IsDHCPEnabled = (networkConfiguration.GetParameterValue("isDHCPEnabled") == "Yes") ? true : false;
        }
    }
}
