#ifndef _SHOEBOX_EVENT_SETTINGS_H
#define _SHOEBOX_EVENT_SETTINGS_H

#include<string>
#include<map>
#include<vector>
#include"json_adapter.h"

class MonitoringStats{
public:
    std::string time;
    std::string resourceId;
    std::string category;
    std::string level;
    std::string operationName;
    std::string properties;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "MonitoringStats", false);
        JSON_E(adapter, time);
        JSON_E(adapter, resourceId);
        JSON_E(adapter, category);
        JSON_E(adapter, level);
        JSON_E(adapter, operationName);
        JSON_T(adapter, properties);
    }
};

class CollectiveMonitoringStats{
public:
    std::vector<MonitoringStats> CollectiveStats;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "CollectiveMonitoringStats", false);

        JSON_T(adapter, CollectiveStats);
    }
};

class  StatsPropertyMap
{
public:

    std::map<std::string, std::string> Map;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "StatsPropertyMap", false);

        JSON_T(adapter, Map);
    }
};
#endif