using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class RemoveProtectionRequestStatus
    {
        public string ProtectionPlanId { get; private set; }
        public string ProtectionPlanName { get; private set; }

        public OperationStatus DeleteStatusOfProtectionPlan { get; private set; }
        public Dictionary<string, OperationStatus> DeleteStatusOfServers { get; private set; }

        public RemoveProtectionRequestStatus(ParameterGroup removeProtectionRequestStatus)
        {
            this.ProtectionPlanId = removeProtectionRequestStatus.GetParameterValue("ProtectionPlanId");
            this.ProtectionPlanName = removeProtectionRequestStatus.GetParameterValue("ProtectionPlanName");
            this.DeleteStatusOfProtectionPlan = Utils.ParseDeleteStatus(removeProtectionRequestStatus.GetParameterValue("ProtectionPlanStatus"));
            ParameterGroup individualStatuses = removeProtectionRequestStatus.GetParameterGroup("Servers");
            if (individualStatuses != null && individualStatuses.ChildList != null)
            {
                DeleteStatusOfServers = new Dictionary<string, OperationStatus>();
                foreach (var item in individualStatuses.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        ParameterGroup serverStatus = (item as ParameterGroup);
                        DeleteStatusOfServers.Add(serverStatus.GetParameterValue("HostId"), Utils.ParseDeleteStatus(serverStatus.GetParameterValue("DeleteStatus")));
                    }
                }
            }
        }

    }
}
