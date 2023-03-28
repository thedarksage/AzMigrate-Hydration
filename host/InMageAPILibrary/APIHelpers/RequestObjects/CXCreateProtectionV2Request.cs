using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXCreateProtectionV2Request : ICXRequest
    {
        // Local Parameters
        private ParameterGroup protectionProperties;
        private ParameterGroup globalOptions;
        private ParameterGroup sourceServers;

        public FunctionRequest Request { get; set; }

        public CXCreateProtectionV2Request()
        {
            InitializeRequest(null, null);
        }

        public CXCreateProtectionV2Request(string planName)
        {
            InitializeRequest(null, planName);
        }

        public CXCreateProtectionV2Request(string planId, string planName)
        {
            InitializeRequest(planId, planName);
        }

        public void SetProtectionProperties(int batchResync, bool multiVMSyncEnabled, int rpoThresholdinMinutes, int consistencyIntervalInMinutes, int crashConsistencyIntervalInSeconds, bool compressionEnabled)
        {
            protectionProperties.AddParameter("BatchResync", batchResync.ToString());
            protectionProperties.AddParameter("MultiVmSync", multiVMSyncEnabled ? "On" : "Off");
            protectionProperties.AddParameter("RPOThreshold", rpoThresholdinMinutes.ToString());
            protectionProperties.AddParameter("ConsistencyInterval", consistencyIntervalInMinutes.ToString());
            protectionProperties.AddParameter("CrashConsistencyInterval", crashConsistencyIntervalInSeconds.ToString());
            protectionProperties.AddParameter("Compression", compressionEnabled ? "Enabled" : "Disabled");
        }

        public void SetAutoResyncOptions(DateTime startTime, DateTime endTime, int intervalInSeconds)
        {
            ParameterGroup autoResyncOptions = new ParameterGroup("AutoResyncOptions");
            protectionProperties.AddParameterGroup(autoResyncOptions);
            autoResyncOptions.AddParameter("StartTime", startTime.GetDateTimeFormats()[108]);
            autoResyncOptions.AddParameter("EndTime", endTime.GetDateTimeFormats()[108]);
            autoResyncOptions.AddParameter("Interval", intervalInSeconds.ToString());
        }

        public void SetGlobalOptions(string processServerID, ProtectionComponents useNatIPFor, string masterTargetID, ProtectionEnvironment protectionDirection)
        {
            globalOptions.AddParameter("ProcessServer", processServerID);
            globalOptions.AddParameter("UseNatIPFor", useNatIPFor.ToString());
            globalOptions.AddParameter("MasterTarget", masterTargetID);
            if (protectionDirection != ProtectionEnvironment.Unknown)
            {
                globalOptions.AddParameter("ProtectionDirection", protectionDirection.ToString());
            }
        }

        public void AddSourceServerForProtection(string hostId, string processServerID, ProtectionComponents useNatIPFor, string masterTargetID, 
            ProtectionEnvironment protectionDirection, List<string> sourceDiskNames)
        {
            ParameterGroup sourceServer = new ParameterGroup("SourceServer" + (sourceServers.ChildList.Count + 1).ToString());
            sourceServers.AddParameterGroup(sourceServer);
            sourceServer.AddParameter("HostId", hostId);
            sourceServer.AddParameter("ProcessServer", processServerID);
            sourceServer.AddParameter("UseNatIPFor", useNatIPFor.ToString());
            sourceServer.AddParameter("MasterTarget", masterTargetID);

            if (protectionDirection != ProtectionEnvironment.Unknown)
            {
                sourceServer.AddParameter("ProtectionDirection", protectionDirection.ToString());
            }
            ParameterGroup diskDetailsPG = new ParameterGroup("DiskDetails");
            sourceServer.AddParameterGroup(diskDetailsPG);
            int diskDetailsCount = 1;
            ParameterGroup diskDet;

            foreach (string sourceDisk in sourceDiskNames)
            {
                diskDet = new ParameterGroup("Disk" + diskDetailsCount.ToString());
                diskDet.AddParameter("SourceDiskName", sourceDisk);
                diskDetailsPG.AddParameterGroup(diskDet);
                ++diskDetailsCount;
            }
        }

        private void InitializeRequest(string planId, string planName)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionCreateProtectionV2;

            if (!string.IsNullOrEmpty(planId))
            {
                Request.AddParameter("PlanId", planId);
            }

            if (!string.IsNullOrEmpty(planName))
            {
                Request.AddParameter("PlanName", planName);
            }

            Request.Include = true;
            protectionProperties = new ParameterGroup("ProtectionProperties");
            Request.AddParameterGroup(protectionProperties);
            globalOptions = new ParameterGroup("GlobalOptions");
            Request.AddParameterGroup(globalOptions);
            sourceServers = new ParameterGroup("SourceServers");
            Request.AddParameterGroup(sourceServers);
        }
    }
}
