#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <utility>

#include "EvtCollForwCommon.h"
#include "EventLogReader.h"
#include "logger.h"
#include "portable.h"
#include "inmageex.h"
#include "portablehelpers.h"

#define EN_US_LOCALE    MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT)

const DWORD g_placeholderInsertionValuesCnt = 99; // Hardcoded event log limit
// All elements are set to EvtVarTypeNull(0), as global values are zero-initialized
EVT_VARIANT g_placeholderInsertionValues[g_placeholderInsertionValuesCnt];

// TODO-SanKumar-1805: The idea here is to insert corresponding placeholder, as
// it would be seen in the message strings, say "User %1 initiated shutdown".
// But this isn't working as expected, one of the following two cases should be
// the culprit:
// 1. We are always passing max args possible, instead of matching the exact
// number of arguments.
// 2. We are not force-sending all the args as strings, while the event could be
// defined with various types.
// Until this issue is debugged, we are simply inserting a blank space in the
// place of each property by passing max possible 99-args as NULL.

// wchar_t g_placeholderStrings[4 * g_placeholderInsertionValuesCnt]; // Max 4-chars per insertion value
//
//void InitializeEventLogPlaceholderInsertionValues()
//{
//    wchar_t *currStringPtr = g_placeholderStrings;
//
//    for (DWORD ind = 1; ind <= g_placeholderInsertionValuesCnt; ind++)
//    {
//        BOOST_STATIC_ASSERT(g_placeholderInsertionValuesCnt < 100);
//
//        g_placeholderInsertionValues[ind - 1].StringVal = currStringPtr;
//        g_placeholderInsertionValues[ind - 1].Type = EvtVarTypeString;
//
//        *currStringPtr = L'%';
//        ++currStringPtr;
//
//        _itow(ind, currStringPtr, 10);
//        currStringPtr += ind < 10 ? 2 : 3; // digits + NULL
//    }
//}

namespace EvtCollForw
{
    EventLogObject::EventLogObject(EVT_HANDLE eventHandle, char* objectType)
        : m_eventHandle(eventHandle), m_objectType(objectType ? objectType : "")
    {
        BOOST_ASSERT(NULL != m_eventHandle);
    }

    bool EventLogObject::CloseEventHandle(EVT_HANDLE eventHandle, const char* objectType)
    {
        BOOST_ASSERT(NULL != eventHandle);

        bool isCloseSuccess = EvtClose(eventHandle);

        if (!isCloseSuccess)
        {
            DebugPrintf(EvCF_LOG_ERROR, "Failed to close the event handle : %d for %s. Error : 0x%08x.\n",
                eventHandle, objectType, GetLastError());
        }

        return isCloseSuccess;
    }

    EventLogObject::~EventLogObject()
    {
        CloseEventHandle(m_eventHandle, m_objectType);
    }

    EventLogBookmark::EventLogBookmark(EVT_HANDLE bookmarkHandle, const std::string &filePath, bool isNew)
        : EventLogObject(bookmarkHandle, "EventLogBookmark"),
        m_filePath(filePath), m_isUpdated(false), m_isNew(isNew)
    {
    }

    std::string EventLogBookmark::GetBookmarkFilePath(const std::string &queryName)
    {
        std::string bookmarkFilePath = (Utils::GetBookmarkFolderPath() / "evtlog" / queryName).string();
        return bookmarkFilePath + ".xml";
    }

