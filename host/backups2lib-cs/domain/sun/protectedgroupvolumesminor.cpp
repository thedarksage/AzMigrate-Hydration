#include "runnable.h"
#include "protectedgroupvolumes.h"

int ProtectedGroupVolumes::ProcessLastIOATLUNRequest(void)
{
    /* This ioctl is not for solaris; our PS is linux */
    int iStatus = SV_FAILURE;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME, GetSourceVolumeName().c_str());
    return iStatus;
}

