#ifndef _RCM_TEST_CLIENT_ARGS_H
#define _RCM_TEST_CLIENT_ARGS_H

#include "json_writer.h"
#include "json_reader.h"

using namespace std;

class CmdOptions
{
public:
    std::string m_option;
    std::string m_input;
    std::string m_settingsFilepath;
    std::string m_outputFilepath;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "CmdOptions", false);

        JSON_E(adapter, m_option);
        JSON_E(adapter, m_input);
        JSON_E(adapter, m_settingsFilepath);
        JSON_T(adapter, m_outputFilepath);
    }
};


class CreateJobsInput
{
public:
    std::vector<std::string> m_disks;
    std::string m_sasUri;
    std::string m_hostname;
    uint64_t m_appConInterval;
    uint64_t m_crashConInterval;
    uint64_t m_sasUriExpiryTime;
    uint64_t m_irTime;
    uint64_t m_drTime;


    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "CreateJobsInput", false);

        JSON_E(adapter, m_disks);
        JSON_E(adapter, m_sasUri);
        JSON_E(adapter, m_hostname);
        JSON_E(adapter, m_appConInterval);
        JSON_E(adapter, m_crashConInterval);
        JSON_E(adapter, m_sasUriExpiryTime);
        JSON_E(adapter, m_irTime);
        JSON_T(adapter, m_drTime);
    }
};


#endif
