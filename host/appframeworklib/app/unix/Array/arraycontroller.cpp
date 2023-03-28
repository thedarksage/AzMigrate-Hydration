#include "Array/arraycontroller.h"
#include "controller.h"
#include "host.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "appexception.h"
#include "inmcommand.h"
#include "serializeatconfigmanagersettings.h"
#include "devicehelper.h"

ArrayControllerPtr ArrayController::m_instancePtr;
ArrayControllerPtr ArrayController::getInstance(ACE_Thread_Manager *tm)
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new  ArrayController(tm));
    }
    return m_instancePtr ;
}


int ArrayController::open(void *args)
{
    return this->activate(THR_BOUND);
}

int ArrayController::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

int ArrayController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return  0 ;
}

 ArrayController::ArrayController(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
    //TODO:: controller specific initialization
    m_policyHandlerMap["Mirror Configuration"] = &ArrayController::OnMirrorConfiguration;
    m_policyHandlerMap["MirrorMonitor"] = &ArrayController::OnMirrorMonitor;
    m_policyHandlerMap["ExecuteCommand"] = &ArrayController::OnExecuteCommand;
    m_policyHandlerMap["MirrorForceDelete"] = &ArrayController::OnMirrorForceDelete;
    m_policyHandlerMap["PrepareTarget"] = &ArrayController::OnPrepareTarget;
    m_policyHandlerMap["MountManagedLuns"] = &ArrayController::OnPrepareTarget;
    m_policyHandlerMap["Target LunMap"] = &ArrayController::OnTargetLunMap;
    m_policyHandlerMap["ReplicationApplianceHeartbeat"] = &ArrayController::OnReplicationApplianceHeartbeat;
}
SVERROR ArrayController::DiscoverLun(const std::string &str)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED ArrayController::%s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;

    if (str.find("NoMaps") != std::string::npos)
    {
        DebugPrintf(SV_LOG_DEBUG,"ArrayController::DiscoverLun Trying to upMap the LUN and No maps exists.\n");
        result = SVS_OK;
    }
    else
    {
        ArrayDiscoverBindingDeviceSetting  arrayBindingSettings;
        try 
        {
            arrayBindingSettings = unmarshal<ArrayDiscoverBindingDeviceSetting>(str);  
            DiscoverBindingDeviceSetting bindingSettings(arrayBindingSettings);
            DeviceHelper dh;
            if(dh.discoverMappedDevice(bindingSettings))
            {
                DebugPrintf(SV_LOG_DEBUG,"ArrayController::DiscoverLun - scsi id is %s\n", bindingSettings.lunId.c_str());
                result = SVS_OK;
            }
            else
            {
                //not returning error if we are deleting the lun map.
                if(DELETE_AT_LUN_BINDING_DEVICE_PENDING == bindingSettings.state)
                {
                    result = SVS_OK;
                }
                else
                {
                    result.error.hr = bindingSettings.resultState;
                }
                DebugPrintf( SV_LOG_ERROR,"DeviceHelper::discoverMappedDevice returned error  - state=%d resultState=%d.\n", bindingSettings.state, bindingSettings.resultState);
            }
        }
        catch (ContextualException const & ie) 
        {
            DebugPrintf(SV_LOG_ERROR, "ArrayController::DiscoverLun:%s", ie.what());
        }
        catch (std::exception const & e) 
        {
            DebugPrintf(SV_LOG_ERROR, "ArrayController::DiscoverLun:%s", e.what());
        } 
        catch (...) 
        {
            DebugPrintf(SV_LOG_ERROR, "ArrayController::DiscoverLun: unknown exception\n");
        }

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED ArrayController::%s\n", FUNCTION_NAME) ;
    return result;
}

SVERROR ArrayController::OnMirrorConfiguration()
{
    SVERROR result = SVS_FALSE;
    std::string errStr;
    DebugPrintf(SV_LOG_DEBUG, "Got Policy Type as Mirror Configuration for Array\n");
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    //Update the policy if it is succeed, otherwise let it be in cache
    if(configureMirror(m_policy.m_policyData, errStr) == SVS_OK)
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","Mirror Configuration success!");
    }
    else
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail",errStr);
        result = SVS_FALSE;
    }
    return result;
}

