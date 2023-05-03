// EsxUtilWin.cpp : Defines the entry point for the console application.
//

#include <ace/Init_ACE.h>

#include "stdafx.h"
#include "Common.h"
#include <ace/OS_main.h> 
#include "logger.h"
#include "cdputil.h"
#include "inmageex.h"
#include "configwrapper.h"
#include "SourceVm.h"
#include "HandleMasterTarget.h"
#include "boost/algorithm/string.hpp"
#include <Security.h> // for getusernameex api
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#define OPTION_ROLE 				"-role"
#define OPTION_OPERATION			"-op"
#define OPTION_SOURCE				"source"
#define OPTION_TARGET				"target"
#define OPTION_FAILBACK 			"failback"
#define OPTION_APPLYTAG 			"applytag"
#define OPTION_SNAPSHOT 			"physnapshot"
#define OPTION_TGTHOSTNAME			"-t"
#define OPTION_TAGNAME				"-tag"
#define OPTION_CRASHCONSISTENCY 	"-crashconsistency"
#define OPTION_P2V                  "-p2v"
#define OPTION_HELP 				"-help"
#define OPTION_DEL_FILES            "-deletefiles"
#define OPTION_LOG_DIRECTOY 		"-d"
#define OPTION_CX_PATH              "-cxpath"
#define OPTION_MBR 					"-mbr"
#define OPTION_PARTITION			"-partition"
#define OPTION_TEST 				"-test"
#define OPTION_CX_PLANID            "-planid"
#define OPTION_CLOUD_ENV			"-cloudenv"
#define OPTION_CONFIG_FILE			"-configfile"
#define OPTION_RESULT_FILE			"-resultfile"

Configurator* TheConfigurator = 0;
bool bConfigStatus = false;

bool InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
			if (!InitializeConfigurator(&TheConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                rv = false;
                break;
            }

            TheConfigurator->Start();
			bConfigStatus = true;
            rv = true;
        }

        catch ( ContextualException& ce )
        {
            rv = false;
            cout << " encountered exception. " << ce.what() << endl;
        }
        catch( exception const& e )
        {
            rv = false;
            cout << " encountered exception. " << e.what() << endl;
        }
        catch ( ... )
        {
            rv = false;
            //cout << FUNCTION_NAME << " encountered unknown exception." << endl;
			cout << "encountered unknown exception." << endl;
        }

    } while(0);

    if(!rv)
    {
        cout << "[WARNING]: Configurator initialization failed. Verify if Vx agent service is running..." << endl;
		bConfigStatus = false;
    }

    return rv;
}

// to get current process Id and command line arguments which we have passed and domain name\username  
void GetCmdLineargsandProcessId(void)
{
    ULONG ulProcessId = 0;
    // To print the current command & arguments     
    cout <<"\nCommand Line: "<< GetCommandLine() << endl;

    //The user's security access manager (SAM) name in the format "DOMAIN\USER".

    LPVOID szUserName;
    DWORD  dwNameLen;
    BOOLEAN bRetVal;

    dwNameLen = MAX_PATH;
    szUserName = LocalAlloc(LPTR, dwNameLen);

    bRetVal = true;

    bRetVal = GetUserNameEx(NameDnsDomain,(LPTSTR)szUserName,&dwNameLen);

    if( ERROR_MORE_DATA == bRetVal )
    {
        LocalFree(szUserName);
        szUserName = LocalAlloc(LPTR, dwNameLen);
        bRetVal = GetUserNameEx(NameDnsDomain, (LPTSTR)szUserName, &dwNameLen);		 
    }

    if( bRetVal )
    {
        cout <<"Running under the user: "<< (LPTSTR)szUserName << endl;
        LocalFree(szUserName);
    }
    else
    { 
        //cout << "[ERROR][" << GetLastError() << "] GetUserNameEx failed.\n";
        bRetVal = GetUserName((LPTSTR)szUserName, &dwNameLen);
        if( bRetVal )
        {
            cout <<"Running under the user: "<< (LPTSTR)szUserName << endl;
            LocalFree(szUserName);
        } 
        else
        {
            cout << "[ERROR][" << GetLastError() << "] GetUserName failed.\n";
        }

    }

    int result = 0;
    char MachineName[512];
    result = gethostname(MachineName,sizeof(MachineName));
    if ( result != 0)
    {
        cout << "ERROR: Could not get the host name of this machine. Error Code = " << result << std::endl;
        // return SVE_FAIL;
    }
    else
        cout<<"Local Machine Name is : "<<MachineName<<endl;


    //Get the current process ID    
    ulProcessId = GetCurrentProcessId();
    cout<< "Process ID: "<< ulProcessId <<endl<<endl;
}

