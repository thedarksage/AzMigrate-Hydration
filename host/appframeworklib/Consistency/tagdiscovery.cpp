#include "util/common/util.h"
#include "controller.h"
#include "tagdiscovery.h"
#include "VacpUtil.h"
#include <sstream>
#include "serializationtype.h"

TagDiscover::TagDiscover()
{
	m_vxConfigurator = NULL;
	m_bconfig = false;
}

SVERROR TagDiscover::init( )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	if( initConfig() == SVS_OK )
	{
		retStatus = SVS_OK;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;	
}

SVERROR TagDiscover::initConfig( )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	do
	{
		try 
		{
            if( m_vxConfigurator  != NULL )
            {
                m_vxConfigurator = NULL ;
                m_bconfig = false ;
            }
            m_vxConfigurator->instanceFlag = false ;
            AppLocalConfigurator configurator ;
            ESERIALIZE_TYPE type = configurator.getSerializerType();
			if(!::InitializeConfigurator(&m_vxConfigurator, USE_ONLY_CACHE_SETTINGS, type))
			{
				break;
			}
			m_bconfig = true;
			retStatus = SVS_OK ;
		} 
		catch ( ContextualException& ce )
		{
			DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
		}
		catch( std::exception const& e )
		{
			DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
		}
		catch(...) 
		{
			DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
		}
	}while(0);

	if( retStatus == SVS_FALSE )
	{
		DebugPrintf(SV_LOG_INFO, "Replication pair information is not available in local cache.\n" );
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::getRetentionDB( const std::string& volume, std::string& retentionDBPath )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	retentionDBPath = m_vxConfigurator->getCdpDbName(volume);
	if( retentionDBPath.empty() == false )
	{
		if( !resolvePathAndVerify( retentionDBPath ) )
		{
			DebugPrintf( SV_LOG_INFO, "Retention Database Path %s for volume %s is not yet created\n", retentionDBPath.c_str(),  volume.c_str() ) ;
		}
        else
        {
            retStatus = SVS_OK;
        }
	}	
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::getListOfEvents( const std::list<std::string>& volList,  std::map<std::string, std::vector<CDPEvent> >& eventInfoMap )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	if( m_bconfig )
	{
		std::list<std::string>::const_iterator volListBegIter = volList.begin();
		std::list<std::string>::const_iterator volListEndIter = volList.end();
		while( volListBegIter != volListEndIter )
		{
			std::string voumePathName;
			voumePathName = *volListBegIter;
			sanitizeVolumePathName(voumePathName);
			voumePathName[0] = toupper(voumePathName[0]);
			bool shouldRun = false;
			CDPUtil::ShouldOperationRun((*m_vxConfigurator), voumePathName, "listevents", shouldRun );
			if( shouldRun )
			{
				std::string retentionDBPath;
				if( getRetentionDB( voumePathName, retentionDBPath ) == SVS_OK )
				{
					CDPMarkersMatchingCndn cndn;
					std::vector<CDPEvent> eventVector;
					if( CDPUtil::getevents( retentionDBPath, cndn, eventVector ) )
					{
						eventInfoMap.insert(std::make_pair( voumePathName, eventVector ));
						retStatus = SVS_OK;
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get the list of events for the volume %s \n", volListBegIter->c_str() );
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get the retention DB for the voume %s \n", volListBegIter->c_str());			
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "The Retention DB for volume %s are being moved \n", volListBegIter->c_str() );
			}
			volListBegIter++;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Config was not initialized Successfully.\n");
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::getRecentConsistencyPoint( const std::list<std::string>& volList, const std::string& appName, CDPEvent& cdpCommonEvent  )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	if( m_bconfig )
	{
		std::vector<std::string> retentionDBPathVect;
		std::list<std::string>::const_iterator volListBegIter = volList.begin();
		std::list<std::string>::const_iterator volListEndIter = volList.end();
		while( volListBegIter != volListEndIter )
		{
			std::string voumePathName;
			voumePathName = *volListBegIter;
			sanitizeVolumePathName(voumePathName);
			voumePathName[0] = toupper(voumePathName[0]);
			bool shouldRun = false;
			CDPUtil::ShouldOperationRun((*m_vxConfigurator), voumePathName, "listevents", shouldRun );
			if( shouldRun )
			{
				std::string retentionDBPath;
				if( getRetentionDB( voumePathName, retentionDBPath) == SVS_OK )
				{
					retentionDBPathVect.push_back(retentionDBPath);
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get the retention DB for the voume %s \n", volListBegIter->c_str());			
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "The Retention DB for volume %s are being moved \n", volListBegIter->c_str() );
			}
			volListBegIter++;
		}
		if(retentionDBPathVect.empty() == false)
		{
			SV_EVENT_TYPE type;
			if( VacpUtil::AppNameToTagType(appName, type) == true )
			{
				if( CDPUtil::MostRecentConsistentPoint( retentionDBPathVect, cdpCommonEvent, type, 0, 0 ) )
				{
					retStatus = SVS_OK;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "CDPUtil::MostRecentConsistentPoint Failed. \n");
				}
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Config was not initialized Successfully.\n");
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}
SVERROR TagDiscover::getRecentCommonConsistencyPoint( const std::list<std::string>& volList, SV_ULONGLONG& eventTime )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_FALSE;
	if( m_bconfig )
	{
		std::vector<std::string> retentionDBPathVect;
		std::list<std::string>::const_iterator volListBegIter = volList.begin();
		std::list<std::string>::const_iterator volListEndIter = volList.end();
		while( volListBegIter != volListEndIter )
		{
			std::string voumePathName;
			voumePathName = *volListBegIter;
			sanitizeVolumePathName(voumePathName);
			voumePathName[0] = toupper(voumePathName[0]);
			bool shouldRun = false;
			CDPUtil::ShouldOperationRun((*m_vxConfigurator), voumePathName, "listevents", shouldRun );
			if( shouldRun )
			{
				std::string retentionDBPath;
				if( getRetentionDB( voumePathName, retentionDBPath) == SVS_OK )
				{
					retentionDBPathVect.push_back(retentionDBPath);
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get the retention DB for the voume %s \n", volListBegIter->c_str());			
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "The Retention DB for volume %s are being moved \n", volListBegIter->c_str() );
			}
			volListBegIter++;
		}
		if(retentionDBPathVect.empty() == false)
		{
			SV_ULONGLONG sequenceId =0;
			if(CDPUtil::MostRecentCCP( retentionDBPathVect, eventTime, sequenceId ) )
			{
				retStatus = SVS_OK;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "CDPUtil::MostRecentCCP Failed. \n");
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Config was not initialized Successfully.\n");
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}
SVERROR TagDiscover::getTimeRanges( const std::list<std::string>& volList, const ACCURACYEMODE& mode, std::map< std::string, std::vector<CDPTimeRange> >& timeRangeInfoMap )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
	if( m_bconfig )
	{
		std::list<std::string>::const_iterator volListBegIter = volList.begin();
		std::list<std::string>::const_iterator volListEndIter = volList.end();
		CDPTimeRangeMatchingCndn cndn;
		if( mode != ACCURACY_ALL )
		{
			cndn.mode( mode );
		}
		while( volListBegIter != volListEndIter )
		{
			std::string voumePathName;
			voumePathName = *volListBegIter;
			sanitizeVolumePathName(voumePathName);
			voumePathName[0] = toupper(voumePathName[0]);
			bool shouldRun = false;
			CDPUtil::ShouldOperationRun((*m_vxConfigurator), voumePathName, "listevents", shouldRun );
			if( shouldRun )
			{
				std::string retentionDBPath;
				if( getRetentionDB( voumePathName, retentionDBPath) == SVS_OK )
				{
					std::vector<CDPTimeRange> timeRangeVector;
					if( CDPUtil::gettimeranges( retentionDBPath, cndn, timeRangeVector ) == true )
					{
						timeRangeInfoMap.insert( std::make_pair( voumePathName, timeRangeVector ));
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get the retention DB for the voume %s \n", volListBegIter->c_str());			
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "The Retention DB for volume %s are being moved \n", volListBegIter->c_str() );
			}
			volListBegIter++;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Config was not initialized Successfully.\n");
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

bool TagDiscover::isTagReached(std::list<std::string>& volList,const std::string& tagName)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
    bool bRet = false;
    if( init() != SVS_OK )
    {
        DebugPrintf(SV_LOG_ERROR, "TagDiscover Initializaiton failed\n") ;
    }
    else
    {
        SV_UINT timeOut = 0 ;
        while( !bRet && !Controller::getInstance()->QuitRequested(60) )
        {
            std::map<std::string, std::vector<CDPEvent> > eventInfoMap;
            getListOfEvents(volList,eventInfoMap);
            std::map<std::string,std::vector<CDPEvent> >::iterator eventMapIter = eventInfoMap.begin();
            DebugPrintf(SV_LOG_DEBUG,"Checking Whether tag %s is  reached or not \n",tagName.c_str());
            while(eventMapIter != eventInfoMap.end())
            {
                std::vector<CDPEvent>::iterator eventIter = eventMapIter->second.begin();
                bool bEventRet = false;
                while(eventIter != eventMapIter->second.end())
                {
                    if(tagName.compare(eventIter->c_eventvalue) == 0)
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Tag %s is reached for the volume %s\n",tagName.c_str(),eventMapIter->first.c_str());
                        bEventRet = true;
                        break;
                    }
                    eventIter++;
                    if(eventIter == eventMapIter->second.end())
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Tag %s is not yet reached for the volume %s\n",tagName.c_str(),eventMapIter->first.c_str());
                        bEventRet = false;
                    }
                }
                if(bEventRet == false)
                {
                    break;
                }
                eventMapIter++;
                if(eventMapIter == eventInfoMap.end())
                {
                    bRet = true;
                }
            }
            timeOut += 60 ;
            if( timeOut > 300 )
            {
                DebugPrintf(SV_LOG_WARNING, "Timed-out at 300 seconds while waiting for the tag %s\n", tagName.c_str()) ;
                bRet = true;
        	}
    	}
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
    return bRet;
}

SVERROR TagDiscover::printListOfEvents( const std::map<std::string, std::vector<CDPEvent> >& eventInfoMap )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );

	SVERROR retStatus = SVS_OK;
	std::map<std::string, std::vector<CDPEvent> >::const_iterator eventInfoMapBegIter = eventInfoMap.begin();
	std::map<std::string, std::vector<CDPEvent> >::const_iterator eventInfoMapEndIter = eventInfoMap.end();
	while( eventInfoMapBegIter != eventInfoMapEndIter )
	{
		
		std::string volumePathName = eventInfoMapBegIter->first;
		std::cout << std::endl << "TAGS TABLE FOR VOLUME:" << volumePathName << std::endl << std::endl;
		std::cout << "-------------------------------------------------------------------------------\n";
		std::cout << std::setw(5)  << std::setiosflags( std::ios::left ) << "No."
				<< std::setw(30) << std::setiosflags( std::ios::left ) << "TimeStamp(GMT)"
				<< std::setw(13) << std::setiosflags( std::ios::left ) << "Accuracy"
				<< std::setw(12) << std::setiosflags( std::ios::left ) << "Application"     
				<< std::setiosflags( std::ios::left ) << "Event" << std::endl;        
		std::cout << "-------------------------------------------------------------------------------\n";	
		std::vector<CDPEvent>::const_iterator cdpEventBegIter = eventInfoMapBegIter->second.begin(); 
		std::vector<CDPEvent>::const_iterator cdpEventEndIter = eventInfoMapBegIter->second.end();
		int NoOfEvents = 0;
        while( cdpEventBegIter != cdpEventEndIter && NoOfEvents < 10)
		{
			std::string displaytime = "", mode = "", appname = "";
			VacpUtil::TagTypeToAppName(cdpEventBegIter->c_eventtype, appname);
			CDPUtil::ToDisplayTimeOnConsole(cdpEventBegIter->c_eventtime, displaytime);
			if(cdpEventBegIter->c_eventmode == 0)
			{
				mode = ACCURACYMODE0;
			}
			else if(cdpEventBegIter->c_eventmode == 1) 
			{
				mode = ACCURACYMODE1;
			}
			else if(cdpEventBegIter->c_eventmode == 2) 
			{
				mode = ACCURACYMODE2;		
			}
			if(cdpEventBegIter->c_eventmode != 0 )
			{
				DebugPrintf( SV_LOG_DEBUG, " Tag is not exact skipping mode=%d,event no=%d \n",cdpEventBegIter->c_eventmode,cdpEventBegIter->c_eventno);
				cdpEventBegIter++;
				continue;
			}
			std::cout << std::setw(5)  << std::setiosflags( std::ios::left ) << cdpEventBegIter->c_eventno
						<< std::setw(30) << std::setiosflags( std::ios::left ) << displaytime
						<< std::setw(13) << std::setiosflags( std::ios::left ) << mode
						<< std::setw(12) << std::setiosflags( std::ios::left ) << appname
						<< std::setiosflags( std::ios::left ) << cdpEventBegIter->c_eventvalue << std::endl;
			cdpEventBegIter++;
            NoOfEvents++;
			DebugPrintf( SV_LOG_DEBUG, " No of Events %d\n",NoOfEvents);
		}
		std::cout << "-------------------------------------------------------------------------------\n";
		eventInfoMapBegIter++;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::printRecentConsistencyPoint( const std::string& appName, const CDPEvent& cdpCommonEvent )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
	std::string displaytime;
	getTimeinDisplayFormat( cdpCommonEvent.c_eventtime, displaytime );
	std::cout << "Common Application Consistency Point:" << std::endl;
	std::cout << "Application Name:" << appName << std::endl;
	std::cout << "Bookmark:" << cdpCommonEvent.c_eventvalue << std::endl;
	std::cout << "Time: " << displaytime << std::endl << std::endl;
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::printRecentCommonConsistencyPoint( const SV_ULONGLONG& eventTime )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
	SV_TIME svtime;
	CDPUtil::ToSVTime( eventTime, svtime );
	std::string displaytime;
	getTimeinDisplayFormat( eventTime, displaytime );

	std::cout << std::endl << "Common Recovery Point: " << displaytime << std::endl;
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::printTimeRanges( const std::map< std::string, std::vector<CDPTimeRange> >& timeRangeInfoMap, const int& lastEntries )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
	std::map< std::string, std::vector<CDPTimeRange> >::const_iterator timeRangeInfoMapBegIter = timeRangeInfoMap.begin();
	std::map< std::string, std::vector<CDPTimeRange> >::const_iterator timeRangeInfoMapEndIter = timeRangeInfoMap.end();
	std::string displayStarttime, displayEndtime;
	std::string mode;
	while( timeRangeInfoMapBegIter != timeRangeInfoMapEndIter )
	{
		std::string volumePathName = timeRangeInfoMapBegIter->first;
		std::cout << std::endl << "TIMERANGES TABLE FOR VOLUME:" << volumePathName << std::endl << std::endl;
		std::cout << "-------------------------------------------------------------------------------\n";
		std::cout << std::setw(6)  << std::setiosflags( std::ios::left ) << "No.";
		std::cout << std::setw(30) << std::setiosflags( std::ios::left ) << "Start Time(GMT)"
				<< std::setw(30) << std::setiosflags( std::ios::left ) << "End Time(GMT)"
				<< std::setiosflags( std::ios::left ) << "Accuracy" << std::endl;
		std::cout << "-------------------------------------------------------------------------------\n";		
		std::vector<CDPTimeRange>::const_iterator cdpTimeRangeBegIter = timeRangeInfoMapBegIter->second.begin();
		std::vector<CDPTimeRange>::const_iterator cdpTimeRangeEndIter = timeRangeInfoMapBegIter->second.end();
		int index = timeRangeInfoMapBegIter->second.size();
		if( index > lastEntries )
		{
			index = lastEntries;
		}
		while( cdpTimeRangeBegIter != cdpTimeRangeEndIter && index != 0 )
		{			
			displayStarttime = "";
			getTimeinDisplayFormat( cdpTimeRangeBegIter->c_starttime, displayStarttime );
			displayEndtime = "";
            getTimeinDisplayFormat( cdpTimeRangeBegIter->c_endtime, displayEndtime );
			mode = "";
			if(cdpTimeRangeBegIter->c_mode == 0)
			{
				mode = ACCURACYMODE0;
			}
			else if(cdpTimeRangeBegIter->c_mode == 1) 
			{
				mode = ACCURACYMODE1;
			}
			else if(cdpTimeRangeBegIter->c_mode == 2) 
			{
				mode = ACCURACYMODE2;		
			}
			std::cout << std::setw(6)  << std::setiosflags( std::ios::left ) << cdpTimeRangeBegIter->c_entryno;
			std::cout << std::setw(30) << std::setiosflags( std::ios::left ) << displayStarttime
				<< std::setw(30) << std::setiosflags( std::ios::left ) << displayEndtime
				<< std::setiosflags( std::ios::left ) << mode << std::endl;
			index--;
			cdpTimeRangeBegIter++;
		}
		std::cout << "-------------------------------------------------------------------------------\n";	
		timeRangeInfoMapBegIter++;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR TagDiscover::getTimeinDisplayFormat( const SV_ULONGLONG& eventTime, std::string& timeInDisplayForm )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
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
	return retStatus;
}

std::string TagDiscover::getTagType(SV_USHORT eventType)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
    std::string tagType;
    VacpUtil::TagTypeToAppName(eventType,tagType);
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
    return tagType;
}

TagDiscover::~TagDiscover()
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	if( m_vxConfigurator != NULL )
	{
		m_vxConfigurator = NULL;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
}

