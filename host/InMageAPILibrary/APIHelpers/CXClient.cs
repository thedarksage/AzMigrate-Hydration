using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;
using InMage.Logging;

namespace InMage.APIHelpers
{
    public class CXClient
    {
        private string cxIp;
        private int cxPort;
        private string cxProtocol;
        private AuthMethod authMethod;
        private string accessKeyId;
        private string passphrase;
        private string fingerPrint;
        private string userName;
        private string password;
        private bool useCustomAuthValues;        
        private bool isCXIPAvailable;

        public string ActivityId { get; set; }
        public string ClientRequestId { get; set; }

        public CXClient()
        {
            this.isCXIPAvailable = false;            
        }

        public CXClient(string cxIp, int cxPort, CXProtocol cxProtocol)
        {
            this.cxIp = cxIp;
            this.cxPort = cxPort;
            this.cxProtocol = cxProtocol.ToString();
            this.useCustomAuthValues = false;
            this.isCXIPAvailable = true;
        }

        public CXClient(IPAddress cxIp, int cxPort, CXProtocol cxProtocol, AuthMethod authMethod, string accessKeyId, string passphrase, string fingerPrint,
            string userName, string password)
        {
            this.cxIp = cxIp.ToString();
            this.cxPort = cxPort;
            this.cxProtocol = cxProtocol.ToString();
            this.authMethod = authMethod;
            this.accessKeyId = accessKeyId;
            this.passphrase = passphrase;
            this.fingerPrint = fingerPrint;
            this.userName = userName;
            this.password = password;            
            this.useCustomAuthValues = true;
            this.isCXIPAvailable = true;
        }

        public string ExecuteCXRequest(ICXRequest cxRequest, ResponseStatus responseStatus)
        {
            string requestId = null;           
            ArrayList functionRequests = ConstructFunctionRequest(cxRequest.Request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, cxRequest.Request.Name, responseStatus))
            {
                requestId = ResponseParser.GetRequestIdFromResponse(response, cxRequest.Request.Name);
            }
            else
            {
                Logger.Error("Response validation failed");
            }

            return requestId;
        }

