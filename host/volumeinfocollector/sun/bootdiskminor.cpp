#include <string>
#include <sstream>
#include "bootdisk.h"
#include "executecommand.h"
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"

void BootDiskInfo::KnowBootDisk(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    /*
     * bash-2.05# /usr/sbin/prtconf -pv | /usr/bin/grep bootpath | tr -d "'" | /usr/bin/awk '{print $2}' | sed 's/disk@/sd@/g'
     * /pci@1f,4000/scsi@3/sd@0,0:a
    */
    std::string cmd("/usr/sbin/prtconf -pv | /usr/bin/grep bootpath | /usr/bin/tr -d \"'\" | /usr/bin/awk '{print $2}' | /usr/bin/sed 's/disk@/sd@/g'");

    std::stringstream results;

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s executePipe failed with errno = %d\n", __LINE__, __FILE__, errno);
        return;
    }

    results >> m_BootDisk;
}

