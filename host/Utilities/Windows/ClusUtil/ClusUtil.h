#ifndef _CLUSUTIL_H
#define _CLUSUTIL_H


#define OPTION_SHUTDOWN					"shutdown"
#define OPTION_PREPARE					"prepare"
#define OPTION_FORCE					"force"
#define OPTION_NO_ACTIVE_NODE_REBOOT    "noActiveNodeReboot"
#define OPTION_NO_CX_DB_CLEANUP			"noCxDbCleanup"

#include "version.h"

class CopyrightNotice
{	
public:
	CopyrightNotice()
	{
		std::cout<<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
	}
};


DWORD ParsePrepareArgs(char* pPrepareArgs,int& iPrepareType, char** ppSystemName);
DWORD RemoveRegKey(char* szSubKey,char* pSystemTobeRestarted);
DWORD CX_CleanDb(char* pListOfSystems);
void ClusUtil_usage();

#endif //_CLUSUTIL_H