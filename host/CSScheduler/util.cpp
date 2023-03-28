#include <vector>
#include <time.h>
#include <ctype.h>
#include "CXConnection.h"
#include "ERP.h"
#include "IRepositoryManager.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <curl/curl.h>
#include "thread.h"
#include "inmsafecapis.h"
#include "authentication.h"
#include "securityutils.h"
#include "genpassphrase.h"
#include "curlwrapper.h"

enum MONTH {JAN,FEB,MAR,APR,MAY,JUN,JUL,AUG,SEP,OCT,NOV,DEC,NON};
enum WEEKDAY {SUN,MON,TUE,WED,THU,FRI,SAT};

using namespace std;
class Year;

int IsDayLightSavingOn( void );


class Month
{
	enum MONTH m_Name;
	int days[7][6];
	int first[2];
	int last[2];	

	void setName(MONTH name)
	{
		m_Name = name;
	}

	void setDays(int nDays, WEEKDAY wday)
	{
		int i		= wday,j=0;
		int dayInc	= 1;
		first[0]	= wday;
		first[1]	= 0;

		while(nDays-- > 0)
		{
			days[ i ][ j ] = dayInc++;

			if(nDays == 0)
			{
				last[ 0 ] = i;
				last[ 1 ] = j;
			}

			i = ++i % 7;

			if(i == 0)
			{
				j++;
			}
		}
	}
public:

	Month()
	{
		for(int i = 0; i < 7 ; i++)
		{
			for(int j = 0; j < 6; j++)
			{
				days[ i ][ j ] = -1;
			}
		}

		m_Name = NON;
	}
	
	MONTH getName()
	{
		return m_Name;
	}

	int getDay(WEEKDAY wday, int nthday)	//nthday is one based index
	{
		//skip initial -1's due to month no starting on weekboundry
		int j = 0;
		
		while(days[ wday ][ j ] == -1)
		{
			j++;
		}

		if(j + nthday < 6)
		{
			return days[ wday ][ j + nthday ];
		}

		return -1;
	}

	int getLastDay()
	{
		return days[ last[0] ][ last[ 1 ] ];
	}

	int getWeekDay(int index)
	{
		if(index == 0)
		{
			if(first[ 0 ] == SUN )
			{
				return days[ first[ 0 ] + 1 ][ first[ 1 ] ];
			}
			else if(first[ 0 ] == SAT)
			{
				return days[ 1 ][ first[ 1 ] + 1 ];
			}
			else
			{
				return days[ first[ 0 ] ][ first[ 1 ] ];
			}
		}
		else
		{
			int j = 0;
			if(days[ 1 ][ 0 ] == -1)
			{
				j = 1;
			}

			if(index < 6)
			{
				return days[ 1 ][ j + index ];		//return the nth monday
			}

			return -1;
		}
	}

	int getFirstWeekDay()
	{
		if(first[ 0 ] == SUN )
		{
			return days[ first[ 0 ] + 1 ][ first[ 1 ] ];
		}
		else if(first[ 0 ] == SAT)
		{
			return days[ 1 ][ first[ 1 ] + 1 ];
		}
		else
		{
			return days[ first[ 0 ] ][ first[ 1 ] ];
		}
	}

	int getLastWeekDay()
	{
		if(last[ 0 ] == SUN )
		{
			return days[ 5 ][ last[ 1 ] - 1 ];
		}
		else if(last[ 0 ] == SAT)
		{
			return days[ 5 ][ last[ 1 ] ];
		}
		else
		{
			return days[ last[ 0 ] ][ last[ 1 ] ];
		}
	}

	WEEKDAY getDayOfWeek(int day)
	{
		for(int i = 0; i < 7; i++)
		{
			for(int j = 0; j < 6; j++)
			{
				if(day == days[ i ][ j  ] )
				{
					return static_cast<WEEKDAY>(i);
				}
			}
		}
	}
	friend class Year;
};

class Year
{
	int m_Year;
	vector<Month> m_Month;

	int getDayOfWeek(int mth, int mday)
	{
		struct tm t1;
        memset(&t1,0,sizeof(t1));
		t1.tm_year			= m_Year - 1900;
		t1.tm_mon			= mth;
		t1.tm_mday			= mday;
		t1.tm_isdst			= 0;
		time_t tt			= mktime(&t1);
		struct tm* tmptr	= localtime(&tt);

		int wday			= tmptr->tm_wday;
		return wday;
	}

public:
	Month getMonth(MONTH month)
	{
		return m_Month[ month ];
	}
	
