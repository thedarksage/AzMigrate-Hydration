#include "vContinuumConfig.h"
#include "logger.h"
#include "portable.h"

#ifdef WIN32
#include <windows.h>
#endif


using namespace std;

extern std::string VERSION;
extern std::string ROLE;

int vContinuumConfig::RoleConfigChanges()
{
	DebugPrintf( SV_LOG_DEBUG , "Entering fucntion %s.\n" , __FUNCTION__ );
	bool bReturnValue	= SVS_OK;
	
#ifdef WIN32
	do 
	{
		setRegisterLayoutOnDisks(false);
	}while(0);
#else

	//Linux
	do 
	{
		//setReportFullDeviceNames(true);
		setDPCacheVolumeHandle(false);
	}while(0);

#endif

	DebugPrintf( SV_LOG_DEBUG , "Exiting fucntion %s.\n" , __FUNCTION__ );
	return bReturnValue;
}
