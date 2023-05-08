#ifndef EVTCOLLFORW_CONVERTERS_H
#define EVTCOLLFORW_CONVERTERS_H

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <string>

#include "logger.h"
#include "configurator2.h"
#include "RcmConfigurator.h"
#include "localconfigurator.h"

#include "LoggerReader.h"

#ifdef SV_WINDOWS
#include "EventLogReader.h"
#endif // SV_WINDOWS

extern boost::shared_ptr<LocalConfigurator> lc;

namespace EvtCollForw
{
    class AgentLogExtendedProperties
    {
    public:
        std::map<std::string, std::string> ExtendedProperties;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "AgentLogExtendedProperties", false);

            JSON_T(adapter, ExtendedProperties);
        }
    };

    template <class TInput, class TOutput>
    class IConverter : boost::noncopyable
    {
    public:
        virtual ~IConverter() {}
        virtual bool Convert(const TInput& input, TOutput& output) = 0;
    };

    class MdsLogUnit
    {
    public:
        std::map<std::string, std::string> Map;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "MdsLogUnit", false);

            JSON_T(adapter, Map);
        }
    };

    template<class TData>
    class NullConverter : public IConverter<TData, TData>
    {
    public:
        virtual ~NullConverter() {}

        virtual bool Convert(const TData& input, TData& output)
        {
            // TODO-SanKumar-1704: Candidate for std::move()
            output = input;
            return true;
        }
    };

    template <class TInput>
    class JsonKVPairMdsConverterBase : public IConverter<TInput, std::string>
    {
    public:
        JsonKVPairMdsConverterBase()
            : m_removeMapKeyName(false)
        {
        }

        virtual ~JsonKVPairMdsConverterBase() {}

        virtual bool Convert(const TInput &input, std::string &jsonObjString)
        {
            try
            {
                MdsLogUnit logUnit;

                // Get KV pairs from sub-class.
                if (!OnConvert(input, logUnit.Map))
                    return false;

                typedef std::map<std::string, std::string>::const_iterator StrMapCItr;
                for (StrMapCItr itr = m_toInjectKVPairs.begin(); itr != m_toInjectKVPairs.end(); itr++)
                {
                    logUnit.Map[itr->first] = itr->second;
                }

                jsonObjString = JSON::producer<MdsLogUnit>::convert(logUnit);

                if (m_removeMapKeyName)
                {
                    // We should be removing the Map key name (basically return the object stored in
                    // the map directly, instead of the Map object itself).

                    // To avoid unnecessary space allocations, set the name Map and enclosing brackets
                    // to empty spaces, which makes the inner object to become the main object.

                    // {"Map":
                    // 7 chars in the beginning
                    jsonObjString.replace(0, 7, 7, ' ');

                    // }
                    // 1 char in the end
                    if (!jsonObjString.empty())
                        *jsonObjString.rbegin() = ' ';
                    boost::trim(jsonObjString);
                }
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_ACTION("Conversion in JsonKVPairMdsConverterBase", return false);

            return true;
        }

    protected:
        virtual bool OnConvert(const TInput &input, std::map<std::string, std::string> &kvPairMap) = 0;

        void Initialize(const std::map<std::string, std::string> &toInjectKVPairs, bool removeMapKeyName)
        {
            m_toInjectKVPairs.insert(toInjectKVPairs.begin(), toInjectKVPairs.end());
            m_removeMapKeyName = removeMapKeyName;
        }

    private:
        std::map<std::string, std::string> m_toInjectKVPairs;
        bool m_removeMapKeyName;
    };

    template <class TInput>
    class JsonKVPairMdsLogConverterBase : public JsonKVPairMdsConverterBase<TInput>
    {
    public:
        virtual ~JsonKVPairMdsLogConverterBase() {}

        void Initialize(const std::string &fabricType, const std::string &biosId,
            const std::string &machineId, const std::string &componentId, bool removeMapKeyName)
        {
            std::map<std::string, std::string> toInjectKVPairs;

            toInjectKVPairs[MdsColumnNames::FabType] = fabricType;
            toInjectKVPairs[MdsColumnNames::BiosId] = biosId;
            toInjectKVPairs[MdsColumnNames::MachineId] = machineId;
            toInjectKVPairs[MdsColumnNames::ComponentId] = componentId;

            JsonKVPairMdsConverterBase<TInput>::Initialize(toInjectKVPairs, removeMapKeyName);
        }
    };

    template <class TInput>
    class JsonKVPairMdsTelemetryConverterBase : public JsonKVPairMdsConverterBase<TInput>
    {
    public:
        virtual ~JsonKVPairMdsTelemetryConverterBase() {}

        void Initialize(const std::string &fabricType, const std::string &biosId, const std::string &hostId, bool removeMapKeyName)
        {
            std::map<std::string, std::string> toInjectKVPairs;

            toInjectKVPairs[MdsColumnNames::FabType] = fabricType;
            toInjectKVPairs[MdsColumnNames::BiosId] = biosId;
            toInjectKVPairs[MdsColumnNames::HostId] = hostId;

            JsonKVPairMdsConverterBase<TInput>::Initialize(toInjectKVPairs, removeMapKeyName);
        }
    };

    class InMageLoggerToMdsLogConverter : public JsonKVPairMdsLogConverterBase<std::string>
    {
    public:
        InMageLoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel, const std::string &subComponent)
            : m_agentLogPostLevel(agentLogPostLevel),
            m_subComponent(subComponent)
        {
        }

        virtual ~InMageLoggerToMdsLogConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        SV_LOG_LEVEL m_agentLogPostLevel;
        std::string m_subComponent;
    };

    class AgentSetupLogConvertor : public IConverter< std::string, std::vector<std::string> >
    {
    public:
        virtual ~AgentSetupLogConvertor() {}

        void Initialize(const std::string &fabricType, const std::string &biosId, const std::string &psHostId, 
            std::string componentName, bool removeMapKeyName)
        {
            m_toInjectKVPairs[MdsColumnNames::FabType] = fabricType;
            m_toInjectKVPairs[MdsColumnNames::BiosId] = biosId;
            m_toInjectKVPairs[MdsColumnNames::PSHostId] = psHostId;
            m_toInjectKVPairs[MdsColumnNames::ComponentId] = componentName;
            m_removeMapKeyName = removeMapKeyName;
        }

    protected:
        virtual bool Convert(const std::string & fileToProcess, std::vector<std::string> & fileContent);
        virtual bool ProcessFile(const std::string & fileToProcess,
            std::string clientrequestId,
            std::string hostId,
            std::vector<std::string> & fileContent);

    private:
        SV_LOG_LEVEL m_agentTelemetryPostLevel;
        std::map<std::string, std::string> m_toInjectKVPairs;
        bool m_removeMapKeyName;
    };

    class V2ALoggerToMdsLogConverter : public InMageLoggerToMdsLogConverter, public boost::noncopyable
    {
    public:
        V2ALoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel, const std::string &subComponent) :
            InMageLoggerToMdsLogConverter(agentLogPostLevel, subComponent),
            m_configurator(nullptr)
        {
            BOOST_ASSERT(nullptr == m_configurator);

            try {
                BOOST_ASSERT(!GetConfigurator(&m_configurator));

                if (!::InitializeConfigurator(&m_configurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
                {
                    throw INMAGE_EX("Unable to instantiate configurator");
                }
            }
            GENERIC_CATCH_LOG_ACTION("V2ALoggerToMdsLogConverter: InitializeConfigurator", throw);
        }

        virtual ~V2ALoggerToMdsLogConverter()
        {}

    protected:
        Configurator* m_configurator; // TODO - nichougu - make m_configurator smart pointer
    };

    class IRSourceLoggerToMdsLogConverter : public V2ALoggerToMdsLogConverter
    {
    public:
        IRSourceLoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel,
            const std::string &subComponent,
            const std::string &logPath) :
            V2ALoggerToMdsLogConverter(agentLogPostLevel, subComponent)
        {
            // Extract extended properties for IR log from log file format,
            // *EndPointDeviceName = "C\Source Host ID\<device name>"
            //  <Agent log path>\vxlogs\completed\<device name>\<volume group ID>\<EndPointDeviceName>\dataprotection.log
            // Windows:
            //   MBR disk: "<Agent log path>\vxlogs\completed\{1619707344}\52\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection.log"
            //   GPT disk: "<Agent log path>\vxlogs\completed\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\52\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log"
            // Linux:
            //             "<Agent log path>/vxlogs/completed/dev/sda/44/C:\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda/dataprotection.log"

#define SLASH "(\\\\|/)+"

#define HOST_ID_LOWER   "([a-f]|[0-9]){8}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){12}"
//                       1                2                3                4                5

#define IR_SRC_LOG_FORMAT "(^.+)" SLASH "(\\d+)" SLASH "(c:?)" SLASH "(" HOST_ID_LOWER ")" SLASH "(.+)" SLASH "(dataprotection.log)$"
//                         0      0      0       0      0      0      0                    1      1     1      1
//                         1      2      3       4      5      6      7  +5                3      4     5      6

            const std::string resyncCompletedLogDir(Utils::GetDPCompletedIRLogFolderPath());
            std::string resyncMetaData = logPath.substr(resyncCompletedLogDir.length(), logPath.length() - resyncCompletedLogDir.length());

            boost::regex resyncSrcFilesRegex(IR_SRC_LOG_FORMAT);
            boost::algorithm::to_lower(resyncMetaData);
            boost::smatch matches;
            if (!boost::regex_search(resyncMetaData, matches, resyncSrcFilesRegex))
            {
                std::stringstream ss;
                ss << FUNCTION_NAME << " failed to extract extended properties from log path " << logPath << ".\n";
                throw INMAGE_EX(ss.str().c_str());
            }

            const std::string DeviceName = matches[1].str();
            const std::string VolumeGroupID = matches[3].str();
            std::string resyncJobID;

            // Extract JobId from settings
            // TODO: nichougu - 1902 - If IR/resync completed before next run of EvtCollForw,
            //   - Settings will not have info for this particular IR session.
            //   - Also it is possible that another IR triggered and settings has JobId for new IR session
            const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgItr = hostVolumeGroupSettings.volumeGroups.begin();
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgItrEnd = hostVolumeGroupSettings.volumeGroups.end();
            for (vgItr; vgItr != vgItrEnd; vgItr++)
            {
                VOLUME_GROUP_SETTINGS::volumes_constiterator volItr = vgItr->volumes.begin();
                if (volItr != vgItr->volumes.end())
                {
                    std::string vgid(boost::lexical_cast<std::string>(vgItr->id));
                    if (boost::iequals(vgid, VolumeGroupID))
                    {
                        resyncJobID = volItr->second.jobId;
                        break;
                    }
                }
            }

            m_extProperties.ExtendedProperties["DeviceName"] = DeviceName;
            m_extProperties.ExtendedProperties["Role"] = lc->getAgentRole();
            m_extProperties.ExtendedProperties["VolumeGroupID"] = VolumeGroupID;
            m_extProperties.ExtendedProperties["resyncJobID"] = resyncJobID;
        }

        virtual ~IRSourceLoggerToMdsLogConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        AgentLogExtendedProperties m_extProperties;
    };

    class IRRcmSourceLoggerToMdsLogConverter : public InMageLoggerToMdsLogConverter
    {
    public:
        IRRcmSourceLoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel,
            const std::string &subComponent,
            const std::string &logPath) :
            InMageLoggerToMdsLogConverter(agentLogPostLevel, subComponent)
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

            const std::string resyncCompletedLogDir(Utils::GetDPCompletedIRLogFolderPath());
            std::string resyncMetaData = logPath.substr(resyncCompletedLogDir.length(), logPath.length() - resyncCompletedLogDir.length());

            boost::regex resyncSrcFilesRegex(IR_SRC_LOG_FORMAT);
            boost::algorithm::to_lower(resyncMetaData);
            boost::smatch matches;
            if (!boost::regex_search(resyncMetaData, matches, resyncSrcFilesRegex))
            {
                std::stringstream ss;
                ss << FUNCTION_NAME << " failed to extract extended properties from log path " << logPath << ".\n";
                throw INMAGE_EX(ss.str().c_str());
            }

            m_extProperties.ExtendedProperties["DeviceName"] = matches[1].str();
        }

        virtual ~IRRcmSourceLoggerToMdsLogConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        AgentLogExtendedProperties m_extProperties;
    };

    class IRMTLoggerToMdsLogConverter : public V2ALoggerToMdsLogConverter
    {
    public:
        IRMTLoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel,
            const std::string &subComponent,
            const std::string &logPath) :
            V2ALoggerToMdsLogConverter(agentLogPostLevel, subComponent)
        {
            // Extract extended properties for IRMT log from log file format,
            // *DeviceName = "C\Source Host ID\<device name>"
            // <Agent log path>\vxlogs\completed\<DeviceName>\dataprotection.log
            // Windows source:
            //   MBR disk: "<Agent log path>\vxlogs\completed\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection.log";
            //   GPT disk: "<Agent log path>\vxlogs\completed\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log";
            // Linux source:
            //             "<Agent log path>\vxlogs\completed\C\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda\dataprotection.log";

#define SLASH "(\\\\|/)+"

#define HOST_ID_LOWER   "([a-f]|[0-9]){8}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){12}"
//                       1                2                3                4                5
#define IR_MT_LOG_FORMAT "(^c)" SLASH "(" HOST_ID_LOWER ")" SLASH "(.+)" SLASH "(dataprotection.log)$"
//                        0     0      0                    0      1     1      1
//                        1     2      3  +5                9      0     1      2

            BOOST_ASSERT(lc);
            const std::string resyncCompletedLogDir(Utils::GetDPCompletedIRLogFolderPath());
            std::string resyncMetaData = logPath.substr(resyncCompletedLogDir.length(), logPath.length() - resyncCompletedLogDir.length());

            boost::regex resyncMTFilesRegex(IR_MT_LOG_FORMAT);
            boost::algorithm::to_lower(resyncMetaData);
            boost::smatch matches;
            if (!boost::regex_search(resyncMetaData, matches, resyncMTFilesRegex))
            {
                std::stringstream ss;
                ss << FUNCTION_NAME << " failed to extract extended properties from log path " << logPath << ".\n";
                throw INMAGE_EX(ss.str().c_str());
            }

            const std::string SourceHostId = matches[3].str();
            const std::string DeviceName = matches[10].str();
            std::string VolumeGroupID;
            std::string resyncJobID;

            // Extract JobId from settings
            // TODO: nichougu - 1902 - If IR/resync completed before next run of EvtCollForw,
            //   - Settings will not have info for this particular IR session.
            //   - Also it is possible that another IR triggered and settings has JobId for new IR session
            const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgItr = hostVolumeGroupSettings.volumeGroups.begin();
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgItrEnd = hostVolumeGroupSettings.volumeGroups.end();
            for (vgItr; vgItr != vgItrEnd; vgItr++)
            {
                VOLUME_GROUP_SETTINGS::volumes_constiterator volItr = vgItr->volumes.begin();
                if (volItr != vgItr->volumes.end())
                {
                    if (boost::iequals(volItr->second.endpoints.begin()->deviceName, DeviceName)
                        && boost::iequals(volItr->second.endpoints.begin()->hostId, SourceHostId))
                    {
                        resyncJobID = volItr->second.jobId;
                        VolumeGroupID = boost::lexical_cast<std::string>(vgItr->id);
                        break;
                    }
                }
            }

            m_extProperties.ExtendedProperties["SourceHostId"] = SourceHostId;
            m_extProperties.ExtendedProperties["DeviceName"] = DeviceName;
            m_extProperties.ExtendedProperties["Role"] = lc->getAgentRole();
            m_extProperties.ExtendedProperties["VolumeGroupID"] = VolumeGroupID;
            m_extProperties.ExtendedProperties["resyncJobID"] = resyncJobID;
        }

        virtual ~IRMTLoggerToMdsLogConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        AgentLogExtendedProperties m_extProperties;

    };

    class IRRcmMTLoggerToMdsLogConverter : public InMageLoggerToMdsLogConverter
    {
    public:
        IRRcmMTLoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel,
            const std::string &subComponent,
            const std::string &logPath) :
            InMageLoggerToMdsLogConverter(agentLogPostLevel, subComponent)
        {
            // Extract extended properties for IRMT log from log file format,
            // *DeviceName = "Source Host ID\<device name>"
            // <Agent log path>\vxlogs\completed\<DeviceName>\dataprotection.log
            // Windows source:
            //   MBR disk: "<Agent log path>\vxlogs\completed\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection.log";
            //   GPT disk: "<Agent log path>\vxlogs\completed\a9ce1755-6f28-44b9-9525-c044b2686f74\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log";
            // Linux source:
            //             "<Agent log path>\vxlogs\completed\C\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda\dataprotection.log";

#define SLASH "(\\\\|/)+"

#define HOST_ID_LOWER   "([a-f]|[0-9]){8}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){4}-([a-f]|[0-9]){12}"
//                       1                2                3                4                5
#define IR_MT_LOG_FORMAT "(" HOST_ID_LOWER ")" SLASH "(.+)" SLASH "(dataprotection.log)$"
//                        0                    0      0     0      1
//                        1 +5                 7      8     9      0

            // Default expected path is E:\PSTelemetry\vxlogs\completed\<HostId>\<Diskid>\dataprotection.log

            std::map<std::string, std::string> telemetryPairs;
            Utils::GetPSSettingsMap(telemetryPairs);
            std::string resyncLogDirPath;
            bool rootPathFound = false;
            for (std::map<std::string, std::string>::iterator itr = telemetryPairs.begin(); itr != telemetryPairs.end(); itr++)
            {
                if (logPath.find(itr->first) != std::string::npos)
                {
                    rootPathFound = true;
                    resyncLogDirPath = Utils::GetRcmMTDPCompletedIRLogFolderPath(itr->second);
                    break;
                }
            }
            if (!rootPathFound)
            {
                throw std::runtime_error(std::string("unable to find settings for the path ") + logPath);
            }

            const std::string resyncCompletedLogDir = resyncLogDirPath;
            std::string resyncMetaData = logPath.substr(resyncCompletedLogDir.length(), logPath.length() - resyncCompletedLogDir.length());

            boost::regex resyncMTFilesRegex(IR_MT_LOG_FORMAT);
            boost::algorithm::to_lower(resyncMetaData);
            boost::smatch matches;
            if (!boost::regex_search(resyncMetaData, matches, resyncMTFilesRegex))
            {
                std::stringstream ss;
                ss << FUNCTION_NAME << " failed to extract extended properties from log path " << logPath << ".\n";
                throw INMAGE_EX(ss.str().c_str());
            }
            const std::string sourceHostId = matches[1].str();
            const std::string deviceName = matches[8].str();
            m_extProperties.ExtendedProperties["SourceHostId"] = sourceHostId;
            m_extProperties.ExtendedProperties["DeviceName"] = deviceName;
        }

        virtual ~IRRcmMTLoggerToMdsLogConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        AgentLogExtendedProperties m_extProperties;

    };

    class DRMTLoggerToMdsLogConverter : public V2ALoggerToMdsLogConverter
    {
    public:
        DRMTLoggerToMdsLogConverter(SV_LOG_LEVEL agentLogPostLevel,
            const std::string &subComponent,
            const std::string &logPath) :
            V2ALoggerToMdsLogConverter(agentLogPostLevel, subComponent)
        {
            // Extract extended properties for DR log from log file format,
            //  <Agent log path>\vxlogs\diffsync\<volume group ID>\dataprotection.log
            //  <Agent log path>\vxlogs\diffsync\48\dataprotection.log

#define SLASH "(\\\\|/)+"

#define DR_MT_LOG_FORMAT "(^\\d+)" SLASH "(dataprotection.log)$"
//                        0        0      0
//                        1        2      3

            BOOST_ASSERT(lc);
            std::string diffsyncLogDir(Utils::GetDPDiffsyncLogFolderPath());

            std::string diffsyncMetaData = logPath.substr(diffsyncLogDir.length(), logPath.length() - diffsyncLogDir.length());

            boost::regex diffsyncMTFilesRegex(DR_MT_LOG_FORMAT);
            boost::algorithm::to_lower(diffsyncMetaData);
            boost::smatch matches;
            if (!boost::regex_search(diffsyncMetaData, matches, diffsyncMTFilesRegex))
            {
                std::stringstream ss;
                ss << FUNCTION_NAME << " failed to extract extended properties from log path " << logPath << ".\n";
                throw INMAGE_EX(ss.str().c_str());
            }

            std::string DeviceName;
            std::string VolumeGroupID = matches[1].str();

            // Extract JobId from settings
            const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgItr = hostVolumeGroupSettings.volumeGroups.begin();
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgItrEnd = hostVolumeGroupSettings.volumeGroups.end();
            for (vgItr; vgItr != vgItrEnd; vgItr++)
            {
                VOLUME_GROUP_SETTINGS::volumes_constiterator volItr = vgItr->volumes.begin();
                if (volItr != vgItr->volumes.end())
                {
                    std::string vgid(boost::lexical_cast<std::string>(vgItr->id));
                    if (boost::iequals(vgid, VolumeGroupID))
                    {
                        DeviceName = volItr->second.endpoints.begin()->deviceName;
                        break;
                    }
                }
            }

            m_extProperties.ExtendedProperties["DeviceName"] = DeviceName;
            m_extProperties.ExtendedProperties["Role"] = lc->getAgentRole();
            m_extProperties.ExtendedProperties["VolumeGroupID"] = VolumeGroupID;
        }

        virtual ~DRMTLoggerToMdsLogConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        AgentLogExtendedProperties m_extProperties;

    };

    class InMageLoggerTelemetryToMdsConverter : public JsonKVPairMdsTelemetryConverterBase<std::string>
    {
    public:
        InMageLoggerTelemetryToMdsConverter(SV_LOG_LEVEL agentTelemetryPostLevel)
            : m_agentTelemetryPostLevel(agentTelemetryPostLevel)
        {
        }

        virtual ~InMageLoggerTelemetryToMdsConverter() {}

    protected:
        virtual bool OnConvert(const std::string &loggerMessage, std::map<std::string, std::string> &kvPairMap);

    private:
        SV_LOG_LEVEL m_agentTelemetryPostLevel;
    };

