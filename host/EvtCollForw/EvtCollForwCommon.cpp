#include <boost/assign.hpp>

#include "localconfigurator.h"
#include "portablehelpersmajor.h"

#include "EvtCollForwCommon.h"
#include "cxps.h"
#include "serveroptions.h"

#include "zlib.h"
#include "zflate.h"
#include "listfile.h"

#include <sstream>

extern boost::shared_ptr<LocalConfigurator> lc;

namespace EvtCollForw
{
    // Initialize remap from v2a tables to a2a tables/csprime tables
    const std::map<std::string, std::string> V2ARcmTableRemap = boost::assign::map_list_of
        (V2ATableNames::AdminLogs, A2ATableNames::AdminLogs)
        (V2ATableNames::DriverTelemetry, A2ATableNames::DriverTelemetry)
        (V2ATableNames::PSV2, A2ATableNames::PSV2)
        (V2ATableNames::PSTransport, A2ATableNames::PSTransport)
        (V2ATableNames::SourceTelemetry, A2ATableNames::SourceTelemetry)
        (V2ATableNames::Vacp, A2ATableNames::Vacp)
        (V2ATableNames::AgentSetupLogs, A2ATableNames::AgentSetupLogs);

    boost::filesystem::path Utils::GetBookmarkFolderPath()
    {
        BOOST_ASSERT(lc);
        boost::filesystem::path installPath = lc->getLogPathname();

        const char* AppDataFolderName =
#ifdef SV_WINDOWS
            "Application Data";
#else
            "ApplicationData";
#endif

        // TODO-SanKumar-1612: Move this value to configuration.
        return installPath / AppDataFolderName / "bookmarks";
    }

    std::string Utils::GetAgentLogFolderPath()
    {
        BOOST_ASSERT(lc);
#ifdef SV_WINDOWS
        std::string logDirName(lc->getInstallPath());
#else
        std::string logDirName(lc->getLogPathname());
#endif
        if (logDirName[logDirName.length() - 1] != ACE_DIRECTORY_SEPARATOR_CHAR_A)
        {
            logDirName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        }
        return logDirName;
    }

    std::string Utils::GetDPResyncLogFolderPath()
    {
        return std::string(GetAgentLogFolderPath() + Vxlogs_Folder + ACE_DIRECTORY_SEPARATOR_CHAR_A + Resync_Folder + ACE_DIRECTORY_SEPARATOR_CHAR_A);
    }

    std::string Utils::GetRcmMTDPResyncLogFolderPath(std::string path)
    {
        std::string RcmMTDPResyncLogFolderPath = (boost::filesystem::path(path) / Vxlogs_Folder / Resync_Folder).string();
        boost::trim_right_if(RcmMTDPResyncLogFolderPath, boost::is_any_of("/\\"));
        return RcmMTDPResyncLogFolderPath + ACE_DIRECTORY_SEPARATOR_STR_A;
    }

    std::string Utils::GetDPCompletedIRLogFolderPath()
    {
        BOOST_ASSERT(lc);
        return std::string(GetAgentLogFolderPath() + Vxlogs_Folder + ACE_DIRECTORY_SEPARATOR_CHAR_A + lc->getEvtCollForwIRCompletedFilesMoveDir() + ACE_DIRECTORY_SEPARATOR_CHAR_A);
    }

    std::string Utils::GetRcmMTDPCompletedIRLogFolderPath(std::string path)
    {
        std::string RcmMTDPCompletedIRLogFolderPath = (boost::filesystem::path(path) / Vxlogs_Folder / lc->getEvtCollForwIRCompletedFilesMoveDir()).string();
        boost::trim_right_if(RcmMTDPCompletedIRLogFolderPath, boost::is_any_of("/\\"));
        return RcmMTDPCompletedIRLogFolderPath + ACE_DIRECTORY_SEPARATOR_STR_A;
    }

    std::string Utils::GetDPDiffsyncLogFolderPath()
    {
        return std::string(GetAgentLogFolderPath() + Vxlogs_Folder + ACE_DIRECTORY_SEPARATOR_CHAR_A + Diffsync_Folder + ACE_DIRECTORY_SEPARATOR_CHAR_A);
    }

