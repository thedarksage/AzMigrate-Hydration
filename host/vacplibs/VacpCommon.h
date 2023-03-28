#ifndef VACP_COMMON_H
#define VACP_COMMON_H

#include <iostream>
#include <string>
#include <sstream>
#include <map>

#include <boost/thread.hpp>

static const std::string ConsistencyTypeApp = "AppConsistency";
static const std::string ConsistencyTypeCrash = "CrashConsistency";
static const std::string ConsistencyTypeBaseline = "BaselineConsistency";

enum TAG_TYPE
{
    INVALID,
    CRASH,
    APP,
    BASELINE
};

#define IS_CRASH_TAG(TagType) (CRASH == TagType)
#define IS_APP_TAG(TagType) (APP == TagType)
#define IS_BASELINE_TAG(TagType) (BASELINE == TagType)

typedef std::map<std::string, std::string> TagTelInfoMap_t;

class TagTelemetryInfo
{
public:

    std::string Get() const
    {
        std::stringstream ss;
        std::map<std::string, std::string>::const_iterator itr = m_tagTelInfoMap.begin();
        for (/*empty*/; itr != m_tagTelInfoMap.end(); itr++)
        {
            ss << itr->first.c_str() << " " << itr->second.c_str() << " " << "\n";
        }

        return ss.str();
    }

    const TagTelInfoMap_t & GetMap() const
    {
        return m_tagTelInfoMap;
    }

    void Add(const std::string &key, const std::string &val)
    {
        boost::mutex::scoped_lock guard(m_mutex);
        m_tagTelInfoMap[key] = val;
    }

    void Add(const TagTelInfoMap_t& other)
    {
        boost::mutex::scoped_lock guard(m_mutex);
        m_tagTelInfoMap.insert(other.begin(), other.end());
    }

    void Clear()
    {
        boost::mutex::scoped_lock guard(m_mutex);
        m_tagTelInfoMap.clear();
    }

private:
    TagTelInfoMap_t m_tagTelInfoMap;
    boost::mutex m_mutex;

};

#endif