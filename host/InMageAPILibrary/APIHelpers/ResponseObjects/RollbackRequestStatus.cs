using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ServerRollbackStatus
    {
        public string HostId { get; private set; }
        public OperationStatus RollbackStatus { get; private set; }
        public string TagName { get; private set; }
        public ErrorDetails RollbackErrorDetails { get; private set; }

        public ServerRollbackStatus(ParameterGroup serverRollbackStatus)
        {
            HostId = serverRollbackStatus.GetParameterValue("HostId");
            string rollbackStatusString = serverRollbackStatus.GetParameterValue("Rollback");
            if(String.IsNullOrEmpty(rollbackStatusString))
            {
                rollbackStatusString = serverRollbackStatus.GetParameterValue("RollbackStatus");
            }
            RollbackStatus = Utils.ParseEnum<OperationStatus>(rollbackStatusString);
            TagName = serverRollbackStatus.GetParameterValue("TagName");
            ParameterGroup rollbackErrorDetailsParamGroup = serverRollbackStatus.GetParameterGroup("ErrorDetails");
            if (rollbackErrorDetailsParamGroup != null)
            {
                RollbackErrorDetails = new ErrorDetails(rollbackErrorDetailsParamGroup);
            }
        }
    }

    public class RollbackRequestStatus
    {
        public string RollbackPlanId { get; private set; }
        //public OperationStatus ReadinessCheckStatus { get; private set; }
        public OperationStatus RollbackStatus { get; private set; }
        public List<ServerRollbackStatus> RollbackStatusOfServers { get; private set; }

        public RollbackRequestStatus(ParameterGroup rollbackRequestStatus)
        {
            this.RollbackPlanId = rollbackRequestStatus.GetParameterValue("RollbackPlanId");
            //this.ReadinessCheckStatus = Utils.ParseEnum<OperationStatus>(rollbackRequestStatus.GetParameterValue("ReadinessCheck"));
            this.RollbackStatus = Utils.ParseEnum<OperationStatus>(rollbackRequestStatus.GetParameterValue("Rollback"));
            ParameterGroup serversStatus = rollbackRequestStatus.GetParameterGroup("Servers");
            if (serversStatus != null && serversStatus.ChildList != null)
            {
                RollbackStatusOfServers = new List<ServerRollbackStatus>();                
                foreach (var item in serversStatus.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        RollbackStatusOfServers.Add(new ServerRollbackStatus(item as ParameterGroup));
                    }
                }
            }
        }

        //public RollbackRequestStatus(FunctionResponse getRollbackStateFunctionResponse)
        //{
        //    this.RollbackPlanId = getRollbackStateFunctionResponse.GetParameterValue("RollbackPlanId");
        //    this.ReadinessCheckStatus = Utils.ParseEnum<OperationStatus>(getRollbackStateFunctionResponse.GetParameterValue("ReadinessCheck"));
        //    this.RollbackStatus = Utils.ParseEnum<OperationStatus>(getRollbackStateFunctionResponse.GetParameterValue("Rollback"));
        //    ParameterGroup serversStatus = getRollbackStateFunctionResponse.GetParameterGroup("Servers");
        //    if (serversStatus != null && serversStatus.ChildList != null)
        //    {
        //        RollbackStatusOfServers = new List<ServerRollbackStatus>();        
        //        foreach (var item in serversStatus.ChildList)
        //        {
        //            if (item is ParameterGroup)
        //            {
        //                RollbackStatusOfServers.Add(new ServerRollbackStatus(item as ParameterGroup));
        //            }
        //        }
        //    }
        //}
    }
}
