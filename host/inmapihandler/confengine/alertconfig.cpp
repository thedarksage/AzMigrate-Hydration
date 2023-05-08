#include <sstream>
#include <boost/lexical_cast.hpp>
#include "alertconfig.h"
#include "portablehelpers.h"
#include "InmsdkGlobals.h"
#include "confschemareaderwriter.h"
#include "time.h"
#include "util.h"
#include "inmsafecapis.h"
AlertConfigPtr AlertConfig::m_alertConfigPtr ;

AlertConfigPtr AlertConfig::GetInstance(bool loadGrp)
{
    if( !m_alertConfigPtr )
    {
        m_alertConfigPtr.reset( new AlertConfig() ) ;
    }
	if( loadGrp )
	{
		m_alertConfigPtr->loadAlertGroup() ;
	}
    return m_alertConfigPtr ;
}

void AlertConfig::loadAlertGroup()
{
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_alertGroup = confSchemaReaderWriter->getGroup(m_alertAttrNamesObj.GetName());
}

bool AlertConfig::isModified()
{
    return m_isModified ;
}
void AlertConfig::AddTmpAlert(const std::string  & errLevel,
              const std::string& agentType,
              SV_ALERT_TYPE alertType,
              SV_ALERT_MODULE alertingModule,
              const std::string& alertMsg,
              const std::string& alertPrefix)
{
	 DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	 std::string localTimeStr = getLocalTimeString ();
	 DebugPrintf (SV_LOG_DEBUG, "Local time str is %s \n ", localTimeStr.c_str() ); 
	 ConfSchema::ObjectsIter_t alertObjBeginIter = m_tmpAlertGroup->m_Objects.begin();
	
        ConfSchema::ObjectsIter_t alertObjEndIter = m_tmpAlertGroup->m_Objects.end();

        ConfSchema::ObjectsIter_t alertObjIter = find_if(alertObjBeginIter,alertObjEndIter,
            ConfSchema::EqAttr(m_tmpAlertAttrNamesObj.GetMessageAttrName(), 
								    alertMsg.c_str()));
	    if(alertObjIter != alertObjEndIter)
        {
            std::stringstream stream ;
            stream << "alert with attribute pair " << m_tmpAlertAttrNamesObj.GetMessageAttrName();
            stream <<  alertMsg << " = found " ;            
            ConfSchema::Object palertObj = alertObjIter->second;
            ConfSchema::AttrsIter_t attrIter = palertObj.m_Attrs.find(m_tmpAlertAttrNamesObj.GetCountAttrName());
            if(attrIter != palertObj.m_Attrs.end())
            {
                std::string tempStr =  attrIter->second;          
                unsigned long count = boost::lexical_cast<unsigned long>(tempStr);
                count++;
                attrIter->second = boost::lexical_cast<std::string>(count);
            }
            attrIter = palertObj.m_Attrs.find(m_tmpAlertAttrNamesObj.GetAlertTimestampAttrName());
		    if(attrIter != palertObj.m_Attrs.end())
            {
                attrIter->second = localTimeStr; // timeStr;
            }

		    std::string objectIdString;		    
			time_t seconds;
			seconds = time (NULL);
			ACE_OS::sleep(1);
			objectIdString = boost::lexical_cast<std::string> (seconds);
		    
		    if (alertObjIter != alertObjEndIter)
		    {
			    m_tmpAlertGroup->m_Objects.erase(alertObjIter);
			    m_tmpAlertGroup->m_Objects.insert(std::make_pair(objectIdString, palertObj));
		    }
        }
        else
        {
            ConfSchema::Object o;
			std::string objectIdString; 
			time_t seconds;
			seconds = time (NULL);
			ACE_OS::sleep (1);
			objectIdString = boost::lexical_cast<std::string> (seconds);
            std::pair<ConfSchema::ObjectsIter_t, bool> objPair = m_tmpAlertGroup->m_Objects.insert(std::make_pair(objectIdString, o));
            ConfSchema::ObjectsIter_t &objIter = objPair.first;
            ConfSchema::Object &newObject = objIter->second;
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetAlertTimestampAttrName(),localTimeStr.c_str()));
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetLevelAttrName(),
                boost::lexical_cast<std::string>(errLevel)));
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetTypeAttrName(),
                boost::lexical_cast<std::string>(alertType)));
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetMessageAttrName(),alertMsg.c_str()));
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetModuleAttrName(),
                boost::lexical_cast<std::string>(alertingModule)));
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetFixStepsAttrNameAttrName(),""));
            newObject.m_Attrs.insert(std::make_pair(m_tmpAlertAttrNamesObj.GetCountAttrName(),"1"));
			ConfSchema::ObjectsIter_t alertObjIter = m_tmpAlertGroup->m_Objects.begin();
			ConfSchema::ObjectsIter_t objectToEraseIter;

			while (alertObjIter != m_tmpAlertGroup->m_Objects.end())
			{
				time_t alert_seconds = boost::lexical_cast <time_t> (alertObjIter->first);
				objectToEraseIter = alertObjIter;
				alertObjIter++;
				if (seconds - alert_seconds > 7*24*60*60 )
				{		
					m_alertGroup->m_Objects.erase(objectToEraseIter);
				}
			}

		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AlertConfig::AddAlert(const std::string  & errLevel,
              const std::string& agentType,
              SV_ALERT_TYPE alertType,
              SV_ALERT_MODULE alertingModule,
              const std::string& alertMsg,
              const std::string& alertPrefix)
{
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
    DebugPrintf(SV_LOG_DEBUG, "time %s err level %s agentType %s alert type %d alertModule %d alertMsg %s\n", timeStr, errLevel.c_str(), agentType.c_str(), alertType, alertingModule, alertMsg.c_str()) ;
    AddAlert(timeStr, errLevel.c_str(), agentType.c_str(), alertType, alertingModule, alertMsg, alertPrefix) ;
}
void AlertConfig::RemoveOldAlerts(const std::string& alertPrefix)
{
    if( !alertPrefix.empty() )
    {
        ConfSchema::ObjectsIter_t alertObjBeginIter = m_alertGroup->m_Objects.begin() ;
        while( alertObjBeginIter != m_alertGroup->m_Objects.end() )
        {
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = alertObjBeginIter->second.m_Attrs.find( m_alertAttrNamesObj.GetMessageAttrName() ) ;
            if( attrIter->second.find( alertPrefix ) != std::string::npos )
            {
                m_alertGroup->m_Objects.erase(alertObjBeginIter) ;
                break ;
            }
            alertObjBeginIter++ ;
        }
    }
}
void AlertConfig::SetAlertGrp(ConfSchema::Group& grp)
{
	m_alertGroup = &grp ;
}

