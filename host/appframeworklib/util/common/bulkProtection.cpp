#include "appglobals.h"
#include "bulkProtection.h"
#include "controller.h"
#include "service.h"
#include "util.h"
#include <boost/lexical_cast.hpp>
#include "Consistency/TagGenerator.h"
#include "host.h"
#include "util/common/util.h"
#ifdef SV_WINDOWS
#include "clusterutil.h"
#include "system.h"
#endif
#include "appexception.h"
#include <boost/algorithm/string.hpp>
#include "util/exportdevice.h"
#include "appcommand.h"
#include "serializationtype.h"

void ExportUnExportDevice(SV_ULONG policyId);

BulkProtectionControllerPtr BulkProtectionController::m_BPControllerPtr;

BulkProtectionControllerPtr BulkProtectionController::getInstance(ACE_Thread_Manager* tm)
{
    if( m_BPControllerPtr.get() == NULL )
    {
        m_BPControllerPtr.reset(new  BulkProtectionController(tm) ) ;
		if( m_BPControllerPtr.get() != NULL )
			m_BPControllerPtr->Init();
    }
    return m_BPControllerPtr ;
}


int BulkProtectionController::open(void *args)
{
    return this->activate(THR_BOUND);
}

int BulkProtectionController::close(u_long flags )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    //TODO:: controller specific code
    return 0 ;
}

int BulkProtectionController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    while(!Controller::getInstance()->QuitRequested(5))
    {
        ProcessRequests() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return  0 ;
}

BulkProtectionController::BulkProtectionController(ACE_Thread_Manager* thrMgr)
: AppController(thrMgr)
{
    //TODO:: controller specific code
	m_pConfig = NULL;
}

SVERROR  BulkProtectionController::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;
    bool bProcess = false ;
    bool bCheckVolAccess = true;
    std::string volumeSparator = ",";
#ifdef SV_WINDOWS
    m_bCluster = isClusterNode();
    bCheckVolAccess = m_bCluster;
    volumeSparator = ";";
#endif

    std::string strAciveNodeName;
    std::list<std::string> networkNameList;
    std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;

    std::string strLocalHost = Host::GetInstance().GetHostName();
    if(bCheckVolAccess)
    {
        std::map<std::string, std::string>::iterator iter;
        iter = propsMap.find("ConsistencyCmd");
        if(iter != propsMap.end())
        {
            std::string strVolNames = iter->second;
            std::size_t index;
            index = strVolNames.find(" -v");
            if(index != std::string::npos)
            {
                strVolNames = strVolNames.substr(index+3,strVolNames.length());
                index = strVolNames.find(" -");
                if(index != std::string::npos)
                {
                    strVolNames = strVolNames.substr(0,index);
                }
            }
			removeChars("\"",strVolNames); // Remove double quotes for the volumes if exist.
            DebugPrintf(SV_LOG_INFO,"Volumes names = %s\n",strVolNames.c_str());
            std::list<std::string> volList;
            trimSpaces(strVolNames);
            volList = tokenizeString(strVolNames, volumeSparator);
            if(volList.empty() == false)
            {
                bProcess = CheckVolumesAccess(volList);				
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to get volume names for consistency Command.\n");
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "there is no option with key as ConsistencyCmd\n") ;
			bProcess = true ;
        }
    }
    else
    {
        bProcess = true ;
    }

    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    //Consistency and BreakClsuter should be run from Active node only
    if( bProcess )
    {
        if(schedulerPtr->isPolicyEnabled(m_policyId))
        {
            SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
            AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
            appConfiguratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
        }
        if( propsMap.find("PolicyType")->second.compare("Consistency") == 0 )
        {
            if( Consistency() == SVS_OK )
            {
                retStatus = SVS_OK;
            }
            else
            {
                DebugPrintf( SV_LOG_ERROR, "Unable to issue Consistency tags.\n" ) ;
            }
        }
        if( propsMap.find("PolicyType")->second.compare("VolumeProvisioning") == 0 ||
            propsMap.find("PolicyType")->second.compare("VolumeProvisioningV2") == 0)
        {
            ProcessVolPack(m_policyId);
        }
        if(propsMap.find("PolicyType")->second.compare("Script") == 0)
        {
            executeScript(m_policyId);
        }
        if(propsMap.find("PolicyType")->second.compare("SetupRepository") == 0)
        {
            setupRepository(m_policyId);
        }
		if(propsMap.find("PolicyType")->second.compare("SetupRepositoryV2") == 0)
		{
			setupRepositoryV2(m_policyId) ;
		}
		if( propsMap.find("PolicyType")->second.compare("ExportDevice") == 0 || 
			 propsMap.find("PolicyType")->second.compare("UnExportDevice") == 0  )
		
		{
			ExportUnExportDevice(m_policyId) ;
		}
    }
    else
    {
        SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}
