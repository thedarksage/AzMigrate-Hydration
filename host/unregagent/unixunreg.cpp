#include <stdio.h>
#include "inmsafecapis.h"
#include "../log/logger.h"
#include "../config/svconfig.h"
#include "../common/hostagenthelpers_ported.h"
#include "../branding/branding_parameters.h"

char szLogFileName[1024];
char szConfigFileName[1024];

///
/// Frees up an available CX license for this agent
///
SVERROR UnregisterHost(char const* pszServerName,
                       SV_INT HttpPort,
                       char const* pszUrl,
                       char const* pszHostId )
{
    SVERROR rc;

    do
    {
        if( NULL == pszServerName || NULL == pszUrl || NULL == pszHostId )
        {
            rc = SVE_INVALIDARG;
            break;
        }

        char szUrl[ SV_INTERNET_MAX_URL_LENGTH ];
        inm_sprintf_s( szUrl, ARRAYSIZE( szUrl ), "%s?id=%s", pszUrl, pszHostId );

        DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
        DebugPrintf( "CALLING PostToSVServer()...\n" );
        char szBuffer[]= "";
        if( PostToSVServer( pszServerName, HttpPort, szUrl, szBuffer, strlen( szBuffer ) ).failed() )
        {
            rc = SVE_FAIL;
            DebugPrintf( "FAILED Couldn't unregister host\n" );
            break;
        }
    } while( false );

    return( rc );
}

SVERROR WorkOnUnregister(const char* pszHostType)
{
    SV_HOST_AGENT_PARAMS AgentParams;
    SV_HOST_AGENT_TYPE AgentType = SV_AGENT_UNKNOWN;
    SVERROR rc;
    char configFilePath[256];
    memset(configFilePath,0,256);
    if(!strcmp(pszHostType, "Vx"))
    {
        AgentType = SV_AGENT_SENTINEL;
    }
    //else if(!strcmp(pszHostType, "outpost"))
    //{
    //    AgentType = SV_AGENT_OUTPOST;
    //}
    else if(!strcmp(pszHostType, "filereplication"))
    {
        AgentType = SV_AGENT_FILE_REPLICATION;
    }
    
    if(AgentType != SV_AGENT_UNKNOWN)
    {
        printf("Unregister %s agent with %s ?[Y/N]: ", pszHostType,PRODUCT_LABEL);
		char ch;
        GetConsoleYesOrNoInput(ch);
        if((ch == 'Y') || (ch == 'y'))
        {
            rc = GetConfigFileName(configFilePath);
            if(rc == SVS_OK) 
            {
                //TODO:  Need to enforce 'rc' checks
                printf("Unregistering %s agent with %s .\n", pszHostType,PRODUCT_LABEL);              
                rc = ReadAgentConfiguration(&AgentParams,configFilePath,AgentType);
#ifdef SV_WINDOWS
				if (SV_AGENT_SENTINEL == AgentType) {
					StopDriverFilter();					
				}
#endif
                rc = UnregisterHost(AgentParams.szSVServerName,
                                    AgentParams.ServerHttpPort,
                                    AgentParams.szUnregisterURL,
                                    AgentParams.szSVHostID);
				
#ifdef SV_WINDOWS
				rc = DeleteRegistrySettings(AgentType);
#endif
            }
            else
            {
                printf("Cannot get config file name\n");
            }
        }
        else
        {
            printf("Keeping %s agent settings on Transband appliance.", pszHostType);
        }
#ifdef SV_WINDOWS
		rc = DeleteStaleRegistrySettings(AgentType);
#endif

    }
    else
    {
        printf("Unknown host agent type.");
    }
    return SVS_OK;  
}

void GetConsoleYesOrNoInput(char & ch)
{
	ch = getchar();
	while((ch != 'y') && (ch != 'n') && (ch != 'Y') && (ch != 'N'))
	{
		fflush(stdin);
		printf("Press y/Y/N/n....\n");
		ch = getchar();
	}
}


int main(int argc, char* argv[])
{
    if(argc != 4)
    {
        printf("Usage: unregagent <agent> <log file name> <config file name><\n");
	printf("where [agent] is 'sentinel', 'outpost' or 'filereplication'.");
        return 1;
    }
    else
    {
         memset(szLogFileName,0,1024);
         memset(szConfigFileName,0,1024);
         inm_strcpy_s(szLogFileName, ARRAYSIZE(szLogFileName), argv[2]);
         szLogFileName[1023] = '\0';
         inm_strcpy_s(szConfigFileName, ARRAYSIZE(szConfigFileName), argv[3]);
         szConfigFileName[1023] = '\0';
         DebugPrintf( SV_LOG_DEBUG,"Log file name = %s\n",szLogFileName );
         DebugPrintf( SV_LOG_DEBUG,"Config file name = %s\n",szConfigFileName );
         SVConfig::GetInstance()->SetConfigFileName(szConfigFileName);
         SetLogFileName(szLogFileName);
         WorkOnUnregister(argv[1]);
    }
    return 0;
}
