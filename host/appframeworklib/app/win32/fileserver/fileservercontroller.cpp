#include "appglobals.h"
#include "fileservercontroller.h"
#include "controller.h"
#include "system.h"
#include "service.h"
#include "util/common/util.h"
#include <boost/lexical_cast.hpp>
#include "fileserverapplication.h"
#include "consistency/taggenerator.h"
#include "Clusteroperation.h"
#include "host.h"
#include "appexception.h"
#include "fileserverdiscovery.h"
#include "registry.h"
#include "systemvalidators.h"
#include <stdlib.h>

FileServerControllerPtr FileServerController::m_FSinstancePtr;

FileServerControllerPtr FileServerController::getInstance(ACE_Thread_Manager* tm)
{
    if( m_FSinstancePtr.get() == NULL )
    {
        m_FSinstancePtr.reset(new  FileServerController(tm) ) ;
		if( m_FSinstancePtr.get() != NULL )
			m_FSinstancePtr->Init();
    }
    return m_FSinstancePtr ;
}

bool FileServerController::isItW2k8R2ClusterNode()
{
	bool bRet = false ;
	if(isClusterNode())
	{
		OSVERSIONINFOEX osVersionInfo ;
		getOSVersionInfo(osVersionInfo) ;
		if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
		{
			bRet = true;
		}
	}
	return bRet;
}

int FileServerController::open(void *args)
{
    return this->activate(THR_BOUND);
}

int FileServerController::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

int FileServerController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return  0 ;
}

FileServerController::FileServerController(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
    //TODO:: controller specific code
}

SVERROR  FileServerController::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;
    bool bProcess = false ;
    m_bCluster = isClusterNode();
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    if( propsMap.find("PolicyType")->second.compare("Consistency") == 0 )
    {
        if( Consistency() == SVS_OK )
        {
            retStatus = SVS_OK;
        }
        else
        {
            DebugPrintf( SV_LOG_ERROR, "Unable to bookmark..\n" ) ;
        }
    }
    else if( propsMap.find("PolicyType")->second.compare("PrepareTarget") == 0 )
    {
        if(prepareTarget() == SVS_OK)
        {
            retStatus = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to prepare target\n") ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}
void FileServerController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    if(schedulerPtr->isPolicyEnabled(m_policyId))
    {
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    }
    if( configuratorPtr->getApplicationPolicies(policyId, "FILESERVER",m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR FileServerController::Consistency()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::string strCommandLineOptions;
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
            iter = propsMap.find("ConsistencyCmd");
            if(iter != propsMap.end())
            {
                strCommandLineOptions = iter->second;
				bool bExecSuccess = false;
				if(!strCommandLineOptions.empty())
				{
					AppLocalConfigurator configurator;
					TagGenerator obj( strCommandLineOptions, configurator.getConsistencyTagIssueTimeLimit());
					SV_ULONG exitCode = 0x1;

					if( obj.issueConsistenyTag( outputFileName,exitCode ) )
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
					appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","Failed to spawn vacp Process.Please check command .");
					DebugPrintf(SV_LOG_ERROR,"Failed to spawn vacp Process. Vacp command: %s \n",strCommandLineOptions.c_str());
				}
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "there is no option with key as ConsistencyCmd\n") ;
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
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}

SVERROR FileServerController::prepareTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;
    std::string strAppType;
    bool bIsINstanceNamePresent = false;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    schedulerPtr->UpdateTaskStatus(policyId,TASK_STATE_STARTED) ;
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    std::map<std::string, std::string>::iterator iter = propsMap.find("ApplicationType");
    if(iter != propsMap.end())
    {
        strAppType = iter->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "there is no option with key as ApplicationType\n") ;
    }

    iter = propsMap.find("InstanceName");

    if(iter != propsMap.end())
    {
        bIsINstanceNamePresent = true;
    }
    else
    {
        std::string outputFilePathName = getOutputFileName( policyId );
        std::ofstream os;
        os.open( outputFilePathName.c_str(), std::ios::app );
        if( os.is_open() && os.good() )
        {
            os << "Nothing to be done for Preparing the target before activating Protection for File Server";
            os.flush();
            os.close();
        }
        else
        {
            DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", outputFilePathName.c_str() );
        }
        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success","Nothing to be done for Preparing the target before activating Protection for File Server" );
    }
    SV_ULONG exitCode = 0x01;
    SV_UINT strTimeout = 600;
    std::string outputFileName = getOutputFileName(policyId);
    if( m_bCluster )
    {
        stopFSServices(strAppType, m_bCluster);
        std::list<std::string> listClsuterNodes;
        if(getNodesPresentInCluster("",listClsuterNodes) == SVS_OK)
        {
            std::string strListOfPoweredOfNodes;
            std::string strLocalHost = Host::GetInstance().GetHostName();
            std::list<std::string>::iterator listIter = listClsuterNodes.begin();

            while(listIter != listClsuterNodes.end())
            {
                //The Current node is Active Node
                if(strcmpi(strLocalHost.c_str(), listIter->c_str()) != 0)
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


            DebugPrintf(SV_LOG_INFO,"The Target breaking command is %s: ",strCommand.c_str());


            ClusterOp objClusterOp(strCommand,strTimeout);
            iter = propsMap.find("PolicyId");
            if(iter != propsMap.end())
            {
                if(objClusterOp.persistClusterResourceState() == SVS_FALSE)
                {	
                    //still we will continue with breaking of cluster
                    DebugPrintf(SV_LOG_ERROR,"Failed to persist cluster resources.\n");
                }
                if(objClusterOp.breakCluster(outputFileName,exitCode))
                {
                    if(exitCode == 0)
                    {
                        DebugPrintf(SV_LOG_INFO,"\n Waiting for reboot of current host: %s to happen",strLocalHost.c_str());
                        //appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",objClusterOp.stdOut());
                        if(!RebootSystems(strLocalHost.c_str(), true, NULL,NULL,NULL))
                        {
                            DebugPrintf(SV_LOG_ERROR,"\n Attempt to reboot  current host: %s to happen",strLocalHost.c_str());
                        }
                        else
                        {
                            retStatus = SVS_OK ;
                            if( !Controller::getInstance()->QuitRequested(1) )
                            {
                                Controller::getInstance()->Stop();
                            }
                            //This update will never reach CX since by this time ,machine will be rebooted
                            DebugPrintf(SV_LOG_INFO,"\n Rebooted host %s successfully\n",strLocalHost.c_str());
                        }
                    }
                    else
                    {
                        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Failed to spawn clusutil.exe . Please check command"));
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Prepare target policy failed for FILESERVER\n") ;
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Prepare target policy failed for FILESERVER"));
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"\n Failed to fetch the PolicyId from policy details");				
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Failed to list of Nodes present in system.Cannot continue with target Preparation");
        }
    }
    else if(bIsINstanceNamePresent)
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
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","Prepare target policy failed for FILESERVER" );
                }
                else
                {
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success","PrepareTarget Operation completed successfully" );
                }
            }
        }
        else
        {
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success","PrepareTarget Operation completed successfully" );
        }
    }
    schedulerPtr->UpdateTaskStatus(boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second), TASK_STATE_STARTED) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}
