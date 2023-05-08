#ifndef _RCM_CLIENT_IMPL_H
#define _RCM_CLIENT_IMPL_H

#include <boost/thread/mutex.hpp>

#include "volumegroupsettings.h"
#include "AgentSettings.h"
#include "RcmClientErrorCodes.h"
#include "RcmJobs.h"
#include "RcmClientSettings.h"
#include "HttpUtil.h"
#include "MachineEntity.h"
#include "ErrorLogger.h"

using namespace AzureStorageRest;

namespace RcmClientLib {



    /// \brief RCM client implementation
    class RcmClientImpl {
    
    public:

        /// \brief posts the JSON string request to the endpoint provided
        virtual SVSTATUS PostWithHeaders(const std::string& rcmProxyEndPoint,
            const std::string &jsonStringInput,
            std::string &jsonStringOutput,
            const std::string &clientRequestId,
            const std::map<std::string, std::string>& headers, 
            const bool& clusterVirtualNodeContext = false) = 0;

        virtual SVSTATUS Post(const std::string& rcmProxyEndPoint,
            const std::string &jsonStringInput,
            std::string &jsonStringOutput,
            const std::string &clientRequestId = std::string(), 
            const bool& clusterVirtualNodeContext = false)
        {
            std::map<std::string, std::string> headers;
            return PostWithHeaders(rcmProxyEndPoint, jsonStringInput,
                jsonStringOutput, clientRequestId, headers, clusterVirtualNodeContext);
        }

        virtual SVSTATUS AddDiskDiscoveryInfo(MachineEntity& me,
            const VolumeSummaries_t& volumeSummaries,
            const VolumeDynamicInfos_t& volumeDynamicInfos,
            const bool bClusterContext) = 0;
#ifdef SV_WINDOWS        
        virtual SVSTATUS AddClusterDiscoveryInfo(MachineEntity& me) = 0;
#endif
        virtual int32_t GetErrorCode() const = 0;

        virtual SVSTATUS AddNetworkDiscoveryInfo(MachineEntity& me) { return SVS_OK; }

        virtual SVSTATUS CsLogin(const std::string& ipaddr,
            const std::string& port)
        { return SVE_FAIL; }

        virtual SVSTATUS ApplianceLogin(const std::vector<std::string> applianceAddresses,
            const std::string& serializedInput,
            std::string& response,
            const std::string& port,
            const std::string& applianceOtp,
            const std::string& endPoint,
            const std::string& clientRequestId = std::string())
        {
            return SVE_FAIL;
        }

        virtual SVSTATUS VerifyClientAuth(
            const RcmProxyTransportSetting &rcmProxyTransportSettings)
        { return SVE_FAIL; }

        virtual SVSTATUS PostAgentMessage(const std::string& endPoint,
            const std::string &serializedMessageInput,
            const AzureStorageRest::headers_t& propertyMap)


        {
            return SVE_FAIL;
        }

    };

    typedef boost::shared_ptr<RcmClientImpl> RcmClientImplPtr;
}

#endif
