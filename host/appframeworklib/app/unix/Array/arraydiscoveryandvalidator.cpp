#include <list>
#include "arraydiscoveryandvalidator.h"
#include "config/appconfigurator.h"
#include <boost/lexical_cast.hpp>
#include "ruleengine/unix/ruleengine.h"
#include "util/common/util.h"
#include "discovery/discoverycontroller.h"
//#include "util/system.h"
#include <boost/lexical_cast.hpp>
#include "appscheduler.h"

#include "inmcommand.h"
#include <string>

ArrayDiscoveryAndValidator::ArrayDiscoveryAndValidator(const ApplicationPolicy& policy)
:pcliStatus(0),DiscoveryAndValidator(policy)
{
}

SVERROR ArrayDiscoveryAndValidator::PerformReadinessCheckAtSource()
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayDiscoveryAndValidator:%s:\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    ReadinessCheckInfo readinessCheckInfo ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
    readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
    //readinessCheckInfo.validationsList = readinessChecksList ;

    discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayDiscoveryAndValidator:%s:\n", FUNCTION_NAME);
    return SVS_OK ;
} 

SVERROR ArrayDiscoveryAndValidator::PerformReadinessCheckAtTarget()
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayDiscoveryAndValidator:%s:\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    ReadinessCheckInfo readinessCheckInfo ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
    readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
    //readinessCheckInfo.validationsList = readinessChecksList ;

    discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayDiscoveryAndValidator:%s:\n", FUNCTION_NAME);
    return SVS_OK ;
} 
int ArrayDiscoveryAndValidator::Run(const std::string &cmd, std::string& response)
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayDiscoveryAndValidator::Run - command is %s\n",cmd.c_str() );
    InmCommand arrCmd(cmd, true);
    std::stringstream msg;

    arrCmd.SetShouldWait(false);
    arrCmd.SetOutputSize(100*1024*1024);//Set to 100MB, what apache can transfer.
    InmCommand::statusType status = arrCmd.Run();
    int rv = -1;

    if(status != InmCommand::running && status != InmCommand::completed)
    {
        DebugPrintf( SV_LOG_ERROR, "internal error occured during execution of the Array cmd: %s\n",  cmd.c_str());
        if(!arrCmd.StdErr().empty())
        {
            msg << arrCmd.StdErr();
        }
        response = msg.str();
        return 1;
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
            rv = -1;
            break;
        } 
        else if (status == InmCommand::completed) 
        {
            if(arrCmd.ExitCode())
            {
                DebugPrintf( SV_LOG_DEBUG ,"Array command failed : Exit Code = %d\n", arrCmd.ExitCode());
                rv = 1;
            }
            else
            {
                rv = 0;
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
    response = msg.str();
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayDiscoveryAndValidator::Run\n" );
    return rv;
}
SVERROR ArrayDiscoveryAndValidator::PostDiscoveryInfoToCx()
{
    SVERROR bRet = SVS_FALSE ;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    DiscoveryInfo discoverInfo ;
    ArrayDiscoveryInfo arrDiscInfo;

    fillPolicyInfoInfo(arrDiscInfo.m_policyInfo);
    if(pcliStatus !=  0)
    {
        arrDiscInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "2")) ;
        arrDiscInfo.m_policyInfo.insert(std::make_pair("ErrorString", pcliFailReason));
    }
    else
    {
        arrDiscInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "1")) ;
        arrDiscInfo.m_policyInfo.insert(std::make_pair("ErrorString", "Axiom discovery completed."));
    }
    discCtrlPtr->m_disCoveryInfo->arrayDiscoveryInfo.push_back(arrDiscInfo);

    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED ArrayDiscoveryAndValidators::%s\n", FUNCTION_NAME) ;
    return bRet ; 
}

