using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class HealthFactor
    {
        public HealthFactorType Type { get; private set; }
        public Criticality Impact { get; private set; }
        public string Description { get; private set; }

        public HealthFactor(ParameterGroup healthFactorPG)
        {
            Type = Utils.ParseEnum<HealthFactorType>(healthFactorPG.GetParameterValue("Type"));
            Impact = Utils.ParseEnum<Criticality>(healthFactorPG.GetParameterValue("Impact"));
            Description = healthFactorPG.GetParameterValue("Description");
        }
    }

    public class RetentionAvailability
    {
        public DateTime? From { get; private set; }
        public DateTime? To { get; private set; }
        
        public RetentionAvailability(ParameterGroup retentionWindowPG)
        {
            From = Utils.UnixTimeStampToDateTime(retentionWindowPG.GetParameterValue("From"));
            To = Utils.UnixTimeStampToDateTime(retentionWindowPG.GetParameterValue("To"));
        }
    }

    public class ProtectedDevice
    {
        public string DeviceName { get; private set; }
        public ProtectionStatus ProtectionStage { get; private set; }
        public int RPOInSeconds { get; private set; }
        public double ResyncProgressPercentage { get; private set; }
        public int ResyncDuration { get; private set; }
        public bool ResyncRequiredEnabled { get; private set; }
        public long DeviceCapacityInBytes { get; private set; }
        public long FileSystemCapacityInBytes { get; private set; }        
        public double DataInTransitOfSourceInMb { get; private set; }
        public double DataInTransitOfPSInMb { get; private set; }
        public double DataInTransitOfTargetInMb { get; private set; }
        public double DailyCompressedDataChangeInMb { get; private set; }
        public double DailyUncompressedDataChangeInMb { get; private set; }
        public List<HealthFactor> HealthFactors { get; private set; }

        public ProtectedDevice(ParameterGroup protectedDevicePG)
        {
            DeviceName = protectedDevicePG.GetParameterValue("DeviceName");
            ProtectionStage = Utils.ParseEnum<ProtectionStatus>(protectedDevicePG.GetParameterValue("ProtectionStage"));
            int value;
            int.TryParse(protectedDevicePG.GetParameterValue("RPO"), out value);
            RPOInSeconds = value;           
            double resynProgress;
            double.TryParse(protectedDevicePG.GetParameterValue("ResyncProgress"), out resynProgress);
            ResyncProgressPercentage = resynProgress;
            int.TryParse(protectedDevicePG.GetParameterValue("ResyncDuration"), out value);
            ResyncDuration = value;
            ResyncRequiredEnabled = String.Compare(protectedDevicePG.GetParameterValue("ResyncRequired"), "Yes", StringComparison.OrdinalIgnoreCase) == 0 ? true : false;
            long capacity;
            long.TryParse(protectedDevicePG.GetParameterValue("DeviceCapacity"), out capacity);
            DeviceCapacityInBytes = capacity;
            long.TryParse(protectedDevicePG.GetParameterValue("FileSystemCapacity"), out capacity);
            FileSystemCapacityInBytes = capacity;

            double dataInMb;
            ParameterGroup parameterGroup = protectedDevicePG.GetParameterGroup("DataInTransit");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                double.TryParse(parameterGroup.GetParameterValue("Source"), out dataInMb);
                DataInTransitOfSourceInMb = dataInMb;
                double.TryParse(parameterGroup.GetParameterValue("PS"), out dataInMb);
                DataInTransitOfPSInMb = dataInMb;
                double.TryParse(parameterGroup.GetParameterValue("Target"), out dataInMb);
                DataInTransitOfTargetInMb = dataInMb;
            }

            parameterGroup = protectedDevicePG.GetParameterGroup("DailyDataChangeRate");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                double.TryParse(parameterGroup.GetParameterValue("Compression"), out dataInMb);
                DailyCompressedDataChangeInMb = dataInMb;
                double.TryParse(parameterGroup.GetParameterValue("Uncompression"), out dataInMb);
                DailyUncompressedDataChangeInMb = dataInMb;                
            }

            parameterGroup = protectedDevicePG.GetParameterGroup("HealthFactors");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                HealthFactors = new List<HealthFactor>();
                foreach (var item in parameterGroup.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        HealthFactors.Add(new HealthFactor(item as ParameterGroup));
                    }
                }
            }
        }
    }

    public class ProtectedServer
    {
        public string InfrastructureVMId { get; private set; }
        public string ResourceId { get; private set; }
        public string ServerHostId { get; private set; }
        public string ServerHostName { get; private set; }
        public IPAddress ServerHostIP { get; private set; }
        public OSType ServerOS { get; private set; }
        public int RPOInSeconds { get; private set; }
        public ProtectionStatus ProtectionStage { get; private set; }
        public double ResyncProgressPercentage { get; private set; }
        public int ResyncDuration { get; private set; }
        public List<HealthFactor> HealthFactors { get; private set; }
        public RetentionAvailability RetentionWindow { get; private set; }
        public double DailyCompressedDataChangeInMb { get; private set; }
        public double DailyUncompressedDataChangeInMb { get; private set; }
        public List<ProtectedDevice> ProtectedDevices { get; private set; }

        public ProtectedServer(ParameterGroup protectedServerPG)
        {
            InfrastructureVMId = protectedServerPG.GetParameterValue("InfrastructureVMId");
            ResourceId = protectedServerPG.GetParameterValue("ResourceId");
            ServerHostId = protectedServerPG.GetParameterValue("ServerHostId");
            ServerHostName = protectedServerPG.GetParameterValue("ServerHostName");
            ServerHostIP = IPAddress.Parse(protectedServerPG.GetParameterValue("ServerHostIp"));
            ServerOS = Utils.ParseEnum<OSType>(protectedServerPG.GetParameterValue("ServerOS"));
            int value;
            int.TryParse(protectedServerPG.GetParameterValue("RPO"), out value);
            RPOInSeconds = value;
            ProtectionStage = Utils.ParseEnum<ProtectionStatus>(protectedServerPG.GetParameterValue("ProtectionStage"));
            double resynProgress;
            double.TryParse(protectedServerPG.GetParameterValue("ResyncProgress"), out resynProgress);
            ResyncProgressPercentage = resynProgress;
            int.TryParse(protectedServerPG.GetParameterValue("ResyncDuration"), out value);
            ResyncDuration = value;
            ParameterGroup parameterGroup = protectedServerPG.GetParameterGroup("HealthFactors");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                HealthFactors = new List<HealthFactor>();
                foreach (var item in parameterGroup.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        HealthFactors.Add(new HealthFactor(item as ParameterGroup));
                    }
                }
            }

            parameterGroup = protectedServerPG.GetParameterGroup("RetentionWindow");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                RetentionWindow = new RetentionAvailability(parameterGroup);
            }

            double dataInMb;
            parameterGroup = protectedServerPG.GetParameterGroup("DailyDataChangeRate");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                double.TryParse(parameterGroup.GetParameterValue("Compressed"), out dataInMb);
                DailyCompressedDataChangeInMb = dataInMb;
                double.TryParse(parameterGroup.GetParameterValue("Uncompressed"), out dataInMb);
                DailyUncompressedDataChangeInMb = dataInMb;
            }

            parameterGroup = protectedServerPG.GetParameterGroup("ProtectedDevices");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                ProtectedDevices = new List<ProtectedDevice>();
                foreach (var item in parameterGroup.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        ProtectedDevices.Add(new ProtectedDevice(item as ParameterGroup));
                    }
                }
            }
        }
    }

    public class ProtectionDetails
    {
        public string ProtectionPlanId { get; private set; }
        public SolutionType SolutionType { get; private set; }
        public int TotalServersProtected { get; private set; }
        public int TotalVolumesProtected { get; private set; }
        public List<HealthFactor> HealthFactors { get; private set; }
        public List<ProtectedServer> ProtectedServers { get; private set; }

        public ProtectionDetails(ParameterGroup protectionDetailsPG)
        {
            ProtectionPlanId = protectionDetailsPG.GetParameterValue("ProtectionPlanId");
            SolutionType = Utils.ParseEnum<SolutionType>(protectionDetailsPG.GetParameterValue("SolutionType"));
            int value;
            int.TryParse(protectionDetailsPG.GetParameterValue("ServersProtected"), out value);
            TotalServersProtected = value;
            int.TryParse(protectionDetailsPG.GetParameterValue("VolumesProtected"), out value);
            TotalVolumesProtected = value;
            ParameterGroup parameterGroup = protectionDetailsPG.GetParameterGroup("HealthFactors");
            if(parameterGroup != null && parameterGroup.ChildList != null)
            {
                HealthFactors = new List<HealthFactor>();
                foreach(var item in parameterGroup.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        HealthFactors.Add(new HealthFactor(item as ParameterGroup));
                    }
                }
            }

            parameterGroup = protectionDetailsPG.GetParameterGroup("ProtectedServers");
            if (parameterGroup != null && parameterGroup.ChildList != null)
            {
                ProtectedServers = new List<ProtectedServer>();
                foreach (var item in parameterGroup.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        ProtectedServers.Add(new ProtectedServer(item as ParameterGroup));
                    }
                }
            }
        }
    }
}
