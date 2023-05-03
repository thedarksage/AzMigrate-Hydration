#ifndef _RCM_CLIENT_PROXY_IMPL_H
#define _RCM_CLIENT_PROCY_IMPL_H

#include <boost/thread/mutex.hpp>

#include "volumegroupsettings.h"
#include "AgentSettings.h"
#include "RcmClientErrorCodes.h"
#include "RcmJobs.h"
#include "HttpUtil.h"
#include "RcmClientImpl.h"
#include "MachineEntity.h"
#include "AADAuthProvider.h"

using namespace AzureStorageRest;
using namespace AADAuthLib;

namespace RcmTestClient {
    class TestClient;
}

namespace RcmClientLib {


    /// \brief RCM Azure client implementation which directly communicates with RCM service
    class RcmClientAzureImpl : public RcmClientImpl {
        
        /// \brief error code in case of failure
#ifdef WIN32
        static __declspec(thread) RCM_CLIENT_STATUS m_errorCode;
#else
        static __thread RCM_CLIENT_STATUS m_errorCode;
#endif
        /// \brief process the error response from RCM Proxy
        void ProcessErrorResponse(long responseCode, const std::string& strResponse);

        /// \brief forms the header for service bus relay
        std::string GetServiceBusAuthHeader(const std::string& uri);

        SVSTATUS GetBearerToken(std::string& token);

        void AadExtendedErrorCallback(const std::string& message);

        RcmClientAzureImpl();

        boost::shared_ptr<AADAuthProvider>    m_ptrAADAuthProvider;

        friend class RcmClient;
        friend class RcmTestClient::TestClient;

    public:

        /// \brief post a request to RCM
        /// \returns
        /// \li SVS_OK on success
        /// \li SVE_HTTP_RESPONSE_FAILED if request returned error
        /// \li SVE_ABORT if exception is caught
        /// \li SVE_FAIL if fails to post request to RCM
        virtual SVSTATUS PostWithHeaders(const std::string& rcmProxyEndPoint,
            const std::string &jsonStringInput,
            std::string &jsonStringOutput,
            const std::string &clientRequestId,
            const std::map<std::string, std::string>& headers, 
            const bool& clusterVirtualNodeContext = false);

        virtual SVSTATUS AddDiskDiscoveryInfo(MachineEntity& me,
            const VolumeSummaries_t& volumeSummaries,
            const VolumeDynamicInfos_t& volumeDynamicInfos,
            const bool bClusterContext);

#ifdef SV_WINDOWS
        virtual SVSTATUS AddClusterDiscoveryInfo(MachineEntity& me);
#endif
        virtual SVSTATUS PostAgentMessage(const std::string& endPoint,
            const std::string &serializedMessageInput,
            const std::map<std::string, std::string>& propertyMap);

        /// \brief return error code
        virtual int32_t GetErrorCode() const { return m_errorCode; }

    };
}

#endif
