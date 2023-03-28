#pragma warning (disable: 4005) // macro redefinition

#include "cloudcontroller.h"
#include "controller.h"
#include "appcommand.h"
#include "Consistency/TagGenerator.h"
#include "HttpFileTransfer.h"
#include "inmalertdefs.h"
#include "ErrorLogger.h"
#ifndef WIN32
#include "deviceidinformer.h"
#include "cdputil.h"
#include "executecommand.h"

const std::string DEVICE_REFRESH_SCRIPT = "/scripts/inm_device_refresh.sh";
const std::string LSSCSI_SCRIPT = "/usr/bin/lsscsi";
#else
#include "../win32/system.h"
#endif

#include "TagTelemetry.h"
#include <boost/algorithm/string.hpp>
#include "VacpErrorCodes.h"
#include "LogCutter.h"

#include "MigrationHelper.h"

using namespace TagTelemetry;
using namespace Logger;

Log gTagTelemetryLog;

CloudControllerPtr CloudController::m_CldControllerPtr;

extern bool GetIPAddressSetFromNicInfo(strset_t &ips, std::string &errMsg);


CloudController::CloudController(ACE_Thread_Manager * tm)
:AppController(tm)
{
	//CloudController specific initializetion
	m_pConfig = NULL;
	//PolicyType and its handler routine mappings
	m_CldPolicyHandlers["PrepareTarget"] = &CloudController::OnPrepareTarget;
	m_CldPolicyHandlers["CloudDrDrill"]  = &CloudController::OnDrDrill;
	m_CldPolicyHandlers["CloudRecovery"] = &CloudController::OnRecover;
	m_CldPolicyHandlers["Consistency"] = &CloudController::OnConsistency;

	// Handlers for CS to RCM migration
	m_CldPolicyHandlers["RcmRegistration"] = &CloudController::OnRcmRegistration;
	m_CldPolicyHandlers["RcmFinalReplicationCycle"] = &CloudController::OnRcmFinalReplicationCycle;
	m_CldPolicyHandlers["RcmResumeReplication"] = &CloudController::OnRcmResumReplication;
	m_CldPolicyHandlers["RcmMigration"]   = &CloudController::OnRcmMigration;
}

CloudControllerPtr CloudController::getInstance(ACE_Thread_Manager *tm)
{
	if( m_CldControllerPtr.get() == NULL)
	{
		m_CldControllerPtr.reset(new CloudController(tm));
		if(m_CldControllerPtr.get() != NULL)
			m_CldControllerPtr->Init();
	}
	return m_CldControllerPtr;
}

int CloudController::open(void *args)
{
	return this->activate(THR_BOUND);
}

int CloudController::close(u_long flags)
{
	return 0;
}

int CloudController::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    {
        boost::shared_ptr<Logger::LogCutter> telemetryLogCutter(new Logger::LogCutter(gTagTelemetryLog));

        LocalConfigurator lConfig;

        std::string logPath;
        if (GetLogFilePath(logPath))
        {
            boost::filesystem::path tagLogPath(logPath);
            tagLogPath /= "tagTelemetry.log";
            gTagTelemetryLog.m_logFileName = tagLogPath.string();

            const  int maxCompletedLogFiles = lConfig.getLogMaxCompletedFiles();
            const boost::chrono::seconds cutInterval(lConfig.getLogCutInterval());
            const uint32_t logFileSize = lConfig.getLogMaxFileSizeForTelemetry();
            gTagTelemetryLog.SetLogFileSize(logFileSize);

            telemetryLogCutter->Initialize(tagLogPath, maxCompletedLogFiles, cutInterval);
            telemetryLogCutter->StartCutting();
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to init tag telemetry logger\n", FUNCTION_NAME);
            throw std::runtime_error("unable to init tag telemetry logger.");
        }

        while (!Controller::getInstance()->QuitRequested(5))
        {
            ProcessRequests();
        }

    }
    gTagTelemetryLog.CloseLogFile();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return  0 ;
}

void CloudController::ProcessMsg(SV_ULONG policyId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();

	schedulerPtr->UpdateTaskStatus(policyId,TASK_STATE_STARTED) ;
	
	if( configuratorPtr->getApplicationPolicies(policyId, "CLOUD",m_policy) )
    {
        Process() ;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "There are no policies available for policyId %ld\n", policyId) ;
    }
	
	schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED);
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR CloudController::Process()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK;
	std::map<std::string,std::string> policyProps = m_policy.m_policyProperties;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
	if( schedulerPtr->isPolicyEnabled(m_policyId) )
	{
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
	}
	
	if(policyProps.find("PolicyType") != policyProps.end())
	{
		std::string strPolicyType = policyProps.find("PolicyType")->second;
		if(m_CldPolicyHandlers.find(strPolicyType) != m_CldPolicyHandlers.end())
		{
			CldPolicyHandlerRoutine_t policyHandler = m_CldPolicyHandlers[strPolicyType];
			retStatus = (this->*policyHandler)();
		}
		else
		{
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Uknown policy type :"+strPolicyType);
			DebugPrintf(SV_LOG_ERROR,"Unkwon policy type %s\n",strPolicyType.c_str());
		}
	}
	else
	{
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","PolicyType property is missing");
		DebugPrintf(SV_LOG_ERROR,"PolicyType property is missing\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}
#ifdef WIN32
std::string CloudController::GetPrepareTargetCommand(const std::string &planName, std::string &configFile, std::string &resultFile, const std::string & cloudEnv, bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	
	LocalConfigurator lConfig;
	std::string strPlanDirPath = lConfig.getInstallPath();
	std::string strEsxUtilWinCmd = lConfig.getInstallPath();

	strPlanDirPath += "\\Failover\\Data\\";
	if(!planName.empty()) strPlanDirPath += planName+"\\";

	if(SVMakeSureDirectoryPathExists(strPlanDirPath.c_str()).failed())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create the plan folder %s\n",strPlanDirPath.c_str());
		return ""; //Returning empty command
	}
	
	configFile = strPlanDirPath + INM_CLOUD_PREP_TARGET_CONF_FILE;
	resultFile = strPlanDirPath + INM_CLOUD_PREP_TARGET_RESULT_FILE;

	//Build the esxutilwin.exe command
	strEsxUtilWinCmd = "\""+strEsxUtilWinCmd+"\\EsxUtilWin.exe\" -role";
	if(bPhySnapshot) 
		strEsxUtilWinCmd += " physnapshot";
	else
		strEsxUtilWinCmd += " target";
	strEsxUtilWinCmd += " -cloudenv " + cloudEnv;
	strEsxUtilWinCmd += " -configfile \"" + configFile + "\"";
	strEsxUtilWinCmd += " -resultfile \"" + resultFile + "\"";

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return strEsxUtilWinCmd;
}
SVERROR CloudController::DoPrepareTarget(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & preTgtPolicyData,const std::string & outputFileName,bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;

	updateMsgStream.str("");
	VMsPrepareTargetUpdateInfo prepareTargetUpdate;
	std::string strCxRemoteFile = getPolicyFileName(m_policyId);

	std::string planName("inm_vm_plan");//Defalt name
	std::map<std::string,std::string>::iterator iterProp = m_policy.m_policyProperties.find("PlanName");
	if(iterProp != m_policy.m_policyProperties.end() && !iterProp->second.empty())
		planName = iterProp->second;
	else
		DebugPrintf(SV_LOG_ERROR,"PlanName is missing in PrepareTarget policy\n");

	std::string cloudEnv("Azure");//assuming the azure as default, handle this properly
	iterProp = m_policy.m_policyProperties.find("ApplicationName");
	if(iterProp != m_policy.m_policyProperties.end() && !iterProp->second.empty())
		cloudEnv = iterProp->second;
	else
		DebugPrintf(SV_LOG_ERROR,"ApplicationName is missing in PrepareTarget policy\n");

	HANDLE h = Controller::getInstance()->getDuplicateUserHandle();
	do
	{
		std::string strHostName = Host::GetInstance().GetHostName();
		PlaceHolder placeHolder;
		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;

		std::stringstream msgStream;
		std::string configFile, resultFile;
		std::string strEsxUtilWinCmd = GetPrepareTargetCommand(planName, configFile, resultFile, cloudEnv, bPhySnapshot);
		if (strEsxUtilWinCmd.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to construct the prepare target command\n");
			msgStream << "Protection failed with an internal error on Master Target " << strHostName << std::endl;
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = msgStream.str();
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0520";
			prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
			placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0520";
			prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

			retVal = SVE_FAIL;
			break;
		}
		//Write the policy data to PrepareTarget config file. This file will be used by EsxUtilWin for prepareTarget.
		std::ofstream confOut(configFile.c_str());
		if (confOut.good())
		{
			confOut << cxArg<SrcCloudVMsInfo>(preTgtPolicyData);
			confOut.close();
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Could not open the prepare target config file %s\n", configFile.c_str());
			msgStream.str("");
			msgStream << "Protection failed with an internal error on Master Target " << strHostName << std::endl;
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = msgStream.str();
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0523";
			prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
			placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0523";
			prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

			retVal = SVE_FAIL;
			break;
		}

		SV_ULONG exitCode = 0;
		AppCommand esxUtilWinCmd(strEsxUtilWinCmd, 0, outputFileName, true, true);
		std::string cmdOutPut;
		if (esxUtilWinCmd.Run(exitCode, cmdOutPut, Controller::getInstance()->m_bActive, "", h) != SVS_OK)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to spawn the prepare target command. Command : %s\n", strEsxUtilWinCmd.c_str());
			msgStream.str("");
			msgStream << "Protection failed with an internal error on Master Target " << strHostName << std::endl;
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = msgStream.str();
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0521";
			prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
			placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0521";
			prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;
			retVal = SVE_FAIL;
		}
		else if (exitCode)
		{
			msgStream.str("");
			msgStream << "Prepare target command failed with exit code " << exitCode << std::endl;
			DebugPrintf(SV_LOG_ERROR, "Prepare target command failed with exit code : %lu\n", exitCode);
			std::ifstream in(resultFile.c_str());
			if (in.good())
			{
				msgStream.str("");
				msgStream << in.rdbuf();
				if (!msgStream.str().empty())
				{
					try
					{
						prepareTargetUpdate.Add(unmarshal<VMsPrepareTargetUpdateInfo>(msgStream.str()));
						prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
					}
					catch (...)
					{
						msgStream << "Exception in parsing Prepare Target result file " << resultFile;
						DebugPrintf(SV_LOG_ERROR, "Exception in parsing Prepare Target result file\n");
					}
				}
				in.close();
			}

			if (prepareTargetUpdate.m_updateProps.find(INM_CLOUD_ERROR_MSG) == prepareTargetUpdate.m_updateProps.end())
			{
				prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Protection failed with an internal error on Master Target " + strHostName;
				prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0524";
				prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0524";
				prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;
			}
			retVal = SVE_FAIL;
		}
		else //Successfully completed perpare target
		{
			//Read the result file and update the content to cx.
			std::ifstream in(resultFile.c_str());
			if (in.good())
			{
				//Onsuccess the updateMsgStream should contrain only the update msg serialized string.
				// No other information should be inserted.
				updateMsgStream.str("");
				updateMsgStream << in.rdbuf();
				in.close();
			}
			else
			{
				retVal = SVE_FAIL;
				DebugPrintf(SV_LOG_ERROR, "Unable to read the prepare target result file [%s] at Master Target %s\n", resultFile.c_str(), strHostName.c_str());
				prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Protection failed with an internal error on Master Target " + strHostName;
				prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0524";
				prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0524";
				prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;
			}
		}		
	} while (false);

	if (h) CloseHandle(h);
	if (retVal == SVE_FAIL)
		updateMsgStream << cxArg<VMsPrepareTargetUpdateInfo>(prepareTargetUpdate);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}

