#include <boost/algorithm/string.hpp>
#include "sqldiscovery.h"
#include "mssqlcontroller.h"
#include "controller.h"
#include "Consistency\TagGenerator.h"
#include "clusterutil.h"
#include "service.h"
#include "util/common/util.h"
#include "ClusterOperation.h"
#include "host.h"
#include "mssqlapplication.h"
#include <boost/lexical_cast.hpp>
#include "system.h"
#include <sstream>
#include "config\appconfigvalueobj.h"
#include "systemvalidators.h"
#include "registry.h"
#define RESOURCE_SQL_SERVER "SQL Server"
MSSQLControllerPtr MSSQLController::m_SQLinstancePtr;

MSSQLControllerPtr MSSQLController::getInstance(ACE_Thread_Manager* tm)
{
    if( m_SQLinstancePtr.get() == NULL )
    {
        m_SQLinstancePtr.reset(new  MSSQLController(tm) ) ;
		if( m_SQLinstancePtr.get() != NULL )
			m_SQLinstancePtr->Init();
    }
    return m_SQLinstancePtr ;
}
int MSSQLController::open(void *args)
{
    return this->activate(THR_BOUND);
}

int MSSQLController::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

int MSSQLController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return  0 ;
}

MSSQLController::MSSQLController(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
    //TODO:: controller specific code
}

SVERROR  MSSQLController::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    bool process = false ;
    m_bCluster = isClusterNode();
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
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

void MSSQLController::UpdateConfigFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_ProtectionSettingsChanged = false;
    if(m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0)
    {
        GetSourceDiscoveryInformation();
        CreateSQLDiscoveryInfoFiles();
    }
    CreateSQLFailoverServicesConfFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

