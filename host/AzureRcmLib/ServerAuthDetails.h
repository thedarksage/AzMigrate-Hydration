/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2021
+------------------------------------------------------------------------------------+
File    : ServerAuthDetails.h

Description : This file contains the auth details struct for RCM Proxy and Process Server.

+------------------------------------------------------------------------------------+
*/


#ifndef SERVER_ATUH_DETAILS_H
#define SERVER_ATUH_DETAILS_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#define MAKE_NAME_VALUE_STR(x) #x << ':' << x << ','

namespace RcmClientLib {

    /// \brief Cert signing types
    namespace CertificateSigningAuthority {
        const char SelfSigned[] = "SelfSigned";
        const char TrustedCertAuthority[] = "TrustedCertAuthority";
    }

    class ServerAuthDetailsBase
    {

    public:

        /// \brief CertificateSigningAuthority type of this auth detail
        std::string ServerCertSigningAuthority;

        /// \brief server cert thumbprint in case of self-signed cert
        std::string ServerCertThumbprint;

        /// \brief server cert thumbprint of self-signed cert that is renewed
        std::string ServerRolloverCertThumbprint;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ServerAuthDetailsBase", false);

            JSON_E(adapter, ServerCertSigningAuthority);
            JSON_E(adapter, ServerCertThumbprint);
            JSON_T(adapter, ServerRolloverCertThumbprint);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_P(node, ServerCertSigningAuthority);
            JSON_P(node, ServerCertThumbprint);
            JSON_P(node, ServerRolloverCertThumbprint);
        }

        bool operator==(const ServerAuthDetailsBase& rhs) const
        {
            return boost::iequals(ServerCertSigningAuthority, rhs.ServerCertSigningAuthority) &&
                boost::iequals(ServerCertThumbprint, rhs.ServerCertThumbprint) &&
                boost::iequals(ServerRolloverCertThumbprint, rhs.ServerRolloverCertThumbprint);
        }

        const std::string str() const
        {
            std::stringstream ss;
            ss << MAKE_NAME_VALUE_STR(ServerCertSigningAuthority)
                << MAKE_NAME_VALUE_STR(ServerCertThumbprint)
                << MAKE_NAME_VALUE_STR(ServerRolloverCertThumbprint);

            return ss.str();
        }
    };

    class RcmProxyAuthDetail : public ServerAuthDetailsBase {

    };

    class ProcessServerAuthDetail : public ServerAuthDetailsBase {

    };
}

#endif