#else
SVERROR CloudController::DoPrepareTarget(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & preTgtPolicyData,const std::string & outputFileName,bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;
	std::string cloudEnv("azure"); //assuming the azure as default, handle this properly
	std::map<std::string,std::string>::iterator iterProp = m_policy.m_policyProperties.find("ApplicationName");
	if(iterProp != m_policy.m_policyProperties.end() && !iterProp->second.empty())
	{
		cloudEnv = iterProp->second;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"ApplicationName is missing in PrepareTarget policy\n");
		/*updateMsgStream << "ApplicationName is missing in PrepareTarget policy" << std::endl;
		return SVE_FAIL;*/
	}

	//Removing if any stale recovery progress file exists in MT. This is required for smooth progress of recovery for this server
	if(DeleteRecoveryProgressFiles(preTgtPolicyData))
	{
		DebugPrintf(SV_LOG_WARNING,"Failed to delete the previous recovery progress files\n");
	}

    if(strcmpi(cloudEnv.c_str(),"azure") == 0)
	{
		retVal = DoPrepareTargetAzure(updateMsgStream,preTgtPolicyData,outputFileName,bPhySnapshot);
	}
	else if(strcmpi(cloudEnv.c_str(),"aws") == 0)
	{
		retVal = DoPrepareTargetAWS(updateMsgStream,preTgtPolicyData,outputFileName,bPhySnapshot);
	}
	else if(strcmpi(cloudEnv.c_str(),"vmware") == 0)
	{
		retVal = PrepareTargetForProtection(cloudEnv,preTgtPolicyData,updateMsgStream);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Uknown ApplicationName : %s\n",cloudEnv.c_str());
		updateMsgStream << "Uknown ApplicationName : " << cloudEnv << std::endl;
		retVal = SVE_FAIL;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}
