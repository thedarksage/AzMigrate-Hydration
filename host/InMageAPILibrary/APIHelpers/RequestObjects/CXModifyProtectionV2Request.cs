using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXModifyProtectionV2Request : ICXRequest
    {
        // Local Parameters
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXModifyProtectionV2Request(string protectionPlanId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionModifyProtectionV2;
            Request.Include = true;
            Request.AddParameter("ProtectionPlanId", protectionPlanId);
            servers = new ParameterGroup("Servers");
            Request.AddParameterGroup(servers);
        }

        public void AddServerForProtection(string hostId, string processServerID, ProtectionComponents useNatIPFor, string masterTargetID, List<string> sourceDiskNames)
        {
            ParameterGroup server = new ParameterGroup("Server" + (servers.ChildList.Count + 1));
            servers.AddParameterGroup(server);
            server.AddParameter("HostId", hostId);
            server.AddParameter("ProcessServer", processServerID);
            server.AddParameter("MasterTarget", masterTargetID);
            server.AddParameter("UseNatIPFor", useNatIPFor.ToString());
            ParameterGroup diskDetailsPG = new ParameterGroup("DiskDetails");
            server.AddParameterGroup(diskDetailsPG);
            int diskDetailsCount = 1;
            ParameterGroup diskDet;
          
            foreach (string sourceDisk in sourceDiskNames)
            {
                diskDet = new ParameterGroup("Disk" + diskDetailsCount.ToString());
                diskDet.AddParameter("SourceDiskName", sourceDisk);
                diskDetailsPG.AddParameterGroup(diskDet);
                ++diskDetailsCount;
            }

        }
    }
}
