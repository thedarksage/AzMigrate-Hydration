#include <string>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "hostinfocollectormajor.h"
#include "hostinfocollectorminor.h"

void CollectCpuInfo(const std::string &cpuinfo, CpuInfos_t *pcpuinfos)
{
    std::stringstream ss(cpuinfo);
    std::string tok, sep;
    std::string procid, mfr, name, physid, coreid;

    /*
    * processor       : 0
    * vendor_id       : GenuineIntel
    * model name      : Intel(R) Xeon(R) CPU           X3210  @ 2.13GHz
    * physical id     : 0
    * core id         : 0
    */

    /* processor */
    ss >> tok >> sep;
    std::getline(ss, procid);
    Trim(procid, " \t");

    /* vendor_id */
    ss >> tok >> sep;
    std::getline(ss, mfr);
    Trim(mfr, " \t");

    /* model name */
    ss >> tok >> tok >> sep;
    std::getline(ss, name);
    Trim(name, " \t");

    /* physical id */
    ss >> tok >> tok >> sep;
    std::getline(ss, physid);
    Trim(physid, " \t");

    /* core id */
    ss >> tok >> tok >> sep;
    std::getline(ss, coreid);
    Trim(coreid, " \t");

    /* 
    DebugPrintf(SV_LOG_DEBUG, "procid = %s\n", procid.c_str());
    DebugPrintf(SV_LOG_DEBUG, "mfr = %s\n", mfr.c_str());
    DebugPrintf(SV_LOG_DEBUG, "name = %s\n", name.c_str());
    DebugPrintf(SV_LOG_DEBUG, "physid = %s\n", physid.c_str());
    DebugPrintf(SV_LOG_DEBUG, "coreid = %s\n", coreid.c_str());
    */
    if (physid.empty())
    {
        /* use the processor number as the cpu id */
        std::pair<CpuInfosIter_t, bool> pr = pcpuinfos->insert(std::make_pair(procid, Object()));
        CpuInfosIter_t nit = pr.first;
        Object &o = nit->second;
        o.m_Attributes.insert(std::make_pair(NSCpuInfo::NAME, name));
        o.m_Attributes.insert(std::make_pair(NSCpuInfo::MANUFACTURER, mfr));
        o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_CORES, "1"));
        o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_LOGICAL_PROCESSORS, "1"));
    }
    else
    {
        /* use the physical id as the cpu id */
        CpuInfosIter_t it = pcpuinfos->find(physid);
        if (it != pcpuinfos->end())
        {
            Object &o = it->second;
            AttributesIter_t ait = o.m_Attributes.find(NSCpuInfo::NUM_LOGICAL_PROCESSORS);
            if (ait != o.m_Attributes.end())
            {
                std::string &numlogicalprocessor = ait->second;
                unsigned int n = boost::lexical_cast<unsigned int>(numlogicalprocessor);
                n++;
                numlogicalprocessor = boost::lexical_cast<std::string>(n);
            }
            UpdateNumberOfCores(coreid, o);
        }
        else
        {
            std::pair<CpuInfosIter_t, bool> pr = pcpuinfos->insert(std::make_pair(physid, Object()));
            CpuInfosIter_t nit = pr.first;
            Object &o = nit->second;
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::NAME, name));
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::MANUFACTURER, mfr));
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_CORES, "1"));
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_LOGICAL_PROCESSORS, "1"));
            std::string scoreid;
            scoreid += SEP;
            scoreid += coreid;
            scoreid += SEP;
            o.m_Attributes.insert(std::make_pair(CORE_IDS, scoreid));
        }
    }
}


