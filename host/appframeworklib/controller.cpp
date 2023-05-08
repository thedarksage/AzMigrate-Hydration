#include "appglobals.h"
#include "controller.h"
#include "config/appconfigurator.h"
#include "cdputil.h"
#include "app/appfactory.h"
#include "appexception.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "task.h"
#include "taskexecution.h"
#include "util/common/util.h"
#include "appcommand.h"
#include "utilmajor.h"
#include "ServiceHelper.h"
#ifdef SV_WINDOWS
#include "util/win32/system.h"	
#endif
#include "serializeapplicationsettings.h"
#include "util/exportdevice.h"
#include "storagerepository/storagerepository.h"
#include "version.h"

ControllerPtr Controller::m_instancePtr ;

SVERROR Controller::init(const std::set<std::string>& selectedApplicationNameSet, bool impersonate)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        m_shouldProcessRequests = true ;
#ifdef SV_WINDOWS
        HANDLE h = NULL ;
		if( impersonate )
		{
			readAndChangeServiceLogOn(h) ;
			setUserHandle(h) ;
			revertToOldLogon();
		}
#endif
        m_bActive = true ;
        m_selectedApplicationNameSet = selectedApplicationNameSet;
        bRet = SVS_OK ;
    }
    catch(AppException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Controller Initialization Failed %s\n", ex.to_string().c_str()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

#ifdef SV_WINDOWS
SVERROR Controller::resetUserHandle()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR svRet = SVS_OK;
	AutoGuard gaurd(m_userTokenLock) ;
	if(m_userToken_)
		CloseHandle(m_userToken_);

	m_userToken_ = NULL;
	if(readAndChangeServiceLogOn(m_userToken_) == SVS_OK)
		RevertToSelf();//Revert thread to its original token

	if(NULL == m_userToken_)
		svRet = SVS_FALSE;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return svRet;
}

HANDLE Controller::getDuplicateUserHandle()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AutoGuard gaurd(m_userTokenLock) ;
	HANDLE hDup = NULL;
	if(m_userToken_ != NULL)
	{
		if(!DuplicateTokenEx(m_userToken_,0,NULL,SecurityImpersonation,TokenPrimary,&hDup))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get the duplicate user handle. Error %d\n",GetLastError());
			hDup = NULL;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return hDup;
}
#endif

SVERROR Controller::checkMigrationRollbackPending()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    std::string errMsg;

    try
    {
        do
        {
            LocalConfigurator lc;
            if (lc.getMigrationState() != Migration::MIGRATION_ROLLBACK_PENDING)
            {
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return SVS_OK;
            }

            status = ServiceHelper::UpdateCSTypeAndRestartSVAgent(std::string("CSLegacy"), errMsg);
            if (status != SVS_OK)
            {
                break;
            }

            PostMsg(RESUME_REPLICATION_POLICY_INTERNAL);
        } while(false);
    }
    catch (ContextualException &ce)
    {
        errMsg = ce.what();
        status = SVE_ABORT;
    }
    catch(...)
    {
        errMsg = "unknown error";
        status = SVE_ABORT;
    }

    //if msg is posted, final migration state will be set in cloudcontroller
    LocalConfigurator lc;
    if (status == SVS_OK)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s : Migration Rollback in progress.\n", FUNCTION_NAME);
        lc.setMigrationState(Migration::MIGRATION_ROLLBACK_IN_PROGRESS);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s : Migration Rollback failed with errors : %s.\n",
            FUNCTION_NAME, errMsg.c_str());
        lc.setMigrationState(Migration::MIGRATION_ROLLBACK_FAILED);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return status;
}

