#include <cstring>
#include <string>
#include <list>
#include <sstream>
#include "runnable.h"
#include "volumegroupsettings.h"
#include "protectedgroupvolumes.h"
#include "cxmediator.h"
#include "devicestream.h"
#include "inmsafecapis.h"

extern Configurator* TheConfigurator;

int ProtectedGroupVolumes::ProcessLastIOATLUNRequest(void)
{
    int iStatus = SV_FAILURE;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    throw std::runtime_error("Not implemented");

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME, GetSourceVolumeName().c_str());
    return iStatus;
}

