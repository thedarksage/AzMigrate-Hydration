#include "appparser.h"
#include <boost/lexical_cast.hpp>


static const char SECTION_VACP_OPTIONS[] = "vacpoptions";
static const char SECTION_RECOVERY_POINT[] = "recoverypoint";
static const char SECTION_RECOVERY_OPTIONS[] = "recoveryoptions";
static const char SECTION_SQL_INSTANCE_NAMES[] = "sqlinstancenames";
static const char SECTION_CUSTOM_PAIR_DETAILS[] = "customvolumeinformation";
static const char SECTION_REPL_PAIR_STATUS[] = "replicationpairstatus";

static const char KEY_APP_TYPE[] = "application";
static const char KEY_APP_VOLUMES[] = "applicationvolumes";
static const char KEY_CUSTOM_VOLUMES[] = "customvolumes";
static const char KEY_VOLUME_GUID[] = "volumeguid";
static const char KEY_TAG_NAME[] = "tagname";
static const char KEY_WRITER_INSTANCE[] = "writerinstance";
static const char KEY_CRASH_CONSISTENCY[] = "crashconsistency";
static const char KEY_SKIP_DISCOVERY[] = "skipdrivercheck";
static const char KEY_PERFORM_FULL_BACKUP[] = "performfullbackup";
static const char KEY_PROVIDER_GUID[] = "providerguid";
static const char KEY_SYNCHRONUS_TAG[] = "synchronoustag";
static const char KEY_TIMEOUT[] ="timeout";
static const char KEY_REMOTE_TAG[]="remotetag";
static const char KEY_SERVER_DEVICE[] = "serverdevice";
static const char KEY_SERVER_IP[] = "serverip";
static const char KEY_SERVER_PORT[] ="serverport"; 
static const char KEY_CONTEX[] = "context";
static const char KEY_CMD_LINE[] = "vacpcommand";
static const char KEY_LATEST_COMMON_TIME[] = "latestcommontime";
static const char KEY_VOLUME_NAME[] = "volumename";
static const char KEY_RECOVERY_POINT_TYPE[] = "recoverypointtype";
static const char KEY_RECOVERY_TAG_NAME[] = "tagname";
static const char KEY_RECOVERY_TAG_TYPE[] = "tagtype";
static const char KEY_RECOVERY_TIMESTAMP[] = "timestamp";
static const char KEY_INSTANCES[] = "instances";
static const char KEY_PAIRS[] = "pairs";
static const char KEY_PAIRSTATUS[] = "pairstatus";


ConfigValueParser::ConfigValueParser()
{
}
ConfigValueParser::ConfigValueParser(std::string cmdLine)    
{
  m_szconfigPath = cmdLine;
}