SVERROR CloudController::DoPrepareTargetAWS(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & preTgtPolicyData,const std::string & outputFileName,bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;
	// For AWS we are just validating the attached devices existence on MT.
	// If all the devices found then the prepare target is success otherwise failure.
	std::vector<std::string> targetDevices;
	if(GetTargetDevices(targetDevices) != SVS_OK)
	{
		retVal = SVE_FAIL;
		updateMsgStream << "Failed to get the target devices" << std::endl;
	}
	else
	{
		bool bSuccess = true;
		CloudVMsDisksVolsMap prepTgtUpdateInfo;
		std::map<std::string,SrcCloudVMDisksInfo>::const_iterator iterVM = 
			preTgtPolicyData.m_VmsInfo.begin();
		while(iterVM != preTgtPolicyData.m_VmsInfo.end())
		{
			std::map<std::string,std::string> mapSrcTgtDisks;
			updateMsgStream << "Source  Host : " << iterVM->first << std::endl;
			std::map<std::string, std::string>::const_iterator iterSrcDisk = iterVM->second.m_vmSrcTgtDiskLunMap.begin();
			while(iterSrcDisk != iterVM->second.m_vmSrcTgtDiskLunMap.end())
			{
				std::vector<std::string>::const_iterator iterTgtDevice = targetDevices.begin();
				
				for(;iterTgtDevice != targetDevices.end(); iterTgtDevice++)
					if(iterTgtDevice->compare(iterSrcDisk->second) == 0)
						break;

				if(iterTgtDevice == targetDevices.end())
				{
					updateMsgStream << "Error: Target disk not found for the disk mapping [" << iterSrcDisk->first 
						<< "] => [" << iterSrcDisk->second << "]" << std::endl;
					bSuccess = false;
				}
				else
				{
					updateMsgStream << "[" << iterSrcDisk->first << "] => [" << iterSrcDisk->second << "] Successfuly mapped." << std::endl;
					mapSrcTgtDisks[iterSrcDisk->first] = iterSrcDisk->second;
				}
				iterSrcDisk++;
			}
			prepTgtUpdateInfo.m_volumeMapping.insert(std::make_pair(iterVM->first,mapSrcTgtDisks));
			iterVM++;
		}
		if(bSuccess)
		{
			//Write the messages to output log file so that policy update call will upload to cx.
			std::ofstream out(outputFileName.c_str());
			if(out.good())
			{
				out << updateMsgStream.str();
				out.close();
			}
			updateMsgStream.str(""); // On success take the serialized format of update structure to report to cx.
			updateMsgStream << CxArgObj<CloudVMsDisksVolsMap>(prepTgtUpdateInfo);
		}
		else
		{
			//On failure report the log messages to CX as update string.
			retVal = SVE_FAIL;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}
SVERROR CloudController::DoPrepareTargetAzure(std::stringstream & updateMsgStream,const SrcCloudVMsInfo & preTgtPolicyData,const std::string & outputFileName,bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;
	
	updateMsgStream.str("");
	VMsPrepareTargetUpdateInfo prepareTargetUpdate;
	std::stringstream msgStream;
	std::string strHostName = Host::GetInstance().GetHostName();
	PlaceHolder placeHolder;
	std::string strCxRemoteFile = getPolicyFileName(m_policyId);

	//Identify the devicenames correspond to each lun, recieved in policy data, on MT.
	//So we have:
	//				Src_device_name=>Lun
	//				Lun=>Tgt_device_name
	//Using this information prepare Src_device_name=>Tgt_device_name and update to CX

	//Retrying the lscsi disks discovery 20 times in 30 secs each (total 10 mins)
	for(int nTry = 20; nTry >= 0 ; nTry--)
	{
		msgStream.str("");
		std::map<std::string,std::string> mapLunDevices;
		if(GetTargetDevicesLunMapping(mapLunDevices) != SVS_OK)
		{
			//This is not a retriable failure. So proceeding with fail.
			retVal = SVE_FAIL;
			//msgStream << "Failed to get the Lun<->Device mapping" << std::endl;
			prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0518";
			break;
		}
		else
		{
			bool bSuccess = true;
			CloudVMsDisksVolsMap prepTgtUpdateInfo;
			std::map<std::string,SrcCloudVMDisksInfo>::const_iterator iterVM = 
				preTgtPolicyData.m_VmsInfo.begin();
			while(iterVM != preTgtPolicyData.m_VmsInfo.end())
			{
				std::map<std::string,std::string> mapSrcTgtDisks;
				msgStream << "Source  Host : " << iterVM->first << std::endl;
				std::map<std::string, std::string>::const_iterator iterSrcDisk = iterVM->second.m_vmSrcTgtDiskLunMap.begin();
				while(iterSrcDisk != iterVM->second.m_vmSrcTgtDiskLunMap.end())
				{
					std::map<std::string,std::string>::const_iterator iterTgtDisk =
						mapLunDevices.find(iterSrcDisk->second);
					if(iterTgtDisk == mapLunDevices.end())
					{
						//Need to set global error message
						//msgStream << "Error: Target disk not found for the source disk [" << iterSrcDisk->first 
						//	<< "] => Trget-Lun [" << iterSrcDisk->second << "]" << std::endl;
						DebugPrintf(SV_LOG_WARNING, "Error: Target disk not found for the source disk[%s] => Target-Lun [%s]\n",  iterSrcDisk->first.c_str(), iterSrcDisk->second.c_str());
						prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0519";
						bSuccess = false;
					}
					else
					{
						msgStream << "[" << iterSrcDisk->first << "] => [" << iterTgtDisk->second << "] Successfuly mapped." << std::endl;
						DebugPrintf(SV_LOG_DEBUG, "Source disk[%s] => Target Disk [%s]\n",  iterSrcDisk->first.c_str(), iterTgtDisk->second.c_str());
						mapSrcTgtDisks[iterSrcDisk->first] = iterTgtDisk->second;
					}
					iterSrcDisk++;
				}
				prepTgtUpdateInfo.m_volumeMapping.insert(std::make_pair(iterVM->first,mapSrcTgtDisks));
				iterVM++;
			}
			if(bSuccess)
			{
				//Write the messages to output log file so that policy update call will upload to cx.
				std::ofstream out(outputFileName.c_str());
				if(out.good())
				{
					out << msgStream.str();
					out.close();
				}
				msgStream.str(""); // On success take the serialized format of update structure to report to cx.
				msgStream << CxArgObj<CloudVMsDisksVolsMap>(prepTgtUpdateInfo);
				break;
			}
			else
			{
				if(nTry > 0)
				{
					//Sleeps 30 secs and checks if quit request has been triggered in each 1 sec
					if(Controller::getInstance()->QuitRequested(30))
					{
						DebugPrintf(SV_LOG_WARNING, "[Warning] service quit has been requested.\n") ;
						//msgStream << "Application agent service stop request has been generated. Prepare target policy failed to process." << std::endl;
						prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0525";
						retVal = SVE_FAIL;
						break;
					}
					DebugPrintf(SV_LOG_WARNING, "Required target devices has not been discovered. Retrying target device discovery again.\n") ;
					continue;
				}
				else
				{
					//On failure report the log messages to CX as update string.
					retVal = SVE_FAIL;
					break;
				}				
			}
		}
	}
	if(retVal == SVE_FAIL)
	{
		msgStream << "Protection failed with an internal error on Master Target " << strHostName << std::endl;
		prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = msgStream.str();
		prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE];
		prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		msgStream.str("");
		msgStream << cxArg<VMsPrepareTargetUpdateInfo>(prepareTargetUpdate);
	}
	
	updateMsgStream << msgStream.str();

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}
SVERROR CloudController::GetTargetDevices(std::vector<std::string> &devices)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;
	std::string tempFile = "/tmp/inmdevicelist.out";
	std::string device_list_script = "/tmp/inmdevicelist.sh";
	std::ofstream script_out(device_list_script.c_str());
	if(script_out.good())
	{
		script_out << "fdisk -l | grep \'^Disk /\' | awk \'{sub(/:/,\"\",$2);print $2}\'";
		script_out.close();
		if(chmod(device_list_script.c_str(),0777) != 0) // Set execute permissions
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to set the execute permissions for list devices script\n");
			return SVS_FALSE;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create the list devicess script\n");
		return SVS_FALSE;
	}

	AppCommand cmd(device_list_script,0,tempFile);
	SV_ULONG exitCode = 0;
	std::string strOutput;
	if(cmd.Run(exitCode,strOutput,Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to run the list devices command [%s]\n",device_list_script.c_str());
		retVal = SVS_FALSE;
	}
	else if(0 != exitCode)
	{
		DebugPrintf(SV_LOG_ERROR,"list devices command failed with error code %d.\n",exitCode);
		retVal = SVS_FALSE;
	}
	else
	{
		std::string lineRead;
		std::ifstream in(tempFile.c_str());
		if(!in.good())
		{
			DebugPrintf(SV_LOG_ERROR,"Could not read the list devices command output. File read error %s\n",tempFile.c_str());
			retVal = SVS_FALSE;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Following devices are found in the machine\n");
			while(std::getline(in,lineRead))
			{
				DebugPrintf(SV_LOG_INFO,"%s\n",lineRead.c_str());
				devices.push_back(lineRead);
			}
			in.close();
		}
		remove(tempFile.c_str());
	}

	remove(device_list_script.c_str());
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}
SVERROR CloudController::GetTargetDevicesLunMapping(std::map<std::string,std::string> & lun_device_mapping)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;

	std::string strScsiHost = GetAzureLinuxScsiHostNumber();  //This will get the scsi_host parameter of retention drive
	std::string tempFile = "/tmp/inmlsscsi.out";
	std::string tmpFileForScsiId = "/tmp/inmscsiid.out";
	std::string lsscsi_cmd = "lsscsi";
	AppCommand cmd(lsscsi_cmd,0,tempFile);
	SV_ULONG exitCode = 0;
	std::string strOutput;
	if(cmd.Run(exitCode,strOutput,Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to run the lscsi command. Please install corresponding rpm if not installed.\n");
		retVal = SVS_FALSE;
	}
	else if(0 != exitCode)
	{
		DebugPrintf(SV_LOG_ERROR,"lscsi command failed with error code %d.\n",exitCode);
		retVal = SVS_FALSE;
	}
	else
	{
		std::string lineRead;
		std::ifstream in(tempFile.c_str());
		if(!in.good())
		{
			DebugPrintf(SV_LOG_ERROR,"Could not read the lscsi command output. File read error %s\n",tempFile.c_str());
			retVal = SVS_FALSE;
		}
		else
		{
		    LocalConfigurator lConfig;
			while(std::getline(in,lineRead))
			{
				std::string device_name,lun,target_number,channel,scsi_host;
				if(ProcessLsScsiCmdLine(lineRead,device_name,lun,target_number,channel,scsi_host) != SVS_OK)
				{
					DebugPrintf(SV_LOG_WARNING,"Error occured with lsscsi output line :%s\n",lineRead.c_str());
					continue;
					//retVal = SVS_FALSE;
					//break;
				}
				if(0 == scsi_host.compare(strScsiHost))//Azure specific
                {
                    /*Run the scsi_id for matched device and get the scsi id of  the disk */
					DeviceIDInformer f;
					std::string strDevice = f.GetID(device_name);
					if(!strDevice.empty())
					{
						lun_device_mapping[lun] = "/dev/mapper/"+ strDevice;
						DebugPrintf(SV_LOG_DEBUG,"Lun ID is %s, Device set is %s\n",lun.c_str(), lun_device_mapping[lun].c_str() );
					}
                }
				else
					DebugPrintf(SV_LOG_INFO,"Device(%s)=>scsi-host(%s):channel(%s):target-number(%s):lun(%s) skipped\n",
					device_name.c_str(),scsi_host.c_str(),channel.c_str(),target_number.c_str(),lun.c_str());

                DebugPrintf(SV_LOG_DEBUG,"Device is %s\n",lun_device_mapping[lun].c_str() );
			}
			in.close();
		}
		remove(tempFile.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}
SVERROR CloudController::ProcessLsScsiCmdLine(
	const std::string &line,
	std::string &device_name,
	std::string &lun,
	std::string &target_number,
	std::string &channel,
	std::string &scsi_host)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retVal = SVS_OK;
	size_t lpos = line.find_first_of("["),
		   rpos = line.find_first_of("]");
	if(std::string::npos == lpos || std::string::npos == rpos || lpos >= rpos)
	{
		DebugPrintf(SV_LOG_ERROR,"Could not get SCSI information. Unexpected line format.\n\t%s\n",line.c_str());
		return SVS_FALSE;
	}
	//Format of scsi lun info [scsi_host:channel:target_number:LUN]
	lpos++;//Excluding the square brackets
	std::string strLunInfo(line,lpos,rpos-lpos);
	
	int tokPos = 0; // 0->scso_host, 1->channel, 2->target_number,3->LUN
	char *pTok = strtok(const_cast<char*>(strLunInfo.c_str()),":");
	while(NULL != pTok)
	{
		switch(tokPos)
		{
		case 0: scsi_host = pTok;
			break;
		case 1: channel = pTok;
			break;
		case 2: target_number = pTok;
			break;
		case 3: lun = pTok;
			break;
		}
		tokPos++;
		pTok = strtok(NULL,":");
	}


	//Find device name
	rpos = line.find_last_not_of(" \t\f\v\n\r");
	lpos = line.find_last_of(" \t\f\v\n\r",rpos);

	if(std::string::npos == lpos || std::string::npos == rpos || lpos >= rpos)
	{
		DebugPrintf(SV_LOG_ERROR,"Could not get disk device name. Unexpected line format.\n\t%s\n",line.c_str());
		return SVS_FALSE;
	}
	device_name.assign(line,lpos+1,rpos-lpos);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}


std::string CloudController::GetAzureLinuxScsiHostNumber()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	std::string strScsiHostNum;
	SVERROR retVal = SVS_OK;
	std::string scsi_host, channel, target_number, lun;

	do
	{
		//This will get the standard device name on which retention is mounted.
		std::string strCmd = "blkid |grep -v \"/dev/mapper\"|grep `grep \"retention\" /etc/fstab|awk '{ print $1 }'|awk -F \"=\" '{ print $2 }'`|awk -F \":\" '{ print $1 }'";
		std::string strDevice = "";
		retVal = ExecuteLinuxCmd(strCmd, strDevice);
		if (retVal == SVS_FALSE || strDevice.empty())
			break;

		//Gets the scsi id for the above device. Output of scsi id format by below command - [scsi_host:channel:target_number:lun]
		strCmd = "lsscsi | grep " + strDevice + " | awk '{ print $1 }'";
		std::string strScsiId;
		retVal = ExecuteLinuxCmd(strCmd, strScsiId);
		if (retVal == SVS_FALSE || strScsiId.empty())
			break;

		//Tokenising the scsiid to get the scsi_host number
		size_t lpos = strScsiId.find_first_of("["),
			rpos = strScsiId.find_first_of("]");
		if (std::string::npos == lpos || std::string::npos == rpos || lpos >= rpos)
		{
			DebugPrintf(SV_LOG_ERROR, "Could not get SCSI information. Unexpected format.\n\t%s\n", strScsiId.c_str());
			retVal = SVS_FALSE;
			break;
		}

		//Format of scsi lun info [scsi_host:channel:target_number:LUN]
		lpos++;//Excluding the square brackets
		std::string strLunInfo(strScsiId, lpos, rpos - lpos);

		int tokPos = 0; // 0->scso_host, 1->channel, 2->target_number,3->LUN
		char *pTok = strtok(const_cast<char*>(strLunInfo.c_str()), ":");
		while (NULL != pTok)
		{
			switch (tokPos)
			{
			case 0: scsi_host = pTok;
				break;
			case 1: channel = pTok;
				break;
			case 2: target_number = pTok;
				break;
			case 3: lun = pTok;
				break;
			}
			tokPos++;
			pTok = strtok(NULL, ":");
		}
		DebugPrintf(SV_LOG_DEBUG, "Scsi host : %s\n", scsi_host.c_str());

	} while (0);

	if (retVal == SVS_FALSE)
		strScsiHostNum = "5";
	else if(scsi_host.empty())
		strScsiHostNum = "5";
	else
		strScsiHostNum = scsi_host;

	DebugPrintf(SV_LOG_DEBUG, "Scsi host to be considered : %s\n", strScsiHostNum.c_str());

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return strScsiHostNum;
}


//This API processes Linux command which will genarate only single line output
//First paramater contains the command and second parameter contains the output for that command
SVERROR CloudController::ExecuteLinuxCmd(const std::string& command, std::string& strCmdOutput)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	SVERROR retVal = SVS_OK;
	std::string lineRead;
	std::string tempFile = "/tmp/cmdoutput.out";
	AppCommand cmd(command, 0, tempFile);
	SV_ULONG exitCode = 0;
	std::string strOutput;
	if (cmd.Run(exitCode, strOutput, Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command [%s].\n", command.c_str());
		retVal = SVS_FALSE;
	}
	else if (0 != exitCode)
	{
		DebugPrintf(SV_LOG_ERROR, "command [%s] failed with error code %d.\n", command.c_str(), exitCode);
		retVal = SVS_FALSE;
	}
	else
	{
		std::ifstream inFile(tempFile.c_str());
		if (!inFile.good())
		{
			DebugPrintf(SV_LOG_ERROR, "Could not read the command output. File read error %s\n", tempFile.c_str());
			retVal = SVS_FALSE;
		}
		else
		{
			std::getline(inFile, lineRead); //AppComman logs command as first line in outfile.
			std::getline(inFile, lineRead); //Read command o/p
			strCmdOutput = lineRead;
			DebugPrintf(SV_LOG_DEBUG, "OutPut : %s\n", strCmdOutput.c_str());
		}
		inFile.close();
		remove(tempFile.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return retVal;
}

#endif
std::string CloudController::GetRecoveryCommand(const std::string &planName, std::string &configFile, std::string &resultFile, const std::string & cloudEnv, bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	LocalConfigurator lConfig;
	std::string strPlanDirPath = lConfig.getInstallPath();
	std::string recoveryCmd = lConfig.getInstallPath();

#ifdef WIN32
	strPlanDirPath += "\\Failover\\Data\\";
#else
	strPlanDirPath += "/failover_data/";
#endif
	if(!planName.empty())
	{
		strPlanDirPath += planName + ACE_DIRECTORY_SEPARATOR_STR_A;
	}

	if(SVMakeSureDirectoryPathExists(strPlanDirPath.c_str()).failed())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create the plan folder %s\n",strPlanDirPath.c_str());
		return ""; //Returning empty command
	}
	
	if(bPhySnapshot)
	{
		configFile = strPlanDirPath + INM_CLOUD_DRDRILL_CONF_FILE;
		resultFile = strPlanDirPath + INM_CLOUD_DRDRILL_RESULT_FILE;
	}
	else
	{
		configFile = strPlanDirPath + INM_CLOUD_RECOVERY_CONF_FILE;
		resultFile = strPlanDirPath + INM_CLOUD_RECOVERY_RESULT_FILE;
	}

	recoveryCmd += ACE_DIRECTORY_SEPARATOR_STR_A;
#ifdef WIN32
	recoveryCmd = "\""+recoveryCmd+"EsxUtil.exe\"";
#else
	recoveryCmd = "\""+recoveryCmd+"/bin/EsxUtil\"";
#endif
	if(bPhySnapshot)
		recoveryCmd += " -physnapshot";
	else
		recoveryCmd += " -rollback";

	recoveryCmd += " -cloudenv " + cloudEnv;
	recoveryCmd += " -configfile \"" + configFile + "\"";
	recoveryCmd += " -resultfile \"" + resultFile + "\"";

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return recoveryCmd;
}

SVERROR CloudController::DoRecover(std::stringstream & updateMsgStream, const std::string & policyData, const std::string & outputFileName, bool bPhySnapshot)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK;
	
	updateMsgStream.str("");
	std::string planName("inm_vm_plan");//Defalt name
	std::map<std::string,std::string>::iterator iterProp = m_policy.m_policyProperties.find("PlanName");
	if(iterProp != m_policy.m_policyProperties.end() && !iterProp->second.empty())
		planName = iterProp->second;
	else
		DebugPrintf(SV_LOG_ERROR,"PlanName is missing in PrepareTarget policy\n");

	std::string cloudEnv("Azure");//assuming the azure as default, handle this properly
	iterProp = m_policy.m_policyProperties.find("ApplicationName");
	if(iterProp != m_policy.m_policyProperties.end() && !iterProp->second.empty())
		cloudEnv = iterProp->second;
	else
		DebugPrintf(SV_LOG_ERROR,"ApplicationName is missing in PrepareTarget policy\n");

	std::string preScript, postScript;
	GetValueFromMap(m_policy.m_policyProperties,"PreScript",preScript);
    GetValueFromMap(m_policy.m_policyProperties,"PostScript",postScript) ;

	VMsRecoveryUpdateInfo recoveryUpdate;
	std::string strHostName = Host::GetInstance().GetHostName();
	PlaceHolder placeHolder;
	placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;

	std::string strCxRemoteFile = getPolicyFileName(m_policyId);

	std::string recoveryCmd,recoveryConfFile,resultFile;
	recoveryCmd = GetRecoveryCommand(planName,recoveryConfFile,resultFile,cloudEnv,bPhySnapshot);
	if(recoveryCmd.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to construct the recovery command\n");
		recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed with an internal error on Master Target " + strHostName;
		recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0610";
		recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0610";
		recoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		updateMsgStream << cxArg<VMsRecoveryUpdateInfo>(recoveryUpdate);

		return SVE_FAIL;
	}

	std::ofstream confOut(recoveryConfFile.c_str());
	if(confOut.good())
	{
		confOut << policyData.c_str();
		confOut.close();
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Could not open the recovery config file %s\n",recoveryConfFile.c_str());
		recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed with an internal error on Master Target " + strHostName;
		recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0612";
		recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0612";
		recoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		updateMsgStream << cxArg<VMsRecoveryUpdateInfo>(recoveryUpdate);

		return SVE_FAIL;
	}

	void *h = NULL;
#ifdef WIN32
	h = Controller::getInstance()->getDuplicateUserHandle();
#endif
	do
	{
		SV_ULONG exitCode = 0;
		std::string cmdOutPut;
		if(!preScript.empty())
		{
			AppCommand preScriptCmd(preScript,0,outputFileName);
			if(preScriptCmd.Run(exitCode,cmdOutPut, Controller::getInstance()->m_bActive,"",h) != SVS_OK)
			{
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed while executing pre-script on Master Target " + strHostName;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_LOG] = std::string("Failed to spawn the pre-script.\nCommand: ") + preScript;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_STATUS] = "Failed";
				recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				DebugPrintf(SV_LOG_ERROR, "%s\n", recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_LOG].c_str());
				retStatus = SVE_FAIL;
				break;
			}
			else if(exitCode)
			{
				std::stringstream scriptMsgStream;
				scriptMsgStream << cmdOutPut << std::endl << "Pre-Script failed with exit code " << exitCode;
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed while executing pre-script on Master Target " + strHostName;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_LOG] = scriptMsgStream.str();
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_STATUS] = "Failed";
				recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				DebugPrintf(SV_LOG_ERROR, "Pre-Script failed with exit code %lu\n", exitCode);
				retStatus = SVE_FAIL;
				break;
			}
			else
			{
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_LOG] = cmdOutPut;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_PRESCRIPT_STATUS] = "Success";
			}
		}

		cmdOutPut = "";
		AppCommand esxUtilCmd(recoveryCmd,0,outputFileName,true,true);
		if(esxUtilCmd.Run(exitCode,cmdOutPut, Controller::getInstance()->m_bActive,"",h) != SVS_OK)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to spawn the recovery command.\n Command: %s", recoveryCmd.c_str());
			recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed with an internal error on Master Target " + strHostName;
			recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0614";
			recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
			placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0614";
			recoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

			retStatus = SVE_FAIL;
			break;
		}
		else if(exitCode)
		{
			std::stringstream msgStream;
			msgStream << "Recovery command failed with exit code " << exitCode << std::endl;
			DebugPrintf(SV_LOG_ERROR, "Recovery command failed with exit code : %lu\n", exitCode);

			std::ifstream in(resultFile.c_str());
			if (in.good())
			{
				std::stringstream resultStream;
				resultStream << in.rdbuf();
				if (!resultStream.str().empty())
				{
					try
					{
						recoveryUpdate.Add(unmarshal<VMsRecoveryUpdateInfo>(resultStream.str()));
						recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
					}
					catch (...)
					{
						msgStream << "Exception in parsing recovery config file " << resultFile;
						DebugPrintf(SV_LOG_ERROR, "Exception in parsing recovery config file\n");
					}
				}
				in.close();
			}

			if (recoveryUpdate.m_updateProps.find(INM_CLOUD_ERROR_MSG) == recoveryUpdate.m_updateProps.end())
			{
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed with an internal error on Master Target " + strHostName;
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0613";
				recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0613";
				recoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;
			}

			retStatus = SVE_FAIL;
			break;
		}
		else
		{
			std::stringstream msgStream;

			//Read the result file
			std::ifstream in(resultFile.c_str());
			if(in.good())
			{
				std::stringstream resultStream;
				resultStream << in.rdbuf();
				if (!resultStream.str().empty())
				{
					try
					{
						recoveryUpdate.Add(unmarshal<VMsRecoveryUpdateInfo>(resultStream.str()));
					}
					catch (...)
					{
						msgStream << "Exception in parsing recovery result file " << resultFile;
						retStatus = SVE_FAIL;
					}
				}
				else
				{
					msgStream << "Recovry result file is empty" << resultFile;
					retStatus = SVE_FAIL;					
				}
				in.close();
			}
			else
			{
				msgStream << "Failed to read the recovery result file " << resultFile;
				retStatus = SVE_FAIL;
			}

			if (retStatus == SVE_FAIL)
			{
				DebugPrintf(SV_LOG_ERROR, "%s\n", msgStream.str().c_str());
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed with an internal error on Master Target " + strHostName;
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0613";
				recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0613";
				recoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;
				break;
			}
		}

		if(!postScript.empty())
		{
			cmdOutPut = "";
			AppCommand postScriptCmd(postScript,0,outputFileName);
			if(postScriptCmd.Run(exitCode,cmdOutPut, Controller::getInstance()->m_bActive,"",h) != SVS_OK)
			{
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed while executing post-script on Master Target " + strHostName;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_LOG] = std::string("Failed to spawn the post-script.\nCommand: ") + postScript;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_STATUS] = "Failed";
				recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				DebugPrintf(SV_LOG_ERROR, "%s\n", recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_LOG].c_str());
				retStatus = SVE_FAIL;
				break;
			}
			else if (exitCode)
			{
				std::stringstream scriptMsgStream;
				scriptMsgStream << cmdOutPut << std::endl << "Post-Script failed with exit code " << exitCode;
				recoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Failover failed while executing post-script on Master Target " + strHostName;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_LOG] = scriptMsgStream.str();
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_STATUS] = "Failed";
				recoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
				DebugPrintf(SV_LOG_ERROR, "Post-Script failed with exit code %lu\n", exitCode);
				retStatus = SVE_FAIL;
				break;
			}
			else
			{
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_LOG] = cmdOutPut;
				recoveryUpdate.m_updateProps[INM_CLOUD_RECOVERY_POSTSCRIPT_STATUS] = "Success";
			}
		}

	}while(false);