void BulkProtectionController::ProcessMsg(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    if( configuratorPtr->getApplicationPolicies(policyId, "",m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
/*#ifdef SV_WINDOWS
std::string BulkProtectionController::GetSystemDir()
{
	TCHAR szSysDrive[MAX_PATH]; 
	std::string SystemDrive ;
	DWORD dw = GetSystemDirectory(szSysDrive, MAX_PATH);
     if(dw == 0) 
	 { 
		DebugPrintf( SV_LOG_ERROR , "GetSystemDirectory failed with error: %d\n", GetLastError() ) ;
	 }
	 else
	 {
		 SystemDrive = std::string(szSysDrive);
		 size_t pos = SystemDrive.find_first_of(":");
		 if( pos  != std::string::npos )  
		 SystemDrive.assign(SystemDrive,0,pos);
	 }
	 return SystemDrive;
}
#else
std::string BulkProtectionController::GetSystemDir()
{
    return "" ;
}
#endif*/
SVERROR BulkProtectionController::Consistency()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR result = SVS_FALSE;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    SV_ULONG policyId ;
    std::map<std::string, std::string>::iterator policyPropsTempIter = m_policy.m_policyProperties.find("PolicyId");
    std::map<std::string, std::string>::iterator policyPropsEndIter = m_policy.m_policyProperties.end();
    if(policyPropsTempIter != policyPropsEndIter)
    {
        policyId = boost::lexical_cast<SV_ULONG>(policyPropsTempIter->second);
    }
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    std::string outputFileName = getOutputFileName(policyId);
    policyPropsTempIter = m_policy.m_policyProperties.find("DoNotSkip");
    bool doNotSkip = false;
    if(policyPropsTempIter != policyPropsEndIter)
    {
        if(policyPropsTempIter->second.compare("1") == 0)
        {
            doNotSkip = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "DoNotSkip key is not available in the m_policy.m_policyProperties map. This is optional key \n");
    }
    bool bAllInDiffSync = false;
    policyPropsTempIter = m_policy.m_policyProperties.find("ScenarioId");
    std::list<std::string> diffSyncVolumeList;
    if(policyPropsTempIter != policyPropsEndIter)
    {
        bAllInDiffSync = getDiffSyncVolumesList(policyPropsTempIter->second, outputFileName, diffSyncVolumeList);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "ScenarioId key is not available in the m_policy.m_policyProperties map. \n");
    }
    policyPropsTempIter = m_policy.m_policyProperties.find("ConsistencyCmd");
    if(policyPropsTempIter != policyPropsEndIter)
    {
        std::string consistencyCommand;
        consistencyCommand = policyPropsTempIter->second;
        LocalConfigurator configurator ;
        std::string consistencyOptions ;
        consistencyOptions = configurator.getConsistencyOptions() ;
        boost::algorithm::replace_all( consistencyCommand, "-a all", consistencyOptions) ;
        std::string::size_type index = std::string::npos;
        index = consistencyCommand.find("-V ");
        if(index == std::string::npos)
        {
            index = consistencyCommand.find("-v ");
        }
         if(index != std::string::npos)
         {
                if( diffSyncVolumeList.empty() )
                {
                    DebugPrintf(SV_LOG_DEBUG, "No Pair is in diffsync. Can not issue the tag. \n");
                    appConfiguratorPtr->PolicyUpdate(policyId,instanceId, "Skipped", "");
                    consistencyCommand = ""    ;         
                }
                else if(doNotSkip )
                {
                    index += 3;
                    std::string leftString, rightString;
                    leftString = consistencyCommand.substr(0, index);
                    rightString = consistencyCommand.substr(index);
                    trimSpaces(rightString);
                    index = rightString.find("\"") ;
                    if(index != std::string::npos)
                    {
                        rightString = rightString.substr(index+1);
						index = rightString.find("\"") ;
						if (index != std::string::npos)
						{
							rightString = rightString.substr(index+1);
						}
						trimSpaces(rightString);
                    }
                    else
                    {
                        rightString = "";
                    }
                    std::string volumeCmd = "\"";
                    std::list<std::string>::iterator diffSyncVolumeListBegIter = diffSyncVolumeList.begin(); 
                    std::list<std::string>::iterator diffSyncVolumeListEndIter = diffSyncVolumeList.end();
                    while(diffSyncVolumeListBegIter != diffSyncVolumeListEndIter)
                    {
                        std::string volumeName = *(diffSyncVolumeListBegIter);
                        volumeCmd += *(diffSyncVolumeListBegIter);
                        if(volumeName.size() == 1)
                        {
                            volumeCmd += ":";
                        }
                        diffSyncVolumeListBegIter++;
                        if(diffSyncVolumeListBegIter != diffSyncVolumeListEndIter)
                        {
                            volumeCmd += ",";
                        }
                    }
                    volumeCmd += "\"";
                    consistencyCommand = leftString + " " + volumeCmd + " " + rightString ;
                    					DebugPrintf (SV_LOG_DEBUG,"leftString is %s \n",leftString.c_str());
					DebugPrintf (SV_LOG_DEBUG,"volumeCmd is %s \n",volumeCmd.c_str());
					DebugPrintf (SV_LOG_DEBUG,"rightString is %s \n",rightString.c_str());
#ifdef SV_WINDOWS
                    std::string systemDrive = GetSystemDir() ;
                    if( std::find( diffSyncVolumeList.begin(), diffSyncVolumeList.end(), systemDrive )
                                    == diffSyncVolumeList.end() )
                    {
                        boost::algorithm::replace_all( consistencyCommand, "-a sr", "") ;
                    }
#endif
                }                
                if(consistencyCommand.empty() == false)
                {
                    TagGenerator obj(consistencyCommand, 600);
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
							std::stringstream stream ;
                            FormatVacpErrorCode( stream, exitCode ) ;
                            appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",obj.stdOut(), stream.str());
                            result = SVS_OK;
                        }
                    }
                    else
                    {
                        appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"Failed",std::string("Failed to spawn vacp Process.Please check Command"));
                    }		
                }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "-V option is not found in the consistency command. Command: %s \n", consistencyCommand.c_str());
            appConfiguratorPtr->PolicyUpdate(policyId,instanceId, "Skipped", "-V option is not found in the consistency command.");            
        }
        DebugPrintf (SV_LOG_DEBUG , "final consistencyCommand is %s \n ",consistencyCommand.c_str () );
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "ConsistencyCmd key is not available in the m_policy.m_policyProperties map. \n");
        appConfiguratorPtr->PolicyUpdate(policyId, instanceId, "Failed", "ConsistencyCmd is not present in the policy");
    }
    

    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return result;
}
bool BulkProtectionController::IsItActiveNode()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bIsActive = true;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bIsActive;
}