void ConfigValueParser::fillVacpOptionsDetails(ACE_Configuration_Heap& configHeap, 
                            ACE_Configuration_Section_Key& sectionKey,
                            AppConfigValueObject& obj ) 
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if( keyName.compare(KEY_APP_TYPE) == 0 )
            {
                obj.setApplicationType( value.c_str() ) ;
            }
            else if( keyName.compare( KEY_APP_VOLUMES ) == 0 )
            {
                if(value.find_first_of(',') != std::string::npos)
                {
                //char szBuffer[250];
                for( char* pszToken = strtok(/*szBuffer*/(char*)value.c_str(), "," ); 
	                (NULL != pszToken);
	                (pszToken = strtok( NULL, "," ))) 
                {
                       obj.setApplicationVolume( pszToken );
                }
            }
                else
                {
                    obj.setApplicationVolume (value);
                }
            }
            else if( keyName.compare( KEY_CUSTOM_VOLUMES ) == 0 )
            {
                
                if(value.find_first_of(',') != std::string::npos)
                {
                //char szBuffer[250];
                for( char* pszToken = strtok(/*szBuffer*/(char*)value.c_str(), "," ); 
	                (NULL != pszToken);
	                (pszToken = strtok( NULL, "," ))) 
                {
                       obj.setCustomVolume( pszToken );
                }
            } 
                else
                {
                     obj.setCustomVolume( value );
                }
            } 
            else if( keyName.compare( KEY_VOLUME_GUID ) == 0 )
            {
                obj.setVolumeGUID( value.c_str() );
            }
            else if( keyName.compare( KEY_TAG_NAME ) == 0 )
            {
                obj.setTagName( value.c_str() );
            } 
            else if( keyName.compare( KEY_WRITER_INSTANCE ) == 0 )
            {
                obj.setWriterInstance( value.c_str() );
            }
            else if( keyName.compare( KEY_CRASH_CONSISTENCY ) == 0 )
            {
               
                obj.setCrashConsistency( boost::lexical_cast<bool>(value.c_str()) );
            } 
            else if( keyName.compare( KEY_SKIP_DISCOVERY ) == 0 )
            {
                obj.setSkipDriverCheck( boost::lexical_cast<bool>(value.c_str()) );
            }
            else if( keyName.compare( KEY_PERFORM_FULL_BACKUP ) == 0 )
            {
                obj.setPerformFullBackup( boost::lexical_cast<bool>(value.c_str()) );
            }
            else if( keyName.compare( KEY_PROVIDER_GUID ) == 0 )
            {
                obj.setProviderGUID( value.c_str() );
            } 
            else if( keyName.compare( KEY_SYNCHRONUS_TAG ) == 0 )
            {
                obj.setSynchronousTag( value.c_str() );
            }
            else if( keyName.compare( KEY_TIMEOUT ) == 0 )
            {
                obj.setTimeout(boost::lexical_cast<SV_ULONG>(value.c_str()) );
            }
            else if( keyName.compare( KEY_SERVER_DEVICE ) == 0 )
            {
                obj.setServerDevice( value.c_str() );
            } 
             else if( keyName.compare( KEY_SYNCHRONUS_TAG ) == 0 )
            {
                obj.setSynchronousTag(boost::lexical_cast<bool>(value.c_str()) );
            }
            else if( keyName.compare( KEY_SERVER_IP ) == 0 )
            {
                obj.setIPAddress( value.c_str() );
            } 
             else if( keyName.compare( KEY_SERVER_PORT ) == 0 )
            {
                obj.setport( boost::lexical_cast<SV_ULONG>(value.c_str()) );
            }
            else if( keyName.compare( KEY_CONTEX ) == 0 )
            {
                obj.setContext( value.c_str() );
            } 
            else if( keyName.compare( KEY_CMD_LINE ) == 0 )
            {
                obj.setVacpCommand( value.c_str() );
            } 
            else if( keyName.compare( KEY_REMOTE_TAG ) == 0 )
            {
                obj.setRemoteTag( boost::lexical_cast<bool>(value.c_str()) );
            } 
            else
            {
                DebugPrintf(SV_LOG_ERROR, " Invalid Key %s under section %s . Being Ignored. \n",keyName.c_str(),sectionName.c_str());               
            }
        }
    }
}
void ConfigValueParser::fillRecoveryOptionsDetails(ACE_Configuration_Heap& configHeap, 
                            ACE_Configuration_Section_Key& sectionKey,
                            RecoveryOptions & recoveryOptions) 
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
 
    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ; 
        keyIndex++; 

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if( keyName.compare( KEY_VOLUME_NAME )== 0 )
            {
                recoveryOptions.m_VolumeName = value.c_str();
            }
            else if( keyName.compare( KEY_RECOVERY_TAG_NAME )== 0 )
            {
                recoveryOptions.m_TagName = value.c_str();
            }
            else if( keyName.compare( KEY_RECOVERY_TIMESTAMP )== 0 )
            {
                recoveryOptions.m_TimeStamp = value.c_str();
            }
            else if( keyName.compare( KEY_RECOVERY_POINT_TYPE )== 0 )
            {
                recoveryOptions.m_RecoveryPointType = value.c_str();
            }
            else if( keyName.compare( KEY_RECOVERY_TAG_TYPE )== 0 )
            {
                recoveryOptions.m_TagType = value.c_str();
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Invalid Key %s under section %s . Being Ignored. \n",
                    keyName.c_str(),
                    sectionName.c_str());
            }
        }
        
    }
}
void ConfigValueParser::fillRecoveryPointDetails(ACE_Configuration_Heap& configHeap, 
                             ACE_Configuration_Section_Key& sectionKey,
                            AppConfigValueObject& obj )
{
    int keyIndex = 0 ;
    int sectionIndex = 0;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
    
    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());


        if( value.empty() == false )
        {
            if( keyName.compare( KEY_LATEST_COMMON_TIME )== 0 )
            {
                obj.setLatestCommonTime(boost::lexical_cast<bool>(value.c_str())); 
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Invalid Key %s under section %s . Being Ignored. \n",
                    keyName.c_str(),
                    sectionName.c_str());
            }
        }
    }
    

}
void ConfigValueParser::fillSQLInstanceDetails(ACE_Configuration_Heap& configHeap, 
                             ACE_Configuration_Section_Key& sectionKey,
                            AppConfigValueObject& obj )
{
    int keyIndex = 0 ;
    int sectionIndex = 0;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
    
    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());


        if( value.empty() == false )
        {
            if( keyName.compare( KEY_INSTANCES )== 0 )
            {

                if(value.find_first_of(';') != std::string::npos)
                {
                    //char szBuffer[250];
					for( char* pszToken = strtok(/*szBuffer*/(char*)value.c_str(), ";" ); 
                        (NULL != pszToken);
                        (pszToken = strtok( NULL, ";" ))) 
                    {
                        obj.setSQLInstanceNameList( pszToken );
                    }
                }
                else
                {
                    obj.setSQLInstanceNameList(value);
                }
                
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Invalid Key %s under section %s . Being Ignored. \n",
                    keyName.c_str(),
                    sectionName.c_str());
            }
        }
    }
}
void ConfigValueParser::fillReplPairStatusDetails(ACE_Configuration_Heap& configHeap,
							 ACE_Configuration_Section_Key& sectionKey,
							 AppConfigValueObject& obj )
{
	int keyIndex = 0;
	int sectionIndex = 0;
	ACE_TString sectionName;
	ACE_TString tStrKeyName;
	ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

	while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
	{
		ACE_TString tStrValue;
		configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue);
		keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
		if(keyName.compare(KEY_PAIRSTATUS) == 0)
			if(!value.empty())
			{
				if(value.find_first_of('|') != std::string::npos)
				{
					for( char* pszToken = strtok((char*)value.c_str(), "|" ); 
						(NULL != pszToken);
						(pszToken = strtok( NULL, "|" ))) 
					{
						std::string tempStr = pszToken;
						size_t found = tempStr.find_first_of('=');
						if(found != std::string::npos)
						{
							obj.setPairStatus(tempStr.substr(0,found),tempStr.substr(found+1));
						}
					}
				}
				else
				{

					size_t found = value.find_first_of('=');
					if(found != std::string::npos)
					{
						obj.setPairStatus(value.substr(0,found),value.substr(found+1));
					}
				}
			}
	}
}
void ConfigValueParser::fillCustomVolumeDetails(ACE_Configuration_Heap& configHeap, 
                             ACE_Configuration_Section_Key& sectionKey,
                            AppConfigValueObject& obj )
{
    int keyIndex = 0 ;
    int sectionIndex = 0;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
    
    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());


        if( value.empty() == false )
        {
            if( keyName.compare( KEY_PAIRS )== 0 )
            {
                if(value.find_first_of(';') != std::string::npos)
                {
                //char szBuffer[250];
					for( char* pszToken = strtok(/*szBuffer*/(char*)value.c_str(), ";" ); 
	                (NULL != pszToken);
	                (pszToken = strtok( NULL, ";" ))) 
                {
                    std::string tempStr = pszToken;
                    size_t found = tempStr.find_first_of('=');
                    if(found != std::string::npos)
                    {
                        obj.setCustomProtectedPair(tempStr.substr(0,found),tempStr.substr(found+1));
                        }
                    }
                }
                else
                {
                    
                    size_t found = value.find_first_of('=');
                    if(found != std::string::npos)
                    {
                        obj.setCustomProtectedPair(value.substr(0,found),value.substr(found+1));
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Invalid Key %s under section %s . Being Ignored. \n",
                    keyName.c_str(),
                    sectionName.c_str());
            }
        }
    }
    

}
bool ConfigValueParser::parseConfigFileInput( AppConfigValueObject& obj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
   
    ACE_Configuration_Heap configHeap ;
    
    if( configHeap.open() >= 0 )
    {
        ACE_Ini_ImpExp iniImporter( configHeap );
        if(iniImporter.import_config(ACE_TEXT_CHAR_TO_TCHAR(m_szconfigPath.c_str())) >= 0)
        {
	     
            int sectionIndex = 0 ;
            ACE_Configuration_Section_Key sectionKey;
            ACE_TString tStrsectionName;
         

            while( configHeap.enumerate_sections(configHeap.root_section(), sectionIndex, tStrsectionName) == 0 )
            {
                configHeap.open_section(configHeap.root_section(), tStrsectionName.c_str(), 0, sectionKey) ;
				std::string sectionName = ACE_TEXT_ALWAYS_CHAR(tStrsectionName.c_str());

                if( sectionName.compare( SECTION_VACP_OPTIONS ) == 0 )
                {
                    fillVacpOptionsDetails( configHeap,sectionKey, obj ) ;
                }
                else if(sectionName.compare( SECTION_RECOVERY_POINT ) == 0 )
                {
                    fillRecoveryPointDetails( configHeap,sectionKey, obj);
                }
                else if (sectionName.compare(SECTION_SQL_INSTANCE_NAMES) == 0)
                {
                    fillSQLInstanceDetails( configHeap,sectionKey, obj);
                }
                else if (sectionName.compare(SECTION_CUSTOM_PAIR_DETAILS) == 0)
                {
                    fillCustomVolumeDetails( configHeap,sectionKey, obj);
                }
				else if (sectionName.compare(SECTION_REPL_PAIR_STATUS) == 0)
				{
					fillReplPairStatusDetails(configHeap, sectionKey, obj);
				}
                else if(strstr(sectionName.c_str(),SECTION_RECOVERY_OPTIONS )!= NULL)
                {
                    
                        RecoveryOptions recoveryOptions;
                        fillRecoveryOptionsDetails(configHeap,sectionKey,recoveryOptions);
                        obj.setRecoveryOptions(recoveryOptions);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, " Illegal Section %s . Ignoring...\n",sectionName.c_str());	            
                }
              
                sectionIndex++; 
            }
        }
        else
        {
            throw ParseException("FAILED to Read Configuration File\n");
        }
    }
    else
    {
        throw ParseException("FAILED to Initialize Parser object.");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return true ;
}



