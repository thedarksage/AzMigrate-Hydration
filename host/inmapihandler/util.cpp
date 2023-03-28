#include <sstream>
#include <iomanip>
#include "util.h"
#include "inmageex.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portable.h"
#include "localconfigurator.h"
#include <ace/File_Lock.h>
#include "portablehelpersmajor.h"
#include "InmsdkGlobals.h"
#include "inmsdkexception.h"
#include "cdputil.h"
#include "ProtectionHandler.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include "confengine/volumeconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "inmstrcmp.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/auditconfig.h"
#include "confengine/agentconfig.h"
#include "confengine/alertconfig.h"

#include "event.h"
#include "cdpplatform.h"
#include "host.h"
#include "APIController.h"
#include "Handler.h"
#include <boost/algorithm/string.hpp>
#include "appcommand.h"
#ifdef SV_WINDOWS
#include "operatingsystem.h"
#include "virtualvolume.h"
#else
#include "unixvirtualvolume.h"
#endif
#include "../inmsafecapis/inmsafecapis.h"

boost::shared_ptr<Event> QuitEvt;

void FetchValueFromMap(const std::map<std::string,std::string> &m ,const std::string &keyName, std::string &value) 
 {
    
     std::map<std::string, std::string>::const_iterator iter = m.find(keyName);
     if(iter != m.end())
     {
         value = iter->second;
     }

     else
     {
         DebugPrintf(SV_LOG_ERROR,"could not find expected parameter\n");        
         throw INMAGE_EX("could not find expected parameter name:")(keyName);
     }
}

std::string ConvertTimeToString(SV_ULONGLONG timeInSec)
{
	time_t t1 = timeInSec;
	char szTime[128];

    if(timeInSec == 0)
        return "0000-00-00 00:00:00";

	strftime(szTime, 128, "%Y-%m-%d %H:%M:%S", localtime(&t1));

	return szTime;
}


void getTimeinDisplayFormat( const SV_ULONGLONG& eventTime, std::string& timeInDisplayForm )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SV_TIME svtime;
	CDPUtil::ToSVTime( eventTime, svtime );
	std::stringstream displaytime;
	displaytime  << svtime.wYear   << "/" 
		<< svtime.wMonth  << "/" 
		<< svtime.wDay    << " " 
		<< svtime.wHour   << ":" 
		<< svtime.wMinute << ":" 
		<< svtime.wSecond << ":"
		<< svtime.wMilliseconds << ":" 
		<< svtime.wMicroseconds << ":"
		<< svtime.wHundrecNanoseconds ;	
	timeInDisplayForm = displaytime.str();
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
}

int is_leap_year(int year)
{
    int result;
    
    if ( (year%4) != 0 )           
        result = false;            
    else if ( (year%400) == 0 )    
        result = true;             
    else if ( (year%100) == 0 )    
        result = false;            
    else                           
        result = true;             
    
    return ( result );
}


bool date_is_valid(int day, int month, int year)
{
    bool valid = true;
    int month_length[13] =
        { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    if ( is_leap_year(year) )
        month_length[2] = 29;
        //  29 days in February in a leap year (including year 2000)
    
    if ( month < 1 || month > 12 )
        valid = false;
    else if ( day < 1 || day > month_length[month] )
        valid = false;

    return ( valid );
}


bool lengthValidation (std::string year,std::string month,std::string date,std::string hour,std::string min,std::string sec)
{
	bool valid = false;
	boost::algorithm::trim(year);
	boost::algorithm::trim (month);
	boost::algorithm::trim (date);
	boost::algorithm::trim (hour);
	boost::algorithm::trim (min);
	boost::algorithm::trim (sec);

	if (year.length () == 4 && 
		month.length() == 2 && 
		date.length () == 2 && 
		hour.length() == 2 && 
		min.length() == 2 && 
		sec.length() == 2 )
	{
		valid = true;			
	}

	return valid;
}
bool emptyDateValidation (std::string year,std::string month , std::string date , std::string hour ,std::string min ,std::string sec )
{
	bool valid = false;
	if (!year.empty() && 
		!month.empty() && 
		!date.empty() && 
		!hour.empty () && 
		!min.empty () && 
		!sec.empty() )
	{
		valid = true;
	}
	else 
		valid = false;
	return valid;
}

bool rangeValidation (std::string year,std::string month , std::string date , std::string hour ,std::string min ,std::string sec)
{
	bool valid = false;
	int int_hour = boost::lexical_cast <int> (hour);
	int int_min	 = boost::lexical_cast <int> (min);
	int int_sec	 = boost::lexical_cast <int> (sec);

	if ( int_hour >= 0 && int_hour < 24 )
	{
		if (int_min >= 0 && int_min < 60)
		{
			if (int_sec >= 0 && int_sec < 60 )
			{
				valid = true;
			}
			else 
				valid = false; 
		}
		else 
			valid = false ;
	}
	else 
		valid = false;
	return valid;
}

bool dateValidator (std::string year,std::string month , std::string date , std::string hour ,std::string min ,std::string sec )
{
	bool valid = false;

	if ( emptyDateValidation (year,month,date,hour,min,sec) == true &&
		 lengthValidation(year,month,date,hour,min,sec) == true && 
		 rangeValidation (year,month,date,hour,min,sec) == true ) 
	{
		valid = true;
	}
	else 
		valid = false;

	if (valid == true )
	{
		int int_day = boost::lexical_cast <int> (date);
		int int_month = boost::lexical_cast <int> (month);
		int int_year = boost::lexical_cast <int> (year);
		valid = date_is_valid (int_day,int_month,int_year);
	}
	else 
		valid = false;
	return valid;
}

void GetDurationAndTimeIntervelUnit(const std::string& duration, time_t& durationInHours,
                                    std::string& timeIntervalUnit)
{
    std::string durationLocal = duration;
    boost::algorithm::to_lower(durationLocal) ;
    size_t found ;
    std::string tempStr ;
    SV_UINT multiplicationFactor = 1 ;
    if( ( found = durationLocal.find( "hr" )) != std::string::npos ||
        ( found = durationLocal.find("hour")) != std::string::npos )
    {
        tempStr = durationLocal.substr(0, found) ;
        multiplicationFactor = 1 ;
    }
    else if( (found = durationLocal.find( "day" )) != std::string::npos )
    {
        tempStr = durationLocal.substr(0, found) ;
        multiplicationFactor = 24 ;
    }
    else if( ( found = durationLocal.find( "week" )) != std::string::npos )
    {
        tempStr = durationLocal.substr(0, found) ;
        multiplicationFactor = 24 * 7 ;
    }
    else if( ( found = durationLocal.find( "month" )) != std::string::npos )
    {
        tempStr = durationLocal.substr(0, found) ;
        multiplicationFactor = 24* 30 ;
    }
    else if( ( found = durationLocal.find( "year" )) != std::string::npos )
    {
        tempStr = durationLocal.substr(0, found) ;
        multiplicationFactor = 24 * 365 ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "duration is not in hour(s) or day(s) or week(s) or year(s)\n");
    }
   
    boost::algorithm::replace_all( tempStr, " ", "") ;
    durationInHours = boost::lexical_cast<time_t>(tempStr) ;
    durationInHours = durationInHours * multiplicationFactor ;
    timeIntervalUnit = durationLocal.substr(found) ;

    DebugPrintf(SV_LOG_DEBUG,"timeIntervelUnit =  %s duration = %ld\n",timeIntervalUnit.c_str(),
                            durationInHours);
}