void BulkProtectionController::MakeAppServicesAutoStart()
{

}

void BulkProtectionController::PreClusterReconstruction()
{
}
bool BulkProtectionController::StartAppServices()
{
    return true ;
}

bool BulkProtectionController::InitializeConfig(ConfigSource configSrc)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false;
	try
	{
		if(m_pConfig)
		{
			bRet = true;
		}
		else if(InitializeConfigurator(&m_pConfig,configSrc,PHPSerialize))
		{
			m_pConfig->Start();
			bRet = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize configurator\n");
			m_pConfig = NULL;
		}
	}
	catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR," encountered exception. %s\n",ce.what());
    }
	catch( std::exception const& e )
    {
        DebugPrintf(SV_LOG_ERROR," encountered exception. %s\n", e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR," encountered unknown exception.\n");
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

std::string BulkProtectionController::GetRetentionDbPath(std::string volumeName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string strRetentionDbPath;
	try
	{
		if(m_pConfig)
		{
#ifdef SV_WINDOWS
			FormatVolumeNameForCxReporting(volumeName);
            volumeName[0]=toupper(volumeName[0]);
#endif
			strRetentionDbPath = m_pConfig->getCdpDbName(volumeName);
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Configurator not initialized\n");
		}
	}
	catch ( ... )
    {
		DebugPrintf(SV_LOG_ERROR," encountered unknown exception. Could not get retention DB path for %s\n",volumeName.c_str());
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return strRetentionDbPath;
}

void BulkProtectionController::UninitializeConfig()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	try
	{
		if(m_pConfig)
		{
			m_pConfig->Stop();
			m_pConfig = NULL;
		}
	}
	catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR," encountered exception. %s\n",ce.what());
    }
	catch( std::exception const& e )
    {
        DebugPrintf(SV_LOG_ERROR," encountered exception. %s\n", e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR," encountered unknown exception.\n");
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void BulkProtectionController::PrePareFailoverJobs()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppLocalConfigurator localConfigurator;
	FailoverJob failoverJob;
	FailoverCommand failoverCmd;
	if(m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0
        //m_FailoverInfo.m_PolicyType.compare("DRServerFastFailback") == 0
		)
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
				DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
            }
            //Build Failover/Failback command
			std::string failoverCmdStr;
			std::map<std::string,std::string>::iterator iterPolicyProp = 
				m_policy.m_policyProperties.find("ConsistencyCmd");
			if(m_policy.m_policyProperties.end() != iterPolicyProp)
			{
				failoverCmdStr = iterPolicyProp->second;
				TagGenerator tg;
				tg.sanitizeVacpCommmand(failoverCmdStr);
				tg.verifyVacpCommand(failoverCmdStr);
				failoverCmdStr = "\"" + tg.getVacpPath() + "\" " + failoverCmdStr +
					" -t "+ m_FailoverInfo.m_TagName;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Could not find the Consistency command for the policy. \
										 Can not construct failover command\n");
				failoverCmd.m_Command = "Warning: Command not available. Failover will fail";
			}
            failoverCmd.m_Command = failoverCmdStr;
            failoverCmd.m_GroupId = ISSUE_CONSISTENCY;
            failoverCmd.m_StepId = 1;
            failoverJob.m_FailoverCommands.push_back(failoverCmd);
			DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());

            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_SOURCE;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
				DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
            }
        }
    }
    else if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0
        //m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0
		)
    {
        if(m_FailoverInfo.m_RecoveryExecutor.compare("Custom") == 0)
        {
            if(m_FailoverInfo.m_Customscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Customscript;
                failoverCmd.m_GroupId = CUSTOM_TARGET_SCRIPT;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
				DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
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
				DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
            }

			//Buil failover/failback command
			std::string cdpcliPath = localConfigurator.getCdpcliExePathname();
			cdpcliPath = "\""+cdpcliPath+"\"";
			if(  m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
                (m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0 && m_policy.m_policyProperties["SolutionType"].compare("ARRAY") == 0 )
              )
			{
				failoverCmd.m_GroupId = ROLLBACK_VOLUMES;
				failoverCmd.m_StepId = 1;

				if(m_FailoverInfo.m_RecoveryPointType.compare("CUSTOM") == 0)
				{
					RecoveryPoints recoveryPoints;
                    std::string volumeName,recoveryPointType,tagName,timeStamp,tagType;

                    std::map<std::string, RecoveryPoints>::iterator RecoverypointMapIter = m_FailoverData.m_recoveryPoints.begin();
                    while(RecoverypointMapIter != m_FailoverData.m_recoveryPoints.end())
                    {
                        recoveryPoints = RecoverypointMapIter->second;
                        volumeName = RecoverypointMapIter->first;

						std::string failoverCmdStr = cdpcliPath + " --rollback --dest=";
#ifdef SV_WINDOWS
						failoverCmdStr += "\"";
#endif
						failoverCmdStr += volumeName;
#ifdef SV_WINDOWS
						failoverCmdStr += "\"";
#endif

                        GetValueFromMap(recoveryPoints.m_properties,"RecoveryPointType",recoveryPointType);
						if(recoveryPointType.compare("TAG") == 0)
						{
							GetValueFromMap(recoveryPoints.m_properties,"TagName",tagName);
							if(!tagName.empty())
							{
								failoverCmdStr += " --event=" + tagName;
								failoverCmdStr += " --picklatest";
								GetValueFromMap(recoveryPoints.m_properties,"TagType",tagType);
								if(!tagType.empty())
									failoverCmdStr += " --app=" + tagType;

								failoverCmdStr += " --deleteretentionlog=yes";
							}
						}
						else if(recoveryPointType.compare("TIME") == 0)
						{
							GetValueFromMap(recoveryPoints.m_properties,"Timestamp",timeStamp);
							if(!timeStamp.empty())
								failoverCmdStr += " --at=\"" + timeStamp + "\"";

							failoverCmdStr += " --deleteretentionlog=yes";
						}
                        else if( recoveryPointType.compare("LCCP") == 0)
                        {
                            failoverCmdStr += " --recentfsconsistentpoint";
                        }
                        else if( recoveryPointType.compare("LCT") == 0)
                        {
                            failoverCmdStr += " --recentcrashconsistentpoint";
                        }
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Recovery point type not available.\
													 Can not construct rollback command for %s\n",volumeName.c_str());
							failoverCmdStr = "WARNING: Recovery point type not available for " + volumeName + ". Roll back will fail.";
						}

						failoverCmd.m_Command = failoverCmdStr;
						failoverJob.m_FailoverCommands.push_back(failoverCmd);
						DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());

                        RecoverypointMapIter++;
                    }
				}
				else
				{
					std::string failoverCmdStr = cdpcliPath + " --rollback --rollbackpairs=";

					failoverCmdStr += "\"";

					std::list<std::string>::const_iterator iterVols = m_AppProtectionSettings.appVolumes.begin();
					while(iterVols != m_AppProtectionSettings.appVolumes.end())
					{
						failoverCmdStr += *iterVols;
						iterVols++;
						if(iterVols != m_AppProtectionSettings.appVolumes.end())
							failoverCmdStr += ";";
					}
					iterVols = m_AppProtectionSettings.CustomVolume.begin();
					while(iterVols != m_AppProtectionSettings.CustomVolume.end())
					{
						failoverCmdStr += *iterVols ;
						iterVols++;
						if(iterVols != m_AppProtectionSettings.CustomVolume.end())
							failoverCmdStr += ";";
					}

					failoverCmdStr += "\"";

					if(m_FailoverInfo.m_RecoveryPointType.compare("LCCP") == 0)
					{
						failoverCmdStr += " --recentfsconsistentpoint";
					}
					else if(m_FailoverInfo.m_RecoveryPointType.compare("LCT") == 0)
					{
						failoverCmdStr += " --recentcrashconsistentpoint";
					}
					failoverCmdStr += " --deleteretentionlog=yes";

					failoverCmd.m_Command = failoverCmdStr;
					failoverJob.m_FailoverCommands.push_back(failoverCmd);
					DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
				}
			}
			else
			{
				std::string failoverCmdStr = cdpcliPath + " --rollback --rollbackpairs=";

				failoverCmdStr += "\"";

				std::list<std::string>::const_iterator iterVols = m_AppProtectionSettings.appVolumes.begin();
				while(iterVols != m_AppProtectionSettings.appVolumes.end())
				{
					failoverCmdStr += *iterVols;
					iterVols++;
					if(iterVols != m_AppProtectionSettings.appVolumes.end())
						failoverCmdStr += ";";
				}
				iterVols = m_AppProtectionSettings.CustomVolume.begin();
				while(iterVols != m_AppProtectionSettings.CustomVolume.end())
				{
					failoverCmdStr += *iterVols;
					iterVols++;
					if(iterVols != m_AppProtectionSettings.CustomVolume.end())
						failoverCmdStr += ";";
				}

				failoverCmdStr += "\"";

				failoverCmdStr += " --app=USERDEFINED";

				failoverCmdStr += " --event=" + m_FailoverInfo.m_TagName;

				failoverCmdStr += " --picklatest";
				
				failoverCmdStr += " --deleteretentionlog=yes";

				failoverCmd.m_Command = failoverCmdStr;
				failoverCmd.m_GroupId = ROLLBACK_VOLUMES;
				failoverCmd.m_StepId = 1;
				failoverJob.m_FailoverCommands.push_back(failoverCmd);
				DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
			}

            if(m_FailoverInfo.m_Postscript.empty() == false)
            {
                failoverCmd.m_Command = m_FailoverInfo.m_Postscript;
                failoverCmd.m_GroupId = POSTSCRIPT_TARGET;
                failoverCmd.m_StepId = 1;
                failoverJob.m_FailoverCommands.push_back(failoverCmd);
				DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",failoverCmd.m_Command.c_str());
            }
        }
    }
    AddFailoverJob(m_policyId,failoverJob);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR BulkProtectionController::ProcessVolPack(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR svRet = SVS_FALSE;
    ApplicationPolicy policy ;
    VolPacks objvolPack;
    std::string strOutPutLog;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    if(!schedulerPtr->isPolicyEnabled(policyId) )
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy %ld not enabled\n",policyId) ;
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
    else
    {
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
        AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
        SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
        configuratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","");

        if(!configuratorPtr->getApplicationPolicies(policyId, "", policy))
        {
            DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
            schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
        }
        else
        {
            try
            {
                objvolPack = unmarshal<VolPacks>(policy.m_policyData);
                std::map<std::string, VolPackAction>& policyPropertyMap = objvolPack.m_volPackActions;
                AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
                schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
                VolPacks objvolPack;
                objvolPack = unmarshal<VolPacks>(policy.m_policyData);
                std::map<std::string, VolPackAction> volpackActions = objvolPack.m_volPackActions;
                std::map<std::string, VolPackAction>::iterator iterPropsMap = volpackActions.begin();
                std::map<std::string,std::string> mountPointToDeviceFileMap;
                SV_ULONG exitCode = -1 ;
                bool isCreateVolpackAvailable = false;
                while(iterPropsMap != volpackActions.end())
                {
				    exitCode = -1 ;
                    VolPack objVolPack(iterPropsMap->first, iterPropsMap->second.m_size, 1000);
                    if(iterPropsMap->second.m_action == VOLPACK_CREATE)
                    {
                        isCreateVolpackAvailable = true;
                        if(createVolPack(policyId, objVolPack, mountPointToDeviceFileMap, exitCode) == SVS_OK)
                        {
                            DebugPrintf(SV_LOG_DEBUG,"Successfully Created the volpacks\n");
                            svRet = SVS_OK;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to Create the volpacks\n");
                        }
                    }
                    else if(iterPropsMap->second.m_action == VOLPACK_DELETE)
                    {
                        if(deleteVolPack(policyId, objVolPack, exitCode) == SVS_OK)
                        {
                            DebugPrintf(SV_LOG_DEBUG,"Successfully deleted the volpacks\n");
                            svRet = SVS_OK;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to delete the volpacks\n");
                        }
                    }
                    else if(iterPropsMap->second.m_action == VOLPACK_RESIZE)
                    {
                        if(resizeVolPack(policyId, objVolPack, exitCode) == SVS_OK)
                        {
                            DebugPrintf(SV_LOG_DEBUG,"Successfully deleted the volpacks\n");
                            svRet = SVS_OK;
                        }
                else
                {
                            DebugPrintf(SV_LOG_ERROR,"Failed to delete the volpacks\n");
                }
            }
            else
            {
					    DebugPrintf(SV_LOG_ERROR, "Invalid Action received.\n");
					    strOutPutLog = "Invalid Action received."; 
					    break ;
                    }
                    if( exitCode != 0 )
                    {
                        break ;
                    }
                    iterPropsMap++;
                }
                SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
                std::string volPackUpdateStr = "";
                if(mountPointToDeviceFileMap.empty() == false)
                {
                    VolPackUpdate volPackUpdateObj;
                    volPackUpdateObj.m_volPackMountInfo = mountPointToDeviceFileMap;
                    try
                    {
                        std::stringstream volPackUpdateStream;
                        volPackUpdateStream << CxArgObj<VolPackUpdate>(volPackUpdateObj);
                        volPackUpdateStr = volPackUpdateStream.str();
                        DebugPrintf(SV_LOG_DEBUG, "VolPackUpdate String %s .\n", volPackUpdateStr.c_str());
            }
                    catch(AppException& exception) 
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable marshal the volPackUpdate Obj %s\n", exception.to_string().c_str());
                    }
                    catch(std::exception& ex)
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to marshal the volPackUpdate Obj %s\n", ex.what());
                    }
                    catch(...) 
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to marshal the volPackUpdate Obj.\n");
                    }
                } 
                if(isCreateVolpackAvailable)
                {
                    if(exitCode == 0)
                    {
                        configuratorPtr->PolicyUpdate(policyId, instanceId, "Success", "", volPackUpdateStr);
                        svRet = SVS_OK;
        }
        else
        {
                        configuratorPtr->PolicyUpdate(policyId, instanceId, "Failed", "", volPackUpdateStr);
        }
    }
    else
    {
                    if(exitCode == 0)
                    {
                        configuratorPtr->PolicyUpdate(policyId,instanceId, "Success", strOutPutLog);
                        svRet = SVS_OK;
                    }
                    else
                    {
                        configuratorPtr->PolicyUpdate(policyId,instanceId, "Failed", strOutPutLog);
                    }				
                }
                schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
            }
            catch(AppException& exception) 
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal volpack policy %s\n", exception.to_string().c_str()) ;
			    strOutPutLog = "Failed to unmarshal volpack policy";
			    strOutPutLog += exception.to_string();
			    configuratorPtr->PolicyUpdate(policyId, instanceId, "Failed", strOutPutLog);
            }
            catch(std::exception ex)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal volpack %s\n", ex.what()) ;
			    strOutPutLog = "Failed to unmarshal volpack policy";
			    strOutPutLog += ex.what();
			    configuratorPtr->PolicyUpdate(policyId, instanceId, "Failed", strOutPutLog);			
            }
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    }
    return svRet;
}

