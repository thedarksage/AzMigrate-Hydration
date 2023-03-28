using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXModifyProtectionRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup servers;

        public FunctionRequest Request { get; set; }

        public CXModifyProtectionRequest(string protectionPlanId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionModifyProtection;
            Request.Include = true;
            Request.AddParameter("ProtectionPlanId", protectionPlanId);
            servers = new ParameterGroup("Servers");
            Request.AddParameterGroup(servers);
        }

        public void AddServerForProtection(string hostId, string processServerID, ProtectionComponents useNatIPFor, string masterTargetID, string retentionDrive, List<DiskMappingForProtection> diskMappings)
        {
            ParameterGroup server = new ParameterGroup("Server" + (servers.ChildList.Count + 1));
            servers.AddParameterGroup(server);
            server.AddParameter("HostId", hostId);
            server.AddParameter("ProcessServer", processServerID);
            server.AddParameter("MasterTarget", masterTargetID);
            server.AddParameter("RetentionDrive", retentionDrive);
            server.AddParameter("UseNatIPFor", useNatIPFor.ToString());
            ParameterGroup diskMappingPG = new ParameterGroup("DiskMapping");
            server.AddParameterGroup(diskMappingPG);
            int diskMappingCount = 1;
            ParameterGroup diskMap;
            foreach (var item in diskMappings)
            {
                diskMap = new ParameterGroup("Disk" + diskMappingCount.ToString());
                diskMap.AddParameter("SourceDiskName", item.SourceDiskName);
                diskMap.AddParameter("TargetLUNId", item.TargetLUNId);
                diskMappingPG.AddParameterGroup(diskMap);
                ++diskMappingCount;
            }
        }
    }
}
