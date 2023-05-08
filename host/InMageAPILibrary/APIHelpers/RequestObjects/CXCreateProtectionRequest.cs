using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class CXCreateProtectionRequest : ICXRequest
    {
        // Local Parameters
        private ParameterGroup protectionProperties;
        private ParameterGroup globalOptions;
        private ParameterGroup sourceServers;

        public FunctionRequest Request { get; set; }

		public CXCreateProtectionRequest()
        {
			InitializeRequest(null, null);
		}

        public CXCreateProtectionRequest(string planName)
        {
            InitializeRequest(null, planName);
        }
		
		public CXCreateProtectionRequest(string planId, string planName)
        {
			InitializeRequest(planId, planName);
		}
		
        public void SetProtectionProperties(int batchResync, bool multiVMSyncEnabled, int rpoThresholdinMinutes, int hoursToRetainConsistencyPoints, int consistencyIntervalInMinutes, bool compressionEnabled)
        {
            protectionProperties.AddParameter("BatchResync", batchResync.ToString());
            protectionProperties.AddParameter("MultiVmSync", multiVMSyncEnabled ? "On" : "Off");
            protectionProperties.AddParameter("RPOThreshold", rpoThresholdinMinutes.ToString());
            protectionProperties.AddParameter("HourlyConsistencyPoints", hoursToRetainConsistencyPoints.ToString());
            protectionProperties.AddParameter("ConsistencyInterval", consistencyIntervalInMinutes.ToString());
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

        public void SetGlobalOptions(string processServerID, ProtectionComponents useNatIPFor, string masterTargetID, string retentionDrive, ProtectionEnvironment protectionDirection)
        {
            globalOptions.AddParameter("ProcessServer", processServerID);
            globalOptions.AddParameter("UseNatIPFor", useNatIPFor.ToString());
            globalOptions.AddParameter("MasterTarget", masterTargetID);
            globalOptions.AddParameter("RetentionDrive", retentionDrive);
            if (protectionDirection != ProtectionEnvironment.Unknown)
            {
                globalOptions.AddParameter("ProtectionDirection", protectionDirection.ToString());
            }
        }

        public void AddSourceServerForProtection(string hostId, string processServerID, ProtectionComponents useNatIPFor, string masterTargetID, string retentionDrive,
            ProtectionEnvironment protectionDirection, List<DiskMappingForProtection> diskMappings)
        {
            ParameterGroup sourceServer = new ParameterGroup("SourceServer" + (sourceServers.ChildList.Count + 1).ToString());
            sourceServers.AddParameterGroup(sourceServer);
            sourceServer.AddParameter("HostId", hostId);
            sourceServer.AddParameter("ProcessServer", processServerID);
            sourceServer.AddParameter("UseNatIPFor", useNatIPFor.ToString());
            sourceServer.AddParameter("MasterTarget", masterTargetID);
            sourceServer.AddParameter("RetentionDrive", retentionDrive);
            if (protectionDirection != ProtectionEnvironment.Unknown)
            {
                sourceServer.AddParameter("ProtectionDirection", protectionDirection.ToString());
            }
            ParameterGroup diskMappingPG = new ParameterGroup("DiskMapping");
            sourceServer.AddParameterGroup(diskMappingPG);
            int diskMappingCount = 1;
            foreach (var item in diskMappings)
            {
                ParameterGroup diskMap = new ParameterGroup("Disk" + diskMappingCount.ToString());
                diskMap.AddParameter("SourceDiskName", item.SourceDiskName);
                diskMap.AddParameter("TargetLUNId", item.TargetLUNId);
                diskMappingPG.AddParameterGroup(diskMap);
                ++diskMappingCount;
            }
        }
		
		private void InitializeRequest(string planId, string planName)
        {
            Request = new FunctionRequest();
            Request.Name = Constants.FunctionCreateProtection;

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