#ifdef WIN32
	if(h) CloseHandle(h);
#endif
	
	updateMsgStream << cxArg<VMsRecoveryUpdateInfo>(recoveryUpdate);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

					// ******** Policy Handler Functions ******** 
SVERROR CloudController::OnRecover()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
	VMsRecoveryUpdateInfo vmsrecoveryUpdate;
	PlaceHolder placeHolder;
	std::string strHostName;
	strHostName = Host::GetInstance().GetHostName();
	std::stringstream updateStream;

	std::string strCxRemoteFile = getPolicyFileName(m_policyId);
	std::string outputFileName;

	try
	{
		//Calling unmarshal to verify that the serialized string is in proper format before sending to recovery binary.
		VMsRecoveryInfo recoveryPolicyData = unmarshal<VMsRecoveryInfo>(m_policy.m_policyData);

		std::stringstream updateStream;
		outputFileName = getOutputFileName(m_policyId);
		ACE_OS::unlink(outputFileName.c_str());

		retStatus = DoRecover(updateStream, m_policy.m_policyData,outputFileName);

		if(retStatus == SVS_OK)
		{
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","",updateStream.str(),true);
		}
		else
		{
			std::ofstream outFile(outputFileName.c_str(),std::ofstream::app);
			if(outFile.good())
			{
				outFile << std::endl << updateStream.str() << std::endl;
				outFile.close();
			}

			UploadFiletoRemoteLocation(INM_CLOUD_CS_REMOTE_DIR, outputFileName);

			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","",updateStream.str(),true);
		}
	}
	catch( std::exception & e)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception in processing Recovery policy %lu. Exception : %s\n", m_policyId, e.what());

		UploadFiletoRemoteLocation(INM_CLOUD_CS_REMOTE_DIR, outputFileName);

		vmsrecoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Protection failed with an internal error on Master Target " + strHostName;
		vmsrecoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0601";
		vmsrecoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0601";
		vmsrecoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		updateStream << cxArg<VMsRecoveryUpdateInfo>(vmsrecoveryUpdate);
		configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", updateStream.str(), true);
	}
	catch ( ... )
	{
		DebugPrintf(SV_LOG_ERROR, "Unknown exception in processing Recovery policy %lu\n", m_policyId);

		UploadFiletoRemoteLocation(INM_CLOUD_CS_REMOTE_DIR, outputFileName);

		vmsrecoveryUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = "Protection failed with an internal error on Master Target " + strHostName;
		vmsrecoveryUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0601";
		vmsrecoveryUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0601";
		vmsrecoveryUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		updateStream << cxArg<VMsRecoveryUpdateInfo>(vmsrecoveryUpdate);
		configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", updateStream.str(), true);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR CloudController::OnPrepareTarget()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
	VMsPrepareTargetUpdateInfo prepareTargetUpdate;
	PlaceHolder placeHolder;
	std::string strHostName;
	strHostName = Host::GetInstance().GetHostName();
	std::stringstream updateMsgStream, msgStream;

	std::string strCxRemoteFile = getPolicyFileName(m_policyId);
	std::string outputFileName;

	try
	{
		outputFileName = getOutputFileName(m_policyId);
		ACE_OS::unlink(outputFileName.c_str());
		SrcCloudVMsInfo preTgtPolicyData = unmarshal<SrcCloudVMsInfo>(m_policy.m_policyData);

		retStatus = DoPrepareTarget(updateMsgStream,preTgtPolicyData,outputFileName);
		
		if(retStatus == SVS_OK)
		{
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","",updateMsgStream.str(),true);
		}
		else
		{
			std::ofstream outFile(outputFileName.c_str(),std::ofstream::app);
			if(outFile.good())
			{
				outFile << std::endl << updateMsgStream.str() << std::endl;
				outFile.close();
			}

			UploadFiletoRemoteLocation(INM_CLOUD_CS_REMOTE_DIR, outputFileName);

			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","",updateMsgStream.str(),true);
		}
	}
	catch( std::exception & e)
	{
		DebugPrintf(SV_LOG_ERROR,"Exception in processing PrepareTarget policy. Exception : %s",e.what());

		UploadFiletoRemoteLocation(INM_CLOUD_CS_REMOTE_DIR, outputFileName);

		msgStream << "Protection failed with an internal error on Master Target " << strHostName << std::endl;
		prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = msgStream.str();
		prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0517";
		prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0517";
		prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		updateMsgStream << cxArg<VMsPrepareTargetUpdateInfo>(prepareTargetUpdate);
		configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", updateMsgStream.str(), true);
	}
	catch ( ... )
	{
		DebugPrintf(SV_LOG_ERROR,"Unknown exception in processing PrepareTarget policy");

		UploadFiletoRemoteLocation(INM_CLOUD_CS_REMOTE_DIR, outputFileName);

		msgStream << "Protection failed with an internal error on Master Target " << strHostName << std::endl;
		prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = msgStream.str();
		prepareTargetUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = "EA0517";
		prepareTargetUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = strCxRemoteFile;
		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = strHostName;
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = "EA0517";
		prepareTargetUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		updateMsgStream << cxArg<VMsPrepareTargetUpdateInfo>(prepareTargetUpdate);
		configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", updateMsgStream.str(), true);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR CloudController::OnDrDrill()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
	try
	{
		VMsCloneInfo drDrillInfo = unmarshal<VMsCloneInfo>(m_policy.m_policyData);
		std::string outputFileName = getOutputFileName(m_policyId);
		ACE_OS::unlink(outputFileName.c_str());
		std::stringstream updateMsgStream;
		//Prepare the snapshot disks for dr-drill
		retStatus = DoPrepareTarget(updateMsgStream,drDrillInfo.m_snapshotVMDisksInfo,outputFileName,true);
		if(retStatus != SVS_OK)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to prepare the snapshot disks for dr-drill\n");
			throw "Failed to prepare the snapshot disks for dr-drill";
		}
		//Successfuly completed snapshot disks preparation

		//Update the snapshot disks information to dr-drill structure and pass this info to esxutil binary for snapshot operation.
		DebugPrintf(SV_LOG_DEBUG,"Updating prepare targt information to dr-drill structure\n");
		drDrillInfo.m_snapshotDisksVolsInfo = unmarshal<CloudVMsDisksVolsMap>(updateMsgStream.str());
		DebugPrintf(SV_LOG_DEBUG,"Successfuly updated prepare target information to dr-drill structure\n");
#ifdef WIN32
		DebugPrintf(SV_LOG_DEBUG,"Sanitizing volume pathes\n");
		std::map<std::string,std::map<std::string,std::string> >::iterator iterVmSnapVolsInfo = 
			drDrillInfo.m_snapshotDisksVolsInfo.m_volumeMapping.begin();
		while(iterVmSnapVolsInfo != drDrillInfo.m_snapshotDisksVolsInfo.m_volumeMapping.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Host : %s\n",iterVmSnapVolsInfo->first.c_str());
			std::map<std::string,std::string> tmpVolMap;
			std::map<std::string,std::string>::iterator iterVolEntry = iterVmSnapVolsInfo->second.begin();
			while(iterVolEntry != iterVmSnapVolsInfo->second.end())
			{
				std::string keyString = iterVolEntry->first,
					valueString = iterVolEntry->second;
				DebugPrintf(SV_LOG_DEBUG,"Before [%s]=>[%s]\n",keyString.c_str(),valueString.c_str());
				//sanitizeVolumePathName((std::string)(iterVolEntry->first));
				//sanitizeVolumePathName((std::string)(iterVolEntry->second));
				sanitizeVolumePathName(keyString);
				sanitizeVolumePathName(valueString);
				tmpVolMap[keyString] = valueString;
				DebugPrintf(SV_LOG_DEBUG,"After [%s]=>[%s]\n",keyString.c_str(),valueString.c_str());
				iterVolEntry++;
			}
			//Update with sanitized volume paths.
			iterVmSnapVolsInfo->second.clear();
			iterVmSnapVolsInfo->second.insert(tmpVolMap.begin(),tmpVolMap.end());

			iterVmSnapVolsInfo++;
		}
#endif
		
		DebugPrintf(SV_LOG_DEBUG,"Serializing dr-drill structure\n");
		updateMsgStream.str("");
		std::stringstream drdrilldatastream;
		drdrilldatastream << CxArgObj<VMsCloneInfo>(drDrillInfo);
		DebugPrintf(SV_LOG_DEBUG,"Successfuly serialized dr-drill structure\n");
		DebugPrintf(SV_LOG_DEBUG,"dr-drill info: \n%s\n",drdrilldatastream.str().c_str());

		retStatus = DoRecover(updateMsgStream,drdrilldatastream.str(),outputFileName,true);

		if(retStatus == SVS_OK)
		{
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Success","",updateMsgStream.str(),true);
		}
		else
		{
			std::ofstream outFile(outputFileName.c_str(),std::ofstream::app);
			if(outFile.good())
			{
				outFile << std::endl << updateMsgStream.str() << std::endl;
				outFile.close();
			}
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","",updateMsgStream.str(),true);
		}
	}
	catch( std::exception & e)
	{
		DebugPrintf(SV_LOG_ERROR,"Exception in processing DrDrill policy. Exception : %s",e.what());
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","","Unmarshal exception for policy data. Exception :"+std::string(e.what()));
	}
	catch ( ... )
	{
		DebugPrintf(SV_LOG_ERROR,"Unknown exception in processing DrDrill policy");
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","","Unknown exception in processing DrDrill policy");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}


static void ParseCmdOptions(const std::string& cmdOptions, TagStatus& tagStatus)
{
    std::string errMsg;

    uint32_t tagTypeConf;
    if (std::string::npos != cmdOptions.find("-cc"))
        tagTypeConf = CRASH_CONSISTENCY;
    else
        tagTypeConf = APP_CONSISTENCY;

    if (std::string::npos == cmdOptions.find("-distributed"))
        tagTypeConf |= SINGLE_NODE;
    else
    {
        tagTypeConf |= MULTI_NODE;

        std::size_t mnPos = cmdOptions.find(NsVacpOutputKeys::MultVmMasterNodeKey);
        if (std::string::npos != mnPos)
        {
            std::string masterNodeIpAddr;
            GetValueForPropertyKey(cmdOptions, NsVacpOutputKeys::MultVmMasterNodeKey, masterNodeIpAddr);

            if (masterNodeIpAddr.empty())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: master node value parsing failed. input %s\n",
                    FUNCTION_NAME,
                    cmdOptions.c_str());
            }
         
            ADD_STRING_TO_MAP(tagStatus, MASTERNODE, masterNodeIpAddr);
        }

        std::size_t cnPos = cmdOptions.find(NsVacpOutputKeys::MultVmClientNodesKey);
        if (std::string::npos != cnPos)
        {
            std::string strClientNodes;
            GetValueForPropertyKey(cmdOptions, NsVacpOutputKeys::MultVmClientNodesKey, strClientNodes);

            if (strClientNodes.empty())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: client nodes value parsing failed. input %s\n",
                    FUNCTION_NAME,
                    cmdOptions.c_str());
            }
            ADD_STRING_TO_MAP(tagStatus, CLIENTNODES, strClientNodes);

            typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
            boost::char_separator<char> ipSep(",");
            tokenizer_t ipTokens(strClientNodes, ipSep);
            std::vector<std::string> numOfTokens(ipTokens.begin(), ipTokens.end());
            ADD_INT_TO_MAP(tagStatus, NUMOFNODES, numOfTokens.size());
        }
    }

    ADD_INT_TO_MAP(tagStatus, TAGTYPECONF, tagTypeConf);

    return;
}


