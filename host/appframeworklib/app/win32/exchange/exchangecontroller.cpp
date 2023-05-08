#include "appglobals.h"
#include "exchangecontroller.h"
#include "exchangeapplication.h"
#include "controller.h"
#include "Consistency/TagGenerator.h"
#include "clusterutil.h"
#include "service.h"
#include "system.h"
#include "ClusterOperation.h"
#include "host.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "appexception.h"
#include "util/common/util.h"
#include "systemvalidators.h"
#include "registry.h"
MSExchangeControllerPtr MSExchangeController::m_instancePtr;
MSExchangeControllerPtr MSExchangeController::getInstance(ACE_Thread_Manager* tm)
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new  MSExchangeController(tm) ) ;
		if( m_instancePtr.get() != NULL )
			m_instancePtr->Init();
    }
    return m_instancePtr ;
}


int MSExchangeController::open(void *args)
{
    return this->activate(THR_BOUND);
}

int MSExchangeController::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

int MSExchangeController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return  0 ;
}

MSExchangeController::MSExchangeController(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
    //TODO:: controller specific initialization
    m_bCluster = false;
}

bool MSExchangeController::IsItActiveNode()
{
    bool process = false ;
    m_exchApp.reset( new ExchangeApplication(false) ) ;
    if( m_exchApp->isInstalled() )
    {
        std::string instanceName ;
        m_exchApp->discoverApplication() ;
        std::list<std::string> activeInstances = m_exchApp->getActiveInstances() ;
        std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
        if( propsMap.find("VirtualServerName") != propsMap.end() )
        {
            instanceName = propsMap.find("VirtualServerName")->second ;
        }
        else if( propsMap.find("InstanceName") != propsMap.end() )
        {
            instanceName = propsMap.find("InstanceName")->second ;
        }
        if( isfound(activeInstances, instanceName) )
        {
            process = true ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "virtual server %s is not active on the current  node\n", instanceName.c_str()) ;
        }
    }
    return process;
}
SVERROR MSExchangeController::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    std::map<std::string, std::string>::iterator iter = propsMap.find("ApplicationType");
    std::string strApplicationType;
    if(iter != propsMap.end())
    {
        strApplicationType = iter->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "there is no option with key as ApplicationType\n") ;
    }
    if( strcmpi(strApplicationType.c_str(), EXCHANGE2010) != 0 )
    {
        m_bCluster = isClusterNode();
    }
    const std::string& policyType = propsMap.find("PolicyType")->second;
    if(policyType.compare("Consistency") == 0)
    {
        if(Consistency() == SVS_OK)
        {
            result = SVS_OK;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to bookmark..\n") ;
        }
    }
    else if(policyType.compare("PrepareTarget") == 0)
    {
        if(prepareTarget() == SVS_OK)
        {
            result = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to prepare target\n") ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}
SVERROR MSExchangeController::Consistency()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::string strCommandLineOptions;
    std::string strAppType;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;

    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    std::string outputFileName = getOutputFileName(policyId);
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    std::map<std::string, std::string>::iterator iter = propsMap.find("ScenarioId");
    if(iter != propsMap.end())
    {
        if(checkPairsInDiffsync(iter->second,outputFileName))
        {        
            /*iter = propsMap.find("ApplicationType");
            if(iter != propsMap.end())
            {
                strAppType = iter->second;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "there is no option with key as ApplicationType\n") ;
            }*/
            iter = propsMap.find("ConsistencyCmd");
            /*if(iter != propsMap.end())
            {
                strCommandLineOptions = iter->second;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "there is no option with key as ConsistencyCmd\n") ;
            }
            if( strAppType.compare("EXCHANGE2003") != 0 )
            {
                if( strCommandLineOptions.find("ExchangeIS") == std::string::npos )
                {
                    strCommandLineOptions += std::string(" -w ExchangeIS");
                }
            }
            iter = propsMap.find("PolicyId");*/
            if(iter != propsMap.end())
            {
				strCommandLineOptions = iter->second;
				bool bExecSuccess = false;
				if(!strCommandLineOptions.empty())
				{
					AppLocalConfigurator configurator;
					TagGenerator obj(strCommandLineOptions,configurator.getConsistencyTagIssueTimeLimit());
					SV_ULONG exitCode = 0x1;

					if(obj.issueConsistenyTag(outputFileName,exitCode))
					{
						bExecSuccess = true;
						if(exitCode == 0)
						{
							appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",obj.stdOut());
							result = SVS_OK;
						}
						else
						{
							//Command Spawned but failed due to invalid parameters.
							appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",obj.stdOut());
							result = SVS_OK;
						}
					}
				}
                if(!bExecSuccess)
                {
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Failed to spawn vacp Process.Please check Command"));
                }		
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "\n Failed to fetch the ConsistencyCmd from policy details");
				appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","Consistency command is missing. Can not issue consistency tag.");
            }
        }
        else
        {
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Skipped","");
            DebugPrintf(SV_LOG_DEBUG,"Pairs not in diff sync\n");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "there is no option with key as ScenraioId\n") ;
		appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","ScenraioId is missing for consistency policy. Can not issue consistency tag.");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    return result;
}
SVERROR MSExchangeController::restoreExchangeSearchDir()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;	
    std::list<std::string> listOfExchnageDataPaths;
    std::string sourceDataDir;
    std::string targetDataDir;
    LocalConfigurator localConfig;
    getDataSearchPath(listOfExchnageDataPaths);
    if( listOfExchnageDataPaths.size() > 0 )
    {
        if( result.failed() )
        {
            DebugPrintf(SV_LOG_ERROR, "Directory: %s doesnt exist. Error Returned: %s\n", targetDataDir.c_str(), ACE_OS::strerror(result.error.ntstatus)) ;						
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
            return result;
        }
        std::list<std::string>::iterator directoryIter = listOfExchnageDataPaths.begin() ;
        while( directoryIter != listOfExchnageDataPaths.end() )
        {
            sourceDataDir = localConfig.getInstallPath() ;
            //Creating a directory
            sourceDataDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
            sourceDataDir += std::string("Failover");
            sourceDataDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
            sourceDataDir += std::string("ExchangeSourceFiles");
            sourceDataDir += ACE_DIRECTORY_SEPARATOR_CHAR  ; 
            sourceDataDir += directoryIter->substr(directoryIter->find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR)); 
            std::string targetDataDir1 =  *directoryIter ;

            SVMakeSureDirectoryPathExists(sourceDataDir.c_str()) ;
            SVMakeSureDirectoryPathExists(targetDataDir1.c_str()) ;
            if(copyExchangeSearchFiles(sourceDataDir,targetDataDir1) == SVS_FALSE)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to copy files from %s to %s.\n",sourceDataDir.c_str(),targetDataDir1.c_str());
                break ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "copied %s to %s\n", directoryIter->c_str(),targetDataDir1.c_str()) ;
            }
            directoryIter++ ;
        }
        if( directoryIter != listOfExchnageDataPaths.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "Could not back up all Exchange Search paths\n") ;
        }
        else
        {
            result = SVS_OK;
        }
    }
    else
    {
        result = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;;
}

SVERROR MSExchangeController::backupExchangeSearchDir()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;	
    AppLocalConfigurator configurator ;
    std::list<std::string> listOfExchnageDataPaths;
    std::string sourceDataDir;
    std::string targetDataDir;
    LocalConfigurator localConfig;
    getDataSearchPath(listOfExchnageDataPaths);
    if( listOfExchnageDataPaths.size() > 0 )
    {
        targetDataDir = localConfig.getInstallPath() ;
        //Creating a directory
        targetDataDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
        targetDataDir += std::string("Failover");
        targetDataDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
        targetDataDir += std::string("ExchangeSourceFiles");    
        if(SVMakeSureDirectoryPathExists(targetDataDir.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR, "Directory: %s doesnt exist. Error Returned: %s\n", targetDataDir.c_str(), ACE_OS::strerror(result.error.ntstatus)) ;						
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
            return result;
        }
        std::list<std::string>::iterator directoryIter = listOfExchnageDataPaths.begin() ;
        while( directoryIter != listOfExchnageDataPaths.end() )
        {
            std::string targetDataDir1 =  targetDataDir + directoryIter->substr(directoryIter->find_last_of("\\") ) ;
            if(SVMakeSureDirectoryPathExists(targetDataDir1.c_str()).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "Directory: %s doesnt exist. Error Returned: %s\n", targetDataDir1.c_str(), ACE_OS::strerror(result.error.ntstatus)) ;						
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
                return result;
            }
            if(copyExchangeSearchFiles(*directoryIter,targetDataDir1) == SVS_FALSE)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to copy files from %s to %s.\n",sourceDataDir.c_str(),targetDataDir1.c_str());
                break ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "copied %s to %s\n", directoryIter->c_str(),targetDataDir1.c_str()) ;
            }
            directoryIter++ ;
        }
        if( directoryIter != listOfExchnageDataPaths.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "Could not back up all Exchange Search paths\n") ;
        }
        else
        {
            result = SVS_OK;
        }
    }
    else
    {
        result = SVS_OK ;
    }
    if( result == SVS_OK )
    {
        configurator.setExchangeFilesBackedUp(std::string("1"));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}
