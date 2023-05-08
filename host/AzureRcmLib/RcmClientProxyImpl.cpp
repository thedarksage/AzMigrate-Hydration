/*
+------------------------------------------------------------------------------------------------ - +
Copyright(c) Microsoft Corp. 2019
+ ------------------------------------------------------------------------------------------------ - +
File        : RcmClientProxyImpl.cpp

Description : RcmClientProxyImpl class implementation

+ ------------------------------------------------------------------------------------------------ - +
*/

#include "SourceComponent.h"
#include "AgentSettings.h"
#include "RcmClientProxyImpl.h"
#include "Converter.h"
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


using namespace RcmClientLib;
using namespace AzureStorageRest;

extern void GetNicInfos(NicInfos_t &nicinfos);

namespace RcmClientLib {

#ifdef WIN32
    __declspec(thread) RCM_CLIENT_STATUS RcmClientProxyImpl::m_errorCode;
#else
    __thread RCM_CLIENT_STATUS RcmClientProxyImpl::m_errorCode;
#endif

    RcmClientProxyImpl::RcmClientProxyImpl()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        m_rcmProxyPort = 0;
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }

    HttpClient RcmClientProxyImpl::GetRcmProxyClient()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        HttpClient rcm_client(true, true, true);
        HttpProxy proxy;

        if (RcmConfigurator::getInstance()->GetProxySettings(proxy.m_address,
            proxy.m_port, proxy.m_bypasslist))
            rcm_client.SetProxy(proxy);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return rcm_client;
    }

    HttpRequest RcmClientProxyImpl::GetRcmProxyRequest(const std::string &rcmEndPoint,
        const std::string &rcmProxyAddr,
        const uint32_t &rcmProxyPort,
        const std::string &clientRequestId,
        std::string &rcmProxyUri,
        std::string &req_id,
        bool isAgentMessage)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        rcmProxyUri = HTTPS_PREFIX;
        rcmProxyUri += rcmProxyAddr;
        rcmProxyUri += ":";
        rcmProxyUri += boost::lexical_cast<std::string>(rcmProxyPort);
        rcmProxyUri += URI_DELIMITER::PATH;
        if (isAgentMessage ||
            rcmEndPoint == GET_RCM_DETAILS ||
            rcmEndPoint == TEST_CONNECTION ||
            rcmEndPoint == CREATE_MOBILITY_AGENT_CONFIG)
            rcmProxyUri += rcmEndPoint;
        else
            rcmProxyUri += RCM_ACTION_CALL_RCM_API;
        HttpRequest request(rcmProxyUri);

        if (RcmConfigurator::getInstance()->GetRcmClientRequestTimeout())
            request.SetTimeout(RcmConfigurator::getInstance()->GetRcmClientRequestTimeout());

        std::string method;
        if (rcmEndPoint == GET_ON_PREM_TO_AZURE_SOURCE_AGENT_SETTINGS_URI ||
            rcmEndPoint == GET_RCM_DETAILS)
        {
            request.SetHttpMethod(AzureStorageRest::HTTP_GET);
            method = "GET";
        }
        else
        {
            request.SetHttpMethod(AzureStorageRest::HTTP_POST);
            method = "POST";
        }

        req_id = clientRequestId.empty() ? GenerateUuid() : clientRequestId;
        std::string activity_id = GenerateUuid();
        DebugPrintf(SV_LOG_DEBUG, "%s: %s ClientRequestId %s.\n", FUNCTION_NAME,
            rcmEndPoint.c_str(), req_id.c_str());

        request.AddHeader(ContentType, AppJson);
        request.AddHeader(X_MS_CLIENT_REQUEST_ID, req_id);
        request.AddHeader(X_MS_CORRELATION_REQUEST_ID, activity_id);

        request.AddHeader(X_MS_AGENT_MACHINE_ID, RcmConfigurator::getInstance()->getHostId());
        request.AddHeader(X_MS_AGENT_COMPONENT_ID, SourceComponent);

        request.AddHeader(X_MS_AGENT_RCM_ACTION, rcmEndPoint);
        request.AddHeader(X_MS_AGENT_RCM_ACTION_TYPE, method);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return request;
    }

    SVSTATUS RcmClientProxyImpl::GetRcmProxyResponse(HttpClient &rcm_client,
        HttpRequest &request,
        std::string &errMsg,
        std::string &jsonOutput,
        const std::string &rcmProxyUri,
        const std::string &rcmEndPoint,
        const std::string &req_id,
        bool isAgentMessage)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        HttpResponse response;

        if (!rcm_client.GetResponse(request, response))
        {
            long responseErrorCode = response.GetErrorCode();
            errMsg += "Request failed for URI " + rcmProxyUri;
            errMsg += ", rcmEndPoint: " + rcmEndPoint;
            errMsg += ", ClientRequestId: " + req_id;
            errMsg += ", ErrorCode: " + boost::lexical_cast<std::string>(responseErrorCode);

            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            m_errorCode = RcmClientUtils::GetRcmClientErrorForCurlError(responseErrorCode);
            status = SVE_FAIL;
        }
        else
        {
            if (response.GetResponseCode() == HttpErrorCode::OK)
            {
                jsonOutput = response.GetResponseData();

                if (rcmEndPoint == GET_ON_PREM_TO_AZURE_SOURCE_AGENT_SETTINGS_URI ||
                    rcmEndPoint == GET_RCM_DETAILS ||
                    (isAgentMessage && !jsonOutput.empty()))
                {
                    if (std::string::npos != jsonOutput.find_first_of('{'))
                    {
                        RcmClientUtils::TrimRcmResponse(jsonOutput);
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: RCM API %s response is not in JSON. %s\n",
                            FUNCTION_NAME, rcmEndPoint.c_str(), jsonOutput.c_str());
                        status = SVE_FAIL;
                        // check if the response has any XML message
                        ProcessErrorResponse(response.GetResponseCode(), response.GetResponseData());
                    }
                }
            }
            else
            {
                status = SVE_HTTP_RESPONSE_FAILED;
                ProcessErrorResponse(response.GetResponseCode(), response.GetResponseData());
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientProxyImpl::PostToRcmProxy(const std::string &rcmEndPoint,
        const std::string &jsonInput,
        bool isAgentMessage,
        const std::map<std::string, std::string> &propertyMap,
        const std::string &clientRequestId,
        std::string &jsonOutput,
        const std::string &rcmProxyAddr,
        const uint32_t &rcmProxyPort)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg, req_id, rcmProxyUri;

        try {

            HttpClient rcm_client = GetRcmProxyClient();
            HttpRequest request = GetRcmProxyRequest(rcmEndPoint, rcmProxyAddr, rcmProxyPort,
                    clientRequestId, rcmProxyUri, req_id, isAgentMessage);

            std::map<std::string, std::string>::const_iterator propIter = propertyMap.begin();
            for (/*empty*/; propIter != propertyMap.end(); propIter++)
            {
                request.AddHeader(propIter->first, propIter->second);
            }
            std::vector<unsigned char> vdata(jsonInput.begin(), jsonInput.end());
            if (!jsonInput.empty())
                request.SetRequestBody(&vdata[0], vdata.size());

            status = GetRcmProxyResponse(rcm_client, request, errMsg, jsonOutput,
                rcmProxyUri, rcmEndPoint, req_id, isAgentMessage);

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

    SVSTATUS RcmClientProxyImpl::PostCommon(const std::string &rcmEndPoint,
        const std::string &jsonInput,
        bool isAgentMessage,
        const std::map<std::string, std::string> &propertyMap,
        const std::string &clientRequestId,
        std::string &jsonOutput)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        if (!m_rcmProxyIp.empty() && m_rcmProxyPort)
        {
            DebugPrintf(SV_LOG_DEBUG, "Using RCM Proxy %s:%u to post request\n", m_rcmProxyIp.c_str(),
                m_rcmProxyPort);

            status = PostToRcmProxy(rcmEndPoint, jsonInput, isAgentMessage,
                propertyMap, clientRequestId, jsonOutput, m_rcmProxyIp, m_rcmProxyPort);
            if (status == SVS_OK)
            {
                DebugPrintf(SV_LOG_DEBUG, "RCM Proxy Post call succeeded\n");
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return status;
            }
        }

        std::vector<std::string> rcmProxyAddr;
        uint32_t port;

        status = RcmConfigurator::getInstance()->GetRcmProxyAddress(rcmProxyAddr, port);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to get RCM proxy address with return status : %d\n",
                status);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return status;
        }

        bool success = false;
        for (std::vector<std::string>::iterator it = rcmProxyAddr.begin();
            it != rcmProxyAddr.end(); it++)
        {
            std::string addr = *it;
            if (addr == m_rcmProxyIp)
                continue;

            DebugPrintf(SV_LOG_DEBUG, "Using RCM Proxy %s:%u to post request\n", addr.c_str(),
                port);
            status = PostToRcmProxy(rcmEndPoint, jsonInput, isAgentMessage,
                propertyMap, clientRequestId, jsonOutput, addr, port);
            if ((status == SVS_OK) || (status == SVE_HTTP_RESPONSE_FAILED))
            {
                DebugPrintf(SV_LOG_DEBUG, "RCM Proxy Post call succeeded\n");
                m_rcmProxyIp = addr;
                m_rcmProxyPort = port;
                success = true;
                break;
            }
        }

        if (!success)
        {
            DebugPrintf(SV_LOG_ERROR, "RCM proxy post call failed for all addresses\n");
            status = SVE_FAIL;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientProxyImpl::PostWithHeaders(const std::string& rcmEndPoint,
        const std::string& jsonInput,
        std::string& jsonOutput,
        const std::string &clientRequestId,
        const std::map<std::string, std::string>& headers, 
        const bool& clusterVirtualNodeContext)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        status = PostCommon(rcmEndPoint, jsonInput, false,
            headers, clientRequestId, jsonOutput);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientProxyImpl::PostAgentMessage(const std::string &endPoint,
        const std::string &serializedMessageInput,
        const std::map<std::string, std::string> &propertyMap)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string jsonOutput;

        status = PostCommon(endPoint, serializedMessageInput, true, propertyMap, std::string(),
            jsonOutput);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientProxyImpl::AddDiskDiscoveryInfo(MachineEntity& me,
        const VolumeSummaries_t& volumeSummaries,
        const VolumeDynamicInfos_t& volumeDynamicInfos,
        const bool bClusterContext)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        if (bClusterContext)
        {
            throw INMAGE_EX("not applicable as called in failover cluster context");
        }

        SVSTATUS status = SVS_OK;
        std::string errMsg;

        try {
            Converter converter(RCM_PROXY_DISK_REPORT);
            status = converter.ProcessVolumeSummaries(me.OperatingSystemType, volumeSummaries, volumeDynamicInfos, bClusterContext);
            if (status != SVS_OK)
            {
                m_errorCode = (RCM_CLIENT_STATUS)converter.GetErrorCode();
                DebugPrintf(SV_LOG_ERROR, "%s : ProcessVolumeSummaries failed: status %d, error %d\n",
                    FUNCTION_NAME, status, m_errorCode);
                return status;
            }

            status = converter.AddDiskEntities(me, bClusterContext);
            if (status != SVS_OK)
            {
                m_errorCode = (RCM_CLIENT_STATUS)converter.GetErrorCode();
                DebugPrintf(SV_LOG_ERROR, "%s : AddDiskEntities failed. status %d, error %d\n",
                    FUNCTION_NAME, status, m_errorCode);
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

    SVSTATUS RcmClientProxyImpl::AddNetworkDiscoveryInfo(MachineEntity& me)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg;

        try {
            status = AddNetworkEntities(me);
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

    /// \brief add disk entities to the machine entity
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS RcmClientProxyImpl::AddNetworkEntities(MachineEntity &me)
    {
        SVSTATUS ret = SVS_OK;
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        std::string errMsg;

        NicInfos_t nicinfos;
        GetNicInfos(nicinfos);
        ConstNicInfosIter_t nicinfosItr = nicinfos.begin();
        if (nicinfosItr == nicinfos.end())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: NIC info missing \n", FUNCTION_NAME);
            return SVE_FAIL;
        }
        std::set<std::string> includedNics;

        for (; nicinfosItr != nicinfos.end(); nicinfosItr++)
        {
            const Objects_t &nicconfs = nicinfosItr->second;

            std::string nicAddress = nicinfosItr->first.c_str();
            boost::trim(nicAddress); // on Linux getting newline

            // on Linux getting multiple entries for same NIC
            if (includedNics.find(nicAddress) != includedNics.end())
                continue;

            if (nicconfs.size() < 1)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: NIC %s has %d NIC configs\n",
                    FUNCTION_NAME,
                    nicAddress.c_str(),
                    nicconfs.size());
                continue;
            }

            // is there a case where one MAC address has more than one NIC config?
            // from the network discovery logic, there can me more than one NIC config for a MAC address
            // each config has a distinct Index
            if (nicconfs.size() > 1)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: NIC %s has %d NIC configs\n",
                    FUNCTION_NAME,
                    nicAddress.c_str(),
                    nicconfs.size());
            }

            std::set<std::string> includedIPAddresses;

            for (ConstObjectsIter_t objectsItr = nicconfs.begin(); objectsItr != nicconfs.end(); objectsItr++)
            {
                NetworkAdaptor netAdapter;
                netAdapter.PhysicalAddress = nicAddress;
                netAdapter.Name = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::NAME);
                netAdapter.Manufacturer = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::MANUFACTURER);
                if (netAdapter.Manufacturer.empty())
                    netAdapter.Manufacturer = "NA"; // not available in Linux
                netAdapter.IsDhcpEnabled = !RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::IS_DHCP_ENABLED).empty();

                std::string ipaddresses = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::IP_ADDRESSES);
                std::string subnetmasks = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::IP_SUBNET_MASKS);
                std::string iptypes = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::IP_TYPES);
                std::string dnsservers = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::DNS_SERVER_ADDRESSES);
                std::string gateways = RcmClientUtils::GetAttributeValue(objectsItr->m_Attributes, NSNicInfo::DEFAULT_IP_GATEWAYS);

                std::vector<std::string> vIpaddresses = RcmClientUtils::GetTokens(ipaddresses, std::string(1, NSNicInfo::DELIM));
                std::vector<std::string> vSubnetMasks = RcmClientUtils::GetTokens(subnetmasks, std::string(1, NSNicInfo::DELIM));
                std::vector<std::string> vDnsServers = RcmClientUtils::GetTokens(dnsservers, std::string(1, NSNicInfo::DELIM));
                std::vector<std::string> vGateways = RcmClientUtils::GetTokens(gateways, std::string(1, NSNicInfo::DELIM));

                // on Linux ip_types is not set, all IPs are considered as physical
                std::vector<std::string> vIptypes;
                if (!iptypes.empty())
                    vIptypes = RcmClientUtils::GetTokens(iptypes, std::string(1, NSNicInfo::DELIM));

                if (vIpaddresses.size() != vSubnetMasks.size())
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: NIC %s has %d IP addresses (%s) but %d Subnet masks (%s).\n",
                        FUNCTION_NAME,
                        nicAddress.c_str(),
                        vIpaddresses.size(),
                        ipaddresses.c_str(),
                        vSubnetMasks.size(),
                        subnetmasks.c_str());
                    continue;
                }

