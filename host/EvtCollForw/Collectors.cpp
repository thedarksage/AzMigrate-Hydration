#include "cxtransport.h"
#include "listfile.h"
#include "portablehelpers.h"

#include "Collectors.h"
#include "EvtCollForwCommon.h"

namespace EvtCollForw
{
#ifdef SV_WINDOWS
    EventLogCollector::EventLogCollector(const std::string& queryName, const std::wstring& query)
        : m_queryName(queryName), m_query(query)
    {
        std::string evtLogBookmarkFilePath = EventLogBookmark::GetBookmarkFilePath(queryName);

        const bool createNewFileIfNotPresent = true;
        m_bookmark = EventLogBookmark::LoadBookmarkFromFile(evtLogBookmarkFilePath, createNewFileIfNotPresent);
        if (!m_bookmark)
        {
            const char *errMsg = "Error in loading bookmark";
            DebugPrintf(EvCF_LOG_ERROR, "%s - %s from %s.\n", FUNCTION_NAME, errMsg, evtLogBookmarkFilePath.c_str());
            throw INMAGE_EX(errMsg);
        }

        // It's not guaranteed that all the channels that we query would be present in the user's
        // machine. So, using the best effort approach provided by event log API itself.
        // TODO-SanKumar-1702: Add this value to configuration.
        const bool tolerateQueryErrors = true;
        if (!m_evtLogReader.Load(m_queryName, m_query, m_bookmark, tolerateQueryErrors))
        {
            const char *errMsg = "Failed to load query";
            DebugPrintf(EvCF_LOG_ERROR, "%s - %s : %s with bookmark : %s.\n", FUNCTION_NAME, errMsg, queryName.c_str(), evtLogBookmarkFilePath.c_str());
            throw INMAGE_EX(errMsg);
        }
    }

    bool EventLogCollector::GetNext(boost::shared_ptr<EventLogRecord> &evtRecord, boost::shared_ptr<EventLogRecord> &newState)
    {
        evtRecord = m_evtLogReader.GetNextEventLog();

        if (!evtRecord)
        {
            // The state continues to be the last successfully parsed record.
            newState = m_lastParsedEvtLogRecord;
            return false;
        }

        newState = m_lastParsedEvtLogRecord = evtRecord;
        return true;
    }

    void EventLogCollector::Commit(const boost::shared_ptr<EventLogRecord> &toBkmrkRecord, bool hasMoreData)
    {
        BOOST_ASSERT(m_bookmark);

        if (toBkmrkRecord)
        {
            if (m_lastBkmrkdRecord &&
                0 == m_lastBkmrkdRecord->GetHashValue().compare(toBkmrkRecord->GetHashValue()))
            {
                // The bookmark has already been persisted in this current session.
                // Note that, this doesn't solve the bookmark persistance across two sessions for the
                // same bookmark, unlike InMageLoggerCollector. This collector incurs at least one write
                // to the bookmark file, irrespective of any event getting collected.
                return;
            }

            if (!m_bookmark->UpdateBookmark(toBkmrkRecord) || !m_bookmark->PersistBookmark())
            {
                const char *errMsg = "Failed to persist the event log bookmark";
                DebugPrintf(EvCF_LOG_ERROR, "%s - %s for query: %s\n", FUNCTION_NAME, errMsg, m_queryName.c_str());
                // TODO-SanKumar-1702: Instead should we let the upload continue, if hasMoreData == true?
                // Probably, at the next commit the bookmarking could succeed.
                throw INMAGE_EX(errMsg);
            }
        }
    }

    std::string EventLogTelemetryCollector::DevContextPtr("DevContextPtr");
    std::string EventLogTelemetryCollector::GroupingID("GroupingID");
    std::string EventLogTelemetryCollector::GroupingEndMarker("GroupingEndMarker");

    EventLogTelemetryCollector::EventLogTelemetryCollector(const std::string &queryName, const std::wstring &query)
        : m_startedGroupsCnt(0), m_endedGroupsCnt(0), m_isCutOffCheckCompleted(false), m_enumerationCompleted(false)
    {
        m_internalColl = boost::make_shared<EventLogCollector>(queryName, query);

        // Setting the cut-off time at 30 secs less than the current time.
        m_cutOffTime = boost::posix_time::second_clock::universal_time();
        m_cutOffTime -= boost::posix_time::time_duration(0, 0, 30);
    }