    void Utils::GetPSSettingsMap(std::map<std::string, std::string> &psSettings)
    {
        psSettings.clear();
        PSSettings::PSSettingsPtr psSettingPtr = PSSettings::PSSettingsConfigurator::GetInstance().GetPSSettings();
        if (psSettingPtr.get() != NULL)
        {
            boost::shared_ptr<PSSettings::PSProtMacTelSettingsPtrsMap> telemetrySettingsPtr = psSettingPtr->TelemetrySettings;
            if (telemetrySettingsPtr.get() != NULL)
            {
                for (PSSettings::PSProtMacTelSettingsPtrsMap::iterator itr = telemetrySettingsPtr->begin(); itr != telemetrySettingsPtr->end(); itr++)
                {
                    PSSettings::PSProtMacTelSettingsPtr hostTelemetrySettingPtr = itr->second;
                    if (hostTelemetrySettingPtr.get() != NULL)
                    {
                        psSettings[hostTelemetrySettingPtr->HostId] = hostTelemetrySettingPtr->TelemetryFolderPath;
                    }
                }
            }
        }
    }

    void Utils::InitPSSettingConfigurator()
    {
        PSSettings::PSSettingsConfigurator& psconfigurator = PSSettings::PSSettingsConfigurator::GetInstance();
        boost::filesystem::path installPath(GetRcmPSInstallationInfo().m_installLocation);
        const boost::filesystem::path psSettingsFilePath(GetRcmPSInstallationInfo().m_settingsPath);

        const boost::filesystem::path psSettingsLckFilePath =
            boost::filesystem::change_extension(
                psSettingsFilePath, psSettingsFilePath.extension().string() + ".lck");

        boost::filesystem::path confFilePath = (installPath / PSComponentPaths::HOME / PSComponentPaths::SVSYSTEMS / PSComponentPaths::TRANSPORT / "cxps.conf");
        boost::filesystem::path inspath = (installPath / PSComponentPaths::HOME / PSComponentPaths::SVSYSTEMS / PSComponentPaths::TRANSPORT);

        serverOptionsPtr serverOptions(new ServerOptions(
            confFilePath.string(),
            inspath.string()));

        psconfigurator.Initialize(psSettingsFilePath, psSettingsLckFilePath, serverOptions);
        psconfigurator.Start();
    }

    void Utils::CleanupPSSettingConfigurator()
    {
        PSSettings::PSSettingsConfigurator& psconfigurator = PSSettings::PSSettingsConfigurator::GetInstance();
        psconfigurator.Stop();
    }

    void Utils::InitRcmConfigurator()
    {
        RcmClientLib::RcmConfigurator::Initialize(FILE_CONFIGURATOR_MODE_CS_PRIME_APPLIANCE_EVTCOLLFORW);
    }

    void Utils::StartRcmConfigurator()
    {
        RcmClientLib::RcmConfigurator::getInstance()->Start(RcmClientLib::SETTINGS_SOURCE_LOCAL_CACHE);
    }

    void Utils::StopRcmConfigurator()
    {
        RcmClientLib::RcmConfigurator::getInstance()->Stop();
    }

    void Utils::InitGlobalParams()
    {
        std::map<std::string, std::string> globalParams;
        setGlobalParams(globalParams);
        RcmClientLib::RcmConfigurator::getInstance()->setGlobalEvtCollForwParams(globalParams);
    }

