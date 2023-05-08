using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXListScoutComponentRequest : ICXRequest
    {
        // Local Parameters
        private bool hasSetComponentType;
        private bool hasSetOSType;
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXListScoutComponentRequest()
        {
            InitializeRequest();
        }

        public CXListScoutComponentRequest(ScoutComponentType componentType)
        {
            InitializeRequest();
            Request.AddParameter("ComponentType", GetComponentTypeRequestString(componentType));
            hasSetComponentType = true;
        }        

        public CXListScoutComponentRequest(ScoutComponentType componentType, OSType osType)
        {
            InitializeRequest();
            Request.AddParameter("ComponentType", GetComponentTypeRequestString(componentType));
            Request.AddParameter("OSType", osType.ToString());
            hasSetComponentType = true;
            hasSetOSType = true;
        }

        public CXListScoutComponentRequest(ScoutComponentType componentType, OSType osType, List<string> serverIds)
        {
            InitializeRequest();
            Request.AddParameter("ComponentType", GetComponentTypeRequestString(componentType));
            hasSetComponentType = true;
            Request.AddParameter("OSType", osType.ToString());
            hasSetOSType = true;
            servers = new ParameterGroup("Servers");
            Request.AddParameterGroup(servers);
            int i = 1;
            foreach (var id in serverIds)
            {
                servers.AddParameter("HostId" + i.ToString(), id);
                ++i;
            }
        }

        public void SetOSType(OSType osType)
        {
            if (hasSetOSType)
            {
                Request.RemoveParameter("OSType");
            }
            else
            {
                hasSetOSType = true;
            }
            Request.AddParameter("OSType", osType.ToString());
        }

        public void SetComponentType(ScoutComponentType componentType)
        {
            if (hasSetComponentType)
            {
                Request.RemoveParameter("ComponentType");
            }
            else
            {
                hasSetComponentType = true;
            }
            Request.AddParameter("ComponentType", GetComponentTypeRequestString(componentType));
        }

        public void AddServer(string serverId)
        {
            if(servers == null)
            {
                servers = new ParameterGroup("Servers");
                Request.AddParameterGroup(servers);
            }
            servers.AddParameter("HostId" + (servers.ChildList.Count + 1), serverId);
        }

        private void InitializeRequest()
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionListScoutComponents;
        }

        private string GetComponentTypeRequestString(ScoutComponentType type)
        {
            switch (type)
            {
                case ScoutComponentType.All:
                    return "All";
                case ScoutComponentType.MasterTarget:
                    return "MasterTargets";
                case ScoutComponentType.ProcessServer:
                    return "ProcessServers";
                case ScoutComponentType.ScoutAgent:
                    return "SourceServers";
                default:
                    return "All";
            }
        }
    }
}
