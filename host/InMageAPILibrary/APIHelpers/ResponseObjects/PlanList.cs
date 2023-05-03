using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ProtectionPlanDetails
    {
        public string PlanId { get; private set; }
        public string PlanName { get; private set; }
        public List<string> SourceServerIds { get; private set; }

        public ProtectionPlanDetails(ParameterGroup protectionPlanPG)
        {
            PlanId = protectionPlanPG.GetParameterValue("ProtectionPlanId");
            PlanName = protectionPlanPG.GetParameterValue("ProtectionPlanName");
            SourceServerIds = new List<string>();
            ParameterGroup sourceServers = protectionPlanPG.GetParameterGroup("SourceServers");
            Parameter sourceServer;
            if(sourceServers != null && sourceServers.ChildList != null)
            {
                foreach(var item in sourceServers.ChildList)
                {
                    if(item is Parameter)
                    {
                        sourceServer = item as Parameter;
                        SourceServerIds.Add(sourceServer.Value);
                    }
                }
            }

        }
    }

    public class RollbackPlanDetails
    {
        public string PlanId { get; private set; }
        public string PlanName { get; private set; }
        public OperationStatus RollbackStatus { get; private set; }
        public List<string> SourceServerIds { get; private set; }

        public RollbackPlanDetails(ParameterGroup rollbackPlanPG)
        {
            PlanId = rollbackPlanPG.GetParameterValue("RollbackPlanId");
            PlanName = rollbackPlanPG.GetParameterValue("RollbackPlanName");
            RollbackStatus = Utils.ParseEnum<OperationStatus>(rollbackPlanPG.GetParameterValue("RollbackStatus"));
            SourceServerIds = new List<string>();
            ParameterGroup sourceServers = rollbackPlanPG.GetParameterGroup("SourceServers");
            Parameter sourceServer;
            if (sourceServers != null && sourceServers.ChildList != null)
            {
                foreach (var item in sourceServers.ChildList)
                {
                    if (item is Parameter)
                    {
                        sourceServer = item as Parameter;
                        SourceServerIds.Add(sourceServer.Value);
                    }
                }
            }

        }
    }

    public class PlanList
    {
        public List<ProtectionPlanDetails> ProtectionPlans { get; private set; }
        public List<RollbackPlanDetails> RollbackPlans { get; private set; }

        public PlanList(FunctionResponse listPlansResponse)
        {
            ProtectionPlans = new List<ProtectionPlanDetails>();
            RollbackPlans = new List<RollbackPlanDetails>();
            ParameterGroup plansPG = listPlansResponse.GetParameterGroup("ProtectionPlans");
            if(plansPG != null)
            {
                foreach(var item in plansPG.ChildList)
                {
                    if(item is ParameterGroup)
                    {
                        ProtectionPlans.Add(new ProtectionPlanDetails(item as ParameterGroup));
                    }
                }
            }

            plansPG = listPlansResponse.GetParameterGroup("RollbackPlans");
            if (plansPG != null)
            {
                foreach (var item in plansPG.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        RollbackPlans.Add(new RollbackPlanDetails(item as ParameterGroup));
                    }
                }
            }
        }
    }
}
