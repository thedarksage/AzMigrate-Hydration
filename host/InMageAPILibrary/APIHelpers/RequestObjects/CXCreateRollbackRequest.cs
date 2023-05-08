using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXCreateRollbackRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

		public CXCreateRollbackRequest()
        {
			InitializeRequest(null, null, null);
		}

        public CXCreateRollbackRequest(string rollbackPlanId)
        {
            InitializeRequest(rollbackPlanId, null, null);
        }

        public CXCreateRollbackRequest(string rollbackPlanId, string rollbackPlanName)
        {
            InitializeRequest(rollbackPlanId, rollbackPlanName, null);
		}

        public CXCreateRollbackRequest(string rollbackPlanId, string rollbackPlanName, string protectionPlanId)
        {
            InitializeRequest(rollbackPlanId, rollbackPlanName, protectionPlanId);
		}        

        public void AddServerForRollback(string hostId, string tagName)
        {           
            if(servers == null)
            {
                servers = new ParameterGroup("Servers");
                Request.AddParameterGroup(servers);
            }
            ParameterGroup server = new ParameterGroup("Server" + (servers.ChildList.Count + 1).ToString());
            server.AddParameter("HostId", hostId);
            server.AddParameter("TagName", tagName);
            servers.AddParameterGroup(server);
        }

        public void SetRollbackOptions(bool multiVMSyncPointRequired, RecoveryPoint recoveryPoint, string customTagName)
        {
            Request.AddParameter("MultiVMSyncPoint", multiVMSyncPointRequired ? "Yes" : "No");
            Request.AddParameter("RecoveryPoint", recoveryPoint.ToString());
            if (!String.IsNullOrEmpty(customTagName))
            {
                Request.AddParameter("TagName", customTagName);
            }
        }

        private void InitializeRequest(string rollbackPlanId, string rollbackPlanName, string protectionPlanId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionCreateRollback;

            if (!String.IsNullOrEmpty(rollbackPlanId))
            {
                Request.AddParameter("RollbackPlanId", rollbackPlanId);
            }

            if (!String.IsNullOrEmpty(rollbackPlanName))
            {
                Request.AddParameter("RollbackPlanName", rollbackPlanName);
            }

            if (!String.IsNullOrEmpty(protectionPlanId))
            {
                Request.AddParameter("ProtectionPlanId", protectionPlanId);
            }

            Request.Include = true;            
        }
    }
}
