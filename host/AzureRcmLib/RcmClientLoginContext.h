#pragma once
#include <string>
#include "HttpResponse.h"
#include "RcmLibConstants.h"

using namespace AzureStorageRest;

namespace RcmClientLib
{
  class RcmClientProxyLoginContext
    {
    public:
        const std::vector<std::string> m_address;

        const std::string m_port;

        const std::string m_action;

        const std::string m_passphrase;

        const std::string m_serializedInput;

        const  std::string m_clientRequestId;

        const std::string m_activityId;

        const std::string m_rcmproxySignatureVerificaionJsonObject;

        std::string m_fingerprint;

        std::string m_certificate;

        std::string m_loginId;

        RcmClientProxyLoginContext(const std::vector<std::string>& address,
            const std::string& port,
            const std::string& action,
            const std::string& passphrase,
            const std::string& serializedInput,
            const std::string& clientRequestId,
            const std::string& activityId,
            const std::string& rcmproxySignatureVerificaionJsonObject)
            :m_address(address),
            m_port(port), m_action(action),
            m_passphrase(passphrase),
            m_serializedInput(serializedInput),
            m_clientRequestId(clientRequestId),
            m_activityId(activityId),
            m_rcmproxySignatureVerificaionJsonObject(rcmproxySignatureVerificaionJsonObject)
        {

        }

        std::vector<std::string> GetServerCertUris() const
        {
            std::vector<std::string> serverCertUris;
            for (int i = 0; i < m_address.size(); i++)
            {
                std::string serverCertUri = HTTPS_PREFIX + m_address[i] + ":" + m_port;
                serverCertUris.push_back(serverCertUri);
            }
             return serverCertUris;
        }

        std::vector<std::string> GetLoginUris() const
        {
            std::vector<std::string> loginUris;
            for (int i = 0; i < m_address.size(); i++)
            {
                std::string serverCertUri = HTTPS_PREFIX + m_address[i] + ":" + m_port + URI_DELIMITER::PATH + m_action;
                loginUris.push_back(serverCertUri);
            }
            return loginUris;
        }
    };
}