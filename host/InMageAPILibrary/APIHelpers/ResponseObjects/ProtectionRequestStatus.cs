using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ServerProtectionStatus
    {
        public string HostId { get; private set; }
        public OperationStatus PrepareTargetStatus { get; private set; }
        public string CreateReplicationPairsStatus { get; private set; }
        public ProtectionStatus ProtectionStage { get; private set; }
        public ErrorDetails ProtectionErrorDetails { get; private set; }

        public ServerProtectionStatus(ParameterGroup protectionStatusOfServer)
        {
            this.HostId = protectionStatusOfServer.GetParameterValue("HostId");
            this.PrepareTargetStatus = Utils.ParseEnum<OperationStatus>(protectionStatusOfServer.GetParameterValue("PrepareTarget"));
            //this.CreateReplicationPairsStatus = Utils.ParseEnum<OperationStatus>(protectionStatusOfServer.GetParameterValue("CreateReplicationPairs"));
            this.CreateReplicationPairsStatus = protectionStatusOfServer.GetParameterValue("CreateReplicationPairs");
            this.ProtectionStage = Utils.ParseEnum<ProtectionStatus>(protectionStatusOfServer.GetParameterValue("ProtectionStage"));
            ParameterGroup protectionErrorDetailsParamGroup = protectionStatusOfServer.GetParameterGroup("ErrorDetails");
            if (protectionErrorDetailsParamGroup != null && protectionErrorDetailsParamGroup.ChildList != null)
            {
                ProtectionErrorDetails = new ErrorDetails(protectionErrorDetailsParamGroup);
            }
        }
    }

    public class ProtectionRequestStatus
    {
        public string ProtectionPlanId { get; private set; }
        public List<ServerProtectionStatus> ProtectionStatusOfServers { get; private set; }

        public ProtectionRequestStatus(ParameterGroup protectionRequestStatus)
        {
            this.ProtectionPlanId = protectionRequestStatus.GetParameterValue("ProtectionPlanId");
            ParameterGroup individualStatuses = protectionRequestStatus.GetParameterGroup("ProtectionStatus");
            if (individualStatuses != null && individualStatuses.ChildList != null)
            {
                ProtectionStatusOfServers = new List<ServerProtectionStatus>();
                ServerProtectionStatus serverProtectionStatus;
                foreach (var item in individualStatuses.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        serverProtectionStatus = new ServerProtectionStatus(item as ParameterGroup);
                        ProtectionStatusOfServers.Add(serverProtectionStatus);
                    }
                }
            }            
        }
    }
}