void GetGranularityUnitAndTimeGranularityUnit(const std::string &granularity,
                                              std::string & granularityUnit,
                                              std::string & timeGranularityUnit)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    size_t found;
    std::string valueStr = granularity ;
    for (size_t i = 0; i < valueStr.length(); ++i)
    {
        valueStr[i] = tolower(valueStr[i]);
    }
     SV_UINT multiplicationFactor = 1 ;
    DebugPrintf(SV_LOG_DEBUG,"Duration %s\n",valueStr.c_str());

    if ((found = valueStr.find("hour")) != std::string::npos ||
          (found = valueStr.find("hr")) != std::string::npos  )
    {
        granularityUnit = valueStr.substr(0,found);        
        timeGranularityUnit = "Hr";
    }  
    else if ( (found = valueStr.find("day")) != std::string::npos )
    {
        granularityUnit = valueStr.substr(0,found);
        multiplicationFactor = 24 ;
        timeGranularityUnit = "day";
    }  
    else if ((found = valueStr.find("week")) != std::string::npos )
    {
        granularityUnit = valueStr.substr(0,found);
        multiplicationFactor = 24 * 7 ;
        timeGranularityUnit = "week";
    }  
    else if ((found = valueStr.find("month")) != std::string::npos )
    {
        granularityUnit = valueStr.substr(0,found);
        multiplicationFactor = 24 * 30 ;
        timeGranularityUnit = "month";
    }  
    else if ( (found = valueStr.find("year")) != std::string::npos )
    {
        granularityUnit = valueStr.substr(0,found);
        multiplicationFactor = 24 * 365 ;
        timeGranularityUnit = "year";
    }  
    else
    {
        DebugPrintf(SV_LOG_ERROR ,"duration is not in hour(s) or day(s) or week(s) or year(s)\n");
    }
    boost::algorithm::replace_all(granularityUnit, " ", "") ;
    SV_UINT granualarityUnitInHours = atoi( granularityUnit.c_str() ) * multiplicationFactor ;
    std::stringstream temp ;
    temp << granualarityUnitInHours ;
    granularityUnit = temp.str() ;
    DebugPrintf(SV_LOG_DEBUG,"granularityUnit =  %s timeGranularityUnit = %s\n",temp.str().c_str(),timeGranularityUnit.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void GetExcludeFromAndTo(const std::string& exception,
                                       time_t &StartTime, time_t &EndTime)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string excludeTime = exception ;
    StartTime = EndTime = 0 ;
	size_t pos = exception.find_first_of('-');
    std::string excldfrm,excldTo;
	if(pos	!= std::string::npos)
    {
        excldfrm = excludeTime.substr(0,pos);
        excldTo =  excludeTime.substr(pos+1,excludeTime.length());

		pos = 0;
		pos = excldfrm.find_first_of(":");
		if(pos != std::string::npos)
		{
			SV_ULONGLONG temp = (boost::lexical_cast<SV_ULONGLONG>(excldfrm.substr(0,pos)))*60*60;
			temp += (boost::lexical_cast<SV_ULONGLONG>(excldfrm.substr(pos+1,excldfrm.length())))*60;
			StartTime = temp;
		}

		pos = 0;
		pos = excldTo.find_first_of(":");
		if(pos != std::string::npos)
		{
			SV_ULONGLONG temp = (boost::lexical_cast<SV_ULONGLONG>(excldTo.substr(0,pos)))*60*60;
			temp += (boost::lexical_cast<SV_ULONGLONG>(excldTo.substr(pos+1,excldTo.length())))*60;
			EndTime = temp;
		}
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}



bool getCDPEventPropertyVector(const std::string &retentionPath, std::vector<CDPEvent>& eventVector, int numberofevents)
{
    bool bRet = true ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    CDPMarkersMatchingCndn cndn;
	cndn.mode(ACCURACYEXACT);
	SV_EVENT_TYPE type;
	std::string applicationType = "USERDEFINED"; //UserDefined type
	VacpUtil::AppNameToTagType(applicationType, type);
	cndn.type(type);
	if( numberofevents != 0 )
	{
		cndn.limit(numberofevents) ;
	}
    if (boost::filesystem::exists(retentionPath))
    {
        CDPUtil::getevents( retentionPath, cndn, eventVector ) ;
    }
	applicationType = "FS"; //filesystem type
	VacpUtil::AppNameToTagType(applicationType, type);
	cndn.type(type);
	if (boost::filesystem::exists(retentionPath))
    {
		CDPUtil::getevents( retentionPath, cndn, eventVector ) ;
	}
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void GetAvailableRestorePointsPg(const std::string& actualbasepath,
								 const std::string& newbasepath,
								 const std::string& retentionPath,
								 SV_UINT rpoThreshold,
                                 ParameterGroup& availableRestorePointsPg, 
								 std::map<SV_ULONGLONG, CDPEvent>& restorePointMap, 
								 const int& nRestoreCount   )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	if(!retentionPath.empty())
    {
		getRestorePointsforVolume(actualbasepath, newbasepath, retentionPath, restorePointMap, rpoThreshold, nRestoreCount ) ;
		std::vector<std::string> RestorePntProperties;
		RestorePntProperties.push_back("RestorePoint");
		RestorePntProperties.push_back("RestorePointTimestamp");
		RestorePntProperties.push_back("RestorePointGUID");
		RestorePntProperties.push_back("Accuracy");
		RestorePntProperties.push_back("ApplicationType");
		insertRestorePointMapToRPPG( availableRestorePointsPg, restorePointMap, RestorePntProperties ) ;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Empty retention path") ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void GetAvailableRecoveryRangesPg(const std::string& actualbasepath,
								  const std::string& newbasepath,
								  const std::string& retentionpath,
								  SV_UINT rpoThreshold,
								  ParameterGroup& availableRecoveryRangesPg, 
								  std::map<SV_ULONGLONG, CDPTimeRange> &recoveryRangeMap, 
								  const int& nRestoreCount )

{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
	if( !retentionpath.empty() )
    {
		getRecoveryRangesforVolume(actualbasepath, newbasepath, retentionpath, recoveryRangeMap, rpoThreshold, nRestoreCount ) ;
		std::vector<std::string> RecoveryRangeProperties;
		RecoveryRangeProperties.push_back("StartTime");
		RecoveryRangeProperties.push_back("EndTime");
		RecoveryRangeProperties.push_back("StartTimeUTC");
		RecoveryRangeProperties.push_back("EndTimeUTC");
		RecoveryRangeProperties.push_back("Accuracy");
		insertTimeRanageMapToRRPG( availableRecoveryRangesPg, recoveryRangeMap, RecoveryRangeProperties )  ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "empty retention path") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


/* for the given actualBasePath actualPath and newBasePath we need to construct newPath */
// actualBasePath = R or \\10.0.1.6\MyRepo
// actualPath = R:\9FA9C0E3-C441-7B4D-832692C4BF05F4F3\catalogue\C\1335257604
// NewBasePath = Q or \\10.0.1.6\MyRepo
// We need to construct newPath


void sanitizeWindowspath(std::string& path)
{
	boost::algorithm::replace_all( path, "/", "\\" ) ;
	if (!( path[0] == '\\' && path[1] == '\\') ) //if not unc path
	{
		boost::algorithm::replace_all(path, "\\\\", "\\" ) ;
	}
}
void sanitizeunixpath(std::string& path)
{
	boost::algorithm::replace_all(  path, "\\", "/" ) ;
    boost::algorithm::replace_all(  path, "//", "/" ) ;
}

void convertPath( const std::string& actualbp, const std::string& actualpath,  
				 const std::string& newbp, std::string& newpath)
{

    std::string actualbasepath, newbasepath ;
	actualbasepath = actualbp ;
	newbasepath = newbp ;
    newpath = actualpath ;
    bool fromIsWindowsPath = false ;
    bool toIsWindowsPath = true;
    if( newbasepath[0] != '/' ) {
        toIsWindowsPath = true ;
        sanitizeWindowspath( newbasepath ) ;
		sanitizeWindowspath( newpath ) ;
    }
    else {
        toIsWindowsPath = false ;
		sanitizeunixpath( newpath ) ;
		sanitizeunixpath( newbasepath ) ;
    }
    if( actualbasepath[0] != '/' ) {
        fromIsWindowsPath = true ;
		sanitizeWindowspath( actualbasepath ) ;		
    }
    else {
        fromIsWindowsPath = false ;
		sanitizeunixpath( actualbasepath ) ;
    }
    int actualbasepathlength = 0 ;
    int newbasepathlength = 0 ;
    if( fromIsWindowsPath ) {
        if( actualbasepath.length() == 1 ) 
		{
            actualbasepath += ":\\" ;
        }
        else if( actualbasepath[actualbasepath.length() - 1] != '\\' ) 
		{
            actualbasepath += "\\" ;
        }
        actualbasepathlength = actualbasepath.length() ;

    }
    else {
        if( (actualbasepath.length() > 1) && (actualbasepath[actualbasepath.length() - 1] != '/') ) {
            actualbasepath += "/" ;
        }
        actualbasepathlength = actualbasepath.length() ;
    }
    if( toIsWindowsPath ) {
        if( newbasepath.length() == 1 )
        {
            newbasepath += ":\\" ;
        }
        else if( newbasepath[newbasepath.length() - 1] != '\\' ) 
		{
            newbasepath += "\\" ;
        }
        newbasepathlength = newbasepath.length() ;
    }
    else {
        if( (newbasepath.length() > 1) && (newbasepath[newbasepath.length() - 1] != '/') ) {
            newbasepath += "/" ;
        }
        newbasepathlength = newbasepath.length() ;
    }
    //DebugPrintf(SV_LOG_DEBUG, "actualbasepath %s actualbasepathlength %d\n", actualbasepath.c_str(), actualbasepathlength) ;
    //DebugPrintf(SV_LOG_DEBUG, "newbasepath %s newbasepathlength %d\n", newbasepath.c_str(), newbasepathlength) ;
    //DebugPrintf(SV_LOG_DEBUG, "actualpath %s\n", actualpath.c_str()) ;
    newpath = newbasepath + actualpath.substr(actualbasepathlength) ;
	if ( toIsWindowsPath == true )
	{
		sanitizeWindowspath(newpath);
	}
	else
	{
		sanitizeunixpath (newpath); 
	}
    DebugPrintf(SV_LOG_DEBUG, "newpath %s\n", newpath.c_str()) ;
}

bool getRestorePointsforVolume(const std::string& actualbasepath,
							   const std::string& newbasepath,
							   const std::string &retentionPath, 
							   std::map<SV_ULONGLONG, CDPEvent>& volumeRPMap, 
							   const int& rpoPolicyinHrs, const int& nRestoreCount  )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bret = false;
	std::vector<CDPEvent> eventVector;
	std::string actualretentionpath = retentionPath ; 
	std::string newretentionpath ;
    convertPath (actualbasepath, actualretentionpath, newbasepath, newretentionpath);
	SV_ULONGLONG latestRestorePointTimeStamp = 0 ;
    std::map<SV_ULONGLONG, CDPEvent> restorePointMap ;
        
	if(getCDPEventPropertyVector(newretentionpath,eventVector, nRestoreCount ))
	{
		std::vector<CDPEvent>::const_iterator eventIter = eventVector.begin();
		while(eventIter != eventVector.end())
		{
            latestRestorePointTimeStamp = (eventIter->c_eventtime > latestRestorePointTimeStamp) ? eventIter->c_eventtime : latestRestorePointTimeStamp ;
            restorePointMap.insert( std::make_pair( eventIter->c_eventtime, *eventIter ) ) ;		
			eventIter++;
		}
      
        SV_ULONGLONG hourIn100NanoSeconds = ( (SV_ULONGLONG) 3600 ) * 10000000 ;
        SV_ULONGLONG rpoPolicyIn100NanoSeconds = hourIn100NanoSeconds * rpoPolicyinHrs ;
        int count = 1 ;
        SV_ULONGLONG roundedTimeInNanoSeconds = latestRestorePointTimeStamp ;
        if( rpoPolicyinHrs != 0 ) 
        {
            roundedTimeInNanoSeconds /=   (SV_ULONGLONG) (rpoPolicyIn100NanoSeconds) ;
            roundedTimeInNanoSeconds *= rpoPolicyIn100NanoSeconds ;
            if( ( rpoPolicyinHrs %24 ) != 0 )
            {
                SV_ULONGLONG timeDiff = latestRestorePointTimeStamp - roundedTimeInNanoSeconds ;
                timeDiff /= hourIn100NanoSeconds ; 
                timeDiff *=  hourIn100NanoSeconds ;
                roundedTimeInNanoSeconds += timeDiff  ;            
            }
        } 
        std::map<SV_ULONGLONG, CDPEvent>::iterator restorePointMapEndIterator = restorePointMap.end() ; 
        std::pair<std::map<SV_ULONGLONG, CDPEvent>::iterator, bool>  restorePointIterPosition ; //restorePointMap.begin() ;
        CDPEvent tempEvent ;
        bool bInsertedAtFirstElement = false ;
        if( restorePointMap.empty() == false )
        {
            while( bInsertedAtFirstElement == false && 
                 (nRestoreCount == 0 || count <= nRestoreCount ) )
            {
                if( rpoPolicyinHrs != 0 ) 
                {
                    tempEvent.c_eventvalue = "RPO Policy Time" ;
                    restorePointIterPosition = restorePointMap.insert( std::make_pair( roundedTimeInNanoSeconds, tempEvent ) ) ;
                    if( restorePointIterPosition.first == restorePointMap.begin() )
                        bInsertedAtFirstElement = true ;
                    restorePointMapEndIterator = restorePointMap.end() ; 
                    for( int nIndex = 0 ; nIndex < count  ; nIndex++ )
                            restorePointMapEndIterator-- ;
                    if(  restorePointIterPosition.first != restorePointMapEndIterator )  
                    {
                        count++ ;                    
                        restorePointMap.erase( restorePointIterPosition.first, restorePointMapEndIterator ) ;                    
                        volumeRPMap.insert( std::make_pair( restorePointMapEndIterator->first, restorePointMapEndIterator->second ) );
                    }
                    else
                    {
                        restorePointMap.erase( restorePointIterPosition.first->first ) ;                    
                    }
                    roundedTimeInNanoSeconds -= rpoPolicyIn100NanoSeconds ;
                }
                else
                {
                    count++ ;
                    restorePointMapEndIterator-- ;
                    if( restorePointMapEndIterator == restorePointMap.begin() )
                        bInsertedAtFirstElement = true ;
                    volumeRPMap.insert( std::make_pair( restorePointMapEndIterator->first, restorePointMapEndIterator->second ) );
                }
            }
        }
		bret = true;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Failed to get the list of events for the volume. retention path %s \n",retentionPath.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bret;
}


bool getRecoveryRangesforVolume(const std::string& actualbasepath,
							    const std::string& newbasepath,
							    const std::string &retentionPath, 
							    std::map< SV_ULONGLONG, CDPTimeRange >& volumeRRMap, 
								const int& rpoPolicyinHrs, 
								const int& nRangeCount )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bret = true ;
	CDPTimeRangeMatchingCndn cndn;	
	cndn.mode(ACCURACYEXACT);
	std::string actualretentionpath = retentionPath ; 
	std::string newretentionpath ;
    convertPath (actualbasepath, actualretentionpath, newbasepath, newretentionpath);

    std::vector<CDPTimeRange> timeRangeVector;
    if(boost::filesystem::exists(newretentionpath) && 
        CDPUtil::gettimeranges(newretentionpath, cndn, timeRangeVector))
	{
        
		std::vector<CDPTimeRange>::const_iterator cdpTimeRangeBegIter = timeRangeVector.begin();
		std::vector<CDPTimeRange>::const_iterator cdpTimeRangeEndIter = timeRangeVector.end();
	    SV_ULONGLONG latestRecoveryEndTime = 0 ;
        std::map< SV_ULONGLONG, CDPTimeRange > timeRangeMap ;
		while(cdpTimeRangeBegIter!=cdpTimeRangeEndIter)
		{
			SV_ULONGLONG  diffTime = ( cdpTimeRangeBegIter->c_endtime - cdpTimeRangeBegIter->c_starttime ) ; 
			if (diffTime > 10000000  )
			{
				latestRecoveryEndTime = cdpTimeRangeBegIter->c_endtime > latestRecoveryEndTime ? cdpTimeRangeBegIter->c_endtime : latestRecoveryEndTime ;
				timeRangeMap.insert( std::make_pair( cdpTimeRangeBegIter->c_endtime, *cdpTimeRangeBegIter ) ) ;
			}
           	cdpTimeRangeBegIter++;
		}
        SV_ULONGLONG hourIn100NanoSeconds = ( (SV_ULONGLONG) 3600 ) * 10000000 ;
        SV_ULONGLONG rpoPolicyIn100NanoSeconds = hourIn100NanoSeconds * rpoPolicyinHrs ;
        int count = 1 ;
        SV_ULONGLONG roundedTimeInNanoSeconds = latestRecoveryEndTime  ;
        if( rpoPolicyinHrs != 0 ) 
        {
            roundedTimeInNanoSeconds /=   (SV_ULONGLONG) (rpoPolicyIn100NanoSeconds) ;
            roundedTimeInNanoSeconds *= rpoPolicyIn100NanoSeconds ;
            if( ( rpoPolicyinHrs % 24 ) != 0 )
            {
                SV_ULONGLONG timeDiff = latestRecoveryEndTime - roundedTimeInNanoSeconds ;
                timeDiff /= hourIn100NanoSeconds ; 
                timeDiff *=  hourIn100NanoSeconds ;
                roundedTimeInNanoSeconds += timeDiff  ;            
            }
        }
        std::map< SV_ULONGLONG, CDPTimeRange>::iterator timeRangeEndIter = timeRangeMap.end() ;
        std::pair<std::map< SV_ULONGLONG, CDPTimeRange>::iterator, bool> timeRangePosition ;
        CDPTimeRange timeRange ;
        bool bInsertedAtFirstElelment = false ;
        if( timeRangeMap.empty() == false )
        {
            while( bInsertedAtFirstElelment == false && (nRangeCount == 0 || count <= nRangeCount ) )
            {
                if ( rpoPolicyinHrs != 0 )
                {
                    timeRangePosition = timeRangeMap.insert( std::make_pair( roundedTimeInNanoSeconds, timeRange ) ) ;
                    timeRangeEndIter = timeRangeMap.end() ; 
                    if( timeRangePosition.first == timeRangeMap.begin() )
                        bInsertedAtFirstElelment = true ;
                    for( int nIndex = 0 ; nIndex < count  ; nIndex++ )
                            timeRangeEndIter-- ;
                    if(  timeRangePosition.first != timeRangeEndIter )  
                    {
                        count++ ;                    
                        timeRangeMap.erase( timeRangePosition.first, timeRangeEndIter) ;
                        volumeRRMap.insert( std::make_pair( timeRangeEndIter->first, timeRangeEndIter->second ) ) ;                    
                    }
                    else
                    {
                        timeRangeMap.erase( timeRangePosition.first->first ) ;                                       
                    }
                    roundedTimeInNanoSeconds -= rpoPolicyIn100NanoSeconds ;
                }
                else
                {
                    count++ ;
                    timeRangeEndIter-- ;
                    if( timeRangeEndIter == timeRangeMap.begin() )
                        bInsertedAtFirstElelment = true ;
                    volumeRRMap.insert( std::make_pair( timeRangeEndIter->first, timeRangeEndIter->second ) ) ;              
                }
            }
        }
	}
	return bret;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
void convertTimeStampToCXformat(std::string& ActualFormat,std::string& CxFormat, bool adjustToNextSec)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string year,month,date,Hrs,minutes,sec;
	
	std::string DateForm;
	std::string TimeForm;
	size_t pos ;
	pos = ActualFormat.find_first_of(" ");
	if(pos != std::string::npos)
	{
		 DateForm = ActualFormat.substr(0,pos);
		 TimeForm = ActualFormat.substr(pos+1,ActualFormat.length());
	}
	
	pos = DateForm.find_first_of("/");
	year = DateForm.substr(0,pos);
	DateForm = DateForm.substr(pos+1,DateForm.length());

	pos = DateForm.find_first_of("/");
	month = DateForm.substr(0,pos);
	date = DateForm.substr(pos+1,DateForm.length());

	pos = TimeForm.find_first_of(":");
	Hrs = TimeForm.substr(0,pos);

	TimeForm = TimeForm.substr(pos+1,TimeForm.length());

	pos = TimeForm.find_first_of(":");
	minutes = TimeForm.substr(0,pos);

	TimeForm = TimeForm.substr(pos+1,TimeForm.length());

	pos = TimeForm.find_first_of(":");
	sec = TimeForm.substr(0,pos);
	TimeForm = TimeForm.substr(pos+1,TimeForm.length());
	pos = TimeForm.find_first_of(":");
    std::string millsec = TimeForm.substr(0,pos);
    if( adjustToNextSec && millsec != "0" )
    {
		addOneSecondToDate ( year ,  month , date ,  Hrs , minutes , sec ); 
    }
    if( sec.length() == 1 )
    {
        sec = "0" + sec ;
    }
	if( month.length() == 1 )
    {
        month = "0" + month ;
    }
    if( date.length() == 1 )
    {
        date = "0" + date ;
    }
	if( Hrs.length() == 1 )
    {
        Hrs = "0" + Hrs ;
    }
	if( minutes.length() == 1 )
    {
        minutes = "0" + minutes ;
    }
	//desired format
	CxFormat = month+"-"+date+"-"+year+" "+Hrs+":"+minutes+":"+sec;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

std::string addOneSecondToDate (std::string &year_s , std::string &month_s , std::string &date_s , std::string &hour_s , std::string &min_s , std::string &sec_s )
{
	int year = boost::lexical_cast <int> (year_s); 
	int month = boost::lexical_cast <int> (month_s);
	int date = boost::lexical_cast <int> (date_s);
	int hour = boost::lexical_cast <int> (hour_s);
	int min = boost::lexical_cast <int> (min_s);
	int sec = boost::lexical_cast <int> (sec_s);

	if (sec == 59 )
	{
		sec = 0;
		if (min == 59)
		{
			min = 0;
			if (hour == 23)
			{
				hour = 12;
				if ( ( month == 1 || month == 3 || month == 5  || month == 7  || month == 8 || month == 10 || month == 12 ) && date == 31 )
				{
					date = 1; 
					month = month + 1; 
				}
				else if ( (month == 4 || month == 6 || month == 9 || month == 11) && date == 30 ) 
				{
					date = 1; 
					month = month + 1; 
				}
				else if (month == 2 )
				{
					if (date == 28 && !isLeapYear (year))
					{
						date = 1;
						month = month + 1 ; 
					}
					else if ( date == 29 && isLeapYear (year ) )
					{
						date = 1;
						month = month + 1 ; 
					}
					else 
					{
						date = date + 1; 
					}
				}
				
				else 
				{
					date = date + 1 ;
				}
				if (month == 13 )
				{
					month = 1; 
					year = year + 1; 
				}
			}
			else 
			{
				hour = hour + 1 ;
			}
		}
		else 
		{
			min = min + 1 ;
		}
	}
	else
	{
		sec  = sec + 1;
	}

	year_s  = boost::lexical_cast <std::string> (year); 
	month_s = boost::lexical_cast <std::string> (month);
	date_s  = boost::lexical_cast <std::string> (date);
	hour_s  = boost::lexical_cast <std::string> (hour);
	min_s   = boost::lexical_cast <std::string> (min);
	sec_s   = boost::lexical_cast <std::string> (sec);
	std::string CxFormat = month_s +"-"+date_s +"-"+year_s +" "+hour_s+":"+min_s+":"+sec_s;
	return CxFormat ; 
}

int isLeapYear (int yr)
{
	bool bRet = false ; 	
	if ((yr % 4 == 0) &&  ( !(yr % 100 == 0)|| (yr % 400 == 0) ) )
	{
		bRet = true ;
	}
	else
	{
		bRet = false ;
	}
	return bRet;
}
void insertTimeRanageMapToRRPG( ParameterGroup &arrangesPG, std::map<SV_ULONGLONG, SV_ULONGLONG>& recoveryRangeMap ) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    int rangecount = recoveryRangeMap.size();
	std::map< SV_ULONGLONG,SV_ULONGLONG >::const_iterator rcrangeMapIter = recoveryRangeMap.begin();
	while(rcrangeMapIter != recoveryRangeMap.end())
	{
		ValueType vtype;
		ParameterGroup recvyrangeschilds;

		std::string displayStarttime;
		CDPUtil::ToDisplayTimeOnConsole(rcrangeMapIter->first, displayStarttime);
		
		std::string CxFormatStartTime;
		convertTimeStampToCXformat(displayStarttime,CxFormatStartTime, true);

		vtype.value = CxFormatStartTime;
		recvyrangeschilds.m_Params.insert(std::make_pair("StartTime",vtype));

		std::string displayEndtime;
		CDPUtil::ToDisplayTimeOnConsole(rcrangeMapIter->second, displayEndtime);

		std::string CxFormatEndTime;
		convertTimeStampToCXformat(displayEndtime,CxFormatEndTime);

		vtype.value = CxFormatEndTime;
		recvyrangeschilds.m_Params.insert(std::make_pair("EndTime",vtype));	

        std::ostringstream rangeNo;
        rangeNo << "Range[" << std::setfill('0') << std::setw(6) << rangecount << "]";		
		arrangesPG.m_ParamGroups.insert(std::make_pair(rangeNo.str(),recvyrangeschilds));
		rangecount--;

		rcrangeMapIter++;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool getCommonRecoveryRanges(std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> >& volumeRecoveryRangeMap, ParameterGroup& CommonRecoveryRangesPG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SV_ULONGLONG MaxofStartTime, MinofEndTime;
	std::map<SV_ULONGLONG, CDPTimeRange> referenceRangeMap, currentRangeMap ;
	std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> >::iterator volumeRRIter = volumeRecoveryRangeMap.begin() ;
	if( volumeRecoveryRangeMap.size() > 0 )
	{
		referenceRangeMap = volumeRRIter->second ;
		if( referenceRangeMap.size() > 0 )
		{
			std::map<SV_ULONGLONG, CDPTimeRange>::iterator referenceIterator = referenceRangeMap.begin() ;
			std::map<SV_ULONGLONG,SV_ULONGLONG> StartEndTimeMap;
			while(referenceIterator != referenceRangeMap.end())
			{
				StartEndTimeMap.insert(std::make_pair(referenceIterator->second.c_starttime,referenceIterator->second.c_endtime));
				referenceIterator++;
			}
			
			volumeRRIter++ ;
			while( volumeRRIter != volumeRecoveryRangeMap.end() )
			{
				std::map<SV_ULONGLONG,SV_ULONGLONG> tempTimeMap;
				std::map<SV_ULONGLONG,SV_ULONGLONG>::iterator StartEndTimeMapIter = StartEndTimeMap.begin();
				while(StartEndTimeMapIter != StartEndTimeMap.end())
				{
					currentRangeMap = volumeRRIter->second;                     
					std::map<SV_ULONGLONG, CDPTimeRange>::iterator currentMapIter = currentRangeMap.begin();
					while(currentMapIter != currentRangeMap.end())
					{
						
						if(StartEndTimeMapIter->first >= currentMapIter->second.c_starttime)
						{
							MaxofStartTime = StartEndTimeMapIter->first;
						}
						else
						{
							MaxofStartTime = currentMapIter->second.c_starttime;
						}
						
						if(StartEndTimeMapIter->second >= currentMapIter->second.c_endtime)
						{
							MinofEndTime = currentMapIter->second.c_endtime;
						}
						else
						{
							MinofEndTime = StartEndTimeMapIter->second;
						}

						if(MinofEndTime >= MaxofStartTime )
						{
							tempTimeMap.insert(std::make_pair(MaxofStartTime,MinofEndTime));
						}
						currentMapIter++;
					}
					StartEndTimeMapIter++;
				}
				StartEndTimeMap.clear();
				StartEndTimeMap = tempTimeMap;
				volumeRRIter++;
			}
			insertTimeRanageMapToRRPG(CommonRecoveryRangesPG,StartEndTimeMap);
		}
	}
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return true ;
}



void insertTimeRangeToRRPG( ParameterGroup &recvyrangeschilds, const CDPTimeRange& timeRange, std::vector<std::string>& RecoveryRangeProperties ) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bStartTime = false,bEndTime = false,bStartTimeUTC = false,bEndTimeUTC = false,bAccuracy = false;
	std::vector<std::string>::const_iterator recvyRanPropIter = RecoveryRangeProperties.begin();
	while(recvyRanPropIter != RecoveryRangeProperties.end())
	{
		if(InmStrCmp<NoCaseCharCmp>(*recvyRanPropIter,"StartTime") == 0)
		{
			bStartTime = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*recvyRanPropIter,"EndTime") == 0)
		{
			bEndTime = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*recvyRanPropIter,"StartTimeUTC") == 0)
		{
			bStartTimeUTC = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*recvyRanPropIter,"EndTimeUTC") == 0)
		{
			bEndTimeUTC = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*recvyRanPropIter,"Accuracy") == 0)
		{
			bAccuracy = true;
		}
		recvyRanPropIter++;
	}

    ValueType vtype;

	if(bStartTime)
	{
		std::string displayStarttime;
		CDPUtil::ToDisplayTimeOnConsole(timeRange.c_starttime, displayStarttime);
		
		std::string CxFormatStartTime;
		convertTimeStampToCXformat(displayStarttime,CxFormatStartTime, true);

		vtype.value = CxFormatStartTime;
		recvyrangeschilds.m_Params.insert(std::make_pair("StartTime",vtype));
	}

	if(bEndTime)
	{
		std::string displayEndtime;
		CDPUtil::ToDisplayTimeOnConsole(timeRange.c_endtime, displayEndtime);

		std::string CxFormatEndTime;
		convertTimeStampToCXformat(displayEndtime,CxFormatEndTime);

		vtype.value = CxFormatEndTime;
		recvyrangeschilds.m_Params.insert(std::make_pair("EndTime",vtype));
	}

	std::string StarttimeUTC,EndTimeUTC;
	StarttimeUTC = boost::lexical_cast<std::string>(timeRange.c_starttime);
	EndTimeUTC = boost::lexical_cast<std::string>(timeRange.c_endtime);
	if(bStartTimeUTC)
	{
		vtype.value = StarttimeUTC;
		recvyrangeschilds.m_Params.insert(std::make_pair("StartTimeUTC",vtype));
	}
	if(bEndTimeUTC)
	{
		vtype.value = EndTimeUTC;
		recvyrangeschilds.m_Params.insert(std::make_pair("EndTimeUTC",vtype));
	}

	if(bAccuracy)
	{
		std::string mode;
		if(timeRange.c_mode == 0)
		{
			mode = ACCURACYMODE0;
		}
		else if(timeRange.c_mode == 1) 
		{
			mode = ACCURACYMODE1;
		}
		else if(timeRange.c_mode == 2) 
		{
			mode = ACCURACYMODE2;		
		}
		vtype.value = mode;
		recvyrangeschilds.m_Params.insert(std::make_pair("Accuracy",vtype));
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return ;
}

