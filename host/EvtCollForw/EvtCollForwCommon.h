#ifndef EVT_COLL_FORW_COMMON_H
#define EVT_COLL_FORW_COMMON_H

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include "json_writer.h"

#include "RcmConfigurator.h"
#include "pssettingsconfigurator.h"

#include "portablehelpers.h"
#include "setpermissions.h"

#define EvCF_LOG_ERROR SV_LOG_DEBUG
#define EvCF_LOG_WARNING SV_LOG_DEBUG
#define EvCF_LOG_INFO SV_LOG_DEBUG
#define EvCF_LOG_DEBUG SV_LOG_DEBUG

#define GENERIC_CATCH_LOG_IGNORE(Operation) \
    catch (const std::exception &ex) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", ex.what()); \
    } \
    catch (const std::string &thrownStr) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", thrownStr.c_str()); \
    } \
    catch (const char *thrownStrLiteral) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", thrownStrLiteral); \
    } \
    catch (...) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". Hit an unknown exception.\n"); \
    }

#define GENERIC_CATCH_LOG_ERRCODE(Operation, err) \
    catch (const std::exception &ex) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", ex.what()); \
        err = EvCF_Exception; \
    } \
    catch (const std::string &thrownStr) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", thrownStr.c_str()); \
        err = EvCF_Exception; \
    } \
    catch (const char *thrownStrLiteral) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", thrownStrLiteral); \
        err = EvCF_Exception; \
    } \
    catch (...) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". Hit an unknown exception.\n"); \
        err = EvCF_Exception; \
    }

#define GENERIC_CATCH_LOG_ACTION(Operation, action) \
    catch (const std::exception &ex) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", ex.what()); \
        action; \
    } \
    catch (const std::string &thrownStr) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", thrownStr.c_str()); \
        action; \
    } \
    catch (const char *thrownStrLiteral) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". %s.\n", thrownStrLiteral); \
        action; \
    } \
    catch (...) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "FAILED: " Operation ". Hit an unknown exception.\n"); \
        action; \
    }

#define CATCH_LOG_AND_RETHROW_ON_NO_MEMORY() \
    catch(std::bad_alloc) \
    { \
        DebugPrintf(EvCF_LOG_ERROR, "%s - Out of memory!.\n", FUNCTION_NAME); \
        throw; \
    }


namespace EvtCollForw
{
    enum ErrorCode
    {
        EvCF_Success = 0,
        // In order to not to collide with the system error code in windows, 29-th (customer) bit
        // is set in our self-defined error codes.
        EvCF_InvalidCmdLineArg = 0x20000001,
        EvCF_DuplicateProcess,
        EvCF_CancellationRequested,
        EvCF_Exception,
        EvCF_Penalized,
    };

    namespace V2AMdsTableNames
    {
        static const char *AdminLogs = "InMageAdminLogV2";
        static const char *DriverTelemetry = "InMageTelemetryInDskFltV2";
        static const char *SourceTelemetry = "InMageTelemetrySourceV2";
        static const char *Vacp = "InMageTelemetryVacpV2";
        static const char *SchedulerTelemetry = "InMageTelemetrySrcTaskSchedV2";
        static const char *AgentSetupLogs = "AgentSetupLogs";
        static const char *PSV2 = "InMageTelemetryPSV2";
        static const char *PSTransport = "InMageTelemetryPSTransport";
    }

    namespace V2ATableNames = V2AMdsTableNames;

    namespace A2ATableNames
    {
        static const char *AdminLogs = "RcmAgentAdminEvent";
        static const char *DriverTelemetry = "RcmAgentTelemetryInDskFltV2";             // need to verify if this holds for V2A Rcm stack
        static const char *SourceTelemetry = "RcmAgentTelemetrySourceV2";
        static const char *Vacp = "RcmAgentTelemetryVacpV2";
        static const char *PSV2 = "RcmAgentTelemetryPSV2";
        static const char *PSTransport = "RcmAgentTelemetryPSTransport";
        static const char *AgentSetupLogs = "AgentSetupLogs";
    }

    namespace A2AMdsTableMapping
    {
        static const char *AdminLogs = "RcmAgentAdminEvent_mapping_v1";
        static const char *DriverTelemetry = "RcmAgentTelemetryInDskFltV2_mapping_v1";             // need to verify if this holds for V2A Rcm stack
        static const char *SourceTelemetry = "RcmAgentTelemetrySourceV2_mapping_v1";
        static const char *Vacp = "RcmAgentTelemetryVacpV2_mapping_v1";
        static const char *PSV2 = "RcmAgentTelemetryPSV2_mapping_v1";
        static const char *PSTransport = "RcmAgentTelemetryPSTransport_mapping_v1";
        static const char *AgentSetupLogs = "AgentSetupLogs_mapping_v1";
    }

    namespace MdsColumnNames
    {
        static const std::string FabType = "FabType";
        static const std::string BiosId = "BiosId";
        static const std::string SequenceNumber = "SequenceNumber";
        static const std::string MemberName = "MemberName";
        static const std::string SourceFilePath = "SourceFilePath";
        static const std::string SourceLineNumber = "SourceLineNumber";
        static const std::string AgentTimeStamp = "AgentTimeStamp";
        static const std::string AgentLogLevel = "AgentLogLevel";
        static const std::string MachineId = "MachineId";
        static const std::string ComponentId = "ComponentId";
        static const std::string ErrorCode = "ErrorCode";
        static const std::string DiskId = "DiskId";
        static const std::string DiskNumber = "DiskNumber";
        static const std::string SubComponent = "SubComponent";
        static const std::string AgentPid = "AgentPid";
        static const std::string AgentTid = "AgentTid";
        static const std::string ExtendedProperties = "ExtendedProperties";
        static const std::string Message = "Message";
        static const std::string SourceEventId = "SourceEventId";
        static const std::string HostId = "HostId";
        static const std::string SrcLoggerTime = "SrcLoggerTime";
        static const std::string EventRecId = "EventRecId";
        static const std::string ClientRequestId = "ClientRequestId";
        static const std::string PSHostId = "PSHostId";
    }

