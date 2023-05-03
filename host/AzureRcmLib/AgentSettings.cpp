/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   AgentSettings.cpp

Description :   contains the agent settings cacher implmentation

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
#include "AgentSettings.h"

namespace RcmClientLib {


    /// \brief constructor for settings cacher
    /// \throws exception if local configurator is not initialized
    AgentSettingsCacher::AgentSettingsCacher(const std::string& cacheFilePath)
        :m_MTime(0)
    {
        if (cacheFilePath.empty())
        {
            std::string configDirName;
            LocalConfigurator::getConfigDirname(configDirName);
            m_cacheFilePath = configDirName;
            m_cacheFilePath /= "settings.json";
        }
        else {
            m_cacheFilePath = cacheFilePath;
        }
    }

    /// \brief persist the agent settings
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_FAIL on failure
    /// \li SVE_ABORT if caught exception
    SVSTATUS AgentSettingsCacher::Persist(AgentSettings& settings)
    {
        try {

            const std::string serializedSettings = JSON::producer<AgentSettings>::convert(settings);
            const std::string checksum = securitylib::genSha256Mac(serializedSettings.c_str(),
                serializedSettings.length());

            DebugPrintf(SV_LOG_DEBUG, "%s: prev checksum %s, current checksum %s\n",
                FUNCTION_NAME, m_prevChecksum.c_str(), checksum.c_str());

            if ((checksum == m_prevChecksum) && m_MTime && (m_MTime == GetLastModifiedTime()))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: no change in settings\n", FUNCTION_NAME);
                return SVS_OK;
            }

            std::string lockFile(m_cacheFilePath.string());
            lockFile += ".lck";
            if (!boost::filesystem::exists(lockFile)) {
                std::ofstream tmpLockFile(lockFile.c_str());
            }
            boost::interprocess::file_lock fileLock(lockFile.c_str());
            boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

            std::ofstream confFile(m_cacheFilePath.string().c_str(), std::ios::trunc);
            if (!confFile.good()) {
                m_errMsg = "unable to open settings file " + m_cacheFilePath.string() +
                    ": " + boost::lexical_cast<std::string>(errno);
                return SVE_FAIL;
            }

            confFile << serializedSettings;
            confFile.close();

            {
                boost::unique_lock<boost::shared_mutex> guard(m_settingsMutex);
                m_settings = settings;
                m_prevChecksum = checksum;
                m_MTime = GetLastModifiedTime();
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: settings updated.\n", FUNCTION_NAME);
        }
        catch (std::exception &e)
        {
            m_errMsg = "Persist caught exception : " + std::string(e.what());
            return SVE_ABORT;
        }
        catch (...)
        {
            m_errMsg = "Persist caught unknown exception.";
            return SVE_ABORT;
        }

        return SVS_OK;
    }

    /// \brief read the cached agent settings
    /// \returns
    /// \li SVS_OK on success
    /// \li SVE_FAIL on failure
    /// \li SVE_ABORT if caught exception
    SVSTATUS AgentSettingsCacher::Read(AgentSettings& settings) const
    {
        boost::system::error_code ec;
        if (!boost::filesystem::exists(m_cacheFilePath, ec))
        {
            m_errMsg = ec.message();
            return SVE_FILE_NOT_FOUND;
        }

        if (m_MTime && (m_MTime == GetLastModifiedTime()))
        {
            boost::shared_lock<boost::shared_mutex> guard(m_settingsMutex);
            settings = m_settings;
            return SVS_OK;
        }

        std::string lockFile(m_cacheFilePath.string());
        lockFile += ".lck";
        if (!boost::filesystem::exists(lockFile)) {
            std::ofstream tmpLockFile(lockFile.c_str());
        }
        boost::interprocess::file_lock fileLock(lockFile.c_str());
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

        std::ifstream confFile(m_cacheFilePath.string().c_str());
        if (!confFile.good()) {
            m_errMsg = "unable to open settings file " + m_cacheFilePath.string() +
                ": " + boost::lexical_cast<std::string>(errno);
            return SVE_FAIL;
        }

        try {
            std::string json((std::istreambuf_iterator<char>(confFile)), std::istreambuf_iterator<char>());
            settings = JSON::consumer<AgentSettings>::convert(json, true);
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

    time_t AgentSettingsCacher::GetLastModifiedTime() const
    {
        boost::system::error_code ec;
        time_t t = boost::filesystem::last_write_time(m_cacheFilePath, ec);
        if (ec)
        {
            m_errMsg = ec.message();
            t = 0;
        }
        return t;
    }
}