SVERROR BulkProtectionController::createVolPack(SV_ULONG policyId, VolPack objVolPack, std::map<std::string,std::string>& mountPointToDeviceFileMap, SV_ULONG& exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR svRet = SVS_FALSE;
    exitCode = 1;
    std::string outputFileName = getOutputFileName(policyId);	
    if(objVolPack.volpackProvision(outputFileName, exitCode) == true)
    {
		svRet = SVS_OK;
	}
    //for valpack update........
#ifdef SV_WINDOWS
    mountPointToDeviceFileMap.insert(std::make_pair(objVolPack.getMountPointName(), objVolPack.getFinalMountPoint()));
#else
    mountPointToDeviceFileMap.insert(std::make_pair(objVolPack.getMountPointName(), objVolPack.m_MountPointName + "_sparsefile"));
#endif
    m_volpackIndex++ ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return svRet;
}

SVERROR BulkProtectionController::deleteVolPack(SV_ULONG policyId, VolPack& objVolPack, SV_ULONG& exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR svRet = SVS_FALSE;
	std::string strOutPutLog;
    std::string outputFileName = getOutputFileName(policyId);	
    if(objVolPack.volpackDeletion(outputFileName,exitCode) == true)
    {
        svRet = SVS_OK;
    }
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return svRet;
}

