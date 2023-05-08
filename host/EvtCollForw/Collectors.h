#ifndef EVTCOLLFORW_COLLECTORS_H
#define EVTCOLLFORW_COLLECTORS_H

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#include "cxtransport.h"

#include "LoggerReader.h"

#ifdef SV_WINDOWS
#include "EventLogReader.h"
#endif // SV_WINDOWS

namespace EvtCollForw
{
    template <class TCollectedData, class TCommitData>
    class CollectorBase : boost::noncopyable
    {
    public:
        virtual ~CollectorBase() {}
        virtual bool GetNext(TCollectedData& data, TCommitData& newState) = 0;
        virtual void Commit(const TCommitData& state, bool hasMoreData) = 0;
    };

    template <class TCommitData>
    class InMageLoggerCollector : public CollectorBase<std::string, TCommitData>
    {
    public:
        InMageLoggerCollector(boost::shared_ptr< LoggerReader<TCommitData> > loggerReader) :
            m_loggerReader(loggerReader)
        {
            if (!m_loggerReader)
                throw INMAGE_EX("loggerReader can't be null.");

            m_logPath = m_loggerReader->GetLogPath();

            const bool createBookmarkFileIfNotFound = true;

            m_loggerReader->Initialize(createBookmarkFileIfNotFound);
            m_loggerReader->GetCurrBookmark(m_lastReadBookmark);
        }

        virtual ~InMageLoggerCollector() {}

        virtual bool GetNext(std::string &fullLogMessage, TCommitData& bkmrkLoc)
        {
            bool isValidData = m_loggerReader->GetNextMessage(fullLogMessage, bkmrkLoc);
            m_lastReadBookmark = bkmrkLoc;
            return isValidData;
        }

        virtual void Commit(const TCommitData &bkmrkLoc, bool hasMoreData)
        {
            // Optimizing the cleanup step that involves file system queries and operations, so that in case
            // of multiple commits, only the last commit would do the heavy cleanup.
            bool shouldCleanup = !hasMoreData;

            // Duplicate and null commits are taken care by logger reader's following method itself. So,
            // no extra handling needed.
            if (!m_loggerReader->MarkCompletionAndCleanup(const_cast<TCommitData&>(bkmrkLoc), shouldCleanup))
            {
                std::string errMsg = "Failed to MarkCompletionAndCleanup for log path : " + m_logPath;
                DebugPrintf(EvCF_LOG_ERROR, "%s - %s\n", FUNCTION_NAME, errMsg.c_str());
                throw INMAGE_EX(errMsg);
            }
        }

    private:
        std::string m_logPath;
        boost::shared_ptr< LoggerReader<TCommitData> > m_loggerReader;
        TCommitData m_lastReadBookmark;
    };

#ifdef SV_WINDOWS
    class EventLogCollector : public CollectorBase<boost::shared_ptr<EventLogRecord>, boost::shared_ptr<EventLogRecord>>
    {
    public:
        EventLogCollector(const std::string& queryName, const std::wstring& query);
        virtual ~EventLogCollector() {}

        virtual bool GetNext(boost::shared_ptr<EventLogRecord> &evtRecord, boost::shared_ptr<EventLogRecord> &newState);
        virtual void Commit(const boost::shared_ptr<EventLogRecord> &toBkmrkRecord, bool hasMoreData);

    private:
        EventLogReader m_evtLogReader;
        boost::shared_ptr<EventLogRecord> m_lastParsedEvtLogRecord, m_lastBkmrkdRecord;
        std::string m_queryName;
        std::wstring m_query;
        boost::shared_ptr<EventLogBookmark> m_bookmark;
    };

    class EventLogTelemetryCollector : public CollectorBase<std::map<std::string, std::string>, boost::shared_ptr<EventLogRecord>>
    {
    public:
        EventLogTelemetryCollector(const std::string &queryName, const std::wstring &query);
        virtual ~EventLogTelemetryCollector() {}

        virtual bool GetNext(std::map<std::string, std::string> &propMap, boost::shared_ptr<EventLogRecord> &newState);
        virtual void Commit(const boost::shared_ptr<EventLogRecord> &toBkmrkRecord, bool hasMoreData);

    private:
        boost::shared_ptr<EventLogCollector> m_internalColl;
        std::map<std::string, std::map<std::string, std::string>> m_groupMaps;
        size_t m_startedGroupsCnt, m_endedGroupsCnt;
        boost::shared_ptr<EventLogRecord> m_lastParsedState;
        boost::shared_ptr<EventLogRecord> m_remainingEvent;
        boost::shared_ptr<EventLogRecord> m_remainingEventState;
        boost::posix_time::ptime m_cutOffTime;
        bool m_isCutOffCheckCompleted;
        bool m_enumerationCompleted;

        static std::string DevContextPtr, GroupingID, GroupingEndMarker;
    };
#endif // SV_WINDOWS

    class FilePollCollector : public CollectorBase < std::vector < std::string >, std::vector<std::string> >
    {
    public:
        FilePollCollector(
            boost::shared_ptr<CxTransport> fileTransport, const boost::filesystem::path& folderPattern,
            const std::string& fileExt, boost::function1<bool, const std::string&> predicate);
        virtual ~FilePollCollector() {}

        virtual bool GetNext(std::vector<std::string>& nextFilePaths, std::vector<std::string>& filePathsToClear);
        virtual void Commit(const std::vector<std::string> &completedFiles, bool hasMoreData);

    private:
        boost::filesystem::path m_pattern;
        boost::function1<bool, const std::string&> m_predicate;
        // TODO-SanKumar-1708: Should we use CxTransport for the query and delete operations? Because,
        // this would restrict us only to PS/MT' environment.
        boost::shared_ptr<CxTransport> m_cxTransport;
        std::vector<std::string> m_nameSortedFilePaths;
        std::map<std::string, std::string> m_pathToNameMap;

        bool m_queryCompleted;
        int m_nextIndex;
        std::string m_fileExt;
    };

    typedef boost::shared_ptr<FilePollCollector> PFilePollCollector;

    class AgentSetupLogsCollector : public CollectorBase <std::string, std::string>
    {
    public:
        AgentSetupLogsCollector(boost::shared_ptr<CxTransport> fileTransport,
            const boost::filesystem::path& folderPattern,
            const std::string& fileExt);
        virtual ~AgentSetupLogsCollector()
        {}

        virtual bool GetNext(std::string& nextFilePath, std::string& filePathsToClear);
        virtual void Commit(const std::string &completedFiles, bool hasMoreData);

    private:
        bool m_queryCompleted;
        boost::filesystem::path m_pattern;
        std::string m_fileExt;
        boost::shared_ptr<CxTransport> m_cxTransport;
        std::vector<std::string> m_fileListToProcess;
    };
}

#endif // !EVTCOLLFORW_COLLECTORS_H