	int getYear()
	{
		return m_Year;
	}
	
	void createYear(int year)
	{
		if(m_Month.size()>0)
		{
			m_Month.clear();
		}

		m_Year = year;

		for(int i = 0; i < 12; i++)
		{
			int isLeapYear = year % 100 == 0 ? year % 400 == 0 ? true : false : year % 4 == 0 ? true : false;

			int day = getDayOfWeek(i,1);

			Month mth;
			mth.setName((MONTH)0);

			if(i == 1)
			{
				if(isLeapYear && i == 1)		//FEB
				{
					mth.setDays(29, (WEEKDAY)day);
				}
				else
				{
					mth.setDays(28, (WEEKDAY)day);
				}
			}
			else
			{
				mth.setDays( (i - (int)(i / 7)) % 2 == 0 ? 31 : 30,(WEEKDAY)day);
			}
			m_Month.push_back(mth);
		}
	}
};

int IsDayLightSavingOn( void )
{
   time_t t;
   time(&t);
   tm* tmt = localtime(&t);
   return tmt->tm_isdst;
}



long calcReqTime(Year yr, int index, SchData schdata);

string convertTimeToString(unsigned long timeInSec)
{
	time_t t1 = timeInSec;
	char szTime[128];

    if(timeInSec == 0)
        return "0000-00-00 00:00:00";

	strftime(szTime, 128, "%Y-%m-%d %H:%M:%S", localtime(&t1));

	return szTime;
}

unsigned long convertStringToTime(string szTime)
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

long getNextStartTime(SchData schdata,long curTime)
{
	string szCurTime	= convertTimeToString(curTime);
	struct tm t1		= convertStringToTm(szCurTime);

	Year yr;			//Create current year
	yr.createYear(t1. tm_year + 1900);

	int yrIndex		= 0;
	int curMth		= t1.tm_mon;
	long tmpTime	= 0;

	if(schdata.everyMonth != 0)
	{
		//Month index is a zero based index
		while(1)
		{
			Month mth		= yr.getMonth((MONTH)curMth);
			tmpTime			= calcReqTime(yr,curMth,schdata);
			string szTime	= convertTimeToString(tmpTime);		

			if(tmpTime >= curTime)
			{
				break;
			}
			else
			{
				//Monthly schedule
				curMth = curMth + schdata.everyMonth;
				if(curMth > 11)
				{
					curMth = curMth % 12;
					yrIndex++;

					yr.createYear(t1.tm_year + yrIndex + 1900);
				}
			}
		}
	}
	else
	{
		//yearly schedule
		while(1)
		{
			Month mth		= yr.getMonth((MONTH)schdata.atMonth);
			tmpTime			= calcReqTime(yr,curMth,schdata);
			string szTime	= convertTimeToString(tmpTime);		
			if(tmpTime >= curTime)
			{
				break;
			}
			else
			{
				//Check for next year month
				yrIndex++;
				yr.createYear(t1.tm_year + yrIndex + 1900);
			}
		}
	}

	return tmpTime;
}

long getScheduleWeekly(SchData schdata, long curTime)
{
	string szCurTime	 = convertTimeToString(curTime);
	struct tm t1		= convertStringToTm(szCurTime);

	Year yr;			//Create current year
	
	int yrIndex		= 0;
	int curMth		= t1.tm_mon;
	long tmpTime	= 0;
	int dayIndex	= 0;

	if(schdata.atDay > 6 || schdata.atDay < 0)
		return 0;

	while(1)
	{
		yr.createYear(t1.tm_year + 1900 + yrIndex);
	
		Month mth = yr.getMonth((MONTH)curMth);
		int recday = mth.getDay((WEEKDAY)schdata.atDay,dayIndex);

		if(recday == -1)
		{
			dayIndex	= 0;
			curMth		= curMth + 1;

			if(curMth == 12)
			{
				curMth = 0;
				yrIndex++;
			}

			continue;
		}
		
		struct tm reqTime;
		memset(&reqTime,0,sizeof(reqTime));

		reqTime.tm_year		= yr.getYear() - 1900;
		reqTime.tm_mon		= curMth;
		reqTime.tm_mday		= recday;
		reqTime.tm_hour		= schdata.atHour;
		reqTime.tm_min		= schdata.atMinute;
		reqTime.tm_isdst	= IsDayLightSavingOn();

		tmpTime = mktime(&reqTime);

		if(tmpTime >= curTime)
		{
			break;
		}

		dayIndex++;
	}

	return tmpTime;
}


