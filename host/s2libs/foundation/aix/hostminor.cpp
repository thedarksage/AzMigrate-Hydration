#include <string>
#include <sstream>
#include <unistd.h>
#include "host.h"
#include "hostinfocollectormajor.h"
#include "hostinfocollectorminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "executecommand.h"

bool Host::SetMemory() 
{
    bool bset = false;

    long memkb = sysconf(_SC_AIX_REALMEM);
    DebugPrintf(SV_LOG_DEBUG, "memory in kB %ld\n", memkb);
    if (-1 != memkb)
    {
        bset = true;
        m_Memory = memkb*1024;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "sysconf to find real aix memory failed with error: %s\n",  
                    strerror(errno));
    }

    return bset;
}


bool Host::SetCpuInfos()
{
    bool bset = true;
    /* TODO: what abould paths of below commands ? */
    std::string cmd = "lsdev -Cc processor | grep Available | awk '{ print $1 }'";
    std::stringstream results;
    
    if (executePipe(cmd, results) && !results.str().empty())
    {
         std::string proc;
         while (!results.eof())
         {
             proc.clear();
             results >> proc; 
             if (!proc.empty())
             {
                 DebugPrintf(SV_LOG_DEBUG, "collecting information for processor %s\n", proc.c_str());
                 CollectCpuInfo(proc, &m_CpuInfos);
             }
         }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "getting processor information from lsdev did not succeed\n");
    }

    SetArchitecture(&m_CpuInfos);
    return bset;
}