void MSSQLController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    if(schedulerPtr->isPolicyEnabled(m_policyId))
    {
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    }
    if( configuratorPtr->getApplicationPolicies(policyId, "SQL", m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR MSSQLController::Consistency()
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
            if(iter != propsMap.end())
            {
                strCommandLineOptions = iter->second;
                /*if( strCommandLineOptions.find("-a SQL2005") == std::string::npos )
                {
                    strCommandLineOptions += " -a SQL2005" ;
                }*/
                AppLocalConfigurator configurator;
            	TagGenerator obj(strCommandLineOptions,configurator.getConsistencyTagIssueTimeLimit());
            	SV_ULONG exitCode = 0x1;

            	if(obj.issueConsistenyTag(outputFileName,exitCode))
            	{
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
            	else
            	{
                	appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Failed to spawn vacp Process.Please check command ."));
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
    }
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}

SVERROR MSSQLController::prepareTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::string strAppType;
    bool bIsInstanceNamePresent = false;
    std::string strInstanceName;
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
        strInstanceName = iter->second;
        bIsInstanceNamePresent = true;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"There is no option with key as InstanceName\n");
        DebugPrintf(SV_LOG_INFO,"\n Going to list all instances present on the current host");
        std::list<std::string> instanceNameList;

        std::string strLocalHost = Host::GetInstance().GetHostName();
        if(getSQLInstanceList(instanceNameList) == SVS_OK)
        {
            DebugPrintf(SV_LOG_DEBUG,"Successfully got instancelist\n");
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Failed get instancelist\n");
        }
        std::list<std::string>::iterator listIter = instanceNameList.begin();
        std::stringstream msg;
        std::list<std::string> succeedInstanceNameList, failedInstanceNameList;

        while(listIter != instanceNameList.end())
        {
            //Not checking return value as stopping service might fail but we still need to continue.
            if(stopSQLServices(strAppType,*listIter,m_bCluster) != SVS_OK)
            {
                failedInstanceNameList.push_back(*listIter);
                result = SVS_FALSE;
            }
            else
            {
                succeedInstanceNameList.push_back(*listIter);
                result = SVS_OK;
            }
            listIter ++;
        }
        if( succeedInstanceNameList.empty() == false )
        {
            msg << std::endl << "Successfully stopped services for the instances: " << std::endl;
            std::list<std::string>::iterator succeedInstanceNameListIter = succeedInstanceNameList.begin();
            while( succeedInstanceNameListIter != succeedInstanceNameList.end() )
            {
                msg << *succeedInstanceNameListIter;
                succeedInstanceNameListIter++;
                if( succeedInstanceNameListIter != succeedInstanceNameList.end() )
                {
                    msg << " &" << std::endl;
                }
            }
        }
        if( failedInstanceNameList.empty() == false )
        {
            msg << std::endl << "Failed to stop services for the instances: " << std::endl;
            std::list<std::string>::iterator failedInstanceNameListIter = failedInstanceNameList.begin();
            while( failedInstanceNameListIter != failedInstanceNameList.end() )
            {
                msg << *failedInstanceNameListIter;
                failedInstanceNameListIter++;
                if( failedInstanceNameListIter != failedInstanceNameList.end() )
                {
                    msg << " &" << std::endl;
                }
            }
        }

        std::string outputFilePathName = getOutputFileName( policyId );
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

        if(result == SVS_OK)
        {
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",msg.str());
        }
        else
        {
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",msg.str());
        }
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;

    }
    std::string outputFileName = getOutputFileName(policyId);			
    SV_ULONG exitCode = 0x01;
    SV_UINT strTimeout = 600;
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
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","Failed to persist the cluster Resources Information");
                    DebugPrintf(SV_LOG_ERROR,"Failed to persist cluster resources.\n");
                }
                else
                {
                    if(objClusterOp.breakCluster(outputFileName,exitCode))
                    {
                        if(exitCode == 0)
                        {

                            DebugPrintf(SV_LOG_INFO,"\n Waiting for reboot of current host: %s to happen",strLocalHost.c_str());
                            if(!RebootSystems(strLocalHost.c_str(), true, NULL,NULL,NULL))
                            {
                                DebugPrintf(SV_LOG_ERROR,"\n Attempt to reboot  current host: %s to happen",strLocalHost.c_str());
                                appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",objClusterOp.stdOut());
                            }
                            else
                            {
                                result = SVS_OK ;
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
                        DebugPrintf(SV_LOG_ERROR, "Prepare target policy failed for MSSQL\n") ;
                        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","Prepare target policy failed for MSSQL" );
                    }
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
    else if(bIsInstanceNamePresent)
    {
        OSVERSIONINFOEX osVersionInfo ;
        getOSVersionInfo(osVersionInfo) ;
        if(osVersionInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
        {
            ClusterOp objClusterOp;
            if(objClusterOp.BringW2k8ClusterDiskOnline(outputFileName,exitCode))
            {
                if(exitCode != 0)
                {
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed","Prepare target policy failed for MSSQL" );
                }
                else
                {
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success","PrepareTarget Operation completed successfully" );
                }
            }
        }
        else
        {
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success","");
        }
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}
SVERROR MSSQLController::stopSQLServices(const std::string &appName,const std::string &strInstanceName,bool m_bCluster)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    std::string svcName;

    if (m_bCluster == false)
    {
        std::list<InmService> listServiceName;

        if( strInstanceName.compare("MSSQLSERVER") != 0 )
        {
            svcName = "SQLAgent$" ;
            svcName += strInstanceName ;
        }
        else
        {
            svcName = "SQLSERVERAGENT" ;
        }

        InmService agentSvc(svcName) ;
        QuerySvcConfig(agentSvc) ;
        listServiceName.push_back(agentSvc);

        if(strInstanceName.compare("MSSQLSERVER") != 0 )
        {
            svcName = "MSSQL$" ;
            svcName += strInstanceName ;
        }
        else
        {
            svcName = "MSSQLSERVER" ;
        }
        InmService serverSvc(svcName) ;
        QuerySvcConfig(serverSvc) ;
        listServiceName.push_back(serverSvc);



        std::list<InmService>::iterator iter = listServiceName.begin();
        while(iter != listServiceName.end())
        {
            if(StpService((*iter).m_serviceName) != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR,"\n Failed to stop service %s",((*iter).m_serviceName).c_str());
                result = SVS_FALSE;
            }
            else
            {
                result = SVS_OK;
                DebugPrintf(SV_LOG_INFO,"\n SuccessFully stopped %s service ",((*iter).m_serviceName).c_str());
            }
            changeServiceType((*iter).m_serviceName, INM_SVCTYPE_MANUAL); //Change service type irrespective of the Service running status
            iter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;

}
void MSSQLController::MakeAppServicesAutoStart()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    MSSQLApplicationPtr sqlAppPtr ;
    std::list<std::string> instanceNameList;
    MSSQLApplicationPtr m_SqlApp ;
    if(m_FailoverInfo.m_InstanceName.empty())
    {
		if(m_FailoverInfo.m_AppType.compare("SQL2012") == 0)
        {
            m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2012) );
        }
        else if(m_FailoverInfo.m_AppType.compare("SQL2008") == 0)
        {
            m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2008) );
        }
        else if(m_FailoverInfo.m_AppType.compare("SQL2005") == 0)
        {
            m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2005) );
        }
        else if(m_FailoverInfo.m_AppType.compare("SQL") == 0)
        {
            m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2000));
        }
        if(m_SqlApp.get() != NULL && m_SqlApp->isInstalled())
        {
            m_SqlApp->discoverApplication();
            m_SqlApp->getInstanceNameList(instanceNameList);
        }
    }
    else
    {
        instanceNameList = tokenizeString(m_FailoverInfo.m_InstanceName,",") ;
    }
    std::list<std::string>::iterator serviceIter = instanceNameList.begin() ;
    std::string svcName1, svcName2 ;
    while( serviceIter != instanceNameList.end() )
    {
        if( serviceIter->compare("MSSQLSERVER") != 0 )
        {
            svcName1 = "SQLAgent$" ;
            svcName1 += *serviceIter ;
            svcName2 = "MSSQL$" ;
            svcName2 += *serviceIter;
        }
        else
        {
            svcName1= "SQLSERVERAGENT" ;
            svcName2 = "MSSQLSERVER" ;
        }
        changeServiceType(svcName1, INM_SVCTYPE_AUTO) ;
        changeServiceType(svcName2, INM_SVCTYPE_AUTO) ;
        serviceIter++ ;
    }  
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool MSSQLController::StartAppServices()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bResult = true;
    std::stringstream stream;
    std::string outputFilePathName = getOutputFileName( m_policyId,START_APP_CLUSTER);
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
    if(!m_FailoverInfo.m_ProductionVirtualServerName.empty())
    {
        ClusterOp clusOpObj;
        std::list<std::string> resourceList;

        resourceList.push_back("SQL Server");
        resourceList.push_back("SQL Server Agent");
        std::list<std::string>::iterator resourceListIter = resourceList.begin();
        while(resourceListIter != resourceList.end())
        {
            if(clusOpObj.OfflineorOnlineResource(*resourceListIter,"sql",m_FailoverInfo.m_ProductionVirtualServerName,true) == true)
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
				stream << "Failed to bring resource online";
				bResult = false;
            }
            resourceListIter++;
        }

    }
    else
    {
        MSSQLApplicationPtr sqlAppPtr ;
        std::list<std::string> instanceNameList;
        MSSQLApplicationPtr m_SqlApp ;
        if(m_FailoverInfo.m_InstanceName.empty())
        {
			if(m_FailoverInfo.m_AppType.compare("SQL2012") == 0)
            {
                m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2012) );
            }
            else if(m_FailoverInfo.m_AppType.compare("SQL2008") == 0)
            {
                m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2008) );
            }
            else if(m_FailoverInfo.m_AppType.compare("SQL2005") == 0)
            {
                m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2005) );
            }
            else if(m_FailoverInfo.m_AppType.compare("SQL") == 0)
            {
                m_SqlApp.reset( new MSSQLApplication(INM_APP_MSSQL2000));
            }
            if(m_SqlApp.get() != NULL && m_SqlApp->isInstalled())
            {
                m_SqlApp->discoverApplication();
                m_SqlApp->getInstanceNameList(instanceNameList);
            }
        }
        else
        {
            instanceNameList.push_back(m_FailoverInfo.m_InstanceName) ;
        }
        std::list<std::string>::iterator serviceIter = instanceNameList.begin() ;
        std::string svcName1, svcName2 ;
        while( serviceIter != instanceNameList.end() )
        {
            if( serviceIter->compare("MSSQLSERVER") != 0 )
            {
                svcName1 = "SQLAgent$" ;
                svcName1 += *serviceIter ;
                svcName2 = "MSSQL$" ;
                svcName2 += *serviceIter;
            }
            else
            {
                svcName1= "SQLSERVERAGENT" ;
                svcName2 = "MSSQLSERVER" ;
            }
            if(StrService(svcName1) != SVS_OK)
            {
                stream<<"Failed to start "<<svcName1<<" service"<<std::endl;
                DebugPrintf(SV_LOG_DEBUG, "Failed to start %s service \n",svcName1.c_str());
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<svcName1<<" service"<<std::endl;
            }
            if(StrService(svcName2) != SVS_OK)
            {
                stream<<"Failed to start "<<svcName2<<" service"<<std::endl;
                DebugPrintf(SV_LOG_DEBUG, "Failed to start %s service \n",svcName2.c_str());
				bResult = false;
            }
            else
            {
                stream<<"Successfully started "<<svcName2<<" service"<<std::endl;
            }
            serviceIter++ ;
        }    
    }
    WriteStringIntoFile(outputFilePathName,stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bResult;
}

