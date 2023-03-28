using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXListConsistencyPointsRequest : ICXRequest
    {
        // Local Parameters        
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXListConsistencyPointsRequest(DateTime tagStartDate, DateTime tagEndDate)
        {
            InitializeRequest(null, tagStartDate, tagEndDate);
        }

        public CXListConsistencyPointsRequest(string protectionPlanId, DateTime tagStartDate, DateTime tagEndDate)
        {
            InitializeRequest(protectionPlanId, tagStartDate, tagEndDate);
        }

        public void SetConsistencyOptions(string consistencyApplication, bool userDefinedEvent, string tagName, 
            ConsistencyTagAccuracy tagAccuracy, bool getRecentTagEnabled, bool multiVMSyncPointEnabled)
        {
            if(!String.IsNullOrEmpty(consistencyApplication))
            {
                Request.AddParameter("ApplicationName" , consistencyApplication);          
            }
            Request.AddParameter("UserDefinedEvent", userDefinedEvent ? "Yes" : "No");
            if(!String.IsNullOrEmpty(tagName))
            {
                Request.AddParameter("TagName", tagName);
            }
            Request.AddParameter("Accuracy", tagAccuracy.ToString());
            Request.AddParameter("GetRecentConsistencyPoint", getRecentTagEnabled ? "Yes" : "No");
            Request.AddParameter("MultiVMSyncPoint", multiVMSyncPointEnabled ? "Yes" : "No");
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

        private void InitializeRequest(string protectionPlanId, DateTime tagStartDate, DateTime tagEndDate)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionListConsistencyPoints;
            Request.Include = true;
            if (!String.IsNullOrEmpty(protectionPlanId))
            {
                Request.AddParameter("ProtectionPlanId", protectionPlanId);
            }            
            Request.AddParameter("TagStartDate", Utils.DateTimeToUnixTimeStamp(tagStartDate).ToString());
            Request.AddParameter("TagEndDate", Utils.DateTimeToUnixTimeStamp(tagEndDate).ToString());
        }
    }
}