long getScheduleDaily(SchData schdata, long curTime)
{
	string szCurTime	= convertTimeToString(curTime);
	struct tm t1		= convertStringToTm(szCurTime);

	Year yr;			//Create current year
	int yrIndex		= 0;
	int curMth		= t1.tm_mon;
	long tmpTime	= 0;
	int recday		= t1.tm_mday;

	while(1)
	{
		yr.createYear(t1.tm_year + 1900 + yrIndex);

		Month mth = yr.getMonth((MONTH)curMth);
		
		struct tm reqTime;
		memset(&reqTime,0,sizeof(reqTime));

		reqTime.tm_year		= yr.getYear() - 1900;
		reqTime.tm_mon		= curMth;
		reqTime.tm_mday		= recday;
		reqTime.tm_hour		= schdata.atHour;
		reqTime.tm_min		= schdata.atMinute;
		reqTime.tm_isdst	= IsDayLightSavingOn();

		tmpTime = mktime(&reqTime);

		if(tmpTime >= curTime)
		{
			break;
		}

		if(mth.getLastDay() == recday)
		{
			recday = 1;
			curMth = curMth + 1;

			if(curMth == 12)
			{
				curMth = 0;
				yrIndex++;
			}
		}	
		else
		{
			recday++;
		}
	}

	return tmpTime;
}

long calcReqTime(Year yr,int index,SchData schdata)
{
	struct tm reqTime;	
	memset(&reqTime,0,sizeof(reqTime));
	long tmpTime		= 0;

	Month mth			= yr.getMonth((MONTH)index);
	reqTime.tm_year		= yr.getYear()-1900;
	reqTime.tm_mon		= index;
	reqTime.tm_hour		= schdata.atHour;
	reqTime.tm_min		= schdata.atMinute;

	switch(schdata.everyDay)
	{
	case 0:
		//First day
		reqTime.tm_mday = schdata.atDay < 4 ? schdata.atDay + 1 : mth.getLastDay();
		break;

	case 1:
		if(schdata.atDay == 4)
		{
			reqTime.tm_mday = mth.getLastWeekDay();
		}
		else
		{
			reqTime.tm_mday = mth.getWeekDay(schdata.atDay);
		}
		break;

	case 2:
		//This case should be taken care by calling function;
		if(schdata.atDay == 4)
		{
			if(mth.getDay(SAT,4) == -1)
				reqTime.tm_mday = mth.getDay(SAT, 3);
		}
		else
		{
			reqTime.tm_mday = mth.getDay(SAT, schdata.atDay);
		}
		break;

	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
		if(schdata.atDay == 4)
		{
			if(mth.getDay((WEEKDAY)(schdata.everyDay - 3), 4) == -1)
			{
				reqTime.tm_mday = mth.getDay((WEEKDAY)(schdata.everyDay - 3), 3);
			}
			else
			{
				reqTime.tm_mday = mth.getDay((WEEKDAY)(schdata.everyDay - 3), 4);
			}
		}
		else
		{
			reqTime.tm_mday = mth.getDay((WEEKDAY)(schdata.everyDay - 3), schdata.atDay);
		}
	}
	
	reqTime.tm_isdst	= IsDayLightSavingOn();

	tmpTime = mktime(&reqTime);

	return tmpTime;
}

long convertSchDataToTime(tm t1)
{
	return mktime(&t1);
}

struct tm convertStringToTm(string szTime)
{
	struct tm t1;
	char szTmp[128];

	memset(&t1,0,sizeof(t1));
	inm_strcpy_s(szTmp, ARRAYSIZE(szTmp), szTime.c_str());

	char* tmp;
	tmp = strtok(szTmp, "- :");

	if(tmp!=NULL)
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

	tmp = strtok(NULL,"- :");
	
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
	
	t1.tm_isdst	= IsDayLightSavingOn();

	return t1;
}

char *findAndReplace(char *str)
{
	//
	//Find and replace space with zero
	//
	int i = -1;

	while(str[++i] != '\0')
	{
		str[i] = (str[i] == ' ') ? '0' : str[i];
	}

	return str;
}