#ifdef SV_WINDOWS
    class EventLogToMdsLogConverter : public JsonKVPairMdsLogConverterBase<boost::shared_ptr<EventLogRecord>>
    {
    public:
        EventLogToMdsLogConverter()
            : m_isEventLogUploadEnabled(lc->isEvtCollForwEventLogUploadEnabled())
        {
        }

        virtual ~EventLogToMdsLogConverter() {}

        static void InitializeGdprFilteringInfo();

    protected:
        virtual bool OnConvert(const boost::shared_ptr<EventLogRecord> &evtRecord, std::map<std::string, std::string> &kvPairMap);

    private:
        static std::map<std::string, std::vector<uint32_t>> s_gdprAllowListedEventsMap;
        static std::set<std::string> s_gdprAllEventsAllowListedSubComponents;
        static std::set<std::string> s_gdprUnstoppableSubComponents;

        // The publisher metadata handles are opened once and stored for the
        // duration of the given iteration of the collection.
        EventLogPublisherHandleStore m_publisherHandleStore;
        bool m_isEventLogUploadEnabled;
    };

    class EventLogTelemetryToMdsConverter : public JsonKVPairMdsTelemetryConverterBase<std::map<std::string, std::string>>
    {
    public:
        virtual ~EventLogTelemetryToMdsConverter() {}

    protected:
        virtual bool OnConvert(const std::map<std::string, std::string> &propMap, std::map<std::string, std::string> &kvPairMap);
    };

#endif // SV_WINDOWS

#ifdef SV_UNIX
    class InVolFltLinuxTelemetryJsonMdsConverter : public JsonKVPairMdsTelemetryConverterBase<std::string>
    {
    public:
        virtual ~InVolFltLinuxTelemetryJsonMdsConverter() {}

        virtual bool OnConvert(const std::string &telemetrylogLine, std::map<std::string, std::string> &kvPairMap);
    };
#endif // SV_UNIX
}

#endif // !EVTCOLLFORW_CONVERTERS_H
