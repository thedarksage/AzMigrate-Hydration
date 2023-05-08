/*
+------------------------------------------------------------------------------------------------ - +
Copyright(c) Microsoft Corp. 2019
+ ------------------------------------------------------------------------------------------------ - +
File        : RcmClientSettings.cpp

Description : RcmClientSettings class implementation

+ ------------------------------------------------------------------------------------------------ - +
*/
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "RcmClientSettings.h"
#include "logger.h"
#include "portablehelpers.h"
#include "fio.h"

using namespace RcmClientLib;

namespace RcmClientLib {

    SVSTATUS RcmClientSettings::BackupRcmClientSettings()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status = SVS_OK;
        boost::system::error_code ec;
        std::string utcTime, fileExtension, m_SettingsFilePathBackup;

        if (m_SettingsFilePath.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "Rcm Settings path is empty\n");
            status = SVE_FAIL;
        }
        else if (!boost::filesystem::exists(m_SettingsFilePath, ec))
        {
            DebugPrintf(SV_LOG_ERROR, "Rcm client settings file : %s does not exist.",
                m_SettingsFilePath.c_str());
            DebugPrintf(SV_LOG_ERROR, "Error : %s.\n", ec.message().c_str());
            status = SVE_FAIL;
        }
        else
        {
            utcTime = boost::posix_time::to_iso_extended_string(
                boost::posix_time::second_clock::universal_time());
            boost::algorithm::replace_all(utcTime, ":", "-");
            m_SettingsFilePathBackup = m_SettingsFilePath;
            fileExtension = boost::filesystem::extension(m_SettingsFilePathBackup);
            boost::algorithm::replace_last(m_SettingsFilePathBackup, fileExtension,
                utcTime + fileExtension);

            //TODO : delete files older than 30 days
            boost::filesystem::copy_file(m_SettingsFilePath, m_SettingsFilePathBackup, ec);
            if (ec != boost::system::errc::success)
            {
                DebugPrintf(SV_LOG_ERROR, "Copy file from %s to %s failed\n",
                    m_SettingsFilePath.c_str(), m_SettingsFilePathBackup.c_str());
                status = SVE_FAIL;
            }
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    SVSTATUS RcmClientSettings::PersistRcmClientSettings()
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS status;

        status = BackupRcmClientSettings();
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to create backup of %s\n", FUNCTION_NAME, m_SettingsFilePath.c_str());
            return status;
        }

        namespace bpt = boost::property_tree;
        bpt::ptree pt;
        bpt::ini_parser::read_ini(m_SettingsFilePath, pt);

        pt.put("rcm.ServiceUri", m_ServiceUri);
        pt.put("rcm.ManagementId", m_ManagementId);
        pt.put("rcm.ClusterManagementId", m_ClusterManagementId);
        pt.put("rcm.BiosId", m_BiosId);
        pt.put("rcm.ClientRequestId", m_ClientRequestId);
        pt.put("rcm.ServiceConnectionMode", m_ServiceConnectionMode);
        pt.put("rcm.EndPointType", m_EndPointType);

        pt.put(bpt::ptree::path_type("rcm.auth/AADUri", '/'), m_AADUri);
        pt.put(bpt::ptree::path_type("rcm.auth/AADAuthCert", '/'), m_AADAuthCert);
        pt.put(bpt::ptree::path_type("rcm.auth/AADTenantId", '/'), m_AADTenantId);
        pt.put(bpt::ptree::path_type("rcm.auth/AADClientId", '/'), m_AADClientId);
        pt.put(bpt::ptree::path_type("rcm.auth/AADAudienceUri", '/'), m_AADAudienceUri);

        pt.put(bpt::ptree::path_type("rcm.relay/RelayKeyPolicyName", '/'), m_RelayKeyPolicyName);
        pt.put(bpt::ptree::path_type("rcm.relay/RelaySharedKey",'/'), m_RelaySharedKey);
        pt.put(bpt::ptree::path_type("rcm.relay/RelayServicePathSuffix", '/'), m_RelayServicePathSuffix);
        pt.put(bpt::ptree::path_type("rcm.relay/ExpiryTimeoutInSeconds", '/'), m_ExpiryTimeoutInSeconds);

        // write and persist the changes to disk
        try
        {
            bpt::ini_parser::write_ini(m_SettingsFilePath, pt);

            FIO::Fio oFio(m_SettingsFilePath.c_str(), FIO::FIO_RW_EXISTING | FIO::FIO_WRITE_THROUGH);
            if (!oFio.flushToDisk())
            {
                status = SVE_FAIL;
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to update %s with error %s\n",
                    FUNCTION_NAME,
                    m_SettingsFilePath.c_str(),
                    oFio.errorAsString().c_str());
            }
            oFio.close();
        }
        catch (const std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to update %s with exception %s\n",
                FUNCTION_NAME,
                m_SettingsFilePath.c_str(),
                e.what());
            return SVE_FAIL;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }
}

