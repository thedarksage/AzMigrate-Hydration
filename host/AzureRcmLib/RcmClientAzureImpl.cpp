/*
+------------------------------------------------------------------------------------------------ - +
Copyright(c) Microsoft Corp. 2019
+ ------------------------------------------------------------------------------------------------ - +
File        : RcmClientAzureImpl.cpp

Description : RcmClientAzureImpl class implementation

+ ------------------------------------------------------------------------------------------------ - +
*/
#include "SourceComponent.h"
#include "AgentSettings.h"
#include "RcmClientAzureImpl.h"
#include "Converter.h"
#include "HttpClient.h"
#include "portablehelperscommonheaders.h"
#include "MonitoringMsgSettings.h"
#include "AgentRcmLogging.h"
#include "RcmClientUtils.h"
#include "RcmConfigurator.h"
#include "RcmLibConstants.h"

#include "portablehelpers.h"
#include "securityutils.h"
#include "defaultdirs.h"
#include "getpassphrase.h"
#include "fingerprintmgr.h"
#include "csgetfingerprint.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>
#include <curl/curl.h>

#ifdef SV_WINDOWS
#include "Failoverclusterinfocollector.h"
#endif

using namespace RcmClientLib;
using namespace AzureStorageRest;
using namespace AADAuthLib;

#ifdef WIN32
__declspec(thread) RCM_CLIENT_STATUS RcmClientAzureImpl::m_errorCode;
#else
__thread RCM_CLIENT_STATUS RcmClientAzureImpl::m_errorCode;
#endif

RcmClientAzureImpl::RcmClientAzureImpl()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