void insertRestorePointToRPPG( ParameterGroup &restrpntchilds, const CDPEvent& tempEvent,std::vector<std::string>& RestorePntProperties)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bRestorePoint = false,bRestorePointTimestamp = false,bRestorePointGUID = false,bAccuracy = false,bApplicationType = false ;
	std::vector<std::string>::const_iterator rstPntPropIter = RestorePntProperties.begin();
	while(rstPntPropIter != RestorePntProperties.end())
	{
		if(InmStrCmp<NoCaseCharCmp>(*rstPntPropIter,"RestorePoint") == 0)
		{
			bRestorePoint = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*rstPntPropIter,"RestorePointTimestamp") == 0)
		{
			bRestorePointTimestamp = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*rstPntPropIter,"RestorePointGUID") == 0)
		{
			bRestorePointGUID = true;
		}
		else if(InmStrCmp<NoCaseCharCmp>(*rstPntPropIter,"Accuracy") == 0)
		{
			bAccuracy = true;
		}
		else if (InmStrCmp<NoCaseCharCmp>(*rstPntPropIter,"ApplicationType") == 0 )
		{
			bApplicationType = true;
		}
		rstPntPropIter++;
	}
  
	std::string  RestorePointName,mode,RestorePointGuid,RestorepntTimestamp,applicationType;
    RestorePointName = tempEvent.c_eventvalue;
    RestorePointGuid = tempEvent.c_identifier;                                

    CDPUtil::ToDisplayTimeOnConsole(tempEvent.c_eventtime, RestorepntTimestamp);        						
    std::string RestorePntCxtimeFormat;
    convertTimeStampToCXformat(RestorepntTimestamp,RestorePntCxtimeFormat);

    if(tempEvent.c_eventmode == 0)
    {
        mode = ACCURACYMODE0;
    }
    else if(tempEvent.c_eventmode == 1) 
    {
        mode = ACCURACYMODE1;
    }
    else if(tempEvent.c_eventmode == 2) 
    {
        mode = ACCURACYMODE2;		
    }
	
	if (tempEvent.c_eventtype == STREAM_REC_TYPE_FS_TAG)
	{
		applicationType = "FS";
	}
	else if (tempEvent.c_eventtype == STREAM_REC_TYPE_USERDEFINED_EVENT)
	{
		applicationType = "USERDEFINED";
	}
    ValueType vtype;
	if(bRestorePoint)
	{
		vtype.value = RestorePointName;
		restrpntchilds.m_Params.insert(std::make_pair("RestorePoint",vtype));
	}
	if(bRestorePointTimestamp)
	{
		vtype.value = RestorePntCxtimeFormat;
		restrpntchilds.m_Params.insert(std::make_pair("RestorePointTimestamp",vtype));
	}
	if(bRestorePointGUID)
	{
		vtype.value = RestorePointGuid;
		restrpntchilds.m_Params.insert(std::make_pair("RestorePointGUID",vtype));
	}
	if(bAccuracy)
	{
		vtype.value = mode;
		restrpntchilds.m_Params.insert(std::make_pair("Accuracy",vtype));
	}

	if (bApplicationType)
	{
		vtype.value = applicationType;
		restrpntchilds.m_Params.insert(std::make_pair("ApplicationType",vtype));
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return ;
}

void insertRestorePointMapToRPPG( ParameterGroup &arPointsPG, std::map<SV_ULONGLONG, CDPEvent>& restorePointMap,std::vector<std::string>& RestorePntProperties )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    int RestorePntcount = restorePointMap.size();
    std::map<ULONGLONG, CDPEvent>::iterator restoreMapIter = restorePointMap.begin() ;
    while( restoreMapIter != restorePointMap.end() )
    {
		ParameterGroup restrpntchilds;
		insertRestorePointToRPPG( restrpntchilds, restoreMapIter->second ,RestorePntProperties ) ;
		std::string RestorePointGrpId = "RestorePoint[" + boost::lexical_cast<std::string>(RestorePntcount) + "]";
		arPointsPG.m_ParamGroups.insert(std::make_pair(RestorePointGrpId,restrpntchilds));
		RestorePntcount--;
        restoreMapIter++ ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return ;
}

