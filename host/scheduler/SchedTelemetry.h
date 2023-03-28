#ifndef SCHED_TELEMETRY_H
#define SCHED_TELEMETRY_H

#include <string>
#include <map>

#include "json_reader.h"
#include "json_writer.h"

#include "Telemetry.h"

using namespace AgentTelemetry;

namespace SchedTelemetry
{

    enum LAST_SCHEDULE_STATUS {
        AGENT_SHUTDOWN_BEFORE_COMPLETED = 1,
        SCHEDULE_COMPLETED
    };

    enum MessageTypes {
        SCHED_TABLE_AGENT_LOG = 1 + SOURCE_TELEMETRY_BASE_MESSAGE_TYPE,
    };

    const std::string POLICYID("PolicyId");
    const std::string CONFINTERVAL("ConfInterval");
    const std::string LASTSCHTIME("LastSchTime");
    const std::string CURRSCHTIMEEXP("CurrSchTimeExp");
    const std::string CURRTIME("CurrTime");
    const std::string NEXTSCHTIME("NextSchTime");
    const std::string LASTSCHSTATUS("LastSchStatus");

    class SchedStatus
    {
    public :
        std::map<std::string, std::string> Map;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SchedStatus", false);

            JSON_T(adapter, Map);
        }
    };

}

#endif