SVERROR FileServerController::stopFSServices(const std::string &appName, bool m_bCluster)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;
    std::string svcName;

    if (m_bCluster == false)
    {
        std::list<InmService> listServiceName;
        svcName = "lanmanserver" ;
        InmService serverSvc(svcName) ;
        QuerySvcConfig(serverSvc) ;
        listServiceName.push_back(serverSvc);

        std::list<InmService>::iterator iter = listServiceName.begin();
        while(iter != listServiceName.end())
        {
            if(StpService((*iter).m_serviceName) != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR,"\n Failed to stop service %s",((*iter).m_serviceName).c_str());
                retStatus = SVS_FALSE;
            }
            else
            {
                retStatus = SVS_OK;
                DebugPrintf(SV_LOG_INFO,"\n SuccessFully stopped %s service ",((*iter).m_serviceName).c_str());
            }
            changeServiceType((*iter).m_serviceName,INM_SVCTYPE_MANUAL); //Change service type irrespective of the Service running status
            iter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;

}


void FileServerController::UpdateConfigFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_ProtectionSettingsChanged = false;

    if(m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0)
    {
        if(GetSourceDiscoveryInformation() == SVS_OK)
        {
            CreateFileServerDiscoveryInfoFiles();
			if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
			{
				if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false)
				{
					CreateFileServerDbFiles();
				}
			}
			else if(m_FailoverInfo.m_FailoverType.compare("Failback") == 0)
			{
				if(m_FailoverInfo.m_DRVirtualServerName.empty() == false)
				{
					CreateFileServerDbFiles();
				}
			}
			CreateFileServerRegistryInfoFiles();
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Source discovery information\n");
        }
    }
    CreateFileServerFailoverServicesConfFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void FileServerController::CreateFileServerDiscoveryInfoFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::ofstream out;
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" + 
        DATA + "\\" ;
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
    }
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_fileserver_config.dat";
    out.open(fileName.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to createFile %s\n",fileName.c_str());
        stream<<"Failed to open conf file :"<<fileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(fileName);
        }
    }
    out << "DISCOVERY VERSION"<<"\t"<<"1.2"<<std::endl;
    out << "HOST NAME"<<"\t"<<srcHostName<<std::endl;
    out << "VOLUMES" <<"\t";
    std::list<std::string>::iterator volIter = m_SrcFileServerDiscInfo.m_VolumeList.begin();
    while(volIter != m_SrcFileServerDiscInfo.m_VolumeList.end())
    {
        out << *volIter << "\t";
        volIter++;
    }
    out <<std::endl;
    out.close();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void FileServerController::CreateFileServerDbFiles()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" + FAILOVER_DATA_PATH + "\\" + DATA + "\\";
    std::string srcHostName;
    std::string tgtHostName;
	std::string srcVirtualServerName;
	std::string tgtVirtualServerName;
    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
    {
		srcVirtualServerName = m_FailoverInfo.m_ProductionVirtualServerName; 
		srcHostName = m_FailoverInfo.m_ProductionServerName;
		tgtVirtualServerName= m_FailoverInfo.m_DRVirtualServerName;
		tgtHostName = m_FailoverInfo.m_DRServerName;
	}
    else 
    {
		srcVirtualServerName = m_FailoverInfo.m_DRVirtualServerName;
		srcHostName = m_FailoverInfo.m_DRServerName;
		tgtVirtualServerName = m_FailoverInfo.m_ProductionVirtualServerName;
		tgtHostName = m_FailoverInfo.m_ProductionServerName;
    }
	std::string strDatabaseName = srcHostName + std::string("_") + tgtHostName + std::string("_") + srcVirtualServerName + std::string("_fsInfo.mdb");
	strDatabaseName = fileName + strDatabaseName;
	DebugPrintf(SV_LOG_DEBUG,"Fileserver Database file name %s\n",strDatabaseName.c_str());

	W2k8FileServerDB fsdbw2k8;
	FILE *fptr = fopen(strDatabaseName.c_str(), "w");
	if(NULL == fptr)
	{
		DebugPrintf(SV_LOG_DEBUG,"Fileserver Database file name %s doesn't exist\n",strDatabaseName.c_str());
	}
	fclose(fptr);
	fsdbw2k8.InitFileServerDB(strDatabaseName);

	std::string registryVersionStr;
    GetValueFromMap(m_SrcFileServerDiscInfo.m_Properties,"SrcRegistryVersion",registryVersionStr);
    std::map<std::string,std::string>::iterator shareInfoMapIter = m_SrcFileServerDiscInfo.m_ShareInfoMap.begin();
    std::map<std::string,std::string>::iterator securityInfoMapIter = m_SrcFileServerDiscInfo.m_SecurityInfoMap.begin();
	std::map<std::string,std::string>::iterator shareUserMapIter = m_SrcFileServerDiscInfo.m_ShareUsersMap.begin();
	std::map<std::string,std::string>::iterator shareTypeMapIter = m_SrcFileServerDiscInfo.m_ShareTypeMap.begin();
	std::map<std::string,std::string>::iterator shareAbsPathMapMapIter = m_SrcFileServerDiscInfo.m_AbsolutePathMap.begin();
	std::map<std::string,std::string>::iterator sharePwdMapIter = m_SrcFileServerDiscInfo.m_SharePwdMap.begin();
	std::map<std::string,std::string>::iterator shareRemarkMapIter = m_SrcFileServerDiscInfo.m_ShareRemarkMap.begin();
	std::map<std::string,std::string>::iterator mountPointMapIter = m_SrcFileServerDiscInfo.m_mountPointMap.begin();

    FileShareInfo fsInfo;
    while(shareInfoMapIter != m_SrcFileServerDiscInfo.m_ShareInfoMap.end())
    {

        fsInfo.m_shareName  = shareInfoMapIter->first;
        fsInfo.m_sharesAscii = shareInfoMapIter->second;
        fsInfo.m_securityAscii = securityInfoMapIter->second;
		fsInfo.m_absolutePath = shareAbsPathMapMapIter->second;
		fsInfo.m_shi503_max_uses = strtoul(shareUserMapIter->second.c_str(), NULL, 16);
		if("0" == sharePwdMapIter->second)
		{
			fsInfo.m_shi503_passwd.empty();
		}
		else
		{
			fsInfo.m_shi503_passwd = sharePwdMapIter->second;
		}
		if("0" == shareRemarkMapIter->second)
		{
			fsInfo.m_shi503_remark.empty();
		}
		else
		{
			fsInfo.m_shi503_remark = shareRemarkMapIter->second;
		}
		fsInfo.m_shi503_type = strtoul(shareTypeMapIter->second.c_str(), NULL, 16);
		fsInfo.m_mountPoint = mountPointMapIter->second;
		fsdbw2k8.AddFileServerInfoinDB(fsInfo);
		shareInfoMapIter++;
		securityInfoMapIter++;
		shareUserMapIter++;
		shareTypeMapIter++;
		shareAbsPathMapMapIter++;
		sharePwdMapIter++;
		shareRemarkMapIter++;
		mountPointMapIter++;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void FileServerController::CreateFileServerRegistryInfoFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" + DATA + "\\";
    std::string srcHostName;
    std::string tgtHostName;
    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
    {
        if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false)
        {
            srcHostName = m_FailoverInfo.m_ProductionVirtualServerName; 
        }
        else
        {
            srcHostName = m_FailoverInfo.m_ProductionServerName;
        }
        if(m_FailoverInfo.m_DRVirtualServerName.empty() ==  false)
        {
            tgtHostName = m_FailoverInfo.m_DRVirtualServerName;
        }
        else
        {
            tgtHostName = m_FailoverInfo.m_DRServerName;
        }
    }
    else 
    {
        if(m_FailoverInfo.m_DRVirtualServerName.empty() == false)
        {
            srcHostName = m_FailoverInfo.m_DRVirtualServerName;
        }
        else
        {
            srcHostName = m_FailoverInfo.m_DRServerName;
        }
        if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false)
        {
            tgtHostName = m_FailoverInfo.m_ProductionVirtualServerName; 
        }
        else
        {
            tgtHostName = m_FailoverInfo.m_ProductionServerName;
        }
    }
    fileName += srcHostName + "_Fileserver_" + tgtHostName +".reg";
    DebugPrintf(SV_LOG_DEBUG,"Fileserver registry file name %s\n",fileName.c_str());
    std::string registryVersionStr;
    GetValueFromMap(m_SrcFileServerDiscInfo.m_Properties,"SrcRegistryVersion",registryVersionStr);
    std::map<std::string,std::string>::iterator shareInfoMapIter = m_SrcFileServerDiscInfo.m_ShareInfoMap.begin();
    std::map<std::string,std::string>::iterator securityInfoMapIter = m_SrcFileServerDiscInfo.m_SecurityInfoMap.begin();
    FileServerDiscoveryImpl fileServerDiscoveryImpl;
    FileShareInfo fileShareInfo;
    std::map<std::string, FileShareInfo> regInfoMap;
    while(shareInfoMapIter != m_SrcFileServerDiscInfo.m_ShareInfoMap.end())
    {
        fileShareInfo.m_shareName  = shareInfoMapIter->first;
        fileShareInfo.m_sharesAscii = shareInfoMapIter->second;
        fileShareInfo.m_securityAscii = securityInfoMapIter->second;
        regInfoMap.insert(std::make_pair(shareInfoMapIter->first,fileShareInfo));
        shareInfoMapIter++;
        securityInfoMapIter++;
    }
    if(fileServerDiscoveryImpl.generateRegistryFile(fileName,registryVersionStr,regInfoMap ) != SVS_OK)
    {
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw AppException("Caught Failed to create fileserver registry file");
        }

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FileServerController::CreateFileServerFailoverServicesConfFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AppLocalConfigurator localconfigurator ;

    std::string confFileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" +
        FILESERVER_DATA_PATH + "\\" ;
    if(SVMakeSureDirectoryPathExists(confFileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", confFileName.c_str());
        throw FileNotFoundException(confFileName);
    }

    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" + 
        DATA + "\\";
    if(SVMakeSureDirectoryPathExists(copyFileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", copyFileName.c_str());
        throw FileNotFoundException(copyFileName);
    }

    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);

    confFileName += srcHostName + "_FailoverServices.conf";
    copyFileName +=  srcHostName + "_FailoverServices.conf";
    std::ofstream os;
    os.open(confFileName.c_str(),std::ios::trunc);
    if(os.is_open())
    {
		os << "[" << m_FailoverInfo.m_AppType << "]" << std::endl;
		for(int i=0; i<2; i++)
		{
			os << (i == 0 ? "START = ":"STOP = ");
			std::list<std::string>::iterator iterService = m_FailoverData.services.begin();
			while(iterService != m_FailoverData.services.end())
			{
				os << iterService->c_str() << ",";
				iterService++;
			}
			os << std::endl ;
		}
        if(m_FailoverInfo.m_FailoverType.compare("Failback") == 0)
        {
            os << "[Failback_IP_Address]" << std::endl;
            if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == true)
            {
                os << m_FailoverInfo.m_ProductionServerName << " = "<< m_FailoverInfo.m_ProductionServerIp;
            }
            else
            {
                os << m_FailoverInfo.m_ProductionVirtualServerName << " = "<< m_FailoverInfo.m_ProductionVirtualServerIp;
            }
        }
        os.close();
        if(CopyConfFile(confFileName.c_str(),copyFileName.c_str()) == false)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
            if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
            {
                throw FileNotFoundException(copyFileName);
            }
        }		
    }
    else
    {
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(confFileName);
        }		
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
void FileServerController::BuildFailoverCommand(std::string& failoverCmd)
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
            DebugPrintf(SV_LOG_ERROR,"Caught Exception %s\n",be.what());
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
			if(boost::lexical_cast<bool>(m_FailoverInfo.m_ADFailover) == true)
            {
                cmdStream << " -doadreplication ";
            }
        }
        catch(boost::bad_lexical_cast &be)
        {
            DebugPrintf(SV_LOG_ERROR,"Caught boost lexical_cast Exception %s\n",be.what());
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
	else if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == true && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == false)
	{
		cmdStream << " -virtualserver ";
		cmdStream << m_FailoverInfo.m_DRVirtualServerName;
	}
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Standalone case\n");
    }

    if(m_FailoverInfo.m_ExtraOptions.empty() == false)
    {
        cmdStream << m_FailoverInfo.m_ExtraOptions;
    }
	cmdStream << " -useuseraccount";
    cmdStream << " -config ";
    cmdStream << "\"";
    cmdStream << m_FailoverInfo.m_AppConfFilePath;
    cmdStream << "\"";
    DebugPrintf(SV_LOG_DEBUG,"FailoverCommand is %s\n",cmdStream.str().c_str());
    failoverCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void FileServerController::PrePareFailoverJobs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    FailoverJob failoverJob;
    FailoverCommand failoverCmd;
    if(m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerFastFailback") == 0)
    {
        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_SOURCE_SCRIPT;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 1;
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
				failoverCmd.m_bUseUserToken = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_PolicyType.compare("DRServerFastFailback") == 0)
            {
                std::string command;
                BuildDRServerFastFailbackCmd(command);
                failoverCmd.m_Command = command;
                failoverCmd.m_GroupId = FILESERVER_CHANGE_SPN;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 0;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
				command.clear();
			}
            else
            {
                std::string failoverCmdStr;
                BuildFailoverCommand(failoverCmdStr);
                failoverCmd.m_Command = failoverCmdStr;
                failoverCmd.m_GroupId = ISSUE_CONSISTENCY;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 0;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_SOURCE;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
    }
    else if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0)
    {
        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_TARGET_SCRIPT;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 1;
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
				failoverCmd.m_bUseUserToken = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
			if(m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0)
            {
                std::string command;
                BuildProductionServerFastFailbackCmd(command);
                failoverCmd.m_Command = command;
                failoverCmd.m_GroupId = FILESERVER_DNS_FAILOVER;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 0;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
                command.clear();
            }
			else
			{				
				std::string failoverCmdStr;
				BuildFailoverCommand(failoverCmdStr);
				failoverCmd.m_Command = failoverCmdStr;
				failoverCmd.m_GroupId = ROLLBACK_VOLUMES;
				failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 0;
				failoverJob.m_FailoverCommands.push_back(failoverCmd);
			}
            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_TARGET;
                failoverCmd.m_StepId = 1;
				failoverCmd.m_bUseUserToken = 1;
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
				failoverCmd.m_bUseUserToken = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
            if(m_FailoverInfo.m_startApplicationService.compare("1") == 0)
            {
                failoverCmd.m_GroupId = START_APP_CLUSTER;
                failoverCmd.m_StepId = 1;
				if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
				{
					failoverCmd.m_Command = "Application Agent will online the FILESERVER resources after successful completion of failover";
				}
				else
				{
					failoverCmd.m_Command = "Application Agent will start the lanmanserver service after successful completion of failover";
				}
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
    }
    AddFailoverJob(m_policyId,failoverJob);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void FileServerController::BuildProductionServerFastFailbackCmd(std::string& fastFailbackCmd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "Application.exe";
	cmdStream <<"\"";
	cmdStream << " -app " ;
    cmdStream << m_FailoverInfo.m_AppType;
	cmdStream << " -fastfailback";
	cmdStream <<" -s ";
	cmdStream << m_FailoverInfo.m_ProductionServerName;
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
	}
	catch(boost::bad_lexical_cast &be)
	{
		DebugPrintf(SV_LOG_ERROR,"Caught Exception %s\n",be.what());
	}
	if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == true)
    {
        cmdStream << " -virtualserver ";
        cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
		cmdStream <<  " -ip ";
		cmdStream << m_FailoverInfo.m_ProductionVirtualServerIp;
    }
	else if(!m_FailoverInfo.m_ProductionVirtualServerName.empty() && !m_FailoverInfo.m_DRVirtualServerName.empty())
	{
		cmdStream << " -sourcevs ";
		cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
		cmdStream << " -targetvs ";
		cmdStream << m_FailoverInfo.m_DRVirtualServerName;
		cmdStream <<  " -ip ";
		cmdStream << m_FailoverInfo.m_ProductionVirtualServerIp;
	}
	else
	{
		cmdStream <<  " -ip ";
		cmdStream << m_FailoverInfo.m_ProductionServerIp;
	}
	cmdStream << " -useuseraccount ";
	fastFailbackCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG,"FileServer ProductionServerFastFailback Command: %s\n",fastFailbackCmd.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void FileServerController::BuildDRServerFastFailbackCmd(std::string& fastFailbackCmd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
	cmdStream << "Application.exe";
	cmdStream <<"\"";
	cmdStream << " -app " ;
    cmdStream << m_FailoverInfo.m_AppType;
	cmdStream << " -fastfailback";
	cmdStream <<" -s ";
	cmdStream << m_FailoverInfo.m_ProductionServerName;
	cmdStream <<  " -t ";
	cmdStream << m_FailoverInfo.m_DRServerName;
	if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == true)
    {
        cmdStream << " -virtualserver ";
        cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
    }
	else if(!m_FailoverInfo.m_ProductionVirtualServerName.empty() && !m_FailoverInfo.m_DRVirtualServerName.empty())
	{
		cmdStream << " -sourcevs ";
		cmdStream << m_FailoverInfo.m_ProductionVirtualServerName;
		cmdStream << " -targetvs ";
		cmdStream << m_FailoverInfo.m_DRVirtualServerName;
	}
	try
	{
		if(boost::lexical_cast<bool>(m_FailoverInfo.m_ADFailover) == true)
		{
			cmdStream << " -doadreplication ";
		}
	}
	catch(boost::bad_lexical_cast &be)
	{
		DebugPrintf(SV_LOG_ERROR,"Caught Exception %s\n",be.what());
	}
	cmdStream << " -useuseraccount ";
		
    fastFailbackCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG,"FileServer DRServerFastFailback Command: %s\n",fastFailbackCmd.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void FileServerController::BuildADReplicationCmd(std::string& adReplicationcmd)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
	cmdStream << "Application.exe";
	cmdStream <<"\"";
	cmdStream << " -app " ;
    cmdStream << m_FailoverInfo.m_AppType;
	cmdStream << " -fastfailback";
	cmdStream <<" -s ";
	cmdStream << m_FailoverInfo.m_ProductionServerName;
	cmdStream <<  " -ip ";
	cmdStream << m_FailoverInfo.m_ProductionServerIp;
	cmdStream <<  " -doadreplication ";
	adReplicationcmd = cmdStream.str();
	DebugPrintf(SV_LOG_DEBUG,"FileServer AD replication FastFailback Command: %s\n",adReplicationcmd.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
SVERROR FileServerController::GetSourceDiscoveryInformation()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;

    std::string marshalledSrcDiscInfoStr = m_AppProtectionSettings.srcDiscoveryInformation;
    try
    {
        m_SrcFileServerDiscInfo = unmarshal<SrcFileServerDiscoveryInfomation>(marshalledSrcDiscInfoStr) ;
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
bool FileServerController::IsItActiveNode()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bProcess = false ;
    std::list<std::string> networkNameList;
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    if( isClusterNode() )
    {
		OSVERSIONINFOEX osVersionInfo ;
		getOSVersionInfo(osVersionInfo) ;
		if(osVersionInfo.dwMajorVersion == 6 /*&& ((m_policy.m_policyProperties.find("PolicyType")->second.compare("PrepareTarget") == 0) || (m_policy.m_policyProperties.find("PolicyType")->second.compare("ProductionServerFastFailback") == 0) || (m_policy.m_policyProperties.find("PolicyType")->second.compare("ProductionServerPlannedFailback") == 0))*/) //making this condition true for windows 2008 sp1,sp2 and R2
		{
			std::list<std::string> fileServerResourceNames;
			CLUSTER_RESOURCE_STATE state;
			std::string groupName, ownerNodeName;
			std::string hostName = Host::GetInstance().GetHostName();
			std::map<std::string, std::string>::iterator iInstanceName = propsMap.find("InstanceName");
			if(iInstanceName == propsMap.end())
			{
				DebugPrintf(SV_LOG_ERROR, "InstanceName property not found in the policy properties. Considering this node as passive node.\n");
			}
			else if( getResourcesByType("", "File Server", fileServerResourceNames) == SVS_OK )
			{
				DebugPrintf(SV_LOG_DEBUG, "Searching Instance Name: %s\n", iInstanceName->second.c_str());

				std::list<std::string> networknameKeyList;
				networknameKeyList.push_back("Name");
				
				std::list<std::string> dependentNameList;

				std::map< std::string, std::string> outputPropertyMap;
				
				std::list<std::string>::iterator fileServerResourceNamesIter = fileServerResourceNames.begin();
				while( fileServerResourceNamesIter != fileServerResourceNames.end() )
				{
					DebugPrintf(SV_LOG_DEBUG, "Verifying File Server Resource Name: %s\n", fileServerResourceNamesIter->c_str());

					dependentNameList.clear();
					outputPropertyMap.clear();
					if( getResourceState("",*fileServerResourceNamesIter, state, groupName, ownerNodeName ) == SVS_OK )
					{
						if( strcmpi(hostName.c_str(), ownerNodeName.c_str()) == 0)
						{
							if( getDependedResorceNameListOfType( *fileServerResourceNamesIter, "Network Name", dependentNameList ) == SVS_OK)
							{
								if(dependentNameList.empty())
								{
									DebugPrintf(SV_LOG_WARNING, "Network Name dependency is not configured for the FileServer Resource %s\n",
										fileServerResourceNamesIter->c_str());
								}
								else
								{
									std::string networkName;
									std::string dependentNetworkName = *(dependentNameList.begin());
									getResourcePropertiesMap( "",dependentNetworkName , networknameKeyList, outputPropertyMap);
									if(outputPropertyMap.find("Name") != outputPropertyMap.end())
										networkName = outputPropertyMap.find("Name")->second;

									if(0 == strcmpi(networkName.c_str(),iInstanceName->second.c_str()))
									{
										bProcess = true;
										break;
									}
								}
							}	
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "getResourceState failed. Considering this node as passive node for %s.\n",
							fileServerResourceNamesIter->c_str()) ;
					}
					fileServerResourceNamesIter++;
				}
			}
		}
		else
		{
			FileServerAplicationPtr fileSvrPtr;
			fileSvrPtr.reset( new FileServerAplication() );
			if( fileSvrPtr->isInstalled() )
			{
				fileSvrPtr->discoverApplication();
				fileSvrPtr->getActiveInstanceNameList(networkNameList);
				std::map<std::string, std::string>::iterator iterMap = propsMap.find("InstanceName");
				if(iterMap != propsMap.end())
				{
					if(isfound(networkNameList, iterMap->second))
					{
						bProcess = true;
						DebugPrintf(SV_LOG_INFO,"Network Name  %s  resides on local Host\n",iterMap->second.c_str());
					}
				}	
				else
				{
					DebugPrintf(SV_LOG_INFO,"\n This node is not active node \n");
				}
			}
			else
			{
				DebugPrintf( SV_LOG_ERROR, "\n File Server is Not installed.. \n" );
			}
		}
    }
    else
    {
        bProcess = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bProcess;
}

void FileServerController::MakeAppServicesAutoStart()
{

}

void FileServerController::PreClusterReconstruction()
{

}

bool FileServerController::StartAppServices()
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
    DebugPrintf( SV_LOG_DEBUG, "Starting FileServer services\n");
    if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
    {
        ClusterOp clusOpObj;
        std::list<std::string> resourceList;
		resourceList.push_back("Network Name");

		std::string resourceType;
		OSVERSIONINFOEX osVersionInfo ;
		getOSVersionInfo(osVersionInfo) ;
		if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
		{			
			resourceType = "File Server";
		}
		else if(osVersionInfo.dwMajorVersion < 6) // win2k3 cluster
		{
			resourceType = "File Share";
		}
		
        std::list<std::string>::iterator resourceListIter = resourceList.begin();
        while(resourceListIter != resourceList.end())
        {
            if(clusOpObj.OfflineorOnlineResource(*resourceListIter,"FILESERVER",m_FailoverInfo.m_ProductionVirtualServerName,true) == true)
            {
                DebugPrintf(SV_LOG_DEBUG,"Successfully brought resouce online\n");
                stream<<"Successfully brought " ;
                stream << *resourceListIter;
                stream << " online";
                stream << std::endl;
				bResult = true;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Failed to bring resouce online\n");
				stream << "Failed to bring resouce online\n";
				bResult = false;
            }
            resourceListIter++;
        }

		if(!resourceType.empty())
		{
			std::string errLog;
			std::list<std::string> FileServerResourceTypeList;
			if( getResourcesByType("", resourceType.c_str(), FileServerResourceTypeList) == SVS_OK )
			{
				std::list<std::string> lstDpdntResources;
				std::list<std::string>::iterator listIter =FileServerResourceTypeList.begin();
				while(listIter != FileServerResourceTypeList.end())
				{
					bool bNwNameMatched = false;
					lstDpdntResources.clear();
					if(getDependedResorceNameListOfType(*listIter,"Network Name",lstDpdntResources) == SVS_OK)
					{
						if(!lstDpdntResources.empty())
						{
							if(strcmpi(lstDpdntResources.begin()->c_str(),m_FailoverInfo.m_ProductionVirtualServerName.c_str()) == 0)
								bNwNameMatched = true;
						}
						else
						{
							stream	<< "Skipping resource online for: " << listIter->c_str() 
									<< ". No Network Name dependency found for this resource." << std::endl;
						}
					}
					else
					{
						stream	<< "Skipping resource online for: " << listIter->c_str() 
								<< ". Failed to get Network Name for this resource." << std::endl
								<< "Online this resoruce manually" << std::endl;
					}
					
					if(bNwNameMatched)
					{
						if(clusOpObj.onlineClusterResources(*listIter,errLog) == SVS_OK)
						{
							stream << "Successfully brought the Cluster Resource " ;
							stream << (*listIter);
							stream << " online \n";
							bResult = true;
						}
						else
						{
							stream << errLog;
						}
					}
					listIter++;
				}
			}
		}
    }
    else
    {
        if( StrService("lanmanserver") != SVS_OK )
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to start lanmanserver service \n");
            stream<<"Failed to start lanmanserver service"<<std::endl;
			bResult = false;
        }
        else
        {
            stream<<"Successfully started lanmanserver service"<<std::endl;
			bResult = true;
        }
    }
    WriteStringIntoFile(outputFilePathName,stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bResult;
}

void W2k8FileServerDB::AddFileServerInfoinDB(FileShareInfo& update)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream insert_query ;
	bool qfailed = false;
	
	insert_query << "INSERT INTO t_FileServerInfo " ;
	insert_query << "( c_NetName ,c_Type,c_Remark,c_MaximumAllowedUsers,c_SharePath,c_PassWord,c_SecurityDescriptor,c_VolumeName) " ;
	insert_query << " VALUES ( " << "'"<<update.m_shareName<<"'" << " , " ;
	insert_query << update.m_shi503_type << ", " ;
	insert_query << "'"<<update.m_shi503_remark<<"'"<<" , ";
	insert_query << update.m_shi503_max_uses <<" , ";
	insert_query << "'"<< update.m_absolutePath<<"'"<<" , ";
	insert_query << "'"<<update.m_shi503_passwd<<"'"<<" , ";
	insert_query << "'"<<update.m_securityAscii<<"'"<<" , ";
	insert_query <<"'"<<update.m_mountPoint<< "'"<<" ) ; " ;
	try
	{
		m_con.execQuery( insert_query.str(), set ) ;
	}
	catch(std::string strException)
	{
		qfailed = true;
		DebugPrintf(SV_LOG_ERROR, "Query failed. strException: %s\n", strException.c_str());
	}
	catch(std::exception ex)
	{
		qfailed = true;
		DebugPrintf(SV_LOG_ERROR, "Query failed. stdException: %s\n", ex.what());
	}
	if (qfailed)
	{
		std::stringstream insert_queryToLog ; // without password; passwords shouldn't be logged
	    insert_queryToLog << "INSERT INTO t_FileServerInfo " ;
	    insert_queryToLog << "( c_NetName ,c_Type,c_Remark,c_MaximumAllowedUsers,c_SharePath,c_PassWord,c_SecurityDescriptor,c_VolumeName) " ;
	    insert_queryToLog << " VALUES ( " << "'"<<update.m_shareName<<"'" << " , " ;
	    insert_queryToLog << update.m_shi503_type << ", " ;
	    insert_queryToLog << "'"<<update.m_shi503_remark<<"'"<<" , ";
	    insert_queryToLog << update.m_shi503_max_uses <<" , ";
	    insert_queryToLog << "'"<< update.m_absolutePath<<"'"<<" , ";
	    insert_queryToLog << "'"<<update.m_securityAscii<<"'"<<" , ";
	    insert_queryToLog <<"'"<<update.m_mountPoint<< "'"<<" ) ; " ;
        DebugPrintf(SV_LOG_ERROR, "Failed Query: %s\n", insert_queryToLog.str().c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void W2k8FileServerDB::InitFileServerDB(const std::string& sDbpath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_con.setDbPath(sDbpath) ;
	m_con.open() ;
	CreateFileServerInfoTable();
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void W2k8FileServerDB::CreateFileServerInfoTable()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;

	select_query << "SELECT name FROM sqlite_master WHERE type=\'table\' AND name=\'t_FileServerInfo\' ;" ;
	
	create_query << "CREATE TABLE t_FileServerInfo " ;
    create_query << "(\"c_SequenceNO\" INTEGER PRIMARY KEY AUTOINCREMENT , " ;
	create_query << "\"c_NetName\" TEXT NOT NULL , " ;
	create_query << "\"c_Type\" INTEGER NOT NULL, "; 
	create_query << "\"c_Remark\" TEXT NOT NULL , ";
    create_query << "\"c_MaximumAllowedUsers\" INTEGER NOT NULL , "; 
	create_query << "\"c_SharePath\" TEXT NOT NULL , ";
	create_query << "\"c_PassWord\" TEXT NOT NULL , ";
	create_query << "\"c_SecurityDescriptor\" TEXT NOT NULL , ";
	create_query << "\"c_VolumeName\" TEXT NOT NULL);";
	
	m_con.execQuery(select_query.str(), set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str(), set) ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX masterIdx1 ON t_FileServerInfo ( c_SequenceNO );" ;
		m_con.execQuery(indexing_query.str(), set ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}