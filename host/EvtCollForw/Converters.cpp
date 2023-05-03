#include <boost/date_time/gregorian/greg_date.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include "portablehelpersmajor.h"
#include "json_reader.h"

#include "EvtCollForwCommon.h"
#include "Converters.h"

namespace EvtCollForw
{
    bool ParseInMageLog(const std::string &log, SV_LOG_LEVEL filterLogLevel,
        boost::posix_time::ptime &time, std::string &agentLogLevel,
        pid_t &pid, ACE_thread_t &tid, int64_t &seqNum, std::string &message)
    {
        SV_LOG_LEVEL currLogLevel;

        try
        {
            if (!LoggerReaderBase::ExtractMetadata(log, time, currLogLevel, message, pid, tid, seqNum))
                return false; // Illegal formatting; Communicate to the runner, so it goes ahead with the next message.

            // TODO-SanKumar-1702: Move this level check into the GetNextMessage itself.
            // Currently, with this approach, even for the discarded messages, we do an
            // unnecessary read and copy.

            // Force send this event, if filterLogLevel is not disable and currLogLevel is always.
            if (filterLogLevel <= SV_LOG_DISABLE || currLogLevel != SV_LOG_ALWAYS)
                // Ignore this event, if the currLogLevel is disable or currLogLevel is above the filterLogLevel.
                if (currLogLevel <= SV_LOG_DISABLE || currLogLevel > filterLogLevel)
                    return false; // This message failed the minimum level filtering.

            if (!GetLogLevelName(currLogLevel, agentLogLevel))
            {
                // If the log level can't be interpretted here at source, the same issue would occur
                // at the destination as well. Better to avoid sending this.
                return false;
            }
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Parsing inMage log", return false);

        return true;
    }

#define AddMdsColumn(map,name,value) BOOST_VERIFY(map.insert( \
                                        std::pair<std::string, std::string>( \
                                        (const std::string&)MdsColumnNames::name, (const std::string&)value)).second)

    bool InMageLoggerToMdsLogConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            boost::posix_time::ptime time;
            std::string agentLogLevel;
            pid_t pid;
            ACE_thread_t tid;
            int64_t seqNum;
            std::string message;

            if (!ParseInMageLog(loggerMessage, m_agentLogPostLevel, time, agentLogLevel, pid, tid, seqNum, message))
                return false;

            AddMdsColumn(kvPairMap, Message, message);
            AddMdsColumn(kvPairMap, AgentLogLevel, agentLogLevel);
            AddMdsColumn(kvPairMap, SubComponent, m_subComponent);
            AddMdsColumn(kvPairMap, AgentTimeStamp, boost::posix_time::to_iso_extended_string(time));
            AddMdsColumn(kvPairMap, AgentPid, boost::lexical_cast<std::string>(pid));
            AddMdsColumn(kvPairMap, AgentTid, boost::lexical_cast<std::string>(tid));
            AddMdsColumn(kvPairMap, SequenceNumber, boost::lexical_cast<std::string>(seqNum));
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting InMage log to MDS JSON", return false);

        return true;
    }

    bool IRSourceLoggerToMdsLogConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {   
            kvPairMap[MdsColumnNames::ExtendedProperties] = JSON::producer<AgentLogExtendedProperties>::convert(m_extProperties);

            InMageLoggerToMdsLogConverter::OnConvert(loggerMessage, kvPairMap);
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting IR Source log to MDS JSON", return false);

        return true;
    }

    bool IRRcmSourceLoggerToMdsLogConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            kvPairMap[MdsColumnNames::ExtendedProperties] = JSON::producer<AgentLogExtendedProperties>::convert(m_extProperties);

            InMageLoggerToMdsLogConverter::OnConvert(loggerMessage, kvPairMap);
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting IR RCM Source log to MDS JSON", return false);

        return true;
    }

    bool IRMTLoggerToMdsLogConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            kvPairMap[MdsColumnNames::ExtendedProperties] = JSON::producer<AgentLogExtendedProperties>::convert(m_extProperties);

            InMageLoggerToMdsLogConverter::OnConvert(loggerMessage, kvPairMap);
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting IR MT log to MDS JSON", return false);

        return true;
    }

    // TOTO: nichougu - revisit base class hierarchy to get rid of m_extProperties member in derived
    //                  classes and in effect duplicate code of OnConvert function.
    bool IRRcmMTLoggerToMdsLogConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            kvPairMap[MdsColumnNames::ExtendedProperties] = JSON::producer<AgentLogExtendedProperties>::convert(m_extProperties);
            // TODO: nichougu - serialize the map to avoid AgentLogExtendedProperties wrapper around it - which also saves the space.

            InMageLoggerToMdsLogConverter::OnConvert(loggerMessage, kvPairMap);
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting IR RCM MT log to MDS JSON", return false);

        return true;
    }

    bool DRMTLoggerToMdsLogConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            kvPairMap[MdsColumnNames::ExtendedProperties] = JSON::producer<AgentLogExtendedProperties>::convert(m_extProperties);

            InMageLoggerToMdsLogConverter::OnConvert(loggerMessage, kvPairMap);
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting DR MT log to MDS JSON", return false);

        return true;
    }

    bool InMageLoggerTelemetryToMdsConverter::OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            boost::posix_time::ptime time;
            std::string agentLogLevel;
            pid_t pid;
            ACE_thread_t tid;
            int64_t seqNum;
            std::string message;

            if (!ParseInMageLog(loggerMessage, m_agentTelemetryPostLevel, time, agentLogLevel, pid, tid, seqNum, message))
                return false;

            MdsLogUnit logUnit = JSON::consumer<MdsLogUnit>::convert(message);
            kvPairMap = logUnit.Map;

            AddMdsColumn(kvPairMap, SrcLoggerTime, boost::posix_time::to_iso_extended_string(time));
            AddMdsColumn(kvPairMap, AgentPid, boost::lexical_cast<std::string>(pid));
            AddMdsColumn(kvPairMap, AgentTid, boost::lexical_cast<std::string>(tid));
            AddMdsColumn(kvPairMap, SequenceNumber, boost::lexical_cast<std::string>(seqNum));
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting InMage telemetry log to MDS JSON", return false);

        return true;
    }

    bool AgentSetupLogConvertor::Convert(const std::string & fileToProcess, std::vector<std::string> & fileContent)
    {
        try
        {
            boost::filesystem::path filepath(fileToProcess);
            if (!filepath.has_parent_path())
            {
                std::string errMsg = "The filepath " + fileToProcess + " does not have a parent path.";
                throw std::runtime_error(errMsg.c_str());
            }

            if (!filepath.parent_path().has_parent_path())
            {
                std::string errMsg = "The filepath " + fileToProcess + " does not have a parent to parent path.";
                throw std::runtime_error(errMsg.c_str());
            }

            std::string clientrequestId = filepath.parent_path().filename().string();
            boost::algorithm::replace_all(clientrequestId, "_", ":");
            std::string hostId = filepath.parent_path().parent_path().filename().string();

            if (!ProcessFile(fileToProcess, clientrequestId, hostId, fileContent))
                return false;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting InMage telemetry log to MDS JSON", return false);

        return true;
    }

    bool AgentSetupLogConvertor::ProcessFile(const std::string & fileToProcess,
        std::string clientrequestId,
        std::string hostId,
        std::vector<std::string> & fileContent)
    {
        try
        {
            std::map<std::string, std::string>subcomponentMap;
            subcomponentMap.insert(std::make_pair("install", "installer"));
            subcomponentMap.insert(std::make_pair("azurercmcli", "azurercmcli"));
            subcomponentMap.insert(std::make_pair("upgrade", "upgrader"));
            subcomponentMap.insert(std::make_pair("svagents", "svagents"));
            subcomponentMap.insert(std::make_pair("msi", "msi"));
            subcomponentMap.insert(std::make_pair("precheck", "precheck"));
            subcomponentMap.insert(std::make_pair("wrapper", "wrapperagent"));

            std::map<std::string, std::string> logrow = m_toInjectKVPairs;
            logrow.insert(std::make_pair(MdsColumnNames::ClientRequestId, clientrequestId));
            logrow.insert(std::make_pair(MdsColumnNames::HostId, hostId));

            bool subcomponentFound = false;
            for (std::map<std::string, std::string>::iterator itr = subcomponentMap.begin();
                itr != subcomponentMap.end(); itr++)
            {
                if (boost::algorithm::icontains(fileToProcess, itr->first))
                {
                    logrow.insert(std::make_pair(MdsColumnNames::SubComponent, itr->second));
                    subcomponentFound = true;
                    break;
                }
            }

            if (!subcomponentFound)
            {
                logrow.insert(std::make_pair(MdsColumnNames::SubComponent, "Unknown"));
            }

            std::ifstream ifs;
            ifs.exceptions(ifstream::badbit);
            ifs.open(fileToProcess.c_str(), ios::in);

            std::string currDataLine;
            fileContent.clear();
            while (!ifs.eof() && std::getline(ifs, currDataLine))
            {
                MdsLogUnit logUnit;
                logUnit.Map = logrow;
                logUnit.Map[MdsColumnNames::Message] = currDataLine;
                logUnit.Map[MdsColumnNames::AgentTimeStamp] = boost::posix_time::to_iso_extended_string(
                    boost::posix_time::second_clock::universal_time());
                std::string jsonObjString = JSON::producer<MdsLogUnit>::convert(logUnit);

                if (m_removeMapKeyName)
                {
                    jsonObjString.replace(0, 7, 7, ' ');

                    if (!jsonObjString.empty())
                        *jsonObjString.rbegin() = ' ';
                    boost::trim(jsonObjString);
                }

                fileContent.push_back(jsonObjString);
                currDataLine.clear();
            }
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Converting InMage telemetry log to MDS JSON", return false);

        return true;
    }

#ifdef SV_WINDOWS
    std::map<std::string, std::vector<uint32_t>> EventLogToMdsLogConverter::s_gdprAllowListedEventsMap;
    std::set<std::string> EventLogToMdsLogConverter::s_gdprAllEventsAllowListedSubComponents;
    std::set<std::string> EventLogToMdsLogConverter::s_gdprUnstoppableSubComponents;

    static const char *GdprFilterInfoJson =
        "{ \
            \"AllEventsAllowListedSubComponents\" : [ \
                \"AzureSiteRecoveryVssProvider/Application\", \
                \"Microsoft-InMage-InDiskFlt/Admin\", \
                \"Microsoft-InMage-InDiskFlt/Operational\", \
                \"InDskFlt/System\" \
            ], \
            \"UnstoppableSubComponents\" : [ \
                \"AzureSiteRecoveryVssProvider/Application\", \
                \"Microsoft-InMage-InDiskFlt/Admin\", \
                \"Microsoft-InMage-InDiskFlt/Operational\", \
                \"InDskFlt/System\", \
                \"Service Control Manager/System\" \
            ], \
            \"AllowListedEvents\" : [ \
                { \
                    \"SubComponent\" : \"EventLog/System\", \
                    \"EventIds\" : [6005, 6006, 6008, 6009, 6011, 6013] \
                }, \
                { \
                    \"SubComponent\" : \"iScsiPrt/System\", \
                    \"EventIds\" : [1, 5, 7, 9, 10, 11, 17, 20, 21, 23, 27, 29, 32, 34, 38, 39, 42, 43, 44, 48, 49, 52, 54, 59, 63, 64, 67, 70, 71, 129] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-Kernel-Boot/Operational\", \
                    \"EventIds\" : [39, 42, 45, 51, 154, 155, 157, 158] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-Kernel-Boot/System\", \
                    \"EventIds\" : [17, 18, 19, 20, 21, 22, 24, 25, 26, 27, 30, 32, 124, 153, 156] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-Kernel-Power/System\", \
                    \"EventIds\" : [40, 41, 42, 89, 109, 125, 172, 508] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-Kernel-Power/Thermal-Operational\", \
                    \"EventIds\" : [84, 116] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-WER-SystemErrorReporting/System\", \
                    \"EventIds\" : [1001, 1005] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-Wininit/System\", \
                    \"EventIds\" : [11, 12, 14] \
                }, \
                { \
                    \"SubComponent\" : \"mpio/System\", \
                    \"EventIds\" : [1, 2, 3, 16, 17, 18, 20, 22, 23, 24, 25, 32, 33, 37, 40, 44, 45, 46, 48] \
                }, \
                { \
                    \"SubComponent\" : \"NTFS/System\", \
                    \"EventIds\" : [57, 132, 133] \
                }, \
                { \
                    \"SubComponent\" : \"partmgr/System\", \
                    \"EventIds\" : [58, 59] \
                }, \
                { \
                    \"SubComponent\" : \"Service Control Manager/System\", \
                    \"EventIds\" : [7000, 7001, 7003, 7009, 7011, 7022, 7023, 7030, 7031, 7032, 7034, 7036, 7039, 7040, 7046] \
                }, \
                { \
                    \"SubComponent\" : \"storflt/System\", \
                    \"EventIds\" : [3, 4, 5] \
                }, \
                { \
                    \"SubComponent\" : \"vhdmp/System\", \
                    \"EventIds\" : [129] \
                }, \
                { \
                    \"SubComponent\" : \"volmgr/System\", \
                    \"EventIds\" : [45, 46, 49, 57] \
                }, \
                { \
                    \"SubComponent\" : \"Volsnap/System\", \
                    \"EventIds\" : [20482] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-BitLocker-DrivePreparationTool/Admin\", \
                    \"EventIds\" : [512] \
                }, \
                { \
                    \"SubComponent\" : \"Microsoft-Windows-BitLocker-DrivePreparationTool/Operational\", \
                    \"EventIds\" : [4097,4098, 4101] \
                } \
            ] \
         }";

    struct GdprFilterInfo
    {
        struct AllowListedEventsInfo
        {
            void serialize(JSON::Adapter &adapter)
            {
                JSON::Class root(adapter, "AllowListedEventsInfo", false);

                JSON_E(adapter, SubComponent);
                JSON_T(adapter, EventIds);
            }

            std::string SubComponent;
            std::vector<int64_t> EventIds;
        };

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "GdprFilterInfo", false);

            JSON_E(adapter, AllEventsAllowListedSubComponents);
            JSON_E(adapter, UnstoppableSubComponents);
            JSON_T(adapter, AllowListedEvents);
        }

        svector_t AllEventsAllowListedSubComponents;
        svector_t UnstoppableSubComponents;
        std::vector<AllowListedEventsInfo> AllowListedEvents;
    };

    void EventLogToMdsLogConverter::InitializeGdprFilteringInfo()
    {
        GdprFilterInfo filterInfo = JSON::consumer<GdprFilterInfo>::convert(GdprFilterInfoJson);

        // Store in better data structures and uniform casing for quick access.

        for (std::string& currSubComponent : filterInfo.AllEventsAllowListedSubComponents)
        {
            boost::to_lower(currSubComponent);
            s_gdprAllEventsAllowListedSubComponents.insert(std::move(currSubComponent));
        }

        for (std::string& currSubComponent : filterInfo.UnstoppableSubComponents)
        {
            boost::to_lower(currSubComponent);
            s_gdprUnstoppableSubComponents.insert(std::move(currSubComponent));
        }

        for (auto &currSubComponentInfo : filterInfo.AllowListedEvents)
        {
            std::vector<uint32_t> uint32EventIds(currSubComponentInfo.EventIds.size());
            std::vector<uint32_t>::iterator uint32EventIdsItr = uint32EventIds.begin();

            for (int64_t currEventId : currSubComponentInfo.EventIds)
            {
                if (!SafeCast(currEventId, *uint32EventIdsItr))
                    throw std::bad_cast::__construct_from_string_literal("EventIds in AllowListedEvents info in Gdpr filter must be uint32");

                ++uint32EventIdsItr;
            }

            BOOST_ASSERT(uint32EventIdsItr == uint32EventIds.end());

            std::sort(uint32EventIds.begin(), uint32EventIds.end());

            boost::to_lower(currSubComponentInfo.SubComponent);

            if (s_gdprAllowListedEventsMap.find(currSubComponentInfo.SubComponent) != s_gdprAllowListedEventsMap.end())
                throw INMAGE_EX("Duplicate entries found in AllowListedEvents info in Gdpr Filter");

            s_gdprAllowListedEventsMap.insert(
                std::make_pair(
                    std::move(currSubComponentInfo.SubComponent),
                    std::move(uint32EventIds)));
        }
    }

    bool EventLogToMdsLogConverter::OnConvert(const boost::shared_ptr<EventLogRecord> &evtRecord, std::map<std::string, std::string> &kvPairMap)
    {
        try
        {
            std::string providerName;
            std::string channelName;
            std::string subComponent;
            std::string lowerSubComponent;
            std::string eventIdStr;

            // Take the precautionary approach that every event is avoided unless
            // every check is passed.
            bool includeUserDataInMdsRow = false;

            AgentLogExtendedProperties extProperties;
            std::map<std::string, std::string> &extPropMap = extProperties.ExtendedProperties;

            // Note-SanKumar-1803: If there's a large amount of event logs and
            // there's network error, it could take us a lot of minutes instead
            // of seconds to complete the upload of all event logs. In the mean
            // time, if an event log publisher is disabled/upgraded, its handle
            // in the store becomes invalidated (on using it, event log API
            // returns ERROR_EVT_PUBLISHER_DISABLED). So, we might miss uploading
            // event logs of that publisher in the current iteration. I think we
            // can take that hit, given this can happen only when the collection
            // and forwarding is long enough to have a upgrade/uninstall in that
            // window. Also, note that we delay load the publisher handle until
            // a first event of it is parsed. Also, we take the log upload miss
            // in many similar cases, since we don't wanna fail at that point, as
            // we don't have any idea when/if the situation will be recovered.
            // I have also validated that the operations on the publisher is not
            // blocked by us holding the handles for it for long time.
            EventLogRecordParser parser(evtRecord, &m_publisherHandleStore);

            parser.GetProviderName(providerName);
            parser.GetChannelName(channelName);

            subComponent = channelName;
            if (0 == channelName.compare("System") ||
                0 == channelName.compare("Application") ||
                0 == channelName.compare("Setup") ||
                0 == channelName.compare("Security"))
            {
                // Additional handling to identify the source of the legacy events.
                if (!providerName.empty())
                {
                    subComponent = providerName + '/' + channelName;
                }
            }
            kvPairMap[MdsColumnNames::SubComponent] = subComponent;

            lowerSubComponent = subComponent;
            boost::to_lower(lowerSubComponent);

            if (!m_isEventLogUploadEnabled &&
                s_gdprUnstoppableSubComponents.find(lowerSubComponent) == s_gdprUnstoppableSubComponents.end())
            {
                // Customer explictly requested not to upload the event logs.
                // Unless this an unstoppable subcomponent, drop this event.

                // TODO-SanKumar-1805: We should add a log (or) counter that
                // would be uploaded to Kusto to inform about the drops.

                return false;
            }

            parser.GetEventId(eventIdStr);
            kvPairMap[MdsColumnNames::SourceEventId] = eventIdStr;

            if (s_gdprAllEventsAllowListedSubComponents.find(lowerSubComponent) != s_gdprAllEventsAllowListedSubComponents.end())
            {
                includeUserDataInMdsRow = true;
            }
            else if (!eventIdStr.empty())
            {
                std::map<std::string, std::vector<uint32_t>>::const_iterator itr;
                itr = s_gdprAllowListedEventsMap.find(lowerSubComponent);

                if (itr != s_gdprAllowListedEventsMap.end())
                {
                    bool parsedEventId = false;
                    uint32_t sourceEventId;

                    try
                    {
                        sourceEventId = std::stoul(eventIdStr);
                        parsedEventId = true;
                    }
                    CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Converting Source Event ID string to uint32");

                    if (parsedEventId)
                    {
                        const std::vector<uint32_t> &AllowListedEventIds = itr->second;

                        includeUserDataInMdsRow = std::binary_search(
                            AllowListedEventIds.cbegin(), AllowListedEventIds.cend(), sourceEventId);
                    }
                }
            }

            parser.GetEvtMessage(includeUserDataInMdsRow, kvPairMap[MdsColumnNames::Message]);

            if (includeUserDataInMdsRow)
                parser.GetProperties(extPropMap);

            extPropMap["EvCF_GdprRedacted"] = includeUserDataInMdsRow? "false" : "true";

            // Legacy events doesn't have version but only manifest-based events.
            std::string eventVer;
            if (parser.GetVersion(eventVer))
                extPropMap["EvCF_Ver"] = eventVer;

            if (!extPropMap.empty() &&
                (0 == lowerSubComponent.compare("microsoft-inmage-indiskflt/admin") ||
                 0 == lowerSubComponent.compare("microsoft-inmage-indiskflt/operational")))
            {
                std::map<std::string, std::string>::iterator itr;

                itr = extPropMap.find("DiskID");
                if (itr != extPropMap.end())
                {
                    kvPairMap[MdsColumnNames::DiskId] = itr->second;
                    extPropMap.erase(itr);
                }

                itr = extPropMap.find("DiskNumber");
                if (itr != extPropMap.end())
                {
                    kvPairMap[MdsColumnNames::DiskNumber] = itr->second;
                    extPropMap.erase(itr);
                }

                itr = extPropMap.find("Status");
                if (itr != extPropMap.end())
                {
                    kvPairMap[MdsColumnNames::ErrorCode] = itr->second;
                    extPropMap.erase(itr);
                }
            }

            if (!extPropMap.empty())
                kvPairMap[MdsColumnNames::ExtendedProperties] = JSON::producer<AgentLogExtendedProperties>::convert(extProperties);

            parser.GetLevelName(kvPairMap[MdsColumnNames::AgentLogLevel]);
            parser.GetSystemTime(kvPairMap[MdsColumnNames::AgentTimeStamp]);

            parser.GetProcessId(kvPairMap[MdsColumnNames::AgentPid]);
            parser.GetThreadId(kvPairMap[MdsColumnNames::AgentTid]);
            parser.GetEventRecordId(kvPairMap[MdsColumnNames::SequenceNumber]);
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
        GENERIC_CATCH_LOG_ACTION("Conversion in EventLogToMdsConverter", return false);

        return true;
    }

    bool EventLogTelemetryToMdsConverter::OnConvert(const std::map<std::string, std::string> &propMap, std::map<std::string, std::string> &kvPairMap)
    {
        BOOST_ASSERT(!propMap.empty());

        try
        {
            kvPairMap = propMap;
            // TODO-SanKumar-1704: std::move() could avoid the copy but its a C++11 feature.
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_ACTION("Conversion in EventLogTelemetryToMdsConverter", return false);

        return true;
    }
#endif // SV_WINDOWS

#ifdef SV_UNIX
    static const boost::posix_time::ptime EpochStartTime(boost::gregorian::date(1970, 1, 1));

    bool InVolFltLinuxTelemetryJsonMdsConverter::OnConvert(const std::string &telemetrylogLine, std::map<std::string, std::string> &kvPairMap)
    {
        MdsLogUnit logUnit = JSON::consumer<MdsLogUnit>::convert(telemetrylogLine);
        // TODO-SanKumar-1704: Avoid this additional copy by using C++11 move feature.
        kvPairMap = logUnit.Map;

        std::map<std::string, std::string>::iterator srcLoggerTimeKV = kvPairMap.find(MdsColumnNames::SrcLoggerTime);
        if (srcLoggerTimeKV != kvPairMap.end())
        {
            try
            {
                uint64_t unixEpoch100ns = boost::lexical_cast<uint64_t>(srcLoggerTimeKV->second);
                boost::posix_time::ptime flattenedPtime = EpochStartTime + boost::posix_time::microseconds(unixEpoch100ns / 10);
                srcLoggerTimeKV->second = boost::posix_time::to_iso_extended_string(flattenedPtime);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Parsing unix epoch from Linux driver telemetry");

            // With the above catch and ignore, if there are any parsing error in converting the
            // SrcLoggerTime from unix epoch to iso string, we would simply leave the unix epoch in
            // the key-value pair and proceed to forwarding this event instead of dropping it.
        }

        return true;
    }
#endif // SV_UNIX
}