SVERROR BulkProtectionController::resizeVolPack(SV_ULONG policyId, VolPack& objVolPack, SV_ULONG& exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR svRet = SVS_FALSE;
	std::string outputFileName = getOutputFileName(policyId);	
    if(objVolPack.volpackResize(outputFileName,exitCode) == true)
    {
		svRet = SVS_OK;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return svRet;
}

SVERROR BulkProtectionController::executeScript(const SV_ULONG& policyId )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;

    std::string outputFileName = getOutputFileName(policyId);
    ACE_OS::unlink(outputFileName.c_str()) ;			
    std::stringstream errorStream;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    configuratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","");
    ApplicationPolicy appPolicy ;
    SV_ULONG exitCode = 1 ;	
    if(configuratorPtr->getApplicationPolicies(policyId, "", appPolicy) == false )
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId: %ld\n", policyId) ;
        errorStream << "There are no policies available for policyId: " << policyId << "of PolicyType: script" << std::endl;
    }
    else
	{
		try
		{
			std::string scriptPolicyData = appPolicy.m_policyData;
			ScriptPolicyDetails scriptDetails = unmarshal<ScriptPolicyDetails>(scriptPolicyData) ;
			std::map<std::string, std::string>::iterator policyParmIter;
			policyParmIter = scriptDetails.m_Prpertires.find("CreateConfFile");
			if(policyParmIter != scriptDetails.m_Prpertires.end())
			{
				std::string appName = "UnKnown";
				if(appPolicy.m_policyProperties.find("ApplicationType") != appPolicy.m_policyProperties.end())
				{
					appName = appPolicy.m_policyProperties.find("ApplicationType")->second;
				}
				if( policyParmIter->second.compare("1") == 0 )
				{
					createConfFile(scriptDetails, appName);
				}
			}
			policyParmIter = scriptDetails.m_Prpertires.find("ScriptCmd");
			if( policyParmIter != scriptDetails.m_Prpertires.end() )
			{
				std::string commandNameToExecute = policyParmIter->second;
				DebugPrintf(SV_LOG_INFO,"The Script command to execute : %s\n",commandNameToExecute.c_str());

				AppCommand appCmd(commandNameToExecute.c_str(), 0, outputFileName);
				std::string commandOutPut;
                void *h = 0;
#ifdef SV_WINDOWS
                h = Controller::getInstance()->getDuplicateUserHandle();
#endif
				if(appCmd.Run(exitCode, commandOutPut, Controller::getInstance()->m_bActive, "", h ) != SVS_OK)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to spawn Script command.\n");
					errorStream << "Failed to spawn Script command" << " Command: " << commandNameToExecute << " Exit Code: " << exitCode << std::endl;
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Script ran Successfully. \n");
					retStatus = SVS_OK;
				}
#ifdef SV_WINDOWS
                if(h)
				{
                    CloseHandle(h);
				}
#endif
				DebugPrintf(SV_LOG_INFO,"\n The exit code is : %d", exitCode);
			}
			else
			{
				errorStream << "Script details are not provided" << std::endl;
				DebugPrintf(SV_LOG_ERROR, "ScriptCmd parameter is not found in the policy. \n");
			}
			std::ofstream os;
			os.open( outputFileName.c_str(), std::ofstream::app );
			if( os.is_open() && os.good() )
			{
				os << errorStream.str();
				os.flush();
				os.close();
			}	
		}
		catch(AppException& ex)
		{
			DebugPrintf(SV_LOG_ERROR, "Got Unmarshal Exception: %ld ,  %s\n", policyId, ex.to_string().c_str()); 
		}
		catch(std::exception& ex1)
		{
			DebugPrintf(SV_LOG_ERROR, "Got Unmarshal exception: %ld, %s\n", policyId, ex1.what()); 
		}
	}
    if( exitCode != 0 )
    {
		configuratorPtr->PolicyUpdate(policyId,instanceId,"Failed","");
	}
	else
	{
		configuratorPtr->PolicyUpdate(policyId,instanceId,"Success","");
	}
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}

