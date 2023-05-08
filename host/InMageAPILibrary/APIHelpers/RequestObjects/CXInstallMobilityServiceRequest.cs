using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXInstallMobilityServiceRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXInstallMobilityServiceRequest(string pushServerId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionInstallMobilityService;
            Request.Include = true;
            Request.AddParameter("PushServerId", pushServerId);
            servers = new ParameterGroup("Servers");           
            Request.AddParameterGroup(servers);
        }

        public void SetConfigurationServerDetails(string cxip, int port, CXProtocol protocol)
        {
            ParameterGroup configServer = new ParameterGroup("ConfigServer");
            configServer.AddParameter("IPAddress", cxip);
            configServer.AddParameter("Port", port.ToString());
            configServer.AddParameter("Protocol", protocol.ToString());
            Request.AddParameterGroup(configServer);
        }

        public void AddServer(string hostIP, string hostName, string displayName, OSType osType, 
            string domain, string userName, string password, ScoutComponentType agentType)
        {
            ParameterGroup server = BuildServerParameterGroup(hostIP, hostName, displayName, osType, agentType);
            if (!String.IsNullOrEmpty(domain))
            {
                server.AddParameter("Domain", domain);
            }
            server.AddParameter("UserName", userName);
            server.AddParameter("Password", password);
            servers.AddParameterGroup(server);
        }   

        public void AddServer(string hostIP, string hostName, string displayName, OSType osType, 
            string accountId, ScoutComponentType agentType)
        {
            ParameterGroup server = BuildServerParameterGroup(hostIP, hostName, displayName, osType, agentType);
            server.AddParameter("AccountId", accountId);                        
            servers.AddParameterGroup(server);
        }

        private ParameterGroup BuildServerParameterGroup(string hostIP, string hostName, string displayName, OSType osType,
            ScoutComponentType agentType)
        {
            ParameterGroup server = new ParameterGroup("Server" + servers.ChildList.Count + 1);
            server.AddParameter("HostIP", hostIP);
            if (!String.IsNullOrEmpty(hostName))
            {
                server.AddParameter("HostName", hostName);
            }
            if (!String.IsNullOrEmpty(displayName))
            {
                server.AddParameter("DisplayName", displayName);
            }
            server.AddParameter("OS", osType.ToString());            
            server.AddParameter("AgentType", agentType.ToString());
            return server;
        }
    }
}