SVERROR Controller::run()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
    try
    {
        AppLocalConfigurator localconfigurator ;
		int regCallInterval = 0;
		bool bRegisterAppAgent = localconfigurator.getRegisterAppAgent();
		AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
		ACE_Time_Value lastRegisterCallTime = ACE_OS::gettimeofday();

        while( !QuitRequested(1) )
        {
			if(bRegisterAppAgent)
			{
				ACE_Time_Value currentTime = ACE_OS::gettimeofday();
				if(currentTime.sec() - lastRegisterCallTime.sec() >= regCallInterval)
				{
					configuratorPtr->registerApplicationAgent();
					lastRegisterCallTime = currentTime;
					regCallInterval = localconfigurator.getRegisterHostInterval();
				}
			}
            checkMigrationRollbackPending();
            if( !m_shouldProcessRequests.value() )
            {
                DebugPrintf(SV_LOG_DEBUG, "Cannot process any requests.. Failover may be in progress\n") ;
                QuitRequested(10) ;
                continue ;
            }
            ACE_Message_Block *mb = NULL;
            ACE_Time_Value waitTime = ACE_OS::gettimeofday ();
            int waitSeconds = 5; //Wait Max 5 seconds while dequeuing message block.
            waitTime.sec (waitTime.sec () + waitSeconds);
            if (-1 == m_MsgQueue.dequeue_head(mb, &waitTime)) 
            {
                if (errno == EWOULDBLOCK || errno == ESHUTDOWN) 
                {
                    continue;
                }
                break;
            }
            SV_ULONG policyId = mb->msg_type() ;
            try
            {
                ProcessRequest(policyId) ;
            }
            catch(AppException& ex)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to process %ld %s\n", policyId, ex.to_string().c_str()); 
            }
            catch(std::exception& ex1)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to process %ld %s\n", policyId, ex1.what()); 
            }
            mb->release() ;
        }
    }
    catch(AppException &ex) //TODO::Proper Exception handling
    {
        DebugPrintf(SV_LOG_ERROR, "Controller failure %s\n", ex.to_string().c_str()) ;
        Stop() ;
        bRet = SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool Controller::isActive()
{
    return m_bActive ;
}

void Controller::Stop()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_bActive = false ;
}
ControllerPtr Controller::createInstance(ACE_Thread_Manager* tm)
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset( new Controller(tm) ) ;
    }
    return m_instancePtr ;
}
ControllerPtr Controller::getInstance()
{
    if(m_instancePtr.get() == NULL )
        {
        throw "Controller instance is not created before using it" ;
            }
    return m_instancePtr ;
        }

bool Controller::QuitRequested(int seconds)
{
    if (seconds > 0)
    {
        for (unsigned int s=0; s< seconds; s++)
        {
            if ( !isActive() )
            {
                return true;
            }
            ACE_OS::sleep(1);
        }
    }
    return (!isActive()) ;
}


void Controller::PostMsg(SV_ULONG PolicyId)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Message_Block *mb = NULL;
    DebugPrintf(SV_LOG_INFO, "Controller got the message with policy Id %ld\n", PolicyId) ;
    mb = new ACE_Message_Block(0, (int) PolicyId);
    if( mb == NULL )
    {
        throw AppException("Failed to create message block\n") ;
    }
    m_MsgQueue.enqueue_prio(mb) ;
    DebugPrintf( SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME ) ;
}

ACE_Thread_Manager* Controller::thrMgr()
{
    return m_ThreadManagerPtr ;
}
Controller::Controller(ACE_Thread_Manager* tm)
{
    m_ThreadManagerPtr = tm ;
#ifdef SV_WINDOWS
    m_userToken_ = NULL ;
#endif
}