char *stringupr(char *str)
{
	unsigned int i = 0;

	for( i = 0; i < strlen(str); i++)
	{
		str[ i ] = toupper(str[ i ]);
	}

	return str;
}
/*
* inm_sql_escape_string handles sql escape characters in string literals, such as \',\",\\,\b,\n,\r,\t characters.
*/
size_t inm_sql_escape_string(char *out_buff,const char *in_buff, size_t in_buff_length)
{
	size_t out_buff_index = 0;
	for(size_t i=0 ; i < in_buff_length; i++)
	{
		char copy_char = in_buff[i];

		switch(copy_char)
		{
		case '\'':
		case '"' :
		case '\\':
		case '\b':
		case '\n':
		case '\r':
		case '\t':
			out_buff[out_buff_index++] = '\\';
		}
		out_buff[out_buff_index++] = copy_char;
	}

	out_buff[out_buff_index++] = 0; //Null Terminated character
	
	return out_buff_index; //Return size of new string.
}

ofstream Logger::infile;
bool Logger::bCreated;
Mutex Logger::mutex;

/*
*   ----- << Helper-Functions: HTTP communication using CURL >> ------
*/

/*
* Callback function used by curl in PostToCX. 
*/
static size_t write_data(void *buffer, size_t size, size_t nmemb, void *context)
{
    std::string& result = *static_cast<std::string*>( context );
    size_t const count = size * nmemb;
    std::string const data( static_cast<char*>( buffer ), count  );
    result += data;
    return count;
}
/*
* true is returned on success, otherwise false.
* Posts the post_buff to specified inm_cx_api_url and fills the response to recv_buff on success.
* On post failure, retry will be happened retry_count counts before sending failure status to the caller.
*/
bool PostToCX(const std::string & inm_cx_api_url, const std::string & post_buff, std::string & recv_buff, bool https,int retry_count)
{
	bool bSuccess = false;
	do{
		int curl_resp_timeout = 300;// In Sec

		Logger::Log(2,1, "CX API URL : %s\n", inm_cx_api_url.c_str());

		/*Set the following input options to CURL 
		* time-out, 
		* post-data,
		* recieve data buffer,
		* writer callback-function to write data to the recieve-buffer,
		* and cx-url
		*/

		CurlOptions options(inm_cx_api_url);
		options.writeCallback( static_cast<void *>( &recv_buff ),write_data);
		options.transferTimeout(curl_resp_timeout);
		options.responseTimeout(curl_resp_timeout);

		try {
			CurlWrapper cw;
			cw.post(options, post_buff);
			bSuccess = true;
			break;
		} catch(ErrorException& exception )
		{
			Logger::Log(2,1, "Could not post request to CS: %s with error %s\n",__FUNCTION__, exception.what());
			if(retry_count-- >0)
			{		
				Logger::Log(2,1, "Retrying same post ");
				sch::Sleep(1000); //1 Sec sleep.
				continue;
			}
			else
			{
				Logger::Log(2,1, "Reached  Maximum Retry attempts ");
				break;
			}
		}


	}while(true);

	return bSuccess;
}

/*
*   ----- << Helper-Functions: XML Parsing using libxml >> ------
*/

/*
* XMLizes the text so that it can be placed as xml-tag content. &,',",>,<,& these are special characters for XML 
*/

//Getfingerprint from cs.fingerprint
inline std::string getFingerprint()
{
	
	//Read fingerprint from file
	std::string fingerprintFileName(securitylib::getFingerprintDir());
	fingerprintFileName += securitylib::SECURITY_DIR_SEPARATOR;
	fingerprintFileName += "cs.fingerprint";
	           
    std::string key;
	std::ifstream file(fingerprintFileName.c_str());
    if (file.good()) {
		file >> key;
        boost::algorithm::trim(key);
    }
    
	return key;
}


std::string GenerateSignature(const std::string &requestId,const std::string& functionName)
{
	string method = "POST";
	string url = "/ScoutAPI/CXAPI.php";
	string version = "1.0";
	string fingerprint;
	string passphrase;
	
	//Get Fingerprint only in case of Https, In HTTP case signature should to be generated with empty fingerprint
	if (SCH_CONF::UseHttps() == true)
	{
		fingerprint = getFingerprint();
	}	
	
	//get passphrase
	passphrase = securitylib::getPassphrase();
		
	//Generate signature
	string inm_sign = Authentication::buildCsLoginId(method.c_str(), url, functionName.c_str(), passphrase, fingerprint, requestId, version);
	//clear passpharse
	securitylib::secureClearPassphrase(passphrase);
		
	return inm_sign;
}