void insertTimeRanageMapToRRPG( ParameterGroup& arrangesPG, std::map<SV_ULONGLONG, CDPTimeRange>& timeRangeMap, std::vector<std::string>& RecoveryRangeProperties )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

	int rangecount = timeRangeMap.size() ;
    std::map<SV_ULONGLONG, CDPTimeRange>::iterator timeRangeIter = timeRangeMap.begin() ;
    while( timeRangeIter != timeRangeMap.end() )
    {
		ParameterGroup recvyrangeschilds;
		insertTimeRangeToRRPG( recvyrangeschilds, timeRangeIter->second, RecoveryRangeProperties) ;
		std::ostringstream rangeNo;
        rangeNo << "Range[" << std::setfill('0') << std::setw(6) << rangecount << "]";		
		arrangesPG.m_ParamGroups.insert(std::make_pair(rangeNo.str(),recvyrangeschilds));
		rangecount--;
        timeRangeIter++ ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void FillCommonRestorePointInfo(ParameterGroup& responsePg,
                                                std::map< std::string, std::map<SV_ULONGLONG, CDPEvent> >& volumeRestorePointsMap,
                                                std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> >&  volumeRecoveryRangeMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
    ParameterGroup  commonrestoreinfoPg ;
    std::list<std::string> ProtectedVollist ;
    std::list<std::string> retentionPaths ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    protectionPairConfigPtr->GetProtectedVolumes(ProtectedVollist) ;
    std::list<std::string>::const_iterator iter = ProtectedVollist.begin() ;
    while( iter != ProtectedVollist.end() )
    {
        std::string retentionPath ;
        if( protectionPairConfigPtr->GetRetentionPathBySrcDevName(*iter, retentionPath) )
        {
            retentionPaths.push_back(retentionPath) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to get the retention path for %s\n", retentionPath.c_str()) ;
        }
        iter++ ;
    }
	std::vector<std::string> CmnRestorePntProperties;
	CmnRestorePntProperties.push_back("RestorePoint");
	CmnRestorePntProperties.push_back("RestorePointTimestamp");
	CmnRestorePntProperties.push_back("RestorePointGUID");
	CmnRestorePntProperties.push_back("Accuracy");
	CmnRestorePntProperties.push_back ("ApplicationType");
    ParameterGroup targetHostIdPg, commonRecoveryRangesPg, commonRestorePoingsPg ;
    if( !getCommonRestorePoints(volumeRestorePointsMap, commonRestorePoingsPg, CmnRestorePntProperties) )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the common restore points\n") ;
    }
    if( !getCommonRecoveryRanges(volumeRecoveryRangeMap, commonRecoveryRangesPg) )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the common recovery ranges\n") ;
    }
    if( !commonRecoveryRangesPg.m_ParamGroups.empty() || 
        !commonRestorePoingsPg.m_ParamGroups.empty() )
    {
        targetHostIdPg.m_ParamGroups.insert(std::make_pair("CommonRestorePoints", commonRestorePoingsPg)) ;
        targetHostIdPg.m_ParamGroups.insert(std::make_pair("CommonRecoveryRanges", commonRecoveryRangesPg)) ;
		commonrestoreinfoPg.m_ParamGroups.insert(std::make_pair(AgentConfig::GetInstance()->getHostId(), targetHostIdPg)) ;
    }
    responsePg.m_ParamGroups.insert(std::make_pair( "CommonRestorePointInfo", commonrestoreinfoPg)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool getCommonRestorePoints(std::map< std::string, std::map<SV_ULONGLONG, CDPEvent> >& volumeRestorePointsMap, ParameterGroup& commonRestorePointsPg ,
											std::vector<std::string>& CmnRestorePntProperties)
{
    bool retVal = true ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<SV_ULONGLONG, CDPEvent> referenceEventMap, currentEventMap ;
	std::map<std::string, std::map<SV_ULONGLONG, CDPEvent> >::iterator volumeRPIter = volumeRestorePointsMap.begin() ;
		
    if( volumeRestorePointsMap.size() > 0 )
    {
        if( volumeRPIter->second.size() > 0 )
        {
            referenceEventMap = volumeRPIter->second ;
 			int count = referenceEventMap.size() ;
            std::map<SV_ULONGLONG, CDPEvent>::iterator commonEventIter = referenceEventMap.begin() ;
            while( commonEventIter != referenceEventMap.end() )
            {
				volumeRPIter = volumeRestorePointsMap.begin() ;
				int commonCount = 0; 
				while (volumeRPIter != volumeRestorePointsMap.end())
				{
					std::map<SV_ULONGLONG, CDPEvent>::iterator referenceEventMapiter = volumeRPIter->second.begin() ;
					while (referenceEventMapiter != volumeRPIter->second.end() )
					{
						if ( commonEventIter->second.c_eventvalue == referenceEventMapiter->second.c_eventvalue )
						{
							commonCount++;
							break ;
						}
						referenceEventMapiter ++; 
					}
					volumeRPIter++; 
				}			
				if( commonCount ==  volumeRestorePointsMap.size())
				{
					ParameterGroup restrpntchilds;
					insertRestorePointToRPPG( restrpntchilds, commonEventIter->second ,CmnRestorePntProperties) ;
					std::string RestorePointGrpId = "RestorePoint[" + boost::lexical_cast<std::string>(count) + "]";
					commonRestorePointsPg.m_ParamGroups.insert(std::make_pair(RestorePointGrpId,restrpntchilds));
					count--;
				}
				commonEventIter++ ;
            }
        }
    }
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retVal ;
}
std::string convertToHumanReadableFormat (SV_ULONGLONG requiredSpace)
{
	std::string humanReadableString;
	double dlrequiredSpace;
	char number [20];
	if (requiredSpace <  1024 )
	{
		humanReadableString = boost::lexical_cast <std::string> (requiredSpace);
		humanReadableString += " bytes "; 
	}
	else if (requiredSpace <  1024 * 1024 )
	{
		dlrequiredSpace = (double) requiredSpace / (1024 );
		inm_sprintf_s(number, ARRAYSIZE(number), "%.2f", dlrequiredSpace);
		humanReadableString = boost::lexical_cast <std::string> (number);
		humanReadableString += " KB "; 
	}
	else if (requiredSpace <  1024 * 1024 * 1024)
	{
		dlrequiredSpace = (double) requiredSpace / (1024 * 1024 );
		inm_sprintf_s(number, ARRAYSIZE(number), "%.2f", dlrequiredSpace);
		humanReadableString = boost::lexical_cast <std::string> (number);
		humanReadableString += " MB "; 
	}
	else 
	{
		dlrequiredSpace = (double) requiredSpace / (1024 * 1024 * 1024);
		inm_sprintf_s(number, ARRAYSIZE(number), "%.2f", dlrequiredSpace);
		humanReadableString = boost::lexical_cast <std::string> (number);
		humanReadableString += " GB ";
	}
	return humanReadableString;
}
bool GetRecoveryFeature(FunctionInfo& request, std::string& recoveryFeature)
{
    bool bRet = true ;
    ConstParametersIter_t paramIter = request.m_RequestPgs.m_Params.find("RecoveryFeaturePolicy") ;
    if( paramIter != request.m_RequestPgs.m_Params.end() )
    {
        recoveryFeature = paramIter->second.value ;
    }
    else
        bRet = false ;

    return bRet ;
}

bool GetConsistencyOptions(FunctionInfo& request, ConsistencyOptions& consistencyOptions)
{
    bool bRet = true ;
    ParameterGroupsIter_t pgIter ;
    pgIter = request.m_RequestPgs.m_ParamGroups.find("ConsistencyPolicy") ;
    if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
    {
        Parameters_t& consistencyParams = pgIter->second.m_Params ;
        ConstParametersIter_t paramIter ;
        if( (paramIter = consistencyParams.find( "ConsistencyLevel" ))
                != consistencyParams.end() )
        {
            consistencyOptions.tagType = paramIter->second.value ;
        }
        else
        {
            consistencyOptions.tagType = "FileSystem" ;
        }
        if( (paramIter = consistencyParams.find( "ConsistencySchedule" ))
                != consistencyParams.end() )
        {
            consistencyOptions.interval = paramIter->second.value ;
        }
        else
        {
            consistencyOptions.interval = "10 minutes" ;
        }
        if( (paramIter = consistencyParams.find( "Exception" ))
                != consistencyParams.end() )
        {
            consistencyOptions.exception = paramIter->second.value ;
        }
        else
        {
            consistencyOptions.exception = "00:00-00:00" ;
        }
    }
    else
        bRet = false ;
    return bRet ;
}

bool GetRPOPolicy( FunctionInfo& request,std::string& rpoPolicy) 
{
    bool bRet = true ;
    ConstParametersIter_t  paramIter ;
    if( (paramIter = request.m_RequestPgs.m_Params.find("RPOPolicy")) 
                != request.m_RequestPgs.m_Params.end() )
    {
        rpoPolicy = paramIter->second.value ;
    }
    else
        bRet = false ;

    return bRet ;
}

bool GetRetentionPolicyFromRequest(FunctionInfo& request, RetentionPolicy& retPolicy )
{
    bool bRet = true ;
    bool dailyWindow, weeklyWindow, monthlyWindow, yearlyWindow ;    
    dailyWindow = weeklyWindow = monthlyWindow = yearlyWindow = false ;
    
    ParameterGroupsIter_t  pgIter, retentionPolicyPgIter, retentionWindowIter ;
    ParameterGroup retentionPolicyPg, dailyRetWindowPg, weeklyRetWindowPg, monthlyRetWindowPg, yearlyRetWindowPg ;
    retentionPolicyPgIter = request.m_RequestPgs.m_ParamGroups.find("RetentionPolicy") ;
    ConstParametersIter_t  paramIter ;

     if( retentionPolicyPgIter != request.m_RequestPgs.m_ParamGroups.end() )
    {
        retentionPolicyPg = retentionPolicyPgIter->second ;
        retentionWindowIter = retentionPolicyPg.m_ParamGroups.find("RetentionWindow") ;
        if( retentionPolicyPg.m_ParamGroups.end() != retentionWindowIter )
        {
            if( (pgIter = retentionWindowIter->second.m_ParamGroups.find("Daily"))
                    != retentionWindowIter->second.m_ParamGroups.end() )
            {
                dailyWindow = true ;
                dailyRetWindowPg = pgIter->second ;
            }
            if( (bRet == true ) && (pgIter = retentionWindowIter->second.m_ParamGroups.find("Weekly")) 
                    != retentionWindowIter->second.m_ParamGroups.end() )
            {
                weeklyWindow = true ;
                weeklyRetWindowPg = pgIter->second ;
                if( dailyWindow == false )
                    bRet = false ;
            }
            if( (bRet == true ) && (pgIter = retentionWindowIter->second.m_ParamGroups.find("Monthly"))
                    != retentionWindowIter->second.m_ParamGroups.end() )
            {
                monthlyWindow  = true ;
                monthlyRetWindowPg = pgIter->second ;
                if( weeklyWindow == false )
                    bRet = false ;
            }
            if( (bRet == true ) && (pgIter = retentionWindowIter->second.m_ParamGroups.find("Yearly"))
                    != retentionWindowIter->second.m_ParamGroups.end() )
            {
                yearlyWindow  = true ;
                yearlyRetWindowPg = pgIter->second ;
                if( monthlyWindow == false )
                    bRet = false ;
            }
        }      

         if( bRet == true )
         {
             if( (paramIter = retentionPolicyPg.m_Params.find("RetentionType")) != 
                        retentionPolicyPg.m_Params.end() )
            {
                retPolicy.retentionType = paramIter ->second.value ;
            }
            if( (paramIter = retentionPolicyPg.m_Params.find("Duration")) != 
                    retentionPolicyPg.m_Params.end() )
            {
                retPolicy.duration = paramIter ->second.value ;
            }
            if( (paramIter = retentionPolicyPg.m_Params.find("Size")) != 
                    retentionPolicyPg.m_Params.end() )
            {
                retPolicy.size = paramIter ->second.value ;
            }
            ValueType attrName, attrValue ;
            attrName.value = "Granularity" ;
            attrValue.value = "1" ;
            ValueType countValue ;
            countValue.value = "1" ;

            if( dailyWindow == true )
            {
                ScenarioRetentionWindow dailyScenarioRetWindow ;
                FillRetentionDetailsFromPG( dailyRetWindowPg, "0",  "days" , "hrs", dailyScenarioRetWindow ) ;                
                retPolicy.retentionWindows.push_back(dailyScenarioRetWindow) ;
            }
            if( weeklyWindow == true )
            {
                ScenarioRetentionWindow weeklyScenarioRetWindow ;
                weeklyRetWindowPg.m_Params.insert( std::make_pair(attrName.value , attrValue ) ) ;
                weeklyRetWindowPg.m_Params.insert( std::make_pair( "Count", countValue ) ) ;
                FillRetentionDetailsFromPG( weeklyRetWindowPg, "1",  "weeks" , "day", weeklyScenarioRetWindow ) ;
                retPolicy.retentionWindows.push_back(weeklyScenarioRetWindow) ;
            }
            if( monthlyWindow == true )
            {
                ScenarioRetentionWindow monthlyScenarioRetWindow ;
                monthlyRetWindowPg.m_Params.insert( std::make_pair( attrName.value , attrValue ) ) ;
                monthlyRetWindowPg.m_Params.insert( std::make_pair( "Count", countValue ) ) ;
                FillRetentionDetailsFromPG( monthlyRetWindowPg, "2",  "months" , "week", monthlyScenarioRetWindow ) ;
                retPolicy.retentionWindows.push_back(monthlyScenarioRetWindow) ;
            }
            if( yearlyWindow == true )
            {
                ScenarioRetentionWindow yearlyScenarioRetWindow ;
                yearlyRetWindowPg.m_Params.insert( std::make_pair( attrName.value , attrValue ) );
                yearlyRetWindowPg.m_Params.insert( std::make_pair( "Count", countValue ) ) ;
                FillRetentionDetailsFromPG( yearlyRetWindowPg, "3",  "years" , "month", yearlyScenarioRetWindow ) ;               
                retPolicy.retentionWindows.push_back(yearlyScenarioRetWindow) ;
            }
         }
		 ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
		 RetentionPolicy  retPolicyOriginal ;
		 scenarioConfigPtr->GetRetentionPolicy(retPolicyOriginal) ;

		/* comparing whether the request came to update retention policy is with updated parameter or with same parameter 
		   as before, according to which the space calculation will be done after return of this method */
		   
		 if ( boost::iequals(retPolicyOriginal.duration, retPolicy.duration ) &&  boost::iequals(retPolicyOriginal.size, retPolicy.size )  )
		 {
			 
			 int size = 0, size1 = 0 ;
			 size = retPolicy.retentionWindows.size() ;
			 size1 = retPolicyOriginal.retentionWindows.size() ;
			
			 if (size1 == size && size1 == 0)
				 bRet = false ;
			 else if (  size1 == size  )
			 {
				
				std::list<ScenarioRetentionWindow>::const_iterator begin = retPolicy.retentionWindows.begin() ;
				std::list<ScenarioRetentionWindow>::const_iterator begin1 = retPolicyOriginal.retentionWindows.begin() ;
				while (  begin != retPolicy.retentionWindows.end() )
					if ((*begin).count == (*begin1).count && (*begin).duration == (*begin1).duration && (*begin).granularity == (*begin1).granularity && (*begin).retentionWindowId == (*begin1).retentionWindowId)
					{
						bRet = false;
						begin++ ;
						begin1++ ;
					}
					else 
					{
						bRet = true;
						break ;
					}
			 }
			else
			 {
				
				bRet = true ;
			 }
				 
		 }
		 else
			 bRet = true ;
    }
     else 
         bRet = false ;
    return bRet ;
}

bool FillRetentionDetailsFromPG( ParameterGroup& retWindowPG, std::string windowId, std::string durationType, std::string granularityType, ScenarioRetentionWindow& retenWindow )
{
    bool bValidWindow = true ;
    SV_UINT durationInNumber ;
    std::stringstream tempStr ;
    ConstParametersIter_t paramIter ;
     if( (paramIter = retWindowPG.m_Params.find("Duration")) !=
                retWindowPG.m_Params.end() )
    {
        durationInNumber = strtoul(  paramIter->second.value.c_str(), NULL, 10 ) ;      
        tempStr << durationInNumber << durationType ; 
        retenWindow.duration = tempStr.str() ;
    }
     tempStr.str("") ;
    if( (paramIter = retWindowPG.m_Params.find("Granularity")) !=
                retWindowPG.m_Params.end() )
    {
       durationInNumber = strtoul(  paramIter->second.value.c_str(), NULL, 10 ) ;      
        tempStr << durationInNumber << granularityType ; 
        retenWindow.granularity = tempStr.str() ;
    }
    if( (paramIter = retWindowPG.m_Params.find("Count")) !=
                retWindowPG.m_Params.end() )
    {
        retenWindow.count= boost::lexical_cast<int>(paramIter->second.value) ;
    }
    retenWindow.retentionWindowId = windowId ;
    return bValidWindow ;
}


bool RemoveDir(const std::string& dir)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "directory: \"%s\" \n", dir.c_str()) ;
	ACE_stat stat ;
	bool retVal ;
	ACE_DIR * ace_dir = NULL ;
	ACE_DIRENT * dirent = NULL ;
	int statRetVal = 0 ;
	
	// PR#10815: Long Path support
	if((statRetVal  = sv_stat(getLongPathName(dir.c_str()).c_str(),&stat)) != 0 )
	{
		DebugPrintf(SV_LOG_ERROR, "FILE LISTER :Unable to stat %s. ACE_OS::stat returned %d\n", dir.c_str(), statRetVal) ; 
		retVal = false;
	}
	else
	{
		ace_dir = sv_opendir(dir.c_str()) ;
		if( ace_dir != NULL )
		{
			do
			{
				dirent = ACE_OS::readdir(ace_dir) ;
				bool isDir = false ;
				if( dirent != NULL )
				{
					std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
					if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
					{
                        ACE_OS::rmdir(dir.c_str()) ;
						continue ;
					}
					std::string absPath = dir + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName ;
					if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
					{
						DebugPrintf(SV_LOG_ERROR, "FILE LISTER : ACE_OS::stat for %s is failed with error %d \n", absPath.c_str(), statRetVal) ;
						continue ;
					}
					if( stat.st_mode & S_IFDIR )
					{
						RemoveDir(absPath) ;
                        ACE_OS::rmdir(absPath.c_str()) ;
					}
					else
					{
						ACE_OS::unlink( absPath.c_str() ) ;
                        ACE_OS::rmdir( absPath.substr( 0, 
                            absPath.find_last_of( ACE_DIRECTORY_SEPARATOR_CHAR) - 1 ).c_str()) ;
					}
				}
			} while ( dirent != NULL  ) ;
			ACE_OS::closedir( ace_dir ) ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "ACE_OS::open_dir failed for %s.\n", dir.c_str()) ;
			retVal = false ;		
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}
std::string BytesAsString(SV_ULONGLONG bytes, int precision)
{
    std::string bytesStr ;
    std::string type ;
    if( bytes > ( 1024 * 1024 * 1024 ) )
    {
        float gbs = ((float) (bytes)) / ((float)(1024 * 1024 * 1024)) ;
        bytesStr = boost::lexical_cast<std::string>( gbs ) ;
        type = "GB" ;
    }
    else if( bytes > ( 1024 * 1024 ) )
    {
        float mbs = ((float) (bytes)) / ((float)(1024 * 1024 )) ;
        bytesStr = boost::lexical_cast<std::string>( mbs ) ;
        type = "MB" ;
    }
    else if( bytes > 1024 )
    {
        float kbs = ((float) (bytes)) / ((float)(1024)) ;
        bytesStr = boost::lexical_cast<std::string>( kbs ) ;
        type = "KB" ;
    }
    else
    {
        bytesStr = boost::lexical_cast<std::string>( bytes ) ;
        type = "Bytes" ;
    }
    std::string::size_type decimalIdx = 0 ;
    if( ( decimalIdx = bytesStr.find( '.' ) ) != std::string::npos)
    {
        if( bytesStr.length() - decimalIdx > precision )
        {
            bytesStr = bytesStr.substr( 0, decimalIdx + precision + 1 ) ;
        }
    }
    bytesStr += type ;
    return bytesStr ;
}
void GetRepoCapacityAndFreeSpace ( std::string &repovolumename , SV_ULONGLONG &capacity , SV_ULONGLONG &repoFreeSpace)
{
	SV_ULONGLONG quota;
	std::string modifiedrepovolumename = repovolumename; 
	if (repovolumename.length() == 1)
	{
		modifiedrepovolumename = modifiedrepovolumename + ":\\";
	}
	std::cout << "modifiedrepovolumename is " << modifiedrepovolumename.c_str() << std::endl;
	GetDiskFreeSpaceP (modifiedrepovolumename ,&quota ,&capacity,&repoFreeSpace);
	return;
}

