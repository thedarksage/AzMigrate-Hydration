#ifndef VACP_CONFIG__H
#define VACP_CONFIG__H    

#include <stdlib.h>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string/trim.hpp>
#include <boost/shared_ptr.hpp>

class VacpConfig;
typedef boost::shared_ptr<VacpConfig> VacpConfigPtr;
static boost::mutex confMutex;

class VacpConfigParser
{
public:
    std::map<std::string, std::string> parseSharedDiskHostIdMappings(std::string value)
    {
        std::map<std::string, std::string> nameIdMap;

        if (!value.empty())
        {
            std::vector < std::string > nameIdPairs;
            std::string nameIdPair;
            std::string hostid;
            std::string hostname;

            boost::split(nameIdPairs, value, boost::is_any_of(";"));
            std::vector < std::string >::const_iterator itr = nameIdPairs.begin();
            for (/**/; itr != nameIdPairs.end(); itr++)
            {
                nameIdPair = *itr;
                if (nameIdPair.empty())
                    continue;
                std::string::size_type idx = nameIdPair.find_first_of(":");
                if (std::string::npos != idx)
                {
                    std::string hostname(nameIdPair.substr(0, idx));
                    boost::algorithm::trim(hostname);
                    std::string hostid(nameIdPair.substr(idx + 1));
                    boost::algorithm::trim(hostid);
                    if (hostname.empty() || hostid.empty())
                        continue;
                    nameIdMap.insert(std::make_pair(hostname, hostid));
                }

            }
        }

        return nameIdMap;
    }
};

class VacpConfig
{
public:

    static VacpConfigPtr getInstance()
    {
        if (m_pVacpConfig.get() == NULL)
        {
            boost::unique_lock<boost::mutex> lock(confMutex);
            if (m_pVacpConfig.get() == NULL)
            {
                m_pVacpConfig.reset(new VacpConfig);
            }
        }

        return m_pVacpConfig;
    }

    void Init(std::string path)
    {
        boost::unique_lock<boost::mutex> lock(confMutex);

        if (m_bInitialized)
            return;

        std::ifstream confFile(path.c_str());

        std::string line;
        while (confFile.good())
        {
            std::getline(confFile, line);

            if ('#' != line[0])
            {
                std::string data(line);
                std::string::size_type idx = data.find_first_of("=");
                if (std::string::npos != idx)
                {
                    std::string param(data.substr(0, idx));
                    boost::algorithm::trim(param);
                    std::string value(data.substr(idx + 1));
                    boost::algorithm::trim(value);

                    m_mapVacpConfParam.insert(std::make_pair(param, value));
                }
            }
        }

        confFile.close();
        m_bInitialized = true;

        return;
    }

    bool getParamValue(std::string param, long &value)
    {
        std::map<std::string, std::string>::iterator iter = m_mapVacpConfParam.find(param);
        if (iter != m_mapVacpConfParam.end())
        {
            value = atoi((*iter).second.c_str());
            return true;
        }
        else
            return false;
    }

    bool getParamValue(std::string param, std::string &value)
    {
        std::map<std::string, std::string>::iterator iter = m_mapVacpConfParam.find(param);
        if (iter != m_mapVacpConfParam.end())
        {
            value = (*iter).second;
            return true;
        }
        else
            return false;
    }

    bool getCustomPrasedConfParamValues(std::string param, std::map<std::string, std::string>& value)
    {
        std::map<std::string, std::map <std::string, std::string> >::const_iterator iter =
            m_mapVacpCustomParsedConfParam.find(param);
        if (iter != m_mapVacpCustomParsedConfParam.end())
        {
            value = (*iter).second;
            return true;
        }
        else
            return false;
    }

    bool initCustomParsedConfParams(std::string param,
        boost::function<std::map<std::string, std::string> (std::string)> parseFunc)
    {
        if (m_mapVacpCustomParsedConfParam.find(param) != m_mapVacpCustomParsedConfParam.end())
        {
            return true;
        }
        boost::unique_lock<boost::mutex> lock(confMutex);

        std::string value;
        bool isParamPresent = getParamValue(param, value);

        if (!isParamPresent)
            return false;

        std::map<std::string, std::string> subFields = parseFunc(value);
        m_mapVacpCustomParsedConfParam.insert(std::pair<std::string, std::map<std::string, std::string> >(param, subFields));
        return true;
    }

    std::map<std::string, std::string>& getParams()
    {
       return m_mapVacpConfParam; 
    }

    std::map<std::string, std::map <std::string, std::string> >& getCustomParsedParams()
    {
        return m_mapVacpCustomParsedConfParam;
    }

private:
    VacpConfig()
        :m_bInitialized(false)
    {}

    static VacpConfigPtr            m_pVacpConfig;
    bool                            m_bInitialized;
    std::map<std::string, std::string>     m_mapVacpConfParam;
    std::map<std::string, std::map <std::string, std::string> > m_mapVacpCustomParsedConfParam;
 };

#endif