void XmlizeText(std::string & text)
{
	std::string xmlText;
	for(size_t iText = 0; iText < text.length(); iText++)
	{
		std::string xml_char;
		switch(text[iText])
		{
		case '&': 
			xml_char = "&amp;";
			break;
		case '\'':
			xml_char = "&apos;";
			break;
		case '"':
			xml_char = "&quot;";
			break;
		case '>':
			xml_char = "&gt;";
			break;
		case '<':
			xml_char = "&lt;";
			break;
		default:
			xml_char = text[iText];
		}
		xmlText += xml_char;
	}
	text = xmlText;
}

inline void xmlAddParamValue(std::stringstream& xOut, const char* ppName, const char* ppValue)
{
	xOut << "<Parameter Name=\"" << ppName
		 << "\" Value=\"" << ppValue << "\"/>";
}
void GenerateXmlRequest(const std::string& id, const std::string & query_type,std::string & sqlQuery, std::string & xmlRequest)
{
	//Generate Access Signature 
	std::string accessSignature = GenerateSignature(id,INM_SCHEDULER_SQL_QUERY_CX_API);
	
	//Handle xml-special characters in sql query.
	std::string tempQuery = sqlQuery;
	
	XmlizeText(sqlQuery);
	std::string procName;
	std::stringstream xOut;
	xOut << "<Request Id=\"" << id << "\" Version=\"1.0\">"
		 << "<Header>"
		 << "<Authentication>"
		 << "<AuthMethod>" << INM_AUTH_NAME <<"</AuthMethod>"
		 << "<AccessKeyID>" << SCH_CONF::GetCxHostId() <<"</AccessKeyID>"
		 << "<AccessSignature>"<< accessSignature <<"</AccessSignature>"
		 << "</Authentication>"
		 << "</Header>"
		 << "<Body>"
		 << "<FunctionRequest Name=\""<< INM_SCHEDULER_SQL_QUERY_CX_API <<"\" Include=\"No\">";
	xmlAddParamValue(xOut,"UpdateType",query_type.c_str());
	xmlAddParamValue(xOut,"UpdateSql",sqlQuery.c_str());

	Logger::Log(0,1," Query_TYPE in GenerateXmlRequest = %s\n",query_type.c_str());
	if(query_type.compare("PROCEDURE")==0)
	{
		size_t index = tempQuery.find("inm_sch_dyn_procedure_");
		size_t index1 = tempQuery.find_first_of("()");
		procName = tempQuery.substr (index,(index1 - index -2));
		Logger::Log(0,1," procedure name extracted from Query = %s\n",procName.c_str());	
	}
	
	xmlAddParamValue(xOut,"ProcedureName",procName.c_str());
	xOut << "</FunctionRequest>"
		 << "</Body>"
		 << "</Request>";

	xmlRequest = xOut.str();
	
}
xmlNodePtr xGetChildEleNode(xmlNodePtr node)
{
	xmlNodePtr curr_node = node ? node->children : NULL;

	while( curr_node && 
		   (XML_ELEMENT_NODE != curr_node->type)
		 )
		curr_node = curr_node->next;
	
	return curr_node;
}
xmlNodePtr xGetNextEleNode(xmlNodePtr node)
{
	xmlNodePtr curr_node = node ? node->next : NULL;

	while( curr_node && 
		   (XML_ELEMENT_NODE != curr_node->type)
		 )
		curr_node = curr_node->next;

	return curr_node;
}
inline std::string xGetProp(xmlNodePtr node, const string & prop_name)
{
	string propVal;
	if(!node || prop_name.empty()) return propVal;

	xmlChar *pProp = xmlGetProp(node,(const xmlChar*)prop_name.c_str());
	
	if(pProp)
	{
		propVal = (const char*)pProp;
		xmlFree(pProp);
	}

	return propVal;
}

inline int xGetIntProp(xmlNodePtr node, const string & prop_name)
{
	return atoi(xGetProp(node,prop_name).c_str());
}