void MSExchangeController::getDataSearchPath(std::list<std::string>& listOfDataPaths)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string strCommaseparatedDataPath;
    AppLocalConfigurator configurator;
    SV_UINT uExchangeFilesAlreadyBackedUp = configurator.getExchangeFilesAlreadyBackedUp();
    if(uExchangeFilesAlreadyBackedUp == 0)
    {
        strCommaseparatedDataPath = configurator.getExchangeSrcDataPaths();
        if(!strCommaseparatedDataPath.empty())
        {
            listOfDataPaths = tokenizeString(strCommaseparatedDataPath,",");
        }	
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Files are already backed up.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
SVERROR MSExchangeController::copyExchangeSearchFiles(const std::string& strSourcePath,const std::string strTargetPath)
{
    USES_CONVERSION;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_stat stat ;
    SVERROR retVal = SVS_OK;
    ACE_DIR * ace_dir = NULL ;
    ACE_DIRENT * dirent = NULL ;
    std::string dir = strSourcePath ;
    int statRetVal = 0 ;
    bool recurse = true;
    ACE_HANDLE hd_in, hd_out ;
    hd_in = hd_out = ACE_INVALID_HANDLE ;
    std::string strTargetFileName ;
    if((statRetVal  = sv_stat(getLongPathName(dir.c_str()).c_str(),&stat)) != 0 )
    {
        DebugPrintf(SV_LOG_ERROR, "FILE LISTER :Unable to stat %s. ACE_OS::stat returned %d\n", dir.c_str(), statRetVal) ; 
        retVal =  SVS_FALSE;
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
                        continue ;
                    }
                    std::string absPath = dir + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName ;

                    if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
                    {
                        DebugPrintf(SV_LOG_ERROR, "ACE_OS::stat for %s is failed with error %d \n", absPath.c_str(), statRetVal) ;
                        continue ;
                    }
                    if( stat.st_mode & S_IFDIR )
                    {
                        isDir = true ;
                        strTargetFileName.clear();
                        strTargetFileName = strTargetPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName;						
                        if(SVMakeSureDirectoryPathExists(strTargetFileName.c_str()).failed())
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", strTargetFileName.c_str());
                            retVal = SVS_FALSE;
                            break ;
                        }
                        copyExchangeSearchFiles(absPath,strTargetFileName);
                    }

                    if(!isDir)
                    {
                        strTargetFileName.clear();
                        strTargetFileName = strTargetPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName;
                        //Copy files from the Directory
                        std::wstring srcFileNameW = A2W( absPath.c_str() ) ;
                        std::wstring tgtFileNameW = A2W( strTargetFileName.c_str() ) ;

                        hd_in = ACE_OS::open( srcFileNameW.c_str(), O_RDONLY ) ;
                        hd_out = ACE_OS::open( tgtFileNameW.c_str(), O_CREAT | O_WRONLY | O_TRUNC) ;
                        if( hd_in != ACE_INVALID_HANDLE )
                        {		
                            if( hd_out != ACE_INVALID_HANDLE )
                            {
                                size_t bytesRead = 0 ;
                                char *buf = new char[1024*1024] ;
                                while( ( bytesRead = ACE_OS::read(hd_in, buf, 1024*1024) ) != 0 )
                                {
                                    size_t bytesWrote = 0 ;
                                    if( ( bytesWrote = ACE_OS::write(hd_out, buf, bytesRead)) != bytesRead )
                                    {
                                        DebugPrintf(SV_LOG_ERROR, "Among %d bytes wrote only %d bytes\n", bytesRead, bytesWrote) ;
                                        retVal = SVS_FALSE;
                                        break;
                                    }
                                }
                                delete[] buf ;
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to Open %s for writing. Error %s\n", tgtFileNameW.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;			
                                retVal =  SVS_FALSE;
                                break;
                            }	
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "Failed to Open %s for writing. Error %s\n", srcFileNameW.c_str(), ACE_OS::strerror(ACE_OS::last_error())) ;		
                            retVal =  SVS_FALSE;
                            break;
                        }
                    }
                    SAFE_ACE_CLOSE_HANDLE(hd_in) ;
                    SAFE_ACE_CLOSE_HANDLE(hd_out) ;
                }
            } while (dirent != NULL  ) ;
            ACE_OS::closedir( ace_dir ) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "ACE_OS::open_dir failed for %s.\n", dir.c_str()) ;
            retVal =  SVS_FALSE;;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retVal;
}

