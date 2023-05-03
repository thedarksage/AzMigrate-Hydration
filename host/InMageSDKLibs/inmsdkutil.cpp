#include "inmsdkutil.h"
#include "InmXmlParser.h"
#include "svtypes.h"
#include "portable.h"
#include "inmageex.h"
#include "confengine/confschemareaderwriter.h"
#include <boost/filesystem/operations.hpp>
#include "localconfigurator.h"
#include "confengine/agentconfig.h"
bool shouldReleaseHostLockAfterRollingBackTransaction(const std::string& apiname)
{
	if( apiname == "ArchiveRepository" || 
		apiname == "MoveRepository" || 
		apiname == "ChangeBackupLocation" || 
		apiname == "ShouldMoveOrArchiveRepository" ||
		apiname == "ListVolumesWithProtectionDetails" || 
		apiname == "GetProtectionDetails" ||
		apiname == "ListRestorePoints" )
	{
		return true ;
	}
	return false ;
}
bool requireRepoLockForHost( const std::string& apiname, 
							 const std::string& repoPathForHost, 
							 std::string& repolockpath) 
{
	bool requireLock = true ;
	if( repoPathForHost.empty() )
	{
		requireLock = false ;
	}
	else if( apiname == "UpdateCredentials" || 
			 apiname == "ListAvailableDrives" )
	{
		requireLock = false ;
	}
	if( requireLock )
	{
		repolockpath = repoPathForHost ; 
		if( repolockpath[repolockpath.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR )
		{
			repolockpath += ACE_DIRECTORY_SEPARATOR_CHAR ;
		}
		SVMakeSureDirectoryPathExists(repolockpath.c_str()); 
		repolockpath += "configstore.lck" ;
	}
	DebugPrintf(SV_LOG_DEBUG, "api %s repo lock path for host %s, shouldlock %d\n", 
		apiname.c_str(), repolockpath.c_str(), requireLock) ;
	return requireLock ;

}
bool requireRepoLockForHost( const FunctionInfo& FunData, 
							const std::string& repoPathForHost, 
							std::string& repolockpath)
{
	return requireRepoLockForHost(FunData.m_RequestFunctionName, repoPathForHost, repolockpath) ;
}

bool requireRepoLock( const FunctionInfo& FunData, const std::string& repopath, std::string& repolockpath)
{
	bool requireLock = false;
	if( FunData.m_RequestFunctionName == "::registerHostStaticInfo" ||
		FunData.m_RequestFunctionName == "SetupBackupProtection" ||
		FunData.m_RequestFunctionName == "DeleteRepository" ||
		FunData.m_RequestFunctionName == "UpdateProtectionPolicies" || 
		FunData.m_RequestFunctionName == "ListHosts" ||
		//FunData.m_RequestFunctionName == "MoveRepository" ||
		FunData.m_RequestFunctionName == "ReconstructRepo" ||
		FunData.m_RequestFunctionName == "SpaceRequired" )
	{
		try
		{
			repolockpath = repopath ;
			requireLock = true ;
		}
		catch(ContextualException& ex)
		{
		}
	}
	if( requireLock )
	{
		if( repolockpath.length() == 1 )
		{
#ifdef SV_WINDOWS
			repolockpath += ":" ;
#endif
		}
		if( repolockpath[repolockpath.length() - 1] != ACE_DIRECTORY_SEPARATOR_CHAR )
		{
			repolockpath += ACE_DIRECTORY_SEPARATOR_CHAR ;
		}
		repolockpath += "repository.lck" ;
	}
	DebugPrintf(SV_LOG_DEBUG, "api %s repo lock path %s, shouldlock %d\n", 
		FunData.m_RequestFunctionName.c_str(), repolockpath.c_str(), requireLock) ;
	return requireLock ;
}

std::string getLockPath(const std::string& repoLocationForHost)
{
	std::string Configstore ; 
	if( repoLocationForHost.empty() )
	{
		Configstore = ConfSchemaMgr::GetInstance()->getRepoLocationForHost()  ;
	}
	else
	{
		Configstore = repoLocationForHost ;
	}
	Configstore	+= "configstore.lck";
	DebugPrintf( SV_LOG_DEBUG, "Lock Path: %s\n", Configstore.c_str() ) ;
	return Configstore ;
}

std::string getRepositoryPath(std::string path)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	std::string RepoPath ;
	try
	{
		RepoPath = path ;
		if( RepoPath.length() == 1 )
		{
#ifdef SV_WINDOWS
			RepoPath += ":" ;
#endif
		}
		if( RepoPath.length() > 0 && RepoPath[ RepoPath.length() -1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
			RepoPath += ACE_DIRECTORY_SEPARATOR_STR_A ;
	}
	catch(ContextualException& ex)
	{
		DebugPrintf(SV_LOG_DEBUG, "exception while obtaining the backup location name from drscout.conf %s\n", ex.what()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return RepoPath ;
}

bool isRepositoryConfiguredOnCIFS( const std::string& repoPath ) 
{
	if( repoPath.length() > 2 && repoPath.find( "\\\\" ) == 0 )
		return true ;
	else
		return false ;
}
std::string constructConfigStoreLocation( const std::string& reposiotyPath, const std::string& hostId )
{
	std::string configStoreLocation = reposiotyPath ;
	if (configStoreLocation[configStoreLocation.length()-1] != ACE_DIRECTORY_SEPARATOR_CHAR )
	{
		configStoreLocation += ACE_DIRECTORY_SEPARATOR_CHAR ;
	}
	configStoreLocation += hostId ;
	SVMakeSureDirectoryPathExists(configStoreLocation.c_str()) ;
	configStoreLocation += ACE_DIRECTORY_SEPARATOR_CHAR ;
	return configStoreLocation ;
}

std::string getConfigCachePath(const std::string& repolocation, const std::string& hostId)
{
	std::string cacheFile = constructConfigStoreLocation( getRepositoryPath( repolocation ),  hostId) ;
	if(cacheFile[cacheFile.length() - 1] != ACE_DIRECTORY_SEPARATOR_CHAR )
	{
		cacheFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	}
	cacheFile += "config.dat" ;
	return cacheFile ;
}
std::string getAPILogPath()
{
    DebugPrintf(  SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    LocalConfigurator localConfig ;
    std::string apiLogFile ;
	apiLogFile = ConfSchemaMgr::GetInstance()->getRepoLocationForHost()  ;
	if( !apiLogFile.empty() && apiLogFile[apiLogFile.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR )
	{
		apiLogFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	}
    apiLogFile += "Logs";
	apiLogFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
    SVMakeSureDirectoryPathExists(apiLogFile.c_str());
    apiLogFile += ACE_DIRECTORY_SEPARATOR_STR_A;
    apiLogFile += "InmApilogger.dat";
    DebugPrintf( SV_LOG_DEBUG, "API log file path: %s\n", apiLogFile.c_str() ) ;
    DebugPrintf(  SV_LOG_DEBUG, " EXITED %s\n", FUNCTION_NAME ) ;
    return apiLogFile;
}

void LogMessageIntoFile(const std::string &logMessage, const std::string& repopathforhost)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    static LocalConfigurator configurator ;
    if( configurator.getLogLevel() > SV_LOG_INFO )
    {
		std::string apiLogFile = repopathforhost + ACE_DIRECTORY_SEPARATOR_STR_A + "Logs" ;
		SVMakeSureDirectoryPathExists( apiLogFile .c_str() ) ;
		apiLogFile += ACE_DIRECTORY_SEPARATOR_STR_A ;
		apiLogFile += "InmApilogger.dat";
        std::ofstream out;
        boost::uintmax_t sizeondisk = 0 ;
        if( boost::filesystem::exists(apiLogFile) )
        {
            sizeondisk = boost::filesystem::file_size(apiLogFile);
        }

        if( sizeondisk > 10 * 1024 * 1024 )
        {
            try
            {
                if( boost::filesystem::exists(apiLogFile + ".1" ) )
                {
                    boost::filesystem::remove(apiLogFile + ".1") ;
                }
                else
                {
                    boost::filesystem::rename(apiLogFile, apiLogFile + ".1") ;
                }
            }
            catch( boost::filesystem::basic_filesystem_error<boost::filesystem::path> e )
            {
                DebugPrintf( SV_LOG_ERROR, "Unable to rename at this time, would try again. %s\n", e.what() ) ;
            }
        }
        out.open(apiLogFile.c_str(), std::ios::app);
        if (!out) 
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", apiLogFile.c_str());
            return;
        }
        std::string timeStr =  getTimeString(time(NULL));
        out	<< "(";
        out << timeStr;
        out	<< "):";
        out << std::endl;
        out << logMessage;
        out << std::endl;
        out.close();
		std::string errmsg ;
		if( !SyncFile(apiLogFile, errmsg) )
		{
			DebugPrintf(SV_LOG_ERROR, "failed to sync file %s errmsg %s\n", apiLogFile.c_str(), errmsg.c_str()) ;
		}
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void WriteRequestToLog(FunctionInfo &funInfo, const std::string& repopathforhost)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream XmlStream;
    XmlRequestValueObject FunRequestObj;
    FunRequestObj.setRequestFunctionList(funInfo);
    try
    {
        FunRequestObj.XmlRequestSave(XmlStream);
    }
    catch(std::exception &ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "createXmlStream Failed with exception %s\n", ex.what()) ;
    }
    try
    {
        LogMessageIntoFile(XmlStream.str(), repopathforhost);
    }
	catch (std::exception &ex )
	{
		DebugPrintf (SV_LOG_ERROR, "%s \n",ex.what());
	}
    catch(ContextualException& ex)
    {

    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void WriteResponseToLog(FunctionInfo &funInfo, const std::string& repopathforhost)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream XmlStream;
    XmlResponseValueObject FunResponseObj;
    FunResponseObj.setFunctionList(funInfo);
    try
    {
        FunResponseObj.XmlResponseSave(XmlStream);
    }
    catch(std::exception &ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "createXmlStream Failed with exception %s\n", ex.what()) ;
    }

    try
    {
        LogMessageIntoFile(XmlStream.str(), repopathforhost);
    }
	catch (std::exception &ex )
	{
		DebugPrintf (SV_LOG_ERROR, "%s \n",ex.what());
	}
    catch(ContextualException& ex)
    {

    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::string getTimeString(unsigned long long timeInSec)
{
	time_t t1 = timeInSec;
	char szTime[128];

	if(timeInSec == 0)
		return "0000-00-00 00:00:00";

	strftime(szTime, 128, "%Y-%m-%d %H:%M:%S", localtime(&t1));

	return szTime;
}

std::string getCataloguePath()
{
	std::string repoPath = ConfSchemaMgr::GetInstance()->getRepoLocation();
	std::string cataloguePath = constructConfigStoreLocation( repoPath,  AgentConfig::GetInstance()->getHostId())  ;
	cataloguePath += "catalogue";
	cataloguePath += ACE_DIRECTORY_SEPARATOR_CHAR;
	return cataloguePath;
}
ACE_Recursive_Thread_Mutex myMutex ;
std::string getConfigStorePath()
{
	std::string configstorepath ;
	getConfigStorePath( configstorepath ) ;
	return configstorepath ;
}
void getConfigStorePath(std::string& configStorePath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	ACE_Guard<ACE_Recursive_Thread_Mutex> autoGuardMutex(myMutex);
	LocalConfigurator configurator ;
	std::string repoPath = ConfSchemaMgr::GetInstance()->getRepoLocation();
	INM_ERROR error = E_SUCCESS ;
	std::string errormsg ;
	if( !repoPath.empty() )
	{
		configStorePath = constructConfigStoreLocation( repoPath,  configurator.getHostId() )  ;
		AccessRemoteShare(repoPath, error, errormsg ) ;
		if( error == E_SUCCESS && SVMakeSureDirectoryPathExists(configStorePath.c_str()) != SVS_OK )
		{
			error = E_REPO_NOT_AVAILABLE ;
			errormsg = "Unable to find availability of " ; 
			errormsg += configStorePath ;
			errormsg += ". Backup location not available. " ;
		}
	}
	else
	{
		errormsg = "SetupRepository before running API " ;
		error = E_NO_REPO_CONFIGERED ;
	}
	if( error != E_SUCCESS )
	{
		throw TransactionException(errormsg, error) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