void ParseXMLResultSet(xmlNodePtr res_set, DBResultSet & result_set,const int num_recrds)
{
	for( int iRec = 0; 
		(iRec < num_recrds) && res_set;
		iRec++, res_set = xGetNextEleNode(res_set)
		)
	{
		DBRow row;
		for(xmlNodePtr node_param = xGetChildEleNode(res_set);
			node_param;
			node_param = xGetNextEleNode(node_param))
		{
			TypedValue column_val(xGetProp(node_param,"Value"));
			row.push(column_val);
		}
		result_set.push(row);
	}
}

int ParseXmlResponse(const std::string& xml_response, DBResultSet & res_set, int& sql_err)
{
	std::string err_msg;
	//Logger::Log(0,1,"XML Response in ParseXmlResponse = %s\n",xml_response.c_str());
	xmlDocPtr xDoc = xmlParseMemory(xml_response.c_str(),xml_response.length());
	if(NULL == xDoc)
	{
		Logger::Log(2,1,"XML Parse error.\n");
		return CX_E_XML_PARSE_ERROR;
	}

	xmlNodePtr curr_node = xmlDocGetRootElement(xDoc);
	if(NULL == curr_node)
	{
		err_msg = "XML Parse error. Could not get root tag";
		Logger::Log(2,1,"%s.\n",err_msg.c_str());
		xmlFreeDoc(xDoc);
		return CX_E_XML_RESP_FORMAT_ERROR;
	}

	if(xmlStrcasecmp(curr_node->name,(const xmlChar*)"Response"))
	{
		err_msg = "Invalid root tag";
		xmlFreeDoc(xDoc);
		Logger::Log(2,1,"%s.\n",err_msg.c_str());
		return CX_E_XML_RESP_FORMAT_ERROR;
	}

	do {
		sql_err = xGetIntProp(curr_node,"Returncode");
		
		if(sql_err)// 0-Success, any other code is failure
		{
			err_msg = xGetProp(curr_node,"Message");
			if(err_msg.empty())
			{
				err_msg = "Unkown error" ;
			}
			err_msg += " @ " + string((const char*)curr_node->name);
			xmlFreeDoc(xDoc);
			Logger::Log(2,1,"%s.\n",err_msg.c_str());
			return CX_E_XML_RESP_FORMAT_ERROR;
		}
		curr_node = xGetChildEleNode(curr_node);
		if( curr_node && 
			xGetChildEleNode(curr_node) && 
			0 == xmlStrcasecmp(curr_node->name,(const xmlChar*)"Body") &&
			0 == xmlStrcasecmp(xGetChildEleNode(curr_node)->name,(const xmlChar*)"Function"))
		{
			curr_node = xGetChildEleNode(curr_node);
			continue;
		}
		else if(0 == curr_node || xmlStrcasecmp(curr_node->name,(const xmlChar*)"FunctionResponse"))
		{
			err_msg = "Invalid (or) Incomplete xml response";
			xmlFreeDoc(xDoc);
			Logger::Log(2,1,"%s.\n",err_msg.c_str());
			return CX_E_XML_RESP_FORMAT_ERROR;
		}
		break;
	}while(true);

	if(NULL == xGetChildEleNode(curr_node))
	{
		err_msg = "No response information found";
		xmlFreeDoc(xDoc);
		Logger::Log(2,1,"%s.\n",err_msg.c_str());
		return CX_E_XML_RESP_FORMAT_ERROR;
	}
	else
	{
		curr_node = xGetChildEleNode(curr_node);
		std::string param_name = 
			xGetProp(curr_node,"Name");
		if(0 == param_name.compare("AffectedRecords"))
		{
			res_set.setRowsUpdated( xGetIntProp(curr_node,"Value") );
		}
		else if(0 == param_name.compare("NumRecords"))
		{
			int num_recds = xGetIntProp(curr_node,"Value");
			ParseXMLResultSet(xGetNextEleNode(curr_node),res_set,num_recds);
		}
		else
		{
			//Unexpected parameter
			err_msg = "Neither AffectedRecords nor NumRecords found";
			Logger::Log(2,1,"%s.\n",err_msg.c_str());
			xmlFreeDoc(xDoc);
			return CX_E_XML_RESP_FORMAT_ERROR;
		}
	}

	xmlFreeDoc(xDoc);
	//Logger::Log(0,0,"Exited %s\n",__FUNCTION__);
	return CX_E_SUCCESS;
}