bool ConfigValueParser::UpdateConfigFile( AppConfigValueObject& obj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
   
    ACE_Configuration_Heap configHeap ;
    
    if( configHeap.open() >= 0 )
    {
        ACE_Ini_ImpExp iniImporter( configHeap );
        if(iniImporter.import_config(ACE_TEXT_CHAR_TO_TCHAR(m_szconfigPath.c_str())) >= 0)
        {
	     
           int sectionIndex = 0 ;
            ACE_Configuration_Section_Key sectionKey;
            ACE_TString tStrsectionName;
            
             std::map<std::string,RecoveryOptions>recoveryoptions =  obj.getRecoveryOptions();
             std::map<std::string,RecoveryOptions>::iterator recoveryOptionIter =  recoveryoptions.begin();

            while( configHeap.enumerate_sections(configHeap.root_section(), sectionIndex, tStrsectionName) == 0 )
            {
                configHeap.open_section(configHeap.root_section(), tStrsectionName.c_str(), 0, sectionKey) ;
                std::string sectionName = ACE_TEXT_ALWAYS_CHAR(tStrsectionName.c_str());
                if( sectionName.compare( SECTION_VACP_OPTIONS ) == 0 )
                {
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_CMD_LINE),ACE_TEXT_CHAR_TO_TCHAR(obj.getVacpCommand().c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_SERVER_IP),ACE_TEXT_CHAR_TO_TCHAR(obj.getIPAddress().c_str()));
                    std::string portStr = boost::lexical_cast<std::string>(obj.getport());
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_SERVER_PORT),ACE_TEXT_CHAR_TO_TCHAR(portStr.c_str()));
                    std::string fullBackupstr = boost::lexical_cast<std::string>(obj.getPerformFullBackup());
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_PERFORM_FULL_BACKUP),ACE_TEXT_CHAR_TO_TCHAR(fullBackupstr.c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_SERVER_DEVICE),ACE_TEXT_CHAR_TO_TCHAR(obj.getServerDevice().c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_WRITER_INSTANCE),ACE_TEXT_CHAR_TO_TCHAR(obj.getWriterInstance().c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_TAG_NAME),ACE_TEXT_CHAR_TO_TCHAR(obj.getTagName().c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_CONTEX),ACE_TEXT_CHAR_TO_TCHAR(obj.getContext().c_str()));
                    std::string timeoutStr =  boost::lexical_cast<std::string>(obj.getTimeout());
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_TIMEOUT),ACE_TEXT_CHAR_TO_TCHAR(timeoutStr.c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_PROVIDER_GUID),ACE_TEXT_CHAR_TO_TCHAR(obj.getProviderGUID().c_str()));
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_APP_TYPE),ACE_TEXT_CHAR_TO_TCHAR(obj.getApplicationType().c_str()));
                    std::list<std::string> appVolumeList = obj.getApplicationVolumeList();
                    std::list<std::string>::iterator iter = appVolumeList.begin();
                    std::string appVolumesStr;
                    while(iter != appVolumeList.end())
                    {
                        appVolumesStr += *iter + ",";
                        iter++;
                    }
                    size_t found;
                    if((found = appVolumesStr.find_last_of(',')) != std::string::npos)
                    {
                        appVolumesStr = appVolumesStr.substr(0,found);
                    }
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_APP_VOLUMES),ACE_TEXT_CHAR_TO_TCHAR(appVolumesStr.c_str()));
                    std::list<std::string> customVolumeList = obj.getCustomVolumesList();
                    std::list<std::string>::iterator customVolIter = customVolumeList.begin();
                    std::string customVolumesStr;
                    while(customVolIter != customVolumeList.end())
                    {
                        customVolumesStr += *customVolIter+ ",";
                        customVolIter++;
                    }
                    if((found = customVolumesStr.find_last_of(',')) != std::string::npos)
                    {
                        customVolumesStr = customVolumesStr.substr(0,found);
                    }
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_CUSTOM_VOLUMES),ACE_TEXT_CHAR_TO_TCHAR(customVolumesStr.c_str()));
                    std::string remoteTag = boost::lexical_cast<std::string>(obj.getRemoteTag());
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_REMOTE_TAG),ACE_TEXT_CHAR_TO_TCHAR(remoteTag.c_str()));
                    std::string syncTag = boost::lexical_cast<std::string>(obj.getSynchronousTag());
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_SYNCHRONUS_TAG),ACE_TEXT_CHAR_TO_TCHAR(syncTag.c_str()));
                }
                else if(sectionName.compare( SECTION_RECOVERY_POINT ) == 0 )
                {
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_LATEST_COMMON_TIME),ACE_TEXT_CHAR_TO_TCHAR(obj.getLatestCommonTime().c_str()));
                }
                else if (sectionName.compare(SECTION_SQL_INSTANCE_NAMES) == 0)
                {
                    std::list<std::string> instanceList = obj.getSQLInstanceNameList();
                    std::list<std::string>::iterator instanceListIter = instanceList.begin();
                    std::string instanceNameStr;
                    while(instanceListIter != instanceList.end())
                    {
                        instanceNameStr += *instanceListIter + ";";
                        instanceListIter++;
                    }
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_INSTANCES),ACE_TEXT_CHAR_TO_TCHAR(instanceNameStr.c_str()));
                }
                else if (sectionName.compare(SECTION_CUSTOM_PAIR_DETAILS) == 0)
                {
                    std::map<std::string,std::string> customVolumeProtectedMap = obj.getCustomProtectedPair();
                    std::map<std::string,std::string>::iterator pairIter = customVolumeProtectedMap.begin();
                    std::string pairDetailsStr;
                    while(pairIter != customVolumeProtectedMap.end())
                    {
                        pairDetailsStr += pairIter->first + "=" + pairIter->second + ";";
                        pairIter++;
                    }
                    configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_PAIRS),ACE_TEXT_CHAR_TO_TCHAR(pairDetailsStr.c_str()));
                }
                else if(strstr(sectionName.c_str(),SECTION_RECOVERY_OPTIONS )!= NULL)
                {
                    
                    if(recoveryoptions.size() != 0 )
                    {
                        RecoveryOptions recoveryOption = recoveryOptionIter->second;
                        configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_RECOVERY_TIMESTAMP),ACE_TEXT_CHAR_TO_TCHAR(recoveryOption.m_TimeStamp.c_str()));
                        configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_RECOVERY_TAG_TYPE),ACE_TEXT_CHAR_TO_TCHAR(recoveryOption.m_TagType.c_str()));
                        configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_RECOVERY_POINT_TYPE),ACE_TEXT_CHAR_TO_TCHAR(recoveryOption.m_RecoveryPointType.c_str()));
                        configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_TAG_NAME),ACE_TEXT_CHAR_TO_TCHAR(recoveryOption.m_TagName.c_str()));
                        configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_VOLUME_NAME),ACE_TEXT_CHAR_TO_TCHAR(recoveryOption.m_VolumeName.c_str()));
                        recoveryOptionIter++;
                    }
                }
				else if(sectionName.compare(SECTION_REPL_PAIR_STATUS) == 0)
				{
					std::map<std::string, std::string> mapPairStatus = obj.getPairStatus();
					std::map<std::string, std::string>::iterator iterPairStatus = mapPairStatus.begin();
					std::string pairStatusStr;
					while(iterPairStatus != mapPairStatus.end())
					{
						pairStatusStr += iterPairStatus->first + "=" + iterPairStatus->second + "|";
						iterPairStatus++;
					}
					configHeap.set_string_value(sectionKey,ACE_TEXT_CHAR_TO_TCHAR(KEY_PAIRSTATUS),ACE_TEXT_CHAR_TO_TCHAR(pairStatusStr.c_str()));
				}
                else
                {
                    DebugPrintf(SV_LOG_ERROR, " Illegal Section %s . Ignoring...\n",sectionName.c_str());	            
                }
                sectionIndex++; 
                
            }
            if(iniImporter.export_config(ACE_TEXT_CHAR_TO_TCHAR(m_szconfigPath.c_str())) < 0)
            {
                 throw ParseException("FAILED to update Configuration File\n");          
            }
        }
        else
        {
            throw ParseException("FAILED to Read Configuration File\n");
        }
    }
    else
    {
        throw ParseException("FAILED to Initialize Parser object.");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return true ;
}
