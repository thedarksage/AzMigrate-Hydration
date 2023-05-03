#include <boost/lexical_cast.hpp>
#include <exception>
#include "appglobals.h"
#include "controller.h"
#include "discoverycontroller.h"
#include "ruleengine.h"
#include "appexception.h"
#include "discoveryfactory.h"
#include "util/common/util.h"

DiscoveryControllerPtr DiscoveryController::m_discoveryControllerPtr ;



void DiscoveryController::start()
{
    if ( -1 == open() ) 
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED DiscoveryController::init failed to open: %d\n", 
            ACE_OS::last_error());
        throw AppException("Failed to start discovery controller thread\n") ;
    }
}

SVERROR DiscoveryController::init(const std::set<std::string>& selectedApplicationNameSet)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_OK ;
    m_selectedApplicationNameSet = selectedApplicationNameSet;
	return bRet ;
}

int DiscoveryController::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

int DiscoveryController::close(u_long flags)
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return 0;
}

int DiscoveryController::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ControllerPtr controllerPtr = Controller::getInstance() ;
    if( !controllerPtr->QuitRequested(5) )
    {
        ProcessRequests() ;        
    }  
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0 ;
}

bool DiscoveryController::createInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
    if(m_discoveryControllerPtr.get() == NULL)
	{
        m_discoveryControllerPtr.reset( new DiscoveryController(Controller::getInstance()->thrMgr()) );
        bRet = true;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}

DiscoveryControllerPtr DiscoveryController::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_discoveryControllerPtr.get() == NULL )
    {
        DebugPrintf(SV_LOG_DEBUG, "Discovery controller pointer is not available \n");
        throw AppException("Discovery controller pointer is not available \n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_discoveryControllerPtr ;
}

DiscoveryController::DiscoveryController(ACE_Thread_Manager* threadManager)
: ACE_Task<ACE_MT_SYNCH>(threadManager)
{

}

/*void DiscoveryController::DoWork()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ControllerPtr controllerPtr = Controller::getInstance() ;
    AppLocalConfigurator appLocalConfigurator ;

    if( !controllerPtr->QuitRequested(5) )
    {
        ProcessRequests() ;        
    }    
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}*/


void DiscoveryController::PostMsg(int priority, SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Message_Block *mb = NULL ;	


    mb = new ACE_Message_Block(0, (int) policyId);
    
	if( mb == NULL )
	{
		throw AppException("Failed to create message block\n") ;
	}
    mb->msg_priority(priority) ;
    m_MsgQueue.enqueue_prio(mb) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void DiscoveryController::ProcessRequests()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppControllerPtr appControllerPtr ;
	AppSchedulerPtr appSchedPtr = AppScheduler::getInstance() ;
    while( !Controller::getInstance()->QuitRequested(1) )
    {
        ACE_Message_Block *mb;
        ACE_Time_Value waitTime = ACE_OS::gettimeofday ();
        int waitSeconds = 5; //Wait Max 5 seconds while dequeuing message block.
        waitTime.sec (waitTime.sec () + waitSeconds) ;
        if (-1 == Queue().dequeue_head(mb, &waitTime)) 
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN) 
            {
                continue;
            }
            break;
        }
        SV_ULONG policyId = mb->msg_type() ;
        mb->release() ;
        bool taskRun = false ;
        try
        {
            appSchedPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
			m_disCoveryInfo.reset( new DiscoveryInfo()) ;
#ifdef SV_WINDOWS
			m_hostLevelDiscoveryInfo.clean();
#endif
			m_ReadinesscheckInfoList.clear();
			if( !Controller::getInstance()->QuitRequested(1) )
			{
				ProcessMsg(policyId) ;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Not processing the policy as Quit signal is caught for policy id %d\n", policyId) ;
			}
			if( !Controller::getInstance()->QuitRequested(1) )
			{
				UpdateToCx(policyId);
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Not updating the CX as Quit signal is caught for policy id %d\n", policyId) ;
			}
            taskRun = true ;
        }
        catch(AppException& exception)
        {
            DebugPrintf(SV_LOG_ERROR, "DiscoveryController::ProcessRequests caught exception %s\n", exception.to_string().c_str()) ;
			DebugPrintf(SV_LOG_ERROR, "Settting state for task %ld asfailed\n",policyId);
        }
        catch(std::exception& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "DiscoveryController::ProcessRequests caught exception %s\n", ex.what()) ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Unavoidable error while discovery controller processing request for policyId %ld\n", policyId) ;
        }
        if( taskRun )
        {
            appSchedPtr->UpdateTaskStatus(policyId,TASK_STATE_COMPLETED);
        }
        else
        {
            appSchedPtr->UpdateTaskStatus(policyId,TASK_STATE_FAILED);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


ACE_SHARED_MQ& DiscoveryController::Queue()
{
    return m_MsgQueue ;
}


void DiscoveryController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "DiscoveryController started processing policyId %ld\n", policyId) ;