    void EventLogTelemetryCollector::Commit(const boost::shared_ptr<EventLogRecord> &toBkmrkRecord, bool hasMoreData)
    {
        // Simply act as a proxy, since there's no extra action needed here.
        BOOST_ASSERT(m_internalColl);
        m_internalColl->Commit(toBkmrkRecord, hasMoreData);
    }

    // TODO-SanKumar-1702: Should we rename it to InDskFltEvtLogTelemetryCollector, since this implementation
    // expects the driver specific properties. Or we can make it generic by getting the grouping Id, primary key
    // and end marker property names.
    bool EventLogTelemetryCollector::GetNext(std::map<std::string, std::string> &propMap, boost::shared_ptr<EventLogRecord> &newState)
    {
        BOOST_ASSERT(m_internalColl);

        std::string devCtxtToFlush;
        std::string groupingIdToFlush;

        // TODO-SanKumar-1702: This can end up being an intensive I/O loop for scale setups. It would
        // be important to do quit check here.
        for (;;)
        {
            boost::shared_ptr<EventLogRecord> evtRecord;
            bool hasNextValue = false;

            if (m_remainingEvent)
            {
                evtRecord.swap(m_remainingEvent);
                newState.swap(m_remainingEventState);
                hasNextValue = true;
            }
            else if (!m_enumerationCompleted)
            {
                hasNextValue = m_internalColl->GetNext(evtRecord, newState);

                // TODO-SanKumar-1703: All the collectors should be forced to stop their enumeration
                // after returning false, i.e. they should return false on calling any number of times
                // once after returning false. Currently, this is not the case in InMageLoggerCollector
                // or EventLogCollector.

                // The internal collector - EventLogCollector might actually return valid data after
                // returning false in a previous iteration. But the below logic is setup in such a way
                // that it expects enumeration will complete in one go.
                m_enumerationCompleted = !hasNextValue;
            }

            BOOST_ASSERT(!m_remainingEvent);

            if (hasNextValue)
            {
                BOOST_ASSERT(evtRecord);

                EventLogRecordParser parser(evtRecord, NULL);

                // No GDPR filtering required here, since we only parse properties
                // of the custom telemetry event log channels developed by ourselves.
                // Messages of these events are not even used here.
                std::map<std::string, std::string> properties;
                parser.GetProperties(properties);

                std::map<std::string, std::string>::iterator itr;

                itr = properties.find(DevContextPtr);
                if (itr == properties.end())
                {
                    m_lastParsedState = newState;
                    continue; // Incomplete event - can't proceed without this property.
                }
                std::string currDevCtxt = itr->second;
                properties.erase(itr);

                itr = properties.find(GroupingID);
                if (itr == properties.end())
                {
                    m_lastParsedState = newState;
                    continue; // Incomplete event - can't proceed without this property.
                }
                std::string currGroupingId = itr->second;
                // Retain the grouping id inside the map, as we find what's the ongoing grouping
                // based on this value. We will remove this before pushing out to the runner.
                // properties.erase(itr);

                itr = properties.find(GroupingEndMarker);
                if (itr == properties.end())
                {
                    m_lastParsedState = newState;
                    continue; // Incomplete event - can't proceed without this property.
                }

                // Note that anything other than true would also be informed as isEndOfGroup, instead
                // of failing. Since we have handling for the end marker not found till the last event,
                // that would apply here.
                bool isEndOfGroup = (0 == itr->second.compare("true"));
                properties.erase(itr);

                // Note that if a group isn't present for the dev ctxt, it would be created below.
                std::map<std::string, std::string> &currGroupMap = m_groupMaps[currDevCtxt];
                itr = currGroupMap.find(GroupingID);
                if (itr != currGroupMap.end() && (0 != currGroupingId.compare(itr->second)))
                {
                    // Contradicting grouping id, that didn't hit the end marker. So, flush it.
                    // But we have already acquired the next event, which should be cached for now.
                    m_remainingEvent = evtRecord;
                    m_remainingEventState = newState;

                    devCtxtToFlush = currDevCtxt;
                    groupingIdToFlush = itr->second;
                    break;
                }

                // At this point, either there's no group at all (or) this event is more data
                // for the current group id.
                BOOST_ASSERT((itr == currGroupMap.end()) == (currGroupMap.empty()));

                if (currGroupMap.empty())
                {
                    // Start of the group.
                    m_startedGroupsCnt++;
                }

                // So by this method, the time stamp of the telemetry would be the last event's time.
                parser.GetSystemTime(currGroupMap[MdsColumnNames::SrcLoggerTime]);

                // CSV of event record IDs constituting the merged telemetry event.
                std::string currSeqNum;
                parser.GetEventRecordId(currSeqNum);
                std::string &evtRecId = currGroupMap[MdsColumnNames::EventRecId];
                if (!evtRecId.empty())
                    evtRecId += ',';
                evtRecId += currSeqNum;

                // TODO-SanKumar-1702: Assert for non-duplicate inserts among partial group events.

                // Insert the acquired properties and persist the state of the internal colletor.
                currGroupMap.insert(properties.begin(), properties.end());
                m_lastParsedState = newState;

                if (!isEndOfGroup)
                    continue;

                devCtxtToFlush = currDevCtxt;
                groupingIdToFlush = currGroupingId;
                break;
            }
            else
            {
                BOOST_ASSERT(!evtRecord);
                typedef std::map<std::string, std::map<std::string, std::string>>::iterator GroupMapItr;

                if (m_groupMaps.empty())
                {
                    // TODO-SanKumar-1703: Bug: After the incomplete events are flushed, this commit would be
                    // returned as null. Incorrect persistance! This would also mean that if we are
                    // logging any shutdown or crash event, we would NEVER get the last commit state as non-null!!!

                    // propMap is left as is.
                    // BUG: This is what is wrong. We should've last parsed and last valid commit.
                    // New state is given by the internal parser.
                    return false; // All the data have been drained.
                }

                if (!m_isCutOffCheckCompleted)
                {
                    // Since we chunk a large event into multiple small events at the event writer,
                    // there could be a chance that few of the chunks are yet to be written, while
                    // we run the query and it could happen that first half (partial - no end marker)
                    // would fall in the current query and the next half (partial - with end marker)
                    // would fall in the next query.
                    // Easier solution is to drop all partial events without end marker. But that
                    // will not help us handle the crash scenario (partial - no end marker) and the
                    // event log circular rotation (partial - with end marker).
                    // Instead, we will initiate the cut off time to currTime-30s and check if we
                    // find any incomplete event between that time and current time. If so, then we
                    // will stop right here as it could only mean that we collected a partial event,
                    // which is bound to be completed in the next query.

                    boost::posix_time::ptime currTime = boost::posix_time::second_clock::universal_time();

                    for (GroupMapItr toChkGroup = m_groupMaps.begin(); toChkGroup != m_groupMaps.end(); toChkGroup++)
                    {
                        std::map<std::string, std::string>::const_iterator logTimeItr =
                            toChkGroup->second.find(MdsColumnNames::SrcLoggerTime);
                        if (logTimeItr == toChkGroup->second.cend())
                        {
                            BOOST_ASSERT(false);
                            continue;
                        }

                        std::string logTimeStr = logTimeItr->second;
                        boost::posix_time::ptime logTime;

                        try
                        {
                            // A typical event log ISO 8601 time string is printed as "2017-02-26T15:07:11.958952400Z"
                            // In order for the boost utility to work, we should remove the Z in the end.
                            if (!logTimeStr.empty() && logTimeStr[logTimeStr.length() - 1] == 'Z')
                            {
                                logTimeStr.erase(logTimeStr.length() - 1, 1);
                            }

                            logTime = boost::date_time::parse_delimited_time<boost::posix_time::ptime>(logTimeStr, 'T');
                        }
                        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                            GENERIC_CATCH_LOG_ACTION("Converting ISO 8601 to ptime", continue);

                        // This condition avoids handling time jump explicitly.
                        if (m_cutOffTime < logTime && logTime < currTime)
                        {
                            // New state must be reset, since this is not an usable state, as this
                            // commit point contains few incomplete events. So, return null and so
                            // at the end of this session, the commit would have been performed only
                            // till the last complete event (previously returned, valid non-null state).
                            // Although there would be few duplicates in the next session because of this,
                            // we would get the full row for this incomplete event then.
                            newState.reset();
                            return false; // No more valid data from this session
                        }
                    }

                    m_isCutOffCheckCompleted = true;
                }

                // Pick the first pending group and finish it.
                GroupMapItr toCompleteGroup = m_groupMaps.begin();
                devCtxtToFlush = toCompleteGroup->first;
                groupingIdToFlush = toCompleteGroup->second[GroupingID];
                break;
            }
        }

        BOOST_ASSERT(!devCtxtToFlush.empty());
        std::map<std::string, std::string> &grpToFlush = m_groupMaps[devCtxtToFlush];
        BOOST_ASSERT(!groupingIdToFlush.empty());
        BOOST_ASSERT(0 == grpToFlush[GroupingID].compare(groupingIdToFlush));

        // Erase the extra grouping id kv pair, as it's only needed for this parser's use.
        grpToFlush.erase(GroupingID);

        // Fill the properties
        propMap.insert(grpToFlush.begin(), grpToFlush.end());
        m_endedGroupsCnt++;

        // Erase the being uploaded merged event.
        m_groupMaps.erase(devCtxtToFlush);

        // TODO-SanKumar-1702: Actually seems like all groups completed at the given
        // point could be achieved with groupMaps.empty() too. Investigate and simplify.
        BOOST_ASSERT((m_startedGroupsCnt == m_endedGroupsCnt) == m_groupMaps.empty());

        // At any given point of returning a merged event, see if the completed range
        // has no pending groups, if so a persistable state could be issued to the runner.
        if (m_startedGroupsCnt == m_endedGroupsCnt)
            newState = m_lastParsedState;
        else
            newState.reset();
        // TODO-SanKumar-1703: Instead of having to resort to returning a null commit point, in case
        // of an event whose position can't be committed because of pending incomplete events, always
        // return a non-null commit that points to the last valid commit point to simplify the logic.

        return true;

        // Unlike other converters, we can't afford to continue ignoring any exception with a blanket catch.
        // If there's a bug in the logic, then it might run in an infinite loop. As a trade off for
        // safety, fail the method here.
    }

#endif // SV_WINDOWS

