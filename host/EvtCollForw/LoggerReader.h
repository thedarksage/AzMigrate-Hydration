#ifndef LOGGER_READER_H
#define LOGGER_READER_H

#include <ace/OS_NS_Thread.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/regex.hpp>
#include <string>
#include "json_adapter.h"

#ifdef SV_UNIX
#include <boost/thread/mutex.hpp>
#include <deque>
#endif // SV_UNIX

#include "logger.h"
#include "portable.h"
#include "inmageex.h"
#include "EvtCollForwCommon.h"

namespace EvtCollForw
{
    struct LoggerReaderBookmark
    {
        LoggerReaderBookmark() : SequenceNumber(0), Offset(0), Completed(false) { }

        // Keeping this POD type to simplify copying.
        uint64_t SequenceNumber;
        uint64_t Offset;
        bool Completed;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "LoggerReaderBookmark", false);

            //JSON_E(adapter, FilePath);
            JSON_E(adapter, SequenceNumber);
            JSON_E(adapter, Offset);
            JSON_T(adapter, Completed);
        }

        bool operator==(const LoggerReaderBookmark& rhs)
        {
            return this->Completed == rhs.Completed
                && this->Offset == rhs.Offset
                && this->SequenceNumber == rhs.SequenceNumber;
        }

        bool operator!=(const LoggerReaderBookmark& rhs)
        {
            return !(*this == rhs);
        }

        bool operator!()
        {
            return this->Completed == false
                && this->Offset == 0
                && this->SequenceNumber == 0;
        }

        virtual std::string GetBookmarkFilePath(const std::string &logFilePath) const
        {
            boost::filesystem::path logFilePathObj(logFilePath);

            std::string bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "loggerreader" / logFilePathObj.stem()).string();
            return bookmarkFilePath + ".json";
        }
    };

    typedef boost::shared_ptr<LoggerReaderBookmark> PLoggerReaderBookmark;

    struct IRSrcLoggerReaderBookmark : LoggerReaderBookmark
    {
        std::string GetBookmarkFilePath(const std::string &logFilePath) const
        {
            //
            //  <Agent log path>\vxlogs\completed\<device name>\<volume group ID>\<EndPointDeviceName>\dataprotection.log
            //                                     0           00                00                   11
            //                                     1           23                45                   56
            //
            // *EndPointDeviceName = "C\Source Host ID\<device name>"
            //
            // Windows:
            //   MBR disk: "<Agent log path>\vxlogs\completed\{1619707344}\52\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection.log"
            //   GPT disk: "<Agent log path>\vxlogs\completed\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\52\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log"
            // Linux:
            //             "<Agent log path>/vxlogs/completed/dev/sda/44/C:\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda/dataprotection.log"
            //

#define SLASH "(\\\\|/)+" // TODO: nichougu - Either use distinct names for each macro in every function or use const char */string.

#define HOST_ID_LOWER   "([a-f]|[0-9]){8}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){12}"
//                       1                2                3                4                5

#define IR_SRC_LOG_FORMAT "(^.+)" SLASH "(\\d+)" SLASH "(c:?)" SLASH "(" HOST_ID_LOWER ")" SLASH "(.+)" SLASH "(dataprotection.log)$"
//                         0      0      0       0      0      0      0                    1      1     1      1
//                         1      2      3       4      5      6      7  +5                3      4     5      6
            BOOST_ASSERT(!logFilePath.empty());
            std::string bookmarkFilePath;
            try
            {
                std::string resyncLogDir(Utils::GetDPCompletedIRLogFolderPath());
                std::string resyncMetaData = logFilePath.substr(resyncLogDir.length(), logFilePath.length() - resyncLogDir.length());

                boost::regex resyncSrcFilesRegex(IR_SRC_LOG_FORMAT);
                boost::algorithm::to_lower(resyncMetaData);
                boost::smatch matches;
                if (!boost::regex_search(resyncMetaData, matches, resyncSrcFilesRegex))
                {
                    std::stringstream sserr;
                    sserr << FUNCTION_NAME << " failed to generate bookmark file path - boost::regex_search match failed for log path " << logFilePath << ".";
                    DebugPrintf(EvCF_LOG_ERROR, "%s.\n", sserr.str().c_str());
                    throw INMAGE_EX(sserr.str().c_str());
                }
                std::string DeviceName = matches[1].str();
                boost::replace_all(DeviceName, ACE_DIRECTORY_SEPARATOR_STR_A, "_");
                std::string VolumeGroupID = matches[3].str();
                std::string bookmarkFileName(DeviceName + '_' + VolumeGroupID + "_dataprotection.json");
                bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "loggerreader" / bookmarkFileName).string();
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_ACTION("Generate bookmark file path", BOOST_ASSERT(false));

            return bookmarkFilePath;
        }
    };

    struct IRRcmSrcLoggerReaderBookmark : LoggerReaderBookmark
    {
        std::string GetBookmarkFilePath(const std::string &logFilePath) const
        {
            //
            //  <Agent log path>\vxlogs\completed\<device name>\dataprotection.log
            //                                     0           00
            //                                     1           23
            // Windows:
            //   MBR disk: "<Agent log path>\vxlogs\completed\{1619707344}\dataprotection.log"
            //   GPT disk: "<Agent log path>\vxlogs\completed\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log"
            // Linux:
            //             "<Agent log path>/vxlogs/completed/dev/sda/dataprotection.log"
            //

#define SLASH "(\\\\|/)+"

#define IR_SRC_LOG_FORMAT "(^.+)" SLASH "(dataprotection.log)$"
//                         0      0      0
//                         1      2      3

            BOOST_ASSERT(!logFilePath.empty());
            std::string bookmarkFilePath;
            try
            {
                std::string resyncLogDir(Utils::GetDPCompletedIRLogFolderPath());
                std::string resyncMetaData = logFilePath.substr(resyncLogDir.length(), logFilePath.length() - resyncLogDir.length());

                boost::regex resyncSrcFilesRegex(IR_SRC_LOG_FORMAT);
                boost::algorithm::to_lower(resyncMetaData);
                boost::smatch matches;
                if (!boost::regex_search(resyncMetaData, matches, resyncSrcFilesRegex))
                {
                    std::stringstream sserr;
                    sserr << FUNCTION_NAME << " failed to generate bookmark file path - boost::regex_search match failed for log path " << logFilePath << ".";
                    throw INMAGE_EX(sserr.str().c_str());
                }
                std::string DeviceName = matches[1].str();
                std::string bookmarkFileName(DeviceName + '_' + "_dataprotection.json");
                bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "loggerreader" / bookmarkFileName).string();
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_ACTION("Generate bookmark file path", BOOST_ASSERT(false));

            return bookmarkFilePath;
        }
    };

    struct IRMTLoggerReaderBookmark : LoggerReaderBookmark
    {
        std::string GetBookmarkFilePath(const std::string &logFilePath) const
        {
            BOOST_ASSERT(!logFilePath.empty());
            std::string bookmarkFilePath;
            try
            {
                std::string resyncLogDirPath(Utils::GetDPCompletedIRLogFolderPath());
                std::string bookmarkFileName = logFilePath.substr(resyncLogDirPath.length(), logFilePath.length() - resyncLogDirPath.length());
                boost::replace_all(bookmarkFileName, ACE_DIRECTORY_SEPARATOR_STR_A, "_");
                bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "loggerreader" / bookmarkFileName).replace_extension(".json").string();
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_ACTION("Generate bookmark file path", BOOST_ASSERT(false));
            return bookmarkFilePath;
        }
    };

    struct IRRcmMTLoggerReaderBookmark : LoggerReaderBookmark
    {
        std::string GetBookmarkFilePath(const std::string &logFilePath) const
        {
            BOOST_ASSERT(!logFilePath.empty());
            std::string bookmarkFilePath;
            std::map<std::string, std::string> telemetryPairs;
            
            try
            {
                Utils::GetPSSettingsMap(telemetryPairs);
                std::string resyncLogDirPath;
                bool rootPathFound = false;
                for (std::map<std::string, std::string>::iterator itr = telemetryPairs.begin(); itr != telemetryPairs.end(); itr++)
                {
                    if (logFilePath.find(itr->first) != std::string::npos)
                    {
                        rootPathFound = true;
                        resyncLogDirPath = Utils::GetRcmMTDPCompletedIRLogFolderPath(itr->second);
                        break;
                    }
                }
                if (!rootPathFound)
                {
                    throw std::runtime_error(std::string("unable to find settings for the path ") + logFilePath);
                }
                std::string bookmarkFileName = logFilePath.substr(resyncLogDirPath.length(), logFilePath.length() - resyncLogDirPath.length());
                boost::replace_all(bookmarkFileName, ACE_DIRECTORY_SEPARATOR_STR_A, "_");
                bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "loggerreader" / bookmarkFileName).replace_extension(".json").string();
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_ACTION("Generate bookmark file path", BOOST_ASSERT(false));
            return bookmarkFilePath;
        }
    };

    struct DRMTLoggerReaderBookmark : LoggerReaderBookmark
    {
        std::string GetBookmarkFilePath(const std::string &logFilePath) const
        {
            BOOST_ASSERT(!logFilePath.empty());
            std::string bookmarkFilePath;
            try
            {
                std::string diffsyncLogDirPath(Utils::GetDPDiffsyncLogFolderPath());
                std::string bookmarkFileName = logFilePath.substr(diffsyncLogDirPath.length(), logFilePath.length() - diffsyncLogDirPath.length());
                boost::replace_all(bookmarkFileName, ACE_DIRECTORY_SEPARATOR_STR_A, "_");
                bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "loggerreader" / bookmarkFileName).replace_extension(".json").string();
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_ACTION("Generate bookmark file path", BOOST_ASSERT(false));
            return bookmarkFilePath;
        }
    };

    class LoggerReaderBase
    {
    public:
        LoggerReaderBase(const std::string &logPath) : m_logPath(logPath) {}

        static bool ExtractMetadata(const std::string &fullLogMessage, boost::posix_time::ptime &logTime,
            SV_LOG_LEVEL &logLevel, std::string &message, pid_t &pid, ACE_thread_t &tid, int64_t &seqNum)
        {
            // Parse timestamp and level from the standard log message of the following format.
            //0         1         2         3         4
            //01234567890123456789012345678901234567890123456789
            //#~> (12-07-2016 08:27:25):   DEBUG  32 4648 102
            try
            {
                // The locale is responsible for calling the matching delete of allocated facet from its own destructor.
                // http://en.cppreference.com/w/cpp/locale/locale/locale
                // TODO-SanKumar-1612: Make this a class member.
                std::locale loggerDateLocale(std::locale::classic(), new boost::local_time::local_time_input_facet("%m-%d-%Y %H:%M:%S"));
                std::istringstream dateStream(fullLogMessage.substr(5, 19));
                dateStream.imbue(loggerDateLocale);
                dateStream >> logTime;
                if (dateStream.fail())
                {
                    // TODO: log
                    return false;
                }

                // 1-space + Level(width=7) + 2-spaces + pid(int64 - max 22 chars) + 1-space + tid(int64 - max 22 chars) + 1-space + seqNum(int64 - max 22 chars) + 1-space + 4-extra = 83
                std::istringstream logMetaStream(fullLogMessage.substr(26, 83));
                std::string logLevelName;
                logMetaStream >> logLevelName >> pid >> tid >> seqNum;
                if (logMetaStream.fail())
                {
                    // TODO: log
                    return false;
                }

                if (!GetLogLevel(logLevelName, logLevel))
                {
                    // TODO: log
                    return false;
                }

                std::string::size_type messageStartInd = 26 + 1 + (std::string::size_type)logMetaStream.tellg();
                message.reserve(fullLogMessage.size() - messageStartInd);
                // TODO-SanKumar-1612: Instead of copying the string, shall we pass the start index in
                // the fullLogMessage?
                message = fullLogMessage.substr(messageStartInd);
            }
            catch (std::bad_alloc)
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to allocate memory, while extracting metadata from the log.\n");
                throw;
            }
            GENERIC_CATCH_LOG_ACTION("Parsing log metadata", return false;);

            return true;
        }

        std::string GetLogPath() const
        {
            return m_logPath;
        }

    protected:
        virtual bool IsBeginningOfMessage(const std::string &currLine) = 0;
        virtual bool GetNextFileForParsing(uint64_t &seqNumber, uint64_t &offset) = 0;

    protected:
        std::string m_logPath;
        std::string m_currLine;
        std::ifstream m_ifstream;
        std::string m_currCompletedFilePath;
    };

    template <class TBookmark>
    class LoggerReader : public LoggerReaderBase
    {
    public:
        LoggerReader(const std::string &logPath);

        virtual void Initialize(bool createBkmrkFileIfNotFound);
        virtual bool GetNextMessage(std::string &message, TBookmark &currBookmark);
        virtual void GetCurrBookmark(TBookmark &currBookmark) const;
        virtual bool MarkCompletionAndCleanup(TBookmark &bookmark, bool shouldCleanup);

        virtual void Cleanup(TBookmark &bookmark, bool shouldCleanup) = 0;

        virtual ~LoggerReader() {}

    protected:
        TBookmark m_bookmark;

    private:
        TBookmark m_lastCommittedBookmark;
    };

    template <class TBookmark>
    class AgentLoggerReader : public LoggerReader<TBookmark>
    {
    public:
        AgentLoggerReader(const std::string &logPath)
            : LoggerReader<TBookmark>(logPath)
        {
        }

        virtual ~AgentLoggerReader() {}

    protected:
        virtual bool IsBeginningOfMessage(const std::string &currLine);
        virtual bool GetNextFileForParsing(uint64_t &seqNumber, uint64_t &offset);
        virtual void Cleanup(TBookmark &bookmark, bool shouldCleanup);
    };

#ifdef SV_UNIX
    template <class TBookmark>
    class InVolFltLinuxTelemetryLoggerReader : public LoggerReader<TBookmark>
    {
    public:
        InVolFltLinuxTelemetryLoggerReader(const std::string &logPath)
            : LoggerReader<TBookmark>(logPath), m_isListingCompleted(false), m_tryDeletedFilesCnt(0)
        {
        }

        virtual ~InVolFltLinuxTelemetryLoggerReader() {}

    protected:
        virtual bool IsBeginningOfMessage(const std::string &currLine);
        virtual bool GetNextFileForParsing(uint64_t &seqNumber, uint64_t &offset);
        virtual void Cleanup(TBookmark &bookmark, bool shouldCleanup);

    private:
        boost::mutex m_listMutex;
        std::deque<uint64_t> m_queuedFiles, m_parsedFiles;
        uint32_t m_tryDeletedFilesCnt;
        bool m_isListingCompleted;
};
#endif // SV_UNIX
}

#include "LoggerReader.cpp"

#endif // LOGGER_READER_H