#include <stdio.h>
#include "../fr_log/logger.h"
#include "../fr_config/svconfig.h"
#include "../fr_common/hostagenthelpers_ported.h"
#include "../fr_common/unregworker.h"
#include "inmsafecapis.h"

char szLogFileName[1024];
char szConfigFileName[1024];

int main(int argc, char* argv[])
{
    //calling init funtion of inmsafec library
	init_inm_safe_c_apis();

	if(argc != 4)
    {
        printf("Usage: unregagent <agent> <log file name> <config file name><\n");
	printf("where [agent] is 'Vx' or 'filereplication'.");
        return 1;
    }
    else
    {
         memset(szLogFileName,0,1024);
         memset(szConfigFileName,0,1024);
         inm_strncpy_s(szLogFileName,ARRAYSIZE(szLogFileName),argv[2],1023);
         szLogFileName[1023] = '\0';
         inm_strncpy_s(szConfigFileName,ARRAYSIZE(szConfigFileName),argv[3],1023);
         szConfigFileName[1023] = '\0';
         DebugPrintf( SV_LOG_DEBUG,"Log file name = %s\n",szLogFileName );
         DebugPrintf( SV_LOG_DEBUG,"Config file name = %s\n",szConfigFileName );
         SVConfig::GetInstance()->SetConfigFileName(szConfigFileName);
         SetLogFileName(szLogFileName);	 
         WorkOnUnregister(argv[1], 1); //ask user opinion 
    }
    return 0;
}
