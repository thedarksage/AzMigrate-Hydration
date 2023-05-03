using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ServerReadinessCheckStatus
    {
        public string HostId { get; private set; }
        public string ReadinessCheckStatus { get; private set; }
        public string Message { get; private set; }

        public ServerReadinessCheckStatus(string hostId, string readinessCheckStatus, string message)
        {
            this.HostId = hostId;
            this.ReadinessCheckStatus = readinessCheckStatus;
            this.Message = message;
        }
    }

    public class ProtectionReadinessCheckStatus
    {
        public string ConfigurationId { get; private set; }
        public List<ServerReadinessCheckStatus> ReadinessCheckStatusOfServers;

        public ProtectionReadinessCheckStatus(ParameterGroup configuration)
        {
            ServerReadinessCheckStatus serverReadinessCheckStatus;
            ParameterGroup serverReadinessCheckStatusPG;
            ReadinessCheckStatusOfServers = new List<ServerReadinessCheckStatus>();
            ConfigurationId = configuration.Id;
            foreach (var item in configuration.ChildList)
            {
                if (item is ParameterGroup)
                {
                    serverReadinessCheckStatusPG = item as ParameterGroup;
                    serverReadinessCheckStatus = new ServerReadinessCheckStatus(
                        serverReadinessCheckStatusPG.Id,
                        serverReadinessCheckStatusPG.GetParameterValue("ReadinessCheck"),
                        serverReadinessCheckStatusPG.GetParameterValue("ErrorMessage"));
                    ReadinessCheckStatusOfServers.Add(serverReadinessCheckStatus);
                }
            }
        }
    }
}
