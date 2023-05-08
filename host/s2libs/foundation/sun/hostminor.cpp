#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "host.h"
#include "hostinfocollectormajor.h"
#include "hostinfocollectorminor.h"
#include "executecommand.h"

bool Host::SetMemory() 
{
    bool bset = false;

    long pagesize = sysconf(_SC_PAGESIZE);
    DebugPrintf(SV_LOG_DEBUG, "pagesize = %ld\n", pagesize);
    if (-1 != pagesize)
    {
        long nphysicalpages = sysconf(_SC_PHYS_PAGES);
        DebugPrintf(SV_LOG_DEBUG, "number of physical pages = %ld\n", nphysicalpages);
        if (-1 != nphysicalpages)
        {
            bset = true;
            /* TODO: this looks some what less than
             * what "prtconf | grep" Mem shows */
            m_Memory = pagesize * nphysicalpages;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "sysconf to find number of physical pages failed with error: %s\n",  
                        strerror(errno));
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "sysconf to find page size failed with error: %s\n",  
                    strerror(errno));
    }

    return bset;
}


bool Host::SetCpuInfos()
{
    bool bset = true;
    std::stringstream results;
    std::string cmd = "/usr/bin/kstat -m cpu_info";
    cmd += SPACE;
    cmd += PIPE_CHAR;
    cmd += SPACE;
    cmd += "egrep \'(";

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += NAME_TOK;
    cmd += OR_EXTREGEX;

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += WHITESPACE_REGEX;
    cmd += ZEROORMORE_REGEX;
    cmd += BRAND_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += OR_EXTREGEX;

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += WHITESPACE_REGEX;
    cmd += ZEROORMORE_REGEX;
    cmd += CHIP_ID_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += OR_EXTREGEX;

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += WHITESPACE_REGEX;
    cmd += ZEROORMORE_REGEX;
    cmd += CORE_ID_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += OR_EXTREGEX;

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += WHITESPACE_REGEX;
    cmd += ZEROORMORE_REGEX;
    cmd += CPU_TYPE_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += OR_EXTREGEX;

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += WHITESPACE_REGEX;
    cmd += ZEROORMORE_REGEX;
    cmd += IMPL_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += OR_EXTREGEX;

    cmd += STARTLINE_ANCHOR_REGEX;
    cmd += WHITESPACE_REGEX;
    cmd += ZEROORMORE_REGEX;
    cmd += VENDOR_ID_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;

    cmd += ")\'";
    DebugPrintf(SV_LOG_DEBUG, "cmd = %s\n", cmd.c_str());

    if (executePipe(cmd, results) && !results.str().empty())
    {
        /* DebugPrintf(SV_LOG_DEBUG, "output = %s\n", results.str().c_str()); */
        size_t currentStartPos = 0;
        size_t offset = 0;
        while ((std::string::npos != currentStartPos) && ((currentStartPos + offset) < results.str().length()))
        {
            /* TODO: use regex of boost to grep for brand[\t ]+ later */
            size_t nextStartPos = results.str().find(NAME_TOK, currentStartPos + offset);
            if (0 == offset)
            {
                offset = strlen(NAME_TOK);
            }
            else
            {
                size_t processorstreamlen = (std::string::npos == nextStartPos) ? std::string::npos : (nextStartPos - currentStartPos);
                std::string processorinfo = results.str().substr(currentStartPos, processorstreamlen);
                /* DebugPrintf(SV_LOG_DEBUG, "virtual processor info: %s\n", processorinfo.c_str()); */
                CollectCpuInfo(processorinfo, &m_CpuInfos);
            }
            currentStartPos = nextStartPos;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "getting processor information from proc did not succeed\n");
    }

    CleanCpuInfos(&m_CpuInfos);
    SetArchitecture(&m_CpuInfos);

    return bset;
}
