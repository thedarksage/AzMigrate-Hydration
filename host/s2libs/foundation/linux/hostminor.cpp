#include <cstring>
#include <fstream>
#include "host.h"
#include "hostinfocollectormajor.h"
#include "hostinfocollectorminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "executecommand.h"

bool Host::SetMemory() 
{
    bool bset = true;
    const char *memfile = "/proc/meminfo";

    std::ifstream in(memfile);

    if (!in.is_open())
    {
        bset = false;
        DebugPrintf(SV_LOG_ERROR, "Failed to open %s to find memory\n", memfile);
    }
    else
    {
        std::string memtotaltok;
        in >> memtotaltok >> m_Memory;
        /* checked free -m code and 
         * it also assumes memory in kB */
        m_Memory *= 1024;
        in.close();
    }

    return bset;
}

bool Host::GetFreeMemory(unsigned long long &freeMemory) const
{
    bool bset = true;
    const char *memfile = "/proc/meminfo";

    std::ifstream in(memfile);

    if (!in.is_open())
    {
        bset = false;
        DebugPrintf(SV_LOG_ERROR, "Failed to open %s to find memory\n", memfile);
    }
    else
    {
        unsigned long long totalmemory;
        std::string memtotaltok, kbtok;
        in >> memtotaltok >> totalmemory >> kbtok;
        in >> memtotaltok >> freeMemory >> kbtok;
        freeMemory *= 1024;
        in.close();
    }

    return bset;
}

bool Host::GetSystemUptimeInSec(unsigned long long& uptime) const
{
    bool bset = true;
    const char *uptimefile = "/proc/uptime";

    std::ifstream in(uptimefile);

    if (!in.is_open())
    {
        bset = false;
        printf("Failed to open %s to find uptime \n", uptimefile);
    }
    else
    {
        in >> uptime;
        in.close();
    }

    return bset;
}

bool Host::SetCpuInfos() 
{
    bool bset = true;
    std::stringstream results;
    std::string cmd = "egrep \'(";
    cmd += PROCESSOR_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += SEP;
    cmd += OR_EXTREGEX;
    cmd += VENDOR_ID_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += SEP;
    cmd += OR_EXTREGEX;
    cmd += MODELNAME_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += SEP;
    cmd += OR_EXTREGEX;
    cmd += PHYSICALID_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += SEP;
    cmd += OR_EXTREGEX;
    cmd += COREID_TOK;
    cmd += WHITESPACE_REGEX;
    cmd += ONEORMORE_REGEX;
    cmd += SEP;
    cmd += ")\'";
    cmd += SPACE;
    cmd += "/proc/cpuinfo";
    DebugPrintf(SV_LOG_DEBUG, "cmd = %s\n", cmd.c_str());

    if (executePipe(cmd, results) && !results.str().empty())
    {
        /* DebugPrintf(SV_LOG_DEBUG, "output = %s\n", results.str().c_str()); */
        size_t currentStartPos = 0;
        size_t offset = 0;
        while ((std::string::npos != currentStartPos) && ((currentStartPos + offset) < results.str().length()))
        {
            /* TODO: use regex of boost to grep for processor[\t ]+ later */
            size_t nextStartPos = results.str().find(PROCESSOR_TOK, currentStartPos + offset);
            if (0 == offset)
            {
                offset = strlen(PROCESSOR_TOK);
            }
            else
            {
                size_t processorstreamlen = (std::string::npos == nextStartPos) ? std::string::npos : (nextStartPos - currentStartPos);
                std::string processorinfo = results.str().substr(currentStartPos, processorstreamlen);
                /* DebugPrintf(SV_LOG_DEBUG, "processor info: %s\n", processorinfo.c_str()); */
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