#ifdef SV_WINDOWS
                if (vIpaddresses.size() != vIptypes.size())
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: NIC %s has %d IP addresses (%s) but %d IP types (%s).\n",
                        FUNCTION_NAME,
                        nicAddress.c_str(),
                        vIpaddresses.size(),
                        ipaddresses.c_str(),
                        vIptypes.size(),
                        iptypes.c_str());
                    continue;
                }
#endif
                for (uint32_t index = 0; index < vIpaddresses.size(); index++)
                {
                    NetworkConfiguration netConf;
#ifdef SV_WINDOWS
                    if (!boost::iequals(vIptypes[index], "physical"))
                    {
                        continue;
                    }
#endif
                    netConf.IpAddress = vIpaddresses[index];

                    // on Linux getting multiple entries for same IP
                    if (includedIPAddresses.find(netConf.IpAddress) != includedIPAddresses.end())
                        continue;

                    netConf.SubnetMask = vSubnetMasks[index];

                    if (vGateways.size() > index)
                        netConf.GatewayServer = vGateways[index];

                    if (!vDnsServers.empty())
                    {
                        std::vector<std::string>::iterator dnsserverIter = vDnsServers.begin();
                        netConf.PrimaryDnsServer = *dnsserverIter;
                        dnsserverIter++;

                        for (/*empty*/; dnsserverIter != vDnsServers.end(); dnsserverIter++)
                        {
                            if (!netConf.AlternateDnsServers.empty())
                                netConf.AlternateDnsServers += NSNicInfo::DELIM;
                            netConf.AlternateDnsServers += *dnsserverIter;
                        }
                    }

                    netAdapter.NetworkConfigurations.push_back(netConf);
                    includedIPAddresses.insert(netConf.IpAddress);
                }

                if (!netAdapter.NetworkConfigurations.empty())
                {
                    me.NetworkAdaptors.push_back(netAdapter);
                    includedNics.insert(nicAddress);
                }
            }
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return ret;
    }

    /// \brief processes the error response received from RCM
    /// \returns none
    void RcmClientProxyImpl::ProcessErrorResponse(long responseCode, const std::string& strResponse)
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
            else if (errorcode == RCM_ERRORS::ApplianceMismatchForSourceMachineRequest)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: RCM error indicates that source machine request had appliance mismatch.\n",
                    FUNCTION_NAME);

                m_errorCode = RCM_CLIENT_APPLIANCE_MISMATCH;
            }
            else
            {
                bUnknownError = true;
                errorcode = pt.get<std::string>("RcmProxyError.ErrorCode", "");
                if (errorcode == RCM_PROXY_ERRORS::RcmProxyNotConnectedToRcm)
                {
                    bUnknownError = false;
                    m_errorCode = RCM_CLIENT_PROXY_IMPL_CLOUD_PAIRING_INCOMPLETE;
                }
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
            cliError.default_message = "Recovery Service RCM Proxy returned unknown error.";
            cliError.error_params.insert(std::make_pair("Response", errMsg));
            ExtendedErrorLogger::ErrorLogger::LogExtendedError(cliError);
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return;
    }


    SVSTATUS RcmClientProxyImpl::VerifyLoginResponse(const std::string& response,
        const RcmClientProxyLoginContext& loginContext)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        SVSTATUS status = SVS_OK;
        std::string errorMsg;

        std::stringstream jsonStream(response);
        boost::property_tree::ptree propTree;
        boost::property_tree::read_json(jsonStream, propTree);
        try {
            std::string error = propTree.get<std::string>("error");
            errorMsg = "RcmProxy returned error response: ";
            errorMsg += response;
            DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errorMsg.c_str());
            return SVE_FAIL;
        }
        catch (...) {
            // the exception actually means everything OK
            // as property tree throws an exception if it does not
            // find the entry. in this case 'error' was not found
            // so means cs did not return an error
        }
        try {

            if (!loginContext.m_rcmproxySignatureVerificaionJsonObject.empty())
            {
                // read the inside object
                std::string proxyVerificationObject = propTree.get<std::string>(loginContext.m_rcmproxySignatureVerificaionJsonObject);
                std::stringstream proxyVerificationstream(proxyVerificationObject);
                boost::property_tree::read_json(proxyVerificationstream, propTree);
            }

            std::string status, action, tag, verifyid;
            status = propTree.get<std::string>("rcmproxy.status");
            action = propTree.get<std::string>("rcmproxy.action");
            tag = propTree.get<std::string>("rcmproxy.tag");
            verifyid = propTree.get<std::string>("rcmproxy.id");
            if (!Authentication::verifyCsLoginReplyId(loginContext.m_loginId, 
                status, action, loginContext.m_passphrase, loginContext.m_fingerprint, 
                tag, REQUEST_VER_CURRENT, verifyid)) {
                errorMsg = "failed csLogin reply id validation";
                errorMsg += "status: ";
                errorMsg += status;
                errorMsg += ", action: ";
                errorMsg += action;
                errorMsg += ", tag: ";
                errorMsg += tag;
                errorMsg += ", id: ";
                errorMsg += loginContext.m_loginId;
                status = SVE_FAIL;
                DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errorMsg.c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "%s succeeded\n", FUNCTION_NAME);
            }
        }
        catch (...) {
            errorMsg = "Invalid response from RcmProxy: ";
            errorMsg += response;
            status = SVE_FAIL;
            DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errorMsg.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return status;
    }


    SVSTATUS RcmClientProxyImpl::GetServerCert(
        const std::string &uri,
        std::string &cert,
        std::string &fingerprint)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s URI: %s\n", FUNCTION_NAME, uri.c_str());
        SVSTATUS status = SVS_OK;
        std::string errMsg;

        HttpClient rcm_client(true, true);
        
        HttpRequest request(uri);
        HttpResponse response;

        if (RcmConfigurator::getInstance()->GetRcmClientRequestTimeout())
            request.SetTimeout(RcmConfigurator::getInstance()->GetRcmClientRequestTimeout());

        HttpProxy proxy;
        if (RcmConfigurator::getInstance()->GetProxySettings(proxy.m_address, proxy.m_port, proxy.m_bypasslist))
            rcm_client.SetProxy(proxy);

        request.SetHttpMethod(AzureStorageRest::HTTP_POST);

        if (!rcm_client.GetResponse(request, response))
        {
            errMsg += "Request failed for URI " + uri;
            errMsg += ", ErrorCode: " + boost::lexical_cast<std::string>(response.GetErrorCode());
            m_errorCode = RcmClientUtils::GetRcmClientErrorForCurlError(response.GetErrorCode());
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n",FUNCTION_NAME, errMsg.c_str());
        }

        cert = response.GetSslServerCert();
        fingerprint = response.GetSslServerCertFingerprint();
        if (cert.empty() || fingerprint.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to get SSL Server cert and fingerprint\n", FUNCTION_NAME);
            status = SVE_FAIL;
        }
        else
        {
            boost::to_lower(fingerprint);
            DebugPrintf(SV_LOG_DEBUG, "%s: SSL Server fingerprint %s - cert %s\n", FUNCTION_NAME, fingerprint.c_str(), cert.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s, STATUS %d\n", FUNCTION_NAME, status);

        return status;
    }

    SVSTATUS RcmClientProxyImpl::doLogin(RcmClientProxyLoginContext& loginContext,
        std::string& response,
        boost::function<SVSTATUS(std::string, RcmClientProxyLoginContext)> verifyResposneFunction)
    {

        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg;

        try {

            do {
                std::vector<std::string> serverCertUris = loginContext.GetServerCertUris();
                for (std::vector<std::string>::iterator it = serverCertUris.begin();
                    it != serverCertUris.end(); it++)
                {
                    std::string serverCertUri = *it;
                    SVSTATUS status = GetServerCert(serverCertUri,
                        loginContext.m_certificate, loginContext.m_fingerprint);
                    if (status == SVS_OK)
                        break;

                    DebugPrintf(SV_LOG_DEBUG, "%s: Failed to get SSL Server cert and fingerprint for Uri=%s\n", FUNCTION_NAME, serverCertUri.c_str());
                }

                if (status != SVS_OK)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to get SSL Server cert and fingerprint\n", FUNCTION_NAME);
                    errMsg += "Failed to get SSL Server cert and fingerprint";
                    break;
                }

                HttpClient rcm_client(false);
                HttpResponse httpResponse;
                bool loginContextHttpCommSuccess = false;
                std::vector<std::string> loginUris = loginContext.GetLoginUris();
                for (std::vector<std::string>::iterator it = loginUris.begin();
                    it != loginUris.end(); it++)
                {
                    std::string loginUri = *it;
                    HttpRequest request(loginUri);
                    request.AddHeader(ContentType, AppJson);
                    request.AddHeader(X_MS_CLIENT_REQUEST_ID, loginContext.m_clientRequestId);
                    request.AddHeader(X_MS_CORRELATION_REQUEST_ID, loginContext.m_activityId);
                    request.AddHeader(X_MS_AGENT_MACHINE_ID, RcmConfigurator::getInstance()->getHostId());
                    request.AddHeader(X_MS_AGENT_COMPONENT_ID, SourceComponent);
                    request.AddHeader(X_MS_AGENT_AUTH_ACTION, loginContext.m_action);
                    request.AddHeader(X_MS_AGENT_AUTH_VERSION, REQUEST_VER_CURRENT);
                    request.AddHeader(X_MS_AGENT_RCM_ACTION_TYPE, "POST");

                    request.SetHttpMethod(AzureStorageRest::HTTP_POST);
                    
                    std::string nonce = securitylib::genRandNonce(32, true);
                    request.AddHeader(X_MS_AGENT_AUTH_NONCE, nonce);
                    
                    loginContext.m_loginId = Authentication::buildCsLoginId("POST",
                        std::string(),
                        loginContext.m_action.c_str(),
                        loginContext.m_passphrase,
                        loginContext.m_fingerprint,
                        nonce,
                        REQUEST_VER_CURRENT); 
                    request.AddHeader(X_MS_AGENT_AUTH_ID, loginContext.m_loginId);

                    if (loginContext.m_serializedInput.empty())
                    {
                        std::vector<unsigned char> vdata(loginContext.m_loginId.begin(), loginContext.m_loginId.end());
                        request.SetRequestBody(&vdata[0], vdata.size());
                    }
                    else
                    {
                        request.SetRequestBody((unsigned char*)&loginContext.m_serializedInput[0],
                            loginContext.m_serializedInput.size());
                    }
                    loginContextHttpCommSuccess = rcm_client.GetResponse(request, httpResponse);

                    if (loginContextHttpCommSuccess)
                        break;
                }

                if (!loginContextHttpCommSuccess)
                {
                    long responseErrorCode = httpResponse.GetErrorCode();
                    errMsg += "Request failed for URIs " + boost::algorithm::join(loginUris, ",");
                    errMsg += ", ClientRequestId: " + loginContext.m_clientRequestId;
                    errMsg += ", CurlErrorCode: " + boost::lexical_cast<std::string>(responseErrorCode);

                    DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
                    m_errorCode = RcmClientUtils::GetRcmClientErrorForCurlError(responseErrorCode);
                    status = SVE_FAIL;
                    break;
                }
                else
                {
                    response = httpResponse.GetResponseData();
                    if (httpResponse.GetResponseCode() == HttpErrorCode::OK)
                    {
                        if (std::string::npos != response.find_first_of('{'))
                        {
                            RcmClientUtils::TrimRcmResponse(response);

                            status = verifyResposneFunction(response, loginContext);

                            if (status == SVS_OK)
                            {
                                if (!loginContext.m_fingerprint.empty()) {
                                    bool isAnyCertSaveSuccess = false;
                                    std::vector<std::string> addresses = loginContext.m_address;
                                    for (std::vector<std::string>::iterator addressItr = addresses.begin();
                                        addressItr != addresses.end(); addressItr++)
                                    {
                                        std::string cert_addr = *addressItr;
                                        bool ok = false;
                                        ok = (securitylib::writeFingerprint(errMsg,
                                            cert_addr, loginContext.m_port,
                                            loginContext.m_fingerprint, true) && securitylib::writeCertificate(errMsg,
                                            cert_addr, loginContext.m_port, loginContext.m_certificate, true));

                                        if (!ok)
                                        {
                                            DebugPrintf(SV_LOG_ALWAYS,
                                                "%s: server fingerpint and certificate for address = %s, failed\n",
                                                FUNCTION_NAME,
                                                cert_addr.c_str());
                                        }
                                        isAnyCertSaveSuccess |= ok;
                                    }

                                    if (isAnyCertSaveSuccess)
                                    {
                                        g_fingerprintMgr.loadFingerprints(true);
                                        return SVS_OK;
                                    }

                                    return SVE_FAIL;
                                }
                            } 
                            else {
                                DebugPrintf(SV_LOG_ERROR, "%s: Response Verification Failed.\n", FUNCTION_NAME);
                            }                      
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "%s: CS Login response is not JSON. %s\n", FUNCTION_NAME,
                                response.c_str());
                            status = SVE_FAIL;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Post failed with response: %s\n", FUNCTION_NAME, response.c_str());
                        switch (httpResponse.GetResponseCode())
                        {
                        case HttpErrorCode::BAD_REQUEST:
                            m_errorCode = RCM_CLIENT_PROXY_IMPL_LOGIN_AUTH_FAILURE;
                            break;
                        case HttpErrorCode::FORBIDDEN:
                            m_errorCode = RCM_CLIENT_PROXY_IMPL_LOGIN_FORBIDDEN;
                            break;
                        default:
                            break;
                        }

                        status = SVE_HTTP_RESPONSE_FAILED;
                    }
                }

            } while (false);
        } 
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        if (status == SVE_ABORT)
        {
            m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
        }

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Login failed with ErrorMessage=%s\n", FUNCTION_NAME,errMsg.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientProxyImpl::ApplianceLogin(const std::vector<std::string> applianceAddresses,
        const std::string& serializedInput,
        std::string& response,
        const std::string& port,
        const std::string& applianceOtp,
        const std::string& endPoint,
        const std::string& clientRequestId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string req_id = clientRequestId.empty() ? GenerateUuid() : clientRequestId;
        std::string activity_id = GenerateUuid();
        
        RcmClientProxyLoginContext reqContext(applianceAddresses, port, endPoint,
            applianceOtp, serializedInput, req_id, activity_id, "ServerSignature");

        boost::function<SVSTATUS(std::string, RcmClientProxyLoginContext)> verifyFunc =
            boost::bind(&RcmClientProxyImpl::VerifyLoginResponse, this,_1, _2);

        status = doLogin(reqContext, response, verifyFunc);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return status;
    }

    SVSTATUS RcmClientProxyImpl::CsLogin(const std::string &ipaddr,
        const std::string &port)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;

        std::string req_id = GenerateUuid();
        std::string activity_id = GenerateUuid();

        std::vector<std::string> ipaddresses;
        ipaddresses.push_back(ipaddr);

        RcmClientProxyLoginContext reqContext(ipaddresses, port,
            RCM_ACTION_CS_LOGIN,
            securitylib::getPassphrase(),
            std::string(),
            req_id,
            activity_id, 
            std::string());

        boost::function<SVSTATUS(std::string,RcmClientProxyLoginContext)> verifyFunc =
            boost::bind(&RcmClientProxyImpl::VerifyLoginResponse, this,_1, _2);

        std::string resposne;
        status = doLogin(reqContext, resposne, verifyFunc);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return status;
    }

    SVSTATUS RcmClientProxyImpl::ConnectToRcmProxy(const std::vector<std::string> &rcmProxyAddr,
        const uint32_t &port, std::string &errMsg)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string req_id, rcmProxyUri, jsonOutput;

        std::string rcmEndPoint = TEST_CONNECTION;
        bool success = false;
        for (std::vector<std::string>::const_iterator it = rcmProxyAddr.begin();
            it != rcmProxyAddr.end(); it++)
        {
            std::string addr = *it;
            HttpClient rcm_client = GetRcmProxyClient();
            HttpRequest request = GetRcmProxyRequest(rcmEndPoint, addr, port,
                    std::string(), rcmProxyUri, req_id, false);

            DebugPrintf(SV_LOG_DEBUG, "Using RCM Proxy %s:%u to post request\n", addr.c_str(),
                port);
            status = GetRcmProxyResponse(rcm_client, request, errMsg, jsonOutput,
                rcmProxyUri, rcmEndPoint, req_id, false);
            if (status == SVS_OK)
            {
                DebugPrintf(SV_LOG_DEBUG, "RCM Proxy Post call succeeded\n");
                m_rcmProxyIp = addr;
                m_rcmProxyPort = port;
                success = true;
                break;
            }
        }

        if (!success)
        {
            DebugPrintf(SV_LOG_ERROR, "RCM proxy post call failed for all addresses\n");
            status = SVE_FAIL;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientProxyImpl::VerifyClientAuth(
        const RcmProxyTransportSetting &rcmProxyTransportSettings)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        std::string errMsg, response;

        try {

            std::vector<std::string> rcmProxyAddr;
            std::string port;
            uint32_t uiport;

            status = RcmConfigurator::getInstance()->GetRcmProxyAddress(rcmProxyTransportSettings,
                rcmProxyAddr, uiport);
            if (status != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to get RCM proxy address\n");
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return status;
            }

            bool success = false;
            for (std::vector<std::string>::iterator it = rcmProxyAddr.begin();
                it != rcmProxyAddr.end(); it++)
            {
                std::string addr = *it;
                port = boost::lexical_cast<std::string>(uiport);
                std::string rcmProxyUri = HTTPS_PREFIX;
                rcmProxyUri += addr;
                rcmProxyUri += ":";
                rcmProxyUri += port;

                std::string certificate, fingerprint;
                status = GetServerCert(rcmProxyUri, certificate, fingerprint);
                if (status != SVS_OK)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Failed to get SSL Server cert and fingerprint for RCM Proxy %s:%u\n",
                        addr.c_str(), uiport);
                    continue;
                }
                if (!boost::iequals(fingerprint,
                    rcmProxyTransportSettings.RcmProxyAuthDetails.ServerCertThumbprint))
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Mismatch of fingerprints between received: %s and in source config: %s\n",
                        fingerprint.c_str(),
                        rcmProxyTransportSettings.RcmProxyAuthDetails.ServerCertThumbprint.c_str());
                    continue;
                }

                bool ok = securitylib::writeFingerprint(response, *it, port, fingerprint, true);
                ok &= securitylib::writeCertificate(response, *it, port, certificate, true);
                if (!ok)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Failed to write server certificate or fingerprint for RCM Proxy %s:%u\n",
                        addr.c_str(), uiport);
                    continue;
                }
                success = true;
            }

            if (success)
            {
                g_fingerprintMgr.loadFingerprints(true);
                status = ConnectToRcmProxy(rcmProxyAddr, uiport, errMsg);
            }
            else {
                DebugPrintf(SV_LOG_ERROR, "Get server cert failed for all rcm proxy address\n");
                status = SVE_FAIL;
            }
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);
        if (status == SVE_ABORT)
        {
            m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }
}

#ifdef SV_WINDOWS
SVSTATUS RcmClientProxyImpl::AddClusterDiscoveryInfo(MachineEntity& me)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    throw INMAGE_EX("not applicable as called in failover cluster context");

}
#endif