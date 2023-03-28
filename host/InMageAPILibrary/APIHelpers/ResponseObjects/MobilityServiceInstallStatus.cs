using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class MobilityServiceInstallStatus
    {
        public string HostIP { get; private set; }
        public OperationStatus Status { get; private set; }
        public string StatusMessage { get; private set; }
        public string LastUpdateTime { get; private set; }
        public string ErrorMessage { get; private set; }
        public ErrorDetails InstallErrorDetails { get; private set; }

        public MobilityServiceInstallStatus(ParameterGroup installMobilityServiceStatusOfServer)
        {
            this.HostIP = installMobilityServiceStatusOfServer.GetParameterValue("HostIP");
            this.Status = Utils.ParseEnum<OperationStatus>(installMobilityServiceStatusOfServer.GetParameterValue("Status"));
            this.StatusMessage = installMobilityServiceStatusOfServer.GetParameterValue("StatusMessage");
            this.LastUpdateTime = installMobilityServiceStatusOfServer.GetParameterValue("LastUpdateTime");
            this.ErrorMessage = installMobilityServiceStatusOfServer.GetParameterValue("ErrorMessage");
            ParameterGroup installErrorDetailsParamGroup = installMobilityServiceStatusOfServer.GetParameterGroup("ErrorDetails");
            if(installErrorDetailsParamGroup != null)
            {
                InstallErrorDetails = new ErrorDetails(installErrorDetailsParamGroup);
            }
        }
    }
}
