using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXSetVPNDetailsRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup servers;        
                
        public FunctionRequest Request { get; set; }

        public CXSetVPNDetailsRequest(ConnectivityType connectivityType)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionSetVPNDetails;
            Request.AddParameter("ConnectivityType", GetConnectivityTypeString(connectivityType));
            servers = new ParameterGroup("Servers");
            Request.AddParameterGroup(servers);
        }

        public void AddServer(string hostId, IPAddress privateIP, string privatePort, string privateSSLPort,
            IPAddress publicIP, string publicPort, string publicSSLPort)
        {
            ParameterGroup server = new ParameterGroup("Server" + (servers.ChildList.Count + 1));            
            server.AddParameter("HostId", hostId);
            if (privateIP != null)
            {
                server.AddParameter("PrivateIP", privateIP.ToString());
            }
            if (!String.IsNullOrEmpty(privatePort))
            {
                server.AddParameter("PrivatePort", privatePort);
            }
            if (!String.IsNullOrEmpty(privateSSLPort))
            {
                server.AddParameter("PrivateSSLPort", privateSSLPort);
            }
            if (publicIP != null)
            {
                server.AddParameter("PublicIP", publicIP.ToString());
            }
            if (!String.IsNullOrEmpty(publicPort))
            {
                server.AddParameter("PublicPort", publicPort);
            }
            if (!String.IsNullOrEmpty(publicSSLPort))
            {
                server.AddParameter("PublicSSLPort", publicSSLPort);
            }
            Request.AddParameterGroup(server);
            servers.AddParameterGroup(server);
        }

        public static string GetConnectivityTypeString(ConnectivityType connectivityType)
        {
            string connectivityTypeString;
            switch (connectivityType)
            {
                case ConnectivityType.VPN:
                    connectivityTypeString = "VPN";
                    break;
                case ConnectivityType.NonVPN:
                    connectivityTypeString = "Non VPN";
                    break;
                default:
                    connectivityTypeString = String.Empty;
                    break;
            }
            return connectivityTypeString;
        }                
    }
}