void AlertConfig::AddAlert(const std::string & timeStr,
              const std::string  & errLevel,
              const std::string& agentType,
              SV_ALERT_TYPE alertType,
              SV_ALERT_MODULE alertingModule,
              const std::string& alertMsg,
              const std::string& alertPrefix)
{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
		std::string localTimeStr = getLocalTimeString ();
		DebugPrintf (SV_LOG_DEBUG, "Local time str is %s \n ", localTimeStr.c_str() ); 
       /* RemoveOldAlerts(alertPrefix) ;
        ConfSchema::ObjectsIter_t alertObjBeginIter = m_alertGroup->m_Objects.begin();*/
		ConfSchema::ObjectsIter_t alertObjBeginIter = m_alertGroup->m_Objects.begin();
		unsigned long  catchCount = 0 ;
		//logic to catch the count value of the old alert and than remove that alert
		if( !alertPrefix.empty() )
		{
			 while( alertObjBeginIter != m_alertGroup->m_Objects.end() )
			{
				ConfSchema::AttrsIter_t attrIter ;
				attrIter = alertObjBeginIter->second.m_Attrs.find( m_alertAttrNamesObj.GetMessageAttrName() ) ;
				if( attrIter->second.find( alertPrefix ) != std::string::npos )
				{
				  ConfSchema::AttrsIter_t attrIter1 = alertObjBeginIter->second.m_Attrs.find( m_alertAttrNamesObj.GetCountAttrName() ) ;
				   if(attrIter1 != alertObjBeginIter->second.m_Attrs.end())
					{
						std::string tempStr1 =  attrIter1->second;          
						catchCount = boost::lexical_cast<unsigned long>(tempStr1);
						m_alertGroup->m_Objects.erase(alertObjBeginIter) ;
						break ;
					}
				}
				alertObjBeginIter++ ;
			}
		}	
       // RemoveOldAlerts(alertPrefix) ;// this is being commented as nearly same logic of "RemoveOldAlerts" method is required to add in this method just before this line. 
		alertObjBeginIter = m_alertGroup->m_Objects.begin();
        ConfSchema::ObjectsIter_t alertObjEndIter = m_alertGroup->m_Objects.end();

        ConfSchema::ObjectsIter_t alertObjIter = find_if(alertObjBeginIter,alertObjEndIter,
            ConfSchema::EqAttr(m_alertAttrNamesObj.GetMessageAttrName(), 
								    alertMsg.c_str()));
	    if(alertObjIter != alertObjEndIter)
        {
            std::stringstream stream ;
            stream << "alert with attribute pair " << m_alertAttrNamesObj.GetMessageAttrName();
            stream <<  alertMsg << " = found " ;            
            ConfSchema::Object palertObj = alertObjIter->second;
            ConfSchema::AttrsIter_t attrIter = palertObj.m_Attrs.find(m_alertAttrNamesObj.GetCountAttrName());
            if(attrIter != palertObj.m_Attrs.end())
            {
                std::string tempStr =  attrIter->second;          
                unsigned long count = boost::lexical_cast<unsigned long>(tempStr);
                count++;
                attrIter->second = boost::lexical_cast<std::string>(count);
            }
            attrIter = palertObj.m_Attrs.find(m_alertAttrNamesObj.GetAlertTimestampAttrName());
		    if(attrIter != palertObj.m_Attrs.end())
            {
                attrIter->second = localTimeStr; // timeStr;
            }

		    std::string objectIdString;		    
			time_t seconds;
			seconds = time (NULL);
			ACE_OS::sleep(1);
			objectIdString = boost::lexical_cast<std::string> (seconds);
		    
		    if (alertObjIter != alertObjEndIter)
		    {
			    m_alertGroup->m_Objects.erase(alertObjIter);
			    m_alertGroup->m_Objects.insert(std::make_pair(objectIdString, palertObj));
		    }
        }
        else
        {
            ConfSchema::Object o;
			std::string objectIdString; 
			time_t seconds;
			seconds = time (NULL);
			ACE_OS::sleep (1);
			objectIdString = boost::lexical_cast<std::string> (seconds);
            std::pair<ConfSchema::ObjectsIter_t, bool> objPair = m_alertGroup->m_Objects.insert(std::make_pair(objectIdString, o));
            ConfSchema::ObjectsIter_t &objIter = objPair.first;
            ConfSchema::Object &newObject = objIter->second;
            newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetAlertTimestampAttrName(),localTimeStr.c_str()));
            newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetLevelAttrName(),
                boost::lexical_cast<std::string>(errLevel)));
            newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetTypeAttrName(),
                boost::lexical_cast<std::string>(alertType)));
            newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetMessageAttrName(),alertMsg.c_str()));
            newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetModuleAttrName(),
                boost::lexical_cast<std::string>(alertingModule)));
            newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetFixStepsAttrNameAttrName(),""));
            /*newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetCountAttrName(),"1"));*/
			if ( catchCount == 0 )
				newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetCountAttrName(),"1"));
			else
			{
			   catchCount++;
               newObject.m_Attrs.insert(std::make_pair(m_alertAttrNamesObj.GetCountAttrName(),boost::lexical_cast<std::string>(catchCount))); 
			}
			ConfSchema::ObjectsIter_t alertObjIter = m_alertGroup->m_Objects.begin();
			ConfSchema::ObjectsIter_t objectToEraseIter;

			while (alertObjIter != m_alertGroup->m_Objects.end())
			{
				time_t alert_seconds = boost::lexical_cast <time_t> (alertObjIter->first);
				objectToEraseIter = alertObjIter;
				alertObjIter++;
				if (seconds - alert_seconds > 7*24*60*60 )
				{		
					m_alertGroup->m_Objects.erase(objectToEraseIter);
				}
			}

		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

