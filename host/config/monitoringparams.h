#ifndef MONITORING_PARAMS_H
#define MONITORING_PARAMS_H

#include "json_reader.h"
#include "json_writer.h"

#define MoitoringParamsFileName "MoitoringParams.json"

class MoitoringParams
{
public:
    std::string LastRebootTime;
    std::string AgentVersion;
    std::string OsVersion;
    std::string OsNameForWhichAgentIsInstalled;

public:
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "MoitoringParams", false);
        JSON_E(adapter, LastRebootTime);
        JSON_E(adapter, AgentVersion);
        JSON_E(adapter, OsVersion);
        JSON_T(adapter, OsNameForWhichAgentIsInstalled);
    }

    void serialize(ptree& node)
    {
        JSON_P(node, LastRebootTime);
        JSON_P(node, AgentVersion);
        JSON_P(node, OsVersion);
        JSON_P(node, OsNameForWhichAgentIsInstalled);
    }
};

#endif //END MONITORING_PARAMS_H