void GetFileSystemOccupiedSizeForVolumes (std::list <std::string> &protectableVolumes, SV_ULONGLONG &fileSystemCapacity, SV_ULONGLONG &fileSystemFreeSpace)
{
	std::list<std::string>::iterator  protectableVolumesIter = protectableVolumes.begin();
	while (protectableVolumesIter != protectableVolumes.end())
	{
		SV_ULONGLONG quota = 0 ,capacity = 0 ,repoFreeSpace = 0 ;
		std::string volumename = *protectableVolumesIter;
		std::string modifiedrepovolumename = volumename; 
		if (volumename.length() == 1)
		{
			modifiedrepovolumename = modifiedrepovolumename + ":\\";
		}
		GetDiskFreeSpaceP (modifiedrepovolumename ,&quota ,&capacity,&repoFreeSpace);
		fileSystemCapacity += capacity;
		fileSystemFreeSpace += repoFreeSpace;
		protectableVolumesIter ++; 
	}
	return;
}

void GetCapacityForVolumes (std::list <std::string> &protectableVolumes, SV_ULONGLONG &fileSystemCapacity)
{
	std::list<std::string>::iterator  protectableVolumesIter = protectableVolumes.begin();
	while (protectableVolumesIter != protectableVolumes.end())
	{
		SV_ULONGLONG quota = 0 ,capacity = 0 ,repoFreeSpace = 0 ;
		std::string volumename = *protectableVolumesIter;
		std::string modifiedrepovolumename = volumename; 
		if (volumename.length() == 1)
		{
			modifiedrepovolumename = modifiedrepovolumename + ":\\";
		}
		GetDiskFreeSpaceP (modifiedrepovolumename ,&quota ,&capacity,&repoFreeSpace);
		fileSystemCapacity += capacity;
		protectableVolumesIter ++; 
	}
	return;
}
double GetSpaceMultiplicationFactorFromRequest (FunctionInfo& request)
{
	ConstParametersIter_t paramIter ;
	paramIter = request.m_RequestPgs.m_Params.find( "SpaceMultiplicationFactor") ;
	double spaceMultiplicationFactor = 1.25; 
	if( paramIter != request.m_RequestPgs.m_Params.end() )
	{
		std::string multiplicationFactor = paramIter->second.value; 
		spaceMultiplicationFactor = boost::lexical_cast <double> (multiplicationFactor); 
	}
	return spaceMultiplicationFactor;
}