#ifdef SV_WINDOWS
	//Perform system level discovery before going for application sepcific discvorey.
	//TODO: verify that the system level discovery info is required for readinesschecks,
	//      if not skipt the discovery for readinesscheck policy types.
	if(getHostLevelDiscoveryInfo(m_hostLevelDiscoveryInfo) == SVS_OK)
		m_hostLevelDiscoveryInfo.updateToDiscoveryInfo(*m_disCoveryInfo);
#endif
	
	std::list<DiscoveryAndValidatorPtr> discoveryAndValidatorPtrList = DiscoveryFactory::getDiscoveryAndValidator(policyId, m_selectedApplicationNameSet) ;
	std::list<DiscoveryAndValidatorPtr>::iterator discoveryAndValidatorPtrIter = discoveryAndValidatorPtrList.begin() ;
	while( discoveryAndValidatorPtrIter != discoveryAndValidatorPtrList.end() )
	{
		if( !Controller::getInstance()->m_shouldProcessRequests.value())
		{
			DebugPrintf(SV_LOG_DEBUG, "Cannot process Dicovery.. Failover may be in progress\n") ;
			ACE_OS::sleep(10);
            continue ;
		}
		(*discoveryAndValidatorPtrIter)->init();
		(*discoveryAndValidatorPtrIter)->Process() ;
		discoveryAndValidatorPtrIter++ ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
SVERROR DiscoveryController::UpdateToCx(SV_ULONG policyId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE;
	AppSchedulerPtr appSchedPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = appSchedPtr->getInstanceId(policyId);
	//AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	std::string policyType ;
	AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	if( appConfiguratorPtr->getApplicationPolicyType(policyId, policyType) == SVS_OK )
	{
		DebugPrintf(SV_LOG_INFO, "Policy Type for policyId %ld is %s\n", policyId, policyType.c_str()) ;
		if( policyType.compare("Discovery") == 0 )
		{
			DebugPrintf(SV_LOG_INFO, "Update Discovery Infomation to CX\n") ;
            if( m_disCoveryInfo->exchInfo.size() != 0 || m_disCoveryInfo->sqlInfo.size() != 0 || m_disCoveryInfo->fileserverInfo.size() != 0 || m_disCoveryInfo->oraclediscoveryInfo.size() != 0 || m_disCoveryInfo->arrayDiscoveryInfo.size() != 0 || m_disCoveryInfo->db2discoveryInfo.size() != 0 )
            {
                    appConfiguratorPtr->UpdateDiscoveryInfo(*m_disCoveryInfo,policyId,instanceId) ;
                    bRet = SVS_OK;
            }
		}
		else if(policyType.compare("SourceReadinessCheck") == 0 || policyType.compare("TargetReadinessCheck") == 0 )
		{
            if( m_ReadinesscheckInfoList.size() > 0 )
            {
			    appConfiguratorPtr->UpdateReadinessCheckInfo(m_ReadinesscheckInfoList,policyId,instanceId);
                bRet = SVS_OK;
            }
		}
        else if(policyType.compare("UnregisterApplication") == 0)
        {
            //appConfiguratorPtr->UpdateUnregisterApplicationInfo(policyId,instanceId);
            bRet = SVS_OK;
        }
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Invalid Policy Type for policyId %ld is %s\n", policyId, policyType.c_str());
		}
	}
	/*UpdateTaskStatus is already handled in processRequests(). Commenting here to avoid repeated update*/
    //appSchedPtr->UpdateTaskStatus(policyId,TASK_STATE_COMPLETED);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}