SVERROR MSExchangeController::persistSearchPath()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    bool bIsExchange2003Version = false;
    AppLocalConfigurator configurator ;
    std::string strSrcDataPaths;
    ExchangeApplicationPtr exchPtr ;
    exchPtr.reset( new ExchangeApplication() ) ;
    if( exchPtr->isInstalled() )
    {
        exchPtr->discoverApplication();
        std::map<std::string, ExchAppVersionDiscInfo> exchangeDiscoveryMap;
        exchangeDiscoveryMap = exchPtr->m_exchDiscoveryInfoMap;

        std::map<std::string, ExchAppVersionDiscInfo> ::iterator iter = exchangeDiscoveryMap.begin();
        std::list<std::string> evsList ;
        while(iter != exchangeDiscoveryMap.end())
        {
            evsList.push_back(iter->second.m_cn ) ;
            DebugPrintf(SV_LOG_DEBUG, "persistSearchPath : EVS name %s\n", iter->second.m_cn.c_str() );
            iter++;
        }
        exchPtr.reset( new ExchangeApplication(evsList) ) ;
        if( exchPtr->isInstalled() )
        {
            exchPtr->discoverApplication();
        } 
        exchangeDiscoveryMap = exchPtr->m_exchDiscoveryInfoMap;
        iter = exchangeDiscoveryMap.begin();
        while(iter != exchangeDiscoveryMap.end())
        {
            std::string exchageSearchPath ;
            exchageSearchPath = iter->second.m_dataPath ;
            exchageSearchPath  += "\\ExchangeServer_" ;
            exchageSearchPath  += iter->second.m_cn ;
            DebugPrintf(SV_LOG_DEBUG, "persistSearchPath: %s\n", exchageSearchPath.c_str()) ;
            strSrcDataPaths += exchageSearchPath ;
            strSrcDataPaths += std::string(",") ;
            if( iter->second.m_appType == INM_APP_EXCH2003 )
            {
                bIsExchange2003Version = true; 
            }
            else
            {
                break ;
            }
            iter++;
        }
        if(bIsExchange2003Version )
        {
            //remove the last comma
            strSrcDataPaths = strSrcDataPaths.substr(0,strSrcDataPaths.length()-1);
            if(!strSrcDataPaths.empty())
            {
                configurator.setExchangeFilesBackedUp(std::string("0"));
                configurator.setExchangeDataPath(strSrcDataPaths);
                bRet = SVS_OK;
            }
        }
        else //other that exchange 2003 version
        {
            bRet = SVS_OK;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR MSExchangeController:: prepareTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::string strAppType;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    schedulerPtr->UpdateTaskStatus(boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second), TASK_STATE_STARTED) ;
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    std::map<std::string, std::string>::iterator iter = propsMap.find("ApplicationType");
    SV_ULONG policyIdInNumeric = 0;

    try
    {
        policyIdInNumeric = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    }
    catch( const boost::bad_lexical_cast & exObj)
    {
        DebugPrintf(SV_LOG_ERROR,"Exception caught. bad cast .Type %s",exObj.what());
        //Cant send update to scheduler since policy conversion is failing
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
        return false;
    }

    if(iter != propsMap.end())
    {
        strAppType = iter->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "There is no option with key as ApplicationType\n") ;
    }
    SV_UINT strTimeout = 600; //Should come from some config file
    SV_ULONG exitCode = 0x01;
    std::string outputFileName = getOutputFileName(policyIdInNumeric);
    std::stringstream msg;
    if(stopExchangeServices(strAppType, m_bCluster) == SVS_OK) 
    {
        do
        {
            if(m_bCluster)
            {
                std::list<std::string> listClsuterNodes;
                if(getNodesPresentInCluster("",listClsuterNodes) == SVS_OK)
                {
                    std::string strListOfPoweredOfNodes;
                    std::string strLocalHost = Host::GetInstance().GetHostName();
                    std::list<std::string>::iterator listIter = listClsuterNodes.begin();

                    while(listIter != listClsuterNodes.end())
                    {
                        //The Current node is Active Node
                        DebugPrintf(SV_LOG_INFO,"Local host name %s and name from list %s",strLocalHost.c_str(),(*listIter).c_str());
                        if(strcmpi(strLocalHost.c_str(),(*listIter).c_str()) != 0)
                        {
                            strListOfPoweredOfNodes += *listIter;
                            strListOfPoweredOfNodes += std::string(",");
                        }
                        listIter++;
                    }

                    if(!strListOfPoweredOfNodes.empty())
                        strListOfPoweredOfNodes.erase(strListOfPoweredOfNodes.length()-1);


                    std::string strCommand = std::string(" -prepare ClusterToStandalone:");

                    strCommand += strLocalHost;
					if(listClsuterNodes.size() > 1)
					{
						strCommand += std::string(" -shutdown ");
						strCommand += strListOfPoweredOfNodes;
						strCommand += std::string(" -force");
					}
                    strCommand += std::string(" -noActiveNodeReboot");

                    DebugPrintf(SV_LOG_INFO,"The Target breaking command is %s: \n",strCommand.c_str());


                    ClusterOp objClusterOp(strCommand,strTimeout);
                    std::string policyId = propsMap.find("PolicyId")->second ;
                    if(persistSearchPath() == SVS_FALSE)
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to persist the Exchange Source Files.\n");
                        msg << "Failed to backup the Exchange source files." << std::endl;
                        updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string("Failed to backup the Exchange source files."));
                        break;
                    }
                    if(objClusterOp.persistClusterResourceState() == SVS_FALSE)
                    {	
                        DebugPrintf(SV_LOG_ERROR,"Failed to persist cluster resources.\n");
                        msg << "Failed to persist cluster resources." << std::endl;
                        updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string("Failed to persist cluster resources."));
                        break;
                    }

                    if(objClusterOp.breakCluster(outputFileName,exitCode))
                    {
                        if(exitCode == 0)
                        {

                            DebugPrintf(SV_LOG_INFO,"\n Waiting for reboot of current host: %s to happen",strLocalHost.c_str());
                            if(!RebootSystems(strLocalHost.c_str(), true, NULL,NULL,NULL))
                            {
                                DebugPrintf(SV_LOG_ERROR,"\n Attempt to reboot  current host: %s to happen\n",strLocalHost.c_str());
                            }
                            else
                            {
                                result = SVS_OK ;
                                /*if( !Controller::getInstance()->QuitRequested(1) )
                                {
                                    Controller::getInstance()->Stop();
                                }*/
                                //This update will never reach CX since by this time ,machine will be rebooted
                                DebugPrintf(SV_LOG_INFO,"\n Rebooted host %s successfully\n",strLocalHost.c_str());
                            }
                        }
                        else //if exitcode is 1(need to confirm
                        {
                            //clusutil spawned but due to bad command it failed . Result should be SVS_OK ?
                            msg << "Failed to spawn clusutil. Please check command." << std::endl;
                            updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string(std::string("Failed to spawn clusutil.Please check command .")));
                            break;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Prepare target policy failed for Exchange Controller.\n") ;
                        msg << "Failed to break cluster." << std::endl;
                        updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string(std::string("Failed to break cluster.")));
                        break;
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG,"Failed to list of Nodes present in system.Cannot continue with target Preparation.\n");
                    msg << "Failed to list of Nodes present in system.Cannot continue with target Preparation" << std::endl;
                    updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string(std::string("Failed to list of Nodes present in system.Cannot continue with target Preparation.")));
                    break;
                }
            }
            else
            {
                if(backupExchangeSearchDir() == SVS_OK)
                {
                    OSVERSIONINFOEX osVersionInfo ;
                    getOSVersionInfo(osVersionInfo) ;
                    ClusterOp objClusterOp;
                    if(osVersionInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
                    {
                        if(objClusterOp.BringW2k8ClusterDiskOnline(outputFileName,exitCode))
                        {
                            if(exitCode != 0)
                            {
                                msg << "Failed to spawn clusutil. Please check command." << std::endl;
                                updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string(std::string("Failed to spawn clusutil.Please check command .")));
                            }
                            else
                            {
                                msg << "PrepareTarget Operation completed successfully." << std::endl;
                                updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Success",std::string(std::string("PrepareTarget Operation completed successfully.")));
                                result = SVS_OK;
                            }
                        }
                    }
                    else
                    {
                        msg << "PrepareTarget Operation completed successfully." << std::endl;
                        updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Success",std::string(std::string("PrepareTarget Operation completed successfully.")));
                        result = SVS_OK;
                    }
                }
                else
                {
                    //Cluster specific update
                    msg << "PrepareTarget Operation failed." << std::endl;
                    updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string(std::string("PrepareTarget Operation failed.")));
                }
            }
        }while(0);

    }
    else
    {
        msg << "Failed to stop  Exchange services." << std::endl;
        updateJobStatus(TASK_STATE_COMPLETED,policyIdInNumeric,"Failed",std::string(std::string("Failed to stop  Exchange services")));
    }
    std::string outputFilePathName = getOutputFileName( policyIdInNumeric );
    std::ofstream os;
    os.open( outputFilePathName.c_str(), std::ios::app );
    if( os.is_open() && os.good() )
    {
        os << msg.str();
        os.flush();
        os.close();
    }
    else
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", outputFilePathName.c_str() );
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}

