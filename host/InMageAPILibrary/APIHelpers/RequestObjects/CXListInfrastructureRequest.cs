using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXListInfrastructureRequest : ICXRequest
    {
        // Local Parameters
        private bool hasSetInfrastructureType;
        private bool hasSetInfrastructureId;
        private bool hasSetInfrastructureVmId;
                
        public FunctionRequest Request { get; set; }

        public CXListInfrastructureRequest()
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionListInfrastructure;
        }

        public CXListInfrastructureRequest(InfrastructureType infraType, string infrastructureId, string infrastructureVmId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionListInfrastructure;
            Request.AddParameter("InfrastructureType", infraType.ToString());
            Request.AddParameter("InfrastructureId", infrastructureId);
            Request.AddParameter("InfrastructureVmId", infrastructureVmId);
            hasSetInfrastructureId = true;
            hasSetInfrastructureType = true;
            hasSetInfrastructureVmId = true;
        }

        public void SetInfrastructureId(string infrastructureId)
        {
            if(hasSetInfrastructureId)
            {
                Request.RemoveParameter("InfrastructureId");
            }
            else
            {
                hasSetInfrastructureId = true;
            }
            Request.AddParameter("InfrastructureId", infrastructureId);
        }

        public void SetInfrastructureType(InfrastructureType infraType)
        {
            if(hasSetInfrastructureType)
            {
                Request.RemoveParameter("InfrastructureType");
            }
            else
            {
                hasSetInfrastructureType = true;
            }
            Request.AddParameter("InfrastructureType", infraType.ToString());
        }

        public void SetInfrastructureVMId(string infrastructureVmId)
        {
            if (hasSetInfrastructureVmId)
            {
                Request.RemoveParameter("InfrastructureVmId");
            }
            else
            {
                hasSetInfrastructureVmId = true;
            }
            Request.AddParameter("InfrastructureVmId", infrastructureVmId);
        }
    }
}