SVSTATUS RcmClientAzureImpl::PostWithHeaders(const std::string& rcmEndPoint,
    const std::string& jsonInput,
    std::string& jsonOutput,
    const std::string &clientRequestId,
    const std::map<std::string, std::string>& headers, 
    const bool& clusterVirtualNodeContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    const std::string serviceUri = RcmConfigurator::getInstance()->GetRcmServiceUri();
    const std::string req_id = clientRequestId.empty() ? GenerateUuid() : clientRequestId;
    const std::string activity_id = GenerateUuid();

    std::string rcmUri;
    if (!boost::starts_with(serviceUri, HTTPS_PREFIX))
        rcmUri += HTTPS_PREFIX;
    rcmUri += serviceUri;
    rcmUri += URI_DELIMITER::PATH;
    rcmUri += RCM_BASE_URI;

    if (RcmConfigurator::getInstance()->GetRcmClientSettings()->m_ServiceConnectionMode == ServiceConnectionModes::RELAY)
    {
        rcmUri += RcmConfigurator::getInstance()->GetRcmClientSettings()->m_RelayServicePathSuffix;
    }

    rcmUri += URI_DELIMITER::PATH;
    rcmUri += rcmEndPoint;
    
    DebugPrintf(SV_LOG_DEBUG, "%s: RCM URI is: %s\n", FUNCTION_NAME, rcmUri.c_str());

    std::string errMsg;
    errMsg += "JSON request string :: ";
    errMsg += rcmUri;
    errMsg += "\n";
    errMsg += jsonInput;
    errMsg += "\n";

    DebugPrintf(SV_LOG_DEBUG,
        "%s: %s ClientRequestId %s.\n",
        FUNCTION_NAME,
        rcmEndPoint.c_str(),
        req_id.c_str());

    try {
        HttpClient rcm_client(true);
        HttpRequest request(rcmUri);

        if (RcmConfigurator::getInstance()->GetRcmClientRequestTimeout())
            request.SetTimeout(RcmConfigurator::getInstance()->GetRcmClientRequestTimeout());

        request.AddHeader(ContentType, AppJson);
        request.AddHeader(X_MS_CLIENT_REQUEST_ID, req_id);
        request.AddHeader(X_MS_CORRELATION_REQUEST_ID, activity_id);
        if (clusterVirtualNodeContext)
        { 
            request.AddHeader(X_MS_AGENT_MACHINE_ID, RcmConfigurator::getInstance()->getClusterId());
        }    
        else
        {
            request.AddHeader(X_MS_AGENT_MACHINE_ID, RcmConfigurator::getInstance()->getHostId());
        }
        request.AddHeader(X_MS_AGENT_COMPONENT_ID, SourceComponent);
        request.AddHeader(X_MS_AUTH_TYPE, AadOAuth2);

        std::string protectionScenario = RcmConfigurator::getInstance()->IsAzureToAzureReplication() ?
            ProetctionScenario::AzureToAzure : ProetctionScenario::AzureToOnPrem;

        request.AddHeader(X_MS_AGENT_PROTECTION_SCENARIO, protectionScenario);

        std::map<std::string, std::string>::const_iterator propIter = headers.begin();
        for (/*empty*/; propIter != headers.end(); propIter++)
        {
            request.AddHeader(propIter->first, propIter->second);
        }

        HttpProxy proxy;
        if (RcmConfigurator::getInstance()->GetProxySettings(proxy.m_address, proxy.m_port, proxy.m_bypasslist))
            rcm_client.SetProxy(proxy);

        if (RcmConfigurator::getInstance()->GetRcmClientSettings()->m_ServiceConnectionMode == ServiceConnectionModes::RELAY)
        {
            std::string headerValue = GetServiceBusAuthHeader(rcmUri);
            request.AddHeader(ServiceBusAuth, headerValue);
        }

        std::string token;
        status = GetBearerToken(token);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : Failed to get bearer token\n", FUNCTION_NAME);
            return status;
        }

        std::string auth = OAUTH_BEARER_TOKEN;
        auth += " ";
        auth += token;
        request.AddHeader(Auth, auth);

        if ( rcmEndPoint == GET_AZURE_TO_AZURE_SOURCE_AGENT_SETTINGS_URI || rcmEndPoint == GET_AZURE_TO_ON_PREM_SOURCE_AGENT_SETTINGS_URI
#ifdef _DEBUG
            || rcmEndPoint == TEST_CONNECTION_URI
#endif
            )
        {
            request.SetHttpMethod(AzureStorageRest::HTTP_GET);
        }
        else
        {
            request.SetHttpMethod(AzureStorageRest::HTTP_POST);
        }

        if ((rcmEndPoint == RENEW_LOG_CONTAINER_SAS_URI) ||
            (rcmEndPoint == RENEW_LOG_CONTAINER_SAS_URI_AZURE_TO_ONPREM_DIFFSYNC) ||
            (rcmEndPoint == RENEW_LOG_CONTAINER_SAS_URI_AZURE_TO_ONPREM_RESYNC))
            request.AddHeader(X_MS_AGENT_DISK_ID, jsonInput);

        std::vector<unsigned char> vdata(jsonInput.begin(), jsonInput.end());
        request.SetRequestBody(&vdata[0], vdata.size());

        HttpResponse response;

        if (!rcm_client.GetResponse(request, response))
        {
            errMsg += "Request failed: client-request-id = ";
            errMsg += req_id + ", Error: ";
            errMsg += boost::lexical_cast<std::string>(response.GetErrorCode());
            errMsg += "\n";

            DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
            long responseErrorCode = response.GetErrorCode();
            m_errorCode = RcmClientUtils::GetRcmClientErrorForCurlError(responseErrorCode);
            status = SVE_FAIL;;
        }
        else
        {
            if (response.GetResponseCode() == HttpErrorCode::OK)
            {
                jsonOutput = response.GetResponseData();

                if (rcmEndPoint == GET_AGENT_SETTINGS_URI)
                {
                    if (std::string::npos != jsonOutput.find_first_of('{'))
                    {
                        RcmClientUtils::TrimRcmResponse(jsonOutput);
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s: RCM API %s client-request-id %s response is not in JSON. %s\n",
                            FUNCTION_NAME,
                            rcmEndPoint.c_str(),
                            req_id.c_str(),
                            jsonOutput.c_str());

                        status = SVE_FAIL;
                    }
                }
            }
            else
            {
                if (rcmEndPoint == RENEW_LOG_CONTAINER_SAS_URI)
                    jsonOutput = response.GetResponseData();

                status = SVE_HTTP_RESPONSE_FAILED;
                ProcessErrorResponse(response.GetResponseCode(), response.GetResponseData());
            }

        }
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}