    void Utils::setGlobalParams(std::map<std::string, std::string> & globalParams)
    {
        boost::filesystem::path installPath(GetRcmPSInstallationInfo().m_installLocation);
        installPath /= PSComponentPaths::HOME;
        installPath /= PSComponentPaths::SVSYSTEMS;

        const boost::filesystem::path psConfFilePath = installPath / PSComponentPaths::ETC / "ProcessServer.conf";
        const std::string psConfParamHostId("HostId");
        const std::string psConfParamLogLevel("LogLevel");

        try
        {
            boost::system::error_code ec;
            if (boost::filesystem::exists(psConfFilePath.string(), ec))
            {
                boost::property_tree::ptree	psconfparams;
                std::ifstream localconffile(psConfFilePath.string().c_str());
                boost::property_tree::json_parser::read_json(localconffile, psconfparams);
                const std::string psHostId = psconfparams.get<std::string>(psConfParamHostId);
                if (!psHostId.empty())
                {
                    globalParams[MdsColumnNames::HostId] = psHostId;
                }
                else
                {
                    DebugPrintf(EvCF_LOG_ERROR, "%s: Param %s is not found in PS conf file \"%s\", error=%s.\n", FUNCTION_NAME,
                        psConfParamHostId.c_str(), psConfFilePath.string().c_str());
                }

                globalParams[psConfParamLogLevel] = boost::lexical_cast<std::string>(SV_LOG_DISABLE);
                const std::string psLogLevel = psconfparams.get<std::string>(psConfParamLogLevel, std::string());
                if (!psLogLevel.empty())
                {
                    SV_LOG_LEVEL logLevel = static_cast<SV_LOG_LEVEL>(boost::lexical_cast<int>(psLogLevel));
                    if (logLevel <= SV_LOG_ALWAYS && logLevel >= SV_LOG_DISABLE)
                    {
                        globalParams[psConfParamLogLevel] = psLogLevel;
                        if (logLevel == SV_LOG_ALWAYS)
                        {
                            RcmClientLib::RcmConfigurator::getInstance()->EnableVerboseLogging();
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(EvCF_LOG_ERROR, "%s: PS conf file does not exists \"%s\", error=%s.\n", FUNCTION_NAME,
                    psConfFilePath.string().c_str(), ec.message().c_str());
            }
        }
        catch (const boost::property_tree::ptree_error &pe)
        {
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s not found in %s, exception caught=%s.\n", FUNCTION_NAME,
                psConfParamHostId.c_str(), psConfFilePath.string().c_str(), pe.what());
        }
        catch (const std::exception &e)
        {
            DebugPrintf(EvCF_LOG_ERROR, "%s: Failed to init config params from \"%s\" with exception %s.\n", FUNCTION_NAME,
                psConfFilePath.string().c_str(), e.what());
        }
    }

#ifdef SV_WINDOWS
    bool Utils::TryConvertWstringToUtf8(const std::wstring& wstr, std::string& str)
    {
        try
        {
            str = convertWstringToUtf8(wstr);
        }
        catch (std::bad_alloc)
        {
            DebugPrintf(EvCF_LOG_ERROR, "%s - Failed to allocate memory.\n", FUNCTION_NAME);
            throw;
        }
        GENERIC_CATCH_LOG_ACTION(FUNCTION_NAME, str.clear(); return false);
        // TODO-SanKumar-1702: Should we rather return the exception string itself, so that we could
        // detect bugs in production?

        return true;
    }
#endif //SV_WINDOWS

    bool Utils::TryDeleteFile(const std::string& filePath)
    {
        try
        {
            return Utils::TryDeleteFile(boost::filesystem::path(filePath));
        }
        GENERIC_CATCH_LOG_ACTION("Deleting file", return false);
    }

    bool Utils::TryDeleteFile(const boost::filesystem::path& filePath)
    {
        try
        {
            if (!filePath.is_absolute())
            {
                // TODO-SanKumar-1703: Should we throw/log in this case?
                BOOST_ASSERT(false);
                return false;
            }

            boost::system::error_code errorCode;
            if (!boost::filesystem::remove(filePath, errorCode))
            {
                DebugPrintf(EvCF_LOG_WARNING,
                    "%s - Failed to delete the file : %s. Error code : 0x%08x.\n",
                    FUNCTION_NAME, filePath.string().c_str(), errorCode.value());

                return false;
            }

            DebugPrintf(EvCF_LOG_DEBUG, "%s - Deleted the file : %s.\n", FUNCTION_NAME, filePath.string().c_str());
        }
        GENERIC_CATCH_LOG_ACTION("Deleting file", return false);

        return true;
    }

    bool Utils::TryGetBiosId(std::string& biosId)
    {
        try
        {
            biosId = GetSystemUUID();
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
        GENERIC_CATCH_LOG_ACTION("Getting bios ID", return false);

        return true;
    }

    // TODO-SanKumar-1708: Should we ship the exception instead of the value of the column,
    // to get bugs in production? Same for above.

    bool Utils::TryGetFabricType(std::string& fabricType)
    {
        try
        {
            std::string hypervisorName, hypervisorVer;
            IsVirtual(hypervisorName, hypervisorVer);

            if (!hypervisorVer.empty())
                fabricType = hypervisorName + '_' + hypervisorVer;
            else
                fabricType = hypervisorName;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
        GENERIC_CATCH_LOG_ACTION("Getting fabric type", return false);

        return true;
    }

    std::string Utils::GetUniqueTime()
    {
        static int counter = 1;
        static uint64_t prevTimestamp = 0;

        uint64_t timestamp = GetTimeInSecSinceEpoch1970();
        if (timestamp != prevTimestamp)
        {
            prevTimestamp = timestamp;
            counter = 1;
        }

        std::ostringstream ss;
        ss << timestamp << (counter++);
        return ss.str();
    }

    void Utils::CompressFile(const std::string& filePath)
    {
        std::string compressName(filePath + ".gz");

        Zflate zFlate(Zflate::COMPRESS);

        zFlate.process(filePath, compressName, true);
    }
    void Utils::DeleteOlderFiles(const std::string& fileSpec, const int maxFileCount)
    {
        boost::filesystem::path fileSpecPath(fileSpec);
        fileSpecPath.concat("*.gz");

        std::string files;
        ListFile::listFileGlob(fileSpecPath, files, false); // only want file names in this case

        DebugPrintf(EvCF_LOG_DEBUG, "%s : spec %s files %s.\n",
            FUNCTION_NAME,
            fileSpecPath.string().c_str(),
            files.c_str());

        if (files.empty()) {
            return;
        }

        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> sep("\n");
        tokenizer_t tokens(files, sep);
        tokenizer_t::iterator iter(tokens.begin());
        tokenizer_t::iterator iterEnd(tokens.end());
        size_t numFiles = 0;
        std::vector<std::string> vlogFiles;
        for (/* empty */; iter != iterEnd; ++iter) {
            std::string::size_type idx = (*iter).find(".gz");
            if ((std::string::npos != idx) &&
                (3 == (*iter).size() - idx)) // check if ends in ".gz"
            {
                vlogFiles.push_back(*iter);
                numFiles++;
                if (numFiles > maxFileCount)
                {
                    DebugPrintf(EvCF_LOG_DEBUG, "%s : deleting file %s.\n",
                        FUNCTION_NAME,
                        vlogFiles.front().c_str());

                    boost::filesystem::remove(vlogFiles.front());
                    vlogFiles.erase(vlogFiles.begin());
                    numFiles--;
                }
            }
        }
    }

    std::string Utils::GetA2ATableNameFromV2ATableName(const std::string& v2aTableName)
    {
        return V2ARcmTableRemap.count(v2aTableName) ? V2ARcmTableRemap.at(v2aTableName) : std::string();
    }

    bool Utils::IsPrivateEndpointEnabled(const std::string& environment)
    {
        bool bPEEnabled = false;
        if (environment == EvtCollForw_Env_V2ARcmSourceOnAzure)
        {
            bPEEnabled = RcmClientLib::RcmConfigurator::getInstance()->IsPrivateEndpointEnabled();
        }
        else if (environment == EvtCollForw_Env_V2ARcmPS)
        {
            bPEEnabled = PSSettings::PSSettingsConfigurator::GetInstance().GetPSSettings()->IsPrivateEndpointEnabled;
        }
        else
        {
            DebugPrintf(EvCF_LOG_ERROR, "%s: unsupported environment %s\n", FUNCTION_NAME, environment.c_str());
        }
        return bPEEnabled;
    }
}