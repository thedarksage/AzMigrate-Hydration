using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXUnInstallMobilityServiceRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXUnInstallMobilityServiceRequest()
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionUnInstallMobilityService;
            Request.Include = true;
            servers = new ParameterGroup("Servers");
            Request.AddParameterGroup(servers);
        }

        public void AddServer(string hostId, string userName, string password, string pushServerId)
        {
            ParameterGroup server = BuildServerParameterGroup(hostId, pushServerId);
            if (!String.IsNullOrEmpty(userName))
            {
                server.AddParameter("UserName", userName);
            }
            if (!String.IsNullOrEmpty(password))
            {
                server.AddParameter("Password", password);
            }           
            servers.AddParameterGroup(server);
        }

        public void AddServer(string hostId, string accountId, string pushServerId)
        {
            ParameterGroup server = BuildServerParameterGroup(hostId, pushServerId);
            server.AddParameter("AccountId", accountId);
            servers.AddParameterGroup(server);
        }

        private ParameterGroup BuildServerParameterGroup(string hostId, string pushServerId)
        {
            ParameterGroup server = new ParameterGroup("Server" + servers.ChildList.Count + 1);
            server.AddParameter("HostId", hostId);            
            if (!String.IsNullOrEmpty(pushServerId))
            {
                server.AddParameter("PushServerId", pushServerId);
            }

            return server;
        }
    }
}