void MSExchangeController::MakeAppServicesAutoStart()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    changeServiceType(EXCHANGE_MANAGEMENT, INM_SVCTYPE_AUTO);
    changeServiceType(EXCHANGE_MTA,INM_SVCTYPE_AUTO);
    changeServiceType(EXCHANGE_ROUTING_ENGINE, INM_SVCTYPE_AUTO);    
    changeServiceType(EXCHANGE_INFORMATION_STORE,INM_SVCTYPE_AUTO);
    changeServiceType(EXCHANGE_REPL, INM_SVCTYPE_AUTO);
    changeServiceType(ECHANGE_TRANS, INM_SVCTYPE_AUTO);
    changeServiceType(EXCHANGE_SYSTEM_ATTENDANT, INM_SVCTYPE_AUTO);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool MSExchangeController::StartAppServices()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bResult = true;
    std::stringstream stream;
    std::string outputFilePathName = getOutputFileName( m_policyId,START_APP_CLUSTER );
    std::ofstream out;
    out.open(outputFilePathName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to createFile %s\n",outputFilePathName.c_str());
    }
    else
    {
        out.close();
    }
    DebugPrintf( SV_LOG_DEBUG, "Starting Exchange services\n");
    if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
    {
        ClusterOp clusOpObj;
        std::list<std::string> resourceList;

        resourceList.push_back("Microsoft Exchange Information Store");
        resourceList.push_back("Microsoft Exchange System Attendant");
		OSVERSIONINFOEX osVersionInfo ;
		getOSVersionInfo(osVersionInfo) ;
		//if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
		if (_stricmp(m_FailoverInfo.m_AppType.c_str(),"EXCHANGE2003") == 0 ||
			_stricmp(m_FailoverInfo.m_AppType.c_str(),"EXCHANGE") == 0)
			resourceList.push_back("Microsoft Exchange Message Transfer Agent");
		/*else if(_stricmp(m_FailoverInfo.m_AppType.c_str(),"EXCHANGE2007") == 0)
			resourceList.push_back("Microsoft Exchange Database Instance");*/
        std::list<std::string>::iterator resourceListIter = resourceList.begin();
        while(resourceListIter != resourceList.end())
        {


            if(clusOpObj.OfflineorOnlineResource(*resourceListIter,"Exchange",m_FailoverInfo.m_ProductionVirtualServerName,true) == true)
            {
                DebugPrintf(SV_LOG_DEBUG,"Successfully brought resouce online\n");
                stream<<"Successfully brought " ;
                stream << *resourceListIter;
                stream << " online";
                stream << std::endl;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Failed to bring resouce online\n");
				stream << "Failed to bring ";
				stream << *resourceListIter;
				stream << "online" ;
				stream << std::endl;
				bResult = false;
            }
            resourceListIter++;
        }
        onlineDependentResources();


    }
    else
    {
        if (StrService(EXCHANGE_INFORMATION_STORE) == SVS_FALSE)
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to start Exchange Information Store Service \n");
            stream<<"Failed to start "<<EXCHANGE_INFORMATION_STORE<<" service"<<std::endl;
			bResult = false;
        }
        else
        {
            stream<<"Successfully started "<<EXCHANGE_INFORMATION_STORE<<" service"<<std::endl;
        }
        if(!_stricmp(m_FailoverInfo.m_AppType.c_str(), EXCHANGE)) 
        {
            if( StrService(EXCHANGE_MANAGEMENT) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to start MSExchangeMGMT service \n");
                stream<<"Failed to start "<<EXCHANGE_MANAGEMENT<<" service"<<std::endl;
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<EXCHANGE_MANAGEMENT<<" service"<<std::endl;
            }
            if( StrService(EXCHANGE_MTA) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to start MSExchnageMTA service \n");
                stream<<"Failed to start "<<EXCHANGE_MTA<<" service"<<std::endl;
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<EXCHANGE_MTA<<" service"<<std::endl;
            }
            if( StrService(EXCHANGE_ROUTING_ENGINE) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to start RESvc service \n");
                stream<<"Failed to start "<<EXCHANGE_ROUTING_ENGINE<<" service"<<std::endl;
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<EXCHANGE_ROUTING_ENGINE<<" service"<<std::endl;
            }
        }
        else if(!_stricmp(m_FailoverInfo.m_AppType.c_str(), EXCHANGE2010))
        {
            if( StrService(EXCHANGE_REPL) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to start MSExchangeRepl service\n");
                stream<<"Failed to start "<<EXCHANGE_REPL<<" service"<<std::endl;
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<EXCHANGE_REPL<<" service"<<std::endl;
            }
            if( StrService(EXCHANGE_TRANSPORT_SERVICE) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to start MSExchangeTransport service\n");
                stream<<"Failed to start "<<EXCHANGE_TRANSPORT_SERVICE<<" service"<<std::endl
					<<"Note: Ignore this failure if Exchange Transport Role is not installed in this machine"<<std::endl;
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<ECHANGE_TRANS<<" service"<<std::endl;
            }
        }
        if (StrService(EXCHANGE_SYSTEM_ATTENDANT) == SVS_FALSE)
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to start MSExchangeSA service\n");
            stream<<"Failed to start "<<EXCHANGE_SYSTEM_ATTENDANT<<" service"<<std::endl;
			bResult = false;
        }
        else
        {
            stream<<"Successfully started "<<EXCHANGE_SYSTEM_ATTENDANT<<" service"<<std::endl;
        }
    }
    WriteStringIntoFile(outputFilePathName,stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bResult;
}

SVERROR MSExchangeController::stopExchangeServices(const std::string &appName,bool m_bCluster)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    USES_CONVERSION;
    if (m_bCluster == false) 
    {
        DebugPrintf( SV_LOG_DEBUG, "Stopping Exchange services\n");
        if (StpService(EXCHANGE_INFORMATION_STORE) == SVS_FALSE)
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to stop Exchange Information Store Service \n");
            return SVE_FAIL;
        }
        changeServiceType(EXCHANGE_INFORMATION_STORE,INM_SVCTYPE_MANUAL);
		if(!_stricmp(appName.c_str(), EXCHANGE) || !_stricmp(appName.c_str(), EXCHANGE2003)) 
        {
            if( StpService(EXCHANGE_MANAGEMENT) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to stop MSExchangeMGMT service \n");
            }
            if( StpService(EXCHANGE_MTA) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to stop MSExchnageMTA service \n");
            }
            if( StpService(EXCHANGE_ROUTING_ENGINE) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to stop RESvc service \n");
            }

            changeServiceType(EXCHANGE_MANAGEMENT, INM_SVCTYPE_MANUAL);
            changeServiceType(EXCHANGE_MTA,INM_SVCTYPE_MANUAL);
            changeServiceType(EXCHANGE_ROUTING_ENGINE, INM_SVCTYPE_MANUAL);
        }
        else if(!_stricmp(appName.c_str(), EXCHANGE2010))
        {
            if( StpService(EXCHANGE_REPL) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to stop MSExchangeRepl service\n");
            }
            if( StpService(ECHANGE_TRANS) != SVS_OK )
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to stop MSExchangeTransportLogSearch service\n");
            }

            changeServiceType(EXCHANGE_REPL, INM_SVCTYPE_MANUAL);
            changeServiceType(ECHANGE_TRANS, INM_SVCTYPE_MANUAL);				
        }
        if (StpService(EXCHANGE_SYSTEM_ATTENDANT) == SVS_FALSE)
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to stop MSExchangeSA service\n");
        }

        changeServiceType(EXCHANGE_SYSTEM_ATTENDANT, INM_SVCTYPE_MANUAL);
        DebugPrintf( SV_LOG_DEBUG, "Successfully stopped all the required Exchange Services\n" );
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return SVS_OK;

}

