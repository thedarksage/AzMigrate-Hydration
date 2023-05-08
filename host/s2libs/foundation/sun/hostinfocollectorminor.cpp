#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include "host.h"
#include "hostinfocollectormajor.h"
#include "hostinfocollectorminor.h"


void CollectCpuInfo(const std::string &cpuinfo, CpuInfos_t *pcpuinfos)
{
    /*
	    brand                           Intel(r) Xeon(r) CPU           X3210  @ 2.13GHz
        chip_id                         0
        core_id                         0
        cpu_type                        i386
        implementation                  x86 (GenuineIntel family 6 model 15 step 11 clock 2133 MHz)
        vendor_id                       GenuineIntel
    */

    std::string brand, chip_id, core_id, cpu_type, implementation, vendor_id;
    std::stringstream ss(cpuinfo);
    std::string tok, value;

    /* skip the name tok */
    std::getline(ss, tok);

    while (!ss.eof())
    {
        tok.clear();
        value.clear();

        ss >> tok;
        std::getline(ss, value);

        if (tok == BRAND_TOK)
        {
            brand = value;
            Trim(brand, " \t");
        }
        else if (tok == CHIP_ID_TOK)
        {
            chip_id = value;
            Trim(chip_id, " \t");
        }
        else if (tok == CORE_ID_TOK)
        {
            core_id = value;
            Trim(core_id, " \t");
        }
        else if (tok == CPU_TYPE_TOK)
        {
            cpu_type = value;
            Trim(cpu_type, " \t");
        }
        else if (tok == IMPL_TOK)
        {
            implementation = value;
            Trim(implementation, " \t");
        }
        else if (tok == VENDOR_ID_TOK)
        {
            vendor_id = value;
            Trim(vendor_id, " \t");
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "brand = %s, chip_id = %s, core_id = %s, cpu_type = %s, "
                              "implementation = %s, vendor_id = %s\n", 
                              brand.c_str(), chip_id.c_str(), core_id.c_str(), 
                              cpu_type.c_str(), implementation.c_str(),
                              vendor_id.c_str());

    if (chip_id.empty())
    {
        return;
    }

    CpuInfosIter_t it = pcpuinfos->find(chip_id);
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
        if (!core_id.empty())
        {
            UpdateNumberOfCores(core_id, o);
        }
    }
    else
    {
        std::pair<CpuInfosIter_t, bool> pr = pcpuinfos->insert(std::make_pair(chip_id, Object()));
        CpuInfosIter_t nit = pr.first;
        Object &o = nit->second;

        if (!brand.empty())
        {
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::NAME, brand));
        }
        else if (!cpu_type.empty())
        {
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::NAME, cpu_type));
        }

        if (!vendor_id.empty())
        {
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::MANUFACTURER, vendor_id));
        }
        else if (!implementation.empty())
        { 
            o.m_Attributes.insert(std::make_pair(NSCpuInfo::MANUFACTURER, implementation));
        }
      
        o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_CORES, "1"));
        o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_LOGICAL_PROCESSORS, "1"));

        if (!core_id.empty())
        {
            std::string scoreid;
            scoreid += SEP;
            scoreid += core_id;
            scoreid += SEP;
            o.m_Attributes.insert(std::make_pair(CORE_IDS, scoreid));
        }
    }
}


