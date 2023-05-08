using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXDiscoverInfrastructureRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup infrastructures;        

        public FunctionRequest Request { get; set; }

        public CXDiscoverInfrastructureRequest()
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionInfrastructureDiscovery;
            Request.Include = true;
            infrastructures = new ParameterGroup("Infrastructures");
            Request.AddParameterGroup(infrastructures);
        }

		public void AddVCenterInfrastructure(string siteName, IPAddress serverIp, string loginId, string password, string discoveryAgent)
        {
            AddVCenterInfrastructure(siteName, serverIp, loginId, password, discoveryAgent, InfrastructureSource.Primary);
        }

        public void AddVCenterInfrastructure(string siteName, IPAddress serverIp, string accountId, string discoveryAgent, InfrastructureSource infraSource)
        {
            ParameterGroup vCenterDetails = BuildVCenterDetailsParameterGroup(siteName, serverIp, discoveryAgent, infraSource);
            vCenterDetails.AddParameter("AccountId", accountId);
            infrastructures.AddParameterGroup(vCenterDetails);
        }
		
        public void AddVCenterInfrastructure(string siteName, IPAddress serverIp, string loginId, string password, string discoveryAgent, InfrastructureSource infraSource)
        {
            ParameterGroup vCenterDetails = BuildVCenterDetailsParameterGroup(siteName, serverIp, discoveryAgent, infraSource);
            vCenterDetails.AddParameter("LoginId", loginId);
            vCenterDetails.AddParameter("Password", password);
            infrastructures.AddParameterGroup(vCenterDetails);
        }
		
        public void AddPhysicalInfrastructure(string siteName, IPAddress serverIp, string hostName, string operatingSystem)
        {
			AddPhysicalInfrastructure(siteName, serverIp, hostName, operatingSystem, InfrastructureSource.Primary);
		}

        public void AddPhysicalInfrastructure(string siteName, IPAddress serverIp, string hostName, string operatingSystem, InfrastructureSource infraSource)
        {
            string infraId = "Infrastructure" + (infrastructures.ChildList.Count + 1);
            ParameterGroup vCenterDetails = new ParameterGroup(infraId);
            if (!String.IsNullOrEmpty(siteName))
            {
                vCenterDetails.AddParameter("SiteName", siteName);
            }
            vCenterDetails.AddParameter("InfrastructureType", infraSource.ToString());
            vCenterDetails.AddParameter("DiscoveryType", InfrastructureType.Physical.ToString());
            vCenterDetails.AddParameter("ServerIP", serverIp.ToString());
            if (!String.IsNullOrEmpty(hostName))
            {
                vCenterDetails.AddParameter("HostName", hostName);
            }
            vCenterDetails.AddParameter("OS", operatingSystem);

            infrastructures.AddParameterGroup(vCenterDetails);            
        }

        private ParameterGroup BuildVCenterDetailsParameterGroup(string siteName, IPAddress serverIp, string discoveryAgent, InfrastructureSource infraSource)
        {
            string infraId = "Infrastructure" + (infrastructures.ChildList.Count + 1);
            ParameterGroup vCenterDetails = new ParameterGroup(infraId);
            if (!String.IsNullOrEmpty(siteName))
            {
                vCenterDetails.AddParameter("SiteName", siteName);
            }
            vCenterDetails.AddParameter("InfrastructureType", infraSource.ToString());
            vCenterDetails.AddParameter("DiscoveryType", InfrastructureType.vCenter.ToString());
            vCenterDetails.AddParameter("ServerIP", serverIp.ToString());
            vCenterDetails.AddParameter("DiscoveryAgent", discoveryAgent);
            return vCenterDetails;
        }
    }
}