    // TODO-SanKumar-1612: Should we check for the size of the file as a safeguard
    // against malicious intent? Simply have a max limit on bookmark file size.
    boost::shared_ptr<EventLogBookmark> EventLogBookmark::LoadBookmarkFromFile(
        const std::string &path, bool newIfFileNotPresent)
    {
        BOOST_ASSERT(!path.empty());

        boost::shared_ptr<EventLogBookmark> bookmark;
        EVT_HANDLE bookmarkHandle = NULL;

        boost::system::error_code errorCode;
        std::wstring fileContent;
        bool isNew = false;

        if (!boost::filesystem::exists(path, errorCode))
        {
            DebugPrintf(newIfFileNotPresent ? EvCF_LOG_INFO : EvCF_LOG_ERROR,
                "Unable to find the given bookmark file path %s in the filesystem. Error : %d (%s).\n",
                path.c_str(), errorCode.value(), errorCode.message().c_str());

            if (!newIfFileNotPresent)
                goto Cleanup;

            isNew = true;
        }
        else
        {
            if (!Utils::ReadFileContents(path, fileContent))
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to read contents of the bookmark file : %s.\n", path.c_str());

                // Try cleaning up the file, so the collection recovers from the next session.
                // TODO-SanKumar-1703: Workaround. We should figure out why the read fails for the file.
                Utils::TryDeleteFile(path);
                goto Cleanup;
            }
        }

