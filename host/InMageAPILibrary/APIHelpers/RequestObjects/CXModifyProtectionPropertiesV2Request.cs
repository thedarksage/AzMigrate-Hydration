using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXModifyProtectionPropertiesV2Request : ICXRequest
    {
        public FunctionRequest Request { get; set; }

        public CXModifyProtectionPropertiesV2Request(string protectionPlanId)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionModifyProtectionPropertiesV2;
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

        public void SetApplicationConsistencyFrequency(int consistencyIntervalInMinutes)
        {
            Request.AddParameter("ApplicationConsistencyPoints", consistencyIntervalInMinutes.ToString());
        }

        public void SetCrashConsistencyFrequency(int crashConsistencyIntervalInSeconds)
        {
            Request.AddParameter("CrashConsistencyInterval", crashConsistencyIntervalInSeconds.ToString());
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
