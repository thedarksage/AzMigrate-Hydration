/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   RcmProxyDetails.cpp

Description :   contains the RCM proxy settings cacher implmentation

+------------------------------------------------------------------------------------+
*/
#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <list>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "securityutils.h"
#include "localconfigurator.h"
#include "svstatus.h"
#include "json_reader.h"
#include "json_writer.h"
#include "RcmProxyDetails.h"

namespace RcmClientLib {


    /// \brief constructor for Rcm proxy settings cacher
    /// \throws exception if local configurator is not initialized
    RcmProxySettingsCacher::RcmProxySettingsCacher(const std::string& cacheFilePathRCMProxySettings)
        :m_MTime(0)
    {
        if (cacheFilePathRCMProxySettings.empty())
        {
            std::string configDirName;
            LocalConfigurator::getConfigDirname(configDirName);
            m_cacheFilePathRCMProxySettings = configDirName;
            m_cacheFilePathRCMProxySettings /= "RCMProxySettings.json";
        }
        else {
            m_cacheFilePathRCMProxySettings = cacheFilePathRCMProxySettings;
        }
    }

    /// \brief read the cached RCM proxy settings
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_FAIL on failure
    /// \li SVE_ABORT if caught exception
    SVSTATUS RcmProxySettingsCacher::Read(RcmProxyTransportSetting& settings) const
    {
        boost::system::error_code ec;
        if (!boost::filesystem::exists(m_cacheFilePathRCMProxySettings, ec))
        {
            m_errMsg = ec.message();
            return SVE_FILE_NOT_FOUND;
        }

        if (m_MTime && (m_MTime == GetLastModifiedTime()))
        {
            boost::shared_lock<boost::shared_mutex> guard(m_rcmProxySettingsMutex);
            settings = m_rcmProxySettings;
            return SVS_OK;
        }

        std::string lockFile(m_cacheFilePathRCMProxySettings.string());
        lockFile += ".lck";
        if (!boost::filesystem::exists(lockFile)) {
            std::ofstream tmpLockFile(lockFile.c_str());
        }
        boost::interprocess::file_lock fileLock(lockFile.c_str());
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

        std::ifstream confFile(m_cacheFilePathRCMProxySettings.string().c_str());
        if (!confFile.good()) {
            m_errMsg = "unable to open settings file " + m_cacheFilePathRCMProxySettings.string() +
                ": " + boost::lexical_cast<std::string>(errno);
            return SVE_FAIL;
        }

        try {
            std::string json((std::istreambuf_iterator<char>(confFile)), std::istreambuf_iterator<char>());
            settings = JSON::consumer<RcmProxyTransportSetting>::convert(json, true);
        }
        catch (std::exception &e)
        {
            m_errMsg = "Read caught exception : " + std::string(e.what());
            return SVE_ABORT;
        }
        catch (...)
        {
            m_errMsg = "Read caught unknown exception.";
            return SVE_ABORT;
        }

        confFile.close();

        return SVS_OK;
    }

    time_t RcmProxySettingsCacher::GetLastModifiedTime() const
    {
        boost::system::error_code ec;
        time_t t = boost::filesystem::last_write_time(m_cacheFilePathRCMProxySettings, ec);
        if (ec)
        {
            m_errMsg = ec.message();
            t = 0;
        }
        return t;
    }

    /// \brief persist the RCM proxy settings
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_FAIL on failure
    /// \li SVE_ABORT if caught exception
    SVSTATUS RcmProxySettingsCacher::PersistRCMProxySettings(RcmProxyTransportSetting& settings)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        try {

            const std::string serializedSettings = JSON::producer<RcmProxyTransportSetting>::convert(settings);
            const std::string checksum = securitylib::genSha256Mac(serializedSettings.c_str(),
                serializedSettings.length());

            DebugPrintf(SV_LOG_DEBUG, "%s: prev checksum %s, current checksum %s\n",
                FUNCTION_NAME, m_prevChecksumRCMProxySettings.c_str(), checksum.c_str());

            if (checksum == m_prevChecksumRCMProxySettings && m_MTime && (m_MTime == GetLastModifiedTime()))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: no change in RCM proxy settings\n", FUNCTION_NAME);
                return SVS_OK;
            }

            std::string lockFile(m_cacheFilePathRCMProxySettings.string());
            lockFile += ".lck";
            if (!boost::filesystem::exists(lockFile)) {
                std::ofstream tmpLockFile(lockFile.c_str());
            }
            boost::interprocess::file_lock fileLock(lockFile.c_str());
            boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

            std::ofstream confFile(m_cacheFilePathRCMProxySettings.string().c_str(), std::ios::trunc);
            if (!confFile.good()) {
                m_errMsg = "unable to open RCM proxy settings file " + m_cacheFilePathRCMProxySettings.string() +
                    ": " + boost::lexical_cast<std::string>(errno);
                return SVE_FAIL;
            }

            confFile << serializedSettings;
            confFile.close();
            {
                boost::unique_lock<boost::shared_mutex> guard(m_rcmProxySettingsMutex);
                m_rcmProxySettings = settings;
                m_prevChecksumRCMProxySettings = checksum;
                m_MTime = GetLastModifiedTime();
            }
            DebugPrintf(SV_LOG_DEBUG, "%s: Rcm proxy settings updated.\n", FUNCTION_NAME);
        }
        catch (std::exception &e)
        {
            m_errMsg = "PersistRCMProxySettings caught exception : " + std::string(e.what());
            return SVE_ABORT;
        }
        catch (...)
        {
            m_errMsg = "PersistRCMProxySettings caught unknown exception.";
            return SVE_ABORT;
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return SVS_OK;
    }
}