SVERROR BulkProtectionController::createConfFile(ScriptPolicyDetails policyDetails, std::string appName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK;
	std::stringstream writeStream;
	boost::algorithm::to_upper(appName) ;
	writeStream << "[" << appName << "VSNAP" << "]" << std::endl;
	std::map<std::string, std::string>::iterator vsnapDetailsIter = policyDetails.m_Prpertires.begin();
	while(vsnapDetailsIter != policyDetails.m_Prpertires.end())
	{
		if( strcmpi(vsnapDetailsIter->first.c_str(), "CreateConfFile") != 0 && 
			strcmpi(vsnapDetailsIter->first.c_str(), "ScriptCmd") != 0 )
		{
			writeStream << vsnapDetailsIter->first << "=" << vsnapDetailsIter->second << std::endl;
		}
		vsnapDetailsIter++;
	}
	std::map<std::string, VSnapDetails>::iterator vSnapDetailsIter = policyDetails.m_vSnapDetails.begin();
	
	std::string VolumeVsnapPath = "", RecoveryTag = "", RecoveryTime = "", VsnapFlags = "";
	std::map<std::string, std::string>::iterator vsnapDetailsMapTempIter;
	std::map<std::string, std::string>::iterator vsnapDetailsMapEndIter;
	while(vSnapDetailsIter != policyDetails.m_vSnapDetails.end())
	{
		vsnapDetailsMapEndIter =  vSnapDetailsIter->second.m_vsnapDetailsMap.end();
		vsnapDetailsMapTempIter = vSnapDetailsIter->second.m_vsnapDetailsMap.find("VsnapPath");
		if( vsnapDetailsMapTempIter != vsnapDetailsMapEndIter)
		{
			if(!vsnapDetailsMapTempIter->second.empty())
			{
				if(!VolumeVsnapPath.empty())
				{
					VolumeVsnapPath += ",";
				}
				VolumeVsnapPath += vSnapDetailsIter->first;
				VolumeVsnapPath += "=";
				VolumeVsnapPath += vsnapDetailsMapTempIter->second;	
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "VsnapPath key is not found for the drive: %s", vSnapDetailsIter->first.c_str());
		}
		vsnapDetailsMapTempIter = vSnapDetailsIter->second.m_vsnapDetailsMap.find("RecoverTagName");
		if( vsnapDetailsMapTempIter != vsnapDetailsMapEndIter)
		{
			if(!vsnapDetailsMapTempIter->second.empty())
			{
				if(!RecoveryTag.empty())
				{
					RecoveryTag += ",";
				}
				RecoveryTag += vSnapDetailsIter->first;
				RecoveryTag += "=";
				RecoveryTag += vsnapDetailsMapTempIter->second;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "RecoverTagName key is not specified for the drive: %s", vSnapDetailsIter->first.c_str());
		}
		vsnapDetailsMapTempIter = vSnapDetailsIter->second.m_vsnapDetailsMap.find("RecoveryTime");
		if( vsnapDetailsMapTempIter != vsnapDetailsMapEndIter)
		{
			if(!vsnapDetailsMapTempIter->second.empty())
			{
				if(!RecoveryTime.empty())
				{
					RecoveryTime += ",";
				}
				RecoveryTime += vSnapDetailsIter->first;
				RecoveryTime += "=";
				RecoveryTime += vsnapDetailsMapTempIter->second;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "RecoveryTime key is not specified for the drive: %s", vSnapDetailsIter->first.c_str());
		}
		
		vsnapDetailsMapTempIter = vSnapDetailsIter->second.m_vsnapDetailsMap.find("VsnapFlag");
		if( vsnapDetailsMapTempIter != vsnapDetailsMapEndIter)
		{
			if(!vsnapDetailsMapTempIter->second.empty())
			{
				if(!VsnapFlags.empty())
				{
					VsnapFlags += ",";
				}
				VsnapFlags += vSnapDetailsIter->first;
				VsnapFlags += "=";
				VsnapFlags += vsnapDetailsMapTempIter->second;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "VsnapFlag key is not specified for the drive: %s", vSnapDetailsIter->first.c_str());
		}
		vSnapDetailsIter++;
	}
	writeStream << "VolumeVsnapPath" << "=" << VolumeVsnapPath <<std::endl;
	writeStream << "RecoveryTag" << "=" << RecoveryTag << std::endl;
	writeStream << "RecoveryTime" << "=" << RecoveryTime <<std::endl;
	writeStream << "VsnapFlags" << "=" << VsnapFlags << std::endl;
	AppLocalConfigurator localconfigurator ;
	std::string filePath = localconfigurator.getInstallPath();
	if(filePath.find_last_of("\\/") != filePath.size() - 1)
	{
		filePath += ACE_DIRECTORY_SEPARATOR_CHAR;
	}
	filePath += "Failover";
	filePath += ACE_DIRECTORY_SEPARATOR_CHAR;
	if(SVMakeSureDirectoryPathExists(filePath.c_str()).failed())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to create directory path %s",filePath.c_str());
	}

	std::string sourceServerName = "";
	if(policyDetails.m_Prpertires.find("SourceVirtualServerName") != policyDetails.m_Prpertires.end())
	{
		sourceServerName = policyDetails.m_Prpertires.find("SourceVirtualServerName")->second;
		trimSpaces(sourceServerName);
	}
	if(sourceServerName.empty() && policyDetails.m_Prpertires.find("SourceServerName") != policyDetails.m_Prpertires.end())
	{
		sourceServerName = policyDetails.m_Prpertires.find("SourceServerName")->second;
	}
	
	filePath += sourceServerName;
	filePath += "_" + appName + "_vsnap_config.conf";
	DebugPrintf(SV_LOG_DEBUG, "ConfigFile Path: %s \n", filePath.c_str());
	std::ofstream os;
	os.open(filePath.c_str(), std::ofstream::trunc);
	if( os.is_open() && os.good() )
	{
		os << writeStream.str();
		os.flush();
		os.close();
	}
	else
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", filePath.c_str() );
		retStatus = SVS_FALSE;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;	
}

