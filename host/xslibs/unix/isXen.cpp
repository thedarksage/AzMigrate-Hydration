#include "isXen.h"

#include <cstdio>
#include <string>

using namespace std;

bool isXen()
{
	DebugPrintf(SV_LOG_INFO, "ENTERED isXen\n");

	FILE *fp = NULL;
	bool ret=false;
	fp = fopen(RELEASE_FILE, "r");
	if( fp != NULL )
	{
		size_t readbytes;
		char buf[10];
		readbytes = fread(buf, 1, 9, fp);
		buf[readbytes] = '\0';

		if(strcasecmp(XENSERVER, buf) == 0)
		{
			DebugPrintf(SV_LOG_INFO, "XenServer Release. OS Release String = %s\n", buf);						
			ret =  true;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "OS Release String  = %s\n", buf);
		}
		fclose(fp);	
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Couldn't open file: %s\n", RELEASE_FILE);
	}

	DebugPrintf(SV_LOG_INFO, "EXITED isXen\n");
	return ret;
}

