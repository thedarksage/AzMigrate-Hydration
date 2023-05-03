#include "Array/arrayHBController.h"
#include "controller.h"
#include "host.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "appexception.h"
#include "inmcommand.h"
#include "serializeatconfigmanagersettings.h"

ArrayHBControllerPtr ArrayHBController::m_instancePtr;
ArrayHBControllerPtr ArrayHBController::getInstance()
{
    if( m_instancePtr.get() == NULL )
    {
        ControllerPtr controllerPtr = Controller::getInstance() ;
        m_instancePtr.reset(new  ArrayHBController(controllerPtr->thrMgr()) ) ;
    }
    return m_instancePtr ;
}


int ArrayHBController::open(void *args)
{
    return this->activate(THR_BOUND);
}

int ArrayHBController::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

int ArrayHBController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return  0 ;
}

    ArrayHBController::ArrayHBController(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
    //TODO:: controller specific initialization
}

SVERROR ArrayHBController::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED ArrayHBController::%s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    bool process = false ;
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

    if(0 == propsMap.find("PolicyType")->second.compare("ReplicationApplianceHeartbeat"))
    {
        DebugPrintf(SV_LOG_DEBUG, "Got policy type as ReplicationApplianceHeartbeat\n");
        LocalConfigurator lc;
        AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
        AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
        std::string cmd = lc.getInstallPath() + "/scripts/Array/arrayManager.pl ReplicationApplianceHeartbeat '" + m_policy.m_policyData + "'";
        std::string response;
        if(0 != Run(cmd, response))
        {
            DebugPrintf(SV_LOG_ERROR, "Error white executing ReplicationApplianceHeartbeat command: %s\n", cmd.c_str());
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Fail", "ReplicationApplianceHeartbeat command execution failed - " + response);
        }
        else
        {
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Success", "ReplicationApplianceHeartbeat command success");
        }

    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED ArrayHBController::%s\n", FUNCTION_NAME) ;
    return result;
}

int ArrayHBController::Run(const std::string &cmd, std::string& response)
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayHBController::Run - command is %s\n",cmd.c_str() );
    InmCommand arrCmd(cmd, true);
    std::stringstream msg;

    arrCmd.SetShouldWait(false);
    arrCmd.SetOutputSize(100*1024*1024);//Set to 100MB, what apache can transfer.
    InmCommand::statusType status = arrCmd.Run();

    int retVal=-1;
    if(status != InmCommand::running && status != InmCommand::completed)
    {
        DebugPrintf( SV_LOG_ERROR, "internal error occured during execution of the Array cmd: %s\n",  cmd.c_str());
        if(!arrCmd.StdErr().empty())
        {
            msg << arrCmd.StdErr();
        }
        response = msg.str();
        return -1;
    }

    while(true)
    {
        status = arrCmd.TryWait(); // just check for exit don't wait
        if (status == InmCommand::internal_error) 
        {
            DebugPrintf( SV_LOG_DEBUG, "internal error occured during execution of the Array command: %s\n", cmd.c_str());
            if(!arrCmd.StdErr().empty())
            {
                msg << arrCmd.StdErr();
            }
            break;
        } 
        else if (status == InmCommand::completed) 
        {
            if(arrCmd.ExitCode())
            {
                DebugPrintf( SV_LOG_DEBUG ,"Array command failed : Exit Code = %d\n", arrCmd.ExitCode());
            }
            if(!arrCmd.StdOut().empty())
            {
                msg<< arrCmd.StdOut() << std::endl;
            }
            if(!arrCmd.StdErr().empty())
            {
                msg << arrCmd.StdErr()<< std::endl;
            }
            break;
        }
        else 
        {
            arrCmd.ReadPipes();
            //If there is nothing to read sleep for a while.
            if (arrCmd.StdOut().empty())
                sleep(1);
        }
    }
    retVal = arrCmd.ExitCode();
    response = msg.str();
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayHBController::Run\n" );
    return retVal;
}


void ArrayHBController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED ArrayHBController::%s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;

    schedulerPtr->UpdateTaskStatus(policyId,TASK_STATE_STARTED) ;
    if(schedulerPtr->isPolicyEnabled(m_policyId))
    {
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    }

    if( configuratorPtr->getApplicationPolicies(policyId, "ARRAY",m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED);
    DebugPrintf(SV_LOG_DEBUG, "EXITED ArrayHBController::%s\n", FUNCTION_NAME) ;
}

