using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXRollbackReadinessCheckRequest : ICXRequest
    {
        // Local Parameters        
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }
        
        public CXRollbackReadinessCheckRequest()
        {
            InitializeRequest(true, RecoveryPoint.LatestTag, null, null);
        }

        public CXRollbackReadinessCheckRequest(bool multiVMSyncPointEnabled, RecoveryPoint recoveryPoint, string tagName)
        {
            InitializeRequest(multiVMSyncPointEnabled, recoveryPoint, tagName, null);
        }

        public CXRollbackReadinessCheckRequest(bool multiVMSyncPointEnabled, RecoveryPoint recoveryPoint, string tagName, string protectionPlanId)
        {
            InitializeRequest(multiVMSyncPointEnabled, recoveryPoint, tagName, protectionPlanId);
        }

        public void AddServer(string hostId)
        {
            if (servers == null)
            {
                servers = new ParameterGroup("Servers");
                Request.AddParameterGroup(servers);
            }
            servers.AddParameter("HostId" + (servers.ChildList.Count + 1).ToString(), hostId);
        }

        public void AddServers(string[] hostIds)
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
                    servers.AddParameter("HostId" + (servers.ChildList.Count + 1).ToString(), hostId);
                }
            }
        }

        private void InitializeRequest(bool multiVMSyncPointEnabled, RecoveryPoint recoveryPoint, string tagName, string protectionPlanId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionRollbackReadinessCheck;
            Request.Include = true;
            Request.AddParameter("MultiVMSyncPoint", multiVMSyncPointEnabled ? "Yes" : "No");
            Request.AddParameter("RecoveryPoint", recoveryPoint.ToString());
            if (!String.IsNullOrEmpty(tagName))
            {
                Request.AddParameter("TagName", tagName.ToString());
            }
            if (!String.IsNullOrEmpty(protectionPlanId))
            {
                Request.AddParameter("ProtectionPlanId", protectionPlanId);
            }            
        }
    }
}