        bookmarkHandle = EvtCreateBookmark(fileContent.empty() ? NULL : fileContent.c_str());
        if (NULL == bookmarkHandle)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "Failed to create bookmark from file : %S. Error : 0x%08x.\n", fileContent.c_str(), GetLastError());

            // Try cleaning up the file, so the collection recovers from the next session.
            // TODO-SanKumar-1703: Workaround. We should figure out why the bookmark contains the
            // invalid data. Should we dump the file content in the log?
            Utils::TryDeleteFile(path);
            goto Cleanup;
        }

        bookmark.reset(new (std::nothrow) EventLogBookmark(bookmarkHandle, path, isNew));
        if (NULL == bookmark)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "Failed to allocate memory for EventLogBookmark. Path : %s. Handle : 0x%08x.\n", path.c_str(), bookmarkHandle);

            goto Cleanup;
        }

    Cleanup:
        if (NULL == bookmark && NULL != bookmarkHandle)
        {
            EventLogObject::CloseEventHandle(bookmarkHandle, "RawBookmarkHandle");
            throw std::bad_alloc();
        }

        return bookmark;
    }

    bool EventLogBookmark::UpdateBookmark(boost::shared_ptr<EventLogRecord> eventLogRecord)
    {
        BOOST_ASSERT(NULL != eventLogRecord);

        if (NULL == eventLogRecord)
            return false;

        bool isSuccess = EvtUpdateBookmark(m_eventHandle, eventLogRecord->m_eventHandle);

        if (!isSuccess)
        {
            DebugPrintf(EvCF_LOG_ERROR, "Failed to update the bookmark : %s with event : 0x%08x. Error : 0x%08x.\n",
                m_filePath.c_str(), eventLogRecord->m_eventHandle, GetLastError());
        }

        return isSuccess;
    }

    static bool RenderEvent(EVT_HANDLE eventHandle, EVT_RENDER_FLAGS flag, std::wstring &renderedStr)
    {
        bool isSuccess;
        DWORD bufferSize = 0, bufferUsed, propertyCount;
        wchar_t *buffer = NULL;

        isSuccess = EvtRender(
            NULL, eventHandle, flag, bufferSize, buffer, &bufferUsed, &propertyCount);

        if (isSuccess || ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            BOOST_ASSERT(!isSuccess);

            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Error in getting the required buffer size for rendering event : 0x%08x. Flag : %d. Error code : 0x%08x.\n",
                FUNCTION_NAME, eventHandle, flag, GetLastError());

            isSuccess = false;
            goto Cleanup;
        }

        bufferSize = bufferUsed;
        buffer = new (std::nothrow) wchar_t[bufferSize];
        if (NULL == buffer)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Failed to allocate memory for rendering event : 0x%08x. Flag : %d.\n",
                FUNCTION_NAME, eventHandle, flag);

            throw std::bad_alloc();
        }

        isSuccess = EvtRender(
            NULL, eventHandle, flag, bufferSize, buffer, &bufferUsed, &propertyCount);

        if (!isSuccess)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Failed to render event : 0x%08x. Flag : %d. Error code : 0x%08x.\n",
                FUNCTION_NAME, eventHandle, flag, GetLastError());

            goto Cleanup;
        }

        try
        {
            // TODO-SanKumar-1612: Avoid this additional allocation and copy of memory by implementing
            // custom stream buf.
            renderedStr.reserve(bufferUsed);
            renderedStr.assign(buffer);
        }
        catch (std::bad_alloc)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Failed to allocate memory in string object, while rendering event : 0x%08x. Flag : %d.\n",
                FUNCTION_NAME, eventHandle, flag);

            delete[] buffer;
            throw;
        }

    Cleanup:
        if (NULL != buffer)
            delete[] buffer;

        return isSuccess;
    }

    static bool FormatEvent(
        EVT_HANDLE publisherMetadataHandle,
        const std::wstring &publisherName,
        EVT_HANDLE eventHandle,
        EVT_FORMAT_MESSAGE_FLAGS flags,
        bool includeUserProperties,
        std::wstring& formattedStr
    )
    {
        bool isSuccess;
        wchar_t* buffer = NULL;
        DWORD bufferSize = 0, bufferUsed;
        bool hasAllocFailed = false;
        PEVT_VARIANT values = (flags != EvtFormatMessageEvent || includeUserProperties) ? NULL : g_placeholderInsertionValues;
        DWORD valuesCount = (flags != EvtFormatMessageEvent || includeUserProperties) ? 0 : g_placeholderInsertionValuesCnt;

        isSuccess = EvtFormatMessage(
            publisherMetadataHandle, eventHandle, 0, valuesCount, values, flags, bufferSize, buffer, &bufferUsed);

        if (isSuccess || ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            BOOST_ASSERT(!isSuccess);

            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Error in getting the required buffer size for formatting event : 0x%08x of publisher : %S with handle : 0x%08x. Flags : %d. Error code : 0x%08x.\n",
                FUNCTION_NAME, eventHandle, publisherName.c_str(), publisherMetadataHandle, flags, GetLastError());

            isSuccess = false;
            goto Cleanup;
        }

        bufferSize = bufferUsed;
        buffer = new (std::nothrow) wchar_t[bufferSize];
        if (NULL == buffer)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Failed to allocate memory of size : %llu for formatting event : 0x%08x of publisher : %S with handle : 0x%08x. Flags : %d.\n",
                FUNCTION_NAME, bufferSize, eventHandle, publisherName.c_str(), publisherMetadataHandle, flags);

            isSuccess = false;
            hasAllocFailed = true;
            goto Cleanup;
        }

        isSuccess = EvtFormatMessage(
            publisherMetadataHandle, eventHandle, 0, valuesCount, values, flags, bufferSize, buffer, &bufferUsed);

        if (!isSuccess)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Failed to format event : 0x%08x of publisher : %S with handle : 0x%08x. Flags : %d. Error code : 0x%08x.\n",
                FUNCTION_NAME, eventHandle, publisherName.c_str(), publisherMetadataHandle, flags, GetLastError());

            goto Cleanup;
        }

        try
        {
            // TODO-SanKumar-1612: Avoid this additional allocation and copy of memory by implementing
            // custom stream buf.
            formattedStr.reserve(bufferUsed);
            formattedStr.assign(buffer);
        }
        catch (std::bad_alloc)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - Failed to allocate memory in string object, while formatting event : 0x%08x of publisher : %S with handle : 0x%08x. Flags : %d.\n",
                FUNCTION_NAME, eventHandle, publisherName.c_str(), publisherMetadataHandle, flags);

            isSuccess = false;
            hasAllocFailed = true;
            goto Cleanup;
        }

    Cleanup:
        if (NULL != buffer)
            delete[] buffer;

        if (hasAllocFailed)
            throw std::bad_alloc();

        return isSuccess;
    }

    bool EventLogBookmark::PersistBookmark()
    {
        bool isSuccess;
        std::wstring renderedStr;

        isSuccess = RenderEvent(m_eventHandle, EvtRenderBookmark, renderedStr);
        if (!isSuccess)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "Failed to persist bookmark into %s failed, as rendering of event : 0x%08x wasn't successful.\n",
                m_filePath.c_str(), m_eventHandle);

            return false;
        }

        if (!Utils::CommitState(m_filePath, renderedStr))
        {
            DebugPrintf(EvCF_LOG_ERROR, "Failed to commit bookmark into %s.\n", m_filePath.c_str());

            return false;
        }

        return true;
    }

    EventLogQuery::EventLogQuery(EVT_HANDLE queryResultHandle, const std::string &queryName)
        : EventLogObject(queryResultHandle, "EventLogQueryResult"), m_queryName(queryName)
    {
    }

    boost::shared_ptr<EventLogQuery> EventLogQuery::CreateQuery(
        std::string queryName, std::wstring queryStr, bool tolerateQueryErrors)
    {
        boost::shared_ptr<EventLogQuery> query;
        EVT_HANDLE queryResultHandle;

        // NOTE: From MSDN, You must only use the query handle that this function returns on the same thread that created the handle.
        queryResultHandle = EvtQuery(NULL, NULL, queryStr.c_str(),
            EvtQueryChannelPath | EvtQueryForwardDirection | (tolerateQueryErrors ? EvtQueryTolerateQueryErrors : 0));

        // TODO-SanKumar-1612: Since we use the structured XML query, if there are errors with one or two
        // channels, the above call would still succeed. To get these sub errors, use EvtGetQueryInfo().
        // Log those errors for debugging. This is not critical, since the enumeration would go through for
        // the succeeded subqueries of channels.

        if (NULL == queryResultHandle)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "Failed to query eventlog with the query : %s. Tolerate query errors : %s. Error : 0x%08x.\n%S\n",
                queryName.c_str(), tolerateQueryErrors ? "true" : "false", GetLastError(), queryStr.c_str());

            goto Cleanup;
        }

        query.reset(new (std::nothrow) EventLogQuery(queryResultHandle, queryName));
        if (NULL == query)
        {
            DebugPrintf(EvCF_LOG_ERROR, "Failed to allocate memory for EventLogQuery : %s.\n", queryName.c_str());
            goto Cleanup;
        }

    Cleanup:
        if (NULL != queryResultHandle && NULL == query)
        {
            EventLogObject::CloseEventHandle(queryResultHandle, "RawQueryHandle");
            throw std::bad_alloc();
        }

        return query;
    }

    bool EventLogQuery::GetEvents(EVT_HANDLE events[], DWORD eventsRequested, DWORD &eventsReturned)
    {
        bool isSuccess;
        // TODO-SanKumar-1612: Read this value from config.
        const DWORD timeoutMs = 60 * 1000;

        eventsReturned = 0;
        isSuccess = EvtNext(m_eventHandle, eventsRequested, events, timeoutMs, 0, &eventsReturned);

        if (!isSuccess)
        {
            if (ERROR_NO_MORE_ITEMS == GetLastError())
            {
                eventsReturned = 0;
                isSuccess = true;
            }
            else
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to enumerate the events for query : %s. Error code : 0x%08x.\n",
                    m_queryName.c_str(), GetLastError());
            }
        }

        return isSuccess;
    }

    EventLogRecord::EventLogRecord(EVT_HANDLE eventHandle, boost::shared_ptr<EventLogQuery> query)
        : EventLogObject(eventHandle, "EventLogRecord"), m_query(query), m_hashValue(GenerateUuid())
    {
        BOOST_ASSERT(NULL != m_query);
    }

    bool EventLogRecord::RenderXml(std::wstring &xml)
    {
        // TODO-SanKumar-1612: Currently we get the information from this Xml and after retrieving the
        // Provider name from this Xml, we go onto query FormatEvent(LevelName), FormatEvent(Message).
        // For these two data, we need the provider name and so we end up invoking 3 calls x 2 (memory
        // needed calls). Instead, we could call RenderEvent(ProviderName) and then FormatEvent(Xml),
        // where this Xml also contains the formatted message and formatted level name unlike RenderXml.
        // Total calls are 2 x 2 but more memory would be used for the second call.
        // Also, have to validate if RenderXml gives the data in en-us always or in current systm language.
        // If it's returning the string in local language, then we should move ahead to get all the data
        // using FormatEvent(Xml) only, as our devs wouldn't be able to recognize language apart from en-us.
        return RenderEvent(m_eventHandle, EvtRenderEventXml, xml);
    }

    bool EventLogRecord::FormatEventMessage(EVT_HANDLE publisherMetadataHandle, const std::wstring& publisherName, bool includeUserProperties, std::wstring &message)
    {
        return FormatEvent(
            publisherMetadataHandle, publisherName, m_eventHandle,
            EvtFormatMessageEvent, includeUserProperties, message);
    }

    bool EventLogRecord::FormatLevelName(EVT_HANDLE publisherMetadataHandle, const std::wstring& publisherName, std::wstring &level)
    {
        return FormatEvent(
            publisherMetadataHandle, publisherName, m_eventHandle,
            EvtFormatMessageLevel, false, level);
    }

    std::string EventLogRecord::GetHashValue()
    {
        return m_hashValue;
    }

    EventLogReader::EventLogReader()
        : m_errorCode(0), m_currCacheIndex(0), m_currCacheBoundary(0), m_currQuery()
    {
    }

    void EventLogReader::Cleanup()
    {
        for (; m_currCacheIndex < m_currCacheBoundary; m_currCacheIndex++)
            EventLogObject::CloseEventHandle(m_handleCache[m_currCacheIndex], "RawEventLogRecordHandle");

        m_errorCode = ERROR_SUCCESS;
    }

    inline DWORD EventLogReader::GetErrorCode() const
    {
        return m_errorCode;
    }

    EventLogReader::~EventLogReader()
    {
        Cleanup();
        BOOST_ASSERT(m_currCacheIndex == m_currCacheBoundary);
    }

    bool EventLogReader::Load(const std::string& queryName, const std::wstring& queryStr, boost::shared_ptr<EventLogBookmark> bookmark, bool tolerateQueryErrors)
    {
        // Clear up any active event handles that were queried but wasn't used.
        Cleanup();
        BOOST_ASSERT(m_currCacheIndex == m_currCacheBoundary);

        m_currQuery = EventLogQuery::CreateQuery(queryName, queryStr, tolerateQueryErrors);
        if (!m_currQuery)
            return false;

        // Seek to the event next to the current bookmark.
        // TODO-SanKumar-1612: Verify if we should do this or do first a search for bookmark itself with
        // strict and relax it in the second seek?
        //  | EvtSeekStrict))
        if (!EvtSeek(m_currQuery->m_eventHandle, 1, bookmark->m_eventHandle, 0, EvtSeekRelativeToBookmark))
        {
            // Probably no new event has occured from the last enumeration.
            DebugPrintf(EvCF_LOG_DEBUG,
                "Failed to seek the resulting events from query : %s to the bookmark : %s. Error code : 0x%08x.\n",
                queryName.c_str(), bookmark->m_filePath.c_str(), GetLastError());

            return false;
        }

        return true;
    }

    boost::shared_ptr<EventLogRecord> EventLogReader::GetNextEventLog()
    {
        if (m_currCacheIndex >= m_currCacheBoundary)
        {
            BOOST_ASSERT(m_currCacheIndex == m_currCacheBoundary);

            if (!m_currQuery->GetEvents(m_handleCache, ARRAYSIZE(m_handleCache), m_currCacheBoundary))
            {
                // Faced error in enumeration.
                m_errorCode = GetLastError();
                return NULL;
            }

            // Got 0 or more elements, so reset the index to beginning.
            m_currCacheIndex = 0;

            // Enumeration is complete.
            if (0 == m_currCacheBoundary)
                return NULL;

            // There are further events to enumerate.
        }

        boost::shared_ptr<EventLogRecord> nextRecord(
            new (std::nothrow) EventLogRecord(m_handleCache[m_currCacheIndex], m_currQuery));
        if (NULL == nextRecord)
        {
            DebugPrintf(EvCF_LOG_ERROR, "Out of memory to allocate EventLogRecord object.\n");
            throw std::bad_alloc();
        }

        m_currCacheIndex++;
        return nextRecord;
    }

    EventLogRecordParser::EventLogRecordParser(
        boost::shared_ptr<EventLogRecord> evtRecord,
        EventLogPublisherHandleStore *pubHandleStore
    )
        : m_evtRecord(evtRecord)
    {
        BOOST_ASSERT(evtRecord != NULL);

        std::wstring eventLogXml;
        // TODO-SanKumar-1702: For getting the Xml, should we use FormatMessage? (since the formatting
        // could be directed to en-us, whereas RenderXml() doesn't take any locale).
        if (!evtRecord->RenderXml(eventLogXml))
        {
            const char *errMsg = "Failed to render the xml of the event";
            DebugPrintf(EvCF_LOG_ERROR, "%s.\n", errMsg);
            throw INMAGE_EX(errMsg);
        }

        // TODO-SanKumar-1702: Avoid this string copy!
        boost::property_tree::read_xml(std::wistringstream(eventLogXml), m_tree);

        m_providerName = m_tree.get(L"Event.System.Provider.<xmlattr>.Name", L"");

        m_publisherMetadataHandle = (pubHandleStore == NULL) ? NULL : pubHandleStore->Get(m_providerName);
    }

    bool EventLogRecordParser::GetProviderName(std::string &providerName)
    {
        return Utils::TryConvertWstringToUtf8(m_providerName, providerName) && !providerName.empty();
    }

    bool EventLogRecordParser::GetLevelName(std::string &levelName)
    {
        std::wstring wLevelName;

        if (!m_evtRecord->FormatLevelName(m_publisherMetadataHandle, m_providerName, wLevelName))
            return false;

        return Utils::TryConvertWstringToUtf8(wLevelName, levelName) && !levelName.empty();
    }

    bool EventLogRecordParser::GetEvtMessage(bool includePropsInMsg, std::string &message)
    {
        std::wstring wMessage;

        // If includeProperties is true, the placeholders in the event message (similar to format string)
        // will be replaced by the parameters. Otherwise, the event message defined in the message compiler/
        // event manifest file would be returned as is, without the actual user data of this event record.
        if (!m_evtRecord->FormatEventMessage(m_publisherMetadataHandle, m_providerName, includePropsInMsg, wMessage))
            return false;

        return Utils::TryConvertWstringToUtf8(wMessage, message) && !message.empty();
    }

    bool EventLogRecordParser::GetEventId(std::string &eventId)
    {
        return Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.EventID", m_emptyWstr), eventId) && !eventId.empty();
    }

    bool EventLogRecordParser::GetEventIdQualifiers(std::string &qualifiers)
    {
        return
            Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.EventID.<xmlattr>.Qualifiers", m_emptyWstr), qualifiers)
            && !qualifiers.empty();
    }

    bool EventLogRecordParser::GetVersion(std::string &version)
    {
        return Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.Version", m_emptyWstr), version) && !version.empty();
    }

    bool EventLogRecordParser::GetChannelName(std::string &channelName)
    {
        return Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.Channel", m_emptyWstr), channelName) && !channelName.empty();
    }

    bool EventLogRecordParser::GetSystemTime(std::string &systemTime)
    {
        // TODO-SanKumar-1702: Although this attribute is mostly present, the XML could only have RawTime,
        // when high precision time collection is enabled. In that case, according to Event log XSD,
        // SystemTime may or may not be available.
        return
            Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.TimeCreated.<xmlattr>.SystemTime", m_emptyWstr), systemTime)
            && !systemTime.empty();
    }

    bool EventLogRecordParser::GetProcessId(std::string &processId)
    {
        return
            Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.Execution.<xmlattr>.ProcessID", m_emptyWstr), processId)
            && !processId.empty();
    }

    bool EventLogRecordParser::GetThreadId(std::string &threadId)
    {
        return
            Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.Execution.<xmlattr>.ThreadID", m_emptyWstr), threadId)
            && !threadId.empty();
    }

    bool EventLogRecordParser::GetEventRecordId(std::string &eventRecordId)
    {
        return
            Utils::TryConvertWstringToUtf8(m_tree.get(L"Event.System.EventRecordID", m_emptyWstr), eventRecordId)
            && !eventRecordId.empty();
    }

    void EventLogRecordParser::GetProperties(std::map<std::string, std::string> &properties)
    {
        // TODO-SanKumar-1702: Add support for UserData, BinaryData and DebugData, if needed.
        boost::optional<wxml_tree&> eventDataProps = m_tree.get_child_optional(L"Event.EventData");

        if (eventDataProps)
        {
            int propertyCnt = 1;
            for (wxml_tree::iterator currProp = eventDataProps->begin(); currProp != eventDataProps->end(); currProp++)
            {
                // TODO-SanKumar-1702: Currently not parsing <ComplexData> and <Binary>
                // We will only parse <Data>.

                if (0 == currProp->first.compare(L"Data"))
                {
                    std::string propName, propValue;
                    Utils::TryConvertWstringToUtf8(currProp->second.get_value(m_emptyWstr), propValue);

                    boost::optional<wxml_tree&> nameAttr = currProp->second.get_child_optional(L"<xmlattr>.Name");
                    if (!nameAttr)
                    {
                        // For legacy events, property name will not be present. So, generate a key for this instance.
                        propName = "Property" + boost::lexical_cast<std::string>(propertyCnt);
                    }
                    else
                    {
                        Utils::TryConvertWstringToUtf8(nameAttr->get_value(m_emptyWstr), propName);

                        if (propName.empty())
                        {
                            // If the property name is empty, we should generate it, since JSON parsing
                            // fails, if the key value is empty.
                            propName = "Property" + boost::lexical_cast<std::string>(propertyCnt);
                        }
                    }

                    // TODO-SanKumar-1702: Care should be taken that the legacy channels mustn't be
                    // doing collating of multiple events, since PropertyN name would keep repeating
                    // and by the design of insert(), we would only get the first of the duplicates.
                    BOOST_VERIFY(properties.insert(std::make_pair(propName, propValue)).second);
                    propertyCnt++;
                }
            }
        }
    }

    EventLogPublisherHandleStore::~EventLogPublisherHandleStore()
    {
        // Close all the handles stored in the map
        for (auto &kvPair : m_map)
            if (kvPair.second != NULL)
                EventLogObject::CloseEventHandle(kvPair.second, "RawPublisherHandle");
    }

    // Don't close the handle returned by this method.
    EVT_HANDLE EventLogPublisherHandleStore::Get(const std::wstring &publisherName)
    {
        auto pubHandleItr = m_map.find(publisherName);
        if (pubHandleItr != m_map.end())
            return pubHandleItr->second; // Return, if handle is found in the map

        // Otherwise, open the metadata handle
        EVT_HANDLE publisherMetadataHandle = EvtOpenPublisherMetadata(
            NULL, publisherName.c_str(), NULL, EN_US_LOCALE, 0);

        if (publisherMetadataHandle == NULL)
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "%s - EvtOpenPublisherMetadata failed for publisher : %S. Error code : 0x%08x.\n",
                FUNCTION_NAME, publisherName.c_str(), GetLastError());

            // Even if the publisher fails to be opened, store NULL handle under
            // the publisher's name, so that the open is not attempted for every
            // event log record of this failing publisher.
        }

        try
        {
            m_map.insert(std::make_pair(publisherName, publisherMetadataHandle));
        }
        catch (std::bad_alloc)
        {
            EventLogObject::CloseEventHandle(publisherMetadataHandle, "RawPublisherHandle");
            publisherMetadataHandle = NULL;

            throw; // Rethrow the memory allocation failure
        }

        return publisherMetadataHandle;
    }
}