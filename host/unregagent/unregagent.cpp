// unregagent.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <cstring>

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#include "hostagenthelpers.h"
#include "devicefilter.h"
#include "device.h"
#include "devicestream.h"
#include "localconfigurator.h"
#endif

#include "svtypes.h"
#include "hostagenthelpers_ported.h"
#include "../branding/branding_parameters.h"
#include "portablehelpers.h"
#include "configurecxagent.h"
#include "configurator2.h"
#include "unregagent.h"
#include "inmageex.h"
#include <ace/Init_ACE.h>
#include "ace/ACE.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"
#include "inmsafecapis.h"
#include "logger.h"

const char* SV_ROOT_KEY = "SOFTWARE\\SV Systems";
using namespace std;

Configurator* TheConfigurator = 0; // singleton

void StopDriverFilter()
{	
#ifdef SV_WINDOWS
	//If filter driver is not present, nothing to do.
	LocalConfigurator lc;
	if (!lc.IsFilterDriverAvailable()) {
		DebugPrintf(SV_LOG_INFO, "Filter driver not present. Hence no clean up required for driver.\n");
		return;
	}

	// Do stop filtering for all protected devices. 
	DeviceStream *p = 0;
	DeviceStream volumeFilterStream(Device(INMAGE_FILTER_DEVICE_NAME));
	DeviceStream diskFilterStream(Device(INMAGE_DISK_FILTER_DOS_DEVICE_NAME));

	//If disk filter driver opens, issue stop filtering for it; else issue for volume driver if it opens
	if (SV_SUCCESS == diskFilterStream.Open(DeviceStream::Mode_Open | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW)) {
		DebugPrintf(SV_LOG_INFO, "Opened disk filter driver.\n");
		p = &diskFilterStream;
	}
	else if (SV_SUCCESS == volumeFilterStream.Open(DeviceStream::Mode_Open | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW)) {
		DebugPrintf(SV_LOG_INFO, "Opened volume filter driver.\n");
		p = &volumeFilterStream;
	}
	else {
		DebugPrintf(SV_LOG_ERROR, "Could not issue stop filtering to filter driver from unregistration agent. Failed to open disk filter driver %s with error %d and failed to open volume filter %s driver with error %d.\n",
			diskFilterStream.GetName().c_str(), diskFilterStream.GetErrorCode(), volumeFilterStream.GetName().c_str(), volumeFilterStream.GetErrorCode());
	}

	if (p) {
		//Prepare stop filtering input. set the STOP_ALL_FILTERING_FLAGS and STOP_FILTERING_FLAGS_DELETE_BITMAP
		STOP_FILTERING_INPUT sfi;
		memset(&sfi, 0, sizeof sfi);
		sfi.ulFlags |= STOP_ALL_FILTERING_FLAGS;
		sfi.ulFlags |= STOP_FILTERING_FLAGS_DELETE_BITMAP;

		//Issue ioctl
		if (SV_SUCCESS == p->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, &sfi, sizeof(sfi), NULL, 0)) {
			// FIX_LOG_LEVEL: note for now we are going to use SV_LOG_ERROR even on success so that we can
			// track this at the cx, eventually a new  SV_LOG_LEVEL will be added that is not an error 
			// but still allows this to be sent to the cx
			DebugPrintf(SV_LOG_ERROR, "NOTE: this is not an error: StopFiltering issued for all protected devices from unregistration agent\n");
		}
		else
			DebugPrintf(SV_LOG_ERROR, "FAILED : Stop filtering for all protected devices failed from unregistration agent with error %d\n", p->GetErrorCode());
	}
#endif
}

