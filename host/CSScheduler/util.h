#ifndef __UTIL_H_
#define __UTIL_H_

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>
#include <string>
#include "Log.h"
#include "InmSignInfo.h"

using namespace std;

#define INM_SCHEDULER_SQL_QUERY_CX_API	"SchedulerUpdate"
#define INM_AUTH_NAME "ComponentAuth_V2015_01"

class SchData
{
public:
	SchData()
	{
		everyDay	= 0;
		everyHour	= 0;
		everyMinute = 0;
		everyMonth	= 0;
		everyYear	= 0;
		atYear		= 0;
		atMonth		= 0;
		atDay		= 0;
		atHour		= 0;
		atMinute	= 0;
	}

	int everyDay;
	int everyHour;
	int everyMinute;
	int everyMonth;
	int everyYear;
	int atYear;
	int atMonth;
	int atDay;
	int atHour;
	int atMinute;

	bool isEmpty()
	{
		if(	everyDay	== 0 &&
			everyHour	== 0 &&
			everyMinute == 0 &&
			everyMonth	== 0 &&
			everyYear	== 0 &&
			atYear		== 0 &&
			atMonth		== 0 &&
			atDay		== 0 &&
			atHour		== 0 &&
			atMinute	== 0)
			return true;
		else
			return false;
	}

	void logDetails()
	{
		Logger::Log(__FILE__, __LINE__, 0, 0, "---------------Schedule Details---------------");
		Logger::Log(__FILE__, __LINE__, 0, 0, "everyDay:%ld", everyDay);
		Logger::Log(__FILE__, __LINE__, 0, 0, "everyHour:%ld", everyHour);
		Logger::Log(__FILE__, __LINE__, 0, 0, "everyMinute:%ld", everyMinute);
		Logger::Log(__FILE__, __LINE__, 0, 0, "everyMonth:%ld", everyMonth);
		Logger::Log(__FILE__, __LINE__, 0, 0, "everyYear:%ld", everyYear);
		Logger::Log(__FILE__, __LINE__, 0, 0, "atYear:%ld", atYear);
		Logger::Log(__FILE__, __LINE__, 0, 0, "atMonth:%ld", atMonth);
		Logger::Log(__FILE__, __LINE__, 0, 0, "atDay:%ld", atDay);
		Logger::Log(__FILE__, __LINE__, 0, 0, "atHour:%ld", atHour);
		Logger::Log(__FILE__, __LINE__, 0, 0, "atMinute:%ld", atMinute);
		Logger::Log(__FILE__, __LINE__, 0, 0, "----------------------------------------------");
	}
};

string convertTimeToString(unsigned long timeInSec);

unsigned long convertStringToTime(string szTime);

struct tm convertStringToTm(string szTime);

long getNextStartTime(SchData schdata,long curTime);

long getScheduleWeekly(SchData schdata,long curTime);

long getScheduleDaily(SchData schdata,long curTime);

char *findAndReplace(char *str);

char *stringupr(char *str);

size_t inm_sql_escape_string(char *out_buff,const char *in_buff, size_t in_buff_length);

bool PostToCX(const std::string & inm_cx_api_url, const std::string & post_buff, std::string & recv_buff, bool https,int retry_count=5);

void GenerateXmlRequest(const std::string& id, const std::string & query_type,std::string & sqlQuery, std::string & xmlRequest);

class DBResultSet;

int ParseXmlResponse(const std::string& xml_response, DBResultSet & res_set, int& sql_err);

std::string GenerateSignature(const std::string& requestId,const std::string& functionName);

#endif