ConfSchema::Group& AlertConfig::GetAlertGroup( ) 
{
    return *m_alertGroup ;
}

AlertConfig::AlertConfig()
{
    m_isModified = false ;
    m_alertGroup = NULL ;
	m_tmpAlertGroup = new ConfSchema::Group() ;
}

void AlertConfig::processAlert (std::string &alertMsg,std::string &alertPrefix)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
	if (alertMsg.find ("Running out of free space on retention folder ") != std::string::npos && 
	( alertMsg.find (" Retention folder was cleaned to free up space.") != std::string::npos ||
	  alertMsg.find (". Retention data was purged to free up space.") != std::string::npos ))
	{
		int index ;
		std::string temp = "retention folder";
		if (( index = alertMsg.find(temp))  != std::string::npos)
		{
			alertMsg.replace ( index ,temp.length() , "backup location folder" );
		}
		temp = "Retention folder" ;
		if (( index = alertMsg.find(temp))  != std::string::npos)
		{
			alertMsg.replace ( index ,temp.length() , "backup location folder" );
		}
				
	}

	if (alertMsg.find ("Running out of free space on retention folder ") != std::string::npos && 
	alertMsg.find ("Free Space :") != std::string::npos)
	{
		int index ;
		std::string temp = "retention folder";
		if (( index = alertMsg.find(temp))  != std::string::npos)
		{
			alertMsg.replace ( index ,temp.length() , "backup location folder" );
		}
		size_t found = alertMsg.find ("Free Space :");
		alertPrefix = alertMsg.substr(0,found);
		
	}
	if (alertMsg.find ("Differential file : ") != std::string::npos && 
	alertMsg.find ("Please do a resync") != std::string::npos)
	{
		int index ;
		std::string temp = "Please do a resync";
		if (( index = alertMsg.find(temp))  != std::string::npos)
		{
			alertMsg.replace ( index ,temp.length() , "Please do Re-initialize once" );
		}
	}
	if( alertMsg.find ("Disk usage on retention folder ") != std::string::npos &&
		alertMsg.find ("now at ") != std::string::npos &&
		alertMsg.find (", alert threshold is configured at ") != std::string::npos )
	{
		int index ;
		std::string temp = "Disk usage on retention folder ";
		alertMsg.replace (0,temp.length(),"Disk usage on backup location ");
		temp = "now at ";
		if (( index = alertMsg.find(temp))  != std::string::npos)
		{
			alertMsg.replace ( index ,temp.length() , "is now at " );
		}
		temp = ", alert threshold is configured at ";
		if (( index = alertMsg.find(temp))  != std::string::npos)
		{
			alertMsg.replace (index , temp.length() , " and has exceeded threshold of ");
		}
		size_t found = alertMsg.find ("is now at ");
		alertPrefix = alertMsg.substr(0,found);
	}
	if( alertMsg.find (" Additional  ") != std::string::npos &&
		alertMsg.find (" free space is required for volume resized operation to perform for resized volume ") != std::string::npos &&
		alertMsg.find (" but only ") != std::string::npos )
	{
		size_t found = alertMsg.find (" but only ") ;
		alertPrefix = alertMsg.substr(0,found);
	}
	if( alertMsg.find ("LocalDirectSync Copy failed for source ") != std::string::npos &&
		alertMsg.find (" target ") != std::string::npos &&
		alertMsg.find (" with error ") != std::string::npos )
	{
		size_t found = alertMsg.find (" target ") ;
		alertPrefix = alertMsg.substr(0,found);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return;
}

void AlertConfig::RemoveAlerts()
{
	m_alertGroup->m_Objects.clear();
	return;
}