    namespace PSComponentPaths
    {
        static const std::string HOME = "home";
        static const std::string SVSYSTEMS = "svsystems";
        static const std::string TRANSPORT = "transport";
        static const std::string VAR = "var";
        static const std::string ETC = "etc";
    }

    class CmdLineSettings
    {
    public:
        std::string Environment;
    };

    class Utils
    {
    public:
        static bool ReadFileContents(const std::string &filePath, std::string &contents)
        {
            return ReadFileContentsInternal<std::string, std::ifstream>(filePath, contents);
        }

        static bool ReadFileContents(const std::string &filePath, std::wstring &contents)
        {
            return ReadFileContentsInternal<std::wstring, std::wifstream>(filePath, contents);
        }

        static bool CommitState(const std::string &filePath, const std::string &stateText)
        {
            return CommitStateInternal<std::string, std::ofstream>(filePath, stateText);
        }

        static bool CommitState(const std::string &filePath, const std::wstring &stateText)
        {
            return CommitStateInternal<std::wstring, std::wofstream>(filePath, stateText);
        }

        static boost::filesystem::path GetBookmarkFolderPath();

        static std::string GetAgentLogFolderPath();

        static std::string GetDPResyncLogFolderPath();

        static std::string GetRcmMTDPResyncLogFolderPath(std::string path);

        static std::string GetDPCompletedIRLogFolderPath();

        static std::string GetRcmMTDPCompletedIRLogFolderPath(std::string path);

        static std::string GetDPDiffsyncLogFolderPath();

        static bool TryConvertWstringToUtf8(const std::wstring& wstr, std::string& str);

        static bool TryDeleteFile(const std::string& filePath);
        static bool TryDeleteFile(const boost::filesystem::path& filePath);

        static bool TryGetBiosId(std::string& biosId);
        static bool TryGetFabricType(std::string& fabricType);

        static void GetPSSettingsMap(std::map<std::string, std::string> &psSettings);

        static void InitPSSettingConfigurator();

        static void CleanupPSSettingConfigurator();

        static void InitRcmConfigurator();

        static void StartRcmConfigurator();

        static void StopRcmConfigurator();

        static void InitGlobalParams();

        static std::string GetUniqueTime();

        static void CompressFile(const std::string& filePath);

        static void DeleteOlderFiles(const std::string& fileSpec, const int maxFileCount);

        static std::string GetA2ATableNameFromV2ATableName(const std::string& v2aTableName);

        static bool IsPrivateEndpointEnabled(const std::string& environment);

    private:
        template <class StringType, class IFileStreamType>
        static bool ReadFileContentsInternal(const std::string &filePath, StringType &contents)
        {
            // Copied from boost::filesystem::load_string_file()
            IFileStreamType file;
            file.open(filePath.c_str(), std::ios_base::binary);
            std::size_t sz = static_cast<std::size_t>(boost::filesystem::file_size(filePath));
            contents.reserve(sz);
            std::istreambuf_iterator<typename StringType::value_type> itrBegin(file), itrEnd;
            contents.assign(itrBegin, itrEnd);

            return !file.fail(); // TODO-SanKumar-1612: Log
        }

        template <class StringType, class OFileStreamType>
        static bool CommitStateInternal(const std::string &filePath, const StringType &stateText)
        {
            boost::filesystem::path targetFilePath(filePath);
            boost::filesystem::path tempFilePath = targetFilePath.parent_path();
            tempFilePath /= targetFilePath.stem().string() + GenerateUuid();
            tempFilePath.replace_extension(targetFilePath.extension());

            boost::system::error_code errorCode;

            {
                // Copied from boost::filesystem::save_string_file()
                OFileStreamType file;

                if (!boost::filesystem::exists(tempFilePath.parent_path(), errorCode))
                    boost::filesystem::create_directories(tempFilePath.parent_path(), errorCode);

                if (errorCode)
                    return false; // TODO-SanKumar-1612: Log

                file.open(tempFilePath.string().c_str(), std::ios_base::binary | std::ios_base::trunc);
                file << stateText;

                if (file.fail())
                    return false; // TODO-SanKumar-1612: Log

                // TODO-SanKumar-1708: Any error while closing the stream is missed.
                // Should we simply go with setting the exception to be thrown for fail|bad?
            }

            boost::filesystem::rename(tempFilePath, targetFilePath, errorCode);
            if (errorCode)
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to commit the created temp file : %s onto "
                    "target file : %s. Error code : 0x%08x (%s).\n",
                    tempFilePath.string().c_str(), targetFilePath.string().c_str(),
                    errorCode.value(), errorCode.message().c_str());

                return false;
            }

            std::string errormsg;
            if (!securitylib::setPermissions(targetFilePath.string(), errormsg)) {
                DebugPrintf(SV_LOG_ERROR, "%s\n", errormsg.c_str());
            }

            DebugPrintf(EvCF_LOG_DEBUG, "Committed the file : %s from temp file %s.\n",
                targetFilePath.string().c_str(), tempFilePath.string().c_str());

            return true;
        }

        static void setGlobalParams(std::map<std::string, std::string> & globalParams);
    };
}

#endif // EVT_COLL_FORW_COMMON_H
