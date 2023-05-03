using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXModifyProtectionPropertiesRequest : ICXRequest
    {
        public FunctionRequest Request { get; set; }

        public CXModifyProtectionPropertiesRequest(string protectionPlanId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionModifyProtectionProperties;
            Request.Include = true;
            Request.AddParameter("ProtectionPlanId", protectionPlanId);
        }

        public void SetBatchResync(int batchResync)
        {
            Request.AddParameter("BatchResync", batchResync.ToString());
        }

        public void EnableMultiVmSync(bool multiVMSyncEnabled)
        {
            Request.AddParameter("MultiVmSync", multiVMSyncEnabled ? "On" : "Off");
        }

        public void SetRPOThreshold(int rpoThresholdinMinutes)
        {
            Request.AddParameter("RPOThreshold", rpoThresholdinMinutes.ToString());
        }

        public void SetHoursToRetainConsistencyPoints(int hoursToRetainConsistencyPoints)
        {
            Request.AddParameter("HourlyConsistencyPoints", hoursToRetainConsistencyPoints.ToString());
        }

        public void SetApplicationConsistencyFrequency(int consistencyIntervalInMinutes)
        {
            Request.AddParameter("ApplicationConsistencyPoints", consistencyIntervalInMinutes.ToString());
        }

        public void EnableCompression(bool compressionEnabled)
        {
            Request.AddParameter("Compression", compressionEnabled ? "Enabled" : "Disabled");
        }

        public void SetAutoResyncOptions(DateTime startTime, DateTime endTime, int intervalInSeconds)
        {
            ParameterGroup autoResyncOptions = new ParameterGroup("AutoResyncOptions");
            Request.AddParameterGroup(autoResyncOptions);
            autoResyncOptions.AddParameter("StartTime", startTime.GetDateTimeFormats()[108]);
            autoResyncOptions.AddParameter("EndTime", endTime.GetDateTimeFormats()[108]);
            autoResyncOptions.AddParameter("Interval", intervalInSeconds.ToString());
        }       
    }
}