/*
	Input:None
			
	OutPut:-	if required space is there in the repository 
				to protect the resized volume then return value is true 
				otherwise false. 
*/
bool SpaceCalculationForResizedVolumeV2 ( 	std::map <std::string,hostDetails> &hostDetailsMap )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string repoName, repoPath;
	std::list <std::string> protectedVolumes; 
	RetentionPolicy retentionPolicy; 
	SV_ULONGLONG repoCapacity  = 0 ,requiredSpace = 0,repoFreeSpace = 0; 
	SV_ULONGLONG protectedVolumeOverHead = 0;
	bool bSpaceAvailabililty = false; 
	spaceCheckParameters spaceParameter ; 
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetIOParameters (spaceParameter); 
	double compressionBenifits = spaceParameter.compressionBenifits; 
	double sparseSavingsFactor = 3.0; 
	double changeRate = spaceParameter.changeRate ;
	double ntfsMetaDataoverHead = 0.13; 

	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance();

	volumeConfigPtr->GetRepositoryNameAndPath( repoName, repoPath ) ;
	volumeConfigPtr->GetRepositoryCapacity (repoName, repoCapacity) ;
	volumeConfigPtr->GetRepositoryFreeSpace (repoName, repoFreeSpace);
	scenarioConfigPtr->GetProtectedVolumes(protectedVolumes) ;
	scenarioConfigPtr->GetRetentionPolicy (retentionPolicy); 

	bSpaceAvailabililty =  SpaceCalculationV2 (	repoPath,
												protectedVolumes , 
												retentionPolicy ,
												requiredSpace , 
												protectedVolumeOverHead,
												repoFreeSpace,hostDetailsMap,
												compressionBenifits,
												sparseSavingsFactor,
												changeRate,
												ntfsMetaDataoverHead);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bSpaceAvailabililty; 
}
bool CheckSpaceAvailabilityForResizedVolume(std::string volume, std::string& msg)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	RetentionPolicy retentionPolicy; 
	SV_ULONGLONG repoCapacity  = 0 ,requiredSpaceBeforeResized = 0,requiredSpaceAfterResized = 0, repoFreeSpace = 0;
	std::string repoName, repoPath;
	bool bSpaceAvailabililty = false; 
	spaceCheckParameters spaceParameter ; 
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetIOParameters (spaceParameter); 
	double compressionBenifits = spaceParameter.compressionBenifits; 
	double sparseSavingsFactor = 3.0; 
	double changeRate = spaceParameter.changeRate ;
	double ntfsMetaDataoverHead = 0.13; 

	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance();

	volumeConfigPtr->GetRepositoryNameAndPath( repoName, repoPath ) ;
	volumeConfigPtr->GetRepositoryCapacity (repoName, repoCapacity) ;
	volumeConfigPtr->GetRepositoryFreeSpace (repoName, repoFreeSpace);
	scenarioConfigPtr->GetRetentionPolicy (retentionPolicy); 
	std::list<std::string> listVolumes ;
	listVolumes.push_back(volume);
	CheckSpaceAvailability (repoPath,
							retentionPolicy ,
							listVolumes,
							requiredSpaceAfterResized,
							repoFreeSpace,
							compressionBenifits , 
							sparseSavingsFactor,
							changeRate ,
							ntfsMetaDataoverHead  );

	double dNoOptimizedDuration = 0, dOptimizedDuration = 0 ;
    GetRetentionDuration( retentionPolicy, dNoOptimizedDuration, dOptimizedDuration ) ;
	SV_ULONGLONG	fileSystemCapacity = 0 ,			
					fileSystemFreeSpace = 0,
					fileSystemOldCapacity = 0 ;
	GetFileSystemOccupiedSizeForVolumes (listVolumes, fileSystemCapacity, fileSystemFreeSpace);

	SV_ULONGLONG totalFsOccupiedSize = fileSystemCapacity - fileSystemFreeSpace ;

	double dlfileSystemOccupiedSize = boost::lexical_cast<double>(totalFsOccupiedSize); 
	dlfileSystemOccupiedSize = dlfileSystemOccupiedSize /( 1024 * 1024 * 1024 ) ;  

	ConfSchema::Object* volumeObj; 
	std::map<time_t, volumeResizeDetails> volumeResizeHistoryMap ; 
	volumeConfigPtr->GetVolumeObjectByVolumeName(volume,&volumeObj ); 
	volumeConfigPtr->GetVolumeResizeHistory(volumeObj,volumeResizeHistoryMap); 
	ProtectionPairConfigPtr protectionpairconfigptr = ProtectionPairConfig::GetInstance() ;
	VOLUME_SETTINGS::PAIR_STATE pairState ;
	protectionpairconfigptr->GetPairState(volume, pairState ) ;
	std::map<time_t, volumeResizeDetails>::reverse_iterator mapIter = volumeResizeHistoryMap.rbegin(); 
	while (mapIter != volumeResizeHistoryMap.rend())
	{
		
		 if( protectionpairconfigptr->IsPairMarkedAsResized(volume) && 
			( pairState != VOLUME_SETTINGS::DELETE_PENDING || pairState != VOLUME_SETTINGS::CLEANUP_DONE)) 
		{
			if ( volumeResizeHistoryMap.size() > 0 )
			{
				fileSystemOldCapacity = mapIter->second.m_oldVolumeCapacity ;	 
				break ;
			}
		}
		mapIter ++; 
	}
	double dltotalProtectedCapacity = ( boost::lexical_cast<double>(fileSystemOldCapacity)) / (1024 * 1024 * 1024);
	double inmageSpace = GetRepoLogsSpace ();
	SV_UINT compressVolpacks = 0;

	//logic to test the new repo supports sparse file or not
	bool supportSparseFile = false ;
	if ( SupportsSparseFiles(repoPath) )
	{
		supportSparseFile = true;
	}
					
	double volPackSize = GetVolpackSizeV2 (dlfileSystemOccupiedSize,dltotalProtectedCapacity,supportSparseFile);
	DebugPrintf ("volPackSize is %lf \n ",volPackSize);
	double NTFSMetaDataOverHead = volPackSize * 13 /100 ; 
	DebugPrintf (SV_LOG_DEBUG,"NTFSMetaDataOverHead is %lf \n ",NTFSMetaDataOverHead);
	double totalSize = 0;
	double cdpSpace = GetTotalCDPSpace (dlfileSystemOccupiedSize, 
										changeRate,
										dNoOptimizedDuration,
										sparseSavingsFactor,
										dOptimizedDuration,
										compressionBenifits); 
	totalSize = TotalSpaceRequired (volPackSize,cdpSpace,NTFSMetaDataOverHead,inmageSpace); 

	requiredSpaceBeforeResized = ( SV_ULONGLONG ) convertGBtoBytes (totalSize);
	
	DebugPrintf (SV_LOG_DEBUG,"requiredSpace before and afetr volResized are %I64d  and  %I64d in Bytes\n " , requiredSpaceBeforeResized,requiredSpaceAfterResized ); 
	DebugPrintf (SV_LOG_DEBUG,"repoFreeSpace  in Bytes is %I64d \n " , repoFreeSpace );
	if ( requiredSpaceAfterResized > requiredSpaceBeforeResized )
	{
		DebugPrintf (SV_LOG_DEBUG,"Additional space required in Bytes is %I64d \n " , requiredSpaceAfterResized - requiredSpaceBeforeResized );
		if (requiredSpaceAfterResized - requiredSpaceBeforeResized < repoFreeSpace )
			bSpaceAvailabililty = true ;	
	}
	else
	{
		bSpaceAvailabililty = true ;
		DebugPrintf (SV_LOG_DEBUG,"Don't required Additional space as volume got shrunk \n ");
	}
	if (bSpaceAvailabililty)
	{
		DebugPrintf (SV_LOG_DEBUG,"Space Check not failed for volume resized.. \n " );
	}
	else
	{
		DebugPrintf (SV_LOG_DEBUG,"Space Check got failed for volume resized.. \n " );
		SV_ULONGLONG additionalSpace = requiredSpaceAfterResized - requiredSpaceBeforeResized ;
		msg += " Additional  " ;
		msg += convertToHumanReadableFormat (additionalSpace) ;
		msg += " free space is required for volume resized operation to perform for resized volume " ;
		msg += volume ;
		msg += " but only " ;
		msg += convertToHumanReadableFormat (repoFreeSpace) ;
		msg += " is available. " ;
		msg += "Please choose a different backup location with atleast " ;
		msg += convertToHumanReadableFormat (repoFreeSpace + additionalSpace ) ;
		msg += " free. " ;
		msg += "You can also try reducing the number of days that backup data is retained." ;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bSpaceAvailabililty ;
}
/*
	Input:
		repovolumename		-- repository volume Name (For CIFS we will get valid network path)
		protectableVolumes	-- list of volumes which need to protect
		retentionPolicy		-- retentionPolicy 
		hostDetailsMap		-- map which will store what is the space used for each host.
		compressionBenifits -- Maximum compression benifits we will get, default value is 0.2 (20%)
		sparseSavingsFactor	-- default value is 3
		changeRate			-- default value is 0.05 (5%)
		ntfsMetaDataoverHead-- default value is 0.13 (13%)

	OutPut:-
		requiredSpace		-- required Space for protection
		existingOverHead	-- Already existing overhead for given Host
		repoFreeSpace		-- RepoFreeSpace for the given repovolumename
		hostDetailsMap		-- hostDetailsMap store information of each host
*/
bool SpaceCalculationV2 (	std::string repovolumename,
							std::list <std::string> &protectableVolumes , 
							RetentionPolicy& retentionPolicy ,
							SV_ULONGLONG &requiredSpace , 
							SV_ULONGLONG &existingOverHead , 
							SV_ULONGLONG &repoFreeSpace ,
							std::map <std::string,hostDetails> &hostDetailsMap, 
							double compressionBenifits, 
							double sparseSavingsFactor,
							double changeRate, 
							double ntfsMetaDataoverHead)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bRet = true; 
	//logic to test the repo supports sparse file or not
	bool supportSparseFile = false ;
	if ( SupportsSparseFiles(repovolumename) )
	{
		DebugPrintf(SV_LOG_DEBUG, "Sparsefileattribute supported  from inside spacecalculationV2 check\n") ;
		supportSparseFile = true;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Sparsefileattribute not supported from inside spacecalculationV2 check\n") ;
	}
	SV_ULONGLONG protectableVolumesCapcity = 0; //Capacity of protectable Volumes
	GetCapacityForVolumes (protectableVolumes, protectableVolumesCapcity);
	SpaceCheckForLocalHostV2(	repovolumename,
								protectableVolumes,
								retentionPolicy,
								requiredSpace,
								existingOverHead,
								repoFreeSpace,
								hostDetailsMap,
								supportSparseFile ,
								compressionBenifits,
								sparseSavingsFactor,
								changeRate,
								ntfsMetaDataoverHead) ; 
	SV_ULONGLONG requiredSpaceCIFS = 0 ;	// Amount of space required for CIFS hosts
	SV_ULONGLONG existingOverHeadCIFS = 0;	// Already existing overhead for CIFS hosts 
	
	SpaceCheckForRemoteHostsV2 (repovolumename , 
								requiredSpaceCIFS, 
								existingOverHeadCIFS , 
								repoFreeSpace , 
								hostDetailsMap,
								supportSparseFile);  

	requiredSpace += requiredSpaceCIFS ;
	SV_ULONGLONG protectedVolumeOverHead = existingOverHead + existingOverHeadCIFS ; 
	//Bug 25623 - 2.0>>Scout SSE>Space check calculation should be on 80% of backup location size because we start pruning beyond it
	if ( ( 0.8 * repoFreeSpace + protectedVolumeOverHead )  > requiredSpace )
	{
		bRet = true;
	}
	else
	{
		bRet = false;
	}
	DebugPrintf ( SV_LOG_DEBUG, " requiredSpace  %I64d " , requiredSpace ) ; 
	std::cout << repovolumename << " requiredSpace " << requiredSpace << " existingOverHead " << existingOverHead << " repoFreeSpace " << repoFreeSpace << std::endl; 
	DebugPrintf ( SV_LOG_DEBUG, " repovolumename %s  requiredSpace  %I64d  existingOverHead %I64d  repoFreeSpace %I64d \n ",repovolumename.c_str(),requiredSpace, existingOverHead ,repoFreeSpace); 
	LocalConfigurator lConfigurator;
	std::string repositoryPath = getRepositoryPath(repovolumename) ;
	std::string hostID = lConfigurator.getHostId(); 
	std::string repoLocationForHost = constructConfigStoreLocation( repositoryPath, hostID ) ;
	ConfSchemaReaderWriter::CreateInstance( "SpaceRequired", getRepositoryPath(repositoryPath), repoLocationForHost, false ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bRet; 
}

void SpaceCheckForLocalHostV2 (	std::string repovolumename,
								std::list <std::string> &protectableVolumes , 
								RetentionPolicy& retentionPolicy ,
								SV_ULONGLONG &requiredSpace , 
								SV_ULONGLONG &existingOverHead , 
								SV_ULONGLONG &repoFreeSpace , 
								std::map <std::string,hostDetails> &hostDetailsMap,
								bool supportSparseFile,
								double compressionBenifits , 
								double sparseSavingsFactor ,
								double changeRate , 
								double ntfsMetaDataoverHead
								 )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string auditmsg ;
	SV_ULONGLONG	repoCapacity = 0 ,					// RepositoryCapacity 
					fileSystemCapacity = 0 ,			// RepositoryFileSystemCapacity
					fileSystemFreeSpace = 0 ,			// fileSystemFreeSpace 
					protectedVolumeOverHeadLocal = 0;		// protectedVolume OverHead for Local Host 	+ 

	SV_ULONGLONG	requiredSpaceLocal = 0 ;			// Space Required for Local Host 

	std::map<std::string,PairDetails> pairDetailsMap;
	ScenarioConfigPtr scenarioConf = ScenarioConfig::GetInstance() ;
	GetRepoCapacityAndFreeSpace ( repovolumename ,repoCapacity, repoFreeSpace);
	GetFileSystemOccupiedSizeForVolumes (protectableVolumes, fileSystemCapacity, fileSystemFreeSpace);
	DebugPrintf (SV_LOG_DEBUG,"fileSystemCapacity is %I64d \n " , requiredSpace ); 
	DebugPrintf (SV_LOG_DEBUG,"fileSystemFreeSpace is %I64d \n " , fileSystemFreeSpace ); 
	SV_ULONGLONG totalFsOccupiedSize = fileSystemCapacity - fileSystemFreeSpace ;
	IsEnoughSpaceInRepoWithOutXMLReadV2(	repoFreeSpace, 
											fileSystemCapacity, 
											totalFsOccupiedSize, 
											retentionPolicy, 
											requiredSpace, 
											protectedVolumeOverHeadLocal,
											supportSparseFile,
											compressionBenifits , 
											sparseSavingsFactor ,
											changeRate , 
											ntfsMetaDataoverHead
											
										);
	hostDetails hostDetail;
	fillHostDetails (	fileSystemCapacity ,
						totalFsOccupiedSize,
						repoFreeSpace,
						requiredSpace,
						protectedVolumeOverHeadLocal,
						hostDetail
					); 
	std::string repositoryPath = getRepositoryPath(repovolumename) ;
	std::string localHostName = Host::GetInstance().GetHostName() ;
	hostDetailsMap.insert (std::make_pair(localHostName,hostDetail));  // max efficiency inserting
	existingOverHead = protectedVolumeOverHeadLocal; 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return;
}

/*

*/