SVERROR ArrayController::OnMirrorMonitor()
{
    SVERROR result = SVS_FALSE;
    std::string errStr;
    DebugPrintf(SV_LOG_DEBUG, "Got Policy Type as Mirror Monitor for Array\n");
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    //Update the policy if it is succeed, otherwise let it be in cache
    if(mirrorMonitor(m_policy.m_policyData, errStr) == SVS_OK)
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","Mirror Monitor success!");
    }
    else
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail",errStr);
        return result;
        result = SVS_FALSE;
    }
    return result;
}

SVERROR ArrayController::OnExecuteCommand()
{
    SVERROR result = SVS_FALSE;
    std::string errStr;
    DebugPrintf(SV_LOG_DEBUG, "Got Policy Type as execute Command for Array\n");
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    if(executeCommand(m_policy.m_policyData, errStr) == SVS_OK)
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","execute Command success!");
    }
    else
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail","execute Command Failed - " + errStr);
        result = SVS_FALSE;
    }
    return result;
}

SVERROR ArrayController::OnMirrorForceDelete()
{
    SVERROR result = SVS_FALSE;
    std::string errStr;
    DebugPrintf(SV_LOG_DEBUG, "Got Policy Type as Force Mirror Delete for Array\n");
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    if(mirrorForceDelete(m_policy.m_policyData, errStr) == SVS_OK)
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","Force Mirror Delete success!");
    }
    else
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail","Force Mirror Delete Failed - " + errStr);
        result = SVS_FALSE;
    }
    return result;
}

SVERROR ArrayController::OnPrepareTarget()
{
    SVERROR result = SVS_FALSE;
    std::string mapInfo;
    std::string errStr;
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    //Update the policy if it is succeed, otherwise let it be in cache
    std::vector<std::string> vInfos;
    std::string delim(m_policy.m_policyData.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(m_policy.m_policyData, vInfos, delim);
    bool bVal = true;
    SVERROR ret;
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf(SV_LOG_DEBUG,"%s LUN info is %s\n",propsMap.find("PolicyType")->second.c_str(), vInfos[i].c_str());

        if(createLunMap( vInfos[i], mapInfo) != SVS_OK)
        {
            DebugPrintf( SV_LOG_ERROR,"createLunMap returned error - %s.\n",mapInfo.c_str());
            errStr = "Error while create LUN Map - " + mapInfo;
            bVal=false;
            continue;
        }
        else if ((ret = DiscoverLun(mapInfo)) != SVS_OK)
        {
            sleep(20); //Sleep for 20 seconds, before discovery when it fails for first time.

            if ((ret = DiscoverLun(mapInfo)) != SVS_OK)
            {
                DebugPrintf( SV_LOG_ERROR,"Discover LUN Failed in Prepare target for %s.\n", mapInfo.c_str());
                std::stringstream err;
                err<<"Error (" <<ret.error.hr<<") while Discovering the LUN - " << mapInfo ;
                errStr = err.str();
                bVal=false;
                continue;
            }
        }

        //Upon successful discovery mount the managed LUN to mount point.
        std::string resInfo;
        if( propsMap.find("PolicyType")->second.compare("MountManagedLuns") == 0) 
        {
            if( mountRetentionLun(vInfos[i], resInfo) != SVS_OK )
            {
                errStr = "Not Able to Mount the Retention LUN " + resInfo;
                DebugPrintf( SV_LOG_ERROR,"Mounting Retention LUN Returned %s.\n", errStr.c_str());
                bVal=false;
                continue;
            }
            else
            {
                DebugPrintf( SV_LOG_DEBUG,"Retention LUN Mounted succesfully %s.\n", resInfo.c_str());
            }
        }

        int retries = 10;
        while (retries)
        {
            if (!configuratorPtr->postLunMapInfo(vInfos[i]))
            {
                DebugPrintf( SV_LOG_DEBUG,"%s Error while call to postLunMap Info. Retrying %d.\n", FUNCTION_NAME, 10 - retries);
                sleep(2);
            }
            else
                break;
            --retries;
        }
        if (retries == 0)
        {
            bVal = false;
            errStr = "Error while Posting LUN map info to CX for LUN Info " + vInfos[i];
        }
    }
    if (bVal)
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","LUN is Mapped successfully");
    else
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail",errStr);
        result = SVS_FALSE;
    }
    return result;
}

