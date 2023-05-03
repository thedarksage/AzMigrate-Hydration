#include "util.h"
#include "inmsafecapis.h"

std::string convertTimeToString(SV_ULONGLONG timeInSec)
{
	time_t t1 = timeInSec;
	char szTime[128];

    if(timeInSec == 0)
        return "0000-00-00 00:00:00";

	strftime(szTime, 128, "%Y-%m-%d %H:%M:%S", localtime(&t1));

	return szTime;
}
int IsDayLightSavingOn( void )
{
   time_t t;
   time(&t);
   tm* tmt = localtime(&t);
   return tmt->tm_isdst;
}

time_t convertStringToTime(std::string szTime)
{
	struct tm t1;
    memset(&t1,0,sizeof(t1));
    if(szTime.compare("0000-00-00 00:00:00") == 0)
        return 0;

	char szTmp[128];
	inm_strcpy_s(szTmp,ARRAYSIZE(szTmp), szTime.c_str());

	char* tmp;
	tmp = strtok(szTmp, "- :");
	if(tmp != NULL)
	{
		t1.tm_year = atoi(tmp) - 1900;
	}

	tmp = strtok(NULL, "- :");

	if(tmp != NULL)
	{
		t1.tm_mon = atoi(tmp) - 1;
	}

	tmp = strtok(NULL, "- :");

	if(tmp != NULL)
	{
		t1.tm_mday = atoi(tmp);
	}

	tmp = strtok(NULL, "- :");

	if(tmp != NULL)
	{
		t1.tm_hour = atoi(tmp);
	}

	tmp = strtok(NULL,"- :");

	if(tmp != NULL)
	{
		t1.tm_min = atoi(tmp);
	}

	tmp = strtok(NULL,"- :");

	if(tmp != NULL)
	{
		t1.tm_sec = atoi(tmp);	
	}

	t1.tm_isdst = IsDayLightSavingOn();		//daylight savings on.
    time_t timet = 0;

    try{
	    timet = mktime(&t1);
    }
    catch(...)
    {
        timet = 0;
    }

	return timet;
}