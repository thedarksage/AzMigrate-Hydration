
#ifndef _DEFS_H
#define _DEFS_H

#include "version.h"

#define OPERATION_NETBIOS			"NetBios"
#define OPERATION_SPN				"Spn"
#define OPERATION_ACTIVE_DIRECTORY	"AD"
#define OPERATION_SYSTEM			"SYSTEM"
#define OPERATION_SECURITY			"SECURITY"
#define OPERATION_CLUSTER			"Cluster"
#define OPERATION_MAP_DRIVE_LETTER  "MapDriveLetter"

//#define PRINT_COPYRIGHT_NOTICE		printf("InMage Scout [Version 5.5]\nCopyright (c) 2003 InMage Systems Inc. All rights reserved.\n\n")
class CopyrightNotice
{	
public:
	CopyrightNotice()
	{
		std::cout<<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
	}
};

#endif //_DEFS_H