#include "host.h"
#include "hostinfocollectormajor.h"
#include "hostinfocollectorminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "executecommand.h"

void CollectCpuInfo(const std::string &cpu, CpuInfos_t *pcpuinfos)
{
    /*
    * bash-3.2# lsattr -El proc0
    * frequency   1898100000     Processor Speed       False
    * smt_enabled true           Processor SMT enabled False
    * smt_threads 2              Processor SMT threads False
    * state       enable         Processor state       False
    * type        PowerPC_POWER5 Processor type        False
    */
    std::string cmd = "lsattr -El ";
    cmd += cpu;
    std::stringstream results;
    
    if (executePipe(cmd, results) && !results.str().empty())
    {
         const std::string SMT_EN_TOK = "smt_enabled";
         const std::string SM_THR_TOK = "smt_threads";
         const std::string TYPE_TOK = "type";
         std::string smten, smtth, type;

         std::string tok, val;
         while (!results.eof())
         {
             tok.clear();
             val.clear();
             results >> tok >> val;
             if (SMT_EN_TOK == tok)
             {
                 smten = val;
             }
             else if (SM_THR_TOK == tok)
             {
                 smtth = val;
             }
             else if (TYPE_TOK == tok)
             {
                 type = val;
             }
             std::getline(results, tok);
         }
         DebugPrintf("For %s, smt_enabled = %s, smt_threads = %s, type = %s\n", cpu.c_str(),
                     smten.c_str(), smtth.c_str(), type.c_str());
         std::pair<CpuInfosIter_t, bool> pr = pcpuinfos->insert(std::make_pair(cpu, Object()));
         CpuInfosIter_t nit = pr.first;
         Object &o = nit->second;

         if (!type.empty())
         {
             o.m_Attributes.insert(std::make_pair(NSCpuInfo::NAME, type));
             o.m_Attributes.insert(std::make_pair(NSCpuInfo::MANUFACTURER, type));

             /* AIX does not at all understand physical processor;
              * hence all information is of cores here instead of processor;
              * Hence not sending the core information 
             o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_CORES, "1")); */
         }

         const char *nlogicalprocessors = "1";
         if (("true" == smten) && !smtth.empty())
         {
             nlogicalprocessors = smtth.c_str();
         }
         o.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_LOGICAL_PROCESSORS, nlogicalprocessors));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "getting processor information for %s did not succeed\n", cpu.c_str());
    }
}