void Usage()
{
    DebugPrintf(SV_LOG_INFO,"\nUsage:\n"
							"EsxUtilWin -role <option [operation]>\n"
							"\n"
							"Options can be \n"
							"\tsource   : To discover the information of source machine\n"
							"\ttarget   : To protect the source machines during failover\n"
							"\tfailback : To protect the source machines during failback\n"
							"\tapplytag : To generate the common consistency point for all\n" 
							"\t           protected volumes on source machine\n"
							"\n"
							"Operations for option source\n"
							"\t-p2v           : If source machine is physical, add this argument\n"
							"\t-deletefiles   : To delete all the files which were generated from\n" 
							"\t                 previous discovery\n"
							"\n"
							"Operations for option target\n"
							"\t-d		: log directory has to be sent by vCon wizard to generate the logs in that directoy [Optional]"
							"\n"
							"Operations for option applytag\n"
							"\t-t <Target Host Name>  : Provide host name of the target onto which\n" 
							"\t                         the replication pairs are set\n"
							"\t-tag <Tag Name>        : Provide tag name\n"
							"\t-crashconsistency      : To generate a crash consistent common tag\n"
							"\t-cloudenv			  : specify the target clould environment name. Ex: Azure\n"
							"\n"
							"To display this info\n"
							"\tEsxUtilWin -help\n"
							"\n"
							"Examples :\n"
							"\n"
							"To discover info on a virtual machine\n"
							"\tEsxUtilWin -role source -deletefiles\n"
							"\n"
							"To discover info on a physical machine\n"
							"\tEsxUtilWin -role source -p2v -deletefiles\n"
							"\n"
							"To protect the source machines on master target during failover\n"
							"\tEsxUtilWin -role target [-d <log directory path>] [-cxpath <cx directory>] [-planid <CX Plan id>] [-op <operation>]\n"
							"\n"
							"To protect the source machines on master target during failback\n"
							"\tEsxUtilWin -role failback [-d <log directory path>] [-cxpath <cx directory>] [-planid <CX Plan id>] [-op <operation>]\n"
							"\n"
							"To generate a consistency point on source machine\n"
							"\tEsxUtilWin -role applytag -t <Source Hostname> -tag <Tag Name>\n"
							"\n"
							"To created required files for physical snapshot\n"
							"\tEsxUtilWin -role physnapshot [-d <log directory path>] [-cxpath <cx directory>] [-planid <CX Plan id>] [-op <operation>]\n"
							"\n"
							"To get the disk and MBR information from input mbr_data file\n"
							"\tEsxUtilWin -mbr <Mbr file with path>\n"
							"\n"
							"To get the disk and corresponding partition information from input partition binary file\n"
							"\tEsxUtilWin -partition <partition file with path>\n"

							"Just for testing , won't work for every case\n"
							"\tEsxUtilWin -test <partition file with path>\n"
							"\n");
}

