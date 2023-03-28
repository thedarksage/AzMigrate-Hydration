using System;
using System.Linq;
using System.Collections.Generic;
using System.Text;
using System.Net;
using InMage.APIHelpers;

namespace InMage.Test.API
{
    public class CXAPITest : BaseAPITest
    {
        CXClient client;

        public bool ShowOutputOnConsole { get; set; }

        public CXAPITest()
        {
            client = new CXClient();
            ShowOutputOnConsole = true;
        }

        public override void SetConfiguration(string cxIP, int port, string protocol)
        {
            client = new CXClient(cxIP, port, Utils.ParseEnum<CXProtocol>(protocol));
        }

        public override FunctionHandler GetFunctionHandler(Section section)
        {
            FunctionHandler handler;

            if (String.Compare(section.Name, Constants.FunctionInstallMobilityService, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(InstallMobilityService);
            }
            else if (String.Compare(section.Name, Constants.FunctionUnInstallMobilityService, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(UnInstallMobilityService);
            }
            else if (String.Compare(section.Name, Constants.FunctionGetRequestStatus, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(GetRequestStatus);
            }
            else if (String.Compare(section.Name, Constants.FunctionProtectionReadinessCheck, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(CheckForProtectionReadiness);
            }
            else if (String.Compare(section.Name, Constants.FunctionCreateProtection, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(CreateProtection);
            }
            else if (String.Compare(section.Name, Constants.FunctionGetProtectionState, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(GetProtectionState);
            }
            else if (String.Compare(section.Name, Constants.FunctionCleanRollbackPlans, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(CleanRollbackPlans);
            }
            else if (String.Compare(section.Name, Constants.FunctionRollbackReadinessCheck, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(CheckForRollbackReadiness);
            }
            else if (String.Compare(section.Name, Constants.FunctionListConsistencyPoints, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ListConsistencyPoints);
            }
            else if (String.Compare(section.Name, Constants.FunctionModifyProtectionProperties, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ModifyProtectionProperties);
            }
            else if (String.Compare(section.Name, Constants.FunctionModifyProtection, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ModifyProtection);
            }
            else if (String.Compare(section.Name, Constants.FunctionRestartResync, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(RestartResync);
            }
            else if (String.Compare(section.Name, Constants.FuntionStopProtection, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(StopProtection);
            }
            else if (String.Compare(section.Name, Constants.FunctionRollbackReadinessCheck, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(CheckForRollbackReadiness);
            }
            else if (String.Compare(section.Name, Constants.FunctionCreateRollback, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(CreateRollback);
            }
            else if (String.Compare(section.Name, Constants.FuntionGetRollbackState, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(GetRollbackState);
            }
            else if (String.Compare(section.Name, Constants.FunctionListPlans, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ListPlans);
            }
            else if (String.Compare(section.Name, Constants.FunctionListEvents, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ListEvents);
            }
            else if (String.Compare(section.Name, Constants.FunctionGetCSPSHealth, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(GetCSPSHealth);
            }
            else if (String.Compare(section.Name, Constants.FunctionGetProtectionDetails, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(GetProtectionDetails);
            }
            else if (String.Compare(section.Name, Constants.FunctionInfrastructureDiscovery, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(DiscoverInfrastructure);
            }
            else if (String.Compare(section.Name, Constants.FunctionRemoveInfrastructure, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(RemoveInfrastructure);
            }
            else if (String.Compare(section.Name, Constants.FunctionListInfrastructure, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ListInfrastructure);
            }
            else if (String.Compare(section.Name, Constants.FunctionListScoutComponents, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(ListScoutComponents);
            }
            else if (String.Compare(section.Name, Constants.FunctionSetPSNatIP, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(SetPSNatIP);
            }
            else if (String.Compare(section.Name, Constants.FunctionSetVPNDetails, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(SetVPNDetails);
            }
            else if (String.Compare(section.Name, Constants.FunctionPSFailover, StringComparison.OrdinalIgnoreCase) == 0)
            {
                handler = new FunctionHandler(PSFailover);
            }
            else
            {
                throw new Exception("Function not implemented");                
            }

            return handler;
        }

        public JobStatus InstallMobilityService(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;            
            CXInstallMobilityServiceRequest request = new CXInstallMobilityServiceRequest(section.GetKeyValue("pushserverid"));
            Key csKey = section.GetKey("cs");
            if(csKey != null && csKey.HasSubKeys)
            {
                request.SetConfigurationServerDetails(
                    csKey.GetSubKeyValue("ipaddress"),
                    int.Parse(csKey.GetSubKeyValue("port")),
                    Utils.ParseEnum<CXProtocol>(csKey.GetSubKeyValue("protocol"))
                    );
            }
            List<Key> hosts = section.GetKeyArray("host");
            string accountId;
            if (hosts != null)
            {
                foreach (Key host in hosts)
                {
                    accountId = host.GetSubKeyValue("accountid");
                    if (String.IsNullOrEmpty(accountId))
                    {
                        request.AddServer(
                            host.GetSubKeyValue("hostip"),
                            host.GetSubKeyValue("hostname"),
                            host.GetSubKeyValue("displayname"),
                            Utils.ParseEnum<OSType>(host.GetSubKeyValue("os")),
                            host.GetSubKeyValue("domain"),
                            host.GetSubKeyValue("username"),
                            host.GetSubKeyValue("password"),
                            Utils.ParseEnum<ScoutComponentType>(host.GetSubKeyValue("agenttype"))
                            );
                    }
                    else
                    {
                        request.AddServer(
                            host.GetSubKeyValue("hostip"),
                            host.GetSubKeyValue("hostname"),
                            host.GetSubKeyValue("displayname"),
                            Utils.ParseEnum<OSType>(host.GetSubKeyValue("os")),
                            accountId,
                            Utils.ParseEnum<ScoutComponentType>(host.GetSubKeyValue("agenttype"))
                            );
                    }
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            string requestId = client.ExecuteCXRequest(request, responseStatus);
            Logger.Debug("\r\n***************************** InstallMobilityService API response *****************************", ShowOutputOnConsole);

            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nInstallMobilityService API execution succeeded. Request ID: " + requestId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(requestId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForInstallMobilityService), requestId, this.AsyncFunctionTimeOut);                                        
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nFailed to install mobility service. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatus(Section section)
        {
            string asyncFunction = section.GetKeyValue("asyncfunction");
            string requestId = section.GetKeyValue("requestid");
            JobStatus jobStatus = JobStatus.Failed;
            OperationStatus operationStatus;

            if (String.IsNullOrEmpty(asyncFunction))
            {
                throw new Exception("Value of asyncoperation is not found for GetRequestStatus API execution");
            }

            if (String.Compare(asyncFunction, Constants.FunctionInfrastructureDiscovery, StringComparison.OrdinalIgnoreCase) == 0)
            {
                jobStatus = GetRequestStatusForInfrastructureDiscovery(requestId, out operationStatus);
            }
            else if (String.Compare(asyncFunction, Constants.FunctionInstallMobilityService, StringComparison.OrdinalIgnoreCase) == 0)
            {
                jobStatus = GetRequestStatusForInstallMobilityService(requestId, out operationStatus);
            }
            else if (String.Compare(asyncFunction, Constants.FunctionUnInstallMobilityService, StringComparison.OrdinalIgnoreCase) == 0)
            {
                jobStatus = GetRequestStatusForUninstallMobilityService(requestId, out operationStatus);
            }
            else if (String.Compare(asyncFunction, Constants.FunctionCreateProtection, StringComparison.OrdinalIgnoreCase) == 0)
            {
                jobStatus = GetRequestStatusForCreateProtection(requestId, out operationStatus);
            }
            else if (String.Compare(asyncFunction, Constants.FunctionCreateRollback, StringComparison.OrdinalIgnoreCase) == 0)
            {
                jobStatus = GetRequestStatusForRollback(requestId, out operationStatus);
            }
            else if (String.Compare(asyncFunction, Constants.FuntionStopProtection, StringComparison.OrdinalIgnoreCase) == 0)
            {
                jobStatus = GetRequestStatusForStopProtection(requestId, out operationStatus);
            }
            else
            {
                Logger.Error("GetRequestStatus not supported for the Asynchronous function " + asyncFunction, ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatusForInfrastructureDiscovery(string requestId, out OperationStatus infrastructureDiscoveryStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            infrastructureDiscoveryStatus = OperationStatus.Unknown;
            if (!string.IsNullOrEmpty(requestId))
            {
                ResponseStatus responseStatus = new ResponseStatus();
                List<Infrastructure> infrastructures = client.GetRequestStatusForInfratructureDiscovery(requestId, responseStatus);
                Logger.Debug("\r\n***************************** GetRequestStatus API response for Infratructure Discovery *****************************", ShowOutputOnConsole);
                if (responseStatus.ReturnCode == 0)
                {
                    jobStatus = JobStatus.Completed;
                    EnumerateInfrastructure(infrastructures, out infrastructureDiscoveryStatus);
                }
                else
                {
                    Logger.Error(String.Format("\r\nGetRequestStatus API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error("Error: Request ID is invalid", ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatusForInstallMobilityService(string requestId, out OperationStatus installMobilityStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            installMobilityStatus = OperationStatus.Unknown;

            if (!string.IsNullOrEmpty(requestId))
            {
                ResponseStatus responseStatus = new ResponseStatus();
                List<MobilityServiceInstallStatus> statusOfServers = client.GetRequestStatusForInstallMobilityService(requestId, responseStatus);
                Logger.Debug("\r\n***************************** GetRequestStatus API response for Mobility Service installation Status *****************************", ShowOutputOnConsole);
                if (responseStatus.ReturnCode == 0)
                {
                    jobStatus = JobStatus.Completed;
                    if (statusOfServers != null)
                    {
                        foreach (MobilityServiceInstallStatus serverStatus in statusOfServers)
                        {
                            Logger.Debug("\r\n-------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Host IP: " + serverStatus.HostIP, ShowOutputOnConsole);
                            Logger.Debug("-------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Status:           " + serverStatus.Status, ShowOutputOnConsole);
                            Logger.Debug("Status Message:   " + serverStatus.StatusMessage, ShowOutputOnConsole);
                            Logger.Debug("Last Update Time: " + serverStatus.LastUpdateTime, ShowOutputOnConsole);
                            Logger.Debug("Error Message:    " + serverStatus.ErrorMessage, ShowOutputOnConsole);
                            DisplayErrorDetails(serverStatus.InstallErrorDetails, null);

                            // Find the overall status based on the individual server status.
                            if (serverStatus.Status == OperationStatus.Unknown)
                            {
                                installMobilityStatus = OperationStatus.Failed;
                            }
                            if ((serverStatus.Status >= OperationStatus.InProgress && serverStatus.Status > installMobilityStatus) ||
                                (installMobilityStatus < OperationStatus.Failed))
                            {
                                // Check if the status of one of the servers is Pending\InProgress\Failed then the overall status must be Pending\InProgress\Failed.
                                installMobilityStatus = serverStatus.Status;
                            }
                        }
                    }
                }
                else
                {
                    Logger.Error(String.Format("\r\nGetRequestStatus API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error("\r\nError: Request ID is invalid", ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatusForUninstallMobilityService(string requestId, out OperationStatus uninstallMobilityStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            uninstallMobilityStatus = OperationStatus.Unknown;
            if (!string.IsNullOrEmpty(requestId))
            {
                ResponseStatus responseStatus = new ResponseStatus();
                List<MobilityServiceUnInstallStatus> statusOfServers = client.GetRequestStatusForUnInstallMobilityService(requestId, responseStatus);
                Logger.Debug("\r\n***************************** GetRequestStatus API response for Mobility Service Uninstall Status *****************************", ShowOutputOnConsole);
                if (responseStatus.ReturnCode == 0)
                {
                    jobStatus = JobStatus.Completed;
                    if (statusOfServers != null)
                    {
                        foreach (MobilityServiceUnInstallStatus serverStatus in statusOfServers)
                        {
                            Logger.Debug("\r\n-------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Host IP: " + serverStatus.HostIP, ShowOutputOnConsole);
                            Logger.Debug("-------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Host ID:          " + serverStatus.HostId, ShowOutputOnConsole);
                            Logger.Debug("Status:           " + serverStatus.Status, ShowOutputOnConsole);
                            Logger.Debug("Status Message:   " + serverStatus.StatusMessage, ShowOutputOnConsole);
                            Logger.Debug("Last Update Time: " + serverStatus.LastUpdateTime, ShowOutputOnConsole);
                            //Logger.Debug("Error Message:       " + serverStatus.ErrorMessage, ShowOutputOnConsole);
                            DisplayErrorDetails(serverStatus.UninstallErrorDetails, null);

                            // Find the overall status based on the individual server status.
                            if (serverStatus.Status == OperationStatus.Unknown)
                            {
                                uninstallMobilityStatus = OperationStatus.Failed;
                            }
                            if ((serverStatus.Status >= OperationStatus.InProgress && serverStatus.Status > uninstallMobilityStatus) ||
                                (uninstallMobilityStatus < OperationStatus.Failed))
                            {
                                // Check if the status of one of the servers is Pending\InProgress\Failed then the overall status must be Pending\InProgress\Failed.
                                uninstallMobilityStatus = serverStatus.Status;
                            }
                        }
                    }
                }
                else
                {
                    Logger.Error(String.Format("\r\nGetRequestStatus API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error("\r\nError: Request ID is invalid", ShowOutputOnConsole);
            }
            return jobStatus;
        }


        public JobStatus UnInstallMobilityService(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXUnInstallMobilityServiceRequest req = new CXUnInstallMobilityServiceRequest();
            List<Key> hosts = section.GetKeyArray("host");
            string accountId;
            if (hosts != null)
            {
                foreach (Key host in hosts)
                {
                    accountId = host.GetSubKeyValue("accountid");
                    if (String.IsNullOrEmpty(accountId))
                    {
                        req.AddServer(
                            host.GetSubKeyValue("hostid"),
                            host.GetSubKeyValue("username"),
                            host.GetSubKeyValue("password"),
                            host.GetSubKeyValue("pushserverid")
                            );
                    }
                    else
                    {
                        req.AddServer(
                            host.GetSubKeyValue("hostid"),
                            accountId,
                            host.GetSubKeyValue("pushserverid")
                            );
                    }
                }
            }

            ResponseStatus responseStatus = new ResponseStatus();
            string reqId = client.ExecuteCXRequest(req, responseStatus);
            Logger.Debug("\r\n***************************** UnInstallMobilityService API response *****************************", ShowOutputOnConsole);

            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nUnInstallMobilityService API execution is successful. Request ID: " + reqId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(reqId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForUninstallMobilityService), reqId, this.AsyncFunctionTimeOut);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nUnInstallMobilityService API is failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus ListEvents(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;

            ResponseStatus responseStatus = new ResponseStatus();
            EventList eventList = client.ListEvents(section.GetKeyValue("category"),
                Utils.ParseEnum<EventSeverity>(section.GetKeyValue("severity")),
                section.GetKeyValue("token"),
                responseStatus);

            Logger.Debug("\r\n***************************** ListEvents API response *****************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (eventList != null && eventList.Events.Count > 0)
                {
                    int count = 1;
                    foreach (Event event1 in eventList.Events)
                    {
                        Logger.Debug(String.Format("\r\n-------------------------------\r\nEvent {0}\r\n-------------------------------", count), ShowOutputOnConsole);
                        Logger.Debug("Category:         " + event1.Category, ShowOutputOnConsole);
                        Logger.Debug("Component:        " + event1.Component, ShowOutputOnConsole);
                        Logger.Debug("HostId:           " + event1.HostId, ShowOutputOnConsole);
                        Logger.Debug("HostName:         " + event1.HostName, ShowOutputOnConsole);
                        Logger.Debug("Severity:         " + event1.Severity, ShowOutputOnConsole);
                        Logger.Debug("Details:          " + event1.Details, ShowOutputOnConsole);
                        Logger.Debug("Summary:          " + event1.Summary, ShowOutputOnConsole);
                        Logger.Debug("EventCode:        " + event1.EventCode, ShowOutputOnConsole);
                        Logger.Debug("EventType:        " + event1.EventType, ShowOutputOnConsole);
                        Logger.Debug("EventTime:        " + event1.EventTime, ShowOutputOnConsole);
                        Logger.Debug("CorrectiveAction: " + event1.CorrectiveAction, ShowOutputOnConsole);
                        Logger.Debug("Placeholders:     ", ShowOutputOnConsole);
                        foreach (string key in event1.PlaceHolders.Keys)
                        {
                            Logger.Debug(String.Format("\t {0}: {1}", key, event1.PlaceHolders[key]), ShowOutputOnConsole);
                        }
                        ++count;
                    }
                }
                else
                {
                    Logger.Debug("\r\nNo events found.", ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nListEvents API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus CheckForProtectionReadiness(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            ResponseStatus responseStatus = new ResponseStatus();
            CXProtectionReadinessCheckRequest req = new CXProtectionReadinessCheckRequest(section.GetKeyValue("processserverid"), section.GetKeyValue("mastertargetid"));
            List<Key> serverMappingKeys = section.GetKeyArray("servermapping");
            if (serverMappingKeys != null)
            {
                foreach (Key key in serverMappingKeys)
                {
                    req.SetServerMappingConfiguration(
                        key.GetSubKeyValue("processserverid"),
                        key.GetSubKeyValue("sourceid"),
                        key.GetSubKeyValue("mastertargetid")
                        );
                }
            }
            List<ProtectionReadinessCheckStatus> statusList = client.CheckForProtectionReadiness(req, responseStatus);
            Logger.Debug("\r\n***************************** ProtectionReadinessCheck API response *****************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (statusList != null)
                {
                    foreach (ProtectionReadinessCheckStatus readinessCheckStatus in statusList)
                    {
                        Logger.Debug("\r\n-------------------------------", ShowOutputOnConsole);
                        Logger.Debug("ConfigurationId: " + readinessCheckStatus.ConfigurationId, ShowOutputOnConsole);
                        Logger.Debug("-------------------------------", ShowOutputOnConsole);
                        if (readinessCheckStatus.ReadinessCheckStatusOfServers != null)
                        {
                            foreach (ServerReadinessCheckStatus serverStatus in readinessCheckStatus.ReadinessCheckStatusOfServers)
                            {
                                Logger.Debug("\r\n\t -------------------------------", ShowOutputOnConsole);
                                Logger.Debug("\t HostId: " + serverStatus.HostId, ShowOutputOnConsole);
                                Logger.Debug("\t -------------------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Protection Readiness Check Status: " + serverStatus.ReadinessCheckStatus, ShowOutputOnConsole);
                                Logger.Debug("\t Message: " + serverStatus.Message, ShowOutputOnConsole);
                            }
                        }
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nProtectionReadinessCheck API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetCSPSHealth(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            ResponseStatus responseStatus = new ResponseStatus();
            CSPSHealth health = client.GetCSPSHealth(responseStatus);
            Logger.Debug("\r\n***************************** GetCSPSHealth API response *****************************", ShowOutputOnConsole);           

            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (health != null)
                {
                    if (health.CSHealth != null)
                    {
                        Logger.Debug("\r\n-------------------------------", ShowOutputOnConsole);
                        Logger.Debug("Health of CS server", ShowOutputOnConsole);
                        Logger.Debug("-------------------------------", ShowOutputOnConsole);
                        Logger.Debug("HostId:                        " + health.CSHealth.HostId, ShowOutputOnConsole);
                        Logger.Debug("ProtectedServers:              " + health.CSHealth.ProtectedServerCount, ShowOutputOnConsole);
                        Logger.Debug("ProcessServerCount:            " + health.CSHealth.ProcessServerCount, ShowOutputOnConsole);
                        Logger.Debug("AgentCount:                    " + health.CSHealth.AgentCount, ShowOutputOnConsole);
                        Logger.Debug("ReplicationPairCount:          " + health.CSHealth.ReplicationPairCount, ShowOutputOnConsole);

                        Console.WriteLine();
                        Logger.Debug("System Load:", ShowOutputOnConsole);
                        Logger.Debug("\t System Load:        " + health.CSHealth.SystemLoad.SystemLoadAverage, ShowOutputOnConsole);
                        Logger.Debug("\t System Load Status: " + health.CSHealth.SystemLoad.Status, ShowOutputOnConsole);

                        Console.WriteLine();
                        Logger.Debug("CPU Load:", ShowOutputOnConsole);
                        Logger.Debug("\t CPU Load Percentage: " + health.CSHealth.CPULoad.CPULoadPercentage, ShowOutputOnConsole);
                        Logger.Debug("\t CPU Load Status:     " + health.CSHealth.CPULoad.Status, ShowOutputOnConsole);

                        Console.WriteLine();
                        Logger.Debug("Memory Usage:", ShowOutputOnConsole);
                        Logger.Debug("\t Total Memory (in bytes):     " + health.CSHealth.MemoryUsage.TotalMemoryInBytes, ShowOutputOnConsole);
                        Logger.Debug("\t Available Memory (in bytes): " + health.CSHealth.MemoryUsage.AvailableMemoryInBytes, ShowOutputOnConsole);
                        Logger.Debug("\t Used Memory (in bytes):      " + health.CSHealth.MemoryUsage.UsedMemoryInBytes, ShowOutputOnConsole);
                        Logger.Debug("\t Memory Usage Percentage:     " + health.CSHealth.MemoryUsage.MemoryUsagePercentage, ShowOutputOnConsole);
                        Logger.Debug("\t Memory Usage Status:         " + health.CSHealth.MemoryUsage.Status, ShowOutputOnConsole);

                        Console.WriteLine();
                        Logger.Debug("Total Space:", ShowOutputOnConsole);
                        Logger.Debug("\t Total Space (in bytes): " + health.CSHealth.FreeSpace.TotalSpaceInBytes, ShowOutputOnConsole);
                        Logger.Debug("\t Used Space (in bytes):  " + health.CSHealth.FreeSpace.UsedSpaceInBytes, ShowOutputOnConsole);
                        Logger.Debug("\t Free Space Percentage:  " + health.CSHealth.FreeSpace.FreeSpacePercentage, ShowOutputOnConsole);
                        Logger.Debug("\t Free Space Status:      " + health.CSHealth.FreeSpace.Status, ShowOutputOnConsole);

                        if (health.CSHealth.DiskActivity != null)
                        {
                            Console.WriteLine();
                            Logger.Debug("Disk Activity:", ShowOutputOnConsole);
                            Logger.Debug("\t Disk Activity (in Mb Per Sec): " + health.CSHealth.DiskActivity.DiskActivityInMbPerSec, ShowOutputOnConsole);
                            Logger.Debug("\t Disk Activity Status:          " + health.CSHealth.DiskActivity.Status, ShowOutputOnConsole);
                        }

                        Console.WriteLine();
                        Logger.Debug("Web Server:", ShowOutputOnConsole);
                        Logger.Debug("\t Web Load:          " + health.CSHealth.WebServerStatus.WebLoad, ShowOutputOnConsole);
                        Logger.Debug("\t Web Server Status: " + health.CSHealth.WebServerStatus.Status, ShowOutputOnConsole);

                        Console.WriteLine();
                        Logger.Debug("DB Server:", ShowOutputOnConsole);
                        Logger.Debug("\t SQL Load:         " + health.CSHealth.DatabaseServerStatus.SqlLoad, ShowOutputOnConsole);
                        Logger.Debug("\t DB Server Status: " + health.CSHealth.DatabaseServerStatus.Status, ShowOutputOnConsole);

                        Console.WriteLine();
                        Logger.Debug("CS Service Status:             " + health.CSHealth.CSServiceStatus, ShowOutputOnConsole);
                    }
                    if (health.HealthOfProcessServers != null && health.HealthOfProcessServers.Count > 0)
                    {
                        foreach (ProcessServerHealth psHealth in health.HealthOfProcessServers)
                        {
                            Logger.Debug("\r\n-------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Health of PS: " + psHealth.PSServerIP, ShowOutputOnConsole);
                            Logger.Debug("-------------------------------", ShowOutputOnConsole);
                            Logger.Debug("HostId:                        " + psHealth.HostId, ShowOutputOnConsole);
                            Logger.Debug("PSServerIP:                    " + psHealth.PSServerIP, ShowOutputOnConsole);
                            Logger.Debug("Heartbeat:                     " + psHealth.Heartbeat, ShowOutputOnConsole);
                            Logger.Debug("ServerCount:                   " + psHealth.ServerCount, ShowOutputOnConsole);
                            Logger.Debug("ReplicationPairCount:          " + psHealth.ReplicationPairCount, ShowOutputOnConsole);
                            Logger.Debug("DataChangeRate:                " + psHealth.DataChangeRate, ShowOutputOnConsole);

                            Console.WriteLine();
                            Logger.Debug("System Load:", ShowOutputOnConsole);
                            Logger.Debug("\t SystemLoadAverage: " + psHealth.SystemLoad.SystemLoadAverage, ShowOutputOnConsole);
                            Logger.Debug("\t SystemLoadStatus:  " + psHealth.SystemLoad.Status, ShowOutputOnConsole);

                            Console.WriteLine();
                            Logger.Debug("CPU Load:", ShowOutputOnConsole);
                            Logger.Debug("\t CPULoadPercentage: " + psHealth.CPULoad.CPULoadPercentage, ShowOutputOnConsole);
                            Logger.Debug("\t CPULoadStatus:     " + psHealth.CPULoad.Status, ShowOutputOnConsole);

                            Console.WriteLine();
                            Logger.Debug("Memory Usage:", ShowOutputOnConsole);
                            Logger.Debug("\t TotalMemoryInBytes:     " + psHealth.MemoryUsage.TotalMemoryInBytes, ShowOutputOnConsole);
                            Logger.Debug("\t AvailableMemoryInBytes: " + psHealth.MemoryUsage.AvailableMemoryInBytes, ShowOutputOnConsole);
                            Logger.Debug("\t UsedMemoryInBytes:      " + psHealth.MemoryUsage.UsedMemoryInBytes, ShowOutputOnConsole);
                            Logger.Debug("\t MemoryUsagePercentage:  " + psHealth.MemoryUsage.MemoryUsagePercentage, ShowOutputOnConsole);
                            Logger.Debug("\t MemoryUsageStatus:      " + psHealth.MemoryUsage.Status, ShowOutputOnConsole);

                            Console.WriteLine();
                            Logger.Debug("Free Space:", ShowOutputOnConsole);
                            Logger.Debug("\t Total Space (in bytes):     " + psHealth.FreeSpace.TotalSpaceInBytes, ShowOutputOnConsole);
                            Logger.Debug("\t Available Space (in bytes): " + psHealth.FreeSpace.AvailableSpaceInBytes, ShowOutputOnConsole);
                            Logger.Debug("\t Used Space (in bytes):      " + psHealth.FreeSpace.UsedSpaceInBytes, ShowOutputOnConsole);
                            Logger.Debug("\t Free Space Percentage:      " + psHealth.FreeSpace.FreeSpacePercentage, ShowOutputOnConsole);
                            Logger.Debug("\t Status:                     " + psHealth.FreeSpace.Status, ShowOutputOnConsole);

                            if (psHealth.DiskActivity != null)
                            {
                                Console.WriteLine();
                                Logger.Debug("Disk Activity:", ShowOutputOnConsole);
                                Logger.Debug("\t Disk Activity (in Mb Per Sec): " + psHealth.DiskActivity.DiskActivityInMbPerSec, ShowOutputOnConsole);
                                Logger.Debug("\t Status:                        " + psHealth.DiskActivity.Status, ShowOutputOnConsole);
                            }
                            Console.WriteLine();
                            Logger.Debug("PS Service Status:             " + psHealth.PSServiceStatus, ShowOutputOnConsole);
                        }
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nFailed to get Health status of CS and PS server(s). [ERROR] {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            
            return jobStatus;
        }

        public JobStatus CreateProtection(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXCreateProtectionRequest req = new CXCreateProtectionRequest(section.GetKeyValue("planid"), section.GetKeyValue("planname"));
            Key protectionPropertiesKey = section.GetKey("protectionproperties");
            if (protectionPropertiesKey != null && protectionPropertiesKey.HasSubKeys)
            {
                req.SetProtectionProperties(
                    int.Parse(protectionPropertiesKey.GetSubKeyValue("batchresync")),
                    bool.Parse(protectionPropertiesKey.GetSubKeyValue("enablemultivmsync")),
                    int.Parse(protectionPropertiesKey.GetSubKeyValue("rpothreshold")),
                    int.Parse(protectionPropertiesKey.GetSubKeyValue("retainconsistencypoints")),
                    int.Parse(protectionPropertiesKey.GetSubKeyValue("consistencyinterval")),
                    bool.Parse(protectionPropertiesKey.GetSubKeyValue("compressionenabled")));
            }
            protectionPropertiesKey = section.GetKey("globaloptions");
            if (protectionPropertiesKey != null && protectionPropertiesKey.HasSubKeys)
            {
                req.SetGlobalOptions(
                    protectionPropertiesKey.GetSubKeyValue("processserverid"),
                    Utils.ParseEnum<ProtectionComponents>(protectionPropertiesKey.GetSubKeyValue("usenatfor")),
                    protectionPropertiesKey.GetSubKeyValue("mastertargetid"),
                    protectionPropertiesKey.GetSubKeyValue("retentiondrive"),
                    Utils.ParseEnum<ProtectionEnvironment>(protectionPropertiesKey.GetSubKeyValue("protectiondirection")));
            }

            protectionPropertiesKey = section.GetKey("autoresyncoptions");
            if (protectionPropertiesKey != null && protectionPropertiesKey.HasSubKeys)
            {
                req.SetAutoResyncOptions(
                    DateTime.Parse("01/01/2015 " + protectionPropertiesKey.GetSubKeyValue("starttime")),
                    DateTime.Parse("01/01/2015 " + protectionPropertiesKey.GetSubKeyValue("endtime")),
                    int.Parse(protectionPropertiesKey.GetSubKeyValue("interval")));
            }

            List<Key> sourceServerKeys = section.GetKeyArray("sourceserver");
            List<Key> diskMappingKeys = null;
            List<DiskMappingForProtection> disks = null;
            if (sourceServerKeys != null)
            {
                foreach (Key sourceServerKey in sourceServerKeys)
                {
                    diskMappingKeys = section.GetKeyArray(sourceServerKey.Name + ".diskmapping");
                    disks = new List<DiskMappingForProtection>();
                    if (diskMappingKeys != null)
                    {
                        foreach (Key diskMappingKey in diskMappingKeys)
                        {
                            disks.Add(new DiskMappingForProtection(diskMappingKey.GetSubKeyValue("sourcediskname"), int.Parse(diskMappingKey.GetSubKeyValue("targetlun"))));
                        }
                    }
                    req.AddSourceServerForProtection(
                        sourceServerKey.GetSubKeyValue("hostid"),
                        sourceServerKey.GetSubKeyValue("processserverid"),
                        Utils.ParseEnum<ProtectionComponents>(sourceServerKey.GetSubKeyValue("usenatipfor")),
                        sourceServerKey.GetSubKeyValue("mastertargetid"),
                        sourceServerKey.GetSubKeyValue("retentiondrive"),
                        Utils.ParseEnum<ProtectionEnvironment>(sourceServerKey.GetSubKeyValue("protectiondirection")),
                        disks);
                }
            }

            ResponseStatus responseStatus = new ResponseStatus();
            string requestId = client.ExecuteCXRequest(req, responseStatus);
            Logger.Debug("\r\n***************************** CreateProtection API response *****************************", ShowOutputOnConsole);

            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nCreateProtection API execution is successful. Request ID: " + requestId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(requestId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForCreateProtection), requestId, this.AsyncFunctionTimeOut);                                                      
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nCreateProtection API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatusForCreateProtection(string requestId, out OperationStatus protectionStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            protectionStatus = OperationStatus.Unknown;
            ResponseStatus responseStatus = new ResponseStatus();
            ProtectionRequestStatus protectionRequestStatus = client.GetRequestStatusForProtection(requestId, responseStatus);
            Logger.Debug("\r\n***************************** GetRequestStatus API response for Create Protection *****************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                EnumerateProtectionStatus(protectionRequestStatus, out protectionStatus);
            }
            else
            {
                Logger.Error(String.Format("\r\nGetRequestStatus API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetProtectionState(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string[] hostIds = null;
            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null && hostIdKeys.Count > 0)
            {
                hostIds = new string[hostIdKeys.Count];
                int index = 0;
                foreach (Key key in hostIdKeys)
                {
                    hostIds[index] = key.Value;
                    ++index;
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            ProtectionRequestStatus protectionStatus = client.GetProtectionState(section.GetKeyValue("protectionplanid"), hostIds, responseStatus);
            Logger.Debug("\r\n***************************** GetProtectionState API response *****************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                OperationStatus operationStatus;
                EnumerateProtectionStatus(protectionStatus, out operationStatus);
            }
            else
            {
                Logger.Error(String.Format("\r\nGetProtectionState API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus ListPlans(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string[] hostIds = null;
            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null && hostIdKeys.Count > 0)
            {
                hostIds = new string[hostIdKeys.Count];
                int index = 0;
                foreach (Key hostIdKey in hostIdKeys)
                {
                    hostIds[index] = hostIdKey.Value;
                    ++index;
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            PlanList planList = client.ListPlans(
                Utils.ParseEnum<PlanType>(section.GetKeyValue("plantype")),
                section.GetKeyValue("planid"),
                hostIds,
                responseStatus);
            int count;
            bool plansFound = false;
            Logger.Debug("\r\n*************************** ListPlans API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (planList != null)
                {
                    if (planList.ProtectionPlans != null && planList.ProtectionPlans.Count > 0)
                    {
                        plansFound = true;
                        Logger.Debug("\r\n---------------------", ShowOutputOnConsole);
                        Logger.Debug("Protection Plans:", ShowOutputOnConsole);
                        Logger.Debug("----------------------", ShowOutputOnConsole);
                        foreach (ProtectionPlanDetails plan in planList.ProtectionPlans)
                        {
                            Logger.Debug("\r\n\t ----------------------", ShowOutputOnConsole);
                            Logger.Debug("\t Plan Id: " + plan.PlanId, ShowOutputOnConsole);
                            Logger.Debug("\t ----------------------", ShowOutputOnConsole);
                            Logger.Debug("\t Plan Name:      " + plan.PlanName, ShowOutputOnConsole);
                            if (plan.SourceServerIds != null && plan.SourceServerIds.Count > 0)
                            {
                                count = 1;
                                Logger.Debug("\t Source Servers:", ShowOutputOnConsole);
                                foreach (string sourceServer in plan.SourceServerIds)
                                {
                                    Logger.Debug(String.Format("\t\t {0})\t{1}", count, sourceServer), ShowOutputOnConsole);
                                    ++count;
                                }
                            }
                        }
                    }
                    if (planList.RollbackPlans != null && planList.RollbackPlans.Count > 0)
                    {
                        plansFound = true;
                        Logger.Debug("\r\n---------------------", ShowOutputOnConsole);
                        Logger.Debug("Rollback Plans:", ShowOutputOnConsole);
                        Logger.Debug("---------------------", ShowOutputOnConsole);
                        foreach (RollbackPlanDetails plan in planList.RollbackPlans)
                        {
                            Logger.Debug("\r\n\t ----------------------", ShowOutputOnConsole);
                            Logger.Debug("\t Plan Id: " + plan.PlanId, ShowOutputOnConsole);
                            Logger.Debug("\t ----------------------", ShowOutputOnConsole);
                            Logger.Debug("\t Plan Name:      " + plan.PlanName, ShowOutputOnConsole);
                            count = 1;
                            foreach (string sourceServer in plan.SourceServerIds)
                            {
                                Logger.Debug("\t Source Servers:", ShowOutputOnConsole);
                                Logger.Debug(String.Format("\t\t {0})\t{1} ", count, sourceServer), ShowOutputOnConsole);
                                ++count;
                            }
                        }
                    }
                }
                if (!plansFound)
                {
                    Logger.Debug("\r\nNo Plans found.", ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\n ListPlans API execution failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus ListConsistencyPoints(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            bool waitForCommonConsistencyTag;
            bool.TryParse(section.GetKeyValue("waitforcommonconsistencytag"), out waitForCommonConsistencyTag);
            if (waitForCommonConsistencyTag)
            {
                jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<Section>(CheckForConsistencyPoints), section, this.AsyncFunctionTimeOut);               
            }
            else
            {
                OperationStatus consistencyTagCheckStatus;
                jobStatus = CheckForConsistencyPoints(section, out consistencyTagCheckStatus);                
            }

            return jobStatus;
        }

        public JobStatus CheckForConsistencyPoints(Section section, out OperationStatus consistencyTagCheckStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            consistencyTagCheckStatus = OperationStatus.Unknown;
            DateTime dt;
            CXListConsistencyPointsRequest req = new CXListConsistencyPointsRequest(
                section.GetKeyValue("protectionplanid"),
                DateTime.TryParse(section.GetKeyValue("tagstartdate"), out dt) ? dt : DateTime.Now.Subtract(new TimeSpan(0, 6, 0, 0)),
                DateTime.TryParse(section.GetKeyValue("tagenddate"), out dt) ? dt : DateTime.Now);

            req.SetConsistencyOptions(
                section.GetKeyValue("tagtype"),
                section.GetKeyValue("userdefinedtag") == bool.TrueString ? true : false,
                section.GetKeyValue("tagname"),
                Utils.ParseEnum<ConsistencyTagAccuracy>(section.GetKeyValue("tagaccuracy")),
                section.GetKeyValue("getrecenttag") == bool.TrueString ? true : false,
                section.GetKeyValue("multivmsync") == bool.TrueString ? true : false);

            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null)
            {
                foreach (Key key in hostIdKeys)
                {
                    req.AddServer(key.Value);
                }
            }

            bool consistencyPointsFound = false;
            ResponseStatus responseStatus = new ResponseStatus();
            ConsistencyPoints consistencyPoints = client.ListConsistencyPoints(req, responseStatus);
            Logger.Debug("\r\n*************************** ListConsistencyPoints API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (consistencyPoints != null)
                {
                    Logger.Debug("ProtectionPlanId: " + consistencyPoints.ProtectionPlanId, ShowOutputOnConsole);                    
                    if (consistencyPoints.ConsistencyPointList != null && consistencyPoints.ConsistencyPointList.Count > 0)
                    {
                        consistencyPointsFound = true;
                        consistencyTagCheckStatus = OperationStatus.Success;
                        Logger.Debug("\r\nCommon consistency points:" + consistencyPoints.ConsistencyPointList.Count, ShowOutputOnConsole);
                        foreach (ConsistencyTagInfo tagInfo in consistencyPoints.ConsistencyPointList)
                        {
                            Logger.Debug("\r\n--------------", ShowOutputOnConsole);
                            Logger.Debug("ID: " + tagInfo.TagId, ShowOutputOnConsole);
                            Logger.Debug("--------------", ShowOutputOnConsole);
                            Logger.Debug("TagName:        " + tagInfo.TagName, ShowOutputOnConsole);
                            Logger.Debug("TagAccuracy:    " + tagInfo.TagAccuracy, ShowOutputOnConsole);
                            Logger.Debug("TagApplication: " + tagInfo.TagApplication, ShowOutputOnConsole);
                            Logger.Debug("TagTimestamp:   " + tagInfo.TagTimestamp, ShowOutputOnConsole);                            
                        }
                    }
                    if (consistencyPoints.ConsistencyPointsOfServers != null && consistencyPoints.ConsistencyPointsOfServers.Count > 0)
                    {
                        consistencyPointsFound = true;
                        foreach (string key in consistencyPoints.ConsistencyPointsOfServers.Keys)
                        {
                            Logger.Debug("\r\n Consistency points of Server " + key + ":", ShowOutputOnConsole);
                            List<ConsistencyTagInfo> tagInfoList = consistencyPoints.ConsistencyPointsOfServers[key];                            
                            foreach (ConsistencyTagInfo tagInfo in tagInfoList)
                            {
                                Logger.Debug("\r\n--------------", ShowOutputOnConsole);
                                Logger.Debug("ID: " + tagInfo.TagId, ShowOutputOnConsole);
                                Logger.Debug("--------------", ShowOutputOnConsole);
                                Logger.Debug("TagName:        " + tagInfo.TagName, ShowOutputOnConsole);
                                Logger.Debug("TagAccuracy:    " + tagInfo.TagAccuracy, ShowOutputOnConsole);
                                Logger.Debug("TagApplication: " + tagInfo.TagApplication, ShowOutputOnConsole);
                                Logger.Debug("TagTimestamp:   " + tagInfo.TagTimestamp, ShowOutputOnConsole);                                
                            }
                        }
                    }
                }
                if (!consistencyPointsFound)
                {
                    Logger.Debug("\r\nNo consistency points found.", ShowOutputOnConsole);
                    consistencyTagCheckStatus = OperationStatus.InProgress;
                }
            }
            else
            {
                Logger.Error(String.Format("\r\n ListConsistency API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
                consistencyTagCheckStatus = OperationStatus.Failed;
            }
            return jobStatus;
        }

        public JobStatus ModifyProtectionProperties(Section section)
        {
            CXModifyProtectionPropertiesRequest req = new CXModifyProtectionPropertiesRequest(section.GetKeyValue("protectionplanid"));
            Key protectionPropertiesKey = section.GetKey("protectionproperties");
            string value = null;
            if (protectionPropertiesKey.HasSubKeys)
            {
                value = protectionPropertiesKey.GetSubKeyValue("compressionenabled");
                if (!String.IsNullOrEmpty(value))
                {
                    req.EnableCompression(bool.Parse(value));
                }
                value = protectionPropertiesKey.GetSubKeyValue("enablemultivmsync");
                if (!String.IsNullOrEmpty(value))
                {
                    req.EnableMultiVmSync(bool.Parse(value));
                }
                value = protectionPropertiesKey.GetSubKeyValue("consistencyinterval");
                if (!String.IsNullOrEmpty(value))
                {
                    req.SetApplicationConsistencyFrequency(int.Parse(value));
                }
                value = protectionPropertiesKey.GetSubKeyValue("retainconsistencypoints");
                if (!String.IsNullOrEmpty(value))
                {
                    req.SetHoursToRetainConsistencyPoints(int.Parse(value));
                }
                value = protectionPropertiesKey.GetSubKeyValue("batchresync");
                if (!String.IsNullOrEmpty(value))
                {
                    req.SetBatchResync(int.Parse(value));
                }
                value = protectionPropertiesKey.GetSubKeyValue("rpothreshold");
                if (!String.IsNullOrEmpty(value))
                {
                    req.SetRPOThreshold(int.Parse(value));
                }
            }
            protectionPropertiesKey = section.GetKey("autoresyncoptions");
            if (protectionPropertiesKey != null && protectionPropertiesKey.HasSubKeys)
            {
                req.SetAutoResyncOptions(
                    DateTime.Parse("01/01/2015 " + protectionPropertiesKey.GetSubKeyValue("starttime")),
                    DateTime.Parse("01/01/2015 " + protectionPropertiesKey.GetSubKeyValue("endtime")),
                    int.Parse(protectionPropertiesKey.GetSubKeyValue("interval")));
            }

            ResponseStatus responseStatus = new ResponseStatus();
            JobStatus jobStatus = JobStatus.Failed;
            bool modifyStatus = client.ModifyProtectionProperties(req, responseStatus);
            Logger.Debug("\r\n*************************** ModifyProtectionProperties API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {                
                if (modifyStatus)
                {
                    Logger.Debug("\r\nSuccessfully modified protection plan properties.", ShowOutputOnConsole);
                    jobStatus = JobStatus.Completed;
                }
                else
                {
                    Logger.Error("\r\nFailed to modify protection plan properties.", ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nModifyProtectionProperties API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus ModifyProtection(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXModifyProtectionRequest req = new CXModifyProtectionRequest(section.GetKeyValue("protectionplanid"));
            List<DiskMappingForProtection> disks = null;

            List<Key> sourceServerKeys = section.GetKeyArray("sourceserver");
            List<Key> diskMappingKeys = null;
            if (sourceServerKeys != null)
            {
                foreach (Key sourceServerKey in sourceServerKeys)
                {
                    diskMappingKeys = section.GetKeyArray(sourceServerKey.Name + ".diskmapping");
                    disks = new List<DiskMappingForProtection>();
                    if (diskMappingKeys != null)
                    {
                        foreach (Key diskMappingKey in diskMappingKeys)
                        {
                            disks.Add(new DiskMappingForProtection(diskMappingKey.GetSubKeyValue("sourcediskname"), int.Parse(diskMappingKey.GetSubKeyValue("targetlun"))));
                        }
                    }
                    req.AddServerForProtection(
                        sourceServerKey.GetSubKeyValue("hostid"),
                        sourceServerKey.GetSubKeyValue("processserverid"),
                        Utils.ParseEnum<ProtectionComponents>(sourceServerKey.GetSubKeyValue("usenatipfor")),
                        sourceServerKey.GetSubKeyValue("mastertargetid"),
                        sourceServerKey.GetSubKeyValue("retentiondrive"),
                        disks);
                }
            }

            ResponseStatus responseStatus = new ResponseStatus();
            string requestId = client.ExecuteCXRequest(req, responseStatus);
            Logger.Debug("\r\n*************************** ModifyProtection API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nModifyProtection API execution is successful. Request ID: " + requestId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(requestId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForCreateProtection), requestId, this.AsyncFunctionTimeOut);                    
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nModifyProtection API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus RestartResync(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string[] hostIds = null;
            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null && hostIdKeys.Count > 0)
            {
                hostIds = new string[hostIdKeys.Count];
                int index = 0;
                foreach (Key key in hostIdKeys)
                {
                    hostIds[index] = key.Value;
                    ++index;
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            Dictionary<string, OperationStatus> resyncStatus = client.RestartResync(
                section.GetKeyValue("protectionplanid"),
                hostIds,
                section.GetKeyValue("forceresyncenabled") == bool.TrueString ? true : false,
                responseStatus);

            Logger.Debug("\r\n*************************** RestartResync API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (resyncStatus != null)
                {
                    foreach (string key in resyncStatus.Keys)
                    {
                        Logger.Debug(String.Format("\r\nRestart resync status of Host ID: {0} is {1}", key, resyncStatus[key]), ShowOutputOnConsole);
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nRestartResync API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus StopProtection(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string protectionPlanId = section.GetKeyValue("protectionplanid");
            string forceDeleteStr = section.GetKeyValue("forcedelete");
            CXStopProtectionRequest req;
            if (String.IsNullOrEmpty(forceDeleteStr))
            {
                req = new CXStopProtectionRequest(protectionPlanId);
            }
            else
            {
                req = new CXStopProtectionRequest(protectionPlanId, bool.Parse(forceDeleteStr));
            }

            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null)
            {
                foreach (Key key in hostIdKeys)
                {
                    req.AddServerForStopProtection(key.Value);
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            string requestId = client.ExecuteCXRequest(req, responseStatus);
            Logger.Debug("\r\n*************************** StopProtection API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nStopProtection API execution succeeded. Request ID: " + requestId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(requestId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForStopProtection), requestId, this.AsyncFunctionTimeOut);                    
                }                
            }
            else
            {
                Logger.Error(String.Format("\r\nStopProtection API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatusForStopProtection(string requestId, out OperationStatus stopProtectionStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            stopProtectionStatus = OperationStatus.Unknown;
            ResponseStatus responseStatus = new ResponseStatus();
            RemoveProtectionRequestStatus removeProtectionStatus = client.GetRequestStatusForRemoveProtection(requestId, responseStatus);
            Logger.Debug("\r\n*************************** GetRequestStatus API Response for Stop Protection ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (removeProtectionStatus != null)
                {
                    Logger.Debug("Protection Plan Id:     " + removeProtectionStatus.ProtectionPlanId, ShowOutputOnConsole);
                    Logger.Debug("Protection Plan Name:   " + removeProtectionStatus.ProtectionPlanName, ShowOutputOnConsole);
                    Logger.Debug("Stop Protection status: " + removeProtectionStatus.DeleteStatusOfProtectionPlan, ShowOutputOnConsole);
                    stopProtectionStatus = removeProtectionStatus.DeleteStatusOfProtectionPlan;
                    foreach (string key in removeProtectionStatus.DeleteStatusOfServers.Keys)
                    {
                        Logger.Debug(String.Format("\r\n\t Stop Protection Status for Host ID {0} is {1}", key, removeProtectionStatus.DeleteStatusOfServers[key]), ShowOutputOnConsole);
                        //if (stopProtectionStatus == OperationStatus.Pending || stopProtectionStatus == OperationStatus.InProgress || stopProtectionStatus == OperationStatus.Failed)
                        //{
                        //    continue;
                        //}
                        //stopProtectionStatus = removeProtectionStatus.DeleteStatusOfServers[key];
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nGetRequestStatus API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus CheckForRollbackReadiness(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXRollbackReadinessCheckRequest req = new CXRollbackReadinessCheckRequest(
                section.GetKeyValue("multivmsyncenabled") == bool.TrueString ? true : false,
                Utils.ParseEnum<RecoveryPoint>(section.GetKeyValue("recoverypoint")),
                section.GetKeyValue("customtagname"),
                section.GetKeyValue("protectionplanid"));
            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null)
            {
                foreach (Key key in hostIdKeys)
                {
                    req.AddServer(key.Value);
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            List<ServerReadinessCheckStatus> statusList = client.CheckForRollbackReadiness(req, responseStatus);
            Logger.Debug("\r\n*************************** RollbackReadinessCheck API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (statusList != null)
                {
                    Logger.Debug("\r\nRollback Readiness Check status:", ShowOutputOnConsole);
                    foreach (ServerReadinessCheckStatus serverReadinessCheckStatus in statusList)
                    {
                        Logger.Debug("\r\n-------------------------------", ShowOutputOnConsole);
                        Logger.Debug("HostId: " + serverReadinessCheckStatus.HostId, ShowOutputOnConsole);
                        Logger.Debug("-------------------------------", ShowOutputOnConsole);
                        Logger.Debug("Readiness Check Status: " + serverReadinessCheckStatus.ReadinessCheckStatus, ShowOutputOnConsole);
                        Logger.Debug("Error Message:          " + serverReadinessCheckStatus.Message, ShowOutputOnConsole);                        
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nRollbackReadinessCheck API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus CreateRollback(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXCreateRollbackRequest req = new CXCreateRollbackRequest(
                section.GetKeyValue("rollbackplanid"),
                section.GetKeyValue("rollbackplanname"),
                section.GetKeyValue("protectionplanid"));

            req.SetRollbackOptions(
                section.GetKeyValue("multivmsyncrequired") == bool.TrueString ? true : false,
                Utils.ParseEnum<RecoveryPoint>(section.GetKeyValue("recoverypoint")),
                section.GetKeyValue("customtagname")
                );

            List<Key> hostKeys = section.GetKeyArray("host");
            if (hostKeys != null)
            {
                foreach (Key key in hostKeys)
                {
                    req.AddServerForRollback(key.GetSubKeyValue("hostid"), key.GetSubKeyValue("customtagname"));
                }
            }

            ResponseStatus responseStatus = new ResponseStatus();
            string requestId = client.ExecuteCXRequest(req, responseStatus);
            Logger.Debug("\r\n*************************** CreateRollback API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nCreateRollback API execution is successful. Request ID: " + requestId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(requestId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForRollback), requestId, this.AsyncFunctionTimeOut);                    
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nCreate Rollback plan failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRequestStatusForRollback(string requestId, out OperationStatus rollbackStatus)
        {
            JobStatus jobStatus = JobStatus.Failed;
            rollbackStatus = OperationStatus.Unknown;
            ResponseStatus responseStatus = new ResponseStatus();
            RollbackRequestStatus rollbackRequestStatus = client.GetRequestStatusForRollback(requestId, responseStatus);
            Logger.Debug("\r\n*************************** GetRequestStatus API Response for Create Rollback ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (rollbackRequestStatus != null)
                {
                    string protectionPlanKey = "rollback_plan_id";
                    if (this.placeholders.ContainsKey(protectionPlanKey) && String.IsNullOrEmpty(this.placeholders[protectionPlanKey]))
                    {
                        this.placeholders[protectionPlanKey] = rollbackRequestStatus.RollbackPlanId;
                    }
                    rollbackStatus = rollbackRequestStatus.RollbackStatus;
                    EnumerateRollbackStatus(rollbackRequestStatus);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nGetRequestStatus API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetRollbackState(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string[] hostIds = null;
            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null && hostIdKeys.Count > 0)
            {
                hostIds = new string[hostIdKeys.Count];
                int index = 0;
                foreach (Key key in hostIdKeys)
                {
                    hostIds[index] = key.Value;
                    ++index;
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            RollbackRequestStatus rollbackStatus = client.GetRollbackState(section.GetKeyValue("rollbackplanid"), hostIds, responseStatus);
            Logger.Debug("\r\n*************************** GetRollbackState API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (rollbackStatus != null)
                {
                    EnumerateRollbackStatus(rollbackStatus);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nGetRollbackState API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus CleanRollbackPlans(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string[] planIds = null;
            List<Key> planIdKeys = section.GetKeyArray("planid");
            if (planIdKeys != null && planIdKeys.Count > 0)
            {
                planIds = new string[planIdKeys.Count];
                int index = 0;
                foreach (Key key in planIdKeys)
                {
                    planIds[index] = key.Value;
                    ++index;
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            Dictionary<string, OperationStatus> statusList = client.CleanRollbackPlans(planIds, responseStatus);
            Logger.Debug("\r\n*************************** CleanRollbackPlans API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (statusList != null)
                {
                    foreach (string key in statusList.Keys)
                    {
                        Console.WriteLine();
                        Logger.Debug(String.Format("Delete Status of Plan ID {0} is {1}", key, statusList[key]), ShowOutputOnConsole);
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nCleanRollbackPlans API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus DiscoverInfrastructure(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXDiscoverInfrastructureRequest req = new CXDiscoverInfrastructureRequest();
            List<Key> infrastructureKeys = section.GetKeyArray("physicalinfrastructure");
            string infrastructureSource = null;
            if (infrastructureKeys != null)
            {
                foreach (Key key in infrastructureKeys)
                {
                    infrastructureSource = key.GetSubKeyValue("infrastructuresource");
                    if (String.IsNullOrEmpty(infrastructureSource))
                    {
                        req.AddPhysicalInfrastructure(key.GetSubKeyValue("sitename"),
                            IPAddress.Parse(key.GetSubKeyValue("serverip")),
                            key.GetSubKeyValue("hostname"),
                            key.GetSubKeyValue("os"));
                    }
                    else
                    {
                        req.AddPhysicalInfrastructure(key.GetSubKeyValue("sitename"),
                            IPAddress.Parse(key.GetSubKeyValue("serverip")),
                            key.GetSubKeyValue("hostname"),
                            key.GetSubKeyValue("os"),
                            Utils.ParseEnum<InfrastructureSource>(infrastructureSource));
                    }
                }
            }
            infrastructureKeys = section.GetKeyArray("vcenterinfrastructure");
            if (infrastructureKeys != null)
            {
                string accountId;
                foreach (Key key in infrastructureKeys)
                {
                    accountId = key.GetSubKeyValue("accountid");
                    if (String.IsNullOrEmpty(accountId))
                    {
                        req.AddVCenterInfrastructure(key.GetSubKeyValue("sitename"),                            
                            IPAddress.Parse(key.GetSubKeyValue("serverip")),
                            key.GetSubKeyValue("loginid"),
                            key.GetSubKeyValue("password"),
                            key.GetSubKeyValue("discoveryagent"),
                            Utils.ParseEnum<InfrastructureSource>(infrastructureSource));
                    }
                    else
                    {
                        req.AddVCenterInfrastructure(key.GetSubKeyValue("sitename"),                            
                            IPAddress.Parse(key.GetSubKeyValue("serverip")),
                            accountId,                            
                            key.GetSubKeyValue("discoveryagent"),
                            Utils.ParseEnum<InfrastructureSource>(infrastructureSource));
                    }
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            string requestId = client.ExecuteCXRequest(req, responseStatus);
            Logger.Debug("\r\n*************************** InfrastructureDiscovery API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                Logger.Debug("\r\nInfrastructureDiscovery API execution is successful. Request ID: " + requestId, ShowOutputOnConsole);
                if (!String.IsNullOrEmpty(requestId))
                {
                    jobStatus = JobMonitor.WaitForJobCompletion(new JobCompletionWaitCallback<string>(GetRequestStatusForInfrastructureDiscovery), requestId, this.AsyncFunctionTimeOut);                    
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nInfrastructureDiscovery API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus RemoveInfrastructure(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            ResponseStatus responseStatus = new ResponseStatus();
            string infraId = section.GetKeyValue("infrastructureid");
            bool removeInfrastructureStatus = client.RemoveInfrastructure(infraId, responseStatus);
            Logger.Debug("\r\n*************************** RemoveInfrastructure API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (removeInfrastructureStatus)
                {
                    Logger.Debug("\r\nSuccessfully removed Infrastructure Id: " + infraId, ShowOutputOnConsole);
                }
                else
                {
                    Logger.Error("\r\nFailed to remove Infrastructure Id: " + infraId, ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nRemoveInfrastructure API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus GetProtectionDetails(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string protectionPlanId = section.GetKeyValue("protectionplanid");
            string[] hostIds = null;
            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null)
            {
                hostIds = new string[hostIdKeys.Count];
                int index = 0;
                foreach (Key key in hostIdKeys)
                {
                    hostIds[index] = key.Value;
                    ++index;
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            List<ProtectionDetails> protectionDetailsList = client.GetProtectionDetails(protectionPlanId, hostIds, responseStatus);
            Logger.Debug("\r\n*************************** GetProtectionDetails API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                int count;
                if (protectionDetailsList != null && protectionDetailsList.Count > 0)
                {
                    foreach (ProtectionDetails protectionDetails in protectionDetailsList)
                    {
                        Logger.Debug("\r\n-----------------------------", ShowOutputOnConsole);
                        Logger.Debug("Plan Id: " + protectionDetails.ProtectionPlanId, ShowOutputOnConsole);
                        Logger.Debug("-----------------------------", ShowOutputOnConsole);
                        Logger.Debug("Protection Plan Id:      " + protectionDetails.ProtectionPlanId, ShowOutputOnConsole);
                        Logger.Debug("Solution Type:           " + protectionDetails.SolutionType, ShowOutputOnConsole);
                        Logger.Debug("Total Servers Protected: " + protectionDetails.TotalServersProtected, ShowOutputOnConsole);
                        Logger.Debug("Total Volumes Protected: " + protectionDetails.TotalVolumesProtected, ShowOutputOnConsole);
                        if (protectionDetails.HealthFactors != null)
                        {
                            Console.WriteLine();
                            Logger.Debug("Health Factors:", ShowOutputOnConsole);
                            count = 1;
                            foreach (HealthFactor hf in protectionDetails.HealthFactors)
                            {
                                Logger.Debug("\r\n\t -----------------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Health Factor: " + count, ShowOutputOnConsole);
                                Logger.Debug("\t -----------------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Type:        " + hf.Type.ToString(), ShowOutputOnConsole);
                                Logger.Debug("\t Impact:      " + hf.Impact.ToString(), ShowOutputOnConsole);
                                Logger.Debug("\t Description: " + hf.Description, ShowOutputOnConsole);
                                ++count;
                            }
                        }

                        if (protectionDetails.ProtectedServers != null)
                        {
                            Console.WriteLine();
                            Logger.Debug("Protected Servers:", ShowOutputOnConsole);
                            int protectedServerCount = 0;
                            foreach (ProtectedServer protectedServer in protectionDetails.ProtectedServers)
                            {
                                Logger.Debug("\r\n\t -----------------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Server " + (++protectedServerCount), ShowOutputOnConsole);
                                Logger.Debug("\t -----------------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Infrastructure VM Id:       " + protectedServer.InfrastructureVMId, ShowOutputOnConsole);
                                Logger.Debug("\t Resource Id:                " + protectedServer.ResourceId, ShowOutputOnConsole);
                                Logger.Debug("\t Server Host Id:             " + protectedServer.ServerHostId, ShowOutputOnConsole);
                                Logger.Debug("\t Server Host IP:             " + protectedServer.ServerHostIP, ShowOutputOnConsole);
                                Logger.Debug("\t Server Host Name:           " + protectedServer.ServerHostName, ShowOutputOnConsole);
                                Logger.Debug("\t Server OS:                  " + protectedServer.ServerOS, ShowOutputOnConsole);
                                Logger.Debug("\t RPO (in seconds):           " + protectedServer.RPOInSeconds, ShowOutputOnConsole);
                                Logger.Debug("\t Protection Stage:           " + protectedServer.ProtectionStage, ShowOutputOnConsole);
                                Logger.Debug("\t Resync Progress Percentage: " + protectedServer.ResyncProgressPercentage, ShowOutputOnConsole);
                                Logger.Debug("\t Resync Duration:            " + protectedServer.ResyncDuration, ShowOutputOnConsole);
                                if (protectedServer.HealthFactors != null)
                                {
                                    Console.WriteLine();
                                    Logger.Debug("\t Health Factors:", ShowOutputOnConsole);
                                    count = 1;
                                    foreach (HealthFactor hf in protectedServer.HealthFactors)
                                    {
                                        Logger.Debug("\r\n\t\t -----------------------------", ShowOutputOnConsole);
                                        Logger.Debug("\t\t Health Factor: " + count, ShowOutputOnConsole);
                                        Logger.Debug("\t\t -----------------------------", ShowOutputOnConsole);
                                        Logger.Debug("\r\n\t\t Type:    " + hf.Type.ToString(), ShowOutputOnConsole);
                                        Logger.Debug("\t\t Impact:      " + hf.Impact.ToString(), ShowOutputOnConsole);
                                        Logger.Debug("\t\t Description: " + hf.Description, ShowOutputOnConsole);
                                        ++count;
                                    }
                                }
                                if (protectedServer.RetentionWindow != null && protectedServer.RetentionWindow.From != null && protectedServer.RetentionWindow.To != null)
                                {
                                    Logger.Debug(String.Format("\r\n\t Retention available:           from {0} to {1}", protectedServer.RetentionWindow.From, protectedServer.RetentionWindow.To), ShowOutputOnConsole);
                                }
                                Logger.Debug("\t Daily Data Change Rate of Compressed Data (in MB):   " + protectedServer.DailyCompressedDataChangeInMb, ShowOutputOnConsole);
                                Logger.Debug("\t Daily Data Change Rate of Uncompressed Data (in MB): " + protectedServer.DailyUncompressedDataChangeInMb, ShowOutputOnConsole);

                                if (protectedServer.ProtectedDevices != null)
                                {
                                    Console.WriteLine();
                                    Logger.Debug("\t Protected Devices:", ShowOutputOnConsole);
                                    foreach (ProtectedDevice device in protectedServer.ProtectedDevices)
                                    {
                                        Logger.Debug("\r\n\t\t -----------------------------", ShowOutputOnConsole);
                                        Logger.Debug("\t\t Device Name: " + device.DeviceName, ShowOutputOnConsole);
                                        Logger.Debug("\t\t -----------------------------", ShowOutputOnConsole);
                                        Logger.Debug("\t\t Protection Stage:                   " + device.ProtectionStage, ShowOutputOnConsole);
                                        Logger.Debug("\t\t RPO (in seconds):                   " + device.RPOInSeconds, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Resync Progress Percentage:         " + device.ResyncProgressPercentage, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Resync Duration:                    " + device.ResyncDuration, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Resync Required Enabled:            " + device.ResyncRequiredEnabled, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Device Capacity (in bytes):         " + device.DeviceCapacityInBytes, ShowOutputOnConsole);
                                        Logger.Debug("\t\t FileSystem Capacity (in bytes):     " + device.FileSystemCapacityInBytes, ShowOutputOnConsole);

                                        Logger.Debug("\t\t Data in transit of Source (in MB):  " + device.DataInTransitOfSourceInMb, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Data in transit of PS (in MB):      " + device.DataInTransitOfPSInMb, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Data in transit of Target (in MB):  " + device.DataInTransitOfTargetInMb, ShowOutputOnConsole);

                                        Logger.Debug("\t\t Daily Data Change Rate of Compressed Data (in MB):   " + device.DailyCompressedDataChangeInMb, ShowOutputOnConsole);
                                        Logger.Debug("\t\t Daily Data Change Rate of Uncompressed Data (in MB): " + device.DailyUncompressedDataChangeInMb, ShowOutputOnConsole);                                        
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nGetProtectionDetails API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus SetPSNatIP(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            string errMsg;
            ResponseStatus responseStatus = new ResponseStatus();
            bool setNatIPStatus = client.SetNATIP(section.GetKeyValue("processserverid"),
                IPAddress.Parse(section.GetKeyValue("natip")),
                out errMsg,
                responseStatus);
            Logger.Debug("\r\n*************************** SetPSNatIP API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {                
                if (setNatIPStatus)
                {
                    Logger.Debug("\r\nSuccessfully set NAT IP Address of PS server.", ShowOutputOnConsole);
                    jobStatus = JobStatus.Completed;
                }
                else
                {
                    Logger.Error("\r\nFailed to set NAT IP Address of PS server. Error: " + errMsg, ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nSetPSNatIP API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus SetVPNDetails(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;            
            CXSetVPNDetailsRequest request = new CXSetVPNDetailsRequest(Utils.ParseEnum<ConnectivityType>(section.GetKeyValue("connectivitytype")));
            List<Key> serverKeys = section.GetKeyArray("server");
            IPAddress ipaddress;
            if (serverKeys != null)
            {
                foreach (Key key in serverKeys)
                {
                    if (key.HasSubKeys)
                    {
                        request.AddServer(key.GetSubKeyValue("hostid"),
                            IPAddress.TryParse(key.GetSubKeyValue("privateip"), out ipaddress) ? ipaddress : null,
                            key.GetSubKeyValue("privateport"),
                            key.GetSubKeyValue("privatesslport"),
                            IPAddress.TryParse(key.GetSubKeyValue("publicip"), out ipaddress) ? ipaddress : null,
                            key.GetSubKeyValue("publicport"),
                            key.GetSubKeyValue("publicsslport"));
                    }
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            bool setVPNDetailsStatus = client.SetVPNDetails(request, responseStatus);
            Logger.Debug("\r\n*************************** SetVPNDetails API Response ***************************", ShowOutputOnConsole);

            if (responseStatus.ReturnCode == 0)
            {
                if (setVPNDetailsStatus)
                {
                    Logger.Debug("\r\nSuccessfully set VPN details.", ShowOutputOnConsole);
                    jobStatus = JobStatus.Completed;
                }
                else
                {
                    Logger.Error("\r\nFailed to set VPN details.", ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nSetVPNDetails API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }           
            return jobStatus;
        }

        public JobStatus ListScoutComponents(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXListScoutComponentRequest request = new CXListScoutComponentRequest();
            string value = section.GetKeyValue("componenttype");
            if (!String.IsNullOrEmpty(value))
            {
                request.SetComponentType(Utils.ParseEnum<ScoutComponentType>(value));
            }

            value = section.GetKeyValue("os");
            if (!String.IsNullOrEmpty(value))
            {
                request.SetOSType(Utils.ParseEnum<OSType>(value));
            }

            List<Key> hostIdKeys = section.GetKeyArray("hostid");
            if (hostIdKeys != null)
            {
                foreach (Key key in hostIdKeys)
                {
                    request.AddServer(key.Value);
                }
            }
            ResponseStatus responseStatus = new ResponseStatus();
            ScoutComponents components = client.ListScoutComponents(request, responseStatus);
            Logger.Debug("\r\n*************************** ListScoutComponents API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                if (components != null)
                {
                    int count;
                    if(components.CS != null)
                    {
                        Logger.Debug("\r\n------------------------------", ShowOutputOnConsole);
                        Logger.Debug("Configuration Server IP: " + components.CS.HostIP, ShowOutputOnConsole);
                        Logger.Debug("------------------------------", ShowOutputOnConsole);
                        Logger.Debug("Host Name:      " + components.CS.HostName, ShowOutputOnConsole);
                        Logger.Debug("Host ID:        " + components.CS.HostId, ShowOutputOnConsole);
                        Logger.Debug("OS Type:        " + components.CS.OSType, ShowOutputOnConsole);
                        Logger.Debug("Version:        " + components.CS.Version, ShowOutputOnConsole);
                        Logger.Debug("Component Type: " + components.CS.ComponentType, ShowOutputOnConsole);
                        if (components.CS.InstalledUpdates != null)
                        {
                            count = 1;
                            Console.WriteLine();
                            Logger.Debug("Installed Updates:", ShowOutputOnConsole);
                            foreach (KeyValuePair<string, DateTime?> update in components.CS.InstalledUpdates)
                            {
                                Logger.Debug("\r\n\t -------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Update " + count, ShowOutputOnConsole);
                                Logger.Debug("\t -------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Version:           " + update.Key, ShowOutputOnConsole);
                                Logger.Debug("\t Installation Time: " + update.Value, ShowOutputOnConsole);
                                ++count;
                            }
                        }


                        if (components.CS.Accounts != null)
                        {                            
                            Console.WriteLine();
                            Logger.Debug("Accounts:", ShowOutputOnConsole);
                            foreach (KeyValuePair<string, string> account in components.CS.Accounts)
                            {
                                Logger.Debug("\r\n\t -------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Account Id: " + account.Key, ShowOutputOnConsole);
                                Logger.Debug("\t -------------------", ShowOutputOnConsole);
                                Logger.Debug("\t Account Name: " + account.Value, ShowOutputOnConsole);                                
                            }
                        }
                    }
                    
                    if (components.ProcessServers != null && components.ProcessServers.Count > 0)
                    {
                        Logger.Debug("\r\nProcess Servers:", ShowOutputOnConsole);
                        foreach (ProcessServer ps in components.ProcessServers)
                        {
                            Logger.Debug("\r\n------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Process Server IP: " + ps.IPAddress, ShowOutputOnConsole);
                            Logger.Debug("------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Host Name:      " + ps.HostName, ShowOutputOnConsole);
                            Logger.Debug("Host ID:        " + ps.HostId, ShowOutputOnConsole);
                            Logger.Debug("PS NAT IP:      " + ps.PSNatIP, ShowOutputOnConsole);
                            Logger.Debug("OS Type:        " + ps.OSType, ShowOutputOnConsole);
                            Logger.Debug("Version:        " + ps.Version, ShowOutputOnConsole);
                            Logger.Debug("Component Type: " + ps.ComponentType, ShowOutputOnConsole);
                            Logger.Debug("HeartBeat:      " + ps.HeartBeat, ShowOutputOnConsole);
                            if (ps.InstalledUpdates != null)
                            {
                                count = 1;
                                Console.WriteLine();
                                Logger.Debug("Installed Updates:", ShowOutputOnConsole);
                                foreach (KeyValuePair<string, DateTime?> update in ps.InstalledUpdates)
                                {
                                    Logger.Debug("\r\n\t -------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Update " + count, ShowOutputOnConsole);
                                    Logger.Debug("\t -------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Version:           " + update.Key, ShowOutputOnConsole);
                                    Logger.Debug("\t Installation Time: " + update.Value, ShowOutputOnConsole);
                                    ++count;
                                }
                            }
                        }
                    }
                    if (components.ScoutAgents != null && components.ScoutAgents.Count > 0)
                    {
                        Console.WriteLine();
                        Logger.Debug("Scout Agents:", ShowOutputOnConsole);
                        string agentKey = null;
                        foreach (ScoutAgent agent in components.ScoutAgents)
                        {
                            if (agent.IPAddress != null)
                            {
                                agentKey = "agent_id_" + agent.IPAddress.ToString();
                                if(this.placeholders.ContainsKey(agentKey) && String.IsNullOrEmpty(this.placeholders[agentKey]))
                                {
                                    this.placeholders[agentKey] = agent.HostId;
                                }
                            }
                            Logger.Debug("\r\n------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Scout Agent IP: " + agent.IPAddress, ShowOutputOnConsole);
                            Logger.Debug("------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Host Name:            " + agent.HostName, ShowOutputOnConsole);
                            Logger.Debug("Host ID:              " + agent.HostId, ShowOutputOnConsole);
                            Logger.Debug("Infrastructure VM Id: " + agent.InfrastructureVMId, ShowOutputOnConsole);
                            Logger.Debug("OS Type:              " + agent.OSType, ShowOutputOnConsole);
                            Logger.Debug("OS Version:           " + agent.OSVersion, ShowOutputOnConsole);
                            Logger.Debug("Server Type:          " + agent.ServerType, ShowOutputOnConsole);
                            Logger.Debug("Resource Id:          " + agent.ResourceId, ShowOutputOnConsole);
                            Logger.Debug("Vendor:               " + agent.Vendor, ShowOutputOnConsole);
                            Logger.Debug("Version:              " + agent.Version, ShowOutputOnConsole);
                            Logger.Debug("Component Type:       " + agent.ComponentType, ShowOutputOnConsole);
                            Logger.Debug("HeartBeat:            " + agent.HeartBeat, ShowOutputOnConsole);
                            Logger.Debug("Is Protected:         " + (agent.IsProtected ? "Yes" : "No"), ShowOutputOnConsole);
                            if (agent.DiskDetails != null && agent.DiskDetails.Count > 0)
                            {
                                Console.WriteLine();
                                Logger.Debug("Disk Details:", ShowOutputOnConsole);
                                foreach (DiskDetail disk in agent.DiskDetails)
                                {
                                    Logger.Debug("\r\n\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t ID: " + disk.Id, ShowOutputOnConsole);
                                    Logger.Debug("\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Disk Name:           " + disk.DiskName, ShowOutputOnConsole);
                                    Logger.Debug("\t Disk ID:             " + disk.DiskId, ShowOutputOnConsole);
                                    Logger.Debug("\t Capacity (in bytes): " + disk.Capacity, ShowOutputOnConsole);
                                    Logger.Debug("\t System Volume :      " + disk.SystemVolume, ShowOutputOnConsole);
                                    Logger.Debug("\t Is Dynamic Disk:     " + disk.IsDynamicDisk, ShowOutputOnConsole);
                                }
                            }
                            if (agent.HardwareConfiguration != null)
                            {
                                Console.WriteLine();
                                Logger.Debug("Hardware Configuration: ", ShowOutputOnConsole);
                                Logger.Debug("\t CPU Count:           " + agent.HardwareConfiguration.CPUCount, ShowOutputOnConsole);
                                Logger.Debug("\t Memory:              " + agent.HardwareConfiguration.Memory, ShowOutputOnConsole);
                            }
                            if (agent.NetworkConfigurations != null && agent.NetworkConfigurations.Count > 0)
                            {
                                Console.WriteLine();
                                Logger.Debug("Network Configurations:", ShowOutputOnConsole);
                                foreach (NetworkConfiguration networkConfig in agent.NetworkConfigurations)
                                {
                                    Logger.Debug("\r\n\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t ID: " + networkConfig.Id, ShowOutputOnConsole);
                                    Logger.Debug("\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t IPAddress:       " + networkConfig.IPAddress, ShowOutputOnConsole);
                                    Logger.Debug("\t Interface Name:  " + networkConfig.InterfaceName, ShowOutputOnConsole);
                                    Logger.Debug("\t MAC Address:     " + networkConfig.MACAddress, ShowOutputOnConsole);
                                    Logger.Debug("\t Subnet:          " + networkConfig.Subnet, ShowOutputOnConsole);
                                    Logger.Debug("\t Gateway:         " + networkConfig.Gateway, ShowOutputOnConsole);                                    
                                    if(networkConfig.DNS != null)
                                    {
                                        Logger.Debug("\t DNS:             ", ShowOutputOnConsole);
                                        for(int index = 0; index < networkConfig.DNS.Length; index++)
                                        {
                                            Logger.Debug("\t\t          " + networkConfig.DNS[index], ShowOutputOnConsole);
                                        }
                                    }
                                    Logger.Debug("\t Is DHCP Enabled: " + (networkConfig.IsDHCPEnabled ? "Yes" : "No"), ShowOutputOnConsole);
                                }
                            }
                            if (agent.InstalledUpdates != null)
                            {
                                count = 1;
                                Console.WriteLine();
                                Logger.Debug("Installed Updates:", ShowOutputOnConsole);
                                foreach (KeyValuePair<string, DateTime?> update in agent.InstalledUpdates)
                                {
                                    Logger.Debug("\r\n\t -------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Update " + count, ShowOutputOnConsole);
                                    Logger.Debug("\t -------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Version:           " + update.Key, ShowOutputOnConsole);
                                    Logger.Debug("\t Installation Time: " + update.Value, ShowOutputOnConsole);
                                    ++count;
                                }
                            }
                        }
                    }
                    if (components.MasterTargets != null && components.MasterTargets.Count > 0)
                    {
                        Logger.Debug("\r\nMaster Targets:", ShowOutputOnConsole);
                        string mtKey = null;
                        foreach (MasterTarget mt in components.MasterTargets)
                        {
                            if (mt.IPAddress != null)
                            {
                                mtKey = "mt_id_" + mt.IPAddress.ToString();
                                if (this.placeholders.ContainsKey(mtKey) && String.IsNullOrEmpty(this.placeholders[mtKey]))
                                {
                                    this.placeholders[mtKey] = mt.HostId;
                                }
                            }
                            Logger.Debug("\r\n------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Master Target IP: " + mt.IPAddress, ShowOutputOnConsole);
                            Logger.Debug("------------------------------", ShowOutputOnConsole);
                            Logger.Debug("Host Name:               " + mt.HostName, ShowOutputOnConsole);
                            Logger.Debug("Host ID:                 " + mt.HostId, ShowOutputOnConsole);
                            Logger.Debug("OS Type:                 " + mt.OSType, ShowOutputOnConsole);
                            Logger.Debug("OS Version:              " + mt.OSVersion, ShowOutputOnConsole);
                            Logger.Debug("Server Type:             " + mt.ServerType, ShowOutputOnConsole);
                            Logger.Debug("Vendor:                  " + mt.Vendor, ShowOutputOnConsole);
                            Logger.Debug("Total Protected Volumes: " + mt.ProtectedVolumesCount, ShowOutputOnConsole);
                            Logger.Debug("Version:                 " + mt.Version, ShowOutputOnConsole);
                            Logger.Debug("Component Type:          " + mt.ComponentType, ShowOutputOnConsole);
                            Logger.Debug("HeartBeat:               " + mt.HeartBeat, ShowOutputOnConsole);
                            if (mt.RetentionVolumes != null && mt.RetentionVolumes.Count > 0)
                            {
                                Console.WriteLine();
                                Logger.Debug("Retention Volumes:", ShowOutputOnConsole);
                                foreach (RetentionVolume retentionVolume in mt.RetentionVolumes)
                                {
                                    Logger.Debug("\r\n\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t ID: " + retentionVolume.Id, ShowOutputOnConsole);
                                    Logger.Debug("\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Volume Name:               " + retentionVolume.VolumeName, ShowOutputOnConsole);
                                    Logger.Debug("\t Capacity (in bytes):       " + retentionVolume.Capacity, ShowOutputOnConsole);
                                    Logger.Debug("\t FreeSpace (in bytes):      " + retentionVolume.FreeSpace, ShowOutputOnConsole);
                                    Logger.Debug("\t Retention Space Threshold: " + retentionVolume.RetentionSpaceThreshold, ShowOutputOnConsole);
                                }
                            }
                            if (mt.InstalledUpdates != null)
                            {
                                count = 1;
                                Console.WriteLine();
                                Logger.Debug("Installed Updates:", ShowOutputOnConsole);
                                foreach (KeyValuePair<string, DateTime?> update in mt.InstalledUpdates)
                                {
                                    Logger.Debug("\r\n\t -------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Update " + count, ShowOutputOnConsole);
                                    Logger.Debug("\t -------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t Version:           " + update.Key, ShowOutputOnConsole);
                                    Logger.Debug("\t Installation Time: " + update.Value, ShowOutputOnConsole);
                                    ++count;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nListScoutComponents API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus ListInfrastructure(Section section)
        {
            JobStatus jobStatus = JobStatus.Failed;
            CXListInfrastructureRequest request = new CXListInfrastructureRequest();
            string value = section.GetKeyValue("infrastructuretype");
            if (!String.IsNullOrEmpty(value))
            {
                request.SetInfrastructureType(Utils.ParseEnum<InfrastructureType>(value));
            }

            value = section.GetKeyValue("infrastructureid");
            if (!String.IsNullOrEmpty(value))
            {
                request.SetInfrastructureId(value);
            }
            ResponseStatus responseStatus = new ResponseStatus();
            List<Infrastructure> infrastructures = client.ListInfrastructures(request, responseStatus);
            Logger.Debug("\r\n*************************** ListInfrastructure API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                jobStatus = JobStatus.Completed;
                OperationStatus infrastructureDiscoveryStatus;
                EnumerateInfrastructure(infrastructures, out infrastructureDiscoveryStatus);
            }
            else
            {
                Logger.Error(String.Format("\r\nListInfrastructure API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }

        public JobStatus PSFailover(Section section)
        {
            string newPSId = section.GetKeyValue("newprocessserver");
            PSFailoverRequest req = new PSFailoverRequest(
                Utils.ParseEnum<FailoverType>(section.GetKeyValue("failovertype")),
                section.GetKeyValue("oldprocessserver"),
                newPSId);

            List<Key> hostKeys = section.GetKeyArray("host");
            if (hostKeys != null)
            {
                foreach (Key hostKey in hostKeys)
                {
                    req.AddServer(hostKey.GetSubKeyValue("hostid"), hostKey.GetSubKeyValue("newprocessserver"));
                }
            }

            ResponseStatus responseStatus = new ResponseStatus();
            JobStatus jobStatus = client.PSFailover(req, responseStatus) ? JobStatus.Completed : JobStatus.Failed;
            Logger.Debug("\r\n*************************** PSFailover API Response ***************************", ShowOutputOnConsole);
            if (responseStatus.ReturnCode == 0)
            {
                if (jobStatus == JobStatus.Completed)
                {
                    Logger.Debug("\r\nPS Failover is successful.", ShowOutputOnConsole);
                }
                else
                {
                    Logger.Error("\r\nPS Failover has failed.", ShowOutputOnConsole);
                }
            }
            else
            {
                Logger.Error(String.Format("\r\nPSFailover API failed. Error {0}: {1}", responseStatus.ReturnCode, responseStatus.Message), ShowOutputOnConsole);
            }
            return jobStatus;
        }
        
        private void EnumerateProtectionStatus(ProtectionRequestStatus protectionRequestStatus, out OperationStatus protectionStatus)
        {
            protectionStatus = OperationStatus.Unknown;
            if (protectionRequestStatus == null)
            {
                Logger.Debug("\r\nProtection status not found.", ShowOutputOnConsole);
            }
            else
            {
                string protectionPlanKey = "protection_plan_id";
                if (this.placeholders != null && this.placeholders.ContainsKey(protectionPlanKey) && String.IsNullOrEmpty(this.placeholders[protectionPlanKey]))
                {
                    this.placeholders[protectionPlanKey] = protectionRequestStatus.ProtectionPlanId;
                }
                Logger.Debug("\r\nProtection Plan Id:" + protectionRequestStatus.ProtectionPlanId, ShowOutputOnConsole);
                
                foreach (ServerProtectionStatus serverStatus in protectionRequestStatus.ProtectionStatusOfServers)
                {
                    Logger.Debug("\r\n\t -------------------------------", ShowOutputOnConsole);
                    Logger.Debug("\t Host ID:" + serverStatus.HostId, ShowOutputOnConsole);
                    Logger.Debug("\t -------------------------------", ShowOutputOnConsole);
                    Logger.Debug("\t Prepare Target Status:           " + serverStatus.PrepareTargetStatus, ShowOutputOnConsole);
                    Logger.Debug("\t Create Replication Pairs Status: " + serverStatus.CreateReplicationPairsStatus, ShowOutputOnConsole);
                    Logger.Debug("\t Protection Stage:                " + serverStatus.ProtectionStage, ShowOutputOnConsole);

                    // Find the overall status based on the individual server status.
                    if(serverStatus.PrepareTargetStatus == OperationStatus.Unknown)
                    {
                        protectionStatus = OperationStatus.Failed;
                    }
                    else if ((serverStatus.PrepareTargetStatus >= OperationStatus.InProgress && serverStatus.PrepareTargetStatus > protectionStatus) ||
                        (protectionStatus < OperationStatus.Failed))
                    {
                        // Check if the status of one of the servers is Pending\InProgress\Failed then the overall status must be Pending\InProgress\Failed.
                        protectionStatus = serverStatus.PrepareTargetStatus;
                    }
                    if(protectionStatus != OperationStatus.Completed && protectionStatus != OperationStatus.Success)
                    {
                        continue;
                    }
                    if (serverStatus.PrepareTargetStatus == OperationStatus.Completed || serverStatus.PrepareTargetStatus == OperationStatus.Success)
                    {
                        if (String.Equals(serverStatus.CreateReplicationPairsStatus, "Complete", StringComparison.OrdinalIgnoreCase))
                        {
                            protectionStatus = serverStatus.ProtectionStage == ProtectionStatus.DifferentialSync ? OperationStatus.Completed : OperationStatus.InProgress;
                        }
                        else
                        {
                            protectionStatus = OperationStatus.Pending;
                        }
                    }
                    DisplayErrorDetails(serverStatus.ProtectionErrorDetails, "\t ");
                }
            }
        }

        private void EnumerateInfrastructure(List<Infrastructure> infrastructures, out OperationStatus infrastructureDiscoveryStatus)
        {
            infrastructureDiscoveryStatus = OperationStatus.Unknown;
            if (infrastructures != null && infrastructures.Count > 0)
            {
                foreach (Infrastructure infra in infrastructures)
                {
                    Logger.Debug("\r\n------------------------------", ShowOutputOnConsole);
                    Logger.Debug("Infrastructure ID: " + infra.InfrastructureId, ShowOutputOnConsole);
                    Logger.Debug("------------------------------", ShowOutputOnConsole);
                    Logger.Debug("Discovery Type:    " + infra.DiscoveryType, ShowOutputOnConsole);
                    Logger.Debug("Reference:         " + infra.Reference, ShowOutputOnConsole);
                    Logger.Debug("Discovery Status:  " + infra.DiscoveryStatus, ShowOutputOnConsole);
                    Logger.Debug("Discovery Error:  " + infra.DiscoveryErrorLog, ShowOutputOnConsole);
                    Logger.Debug("LastDiscoveryTime: " + (infra.LastDiscoveryTime == null ? String.Empty : infra.LastDiscoveryTime.ToString()), ShowOutputOnConsole);
                    DisplayErrorDetails(infra.DiscoveryErrorDetails, null);
                    if (infra.PhysicalServers != null && infra.PhysicalServers.Count > 0)
                    {
                        Logger.Debug("Physical Servers:", ShowOutputOnConsole);
                        foreach (SourceServer sourceServer in infra.PhysicalServers)
                        {
                            Logger.Debug("\r\n\t ------------------------------", ShowOutputOnConsole);
                            Logger.Debug("\t ID: " + sourceServer.Id, ShowOutputOnConsole);
                            Logger.Debug("\t ------------------------------", ShowOutputOnConsole);
                            Logger.Debug("\t Physical Server IP:   " + sourceServer.ServerIP, ShowOutputOnConsole);
                            Logger.Debug("\t Display Name:         " + sourceServer.DisplayName, ShowOutputOnConsole);
                            Logger.Debug("\t OS:                   " + sourceServer.OS, ShowOutputOnConsole);
                            Logger.Debug("\t Infrastructure VM Id: " + sourceServer.InfrastructureVMId, ShowOutputOnConsole);
                            Logger.Debug("\t Powered On:           " + sourceServer.PoweredOn, ShowOutputOnConsole);
                            Logger.Debug("\t Agent Installed:      " + sourceServer.AgentInstalled, ShowOutputOnConsole);
                            Logger.Debug("\t Is Protected:         " + sourceServer.Protected, ShowOutputOnConsole);
                        }
                    }

                    if (infra.VirtualizedHosts != null && infra.VirtualizedHosts.Count > 0)
                    {
                        Logger.Debug("Virtual Hosts:", ShowOutputOnConsole);
                        foreach (VirtualizedHostServer virtualHost in infra.VirtualizedHosts)
                        {
                            Logger.Debug("\r\n\t ------------------------------", ShowOutputOnConsole);
                            Logger.Debug("\t ID: " + virtualHost.Id, ShowOutputOnConsole);
                            Logger.Debug("\t ------------------------------", ShowOutputOnConsole);
                            Logger.Debug("\t Virtual Host Name:   " + virtualHost.ServerName, ShowOutputOnConsole);
                            Logger.Debug("\t Virtual Host IP:     " + virtualHost.ServerIP, ShowOutputOnConsole);
                            Logger.Debug("\t Discovery Type:      " + virtualHost.DiscoveryType, ShowOutputOnConsole);
                            Logger.Debug("\t Organization:        " + virtualHost.Organization, ShowOutputOnConsole);
                            Logger.Debug("\t Last Discovery Time: " + virtualHost.LastDiscoveryTime, ShowOutputOnConsole);
                            Logger.Debug("\t Discovery Error Log: " + virtualHost.DiscoveryErrorLog, ShowOutputOnConsole);
                            if (virtualHost.Servers != null && virtualHost.Servers.Count > 0)
                            {
                                Logger.Debug("\t Source Servers:", ShowOutputOnConsole);
                                foreach (SourceServer sourceServer in virtualHost.Servers)
                                {
                                    Logger.Debug("\r\n\t\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t\t ID: " + sourceServer.Id, ShowOutputOnConsole);
                                    Logger.Debug("\t\t ------------------------------", ShowOutputOnConsole);
                                    Logger.Debug("\t\t Source Server IP:     " + sourceServer.ServerIP, ShowOutputOnConsole);
                                    Logger.Debug("\t\t Display Name:         " + sourceServer.DisplayName, ShowOutputOnConsole);
                                    Logger.Debug("\t\t Infrastructure VM Id: " + sourceServer.InfrastructureVMId, ShowOutputOnConsole);
                                    Logger.Debug("\t\t OS:                   " + sourceServer.OS, ShowOutputOnConsole);
                                    Logger.Debug("\t\t Powered On:           " + sourceServer.PoweredOn, ShowOutputOnConsole);
                                    Logger.Debug("\t\t Agent Installed:      " + sourceServer.AgentInstalled, ShowOutputOnConsole);
                                    Logger.Debug("\t\t Is Protected:         " + sourceServer.Protected, ShowOutputOnConsole);
                                }
                            }
                        }
                    }
                    // Find the overall status based on the individual server status.                            
                    if ((infra.DiscoveryStatus >= OperationStatus.InProgress && infra.DiscoveryStatus > infrastructureDiscoveryStatus) ||
                        (infrastructureDiscoveryStatus < OperationStatus.Failed))
                    {
                        // Check if the status of one of the servers is Pending\InProgress\Failed then the overall status must be Pending\InProgress\Failed.
                        infrastructureDiscoveryStatus = infra.DiscoveryStatus;
                    }
                }
            }
            else
            {
                Logger.Debug("\r\n No Infrastructure found.", ShowOutputOnConsole);
            }
        }

        private void EnumerateRollbackStatus(RollbackRequestStatus rollbackStatus)
        {
            Console.WriteLine();
            Logger.Debug("Rollback Plan Id: " + rollbackStatus.RollbackPlanId, ShowOutputOnConsole);
            //Logger.Debug("Rollback Readiness Check Status: " + rollbackStatus.ReadinessCheckStatus, ShowOutputOnConsole);                    
            Logger.Debug("Rollback Status:  " + rollbackStatus.RollbackStatus, ShowOutputOnConsole);

            if (rollbackStatus.RollbackStatusOfServers != null)
            {
                foreach (ServerRollbackStatus serverRollbackStatus in rollbackStatus.RollbackStatusOfServers)
                {
                    Console.WriteLine();
                    Logger.Debug("\t ------------------------------", ShowOutputOnConsole);
                    Logger.Debug("\t Host ID: " + serverRollbackStatus.HostId, ShowOutputOnConsole);
                    Logger.Debug("\t ------------------------------", ShowOutputOnConsole);

                    Logger.Debug("\t Rollback Status: " + serverRollbackStatus.RollbackStatus, ShowOutputOnConsole);
                    Logger.Debug("\t Tag Name:        " + serverRollbackStatus.TagName, ShowOutputOnConsole);
                    DisplayErrorDetails(serverRollbackStatus.RollbackErrorDetails, "\t ");
                }
            }
        }

        private void DisplayErrorDetails(ErrorDetails errorDetails, string indent)
        {
            if(errorDetails != null)
            {
                Logger.Error(indent + "Error Details:", ShowOutputOnConsole);
                Logger.Error(String.Format("{0}\tError Code: {1}", indent, errorDetails.ErrorCode), ShowOutputOnConsole);
                Logger.Error(String.Format("{0}\tError Message: {1}", indent, errorDetails.ErrorMessage), ShowOutputOnConsole);
                Logger.Error(String.Format("{0}\tPossibleCauses: {1}", indent, errorDetails.PossibleCauses), ShowOutputOnConsole);
                Logger.Error(String.Format("{0}\tRecommendation: {1}", indent, errorDetails.Recommendation), ShowOutputOnConsole);
                if(errorDetails.Placeholders != null && errorDetails.Placeholders.Count > 0)
                {
                    Logger.Error(indent + "\tPlaceholders:", ShowOutputOnConsole);
                    foreach(string key in errorDetails.Placeholders.Keys)
                    {
                        Logger.Error(String.Format("{0}\t\t{1} <=> {2}", indent, key, errorDetails.Placeholders[key]), ShowOutputOnConsole);
                    }
                }
            }
        }
    }
}
