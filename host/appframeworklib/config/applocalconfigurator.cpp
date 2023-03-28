#include "applocalconfigurator.h"
#include "boost/lexical_cast.hpp"
#include "inmageex.h"

std::string AppLocalConfigurator::getAppSettingsCachePath() const
{
    return boost::lexical_cast<std::string>(get( SECTION_APPAGENT, KEY_APPSETTINGS_CACHEPATH ));
}

SV_UINT AppLocalConfigurator::getAppSettingsPollingInterval() const
{
    return boost::lexical_cast<SV_UINT>(get( SECTION_APPAGENT, KEY_APPSETTINGS_POLLINGINTERVAL, 60));
}

SV_UINT AppLocalConfigurator::getApplicationDiscoveryInterval() const
{
    return boost::lexical_cast<SV_UINT>(get( SECTION_APPAGENT, KEY_APPDISCOVERY_INTERVAL, 120));
}


std::string AppLocalConfigurator::getAppSchedulerCachePath() const
{
    return boost::lexical_cast<std::string>(get( SECTION_APPAGENT, KEY_APPSCHEDULER_CACHEPATH ));
}

SV_UINT AppLocalConfigurator::getCompressVolPacks() const
{
    return boost::lexical_cast<SV_UINT>(get( SECTION_APPAGENT, KEY_COMPRESS_VOLPACKS, 1));
}

void AppLocalConfigurator::setVolpackCompression(bool compression) const
{
    if( compression )
    {
        set( SECTION_APPAGENT, KEY_COMPRESS_VOLPACKS, "1") ;
    }
    else
    {
        set( SECTION_APPAGENT, KEY_COMPRESS_VOLPACKS, "0");
    }
}
std::string AppLocalConfigurator::getFailoverJobsCachePath() const
{
    return boost::lexical_cast<std::string>(get( SECTION_APPAGENT, KEY_FAILOVERJOBS_CACHEPATH ));
}

std::string AppLocalConfigurator::getExchangeSrcDataPaths() const
{
	return boost::lexical_cast<std::string>(get( SECTION_APPAGENT,KEY_EXCHANGE_SEARCHPATH,std::string("")));
}
SV_UINT AppLocalConfigurator::getExchangeFilesAlreadyBackedUp() const
{
	return boost::lexical_cast<SV_UINT>(get( SECTION_APPAGENT,KEY_EXCHANGE_SEARCH_ALREADY_BACKED_UP, 0));
}
SV_UINT AppLocalConfigurator::getConsistencyTagIssueTimeLimit() const
{
	return boost::lexical_cast<SV_UINT>(get( SECTION_APPAGENT,KEY_CONSISTENCY_TAG_ISSUE_TIME_LIMIT,600));
}
void AppLocalConfigurator::setExchangeDataPath(const std::string &strKeyValues)
{
	set(SECTION_APPAGENT,KEY_EXCHANGE_SEARCHPATH,strKeyValues);
}
void AppLocalConfigurator::setExchangeFilesBackedUp(const std::string &strKeyValue)
{
	set(SECTION_APPAGENT,KEY_EXCHANGE_SEARCH_ALREADY_BACKED_UP,strKeyValue);
}

bool AppLocalConfigurator::shouldDiscoverOracle() const
{
     return boost::lexical_cast<bool>(get( SECTION_APPAGENT,KEY_ORACLE_DISCOVERY, 0));
}
bool AppLocalConfigurator::shouldDiscoverDb2() const
{
     return boost::lexical_cast<bool>(get( SECTION_APPAGENT,KEY_DB2_DISCOVERY, 0));
}
bool AppLocalConfigurator::shouldDiscoverArrays() const
{
     return boost::lexical_cast<bool>(get( SECTION_APPAGENT,KEY_ARRAY_DISCOVERY, 0));
}
bool AppLocalConfigurator::shouldDiscoverMySql() const
{
     return boost::lexical_cast<bool>(get( SECTION_APPAGENT,KEY_MYSQL_DISCOVERY, 0));
}

bool AppLocalConfigurator::shouldDiscoverMSExchange() const
{
     return boost::lexical_cast<bool>(get( SECTION_APPAGENT,KEY_MSEXCHANGE_DISCOVERY, 0));
}

bool AppLocalConfigurator::getRegisterAppAgent() const
{
	return boost::lexical_cast<bool>(get( SECTION_APPAGENT,KEY_REGISTER_APP_AGENT, 0));
}

std::string AppLocalConfigurator::getApplicatioAgentCachePath()
{
	std::string cxupdateCachePath = getAppSettingsCachePath();
	std::string::size_type index = std::string::npos;
	index = cxupdateCachePath.find_last_of("\\/");
	if(index != std::string::npos)
	{
		cxupdateCachePath = cxupdateCachePath.substr(0, index+1); // sends including ending slash.
	}
	return cxupdateCachePath;
}

std::string AppLocalConfigurator::getApplicationPolicyLogPath()
{
#ifdef SV_WINDOWS
    std::string appPolicyLogPath = getCacheDirectory();
#else
    std::string appPolicyLogPath = getLogPathname();
#endif

    appPolicyLogPath += ACE_DIRECTORY_SEPARATOR_CHAR ;
	appPolicyLogPath += "ApplicationPolicyLogs";
	appPolicyLogPath += ACE_DIRECTORY_SEPARATOR_CHAR;
	SVMakeSureDirectoryPathExists(appPolicyLogPath.c_str());
	return appPolicyLogPath;
}

std::string AppLocalConfigurator::getSupportedAppNames() const
{
    std::string str;
#ifdef SV_WINDOWS
    str = "EXCHANGE,EXCHANGE2003,EXCHANGE2007,EXCHANGE2010,SQL,SQL2000,SQL2005,SQL2008,SQL2012,FILESERVER,BULK,SYSTEM,CLOUD";
#else
    str = "BULK,SYSTEM,ORACLE,FABRIC,DB2,CLOUD";
#endif    

    str = boost::lexical_cast<std::string>(get(SECTION_APPAGENT, KEY_SUPPORTED_APPS,str));   

    return str;
}