void SpaceCheckForRemoteHostsV2 (std::string &repositoryPath , 
								 SV_ULONGLONG &totalrequiredSpaceCIFS, 
								 SV_ULONGLONG &existingOverHeadCIFS , 
								 SV_ULONGLONG &repoFreeSpace , 
								 std::map <std::string,hostDetails> &hostDetailsMap, 
								 bool supportSparseFile,
								 double compressionBenifits , 
 								 double sparseSavingsFactor ,
								 double changeRate , 
								 double ntfsMetaDataoverHead
								)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ProtectionHandler potectionHandler;
	std::list<std::map<std::string, std::string> >hostInfoList ;
    potectionHandler.GetHostDetailsToRepositoryConf(hostInfoList) ;
    std::list<std::map<std::string, std::string> >::const_iterator itB, itE ;
    itB = hostInfoList.begin() ;
    itE = hostInfoList.end() ;
	std::string auditmsg ;
	LocalConfigurator lc ;
	std::string localHostName = Host::GetInstance().GetHostName() ;
	while( itB != itE )
    {
        const std::map<std::string, std::string>& propertyPairs = *itB ;
        std::string agentHostName,agentGuid ;
		hostDetails hostDetail;
        if( propertyPairs.find("hostname") != propertyPairs.end() )
        {
            agentHostName = propertyPairs.find("hostname")->second ;
        }
		if( propertyPairs.find("hostid") != propertyPairs.end() )
		{
			agentGuid = propertyPairs.find("hostid")->second ;
		}
		
		if (lc.getHostId() !=  agentGuid )
		{
			std::string repoLocationForHost = constructConfigStoreLocation( getRepositoryPath(repositoryPath), agentGuid ) ;
			std::string lockPath = getLockPath(repoLocationForHost) ;
			RepositoryLock repolock(lockPath) ;
			repolock.Acquire() ;
			ConfSchemaReaderWriter::CreateInstance( "SpaceRequired", repositoryPath,  repoLocationForHost, false) ;
			std::list <std::string> protectedVolumes;
			ScenarioConfigPtr scenarioConf = ScenarioConfig::GetInstance() ;
			VolumeConfigPtr volumeConf = VolumeConfig::GetInstance() ;
			scenarioConf->GetProtectedVolumes(protectedVolumes); 
			if (protectedVolumes.size() > 0 )
			{
				std::string repoName,repoVol;
				std::map<std::string, PairDetails> pairDetails;
				SV_ULONGLONG repoFreeSpaceCIFS = 0 , requiredSpace = 0, requiredSpaceCIFS = 0 , protectedVolumeOverHeadCIFS = 0;
				volumeConf->GetRepositoryVolume(repoName, repoVol) ;
				volumeConf->GetRepositoryFreeSpace (repoName, repoFreeSpaceCIFS) ;
				scenarioConf->GetPairDetails(pairDetails);	
				IsEnoughSpaceInRepoV2(	repoFreeSpaceCIFS ,  
										pairDetails ,
										requiredSpaceCIFS ,
										protectedVolumeOverHeadCIFS , 
										supportSparseFile,
										compressionBenifits , 
										sparseSavingsFactor ,
										changeRate , 
										ntfsMetaDataoverHead
										);
				SV_ULONGLONG filecount = 0 , size_on_disk = 0 , logicalsize = 0; 
				sv_get_filecount_spaceusage (repoLocationForHost.c_str(), "*" ,filecount,size_on_disk,logicalsize) ; 
				std::list<std::string> volumes; 
				SV_ULONGLONG protectedCapacity = 0;
				SV_ULONGLONG fileSystemUsedSpace = 0 ; 
				scenarioConf->GetProtectedVolumes(volumes) ;
				GetTotalCapacity(volumes , protectedCapacity);
				GetTotalFileSystemUsedSpace(volumes,fileSystemUsedSpace);
				fillHostDetails (protectedCapacity ,fileSystemUsedSpace,repoFreeSpace,requiredSpaceCIFS,protectedVolumeOverHeadCIFS,hostDetail); 
				hostDetailsMap.insert (std::make_pair(agentHostName,hostDetail));  // max efficiency inserting
				//existingOverHeadCIFS += protectedVolumeOverHeadCIFS;
				existingOverHeadCIFS += size_on_disk ;
				totalrequiredSpaceCIFS += requiredSpaceCIFS;
			}
		}
        itB++ ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__) ;
	return;
}
bool SpaceCalculation ( std::string repovolumename,std::list <std::string> &protectableVolumes , RetentionPolicy& retentionPolicy ,SV_ULONGLONG &requiredSpace , SV_ULONGLONG &existingOverHead , SV_ULONGLONG &repoFreeSpace , double spaceMultiplicationFactor, std::map <std::string,hostDetails> &hostDetailsMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bRet = true;
	
	SV_ULONGLONG fileSystemCapacity, defaultRequiredSpace = 0; 
	GetCapacityForVolumes (protectableVolumes, defaultRequiredSpace);
	SpaceCheckForLocalHost (repovolumename,protectableVolumes,retentionPolicy,requiredSpace,existingOverHead,repoFreeSpace,hostDetailsMap) ; 
	SV_ULONGLONG requiredSpaceCIFS = 0 , existingOverHeadCIFS = 0;
	SpaceCheckForRemoteHosts(repovolumename,requiredSpaceCIFS,existingOverHeadCIFS ,repoFreeSpace,hostDetailsMap); 
	requiredSpace += requiredSpaceCIFS ;
	SV_ULONGLONG protectedVolumeOverHead = existingOverHead + existingOverHeadCIFS ; 
	defaultRequiredSpace = defaultRequiredSpace * spaceMultiplicationFactor ; 
	DebugPrintf ( SV_LOG_DEBUG, " spaceMultiplicationFactor  %lf " , spaceMultiplicationFactor ) ;
	DebugPrintf ( SV_LOG_DEBUG, " defaultRequiredSpace  %I64d " , defaultRequiredSpace ) ;

	if (defaultRequiredSpace > requiredSpace)
	{
		requiredSpace = defaultRequiredSpace ;
	}
	if ( ( repoFreeSpace + protectedVolumeOverHead )  > requiredSpace )
	{
		bRet = true;
	}
	else
	{
		bRet = false;
	}
	DebugPrintf ( SV_LOG_DEBUG, " requiredSpace  %I64d " , requiredSpace ) ; 
	std::cout << repovolumename << " requiredSpace " << requiredSpace << " existingOverHead " << existingOverHead << " repoFreeSpace " << repoFreeSpace << std::endl; 
	DebugPrintf ( SV_LOG_DEBUG, " repovolumename %s  requiredSpace  %I64d  existingOverHead %I64d  repoFreeSpace %I64d \n ",repovolumename.c_str(),requiredSpace, existingOverHead ,repoFreeSpace); 
	LocalConfigurator lConfigurator;
	std::string repositoryPath = getRepositoryPath(repovolumename) ;
	std::string hostID = lConfigurator.getHostId(); 
	std::string repoLocationForHost = constructConfigStoreLocation( repositoryPath, hostID ) ;
	ConfSchemaReaderWriter::CreateInstance( "SpaceRequired", getRepositoryPath(repositoryPath), repoLocationForHost, false ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bRet ;
}

void WriteIntoAuditLog (std::map <std::string,hostDetails> &hostDetailsMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::map <std::string,hostDetails>::const_iterator hostDetailsMapIter = hostDetailsMap.begin(); 
	AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
	std::string currentHostName = Host::GetInstance().GetHostName();
	bool currentHostFound = false;
	if (hostDetailsMap.size() != 0)
	{
		while( hostDetailsMapIter != hostDetailsMap.end() )
		{
			if (currentHostFound == false && hostDetailsMapIter->first == currentHostName) 
			{
				currentHostFound = true; 
			}
			auditmsg = "HostName is " + hostDetailsMapIter->first + " File System Used Space is : " + convertToHumanReadableFormat( hostDetailsMapIter->second.fileSystemUsedSpace ) + "ProtectedCapacity is " + convertToHumanReadableFormat( hostDetailsMapIter->second.protectedCapacity )  + "Repository Free Space is " + convertToHumanReadableFormat( hostDetailsMapIter->second.repoFreeSpace )  + "Required Space is " + convertToHumanReadableFormat( hostDetailsMapIter->second.requiredSpace);
			auditConfigPtr->LogAudit(auditmsg) ;
			hostDetailsMapIter++ ; 
		}
	}
	if (currentHostFound == false) 
	{
		auditmsg = "HostName is " + Host::GetInstance().GetHostName() + " File System Used Space is : " + convertToHumanReadableFormat( hostDetailsMapIter->second.fileSystemUsedSpace ) + "ProtectedCapacity is " + convertToHumanReadableFormat( hostDetailsMapIter->second.protectedCapacity )  + "Repository Free Space is " + convertToHumanReadableFormat( hostDetailsMapIter->second.repoFreeSpace )  + "Required Space is " + convertToHumanReadableFormat( hostDetailsMapIter->second.requiredSpace);
		auditConfigPtr->LogAudit(auditmsg) ;
	}
	ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
	confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
	confschemareaderwriterPtr->Sync() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;

}
void SpaceCheckForLocalHost (std::string repovolumename,std::list <std::string> &protectableVolumes , RetentionPolicy& retentionPolicy ,SV_ULONGLONG &requiredSpace , SV_ULONGLONG &existingOverHead , SV_ULONGLONG &repoFreeSpace , std::map <std::string,hostDetails> &hostDetailsMap) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string auditmsg ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	SV_ULONGLONG repoCapacity = 0 ,fileSystemCapacity = 0 ,fileSystemFreeSpace = 0 ,protectedVolumeOverHead = 0;
	SV_ULONGLONG requiredSpaceLocal = 0 , protectedVolumeOverHeadLocal = 0; 
	SV_ULONGLONG totalRequiredSpaceCIFS = 0; 
	std::map<std::string,PairDetails> pairDetailsMap;
	ScenarioConfigPtr scenarioConf = ScenarioConfig::GetInstance() ;
	GetRepoCapacityAndFreeSpace ( repovolumename ,repoCapacity, repoFreeSpace);
	GetFileSystemOccupiedSizeForVolumes (protectableVolumes, fileSystemCapacity, fileSystemFreeSpace);
	SV_ULONGLONG totalFsOccupiedSize = fileSystemCapacity - fileSystemFreeSpace ;
	if (totalFsOccupiedSize <= 5 * 1024 *1024 *1024 )
	{
		totalFsOccupiedSize =  5 * 1024 *1024 *1024; // Default Space is 5 GB
	}
	IsEnoughSpaceInRepoWithOutXMLRead (repoFreeSpace, fileSystemCapacity, totalFsOccupiedSize, retentionPolicy, requiredSpace, protectedVolumeOverHeadLocal);
	hostDetails hostDetail;
	fillHostDetails (fileSystemCapacity ,totalFsOccupiedSize,repoFreeSpace,requiredSpace,protectedVolumeOverHeadLocal,hostDetail); 
	std::string repositoryPath = getRepositoryPath(repovolumename) ;
	std::string localHostName = Host::GetInstance().GetHostName() ;
	hostDetailsMap.insert (std::make_pair(localHostName,hostDetail));  // max efficiency inserting
	existingOverHead = protectedVolumeOverHeadLocal; 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return;
}

void fillHostDetails (const SV_ULONGLONG &capacity, const SV_ULONGLONG &totalFsOccupiedSize,const SV_ULONGLONG &repoFreeSpace,const SV_ULONGLONG &requiredSpace, const SV_ULONGLONG &protectedVolumeOverHeadLocal, hostDetails &hostDetail)
{
	hostDetail.protectedCapacity = capacity; 
	hostDetail.fileSystemUsedSpace = totalFsOccupiedSize; 
	hostDetail.repoFreeSpace  = repoFreeSpace;
	hostDetail.requiredSpace  = requiredSpace;
	hostDetail.protectedVolumeOverHeadLocal  = protectedVolumeOverHeadLocal;
	return; 
}
void SpaceCheckForRemoteHosts(std::string &repositoryPath , SV_ULONGLONG &totalrequiredSpaceCIFS, SV_ULONGLONG &existingOverHeadCIFS , SV_ULONGLONG &repoFreeSpace , std::map <std::string,hostDetails> &hostDetailsMap) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ProtectionHandler potectionHandler;
	std::list<std::map<std::string, std::string> >hostInfoList ;
    potectionHandler.GetHostDetailsToRepositoryConf(hostInfoList) ;
    std::list<std::map<std::string, std::string> >::const_iterator itB, itE ;
    itB = hostInfoList.begin() ;
    itE = hostInfoList.end() ;
	std::string auditmsg ;
	LocalConfigurator lc ;
	std::string localHostName = Host::GetInstance().GetHostName() ;
	while( itB != itE )
    {
        const std::map<std::string, std::string>& propertyPairs = *itB ;
        std::string agentHostName,agentGuid ;
		hostDetails hostDetail;
        if( propertyPairs.find("hostname") != propertyPairs.end() )
        {
            agentHostName = propertyPairs.find("hostname")->second ;
        }
		if( propertyPairs.find("hostid") != propertyPairs.end() )
		{
			agentGuid = propertyPairs.find("hostid")->second ;
		}
		
		std::string repoLocationForHost = constructConfigStoreLocation( repositoryPath, agentGuid ) ;
		std::string lockPath = getLockPath(repoLocationForHost) ;
		LocalConfigurator lc ;
		if (lc.getHostId() !=  agentGuid )
		{
			RepositoryLock repolock( lockPath ) ;
			repolock.Acquire() ;
			ConfSchemaReaderWriter::CreateInstance( "SpaceRequired", getRepositoryPath(repositoryPath), repoLocationForHost, false  ) ;
			std::list <std::string> protectedVolumes;
			ScenarioConfigPtr scenarioConf = ScenarioConfig::GetInstance() ;
			VolumeConfigPtr volumeConf = VolumeConfig::GetInstance() ;
			scenarioConf->GetProtectedVolumes(protectedVolumes); 
			if (protectedVolumes.size() > 0 )
			{
				std::string repoName,repoVol;
				std::map<std::string, PairDetails> pairDetails;
				SV_ULONGLONG repoFreeSpaceCIFS = 0 , requiredSpace = 0, requiredSpaceCIFS = 0 , protectedVolumeOverHeadCIFS = 0;
				volumeConf->GetRepositoryVolume(repoName, repoVol) ;
				volumeConf->GetRepositoryFreeSpace (repoName, repoFreeSpaceCIFS) ;
				scenarioConf->GetPairDetails(pairDetails);	
				IsEnoughSpaceInRepo( repoFreeSpaceCIFS ,  pairDetails ,requiredSpaceCIFS ,protectedVolumeOverHeadCIFS );
				std::list<std::string> volumes; 
				SV_ULONGLONG protectedCapacity = 0;
				SV_ULONGLONG fileSystemUsedSpace = 0 ; 
				scenarioConf->GetProtectedVolumes(volumes) ;
				GetTotalCapacity(volumes , protectedCapacity);
				GetTotalFileSystemUsedSpace(volumes,fileSystemUsedSpace);
				fillHostDetails (protectedCapacity ,fileSystemUsedSpace,repoFreeSpace,requiredSpaceCIFS,protectedVolumeOverHeadCIFS,hostDetail); 
				hostDetailsMap.insert (std::make_pair(agentHostName,hostDetail));  // max efficiency inserting
				existingOverHeadCIFS += protectedVolumeOverHeadCIFS;
				totalrequiredSpaceCIFS += requiredSpaceCIFS;
			}
		}
        itB++ ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", __FUNCTION__) ;
	return;
}


std::string FormatSpaceCheckMsg(const std::string& apiname,
								const std::map <std::string,hostDetails>& hostDetailsMap)
{

	std::string msg ;
	msg = "The amount of free space in backup location not sufficient for the  backup period chosen. " ;
	SV_LONGLONG freeSpace , alreadyoccupiedspace, protectedVolCapacity, fsuspedspace, requiredspace ;
	freeSpace = alreadyoccupiedspace = protectedVolCapacity = fsuspedspace = requiredspace = 0 ;
	
	std::map <std::string,hostDetails>::const_iterator hostDetailIter = hostDetailsMap.begin() ;
	while( hostDetailIter != hostDetailsMap.end() )
	{
		if( freeSpace == 0 )
		{
			freeSpace = hostDetailIter->second.repoFreeSpace ;
		}
		alreadyoccupiedspace +=  hostDetailIter->second.protectedVolumeOverHeadLocal ;
		protectedVolCapacity += hostDetailIter->second.protectedCapacity ;
		fsuspedspace += hostDetailIter->second.fileSystemUsedSpace ;
		requiredspace += hostDetailIter->second.requiredSpace ;
		DebugPrintf ( SV_LOG_DEBUG, " protectedVolCapacity of each host %I64d \n" , hostDetailIter->second.protectedCapacity) ;
		DebugPrintf ( SV_LOG_DEBUG, " already used Space of each host %I64d \n" , hostDetailIter->second.protectedVolumeOverHeadLocal) ;
		DebugPrintf ( SV_LOG_DEBUG, " required space for each host %I64d \n" , hostDetailIter->second.requiredSpace) ;

		hostDetailIter ++ ;
	}
	msg += "About " ;
	msg += convertToHumanReadableFormat (requiredspace - alreadyoccupiedspace) ;
	msg += " is recommended, but only " ;
	msg += convertToHumanReadableFormat (freeSpace) ;
	msg += " is available. " ;
	msg += "Please choose a different backup location with atleast " ;
	msg += convertToHumanReadableFormat (requiredspace - alreadyoccupiedspace) ;
	msg += " free. " ;
	msg += "You can also try reducing the number of days that backup data is retained." ;
	return msg ;
}

