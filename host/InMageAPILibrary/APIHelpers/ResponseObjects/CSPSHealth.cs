using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class SystemLoadMetrics
    {
        public double SystemLoadAverage { get; private set; }
        public HealthState Status { get; private set; }

        public SystemLoadMetrics(ParameterGroup systemLoadMetrics)
        {
            string value = systemLoadMetrics.GetParameterValue("SystemLoad");
            SystemLoadAverage = String.IsNullOrEmpty(value) ? 0 : double.Parse(value);
            Status = Utils.ParseEnum<HealthState>(systemLoadMetrics.GetParameterValue("Status"));
        }
    }

    public class CPULoadMetrics
    {
        public double CPULoadPercentage { get; private set; }
        public HealthState Status { get; private set; }

        public CPULoadMetrics(ParameterGroup cpuLoadMetrics)
        {
            string value = cpuLoadMetrics.GetParameterValue("CPULoad");
            CPULoadPercentage = String.IsNullOrEmpty(value) ? 0 : double.Parse(value.TrimEnd(new char[] { '%' }));
            Status = Utils.ParseEnum<HealthState>(cpuLoadMetrics.GetParameterValue("Status"));
        }
    }

    public class MemoryUsageMetrics
    {
        public long TotalMemoryInBytes { get; private set; }
        public long AvailableMemoryInBytes { get; private set; }
        public long UsedMemoryInBytes { get; private set; }        
        public double MemoryUsagePercentage { get; private set; }
        public HealthState Status { get; private set; }

        public MemoryUsageMetrics(ParameterGroup memoryUsage)
        {
            long value;
            long.TryParse(memoryUsage.GetParameterValue("TotalMemory"), out value);
            TotalMemoryInBytes = value;
            long.TryParse(memoryUsage.GetParameterValue("AvailableMemory"), out value);
            UsedMemoryInBytes = value;
            long.TryParse(memoryUsage.GetParameterValue("UsedMemory"), out value);
            AvailableMemoryInBytes = value;
            string memoryUsageValue = memoryUsage.GetParameterValue("MemoryUsage");
            MemoryUsagePercentage = String.IsNullOrEmpty(memoryUsageValue) ? 0 : double.Parse(memoryUsageValue.TrimEnd(new char[] { '%' }));
            Status = Utils.ParseEnum<HealthState>(memoryUsage.GetParameterValue("Status"));
        }
    }

    public class FreeSpaceMetrics
    {
        public long TotalSpaceInBytes { get; private set; }
        public long AvailableSpaceInBytes { get; private set; }
        public long UsedSpaceInBytes { get; private set; }
        public double FreeSpacePercentage { get; private set; }
        public HealthState Status { get; private set; }

        public FreeSpaceMetrics(ParameterGroup freeSpace)
        {
            long value;
            long.TryParse(freeSpace.GetParameterValue("TotalSpace"), out value);
            TotalSpaceInBytes = value;
            long.TryParse(freeSpace.GetParameterValue("AvailableSpace"), out value);
            AvailableSpaceInBytes = value;
            long.TryParse(freeSpace.GetParameterValue("UsedSpace"), out value);
            UsedSpaceInBytes = value;
            string freeSpacePercentage = freeSpace.GetParameterValue("FreeSpace");
            FreeSpacePercentage = String.IsNullOrEmpty(freeSpacePercentage) ? 0 : double.Parse(freeSpacePercentage.TrimEnd(new char[] { '%' }));
            Status = Utils.ParseEnum<HealthState>(freeSpace.GetParameterValue("Status"));
        }
    }

    public class DiskActivityStatus
    {
        public double DiskActivityInMbPerSec { get; private set; }
        public HealthState Status { get; private set; }

        public DiskActivityStatus(ParameterGroup freeSpace)
        {
            string value = freeSpace.GetParameterValue("Activity");
            DiskActivityInMbPerSec = String.IsNullOrEmpty(value) ? 0 : double.Parse(value);
            Status = Utils.ParseEnum<HealthState>(freeSpace.GetParameterValue("Status"));
        }
    }

    public class WebServerHealth
    {
        public double WebLoad { get; private set; }
        public HealthState Status { get; private set; }

        public WebServerHealth(ParameterGroup freeSpace)
        {
            string value = freeSpace.GetParameterValue("WebLoad");
            WebLoad = String.IsNullOrEmpty(value) ? 0 : double.Parse(value);
            Status = Utils.ParseEnum<HealthState>(freeSpace.GetParameterValue("Status"));
        }
    }

    public class DatabaseServerHealth
    {
        public double SqlLoad { get; private set; }
        public HealthState Status { get; private set; }

        public DatabaseServerHealth(ParameterGroup freeSpace)
        {
            string value = freeSpace.GetParameterValue("SqlLoad");
            SqlLoad = String.IsNullOrEmpty(value) ? 0 : double.Parse(value);
            Status = Utils.ParseEnum<HealthState>(freeSpace.GetParameterValue("Status"));
        }
    }

    public class ConfigurationServerHealth
    {
        public string HostId { get; private set; }
        public int ProtectedServerCount { get; private set; }
        public int ReplicationPairCount { get; private set; }
        public int ProcessServerCount { get; private set; }
        public int AgentCount { get; private set; }
        public SystemLoadMetrics SystemLoad { get; private set; }
        public CPULoadMetrics CPULoad { get; private set; }
        public MemoryUsageMetrics MemoryUsage { get; private set; }
        public FreeSpaceMetrics FreeSpace { get; private set; }
        // DiskActivityStatus is null in case of Linux CS
        public DiskActivityStatus DiskActivity { get; private set; }
        public WebServerHealth WebServerStatus { get; private set; }
        public DatabaseServerHealth DatabaseServerStatus { get; private set; }
        public ServiceStatus CSServiceStatus { get; private set; }

        public ConfigurationServerHealth(ParameterGroup csServerHealth)
        {
            HostId = csServerHealth.GetParameterValue("HostId");
            string value = csServerHealth.GetParameterValue("ProtectedServers");
            ProtectedServerCount = String.IsNullOrEmpty(value) ? 0 : int.Parse(value);
            value = csServerHealth.GetParameterValue("ReplicationPairCount");
            ReplicationPairCount = String.IsNullOrEmpty(value) ? 0 : int.Parse(value);
            value = csServerHealth.GetParameterValue("ProcessServerCount");
            ProcessServerCount = String.IsNullOrEmpty(value) ? 0 : int.Parse(value);
            value = csServerHealth.GetParameterValue("AgentCount");
            AgentCount = String.IsNullOrEmpty(value) ? 0 : int.Parse(value);
            CSServiceStatus = Utils.ParseEnum<ServiceStatus>(csServerHealth.GetParameterValue("CSServiceStatus"));
            ParameterGroup systemPerformance = ResponseParser.GetParameterGroup(csServerHealth.ChildList, "SystemPerformance");
            if(systemPerformance != null && systemPerformance.ChildList != null)
            {
                SystemLoad = new SystemLoadMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "SystemLoad"));
                CPULoad = new CPULoadMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "CPULoad"));
                MemoryUsage = new MemoryUsageMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "MemoryUsage"));
                FreeSpace = new FreeSpaceMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "FreeSpace"));
                ParameterGroup parameterGroup = ResponseParser.GetParameterGroup(systemPerformance.ChildList, "DiskActivity");
                DiskActivity = parameterGroup == null ? null : new DiskActivityStatus(parameterGroup);
                WebServerStatus = new WebServerHealth(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "WebServer"));
                DatabaseServerStatus = new DatabaseServerHealth(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "DatabaseServer"));                
            }
        }
    }

    public class ProcessServerHealth
    {
        public string HostId { get; private set; }
        public IPAddress PSServerIP { get; private set; }
        public DateTime? Heartbeat { get; private set; }
        public int ServerCount { get; private set; }
        public int ReplicationPairCount { get; private set; }
        public double DataChangeRate { get; private set; }
        public SystemLoadMetrics SystemLoad { get; private set; }
        public CPULoadMetrics CPULoad { get; private set; }
        public MemoryUsageMetrics MemoryUsage { get; private set; }
        public FreeSpaceMetrics FreeSpace { get; private set; }
        // DiskActivityStatus is null in case of Linux PS
        public DiskActivityStatus DiskActivity { get; private set; }
        public ServiceStatus PSServiceStatus { get; private set; }

        public ProcessServerHealth(ParameterGroup processServerHealth)
        {
            HostId = processServerHealth.GetParameterValue("HostId");
            PSServerIP = Utils.ParseIP(processServerHealth.GetParameterValue("PSServerIP"));
            Heartbeat = Utils.UnixTimeStampToDateTime(processServerHealth.GetParameterValue("Heartbeat"));
            string value = processServerHealth.GetParameterValue("ServerCount");
            ServerCount = String.IsNullOrEmpty(value) ? 0 : int.Parse(value);
            value = processServerHealth.GetParameterValue("ReplicationPairCount");
            ReplicationPairCount = String.IsNullOrEmpty(value) ? 0 : int.Parse(value);
            value = processServerHealth.GetParameterValue("DataChangeRate");
            DataChangeRate = String.IsNullOrEmpty(value) ? 0 : double.Parse(value);
            PSServiceStatus = Utils.ParseEnum<ServiceStatus>(processServerHealth.GetParameterValue("PSServiceStatus"));
            ParameterGroup systemPerformance = ResponseParser.GetParameterGroup(processServerHealth.ChildList, "SystemPerformance");
            if (systemPerformance != null && systemPerformance.ChildList != null)
            {
                SystemLoad = new SystemLoadMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "SystemLoad"));
                CPULoad = new CPULoadMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "CPULoad"));
                MemoryUsage = new MemoryUsageMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "MemoryUsage"));
                FreeSpace = new FreeSpaceMetrics(ResponseParser.GetParameterGroup(systemPerformance.ChildList, "FreeSpace"));
                ParameterGroup parameterGroup = ResponseParser.GetParameterGroup(systemPerformance.ChildList, "DiskActivity");
                DiskActivity = parameterGroup == null ? null : new DiskActivityStatus(parameterGroup);                
            }
        }
    }

    public class CSPSHealth
    {
        public ConfigurationServerHealth CSHealth { get; private set; }
        public List<ProcessServerHealth> HealthOfProcessServers { get; private set; }

        public CSPSHealth(FunctionResponse cspsFunctionResponse)
        {
            ParameterGroup paramGroup = ResponseParser.GetParameterGroup(cspsFunctionResponse.ChildList, "ConfigServer");
            if(paramGroup != null)
            {
                CSHealth = new ConfigurationServerHealth(paramGroup);
            }
            paramGroup = ResponseParser.GetParameterGroup(cspsFunctionResponse.ChildList, "ProcessServers");
            if (paramGroup != null)
            {
                HealthOfProcessServers = new List<ProcessServerHealth>();
                foreach (ParameterGroup psServerHealth in paramGroup.ChildList)
                {
                    HealthOfProcessServers.Add(new ProcessServerHealth(psServerHealth));
                }
            }
        }
    }
}
