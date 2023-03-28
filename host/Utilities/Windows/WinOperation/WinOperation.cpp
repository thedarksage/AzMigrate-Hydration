// WinOperation.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\WinOpLib\NetBios.h"
#include "..\WinOpLib\Spn.h"
#include "..\WinOpLib\ActiveDirectory.h"
#include "..\WinOpLib\System.h"
#include "..\WinOpLib\InMageSecurity.h"
#include "..\WinOpLib\ClusOp.h"
#include "..\WinOpLib\MapDriveLetter.h"
#include "defs.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

using namespace std;

#ifdef _CLUSUTIL_LOG
	#define printf LogAndPrint
#endif
void usage();

int _tmain(int argc, char* argv[])
{
	TerminateOnHeapCorruption();
	init_inm_safe_c_apis();
	CopyrightNotice displayCopyrightNotice;

	int ret = 0;
	
	if(argc == 1)
	{
		usage();
		exit(1);
	}

	if(stricmp((argv[1]),OPERATION_NETBIOS) ==0)
	{
		ret = NetBiosMain(argc,argv);
	}
	else if(stricmp((argv[1]), OPERATION_SPN) ==0)
	{
		ret = SpnMain(argc, argv);
	}
	else if(stricmp((argv[1]), OPERATION_ACTIVE_DIRECTORY) ==0)
	{
		ret = ActiveDirectoryMain(argc, argv);
	}
	else if(stricmp((argv[1]), OPERATION_SYSTEM) ==0)
	{
		ret = SystemMain(argc, argv);
	}
	else if(stricmp((argv[1]), OPERATION_SECURITY) ==0)
	{
		ret = SecurityMain(argc, argv);
	}
	else if(stricmp((argv[1]), OPERATION_CLUSTER) ==0)
	{
		ret = ClusOpMain(argc, argv);
	}
	else if(stricmp((argv[1]),OPERATION_MAP_DRIVE_LETTER)== 0)
	{
		ret = MapDriveLetterMain(argc,argv);
	}
	else
	{
		printf("Invalid operation name specified. please see the list of supported operation in usage.\n");
		usage();
	}

    return ret;
}

void usage()
{
	printf("\nUsage:\n");
	printf("Winop.exe  <OPERATIONNAME>\n");
	printf("\nOPERATIONNAME      This can be any of listed below\n");
	printf("	NETBIOS\n");
	printf("	SPN\n");
	printf("	AD\n");
	printf("	SYSTEM\n");
	printf("	SECURITY\n");
	printf("	CLUSTER\n");
	printf("	MAPDRIVELETTER\n");

	printf("\nFor each operation's help type /h or /? after OPERATIONNAME.\n");
	printf("   Example: WinOp.exe NETBIOS /h \n");
}
	
