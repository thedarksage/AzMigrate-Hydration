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

    std::string cmd("getconf BOOT_DEVICE");

    std::stringstream results;

    std::string bootDisk;

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s executePipe failed with errno = %d\n", __LINE__, __FILE__, errno);
		return ;
    }

    results >> m_BootDisk;
}

