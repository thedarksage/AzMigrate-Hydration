//ScoutConfigurator.cpp -- defines entry point for ScoutConfiguration.
//It is implemented on basis of version and role.

//#pragma once
//#include "localconfigurator.h"
//#include "ScoutConfiguration.h"
#include "vContinuumConfig.h"
#include "version.h"
#include "logger.h"
#include "portable.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#ifdef WIN32
#include <windows.h>
#endif
using namespace std;

std::string VERSION			= "";
std::string ROLE			= "";

class CopyrightNotice
{ 
public:
    CopyrightNotice()
    {
        std::cout <<"\n"<< INMAGE_COPY_RIGHT <<"\n\n";
    }
};

void Usage()
{
	DebugPrintf( SV_LOG_INFO, "\nUsage of ScoutTuning :\n" );
	DebugPrintf( SV_LOG_INFO, "\nScoutTuning -version \"version of Agent\" -role \"[vconmt]\".\n" );
}

int main ( int argc , char* argv[] )
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	CopyrightNotice displayCopyrightNotice;

	bool bReturnValue		= SVS_OK;
	
	LocalConfigurator localConfigurator;
	std::string sLogFileName;
	try 
    {
		sLogFileName = localConfigurator.getLogPathname();
		
#ifdef WIN32
		sLogFileName = sLogFileName + "\\ScoutTuning.log";
#else
		sLogFileName = sLogFileName + "/ScoutTuning.log";
#endif
		DebugPrintf(SV_LOG_INFO, "Log file = %s.\n" , sLogFileName.c_str() );

        SetLogFileName(sLogFileName.c_str()) ;
	    SetLogLevel( 7 ) ;
		SetLogHttpIpAddress(GetCxIpAddress().c_str());
        SetLogHttpPort( localConfigurator.getHttp().port );
        SetLogHostId( localConfigurator.getHostId().c_str() );
		SetLogRemoteLogLevel( localConfigurator.getRemoteLogLevel() );
		SetSerializeType( localConfigurator.getSerializerType() ) ;
		SetLogHttpsOption(localConfigurator.IsHttps());
    }
    catch (...) 
    {
        DebugPrintf( SV_LOG_ERROR , "\n Log initialization failed.\n" );
		return SVS_FALSE;
    }

	if ( ( 5 > argc ) && ( 2 != argc ) )
	{
		DebugPrintf( SV_LOG_ERROR , "Insufficient Arguments.\n" );
		Usage();
		return SVS_FALSE;
	}

	for( int iOptionNumber = 1 ; iOptionNumber < argc ; ++iOptionNumber )
	{
		if ( 0 == strcmpi( OPT_VERSION , argv[iOptionNumber] ) )
		{
			iOptionNumber++;
			if( ( iOptionNumber >= argc ) || ( argv[iOptionNumber][0] == '-' ) )
            {   
				bReturnValue	= SVS_FALSE;
				DebugPrintf(SV_LOG_ERROR,"Invalid value for option : %s.\n" , OPT_VERSION );
                Usage();
				break;
            }
			VERSION	= argv[iOptionNumber];
			DebugPrintf( SV_LOG_INFO, "Version : %s.\n", VERSION.c_str() );
		}
		else if ( 0 == strcmpi( OPT_ROLE , argv[iOptionNumber] ) )
		{
			iOptionNumber++;
			if( ( iOptionNumber >= argc ) || ( argv[iOptionNumber][0] == '-' ) )
            {   
				bReturnValue	= SVS_FALSE;
				DebugPrintf(SV_LOG_ERROR,"Invalid value for option : %s.\n" , OPT_ROLE );
                Usage();
				break;
            }
			ROLE = argv[iOptionNumber];
			DebugPrintf( SV_LOG_INFO, "Role : %s.\n", ROLE.c_str() );
		}
		else if ( 0 == strcmpi( OPT_HELP , argv[iOptionNumber] ) )
		{
			bReturnValue	= SVS_FALSE;
			Usage();
			break;
		}
		else
		{
			bReturnValue	= SVS_FALSE;
			DebugPrintf( SV_LOG_ERROR , "Invalid option : %s.\n" , argv[iOptionNumber] );
			Usage();
			break;
		}
	}

	if ( SVS_FALSE == bReturnValue )
	{
		return SVS_FALSE;
	}

	
	
	if ( 0 == strcmpi( "vconmt" , ROLE.c_str() ) )
	{
#ifdef WIN32
		vContinuumConfig objvContinuum;
		objvContinuum.m_drscoutConfFilePath = localConfigurator.getConfigPathname();
		if ( SVS_OK != objvContinuum.RoleConfigChanges() )
		{
			DebugPrintf( SV_LOG_ERROR , "Failed to role changes in respective to %s.\n" , ROLE.c_str() );
			bReturnValue	= SVS_FALSE;
		}
#else
		//In Case of Linux Master target : set 'ReportFullDeviceNamesOnly' to 1.
		vContinuumConfig objvContinuum;
		objvContinuum.m_drscoutConfFilePath = localConfigurator.getConfigPathname();
		DebugPrintf( SV_LOG_INFO , "drScout conf file path = %s.\n", (objvContinuum.m_drscoutConfFilePath).c_str());
		if ( SVS_OK != objvContinuum.RoleConfigChanges() )
		{
			DebugPrintf( SV_LOG_ERROR , "Failed to role changes in respective to %s.\n" , ROLE.c_str() );
			bReturnValue	= SVS_FALSE;
		}
#endif
	}
	else
	{
		bReturnValue	= SVS_FALSE;
		DebugPrintf( SV_LOG_ERROR , "Invalid Role Passed. Please choose a role listed from Usage content below.\n");
		Usage();
	}

	return bReturnValue;
}
