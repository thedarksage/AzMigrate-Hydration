#ifndef _RCM_CLIENT_PROXY_IMPL_H
#define _RCM_CLIENT_PROCY_IMPL_H

#include <boost/thread/mutex.hpp>

#include "volumegroupsettings.h"
#include "AgentSettings.h"
#include "RcmClientErrorCodes.h"
#include "RcmJobs.h"
#include "HttpUtil.h"
#include "RcmClientImpl.h"
#include "HttpClient.h"
#include "RcmClientLoginContext.h"

using namespace AzureStorageRest;

namespace RcmTestClient {
    class TestClient;
}

namespace RcmClientLib {

   /// \brief RCM Proxy client implementation
    class RcmClientProxyImpl : public RcmClientImpl {
        
        std::string m_rcmProxyIp;

        uint32_t m_rcmProxyPort;

        std::string m_passphrase;

        /// \brief error code in case of failure
#ifdef WIN32
        static __declspec(thread) RCM_CLIENT_STATUS m_errorCode;
#else
        static __thread RCM_CLIENT_STATUS m_errorCode;
#endif

        /// \brief add the network adapters details to Machine entity
        SVSTATUS AddNetworkEntities(MachineEntity &me);

        /// \brief process the error response from RCM Proxy
        void ProcessErrorResponse(long responseCode, const std::string& strResponse);

        /// \brief fetch the cert and fingerprint of the server
        SVSTATUS GetServerCert(const std::string &uri,
            std::string &cert,
            std::string &fingerprint);

        SVSTATUS VerifyLoginResponse(const std::string& response, 
            const RcmClientProxyLoginContext& loginContext);

        SVSTATUS doLogin(RcmClientProxyLoginContext& loginContext,
            std::string& response,
            boost::function<SVSTATUS(std::string, RcmClientProxyLoginContext)> verifyResposneFunction);

        RcmClientProxyImpl();

        friend class RcmClient;
        friend class RcmTestClient::TestClient;

    public:

        virtual HttpClient GetRcmProxyClient();

        virtual HttpRequest GetRcmProxyRequest(const std::string &rcmEndPoint,
            const std::string &rcmProxyAddr,
            const uint32_t &rcmProxyPort,
            const std::string &clientRequestId,
            std::string &rcmProxyUri,
            std::string &req_id,
            bool isAgentMessage);

        virtual SVSTATUS GetRcmProxyResponse(HttpClient &rcm_client,
            HttpRequest &request,
            std::string &errMsg,
            std::string &jsonOutput,
            const std::string &rcmProxyUri,
            const std::string &rcmEndPoint,
            const std::string &req_id,
            bool isAgentMessage);

        virtual SVSTATUS PostToRcmProxy(const std::string &rcmEndPoint,
            const std::string &jsonInput,
            bool isAgentMessage,
            const std::map<std::string, std::string> &propertyMap,
            const std::string &clientRequestId,
            std::string &jsonOutput,
            const std::string &rcmProxyAddr,
            const uint32_t &rcmProxyPort);

        virtual SVSTATUS PostCommon(const std::string &rcmEndPoint,
            const std::string &jsonInput,
            bool isAgentMessage,
            const std::map<std::string, std::string> &propertyMap,
            const std::string &clientRequestId,
            std::string &jsonOutput);

        /// \brief posts the JSON string request to the RCMProxy
        virtual SVSTATUS PostWithHeaders(const std::string& rcmProxyEndPoint,
            const std::string &jsonStringInput,
            std::string &jsonStringOutput,
            const std::string &clientRequestId,
            const std::map<std::string, std::string>& headers,
            const bool& clusterVirtualNodeContext = false);

        virtual SVSTATUS PostAgentMessage(const std::string& endPoint,
            const std::string &serializedMessageInput,
            const std::map<std::string, std::string>& propertyMap);

        virtual SVSTATUS AddDiskDiscoveryInfo(MachineEntity& me,
            const VolumeSummaries_t& volumeSummaries,
            const VolumeDynamicInfos_t& volumeDynamicInfos,
            const bool bClusterContext);
#ifdef SV_WINDOWS
        virtual SVSTATUS AddClusterDiscoveryInfo(MachineEntity& me);
#endif
        virtual SVSTATUS AddNetworkDiscoveryInfo(MachineEntity& me);

        /// \brief CS Login implementation for connection mode RCM Proxy
        virtual SVSTATUS CsLogin(const std::string& ipaddr,
            const std::string& port);

        /// \brief does an Appliance login in RCM Proxy mode
        virtual SVSTATUS ApplianceLogin(const std::vector<std::string> applianceAddresses,
            const std::string& serializedInput,
            std::string& response,
            const std::string& port,
            const std::string& applianceOtp,
            const std::string& endPoint,
            const std::string& clientRequestId = std::string());

        virtual SVSTATUS ConnectToRcmProxy(const std::vector<std::string> &rcmProxyAddr,
            const uint32_t &port, std::string &errMsg);

        /// \brief Verify client authentication
        virtual SVSTATUS VerifyClientAuth(
            const RcmProxyTransportSetting &rcmProxyTransportSettings);

        /// \brief return error code
        virtual int32_t GetErrorCode() const { return m_errorCode; }
    };
}

#endif