SVERROR CloudController::OnConsistency()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    static uint16_t ccFailCnt = 0;
    static bool sbRetryAppConsistencyOnReboot = false;
    static bool sbInternalAppConsistencyPolicy = false;

    const std::string ccOption("-cc");
    const std::string multivmOption("-distributed");


    SVERROR retStatus = SVS_OK;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    std::string strLastNLines;
    TagStatus tagStatus;
    bool bIsCrashTag = false;
    bool bMultiVmTag = false;

    try
    {
        AppLocalConfigurator lConfig;

        ADD_INT_TO_MAP(tagStatus, POLICYID, m_policyId);
        ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG);
        ADD_INT_TO_MAP(tagStatus, SYSTIMETAGT, GetTimeInSecSinceEpoch1970());

		std::string outputFileName = 
            getOutputFileName(m_policyId,
            0,
            sbInternalAppConsistencyPolicy ? GetTimeInSecSinceEpoch1970() : 0);

        ACE_OS::unlink(outputFileName.c_str());
        std::stringstream updateMsgStream;
        std::string cmdOutput;
        std::string cmdOptions;
        unsigned long long systemUptime = 0;

        std::map<std::string, std::string>::iterator iterProp = m_policy.m_policyProperties.find("CmdOptions");
        if (iterProp != m_policy.m_policyProperties.end() && iterProp->second.compare("0") != 0)
            cmdOptions = iterProp->second;

        size_t ccPos = cmdOptions.find(ccOption);
        bIsCrashTag = (std::string::npos != ccPos);
        bMultiVmTag = (std::string::npos != cmdOptions.find(multivmOption));

        assert(!(bMultiVmTag && sbInternalAppConsistencyPolicy));

        if (sbInternalAppConsistencyPolicy && bIsCrashTag)
        {
            cmdOptions.erase(ccPos, ccOption.length());
            bIsCrashTag = false;
        }

        DebugPrintf(SV_LOG_ALWAYS, "processing %s consistency\n",
            bIsCrashTag ? "Crash" : "App");

        ParseCmdOptions(cmdOptions, tagStatus);

        std::string strStatusMsg = "Success";
		bool bVerifyPairState = VerifyPairStatus(updateMsgStream);

        // m_AppProtectionSettings gets populated in VerifyPairStatus,
        // so should always be after that call
        ADD_INT_TO_MAP(tagStatus, NUMOFDISKINPOLICY, m_AppProtectionSettings.Pairs.size());
        std::string diskList;
        std::map<std::string, ReplicationPair>::iterator pairsIter = m_AppProtectionSettings.Pairs.begin();
        while (pairsIter != m_AppProtectionSettings.Pairs.end())
        {
            diskList += pairsIter->first;
            diskList += ",";
            pairsIter++;
        }

        ADD_STRING_TO_MAP(tagStatus, DISKIDLISTINPOLICY, diskList);

        if (bVerifyPairState)
        {
            SV_ULONG exitCode = 0;
            std::string strVacpCmd = "\"" + TagGenerator().getVacpPath() + "\"";
            strVacpCmd += " -systemlevel";
            strVacpCmd += " " + cmdOptions;

            AppCommand vacpCmd(strVacpCmd, lConfig.getConsistencyTagIssueTimeLimit(), outputFileName);

            void *user_handle = 0;
#ifdef WIN32
            user_handle = Controller::getInstance()->getDuplicateUserHandle();
#endif
            ADD_INT_TO_MAP(tagStatus, VACPSTARTTIME, GetTimeInSecSinceEpoch1970());

            if (vacpCmd.Run(exitCode, cmdOutput, Controller::getInstance()->m_bActive, "", user_handle) != SVS_OK)
            {
                retStatus = SVS_FALSE;
            }
            ADD_INT_TO_MAP(tagStatus, VACPENDTIME, GetTimeInSecSinceEpoch1970());

            if (!bIsCrashTag && (VACP_E_DRIVER_IN_NON_DATA_MODE != exitCode))
            {
                sbRetryAppConsistencyOnReboot = false;
            }
            else if (!bMultiVmTag && !bIsCrashTag && (VACP_E_DRIVER_IN_NON_DATA_MODE == exitCode))
            {
                if (!Host::GetInstance().GetSystemUptimeInSec(systemUptime))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: failed to get system uptime\n", FUNCTION_NAME);
                    sbRetryAppConsistencyOnReboot = false;
                }
                else if (systemUptime < lConfig.getAppConsistencyRetryOnRebootMaxTime())
                {
                    ADD_INT_TO_MAP(tagStatus, MESSAGETYPE, TAG_TABLE_AGENT_LOG_APP_RETRY_ON_REBOOT);
                    sbRetryAppConsistencyOnReboot = true;
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s: no app consistency retry on reboot as max retry time has elapsed.\n", FUNCTION_NAME);
                    sbRetryAppConsistencyOnReboot = false;
                }
            }

            std::stringstream failedNodes;
            failedNodes.str("");
            std::size_t start = 0;
            // Using filter "Host:" to identify vacp exit status
            start = cmdOutput.find(HostKey);
            if (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    failedNodes << cmdOutput.substr(start + HostKey.length(), end - (start + HostKey.length())).c_str();
                }
            }
            start = cmdOutput.find(CoordNodeKey);
            while (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    if (!failedNodes.str().empty()) { failedNodes << ", "; } // appending delimeter
                    failedNodes << cmdOutput.substr(start + CoordNodeKey.length(), end - (start + CoordNodeKey.length())).c_str();
                }
                start = cmdOutput.find(CoordNodeKey, end);
            }

            start = cmdOutput.find(NotConnectedNodes);
            if (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    if (!failedNodes.str().empty()) { failedNodes << ", "; }
                    failedNodes << cmdOutput.substr(start, end - start).c_str();
                }
            }

            start = cmdOutput.find(ExcludedNodes);
            if (start != std::string::npos)
            {
                std::size_t end = cmdOutput.find(SearchEndKey, start);
                if (end != std::string::npos)
                {
                    if (!failedNodes.str().empty()) { failedNodes << ", "; }
                    failedNodes << cmdOutput.substr(start, end - start).c_str();
                }
            }

            if (0 != exitCode || !failedNodes.str().empty())
            {
                if (!failedNodes.str().empty())
                {
                    ADD_INT_TO_MAP(tagStatus, MULTIVMSTATUS, VACP_MULTIVM_FAILED);
                    ADD_STRING_TO_MAP(tagStatus, MULTIVMREASON, failedNodes.str());
                }
                bool sendAlert = true;
                bool masterNode = true;
                if (std::string::npos != strVacpCmd.find("-distributed"))
                {
                    std::size_t first = strVacpCmd.find("-mn");
                    std::size_t second = strVacpCmd.find(" ", first + 4);
                    if (strVacpCmd.substr(first + 4, second - first - 4).compare(Host::GetInstance().GetIPAddress()) != 0)
                    {
                        masterNode = false;
                    }
                }
                if (std::string::npos != strVacpCmd.find("-cc"))
                {
                    // flag alert for each app and each 3rd consecutive crash consistency failure
                    if (++ccFailCnt %= 3)
                    {
                        sendAlert = false;
                    }
                }
                bool isNonDataNode = false;
                if (!failedNodes.str().empty())
                {
                    if (std::string::npos != failedNodes.str().find(NonDataModeError))
                    {
                        // this covers for Linux master node as well
                        isNonDataNode = true;
                    }
                }

                boost::shared_ptr<InmAlert> pInmAlert;
                if (std::string::npos != strVacpCmd.find("-cc"))
                {
                    if (VACP_E_DRIVER_IN_NON_DATA_MODE == exitCode || isNonDataNode)
                    {
                        pInmAlert.reset(new CrashConsistencyFailureNonDataModeAlert(m_policyId, failedNodes.str(), strVacpCmd));
                    }
                    else
                    {
                        pInmAlert.reset(new CrashConsistencyFailureAlert(m_policyId, failedNodes.str(), strVacpCmd));
                    }
                }
                else
                {
                    pInmAlert.reset(new AppConsistencyFailureAlert(m_policyId, failedNodes.str(), strVacpCmd));
                }

                if (sendAlert && masterNode && !sbInternalAppConsistencyPolicy)
                {
                    SendAlertToCx(SV_ALERT_SIMPLE, SV_ALERT_MODULE_CONSISTENCY, *pInmAlert);
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Sent alert %s.\n", FUNCTION_NAME, pInmAlert->GetMessage().c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Skipping alert %s.\n", FUNCTION_NAME, pInmAlert->GetMessage().c_str());
                }
            }
            else
            {
                // reset crash consistency failure count when successful
                if (std::string::npos != strVacpCmd.find("-cc"))
                {
                    ccFailCnt = 0;
                }
            }

            ParseCmdOutput(cmdOutput, tagStatus, exitCode);

