using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXStopProtectionRequest : ICXRequest
    {
         // Local Parameters
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXStopProtectionRequest(string protectionPlanId)
        {
            InitializeRequest(protectionPlanId, false);
        }

        public CXStopProtectionRequest(string protectionPlanId, bool forceDelete)
        {
            InitializeRequest(protectionPlanId, forceDelete);
		}

        public void AddServerForStopProtection(string hostId)
        {
            if(servers == null)
            {
                servers = new ParameterGroup("Servers");
                Request.AddParameterGroup(servers);
            }
            servers.AddParameter("Server" + (servers.ChildList.Count + 1).ToString(), hostId);
        }

        public void AddServersForStopProtection(string[] hostIds)
        {
            if (hostIds != null)
            {
                if(servers == null)
                {
                    servers = new ParameterGroup("Servers");
                    Request.AddParameterGroup(servers);
                }
                foreach (string hostId in hostIds)
                {
                    servers.AddParameter("Server" + (servers.ChildList.Count + 1).ToString(), hostId);
                }
            }
        }

        private void InitializeRequest(string protectionPlanId, bool forceDelete)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FuntionStopProtection;

            if (!string.IsNullOrEmpty(protectionPlanId))
            {
                Request.AddParameter("ProtectionPlanId", protectionPlanId);
            }

            Request.Include = true;
            Request.AddParameter("ForceDelete", forceDelete ? "1" : "0");
        }
    }
}
