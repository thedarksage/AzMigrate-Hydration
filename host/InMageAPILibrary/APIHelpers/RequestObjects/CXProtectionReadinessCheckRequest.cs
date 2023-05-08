using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXProtectionReadinessCheckRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup serverMapping;

        public FunctionRequest Request { get; set; }

        public CXProtectionReadinessCheckRequest(string processServerId, string masterTargetId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionProtectionReadinessCheck;
            Request.Include = true;
            Request.AddParameter("ProcessServer", processServerId);
            Request.AddParameter("MasterTarget", masterTargetId);
            serverMapping = new ParameterGroup("ServerMapping");
            Request.AddParameterGroup(serverMapping);
        }

        public void SetServerMappingConfiguration(string processServerId, string sourceId, string masterTargetId)
        {
            ParameterGroup configuration = new ParameterGroup("Configuration" + serverMapping.ChildList.Count + 1);
            if (!string.IsNullOrEmpty(processServerId))
            {
                configuration.AddParameter("ProcessServer", processServerId);
            }
            configuration.AddParameter("Source", sourceId);
            if (!string.IsNullOrEmpty(masterTargetId))
            {
                configuration.AddParameter("MasterTarget", masterTargetId);
            }
            serverMapping.AddParameterGroup(configuration);
        }
    }
}