SVSTATUS RcmClientAzureImpl::AddDiskDiscoveryInfo(MachineEntity& me,
    const VolumeSummaries_t& volumeSummaries,
    const VolumeDynamicInfos_t& volumeDynamicInfos,
    const bool bClusterContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;

    try {
        Converter converter(RCM_DISK_REPORT);
        status = converter.ProcessVolumeSummaries(me.OperatingSystemType, volumeSummaries, volumeDynamicInfos, bClusterContext);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)converter.GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s : ProcessVolumeSummaries failed. Error: %d, Status: %d\n",
                FUNCTION_NAME,
                m_errorCode,
                status);
            return status;
        }

        status = converter.AddDiskEntities(me, bClusterContext);
        if (status != SVS_OK)
        {
            m_errorCode = (RCM_CLIENT_STATUS)converter.GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s : AddDiskEntities failed. Error: %d, Status: %d\n",
                FUNCTION_NAME,
                m_errorCode,
                status);
            return status;
        }
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status == SVE_ABORT)
    {
        m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void RcmClientAzureImpl::AadExtendedErrorCallback(const std::string& message)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    // construct the error message to be shown in portal
    ExtendedErrorLogger::ExtendedError cliError;
    cliError.error_name = "ConfiguratorAADHttpError";
    cliError.default_message = "Azure AD service returned error.";
    cliError.error_params.insert(std::make_pair("Response", message));
    ExtendedErrorLogger::ErrorLogger::LogExtendedError(cliError);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

SVSTATUS RcmClientAzureImpl::GetBearerToken(std::string &token)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;

    try
    {
        if (m_ptrAADAuthProvider.get() == NULL)
            m_ptrAADAuthProvider.reset(
            new AADAuthProvider(*(RcmConfigurator::getInstance()->GetRcmClientSettings().get()),
                boost::bind(&RcmClientAzureImpl::AadExtendedErrorCallback, this, _1)));

        std::string ip, port, bypasslist;
        if (RcmConfigurator::getInstance()->GetProxySettings(ip, port, bypasslist))
            m_ptrAADAuthProvider->SetHttpProxy(ip, port, bypasslist);

        status = m_ptrAADAuthProvider->GetBearerToken(token);
        if (SVS_OK != status)
        {
            m_errorCode = (RCM_CLIENT_STATUS) m_ptrAADAuthProvider->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR,
                "%s: Failed to get bearer token. status=%d, Error=%d\n",
                FUNCTION_NAME,
                status,
                m_errorCode);

        }
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status == SVE_ABORT)
    {
        m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

/// \brief generates the service bus auth header for relay mode
std::string RcmClientAzureImpl::GetServiceBusAuthHeader(const std::string& resourceUrl)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string expiry = boost::lexical_cast<std::string>(
        GetTimeInSecSinceEpoch1970()
        + RcmConfigurator::getInstance()->GetRcmClientSettings()->m_ExpiryTimeoutInSeconds);

    std::string stringToSign = HttpRestUtil::UrlEncode(resourceUrl);
    stringToSign += "\n" + expiry;

    std::string key = RcmConfigurator::getInstance()->GetRcmClientSettings()->m_RelaySharedKey;
    std::string hashString = securitylib::genHmacWithBase64Encode(key.c_str(),
        key.length(),
        stringToSign);

    std::string header = "SharedAccessSignature sr=";
    header += HttpRestUtil::UrlEncode(resourceUrl);
    header += "&sig=";
    header += HttpRestUtil::UrlEncode(hashString);
    header += "&se=";
    header += expiry;
    header += "&skn=RootManageSharedAccessKey";

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return header;
}