SVERROR ArrayController::OnTargetLunMap()
{
    SVERROR result = SVS_FALSE;
    std::string resp;
    DebugPrintf(SV_LOG_DEBUG, "Got Policy Type as Target LunMap for Array\n");
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    //Update the policy if it is succeed, otherwise let it be in cache
    bool bVal;
    std::vector<std::string> vInfos;
    std::string delim(m_policy.m_policyData.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(m_policy.m_policyData, vInfos, delim);
    for (int i=0; i < vInfos.size(); i++)
    {
        bool bVal = false;
        DebugPrintf(SV_LOG_DEBUG,"Target LunMap LUN info is %s\n",vInfos[i].c_str());

        std::string resResp="";
        if(createLunMap( vInfos[i], resp) == SVS_OK)
        {
            /*
               if(Scsi3Reserve(vInfos[i], resResp) != SVS_OK)
               {
               resResp=", but SCSI3 Clear Reserve Failed. Try Manualy.";
               }
               else
               resResp = "";
               */

            DiscoverLun(resp);
            int retries = 10;
            while (retries)
            {
                bVal = configuratorPtr->postLunMapInfo(vInfos[i]);
                if (!bVal)
                {
                    DebugPrintf (SV_LOG_DEBUG,"Error while call to postLunMap Info. Retrying %d.\n", retries);
                    sleep(2);
                }
                else
                    break;
                --retries;
            }
            if (bVal)
                configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","Target LUN is UnMapped successfully.");
            else
                configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail","Failed to post Target LUN UnMap info to CS.");

        }
        else
        {
            configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Fail",resp);
            result = SVS_FALSE;
        }
    }
    return result;
}

SVERROR ArrayController::OnReplicationApplianceHeartbeat()
{
    SVERROR result = SVS_FALSE;
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
    return result;
}

SVERROR ArrayController::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED ArrayController::%s\n", FUNCTION_NAME) ;
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
    std::string strPolicyType = propsMap.find("PolicyType")->second;
    if ( m_policyHandlerMap.end() != m_policyHandlerMap.find(strPolicyType) )
    {
        DebugPrintf(SV_LOG_DEBUG, "ArrayController::Process Policy Type :: %s\n",strPolicyType.c_str());
        PolicyHandler_t pPolicyHandler = m_policyHandlerMap[strPolicyType];
        result = (this->*pPolicyHandler)();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ArrayController::Process Invalid policy type :: %s \n", strPolicyType.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED ArrayController::%s\n", FUNCTION_NAME) ;
    return result;
}

int ArrayController::Run(const std::string &cmd, std::string& response)
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ArrayController::Run - command is %s\n",cmd.c_str() );
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
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ArrayController::Run\n" );
    return retVal;
}
SVERROR ArrayController::deleteLunMap(const std::string &lunMapSettings, std::string &response)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " deleteLunMap " +"\'" + lunMapSettings + "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    return sv;
}

SVERROR ArrayController::mountRetentionLun(const std::string &lunInfo, std::string &response)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " mountLun " +"\'" + lunInfo + "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    DebugPrintf(SV_LOG_DEBUG, "Mounting LUN returned %s\n", response.c_str()) ;
    return sv;
}

SVERROR ArrayController::Scsi3Reserve(const std::string &lunInfo, std::string &response)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " scsi3Reserve " +"\'" + lunInfo + "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    DebugPrintf(SV_LOG_DEBUG, "SCSI Reserve returned %s\n", response.c_str()) ;
    return sv;
}

SVERROR ArrayController::createLunMap(const std::string &lunMapSettings, std::string &response)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " creatLunMap " +"\'" + lunMapSettings+ "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    return sv;
}