bool checkRepositoryPathexistance (const std::string &repositoryPath)
{
	bool repoExists = false;
	try 
	{
		std::string fp = repositoryPath ;
		if( fp.length() == 1 )
		{
			fp += ":" ;
		}
		if (boost::filesystem::exists (fp.c_str())) 
		{
			repoExists = true; 
		}
		else 
		{
			repoExists = false; 
		}
	}
	catch (...)
	{
		repoExists = false; 
	}	
	return repoExists;
}

std::string getLocalTimeString ()
{
	std::string localTime; 
    time_t ltime;
    struct tm *today;

    time( &ltime );
    //today = gmtime( &ltime );
	today = localtime (&ltime);
    char timeStr[100]; /* To hold the time in  buffer form */

    // initialize 100 bytes of the character string to 0
    memset(timeStr,0,100);

	inm_sprintf_s(timeStr, ARRAYSIZE(timeStr), "(20%02d-%02d-%02d %02d:%02d:%02d):",
        today->tm_year - 100,
        today->tm_mon + 1,
        today->tm_mday,		
        today->tm_hour,
        today->tm_min,
        today->tm_sec
        );
	localTime = timeStr; 
	return localTime;
}

bool copyDir(boost::filesystem::path const & source,boost::filesystem::path const & destination )
{
    try
    {
        // Check whether the function call is valid
        if(
            !boost::filesystem::exists(source) ||
            !boost::filesystem::is_directory(source)
        )
        {
            std::cerr << "Source directory " << source.string()
                << " does not exist or is not a directory." << std::endl
            ;
            return false;
        }
        if(boost::filesystem::exists(destination))
        {
			boost::filesystem::remove_all(destination); 
        }
        // Create the destination directory
        if(!boost::filesystem::create_directory(destination))
        {
            std::cerr << "Unable to create destination directory"
                << destination.string() << std::endl
            ;
            return false;
        }
    }
    catch(boost::filesystem::filesystem_error const & e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
    // Iterate through the source directory
    for(boost::filesystem::directory_iterator file(source);
        file != boost::filesystem::directory_iterator(); ++file
    )
    {
        try
        {
            boost::filesystem::path current(file->path());
            if(boost::filesystem::is_directory(current))
            {
                // Found directory: Recursion
                if(
                    !copyDir(
                        current,
                        destination / current.filename()
                    )
                )
                {
                    return false;
                }
            }
            else
            {
                // Found file: Copy
                boost::filesystem::copy_file(
                    current,
                    destination / current.filename()
                );
            }
        }
        catch(boost::filesystem::filesystem_error const & e)
        {
            std:: cerr << e.what() << std::endl;
        }
    }
    return true;
}
/*
	This method constructs the required CDPCLI command once volume is resized
	Inparameter:- volumeName (Ex:- C,Z, C:\SRV)
	OutParameter:- resizeCommand (resizeCommand contains required CDPCLI command).
	ReturnType:- this method returns nothing.
*/

void constructVolumeResizeCDPCLICommand (const std::string &volumeName , 
										 std::string &resizeCommand)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	SV_LONGLONG rawSize; 
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	volumeConfigPtr->GetRawSize (volumeName , rawSize ); 		
	/*construct the CDPCLI voluemeResize command */
	LocalConfigurator configurator;

	std::string volumeResizeCmd = configurator.getInstallPath();
				volumeResizeCmd += ACE_DIRECTORY_SEPARATOR_CHAR ;
				volumeResizeCmd += "cdpcli.exe --virtualvolume --op=resize --devicename=" ;
				volumeResizeCmd += GetCdpClivolumeName ( volumeName ) ; 
				volumeResizeCmd += " --sizeinbytes="; 
				volumeResizeCmd += boost::lexical_cast <std::string> ( rawSize ) ; 	
	resizeCommand = volumeResizeCmd; 
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	return; 
}

/*
	This method executes the command constructed by constructVolumeResizeCDPCLICommand method
	InputParameter:- volumeName
	OutParameter:- command execution status. (true if command is successfully executed fails 
												if command execution fails)
*/
bool executeVolumeResizeCDPCLICommand (const std::string &volumeName) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false; 
	SV_ULONG dwExitCode = 0 ;
	 bool bProcessActive = true;
	std::string stdOutput = "" ;
	std::string resizeCommand ; 
	constructVolumeResizeCDPCLICommand (volumeName , resizeCommand ); 
	LocalConfigurator configurator;
	std::string volumeResizeCmd = configurator.getInstallPath();
	volumeResizeCmd += ACE_DIRECTORY_SEPARATOR_CHAR ;

	std::string logfile =	volumeResizeCmd ; 
				logfile +=  "volumeResize.log"; 
    AppCommand showSummaryCmd (resizeCommand, 0, logfile ) ;
	DebugPrintf (SV_LOG_DEBUG, "resizeCommand is %s \n ", resizeCommand.c_str()); 
	DebugPrintf (SV_LOG_DEBUG, "logfile in executeVolumeResizeCDPCLICommand %s \n ", logfile.c_str()); 
    stdOutput = "" ;
    showSummaryCmd.Run(dwExitCode,stdOutput, bProcessActive)  ;
	if (dwExitCode == 0 )
	{
		SV_LONGLONG rawSize = 0; 
		VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
		volumeConfigPtr->GetRawSize (volumeName , rawSize ); 		
		std::string virtualVolumeName = GetCdpClivolumeName(volumeName);
		volumeConfigPtr->SetRawSize(virtualVolumeName,rawSize ); 
		SV_LONGLONG latestResizeTs = 0; 
		volumeConfigPtr->GetLatestResizeTs(volumeName, latestResizeTs); 
		volumeConfigPtr->SetIsSparseFilesCreated(volumeName, latestResizeTs); 
		bRet = true; 
	}	
	else 
	{
		AlertConfigPtr alertConfigPtr ;
		alertConfigPtr = AlertConfig::GetInstance() ;
		std::string alertMsg = resizeCommand;
		alertMsg += " failed. Please do the manual steps explained in the help."; 	
		alertConfigPtr->AddAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
		DebugPrintf (SV_LOG_ERROR , "Command execution failed: %s \n ",resizeCommand.c_str()); 
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet; 
}

/*
	This method constructs the required stop filtering command once volume is resized
	Inparameter:- volumeName (Ex:- C,Z,C:\SRV )
	OutParameter:- stopFilteringCmd (stopFilteringCmd contains required stopfiltering command).
	ReturnType:- this method returns nothing.
*/

void constructStopFilteringCommand(const std::string &volumeName , 
								   std::string &stopFilteringCmd )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	/*construct stop filtering command */
	LocalConfigurator configurator;
	std::string command = configurator.getInstallPath();
				command += ACE_DIRECTORY_SEPARATOR_CHAR ;
				command += "drvutil.exe --stopf " ;
				command += volumeName;
				command += " -deletebitmap "; 
	stopFilteringCmd = command; 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return; 
}

/*
	This method executes the command constructed by constructStopFilteringCommand method
	InputParameter:- volumeName
	OutParameter:- command execution status. (true if command is successfully executed 
												fails if command execution fails)
*/
bool executeStopFilteringCommand (const std::string &volumeName) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator configurator;
	bool bRet = false ; 
	bool bProcessActive = true; 
	SV_ULONG dwExitCode = 0 ;
	std::string drvutilLog= configurator.getInstallPath();
	drvutilLog += ACE_DIRECTORY_SEPARATOR_CHAR ;

	std::string logfile =	drvutilLog; 
				logfile +=  "volumeResize.log"; 
	std::string stopFilteringCmd ; 
	constructStopFilteringCommand(volumeName , stopFilteringCmd ); 
    AppCommand showSummaryCmd (stopFilteringCmd , 0, logfile ) ;
	std::string stdOutput = "" ;
	DebugPrintf (SV_LOG_DEBUG, "stopFilteringCmd is %s \n ", stopFilteringCmd.c_str()); 
	DebugPrintf (SV_LOG_DEBUG, "logfile in  executeStopFilteringCommand is %s \n ", logfile.c_str()); 
    showSummaryCmd.Run(dwExitCode,stdOutput, bProcessActive)  ;
	if (dwExitCode == 0 )
	{
		bRet = true; 
	}	
	else 
	{
		AlertConfigPtr alertConfigPtr ;
		alertConfigPtr = AlertConfig::GetInstance() ;
		std::string alertMsg = stopFilteringCmd;
		alertMsg += " failed. Please do the manual steps explained in the help."; 	
		alertConfigPtr->AddAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
		DebugPrintf (SV_LOG_ERROR , "Command execution failed: %s \n ",stopFilteringCmd.c_str()); 
	}
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	return bRet; 
}
std::string GetCdpClivolumeName(const std::string& sourceName )
{
	std::string targetVolume; 
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetTargetVolume(sourceName,targetVolume); 
	return targetVolume ; 
}

void constructIsStopFilteringSuccess(const std::string &volumeName , 
								   std::string &stopFilteringCommand)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	SV_LONGLONG rawSize; 
	/*construct stop filtering command */
	LocalConfigurator configurator;
	std::string command = "\"";
				command += configurator.getInstallPath();
				command += ACE_DIRECTORY_SEPARATOR_CHAR ;
				command += "drvutil.exe\""; 
				command += " --gvfl ";
				command += volumeName;
	stopFilteringCommand = command; 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return; 
}

/*
This function returns 1 if stop filtering success
					  2 if stop filtering fails 
					  3 if not able to execute the command.
*/
DRIVER_STATE getStopFilteringStatus (const std::string &volumeName) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator configurator;
	DRIVER_STATE driverState = UNKNOWN; 
	bool bProcessActive = true; 
	SV_ULONG dwExitCode = 0 ;
	std::string drvutilLog= configurator.getInstallPath();
	drvutilLog += ACE_DIRECTORY_SEPARATOR_CHAR ;

	std::string logfile =	drvutilLog; 
				logfile +=  "volumeResize.log"; 
	std::string stopFilteringCmd ; 
	constructIsStopFilteringSuccess(volumeName ,stopFilteringCmd); 
    AppCommand showSummaryCmd (stopFilteringCmd, 0, logfile ) ;
	std::string stdOutput = "" ;
	DebugPrintf (SV_LOG_DEBUG , "stopFilteringCmd is %s \n ", stopFilteringCmd .c_str()); 
	DebugPrintf (SV_LOG_DEBUG , "logfile in  executeStopFilteringCommand is %s \n ", logfile.c_str()); 
	showSummaryCmd.Run(dwExitCode,stdOutput, bProcessActive)  ;
	if (dwExitCode == 0 )
	{
		size_t found; 	
		found = stdOutput.find("VSF_FILTERING_STOPPED"); 
		DebugPrintf (SV_LOG_DEBUG, "stdOut is %s \n ", stdOutput.c_str()); 
		if (found != std::string::npos)
		{
			driverState  = STOPPED_FILTERING; 
		}
		else 
		{
			driverState  = START_FILTERING;
		}
	}	
	else 
	{
		DebugPrintf (SV_LOG_ERROR , "Command Execution Failed: %s \n ", stopFilteringCmd.c_str()); 
	}
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	return driverState ; 
}

SV_LONGLONG  getVirtualVolumeRawSize (const std::string &virtualVolumeName) 
{
	LocalConfigurator localConfigurator;
    int counter = localConfigurator.getVirtualVolumesId();
    int novols = 1;
	VirVolume vVolume;
	SV_LONGLONG rawSize = 0 ; 
    while(counter != 0) {
        std::string  compressionenable = "OFF";
        char regData[26];
		inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

        std::string data = localConfigurator.getVirtualVolumesPath(regData);

        std::string sparsefilename;
        std::string volume;

        if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            counter--;
            continue;
        }
        if(novols == 1)
        {
            DebugPrintf(SV_LOG_INFO," Following is the list of virtual volumes mounted in the system\n\n");
        }
        FormatVolumeNameForCxReporting(volume);
        DebugPrintf(SV_LOG_INFO, "%d) %s\n", novols, volume.c_str());
        SV_ULONGLONG size = 0;
        ACE_stat s;
        bool vv_state = false;
        //                    VirVolume virtualvolume;
		DebugPrintf (SV_LOG_ERROR, "volume is %s \n " , volume.c_str() );
		DebugPrintf (SV_LOG_ERROR, "virtualVolumeName is %s \n " , virtualVolumeName.c_str() );

		if (boost::iequals ( volume , virtualVolumeName) ) 
		{
			if(vVolume.IsNewSparseFileFormat(sparsefilename.c_str() ))
			{
				int i = 0;
				std::stringstream sparsepartfile;
				while(true)
				{
					sparsepartfile.str("");
					sparsepartfile << sparsefilename << SPARSE_PARTFILE_EXT << i;
					DebugPrintf (SV_LOG_ERROR, "sparsepartfile is %s \n ",sparsepartfile.str().c_str() ); 
					if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
					{
						break;
					}
					size += s.st_size;
					vv_state = true;
					//added the below condition to determine whether compresion is enabled or not
	#ifdef SV_WINDOWS
					if(i==0)
					{
						WIN32_FIND_DATA data;
						HANDLE h = FindFirstFile(sparsepartfile.str().c_str(),&data);
						if (h != INVALID_HANDLE_VALUE)
						{
							if(data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
								compressionenable = "ON";
						}
						FindClose(h);
					}
	#endif
					i++;
				}
			}
			else
			{
				size  = ( sv_stat( getLongPathName(sparsefilename.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;
				if(size)
					vv_state = true;
	#ifdef SV_WINDOWS
				WIN32_FIND_DATA data;
				HANDLE h = FindFirstFile(sparsefilename.c_str(),&data);
				if (h != INVALID_HANDLE_VALUE)
				{
					if(data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
						compressionenable = "ON";
				}
				FindClose(h);
	#endif
			}
			rawSize = size;  
		}
		novols++;
		counter--;
    }
	return rawSize; 
}


bool isVsnapsMounted( ) 
{
	bool bRet = false; 
	#ifdef SV_WINDOWS
		VsnapMgr *Vsnap;
		WinVsnapMgr vsnapshot;
		Vsnap=&vsnapshot;
		std::vector<VsnapPersistInfo> vsnaps;
		 if(Vsnap->RetrieveVsnapInfo(vsnaps,  "all") )
		 {
			 if( !vsnaps.empty() )
			 {
				 bRet = true; 
			 }
		 }
	#endif
	return bRet; 
}
