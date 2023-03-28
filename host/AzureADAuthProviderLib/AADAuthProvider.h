#ifndef AAD_AUTH_PROVIDER_H
#define AAD_AUTH_PROVIDER_H

#include "json_reader.h"
#include "json_writer.h"

#include <boost/thread/mutex.hpp>

#include "svstatus.h"
#include "HttpUtil.h"
#include "HttpClient.h"
#include "RcmClientSettings.h"
#include "RcmClientErrorCodes.h"

using namespace RcmClientLib;
using namespace AzureStorageRest;

namespace AADAuthLib
{
    class AzureAuth {
    public:
        AzureAuth()
        {
            m_notBeforeTimeInSecSinceEpoch = 0;
            m_expiryTimeInSecSinceEpoch = 0;
        }

        bool GetAccessToken(std::string& token);

        SVSTATUS StoreAccessToken(std::string& response);

    private:
        std::string m_accessToken;
        uint64_t m_notBeforeTimeInSecSinceEpoch;
        uint64_t m_expiryTimeInSecSinceEpoch;
    };

    /// \brief RCM client settings are populated during the agent install
    class AADAuthProvider
    {
    public:
        /// \brief initialize HttpClient and curl
        static void Initialize()
        {
            HttpClient::GlobalInitialize();
            HttpClient::SetRetryPolicy(0);
        }

        /// \brief uninitialize HttpClient and curl
        static void Uninitialize()
        {
            HttpClient::GlobalUnInitialize();
        }

        AADAuthProvider(const RcmClientSettings &settings,
            boost::function<void(const std::string& msg)> extendedErrorCallback = 0) : m_clientSettings(settings)
        {
            if (extendedErrorCallback)
                m_extendedErrorCallback = extendedErrorCallback;
        }

        int32_t GetErrorCode()
        {
            return (int32_t)m_errorCode;
        }

        void SetHttpProxy(const std::string& address,
            const std::string& port,
            const std::string& bypasslist)
        {
            m_proxy.m_address = address;
            m_proxy.m_port = port;
            m_proxy.m_bypasslist = bypasslist;
            return;
        }

        /// \brief create the JWT request to for access token
        SVSTATUS GetJwtRequest(std::string& jwtRequest);

        /// \brief gets the bearer token from Azure AD
        SVSTATUS GetBearerToken(std::string& token);

    protected:
        std::string JwtBase64Encode(const std::string& data);

        bool IsUriAdfs(const std::string& AADUri);

    private:
        /// \brief the RCM settings as read from the RCMInfo.conf
        const RcmClientSettings m_clientSettings;

        boost::mutex m_authTokenMutex;

        AzureAuth m_auth;

        HttpProxy m_proxy;

        /// \brief error code in case of failure
#ifdef WIN32
        static __declspec(thread) RCM_CLIENT_STATUS m_errorCode;
#else
        static __thread RCM_CLIENT_STATUS m_errorCode;
#endif

    private:
        /// \brief a callback for extended errors
        boost::function<void(const std::string& msg)> m_extendedErrorCallback;

        void ProcessErrorResponse(long responseCode, const std::string& strResponse);
    };

    /// <summary>
    ///  ref https://docs.microsoft.com/en-us/azure/active-directory/develop/reference-aadsts-error-codes
    /// </summary>
    class OAuthErrorResponse {
    public:
        std::string error;
        std::string error_description;
        std::vector<int64_t> error_codes;

        /// \brief a serialize function for JSON serializer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OAuthErrorResponse", false);

            JSON_E(adapter, error);
            JSON_E(adapter, error_description);
            JSON_T(adapter, error_codes);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, error);
            JSON_P(node, error_description);
            JSON_VP(node, error_codes);
        }
    };
}

#endif // AAD_AUTH_PROVIDER_H