SVERROR ArrayController::setWriteSplit(const std::string &mirrorSetting, std::string &response)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " creatMirror " +"\'" + mirrorSetting + "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    DebugPrintf(SV_LOG_DEBUG, "setWriteSplit returned %s\n", response.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return sv;
}

SVERROR ArrayController::deleteWriteSplit(const std::string &mirrorSetting, std::string &response)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " forceDeleteWriteSplit " +"\'" + mirrorSetting + "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    DebugPrintf(SV_LOG_DEBUG, "forceDeleteWriteSplit returned %s\n", response.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return sv;
}

SVERROR ArrayController::monitorWriteSplit(const std::string &mirrorSetting, std::string &response)
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";// getLunInfo ";

    std::string cmd = cmdPath + " getMirrorLunStatus " +"\'" + mirrorSetting + "\'";;
    SVERROR sv = SVS_FALSE;
    if(0 == Run(cmd, response))
    {
        sv = SVS_OK;
    }
    //DebugPrintf(SV_LOG_DEBUG, "getMirrorLunStatus returned %s\n", response.c_str()) ;
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return sv;
}
SVERROR ArrayController::executeCommand(const std::string &mSettings, std::string & errStr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();

    SVERROR rVal = SVS_OK;
    std::vector<std::string> vInfos;
    std::string delim(mSettings.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(mSettings, vInfos, delim);
    std::string executeCommandStatus;
    //Get .pl  path
    LocalConfigurator lc;
    std::string cmdPath = lc.getInstallPath() + "/scripts/Array/arrayManager.pl";

    for (int i=0; i < vInfos.size(); i++)
    {
        std::string cmd = cmdPath + " executeCommand " +"\'" + vInfos[i] + "\'";;
        if(0 != Run(cmd, executeCommandStatus))
        {
            DebugPrintf(SV_LOG_ERROR,"Error while executeCommand for info %s\n",vInfos[i].c_str());
            errStr += executeCommandStatus;
            rVal = SVS_FALSE;
            continue;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return rVal;
}

SVERROR ArrayController::mirrorForceDelete(const std::string &mSettings, std::string & errStr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();

    SVERROR rVal = SVS_OK;
    std::vector<std::string> vInfos;
    std::string delim(mSettings.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(mSettings, vInfos, delim);
    std::string mirrorDeleteStatus;
    DebugPrintf(SV_LOG_ERROR,"Total number of the Mirror delete luns are %d\n",vInfos.size());
    for (int i=0; i < vInfos.size(); i++)
    {
        DebugPrintf(SV_LOG_ERROR,"Info [%d] is %s\n",i, vInfos[i].c_str());
    }
    for (int i=0; i < vInfos.size(); i++)
    {
        bool bVal = false;
        DebugPrintf(SV_LOG_DEBUG,"mirror delete info is %s\n",vInfos[i].c_str());
        if (deleteWriteSplit(vInfos[i], mirrorDeleteStatus) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Error while force deleting for mirror info %s\n",vInfos[i].c_str());
            errStr = "Mirror Force Delete returned error - " + mirrorDeleteStatus;
            rVal = SVS_FALSE;
            continue;
        }

        int retries = 10; 
        while (retries)
        {
            bVal = configuratorPtr->postForceMirrorDeleteInfo(mirrorDeleteStatus);
            if (!bVal)
            {
                DebugPrintf( SV_LOG_DEBUG,"Error while call to postForceMirrorDeleteInfo. Retrying %d.\n", 10 - retries);
                sleep(2);
            }
            else
                break;
            --retries;
        }
        if (!bVal)
        {  
            rVal = SVS_FALSE;
            errStr = "postForceMirrorDeleteInfo Failed to update mirror info to CX.\n";
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return rVal;
}
SVERROR ArrayController::mirrorMonitor(const std::string &mSettings, std::string & errStr)
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();

    SVERROR rVal = SVS_OK;
    std::vector<std::string> vInfos;
    std::string delim(mSettings.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(mSettings, vInfos, delim);
    std::string mirrStatus;
    for (int i=0; i < vInfos.size(); i++)
    {
        bool bVal = false;
        //DebugPrintf(SV_LOG_DEBUG,"mirror info is %s\n",vInfos[i].c_str());
        if (monitorWriteSplit(vInfos[i], mirrStatus) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Error while monitoring mirror for mirror info %s\n",vInfos[i].c_str());
            errStr = "Mirror monitor returned error - " + mirrStatus;
            rVal = SVS_FALSE;
            continue;
        }

        int retries = 10;    
        while (retries)
        {
            bVal = configuratorPtr->postMirrorStatusInfo(mirrStatus);
            if (!bVal)
            {
                DebugPrintf( SV_LOG_DEBUG,"Error while call to postMirrorInfo. Retrying %d.\n", 10 - retries);
                sleep(2);
            }
            else
                break;
            --retries;
        }
        if (!bVal)
        {  
            rVal = SVS_FALSE;
            errStr = "postMirrorInfo Failed to update mirror info to CX.\n";
        }
    }
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return rVal;
}
SVERROR ArrayController::configureMirror(const std::string &mSettings, std::string & errStr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();

    SVERROR rVal = SVS_OK; //Set to false even if it fails for one lun.
    std::vector<std::string> vInfos;
    std::string delim(mSettings.c_str(),1);//Delemiter is prepended to the info string by CX.
    Tokenize(mSettings, vInfos, delim);
    std::string lmapResp, mirrResp;
    SVERROR ret;
    for (int i=0; i < vInfos.size(); i++)
    {
        bool isLunMapped = false;

        DebugPrintf(SV_LOG_DEBUG,"lun info is %s\n",vInfos[i].c_str());

        if (createLunMap(vInfos[i], lmapResp) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Error while LUN map for LUN info %s - returned %s\n",vInfos[i].c_str(),lmapResp.c_str());
            rVal = SVS_FALSE;
            errStr = "Source LUN Map failed - " + lmapResp;
        }
        else if ((ret = DiscoverLun(lmapResp)) != SVS_OK) 
        {
            sleep(20); //Sleep for 20 seconds, before discovery when it fails for first time.
            if ((ret = DiscoverLun(lmapResp)) != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR,"Error while discovering the LUN from map for LUN info %s.\n", lmapResp.c_str());
                std::stringstream err;
                err<<"Error (" <<ret.error.hr<<") while Discovering the LUN - " << lmapResp;
                errStr = err.str();
                rVal = SVS_FALSE;
            }
            else
                isLunMapped = true;
        }
        else
            isLunMapped = true;

        if (isLunMapped && setWriteSplit(vInfos[i], mirrResp) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Error while mirror creation for LUN info %s\n",mirrResp.c_str());
            if (deleteLunMap(vInfos[i], lmapResp) != SVS_OK )
            {
                DebugPrintf(SV_LOG_ERROR,"Error while deleting LUN map for LUN info %s\n",lmapResp.c_str());
            }
            DiscoverLun(lmapResp);
            errStr = "Write Split Configuration failed - " + mirrResp;
            rVal = SVS_FALSE;
        }

        int retries = 10;    
        bool bVal = false;
        while (retries)
        {
            //Sending the first parameter to identify the array and source luninfo
            // incase if lun mapping fails, the mirrResp will be null
            bVal = configuratorPtr->postMirrorInfo(vInfos[i], mirrResp);
            if (!bVal)
            {
                DebugPrintf( SV_LOG_DEBUG,"Error while call to postMirrorInfo. Retrying %d.\n", 10 - retries);
                sleep(2);
            }
            else
                break;
            --retries;
        }
        if (!bVal)
        {  
            rVal = SVS_FALSE;
            errStr = "postMirrorInfo Failed to update mirror info to CX.\n";
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return rVal;
}
SVERROR ArrayController::prepareTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;

    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    schedulerPtr->UpdateTaskStatus(policyId,TASK_STATE_STARTED) ;

    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;

    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Success",std::string("Test Check for Array Application PrepareTarget. Sending Success for now"));

    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED);

    result = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}


void ArrayController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED ArrayController::%s\n", FUNCTION_NAME) ;
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
    DebugPrintf(SV_LOG_DEBUG, "EXITED ArrayController::%s\n", FUNCTION_NAME) ;
}