SVERROR ArrayDiscoveryAndValidator::Discover(bool shouldUpdateCx)
{
    SVERROR bRet = SVS_OK;
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME );

    //This is ArrayInfo routine.
    std::string info;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    configuratorPtr->GetCXAgent().GetArrayInfos(info);
    LocalConfigurator lc;
    std::vector<std::string> vInfos;
    std::string delim(info.c_str(),1);
    Tokenize(info, vInfos, delim);
    std::string response;
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf( SV_LOG_DEBUG,"Array info %d is  %s\n", i, vInfos[i].c_str());
        std::string cmd = lc.getInstallPath() + "/scripts/Array/arrayManager.pl getArrayInfo ";
        cmd += "\'" + vInfos[i] + "\'";;
        pcliStatus = Run(cmd, response);
        if (pcliStatus != 0 )
        {
            pcliFailReason = response;
        }
        DebugPrintf( SV_LOG_DEBUG,"DiscoverArrays:: %s\n",response.c_str() );
        try 
        {
            configuratorPtr->GetCXAgent().PostStorageArrayInfoToReg(response);
            ReportLuns(shouldUpdateCx);
            RegisterCxWithArray();
        }
        catch (...)
        {
            DebugPrintf( SV_LOG_ERROR,"Exception occured while calling ArrayDiscoveryAndValidator::.PostStorageArrayInfoToReg\n");
        }
    }
    
    try 
    {
        //Register Cx Information along with Agent version, logpath with array.
        RegisterCxWithArray();
        DeregisterCxWithArray();
    }
    catch (...)
    {
        DebugPrintf( SV_LOG_ERROR,"Exception occured while calling ArrayDiscoveryAndValidator::.RegisterCxWithArray\n");
    }
    //* Report Lun information to Cx for all registered arrays.
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME );
    return bRet;
}
void ArrayDiscoveryAndValidator::DiscoverForArrayInfo(const std::string &info)
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME );
    RegisterCxWithArray();
    DeregisterCxWithArray();

    //This is ArrayInfo routine.
    LocalConfigurator lc;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    std::vector<std::string> vInfos;
    std::string delim(info.c_str(),1);
    Tokenize(info, vInfos, delim);
    std::string response;
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf( SV_LOG_DEBUG,"Array info %d is  %s\n", i, vInfos[i].c_str());
        std::string cmd = lc.getInstallPath() + "/scripts/Array/arrayManager.pl getArrayInfo ";
        cmd += "\'" + vInfos[i] + "\'";;
        pcliStatus = Run(cmd, response);
        if (pcliStatus != 0 )
        {
            pcliFailReason = response;
        }
        DebugPrintf( SV_LOG_DEBUG,"DiscoverArrays:: %s\n",response.c_str() );
        configuratorPtr->GetCXAgent().PostStorageArrayInfoToReg(response);
    }
 
    ReportLunsForArrayInfo(info);
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME );
}

SVERROR ArrayDiscoveryAndValidator::Process()
{
    SVERROR bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED: ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME);

    if (m_policyId == 0) // Default Discovery
    {
        DiscoveryAndValidator::Process();
    }
    else 
    {
        if(m_policyType.compare("SourceReadinessCheck") == 0)
        {
            PerformReadinessCheckAtSource() ;
        }
        else if(m_policyType.compare("TargetReadinessCheck") == 0)
        {
            PerformReadinessCheckAtTarget() ;
        }
        else if(m_policyType.compare("Discovery") == 0)
        {
            ApplicationPolicy policy ;
            AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
            AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
            SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);

            SV_ULONGLONG taskFrequency = schedulerPtr->getTaskFrequency(m_policyId);
            if(!configuratorPtr->getApplicationPolicies(m_policyId, "", policy))
            {
                DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", m_policyId) ;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_COMPLETED) ;
            }
            else
            {
                configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
                DebugPrintf(SV_LOG_DEBUG,"ArrayDiscoveryAndValidator::Policy Data for policy id %d is %s\n\n", m_policyId, policy.m_policyData.c_str());
                if (taskFrequency == 0) // run now is for lun discovery
                {
                    DiscoverForArrayInfo(policy.m_policyData);
                    //ReportLunsForArrayInfo(policy.m_policyData);
                }
                else //run every is for whole array discovery
                {
                    DiscoverForArrayInfo(policy.m_policyData);
                }

                if (PostDiscoveryInfoToCx() != SVS_OK)
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed to PostDiscoveryInfoToCx:\n");
                }
            }
        }
        else
            DebugPrintf(SV_LOG_DEBUG,"ArrayDiscoveryAndValidator:: Invalid policy - policy id %d \n", m_policyId);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED: ArrayDiscoveryAndValidator::%s\n", FUNCTION_NAME);
    return bRet;
}
void ArrayDiscoveryAndValidator::RegisterCxWithArray()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED: %s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_OK;
    std::string info;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    configuratorPtr->GetCXAgent().GetCxInfoToRegisterWithArray(info);
    LocalConfigurator lc;
    std::vector<std::string> vInfos;
    DebugPrintf( SV_LOG_DEBUG, "info is : %s\n", info.c_str());
    std::string delim(info.c_str(),1);
    Tokenize(info, vInfos, delim);
    std::string response;
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf( SV_LOG_DEBUG,"Array info %d is  %s\n", i, vInfos[i].c_str());
        std::string cmd = lc.getInstallPath() + "/scripts/Array/arrayManager.pl regCxWithArray ";
        cmd += "\'" + vInfos[i] + "\'";
        pcliStatus = Run(cmd, response);
        if (pcliStatus != 0 )
        {
            pcliFailReason = response;
        }
        DebugPrintf( SV_LOG_DEBUG, "Cx Registration With Array Response:: %s\n",response.c_str() );
        configuratorPtr->GetCXAgent().PostCxRegWithArrayResponse(response);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED: %s\n", FUNCTION_NAME);
}