SVERROR BulkProtectionController::setupRepositoryV2(SV_ULONG policyId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
	ApplicationPolicy policy ;
    configuratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","");
	if(!configuratorPtr->getApplicationPolicies(policyId, "", policy))
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
	else
	{
        std::string outputFileName = getOutputFileName(policyId);
        ACE_OS::unlink(outputFileName.c_str()) ;
        try
        {
			DebugPrintf(SV_LOG_ERROR, "before unmarshal\n") ;
            SetupRepositoryDetails setupRepoDetails = unmarshal<SetupRepositoryDetails>(policy.m_policyData);
			DebugPrintf(SV_LOG_ERROR, "after unmarshal\n") ;
			SetupRepositoryStatus repositoryStatus ;
			StorageRepositoryObjPtr storageRepoPtr = StorageRepository::getSetupRepoObjV2(outputFileName) ;
			std::stringstream repoStatusStr ;
			if(storageRepoPtr->setupRepositoryV2(policy, repositoryStatus) == SVS_OK)
			{
				DebugPrintf(SV_LOG_DEBUG, "Repository was setup successfully\n") ;
	            repoStatusStr << CxArgObj<SetupRepositoryStatus>(repositoryStatus);
            	configuratorPtr->PolicyUpdate(policyId, instanceId, "Success", "", repoStatusStr.str()) ;
				bRet = SVS_OK ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to setup the repository\n") ;
                configuratorPtr->PolicyUpdate(policyId, instanceId, "Failed", "", repoStatusStr.str());
			}
		}
		catch(ContextualException& ex)
		{
			DebugPrintf(SV_LOG_ERROR, "Unmarshal error while finding the repository details: %s\n", ex.what()) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}
SVERROR BulkProtectionController::setupRepository(SV_ULONG policyId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	ApplicationPolicy policy ;
    std::string strOutPutLog;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    configuratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","");
    if(!configuratorPtr->getApplicationPolicies(policyId, "", policy))
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
    else
    {
		std::string outputFileName = getOutputFileName(policyId);
		ACE_OS::unlink(outputFileName.c_str()) ;
        try
        {
			bool bAtleastOnefail = false;
			StorageReposUpdate updateStruct;			
			StorageRepositories setupRepos = unmarshal<StorageRepositories>(policy.m_policyData);
			std::list<StorageRepositoryStr>::iterator setupReposBegIter = setupRepos.m_repositories.begin();
			std::list<StorageRepositoryStr>::iterator setupReposEndIter = setupRepos.m_repositories.end();
			std::list<StorageRepositoryStr>::iterator setupReposTempIter = setupReposEndIter;
			while(setupReposBegIter != setupReposEndIter)
			{
				try
				{			
					StorageRepositoryObjPtr storageRepositoryObj = StorageRepository::getSetupRepoObj(setupReposBegIter->m_properties, outputFileName);
					if(storageRepositoryObj->setupRepository() != SVS_OK)
					{
						bAtleastOnefail = true;
					}
					updateStruct.m_setupRepoUpdate.insert(std::make_pair<std::string>(storageRepositoryObj->getDeviceNamePath(), storageRepositoryObj->getStatusMessage()));
				}
				catch(AppException& ex)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to process %ld %s\n", policyId, ex.to_string().c_str());
                    std::string devName = "Not Found";
					if(setupReposBegIter->m_properties.find("DeviceName") != setupReposBegIter->m_properties.end())
					{					
   					    devName = setupReposBegIter->m_properties.find("DeviceName")->second;
					    updateStruct.m_setupRepoUpdate.insert(std::make_pair(devName, ex.to_string()));
                    }
                    bAtleastOnefail = true;
				}
				catch(std::exception ex)
				{
					DebugPrintf(SV_LOG_ERROR, "setupRepository failed %s\n", ex.what()) ;
                    std::string devName = "Not Found";
					if(setupReposBegIter->m_properties.find("DeviceName") != setupReposBegIter->m_properties.end())
					{					
					    devName = setupReposBegIter->m_properties.find("DeviceName")->second;
					    updateStruct.m_setupRepoUpdate.insert(std::make_pair(devName, ex.what()));	
                    }
                    bAtleastOnefail = true;
				}	
				setupReposBegIter++;
			}
			std::stringstream updateStr;
			updateStr << CxArgObj<StorageReposUpdate>(updateStruct);
			if(!bAtleastOnefail)
			{
				configuratorPtr->PolicyUpdate(policyId, instanceId, "Success", "", updateStr.str());
			}
			else
			{
				retStatus = SVS_OK;
				configuratorPtr->PolicyUpdate(policyId, instanceId, "Failed", "", updateStr.str());
			}			
		}
        catch(AppException& exception) 
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal setupRepository policy %s\n", exception.to_string().c_str()) ;
        }
        catch(std::exception ex)
{
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal setupRepository %s\n", ex.what()) ;
        }
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;	
}

void ExportUnExportDevice(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string strOutPutLog;
    ApplicationPolicy policy ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    if(!configuratorPtr->getApplicationPolicies(policyId, "", policy))
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
}

    else
    {
		bool validsettings = false ;
       	DeviceExportPolicyData policyData ;
		try
		{
        	policyData = unmarshal<DeviceExportPolicyData>(policy.m_policyData);
			validsettings = true ;
		}
		catch(ContextualException& ce)
		{
			DebugPrintf(SV_LOG_ERROR, "Exception while unmarshalling the DeviceExportPolicyData %s\n", ce.what()) ;
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR, "Unknown exception while unmarshalling\n") ;
		}
       	bool exportStatus = true ;
        std::stringstream statusMsg ;
		DebugPrintf(SV_LOG_DEBUG, "No of export operations %d\n", policyData.m_ExportDevices.size()) ;
		if( validsettings )
		{
        	configuratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","");
	        std::map<std::string, DeviceExportParams>::iterator devExportParamBeg ;
    	    std::map<std::string, DeviceExportParams>::iterator devExportParamEnd ;
    	    devExportParamBeg = policyData.m_ExportDevices.begin() ;
        	devExportParamEnd = policyData.m_ExportDevices.end() ;
    	    while( devExportParamBeg != devExportParamEnd )
	        {
    	        std::string DeviceName = devExportParamBeg->first ;
				DebugPrintf(SV_LOG_DEBUG, "Device Name %s is being exported using CIFS\n", DeviceName.c_str()) ;
	            DeviceExportParams& exportParams = devExportParamBeg->second ;
    	        if( exportParams.m_Exportparams.find("ExportType") != exportParams.m_Exportparams.end() )
        	    {
            	    std::string exportType = exportParams.m_Exportparams.find("ExportType")->second ;
            	    if( exportType.compare("ISCSI") == 0 )
                	{
	                    if( !ExportDeviceUsingIscsi( DeviceName, exportParams, statusMsg ) )
        	                exportStatus = false ;
                	}
	                else if( exportType.compare("CIFS") == 0 )
    	            {
        	            if( !ExportDeviceUsingCIFS(DeviceName, exportParams,statusMsg) )
        	                exportStatus = false ;
    	            }
					else if( exportType.compare("NFS") == 0 )
					{
						if( !ExportDeviceUsingNFS(DeviceName, exportParams, statusMsg) )
							exportStatus = false ;
					}
	            }
				devExportParamBeg++ ;
        	}
		}
		if( exportStatus && validsettings )
        {
			configuratorPtr->PolicyUpdate(policyId, instanceId, "Success", statusMsg.str()) ;
        }
		else
        {
			configuratorPtr->PolicyUpdate(policyId, instanceId, "Failed", statusMsg.str()) ;
		}
    	schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
