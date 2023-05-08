using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class MobilityServiceUnInstallStatus
    {
        public string HostId { get; private set; }
        public OperationStatus Status { get; private set; }
        public IPAddress HostIP { get; private set; }        
        public string StatusMessage { get; private set; }
        public DateTime? LastUpdateTime { get; private set; }
        public ErrorDetails UninstallErrorDetails { get; private set; }

        public MobilityServiceUnInstallStatus(ParameterGroup unInstallMobilityServiceStatusOfServer)
        {
            this.HostId = unInstallMobilityServiceStatusOfServer.GetParameterValue("HostId");
            this.HostIP = IPAddress.Parse(unInstallMobilityServiceStatusOfServer.GetParameterValue("HostIP"));
            this.Status = Utils.ParseEnum<OperationStatus>(unInstallMobilityServiceStatusOfServer.GetParameterValue("Status"));
            string value = unInstallMobilityServiceStatusOfServer.GetParameterValue("LastUpdateTime");
            this.LastUpdateTime = Utils.UnixTimeStampToDateTime(value);
            this.StatusMessage = unInstallMobilityServiceStatusOfServer.GetParameterValue("StatusMessage");
            ParameterGroup uninstallErrorDetailsParamGroup = unInstallMobilityServiceStatusOfServer.GetParameterGroup("ErrorDetails");
            if (uninstallErrorDetailsParamGroup != null)
            {
                UninstallErrorDetails = new ErrorDetails(uninstallErrorDetailsParamGroup);
            }
        }
    }
}
