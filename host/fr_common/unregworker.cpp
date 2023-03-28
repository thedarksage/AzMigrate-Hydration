#include <stdio.h>
#include <cstring>

#include "hostagenthelpers_ported.h"
#include "unregworker.h"
#include "../branding/branding_parameters.h"

///
/// Frees up an available CX license for this agent
///
SVERROR UnregisterHost(char const* pszServerName,
                       SV_INT HttpPort,
                       char const* pszUrl,
                       char const* pszHostId,bool https )
{
    /* place holder function */
	return SVS_OK;
}

SVERROR WorkOnUnregister(const char* pszHostType, const bool askuser)
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
		char ch;
		if( askuser )
		{
            printf("Unregister %s agent with %s ?[Y/N]: ", pszHostType,PRODUCT_LABEL);		
			GetConsoleYesOrNoInput(ch);
		}
		else
			ch = 'Y';		

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
                                    AgentParams.szSVHostID,AgentParams.Https);
				
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
