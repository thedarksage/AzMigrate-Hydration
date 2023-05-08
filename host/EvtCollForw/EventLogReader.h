#ifndef EVENT_LOG_READER_H
#define EVENT_LOG_READER_H

#include <boost/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/static_assert.hpp>
#include <cassert>
#include <string>
#include <Windows.h>
#include <winevt.h>

#ifndef SV_WINDOWS
BOOST_STATIC_ASSERT_MSG(false, "Event log collection is unsupported for non-windows OS.")
#endif SV_WINDOWS

void InitializeEventLogPlaceholderInsertionValues();

namespace EvtCollForw
{
    class EventLogObject : private boost::noncopyable
    {
    public:
        EventLogObject(EVT_HANDLE eventHandle, char* objectType);
        virtual ~EventLogObject();

        static bool CloseEventHandle(EVT_HANDLE eventHandle, const char* objectType);

    protected:
        EVT_HANDLE m_eventHandle;

    private:
        char *m_objectType;
    };

    class EventLogBookmark;
    class EventLogRecord;
    class EventLogQuery;
    class EventLogPublisherHandleStore;

    class EventLogReader
    {
    public:
        EventLogReader();
        virtual ~EventLogReader();

        bool Load(
            const std::string& queryName, const std::wstring& queryStr,
            boost::shared_ptr<EventLogBookmark> bookmark, bool tolerateQueryErrors);

        boost::shared_ptr<EventLogRecord> GetNextEventLog();

        DWORD GetErrorCode() const;

    private:
        DWORD m_currCacheIndex;
        DWORD m_currCacheBoundary;
        DWORD m_errorCode;
        boost::shared_ptr<EventLogQuery> m_currQuery;

        static const size_t HandleCacheSize = 25;
        EVT_HANDLE m_handleCache[HandleCacheSize];

        void Cleanup();
    };

    class EventLogBookmark : protected EventLogObject
    {
    public:
        virtual ~EventLogBookmark() {}

        bool UpdateBookmark(boost::shared_ptr<EventLogRecord> eventLogRecord);
        bool PersistBookmark();

        static boost::shared_ptr<EventLogBookmark>
            LoadBookmarkFromFile(const std::string &path, bool newIfFileNotPresent);

        static std::string GetBookmarkFilePath(const std::string &queryName);

    private:
        std::string m_filePath;
        bool m_isUpdated, m_isNew;

        EventLogBookmark(EVT_HANDLE bookmarkHandle, const std::string &filePath, bool isNew);

        friend bool EventLogReader::Load(
            const std::string& queryName, const std::wstring& queryStr,
            boost::shared_ptr<EventLogBookmark> bookmark, bool tolerateQueryErrors);
    };

    class EventLogQuery : protected EventLogObject
    {
    public:
        virtual ~EventLogQuery() {}

        static boost::shared_ptr<EventLogQuery> CreateQuery(
            std::string queryName, std::wstring queryStr, bool tolerateQueryErrors);

    private:
        std::string m_queryName;

        EventLogQuery(EVT_HANDLE queryResultHandle, const std::string &queryName);
        bool GetEvents(EVT_HANDLE events[], DWORD eventsRequested, DWORD &eventsReturned);

        friend bool EventLogReader::Load(
            const std::string& queryName, const std::wstring& queryStr,
            boost::shared_ptr<EventLogBookmark> bookmark, bool tolerateQueryErrors);

        friend boost::shared_ptr<EventLogRecord> EventLogReader::GetNextEventLog();
    };

    class EventLogRecord : protected EventLogObject
    {
    public:
        virtual ~EventLogRecord() {}

        bool RenderXml(std::wstring& xml);
        bool FormatEventMessage(EVT_HANDLE publisherMetadataHandle, const std::wstring& publisherName, bool includeUserProperties, std::wstring &message);
        bool FormatLevelName(EVT_HANDLE publisherMetadataHandle, const std::wstring& publisherName, std::wstring &level);
        std::string GetHashValue();

    private:
        boost::shared_ptr<EventLogQuery> m_query;
        std::string m_hashValue;

        EventLogRecord(EVT_HANDLE eventHandle, boost::shared_ptr<EventLogQuery> query);

        friend bool EventLogBookmark::UpdateBookmark(boost::shared_ptr<EventLogRecord> eventLogRecord);
        friend boost::shared_ptr<EventLogRecord> EventLogReader::GetNextEventLog();
    };

    class EventLogRecordParser : private boost::noncopyable
    {
    public:
        EventLogRecordParser(boost::shared_ptr<EventLogRecord> evtRecord, EventLogPublisherHandleStore *pubHandleStore);
        virtual ~EventLogRecordParser() {}

        bool GetProviderName(std::string &providerName);
        bool GetLevelName(std::string &levelName);
        bool GetEvtMessage(bool includePropsInMsg, std::string &message);
        bool GetEventId(std::string &eventId);
        bool GetEventIdQualifiers(std::string &qualifiers);
        bool GetVersion(std::string &version);
        bool GetChannelName(std::string &channelName);
        bool GetSystemTime(std::string &systemTime);
        bool GetProcessId(std::string &processId);
        bool GetThreadId(std::string &threadId);
        bool GetEventRecordId(std::string &eventRecordId);
        void GetProperties(std::map<std::string, std::string> &properties);

    private:
        typedef boost::property_tree::basic_ptree<std::wstring, std::wstring> wxml_tree;

        const std::wstring m_emptyWstr;
        const std::string m_emptyStr;

        wxml_tree m_tree;
        boost::shared_ptr<EventLogRecord> m_evtRecord;
        EVT_HANDLE m_publisherMetadataHandle;
        std::wstring m_providerName;
    };

    class EventLogPublisherHandleStore : private boost::noncopyable
    {
    public:
        virtual ~EventLogPublisherHandleStore();

        EVT_HANDLE Get(const std::wstring &publisherName);

    private:
        std::map<std::wstring, EVT_HANDLE> m_map;
    };
}

#endif EVENT_LOG_READER_H