using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class PSFailoverRequest : ICXRequest
    {
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public PSFailoverRequest(FailoverType failoverType, string oldProcessServerId, string newProcessServerId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionPSFailover;
            Request.Include = true;
            Request.AddParameter("FailoverType", failoverType.ToString());
            if (!String.IsNullOrEmpty(oldProcessServerId))
            {
                Request.AddParameter("OldProcessServer", oldProcessServerId);
            }
            Request.AddParameter("NewProcessServer", newProcessServerId);
        }

        public void AddServer(string hostId, string newProcessServerId)
        {
            if(servers == null)
            {
                servers = new ParameterGroup("Servers");
                Request.AddParameterGroup(servers);
            }
            ParameterGroup serverParamGroup = new ParameterGroup("Server" + (servers.ChildList.Count + 1));
            serverParamGroup.AddParameter("HostId", hostId);
            serverParamGroup.AddParameter("NewProcessServer", newProcessServerId);
            servers.AddParameterGroup(serverParamGroup);
        }
    }
}