        public List<Infrastructure> GetRequestStatusForInfratructureDiscovery(string requestId, ResponseStatus responseStatus)
        {
            List<Infrastructure> discoveredInfraStructures = null;
            Response response = GetRequestStatus(requestId);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionGetRequestStatus, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetRequestStatus);
                if (functionResponse != null)
                {
                    ParameterGroup actualResponse = ResponseParser.GetParameterGroup(functionResponse.ChildList, requestId);
                    if (actualResponse != null)
                    {
                        discoveredInfraStructures = new List<Infrastructure>();
                        foreach (var item in actualResponse.ChildList)
                        {
                            if (item is ParameterGroup)
                            {
                                discoveredInfraStructures.Add(new Infrastructure(item as ParameterGroup));
                            }
                        }
                    }
                }
            }

            return discoveredInfraStructures;
        }

        public ProtectionRequestStatus GetRequestStatusForProtection(string requestId, ResponseStatus responseStatus)
        {
            ProtectionRequestStatus protectionRequestStatus = null;
            Response response = GetRequestStatus(requestId);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionGetRequestStatus, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetRequestStatus);
                if (functionResponse != null)
                {
                    ParameterGroup actualResponse = ResponseParser.GetParameterGroup(functionResponse.ChildList, requestId);
                    if (actualResponse != null)
                    {
                        protectionRequestStatus = new ProtectionRequestStatus(actualResponse);
                    }
                }
            }

            return protectionRequestStatus;
        }

        public RollbackRequestStatus GetRequestStatusForRollback(string requestId, ResponseStatus responseStatus)
        {
            RollbackRequestStatus rollbackRequestStatus = null;
            Response response = GetRequestStatus(requestId);            
            if (ResponseParser.ValidateResponse(response, Constants.FunctionGetRequestStatus, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetRequestStatus);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    foreach (var item in functionResponse.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            rollbackRequestStatus = new RollbackRequestStatus(item as ParameterGroup);
                            break;
                        }
                    }
                }
            }

            return rollbackRequestStatus;
        }

        public RemoveProtectionRequestStatus GetRequestStatusForRemoveProtection(string requestId, ResponseStatus responseStatus)
        {
            RemoveProtectionRequestStatus removeProtectionRequestStatus = null;
            Response response = GetRequestStatus(requestId);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionGetRequestStatus, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetRequestStatus);
                if (functionResponse != null)
                {
                    ParameterGroup requestStatusPG = ResponseParser.GetParameterGroup(functionResponse.ChildList, requestId);
                    if (requestStatusPG != null)
                    {
                        removeProtectionRequestStatus = new RemoveProtectionRequestStatus(requestStatusPG);
                    }
                }
            }

            return removeProtectionRequestStatus;
        }

        public List<MobilityServiceInstallStatus> GetRequestStatusForInstallMobilityService(string requestId, ResponseStatus responseStatus)
        {
            List<MobilityServiceInstallStatus> installStatusOfServers = null;
            Response response = GetRequestStatus(requestId);            
            if (ResponseParser.ValidateResponse(response, Constants.FunctionGetRequestStatus, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetRequestStatus);
                ParameterGroup requestStatusPG = ResponseParser.GetParameterGroup(functionResponse.ChildList, requestId);
                if (requestStatusPG != null)
                {
                    installStatusOfServers = new List<MobilityServiceInstallStatus>();
                    foreach(ParameterGroup serverStatus in requestStatusPG.ChildList)
                    {
                        installStatusOfServers.Add(new MobilityServiceInstallStatus(serverStatus));
                    }
                }
            }

            return installStatusOfServers;
        }

        public List<MobilityServiceUnInstallStatus> GetRequestStatusForUnInstallMobilityService(string requestId, ResponseStatus responseStatus)
        {
            List<MobilityServiceUnInstallStatus> unInstallStatusOfServers = null;
            Response response = GetRequestStatus(requestId);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionGetRequestStatus, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetRequestStatus);
                ParameterGroup requestStatusPG = ResponseParser.GetParameterGroup(functionResponse.ChildList, requestId);
                if (requestStatusPG != null)
                {
                    unInstallStatusOfServers = new List<MobilityServiceUnInstallStatus>();
                    foreach (ParameterGroup serverStatus in requestStatusPG.ChildList)
                    {
                        unInstallStatusOfServers.Add(new MobilityServiceUnInstallStatus(serverStatus));
                    }
                }
            }

            return unInstallStatusOfServers;
        }

        public RollbackRequestStatus GetRollbackState(string rollbackPlanId, string[] hostIds, ResponseStatus responseStatus)
        {
            RollbackRequestStatus rollbackRequestStatus = null;
            FunctionRequest request = new FunctionRequest();
            
            request.Name = Constants.FuntionGetRollbackState;
            request.AddParameter("RollbackPlanId", rollbackPlanId);            
            if (hostIds != null)
            {
                int count = 1;
                ParameterGroup servers = new ParameterGroup("Servers");                
                foreach (string hostId in hostIds)
                {
                    servers.AddParameter("Server" + count, hostId);
                    ++count;
                }
                request.AddParameterGroup(servers);
            }           
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FuntionGetRollbackState);               
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    foreach (var item in functionResponse.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            rollbackRequestStatus = new RollbackRequestStatus(item as ParameterGroup);
                            break;
                        }                        
                    }
                }
            }

            return rollbackRequestStatus;
        }

        public ProtectionRequestStatus GetProtectionState(string protectionPlanId, string[] hostIds, ResponseStatus responseStatus)
        {
            ProtectionRequestStatus protectionRequestStatus = null;
            FunctionRequest request = new FunctionRequest();            
            request.Name = Constants.FunctionGetProtectionState;
            request.AddParameter("ProtectionPlanId", protectionPlanId);
            if (hostIds != null)
            {
                int count = 1;
                ParameterGroup servers = new ParameterGroup("Servers");
                foreach (string hostId in hostIds)
                {
                    servers.AddParameter("SourceHostId" + count, hostId);
                    ++count;
                }
                request.AddParameterGroup(servers);
            }
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetProtectionState);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    ParameterGroup protectionStatusPG = ResponseParser.GetParameterGroup(functionResponse.ChildList, protectionPlanId);
                    if (protectionStatusPG != null)
                    {
                        protectionRequestStatus = new ProtectionRequestStatus(protectionStatusPG);
                    }
                }
            }

            return protectionRequestStatus;
        }

        public List<Infrastructure> ListInfrastructures(CXListInfrastructureRequest listInfrastructureRequest, ResponseStatus responseStatus)
        {
            List<Infrastructure> infraStructures = null;
            ArrayList functionRequests = ConstructFunctionRequest(listInfrastructureRequest.Request);
            Response response = Post(functionRequests);

            if (ResponseParser.ValidateResponse(response, Constants.FunctionListInfrastructure, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionListInfrastructure);
                if (functionResponse != null)
                {
                    infraStructures = new List<Infrastructure>();
                    foreach (var item in functionResponse.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            infraStructures.Add(new Infrastructure(item as ParameterGroup));
                        }
                    }
                }
            }

            return infraStructures;
        }

        public bool SetNATIP(string processServerId, IPAddress natIP, out string errorMessage, ResponseStatus responseStatus)
        {
            bool setStatus = false;
            errorMessage = null;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionSetPSNatIP;
            ParameterGroup processServerNATIPPair = new ParameterGroup("ProcessServer1");
            request.AddParameterGroup(processServerNATIPPair);
            processServerNATIPPair.AddParameter("HostId", processServerId);
            processServerNATIPPair.AddParameter("NatIPAddress", natIP.ToString());

            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);

            if (ResponseParser.ValidateResponse(response, Constants.FunctionSetPSNatIP, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionSetPSNatIP);
                if (functionResponse != null)
                {
                    ParameterGroup processServerStatus = functionResponse.GetParameterGroup("ProcessServer1");
                    errorMessage = processServerStatus.GetParameterValue("ErrorMessage");
                    setStatus = processServerStatus.GetParameterValue("Status") == "Success" ? true : false;
                }
            }

            return setStatus;
        }

        public bool SetVPNDetails(CXSetVPNDetailsRequest setVPNDetailsRequest, ResponseStatus responseStatus)
        {
            bool setStatus = false;
            ArrayList functionRequests = ConstructFunctionRequest(setVPNDetailsRequest.Request);
            Response response = Post(functionRequests);

            if (ResponseParser.ValidateResponse(response, Constants.FunctionSetVPNDetails, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionSetVPNDetails);
                if (functionResponse != null && functionResponse.ChildList != null && functionResponse.ChildList.Count > 0)
                {
                    setStatus = functionResponse.GetParameterValue("Status") == "Success" ? true : false;
                }
            }

            return setStatus;
        }

        public ScoutComponents ListScoutComponents(CXListScoutComponentRequest listScoutComponentRequest, ResponseStatus responseStatus)
        {
            ScoutComponents scoutComponents = null;
            ArrayList functionRequests = ConstructFunctionRequest(listScoutComponentRequest.Request);
            Response response = Post(functionRequests);

            if (ResponseParser.ValidateResponse(response, Constants.FunctionListScoutComponents, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionListScoutComponents);
                if (functionResponse != null)
                {                    
                    scoutComponents = new ScoutComponents(functionResponse);
                }
            }

            return scoutComponents;
        }

        public EventList ListEvents(string category, EventSeverity severity, string token, ResponseStatus responseStatus)
        {
            EventList events = null;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionListEvents;
            if (!String.IsNullOrEmpty(category))
            {
                request.AddParameter("Category", category);
            }
            request.AddParameter("Severity", severity.ToString());
            if (!String.IsNullOrEmpty(token))
            {
                request.AddParameter("Token", token);
            }
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionListEvents);
                events = new EventList(functionResponse);
            }

            return events;
        }

        public Dictionary<string, OperationStatus> CleanRollbackPlans(string[] planIds, ResponseStatus responseStatus)
        {
            Dictionary<string, OperationStatus> stalePlansStatus = null;
            FunctionRequest request = new FunctionRequest();            
            request.Name = Constants.FunctionCleanRollbackPlans;
            if (planIds != null)
            {
                int count = 1;
                foreach (string planId in planIds)
                {
                    request.AddParameter("PlanId" + count, planId);
                    ++count;
                }
            }
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);

            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, request.Name);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    stalePlansStatus = new Dictionary<string, OperationStatus>();
                    foreach(var item in functionResponse.ChildList)
                    {
                        if(item is Parameter)
                        {
                            Parameter plasnStatus = item as Parameter;
                            stalePlansStatus.Add( plasnStatus.Name, Utils.ParseDeleteStatus(plasnStatus.Value));
                        }
                    }
                }
            }

            return stalePlansStatus;
        }

        public bool RemoveInfrastructure(string infrastructureId, ResponseStatus responseStatus)
        {
            bool setStatus = false;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionRemoveInfrastructure;
            request.AddParameter("InfrastructureId", infrastructureId);
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionRemoveInfrastructure);
                setStatus = String.Compare(functionResponse.GetParameterValue("RemoveInfrastructure"), "Success", StringComparison.OrdinalIgnoreCase) == 0 ? true : false;
            }

            return setStatus;
        }

        public bool ModifyProtectionProperties(CXModifyProtectionPropertiesRequest modifyProtectionPropertiesRequest, ResponseStatus responseStatus)
        {
            bool setStatus = false;
            ArrayList functionRequests = ConstructFunctionRequest(modifyProtectionPropertiesRequest.Request);            
            Response response = Post(functionRequests);            
            if (ResponseParser.ValidateResponse(response, Constants.FunctionModifyProtectionProperties, responseStatus))
            {                
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionModifyProtectionProperties);                
                if (functionResponse != null)
                {                    
                    setStatus = String.Compare(functionResponse.GetParameterValue("ModifyProtectionProperties"), "Success", StringComparison.OrdinalIgnoreCase) == 0 ? true : false;
                }
            }

            return setStatus;
        }

        public bool ModifyProtectionPropertiesV2(CXModifyProtectionPropertiesV2Request modifyProtectionPropertiesRequestV2, ResponseStatus responseStatus)
        {
            bool setStatus = false;
            ArrayList functionRequests = ConstructFunctionRequest(modifyProtectionPropertiesRequestV2.Request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionModifyProtectionPropertiesV2, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionModifyProtectionPropertiesV2);
                if (functionResponse != null)
                {
                    setStatus = String.Compare(functionResponse.GetParameterValue("Status"), "Success", StringComparison.OrdinalIgnoreCase) == 0 ? true : false;
                }
            }

            return setStatus;
        }

        public List<ProtectionReadinessCheckStatus> CheckForProtectionReadiness(CXProtectionReadinessCheckRequest protectionReadinessCheckRequest, ResponseStatus responseStatus)
        {
            List<ProtectionReadinessCheckStatus> readinessCheckStatusOfConfigurations = null;
            ProtectionReadinessCheckStatus configurationReadinessCheckStatus;
            ArrayList functionRequests = ConstructFunctionRequest(protectionReadinessCheckRequest.Request);
            Response response = Post(functionRequests);            
            if (ResponseParser.ValidateResponse(response, Constants.FunctionProtectionReadinessCheck, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionProtectionReadinessCheck);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    readinessCheckStatusOfConfigurations = new List<ProtectionReadinessCheckStatus>();
                    foreach (var item in functionResponse.ChildList)
                    {                        
                        configurationReadinessCheckStatus = new ProtectionReadinessCheckStatus(item as ParameterGroup);
                        readinessCheckStatusOfConfigurations.Add(configurationReadinessCheckStatus);
                    }
                }
            }

            return readinessCheckStatusOfConfigurations;
        }

        public List<ServerReadinessCheckStatus> CheckForRollbackReadiness(CXRollbackReadinessCheckRequest rollbackReadinessCheckRequest, ResponseStatus responseStatus)
        {
            List<ServerReadinessCheckStatus> rollbackReadinessCheckStatusOfServers = null;            
            ArrayList functionRequests = ConstructFunctionRequest(rollbackReadinessCheckRequest.Request);
            Response response = Post(functionRequests);            
            if (ResponseParser.ValidateResponse(response, Constants.FunctionRollbackReadinessCheck, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionRollbackReadinessCheck);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    rollbackReadinessCheckStatusOfServers = new List<ServerReadinessCheckStatus>();
                    ParameterGroup serversPG = functionResponse.GetParameterGroup("Servers");
                    if (serversPG != null)
                    {
                        ParameterGroup serverReadinessCheckStatusPG;
                        ServerReadinessCheckStatus serverReadinessCheckStatus;
                        foreach (var item in serversPG.ChildList)
                        {
                            if (item is ParameterGroup)
                            {
                                serverReadinessCheckStatusPG = item as ParameterGroup;
                                serverReadinessCheckStatus = new ServerReadinessCheckStatus(
                                    serverReadinessCheckStatusPG.GetParameterValue("HostId"),
                                    serverReadinessCheckStatusPG.GetParameterValue("Status"),
                                    serverReadinessCheckStatusPG.GetParameterValue("ErrorMessage"));
                                rollbackReadinessCheckStatusOfServers.Add(serverReadinessCheckStatus);
                            }
                        }
                    }
                }
            }

            return rollbackReadinessCheckStatusOfServers;
        }

        public ConsistencyPoints ListConsistencyPoints(CXListConsistencyPointsRequest listconsistencyPointsRequest, ResponseStatus responseStatus)
        {
            ConsistencyPoints consistencyPoints = null;            

            ArrayList functionRequests = ConstructFunctionRequest(listconsistencyPointsRequest.Request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionListConsistencyPoints, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionListConsistencyPoints);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    foreach (var item in functionResponse.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            consistencyPoints = new ConsistencyPoints(item as ParameterGroup);
                            break;
                        }
                    }
                }
            }

            return consistencyPoints;
        }

        public Dictionary<string, OperationStatus> RestartResync(string protectionPlanId, string[] serverIds, bool forceResyncEnabled, ResponseStatus responseStatus)
        {
            Dictionary<string, OperationStatus> restartResyncStatusOfServers = null;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionRestartResync;
            request.AddParameter("ProtectionPlanId", protectionPlanId);
            if(serverIds != null)
            {
                int count = 1;
                ParameterGroup servers = new ParameterGroup("Servers");                
                foreach (string serverId in serverIds)
                {
                    servers.AddParameter("Server" + count, serverId);
                    ++count;
                }
                request.AddParameterGroup(servers);
            }
            request.AddParameter("ForceResync", forceResyncEnabled ? "Yes" : "No");
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);

            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionRestartResync);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    ParameterGroup serversStatus = functionResponse.GetParameterGroup("Servers");
                    if(serversStatus != null)
                    {
                        restartResyncStatusOfServers = new Dictionary<string, OperationStatus>();
                        foreach(ParameterGroup resyncStatus in serversStatus.ChildList)
                        {
                            restartResyncStatusOfServers.Add(
                                resyncStatus.GetParameterValue("HostId"), 
                                Utils.ParseEnum<OperationStatus>(resyncStatus.GetParameterValue("Status")));
                        }
                    }
                }
            }

            return restartResyncStatusOfServers;
        }

        public CSPSHealth GetCSPSHealth(ResponseStatus responseStatus)
        {
            CSPSHealth cspsHealth = null;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionGetCSPSHealth;            
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionGetCSPSHealth);
                if (functionResponse != null)
                {
                    cspsHealth = new CSPSHealth(functionResponse);
                }
            }

            return cspsHealth;
        }

        /// <summary>
        /// Gets plans of type Protection\Rollback from CS.
        /// Also if the value is null or empty, it returns the Enum whose value is 0(default)
        /// </summary>
        /// <param name="planType">If this value is Unknown all Plans will be returned</param>
        /// <param name="planId">If this value is null, all the Plans of the given Plan type will be returned</param>
        /// <param name="hostIds">Host GUID List</param>
        /// <returns>Returns Protection\Rollback plans</returns>
        public PlanList ListPlans(PlanType planType, string planId, string[] hostIds, ResponseStatus responseStatus)
        {
            PlanList plans = null;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionListPlans;
            if (planType != PlanType.Unknown)
            {
                request.AddParameter("PlanType", planType.ToString());
            }
            if(!String.IsNullOrEmpty(planId))
            {
                request.AddParameter("PlanId", planId);
            }
            if (hostIds != null)
            {
                int count = 1;
                ParameterGroup servers = new ParameterGroup("Servers");                
                foreach(string hostId in hostIds)
                {
                    servers.AddParameter("HostId" + count, hostId);
                    ++count;
                }
                request.AddParameterGroup(servers);
            }
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionListPlans, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionListPlans);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    plans = new PlanList(functionResponse);
                }
            }
            return plans;
        }

        public List<ProtectionDetails> GetProtectionDetails(string protectionPlanId, string[] hostIds, ResponseStatus responseStatus)
        {
            List<ProtectionDetails> protectionDetailsList = null;
            FunctionRequest request = new FunctionRequest();
            request.Name = Constants.FunctionGetProtectionDetails;
            if (!String.IsNullOrEmpty(protectionPlanId))
            {
                request.AddParameter("ProtectionPlanId", protectionPlanId);
            }
            if (hostIds != null)
            {
                int count = 1;
                ParameterGroup servers = new ParameterGroup("Servers");
                foreach (string hostId in hostIds)
                {
                    servers.AddParameter("SourceHostId" + count, hostId);
                    ++count;
                }
                request.AddParameterGroup(servers);
            }
            ArrayList functionRequests = ConstructFunctionRequest(request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, request.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, request.Name);
                if (functionResponse != null && functionResponse.ChildList != null)
                {
                    protectionDetailsList = new List<ProtectionDetails>();
                    foreach (var item in functionResponse.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            protectionDetailsList.Add(new ProtectionDetails(item as ParameterGroup));                            
                        }
                    }
                }
            }

            return protectionDetailsList;
        }

        public bool PSFailover(PSFailoverRequest psFailoverRequest, ResponseStatus responseStatus)
        {
            bool setStatus = false;
            ArrayList functionRequests = ConstructFunctionRequest(psFailoverRequest.Request);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, Constants.FunctionPSFailover, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, Constants.FunctionPSFailover);
                if (functionResponse != null)
                {
                    setStatus = String.Compare(functionResponse.GetParameterValue("PSFailover"), "Success", StringComparison.OrdinalIgnoreCase) == 0 ? true : false;
                }
            }

            return setStatus;
        }

        public bool CreateAccounts(List<UserAccount> accountList, ResponseStatus responseStatus)
        {
            bool status = false;

            if (accountList == null || accountList.Count == 0)
            {
                return false;
            }
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = Constants.FunctionCreateAccount;
            ParameterGroup accountsParameterGroup = new ParameterGroup("Accounts");
            ParameterGroup accountDetailsParameterGroup;
            int count = 1;
            foreach (UserAccount account in accountList)
            {
                accountDetailsParameterGroup = new ParameterGroup("Account" + count);                
                accountDetailsParameterGroup.AddParameter("AccountName", account.AccountName);
                if (!String.IsNullOrEmpty(account.Domain))
                {
                    accountDetailsParameterGroup.AddParameter("Domain", account.Domain);
                }
                accountDetailsParameterGroup.AddParameter("UserName", account.UserName);
                accountDetailsParameterGroup.AddParameter("Password", account.Password);
                if (!String.IsNullOrEmpty(account.AccountType))
                {
                    accountDetailsParameterGroup.AddParameter("AccountType", account.AccountType);
                }

                accountsParameterGroup.AddParameterGroup(accountDetailsParameterGroup);
                count++;
            }
            functionRequest.AddParameterGroup(accountsParameterGroup);
            ArrayList functionRequests = ConstructFunctionRequest(functionRequest);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, functionRequest.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, functionRequest.Name);
                status = true;
                if (functionResponse != null)
                {
                    status = String.Equals(functionResponse.GetParameterValue("CreateAccount"), "Success", StringComparison.OrdinalIgnoreCase);
                }
            }
            return status;
        }

        public bool UpdateAccounts(List<UserAccount> accountList, ResponseStatus responseStatus)
        {
            bool status = false;

            if (accountList == null || accountList.Count == 0)
            {
                return false;
            }
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = Constants.FunctionUpdateAccount;
            ParameterGroup accountsParameterGroup = new ParameterGroup("Accounts");
            ParameterGroup accountDetailsParameterGroup;
            int count = 1;
            foreach (UserAccount account in accountList)
            {
                accountDetailsParameterGroup = new ParameterGroup("Account" + count);
                accountDetailsParameterGroup.AddParameter("AccountId", account.AccountId);
                accountDetailsParameterGroup.AddParameter("AccountName", account.AccountName);
                if (!String.IsNullOrEmpty(account.Domain))
                {
                    accountDetailsParameterGroup.AddParameter("Domain", account.Domain);
                }
                accountDetailsParameterGroup.AddParameter("UserName", account.UserName);
                accountDetailsParameterGroup.AddParameter("Password", account.Password);
                if (!String.IsNullOrEmpty(account.AccountType))
                {
                    accountDetailsParameterGroup.AddParameter("AccountType", account.AccountType);
                }
                accountsParameterGroup.AddParameterGroup(accountDetailsParameterGroup);
                count++;
            }
            functionRequest.AddParameterGroup(accountsParameterGroup);
            ArrayList functionRequests = ConstructFunctionRequest(functionRequest);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, functionRequest.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, functionRequest.Name);
                status = true;
                if (functionResponse != null)
                {
                    status = String.Equals(functionResponse.GetParameterValue("UpdateAccount"), "Success", StringComparison.OrdinalIgnoreCase);
                }
            }
            return status;
        }

        public bool RemoveAccounts(List<string> accountIds, ResponseStatus responseStatus)
        {
            bool status = false;

            if (accountIds == null || accountIds.Count == 0)
            {
                return false;
            }
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = Constants.FunctionRemoveAccount;
            ParameterGroup accountsParameterGroup = new ParameterGroup("Accounts");
            int count = 1;
            foreach (string accountId in accountIds)
            {
                accountsParameterGroup.AddParameter("AccountId" + count, accountId);
                count++;
            }
            functionRequest.AddParameterGroup(accountsParameterGroup);
            ArrayList functionRequests = ConstructFunctionRequest(functionRequest);
            Response response = Post(functionRequests);            
            if (ResponseParser.ValidateResponse(response, functionRequest.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, functionRequest.Name);
                status = true;
                if (functionResponse != null)
                {
                    status = String.Equals(functionResponse.GetParameterValue("RemoveAccount"), "Success", StringComparison.OrdinalIgnoreCase);
                }
            }
            return status;
        }

        public List<UserAccount> ListAccounts(ResponseStatus responseStatus)
        {
            List<UserAccount> accountList = null;
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = Constants.FunctionListAccounts;
            ArrayList functionRequests = ConstructFunctionRequest(functionRequest);
            Response response = Post(functionRequests);
            if (ResponseParser.ValidateResponse(response, functionRequest.Name, responseStatus))
            {
                FunctionResponse functionResponse = ResponseParser.GetFunctionResponse(response, functionRequest.Name);
                if (functionResponse != null)
                {
                    ParameterGroup accountsParameterGroup = functionResponse.GetParameterGroup("Accounts");
                    ParameterGroup accountParameterGroup = null;
                    if (accountsParameterGroup != null && accountsParameterGroup.ChildList.Count > 0)
                    {
                        UserAccount account;
                        accountList = new List<UserAccount>();
                        foreach (var item in accountsParameterGroup.ChildList)
                        {
                            if (item is ParameterGroup)
                            {
                                accountParameterGroup = item as ParameterGroup;
                                account = new UserAccount()
                                {
                                    AccountId = accountParameterGroup.GetParameterValue("AccountId"),
                                    AccountName = accountParameterGroup.GetParameterValue("AccountName"),
                                    Domain = accountParameterGroup.GetParameterValue("Domain"),
                                    UserName = accountParameterGroup.GetParameterValue("UserName"),
                                    Password = accountParameterGroup.GetParameterValue("Password"),
                                    AccountType = accountParameterGroup.GetParameterValue("AccountType")
                                };
                                accountList.Add(account);
                            }
                        }
                    }
                }
            }

            return accountList;
        }

        private Response GetRequestStatus(string requestId)
        {
            ArrayList functionRequests = ConstructFunctionRequest(new CXStatusRequest(requestId).Request);
            Response response = Post(functionRequests);
            return response;
        }

        private ArrayList ConstructFunctionRequest(object obj)
        {
            ArrayList functionRequests = new ArrayList();
            functionRequests.Add(obj);
            return functionRequests;
        }

        private ArrayList ConstructFunctionRequest(List<object> obj)
        {
            ArrayList functionRequests = new ArrayList();
            functionRequests.AddRange(obj.GetRange(0, obj.Count));
            return functionRequests;
        }

        private Response Post(ArrayList functionRequests)
        {
            if (!isCXIPAvailable)
            {
                return CXAPI.post(functionRequests, ActivityId, ClientRequestId);
            }
            else if (useCustomAuthValues)
            {
                return CXAPI.post(functionRequests, cxIp, cxPort, cxProtocol, authMethod, accessKeyId, passphrase, fingerPrint, userName, password, ActivityId, ClientRequestId);
            }
            else
            {
                return CXAPI.post(functionRequests, cxIp, cxPort, cxProtocol, ActivityId, ClientRequestId);
            }
        }
    }
}
