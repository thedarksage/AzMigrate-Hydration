// DRDrillTestBed.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "Common.h"
using namespace DrDrillNS;

#include <winsock2.h>
#include "Exchange.h"
#include "Drdefs.h"

/********Function Prototype********/
void DRusage();


int main(int argc, char* argv[])
{
	bool bDrDrill= false;
	bool bSource = false;
    bool bTarget = false;
	bool bApplication = false;
  	bool bEvs = false;
	bool bFailover = false;
	bool bBackup = false;
	bool result = false;


	int iParsedOptions = 1;
	std::list<_tstring> supportFlagList;
	std::list<_tstring>::const_iterator supportFlagIter;
	_tstring szSourceHost = "";
	_tstring szAppName = "";
	_tstring szTargetHost = "";
	_tstring vServer = "";

	supportFlagList.push_back("-drdrill");
	supportFlagList.push_back("-failover");
	supportFlagList.push_back("-backup");
	supportFlagList.push_back("-s");
	supportFlagList.push_back("-t");
	supportFlagList.push_back("-virtualserver");
	supportFlagList.push_back("-app");
	
	//for gethostname()
	WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(1,1), &wsaData);
    if (err != 0) 
	{
		std::cout << "Could not find a usable WINSOCK DLL\n";
    }
	if(argc < 2)
	{
		std::cout << "\nPlease specify an operation\n\n";
		std::cout << "OPERATION : -drdrill	\n";
        DRusage();
		goto cleanup;
	}
	
	for (;iParsedOptions < argc; iParsedOptions++)
    {
		if(argv[iParsedOptions][0] == '-')
		{	
			bool found = false ;
			for(supportFlagIter = supportFlagList.begin();
				supportFlagIter != supportFlagList.end();supportFlagIter++)
			{
				if(strcmpi(argv[iParsedOptions],(*supportFlagIter).c_str()) == 0)
				{
					found = true;				
				}				
			}			
			if(found == false)
			{
				std::cout<<"\nUnsupported flag : "<<argv[iParsedOptions];
				DRusage();
				goto cleanup;	
			}
			if(strcmpi(argv[iParsedOptions],OPTION_DRDRILL) == 0)
			{
				bDrDrill = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_FAILOVER) == 0)
			{
				
				bFailover = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_BACKUP) == 0)
			{
				
				bBackup = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_APPLICATION) == 0)
			{
				
				iParsedOptions++;
				if(iParsedOptions >= argc)
				{
					std::cout << "\nMissing application name for the option: -app\n\n";
					DRusage();
					goto cleanup;
				}
				else if(argv[iParsedOptions][0] == '-')
				{
					std::cout << "\nMissing application name for the option: -app\n\n";
					DRusage();
					goto cleanup;
				}
				szAppName = argv[iParsedOptions];
				if( szAppName.empty() )
				{
					std::cout << "Empty application name supplied. Please provide a valid application name.\n";
					goto cleanup;
				}	
				else
					bApplication = true;
					
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_SOURCE_HOST) == 0)
			{
				
				iParsedOptions++;
				if(iParsedOptions >= argc) 
				{
					std::cout << "\nMissing source server name for the option: -s\n\n";
					DRusage();
					goto cleanup;
				}
				else if(argv[iParsedOptions][0] == '-')
				{
					std::cout << "\nMissing source server name for the option: -s\n\n";
					DRusage();
					goto cleanup;
				}
				szSourceHost = argv[iParsedOptions];
				if( szSourceHost.empty() )
				{
					std::cout << "Empty source server name supplied. Please provide a valid source server name.\n";
					goto cleanup;
				}
				else
					bSource  = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TARGET_HOST) == 0)
			{
				
				iParsedOptions++;
				if(iParsedOptions >= argc)
				{
					std::cout << "\nMissing target server name for the option: -t\n\n";
					DRusage();
					goto cleanup;
				}
				else if(argv[iParsedOptions][0] == '-')
				{
					std::cout << "\nMissing target server name for the option: -t\n\n";
					DRusage();
					goto cleanup;
				}

				szTargetHost = argv[iParsedOptions];
				if( szTargetHost.empty() )
				{
					std::cout << "Empty target server name supplied. Please provide a valid target server name.\n";
					goto cleanup;
				}
				else
					bTarget = true;				
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_VIRTUALSERVER) == 0)
			{
				
				iParsedOptions++;
				if(iParsedOptions >= argc)
				{
					std::cout << "\nMissing virtual server name for the option: -virtualserver\n\n";
					DRusage();
					goto cleanup;
				}
				else if(argv[iParsedOptions][0] == '-')
				{
					std::cout << "\nMissing virtual server name for the option: -virtualserver\n\n";
					DRusage();
					goto cleanup;
				}
				vServer = argv[iParsedOptions];
				if( vServer.empty() )
				{
					std::cout << "Empty virtual server name supplied. Please provide a valid virtual server name.\n";
					goto cleanup;
				}
				else
					bEvs = true;
			}			
		}//end of if()
	}//end of for(;;)
		
	if(bDrDrill)
	{
		if(bFailover)
		{
			if(bEvs)
			{
				if(argc < 11)
				{
					std::cout<<"\nPlease provide required/appropriate inputs to perform DrDrill\n\n";
					DRusage();
					goto cleanup;
				}

			}
			else
			{
				//check for standalone
				if(argc < 9)
				{
					std::cout<<"\nPlease provide required/appropriate inputs to perform DrDrill\n\n";
					DRusage();
					goto cleanup;
				}
				
			}
		}
		else if(bBackup)
		{
			//check for backup option
			std::cout<<"\n";
		}
		else
		{
			std::cout<<"\nPlease provide required/appropriate inputs to perform DrDrill\n\n";
			DRusage();
			goto cleanup;
		}

	}	
	
	if(bDrDrill)
	{
		if((strcmpi(szAppName.c_str(), EXCHANGE) != 0) || (strcmpi(szAppName.c_str(), EXCHANGE2007) != 0))
		{
			
			Exchange drExch(szSourceHost,szTargetHost,vServer,szAppName,bDrDrill);
			
			//Collect Data for Exchange DR-Drill
			result = drExch.CollectData();
			result = drExch.CollectAppData();
		}
		else
		{
			std::cout<<"\nUnsupported Application. Only Exchange is supported\n\n";
			goto cleanup;
		}

	}
	
	
  

  //changes
  //MsExchangeCollector exchangeCollector;
  //exchangeCollector.CollectExchData();
  /*ExchangeValidator
  MsSqlValidator
  MsExchangeCollector
  MsSqlCollector
  MsSqlReport
  ExchangeReport*/


cleanup:
	WSACleanup();
	if (!result)
    {		
        return 1;
    }

	return 0;
}


void DRusage()
{
	std::cout<<"\nUSAGE : ";
	std::cout<<"-drdrill -failover \n\t-s <sourceservername>\n\t-t <targetservername>\n\t-app <applicationname>\n\t[-virtualserver <evsname>]\n\n";
	std::cout<<"\nEXAMPLE [FOR STANDALONE]: DRDrillTestBed.exe -drdrill -failover -s Exchsource -t Exchtarget -app Exchange2007\n\n";
	std::cout<<"\nEXAMPLE [FOR CLUSTER]: DRDrillTestBed.exe -drdrill -failover -s ExchActivenode -t Exchtarget -app Exchange2007 -virtualserver Exchevs\n\n";
}
