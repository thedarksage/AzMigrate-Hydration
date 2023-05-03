using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXStatusRequest : ICXRequest
    {
        // Local Parameters
        private static readonly string RequestIdList = "RequestIdList";
        private ParameterGroup requestIdList;

        public FunctionRequest Request { get; set; }

        public CXStatusRequest()
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionGetRequestStatus;
            Request.Include = false;
            requestIdList = new ParameterGroup(RequestIdList);
            Request.AddParameterGroup(requestIdList);
        }

        public CXStatusRequest(string requestId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionGetRequestStatus;
            Request.Include = false;
            requestIdList = new ParameterGroup(RequestIdList);
            Request.AddParameterGroup(requestIdList);
            requestIdList.AddParameter(Constants.KeyRequestId, requestId);
        }

        public CXStatusRequest(List<string> requestIds)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionGetRequestStatus;
            Request.Include = false;
            requestIdList = new ParameterGroup(RequestIdList);
            Request.AddParameterGroup(requestIdList);
            foreach (var id in requestIds)
            {
                requestIdList.AddParameter(Constants.KeyRequestId, id);
            }
        }
    }
}