#ifdef WIN32
            if (user_handle) CloseHandle(user_handle);
#endif
        }
        else
        {
            updateMsgStream << "Skipping consistency" << std::endl;
            std::ofstream out_file(outputFileName.c_str());
            if(out_file.good())
            {
                out_file << updateMsgStream.str() << std::endl;
                cmdOutput = updateMsgStream.str();
                out_file.close();
            }
            strStatusMsg = "Skipped";
            ADD_INT_TO_MAP(tagStatus, FINALSTATUS, TAG_SKIPPED);
        }

        if (!sbInternalAppConsistencyPolicy)
        {
            int nLastNLinesCount = lConfig.getLastNLinesCountToReadFromLogOrBuffer();
            GetLastNLinesFromBuffer(cmdOutput, nLastNLinesCount, strLastNLines);//Collect the last N lines of the consistency job's output. By default, this value is 8
            configuratorPtr->PolicyUpdate(
                m_policyId,
                instanceId,
                strStatusMsg,
                "",
                strLastNLines,
                true);
        }
    }
    catch( std::exception & e)
    {
		DebugPrintf(SV_LOG_ERROR,"Exception while processing Consistency policy. Exception : %s",e.what());
        if (!sbInternalAppConsistencyPolicy)
		    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","","Unmarshal exception for policy data. Exception :"+std::string(e.what()));
    }
    catch ( ... )
    {
		DebugPrintf(SV_LOG_ERROR,"Unknown exception while processing Consistency policy");
        if (!sbInternalAppConsistencyPolicy)
            configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","","Unknown exception while processing Consistency policy");
    }

    try {
        std::string strTagStatus = JSON::producer<TagStatus>::convert(tagStatus);
        gTagTelemetryLog.Printf(strTagStatus);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to serialize tag status for telemetry. %s\n", e.what());
    }

    // run the internal app consistency policy
    if (bIsCrashTag && sbRetryAppConsistencyOnReboot)
    {
        ACE_OS::sleep(1); // to get a unique policy log file name
        sbInternalAppConsistencyPolicy = true;
        OnConsistency();
        sbInternalAppConsistencyPolicy = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

bool CloudController::VerifyPairStatus(std::stringstream & updateMsgStream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = true;
	std::map<std::string, std::string>::iterator iterProp = m_policy.m_policyProperties.find("ScenarioId");
	if(iterProp != m_policy.m_policyProperties.end())
	{
		std::map<std::string, std::string> resync_I_Pairs;
		if(GetPairsOfState(iterProp->second,PAIR_STATE_RESYNC_I,resync_I_Pairs) == SVS_OK)
		{
			if(!resync_I_Pairs.empty())
			{
				bRet = false;
				updateMsgStream << "Following pairs are in Resync-I:" << std::endl;
				std::map<std::string, std::string>::iterator iterPair = resync_I_Pairs.begin();
				for(;iterPair != resync_I_Pairs.end(); iterPair++)
					updateMsgStream << iterPair->first << " -> " << iterPair->second << std::endl;
				updateMsgStream << std::endl;
			}
			else
			{
				updateMsgStream << "Pairs status verified: No pair with Resync-I found" << std::endl;
			}
		}
		else
		{
			updateMsgStream << "Could not get the resync pairs. Issuing consistency tag without validating pair state" << std::endl;
			DebugPrintf(SV_LOG_ERROR,"Could not get the resync pairs. Issuing consistency tag without validating pair state\n");
		}
	}
	else
	{
		updateMsgStream << "ScenarioId is missing in policy properties. Issuing tag without checking pairs resync-state" << std::endl;
		DebugPrintf(SV_LOG_DEBUG,"ScenaruiId is missing in consistency policy properties\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

bool CloudController::UploadFiletoRemoteLocation(const std::string& strRemoteFile, const std::string& strFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bRet = true;

	//Currently configurator initialisation has been placed inside this function.
	//Need to place in a proper call based on requirement.
	if (!InitializeConfig())
	{
		DebugPrintf(SV_LOG_ERROR, "Configurator initialisation failed.");
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
		return false;
	}

	LocalConfigurator config;
	HttpFileTransfer http_ft(config.IsHttps(), GetCxIpAddress(), boost::lexical_cast<std::string>(config.getHttp().port));
	if (http_ft.Init() && http_ft.Upload(strRemoteFile, strFileName))
	{
		DebugPrintf(SV_LOG_DEBUG, "File upload successful.\nRemote-File : %s\nLocal-copy: %s\n", strRemoteFile.c_str(), strFileName.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "File upload Failed.\nRemote-File : %s\nLocal-copy: %s\n", strRemoteFile.c_str(), strFileName.c_str());
		bRet = false;
	}
	UninitializeConfig();

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bRet;
}

bool CloudController::DeleteRecoveryProgressFiles(const SrcCloudVMsInfo & preTgtPolicyData)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bRet = true;

	LocalConfigurator lConfig;
	std::string strPlanDirPath = lConfig.getInstallPath();

#ifdef WIN32
	strPlanDirPath += "\\Failover\\Data\\";
#else
	strPlanDirPath += "/failover_data/";
#endif

	std::map<std::string, SrcCloudVMDisksInfo>::const_iterator iterVM = preTgtPolicyData.m_VmsInfo.begin();
	while (iterVM != preTgtPolicyData.m_VmsInfo.end())
	{
		std::string strFileName = strPlanDirPath + iterVM->first + INM_CLOUD_RECOVERY_STATUS_FILE;
		if (!isFileExited(strFileName))
		{
			DebugPrintf(SV_LOG_DEBUG, "File [%s] does not exist, ignoring the deletion.\n", strFileName.c_str());
		}
		else
		{
			if (-1 == remove(strFileName.c_str()))
			{
				DebugPrintf(SV_LOG_WARNING, "Failed to delete the file - %s\n", strFileName.c_str());
				bRet = false;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "SuccessFully deleted file - %s\n", strFileName.c_str());
			}
		}
		iterVM++;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bRet;
}


bool CloudController::InitializeConfig(ConfigSource configSrc)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bRet = false;
	try
	{
		if (m_pConfig)
		{
			bRet = true;
		}
		else if (InitializeConfigurator(&m_pConfig, configSrc, PHPSerialize))
		{
			m_pConfig->Start();
			bRet = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to initialize configurator\n");
			m_pConfig = NULL;
		}
	}
	catch (ContextualException& ce)
	{
		DebugPrintf(SV_LOG_ERROR, " encountered exception. %s\n", ce.what());
	}
	catch (std::exception const& e)
	{
		DebugPrintf(SV_LOG_ERROR, " encountered exception. %s\n", e.what());
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, " encountered unknown exception.\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bRet;
}


void CloudController::UninitializeConfig()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	try
	{
		if (m_pConfig)
		{
			m_pConfig->Stop();
			m_pConfig = NULL;
		}
	}
	catch (ContextualException& ce)
	{
		DebugPrintf(SV_LOG_ERROR, " encountered exception. %s\n", ce.what());
	}
	catch (std::exception const& e)
	{
		DebugPrintf(SV_LOG_ERROR, " encountered exception. %s\n", e.what());
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, " encountered unknown exception.\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}



#ifndef WIN32

// A2E Failback Prepate Target changes for Linux
// Windows is covered in EsxUtilWind (HandleMasterTarget.cpp)
// note: this needs to be moved platform specific implementation file
//       and remove the #ifdef macros
// ===========================================================

SVERROR CloudController::PrepareTargetForProtection(const std::string & virtualizationPlatform,
	const SrcCloudVMsInfo & protectionDetails,
	std::stringstream & serializedResponse)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool rv = false;

	try {

		// Note: Prepare Target response uses different structures for success and failure update to CS
		//  Success : CloudVMsDisksVolsMap
		//  Failure : VMsPrepareTargetUpdateInfo

		CloudVMsDisksVolsMap       successUpdate;

		std::string MTHostName = Host::GetInstance().GetHostName();

		// step 1 : refresh device entries for the target luns
		//          echo "scsi remove-single-device [h:c:t:l]" > /proc/scsi/scsi
		//          echo "scsi add-single-device [h:c:t:l]" > /proc/scsi/scsi
		//
		//


		std::map<std::string, SrcCloudVMDisksInfo>::const_iterator m_VmsIter = protectionDetails.m_VmsInfo.begin();
		for (; m_VmsIter != protectionDetails.m_VmsInfo.end(); ++m_VmsIter)
		{
			srcdiskTgtlunMap_t srctgtLunMap = m_VmsIter->second.m_vmSrcTgtDiskLunMap;

			RefreshDeviceNodes(virtualizationPlatform, m_VmsIter->first, srctgtLunMap);
		}

		//
		// step 2: gather disk devices on the system and corresponding hctl
		//

		hctlDeviceMap_t hctlDeviceMap;
		CollectDiskDevices(hctlDeviceMap);

		// step 3: Find Matching disks and prepare the mapping
		//       input : src_device_name =>Lun
		//       output: src_device_name =>Tgt_device_name
		//

		for (m_VmsIter = protectionDetails.m_VmsInfo.begin(); m_VmsIter != protectionDetails.m_VmsInfo.end(); ++m_VmsIter)
		{
			// src_device_name =>Lun 
			srcdiskTgtlunMap_t srctgtLunMap = m_VmsIter->second.m_vmSrcTgtDiskLunMap;
			
			// src_device_name => target native device (eg: /dev/sda)                 
			srcdiskTgtDiskMap_t srctgtNativeDiskMap;

			// src_device_name => dev mapper target device (eg: /dev/mapper/<scsi id>)
			srcdiskTgtDMDiskMap_t srctgtDMDiskMap;

			FindMatchingDisks(virtualizationPlatform, m_VmsIter->first, hctlDeviceMap, srctgtLunMap, srctgtNativeDiskMap);
			FindCorrespondingDevMapperDisks(m_VmsIter->first, srctgtNativeDiskMap, srctgtDMDiskMap);
			successUpdate.m_volumeMapping.insert(std::make_pair(m_VmsIter->first, srctgtDMDiskMap));
		}

		// step 4: convert to serliazed format for sending to CS

		serializedResponse.str("");
		serializedResponse << CxArgObj<CloudVMsDisksVolsMap>(successUpdate);
		rv = true;
	}
	catch (const AppFrameWorkException & e)
	{

		// Note: Prepare Target response uses different structures for success and failure update to CS
		//  Success : CloudVMsDisksVolsMap
		//  Failure : VMsPrepareTargetUpdateInfo
		//
		VMsPrepareTargetUpdateInfo failureUpdate;
		PlaceHolder placeHolder;
		failureUpdate.m_updateProps[INM_CLOUD_ERROR_CODE] = e.code();
		failureUpdate.m_updateProps[INM_CLOUD_ERROR_MSG] = e.what();
		failureUpdate.m_updateProps[INM_CLOUD_LOG_FILE] = getPolicyFileName(m_policyId);

		placeHolder.m_Props[INM_CLOUD_MT_HOSTNAME] = Host::GetInstance().GetHostName();
		placeHolder.m_Props[INM_CLOUD_ERROR_CODE] = e.code();
		failureUpdate.m_PlaceHolderProps[INM_CLOUD_PLACE_HOLDER] = placeHolder;

		serializedResponse.str("");
		serializedResponse << cxArg<VMsPrepareTargetUpdateInfo>(failureUpdate);
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return (rv ? SVS_OK : SVE_FAIL);
}

void CloudController::RefreshDeviceNodes(const std::string & virtualizationPlatform,
	const std::string & vm,
	const srcdiskTgtlunMap_t &srcdiskTgtlunMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, vm.c_str());
	
	std::string errorCode;
	std::stringstream errorMsg;
	
	std::string executionLogName = getOutputFileName(m_policyId);
	std::ofstream o(executionLogName.c_str(), std::ios::app);
	if (o.bad())
	{
		// log error and continue;
		DebugPrintf(SV_LOG_ERROR, "%s failed to open %s with error %d.\n",
			FUNCTION_NAME, executionLogName.c_str(), errno);
	}

	o << "Refreshing device nodes for VM " << vm << "\n";
	o.flush();
	o.close();


	LocalConfigurator lConfig;
	std::string deviceRefreshScript = lConfig.getInstallPath() + DEVICE_REFRESH_SCRIPT;

	// loop through src_disk => tgt lun map
	// convert lun to hctl format
	// append it a command line parameter to the device refresh script

	std::string commandLineArgs;
	srcdiskTgtlunMap_t::const_iterator srctgtIter = srcdiskTgtlunMap.begin();

	for (; srctgtIter != srcdiskTgtlunMap.end(); ++srctgtIter)
	{
		std::string srcdisk = srctgtIter->first;
		std::string tgtLun = srctgtIter->second;

		commandLineArgs += " ";
		commandLineArgs += GetHctl(virtualizationPlatform, vm, tgtLun);
	}


	deviceRefreshScript += commandLineArgs;

	SV_ULONG exitCode = 0;
	std::string strOutput;
	AppCommand cmd(deviceRefreshScript, 0, executionLogName);
	SVERROR svretCode = cmd.Run(exitCode, strOutput, Controller::getInstance()->m_bActive);
	if (svretCode != SVS_OK || exitCode)
	{
		errorCode = "EA0519";
		errorMsg << "Protection could not be enabled.\n";

		if (svretCode == SVS_OK) {
			errorMsg << "device refresh script failed with exit code " << exitCode << ".\n";
		} else {
			errorMsg << "unable to execute device refresh script.\n";
		}

		errorMsg << "Possible Causes: Enable protection failed on the master target server "
			<< Host::GetInstance().GetHostName()
			<< " with error code "
			<< errorCode << ".\n"
			<< "There might be some required RPMs missing on the master target server.\n"
			<< "Recommended Action: Install required RPMs by following the Linux Master Target preparation guide at http ://go.microsoft.com/fwlink/?LinkId=613319.\n";

		throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, vm.c_str());
}

std::string CloudController::GetHctl(const std::string & virtualizationPlatform,
	const std::string &vm,
	const std::string & lunId)
{

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for vm %s lun %s\n", FUNCTION_NAME, vm.c_str(), lunId.c_str());

	std::string hctl;
	std::string errorCode;
	std::stringstream errorMsg;

	if (strcmpi(virtualizationPlatform.c_str(), "vmware") == 0){
		svector_t tokens = CDPUtil::split(lunId, ":", 2);
		if (tokens.size() == 2)	{
			CDPUtil::trim(tokens[0]);
			CDPUtil::trim(tokens[1]);
			hctl = tokens[0] + ":0:" + tokens[1] + ":0";

			DebugPrintf("vm:%s lun:%s => hctl:%s\n", vm.c_str(), lunId.c_str(), hctl.c_str());
		}
		else {


			errorCode = "EA0518";
			errorMsg << "Protection could not be enabled.\n"
				<< "Invalid lun id " << lunId << " for VM " << vm << ".\n"
				<< "Possible Causes: Enable protection failed on the master target server "
				<< Host::GetInstance().GetHostName()
				<< " with error code "
				<< errorCode << ".\n"
				<< "Either the disks are not attached or not identified by the master target server or the LUN information passed is incorrect.\n"
				<< "Recommended Action: Ensure that the disks are properly attached to the master target server and reboot the master target server. If the issue persists, disable the protection and retry the operation.\n";
			throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());
		}
	}
	else
	{
		errorCode = "EA0518";
		errorMsg << "Protection could not be enabled.\n"
			<< "unsupported platform " << virtualizationPlatform << " for VM " << vm << ".\n"
			<< "Possible Causes: Enable protection failed on the master target server "
			<< Host::GetInstance().GetHostName()
			<< " with error code "
			<< errorCode << ".\n"
			<< "Either the disks are not attached or not identified by the master target server or the LUN information passed is incorrect.\n"
			<< "Recommended Action: Ensure that the disks are properly attached to the master target server and reboot the master target server. If the issue persists, disable the protection and retry the operation.\n";

		throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s for vm %s lun %s\n", FUNCTION_NAME, vm.c_str(), lunId.c_str());
	return hctl;
}

void CloudController::CollectDiskDevices(hctlDeviceMap_t & deviceHctlMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	std::string lscsicmd = LSSCSI_SCRIPT;
	std::stringstream results;
	bool rv = false;

	rv = executePipe(lscsicmd, results);
	
	if(!rv || results.str().empty()) {

		std::string errorCode = "EA0519";
		std::stringstream errorMsg;

		errorMsg << "Protection could not be enabled.\n"
			<< "Failed to list scsi device using " << lscsicmd << ".\n"
			<< "Possible Causes: Enable protection failed on the master target server "
			<< Host::GetInstance().GetHostName()
			<< " with error code "
			<< errorCode << ".\n"
			<< "There might be some required RPMs missing on the master target server.\n"
			<< "Recommended Action: Install required RPMs by following the Linux Master Target preparation guide at http ://go.microsoft.com/fwlink/?LinkId=613319.\n";
		throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());		
	}

	std::string lineRead;
	while(std::getline(results,lineRead))
	{
		std::string device_name,lun,target_number,channel,scsi_host;
		if(ProcessLsScsiCmdLine(lineRead,device_name,lun,target_number,channel,scsi_host) != SVS_OK) {

			std::string errorCode = "EA0519";
			std::stringstream errorMsg;

			errorMsg << "Protection could not be enabled.\n"
				<< "Failed to parse " << lscsicmd << " output.\n"
				<< "Output: " << results.str() << "\n"
				<< "unexpected line:" << lineRead << "\n"
				<< "Possible Causes: Enable protection failed on the master target server "
				<< Host::GetInstance().GetHostName()
				<< " with error code "
				<< errorCode << ".\n"
				<< "There might be some required RPMs missing on the master target server.\n"
				<< "Recommended Action: Install required RPMs by following the Linux Master Target preparation guide at http ://go.microsoft.com/fwlink/?LinkId=613319.\n";
			throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());	
		}

		std::string hctl = scsi_host + ":" + channel + ":" + target_number + ":" + lun;
		deviceHctlMap.insert(std::make_pair(hctl,device_name));
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void CloudController::FindMatchingDisks(const std::string & virtualizationPlatform,
	const std::string & vm,
	const hctlDeviceMap_t & hctlDeviceMap,
	const srcdiskTgtlunMap_t & srcdiskTgtLunMap,
	srcdiskTgtDiskMap_t & srcdiskTgtNativeDiskMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, vm.c_str());

	// step 1 : loop through the src_device => lun map
	//  for each lun, get hctl and find the corresponding native disk device
	//  

	srcdiskTgtlunMap_t::const_iterator srctgtlunIter = srcdiskTgtLunMap.begin();
	for (; srctgtlunIter != srcdiskTgtLunMap.end(); ++srctgtlunIter)
	{
		std::string srcDisk = srctgtlunIter->first;
		std::string lun = srctgtlunIter->second;

		std::string nativeDevice = FindMatchingDisk(virtualizationPlatform,
			vm,
			lun,
			hctlDeviceMap);

		DebugPrintf(SV_LOG_DEBUG, "vm: %s srcdisk: %s lun:%s native device:%s.\n",
			vm.c_str(),
			srcDisk.c_str(),
			lun.c_str(),
			nativeDevice.c_str());

		srcdiskTgtNativeDiskMap.insert(std::make_pair(srcDisk, nativeDevice));
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, vm.c_str());
}


std::string CloudController::FindMatchingDisk(const std::string & virtualizationPlatform,
	const std::string & vm,
	const std::string & lun,
	const hctlDeviceMap_t & hctlDeviceMap
	)
{
	std::string lunWoSpaces = lun;
	CDPUtil::trim(lunWoSpaces);
	std::string hctl = GetHctl(virtualizationPlatform, vm, lunWoSpaces);
	hctlDeviceMap_t::const_iterator hctlDeviceIter = hctlDeviceMap.find(hctl);
	if (hctlDeviceIter == hctlDeviceMap.end())
	{
		std::string  errorCode = "EA0518";
		std::stringstream errorMsg;
		errorMsg << "Protection could not be enabled.\n"
			<< "could not detect device for " << lun << " VM " << vm << ".\n"
			<< "Possible Causes: Enable protection failed on the master target server "
			<< Host::GetInstance().GetHostName()
			<< " with error code "
			<< errorCode << ".\n"
			<< "Either the disks are not attached or not identified by the master target server or the LUN information passed is incorrect.\n"
			<< "Recommended Action: Ensure that the disks are properly attached to the master target server and reboot the master target server. If the issue persists, disable the protection and retry the operation.\n";
		throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());
	}
	return hctlDeviceIter->second;
}

void CloudController::FindCorrespondingDevMapperDisks(const std::string & vm,
	const srcdiskTgtDiskMap_t & srcdiskTgtNativeDiskMap,
	srcdiskTgtDMDiskMap_t & srcTgtDMDiskMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for %s\n", FUNCTION_NAME, vm.c_str());

	// step 1 : loop through the src_device => tgt native device map
	//  for each device, get scsi id and construct the dev mapper disk device
	//  

	srcdiskTgtDiskMap_t::const_iterator srctgtDiskIter = srcdiskTgtNativeDiskMap.begin();
	for (; srctgtDiskIter != srcdiskTgtNativeDiskMap.end(); ++srctgtDiskIter)
	{
		std::string srcDisk = srctgtDiskIter->first;
		std::string nativeDisk = srctgtDiskIter->second;

		// fetch the scsi_id for matched device and get the scsi id of  the disk 
		DeviceIDInformer f;
		std::string scsiId = f.GetID(nativeDisk);
		if (scsiId.empty())
		{
			std::string errorCode = "EA0519";
			std::stringstream errorMsg;

			errorMsg << "Protection could not be enabled.\n"
				<< "Failed to get scsi id for " << nativeDisk << ".\n"
				<< "Possible Causes: Enable protection failed on the master target server "
				<< Host::GetInstance().GetHostName()
				<< " with error code "
				<< errorCode << ".\n"
				<< "There might be some required RPMs missing on the master target server.\n"
				<< "Recommended Action: Install required RPMs by following the Linux Master Target preparation guide at http ://go.microsoft.com/fwlink/?LinkId=613319.\n";

			throw APPFRAMEWORK_EXCEPTION(errorCode, errorMsg.str());
		}

		std::string dmDevice = "/dev/mapper/" + scsiId;
		
		DebugPrintf(SV_LOG_DEBUG, "vm: %s srcdisk: %s tgtdisk:%s dev mapper device:%s.\n",
			vm.c_str(),
			srcDisk.c_str(),
			nativeDisk.c_str(),
			dmDevice.c_str());

		srcTgtDMDiskMap.insert(std::make_pair(srcDisk, dmDevice));
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s for %s\n", FUNCTION_NAME, vm.c_str());
}

#endif

SVERROR CloudController::OnRcmRegistration()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    ExtendedErrorLogger::ExtendedErrors extendedErrors;
    std::string errMsg;
    bool statusUpdated = false;
    try
    {
        std::string policyId = boost::lexical_cast<std::string>(m_policyId);
        RcmRegistratonMachineInfo rcmMachineInfo = JSON::consumer<RcmRegistratonMachineInfo>::convert(m_policy.m_policyData, true);
        MigrationHelper mh;
		
		int retryCount = 0;
		while (SVS_OK != (status = mh.RegisterAgent(rcmMachineInfo, extendedErrors)))
		{			
			if (++retryCount >= Migration::MaxRetryCount)
			{
				if (extendedErrors.Errors.empty())
				{
					errMsg += mh.GetErrorMessage();
					std::map<std::string, std::string> errorMap;
					errorMap[APPLIANCE_FQDN] = rcmMachineInfo.ApplianceFqdn;
					UpdateGenericError(extendedErrors, Migration::RCM_REGISTRATION_POLICY_FAILURE, errMsg, errorMap);
				}
				errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(extendedErrors);
				DebugPrintf(SV_LOG_ERROR, "%s Sending failure for policy with id=%s after retries=%d and last error message=%s\n", FUNCTION_NAME, policyId.c_str(), retryCount, errMsg.c_str());
				configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg, true);
				break;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "%s policy with id=%s, failed with error message=%s, retry attempt=%d\n",
					FUNCTION_NAME, policyId.c_str(), mh.GetErrorMessage().c_str(), retryCount);
			}

			ACE_OS::sleep(30);
		}

        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s policy with policy id=%s, success.\n", FUNCTION_NAME, policyId.c_str());
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Success", "", "", true);
        }

        statusUpdated = true;
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to unmarshall RcmRegistratonMachineInfo with exception %s.\n", FUNCTION_NAME, jsone.what());
        errMsg += "Failed to unmarshall RcmRegistratonMachineInfo policy data. Exception :" + std::string(jsone.what());
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmRegistration policy with exception %s.\n", FUNCTION_NAME, e.what());
        errMsg += "Failed to process RcmRegistration policy. Exception :" + std::string(e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmRegistration policy with an unknown exception.\n", FUNCTION_NAME);
        errMsg += "Unknown exception while processing RcmRegistration policy.";
    }

    if (!statusUpdated)
    {
        UpdateGenericError(extendedErrors, Migration::RCM_REGISTRATION_POLICY_FAILURE, errMsg);
        errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(extendedErrors);
        configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVERROR CloudController::GetProtectedDiskList(std::set<std::string> &protectedDiskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    std::stringstream strErrMsg;

    if(!VerifyPairStatus(strErrMsg))
    {
        DebugPrintf(SV_LOG_ERROR, "%s : Verify pair status failed with error : %s\n",
            FUNCTION_NAME, strErrMsg.str().c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return SVE_FAIL;
    }
    // m_AppProtectionSettings gets populated in VerifyPairStatus,
    // so should always be after that call
    std::map<std::string, ReplicationPair>::iterator pairsIter = m_AppProtectionSettings.Pairs.begin();
    while (pairsIter != m_AppProtectionSettings.Pairs.end())
    {
        protectedDiskIds.insert(pairsIter->first);
        pairsIter++;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void CloudController::UpdateGenericError(ExtendedErrorLogger::ExtendedErrors &extendedErrors,
    std::string errorName, std::string &errMsg, std::map<std::string, std::string> errorMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ExtendedErrorLogger::ExtendedError extendedError(errorName, errorMap, std::string());
    extendedError.error_params[INM_CLOUD_ERROR_MSG] = errMsg;
    extendedError.error_params[VM_NAME] = RcmConfigurator::getInstance()->GetFqdn();
    extendedErrors.Errors.push_back(extendedError);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVERROR CloudController::OnRcmFinalReplicationCycle()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    std::set<std::string> protectedDiskIds;
    ExtendedErrorLogger::ExtendedErrors extendedErrors;
    std::string errMsg;
    bool statusUpdated = false;

    try
    {
        do
        {
            status = GetProtectedDiskList(protectedDiskIds);
            if (status != SVS_OK)
            {
                ExtendedErrorLogger::ExtendedError extendedError;
                extendedError.error_name = "VerifyPairFailed";
                extendedError.error_name = Migration::VERIFY_PAIR_FAILED;
                extendedErrors.Errors.push_back(extendedError);
                break;
            }
            RcmFinalReplicationCycleInfo replicationCycleInfo = JSON::consumer<RcmFinalReplicationCycleInfo>::convert(m_policy.m_policyData, true);
            std::string policyId = boost::lexical_cast<std::string>(m_policyId);
            MigrationHelper mh(true);

            status = mh.FinalReplicationCycle(replicationCycleInfo, policyId,
                protectedDiskIds, extendedErrors);
            errMsg = mh.GetErrorMessage();

        } while (false);

        if (status == SVS_OK)
        {
            //errMsg contains the bookmark output details here.
            //This is updated by MigrationHelper while setting the bookmark
            DebugPrintf(SV_LOG_ALWAYS, "%s policy with policy=%lu, instanceId=%llu, success.\n",
                FUNCTION_NAME, m_policyId, instanceId);
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Success", "", errMsg, true);
        }
        else
        {
            if (extendedErrors.Errors.empty())
            {
                UpdateGenericError(extendedErrors, Migration::FINAL_REPLICATION_CYCLE_FAILURE, errMsg);
            }
            errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(
                extendedErrors);
            DebugPrintf(SV_LOG_ERROR, "%s RcmFinalReplicationCycleInfo failed with errors : %s.\n",
                FUNCTION_NAME, errMsg.c_str());
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg, true);
        }
        statusUpdated = true;
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to unmarshall RcmFinalReplicationCycleInfo with exception %s.\n", FUNCTION_NAME, jsone.what());
        errMsg = "Failed to unmarshall RcmFinalReplicationCycleInfo policy data. Exception :" + std::string(jsone.what());
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmFinalReplicationCycle policy with exception %s.\n", FUNCTION_NAME, e.what());
        errMsg = "Failed to process RcmFinalReplicationCycle policy. Exception :" + std::string(e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmFinalReplicationCycle policy with an unknown exception.\n", FUNCTION_NAME);
        errMsg = "Unknown exception while processing RcmFinalReplicationCycle policy";
    }

    if (!statusUpdated)
    {
        UpdateGenericError(extendedErrors, Migration::FINAL_REPLICATION_CYCLE_FAILURE, errMsg);
        errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(extendedErrors);
        configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVERROR CloudController::OnRcmResumReplication()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    std::set<std::string> protectedDiskIds;
    ExtendedErrorLogger::ExtendedErrors extendedErrors;
    std::string errMsg;
    bool statusUpdated = false;

    try
    {
        do
        {
            //Continue even if pairs are in IR/resync
            GetProtectedDiskList(protectedDiskIds);

            MigrationHelper mh(true);
            status = mh.ResumeReplication(protectedDiskIds, extendedErrors);
            errMsg = mh.GetErrorMessage();

        } while (false);

        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s policy with policy id=%lu, instanceId=%llu success.\n",
                FUNCTION_NAME, m_policyId, instanceId);
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Success", "", "", true);
        }
        else
        {
            if (extendedErrors.Errors.empty())
            {
                UpdateGenericError(extendedErrors, Migration::RESUME_REPLICATION_FAILURE, errMsg);
            }
            errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(
                extendedErrors);
            DebugPrintf(SV_LOG_ERROR, "%s RcmResumeReplicationInfo failed with errors : %s.\n",
                FUNCTION_NAME, errMsg.c_str());
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg);
        }
        statusUpdated = true;
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to unmarshall RcmResumeReplicationInfo with exception %s.\n", FUNCTION_NAME, jsone.what());
        errMsg = "Failed to unmarshall RcmResumeReplicationInfo policy data. Exception :" + std::string(jsone.what());
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmResumReplication policy with exception %s.\n", FUNCTION_NAME, e.what());
        errMsg = "Failed to process RcmResumReplication policy. Exception :" + std::string(e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmResumReplication policy with an unknown exception.\n", FUNCTION_NAME);
        errMsg = "Unknown exception while processing RcmResumReplication policy";
    }

    if (!statusUpdated)
    {
        UpdateGenericError(extendedErrors, Migration::RESUME_REPLICATION_FAILURE, errMsg);
        errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(extendedErrors);
        configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg, true);
    }

    //For resume replication internal policy, set rollback state
    if (m_policyId == RESUME_REPLICATION_POLICY_INTERNAL)
    {
        LocalConfigurator lc;
        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : Migration Rollback succeeded.\n", FUNCTION_NAME);
            lc.setMigrationState(Migration::MIGRATION_ROLLBACK_SUCCESS);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s : Migration Rollback failed with errors : %s.\n",
                FUNCTION_NAME, errMsg.c_str());
            lc.setMigrationState(Migration::MIGRATION_ROLLBACK_FAILED);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVERROR CloudController::OnRcmMigration()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR status = SVS_OK;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    ExtendedErrorLogger::ExtendedErrors extendedErrors;
    std::string errMsg;
    bool statusUpdated = false;

    try
    {
        MigrationHelper mh;
        status = mh.Migrate(extendedErrors);
        errMsg = mh.GetErrorMessage();

        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s policy with policy id=%lu, instanceId=%llu success.\n",
                FUNCTION_NAME, m_policyId, instanceId);
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Success", "", "", true);
        }
        else
        {
            if (extendedErrors.Errors.empty())
            {
                UpdateGenericError(extendedErrors, Migration::RCM_MIGRATION_POLICY_FAILURE, errMsg);
            }
            errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(
                extendedErrors);
            DebugPrintf(SV_LOG_ERROR, "%s RcmMigrationPolicy failed with errors : %s.\n",
                FUNCTION_NAME, errMsg.c_str());
            configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg);
        }
        statusUpdated = true;
    }
    catch (const JSON::json_exception& jsone)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to unmarshall RcmMigrationInfo with exception %s.\n", FUNCTION_NAME, jsone.what());
        errMsg = "Failed to unmarshall RcmMigrationInfo policy data. Exception :" + std::string(jsone.what());
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmMigration policy with exception %s.\n", FUNCTION_NAME, e.what());
        errMsg = "Failed to process RcmMigration policy. Exception :" + std::string(e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to process RcmMigration policy with an unknown exception.\n", FUNCTION_NAME);
        errMsg = "Unknown exception while processing RcmMigration policy";
    }

    if (!statusUpdated)
    {
        UpdateGenericError(extendedErrors, Migration::RCM_MIGRATION_POLICY_FAILURE, errMsg);
        errMsg = JSON::producer<ExtendedErrorLogger::ExtendedErrors>::convert(extendedErrors);
        configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "", errMsg, true);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