void MSSQLController::BuildFailoverCommand(std::string& failoverCmd)
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
        policyType.compare("DRServerPlannedFailback") == 0 ||
        policyType.compare("ProductionServerPlannedFailback") == 0)
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
        policyType.compare("DRServerPlannedFailback") == 0 ||
        policyType.compare("ProductionServerPlannedFailback") == 0)
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
            if(boost::lexical_cast<bool>(m_FailoverInfo.m_DNSFailover) == false)
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
    cmdStream << "\"";
    cmdStream << m_FailoverInfo.m_AppConfFilePath;
    cmdStream << "\"";
    DebugPrintf(SV_LOG_DEBUG,"FailoverCommand is %s\n",cmdStream.str().c_str());
    failoverCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
void MSSQLController::PrePareFailoverJobs()
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
                BuildDNSCommand(command);
                failoverCmd.m_Command = command;
                failoverCmd.m_GroupId = FAST_FAILBACK_DNS_FAILOVER;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
                command.clear();
                if( m_FailoverInfo.m_startApplicationService.compare("1") == 0)
                {
                    failoverCmd.m_GroupId = START_APP_CLUSTER;
                    failoverCmd.m_StepId = 1;
                    failoverCmd.m_Command = "Application Agent will start the sevices after successful completion of failover";
                    failoverJob.m_FailoverCommands.push_back(failoverCmd);
                }
                if(m_FailoverInfo.m_ADFailover.compare("1") == 0)
                {
                    BuildWinOpCommand(command,false);
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
					failoverCmd.m_Command = "Application Agent will online the sql server and sql server agent of the failover instance after successful completion of failover";
				}
				else
				{
					failoverCmd.m_Command = "Application Agent will start MSSQL and Sql Server agent agent of the failoover instance after successful completion of failover";
				}
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
            }
        }
    }
    AddFailoverJob(m_policyId,failoverJob);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR MSSQLController::GetSourceDiscoveryInformation()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;
    std::stringstream cmdStream;

    std::string marshalledSrcDiscInfoStr = m_AppProtectionSettings.srcDiscoveryInformation;
    try
    {
        m_SrcDiscInfo = unmarshal<SourceSQLDiscoveryInformation>(marshalledSrcDiscInfoStr) ;
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

void MSSQLController::CreateSQLInstanceDiscoveryInfofile(const std::string& instanceName,SourceSQLInstanceInfo& srcDiscInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::ofstream out;
    AppLocalConfigurator localconfigurator ;
    std::string fileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" +
        MSSQL_DATA_PATH + "\\" ;
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
    }
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" + DATA + "\\";

    if(SVMakeSureDirectoryPathExists(copyFileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", copyFileName.c_str());
    }

    fileName += instanceName + "_sql_config.dat";
    copyFileName +=  instanceName + "_sql_config.dat";
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
    out << "SQL SERVER NAME"<<"\t";
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    out<<srcHostName<<std::endl;
    if(srcDiscInfo.properties.find("Version") != srcDiscInfo.properties.end())
    {
        out <<"VERSION" << "\t"<<srcDiscInfo.properties.find("Version")->second<<std::endl;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"No Verion Key in SourceSQLInstanceInfo properties map\n");
    }
    out << "VOLUMES" <<"\t";
    std::list<std::string>::iterator volIter = srcDiscInfo.m_volumes.begin();
    while(volIter != srcDiscInfo.m_volumes.end())
    {
        out << *volIter;
        volIter++;
        if(volIter != srcDiscInfo.m_volumes.end())
        {
            out <<"\t";
        }
    }
    out <<std::endl;
    std::map<std::string, SourceSQLDatabaseInfo>::iterator dbMapIter = srcDiscInfo.m_databases.begin() ; 
    while(dbMapIter != srcDiscInfo.m_databases.end())
    {

        out << "DATABASE" << "\t" << dbMapIter->first <<"\t";
        bool isTempDb = false ;
        if( dbMapIter->first.compare("tempdb") == 0 )
        {
            isTempDb = true ;
        }
        std::list<std::string>::iterator dblistIter = dbMapIter->second.m_files.begin();
        while(dblistIter != dbMapIter->second.m_files.end())
        {
            if( isTempDb )
            {
                std::string dirName = dblistIter->substr(0, dblistIter->find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR)) ;
                DebugPrintf(SV_LOG_DEBUG, "tempdb file name %s and its directory name %s\n", dblistIter->c_str(), dirName.c_str()) ;
                SVMakeSureDirectoryPathExists(dirName.c_str()) ;
            }
            out << *dblistIter;
            dblistIter++;
            if(dblistIter != dbMapIter->second.m_files.end())
            {
                out<< "\t";
            }
        }
        out<<std::endl;
        dbMapIter++;
    }
    out.close();
    if(!CopyConfFile(fileName.c_str(),copyFileName.c_str()))
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s Error:[%d]\n",copyFileName.c_str(),GetLastError());
        stream<<"Failed to copy app conf file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSSQLController::CreateSQLFailoverServicesConfFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::ofstream out;
    std::string PrefixName;
    AppLocalConfigurator localconfigurator ;
    std::map<std::string,SourceSQLInstanceInfo>::iterator discIter = m_SrcDiscInfo.m_srcsqlInstanceInfo.begin();
    std::string fileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" +
        MSSQL_DATA_PATH + "\\" ;
    if(SVMakeSureDirectoryPathExists(fileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", fileName.c_str());
        throw FileNotFoundException(fileName);
    }
    std::string copyFileName = localconfigurator.getInstallPath()+ "\\" +
        FAILOVER_DATA_PATH + "\\" + DATA + "\\";
    if(SVMakeSureDirectoryPathExists(copyFileName.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to create path directory %s.\n", copyFileName.c_str());
        throw FileNotFoundException(copyFileName);
    }

    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    fileName += srcHostName + "_FailoverServices.conf";
    copyFileName +=  srcHostName + "_FailoverServices.conf";
    PrefixName = srcHostName;
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
    out<<"["<< srcHostName<<"]"<<std::endl;
    out << "MSSQL_INSTANCE_NAMES = ";
    while( discIter != m_SrcDiscInfo.m_srcsqlInstanceInfo.end())
    {
        if(discIter->first.compare("MSSQLSERVER") == 0)
        {
            out<<PrefixName<<";";
        }
        else if(PrefixName.compare(discIter->first) == 0)
        {
            out<<discIter->first << ";";
        }
        else
        {
            out<<PrefixName<<"\\"<<discIter->first << ";";
        }
        discIter++;
    }
    out<<std::endl;
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
        stream<<"Failed to copy app conf file :"<<copyFileName;
        if(m_FailoverInfo.m_ProtecionDirection.compare("forward") != 0)
        {
            throw FileNotFoundException(copyFileName);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void MSSQLController::CreateSQLDiscoveryInfoFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string PrefixName;
    getSourceHostNameForFileCreation(PrefixName);
    std::string instanceName;
    std::map<std::string,SourceSQLInstanceInfo>::iterator discIter = m_SrcDiscInfo.m_srcsqlInstanceInfo.begin();
    while( discIter != m_SrcDiscInfo.m_srcsqlInstanceInfo.end())
    {
        if(discIter->first.compare("MSSQLSERVER") == 0)
        {
            instanceName = PrefixName;
        }
        else if(PrefixName.compare(discIter->first) == 0)
        {
            instanceName = discIter->first ;
        }
        else
        {
            instanceName = PrefixName + "_"+ discIter->first ;
        }
        CreateSQLInstanceDiscoveryInfofile(instanceName,discIter->second);
        discIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
bool MSSQLController::IsItActiveNode()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bIsActiveNode = false;
    std::string strAciveNodeName;
    std::list<std::string> instanceNameList;
    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;

    std::string strLocalHost = Host::GetInstance().GetHostName();
    MSSQLApplicationPtr m_SqlApp ;
    m_SqlApp.reset( new MSSQLApplication() );
    if(m_SqlApp->isInstalled())
    {
        m_SqlApp->discoverApplication();
        m_SqlApp->getActiveInstanceNameList(instanceNameList);
        std::map<std::string, std::string>::iterator iterMap = propsMap.find("InstanceName");
        if(iterMap != propsMap.end())
        {
            if(isfound(instanceNameList,iterMap->second))
            {
                bIsActiveNode = true;
                DebugPrintf(SV_LOG_INFO,"Instance Name  %s  resides on local Host :%s\n",iterMap->second.c_str(),strLocalHost.c_str());
            }
        }	
        else
        {
            DebugPrintf(SV_LOG_INFO,"\n This node is not active node %s\n",strLocalHost.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"\n SQL is not installed on this node. \n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bIsActiveNode;
}

void MSSQLController::PreClusterReconstruction()
{

}
SVERROR MSSQLController::getSQLInstanceList(std::list<std::string>& sqlInstancesList)
{   
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR retValue = SVS_FALSE;
    PrePareTargetData prepareTargetData;
    std::string marshalledPreparedTargetDataStr = m_policy.m_policyData ;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    try
    {
        prepareTargetData = unmarshal<PrePareTargetData>(marshalledPreparedTargetDataStr ) ;
        sqlInstancesList = prepareTargetData.m_InstancesList;
        std::list<std::string>::iterator iter = sqlInstancesList.begin();
        while(iter != sqlInstancesList.end())
        {
            DebugPrintf(SV_LOG_DEBUG,"Got %s instance from cx \n",iter->c_str());
            iter++;
        }
        retValue = SVS_OK;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling %s\n", ex.what()) ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling\n") ;
        return SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retValue;
}