void ArrayDiscoveryAndValidator::DeregisterCxWithArray()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_OK;
    std::string info;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    configuratorPtr->GetCXAgent().GetCxInfoToDeregisterWithArray(info);
    LocalConfigurator lc;
    std::vector<std::string> vInfos;
    DebugPrintf( SV_LOG_DEBUG, "info is : %s\n", info.c_str());
    std::string delim(info.c_str(),1);
    Tokenize(info, vInfos, delim);
    std::string response;
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf(SV_LOG_DEBUG,"Array info %d is %s\n", i, vInfos[i].c_str());
        std::string cmd = lc.getInstallPath() + "/scripts/Array/arrayManager.pl unregCxWithArray ";
        cmd += "\'" + vInfos[i] + "\'";
        pcliStatus = Run(cmd, response);
        if (pcliStatus != 0 )
        {
            pcliFailReason = response;
        }
        DebugPrintf(SV_LOG_DEBUG, "Cx Deregistration With Array Response:: %s\n", response.c_str());
        configuratorPtr->GetCXAgent().PostCxDeregWithArrayResponse(response);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

void ArrayDiscoveryAndValidator::ReportLunsForArrayInfo(const std::string &info)
{
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::vector<std::string> vInfos;
    std::string delim(info.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(info, vInfos, delim);
    std::string response;
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf( SV_LOG_DEBUG,"%d - Lun Discovery for Array info - %s\n", i, vInfos[i].c_str());
        std::string cmd = cmdPath + " getLunInfo " +"\'" + vInfos[i] + "\'";;
        pcliStatus = Run(cmd, response);
        if (pcliStatus != 0 )
        {
            pcliFailReason = response;
        }
        DebugPrintf( SV_LOG_DEBUG,"DiscoverLuns are:: %s\n",response.c_str() );
        
        bool rVal = false;
        int tries = 0;
        while (!rVal)
        {
            try
            {
                rVal = configuratorPtr->GetCXAgent().PostLunInfoToReg(response);
                if (!rVal)
                {
                    if (tries == 5)
                        break;
                    tries++;
                    DebugPrintf( SV_LOG_DEBUG,"Error while call to  ArrayDiscoveryAndValidator::PostLunInfoToReg. Retrying...\n");
                    sleep(2);
                }
                    
            }
            catch (...)
            {
                DebugPrintf( SV_LOG_ERROR,"Exception occurid while call to  ArrayDiscoveryAndValidator::PostLunInfoToReg\n");
                tries++;
                if (tries == 5)
                    break;
                sleep(2);
            }
        }
    }
    
}
void ArrayDiscoveryAndValidator::ReportLuns(bool shouldUpdateCx)
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ReportLuns\n" );
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    std::string info;
    try
    {
        configuratorPtr->GetCXAgent().GetRegisteredArrays(info);
    }
    catch (...)
    {
        DebugPrintf( SV_LOG_ERROR,"Exception occurid while call to GetRegisteredArrays\n");
        DebugPrintf( SV_LOG_DEBUG,"EXITED: ReportLuns\n" );
        return ;
    }
    ReportLunsForArrayInfo(info);
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ReportLuns\n" );
}