    FilePollCollector::FilePollCollector(
        boost::shared_ptr<CxTransport> fileTransport,
        const boost::filesystem::path& folderPattern,
        const std::string& fileExt,
        boost::function1<bool, const std::string&> predicate)
        : m_cxTransport(fileTransport),
        m_queryCompleted(0),
        m_nextIndex(0),
        m_fileExt(fileExt),
        m_predicate(predicate)
    {
        m_pattern = folderPattern / ("*_completed_*" + fileExt);
    }

    static bool IsFileNameLessThan(std::map<std::string, std::string> &map, const std::string &filePath1, const std::string &filePath2)
    {
        BOOST_ASSERT(map.find(filePath1) != map.end());
        BOOST_ASSERT(map.find(filePath2) != map.end());

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // TODO-SanKumar-NOW!!!: The string sorting isn't enough, as number might be of different sizes. So we should also see
        // the length of the number.

        return map[filePath1] < map[filePath2];
    }

    bool FilePollCollector::GetNext(std::vector<std::string>& nextFilePaths, std::vector<std::string>& filePathsToClear)
    {
        if (!m_queryCompleted)
        {
            const bool includePath = true;
            FileInfos_t matchingFiles;

            BOOST_ASSERT(m_cxTransport);

            boost::tribool queryResult = m_cxTransport->listFile(m_pattern.string(), matchingFiles, includePath);

            if (!boost::indeterminate(queryResult) && !queryResult)
            {
                const char *errMsg = "Failed to list the files in FilePollCollector";
                DebugPrintf(EvCF_LOG_ERROR, "%s - %s with pattern : %s. Error : %s.\n",
                    FUNCTION_NAME, errMsg, m_pattern.c_str(), m_cxTransport->status());
                throw INMAGE_EX(errMsg);
            }

            BOOST_ASSERT(boost::indeterminate(queryResult) == matchingFiles.empty());

            for (FileInfos_t::iterator itr = matchingFiles.begin(); itr != matchingFiles.end(); itr++)
            {
                const std::string &currFilePath = itr->m_name;
                // TODO-SanKumar-1702: Is the host ID matching case insensitive?!
                if (m_predicate && !m_predicate(currFilePath))
                    continue;

                const std::string &currFileName = boost::filesystem::path(currFilePath).filename().string();
                m_nameSortedFilePaths.push_back(currFilePath);
                m_pathToNameMap.insert(std::make_pair(currFilePath, currFileName));
                //m_pathToNameMap.insert(std::pair<std::string, std::string>(currFilePath, currFileName));
            }

            // TODO-SanKumar-1702: Wrong kind of sorting - Get the created time for this!
            // !!!!!!!!!!!!!!!!!!!!!!!!!
            std::sort(m_nameSortedFilePaths.begin(), m_nameSortedFilePaths.end(),
                boost::bind(IsFileNameLessThan, m_pathToNameMap, _1, _2));

            m_queryCompleted = true;
        }

        nextFilePaths.clear();

        int currIndex = m_nextIndex;
        int startIndex = currIndex;
        std::string currMatchingPrefix;

        for (; currIndex != m_nameSortedFilePaths.size(); currIndex++)
        {
            const std::string &currFilePath = m_nameSortedFilePaths[currIndex];
            const std::string &currFileName = m_pathToNameMap[currFilePath];

            if (currMatchingPrefix.empty())
            {
                currMatchingPrefix = std::string(currFileName.begin(),
                    std::find(currFileName.begin(), currFileName.end(), '_'));
            }
            else
            {
                if (!boost::starts_with(currFileName, currMatchingPrefix))
                    break;
            }

            // TODO-SanKumar-1702: Currently we trust that ListFile gives us the files matching
            // the requested pattern only. Should we rather check the name integrity anyway?!
            BOOST_ASSERT(boost::starts_with(
                boost::make_iterator_range(currFileName.cbegin() + currMatchingPrefix.size(), currFileName.cend()),
                "_completed_"));
            BOOST_ASSERT(boost::iends_with(currFileName, m_fileExt));
        }

        m_nextIndex = currIndex;

        nextFilePaths = std::vector < std::string >(m_nameSortedFilePaths.begin() + startIndex, m_nameSortedFilePaths.begin() + currIndex);

        // This is a completely redundant information that could be derived from the collected data
        // itself. This also gives the forwarder the flexibility to delete selected files, say succeeded
        // files or commit in small chunks, etc.
        //filePathsToClear = nextFilePaths;
        filePathsToClear.clear();

        return !nextFilePaths.empty();
    }