//Returns the priority of the policy, which is used while equeueing the policy into a corresponding controler's message queue.
//Note: Pass the policyType to the fucntion to reduce the load.
int Controller::getPolicyPriority(SV_ULONG policyId,std::string policyType)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	int priority = INM_MSG_PRIORITY_IDLE;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	do
	{
		if(0 == policyId)// 0 policy is always run with INM_MSG_PRIORITY_IDLE
			break;
        else if(policyId == RESUME_REPLICATION_POLICY_INTERNAL)// for internal policy set priority to normal
        {
            priority = INM_MSG_PRIORITY_NORMAL;
            break;
        }
		if(policyType.empty())
			if(configuratorPtr->getApplicationPolicyType(policyId, policyType) != SVS_OK )
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the policy type for the policy %d. Considering default priority.\n",policyId);
				break;
			}

		std::string strScheduleType;
		if(configuratorPtr->getApplicationPolicyPropVal(policyId,"ScheduleType",strScheduleType) == SVS_OK)
		{
			try
			{
				//Todo: implement the logic for other policies also.
				switch(boost::lexical_cast<int>(strScheduleType))
				{
				case 0:     //Not scheduled
				case 1:	    //Run every
					break;
				case 2:     //Run once
					if( policyType.compare("SourceReadinessCheck") == 0 ||
						policyType.compare("TargetReadinessCheck") == 0 )
						priority = INM_MSG_PRIORITY_ABOVE_NORMAL;
					else if( policyType.compare("Discovery") == 0 )
						priority = INM_MSG_PRIORITY_NORMAL;
                    else if( policyType.compare("UnregisterApplication") == 0)
                        priority = INM_MSG_PRIORITY_NORMAL;
					break;
				default:    //Unknown
					break;
				}
			}
			catch (const boost::bad_lexical_cast & cast_exp)
			{
				DebugPrintf(SV_LOG_ERROR,"Exception: bad cast. Type %s\n",cast_exp.what());
				DebugPrintf(SV_LOG_DEBUG,"Considering default priority\n");
			}
			catch (std::exception e)
			{
				DebugPrintf(SV_LOG_ERROR,"Caught unknown exception.\n");
				DebugPrintf(SV_LOG_DEBUG,"Considering default priority\n");
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the ScheduleType for the policy %d. Considering default priority.\n",policyId);
		}
	}while(false);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return priority;

}
void Controller::ProcessRequest(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    std::string policyType ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    m_volpackIndex = 0 ;           
    if( configuratorPtr->getApplicationPolicyType(policyId, policyType) == SVS_OK )
    {
        DebugPrintf(SV_LOG_INFO, "Policy Type for policyId %ld is %s\n", policyId, policyType.c_str()) ;
        if( policyType.compare("Discovery") == 0 || 
            policyType.compare("SourceReadinessCheck") == 0 ||
            policyType.compare("TargetReadinessCheck") == 0 || 
            policyType.compare("UnregisterApplication") == 0 )
        {
            if(policyType.compare("Discovery") != 0 || isDiscoveryApllicable(m_selectedApplicationNameSet))
            {
                DebugPrintf(SV_LOG_INFO, "Dispatching message to discovery controller\n") ;
                int priority = getPolicyPriority(policyId,policyType);
                DebugPrintf(SV_LOG_INFO, "Message priority is %d\n",priority) ;

                DiscoveryController::getInstance()->PostMsg(priority, policyId) ;
            }
        }
        else if(policyId != 0 &&
            policyType.compare("ProductionServerPlannedFailover") == 0 ||
            policyType.compare("DRServerPlannedFailover") == 0 ||
            policyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
            policyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
            policyType.compare("DRServerUnPlannedFailover") == 0  ||
            policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
            policyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
            policyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
            policyType.compare("ProductionServerPlannedFailback") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
            policyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
            policyType.compare("DRServerPlannedFailback") == 0 ||
            policyType.compare("ProductionServerFastFailback") == 0 ||
            policyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
            policyType.compare("ProductionServerFastFailbackStartApplication") == 0 ||
            policyType.compare("DRServerFastFailback") == 0 ||
            policyType.compare("Consistency") == 0 ||
            policyType.compare("PrepareTarget") == 0 ||
            policyType.compare("MountManagedLuns") == 0 ||
            policyType.compare("Mirror Configuration") == 0 ||
            policyType.compare("MirrorMonitor") == 0 ||
            policyType.compare("MirrorForceDelete") == 0 ||
            policyType.compare("Target LunMap") == 0 ||
            policyType.compare("ExecuteCommand") == 0 ||
            policyType.compare("CloudDrDrill") == 0 ||
			policyType.compare("CloudRecovery") == 0 ||
            policyType.compare("RcmRegistration") == 0 ||
            policyType.compare("RcmFinalReplicationCycle") == 0 ||
            policyType.compare("RcmMigration") == 0 ||
            policyType.compare("RcmResumeReplication") == 0
            )
        {
            std::string strAppType ;
            if(configuratorPtr->getApplicationType(policyId, strAppType) == SVS_OK)
            {
                int priority = getPolicyPriority(policyId, policyType);
                DebugPrintf(SV_LOG_INFO, "Message priority is %d\n",priority) ;
                AppControllerPtr appControllerPtr = AppFactory::GetInstance()->CreateAppController(m_ThreadManagerPtr, strAppType, m_selectedApplicationNameSet);
                appControllerPtr->PostMsg(priority, policyId) ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Application Type not found for policy Id %d\n", policyId) ;
            }
        }
        else if(policyType.compare("ReplicationApplianceHeartbeat") == 0)
        {
            std::string strAppType ;
            if(configuratorPtr->getApplicationType(policyId, strAppType) == SVS_OK)
            {
                int priority = getPolicyPriority(policyId, policyType);
                DebugPrintf(SV_LOG_INFO, "Message priority is %d\n",priority) ;
                if ((strAppType.find("ARRAY") == 0) || (strAppType.find("Array") == 0))
                    strAppType = "ARRAYHB";//If the app type is ARRAY and  the policy is heartbeat then send it to ARRAY Heart Beat controller.
                AppControllerPtr appControllerPtr = AppFactory::GetInstance()->CreateAppController(m_ThreadManagerPtr, strAppType, m_selectedApplicationNameSet);
                appControllerPtr->PostMsg(priority, policyId) ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Application Type not found for policy Id %d\n", policyId) ;
            }
        }
        else if(policyId != 0 &&  policyType.compare("ProductionServerPlannedFailover") == 0 ||
            policyType.compare("DRServerPlannedFailover") == 0 ||
            policyType.compare("DRServerUnPlannedFailover") == 0  || 
            policyType.compare("ProductionServerPlannedFailback") == 0 ||
            policyType.compare("DRServerPlannedFailback") == 0 ||
            policyType.compare("ProductionServerFastFailback") == 0 ||
            policyType.compare("DRServerFastFailback") == 0)
        {
            std::string strAppType ;
            if(configuratorPtr->getApplicationType(policyId, strAppType) == SVS_OK)
            {
                int priority = getPolicyPriority(policyId, policyType);
                DebugPrintf(SV_LOG_INFO, "Message priority is %d\n",priority) ;
                AppControllerPtr appControllerPtr = AppFactory::GetInstance()->CreateAppController(m_ThreadManagerPtr, strAppType, m_selectedApplicationNameSet);
                appControllerPtr->PostMsg(priority, policyId) ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Application Type not found for policy Id %d\n", policyId) ;
            }
        }
        else if(policyType.compare("VolumeProvisioning") == 0 || 
            policyType.compare("VolumeProvisioningV2") == 0 ||
            policyType.compare("Script") == 0 ||
            policyType.compare("SetupRepository") == 0 ||
            policyType.compare("SetupRepositoryV2") == 0 ||
            policyType.compare("ExportDevice") == 0 ||
            policyType.compare("UnExportDevice") == 0 )
        {
            AppControllerPtr appControllerPtr = AppFactory::GetInstance()->CreateAppController(m_ThreadManagerPtr, std::string("BULK"), m_selectedApplicationNameSet);
            appControllerPtr->PostMsg(0, policyId) ;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "The policy type %s is unknown\n", policyType.c_str()) ;
            DebugPrintf(SV_LOG_WARNING, "The corresponding policy id is %ld\n", policyId) ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the policyType for the policyId %ld\n", policyId) ;
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_FAILED) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
}
void Controller::setProcessRequests(bool process)
{
    m_shouldProcessRequests = process ;
}