int main(int argc,char *argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    CopyrightNotice displayCopyrightNotice;

	bool bRoleEsx = false;
	bool isSourceEsx = false;
	bool isTargetEsx = false;
    bool isFailbackEsx = false;
    bool isApplyTag = false;
	bool isPhySnapshot = false;
    bool bCrashConsistency = false;
    bool bP2V = false;
    bool bDeleteFiles = false;
	bool bMbr = false;
	bool bPartition = false;
	bool bTest = false;
	bool bCloudMT = false;
	string strRole = "";
    string strTgtHostName = "";
    string strTagName = "";
	string strLogDir = "";
	string strMbrFile = "";
	string strCxPath = "";
	string strPlanId = "1";
	string strOperation = "";
	string strCloudEnv = "";
	string strConfFile = "";
	string strResultFile = "";

	LocalConfigurator m_localConfigurator;
	 // PR#10815: Long Path support
	ACE::init();
    
	int iResult = 0;
	
    //Display processID,Commandline and user
    GetCmdLineargsandProcessId();
	
	WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);	
    if (iResult != NO_ERROR) {
		cout << "WSAStartup failed with error code : " << iResult;
		return 1;
	}

    //initilizing cdp init quit event
	bool callerCli = true;
	CDPUtil::InitQuitEvent(callerCli);
	if(!InitializeConfigurator())
	{
		//Configurator initialization failed. Existing.
		return EXIT_FAILURE;
	}

	//Validating the arguments
	if(argc <= 1)
	{
		DebugPrintf(SV_LOG_ERROR,"Invalid number of  arguments detected.\n");
        Usage();
        return EXIT_FAILURE;
	}

	int iParsedOptions = 1;
	for (; iParsedOptions < argc; iParsedOptions++)
	{
		if(argv[iParsedOptions][0] == '-')
        {
			if(strcmpi(argv[iParsedOptions],OPTION_ROLE) == 0)
			{
				DebugPrintf(SV_LOG_INFO, "choosen option: -role\n");
				bRoleEsx = true;
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing parameter for Role.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strRole = argv[iParsedOptions];

				//Parsing the Role
				if(strcmpi(strRole.c_str(),OPTION_SOURCE) == 0)
				{
					DebugPrintf(SV_LOG_INFO, "Selected Role: Source\n");
					isSourceEsx = true;
				}
				else if(strcmpi(strRole.c_str(),OPTION_TARGET) == 0)
				{
					DebugPrintf(SV_LOG_INFO, "Selected Role: Target\n");
					isTargetEsx = true;
					isFailbackEsx = false;
					strOperation = "Protection";
				}
				else if(strcmpi(strRole.c_str(),OPTION_FAILBACK) == 0)
				{
					DebugPrintf(SV_LOG_INFO, "Selected Role: Failback\n");
					isTargetEsx = true;
					isFailbackEsx = true;
					strOperation = "Failback";
				}
				else if(strcmpi(strRole.c_str(),OPTION_APPLYTAG) == 0)
				{
					DebugPrintf(SV_LOG_INFO, "Selected Role: ApplyTag\n");
					isApplyTag = true;
				}
				else if(strcmpi(strRole.c_str(), OPTION_SNAPSHOT) == 0)
				{
					DebugPrintf(SV_LOG_INFO, "Selected Role: PhysicalSnapshot\n");
					isPhySnapshot = true;
					strOperation = "Drdrill";
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Invalid parameter for Role.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_OPERATION) == 0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing Opeartion name.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strOperation = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -op %s\n", strOperation.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TGTHOSTNAME) == 0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the Target Hostname.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strTgtHostName = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -t %s\n", strTgtHostName.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TAGNAME) == 0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the Tag Name.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strTagName = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -tag %s\n", strTagName.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_CRASHCONSISTENCY) == 0)
			{
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -crashconsistency");
				bCrashConsistency = true;
			}
            else if(strcmpi(argv[iParsedOptions],OPTION_P2V) == 0)
			{
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -p2v\n");
				bP2V = true;
                DebugPrintf(SV_LOG_INFO,"Source is Physical Machine.\n");
			}
            else if(strcmpi(argv[iParsedOptions],OPTION_DEL_FILES) == 0)
			{
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -deletefiles\n");
				bDeleteFiles = true;                
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_LOG_DIRECTOY) == 0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the Log directory path.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strLogDir = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -d %s\n", strLogDir.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_CX_PATH) == 0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the CX path.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strCxPath = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -cxpath %s\n", strCxPath.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_CX_PLANID) == 0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the Plan id.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strPlanId = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -planid %s\n", strPlanId.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_MBR) == 0)
			{
				bMbr = true;
				bRoleEsx = true;
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the mbr_data path.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strMbrFile = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -mbr %s\n", strMbrFile.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_PARTITION) == 0)
			{
				bPartition = true;
				bRoleEsx = true;
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the mbr_data path.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strMbrFile = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -partition %s\n", strMbrFile.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TEST) == 0)
			{
				bTest = true;
				bRoleEsx = true;
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the mbr_data path.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strMbrFile = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -partition %s\n", strMbrFile.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_CLOUD_ENV) ==0)
			{
				iParsedOptions++;
				bCloudMT = true;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the -cloudenv parameter value.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strCloudEnv = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -cloudenv %s\n", strCloudEnv.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_CONFIG_FILE) ==0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the -configfile parameter value.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strConfFile = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -configfile %s\n", strConfFile.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_RESULT_FILE) ==0)
			{
				iParsedOptions++;
				if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
				{				
					DebugPrintf(SV_LOG_ERROR,"Missing the -resultfile parameter value.\n\n");
					Usage();
					return EXIT_FAILURE;
				}
				strResultFile = argv[iParsedOptions];
				DebugPrintf(SV_LOG_INFO, "Choosen Option: -resultfile %s\n", strResultFile.c_str());
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_HELP) == 0)
	        {
		        Usage();
		        return EXIT_SUCCESS;
	        }
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Invalid argument detected.\n\n");
				Usage();
				return EXIT_FAILURE;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Invalid argument detected.\n\n");
			Usage();
			return EXIT_FAILURE;
		}
	}

	if(! bRoleEsx)
	{
		 Usage();
		 return EXIT_FAILURE;
	}

	if((isApplyTag) && (strTgtHostName.empty()))
	{
		DebugPrintf(SV_LOG_ERROR,"Missing the Target Hostname for applying Tag.\n\n");
		Usage();
		return EXIT_FAILURE;
	}

	if(bCloudMT && (strConfFile.empty() || strResultFile.empty()))
	{
		DebugPrintf(SV_LOG_INFO,
			"Usage:\n\t%s target [physnapshot] -cloudenv Azure -configfile <config-file-path> -resultfile <result-file-path>\n%s\n",
			argv[0],
			"Note: If its a physicalsnapshot based recovery then include \"physnapshot\" switch");
		return EXIT_FAILURE;
	}

	if(!strLogDir.empty() && FALSE == CreateDirectory(strLogDir.c_str(),NULL))
	{
		if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
		{
			DebugPrintf(SV_LOG_INFO,"Directory already exists. Considering it not as an Error.\n");            				
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu]\n",strLogDir.c_str(),GetLastError());
		}
	}

	MasterTarget objMT;
	//Assign Secure permission to the directory
    if(!strLogDir.empty())
	    objMT.AssignSecureFilePermission(strLogDir, true);

	std::string sLogFileName;
	try 
	{		
		if(strLogDir.empty())
			sLogFileName = m_localConfigurator.getLogPathname()+ "\\Failover\\Data\\EsxUtilWin.log";
		else
		{
			strLogDir = strLogDir + "\\";
			sLogFileName = strLogDir + "EsxUtilWin.log";
		}
		SetLogHttpsOption(m_localConfigurator.IsHttps());
        SetLogFileName(sLogFileName.c_str()) ;
	    SetLogLevel( 7 ) ;
        SetLogHttpIpAddress(GetCxIpAddress().c_str());
        SetLogHttpPort( m_localConfigurator.getHttp().port );
        SetLogHostId( m_localConfigurator.getHostId().c_str() );
	}
	catch (...) 
    {
        std::cerr << "Local Logging initialization failed.\n";
	}

	//Assign Secure permission to the log file
	objMT.AssignSecureFilePermission(sLogFileName);

	//setting the string to use http (or) https
	string strHttps = string("http://");
	if(m_localConfigurator.IsHttps())
		strHttps = string("https://");

    //Calling the appropriate function.
	if(isApplyTag)
	{		
        Source obj;
        if(false == obj.GenerateTagForSrcVol(strTgtHostName,strTagName,bCrashConsistency,TheConfigurator))
        {
            iResult = 1;
        }
        goto cleanup;
	}
	else if(isSourceEsx)
	{
        Source obj(bP2V, bDeleteFiles);
		if(1 == obj.initSource())
		{
			iResult = 1;
			goto cleanup;
		}
        else
        {
            DebugPrintf(SV_LOG_INFO,"\n************************************************************************\n");
            DebugPrintf(SV_LOG_INFO,"\n      DISCOVERED THE COMPLETE INFORMATION OF THE HOST SUCCESSFULLY      \n");
            DebugPrintf(SV_LOG_INFO,"\n************************************************************************\n");
            iResult = 0;
            goto cleanup;
        }
	}
	else if(isTargetEsx || isPhySnapshot)
	{
		string strHostID = m_localConfigurator.getHostId();
		if (strHostID.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to fetch the HOST ID for this machine.\n");
			iResult = 1;
			goto cleanup;
		}

        MasterTarget obj(isFailbackEsx,strHostID, isPhySnapshot, strLogDir, strCxPath, strPlanId, strOperation);
		obj.m_strLogFile = sLogFileName;
		obj.m_strCmdHttp = strHttps;

		if(bCloudMT)
		{
			obj.SetCloudEnv(bCloudMT,strCloudEnv);
			if (strCloudEnv.compare("azure") == 0){
				iResult = obj.PrepareTargetDisks(strConfFile, strResultFile);
			}
			else {
				iResult = obj.PrepareTargetDisksForFailback(strConfFile, strResultFile);
			}
		}
		else if(1 == obj.initTarget())
		{
			iResult = 1;
			goto cleanup;
		}
	}
	else if(bMbr)
	{
		if(1 == objMT.GetDiskAndMbrDetails(strMbrFile))
		{
			iResult = 1;
			goto cleanup;
		}
	}
	else if(bPartition)
	{
		if(1 == objMT.GetDiskAndPartitionDetails(strMbrFile))
		{
			iResult = 1;
			goto cleanup;
		}
	}
	else if(bTest)
	{
		if(1 == objMT.UpdateDiskAndPartitionDetails(strMbrFile))
		{
			iResult = 1;
			goto cleanup;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"NO ROLE IS SELECTED.\n");
        iResult = 1;
        goto cleanup;
	}

cleanup:
	WSACleanup();
	//invoking unint quit event
	CDPUtil::UnInitQuitEvent();
	//need to add stop local logging 
	CloseDebug();

	//stoppinfg the configurator
	if(bConfigStatus)
	{
		TheConfigurator->Stop();
		TheConfigurator = NULL;
	}

	return iResult;
}

