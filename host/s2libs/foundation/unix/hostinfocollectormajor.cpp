#include <string>
#include <sstream>
#include <algorithm>
#include <utility>
#include <boost/lexical_cast.hpp>
#include "portablehelpers.h"
#include "logger.h"
#include "volumegroupsettings.h"
#include "hostinfocollectorminor.h"
#include "hostinfocollectormajor.h"

bool SetArchitecture(CpuInfos_t *pcpuinfos)
{
    std::string output, err;
    int ecode = 0;

    bool bset = RunInmCommand(PROCESSORCMD, output, err, ecode);
    if (bset)
    {
        Trim(output, "\n \t");
        for (CpuInfosIter_t it = pcpuinfos->begin(); it != pcpuinfos->end(); it++)
        {
            Object &o = it->second;
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::ARCHITECTURE, output));
        }
    }
    else
    {
        std::stringstream ssecode;
        ssecode << ecode;
        std::string errormsg = "getting processor command failed with exit code ";
        errormsg += ssecode.str();
        DebugPrintf(SV_LOG_DEBUG, "%s\n", errormsg.c_str());
    }

    return bset;
}


void UpdateNumberOfCores(const std::string &coreid, Object &o)
{
    AttributesIter_t ait = o.m_Attributes.find(CORE_IDS);
    if (ait != o.m_Attributes.end())
    {
        std::string scoreid;
        scoreid += SEP;
        scoreid += coreid;
        scoreid += SEP;
        std::string &coreids = ait->second;
        if (std::string::npos == coreids.find(scoreid))
        {
            /* not found; new core; increment core count */
            coreids += scoreid;
            AttributesIter_t nit = o.m_Attributes.find(NSCpuInfo::NUM_CORES);
            if (nit != o.m_Attributes.end())
            {
                std::string &numcores = nit->second;
                unsigned int n = boost::lexical_cast<unsigned int>(numcores);
                n++;
                numcores = boost::lexical_cast<std::string>(n);
            }
        }
        else
        {
            /* found; do nothing */
        }
    }
}


void CleanCpuInfos(CpuInfos_t *pcpuinfos)
{
    for (CpuInfosIter_t it = pcpuinfos->begin(); it != pcpuinfos->end(); it++)
    {
        Object &o = it->second;
        o.m_Attributes.erase(CORE_IDS);
    }
}