void MSExchangeController::UpdateConfigFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_ProtectionSettingsChanged = false;
    if(m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0)
    {
        if(GetSourceDiscoveryInformation() == SVS_OK)
        {
            if(m_FailoverInfo.m_AppType.compare("EXCHANGE2010") != 0)
            {
                CreateExchangeDiscoveryInfoFiles();
            }
            else
            {
                CreateExchange2010DiscoveryInfoFiles();
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Source discovery information\n");
        }
    }
    CreateExchangeFailoverServicesConfFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    if(schedulerPtr->isPolicyEnabled(m_policyId))
    {
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    }
    if( configuratorPtr->getApplicationPolicies(policyId, "EXCHANGE",m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void MSExchangeController::BuildFailoverCommand(std::string& failoverCmd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator localconfigurator ;
    std::stringstream cmdStream;

    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "Application.exe";
    cmdStream <<"\"";
    const std::string& policyType = m_FailoverInfo.m_PolicyType;
    if(policyType.compare("ProductionServerPlannedFailover") == 0  || 
        policyType.compare("DRServerPlannedFailover") == 0 ||
        policyType.compare("ProductionServerPlannedFailback") == 0 ||
        policyType.compare("DRServerPlannedFailback") == 0)
    {
        cmdStream << " -planned " ;
    }
    else if(policyType.compare("DRServerUnPlannedFailover") == 0)
    {
        cmdStream << " -unplanned ";
    }
    cmdStream << " -app " ;
    if(m_FailoverInfo.m_AppType.compare("EXCHANGE2003") == 0 ||
        m_FailoverInfo.m_AppType.compare("Exchange2003") == 0 )
    {
        m_FailoverInfo.m_AppType  = "EXCHANGE";
    }
    cmdStream << m_FailoverInfo.m_AppType;
    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
    {
        cmdStream << " -failover ";
        cmdStream <<" -s ";
        cmdStream << m_FailoverInfo.m_ProductionServerName;
        cmdStream <<  " -t ";
        cmdStream << m_FailoverInfo.m_DRServerName;
    }
    else if(m_FailoverInfo.m_FailoverType.compare("Failback") == 0)
    {
        cmdStream << " -failback ";
        cmdStream <<" -s ";
        cmdStream << m_FailoverInfo.m_DRServerName;
        cmdStream <<  " -t ";
        cmdStream << m_FailoverInfo.m_ProductionServerName;
    }
    cmdStream << " -builtIn ";
    cmdStream << " -tag ";
    if(policyType.compare("ProductionServerPlannedFailover") == 0 || 
        policyType.compare("DRServerPlannedFailover") == 0 ||
        policyType.compare("ProductionServerPlannedFailback") == 0 ||
        policyType.compare("DRServerPlannedFailback") == 0)
    {
        cmdStream << m_FailoverInfo.m_TagName;
        if(policyType.compare("DRServerPlannedFailover") == 0 ||
            policyType.compare("ProductionServerPlannedFailback") == 0)
        {
            cmdStream << " -tagType USERDEFINED ";
        }
    }
    else if(policyType.compare("DRServerUnPlannedFailover") == 0)
    {
        if(m_FailoverInfo.m_RecoveryPointType.compare("LCCP") == 0)
        {
            cmdStream << " LATEST ";
        }
        else if(m_FailoverInfo.m_RecoveryPointType.compare("LCT") == 0)
        {
            cmdStream << " LATESTTIME ";
        }
        else if(m_FailoverInfo.m_RecoveryPointType.compare("CUSTOM") == 0)
        {
            cmdStream << " CUSTOM ";
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Unknown recoverypoint type %s\n",m_FailoverInfo.m_RecoveryPointType.c_str());
        }
    }
    if( policyType.compare("DRServerUnPlannedFailover") == 0 ||
        policyType.compare("DRServerPlannedFailover") == 0 ||
        policyType.compare("ProductionServerPlannedFailback") == 0 )
    {
        try
        {
            if(boost::lexical_cast<bool>(m_FailoverInfo.m_DNSFailover)== false)
            {
                cmdStream << " -nodnsfailover ";
            }
            if(boost::lexical_cast<bool>(m_FailoverInfo.m_ADFailover) == true)
            {
                cmdStream << " -doadreplication ";
            }
            else
            {
                cmdStream << " -noadreplication ";
            }
        }
        catch(boost::bad_lexical_cast &be)
        {
            DebugPrintf(SV_LOG_ERROR,"Caught boost lexical_cast Exception %s\n",be.what());
        }

    }
    if( policyType.compare("ProductionServerPlannedFailover") == 0 ||
        policyType.compare("DRServerPlannedFailback") == 0 )
    {
        try
        {
            if(boost::lexical_cast<bool>(m_FailoverInfo.m_DNSFailover) == false)
            {
                cmdStream << " -nodnsfailover ";
            }
        }
        catch(boost::bad_lexical_cast &be)
        {
            DebugPrintf(SV_LOG_ERROR,"Caught boost lexical_cast Exception %s\n",be.what());
        }
    }
    if(m_FailoverData.m_RepointInfoMap.size())
    {
        std::map<std::string,std::string>::iterator privateMailServerIterator = m_FailoverData.m_RepointInfoMap.begin();
        std::string publicFolderSource;
        std::string publicFolderTarget;
        if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
        {
            publicFolderSource = m_FailoverInfo.m_ProductionVirtualServerName;
        }
        else
        {
            publicFolderSource = m_FailoverInfo.m_ProductionServerName;
        }
        if(!m_FailoverInfo.m_DRVirtualServerName.empty())
        {
            publicFolderTarget = m_FailoverInfo.m_DRVirtualServerName;
        }
        else
        {
            publicFolderTarget = m_FailoverInfo.m_DRServerName;
        }
        while(privateMailServerIterator != m_FailoverData.m_RepointInfoMap.end())
        {

            if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
            {
                if((privateMailServerIterator->first.compare(publicFolderSource) != 0 &&
                    privateMailServerIterator->second.compare("0") == 0 )|| 
                    (privateMailServerIterator->first.compare(publicFolderSource) == 0 && 
                    privateMailServerIterator->second.compare("0") == 0))
                {
                    privateMailServerIterator++;
                    continue;
                }
            }
            else 
            {
                if((privateMailServerIterator->first.compare(publicFolderTarget) != 0 &&
                    privateMailServerIterator->second.compare("0") == 0 )|| 
                    (privateMailServerIterator->first.compare(publicFolderTarget) == 0 && 
                    privateMailServerIterator->second.compare("0") == 0))
                {
                    privateMailServerIterator++;
                    continue;
                }
            }
            if(privateMailServerIterator->first.compare("0") != 0 && 
                privateMailServerIterator->first != privateMailServerIterator->second) 

            {
                cmdStream << " -PFS " ;
                cmdStream << privateMailServerIterator->first;
                cmdStream << " -PFT " ;
                if(privateMailServerIterator->second.compare("0") != 0)
                {
                    cmdStream << privateMailServerIterator->second;
                }
                else
                {
                    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
                    {
                        cmdStream << publicFolderTarget;
                    }
                    else 
                    {
                        cmdStream << publicFolderSource;
                    }
                }
                break;
            }
            privateMailServerIterator++;
        }
    }
    if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == false)
    {
        if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
        {
            cmdStream << " -sourcevs ";
            cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
            cmdStream << " -targetvs ";
            cmdStream << m_FailoverInfo.m_DRVirtualServerName;
        }
        else if(m_FailoverInfo.m_FailoverType.compare("Failback") == 0)
        {
            cmdStream << " -sourcevs ";
            cmdStream << m_FailoverInfo.m_DRVirtualServerName;
            cmdStream << " -targetvs ";
            cmdStream <<  m_FailoverInfo.m_ProductionVirtualServerName;
        }

    }
    else if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == true)
    {
        cmdStream << " -virtualserver ";
        cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Standalone case\n");
    }
    if(m_FailoverInfo.m_ExtraOptions.empty() == false)
    {
        cmdStream << "  ";
        cmdStream << m_FailoverInfo.m_ExtraOptions;
    }
    cmdStream << " -config ";
    cmdStream <<"\"";
    cmdStream << m_FailoverInfo.m_AppConfFilePath;
    cmdStream <<"\"";
    DebugPrintf(SV_LOG_DEBUG,"FailoverCommand is %s\n",cmdStream.str().c_str());
    failoverCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::PrePareFailoverJobs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    FailoverJob failoverJob;
    FailoverCommand failoverCmd;
    if(m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0)
    {

        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_SOURCE_SCRIPT;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
        else
        {
            if(m_FailoverInfo.m_Prescript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Prescript;
                failoverCmd.m_GroupId = PRESCRIPT_SOURCE;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0)
            {
                std::string command;
                if(m_FailoverInfo.m_RepointAllStores.compare("1") == 0)
                {
                    std::set<std::string> mailServers;
                    std::string exFailoverCmd;
                    std::string srcMailServerName;
                    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
                    {
                        if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
                        {
                            srcMailServerName = m_FailoverInfo.m_ProductionVirtualServerName;
                        }
                        else
                        {
                            srcMailServerName = m_FailoverInfo.m_ProductionServerName;
                        }
                    }
                    else
                    {
                        if(!m_FailoverInfo.m_DRVirtualServerName.empty())
                        {
                            srcMailServerName = m_FailoverInfo.m_DRVirtualServerName;
                        }
                        else
                        {
                            srcMailServerName = m_FailoverInfo.m_DRServerName;
                        }
                    }
                    ExchangeApplication::getdependentaPrivateMailServers(mailServers,srcMailServerName);
                    std::set<std::string>::iterator mailServerIter = mailServers.begin();
                    SV_INT stepId = 1;
                    while(mailServerIter != mailServers.end())
                    {
                        exFailoverCmd.clear();
                        BuildExFailoverCommand(exFailoverCmd,*mailServerIter);
                        failoverCmd.m_Command = exFailoverCmd;
                        failoverCmd.m_GroupId = REPOINT_PRIVATE_STORES;
                        failoverCmd.m_StepId = stepId++;
                        failoverJob.m_FailoverCommands.push_back(failoverCmd);
                        mailServerIter++;
                    }
                }
                BuildProductionServerFastFailbackExFailoverCmd(command);
                failoverCmd.m_Command = command;
                failoverCmd.m_GroupId = FAST_FAILABACK_EXFAILOVER;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
                command.clear();
                BuildDNSCommand(command);
                failoverCmd.m_Command = command;
                failoverCmd.m_GroupId = FAST_FAILBACK_DNS_FAILOVER;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
                command.clear();
                if(m_FailoverInfo.m_startApplicationService.compare("1") == 0)
                {
                    failoverCmd.m_GroupId = START_APP_CLUSTER;
                    failoverCmd.m_StepId = 1;
                    failoverCmd.m_Command = "Application Agent will start the sevices after successful completion of failover";
                    failoverJob.m_FailoverCommands.push_back(failoverCmd);
                }
                if(m_FailoverInfo.m_ADFailover.compare("1") == 0)
                {
                    BuildWinOpCommand(command);
                    failoverCmd.m_Command = command;
                    failoverCmd.m_GroupId = FAST_FAILBACK_WINOP;
                    failoverCmd.m_StepId = 1;
                    failoverJob.m_FailoverCommands.push_back(failoverCmd);
                    command.clear();
                }

            }
            else
            {
                std::string failoverCmdStr;
                BuildFailoverCommand(failoverCmdStr);
                failoverCmd.m_Command = failoverCmdStr;
                failoverCmd.m_GroupId = ISSUE_CONSISTENCY;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_SOURCE;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
    }
    else if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0)
    {

        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_TARGET_SCRIPT;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
        else
        {
            if(m_FailoverInfo.m_Prescript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Prescript;
                failoverCmd.m_GroupId = PRESCRIPT_TARGET;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            std::string failoverCmdStr;
            BuildFailoverCommand(failoverCmdStr);
            failoverCmd.m_Command = failoverCmdStr;
            failoverCmd.m_GroupId = ROLLBACK_VOLUMES;
            failoverCmd.m_StepId = 1;
            failoverJob.m_FailoverCommands.push_back(failoverCmd);
            if(m_FailoverInfo.m_RepointAllStores.compare("1") == 0)
            {
                std::set<std::string> mailServers;
                std::string exFailoverCmd;
                std::string srcMailServerName;
                if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
                {
                    if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
                    {
                        srcMailServerName = m_FailoverInfo.m_ProductionVirtualServerName;
                    }
                    else
                    {
                        srcMailServerName = m_FailoverInfo.m_ProductionServerName;
                    }
                }
                else
                {
                    if(!m_FailoverInfo.m_DRVirtualServerName.empty())
                    {
                        srcMailServerName = m_FailoverInfo.m_DRVirtualServerName;
                    }
                    else
                    {
                        srcMailServerName = m_FailoverInfo.m_DRServerName;
                    }
                }
                ExchangeApplication::getdependentaPrivateMailServers(mailServers,srcMailServerName);
                std::set<std::string>::iterator mailServerIter = mailServers.begin();
                SV_INT stepId = 1;
                while(mailServerIter != mailServers.end())
                {
                    exFailoverCmd.clear();
                    BuildExFailoverCommand(exFailoverCmd,*mailServerIter);
                    failoverCmd.m_Command = exFailoverCmd;
                    failoverCmd.m_GroupId = REPOINT_PRIVATE_STORES;
                    failoverCmd.m_StepId = stepId++;
                    failoverJob.m_FailoverCommands.push_back(failoverCmd);
                    mailServerIter++;
                }
            }
            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_TARGET;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_ConvertToCluster.compare("1") == 0)
            {
                failoverCmd.m_GroupId = CLUSTER_RECONSTRUCTION;
                failoverCmd.m_StepId = 1;
                failoverCmd.m_State = CLUSTER_RECONSTRUCTION_NOTSTARTED;
				std::string clusConstructionCmdStr;
				std::string clusConsCmdStr = " \
											   1. Make sure to up passive nodes up after failover or failback completed \
											   2. Make sure all the resources online to make application available";
				BuildClusReConstructionCommand(clusConstructionCmdStr);
                failoverCmd.m_Command = clusConstructionCmdStr + clusConsCmdStr;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_startApplicationService.compare("1") == 0)
            {
                failoverCmd.m_GroupId = START_APP_CLUSTER;
                failoverCmd.m_StepId = 1;
				if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
				{
					std::string appServiceCmd;
					failoverCmd.m_Command = " Manual Intervention required to start the services after successful completion of failover \
											  The required services to start are: 1. Microsoft Exchange Information Store \
																				  2. Microsoft Exchange System Attendant";
					OSVERSIONINFOEX osVersionInfo ;
					getOSVersionInfo(osVersionInfo) ;
					if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
					{
						appServiceCmd = " 3. Microsoft Exchange Database Instance ";
					}
					else
					{
						appServiceCmd = " 3. Microsoft Exchange Message Transfer Agent ";
					}
					failoverCmd.m_Command = failoverCmd.m_Command + appServiceCmd;
				}
				else
				{
					std::string appServiceCmd;
					failoverCmd.m_Command = " Manual Intervention required to start the services after successful completion of failover \
									          The required services to start are: 1. MSExchangeIS \
																		          2. MSExchangeSA ";
					if(!_stricmp(m_FailoverInfo.m_AppType.c_str(), EXCHANGE)) 
					{
						appServiceCmd = " 3. MSExchangeMGMT \
										  4. MSExchangeMTA \
										  5. RESvc ";
					}
					else if(!_stricmp(m_FailoverInfo.m_AppType.c_str(), EXCHANGE2010))
					{
						appServiceCmd = " 3. MSExchangeRepl \
										  4. MSExchangeTransportLogSearch ";
					}
					failoverCmd.m_Command = failoverCmd.m_Command + appServiceCmd;
				}
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }

        }
    }
    AddFailoverJob(m_policyId,failoverJob);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::BuildExFailoverCommand(std::string& exFailoverCmd,const std::string& mailServerName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ; 
    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    std::string publicFolderSource;
    std::string publicFolderTarget; 
    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "exfailover.exe";
    cmdStream <<"\"";
    cmdStream <<" -failover ";
    cmdStream <<" -s ";       
    cmdStream <<mailServerName;
    cmdStream <<" -t ";
    cmdStream <<mailServerName;
    if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
    {
        publicFolderSource = m_FailoverInfo.m_ProductionVirtualServerName;
    }
    else
    {
        publicFolderSource = m_FailoverInfo.m_ProductionServerName;
    }
    if(!m_FailoverInfo.m_DRVirtualServerName.empty())
    {
        publicFolderTarget = m_FailoverInfo.m_DRVirtualServerName;
    }
    else
    {
        publicFolderTarget = m_FailoverInfo.m_DRServerName;
    }
    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
    {
        cmdStream << " -PFS " ;
        cmdStream << publicFolderSource;
        cmdStream << " -PFT " ;
        cmdStream << publicFolderTarget;
    }
    else
    {
        cmdStream << " -PFS " ;
        cmdStream << publicFolderTarget;
        cmdStream << " -PFT " ;
        cmdStream << publicFolderSource;
    }
    cmdStream << " -cs ";
    cmdStream << " -autorefine ";
    exFailoverCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG,"BuildExFailoverCommand is %s\n",exFailoverCmd.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::BuildProductionServerFastFailbackExFailoverCmd(std::string& exfailoverCmd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "exfailover.exe";
    cmdStream <<"\"";
    cmdStream <<" -failback ";
    if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == false)
    {

        cmdStream << " -s ";
        cmdStream << m_FailoverInfo.m_DRVirtualServerName;
        cmdStream << " -t ";
        cmdStream <<  m_FailoverInfo.m_ProductionVirtualServerName;
    }
    else if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == true)
    {
        cmdStream << " -s ";
        cmdStream << m_FailoverInfo.m_DRServerName;
        cmdStream << " -t ";
        cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
    }
    else
    {
        cmdStream << " -s ";
        cmdStream << m_FailoverInfo.m_DRServerName;
        cmdStream << " -t ";
        cmdStream <<  m_FailoverInfo.m_ProductionServerName;
    }
    cmdStream << " -autorefine";
	if(!m_FailoverInfo.m_ExtraOptions.empty())
	{
		cmdStream << " " << m_FailoverInfo.m_ExtraOptions.c_str();
	}
    exfailoverCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG,"BuildExFailoverCommand is %s\n",exfailoverCmd.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::CreateExchangeDiscoveryInfoFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    CreateExchangeDiscoveryDBInfoFile();
    CreateExchangeDiscoveryLogInfoFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void MSExchangeController::CreateExchange2010DiscoveryInfoFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    CreateExchange2010DiscoveryDBInfoFile();
    CreateExchange2010DiscoveryLogInfoFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void MSExchangeController::CreateExchangeDiscoveryDBInfoFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string exchangeVersion;
    std::ofstream out;
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH+ '\\'+ DATA + "\\";
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH + "\\";
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
    }
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_exchange_config.dat";
    copyFileName +=  srcHostName + "_exchange_config.dat";
    out.open(fileName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s\n",fileName.c_str());
        stream<<"Failed to open file :"<<fileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(fileName);
        }

    }
    out << "DISCOVERY VERSION"<<"\t"<<"1.2"<<std::endl;
    out << "HOST NAME"<<"\t"<<m_FailoverInfo.m_ProductionServerName<<std::endl;
    GetValueFromMap(m_SrcDiscInfo.m_properties,"VersionName",exchangeVersion);
    out << "VERSION"<<"\t"<<exchangeVersion<<std::endl;    
    out << "VOLUMES" <<"\t";
    std::set<std::string> volumesSet;
    std::map<std::string, SrcExchStorageGrpInfo>::iterator storageGrpInfoIter = m_SrcDiscInfo.m_storageGrpMap.begin();
    while(storageGrpInfoIter != m_SrcDiscInfo.m_storageGrpMap.end())
    {
        SrcExchStorageGrpInfo srcExchStorageGrpInfo = storageGrpInfoIter->second;
        if(srcExchStorageGrpInfo.m_properties.find("VolumeName") != srcExchStorageGrpInfo.m_properties.end())
        {
            std::string storageGroupVolume = srcExchStorageGrpInfo.m_properties.find("VolumeName")->second;
			volumesSet.insert(storageGroupVolume);
            // out<< storageGroupVolume <<"\t";
			if(srcExchStorageGrpInfo.m_properties.find("SystemVolume") != srcExchStorageGrpInfo.m_properties.end())
			{
				volumesSet.insert(srcExchStorageGrpInfo.m_properties.find("SystemVolume")->second);
			}
            std::map<std::string,SrcExchMailStoreInfo>::iterator MailStoreInfoIter = storageGrpInfoIter->second.m_mailStores.begin();
            while(MailStoreInfoIter != storageGrpInfoIter->second.m_mailStores.end())
            {
                SrcExchMailStoreInfo srcMailStoreInfo = MailStoreInfoIter->second;
                std::map<std::string,std::string>::iterator mailStoreFilesIter = srcMailStoreInfo.mailStoreFiles.begin() ;
                if(mailStoreFilesIter != srcMailStoreInfo.mailStoreFiles.end())
                {
                    if(storageGroupVolume.compare(mailStoreFilesIter->second) != 0)
                    {

                        volumesSet.insert(mailStoreFilesIter->second);
                        //  out<< mailStoreFilesIter->second <<"\t";
                        mailStoreFilesIter++;
                    }
                }
                MailStoreInfoIter++;
            }
        }
        storageGrpInfoIter++;
    }
    std::set<std::string>::iterator volumesSetIter =  volumesSet.begin();
    while(volumesSetIter != volumesSet.end())
    {
        out<< *volumesSetIter <<"\t";
        volumesSetIter++;
    }
    out <<std::endl;
    out.close();
    if(!CopyConfFile(fileName.c_str(),copyFileName.c_str()))
    {
        std::stringstream stream;
		DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
        stream<<"Failed to copy discovery db file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::CreateExchangeDiscoveryLogInfoFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator localconfigurator ;
    std::ofstream out;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH+ "\\" + DATA + "\\";
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
        throw FileNotFoundException(fileName);
    }
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH + "\\";
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_exchange_log_config.dat";
    copyFileName +=  srcHostName + "_exchange_log_config.dat";
    out.open(fileName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s\n",fileName.c_str());
        stream<<"Failed to open  file :"<<fileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(fileName);
        }
    }
    std::map<std::string, SrcExchStorageGrpInfo>::iterator storageGrpInfoIter = m_SrcDiscInfo.m_storageGrpMap.begin();
    while(storageGrpInfoIter != m_SrcDiscInfo.m_storageGrpMap.end())
    {
        SrcExchStorageGrpInfo srcExchStorageGrpInfo = storageGrpInfoIter->second;
        if(srcExchStorageGrpInfo.m_properties.find("LogLocation") != srcExchStorageGrpInfo.m_properties.end())
        {
            out<< srcExchStorageGrpInfo.m_properties.find("LogLocation")->second <<"\t";
        }
        if(srcExchStorageGrpInfo.m_properties.find("VolumeName") != srcExchStorageGrpInfo.m_properties.end())
        {
            out<< srcExchStorageGrpInfo.m_properties.find("VolumeName")->second<<"\t";
        }
        out<<std::endl;
        if(srcExchStorageGrpInfo.m_properties.find("SystemPath") != srcExchStorageGrpInfo.m_properties.end())
        {
            out<< srcExchStorageGrpInfo.m_properties.find("SystemPath")->second <<"\t";
        }
        if(srcExchStorageGrpInfo.m_properties.find("SystemVolume") != srcExchStorageGrpInfo.m_properties.end())
        {
            out<< srcExchStorageGrpInfo.m_properties.find("SystemVolume")->second<<"\t";
        }
        out<<std::endl;
        storageGrpInfoIter++;
    }

    storageGrpInfoIter = m_SrcDiscInfo.m_storageGrpMap.begin();
    while(storageGrpInfoIter != m_SrcDiscInfo.m_storageGrpMap.end())
    {
        SrcExchStorageGrpInfo srcExchStorageGrpInfo = storageGrpInfoIter->second;
        std::map<std::string, SrcExchMailStoreInfo>::iterator mailStoreIter =  srcExchStorageGrpInfo.m_mailStores.begin(); 
        while(mailStoreIter != srcExchStorageGrpInfo.m_mailStores.end())
        {
            SrcExchMailStoreInfo srcExchMailSotreInfo = mailStoreIter->second; 
            std::map<std::string,std::string>::iterator  mailStoreFilesIter = srcExchMailSotreInfo.mailStoreFiles.begin();
            while(mailStoreFilesIter != srcExchMailSotreInfo.mailStoreFiles.end())
            {
                out<<mailStoreFilesIter->first<<"\t"<<mailStoreFilesIter->second<<"\t"<<std::endl;
                mailStoreFilesIter++;
            }
            mailStoreIter++;
        }
        storageGrpInfoIter++;
    }
    out.close();
    if(!CopyConfFile(fileName.c_str(),copyFileName.c_str()))
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
        stream<<"Failed to copy discovery log file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void MSExchangeController::CreateExchangeFailoverServicesConfFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::ofstream out;
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" + DATA + "\\";
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
        throw FileNotFoundException(fileName);
    }
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH + "\\";
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_FailoverServices.conf";
    copyFileName +=  srcHostName + "_FailoverServices.conf";
    out.open(fileName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to createFile %s\n",fileName.c_str());
        stream<<"Failed to open file :"<<fileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(fileName);
        }
    }

    out<<"["<< m_FailoverInfo.m_AppType<<"]"<<std::endl;
    for(int i = 0;i<2;i++)
    {
        std::list<std::string>::iterator servicesIter = m_FailoverData.services.begin() ;        
        (i == 0)?out<<"START = ": out<<"STOP = ";
        while(servicesIter != m_FailoverData.services.end())
        {
            out<<*servicesIter<<",";
            servicesIter++;
        }
        out<<std::endl;
    }
    if(m_FailoverInfo.m_FailoverType.compare("Failback") == 0)
    {
        out<<"[Failback_IP_Address]"<<std::endl;
        if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == true)
        {
            out << m_FailoverInfo.m_ProductionServerName << " = "<< m_FailoverInfo.m_ProductionServerIp;
        }
        else
        {
            out << m_FailoverInfo.m_ProductionVirtualServerName << " = "<< m_FailoverInfo.m_ProductionVirtualServerIp;
        }
    }
    out.close();
    if(!CopyConfFile(fileName.c_str(),copyFileName.c_str()))
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
        stream<<"Failed to copy failover services conf file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR MSExchangeController::GetSourceDiscoveryInformation()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;

    std::string marshalledSrcDiscInfoStr = m_AppProtectionSettings.srcDiscoveryInformation;
    try
    {
        if(m_FailoverInfo.m_AppType.compare("EXCHANGE2010") != 0)
        {
            m_SrcDiscInfo = unmarshal<SrcExchangeDiscInfo>(marshalledSrcDiscInfoStr) ;
        }
        else
        {
            m_Exchange2010DiscInfo = unmarshal<SrcExchange2010DiscInfo>(marshalledSrcDiscInfoStr) ;
        }
        bRet = SVS_OK;
    }   
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledSrcDiscInfoStr.c_str());
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") == 0)
        {
            bRet = SVS_OK;
        }
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal srcDiscoveryInformation %s\n for the policy %ld", ex.what(),m_policyId) ;
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledSrcDiscInfoStr.c_str());
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") == 0)
        {
            bRet = SVS_OK;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;   
}