    void FilePollCollector::Commit(const std::vector<std::string> &completedFiles, bool hasMoreData)
    {
        // One another option is to add up all the completed files and finally (hasMoreData == false)
        // and try deleting them all. But, the unit of collection here is a file and it would rather
        // be favorable to delete the completed files at every commit.

        for (std::vector < std::string > ::const_iterator itr = completedFiles.begin(); itr != completedFiles.end(); itr++)
        {
            BOOST_ASSERT(!itr->empty());
            Utils::TryDeleteFile(*itr);
        }
    }

    AgentSetupLogsCollector::AgentSetupLogsCollector(boost::shared_ptr<CxTransport> fileTransport,
        const boost::filesystem::path& folderPattern,
        const std::string& fileExt)
        : m_cxTransport(fileTransport),
        m_fileExt(fileExt),
        m_queryCompleted(false)
    {
        m_pattern = folderPattern / ("ASR*" + fileExt);
    }

    bool AgentSetupLogsCollector::GetNext(
        std::string& nextFilePath,
        std::string& filePathsToClear)
    {
        if (!m_queryCompleted)
        {
            m_fileListToProcess.empty();
            const bool includePath = true;
            FileInfos_t matchingFiles;

            BOOST_ASSERT(m_cxTransport);

            boost::tribool queryResult = m_cxTransport->listFile(m_pattern.string(), matchingFiles, includePath);

            if (!boost::indeterminate(queryResult) && !queryResult)
            {
                const char *errMsg = "Failed to list the files in FilePollCollector";
                DebugPrintf(EvCF_LOG_ERROR, "%s - %s with pattern : %s. Error : %s.\n",
                    FUNCTION_NAME, errMsg, m_pattern.c_str(), m_cxTransport->status());
                throw INMAGE_EX(errMsg);
            }

            BOOST_ASSERT(boost::indeterminate(queryResult) == matchingFiles.empty());

            for (FileInfos_t::iterator itr = matchingFiles.begin(); itr != matchingFiles.end(); itr++)
            {
                const std::string &currFilePath = itr->m_name;
                m_fileListToProcess.push_back(currFilePath);
            }

            m_queryCompleted = true;
        }

        filePathsToClear.clear();
        nextFilePath.clear();

        if (!m_fileListToProcess.empty())
        {
            std::vector<std::string>::iterator itr = m_fileListToProcess.begin();
            nextFilePath = *itr;
            m_fileListToProcess.erase(itr);
            filePathsToClear = nextFilePath;
        }

        return !nextFilePath.empty();
    }

    void AgentSetupLogsCollector::Commit(
        const std::string &completedFile,
        bool hasMoreData)
    {
        BOOST_ASSERT(!completedFile.empty());
        Utils::TryDeleteFile(completedFile);
    }
}