/// \brief processes the error response received from RCM
/// \returns none
void RcmClientAzureImpl::ProcessErrorResponse(long responseCode, const std::string& strResponse)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bUnknownError = false;
    try {
        std::stringstream ss(strResponse);
        boost::property_tree::ptree pt;
        boost::property_tree::read_xml(ss, pt);

        std::string errorcode = pt.get<std::string>("RcmError.ErrorCode", "");

        if ((errorcode == RCM_ERRORS::IdentityDoesNotExist) ||
            (errorcode == RCM_ERRORS::MachineDoesNotExist) ||
            (errorcode == RCM_ERRORS::SourceAgentDoesNotExist))
        {
            DebugPrintf(
                SV_LOG_ERROR,
                "%s: RCM error indicates protection is disabled.\n",
                FUNCTION_NAME);

            m_errorCode = RCM_CLIENT_PROTECTION_DISABLED;
        }
        else if ((errorcode == RCM_ERRORS::MachineInUse) ||
            (errorcode == RCM_ERRORS::SourceAgentInUse))
        {
            DebugPrintf(
                SV_LOG_ERROR,
                "%s: RCM error indicates protection is still enabled.\n",
                FUNCTION_NAME);

            m_errorCode = RCM_CLIENT_PROTECTION_ENABLED;
        }
        else if ((errorcode == RCM_ERRORS::AzureToOnPremFreshProtectionIsNotSupported) ||
            (errorcode == RCM_ERRORS::ReprotectWithoutFailoverIsNotsupported)) {
            DebugPrintf(
                SV_LOG_ERROR,
                "%s: RCM error indicates registration is blocked.\n",
                FUNCTION_NAME);

            m_errorCode = RCM_CLIENT_REGISTRATION_NOT_SUPPORTED;
        }
        else if (errorcode == RCM_ERRORS::RegistrationNotAllowed) {
            DebugPrintf(
                SV_LOG_ERROR,
                "%s: RCM error indicates registration is not allowed.\n",
                FUNCTION_NAME);
            m_errorCode = RCM_CLIENT_REGISTRATION_NOT_ALLOWED;
        }
        else
        {
            bUnknownError = true;
        }
    }
    catch (...)
    {
        bUnknownError = true;
    }

    if (bUnknownError)
    {
        std::string errMsg;
        errMsg += "HTTP response code: ";
        errMsg += boost::lexical_cast<std::string>(responseCode);
        errMsg += ", response data : \n";
        errMsg += strResponse;
        errMsg += "\n";

        DebugPrintf(SV_LOG_ERROR, "%s: request completed with non-XML response. %s\n", FUNCTION_NAME, errMsg.c_str());

        m_errorCode = RCM_CLIENT_ERROR_HTTP_RESPONSE_UNKNOWN;

        // construct the error message to be shown in portal
        ExtendedErrorLogger::ExtendedError cliError;
        cliError.error_name = "ConfiguratorRecoveryServiceUnknownHttpError";
        cliError.default_message = "Recovery Service RCM returned unknown error.";
        cliError.error_params.insert(std::make_pair("Response", errMsg));
        ExtendedErrorLogger::ErrorLogger::LogExtendedError(cliError);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

SVSTATUS RcmClientAzureImpl::PostAgentMessage(const std::string& endPoint,
    const std::string &serializedMessageInput,
    const std::map<std::string, std::string>& propertyMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;

    try {

        status = Post(endPoint, serializedMessageInput, errMsg);

    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status == SVE_ABORT)
    {
        m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}


#ifdef SV_WINDOWS
SVSTATUS RcmClientAzureImpl::AddClusterDiscoveryInfo(MachineEntity& me)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;

    try {

        FailoverClusterInfo clusterInfo;

        if (!clusterInfo.IsClusterNode())
        {
            errMsg = "InValid cluster service status. Error is: " + clusterInfo.GetLastErrorMessage();
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());

            m_errorCode = RCM_CLIENT_INVALID_CLUSTER_SERVICE_STATUS;
            return SVE_FAIL;
        }

        clusterInfo.AddFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_MANAGEMENT_ID, RcmConfigurator::getInstance()->GetClusterManagementId());
        clusterInfo.AddFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_TYPE, FailoverCluster::DEFAULT_FAILOVER_CLUSTER_TYPE);
            
        status = clusterInfo.CollectFailoverClusterProperties();

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: CollectFailoverClusterProperties operation failed with error: %s\n", FUNCTION_NAME, clusterInfo.GetLastErrorMessage().c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_PROPERTIES_COLLECTOR_ERROR;

            return SVE_FAIL;
        }

        RcmConfigurator::getInstance()->setClusterId(clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_ID));
        RcmConfigurator::getInstance()->setClusterName(clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_NAME));
        
        me.RegisterSourceClusterInput.ClusterId = clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_ID);
        me.RegisterSourceClusterInput.RegisterSourceClusterVirtualNodeInput.ClusterVirtualNodeId = clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_ID);
        
        if (me.RegisterSourceClusterInput.ClusterId.empty())
        {
            errMsg = "Received empty cluster id.";
            DebugPrintf(SV_LOG_ERROR,"%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_ID;

            return SVE_FAIL;
        }

        me.RegisterSourceClusterInput.ClusterManagementId = clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_MANAGEMENT_ID);
        if (me.RegisterSourceClusterInput .ClusterManagementId.empty())
        {
            errMsg = "Received empty cluster management Id.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_MANAGEMENT_ID;

            return SVE_FAIL;
        }
        me.RegisterSourceClusterInput.ClusterName = clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_NAME);
        if (me.RegisterSourceClusterInput.ClusterName.empty())
        {
            errMsg = "Received empty cluster name.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_NAME;
           
            return SVE_FAIL;
        }

        me.RegisterSourceClusterInput.ClusterType = clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_TYPE);
        if (me.RegisterSourceClusterInput.ClusterType.empty())
        {
            errMsg = "Received empty cluster Cluster Type.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_TYPE;

            return SVE_FAIL;
        }
        me.RegisterSourceClusterInput.CurrentClusterNodeName = clusterInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_CURRENT_CLUSTER_NODE_NAME);
        if (me.RegisterSourceClusterInput.CurrentClusterNodeName.empty())
        {
            errMsg = "Received empty current cluster name.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_CURRENT_NODE_NAME;

            return SVE_FAIL;
        }
        
        std::set<NodeEntity> clusterNodes = clusterInfo.GetClusterNodeSet();
        std::set<NodeEntity>::iterator iter = clusterNodes.begin();
        for (; iter != clusterNodes.end(); ++iter)
        {
            me.RegisterSourceClusterInput.ClusterNodeNames.push_back(iter->nodeName);
        }

        if (me.RegisterSourceClusterInput.ClusterNodeNames.empty())
        {
            errMsg = "Received empty list of cluster nodes name.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RCM_CLIENT_FAILOVER_CLUSTER_EMPTY_NODE_LIST;

            return SVE_FAIL;
        }

    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status == SVE_ABORT)
    {
        m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

