#ifndef RCM_CLIENT_OPTIONS_H
#define RCM_CLIENT_OPTIONS_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "svstatus.h"

namespace RcmClientLib
{

    /// \brief RCM client settings are populated during the agent install
    class RcmClientSettings
    {
    public:
        RcmClientSettings(const std::string& file)
        {
            m_SettingsFilePath = file;
            namespace bpt = boost::property_tree;
            bpt::ptree pt;
            bpt::ini_parser::read_ini(m_SettingsFilePath, pt);

            m_ServiceUri = pt.get("rcm.ServiceUri", "");
            m_ManagementId = pt.get("rcm.ManagementId", "");
            m_ClusterManagementId = pt.get("rcm.ClusterManagementId", "");
            m_BiosId = pt.get("rcm.BiosId", "");
            m_ClientRequestId = pt.get("rcm.ClientRequestId", "");
            m_ServiceConnectionMode = pt.get("rcm.ServiceConnectionMode", "direct");
            m_EndPointType  = pt.get("rcm.EndPointType", "Public");

            m_AADUri = pt.get(bpt::ptree::path_type("rcm.auth/AADUri", '/'), "");
            m_AADAuthCert = pt.get(bpt::ptree::path_type("rcm.auth/AADAuthCert", '/'), "");
            m_AADTenantId = pt.get(bpt::ptree::path_type("rcm.auth/AADTenantId", '/'), "");
            m_AADClientId = pt.get(bpt::ptree::path_type("rcm.auth/AADClientId", '/'), "");
            m_AADAudienceUri = pt.get(bpt::ptree::path_type("rcm.auth/AADAudienceUri", '/'), "");

            m_RelayKeyPolicyName = pt.get(bpt::ptree::path_type("rcm.relay/RelayKeyPolicyName", '/'), "");
            m_RelaySharedKey = pt.get(bpt::ptree::path_type("rcm.relay/RelaySharedKey",'/'), "");
            m_RelayServicePathSuffix = pt.get(bpt::ptree::path_type("rcm.relay/RelayServicePathSuffix", '/'), "");
            m_ExpiryTimeoutInSeconds = pt.get(bpt::ptree::path_type("rcm.relay/ExpiryTimeoutInSeconds", '/'), 60);
        }

        /// \brief [rcm] section params
        /// \brief RCM service URI
        std::string m_ServiceUri;

        /// \brief Management ID assigned by the management plane
        std::string m_ManagementId;

        /// \brief Cluster Management ID assigned by the management plane
        std::string m_ClusterManagementId;

        /// \brief BIOS ID as read from the machine when agent installed
        std::string m_BiosId;

        /// \brief Client Request ID to be used for register machine and register source agent
        std::string m_ClientRequestId;

        /// \brief supports 'relay' or 'direct' modes
        /// in relay mode, the agent will use the [rcm.relay] section
        std::string m_ServiceConnectionMode;

        /// \brief EndPointType
        ///  The type of end point used for 
        std::string m_EndPointType;

        /// \brief [rcm.auth] section
        /// \brief URI of Azure AD
        std::string m_AADUri;

        /// \brief cert to use for Azure AD
        std::string m_AADAuthCert;

        /// \brief tenant ID 
        std::string m_AADTenantId;

        /// \brief client ID
        std::string m_AADClientId;

        /// \brief Azure AD audience URI
        std::string m_AADAudienceUri;

        /// \brief [rcm.relay] section
        /// \brief relay key policy name
        std::string m_RelayKeyPolicyName;

        /// \brief relaay shared key
        std::string m_RelaySharedKey;

        /// \brief relay service path suffix to use
        std::string m_RelayServicePathSuffix;

        /// \brief relay shared key expiry time in seconds
        uint64_t m_ExpiryTimeoutInSeconds;

    private:
        std::string m_SettingsFilePath;
    public:

        SVSTATUS BackupRcmClientSettings();
        SVSTATUS PersistRcmClientSettings();
    };

	typedef boost::shared_ptr<RcmClientSettings> RcmClientSettingsPtr;
	typedef boost::shared_ptr<const RcmClientSettings> RcmClientSettingsConstPtr;
}

#endif