/*
* FUNCTION NAME :  InitializeConfigurator
*
* DESCRIPTION : initialize cofigurator to fetch initialsettings
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
            if(!InitializeConfigurator(&TheConfigurator, FETCH_SETTINGS_FROM_CX, PHPSerialize))
            {
                rv = false;
                break;
            }

            rv = true;
        }

        catch ( ContextualException& ce )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch( exception const& e )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, e.what());
        }
        catch ( ... )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered unknown exception.\n",
                FUNCTION_NAME);
        }

    } while(0);

    if(!rv)
    {
        DebugPrintf(SV_LOG_INFO, 
            "Replication pair information is not available.\n"
            "unregagent cannot run now. please try again\n");
    }

    return rv;
}

SVERROR WorkOnUnregister(const char* pszHostType, const bool askuser)
{
    SV_HOST_AGENT_PARAMS AgentParams;
    SV_HOST_AGENT_TYPE AgentType = SV_AGENT_UNKNOWN;
    SVERROR rc;
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
            printf("Unregister %s agent?[Y/N]: ", pszHostType);		
            GetConsoleYesOrNoInput(ch);
        }
        else
            ch = 'Y';		
        if((ch == 'Y') || (ch == 'y'))
        {
            //TODO:  Need to enforce 'rc' checks
            DebugPrintf(SV_LOG_INFO, "Unregistering %s agent.\n", pszHostType);
            if (SV_AGENT_SENTINEL == AgentType) {
                StopDriverFilter();					
            }

            if(AgentType != SV_AGENT_FILE_REPLICATION)
            {
				if(!InitializeConfigurator())
				{
					return SVE_FAIL;
				}
				TheConfigurator->Start();
                if(!GetConfigurator(&TheConfigurator))
                {
                    DebugPrintf(SV_LOG_ERROR, "Unable to obtain configurator %s %d\n", FUNCTION_NAME, LINE_NO);
                }
                else
                {
                    TheConfigurator->getCxAgent().UnregisterHost();
                }

            } 
            else
            {
                std::string strConfigFilePath;
                rc = GetConfigFileName(strConfigFilePath);
                if(rc == SVS_OK) 
                {

                    DebugPrintf(SV_LOG_INFO, "Unregistering %s agent.\n", pszHostType);              
                    rc = ReadAgentConfiguration(&AgentParams, strConfigFilePath.c_str(), AgentType);
                    rc = UnregisterHost(AgentParams.szSVServerName,
                        AgentParams.ServerHttpPort,
                        AgentParams.szUnregisterURL,
						AgentParams.szSVHostID,AgentParams.Https);


                }
            }
#ifdef SV_WINDOWS
            rc = DeleteRegistrySettings(AgentType);
#endif
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "Keeping %s agent settings on appliance.", pszHostType);
        }
    }
    else
    {
        printf("Unknown host agent type.");
    }
    return SVS_OK;  
}

/*
* FUNCTION NAME :  SetupLocalLogging
*
* DESCRIPTION : get the local logging settings from configurator
*               and enable local logging
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : true on success, false otherwise
*
*/
bool SetupLocalLogging()
{
    bool rv = false;

    try
    {
        LocalConfigurator localconfigurator;
        std::string logName = localconfigurator.getUnregisterAgentLogPath();
        
        SetLogFileName(logName.c_str());
        SetLogLevel(SV_LOG_ALWAYS);
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        SetDaemonMode();

        rv = true;
    }

    catch (ContextualException& ce)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch (exception const& e)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR,
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    if (!rv)
    {
        std::cerr << "Local Logging initialization failed.\n";
    }

    return rv;
}

//int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
int main(int argc, char *argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis(); 
	// PR#10815: Long Path support
	ACE::init();

    MaskRequiredSignals();

    if (!SetupLocalLogging())
    {
        return -1;
    }

    int nRetCode = 0;
    
    if(argc == 2)
    {
        try {         
            WorkOnUnregister(argv[1],1);
        }
        catch(...) {
            DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while unregistering host from CX.\n");
        }
    }
    else if(argc == 3)
    {
        try {         
            WorkOnUnregister(argv[1],0);
        }
        catch(...) {
            DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while unregistering host from CX.\n");
        }
    }    
    else
    {
        printf("Usage: unregagent [agent]\n");
        printf("where [agent] is 'sentinel', 'outpost' or 'filereplication'.");
    }

    CloseDebug();
    return nRetCode;
}


SVERROR UnregisterHost(char const* pszServerName,
                       SV_INT HttpPort,
                       char const* pszUrl,
                       char const* pszHostId,bool https )
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
        inm_sprintf_s(szUrl, ARRAYSIZE(szUrl), "%s?id=%s", pszUrl, pszHostId);

        DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
        DebugPrintf( "CALLING PostToSVServer()...\n" );
        char szBuffer[]= "";



        if( postToCx( pszServerName, HttpPort, szUrl, szBuffer, NULL,https ).failed())
        {
            rc = SVE_FAIL;
            DebugPrintf( "FAILED Couldn't unregister host\n" );
            break;
        }
    } while( false );

    return( rc );
}