void MSExchangeController::updateJobStatus(InmTaskState inm,SV_ULONG policyId,const std::string &jobResult,const std::string &LogMsg,bool updateCxStatus ,bool updateSchedulerStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    if(updateCxStatus)
    {
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
        AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,jobResult,LogMsg) ;
    }
    if(updateSchedulerStatus)
    {
        AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSExchangeController::PreClusterReconstruction()
{
    restoreExchangeSearchDir() ;    
}

void MSExchangeController::CreateExchange2010DiscoveryLogInfoFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator localconfigurator ;
    std::ofstream out;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH+ "\\" + DATA + "\\";
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
        throw FileNotFoundException(fileName);
    }
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH + "\\";
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_exchange_log_config.dat";
    copyFileName +=  srcHostName + "_exchange_log_config.dat";
    out.open(fileName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s\n",fileName.c_str());
        stream<<"Failed to open  file :"<<fileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(fileName);
        }
    }
    std::map<std::string, SrcExchMailStoreInfo>::iterator mailStoreIter =  m_Exchange2010DiscInfo.m_mailStores.begin(); 
    while(mailStoreIter != m_Exchange2010DiscInfo.m_mailStores.end())
    {
        SrcExchMailStoreInfo srcMailStoreInfo = mailStoreIter->second;
        if(srcMailStoreInfo.m_properties.find("LogLocation") != srcMailStoreInfo.m_properties.end())
        {
            std::string logLocation = srcMailStoreInfo.m_properties.find("LogLocation")->second;
            out<< logLocation <<"\t";
        }
        if(srcMailStoreInfo.m_properties.find("VolumeName") != srcMailStoreInfo.m_properties.end())
        {
            std::string mailstoreVolume = srcMailStoreInfo.m_properties.find("VolumeName")->second;
            out<< mailstoreVolume <<"\t";
        }
        out<<std::endl;
        mailStoreIter++;
    }
    mailStoreIter =  m_Exchange2010DiscInfo.m_mailStores.begin(); 
    while(mailStoreIter != m_Exchange2010DiscInfo.m_mailStores.end())
    {
        SrcExchMailStoreInfo srcExchMailSotreInfo = mailStoreIter->second; 
        std::map<std::string,std::string>::iterator mailStoreFilesIter = srcExchMailSotreInfo.mailStoreFiles.begin();
        while(mailStoreFilesIter != srcExchMailSotreInfo.mailStoreFiles.end())
        {
            out<<mailStoreFilesIter->first<<"\t"<<mailStoreFilesIter->second<<"\t"<<std::endl;
            mailStoreFilesIter++;
        }
        mailStoreIter++;
    }
    out.close();
    std::string exchRegKeyDbStateFile = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH+ "\\" + DATA + "\\" + srcHostName + "_ExchRegKeyDbState.txt";
    std::string exchRegKeyDbStateFile_copyFile = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH+ "\\";
    exchRegKeyDbStateFile_copyFile += srcHostName + "_ExchRegKeyDbState.txt";

    std::ofstream regOutStream;
    regOutStream.open( exchRegKeyDbStateFile.c_str(), std::ofstream::out );
    if( regOutStream.is_open() && regOutStream.good() )
    {
        mailStoreIter =  m_Exchange2010DiscInfo.m_mailStores.begin(); 
        while(mailStoreIter != m_Exchange2010DiscInfo.m_mailStores.end())
        {
            SrcExchMailStoreInfo srcExchMailSotreInfo = mailStoreIter->second; 
            if(srcExchMailSotreInfo.m_properties.find("GUID") != srcExchMailSotreInfo.m_properties.end() &&
                srcExchMailSotreInfo.m_properties.find("MountInfo") != srcExchMailSotreInfo.m_properties.end() )
            {
                regOutStream <<"[" << srcExchMailSotreInfo.m_properties.find("GUID")->second << "]=\"" 
                    << srcExchMailSotreInfo.m_properties.find("MountInfo")->second << "\"" << std::endl;
            }
            mailStoreIter++;
        }
        regOutStream.flush();
        regOutStream.close();
    }
    else
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", exchRegKeyDbStateFile.c_str() );
    }
    if(!CopyConfFile(fileName.c_str(),copyFileName.c_str()))
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
        stream<<"Failed to copy discovery log file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    if(!CopyConfFile(exchRegKeyDbStateFile.c_str(), exchRegKeyDbStateFile_copyFile.c_str()))
    {
        std::stringstream stream;
		DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",exchRegKeyDbStateFile_copyFile.c_str(),GetLastError());
        stream<<"Failed to copy discovery log file : " << exchRegKeyDbStateFile_copyFile;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(exchRegKeyDbStateFile_copyFile);
        }
    }   

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void MSExchangeController::CreateExchange2010DiscoveryDBInfoFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string exchangeVersion;
    std::ofstream out;
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH+ '\\'+ DATA + "\\";
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH + "\\";
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
    }
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_exchange_config.dat";
    copyFileName +=  srcHostName + "_exchange_config.dat";
    out.open(fileName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s\n",fileName.c_str());
        stream<<"Failed to open file :"<<fileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(fileName);
        }

    }
    out << "DISCOVERY VERSION"<<"\t"<<"1.2"<<std::endl;
    out << "HOST NAME"<<"\t"<<m_FailoverInfo.m_ProductionServerName<<std::endl;
    GetValueFromMap(m_Exchange2010DiscInfo.m_properties,"VersionName",exchangeVersion);
    out << "VERSION"<<"\t"<<exchangeVersion<<std::endl;    
    out << "VOLUMES" <<"\t";
    std::set<std::string> volumesSet;
    std::map<std::string,SrcExchMailStoreInfo>::iterator MailStoreInfoIter = m_Exchange2010DiscInfo.m_mailStores.begin();
    while(MailStoreInfoIter != m_Exchange2010DiscInfo.m_mailStores.end())
    {
        SrcExchMailStoreInfo srcMailStoreInfo = MailStoreInfoIter->second;
        std::map<std::string,std::string>::iterator mailStoreFilesIter = srcMailStoreInfo.mailStoreFiles.begin();
        while(mailStoreFilesIter != srcMailStoreInfo.mailStoreFiles.end())
        {
            // out<< mailStoreFilesIter->second <<"\t";
            volumesSet.insert(mailStoreFilesIter->second);
            mailStoreFilesIter++;
        }
		if(srcMailStoreInfo.m_properties.find("VolumeName") != srcMailStoreInfo.m_properties.end())
			volumesSet.insert(srcMailStoreInfo.m_properties.find("VolumeName")->second); // Log file volume;
        MailStoreInfoIter++;
    }
    std::set<std::string>::iterator volumesSetIter = volumesSet.begin();
    while(volumesSetIter != volumesSet.end())
    {
        out<< *volumesSetIter <<"\t";
        volumesSetIter++;
    }
    out <<std::endl;
    out.close();
    if(!CopyConfFile(fileName.c_str(),copyFileName.c_str()))
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
        stream<<"Failed to copy discovery db file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void MSExchangeController::onlineDependentResources()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ClusterOp clusOpObj;
    std::string resourceName;
    std::list<std::string> ResourceTypeList;
    std::list<std::string> dependentResourceList; 
    if( getResourcesByType("", "Microsoft Exchange System Attendant", ResourceTypeList) == SVS_OK )
    {
        bool bfound = false;
        std::list<std::string>::iterator listIter = ResourceTypeList.begin();  
        while(listIter != ResourceTypeList.end())
        {
            getDependedResorceNameListOfType(*listIter,"Network Name",dependentResourceList);
            std::list<std::string>::iterator dependentResourceListIter = dependentResourceList.begin();
            while(dependentResourceListIter != dependentResourceList.end())
            {
                if( (strcmpi((*dependentResourceListIter).c_str(), m_FailoverInfo.m_ProductionVirtualServerName.c_str()) == 0))
                {
                    resourceName = *listIter;
                    DebugPrintf(SV_LOG_DEBUG,"ResourceName = %s\n",resourceName.c_str());
                    bfound = true;
                    break;
                }
                dependentResourceListIter++;
            }
            if(bfound == true)
            {
                break;
            }
            listIter++;
        }
    }
    std::string errLog;
    std::list<RescoureInfo> cluterResouceInfoList;
    clusOpObj.getClusterResources(cluterResouceInfoList);
    std::list<RescoureInfo>::iterator cluterResouceInfoListIter = cluterResouceInfoList.begin();
    while(cluterResouceInfoListIter !=  cluterResouceInfoList.end())
    {    
        dependentResourceList.clear();
        getDependedResorceNameListOfType(cluterResouceInfoListIter->rescourceName,"Microsoft Exchange System Attendant",dependentResourceList);
        std::list<std::string>::iterator dependentResourceListIter = dependentResourceList.begin();
        while(dependentResourceListIter != dependentResourceList.end())
        {
            DebugPrintf(SV_LOG_DEBUG,"Cluster ResourceName = %s\n",(*dependentResourceListIter).c_str());
            if( (strcmpi((*dependentResourceListIter).c_str(), resourceName.c_str()) == 0))
            {
                errLog.clear();
                if(clusOpObj.onlineClusterResources(cluterResouceInfoListIter->rescourceName,errLog) == SVS_OK)
                {
                    DebugPrintf(SV_LOG_DEBUG,"Initiated Cluster Resource = %s for online\n",cluterResouceInfoListIter->rescourceName.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG,"Error while onling  the resource %s\n",errLog.c_str());
                }
            }
            dependentResourceListIter++;
        }
        cluterResouceInfoListIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
