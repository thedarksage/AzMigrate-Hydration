#include "HandleMasterTarget.h"
#include "Common.h"
#include "VsnapUser.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "curlwrapper.h"

#include "scopeguard.h"
#include "disk.h"

/******************************************************************************************************
This will process the CX CLI calls and Stores the output in a file temp_InMage_Esx_<timestamp>.txt
Here currently we are processing two Cx CLI calls:
3--> to get the host info, from theer we will fetch the latest CX ip.
8--> to check the volume is registered or not.
******************************************************************************************************/
bool MasterTarget::ProcessCxCliCall(const string strCxUrl, string strInPut, bool isTargetVol, string &strOutPut)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	static int iWaitCount = 1;
	if(isTargetVol)
	{
		RemoveTrailingCharactersFromVolume(strInPut);
		DebugPrintf(SV_LOG_INFO,"Volume to be searched in CX database = %s\n",strInPut.c_str());
	}
LABEL:
	bool bResult = true;
	std::string strOperation;
	if(isTargetVol)
		strOperation = "8";
	else
		strOperation = "3";
	std::string strCommand;
	std::string strCxCliPath ;
	std::string strTimeStamp = GeneratreUniqueTimeStampString();
	std::string strTempFileNameWithTimeStamp  = string("temp_InMage_Esx_") + strOperation + strTimeStamp + string(".txt");
	strCxCliPath = m_strInstallDirectoryPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("cxcli.exe");
	std::string strXmlFile,strXmlBatFile,strTempXmlFile,strTemp;
	strXmlFile = m_strInstallDirectoryPath + string("\\Failover") + string("\\data\\") + std::string("temp_for_cxCLi.Xml");
    strXmlBatFile = m_strInstallDirectoryPath + string("\\Failover") + string("\\data\\")+ std::string("InMage_Esx_") + strTimeStamp + std::string(".bat");
	strTempXmlFile = m_strInstallDirectoryPath + string("\\Failover") + string("\\data\\") + strTempFileNameWithTimeStamp;
	ofstream outfile;
	int i =0;
	for(; i < 2; i++)
	{
		outfile.open(strXmlFile.c_str(),std::ios::out);
		if(outfile.fail())
		{
#ifdef WIN32 
			DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing Error: %d...Retrying to open the file again.\n", strXmlFile.c_str() , GetLastError() );
#else
			DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing...Retrying to open the file again.\n",strXmlFile.c_str());
#endif
			Sleep(2000);
		}
		else
			break;
	}
	if(i == 2)
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing.\n",strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}

	outfile<<"Inmage_Temp_file_for_cxcli"<<endl;
	outfile.close();
	
	m_strCommandToExecute = string("\"") + strCxCliPath + string("\"") + string(" ") + strCxUrl + string(" ") + string("\"") + strXmlFile + string("\"") + string(" ") + strOperation;
    DebugPrintf(SV_LOG_INFO,"Command to run = %s\n",m_strCommandToExecute.c_str());

	ofstream outBatFile;
	i =0;
	for(; i < 2; i++)
	{
		outBatFile.open(strXmlBatFile.c_str(),std::ios::out);	
		if(outBatFile.fail())
		{
#ifdef WIN32 
			DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing Error: %d...Retrying to open the file again.\n",strXmlBatFile.c_str(),GetLastError());
#else
			DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing...Retrying to open the file again.\n",strXmlBatFile.c_str());
#endif
			Sleep(2000);
		}
		else
			break;
	}
	if(i == 2)
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing.\n",strXmlBatFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	strTemp = string("\"") + strTempXmlFile + string("\"");
	outBatFile<<m_strCommandToExecute<< " "<< " > "<< strTemp;
	outBatFile.close();
//Make it separate module....
	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	memset(&StartupInfo, 0x00, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	memset(&ProcessInformation, 0x00, sizeof(PROCESS_INFORMATION));
	if(!::CreateProcess(NULL,const_cast<char*>(strXmlBatFile.c_str()),NULL,NULL,FALSE,NULL/*CREATE_DEFAULT_ERROR_MODE|CREATE_SUSPENDED*/,NULL, NULL, &StartupInfo, &ProcessInformation))
	{
		DebugPrintf(SV_LOG_ERROR,"CreateProcess failed with Error : [%lu].\n",GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	//Sleep(100000);
	DWORD retValue = WaitForSingleObject( ProcessInformation.hProcess, INFINITE );
	if(retValue == WAIT_FAILED)
	{
		 DebugPrintf(SV_LOG_ERROR,"WaitForSingleObject has failed.\n");
		 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		::CloseHandle(ProcessInformation.hProcess);
		::CloseHandle(ProcessInformation.hThread);
		return true;
	}
	::CloseHandle(ProcessInformation.hProcess);
	::CloseHandle(ProcessInformation.hThread);
	bool bMatchFound;
	if(isTargetVol)
	{
		bMatchFound = bSearchVolume(strTempXmlFile,strInPut);
		DebugPrintf(SV_LOG_INFO,"bMatchFound = %d\n",bMatchFound);	
		if(bMatchFound )
		{
			DebugPrintf(SV_LOG_INFO,"Volume : %s found.\n",strInPut.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Volume : %s is not registered with CX till now. Going to sleep for 30 seconds and check after that.\n",strInPut.c_str());		
			if(iWaitCount >= 30) //30 -> 30 * 30 sec = 15 mins.
			{
				DebugPrintf(SV_LOG_ERROR,"Reached the limit of wait time(15 minutes) overall. So no more retries.\n");
				bResult = false;
			}
			else
			{
				Sleep(30000);//30 seconds.
				iWaitCount++;
				goto LABEL;
			}
		}
	}
	else
	{
		bMatchFound = GetHostIpFromXml(strTempXmlFile, strInPut, strOutPut);
		DebugPrintf(SV_LOG_INFO,"bMatchFound = %d\n",bMatchFound);	
		if(bMatchFound)
		{
			DebugPrintf(SV_LOG_INFO,"Got the IP %s For host %s using CXCLI call 3\n",strOutPut.c_str(), strInPut.c_str());
		}
		else
		{
			bResult = false;
		}
	}
    DebugPrintf(SV_LOG_INFO,"Deleting temp files...\n");
    //Previously the following delete lines are commented.
	//For CX bug..Replication pair is not set for some of the volumes...
	::DeleteFile(strXmlBatFile.c_str());
	::DeleteFile(strXmlFile.c_str());
	::DeleteFile(strTempXmlFile.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//It initilises the protection task status structure.
//It will initilises with the defauly values initially
void MasterTarget::InitSnapshotTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	objTaskInfo.TaskFixSteps	= "";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	objTaskInfo.TaskFixSteps	= "";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_6;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_6;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_6_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_7;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_7;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_7_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_TaskUpdateInfo.HostId			= getInMageHostId();
	m_TaskUpdateInfo.PlanId			= m_strPlanId;
	m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//It initilises the protection task status structure.
//It will initilises with the defauly values initially
void MasterTarget::InitProtectionTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath +  "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_TaskUpdateInfo.HostId			= getInMageHostId();
	m_TaskUpdateInfo.PlanId			= m_strPlanId;
	m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//It initilises the protection task status structure.
//It will initilises with the defauly values initially
void MasterTarget::InitVolResizeTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath +  "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_6;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_6;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_6_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtilWin.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_TaskUpdateInfo.HostId			= getInMageHostId();
	m_TaskUpdateInfo.PlanId			= m_strPlanId;
	m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}
bool MasterTarget::PersistPrepareTargetVMDisksResult(const std::string &strFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	if(!strFileName.empty())
	{
		ofstream out(strFileName.c_str());
		if(out.good())
		{
			try
			{
				out << CxArgObj<CloudVMsDisksVolsMap>(m_prepTgtVMsUpdate);
			}
			catch(std::exception &e)
			{
				DebugPrintf(SV_LOG_ERROR,"Clould not unserialize VM Disks info. Exception: %s\n",e.what());
				bRet = false;
			}
			catch( ... )
			{
				DebugPrintf(SV_LOG_ERROR,"Clould not unserialize VM Disks info. Unknown Exception\n");
				bRet = false;
			}
			out.close();
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Clould not open file for reading. File : %s\n",strFileName.c_str());
			bRet = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Config file path is missing.");
		bRet = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
bool MasterTarget::GetVmDisksInfoFromConfigFile(const std::string &strFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	if(!strFileName.empty())
	{
		ifstream in(strFileName.c_str());
		if(in.good())
		{
			std::stringstream filebuff;
			filebuff << in.rdbuf();
			try
			{
				m_prepTgtVmsInfo = unmarshal<SrcCloudVMsInfo>(filebuff.str());
			}
			catch(std::exception &e)
			{
				DebugPrintf(SV_LOG_ERROR,"Clould not unserialize VM Disks info. Exception: %s\n",e.what());
				bRet = false;
			}
			catch( ... )
			{
				DebugPrintf(SV_LOG_ERROR,"Clould not unserialize VM Disks info. Unknown Exception\n");
				bRet = false;
			}
			in.close();
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Clould not open file for reading. File : %s\n",strFileName.c_str());
			bRet = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Config file path is missing.");
		bRet = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
bool MasterTarget::DowloadAndReadDiskInfoFromBlobFiles(std::string& strErrorMsg, std::string& strErrorCode)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	//Get the list of hosts and its Host-ids
	map<string,SrcCloudVMDisksInfo>::const_iterator iterVM = m_prepTgtVmsInfo.m_VmsInfo.begin();
	while(iterVM != m_prepTgtVmsInfo.m_VmsInfo.end())
	{
		string hostName;
		if(iterVM->second.m_vmProps.find("HostName") != iterVM->second.m_vmProps.end())
			hostName = iterVM->second.m_vmProps.find("HostName")->second;
		else
			DebugPrintf(SV_LOG_DEBUG,"HostName is missing for the host : %s\n",iterVM->first.c_str());

		DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
		if( iterVM->second.m_vmProps.find("MBRFilePath") != iterVM->second.m_vmProps.end() &&
			!iterVM->second.m_vmProps.find("MBRFilePath")->second.empty())
		{
			string mbrFileUrl = iterVM->second.m_vmProps.find("MBRFilePath")->second;
			string mbrFilePathLocal = m_strDataFolderPath + "\\" + iterVM->first + "_mbrdata";
			if(!m_bPhySnapShot && (!DownloadFileFromCx(mbrFileUrl,mbrFilePathLocal) || !checkIfFileExists(mbrFilePathLocal)) )
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to download Disk Info blob file from cx. File : %s\n",mbrFileUrl.c_str());
				LocalConfigurator config;
                string strCxIP = GetCxIpAddress();
				strErrorMsg = "Source disk layout file download failed on Master Target " + m_strMtHostName;
				strErrorCode = "EA0506";
				m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				m_PlaceHolder.Properties[CS_HOST_NAME] = strCxIP;

				bRet = false;
				break;
			}
			DLM_ERROR_CODE dlmErr = ReadDiskInfoMapFromFile(mbrFilePathLocal.c_str(),srcMapOfDiskNamesToDiskInfo);
			if(DLM_ERR_SUCCESS != dlmErr)
			{
				DebugPrintf(SV_LOG_ERROR,"Could not read Disk Info from blob file. File : %s\n",mbrFilePathLocal.c_str());
				strErrorMsg = "Source disk layout file validation failed on Master Target " + m_strMtHostName;
				strErrorCode = "EA0507";
				m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				m_PlaceHolder.Properties[SOURCE_HOSTNAME] = hostName;

				bRet = false;
				break;
			}
			if(!m_bPhySnapShot && !DownloadPartitionFile(mbrFileUrl,mbrFilePathLocal,srcMapOfDiskNamesToDiskInfo))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to download the partition files for the host %s\n",iterVM->first.c_str());
				LocalConfigurator config;
				string strCxIP = GetCxIpAddress();
				strErrorMsg = "Source disk layout file download failed on Master Target " + m_strMtHostName;
				strErrorCode = "EA0506";
				m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				m_PlaceHolder.Properties[CS_HOST_NAME] = strCxIP;

				bRet = false;
				break;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Disk Info blob file path is missing for the host : %s\n",iterVM->first.c_str());
			strErrorMsg = "Source disk layout file path is missing for source server " + hostName + " on Master Target " + m_strMtHostName;
			strErrorCode = "EA0505";
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[SOURCE_HOSTNAME] = hostName;

			bRet = false;
			break;
		}
		
		m_listHost.push_back(iterVM->first);
		m_mapHostIdToHostName[iterVM->first] = hostName;
		m_mapOfHostToDiskInfo[iterVM->first] = srcMapOfDiskNamesToDiskInfo;
		
		iterVM++;
	}

	if(m_listHost.empty())
		bRet = false;
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
// If all required disks are discovered, return 0
// If all disks are not found, then return 1 ( retry can be done here )
int MasterTarget::VerifyAllRequiredDisks()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int iRet = 0;

    do
    {
        // Get vector of all target Lun ids expected to be present on MT
        vector<string> vecTgtLunsReqd;
		map<string,SrcCloudVMDisksInfo>::const_iterator iterVM = m_prepTgtVmsInfo.m_VmsInfo.begin();
		while(iterVM != m_prepTgtVmsInfo.m_VmsInfo.end())
		{
			map<string,string>::const_iterator iterVMLun = iterVM->second.m_vmSrcTgtDiskLunMap.begin();
			while(iterVMLun != iterVM->second.m_vmSrcTgtDiskLunMap.end())
			{
				vecTgtLunsReqd.push_back(iterVMLun->second);
				iterVMLun++;
			}
			iterVM++;
		}

        // Check whether all vecTgtLunsReqd are present in m_mapOfLunIdAndDiskName
        vector<string>::iterator iterReqd = vecTgtLunsReqd.begin();                
        for(; iterReqd != vecTgtLunsReqd.end(); iterReqd++)
        {            
			DebugPrintf(SV_LOG_DEBUG,"Disk's Lun ID to be checked - %s\n",iterReqd->c_str());
            map<string,string>::iterator iterFound = m_mapOfLunIdAndDiskName.find(*iterReqd);
            if(iterFound == m_mapOfLunIdAndDiskName.end())
            {
                iRet = 1;
				DebugPrintf(SV_LOG_ERROR,"Disk with Lun id %s is NOT present.\n", iterReqd->c_str());
                break;
            }
        }        
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return iRet;
}

bool MasterTarget::CreateSrcTgtDiskMapForVMs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	map<string,SrcCloudVMDisksInfo>::const_iterator iterVM = m_prepTgtVmsInfo.m_VmsInfo.begin();
	while(iterVM != m_prepTgtVmsInfo.m_VmsInfo.end())
	{
		map<int,int> mapSrcTgtDiskNum;
		map<string,string>::const_iterator iterVMLun = iterVM->second.m_vmSrcTgtDiskLunMap.begin();
		while(iterVMLun != iterVM->second.m_vmSrcTgtDiskLunMap.end())
		{
			int iSrcDisk = GetDiskNumFromName(iterVMLun->first);
			int iTgtDisk = GetDiskNumFromName(m_mapOfLunIdAndDiskName.find(iterVMLun->second)->second);
			if(iSrcDisk < 0 || iTgtDisk < 0)
			{
				DebugPrintf(SV_LOG_ERROR," Host(%s) disk mapping [%s]->[%s] is invalid\n",iterVM->first.c_str(),iterVMLun->first.c_str(),iterVMLun->second.c_str());
				return false;
			}
			mapSrcTgtDiskNum[iSrcDisk] = iTgtDisk;
			iterVMLun++;
		}
		m_mapOfVmToSrcAndTgtDiskNmbrs.insert(std::make_pair(iterVM->first,mapSrcTgtDiskNum));
		iterVM++;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

int MasterTarget::PrepareTargetDisks(const string & strConfFile, const string & strResultFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iResult = 0;
	
	do {
		m_vConVersion = 4.2;
		string strErrorMsg, strErrorCode;
		m_strMtHostID = getInMageHostId();
		m_strMtHostName = stGetHostName();
		if(m_strMtHostID.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Host id of Master target.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0501";
			UpdateErrorStatusStructure("Error while parsing the drscout.conf file from the Master Target " + m_strMtHostName, "EA0501");
			iResult = 1;
			break;
		}
		if(0 != getAgentInstallPath())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find the Agent Install Path.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0502";
			UpdateErrorStatusStructure("Failed to get Mobility service installation path from " + m_strMtHostName, "EA0502");
			iResult = 1;
			break;
		}
		if(!m_bPhySnapShot)
		{
			//disables the automount for newly created volumes.
			if(false == DisableAutoMount())
			{
				DebugPrintf(SV_LOG_ERROR,"Error: Failed to disable the automount for newly created volumes.\n");
				DebugPrintf(SV_LOG_INFO,"Info  :Manually delete the volume names at the Master Target if not required.\n");
			}
		}
		
		//Read configuration information from configfile
		if(strConfFile.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Config file path is missing. Can not prepare target disks for protection.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0503";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0503");
			iResult = 1;
			break;
		}
		if(false == GetVmDisksInfoFromConfigFile(strConfFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the VM Disks information from config file.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0504";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0504");
			iResult = 1;
			break;
		}

		//Cleanup any missing disks if exists.
		CVdsHelper objVds;
		try
		{
			if(objVds.InitilizeVDS())
				objVds.RemoveMissingDisks();  //Removes if any missing disks available in MT.
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to remove the missing disks\n");
		}

		//Removes the recoveryprogress status file which was created in earlier recovery operaions
		RemoveRecoveryStatusFile();

		//Get the list of VMs/Machines invoved in this protection.
		if (false == DowloadAndReadDiskInfoFromBlobFiles(strErrorMsg, strErrorCode))
		{
			DebugPrintf(SV_LOG_ERROR,"No machine informaton is available in config file.\n");
			m_PlaceHolder.Properties[ERROR_CODE] = strErrorCode;
			UpdateErrorStatusStructure(strErrorMsg, strErrorCode);
			iResult = 1;
			break;
		}
		
		//Stop this process to avoid the format popups.
        if(false == StopService("ShellHWDetection"))
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to stop the service - ShellHWDetection.\n");
            DebugPrintf(SV_LOG_WARNING,"Please cancel all the popups of format volumes if observed.\n");
        }

		//
		if(false == DiscoverAndCollectAllDisks())
		{
			DebugPrintf(SV_LOG_ERROR,"Some of the disks are missing on MT.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0508";
			UpdateErrorStatusStructure("Master target server " + m_strMtHostName + " couldn't detect newly attached disks.", "EA0508");
			iResult = 1;
			break;
		}
		//
		if(false == CreateSrcTgtDiskMapForVMs())
		{
			DebugPrintf(SV_LOG_ERROR,"Error in creating source to target disk mapping.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0509";
			UpdateErrorStatusStructure("Internal error occurred on " + m_strMtHostName, "EA0509");
			iResult = 1;
			break;
		}
		ClearReadOnlyAttributeOfDisk();
		if(!m_bArrBasedSnapshot)
		{
			if(false == BringAllDisksOnline()) //First time(This is done bcz, if offline disks are there then we are failing to clean the disks).
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");
				iResult = 1;
				break;
			}

			CleanAttachedSrcDisks();	//This will uninitialize and offline the attached disk of MT(source disks)
		}
		else
		{
			FillZerosInAttachedSrcDisks();  //This will fill 2KB of zero data on the source respective attached target disks, which will uninitialise the disks.
			rescanIoBuses();
			Sleep(5000); //Sleeping for 5 secs.
		}

		if(false == BringAllDisksOnline()) //Second time(Prepare disks to dump MBRs).
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");
			iResult = 1;
			break;
		}

		if(!m_mapOfHostToDiskInfo.empty())
		{
			strErrorMsg.clear();
			if (false == InitialiseDisks(strErrorMsg))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Initialise the required disks\n");
				m_PlaceHolder.Properties[ERROR_CODE] = "EA0510";
				UpdateErrorStatusStructure(strErrorMsg, "EA0510");
				iResult = 1;
				break;
			}

			rescanIoBuses(); 

			try
			{	
				if(objVds.InitilizeVDS())
					objVds.GetDisksSignature( m_diskNameToSignature );  //After we online disk we are getting the disk Signature values which will be updated to each source Disk.
				objVds.unInitialize();
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed while trying to get disk signatures.\n\n");
			}

			CleanSrcGptDynDisks();	//This will uninitialize the attached dynamic Gpt disks of MT(source disks)
			rescanIoBuses(); 
		}
		//First Of all find all the present Volumes(master target) in the Master Target
		DebugPrintf(SV_LOG_DEBUG,"Going to Find the Master target's private volumes.\n");
		if(-1 == startTargetVolumeEnumeration(false,true))
		{			
			DebugPrintf(SV_LOG_ERROR,"Failed to enumerate the volumes on MT.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0511";
			UpdateErrorStatusStructure("Volume discovery failed on Master Target server " + m_strMtHostName, "EA0511");
			iResult = 1;
			break;
		}

		if(S_FALSE == ConfigureTgtDisksSameAsSrcDisk())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create the disk layouts on MT.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0512";
			UpdateErrorStatusStructure("Internal error occurred on Master Target server " + m_strMtHostName, "EA0512");
			iResult = 1;
			break;
		}
		rescanIoBuses();  // once dumping the MBR\GPT rescanning the disks.
		Sleep(10000);

		if(false == CleanESXFolder())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to cleanup stale entries in mount points folder.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0522";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0522");
            iResult = 1;
            break;
		}

		if(false == BringAllDisksOnline()) //third Time(Post Dumping MBRs)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");                
			iResult = 1;
			break;
		}

		//This is for updating the src disk number to target disk signature mapping, required for rollback
		if(!objVds.InitilizeVDS())
		{
			DebugPrintf(SV_LOG_ERROR,"COM Initialisation failed while trying to use VDS Interface\n");
			objVds.unInitialize();
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0513";
			UpdateErrorStatusStructure("Internal error occurred on " + m_strMtHostName, "EA0513");
			iResult = 1;
			break;
		}
		else
		{
			map<string, string> mapOfTgtDiskNameToTgtDiskSig;
			objVds.GetDisksSignature(mapOfTgtDiskNameToTgtDiskSig);
			if(mapOfTgtDiskNameToTgtDiskSig.empty())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the Map of Target Disk Number to Target Disk signature using VDS interface\n");
				objVds.unInitialize();
				m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				m_PlaceHolder.Properties[ERROR_CODE] = "EA0514";
				UpdateErrorStatusStructure("Internal error occurred on " + m_strMtHostName, "EA0514");
				iResult = 1;
				break;
			}
			map<string,map<int,int>>::iterator IterVmDisk = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
			for(; IterVmDisk != m_mapOfVmToSrcAndTgtDiskNmbrs.end(); IterVmDisk++)
			{
				map<int, string> mapSrcDiskToTgtScsiId;
				map<string, string>::iterator iterTgt;
				map<int, int>::iterator iterSrc = IterVmDisk->second.begin();
				for(; iterSrc != IterVmDisk->second.end(); iterSrc++)
				{
					string srcDisk, tgtDisk;
					stringstream src, tgt;
					//src << iterSrc->first;
					tgt << iterSrc->second;
					//src >> srcDisk;
					tgt >> tgtDisk;
					string strTgtDisk = string("\\\\?\\PhysicalDrive") + tgtDisk;
					iterTgt = mapOfTgtDiskNameToTgtDiskSig.find(strTgtDisk);
					if(iterTgt != mapOfTgtDiskNameToTgtDiskSig.end())
					{
						DebugPrintf(SV_LOG_ERROR,"Got the Target Disk : %s : having Signature: %s\n",strTgtDisk.c_str(), iterTgt->second.c_str());
						mapSrcDiskToTgtScsiId.insert(make_pair(iterSrc->first, iterTgt->second));
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the Map of Source Disk Number to Target Disk Scsi ID\n");
						objVds.unInitialize();
						m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
						m_PlaceHolder.Properties[ERROR_CODE] = "EA0514";
						UpdateErrorStatusStructure("Internal error occurred on " + m_strMtHostName, "EA0514");
						iResult = 1;
						break;
					}
				}
				m_prepTgtVMsUpdate.m_diskSignatureMapping[IterVmDisk->first] = mapSrcDiskToTgtScsiId;
			}
			if(1 == iResult)
				break;

			DebugPrintf(SV_LOG_DEBUG, "Searching for any hidden and Read-only and shadow copy flag enabled volumes, if exists then clear the hidden/read-only/shadow-copy flag for the volume\n");
			objVds.ClearFlagsOfVolume();

			objVds.unInitialize();
		}

		//Create the source and target disk/volume mapping
		if (false == CreateSrcTgtVolumeMapForVMs(strErrorMsg))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create the source & target volume mappings for the VMs.\n");
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0515";
			UpdateErrorStatusStructure(strErrorMsg, "EA0515");
			iResult = 1;
			break;
		}
		//Persis the result to the result file.
		if(false == PersistPrepareTargetVMDisksResult(strResultFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to persist prepare target result to the file %s.\n",strResultFile.c_str());
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0516";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0516");
			iResult = 1;
			break;
		}
		//Make a Register-Host to update the newly created volumes to CX.
		if(!m_bPhySnapShot)
		{
			DebugPrintf(SV_LOG_DEBUG,"Registering the the host with cx to update the newly created volumes\n");
			RegisterHostUsingCdpCli();
		}
	}while(false);

	if (1 == iResult)
	{
		if (!UpdatePrepareTargetStatusInFile(strResultFile, m_statInfo))
			DebugPrintf(SV_LOG_ERROR, "Serialization of Protected vms informaton failed.\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iResult;
}


int MasterTarget::initTarget(void)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iResult = 0;

	taskInfo_t currentTask, nextTask;
	m_bUpdateStep = false;
	m_bVolPairPause = false;
	bool bUpdateToCXbyMTid = false;

	//Initially Filling the CX update structure and updating the status as queued to CX.
	DebugPrintf(SV_LOG_INFO,"Updating all the tasks\n");


	do
	{
		if (m_bPhySnapShot)
		{
			string strRecoveredVmFileName;
			if (m_strDirPath.empty())
				strRecoveredVmFileName = m_strDataFolderPath + string("InMage_Recovered_Vms.snapshot");
			else
				strRecoveredVmFileName = m_strDirPath + string("\\") + string("InMage_Recovered_Vms.snapshot");

			bool bRetuenValue = checkIfFileExists(strRecoveredVmFileName);
			if (bRetuenValue)
			{
				DebugPrintf(SV_LOG_INFO, "File(%s) already exist. Considering Dr-Drill for this completed earlier.\n\n", strRecoveredVmFileName.c_str());
				break;
			}
		}

		if (m_strOperation.compare("volumeresize") == 0)
		{
			m_bVolResize = true;
		}

		if (m_bPhySnapShot)
			InitSnapshotTaskUpdate();
		else if (m_bVolResize)
			InitVolResizeTaskUpdate();
		else
			InitProtectionTaskUpdate();
		UpdateTaskInfoToCX();

		if(!m_bPhySnapShot)
		{
			//disables the automount for newly created volumes.
			if(false == DisableAutoMount())
			{
				DebugPrintf(SV_LOG_WARNING,"Failed to disable the automount for newly created volumes.\n");
				DebugPrintf(SV_LOG_WARNING,"Manually delete the volume names at the Master Target if not required.\n");
			}
		}
		if((!m_bPhySnapShot) && (!m_bVolResize))
		{
			string strProtectionFile;
			if(m_strDirPath.empty())
				strProtectionFile = m_strDataFolderPath + string("ProtectionStatusFile");
			else
				strProtectionFile = m_strDirPath + string("\\") + string("ProtectionStatusFile");

			if(checkIfFileExists(strProtectionFile))
			{
				DebugPrintf(SV_LOG_ERROR,"Protection is already ran for this Plan..Check the FX logs for more information...");
				currentTask.TaskNum = INM_VCON_TASK_1;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Protection is already ran for this Plan. If protection plan required to rerun then delete file ProtectionStatusFile under protection plan name directory of MT and rerun the protection FX job.";
				currentTask.TaskFixSteps = "";
				nextTask.TaskNum = INM_VCON_TASK_2;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				bUpdateToCXbyMTid = true;

				iResult = 1;
				break;
			}
		}
		//Find the Master target inmage host id
		m_strMtHostID = FetchInMageHostId();
		if(m_strMtHostID.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Host id of Master target.\n");
			currentTask.TaskNum = INM_VCON_TASK_1;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to fetch InMage Host id of Master target.";
			currentTask.TaskFixSteps = "";
			nextTask.TaskNum = INM_VCON_TASK_2;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			bUpdateToCXbyMTid = true;

			iResult = 1;
			break;
		}
		if(0 != getAgentInstallPath())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find the Agent Install Path.\n");
			currentTask.TaskNum = INM_VCON_TASK_1;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to find the Agent Install Path.";
			currentTask.TaskFixSteps = "Restart the VxAgent service and rerun the job again";
			nextTask.TaskNum = INM_VCON_TASK_2;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			bUpdateToCXbyMTid = true;

			iResult = 1;
			break;
		}

		if(FALSE == CreateDirectory(m_strDirPath.c_str(),NULL))
		{
			if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
				DebugPrintf(SV_LOG_INFO,"Directory already exists. Considering it not as an Error.\n");            				
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu]. proceeding with %s\n",m_strDirPath.c_str(),GetLastError(), m_strDataFolderPath.c_str());
				m_strDirPath = m_strDataFolderPath;
			}
		}
		//Assign Secure permission to the directory
		AssignSecureFilePermission(m_strDirPath, true);

		if(m_bPhySnapShot)
		{
			if(m_strDirPath.empty())
				m_strXmlFile = m_strDataFolderPath + string(SNAPSHOT_FILE_NAME);
			else
				m_strXmlFile = m_strDirPath + string("\\") + string(SNAPSHOT_FILE_NAME);
		}
		else if(m_bVolResize)
		{
			if(m_strDirPath.empty())
				m_strXmlFile = m_strDataFolderPath + string(VOL_RESIZE_FILE_NAME);
			else
				m_strXmlFile = m_strDirPath + string("\\") + string(VOL_RESIZE_FILE_NAME);
		}
		else
		{
			if(m_strDirPath.empty())
				m_strXmlFile = m_strDataFolderPath + string(VX_JOB_FILE_NAME);
			else
				m_strXmlFile = m_strDirPath + string("\\") + string(VX_JOB_FILE_NAME);
		}

		//Updating Task1 as completed and Task2 as inprogress
		DebugPrintf(SV_LOG_INFO,"Updating Task1 as Completed\n");
		currentTask.TaskNum = INM_VCON_TASK_1;
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskNum = INM_VCON_TASK_2;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		DebugPrintf(SV_LOG_DEBUG,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
		ModifyTaskInfoForCxApi(currentTask, nextTask);
		UpdateTaskInfoToCX();

		if(m_strCxPath.empty())
		{
			DebugPrintf(SV_LOG_INFO,"[WARNING]CX Path is not provided to download the protection related files.\n continue with normal way\n");
		}
		else
		{
			if(false == ProcessCxPath()) //This will download all the required files from the given CX Path.
			{
				currentTask.TaskNum = INM_VCON_TASK_2;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to download the required files from the given CX path.";
				currentTask.TaskFixSteps = "Check the existance CX path and the required files inside that one. Rerun the job again after specifying the correct details.";
				nextTask.TaskNum = INM_VCON_TASK_3;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				bUpdateToCXbyMTid = true;

				DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed to download the required files from the given CX path %s\n", m_strCxPath.c_str());
				DebugPrintf(SV_LOG_ERROR,"Can't Proceed further, Stopping the Protection/Snapshot operation.\n");
				iResult = 1;
			    break;
			}
		}

		CVdsHelper objVds;
		try
		{
			if(objVds.InitilizeVDS())
				objVds.RemoveMissingDisks();  //Removes if any missing disks available in MT.
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to remove the missing disks\n\n");
		}

		m_mapHostIdToHostName.clear();
		m_listHost.clear();

		if (false == ListHostForProtection())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to List out all the Hosts info\n");
			currentTask.TaskNum = INM_VCON_TASK_2;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to List out all the Hosts info from inmage_scsi_unit.txt file.";
			currentTask.TaskFixSteps = "Check the existance of the file in the MT. If not rerun the job.";
			nextTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			bUpdateToCXbyMTid = true;

			iResult = 1;
			break;
		}

		UpdateSrcHostStaus();

		if(false == xCheckXmlFileExists())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find the XML document with all the Virtual Machine info.\n");
			currentTask.TaskNum = INM_VCON_TASK_2;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to find the XML document with all the Virtual Machine info.";
			currentTask.TaskFixSteps = "Check the Xml file existance in the MT. If not rerun the job.";
			nextTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

			iResult = 1;
			break;
		}

		if(false == xGetvConVersion(m_strXmlFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the vCon version from Esx.xml file.\n");
			m_vConVersion = 1.2;
		}
        
		if(!m_bVolResize)
		{
			if(!m_bPhySnapShot)
			{
				//Update the flag for Addition of disks. Used during UpdatePinfoFile function.
				UpdateAoDFlag();
			}
			else
				bIsAdditionOfDisk = false;

			if(!m_bPhySnapShot)
			{
				//Update the flag for V2P.
				UpdateV2PFlag();
			}
			else
				m_bV2P = false;

			if(m_bPhySnapShot)
			{
				//Update the flag for Arraybasedsnapshot.
				UpdateArrayBasedSnapshotFlag();
			}
			else
				m_bArrBasedSnapshot = false;
		}
		else
		{
			bIsAdditionOfDisk = false;
			m_bV2P = false;
			m_bArrBasedSnapshot = false;
		}

		if(false == xGetClusNodesAndClusDisksInfo())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the cluster information from XML files.\n");
			currentTask.TaskNum = INM_VCON_TASK_2;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to get the cluster information from XML files, For extended Error information download EsxUtilWin.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check the existance of the file in the MT. If not rerun the job.";
			nextTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

			iResult = 1;
			break;
		}

		if(m_vConVersion >= 4.0)
		{
			m_mapOfHostToDiskInfo.clear();
			list<string>::iterator iter = m_listHost.begin();
			for(; iter != m_listHost.end(); iter++)
			{
				if(m_mapHostToClusterInfo.find(*iter) != m_mapHostToClusterInfo.end())
				{
					map<string, clusterMbrInfo_t>::iterator iterClusInfo = m_mapHostToClusterInfo.begin();
					for(; iterClusInfo != m_mapHostToClusterInfo.end(); iterClusInfo++)
					{
						if(false == iterClusInfo->second.bDownloaded)
						{
							DebugPrintf(SV_LOG_INFO, "Downloading DLM MBR File for cluster node : %s\n", iterClusInfo->first.c_str());
							if(false == DownloadDlmMbrFileForSrcHost(iterClusInfo->first, iterClusInfo->second.MbrFile))
							{
								DebugPrintf(SV_LOG_ERROR, "DLM MBR File download failed for cluster node : %s\n", iterClusInfo->first.c_str());
								currentTask.TaskNum = INM_VCON_TASK_2;
								currentTask.TaskStatus = INM_VCON_TASK_FAILED;
								currentTask.TaskErrMsg = "DLM MBR File download failed for cluster node : " + iterClusInfo->first + " For extended Error information download EsxUtilWin.log in vCon Wiz";
								currentTask.TaskFixSteps = "Check the Source MBR path is proper or not. corret it in xml file and Rerun the job.";
								nextTask.TaskNum = INM_VCON_TASK_3;
								nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
								iResult = 1;
								break;
							}
							iterClusInfo->second.bDownloaded = true;
						}
						else
						{
							DebugPrintf(SV_LOG_INFO, "DLM MBR File already downloaded for cluster node : %s\n", iterClusInfo->first.c_str());
						}
					}
					if(1 == iResult)
						break;
				}
				else
				{
					string strMbrUrl;
					if(false == xGetMbrPathFromXmlFile(*iter, strMbrUrl))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to read the file %s for host %s\n", m_strXmlFile.c_str(), iter->c_str());
						currentTask.TaskNum = INM_VCON_TASK_2;
						currentTask.TaskStatus = INM_VCON_TASK_FAILED;
						currentTask.TaskErrMsg = "Failed to Read the Xml file. For extended Error information download EsxUtilWin.log in vCon Wiz";
						currentTask.TaskFixSteps = "Check the existance of the file in the MT. Rerun the job.";
						nextTask.TaskNum = INM_VCON_TASK_3;
						nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
						iResult = 1;
						break;
					}
					
					if(false == DownloadDlmMbrFileForSrcHost(*iter, strMbrUrl))
					{
						DebugPrintf(SV_LOG_ERROR, "DLM MBR File download failed for host %s\n", iter->c_str());
						currentTask.TaskNum = INM_VCON_TASK_2;
						currentTask.TaskStatus = INM_VCON_TASK_FAILED;
						currentTask.TaskErrMsg = "DLM MBR File download failed for host: " + (*iter) + " For extended Error information download EsxUtilWin.log in vCon Wiz";
						currentTask.TaskFixSteps = "Check the Source MBR path is proper or not. corret it in xml file and Rerun the job.";
						nextTask.TaskNum = INM_VCON_TASK_3;
						nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
						iResult = 1;
						break;
					}
				}
			}

			if(1 == iResult)
				break;

			if(m_strCxPath.empty())
			{
				map<string,bool>::iterator p2vIter= m_MapHostIdMachineType.begin();
				for(;p2vIter != m_MapHostIdMachineType.end(); p2vIter++)
				{
					if(p2vIter->second && !bIsAdditionOfDisk && !m_bPhySnapShot && !m_bVolResize)
					{
						map<string, string>::iterator iterOs = m_MapHostIdOsType.find(p2vIter->first);  //we will get it everytime

						string strSrcPathFile, strDestPathFile ;
						string strSrcPath, strDestPath ;
						
						if(iterOs->second.compare("Windows_XP_32") == 0)
						{
							strSrcPathFile = m_strDirPath + string("\\Windows_XP_32\\symmpi.sys");
							strDestPathFile = m_strDataFolderPath + string("Windows_XP_32\\symmpi.sys");

							strSrcPath = m_strDirPath + string("\\Windows_XP_32");
							strDestPath = m_strDataFolderPath + string("Windows_XP_32");
						}
						else
						{
							continue;
						}
						bool bDestFile = checkIfFileExists(strDestPathFile);
						bool bSrcFile = checkIfFileExists(strSrcPathFile);
						
						if(bDestFile)
						{
							DebugPrintf(SV_LOG_ERROR,"W2K3/WindowsXP P2V, File %s already exist",strDestPathFile.c_str());
							continue;
						}

						if(!bSrcFile)
						{
							DebugPrintf(SV_LOG_ERROR,"W2K3/WindowsXP P2V, File %s does not exist",strSrcPathFile.c_str());
							DebugPrintf(SV_LOG_ERROR,"Recovery will fail if we proceed with protection, So exiting over here");
							currentTask.TaskNum = INM_VCON_TASK_2;
							currentTask.TaskStatus = INM_VCON_TASK_FAILED;
							currentTask.TaskErrMsg = "W2K3/WindowsXP P2V, File " + strSrcPathFile + " does not exist in MT, Recovery will fail if we proceed with protection." + 
														" For extended Error information download EsxUtilWin.log in vCon Wiz.";
							currentTask.TaskFixSteps = "Check for the P2V related files existance and rerun the job";
							nextTask.TaskNum = INM_VCON_TASK_3;
							nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

							iResult = 1;
							break;
						}
						if(!bDestFile)
						{
							if(false == CopyDirectory(strSrcPath, strDestPath, true))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to copy the directory %s to %s",strSrcPath.c_str(), strDestPath.c_str());
								DebugPrintf(SV_LOG_ERROR,"Recovery will fail if we proceed with protection, So exiting over here");
								//Updating Task-2 as failed
								currentTask.TaskNum = INM_VCON_TASK_2;
								currentTask.TaskStatus = INM_VCON_TASK_FAILED;
								currentTask.TaskErrMsg = "Failed to copy the directory " + strSrcPath + " to " + strDestPath + ". Recovery will fail if we proceed with protection." + 
															" For extended Error information download EsxUtilWin.log in vCon Wiz.";
								currentTask.TaskFixSteps = "Check for the Paths are in accessible state in MT, if yes then rerun the job. If not then give access and rerun again.";
								nextTask.TaskNum = INM_VCON_TASK_3;
								nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

								iResult = 1;
								break;
							}
						}
						break;
					}
				}
				if(1 == iResult)
					break;
			}
		}

		//Updating Task-2 as completed and Task-3 as inprogress
		currentTask.TaskNum = INM_VCON_TASK_2;
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskNum = INM_VCON_TASK_3;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		DebugPrintf(SV_LOG_DEBUG,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
		ModifyTaskInfoForCxApi(currentTask, nextTask);
		UpdateTaskInfoToCX();

        //Stop this process to avoid the format popups.
        if(false == StopService("ShellHWDetection"))
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to stop the service - ShellHWDetection.\n");
            DebugPrintf(SV_LOG_WARNING,"Please cancel all the popups of format volumes if observed.\n");
        }

		if(!m_bVolResize)
		{
			if(false == DiscoverAndCollectAllDisks())
			{
				DebugPrintf(SV_LOG_ERROR,"DiscoverAndCollectAllDisks() failed.\n");
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Discovery of all disks Failed in MT. For extended Error information download EsxUtilWin.log in vCon Wiz.";
				currentTask.TaskFixSteps = "Refresh the disk mamangement and check all the source disks are available or not. Rerun the job";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				break;
			}

			if(false == createMapOfVmToSrcAndTgtDiskNmbrs())
			{
				DebugPrintf(SV_LOG_ERROR,"createMapOfVmToSrcAndTgtDiskNmbrs() failed.\n");
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Creation of mapping between source and Target Disk failed. For extended Error information download EsxUtilWin.log in vCon Wiz.";
				currentTask.TaskFixSteps = string("Check the inmage_scsi_unit_disks.txt file having proper entries or not. Modify it with proepr values. ") +
											string("Check source respective target disk existance in MT. If everything is clear then rerun the job else contanct InMage customer support");
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				break;
			}
		
			ClearReadOnlyAttributeOfDisk();

			if(!m_bArrBasedSnapshot)
			{
				if(false == BringAllDisksOnline()) //First time(This is done bcz, if offline disks are there then we are failing to clean the disks).
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");                
					DebugPrintf(SV_LOG_ERROR,"Manually bring all the disks to Online Mode, clear the Read Only attributes, and rerun the job.\n");
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to bring the Offline Disks to Online Mode in all 3 attempts. For extended Error information download EsxUtilWin.log in vCon Wiz";
					currentTask.TaskFixSteps = "Manually bring all the disks to Online Mode, clear the Read Only attributes, and rerun the job.";
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					iResult = 1;
					break;
				}

				CleanAttachedSrcDisks();	//This will uninitialize and offline the attached disk of MT(source disks)
			}
			else
			{
				FillZerosInAttachedSrcDisks();  //This will fill 2KB of zero data on the source respective attached target disks, which will uninitialise the disks.
				rescanIoBuses();
				Sleep(5000); //Sleeping for 5 secs.
			}
		}
		else
		{
			if(false == PauseAllReplicationPairs())
			{
				DebugPrintf(SV_LOG_ERROR,"PauseAllReplicationPairs() failed.\n");
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Pausing the replication pairs for soem of the VM's Failed";
				currentTask.TaskFixSteps = "";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				
				iResult = 1;
				break;
			}

			currentTask.TaskNum = INM_VCON_TASK_3;
			currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			nextTask.TaskNum = INM_VCON_TASK_4;
			nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
			DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
			ModifyTaskInfoForCxApi(currentTask, nextTask);
			UpdateTaskInfoToCX();
			m_bVolPairPause = true;

			if (m_bVolResize && !UnmountProtectedVolumes())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to unmount the earlier protected volumes during volume resize.\n");
			}

			try
			{	
				if(objVds.InitilizeVDS())
					objVds.GetDisksSignature( m_diskNameToSignature );  //After we online disk we are getting the disk Signature values which will be updated to each source Disk.
				objVds.unInitialize();
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed while trying to get disk signatures.\n\n");
			}
			
			map<string, string>::iterator iterClusHost = m_mapHostIdToHostName.begin();
			set<string> setClusNodes;
			for(; iterClusHost != m_mapHostIdToHostName.end(); iterClusHost++)
			{
				if(m_mapHostToClusterInfo.find(iterClusHost->first) != m_mapHostToClusterInfo.end())
				{
					map<string, clusterMbrInfo_t>::iterator iterClusInfo = m_mapHostToClusterInfo.begin();
					for(; iterClusInfo != m_mapHostToClusterInfo.end(); iterClusInfo++)
					{
						if(m_mapHostIdToHostName.find(iterClusInfo->first) == m_mapHostIdToHostName.end())
						{
							setClusNodes.insert(iterClusInfo->first);
						}
					}
				}
			}
			for(set<string>::iterator iterClusSet = setClusNodes.begin(); iterClusSet != setClusNodes.end(); iterClusSet++)
			{
				DebugPrintf(SV_LOG_INFO,"Inserting Cluster Node [%s] inside m_mapHostIdToHostName.\n\n", iterClusSet->c_str());
				m_mapHostIdToHostName.insert(make_pair(*iterClusSet, ""));
			}
			
			if(false == createMapOfVmToSrcAndTgtDiskNmbrs(true))  //calling incase of Volume reise only
			{
				DebugPrintf(SV_LOG_ERROR,"createMapOfVmToSrcAndTgtDiskNmbrs() failed.\n");
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Creation of mapping between source and Target Disk failed. For extended Error information download EsxUtilWin.log in vCon Wiz.";
				currentTask.TaskFixSteps = string("");
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				break;
			}
		}

		if(!m_bVolResize)
		{
			if(false == BringAllDisksOnline()) //Second time(Prepare disks to dump MBRs).
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");                
				DebugPrintf(SV_LOG_ERROR,"Manually bring all the disks to Online Mode, clear the Read Only attributes, and rerun the job.\n");
				DebugPrintf(SV_LOG_INFO,"Updating Task3 as Failed\n");
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to bring the Offline Disks to Online Mode in all 3 attempts. For extended Error information download EsxUtilWin.log in vCon Wiz";
				currentTask.TaskFixSteps = "Manually bring all the disks to Online Mode, clear the Read Only attributes, and rerun the job.";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				break;
			}

			if(!m_mapOfHostToDiskInfo.empty())
			{
				string strErrorMsg;
				if (false == InitialiseDisks(strErrorMsg))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to Initialise the required disks\n");
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to Initialise the required disks. For extended Error information download EsxUtilWin.log in vCon Wiz";
					currentTask.TaskFixSteps = "Manually initialise all the source respective target disks and rerun the job.";
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					iResult = 1;
					break;
				}

				rescanIoBuses(); 

				try
				{	
					if(objVds.InitilizeVDS())
						objVds.GetDisksSignature( m_diskNameToSignature );  //After we online disk we are getting the disk Signature values which will be updated to each source Disk.
					objVds.unInitialize();
				}
				catch(...)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed while trying to get disk signatures.\n\n");
				}

				CleanSrcGptDynDisks();	//This will uninitialize the attached dynamic Gpt disks of MT(source disks)
				rescanIoBuses(); 
			}
			if(bIsAdditionOfDisk)
			{
				if(false == CreateHostToDynDiskAddOption())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to create the map between host to dynamic disk.\n");
					DebugPrintf(SV_LOG_INFO,"Updating Task3 as Failed\n");
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to create the map between host to dynamic disk. For extended Error information download EsxUtilWin.log in vCon Wiz";
					currentTask.TaskFixSteps = "";
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					iResult = 1;
					break;
				}

				map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.begin();
				for(; iterDyn != m_mapHostIdDynAddDisk.end(); iterDyn++)
				{
					if(true == iterDyn->second)
					{
						if(false == PauseReplicationPairs(iterDyn->first, m_strMtHostID))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to Pause some replication pairs.\n");
							DebugPrintf(SV_LOG_INFO,"Updating Task3 as Failed\n");
							currentTask.TaskNum = INM_VCON_TASK_3;
							currentTask.TaskStatus = INM_VCON_TASK_FAILED;
							currentTask.TaskErrMsg = "Failed to Pause some replication pairs. For extended Error information download EsxUtilWin.log in vCon Wiz";
							currentTask.TaskFixSteps = "Manually resume the pairs if any pairs are paused.";
							nextTask.TaskNum = INM_VCON_TASK_4;
							nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

							iResult = 1;
							break;
						}
						
						m_bVolPairPause = true;
					}
				}

				if(1 == iResult)
				{
					//Need to write the failure case
					break;
				}

				if(false == createMapOfVmToSrcAndTgtDiskNmbrs(true))  //Information is required during dynamic disk addition
				{
					DebugPrintf(SV_LOG_ERROR,"createMapOfVmToSrcAndTgtDiskNmbrs() failed.\n");
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Creation of mapping between source and Target Disk failed. For extended Error information download EsxUtilWin.log in vCon Wiz.";
					currentTask.TaskFixSteps = string("");
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					iResult = 1;
					break;
				}
			}
		}
		if(m_bVolResize || bIsAdditionOfDisk)
		{
			rescanIoBuses();
			if(false == OfflineDiskAndCollectGptDynamicInfo())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to bring the disks Offline (or) failed to collect the 2KB GPT information for dynamic disks.\n");
				DebugPrintf(SV_LOG_ERROR,"Failed to Initialise the required disks\n");
				currentTask.TaskNum = INM_VCON_TASK_4;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to bring the disks Offline (or) failed to collect the 2KB GPT information for dynamic disks.";
				currentTask.TaskFixSteps = "";
				nextTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				break;
			}
		}
	    
		//First Of all find all the present Volumes(master target) in the Master Target
		DebugPrintf(SV_LOG_DEBUG,"Going to Find the Master target's private volumes.\n");
		if(-1 == startTargetVolumeEnumeration(false,true))
		{			
			DebugPrintf(SV_LOG_ERROR,"startTargetVolumeEnumeration() failed.\n");
			if(!m_bVolResize)
			{
				currentTask.TaskNum = INM_VCON_TASK_3;
				nextTask.TaskNum = INM_VCON_TASK_4;
			}
			else
			{
				currentTask.TaskNum = INM_VCON_TASK_4;				
				nextTask.TaskNum = INM_VCON_TASK_5;				
			}
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed while enumerating the MT volumes. For extended Error information download EsxUtilWin.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check the MT volumes are in proper state or not and rerun the job.";
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

			iResult = 1;
			break;
		}

		if(!m_bVolResize)
		{
			currentTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskNum = INM_VCON_TASK_4;
		}
		else
		{
			currentTask.TaskNum = INM_VCON_TASK_4;
			nextTask.TaskNum = INM_VCON_TASK_5;
		}
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
		ModifyTaskInfoForCxApi(currentTask, nextTask);
		UpdateTaskInfoToCX();

		if(S_FALSE == ConfigureTgtDisksSameAsSrcDisk())
		{
			DebugPrintf(SV_LOG_ERROR,"ConfigureTgtDisksSameAsSrcDisk() failed.\n");
			if(!m_bVolResize)
			{
				currentTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskNum = INM_VCON_TASK_5;
			}
			else
			{
				currentTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskNum = INM_VCON_TASK_6;
			}
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to deploy the MBR/GPT/LDM on target disks. For extended Error information download EsxUtilWin.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check all the disks avaliable or not and rerun the job. IF still faild then contact inmage customer support";
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

			iResult = 1;
			break;
		}
		rescanIoBuses();  // once dumping the MBR\GPT rescanning the disks.
		Sleep(10000);

        if(false == CleanESXFolder())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to clean stale entries in ESX folder.\n");
			if(!m_bVolResize)
			{
				currentTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskNum = INM_VCON_TASK_5;
			}
			else
			{
				currentTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskNum = INM_VCON_TASK_6;
			}
			if(m_bPhySnapShot)
				currentTask.TaskErrMsg = "Failed to clean stale entries in ESX\\SNAPSHOT folder. For extended Error information download EsxUtilWin.log in vCon Wiz";
			else
				currentTask.TaskErrMsg = "Failed to clean stale entries in ESX folder. For extended Error information download EsxUtilWin.log in vCon Wiz";
			currentTask.TaskFixSteps = "Clean if any unwanted folders present in ESX folder and rerun the job.";
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;		
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

            iResult = 1;
            break;
		}
		
		if(!m_bVolResize)
		{			
			if(false == BringAllDisksOnline()) //third Time(Post Dumping MBRs)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");                
				DebugPrintf(SV_LOG_ERROR,"Manually bring all the disks to Online Mode, clear the Read Only attributes, and rerun the job.\n");

				currentTask.TaskNum = INM_VCON_TASK_4;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to bring the Offline Disks to Online Mode in all 3 attempts. For extended Error information download EsxUtilWin.log in vCon Wiz";
				currentTask.TaskFixSteps = "Manually bring all the disks to Online Mode, clear the Read Only attributes, and rerun the job.";
				nextTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				break;
			}

			//This is for updating the dinfo file. required for rollback
			if(!objVds.InitilizeVDS())
			{
				DebugPrintf(SV_LOG_ERROR,"COM Initialisation failed while trying to use VDS Interface\n");

				currentTask.TaskNum = INM_VCON_TASK_4;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "COM Initialisation failed while trying to use VDS Interface. For extended Error information download EsxUtilWin.log in vCon Wiz";
				currentTask.TaskFixSteps = "Rerun the job. If still fails contact inmage customer support";
				nextTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				iResult = 1;
				objVds.unInitialize();
				break;
			}
			else
			{
				map<string, string> mapOfTgtDiskNameToTgtDiskSig;
				objVds.GetDisksSignature(mapOfTgtDiskNameToTgtDiskSig);
				if(mapOfTgtDiskNameToTgtDiskSig.empty())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the Map of Target Disk Number to Target Disk signature using VDS interface\n");
					objVds.unInitialize();

					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to get the Map of Target Disk Number to Target Disk Signature using VDS interface. For extended Error information download EsxUtilWin.log in vCon Wiz";
					currentTask.TaskFixSteps = "Check all the disks are available in MT, Rerun the job. If still fails contact inmage customer support";
					nextTask.TaskNum = INM_VCON_TASK_5;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					iResult = 1;
					break;
				}
				map<string,map<int,int>>::iterator IterVmDisk = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
				for(; IterVmDisk != m_mapOfVmToSrcAndTgtDiskNmbrs.end(); IterVmDisk++)
				{					
					string srcDisk, tgtDisk, strTgtDisk;
					map<string, string> mapSrcDiskToTgtScsiId;
					map<string, dInfo_t> mapTgtDiskSigToSrcDiskInfo;
					map<string, string>::iterator iterTgt;

					double dlmVersion = 0;
					if(m_mapOfHostIdDlmVersion.find(IterVmDisk->first) != m_mapOfHostIdDlmVersion.end())
						dlmVersion = m_mapOfHostIdDlmVersion[IterVmDisk->first];
					
					bool bSrcDiskInfoFound = true;
					if(m_mapOfHostToDiskInfo.find(IterVmDisk->first) == m_mapOfHostToDiskInfo.end())
					{
						DebugPrintf(SV_LOG_ERROR,"Could not find the disk information for the host %s\n",IterVmDisk->first.c_str());
						bSrcDiskInfoFound = false;
					}

					map<int, int>::iterator iterSrc = IterVmDisk->second.begin();
					for(; iterSrc != IterVmDisk->second.end(); iterSrc++)
					{
						dInfo_t diskInfo;
						stringstream src, tgt;
						src << iterSrc->first;
						tgt << iterSrc->second;
						src >> srcDisk;
						tgt >> tgtDisk;

						if(bSrcDiskInfoFound && dlmVersion >= 1.2)
						{
							string strSrcFullDisk = "\\\\.\\PHYSICALDRIVE" + srcDisk;
							DisksInfoMapIter_t iterDisk = m_mapOfHostToDiskInfo[IterVmDisk->first].find(strSrcFullDisk);
							if(iterDisk != m_mapOfHostToDiskInfo[IterVmDisk->first].end())
							{								
								diskInfo.DiskSignature = iterDisk->second.DiskSignature;
								diskInfo.DiskDeviceId = iterDisk->second.DiskDeviceId;
								diskInfo.DiskNum = srcDisk;
							}
						}

						strTgtDisk = string("\\\\?\\PhysicalDrive") + tgtDisk;
						iterTgt = mapOfTgtDiskNameToTgtDiskSig.find(strTgtDisk);
						if(iterTgt != mapOfTgtDiskNameToTgtDiskSig.end())
						{
							DebugPrintf(SV_LOG_INFO,"Got the Target Disk: %s = Signature: %s\n",strTgtDisk.c_str(), iterTgt->second.c_str());
							if(dlmVersion >= 1.2)
								mapTgtDiskSigToSrcDiskInfo.insert(make_pair(iterTgt->second, diskInfo));
							else
								mapSrcDiskToTgtScsiId.insert(make_pair(srcDisk, iterTgt->second));
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get the Map of Source Disk Number to Target Disk Scsi ID\n");
							DebugPrintf(SV_LOG_ERROR,"Failed to write source and target SCSI IDs info into dinfo file for VM - %s.\n",IterVmDisk->first.c_str());
							objVds.unInitialize();

							currentTask.TaskNum = INM_VCON_TASK_4;
							currentTask.TaskStatus = INM_VCON_TASK_FAILED;
							currentTask.TaskErrMsg = string("Failed to get the Map of Source Disk Number to Target Disk Scsi ID") + 
														string("Failed to write source and target SCSI IDs info into dinfo file for VM - ") + IterVmDisk->first +
														string(". For extended Error information download EsxUtilWin.log in vCon Wiz");
							currentTask.TaskFixSteps = "Check all the disks are available in MT, Rerun the job. If still fails contact inmage customer support";
							nextTask.TaskNum = INM_VCON_TASK_5;
							nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

							iResult = 1;
							break;
						}
					}

					string strDpinfoFile = m_strDataFolderPath + IterVmDisk->first + string(".dpinfo");

					if(bIsAdditionOfDisk && (false == checkIfFileExists(strDpinfoFile)) && (!m_mapOfHostToDiskNumMap.empty()))
					{
						map<string,map<int,int>>::iterator iterAddDiskHost = m_mapOfHostToDiskNumMap.find(IterVmDisk->first);
						if(iterAddDiskHost != m_mapOfHostToDiskNumMap.end())
						{
							for(iterSrc = iterAddDiskHost->second.begin(); iterSrc != iterAddDiskHost->second.end(); iterSrc++)
							{
								dInfo_t diskInfo;
								srcDisk.clear();
								tgtDisk.clear();
								stringstream src, tgt;
								src << iterSrc->first;
								tgt << iterSrc->second;
								src >> srcDisk;
								tgt >> tgtDisk;

								if(bSrcDiskInfoFound && dlmVersion >= 1.2)
								{
									string strSrcFullDisk = "\\\\.\\PHYSICALDRIVE" + srcDisk;
									DisksInfoMapIter_t iterDisk = m_mapOfHostToDiskInfo[IterVmDisk->first].find(strSrcFullDisk);
									if(iterDisk != m_mapOfHostToDiskInfo[IterVmDisk->first].end())
									{								
										diskInfo.DiskSignature = iterDisk->second.DiskSignature;
										diskInfo.DiskDeviceId = iterDisk->second.DiskDeviceId;
										diskInfo.DiskNum = srcDisk;
									}
								}

								strTgtDisk = string("\\\\?\\PhysicalDrive") + tgtDisk;
								iterTgt = mapOfTgtDiskNameToTgtDiskSig.find(strTgtDisk);
								if(iterTgt != mapOfTgtDiskNameToTgtDiskSig.end())
								{
									DebugPrintf(SV_LOG_INFO,"Got the Target Disk: %s = Signature: %s\n",strTgtDisk.c_str(), iterTgt->second.c_str());
									if(dlmVersion >= 1.2)
										mapTgtDiskSigToSrcDiskInfo.insert(make_pair(iterTgt->second, diskInfo));
									else
										mapSrcDiskToTgtScsiId.insert(make_pair(srcDisk, iterTgt->second));
								}
							}
						}
					}

					string strDInfofile;
					if(m_bPhySnapShot)
						strDInfofile = ".dsinfo";
					else
						strDInfofile = ".dpinfo";

					if(dlmVersion >= 1.2)
					{
						if (false == UpdateDiskInfoFile(IterVmDisk->first, mapTgtDiskSigToSrcDiskInfo, strDInfofile))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to write source disk info to target disk signeture into dinfo file for VM - %s.\n",IterVmDisk->first.c_str());
							objVds.unInitialize();

							currentTask.TaskNum = INM_VCON_TASK_4;
							currentTask.TaskStatus = INM_VCON_TASK_FAILED;
							currentTask.TaskErrMsg = "Failed to write source disk info to target disk signeture into dinfo file for VM - " + IterVmDisk->first +
														". For extended Error information download EsxUtilWin.log in vCon Wiz";
							currentTask.TaskFixSteps = "Check all the disks are available in MT, Rerun the job. If still fails contact inmage customer support";
							nextTask.TaskNum = INM_VCON_TASK_5;
							nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

							iResult = 1;
							break;
						}
					}
					else
					{
						if (false == UpdateInfoFile(IterVmDisk->first, mapSrcDiskToTgtScsiId,strDInfofile))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to write source and target SCSI IDs info into dinfo file for VM - %s.\n",IterVmDisk->first.c_str());
							objVds.unInitialize();

							currentTask.TaskNum = INM_VCON_TASK_4;
							currentTask.TaskStatus = INM_VCON_TASK_FAILED;
							currentTask.TaskErrMsg = "Failed to write source and target SCSI IDs info into dinfo file for VM - " + IterVmDisk->first +
														". For extended Error information download EsxUtilWin.log in vCon Wiz";
							currentTask.TaskFixSteps = "Check all the disks are available in MT, Rerun the job. If still fails contact inmage customer support";
							nextTask.TaskNum = INM_VCON_TASK_5;
							nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

							iResult = 1;
							break;
						}
					}
				}
				if(1 == iResult)
					break;

				DebugPrintf(SV_LOG_DEBUG, "Searching for any hidden and Read-only and shadow copy flag enabled volumes, if exists then clear the hidden/read-only/shadow-copy flag for the volume\n");
				objVds.ClearFlagsOfVolume();

				objVds.unInitialize();
			}
		}

		iResult = processTargetSidedisks();
		if (0 != iResult)
		{
			DebugPrintf(SV_LOG_ERROR,"processTargetSidedisks() failed.\n");

			if (!m_bVolResize)
			{
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				if (1 == iResult)
				{
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskErrMsg = string("Failed to assign mount ponits to the newly created Volumes.");
				}
				else if (2 == iResult)
				{
					currentTask.TaskNum = INM_VCON_TASK_5;
					currentTask.TaskErrMsg = string("Failed to create protection pairs. Activating protection plan failed.");
				}
			}
			iResult = 1;
			break;
		}

		if(!m_bPhySnapShot)
		{
			if(!m_bVolResize)
				currentTask.TaskNum = INM_VCON_TASK_5;
			else
				currentTask.TaskNum = INM_VCON_TASK_6;
			currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			nextTask.TaskNum = m_strOperation;
			nextTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
			ModifyTaskInfoForCxApi(currentTask, nextTask, true);
			UpdateTaskInfoToCX(true);
		}
	}while(0);

	if(1 == iResult)
	{
		if(!m_bUpdateStep)
		{
			DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
			ModifyTaskInfoForCxApi(currentTask, nextTask);
			UpdateTaskInfoToCX();
		}
		if(m_bVolResize && m_bVolPairPause)
		{
			DebugPrintf(SV_LOG_INFO,"Resuming the protected pairs...\n");
			if(false == ResumeAllReplicationPairs())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to resume the protected pairs!!!\n");
			}
		}

		if(bIsAdditionOfDisk && m_bVolPairPause)
		{
			DebugPrintf(SV_LOG_INFO,"Resuming the protected pairs...\n");
			map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.begin();
			for(; iterDyn != m_mapHostIdDynAddDisk.end(); iterDyn++)
			{
				if(true == iterDyn->second)
				{
					if(false == ResumeReplicationPairs(iterDyn->first, m_strMtHostID))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to resume the protected pairs!!!\n");
					}
				}
			}
		}
	}

	if (!m_bPhySnapShot && !m_bVolResize)
	{
		map<string, SendAlerts> sendAlertsInfo;
		if (bUpdateToCXbyMTid)
			UpdateSendAlerts(currentTask, m_strCxPath, sendAlertsInfo);
		else
		{
			UpdateSrcHostStaus(currentTask);
			UpdateSendAlerts(sendAlertsInfo);
		}
		if (!SendAlertsToCX(sendAlertsInfo))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update the Alerts to CX!!!\n");
		}
	}

	if(!m_strCxPath.empty())
	{
		//uploads the Log file to CX
		if(false == UploadFileToCx(m_strLogFile, m_strCxPath))
			DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX\n", m_strLogFile.c_str());
		else
			DebugPrintf(SV_LOG_INFO, "Successfully upload file %s file to CX\n", m_strLogFile.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iResult;
}
/*
* Dumping of MBR/EBR is already done.We have map of master target's private volumes.
*/
int MasterTarget::processTargetSidedisks()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iResult = 0;

	taskInfo_t currentTask, nextTask;
	string strErrorMsg = ""; 
	string strFixStep = "";
	do
	{
		if(m_bPhySnapShot)
			DebugPrintf(SV_LOG_INFO,"\nProcessing the volumes on target disks to prepare for Snapshot.\n\n");
		else
			DebugPrintf(SV_LOG_INFO,"\nProcessing the volumes on target disks to prepare for replication.\n\n");

		Sleep(10000); //Sleeping for 10 secs to Mount Mgr to update.

		if(bIsAdditionOfDisk)
		{
			//Including the disks information of dynamic disks to the m_mapOfVmToSrcAndTgtDiskNmbrs map. This is reuired to create the volumes
			map<string, map<int, int>>::iterator iterMap, iterAddDynDisk;
			map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.begin();
			for(; iterDyn != m_mapHostIdDynAddDisk.end(); iterDyn++)
			{
				if(iterDyn->second)
				{
					iterAddDynDisk = m_mapOfHostToDiskNumMap.find(iterDyn->first);
					iterMap = m_mapOfVmToSrcAndTgtDiskNmbrs.find(iterDyn->first);
					if(iterAddDynDisk != m_mapOfHostToDiskNumMap.end() && iterMap != m_mapOfVmToSrcAndTgtDiskNmbrs.end())
					{
						map<int, int>::iterator iterDiskNum = iterAddDynDisk->second.begin();
						for(; iterDiskNum != iterAddDynDisk->second.end(); iterDiskNum++)
						{
							iterMap->second.insert(make_pair(iterDiskNum->first, iterDiskNum->second));
						}
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"\nRespective host [%s] does not exist in m_mapOfHostToDiskNumMap or m_mapOfVmToSrcAndTgtDiskNmbrs.\n", iterDyn->first.c_str());
					}
				}
			}
		}

		m_bUpdateStep = true;
		if(false == mapSrcAndTgtVolumes())
		{
			if(!m_bVolResize)
			{
				currentTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskNum = INM_VCON_TASK_5;
			}
			else
			{
				currentTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskNum = INM_VCON_TASK_6;
			}
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = string("Failed to assign mount ponits to the newly created Volumes.") +
									string("For extended Error information download EsxUtilWin.log in vCon Wiz");
			currentTask.TaskFixSteps = string("Check the MT disks are in proper state or not ") +  
							string("Check the volume layouts created on teh disks") +
							string("Rerun the job. If still fails contact inmage customer support");
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
			ModifyTaskInfoForCxApi(currentTask, nextTask);
			UpdateTaskInfoToCX();

			DebugPrintf(SV_LOG_ERROR,"mapSrcAndTgtVolumes() failed.\n");
			iResult = 1;
			break;
		}

		if(!m_bVolResize)
		{
			currentTask.TaskNum = INM_VCON_TASK_4;
			nextTask.TaskNum = INM_VCON_TASK_5;
		}
		else
		{
			currentTask.TaskNum = INM_VCON_TASK_5;
			nextTask.TaskNum = INM_VCON_TASK_6;
		}
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
		ModifyTaskInfoForCxApi(currentTask, nextTask);
		UpdateTaskInfoToCX();

		if(!m_bPhySnapShot && !m_bVolResize)
		{
			if(bIsAdditionOfDisk && m_bVolPairPause)
			{
				DebugPrintf(SV_LOG_INFO,"Resuming the protected pairs...\n");
				map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.begin();
				for(; iterDyn != m_mapHostIdDynAddDisk.end(); iterDyn++)
				{
					if(true == iterDyn->second)
					{
						if(false == ResumeReplicationPairs(iterDyn->first, m_strMtHostID))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to resume the protected pairs!!!\n");
						}
						if(false == RestartResyncPairs(iterDyn->first, m_strMtHostID))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to restart resync the protected pairs!!!\n");
						}
					}
				}
			}
			m_bVolResize = false;

			if(false == SetVxJobs(strErrorMsg, strFixStep))
			{
				m_bUpdateStep = true;
				currentTask.TaskNum = INM_VCON_TASK_5;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = strErrorMsg;
				currentTask.TaskFixSteps = strFixStep;
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
				DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
				ModifyTaskInfoForCxApi(currentTask, nextTask, true);
				UpdateTaskInfoToCX(true);

				DebugPrintf(SV_LOG_ERROR,"SetVxJobs() failed.\n");
				iResult = 2;
				break;
			}
		}
		else if(m_bPhySnapShot)
		{
			DebugPrintf(SV_LOG_INFO,"All the required files are created in Master target for taking physical snapshot\n");
		}
		else
		{			
			if(false == RegisterHostUsingCdpCli())
			{
				DebugPrintf(SV_LOG_ERROR,"\nFailed to Register the Master Target disks using cdpcdli\n\n");
			}
			DebugPrintf(SV_LOG_INFO,"Resuming the protected pairs...\n");
			if(false == ResumeAllReplicationPairs())
			{
				m_bUpdateStep = true;
				currentTask.TaskNum = INM_VCON_TASK_6;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Resume for some of the pairs failed";
				currentTask.TaskFixSteps = "Do resume manually by using CXUI";
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
				DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
				ModifyTaskInfoForCxApi(currentTask, nextTask, true);
				UpdateTaskInfoToCX(true);

				DebugPrintf(SV_LOG_INFO,"Failed to resume the protected pairs!!!\n");
				iResult = 1;
				break;
			}
			m_bVolPairPause = false;
			if(false == RestartResyncforAllPairs())
			{
				m_bUpdateStep = true;
				currentTask.TaskNum = INM_VCON_TASK_6;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Restart Resync failed for some of the pairs.";
				currentTask.TaskFixSteps = "Do restart Resync manually by using CX UI";
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
				DebugPrintf(SV_LOG_INFO,"Updating %s As %s\n", currentTask.TaskNum.c_str(), currentTask.TaskStatus.c_str());
				ModifyTaskInfoForCxApi(currentTask, nextTask, true);
				UpdateTaskInfoToCX(true);

				DebugPrintf(SV_LOG_ERROR,"Failed to restart resync the protected pairs!!!\n");
				iResult = 1;
				break;
			}
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iResult;
}

//Rescanning the disks configuration using diskpart
BOOL MasterTarget::rescanIoBuses()
{	
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bResult = TRUE;
	string strParameterToDiskPartExe;
	string strExeName;
	string strDiskPartScriptFilePath = m_strDataFolderPath + RESCAN_DISKS_FILE ;
	ofstream outfile(strDiskPartScriptFilePath.c_str(), ios_base::out);
	bool bNeedToCallRevert = false;
	bNeedToCallRevert = enableDiableFileSystemRedirection() ;	

	do
	{
		if(outfile.fail())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s. Cannot continue further.\n",strDiskPartScriptFilePath.c_str());
			bResult = FALSE;
			break;
		}
		outfile<<RESCAN_DISK_SWITCH;
		outfile<<endl;
		outfile.flush();
		outfile.close();
		strExeName = string(DISKPART_EXE);
		strParameterToDiskPartExe = string (" /s ") +  string("\"") + strDiskPartScriptFilePath + string("\"");
		
		if(FALSE == ExecuteProcess(strExeName,strParameterToDiskPartExe))
		{
			 bResult = false;
			 DebugPrintf(SV_LOG_ERROR,"Rescanning of IO buses failed. Please run rescan from diskmgmt.msc manually and then try again.\n");
		}
		if(bNeedToCallRevert)
		{
			enableDiableFileSystemRedirection( false ) ;
		}
	}while(0);

    DeleteFile(strDiskPartScriptFilePath.c_str());
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bResult;
}

//Read the File with 4 Value separated by following separator.
//Separator - !@!@!
//Sample Format - val1!@!@!val2!@!@!val3!@!@!val4
//Input - File Name , Output - map of values in the files.
//Return value - true if file is successfully read and in correct format; else false.  
bool MasterTarget::ReadDiskInfoFile(string strInfoFile, map<string, dInfo_t>& mapPairs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	string token("!@!@!");
	size_t index = 0;
    string strLineRead;

    if(false == checkIfFileExists(strInfoFile))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strInfoFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    ifstream readFile(strInfoFile.c_str());
    if (!readFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strInfoFile.c_str(),GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
        return false;
    }
    while(getline(readFile,strLineRead))
    {
		if(strLineRead.empty())
		{
			DebugPrintf(SV_LOG_DEBUG, "Reached end of the file %s", strInfoFile.c_str());
			break;
		}

		
		string strTargetDiskSig;
		dInfo_t dinfo;
		vector<string> v;
        DebugPrintf(SV_LOG_DEBUG,"strLineRead =  %s\n",strLineRead.c_str());
		try
		{
			index = 0;
			size_t found = strLineRead.find(token, index);
			while(found != std::string::npos)
			{
				DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",strLineRead.substr(index,found-index).c_str());
				v.push_back(strLineRead.substr(index,found-index));
				index = found + 5; 
				found = strLineRead.find(token, index);            
			}
			DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",strLineRead.substr(index).c_str());
			v.push_back(strLineRead.substr(index));
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_DEBUG,"failed while reading the file, try to read in old format");
			bResult = false;
            break;
		}

        if(v.size() < 4)
        {
            DebugPrintf(SV_LOG_DEBUG,"Number of elements in this line - %u\n",v.size());
			bResult = false;
            break;
        }
		dinfo.DiskNum = v[0];
		dinfo.DiskSignature = v[1];
		dinfo.DiskDeviceId = v[2];
		strTargetDiskSig = v[3];

		mapPairs.insert(make_pair(strTargetDiskSig, dinfo));
    }
	readFile.close();

	if(!bResult)
	{
		bResult = true;
		ifstream readFile(strInfoFile.c_str());
		if (!readFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strInfoFile.c_str(),GetLastError());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
			return false;
		}
		while(getline(readFile,strLineRead))
		{
			if(strLineRead.empty())
			{
				DebugPrintf(SV_LOG_DEBUG, "Reached end of the file %s", strInfoFile.c_str());
				break;
			}
			string strSecondValue;
			dInfo_t dinfo;
			DebugPrintf(SV_LOG_DEBUG,"strLineRead =  %s\n",strLineRead.c_str());
			index = strLineRead.find_first_of(token);
			if(index != std::string::npos)
			{
				dinfo.DiskNum = strLineRead.substr(0,index);
				strSecondValue = strLineRead.substr(index+(token.length()),strLineRead.length());
				DebugPrintf(SV_LOG_DEBUG,"strFirstValue = %s  <==>  ", dinfo.DiskNum.c_str());
				DebugPrintf(SV_LOG_DEBUG,"strSecondValue = %s\n", strSecondValue.c_str());
				if ((!dinfo.DiskNum.empty()) && (!strSecondValue.empty()))
					mapPairs.insert(make_pair(strSecondValue,dinfo));
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strInfoFile.c_str());
				bResult = false;
				mapPairs.clear();
			}
		}
		readFile.close();
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bResult;
}

//Read the File with 2 Value separated by following separator.
//Separator - !@!@!
//Sample Format - val1!@!@!val2
//Input - File Name , Output - map of values in the files.
//Return value - true if file is successfully read and in correct format; else false.            
bool MasterTarget::ReadFileWith2Values(string strFileName, map<string,string> &mapOfStrings)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    std::string token("!@!@!");
	std::size_t index;
    string strLineRead;
    string strFirstValue,strSecondValue;

    if(false == checkIfFileExists(strFileName))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strFileName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    ifstream readFile(strFileName.c_str());
    if (!readFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFileName.c_str(),GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
        return false;
    }
    while(getline(readFile,strLineRead))
    {
		if(strLineRead.empty())
		{
			DebugPrintf(SV_LOG_DEBUG, "Reached end of the file %s", strFileName.c_str());
			break;
		}
        stringstream sstream;
        sstream << strLineRead;
        DebugPrintf(SV_LOG_DEBUG,"strLineRead =  %s\n",strLineRead.c_str());
        index = strLineRead.find_first_of(token);
        if(index != std::string::npos)
		{
            strFirstValue = strLineRead.substr(0,index);
            strSecondValue = strLineRead.substr(index+(token.length()),strLineRead.length());
            DebugPrintf(SV_LOG_DEBUG,"strFirstValue = %s  <==>  ",strFirstValue.c_str());
            DebugPrintf(SV_LOG_DEBUG,"strSecondValue = %s\n",strSecondValue.c_str());
            if ((!strFirstValue.empty()) && (!strSecondValue.empty()))
                mapOfStrings.insert(make_pair(strFirstValue,strSecondValue));
        }
        else
		{
            DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strFileName.c_str());
			readFile.close();
            mapOfStrings.clear();
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);			
			return false;
		}
    }
	readFile.close();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

/*******************************************************************************************************************************************************
Create the map for each VM to map of Source and Target Disk Numbers (mapOfVmToSrcAndTgtDiskNmbrs).
1. Read Inmage_scsi_unit_disks.txt file and get the map of Src and Tgt SCSI Ids per Vm
2. Within the same loop, using the vmname fetch the map of Src SCSI ID and disk number from vmname_ScsiIDsandDiskNmbrs.txt (or) "m_mapOfHostToDiskInfo"
3. Generate the map of Src Disk number to Tgt SCSI ID using above two maps.
4. Generate the map of Src Disk number to Tgt Disk number using map of Tgt Disk numbers and SCSI IDs(obtained via WMI earlier)
Return value  - true if all the maps are successfully created
              - false else
*********************************************************************************************************************************************************/
bool MasterTarget::createMapOfVmToSrcAndTgtDiskNmbrs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetvalue = true;
    string strCommonPathName;
	if(m_strDirPath.empty())
		strCommonPathName = m_strDataFolderPath;
	else
		strCommonPathName = m_strDirPath + string("\\");
	string strFileName = strCommonPathName + string(SCSI_DISKIDS_FILE_NAME);
    string strDSFileExtensionName = string(SCSI_DISK_FILE_NAME);
    string strDSFileName; //This will have full path to _DiskAndScsiIDs.txt per vm
    string token("!@!@!");
	size_t index;
    string strVmName;
    string strLineRead;
    map<string,string> mapOfSrcScsiIdsAndSrcDiskNmbrs; //map of src scsi ids to src disk numbers 
    map<string,string> mapOfSrcScsiIdsAndTgtScsiIds; //map of src scsi ids to tgt scsi ids    
    map<string,string> mapOfSrcDiskNmbrsAndTgtScsiIds; //map of src disk numbers to tgt scsi ids (result of above two)
    map<string,string> mapOfSrcDiskNmbrsAndTgtDiskNmbrs; //map of src to tgt disk numbers obtained by above two

	std::map<string, map<string, string> > mapVMtoScsiDiskSign;
	std::multimap<string, pair<string, string> > mapMissingClusDisks;

    if(false == checkIfFileExists(strFileName))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    ifstream inFile(strFileName.c_str());
    do
    {
        if(!inFile.is_open())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFileName.c_str(),GetLastError());
	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            bRetvalue = false;
            break;
        }
		while(getline(inFile,strLineRead))
        {			
			if(strLineRead.empty())
			{
				DebugPrintf(SV_LOG_INFO, "End of file reached...\n" );
				break;
			}

			string strVmName, strVmHostId;
			if(m_vConVersion >= 4.0)
			{
				size_t pos1 = strLineRead.find_first_of("(");
				size_t pos2 = strLineRead.find_last_of(")");
				if(pos1 != string::npos)
				{
					strVmHostId = strLineRead.substr(0, pos1);
					if(pos2 != string::npos)
						strVmName = strLineRead.substr(pos1+1, pos2-(pos1+1));
					else
						strVmName = strLineRead.substr(pos1+1);
				}
				else
					strVmHostId = strLineRead;
			}

			else
				strVmName = strLineRead;
			
			DebugPrintf(SV_LOG_INFO,"\n");
			DebugPrintf(SV_LOG_INFO,"VM Name : %s\n",strVmName.c_str());
			strDSFileName = m_strDataFolderPath + strVmName + strDSFileExtensionName;

			mapOfSrcScsiIdsAndSrcDiskNmbrs.clear();
			mapOfSrcScsiIdsAndTgtScsiIds.clear();
			mapOfSrcDiskNmbrsAndTgtScsiIds.clear();
			mapOfSrcDiskNmbrsAndTgtDiskNmbrs.clear();
			std::map<string,string> mapScsiDiskSign;

			if(getline(inFile,strLineRead))
			{
				map<string, map<string, string> >::iterator iterNode;
				bool bClusterNode = false;
				if(IsClusterNode(strVmHostId))
				{
					bClusterNode = true;

					iterNode = m_mapNodeDiskSignToDiskType.find(strVmHostId);
					if(iterNode == m_mapNodeDiskSignToDiskType.end())
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to find the cluster disk information for node [%s] provided in file : %s\n",
							strVmHostId.c_str(), strFileName.c_str());
						bRetvalue = false;
						break;
					}
				}
				stringstream sstream ;
				sstream << strLineRead;
				int iCount;
				sstream >> iCount;
				for(int i = 0; i < iCount; i++ )
				{
					string strSrcScsiId;
					string strTgtScsiId;
					if(getline(inFile,strLineRead))
					{
						//listOfScsiIds.push_back(strLineRead);
						index = strLineRead.find_first_of(token);
						if(index != std::string::npos)
						{
							strSrcScsiId = strLineRead.substr(0,index);
							strTgtScsiId = strLineRead.substr(index+(token.length()),strLineRead.length());
							DebugPrintf(SV_LOG_INFO,"strSrcScsiId =  %s  <==>  ",strSrcScsiId.c_str());
							DebugPrintf(SV_LOG_INFO,"strTgtScsiId =  %s\n",strTgtScsiId.c_str());
							if ((!strSrcScsiId.empty()) && (!strTgtScsiId.empty()))
							{
								if(bClusterNode)
								{
									string strDiskSign;
									size_t pos3 = strSrcScsiId.find_first_of("(");
									size_t pos4 = strSrcScsiId.find_last_of(")");
									if(pos3 != string::npos)
									{
										if(pos4 != string::npos)
										{
											strDiskSign = strSrcScsiId.substr(pos3+1, pos4-(pos3+1));
											map<string, string>::iterator iterDisk;											
											iterDisk = iterNode->second.find(strDiskSign);
											if(iterDisk == iterNode->second.end())
											{
												DebugPrintf(SV_LOG_ERROR,"Failed to find the disk signature [%s] under XML file information.\n",strDiskSign.c_str());
												bRetvalue = false;
												break;
											}
											if(iterDisk->second.compare("MBR") == 0)
												strDiskSign = ConvertHexToDec(strDiskSign);
										}
										else
										{
											DebugPrintf(SV_LOG_ERROR,"Failed to find the disk signature in the file : %s\n",strFileName.c_str());
							                bRetvalue = false;
							                break;
										}
										strSrcScsiId = strDiskSign;
									}
								}
								mapOfSrcScsiIdsAndTgtScsiIds.insert(make_pair(strSrcScsiId,strTgtScsiId));
							}
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strFileName.c_str());
							bRetvalue = false;
							break;
						}
					}
					else
					{
						inFile.close();
						DebugPrintf(SV_LOG_ERROR,"1.Cant read the disk mapping file.Cannot continue.\n");
						bRetvalue = false;
						break;
					}
				}
				if(bRetvalue == false)
					break;
			}
			else
			{
				inFile.close();
				DebugPrintf(SV_LOG_ERROR,"2.Cant read the disk mapping file.Cannot continue..\n");
				bRetvalue = false;
				break;
			}

			strLineRead.clear();

            if(bRetvalue == false)
                break;
			else if(mapOfSrcScsiIdsAndTgtScsiIds.empty() && bIsAdditionOfDisk)
			{
				DebugPrintf(SV_LOG_INFO,"Addition of disk so proceeding further even though scsi mapping does not exists for VM : %s\n",strVmHostId.c_str());
				continue;
			}
            else
            {
				if(false == ReadSrcScsiIdAndDiskNmbrFromMap(strVmHostId, mapOfSrcScsiIdsAndSrcDiskNmbrs))
				{
					DebugPrintf(SV_LOG_INFO,"Failed to fetch the Source Disk Numbers and SCSI IDs for this VM : %s\n",strVmName.c_str());
					if(false == ReadFileWith2Values(strDSFileName,mapOfSrcScsiIdsAndSrcDiskNmbrs))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Source Disk Numbers and SCSI IDs for this VM : %s\n",strVmName.c_str());
						bRetvalue = false;
						break;
					}
				}
				
                if(mapOfSrcScsiIdsAndSrcDiskNmbrs.empty())
                {
                    DebugPrintf(SV_LOG_ERROR,"Map of Source SCSI IDs and Disk Numbers is empty.\n");
                    bRetvalue = false;
                    break;
                }

                double dlmVesion = m_mapOfHostIdDlmVersion.find(strVmHostId) == m_mapOfHostIdDlmVersion.end() ? 1.0 : m_mapOfHostIdDlmVersion[strVmHostId];

				//This is required for W2K3 cluster case if we found any disk missing from MBR files
				mapScsiDiskSign.clear();
				if(dlmVesion >= 1.2 && IsClusterNode(strVmHostId) && IsWin2k3OS(strVmHostId))
				{
					if(false == GenerateSrcScsiIdToDiskSigFromXml(strVmHostId, mapOfSrcScsiIdsAndTgtScsiIds, mapScsiDiskSign))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to find the source disk scsi id to source disk signature mapping from xml file\n");
						bRetvalue = false;
						break;
					}
					mapVMtoScsiDiskSign[strVmHostId] = mapScsiDiskSign;
				}
				bool bMissingDiskExist = false;
                //We have the map for Src and Tgt Scsi Ids (mapOfSrcScsiIdsAndTgtScsiIds) from Inmage_scsi_unit_disks.txt.
                //And we have map for Src Disk Numbers and Scsi Ids (mapOfSrcScsiIdsAndSrcDiskNmbrs).
                //Hence we will create map of Src Disk numbers to Tgt Scsi Ids (mapOfSrcDiskNmbrsAndTgtScsiIds).
                //Take the first from mapOfSrcScsiIdsAndTgtScsiIds and search in mapOfSrcScsiIdsAndSrcDiskNmbrs
                map<string,string>::iterator iterSSTS = mapOfSrcScsiIdsAndTgtScsiIds.begin();
                map<string,string>::iterator iterFind;
                while(iterSSTS != mapOfSrcScsiIdsAndTgtScsiIds.end())
                {
                    iterFind = mapOfSrcScsiIdsAndSrcDiskNmbrs.find(iterSSTS->first);
                    if (iterFind != mapOfSrcScsiIdsAndSrcDiskNmbrs.end())
                    {
                        DebugPrintf(SV_LOG_INFO,"iterFind->second=%s <==> iterSSTS->second=%s\n",iterFind->second.c_str(),iterSSTS->second.c_str());
                        mapOfSrcDiskNmbrsAndTgtScsiIds.insert(make_pair(iterFind->second,iterSSTS->second));
                    }
					else if(dlmVesion >= 1.2 && IsClusterNode(strVmHostId) && IsWin2k3OS(strVmHostId))
					{
						DebugPrintf(SV_LOG_INFO,"Missing disk scsi entry [%s]->[%s] for %s\n",
							iterSSTS->first.c_str(),
							iterSSTS->second.c_str(),
							strVmHostId.c_str());
						mapMissingClusDisks.insert(make_pair(strVmHostId,make_pair(iterSSTS->first,iterSSTS->second)));
						bMissingDiskExist = true;
					}
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR,"Searching for source SCSI ID : %s\n",iterSSTS->first.c_str());
                        DebugPrintf(SV_LOG_ERROR,"Failed to find the above SCSI entry in mapOfSrcScsiIdsAndSrcDiskNmbrs\n");
                        bRetvalue = false;
                        break;
                    }
                    iterSSTS++;
                }                
                if(bRetvalue == false)
				{
					//This code is only for P2V and written to get rid of multipath scsi id issue.
					//Here we will create the source disk number to target scsi id by using the xml file.
					//we will pass the mapOfSrcScsiIdsAndTgtScsiIds as input and will get the oupt as mapOfSrcDiskNmbrsAndTgtScsiIds
					bool bp2v;
					map<string,bool>::iterator p2vIter= m_MapHostIdMachineType.find(strVmHostId);
					if(p2vIter != m_MapHostIdMachineType.end())
						bp2v = p2vIter->second;
					else
						bp2v = false;

					if(bp2v)
					{
						DebugPrintf(SV_LOG_INFO,"Physical machine, Trying with device name\n");
						mapOfSrcDiskNmbrsAndTgtScsiIds.clear();
						if(false == GenerateSrcDiskToTgtScsiIDFromXml(strVmHostId, mapOfSrcScsiIdsAndTgtScsiIds, mapOfSrcDiskNmbrsAndTgtScsiIds))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to find the source disk number to target scsi id disk mapping from xml file\n");
							bRetvalue = false;
							break;
						}
						else
							bRetvalue = true;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Virtual machine\n");
						break;
					}
				}

                if(mapOfSrcDiskNmbrsAndTgtScsiIds.empty())
                {
					if(bMissingDiskExist)
						continue;
                    DebugPrintf(SV_LOG_ERROR,"Map of Source Disk Numbers to Target SCSI IDs is empty.\n");
                    bRetvalue = false;
                    break;
                }
				else
					bRetvalue = true;

                //So now we have map of Src Disk numbers to Tgt Scsi Ids(mapOfSrcDiskNmbrsAndTgtScsiIds).
                //Using map of Tgt Disk numbers and SCSI IDs obtained via WMI earlier(m_mapOfDeviceIdAndControllerIds)
                //We will create required map of Src to Tgt Disk numbers(mapOfSrcDiskNmbrsAndTgtDiskNmbrs)
                //Take the second from mapOfSrcDiskNmbrsAndTgtScsiIds and search in m_mapOfDeviceIdAndControllerIds
				//V2P case the target device entry in inmage_scsi_unit_disks.txt file is device number (Added post 8.0).
				map<string,string>::iterator iterSDTS;
				if(m_bV2P)
				{
					for(iterSDTS = mapOfSrcDiskNmbrsAndTgtScsiIds.begin(); iterSDTS != mapOfSrcDiskNmbrsAndTgtScsiIds.end(); iterSDTS++)
					{
						if(iterSDTS->second.find(":") != string::npos)   // This means target device entry is found as scsi id format (If vCon is lower then 8.0 then it may come)
						{
							mapOfSrcDiskNmbrsAndTgtDiskNmbrs.clear();
							break;
						}
						else
						{
							DebugPrintf(SV_LOG_INFO,"iterSDTS->first=%s  <==>  iterSDTS->second=%s\n",iterSDTS->first.c_str(),iterSDTS->second.c_str());
							mapOfSrcDiskNmbrsAndTgtDiskNmbrs.insert(make_pair(iterSDTS->first, iterSDTS->second));
						}
					}
				}
				if(!m_bV2P || (m_bV2P && mapOfSrcDiskNmbrsAndTgtDiskNmbrs.empty()))
				{
					string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE";                
					iterSDTS = mapOfSrcDiskNmbrsAndTgtScsiIds.begin();
					while(iterSDTS != mapOfSrcDiskNmbrsAndTgtScsiIds.end())
					{
						iterFind = m_mapOfDeviceIdAndControllerIds.find(iterSDTS->second);
						if (iterFind != m_mapOfDeviceIdAndControllerIds.end())
						{
							try
							{
								string strTemp = iterFind->second.substr(strPhysicalDeviceName.size());
								DebugPrintf(SV_LOG_INFO,"iterSDTS->first=%s  <==>  strTemp=%s\n",iterSDTS->first.c_str(),strTemp.c_str());
								mapOfSrcDiskNmbrsAndTgtDiskNmbrs.insert(make_pair(iterSDTS->first,strTemp));
							}
							catch(...)
							{
								DebugPrintf(SV_LOG_ERROR,"[Exception]Failed to find the SCSI entries in m_mapOfDeviceIdAndControllerIds\n");
								bRetvalue = false;
								break;
							}
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Searching for target SCSI entry : %s\n",iterSDTS->second.c_str());
							DebugPrintf(SV_LOG_ERROR,"Failed to find the above SCSI entry in m_mapOfDeviceIdAndControllerIds\n");
							bRetvalue = false;
							break;
						}
						iterSDTS++;
					}	
				}
                if(bRetvalue == false)
                    break;

                if(mapOfSrcDiskNmbrsAndTgtDiskNmbrs.empty())
                {
					if(bMissingDiskExist)
						continue;
                    DebugPrintf(SV_LOG_ERROR,"Map of Source Disk Numbers to Target Disk Numbers is empty.\n");
                    bRetvalue = false;
                    break;
                }

                map<int,int> mapOfDiskNmbrs; //convert mapOfSrcDiskNmbrsAndTgtDiskNmbrs to integers
                if (false == ConvertMapofStringsToInts(mapOfSrcDiskNmbrsAndTgtDiskNmbrs,mapOfDiskNmbrs))
                {
                    DebugPrintf(SV_LOG_ERROR,"ConvertMapofStringsToInts failed.\n");
                    bRetvalue = false;
                    break;
                }

                //So we finally have map of Src to Tgt Disk numbers(mapOfDiskNmbrs)
                //Insert this into map of VMs to mapOfSrcDiskNmbrsAndTgtDiskNmbrs(mapOfVmToSrcAndTgtDiskNmbrs
				if(m_vConVersion >= 4.0)
					m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(strVmHostId,mapOfDiskNmbrs));
				else
					m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(strVmName,mapOfDiskNmbrs));

				if(m_vConVersion < 4.0)
				{
					//Update dInfo file with source and target scsi ids.
					//This will be used at recovery time.
					if (false == UpdateInfoFile(strVmName,mapOfSrcScsiIdsAndTgtScsiIds,string(DISKINFO_EXTENSION)))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to write source and target SCSI IDs info into file for VM - %s.\n",strVmName.c_str());
						bRetvalue = false;
						break;
					}
				}
            }
        }

    }while(0);

	if(!mapMissingClusDisks.empty())
	{
		std::multimap<string, pair<string, string> >::iterator iterMisDisk = mapMissingClusDisks.begin();
		for(;iterMisDisk != mapMissingClusDisks.end() && bRetvalue; iterMisDisk++)
		{
			if( IsClusterNode(iterMisDisk->first) && IsWin2k3OS(iterMisDisk->first) &&
				mapVMtoScsiDiskSign.find(iterMisDisk->first) != mapVMtoScsiDiskSign.end())
			{
				map<string,string>::iterator iterScsi = mapVMtoScsiDiskSign[iterMisDisk->first].find(iterMisDisk->second.first);
				if(iterScsi == mapVMtoScsiDiskSign[iterMisDisk->first].end())
				{
					DebugPrintf(SV_LOG_ERROR,"Disk information not found for [%s]=[%s] in %s mbr file.\n",
						iterMisDisk->second.first.c_str(),
						iterMisDisk->second.second.c_str(),
						iterMisDisk->first.c_str());
					DebugPrintf(SV_LOG_ERROR,"The corresponding disk might not protected from this node. Hence skipping the corresponding disk\n");
				}
				else
				{
					string strDiskSing = iterScsi->second;
					list<string> nodes;
					GetClusterNodes(GetNodeClusterName(iterMisDisk->first),nodes);
					list<string>::iterator iterNode = nodes.begin();
					for(;iterNode != nodes.end(); iterNode++)
					{
						DisksInfoMapIter_t iterDiskInfo;
						if(GetClusterDiskMapIter(*iterNode,strDiskSing, iterDiskInfo))
						{
							DebugPrintf(SV_LOG_INFO,"Missing disk [%s]=[%s] information found in %s MBR file.\n",
							iterMisDisk->second.first.c_str(),
							iterMisDisk->second.second.c_str(),
							iterNode->c_str());
							map<string,string>::iterator iterTgtDisk =
								m_mapOfDeviceIdAndControllerIds.find(iterMisDisk->second.second);
							if(iterTgtDisk != m_mapOfDeviceIdAndControllerIds.end())
							{
								size_t pos = 17; //Size of "\\\\.\\PHYSICALDRIVE"
								string strTgtDiskNum = iterTgtDisk->second.substr(pos);
								string strSrcDiskNum = iterDiskInfo->first.substr(pos);
								int iSrcDisk = atoi(strSrcDiskNum.c_str()),
									iTgtDisk = atoi(strTgtDiskNum.c_str());
								DebugPrintf(SV_LOG_INFO,"Source disk %d => Target disk %d \n",iSrcDisk,iTgtDisk);
								if(m_mapOfVmToSrcAndTgtDiskNmbrs.find(*iterNode) != m_mapOfVmToSrcAndTgtDiskNmbrs.end())
								{
									m_mapOfVmToSrcAndTgtDiskNmbrs[*iterNode].insert(make_pair(iSrcDisk,iTgtDisk));
								}
								else
								{
									/*DebugPrintf(SV_LOG_ERROR,"Could not found Source-Target disk mapping for %s\n",iterNode->c_str());
									bRetvalue = false;*/
									map<int, int> mapTemp;
									mapTemp.insert(make_pair(iSrcDisk, iTgtDisk));
									m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(*iterNode, mapTemp));
								}
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR,"Could not found scsi id %s on MT.\n",iterMisDisk->second.second.c_str());
								bRetvalue = false;
							}
							break;
						}

					}
					if(iterNode == nodes.end())
					{ // disk information not found.
						DebugPrintf(SV_LOG_ERROR,"Disk information not found for [%s]=[%s].\n",
						iterMisDisk->second.first.c_str(),
						iterMisDisk->second.second.c_str());
						bRetvalue = false;
						break;
					}
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Disk-singnature mapping information not available for the host : %s.\n",
						iterMisDisk->first.c_str());
				bRetvalue = false;
				break;
			}
		}
	}

    if(m_mapOfVmToSrcAndTgtDiskNmbrs.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Map Of VMname to Source and Target Disk Numbers is empty.\n");
        bRetvalue = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetvalue;
}


bool MasterTarget::createMapOfVmToSrcAndTgtDiskNmbrs(bool bVolResize)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetvalue = true;
	string strPhysicalDrive = "\\\\?\\PhysicalDrive";

	string strDpinfoFile;

	map<string, string>::iterator iter = m_mapHostIdToHostName.begin();
	for(;iter != m_mapHostIdToHostName.end(); iter++)
	{
		map<int,int> mapOfDiskNmbrs;
		map<string, string> mapOfSrcDiskToTgtDisk;
		map<string, string> mapMissingDiskSigToTgtDiskNum;
		strDpinfoFile = m_strDataFolderPath + iter->first + string(".dpinfo");
		if(true == checkIfFileExists(strDpinfoFile))
		{
			map<string, dInfo_t> mapTgtDiskSigToSrcDiskInfo;
			if(false == ReadDiskInfoFile(strDpinfoFile, mapTgtDiskSigToSrcDiskInfo))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk information for VM - %s\n",iter->first.c_str());
				bRetvalue = false; break;
			}

			double dlmVesion = m_mapOfHostIdDlmVersion.find(iter->first) == m_mapOfHostIdDlmVersion.end() ? 1.0 : m_mapOfHostIdDlmVersion[iter->first];

			map<string, dInfo_t>::iterator iterDisk;
			DebugPrintf(SV_LOG_INFO, "Creating source to target disk mapping for host %s\n", iter->first.c_str());

			map<string, string>::iterator iterMap = m_diskNameToSignature.begin();
			for(; iterMap != m_diskNameToSignature.end(); iterMap++)
			{
				iterDisk = mapTgtDiskSigToSrcDiskInfo.find(iterMap->second);
				if(iterDisk != mapTgtDiskSigToSrcDiskInfo.end())
				{				
					size_t pos = iterMap->first.find(strPhysicalDrive);
					if(pos != string::npos)
					{				
						string strTgtDiskNum = iterMap->first.substr(strPhysicalDrive.length());
						if(!strTgtDiskNum.empty())
						{
							if(dlmVesion >= 1.2)
							{
								//Need to write the logic to identify the current source number from the new MBR file information
								map<string, DisksInfoMap_t>::iterator vmIter;
								vmIter = m_mapOfHostToDiskInfo.find(iter->first);
								if(vmIter == m_mapOfHostToDiskInfo.end())
								{
									DebugPrintf(SV_LOG_ERROR,"Failed to find the Scsi and disk number mapping for vm : %s \n", iter->first.c_str());
									DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
									return false;
								}

								bool bDiskMissing = true;
								DisksInfoMapIter_t iterDiskSrc = vmIter->second.begin();
								for(; iterDiskSrc != vmIter->second.end(); iterDiskSrc++)
								{
									if(iterDisk->second.DiskSignature.compare(iterDiskSrc->second.DiskSignature) == 0)
									{
										//17 is Size of "\\\\.\\PHYSICALDRIVE"
										mapOfSrcDiskToTgtDisk.insert(make_pair(iterDiskSrc->first.substr(17), strTgtDiskNum));
										bDiskMissing = false;
										break;
									}
								}
								if(bDiskMissing)
								{
									DebugPrintf(SV_LOG_INFO, "Disk Signature [%s] not exist in Mbr metadata info of host : %s.\n", iterDisk->second.DiskSignature.c_str(), iter->first.c_str());

                                    //Fix for upgrade from 7.0/7.1, where disk signature does not exist
									if(iterDisk->second.DiskSignature.empty())
										mapOfSrcDiskToTgtDisk.insert(make_pair(iterDisk->second.DiskNum, strTgtDiskNum));
									else if(!IsClusterNode(iter->first))
										mapOfSrcDiskToTgtDisk.insert(make_pair(iterDisk->second.DiskNum, strTgtDiskNum));
									else
										mapMissingDiskSigToTgtDiskNum.insert(make_pair(iterDisk->second.DiskSignature, strTgtDiskNum));
								}
							}
							else
							{
								mapOfSrcDiskToTgtDisk.insert(make_pair(iterDisk->second.DiskNum, strTgtDiskNum));
							}
						}
					}
				}
				else
				{
					//Target Disk Signature dose not exist in the dpinfo file of this host.
				}
			}
			
			if ((!mapOfSrcDiskToTgtDisk.empty()) && (false == ConvertMapofStringsToInts(mapOfSrcDiskToTgtDisk,mapOfDiskNmbrs)))
			{
				DebugPrintf(SV_LOG_ERROR,"ConvertMapofStringsToInts failed or the mapOfSrcDiskToTgtDisk is empty.\n");
				bRetvalue = false;
				break;
			}
			
		}
		else if(IsClusterNode(iter->first))
		{
			DebugPrintf(SV_LOG_INFO,"Proceeding further without any failure as its a cluster node...\n", iter->first.c_str());
			continue;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Failed to get the file %s for VM - %s, considering this as upgraded from 6.2.1\n",strDpinfoFile.c_str(), iter->second.c_str());
			if(false == GenerateDisksMap(iter->second, mapOfDiskNmbrs))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the map of source to target disk numbers for VM - %s\n", iter->second.c_str());
				bRetvalue = false; break;
			}
		}

		map<string, map<int, int> >::iterator iterMapNode;
		if(bIsAdditionOfDisk)
		{
			iterMapNode = m_mapOfHostToDiskNumMap.find(iter->first);
			if(iterMapNode != m_mapOfHostToDiskNumMap.end())
			{
				iterMapNode->second.insert(mapOfDiskNmbrs.begin(), mapOfDiskNmbrs.end());
			}
			else
			{
				m_mapOfHostToDiskNumMap.insert(make_pair(iter->first, mapOfDiskNmbrs));
			}
		}
		else
		{
			iterMapNode = m_mapOfVmToSrcAndTgtDiskNmbrs.find(iter->first);
			if(iterMapNode != m_mapOfVmToSrcAndTgtDiskNmbrs.end())
			{
				iterMapNode->second.insert(mapOfDiskNmbrs.begin(), mapOfDiskNmbrs.end());
			}
			else
			{
				m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(iter->first, mapOfDiskNmbrs));
			}
		}

		//Logic to get the missing disks information from the hosts
		if(!mapMissingDiskSigToTgtDiskNum.empty())
		{
			mapOfSrcDiskToTgtDisk.clear();
			mapOfDiskNmbrs.clear();
			//map<string, map<string, string> >::iterator iterMapMissing = mapHostToMissingDisks.begin();			
			DebugPrintf(SV_LOG_INFO, "Proceesing Missing Disk information for host : %s.\n", iter->first.c_str());
			if(IsClusterNode(iter->first))
			{
				list<string> nodes;
				GetClusterNodes(GetNodeClusterName(iter->first),nodes);
				for(map<string, string>::iterator iterMissingDisk = mapMissingDiskSigToTgtDiskNum.begin(); iterMissingDisk != mapMissingDiskSigToTgtDiskNum.end(); iterMissingDisk++)
				{					
					list<string>::iterator iterNode = nodes.begin();
					for(;iterNode != nodes.end(); iterNode++)
					{
						if(iterNode->compare(iter->first) == 0)
							continue;

						DisksInfoMapIter_t iterDiskInfo;
						if(GetClusterDiskMapIter(*iterNode,iterMissingDisk->first, iterDiskInfo))
						{
							//17 is Size of "\\\\.\\PHYSICALDRIVE"
							mapOfSrcDiskToTgtDisk.insert(make_pair(iterDiskInfo->first.substr(17), iterMissingDisk->second));
							ConvertMapofStringsToInts(mapOfSrcDiskToTgtDisk,mapOfDiskNmbrs);
							if(bIsAdditionOfDisk)
							{
								iterMapNode = m_mapOfHostToDiskNumMap.find(*iterNode);
								if(iterMapNode != m_mapOfHostToDiskNumMap.end())
								{
									iterMapNode->second.insert(mapOfDiskNmbrs.begin(), mapOfDiskNmbrs.end());
								}
								else
								{
									m_mapOfHostToDiskNumMap.insert(make_pair(*iterNode, mapOfDiskNmbrs));
								}
							}
							else
							{
								iterMapNode = m_mapOfVmToSrcAndTgtDiskNmbrs.find(*iterNode);
								if(iterMapNode != m_mapOfVmToSrcAndTgtDiskNmbrs.end())
								{
									iterMapNode->second.insert(mapOfDiskNmbrs.begin(), mapOfDiskNmbrs.end());
								}
								else
								{
									m_mapOfVmToSrcAndTgtDiskNmbrs.insert(make_pair(*iterNode, mapOfDiskNmbrs));
								}
							}
						}
					}
				}
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetvalue;
}

//Convert the map of strings to map of integers
//Input - Map of strings 
//Output - Map of integers
//Return value - true if successfull and false else
bool MasterTarget::ConvertMapofStringsToInts(map<string,string> mapOfStrings, map<int,int> &mapOfIntegers)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,string>::iterator iterString;
    if (mapOfStrings.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Input Map of strings is empty. Cannot continue.\n");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    iterString = mapOfStrings.begin();
    while (iterString != mapOfStrings.end())
    {
        int iFirst,iSecond;
        DebugPrintf(SV_LOG_INFO,"iterString->first=%s  <==>  iterString->second=%s\n",iterString->first.c_str(),iterString->second.c_str());
        if(isAnInteger(iterString->first.c_str()))
            iFirst = atoi(iterString->first.c_str());
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert %s into integer(1).\n",iterString->first.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        if(isAnInteger(iterString->second.c_str()))
            iSecond = atoi(iterString->second.c_str());
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to convert %s into integer(2).\n",iterString->second.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
		mapOfIntegers.insert(make_pair(iFirst,iSecond));
        DebugPrintf(SV_LOG_INFO,"iFirst=%d  <==>  iSecond=%d\n",iFirst,iSecond);
        iterString++;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Get the vector from map of integers based on input (first or second values)
//Input - Map of Integers ,option to fetch first or second values[Accepted option - "first"|"second"]
//Output - Vector of Integers
//Return value - true if successfull and false else
bool MasterTarget::GetVectorFromMap(map<int,int> mapTemp,vector<int> &vecTemp,string strFirstOrSecond)    
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strFirst = string("first");
    string strSecond = string("second");
	map<int,int>::iterator iterTemp;

    if(mapTemp.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"Map is empty. Cannot convert to vector.\n");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    iterTemp = mapTemp.begin();
	while(iterTemp != mapTemp.end())
	{
		if (!( strcmp(strFirst.c_str(),strFirstOrSecond.c_str()) ))
		{
			DebugPrintf(SV_LOG_DEBUG,"iterTemp->first = %d\n",iterTemp->first);
			vecTemp.push_back(iterTemp->first);
		}
		else if (!( strcmp(strSecond.c_str(),strFirstOrSecond.c_str()) ))
		{
			DebugPrintf(SV_LOG_DEBUG,"iterTemp->second = %d\n",iterTemp->second);
			vecTemp.push_back(iterTemp->second);
		}
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Invalid input. strFirstOrSecond should be first/second.\n");
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
		iterTemp++;
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


//----------------------------------------------------------------------
// This module will fetch all volume Guids and then Mount these volume Guids in Mount Points(it'll create mount points if the CrtMntPt is true.otheriwse it'll just get Mount Point GUIDS).
// 1. First we are enumerating all mount points present in the master target .
// 2. Then we are mounting these guids in the default folder.
//----------------------------------------------------------------------
int MasterTarget::startTargetVolumeEnumeration(bool CrtMntPt = true,bool bEnumerateOnlyMasterTargetVolumes = false)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iResult = 0;
	TCHAR buf[BUFSIZE];      // buffer for unique volume identifiers
	HANDLE volumeHandle;     // handle for the volume scan
	BOOL bFlag;              // generic results flag
	BOOL bSource = FALSE;
	BOOL bTarget = FALSE;
	int iCmdLineArg = 1;
	map<string,string> tempMap;

	map<string,string>::iterator iter_beg;
	map<string,string>::iterator iter_end;

	int i = 0;
	for(; i < 3; i++)
	{
		m_mapOfGuidsWithLogicalNames.clear();
		// Open a scan for volumes.
		m_setVsnapVols.clear();

		CollectVsnapVols();
		volumeHandle = FindFirstVolume (buf, BUFSIZE );

		if (volumeHandle == INVALID_HANDLE_VALUE)
		{
			DebugPrintf(SV_LOG_ERROR,"No volume found in System...Retrying again(%d)\n", i);
			continue;
		}

		// We have a volume,now process it.
		bFlag = FetchAllTargetvolumes (volumeHandle, buf, BUFSIZE);

		// Do while we have volumes to process.
		while (bFlag) 
		{
			bFlag = FetchAllTargetvolumes (volumeHandle, buf, BUFSIZE);
		}

		// Close out the volume scan.
		bFlag = FindVolumeClose(volumeHandle);

		iter_beg = m_mapOfGuidsWithLogicalNames.begin();
		iter_end = m_mapOfGuidsWithLogicalNames.end();

			
		//Sleep(1000);

		/*In VISTA, OS is naming all the volumes with drive letters.This'll cause our create mount call to fail.
		Now,we are finding already present volumes in the master target and persisting this information in the map.
		Later when we are calling for the creation of mount point we'll look into this map and will deduce whether the GUID belongs to master target or not.If not,we'll mount his GUIDs.*/
		if(bEnumerateOnlyMasterTargetVolumes)
		{
			m_mapOfAlreadyPresentMasterVolumes = m_mapOfGuidsWithLogicalNames;
			
			if(false == GetMasterTargetVolumesExtent())
			{
				DebugPrintf(SV_LOG_ERROR,"GetMasterTargetVolumesExtent() failed...Retrying again (%d)", i);
				continue;
			}
			map<string, string>::iterator iterPvtVol = m_mapOfAlreadyPresentMasterVolumes.begin();
			DebugPrintf(SV_LOG_DEBUG,"Master Target Private Volumes are: \n");
			for(; iterPvtVol != m_mapOfAlreadyPresentMasterVolumes.end(); iterPvtVol++)
			{
				DebugPrintf(SV_LOG_DEBUG,"\t Vol GUID: %s \tVol Name: %s\n", iterPvtVol->first.c_str(), iterPvtVol->second.c_str());
			}
		   
		}
		if(i < 3)
		{
			DebugPrintf(SV_LOG_DEBUG,"Successfully retrived the required Volume information..Coming out of the loop\n\n");
			break;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"\nFailed to retrive the Volume information even after trying 3 times.\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return (-1);
		}
	}
	if(i == 3)
	{
		DebugPrintf(SV_LOG_ERROR,"\nFailed to retrive the Volume information even after trying 3 times.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return (-1);
	}

	map<string,string>::iterator iterMapOfMasterVolumes;
	static int  iVolCnt = 0;
	while(iter_beg != iter_end)
	{
		//iVolCnt cant be in a series. There might be some Volumes which are named in the Entire System(Fixed vols like Disk and Floppy Disks).We are not considering them,and hence 
		//Counter is increased by one. Next mountable volume will have name 2+ than the Previos(becoz of this).....
		iterMapOfMasterVolumes = m_mapOfAlreadyPresentMasterVolumes.find(iter_beg->first);
		vector<string>::iterator iterAlreadyExistingVolumes;
		if(iterMapOfMasterVolumes == m_mapOfAlreadyPresentMasterVolumes.end())
		{
			//See if the volume guid which we are going to mount is already mounted or not.If not only then create mount points.
			iterAlreadyExistingVolumes = find(m_vectorOfAlreadyMountedVolGuids.begin(),m_vectorOfAlreadyMountedVolGuids.end(),iter_beg->first);
			if(iterAlreadyExistingVolumes == m_vectorOfAlreadyMountedVolGuids.end())
			{
				if(CrtMntPt)
				{
					//If drive letters assigned at the time of Dumping MBR then deleting those mount points.
					if(!iter_beg->second.empty())
					{
						if(!DeleteVolumeMountPoint(iter_beg->second.c_str()))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to unmount the mount point - %s.\n",iter_beg->second.c_str());
							DebugPrintf(SV_LOG_ERROR,"Please unmount it manually and rerun the job.\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							iResult = -1;
							break;
						}
					}
					if(S_FALSE == createMountPointForVolumeGuid(iter_beg->first,iVolCnt))
					{
						DebugPrintf(SV_LOG_ERROR,"createMountPointForVolumeGuid() failed. Cannot continue..\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						iResult = -1;
						break;
					}
				}	
			}
		}	
		iter_beg++;
		iVolCnt++;
	}
	//Sleep(1000);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iResult;
}

BOOL MasterTarget::FetchAllTargetvolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bFlag;           // generic results flag for return
	DWORD dwSysFlags;     // flags that describe the file system
	TCHAR FileSysNameBuf[FILESYSNAMEBUFSIZE];
	TCHAR DeviceName[BUFSIZE];

	if(DRIVE_FIXED != GetDriveType(szBuffer))
	{
		bFlag = FindNextVolume(volumeHandle, szBuffer, iBufSize);
		return (bFlag);
	}

	char szVolumePath[MAX_PATH+1] = {0};
	char *pszVolumePath = NULL;//new char(MAX_PATH);
	DWORD dwVolPathSize = 0;
	string strVolumeGuid = string(szBuffer);

	string strTemp = "\\\\?\\";
	size_t len = strTemp.length();
	size_t lastPos = strVolumeGuid.find_last_of("\\");
	string strQueryDosDev = strVolumeGuid.substr(len, lastPos-len);

	DWORD CharCount = 0;
	CharCount = QueryDosDevice(strQueryDosDev.c_str(), DeviceName, BUFSIZE);

	if(CharCount != 0)
	{
		if(GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
		{
			if(m_setVsnapVols.empty())
			{
				m_mapOfGuidsWithLogicalNames[strVolumeGuid] = string(szVolumePath);
			}
			else
			{
				if(m_setVsnapVols.find(szVolumePath) == m_setVsnapVols.end())
				{
					m_mapOfGuidsWithLogicalNames[strVolumeGuid] = string(szVolumePath);
				}
			}
		}
		else
		{
			if(GetLastError() == ERROR_MORE_DATA)
			{
				pszVolumePath = new char[dwVolPathSize];
				if(!pszVolumePath)
				{
					DebugPrintf(SV_LOG_ERROR,"\n Failed to allocate required memory to get the list of Mount Point Paths.");
					return FALSE;
				}
				else
				{
					//Call GetVolumePathNamesForVolumeName function again
					if(GetVolumePathNamesForVolumeName(strVolumeGuid.c_str(), pszVolumePath, dwVolPathSize, &dwVolPathSize))
					{
						if(m_setVsnapVols.empty())
						{
							m_mapOfGuidsWithLogicalNames[strVolumeGuid] = string(szVolumePath);
						}
						else
						{
							if(m_setVsnapVols.find(szVolumePath) == m_setVsnapVols.end())
							{
								m_mapOfGuidsWithLogicalNames[strVolumeGuid] = string(szVolumePath);
							}
						}
					}
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : [%lu] for Volume GUID %s\n",GetLastError(), strVolumeGuid.c_str());
			}
		}

        SVGetVolumeInformation(szBuffer, NULL, 0, NULL, NULL, &dwSysFlags, FileSysNameBuf, ARRAYSIZE(FileSysNameBuf));
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"QueryDosDevice Failed with ErrorCode : [%lu] for Volume GUID %s\n",GetLastError(), strVolumeGuid.c_str());
	}

	bFlag = FindNextVolume(volumeHandle, szBuffer, iBufSize);
   
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return (bFlag); 
}


HRESULT MasterTarget::createMountPointForVolumeGuid(const string &strVolGuid,int vol_count)
{ 
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bFlag = FALSE;
	do
	{
		CHAR szSysPath[MAX_PATH];
		CHAR szRootVolumeName[MAX_PATH];
		CHAR VolumeGUID[MAX_PATH];

		//System Directory Path
		if(0 == GetSystemDirectory(szSysPath, MAX_PATH))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get System Directory Path. ErrorCode : [%lu].\n",GetLastError());
			bFlag = FALSE;
			break;
		}

        if (0 == SVGetVolumePathName(szSysPath, szRootVolumeName, ARRAYSIZE(szRootVolumeName)))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the Root volume Name of the System Directory. ErrorCode : [%lu].\n",GetLastError());
			bFlag = FALSE;
			break;
		}

		if(FALSE == GetVolumeNameForVolumeMountPoint(szRootVolumeName, VolumeGUID, MAX_PATH))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get GUID for the System volume. ErrorCode : [%lu].\n",GetLastError());
			bFlag = FALSE;
			break;
		}
	    static int iCreateDirectoryFirstTime = 0;

		if(0 == iCreateDirectoryFirstTime)
		{
			string strInmageTempDirectory;
			if(m_bPhySnapShot)
				strInmageTempDirectory = string(ESX_DIRECTORY) + string("\\") + string(SNAPSHOT_DIRECTORY);
			else
				strInmageTempDirectory = string(ESX_DIRECTORY);

			strInmageTempDirectory = string(szRootVolumeName) + strInmageTempDirectory;
			if(FALSE == CreateDirectory(strInmageTempDirectory.c_str(),NULL))
			{
				if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
				{
                    DebugPrintf(SV_LOG_DEBUG,"'ESX' Directory already exists. Considering it not as an Error.\n");
				}
				else
				{
                    DebugPrintf(SV_LOG_ERROR,"Failed to create the Root Directory - %s. ErrorCode : [%lu].\n",strInmageTempDirectory.c_str(),GetLastError());
					bFlag = FALSE;
					break;
				}
			}
			strDirectoryPathName = strInmageTempDirectory;
			iCreateDirectoryFirstTime++;
		}

		std::string strTemp1;
		std::stringstream out;
		out<<vol_count;
		strTemp1 = out.str();
		string strMountPtPath = strDirectoryPathName + string("\\") + string("_") + string(strTemp1);
        DebugPrintf(SV_LOG_DEBUG,"Mount Path = %s.\n",strMountPtPath.c_str());
		//Creating a directory where all other GUIDS will be mounted.
		if(FALSE == CreateDirectory(strMountPtPath.c_str(),NULL))
		{
			if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
                DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");
				
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create Mount Directory - %s. ErrorCode : [%lu].\n",strMountPtPath.c_str(),GetLastError());
				bFlag = FALSE;
				break;
			}
		}
		//For SetMountPoints,trailing backslashes is necessary.
		strMountPtPath += '\\';
		//This will cause the GUID to be mounted on "Inmage_tempDirectory"
		bFlag = SetVolumeMountPoint((strMountPtPath.c_str()), ((strVolGuid).c_str()));
		if(bFlag == FALSE)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to mount %s at %s with Error Code : [%lu].\n",strVolGuid.c_str(),strMountPtPath.c_str(),GetLastError());
			//We cant Ignore this failed case.Becoz this is the point where all volumes will be mounted initially.
			break;
		}

		else
		{
			Sleep(1000);//Just to Ensure Format succeeds properly
            DebugPrintf(SV_LOG_DEBUG,"Successfully mounted the volume %s at %s.\n",strVolGuid.c_str(),strMountPtPath.c_str());

            if(false == ClearMountPoints(strMountPtPath))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to clear above Mount Points present inside Volume - %s.\n",strMountPtPath.c_str());
                DebugPrintf(SV_LOG_ERROR,"Please delete them manually and rerun this job.\n");
                bFlag = FALSE;	 
                break;
            }
		}
	}while(0);

	if( bFlag == FALSE)
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
	else
	{
		m_vectorOfAlreadyMountedVolGuids.push_back(strVolGuid);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;	
}


//--------------------------------------------------------------------------------------------------------
//           ConfigureTgtDisksSameAsSrcDisk() -- Earlier function name was read_mbr_information()

//In 6.2.1 or earlier versions,
// 1.Here we are opening the "disk.bin" of each individual Source Guest OS.
// 2.Then we are iterating in the 2nd field of vmdiskmap1 
//		a).Dumping MBR on each disk.We pick disk from, disk no mentioned in the maping.txt file 
//		b).Then we are reading the Disk.bin file in ReadDiskInfo()
//		c).In processDisk(),we are applying the EBR data on the disks.

//In 7.0 Onwards,
//Updating the source MBR read from DLM mbr data file previously using DLM API's
//Updating the MBR's on respective target disks by iterating each disk info.
//This will make the target disk to look same as source disk.
//--------------------------------------------------------------------------------------------------------

HRESULT MasterTarget::ConfigureTgtDisksSameAsSrcDisk()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
	map<string,map<int,int>>::iterator iterAddDynDisk;
	map<int, int> mapDiskNumbers;
	mapDiskNumbers.clear();

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	int iNumberOfBytesToBeRead = 0;
	DiskInformation disk_info;
	
	//This outer Loop is for each VM
    for(; Iter_beg != Iter_end;Iter_beg++)
	{
		string  hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_DEBUG,"\n\n");
		DebugPrintf(SV_LOG_DEBUG,"==============================================================================================\n");
		DebugPrintf(SV_LOG_INFO,"Virtual Machine Name = %s.\n",hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"==============================================================================================\n");

		string strSrcHostName = "";
		if (m_mapHostIdToHostName.find(hostname) != m_mapHostIdToHostName.end())
			strSrcHostName = m_mapHostIdToHostName.find(hostname)->second;
		m_PlaceHolder.Properties[SOURCE_HOSTNAME] = strSrcHostName;

		if(m_vConVersion >= 4.0 && (!m_mapOfHostToDiskInfo.empty()))
		{
			if(bIsAdditionOfDisk)
			{
				mapDiskNumbers.clear();
				map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.find(hostname);
				if(iterDyn != m_mapHostIdDynAddDisk.end())
				{
					if(false == iterDyn->second)
					{
						DebugPrintf(SV_LOG_DEBUG,"No dynamic disk is added for this host = %s.\n",hostname.c_str());
					}
					else
					{
						iterAddDynDisk = m_mapOfHostToDiskNumMap.find(hostname);
						if(iterAddDynDisk != m_mapOfHostToDiskNumMap.end())
						{
							mapDiskNumbers = iterAddDynDisk->second;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in m_mapOfHostToDiskNumMap Map\n", hostname.c_str());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return S_FALSE;
						}
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in m_mapHostIdDynAddDisk Map\n", hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
			}

			if(false == UpdateMbrOrGptInTgtDisk(hostname, Iter_beg->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update MBR or GPT into Target disk for host : %s\n",hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return S_FALSE;
			}
			if(!mapDiskNumbers.empty())
			{
				if(false == UpdateMbrOrGptInTgtDisk(hostname, mapDiskNumbers))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to update MBR or GPT into Target disk for host : %s\n",hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
			}
			rescanIoBuses();  //Rescans the disks once bcz some of the disk onlined but diskmanagement is not updated with latest data
			//Sleep(20000);
			if(false == UpdateSignatureDiskIdentifierOfDisk( hostname , Iter_beg->second) )
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update the disk signature for the dynamic disks of host: %s\n", hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return S_FALSE;
			}

			if(!mapDiskNumbers.empty())
			{
				if(false == UpdateSignatureDiskIdentifierOfDisk( hostname , mapDiskNumbers) )
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to update the disk signature for the dynamic disks of host: %s\n", hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
			}

			if(m_bVolResize)
			{
				if(false == OnlineVolumeResizeTargetDisk(hostname, Iter_beg->second))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to online some of the disks for host: %s\n", hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
			}
			else
			{
				if(!mapDiskNumbers.empty())
				{
					if(false == OnlineVolumeResizeTargetDisk(hostname, mapDiskNumbers))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to online some of the disks for host: %s\n", hostname.c_str());
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return S_FALSE;
					}
				}
				//Introduced this one because the Dr-Drill for basic disk is failing. 
				//After dumping the MBR's the volumes are going to inaccessible state
				OfflineOnlineBasicDisk(hostname, Iter_beg->second);
			}

			if(!mapDiskNumbers.empty())
			{
				if(false == ImportDynamicDisk(hostname, mapDiskNumbers))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to import the dynamic disk of host: %s\n", hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
			}

			if(false == ImportDynamicDisk(hostname, Iter_beg->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to import the dynamic disk of host: %s\n", hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return S_FALSE;
			}
			//Checking whether any unknown disks present after dumping mbr/ldm. then we need to restart vds service.
			//Adding code for restartin vds service. This is only required for win2k3.But safer side to verify for all windows platforms.
			bool bAvail = true;
			CVdsHelper obj;
			if(obj.InitilizeVDS())
				bAvail = obj.CheckUnKnowndisks();
			obj.unInitialize();
			
			if(bAvail)
			{
				if(false == RestartVdsService())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to restart Vds Service, proceeding further\n");
				}
				rescanIoBuses(); 

				//Verifying whether all the disks of given VM are online or not .
				if((false == CheckOnlineStatusDisks(hostname, Iter_beg->second)) || ((!mapDiskNumbers.empty()) && (false == CheckOnlineStatusDisks(hostname, mapDiskNumbers))))
				{
					DebugPrintf(SV_LOG_ERROR,"Disks are not onlin of host: %s  Re-Starting the VDS service again\n", hostname.c_str());
					if(false == RestartVdsService())
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to restart Vds Service, proceeding further\n");
					}
				}
			}
			rescanIoBuses(); 

			if(false == CreateMapOfSrcDisktoVolInfo(hostname, Iter_beg->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update MBR or GPT into Target disk for host : %s\n",hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return S_FALSE;
			}
			if(!mapDiskNumbers.empty())
			{
				if(false == CreateMapOfSrcDisktoVolInfo(hostname, mapDiskNumbers))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to update MBR or GPT into Target disk for host : %s\n",hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
			}
		}
		else
		{
			string Disk_binFileName;
			hostname = m_mapHostIdToHostName.find(Iter_beg->first)->second;
			Disk_binFileName = m_strDataFolderPath + hostname + string("_")+ string("disk.bin");
			
			FILE *fp = fopen(Disk_binFileName.c_str(),"rb");
			if(!fp)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",Disk_binFileName.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return S_FALSE;
			}		
			
			string filename = hostname + string("_Disk_");
			map<int,int>::iterator iterDiskNmbrs = Iter_beg->second.begin(); //first-src ; second-tgt
			//this Loop is for Each Disk Present in the VM
			for(; iterDiskNmbrs != Iter_beg->second.end(); iterDiskNmbrs++)
			{
				stringstream ss;
				string tempfilename;
				string strdisknumber;
				ss << iterDiskNmbrs->first;
				strdisknumber = ss.str();

				//Restoring the file pointer to beginning.
				if(0 != fseek(fp, 0, SEEK_SET))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to restore the file pointer to beginning. Cannot continue!\n");
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
				//Keep the file pointer pointed to beginning for changed logic of ReadDiskInfo
				if(S_FALSE == ReadDiskInfo(iterDiskNmbrs->first, fp, &disk_info, m_partitionVolumePairVector))
				{
					//If an unintialized disk is Present in the Source VM,then We need to ignore this disk.
					//Either we need to write continue or skip the return value of this Disk.
					//If we write continue,Then,we'll skip the Later Call to Process_Disk
					DebugPrintf(SV_LOG_ERROR,"ReadDiskInfo() failed. Cannot continue!\n");
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}

				tempfilename = m_strDataFolderPath + filename + strdisknumber +  MBR_FILE_NAME;
				hFind = FindFirstFile(tempfilename.c_str(), &FindFileData);
				if (hFind == INVALID_HANDLE_VALUE) 
 				{
					tempfilename.clear();
					tempfilename = m_strDataFolderPath + filename + strdisknumber +  GPT_FILE_NAME;
					hFind = FindFirstFile(tempfilename.c_str(), &FindFileData);
					if (hFind == INVALID_HANDLE_VALUE) 
 					{
						DebugPrintf(SV_LOG_ERROR,"Invalid File Handle with Error : [%lu].\n",GetLastError());
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
 						return S_FALSE;
					}
					else 
					{
						iNumberOfBytesToBeRead = 17408;
						FindClose(hFind);
					}

				}
				else 
				{
					iNumberOfBytesToBeRead = 512;
					FindClose(hFind);
				}
				DebugPrintf(SV_LOG_INFO,"iterDiskNmbrs->second = %d.\n",iterDiskNmbrs->second);
				DebugPrintf(SV_LOG_INFO,"MBR/GPT tempfilename  = %s.\n",tempfilename.c_str());
				DebugPrintf(SV_LOG_INFO,"no of bytes read      = %d.\n",iNumberOfBytesToBeRead);

				LARGE_INTEGER iLargeInt;
				iLargeInt.LowPart = 0;
				iLargeInt.HighPart = 0;
				if(false == CompareAndWriteMbr(iterDiskNmbrs->second,iLargeInt,tempfilename,iNumberOfBytesToBeRead))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to write %s to disk-%d.\n",tempfilename.c_str(),iterDiskNmbrs->second);
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}

				//Process the partition_volume_pair_vector and Disk structure for all partitions on this disk_info
				if(S_FALSE == processDisk(hostname,iterDiskNmbrs->second,&disk_info,m_partitionVolumePairVector))
				{
					DebugPrintf(SV_LOG_ERROR,"processDisk() failed. Cannot continue!\n");
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return S_FALSE;
				}
				DebugPrintf(SV_LOG_INFO,"--------------------------------------------------------\n");
			}
			fclose(fp);			
		}
		//Insert the m_Disk_Vol_ReadFromBinFile to m_mapOfVmToDiskVolReadFromBinFile
		m_mapOfVmToDiskVolReadFromBinFile.insert(make_pair(Iter_beg->first,m_Disk_Vol_ReadFromBinFile));
		m_Disk_Vol_ReadFromBinFile.clear();
    }
	m_PlaceHolder.clear();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;
}



bool MasterTarget::UpdateDiskWithMbrData(const char* chTgtDisk, SV_LONGLONG iBytesToSkip, const SV_UCHAR * chDiskData, unsigned int noOfBytesToDeal = 512)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_INFO,"Drive name is %s.\n", chTgtDisk);
	DebugPrintf(SV_LOG_INFO,"No. of bytes to skip : %lld, And No. bytes to write : %u\n", iBytesToSkip, noOfBytesToDeal);

	ACE_HANDLE hdl;    
    size_t BytesWrite = 0;

    hdl = ACE_OS::open(chTgtDisk,O_RDWR,ACE_DEFAULT_OPEN_PERMS);
    if(ACE_INVALID_HANDLE == hdl)
    {
		DebugPrintf(SV_LOG_ERROR,"[Error] ACE_OS::open failed: disk %s, err = %d\n ", chTgtDisk, ACE_OS::last_error());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    if (ACE_OS::llseek(hdl, (ACE_LOFF_T)iBytesToSkip, SEEK_SET) < 0)
    {   
        DebugPrintf(SV_LOG_ERROR,"[Error] ACE_OS::lseek failed: disk %s, offset 0, err = %d\n", chTgtDisk, ACE_OS::last_error());
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    BytesWrite = ACE_OS::write(hdl, chDiskData, noOfBytesToDeal);
	if(BytesWrite != noOfBytesToDeal)
    {
        DebugPrintf(SV_LOG_ERROR,"[Error] ACE_OS::write failed: disk %s, err = %d\n", chTgtDisk, ACE_OS::last_error());
		DebugPrintf(SV_LOG_ERROR,"Number of bytes has to be written : %ud, But written bytes are : %u\n", noOfBytesToDeal, BytesWrite);
        ACE_OS::close(hdl);
	    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n",__FUNCTION__);
        return false;
	}
    ACE_OS::close(hdl);

	DebugPrintf(SV_LOG_INFO,"Successfully Updated the MBR/GPT/LDM in disk %s.\n No. of bytes Written : %u\n", chTgtDisk, BytesWrite);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}



int MasterTarget::WriteMbrToDisk(unsigned int iDiskNumber,LARGE_INTEGER iBytesToSkip,const string &strInPutMbrFileName,unsigned int noOfBytesToDeal = 512)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HANDLE diskHandle;
	unsigned char *buf;
	unsigned long bytes,bytesWritten;
	char driveName [256];

    buf = (unsigned char *)malloc(noOfBytesToDeal * (sizeof(unsigned char)));
    if (buf == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory for buf.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return -1;
	}

	FILE *fp;
	fp = fopen(strInPutMbrFileName.c_str(),"rb");
	if(!fp)
	{
		 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strInPutMbrFileName.c_str());
         free(buf);
		 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		 return -1;
	}
    int inumberOfBytesRead = fread(buf, 1, noOfBytesToDeal, fp);

    if(noOfBytesToDeal != inumberOfBytesRead)
    {
        DebugPrintf(SV_LOG_ERROR,"Reading failed in WriteMbrToDisk().\n");
        fclose(fp);
        free(buf);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return -1;
    }
	fclose(fp);

	inm_sprintf_s(driveName, ARRAYSIZE(driveName), "\\\\.\\PHYSICALDRIVE%d", iDiskNumber);
    DebugPrintf(SV_LOG_DEBUG,"Drive name is %s.\n",driveName);
	// PR#10815: Long Path support
	diskHandle = SVCreateFile(driveName,GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	
	if(diskHandle == INVALID_HANDLE_VALUE)
	{
		DebugPrintf(SV_LOG_ERROR,"Error in opening device. Error Code : [%lu].\n",GetLastError());
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

	DWORD dwPtr = SetFilePointerEx(diskHandle, iBytesToSkip, NULL, FILE_BEGIN ); 
	 
	if(0 == dwPtr)
	{
		DebugPrintf(SV_LOG_ERROR,"SetFilePointerEx() failed in WriteMbrToDisk() with Error code : [%lu].\n",GetLastError());
		CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

    bytes = noOfBytesToDeal;
	OVERLAPPED osWrite = {0};
	BOOL fRes;

	// Create this write operation's OVERLAPPED structure's hEvent.
	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWrite.hEvent == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"CreateEvent() failed in WriteMbrToDisk() with Error code : [%lu].\n",GetLastError());
		// error creating overlapped event handle
		CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}

	// Issue write.
	if (!WriteFile(diskHandle,buf,noOfBytesToDeal,&bytesWritten, NULL))
	{
		DebugPrintf(SV_LOG_ERROR,"WriteFile() failed in WriteMbrToDisk() with Error code : [%lu].\n",GetLastError());
		fRes = FALSE;
        CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}	
	else
	{
		// WriteFile completed immediately.
		fRes = TRUE;
	}
	
	DWORD lpBytesReturned;
	//Disk Update to reflect MBR 
	bool flag = DeviceIoControl((HANDLE) diskHandle, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, (LPDWORD)&lpBytesReturned, (LPOVERLAPPED)NULL);
	if(!flag)
	{
		DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_UPDATE_PROPERTIES Failed with Error Code : [%lu].\n",GetLastError());
		CloseHandle(diskHandle);
        free(buf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return -1;
	}
	CloseHandle(diskHandle);
	free(buf);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return 0;
}

//Read the disk info from the disk.bin file obtained from source side via FX.
//Input  - file pointer to vmname_disk.bin , Disk number of source VM which we need to read the info
//Output - pointer to DiskInformation of disk and vector of pairs(partition to volumes on this partition)
//Also we update m_Disk_Vol_ReadFromBinFile in this for each disk.Check ConfigureTgtDisksSameAsSrcDisk().
//Logic  - Since this is byte by byte reading and we have written in a specific format on source side,
//         we read disk to disk until we get the required disk
//           and fetch the info of partitions on this disk and volumes on each of these partitions.
//         To reach the next disk from current disk, we have to traverse all data of partitions and 
//           volumes on current disk. Then automatically file pointer points to starting of next disk
//         So when the required disk is found, then we write the data to above output variables.
HRESULT MasterTarget::ReadDiskInfo(int iDiskNumber, FILE* f, DiskInformation* pDiskInfo, vector<pair<PartitionInfo,volInfo*>>& m_partitionVolumePairVector)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bDiskFound = false;
	PartitionInfo* ArrPartInfo;
	volInfo* ArrVolInfo;

	m_partitionVolumePairVector.clear();
	
    memset(pDiskInfo,0,sizeof(*pDiskInfo));

    while(false == bDiskFound)
    {
	    if(fread(pDiskInfo,sizeof(*pDiskInfo),1,f))
	    {
            if(iDiskNumber == pDiskInfo->uDiskNumber) 
                bDiskFound = true; //when true,display and store the info; else just proceed to next

            if(true == bDiskFound)
            {
                DebugPrintf(SV_LOG_INFO,"========================================================\n");                
                DebugPrintf(SV_LOG_INFO,"DISK INFORMATION for source disk = %d\n",iDiskNumber);
                DebugPrintf(SV_LOG_INFO,"---------------------------------------\n");
                DebugPrintf(SV_LOG_INFO," Disk Id                      = %u\n",pDiskInfo->uDiskNumber);
		        DebugPrintf(SV_LOG_INFO," Disk Type                    = %d\n",pDiskInfo->dt);
		        DebugPrintf(SV_LOG_INFO," Disk No of Actual Partitions = %lu\n",pDiskInfo->dwActualPartitonCount);
		        DebugPrintf(SV_LOG_INFO," Disk No of Partitions        = %lu\n",pDiskInfo->dwPartitionCount);
		        DebugPrintf(SV_LOG_INFO," Disk Partition Style         = %d\n",pDiskInfo->pst);
		        DebugPrintf(SV_LOG_INFO," Disk Signature               = %lu\n",pDiskInfo->ulDiskSignature);
                DebugPrintf(SV_LOG_INFO," Disk Size                    = %I64d\n",pDiskInfo->DiskSize.QuadPart);
            }

            ArrPartInfo = new PartitionInfo [pDiskInfo->dwActualPartitonCount];
		    if(NULL == ArrPartInfo)
		    {
			    DebugPrintf(SV_LOG_ERROR,"Cannot create Partition Information structures. Cannot continue!\n");
			    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			    return S_FALSE;
		    }
            //Fetch partitions on the disk
		    for(unsigned int i = 0; i < pDiskInfo->dwActualPartitonCount; i++)
		    {			    
			    fread(&ArrPartInfo[i],sizeof(ArrPartInfo[i]),1,f);
                if(true == bDiskFound)
                {
                    DebugPrintf(SV_LOG_INFO,"\n  Start of Partition : %d\n  ------------------------\n",i+1);
		            DebugPrintf(SV_LOG_INFO,"  Partition Disk Number       = %u\n",ArrPartInfo[i].uDiskNumber);
		            DebugPrintf(SV_LOG_INFO,"  Partition Partition Type    = %d\n",ArrPartInfo[i].uPartitionType);
		            DebugPrintf(SV_LOG_INFO,"  Partition Boot Indicator    = %d\n",ArrPartInfo[i].iBootIndicator);
		            DebugPrintf(SV_LOG_INFO,"  Partition No of Volumes     = %u\n",ArrPartInfo[i].uNoOfVolumesInParttion);
		            DebugPrintf(SV_LOG_INFO,"  Partition Partition Length  = %I64d\n",ArrPartInfo[i].PartitionLength.QuadPart);
		            DebugPrintf(SV_LOG_INFO,"  Partition Start Offset      = %I64d\n",ArrPartInfo[i].startOffset.QuadPart);
		            DebugPrintf(SV_LOG_INFO,"  Partition Ending Offset     = %I64d\n",ArrPartInfo[i].EndingOffset.QuadPart);
                }

                ArrVolInfo = new volInfo [ArrPartInfo[i].uNoOfVolumesInParttion];
			    if(NULL == ArrVolInfo)
			    {
				    DebugPrintf(SV_LOG_ERROR,"Cannot create Volume Information Structures. Cannot continue!\n");
				    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                    delete[] ArrPartInfo;
				    return S_FALSE;
			    }
                //Fetch volumes on each partition.
			    for(unsigned int j = 0; j < ArrPartInfo[i].uNoOfVolumesInParttion; j++)
			    {
				    fread(&ArrVolInfo[j],sizeof(ArrVolInfo[j]),1,f);
                    if(true == bDiskFound)
                    {
                        DebugPrintf(SV_LOG_INFO,"\n   Start of Volume : %d\n   ---------------------\n",j+1);
			            DebugPrintf(SV_LOG_INFO,"   Volume Volume Name         = %s\n",ArrVolInfo[j].strVolumeName);
			            DebugPrintf(SV_LOG_INFO,"   Volume Partition Length    = %I64d\n",ArrVolInfo[j].partitionLength.QuadPart);
			            DebugPrintf(SV_LOG_INFO,"   Volume Starting Offset     = %I64d\n",ArrVolInfo[j].startingOffset.QuadPart);
			            DebugPrintf(SV_LOG_INFO,"   Volume Ending Offset       = %I64d\n",ArrVolInfo[j].endingOffset.QuadPart);
			            m_Disk_Vol_ReadFromBinFile.insert(make_pair(pDiskInfo->uDiskNumber,ArrVolInfo[j]));
                    }
			    }

                if(true == bDiskFound)
                {
                    pair<PartitionInfo,volInfo*> partitionVolumePair;
                    partitionVolumePair.first = ArrPartInfo[i];
	                partitionVolumePair.second = new volInfo[ArrPartInfo[i].uNoOfVolumesInParttion];
	                for(unsigned int j = 0; j < ArrPartInfo[i].uNoOfVolumesInParttion; j++)
	                    partitionVolumePair.second[j] = ArrVolInfo[j];
	                m_partitionVolumePairVector.push_back(partitionVolumePair);
                }
                delete[] ArrVolInfo;
		    }
            delete[] ArrPartInfo;
	    }
	    else
	    {
		    DebugPrintf(SV_LOG_ERROR,"\nSource Disk Information File does not contain information about disk number - %d\n", iDiskNumber);
		    DebugPrintf(SV_LOG_ERROR,"Verify if the above disk on source machine is not dynamic disk and contains atleast one partition in it.\n\n");
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return S_FALSE;
	    }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;
}

HRESULT MasterTarget::processDisk(const string &strMachineName,unsigned int TgtDiskNumber,DiskInformation* pDiskInfo,vector<pair<PartitionInfo,volInfo*> >& m_partitionVolumePairVector)
{	
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	bool flag = false;
	vector<pair<PartitionInfo,volInfo*> > ::iterator iter_beg = m_partitionVolumePairVector.begin();
	vector<pair<PartitionInfo,volInfo*> > ::iterator iter_end = m_partitionVolumePairVector.end();
	pair<PartitionInfo,volInfo*> partVolumePair;
	char disk_name[256];
	HANDLE volumeHandle;
	DWORD dwSize = 0;
	DWORD lpBytesReturned = 0;

	inm_sprintf_s(disk_name, ARRAYSIZE(disk_name), "\\\\.\\PHYSICALDRIVE%d", TgtDiskNumber);
	volumeHandle = SVCreateFile(disk_name,GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	if(volumeHandle == INVALID_HANDLE_VALUE)
	{
		DebugPrintf(SV_LOG_ERROR,"Error in opening volumeHandle for drive - %u. Error Code : [%lu].\n",TgtDiskNumber,GetLastError());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
  

	while(iter_beg != iter_end)
	{
		partVolumePair = *iter_beg;
		DWORD lpBytesReturned;
		//Disk Update to reflect MBR 
		bool flag = DeviceIoControl((HANDLE) volumeHandle, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, (LPDWORD)&lpBytesReturned, (LPOVERLAPPED)NULL);
		if(!flag)
		{
			DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_UPDATE_PROPERTIES Failed with Error Code : [%lu].\n",GetLastError());
			CloseHandle(volumeHandle);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return S_FALSE;
		}

		//Means Extended Partition found...Now We need to apply E Record Data...
	    if( (5 == partVolumePair.first.uPartitionType) ||
            (15 == partVolumePair.first.uPartitionType) )
		{
            ACE_LOFF_T bytes_to_skip = (ACE_LOFF_T)partVolumePair.first.startOffset.QuadPart;
            int vol_count = 1;
            string strLogVolCount;
            string strTgtDiskNumber;
            string strSrcDiskNumber;
            string strEBRFileName;
            stringstream outstringStream;

            outstringStream<<TgtDiskNumber;
			strTgtDiskNumber = outstringStream.str();
			outstringStream.str(std::string());

            outstringStream<<pDiskInfo->uDiskNumber; 
			strSrcDiskNumber = outstringStream.str();
			outstringStream.str(std::string());

            while(1)
            {               
                outstringStream<<vol_count;
			    strLogVolCount = outstringStream.str();
			    outstringStream.str(std::string());

			    strEBRFileName = m_strDataFolderPath + 
                                 strMachineName + 
                                 string("_Disk_") + 
                                 strSrcDiskNumber + 
                                 string("_LogicalVolume_") + 
                                 strLogVolCount + 
                                 EBR_FILE_NAME;

                DebugPrintf(SV_LOG_INFO,"Bytes to skip = %I64d\n",bytes_to_skip);
                unsigned char ebr_sector[MBR_BOOT_SECTOR_LENGTH];
                if(1 == ReadFromFileInBinary(ebr_sector, MBR_BOOT_SECTOR_LENGTH, strEBRFileName.c_str()))
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed to read the data from file - %s\n",strEBRFileName.c_str());
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		            return S_FALSE;
                }

                bool bWriteEBR = true;
                if(bIsFailbackEsx)
                {
                    unsigned char ebr_sector_temp[MBR_BOOT_SECTOR_LENGTH];
                    if(1 == ReadDiskData(disk_name, bytes_to_skip, MBR_BOOT_SECTOR_LENGTH, ebr_sector_temp))
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to read the EBR data from disk - %s\n",disk_name);
                        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return S_FALSE;
                    }
                    if(true == bMyStrCmp(ebr_sector, ebr_sector_temp, MBR_BOOT_SECTOR_LENGTH))
                    {
                        DebugPrintf(SV_LOG_INFO,"EBR of source is matching with target. EBR File - %s\n", strEBRFileName.c_str());
                        bWriteEBR = false;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_INFO,"EBR of source is not matching with target. EBR File - %s\n", strEBRFileName.c_str());
                        bWriteEBR = true;
                    }
                }

                if(bWriteEBR)
                {
                    if(1 == WriteDiskData(disk_name, bytes_to_skip, MBR_BOOT_SECTOR_LENGTH, ebr_sector))
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to write the data to disk - %s\n", disk_name);
                        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		                return S_FALSE;
                    }
                    else
                        DebugPrintf(SV_LOG_INFO,"Copied the %s onto disk-%s\n", strEBRFileName.c_str(), disk_name);
                }

                unsigned char ebr_second_entry[EBR_SECOND_ENTRY_SIZE];
                GetBytesFromMBR(ebr_sector, EBR_SECOND_ENTRY_STARTING_INDEX, EBR_SECOND_ENTRY_SIZE, ebr_second_entry);

                ACE_LOFF_T offset = 0;
                ConvertHexArrayToDec(ebr_second_entry, EBR_SECOND_ENTRY_SIZE, offset);
                if(offset == 0)
                    break;

                bytes_to_skip = (ACE_LOFF_T)partVolumePair.first.startOffset.QuadPart + offset * 512; //assuming 512 bytes per sector.
                DebugPrintf(SV_LOG_INFO,"Offset = %I64d\n",offset);
                vol_count++;
            }
		}
		iter_beg++;

		//Once Applied the EBR,refresh diskmgmt
		flag = DeviceIoControl((HANDLE) volumeHandle, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, (LPDWORD)&lpBytesReturned, (LPOVERLAPPED)NULL);
		if(!flag)
		{
			DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_UPDATE_PROPERTIES Failed with Error Code : [%lu].\n",GetLastError());
			CloseHandle(volumeHandle);
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return S_FALSE;
		}
	}
	CloseHandle(volumeHandle);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;
}
/*
* See If the disknumber belongs to Master Target Private disks
* 
*/
bool MasterTarget::IsItMtPrivateDisk(const unsigned int &uDiskNumber)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = false;
	map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
	vector<int> diskNumbersOfProtectedVm;
	vector<int> ::iterator iterVec;
	while(Iter_beg != Iter_end)
	{
        if (false == GetVectorFromMap(Iter_beg->second,diskNumbersOfProtectedVm,"second"))
        {
            DebugPrintf(SV_LOG_ERROR,"GetVectorFromMap failed for %s\n",Iter_beg->first.c_str());
		    bResult = false;
            break;
        }
		iterVec = find(diskNumbersOfProtectedVm.begin(),diskNumbersOfProtectedVm.end(),uDiskNumber);
		if(iterVec == diskNumbersOfProtectedVm.end())
		{
			bResult = true;
		}
		else
		{
			bResult = false;
			break;
		}
		Iter_beg++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}
/*
* Initial run of startTargetVolumeEnumertion will give the list of volumes present in the MT.We need to filter out the
* private volumes of master target.Two sceanrios exists:---->
* i) All the volumes (present) are Master Target's Private volumes. We'll verify by dumping the disk extents.If volumes lies on MT's
* private disks,push the list of volumes into respective map(master target volume map)
* ii)If after protecting say 3 vms,we are going to protect one more VM,in this case,we'll see if Volume is already protected,If yes
* then treat this volume as master target volume even though this volume does not lying on MAster target's private disks.
*/
bool MasterTarget::GetMasterTargetVolumesExtent()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	HANDLE	hVolume;
	ULONG	extent;
	int     uDiskNumber = -1;
	ULONG	bytesWritten;
	UCHAR	DiskExtentsBuffer[0x4000];
	PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
	DWORD returnBufferSize = sizeof(DiskExtentsBuffer);
	map<string,string> tempMap;
	BOOLEAN bSuccess;

	map <string,string> tempMapDel ;
	tempMap = m_mapOfAlreadyPresentMasterVolumes;
	map<string,string>::iterator iter_beg = tempMap.begin();
	map<string,string>::iterator iter_end = tempMap.end();
    map <string,string> realPrivateVolumesOfMasterTarget;
	while(iter_beg != iter_end)
	{
	   string strTemp = (iter_beg->first);
	   string::size_type pos = strTemp.find_last_of("\\");
	   if(pos !=string::npos)
	   {
		   string strSecondPart = (iter_beg)->second;
		   strTemp.erase(pos);
		   tempMapDel[strTemp]= strSecondPart;
		}
		else
		{
			tempMapDel[iter_beg->first] = iter_beg->second;
		}
	    iter_beg++;	
	}
	tempMap.clear();
	tempMap = tempMapDel;
	iter_beg = tempMap.begin();
	map<string,string>::iterator iterMasterTargetGuids = m_mapOfAlreadyPresentMasterVolumes.begin();
	while(iter_beg != iter_end)
	{
        DebugPrintf(SV_LOG_DEBUG,"Volume GUID :- iter_beg->first = %s.\n",(iter_beg->first).c_str());
		/*
		*Suppose we protected 3 vms.Once replication pair done,we are going to protect one more VM.Not,in this scenario,We'll treat
		* these protected volumes also as part of Mster target's private volumes.
		* So,Before dumping the volumes extents on disks we'll see,if its already protected,If yes,treat this s part of Mt's private vols.
		*/
		if(true == IsGuidAlreadyMounted(iter_beg->first))
		{
			DebugPrintf(SV_LOG_DEBUG,"Volume %s is already mounted.Considering it part of MT's Private volumes.\n",(iter_beg->first.c_str()));
			realPrivateVolumesOfMasterTarget.insert(make_pair(iterMasterTargetGuids->first,iterMasterTargetGuids->second));
			iter_beg++;
			continue;
		}
		//Bfore opening the Volume handle see,if volume is already protected,If yes ignore the iteration
		
		hVolume = SVCreateFile((iter_beg->first).c_str() ,
					GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
					NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |FILE_FLAG_OPEN_REPARSE_POINT  , NULL );

		if( hVolume == INVALID_HANDLE_VALUE ) 
		{
			/* There might be some volumes which are protected during last run.These volumes are opened in exclusive
			* mode by dataprotection.If we get Access_Denied msg,trea these volumes even part of the MasterTarget.
			*/
			if(GetLastError() == ERROR_ACCESS_DENIED)// To Support Rerun scenario
			{
                DebugPrintf(SV_LOG_DEBUG,"Volume %s is under protection.\n",iter_beg->first.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Considering it as Master Target Private volume.\n");
				realPrivateVolumesOfMasterTarget.insert(make_pair(iterMasterTargetGuids->first,iterMasterTargetGuids->second));
				iter_beg++;
				continue;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Error getting extents for drive : %s. Error Code [%lu].\n",((iter_beg->first).c_str()),GetLastError());
				bResult = false;
				break;
			}
		}

		// If we got upto here,its guranteed that current Volume is not the master target private volume.
		bSuccess = DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, DiskExtents, returnBufferSize, &bytesWritten, NULL);
		if(bSuccess)
		{
			LARGE_INTEGER startOffset;
			LARGE_INTEGER PartitionLength;
			LARGE_INTEGER EndingOffset;
			for( extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
				startOffset = DiskExtents->Extents[extent].StartingOffset;
				PartitionLength = DiskExtents->Extents[extent].ExtentLength;
				EndingOffset.QuadPart = startOffset.QuadPart + PartitionLength.QuadPart;
				uDiskNumber			= DiskExtents->Extents[extent].DiskNumber;
				if(true == IsItMtPrivateDisk(uDiskNumber))
				{
					realPrivateVolumesOfMasterTarget.insert(make_pair(iterMasterTargetGuids->first,iterMasterTargetGuids->second));
				}
			}			
		}
		else
		{
			if(ERROR_MORE_DATA == bSuccess)
			{
                DWORD nde;
                INM_SAFE_ARITHMETIC(nde = InmSafeInt<DWORD>::Type(DiskExtents->NumberOfDiskExtents) - 1, INMAGE_EX(DiskExtents->NumberOfDiskExtents))
				INM_SAFE_ARITHMETIC(returnBufferSize = sizeof(VOLUME_DISK_EXTENTS) + (nde * InmSafeInt<size_t>::Type(sizeof(DISK_EXTENT))), INMAGE_EX(sizeof(VOLUME_DISK_EXTENTS))(nde)(sizeof(DISK_EXTENT)))
				delete DiskExtents;
				DiskExtents = (PVOLUME_DISK_EXTENTS) new UCHAR[returnBufferSize];
				bSuccess = DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, DiskExtents, returnBufferSize, &bytesWritten, NULL);
				if( bSuccess ) 
				{
					LARGE_INTEGER startOffset;
					LARGE_INTEGER PartitionLength;
					LARGE_INTEGER EndingOffset;
					for( extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
					{
						startOffset = DiskExtents->Extents[extent].StartingOffset;
						PartitionLength = DiskExtents->Extents[extent].ExtentLength;
						EndingOffset.QuadPart = startOffset.QuadPart + PartitionLength.QuadPart;
						uDiskNumber			= DiskExtents->Extents[extent].DiskNumber;
						if(true == IsItMtPrivateDisk(uDiskNumber))
						{
							realPrivateVolumesOfMasterTarget.insert(make_pair(iterMasterTargetGuids->first,iterMasterTargetGuids->second));
						}
					}
					
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed for Volume: %s with Error code : [%lu].\n",(iter_beg->first).c_str(), GetLastError());
					char chVolumePath[MAX_PATH+1] = {0};
					DWORD dwVolPathSize = 0;
					string strVolGuid = iter_beg->first + "\\";
					if(GetVolumePathNamesForVolumeName (strVolGuid.c_str(),chVolumePath,MAX_PATH, &dwVolPathSize))
					{
						if(0 != strcmpi(chVolumePath, ""))
						{
							DebugPrintf(SV_LOG_INFO,"GetVolumePathNamesForVolumeName succeed on getting the Volume Name: %s  for Volume GUID %s\n", chVolumePath, strVolGuid.c_str());
							DebugPrintf(SV_LOG_INFO,"Might Be Some issue on enumerating the Volumes of MT...Retrying once again on enumirating the volumes...\n");
							bResult = false;
							break;
						}
					}
					else
					{
						if(GetLastError() == ERROR_MORE_DATA)
						{
							DebugPrintf(SV_LOG_INFO,"GetVolumePathNamesForVolumeName succeed on getting the Volume Name: %s  for Volume GUID %s\n", chVolumePath, strVolGuid.c_str());
							DebugPrintf(SV_LOG_INFO,"Might Be Some issue on enumerating the Volumes of MT...Retrying once again on enumirating the volumes...\n");
							bResult = false;
							break;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : [%lu] for Volume GUID %s\n",GetLastError(), strVolGuid.c_str());
							DebugPrintf(SV_LOG_INFO, "The Volume GUID: %s is not exist, So skipping it and proceeding with other Volumes\n",strVolGuid.c_str());
						}
					}
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed for Volume: %s with Error code : [%lu].\n",(iter_beg->first).c_str(), GetLastError());
				char chVolumePath[MAX_PATH+1] = {0};
				DWORD dwVolPathSize = 0;
				string strVolGuid = iter_beg->first + "\\";
				if(GetVolumePathNamesForVolumeName (strVolGuid.c_str(),chVolumePath,MAX_PATH, &dwVolPathSize))
				{
					if(0 != strcmpi(chVolumePath, ""))
					{
						DebugPrintf(SV_LOG_INFO,"GetVolumePathNamesForVolumeName succeed on getting the Volume Name: %s  for Volume GUID %s\n", chVolumePath, strVolGuid.c_str());
						DebugPrintf(SV_LOG_INFO,"Might Be Some issue on enumerating the Volumes of MT...Retrying once again on enumirating the volumes...\n");
						bResult = false;
						break;
					}
				}
				else
				{
					if(GetLastError() == ERROR_MORE_DATA)
					{
						DebugPrintf(SV_LOG_INFO,"GetVolumePathNamesForVolumeName succeed on getting the Volume Name: %s  for Volume GUID %s\n", chVolumePath, strVolGuid.c_str());
						DebugPrintf(SV_LOG_INFO,"Might Be Some issue on enumerating the Volumes of MT...Retrying once again on enumirating the volumes...\n");
						bResult = false;
						break;
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : [%lu] for Volume GUID %s\n",GetLastError(), strVolGuid.c_str());
						DebugPrintf(SV_LOG_INFO, "The Volume GUID: %s is not exist, So skipping it and proceeding with other Volumes\n",strVolGuid.c_str());
					}
				}
			}
		}
		CloseHandle( hVolume );
		iterMasterTargetGuids++;
		iter_beg++;
	}
	m_mapOfAlreadyPresentMasterVolumes.clear();
	//Assign the True Private Volumes Map
	m_mapOfAlreadyPresentMasterVolumes = realPrivateVolumesOfMasterTarget;
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::DumpVolumesOrMntPtsExtentOnDisk( )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	m_mapVolInfo.clear(); // Make sure to clean map before persisting new values
	bool bResult = true;
	HANDLE	hVolume;
	ULONG	extent;
	int     uDiskNumber = -1;
	ULONG	bytesWritten;
	UCHAR	DiskExtentsBuffer[0x4000];
	PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
	volInfo obj;
	map<string,string> tempMap;

	map<string,string>::iterator iter_beg = m_mapOfGuidsWithLogicalNames.begin();
	map<string,string>::iterator iter_end = m_mapOfGuidsWithLogicalNames.end();
	map<string,string>::iterator iter_MasterTgtVolume;
	while(iter_beg != iter_end)
	{
	   string strTemp = (iter_beg->first);
	   string::size_type pos = strTemp.find_last_of("\\");
	   if(pos !=string::npos)
	   {
		   string strSecondPart = (iter_beg)->second;
		   strTemp.erase(pos);
		   tempMap[strTemp]= strSecondPart;
		}
		else
		{
			tempMap[iter_beg->first] = iter_beg->second;
		}
	    iter_beg++;	
	}
	m_mapOfGuidsWithLogicalNames = tempMap;
	iter_beg = m_mapOfGuidsWithLogicalNames.begin();
	
	while(iter_beg != iter_end)
	{
        DebugPrintf(SV_LOG_DEBUG,"Volume GUID - iter_beg->first = %s.\n",(iter_beg->first).c_str());
		//Duplicate code..Already taken care during the master target volumes extent call
		if(true == IsGuidAlreadyMounted(iter_beg->first))
		{
			DebugPrintf(SV_LOG_DEBUG,"Volume %s is already mounted. Skipping it.\n",(iter_beg->first.c_str()));
			iter_beg++;
			continue;
		}
		iter_MasterTgtVolume = m_mapOfAlreadyPresentMasterVolumes.find(iter_beg->first);
		if(iter_MasterTgtVolume != m_mapOfAlreadyPresentMasterVolumes.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Volume %s is part of master target private volume list. Skipping it.\n",(iter_beg->first.c_str()));
			iter_beg++;
			continue;
		}
		//Before opening the Volume handle see,if volume is already protected,If yes ignore the iteration
		hVolume = SVCreateFile((iter_beg->first).c_str(),
					GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
					NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |FILE_FLAG_OPEN_REPARSE_POINT  , NULL );
		if( hVolume == INVALID_HANDLE_VALUE ) 
		{
			if(GetLastError() == ERROR_ACCESS_DENIED)// To Support Rerun scenario
			{
					DebugPrintf(SV_LOG_DEBUG,"Volume %s is under protection.\n",(iter_beg->first).c_str());
					DebugPrintf(SV_LOG_DEBUG,"Considering it as Master Target Private volume.\n");
					iter_beg++;
					continue;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Error getting extents for drive : %s. Error Code [%lu].\n",((iter_beg->first).c_str()),GetLastError());
				bResult = false;
				break;
			}
		}
		if(DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer), &bytesWritten, NULL)) 
		{
			LARGE_INTEGER startOffset;
			LARGE_INTEGER PartitionLength;
			LARGE_INTEGER EndingOffset;
			for( extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
				startOffset = DiskExtents->Extents[extent].StartingOffset;
				PartitionLength = DiskExtents->Extents[extent].ExtentLength;
				EndingOffset.QuadPart = startOffset.QuadPart + PartitionLength.QuadPart;
				uDiskNumber			= DiskExtents->Extents[extent].DiskNumber;
				obj.startingOffset  = startOffset;
				obj.partitionLength = PartitionLength;
				obj.endingOffset    = EndingOffset;
				inm_strncpy_s(obj.strVolumeName, ARRAYSIZE(obj.strVolumeName), iter_beg->second.c_str(),MAX_PATH);
				m_mapVolInfo.insert(pair<int,volInfo>(uDiskNumber,obj));
			}			
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed with Error code : [%lu].\n",GetLastError());
			bResult = false;
			break;
		}
		CloseHandle( hVolume );
		iter_beg++;
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Restrats a Service
//Input : Service name
//Returns True on Success
HRESULT MasterTarget::restartService(const string& serviceName)
{
	 DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	 SC_HANDLE schService;        
     SERVICE_STATUS_PROCESS serviceStatusProcess;
	 SERVICE_STATUS serviceStatus;
	 DWORD actualSize = 0;
	 ULONG dwRet = 0;
	 LPVOID lpMsgBuf;
	 SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	 if (NULL == schSCManager) 
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;
    }

	// stop requested service
	schService = ::SVOpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
    if(NULL != schService) 
	{ 
		if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == serviceStatusProcess.dwCurrentState) 
		{
            DebugPrintf(SV_LOG_INFO,"Stopping %s service.\n",serviceName.c_str());
            // stop service
			if (::ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) 
			{
                // wait for it to actually stop
				int retry = 1;
				do 
				{
					Sleep(1000); 
				} while (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState && retry++ <= 600/*180 */);//some times service was taking too much time to stop..So increased this value to 10 mins as suggested by Jayesh
				if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState) 
				{
					dwRet = GetLastError();     
					DebugPrintf(SV_LOG_ERROR,"Failed to stop the service: %s with Error Code : [%lu].\n",serviceName.c_str(),GetLastError());
					::CloseServiceHandle(schService); 
					goto Error_return_fail;    
				}
            }
        }
		else
		{
			DebugPrintf(SV_LOG_INFO,"The service  %s is currently not running.\n",serviceName.c_str());
		}

		// start the service
        DebugPrintf(SV_LOG_INFO,"Starting %s service.\n",serviceName.c_str());
		if (::StartService(schService, 0, NULL) == 0) 
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to start the service: %s with Error Code : [%lu].\n",serviceName.c_str(),GetLastError());
			dwRet = GetLastError();
			::CloseServiceHandle(schService); 
			goto Error_return_fail;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully started the service  %s \n",serviceName.c_str());
		}
		::CloseServiceHandle(schService); 
	}
	else
	{
		dwRet = GetLastError();
		goto Error_return_fail;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;

	Error_return_fail: 
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwRet, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
	if(dwRet)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to restart %s service Error Code : [%lu].\n",serviceName.c_str(), dwRet);
        DebugPrintf(SV_LOG_ERROR,"Error %lu = %s",dwRet,(LPCTSTR)lpMsgBuf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	}  

	// Free the buffer
	LocalFree(lpMsgBuf);
	return S_FALSE;
}

bool MasterTarget::CreateSrcTgtVolumeMapForVMs(std::string& strErrorMsg)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    BOOL bIsMasterTgtPrivateVolume = true;

	if(m_bPhySnapShot)
		DebugPrintf(SV_LOG_INFO,"\nProcessing the volumes on target disks to prepare for Snapshot.\n\n");
	else
		DebugPrintf(SV_LOG_INFO,"\nProcessing the volumes on target disks to prepare for replication.\n\n");

	Sleep(10000);

	DebugPrintf(SV_LOG_DEBUG,"vmDiskMap size = %d.\n",m_mapOfVmToSrcAndTgtDiskNmbrs.size());

	//We'll Read complete Disks corresponding to a given VM and then try to set Replication Pair for that VM.
	map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
	//This outer Loop is for each VM
    for(; Iter_beg != Iter_end;Iter_beg++)
    {
        DebugPrintf(SV_LOG_DEBUG,"----------------------------------------------------------------------------------------------\n");
        DebugPrintf(SV_LOG_INFO,"Virtual Machine Name = %s\n",Iter_beg->first.c_str());
		DebugPrintf(SV_LOG_DEBUG,"----------------------------------------------------------------------------------------------\n");

		string strSrcHostName = "";
		if (m_mapHostIdToHostName.find(Iter_beg->first) != m_mapHostIdToHostName.end())
			strSrcHostName = m_mapHostIdToHostName.find(Iter_beg->first)->second;

		strErrorMsg = "Volume mismatch between target volumes created on " + m_strMtHostName + " and source volumes on " + strSrcHostName;
		m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
		m_PlaceHolder.Properties[SOURCE_HOSTNAME] = strSrcHostName;

		//The following will generate the map of source and target volumes(myReplicationMap)
	    map<string,string> myReplicationMap;
        HRESULT hrRetVal = S_FALSE;
        for(int i = 1; i <= 3; i++)
        {
            myReplicationMap.clear();
            hrRetVal = setReplicationPair(Iter_beg->first,Iter_beg->second,myReplicationMap);
            if(S_FALSE == hrRetVal)
            {
			    DebugPrintf(SV_LOG_ERROR,"%d. Could not get the map of source and target volumes. ",i);
            }
            else
            {
		        DebugPrintf(SV_LOG_INFO,"\nFetched the map of source and target volumes for this VM.\n");
                break;
            }
            if(i!=3)
            {
                DebugPrintf(SV_LOG_ERROR,"Trying again.\n");
                Sleep(60*1000); 	            
            }
        }	
        if(S_FALSE == hrRetVal)
	    {
		    DebugPrintf(SV_LOG_ERROR,"\n\nFailed to fetch the map of source and target volumes.\n");
            DebugPrintf(SV_LOG_ERROR,"Please check on Master Target whether disks are in proper state or not.\n");
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return false;
        }	    

		map<string,string>::iterator iter_beg = myReplicationMap.begin();
		map<string,string>::iterator iter_end = myReplicationMap.end();
        DebugPrintf(SV_LOG_DEBUG,"The Volume pairs for this VM are as follows(before renaming).\n");
		for(;iter_beg != iter_end;iter_beg++)
		{
            DebugPrintf(SV_LOG_DEBUG,"Source Volume = %s  <=====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
		}
		iter_beg = myReplicationMap.begin();

		//Here we'll delete the existing count based mount points .We'll use the VM name and the coreesponding volume name for creating new mount point.
        DebugPrintf(SV_LOG_DEBUG,"Renaming the Volume Mount points for this VM.\n");
        char volGuid[MAX_PATH];		
		map<string,string> myTempMapForNewMountPoins;
		map<string, string> mapTgtVolGuidToVolMntPoint;
		while(iter_beg != iter_end)
		{
            DebugPrintf(SV_LOG_DEBUG,"Changing Volume Name for Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());				
			memset(volGuid,0,MAX_PATH);
			//We need to fetch the Volume Guid for the corresponding mount point name.This guid we'll use in the SetvolumeMountPoint
			if(0 == GetVolumeNameForVolumeMountPoint(iter_beg->second.c_str(),volGuid,MAX_PATH))
			{
                DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				DebugPrintf(SV_LOG_ERROR,"GetVolumeNameForVolumeMountPoint failed with Errorcode : [%lu].\n",GetLastError());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;                    
			}
            if(!iter_beg->second.empty())
            {
                //delete the existing mount point C:\inmage_part\_0
			    if(0 == DeleteVolumeMountPoint(iter_beg->second.c_str()))
			    {
                    DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				    DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint failed with ErrorCode : [%lu].\n",GetLastError());
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				    return false;
			    }
            }
			string strNewMountPointName ;
			string strSrcVolName = iter_beg->first;
			RemoveTrailingCharactersFromVolume(strSrcVolName);

			//Removing unwanted characters from Source Volume name
			for(size_t indx = 0;indx<strSrcVolName.length();indx++)
			{
                DebugPrintf(SV_LOG_DEBUG,"In the loop for removing unwanted characters from source volume name.\n");
				if((strSrcVolName[indx] == '\\') || (strSrcVolName[indx] == '/') || (strSrcVolName[indx] == ':') || (strSrcVolName[indx] == '?'))
				{
					strSrcVolName[indx] = '_';
				}
			}
			
			//If the Path length is more than 255 //Need to work on the algo...
			if(strSrcVolName.length() >= (MAX_PATH - (strDirectoryPathName.length()+ 50)))
			{	
				DebugPrintf(SV_LOG_DEBUG,"in the MAX path sceanrio.\n");
                string strVolGuid(volGuid);                   
                size_t iCount = strSrcVolName.find_last_of("_");
                if(iCount != std::string::npos)
                {
                    string strLastPartOfString = strSrcVolName.substr(iCount,strSrcVolName.length());
                    if(strLastPartOfString.length() >= (MAX_PATH-6))
                    {
                        strSrcVolName = strVolGuid;			
                    }
                }
			}
			strNewMountPointName = strDirectoryPathName + string("\\") + Iter_beg->first + string("_") + strSrcVolName ;			
			
			string strTemp = iter_beg->second;
			string::size_type pos = strTemp.find_last_of("\\");
			if(pos !=string::npos)
			{
				strTemp.erase(pos);
			}
			
			DebugPrintf(SV_LOG_DEBUG,"strTemp = %s\n", strTemp.c_str());
			DebugPrintf(SV_LOG_DEBUG,"strNewMountPointName = %s\n", strNewMountPointName.c_str());
			
			if(0 != strTemp.compare(strNewMountPointName))
			{
				//check if any file/folder with same name as strNewMountPointName exists already, 
				// and then try to delete it, else return false because we need our mountpoint with this name itself.
				if(checkIfFileExists(strNewMountPointName))
				{
					DWORD File_Attribute;
					File_Attribute = GetFileAttributes(strNewMountPointName.c_str());

					if(FILE_ATTRIBUTE_DIRECTORY & File_Attribute)
					{
						if(false == RemoveDirectory(strNewMountPointName.c_str()))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to delete the folder %s with error code : [%lu].\n",strNewMountPointName.c_str(),GetLastError());
							DebugPrintf(SV_LOG_ERROR,"Please delete this folder manually and rerun the job.\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
					}
					else
					{
						if(false == DeleteFile(strNewMountPointName.c_str()))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to delete the file %s with error code : [%lu].\n",strNewMountPointName.c_str(),GetLastError());
							DebugPrintf(SV_LOG_ERROR,"Please delete this file manually and rerun the job.\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
					}
				}

				// In some cases, temporary volume mount point name(ESX/_Number) is not created
				//   and instead drive letter is present for the required target volume.
				// In this scenario, create the directory C:\ESX\hostname_volume instead 
				//   of moving the temp mnt pt as done earlier(moved to else case now)
				if (strTemp.length() <= 3)
				{
					if(FALSE == CreateDirectory(strNewMountPointName.c_str(),NULL))
					{
						if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
						{
							DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
							DebugPrintf(SV_LOG_ERROR,"Failed to create Mount Directory - %s. ErrorCode : [%lu].\n",strNewMountPointName.c_str(),GetLastError());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return FALSE;
						}
					}
				}
				else
				{
					//after unmounting mount point(say C:\ESX\_15),rename the file(say C:\ESX\hostname_volume).
					if(0 == MoveFile(strTemp.c_str(),strNewMountPointName.c_str()))
					{
						int iErrorCode = GetLastError();
						//Ignore Already existing File code.Ignore already existing Folder
						if(183 == iErrorCode)
						{
							::RemoveDirectory(strTemp.c_str()); //Although in createmount point func,we are taking care of this scenario "Directory already exists" for the mount points as well as for ESX directory.
							DebugPrintf(SV_LOG_WARNING,"Directory %s already exists.Will use this for mounting.\n",strNewMountPointName.c_str());
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
							DebugPrintf(SV_LOG_ERROR,"MoveFile operation failed with error code : [%lu].\n",GetLastError());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
					}
				}
            }
			strNewMountPointName += "\\";
			if(0 == SetVolumeMountPoint(strNewMountPointName.c_str(),volGuid))
			{
				DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				DebugPrintf(SV_LOG_ERROR,"Failed to mount %s at %s with Error Code : [%lu].\n",volGuid,strNewMountPointName.c_str(),GetLastError());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			myTempMapForNewMountPoins.insert(make_pair(iter_beg->first,strNewMountPointName));
			mapTgtVolGuidToVolMntPoint[string(volGuid)] = strNewMountPointName;
			memset(volGuid,0,MAX_PATH);
			bIsMasterTgtPrivateVolume = true;
			iter_beg++;
		}
		myReplicationMap.clear();
		myReplicationMap = myTempMapForNewMountPoins;

		map<string,string>::iterator sd = myReplicationMap.begin();
        DebugPrintf(SV_LOG_INFO,"The Volume pairs for this VM are as follows.\n");
		while(sd != myReplicationMap.end())
		{
            DebugPrintf(SV_LOG_INFO,"Source Volume = %s  <=======> Target Volume = %s\n",sd->first.c_str(),sd->second.c_str());
			sd++;
		}

		//Update the tgtvolinfo file.
		if (false == UpdateInfoFile(Iter_beg->first, mapTgtVolGuidToVolMntPoint, string(TGT_VOLUMESINFO)))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to update the tgtVolInfo file for VM - %s.\n", Iter_beg->first.c_str());
		}

		m_prepTgtVMsUpdate.m_volumeMapping[Iter_beg->first] = myReplicationMap;
    }
	
	m_statInfo.clear();
	m_PlaceHolder.clear();

 	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//All the information related to VX jobs is collected here.
//Before that volumes are prepared by finding the matching volumes and renaming them accordingly
bool MasterTarget::mapSrcAndTgtVolumes()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    m_mapOfTgtVolToVxJobObj.clear(); //Build this map here. Will be used to set Vx Jobs
	char szBaseVolumeOfRetentionPath[MAX_PATH];
    BOOL bIsMasterTgtPrivateVolume = true;
	map<string,string> VM_IP_map; //map of VMs to its IPs
	string strHostIPAddress;

	if(!m_bPhySnapShot)
	{
		if(m_vConVersion >= 4.0)
		{
			//Find the Master target inmage host id
			strHostIPAddress = m_strMtHostID;
		}
		else
		{
			//Find the Master target IP's address
			strHostIPAddress = FetchHostIPAddress();
			if(strHostIPAddress.empty())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to fetch IP address of this host.\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			//To Set Replication Pair We need IPAddress of guest OSes(Sources)			
			VM_IP_map = readPersistedIPaddress();	
			if(VM_IP_map.empty())
			{
				DebugPrintf(SV_LOG_ERROR,"readPersistedIPaddress() module failed.\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			map<string,string>::const_iterator iter_beg = VM_IP_map.begin();
			map<string,string>::const_iterator iter_end = VM_IP_map.end();
			while(iter_beg != iter_end)
			{
				DebugPrintf(SV_LOG_DEBUG,"Machine Name = %s <==> IP Address = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				iter_beg++;
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"vmDiskMap size = %d.\n",m_mapOfVmToSrcAndTgtDiskNmbrs.size());

	//We'll Read complete Disks corresponding to a given VM and then try to set Replication Pair for that VM.
	map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
	//This outer Loop is for each VM
    for(; Iter_beg != Iter_end;Iter_beg++)
    {
        DebugPrintf(SV_LOG_DEBUG,"----------------------------------------------------------------------------------------------\n");
        DebugPrintf(SV_LOG_INFO,"Virtual Machine Name = %s\n",Iter_beg->first.c_str());
		DebugPrintf(SV_LOG_DEBUG,"----------------------------------------------------------------------------------------------\n");

		string strVxJobFileName = m_strXmlFile;
        VxJobXml VmObj; 
        //First we will update VmObj.ProcessServerIp,VmObj.RetainChangeDays,VmObj.RetainChangeMB,VmObj.RetentionLogPath
		if (false == xGetValuesFromXML(VmObj,strVxJobFileName,Iter_beg->first))
		{            
			DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Retention details and Process Server IP for this VM.\n");
 	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
        if (SVGetVolumePathName(VmObj.RetentionLogPath.c_str(), szBaseVolumeOfRetentionPath, ARRAYSIZE(szBaseVolumeOfRetentionPath)) == 0)
		{
            DebugPrintf(SV_LOG_ERROR,"GetVolumePathName() failed for retention path - [%s] with Error : [%lu].\n", VmObj.RetentionLogPath.c_str(), GetLastError());
            DebugPrintf(SV_LOG_ERROR,"Please check whether above retention path exists or not on the Master Target.\n");
 	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		//The following will generate the map of source and target volumes(myReplicationMap)
	    map<string,string> myReplicationMap;
        HRESULT hrRetVal = S_FALSE;
        for(int i = 1; i <= 3; i++)
        {
            myReplicationMap.clear();
            hrRetVal = setReplicationPair(Iter_beg->first,Iter_beg->second,myReplicationMap);
            if(S_FALSE == hrRetVal)
            {
			    DebugPrintf(SV_LOG_ERROR,"%d. Could not get the map of source and target volumes. ",i);
            }
            else
            {
		        DebugPrintf(SV_LOG_DEBUG,"\nFetched the map of source and target volumes for this VM.\n");
                break;
            }
            if(i!=3)
            {
                DebugPrintf(SV_LOG_ERROR,"Trying again.\n");
                Sleep(120*1000); 	            
            }
        }	
        if(S_FALSE == hrRetVal)
	    {
		    DebugPrintf(SV_LOG_ERROR,"\n\nFailed to fetch the map of source and target volumes.\n");
            DebugPrintf(SV_LOG_ERROR,"Please check on Master Target whether the disk management is stopped responding or it is very slow.\n");
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return false;
        }	    

		map<string,string>::iterator iter_beg = myReplicationMap.begin();
		map<string,string>::iterator iter_end = myReplicationMap.end();
        DebugPrintf(SV_LOG_DEBUG,"The Volume pairs for this VM are as follows(before renaming).\n");
		for(;iter_beg != iter_end;iter_beg++)
		{
            DebugPrintf(SV_LOG_DEBUG,"Source Volume = %s  <=====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
		}
		iter_beg = myReplicationMap.begin();

		//Here we'll delete the existing count based mount points .We'll use the VM name and the coreesponding volume name for creating new mount point.
        DebugPrintf(SV_LOG_DEBUG,"Renaming the Volume Mount points for this VM.\n");
        char volGuid[MAX_PATH];		
		map<string,string> myTempMapForNewMountPoins;
		map<string, string> mapTgtVolGuidToVolMntPoint;
		while(iter_beg != iter_end)
		{
            DebugPrintf(SV_LOG_DEBUG,"Changing Volume Name for Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
            memset(volGuid,0,MAX_PATH);
			//We need to fetch the Volume Guid for the corresponding mount point name.This guid we'll use in the SetvolumeMountPoint
			if(0 == GetVolumeNameForVolumeMountPoint(iter_beg->second.c_str(),volGuid,MAX_PATH))
			{
                DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				DebugPrintf(SV_LOG_ERROR,"GetVolumeNameForVolumeMountPoint failed with Errorcode : [%lu].\n",GetLastError());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;                    
			}
            if(!iter_beg->second.empty())
            {
                //delete the existing mount point C:\inmage_part\_0
			    if(0 == DeleteVolumeMountPoint(iter_beg->second.c_str()))
			    {
                    DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				    DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint failed with ErrorCode : [%lu].\n",GetLastError());
                    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				    return false;
			    }
            }

			string strNewMountPointName;
			bool bGotMountPoint = false;
			bool bAddDynamicDisk = false;

			if(bIsAdditionOfDisk)
			{
				map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.find(Iter_beg->first);
				if(iterDyn != m_mapHostIdDynAddDisk.end())
					bAddDynamicDisk = iterDyn->second;
			}

			//Cluster case the volume could be exist anywhere in the latest infomration but we need the Old mount points back to these volumes
			double dlmVesion = m_mapOfHostIdDlmVersion.find(Iter_beg->first) == m_mapOfHostIdDlmVersion.end() ? 1.0 : m_mapOfHostIdDlmVersion[Iter_beg->first];
			if((m_bVolResize || bAddDynamicDisk) && dlmVesion >= 1.2 && IsClusterNode(Iter_beg->first))
			{
				map<string,string> mapPairs;
				map<string, string>::iterator iterMapPair;
				string strInfoFile	= m_strDataFolderPath + Iter_beg->first + string(VOLUMESINFO_EXTENSION);
				if(false==ReadFileWith2Values(strInfoFile,mapPairs))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to fetch the map of Replicated Volumes for VM : %s.\n",Iter_beg->first.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}

				iterMapPair = mapPairs.find(iter_beg->first);
				if(iterMapPair != mapPairs.end())
				{
					strNewMountPointName = iterMapPair->second;
					DebugPrintf(SV_LOG_DEBUG,"Got the Earlier mount point : %s.\n",strNewMountPointName.c_str());
					bGotMountPoint = true;
				}
				else
				{
					list<string> nodes;
					GetClusterNodes(GetNodeClusterName(Iter_beg->first),nodes);

					for(list<string>::iterator iterNode = nodes.begin(); iterNode != nodes.end(); iterNode++)
					{
						if(iterNode->compare(Iter_beg->first) != 0)
						{
							mapPairs.clear();
							strInfoFile	= m_strDataFolderPath + (*iterNode) + string(VOLUMESINFO_EXTENSION);
							if(false==ReadFileWith2Values(strInfoFile,mapPairs))
							{
								DebugPrintf(SV_LOG_WARNING,"Failed to fetch the map of Replicated Volumes for node : %s.\n",iterNode->c_str());
								continue;
							}

							iterMapPair = mapPairs.find(iter_beg->first);
							if(iterMapPair != mapPairs.end())
							{
								strNewMountPointName = iterMapPair->second;
								bGotMountPoint = true;
								DebugPrintf(SV_LOG_DEBUG,"Got the Earlier mount point : %s.\n",strNewMountPointName.c_str());
								break;
							}
						}
					}
				}
			}

			if(!bGotMountPoint)
			{
				string strSrcVolName = iter_beg->first;
				RemoveTrailingCharactersFromVolume(strSrcVolName);

				//Removing unwanted characters from Source Volume name
				for(size_t indx = 0;indx<strSrcVolName.length();indx++)
				{
					DebugPrintf(SV_LOG_DEBUG,"In the loop for removing unwanted characters from source volume name.\n");
					if((strSrcVolName[indx] == '\\') || (strSrcVolName[indx] == '/') || (strSrcVolName[indx] == ':') || (strSrcVolName[indx] == '?'))
					{
						strSrcVolName[indx] = '_';
					}
				}
				
				//If the Path length is more than 255 //Need to work on the algo...
				if(strSrcVolName.length() >= (MAX_PATH - (strDirectoryPathName.length()+ 50)))
				{	
					DebugPrintf(SV_LOG_DEBUG,"in the MAX path sceanrio.\n");
					string strVolGuid(volGuid);                   
					size_t iCount = strSrcVolName.find_last_of("_");
					if(iCount != std::string::npos)
					{
						string strLastPartOfString = strSrcVolName.substr(iCount,strSrcVolName.length());
						if(strLastPartOfString.length() >= (MAX_PATH-6))
						{
							strSrcVolName = strVolGuid;			
						}
					}
				}
				strNewMountPointName = strDirectoryPathName + string("\\") + Iter_beg->first + string("_") + strSrcVolName ;
			}

			string strTemp = iter_beg->second;
			string::size_type pos = strTemp.find_last_of("\\");
			if(pos !=string::npos)
			{
				strTemp.erase(pos);
			}
			
			DebugPrintf(SV_LOG_DEBUG,"strTemp = %s\n", strTemp.c_str());
			DebugPrintf(SV_LOG_DEBUG,"strNewMountPointName = %s\n", strNewMountPointName.c_str());
				
			if(0 != strTemp.compare(strNewMountPointName))
			{
				//check if any file/folder with same name as strNewMountPointName exists already, 
				// and then try to delete it, else return false because we need our mountpoint with this name itself.
				if(checkIfFileExists(strNewMountPointName))
				{
					DWORD File_Attribute;
					File_Attribute = GetFileAttributes(strNewMountPointName.c_str());

					if(FILE_ATTRIBUTE_DIRECTORY & File_Attribute)
					{
						if(false == RemoveDirectory(strNewMountPointName.c_str()))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to delete the folder %s with error code : [%lu].\n",strNewMountPointName.c_str(),GetLastError());
							DebugPrintf(SV_LOG_ERROR,"Please delete this folder manually and rerun the job.\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
					}
					else
					{
						if(false == DeleteFile(strNewMountPointName.c_str()))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to delete the file %s with error code : [%lu].\n",strNewMountPointName.c_str(),GetLastError());
							DebugPrintf(SV_LOG_ERROR,"Please delete this file manually and rerun the job.\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
					}
				}

				// In some cases, temporary volume mount point name(ESX/_Number) is not created
				//   and instead drive letter is present for the required target volume.
				// In this scenario, create the directory C:\ESX\hostname_volume instead 
				//   of moving the temp mnt pt as done earlier(moved to else case now)
				if (strTemp.length() <= 3)
				{
					if(FALSE == CreateDirectory(strNewMountPointName.c_str(),NULL))
					{
						if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
						{
							DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
							DebugPrintf(SV_LOG_ERROR,"Failed to create Mount Directory - %s. ErrorCode : [%lu].\n",strNewMountPointName.c_str(),GetLastError());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return FALSE;
						}
					}
				}
				else
				{
					//after unmounting mount point(say C:\ESX\_15),rename the file(say C:\ESX\hostname_volume).
					if(0 == MoveFile(strTemp.c_str(),strNewMountPointName.c_str()))
					{
						int iErrorCode = GetLastError();
						//Ignore Already existing File code.Ignore already existing Folder
						if(183 == iErrorCode)
						{
							::RemoveDirectory(strTemp.c_str()); //Although in createmount point func,we are taking care of this scenario "Directory already exists" for the mount points as well as for ESX directory.
							DebugPrintf(SV_LOG_DEBUG,"Directory %s already exists.Will use this for mounting.\n",strNewMountPointName.c_str());
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
							DebugPrintf(SV_LOG_ERROR,"MoveFile operation failed with error code : [%lu].\n",GetLastError());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
					}
				}
            }
			if(strNewMountPointName[strNewMountPointName.length()-1] != '\\')
				strNewMountPointName += "\\";
			if(0 == SetVolumeMountPoint(strNewMountPointName.c_str(),volGuid))
			{
				DebugPrintf(SV_LOG_ERROR,"Source Volume = %s <====> Target Volume = %s\n",iter_beg->first.c_str(),iter_beg->second.c_str());
				DebugPrintf(SV_LOG_ERROR,"Failed to mount %s at %s with Error Code : [%lu].\n",volGuid,strNewMountPointName.c_str(),GetLastError());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			myTempMapForNewMountPoins.insert(make_pair(iter_beg->first,strNewMountPointName));
			mapTgtVolGuidToVolMntPoint[string(volGuid)] = strNewMountPointName;
			memset(volGuid,0,MAX_PATH);
			bIsMasterTgtPrivateVolume = true;
			iter_beg++;
		}
		myReplicationMap.clear();
		myReplicationMap = myTempMapForNewMountPoins;

		map<string,string>::iterator sd = myReplicationMap.begin();
        DebugPrintf(SV_LOG_INFO,"The Volume pairs for this VM are as follows.\n");
		while(sd != myReplicationMap.end())
		{
            DebugPrintf(SV_LOG_INFO,"Source Volume = %s  <=======> Target Volume = %s\n",sd->first.c_str(),sd->second.c_str());
			sd++;
		}

		if(!m_bPhySnapShot)
		{
			if(m_vConVersion >= 4.0)
			{
				VmObj.SourceInMageHostId = Iter_beg->first;
			}
			else
			{
				map<string,string>::iterator iterVmIp;
				iterVmIp = VM_IP_map.find(Iter_beg->first);
				if(iterVmIp == VM_IP_map.end())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to fetch IP address corresponding to this VM.\n");
 					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				//Earlier we updated VmObj.ProcessServerIp,VmObj.RetainChangeDays,VmObj.RetainChangeMB,VmObj.RetentionLogPath
				VmObj.SourceMachineIP = iterVmIp->second;
			}
			string strRetLogVolName = string(szBaseVolumeOfRetentionPath);
			FormatVolumeNameForCxReporting(strRetLogVolName);
			if(VmObj.RetentionLogVolumeName.empty())
				VmObj.RetentionLogVolumeName = strRetLogVolName;
			if(m_vConVersion >= 4.0)
				VmObj.TargetInMageHostId = strHostIPAddress;
			else
				VmObj.TargetMachineIP = strHostIPAddress;
		}
        VmObj.VmHostName = Iter_beg->first;

		map<string,string>::iterator iterRepMap;

		if(bIsAdditionOfDisk)
		{
			map<string,string> mapPairs;
			map<string, string>::iterator iterMapPair;
			string strInfoFile	= m_strDataFolderPath + VmObj.VmHostName + string(VOLUMESINFO_EXTENSION);
			string strTgtVolFile = m_strDataFolderPath + VmObj.VmHostName + string(TGT_VOLUMESINFO);
			if (!checkIfFileExists(strInfoFile))
			{
				string strHostVmName;
				if(false == xGetHostNameFromHostId( VmObj.VmHostName, strHostVmName ) )
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the hostname for hostid. %s\n",VmObj.VmHostName.c_str());
				}
				strInfoFile	= m_strDataFolderPath + strHostVmName + string(VOLUMESINFO_EXTENSION);
				strTgtVolFile = m_strDataFolderPath + strHostVmName + string(TGT_VOLUMESINFO);
			}
			if(false==ReadFileWith2Values(strInfoFile,mapPairs))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to fetch the map of Replicated Volumes for VM : %s.\n",VmObj.VmHostName.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			iterRepMap = mapPairs.begin();
			for(; iterRepMap != mapPairs.end(); iterRepMap++)
			{
				iterMapPair = myReplicationMap.find(iterRepMap->first);
				if(iterMapPair != myReplicationMap.end())
				{
					if(0 == iterRepMap->second.compare(iterMapPair->second))
						myReplicationMap.erase(iterMapPair);
				}
			}
			
			mapPairs.clear();
			if (false == ReadFileWith2Values(strTgtVolFile, mapPairs))
			{
				DebugPrintf(SV_LOG_WARNING, "Failed to fetch the map of target Volumes for VM : %s.\n", VmObj.VmHostName.c_str());
			}
			else
			{
				iterRepMap = mapPairs.begin();
				for (; iterRepMap != mapPairs.end(); iterRepMap++)
				{
					iterMapPair = mapTgtVolGuidToVolMntPoint.find(iterRepMap->first);
					if (iterMapPair != mapTgtVolGuidToVolMntPoint.end())
					{
						if (0 == iterRepMap->second.compare(iterMapPair->second))
							mapTgtVolGuidToVolMntPoint.erase(iterMapPair);
					}
				}
			}
		}

        //Now update VmObj for each pair and push to map m_mapOfTgtVolToVxJobObj
        iterRepMap = myReplicationMap.begin();
        for(;iterRepMap != myReplicationMap.end();iterRepMap++)
        {
            VmObj.SourceVolumeName = iterRepMap->first;
            VmObj.TargetVolumeName = iterRepMap->second;
            m_mapOfTgtVolToVxJobObj.insert(make_pair(VmObj.TargetVolumeName,VmObj));
            //Store this Value in Global vector .At the time of dumpdiskextents,see if volume is 
            //protected,If yes skip this volume,else we'll get Access denied Error msg
		    if(false == persistReplicatedTgtVolume(VmObj.TargetVolumeName))
		    {
			    DebugPrintf(SV_LOG_ERROR,"Failed to persist %s (Locked Volume)\n",VmObj.TargetVolumeName.c_str());
 	            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			    return false;			    
		    }
        }
		if(m_bPhySnapShot)
		{
			//Update the pinfo file.
			if(false == UpdateInfoFile(VmObj.VmHostName,myReplicationMap,string(SNAPSHOTINFO_EXTENSION)))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update the sInfo file for VM - %s.\n",VmObj.VmHostName.c_str());
 				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Completed the update of sInfo file for VM - %s.\n",VmObj.VmHostName.c_str());
			}
		}
		else
		{
			if(!m_bVolResize)
			{
				//Update the pinfo file.
				if(false == UpdateInfoFile(VmObj.VmHostName,myReplicationMap,string(VOLUMESINFO_EXTENSION)))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to update the pInfo file for VM - %s.\n",VmObj.VmHostName.c_str());
 					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}

				//Update the tgtvolinfo file.
				if (false == UpdateInfoFile(VmObj.VmHostName, mapTgtVolGuidToVolMntPoint, string(TGT_VOLUMESINFO)))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update the tgtVolInfo file for VM - %s.\n", VmObj.VmHostName.c_str());
				}
			}
		}

		myReplicationMap.clear();
    }
	    
 	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Process all the Vx Pairs,
//Start the Vx service(svagents) if not.
//Separate registered and unregistered volumes with Cx.
//Set the Vx Jobs for registered volumes automatically if all volumes are registered.
//Update the user about the unregistered volumes and ask him to restart the job
bool MasterTarget::SetVxJobs(string &strErrorMsg, string &strFixStep)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    DebugPrintf(SV_LOG_INFO,"\n----------------------------------------------------------------------------------------------------\n");
    DebugPrintf(SV_LOG_INFO,"Preparing the volumes to set the volume replication pairs on CX.\n");
    DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------------\n");

	if (false == MountVolCleanup())
	{
		DebugPrintf(SV_LOG_ERROR, "\nFailed to clean stale mount points using mountvol\n\n");
	}

	if(false == RegisterHostUsingCdpCli())
	{
        DebugPrintf(SV_LOG_ERROR,"\nFailed to Register the Master Target disks using cdpcdli\n\n");
	}

    //Find CX Ip from Local Configurtaor
	string strCxIpAddress = getCxIpAddress();
    if (strCxIpAddress.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX IP Address.\n");
		strErrorMsg = "Failed to get the CX IP Address. For extended Error information download EsxUtilWin.log in vCon Wiz";
		strFixStep = string("Check the Vxagent is pointed to CX or not and availability of CX ") + 
						string("Restart the vxagent and rerun the job again");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Find CX port number
	string strCXHttpPortNumber = getCxHttpPortNumber();
	if (strCXHttpPortNumber.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX HTTP Port Number.\n");
		strErrorMsg = "Failed to get the CX HTTP Port Number. For extended Error information download EsxUtilWin.log in vCon Wiz";
		strFixStep = string("Check the Vxagnet is pointed to CX or not and availability of CX ") + 
						string("Restart the vxagent and rerun the job again");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	//Appending HTTP Port number to CX IP.
	strCxIpAddress += string(":") + strCXHttpPortNumber;
    string strCxUrl = m_strCmdHttp + strCxIpAddress + string("/cli/cx_cli_no_upload.php");

    //Check the registered volumes on Cx and generate two vectors - 
    //1. Registered Set  2. Not Yet Registered Set
    vector<VxJobXml> vecRegVolumes;
    vector<VxJobXml> vecUnregVolumes;
    if(false == SeparateVolumesSet(m_mapOfTgtVolToVxJobObj, strCxUrl, vecRegVolumes, vecUnregVolumes))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to separate the Registered Volumes and Unregistered Volumes set.\n");
		strErrorMsg = "Failed to separate the Registered Volumes and Unregistered Volumes set in CX";
		strFixStep = "Rerun the job, if issue persists contact inmage customer support";
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    //Make XMLs for registered and unregistered volumes separately
    string strRegVolFileName;
	if(m_strDirPath.empty())
		strRegVolFileName = m_strDataFolderPath + "Volumes_CxCli.xml";
	else
		strRegVolFileName = m_strDirPath + string("\\") + "Volumes_CxCli.xml";
    
    if(!vecUnregVolumes.empty())
    {        
        DebugPrintf(SV_LOG_ERROR,"\nThe following volumes of Mater Target are not registered with CX. So volume pairs are not set.\n");
        vector<VxJobXml>::iterator iter = vecUnregVolumes.begin();
        for(int i=1 ; iter !=  vecUnregVolumes.end(); iter++,i++)
        {
            DebugPrintf(SV_LOG_INFO,"%d. %s\n",i,iter->TargetVolumeName.c_str());
        }

        // Generate Volumes_CxCli.xml even in this case. so that we can avoid rerun of job
        // in the case where if all volumes are registered this can be used to set jobs.
        // This can be used in debugging or quick setting of jobs.
		DebugPrintf(SV_LOG_INFO,"\nRetrying the Volume Registration of Master Target with CX once again..\n");
		if(false == RegisterHostUsingCdpCli())
		{
			DebugPrintf(SV_LOG_ERROR,"\nFailed to Register the Master Target disks using cdpcdli\n\n");
		}
		Sleep(5000);
		std::vector<VxJobXml>::iterator iterTemp = vecUnregVolumes.begin();
        for( ; iterTemp != vecUnregVolumes.end(); iterTemp++) 
            vecRegVolumes.push_back(*iterTemp);       
    }

    //Post the XML for Registered Volumes
    if(!vecRegVolumes.empty())
    {
        if(false == MakeXmlForCxcli(vecRegVolumes, strRegVolFileName))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to make the XML for registered volumes.\n");
			strErrorMsg = "Failed to make the XML for registered volumes in CX";
			strFixStep = "Rerun the job, if issue persists contact inmage customer support";
 	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }

		if((!m_bPhySnapShot) && (!m_bVolResize))
		{
			string strProtectionFile;
			if(m_strDirPath.empty())
				strProtectionFile = m_strDataFolderPath + string("ProtectionStatusFile");
			else
				strProtectionFile = m_strDirPath + string("\\") + string("ProtectionStatusFile");

			ofstream outFile(strProtectionFile.c_str());
			if(!outFile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open File : %s.\n",strProtectionFile.c_str());
			}
			outFile<<"Successfully completed the protection pair creation for this plan";
			outFile.close();
			
			//Assign Secure permission to the file
			AssignSecureFilePermission(strProtectionFile);
		}

		for(int retry = 0; retry < 3; retry++)
		{
			if(false == PostXmlToCxcli(strCxUrl,strRegVolFileName,42))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to set the volume pairs on CX using Cxcli.\n");
				if(retry < 2)
				{
					DebugPrintf(SV_LOG_INFO,"Once again will retry Creation of pairs after 1 min.\n");
					Sleep(60000);
					continue;
				}
				strErrorMsg = "Failed to set the volume pairs on CX using Cxcli in 3 attempts. For extended Error information download EsxUtilWin.log in vCon Wiz";
				strFixStep = string("Check the CX availablity, Ping from MT, Restart the vxagent and rerun the job again");

 				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			else
			{
				//Process the Result CXCLI XML file to check any Vx pair is failed to set.
				if(false == ProcessCxcliOutputForVxPairs())
				{
					DebugPrintf(SV_LOG_INFO,"ProcessCxcliOutputForVxPairs failed.\n");
					if(retry < 2)
					{
						DebugPrintf(SV_LOG_INFO,"Once again will retry Creation of pairs after 1 min.\n");
						Sleep(60000);
						continue;
					}
					                
					//Display manual steps to be done by user if any volume pairs are not set                    
					string strCommand = string("\"") + m_strInstallDirectoryPath + string("\\cxcli.exe\" ") + 
										strCxUrl + string(" \"") + 
										strRegVolFileName + string("\" 42");
					DebugPrintf(SV_LOG_ERROR,"\n******************************** Important Information *********************************\n\n");
					DebugPrintf(SV_LOG_ERROR," Failed to set replication pairs for some of the volumes.\n");
					DebugPrintf(SV_LOG_ERROR," Set the replication pairs manually for the failed volumes.\n");
					DebugPrintf(SV_LOG_ERROR,"                              Or              \n");
					DebugPrintf(SV_LOG_ERROR," Run the following command on master target : \n");
					DebugPrintf(SV_LOG_ERROR," %s\n\n",strCommand.c_str());
					DebugPrintf(SV_LOG_ERROR,"****************************************************************************************\n\n");        

					strErrorMsg = "Failed to set replication pairs for some of the volumes. For extended Error information download EsxUtilWin.log in vCon Wiz";
					strFixStep = string("Set the replication pairs manually for the failed volumes (or) Run the following command on master target :\n") + strCommand + string("\n");

					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
	                
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"Successfully set the volume pairs on CX.\n\n");
					break;
				}
			}
		}
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"List of master target registered volumes on CX is empty.\n");
		if(!bIsAdditionOfDisk)
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
    }    

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

BOOL MasterTarget::IsThisMasterTgtVol(const string &strVolumeName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	map<string,string>::iterator iter = m_mapOfAlreadyPresentMasterVolumes.begin();
	while(iter != m_mapOfAlreadyPresentMasterVolumes.end())
	{
		if(iter->second == strVolumeName)
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return true;
		}
		iter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return false;
}


//This will compare source and target volumes and returns the source and target volume map
//Input  - strVmName is VM name
//Input  - mapOfDiskNumbers contains the map of source and target disk numbers of above VM
//Output - Src_Tgt_Vol_map contains the map of source and target volume names.
HRESULT MasterTarget::setReplicationPair(string strVmName, map<int,int> mapOfDiskNumbers, map<string,string> &Src_Tgt_Vol_map)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hResult = S_OK;

    map<string,multimap<unsigned int,volInfo>>::iterator iterFind;
    iterFind = m_mapOfVmToDiskVolReadFromBinFile.find(strVmName); //Will exist always
    m_Disk_Vol_ReadFromBinFile = iterFind->second;

	multimap<unsigned int,volInfo> ::iterator iter_beg = m_Disk_Vol_ReadFromBinFile.begin();
    multimap<unsigned int,volInfo> ::iterator iter_end = m_Disk_Vol_ReadFromBinFile.end();
	while(iter_beg != iter_end)
	{
        DebugPrintf(SV_LOG_DEBUG,"iter_beg->first  = %d  <==>  iter_beg->second = %s\n",
                        iter_beg->first,iter_beg->second.strVolumeName);
		iter_beg++;
	}
  
	//Added to make sure that we have Taken all mount points in consideration
	//Some strange behaviour observed during mntpt enumeration.sometimes some of the Original volumes remain un-named..
	//To overcome this we'll again enumerate Target volumes.If some mismatch found during volume enumeration,We'll again set the mnt pnt for un mounted vol Guids....	
	if(-1 == startTargetVolumeEnumeration(true,false))
	{
		DebugPrintf(SV_LOG_ERROR,"  startTargetVolumeEnumeration() failed.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}
    Sleep(1000);
	if(-1 == startTargetVolumeEnumeration(false,false))
	{
		DebugPrintf(SV_LOG_ERROR,"  startTargetVolumeEnumeration() failed.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return S_FALSE;
	}			
	//Prepare m_mapVolInfo for checking the volumes with source ones
	if(false == DumpVolumesOrMntPtsExtentOnDisk())
	{
		DebugPrintf(SV_LOG_ERROR,"  DumpVolumesOrMntPtsExtentOnDisk() failed.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return  S_FALSE;
	}

	multimap<int ,volInfo>::iterator iter_mapVolInfo_beg = m_mapVolInfo.begin();
	multimap<int ,volInfo>::iterator iter_mapVolInfo_end = m_mapVolInfo.end();
	while(iter_mapVolInfo_beg != iter_mapVolInfo_end)
	{
        DebugPrintf(SV_LOG_DEBUG,"iter_mapVolInfo_beg->first  = %d  <==>  iter_mapVolInfo_beg->second = %s\n",
                        iter_mapVolInfo_beg->first,iter_mapVolInfo_beg->second.strVolumeName);
		iter_mapVolInfo_beg++;
	}

    //m_Disk_Vol_ReadFromBinFile -> first is disk number from source side
    //m_mapVolInfo -> first is disk number from target side
    //mapOfDiskNumbers -> map of source to target disk numbers
    //so get the volInfo of corresponding disk numbers and obtain the SrcAndTgtmap.
    map<int,int>::iterator iterMapOfDiskNmbrs = mapOfDiskNumbers.begin();
    for(; iterMapOfDiskNmbrs != mapOfDiskNumbers.end(); iterMapOfDiskNmbrs++)
    {
        multimap<unsigned int,volInfo>::iterator iterSrcVol = m_Disk_Vol_ReadFromBinFile.find(iterMapOfDiskNmbrs->first);
        if(iterSrcVol == m_Disk_Vol_ReadFromBinFile.end())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to find source disk number %d in Map read from source.\n",iterMapOfDiskNmbrs->first);
			DebugPrintf( SV_LOG_INFO , "Might be source disk DISK-%d is either Un-Initialised or Un-Partitioned. For now not considering as error.\n", iterMapOfDiskNmbrs->first );
		    continue;
        }
        multimap<unsigned int,volInfo>::iterator iterSrcVolLast = m_Disk_Vol_ReadFromBinFile.upper_bound(iterMapOfDiskNmbrs->first);

        multimap<int,volInfo>::iterator iterTgtVolFirst = m_mapVolInfo.find(iterMapOfDiskNmbrs->second);
        if(iterTgtVolFirst == m_mapVolInfo.end())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to find target disk number %d in Map read from target. Cannot continue.\n",iterMapOfDiskNmbrs->second);
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return  S_FALSE;
        }
        multimap<int,volInfo>::iterator iterTgtVolLast = m_mapVolInfo.upper_bound(iterMapOfDiskNmbrs->second);

        //Now we have volumes for only matching source and target matching disk.
        //Compare the VolInfo And fetch the corresponding volumes and insert to the map.
        //For each source volume, compare with volumes on the target and map them.
        for(; iterSrcVol!=iterSrcVolLast; iterSrcVol++)
        {
            bool bMatchExist = false;
            DebugPrintf(SV_LOG_DEBUG,"iterSrcVol = %s\n",iterSrcVol->second.strVolumeName);
			string strTemp = iterSrcVol->second.strVolumeName;
			if(strTemp.find("RAWVOLUME") != string::npos)
			{
				DebugPrintf(SV_LOG_DEBUG,"Found source volume name as \"RAWVOLUME\", skipping it for protection\n");
				continue;
			}
            multimap<int,volInfo>::iterator iterTgtVol;
            for(iterTgtVol = iterTgtVolFirst; iterTgtVol!=iterTgtVolLast; iterTgtVol++)
            {
                DebugPrintf(SV_LOG_DEBUG,"  iterTgtVol = %s\n",iterTgtVol->second.strVolumeName);
                if((iterSrcVol->second.startingOffset.QuadPart      == iterTgtVol->second.startingOffset.QuadPart)
                    && (iterSrcVol->second.partitionLength.QuadPart == iterTgtVol->second.partitionLength.QuadPart)
                    && (iterSrcVol->second.endingOffset.QuadPart    == iterTgtVol->second.endingOffset.QuadPart))
                {
                    if((strlen(iterSrcVol->second.strVolumeName) != 0)
                        &&(strlen(iterTgtVol->second.strVolumeName) != 0))
                    {
                        Src_Tgt_Vol_map.insert(make_pair(iterSrcVol->second.strVolumeName,
                                                            iterTgtVol->second.strVolumeName));
                        bMatchExist = true;
                        break;
                    }
                }
            }
            if(false == bMatchExist)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the corresponding target volume for source volume - %s.\n",iterSrcVol->second.strVolumeName);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return  S_FALSE;
            }
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hResult;
}


string MasterTarget::FetchHostIPAddress()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strHostIPAddress;
	string strHostName = stGetHostName();
	struct hostent *ptrHostent = NULL;
	if((ptrHostent = gethostbyname(strHostName.c_str()))== NULL)
	{
        DebugPrintf(SV_LOG_ERROR,"GetHostByName for this host(%s) failed with error [%d].\n",strHostName.c_str(),WSAGetLastError());
		return string("");
	}
	string HostFQDNName = ptrHostent->h_name;
	bool bsrc = false;
	if(true == GetSrcIPUsingCxCli(HostFQDNName, strHostIPAddress))
	{
		DebugPrintf(SV_LOG_INFO,"Master Target Name :(%s) ,IP: [%s] ",HostFQDNName.c_str(),strHostIPAddress.c_str());
	}
	//reading the master target host natip or normal ip from Esx.xml file
	else if(readIpAddressfromEsxXmlfile(HostFQDNName,strHostIPAddress,bsrc))
	{
		DebugPrintf(SV_LOG_INFO,"Master Target Name :(%s) ,IP: [%s]\n",HostFQDNName.c_str(),strHostIPAddress.c_str());
	}
	else
	{
		struct in_addr addr;
		addr.s_addr =  *(u_long *) ptrHostent->h_addr_list[0];
		char *address = inet_ntoa(addr);
		strHostIPAddress = string(address);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strHostIPAddress;

}

string MasterTarget::FetchInMageHostId()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strHostInMageId = getInMageHostId();
	if(strHostInMageId.empty())
	{
		string strHostName = stGetHostName();
		struct hostent *ptrHostent = NULL;
		if((ptrHostent = gethostbyname(strHostName.c_str()))== NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"GetHostByName for this host(%s) failed with error [%d].\n",strHostName.c_str(),WSAGetLastError());
			return string("");
		}
		string HostFQDNName = ptrHostent->h_name;
		bool bsrc = false;

		//reading the master target inmage host id from Esx.xml file
		if(readInMageHostIdfromEsxXmlfile(HostFQDNName,strHostInMageId,bsrc))
		{
			DebugPrintf(SV_LOG_INFO,"Master Target Name :(%s) ,InMage HostId: [%s] \n",HostFQDNName.c_str(),strHostInMageId.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Failed to get the InMage Host-id for MT %s\n",HostFQDNName.c_str());
		}
	}
	DebugPrintf(SV_LOG_INFO,"Master Target InMage HostId: [%s] ", strHostInMageId.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strHostInMageId;
}

void MasterTarget::RemoveTrailingCharactersFromVolume(std::string& volumeName)
{  
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	// we need to strip off any leading \, ., ? if they exists
	std::string::size_type idx = volumeName.find_first_not_of("\\.?");
	if (std::string::npos != idx) {
		volumeName.erase(0, idx);
	}

	// strip off trailing :\ or : if they exist
	std::string::size_type len = volumeName.length();
	idx = len;
	if ('\\' == volumeName[len - 1] || (':' == volumeName[len - 1])) 
	{
		--idx;
	} 

	if (':' == volumeName[len - 2]) 
	{
		--idx;
	} 

	if (idx < len) {
		volumeName.erase(idx);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


std::map<string,string>MasterTarget::readPersistedIPaddress()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    string strProcessSvrFile = m_strDataFolderPath;
 	map<string ,string > VM_IP_map;
	string strGuestOsName;
	string strPersistedIPAddressFilePath;
	string strPersistedIPFileName;
	map<string,map<int,int>>::const_iterator iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
	map<string,map<int,int>>::const_iterator iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
	ifstream IpAddressFile;
	string strIPAddress;

	while(iter_beg != iter_end)
	{
		strIPAddress.clear();
		strGuestOsName = iter_beg->first;
		bool bsrcnode = true;
		if(true == GetSrcIPUsingCxCli(strGuestOsName, strIPAddress))
		{
			DebugPrintf(SV_LOG_INFO,"Source Vm Name :(%s) ,IP:[%s]",strGuestOsName.c_str(),strIPAddress.c_str());
			VM_IP_map.insert(make_pair(strGuestOsName,strIPAddress));
		}
		//reading the source vms natip or normal ip from Esx.xml file
		else if(readIpAddressfromEsxXmlfile(strGuestOsName,strIPAddress,bsrcnode))
		{
			//For debug 
			DebugPrintf(SV_LOG_INFO,"Source Vm Name :(%s) ,IP:[%s]",strGuestOsName.c_str(),strIPAddress.c_str());
			VM_IP_map.insert(make_pair(strGuestOsName,strIPAddress));
		}
		else
		{
			strPersistedIPFileName = strGuestOsName + string("_IP_Address.txt");
			strPersistedIPAddressFilePath =  m_strInstallDirectoryPath + string("\\Failover") + string("\\data\\") + strPersistedIPFileName;
			DebugPrintf(SV_LOG_INFO,"IP Address File name = %s.\n",strPersistedIPAddressFilePath.c_str());
			IpAddressFile.open(strPersistedIPAddressFilePath.c_str());
			if(!IpAddressFile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n",strPersistedIPAddressFilePath.c_str());			
				VM_IP_map.clear();
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return VM_IP_map;
			}
			while(IpAddressFile >> strIPAddress)
			{
				VM_IP_map.insert(make_pair(strGuestOsName,strIPAddress));
			}
			IpAddressFile.close();
			IpAddressFile.clear();
		}
		iter_beg++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return VM_IP_map;
}


bool MasterTarget::readInMageHostIdfromEsxXmlfile(string HostName, string &strHostInMageId,bool bsrc)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	
	
	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string xmlhandlenode;
	if(bsrc)
	{
		xmlhandlenode = "SRC_ESX";
	}
	else
	{
		xmlhandlenode = "TARGET_ESX";
	}
	xCur = xGetChildNode(xCur,(const xmlChar*)xmlhandlenode.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format: %s entry not found\n",m_strXmlFile.c_str(),xmlhandlenode.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	xmlNodePtr xPsPtr;

	xPsPtr = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
	if (xPsPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with hostname> entry not found\n", m_strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;	
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"inmage_hostid");
	if( (attr_value_temp == NULL) || (xmlStrlen(attr_value_temp) == 0) )
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with inmage_hostid attribute> entry not found or empty\n", m_strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	strHostInMageId = string((char *)attr_value_temp);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

//reading the source vms and master target host natip or normal ip from Esx.xml file
bool MasterTarget::readIpAddressfromEsxXmlfile(string &HostName,string &HostIp,bool bsrc)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	
	
	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string xmlhandlenode;
	if(bsrc)
	{
		xmlhandlenode = "SRC_ESX";
	}
	else
	{
		xmlhandlenode = "TARGET_ESX";
	}
	xCur = xGetChildNode(xCur,(const xmlChar*)xmlhandlenode.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format: %s entry not found\n",m_strXmlFile.c_str(),xmlhandlenode.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	xmlNodePtr xPsPtr;

	if(bsrc && (m_vConVersion >= 4.0))
	{
		xPsPtr = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
		if (xPsPtr == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with inmage_hostid> entry not found\n", m_strXmlFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;	
		}
	}
	else
	{
		xPsPtr = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xPsPtr == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with hostname> entry not found\n", m_strXmlFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;	
		}
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"natIPAddress");
	if( (attr_value_temp == NULL) || (xmlStrlen(attr_value_temp) == 0) )
	{
		DebugPrintf(SV_LOG_INFO,"We are going to collect the ip_address as the NAT IP address value is NULL or empty.\n");
		attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"ip_address");
		if( (attr_value_temp == NULL) || (xmlStrlen(attr_value_temp) == 0) )
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with ip_address attribute> entry not found or empty\n", m_strXmlFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}

	HostIp = string((char *)attr_value_temp);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

	return true;
}

bool MasterTarget::persistReplicatedTgtVolume(const string &strVolumeName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	char volGuid[MAX_PATH];
	if(0 == GetVolumeNameForVolumeMountPoint(strVolumeName.c_str(),volGuid,MAX_PATH))
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to get GUID for the  volume %s. ErrorCode : [%lu].\n",strVolumeName.c_str(),GetLastError());
		bResult = false;	
	}
	m_mapLockedVolumes.insert(make_pair(strVolumeName,string(volGuid)));
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}
bool MasterTarget::IsGuidAlreadyMounted(const std::string &strVolGuid)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	map<std::string ,std::string>::iterator iterMap;
	iterMap = m_mapLockedVolumes.find(strVolGuid);
	if(iterMap == m_mapLockedVolumes.end()) //Means volume is not already mounted
	{
		DebugPrintf(SV_LOG_DEBUG,"Volume is not mounted\n");
		bResult = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}
//HRESULT MasterTarget::findAllvolumesPresentInAllSource()
//{
//	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
//
//	uTotalNoOfSourceVolumes = 0;
//
//	if(m_vConVersion >= 4.0)
//	{
//		map<string, DisksInfoMap_t>::iterator vmIter;
//		vmIter = m_mapOfHostToDiskInfo.begin();
//		for(;vmIter != m_mapOfHostToDiskInfo.end(); vmIter++)
//		{
//			DisksInfoMapIter_t mapDiskIter = vmIter->second.begin();
//			for(;mapDiskIter != vmIter->second.end(); mapDiskIter++)
//			{
//				uTotalNoOfSourceVolumes += (int)mapDiskIter->second.DiskInfoSub.VolumeCount;
//			}
//		}
//		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//		return S_OK;
//	}
//
//	string strFileName;
//    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
//    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
//	
//	//This outer Loop is for each VM
//	for(; Iter_beg != Iter_end;Iter_beg++)
//	{
//		strFileName = m_strDataFolderPath + (Iter_beg->first) + VOL_CNT_FILE;
//		ifstream in(strFileName.c_str());
//		if (!in.is_open() )
//		{
//			DebugPrintf(SV_LOG_ERROR,"Failed to open %s.\n",strFileName.c_str());
//			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//			return S_FALSE;
//		}
//		string line;
//		try
//		{
//			while(getline(in, line))
//			{
//				stringstream sstream ;
//				sstream << line;
//				int iCount;
//				sstream >> iCount; 
//				uTotalNoOfSourceVolumes += iCount;
//			}
//		}
//		catch(exception& e) 
//		{
//			DebugPrintf(SV_LOG_ERROR,"Exception caught %d",e.what());
//			in.close();
//			return S_FALSE;
//		}
//		catch(...) 
//		{
//			DebugPrintf(SV_LOG_ERROR,"Unknown Error occured ");
//			in.close();
//			return S_FALSE;
//		}
//		in.close();
//	}		
//	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
//	return S_OK;
//}

/* Finds the required Child node and returns its pointer
	Input - Parent Pointer , Value of Child node
	Output - Pointer to required Childe node (returns NULL if not found)
*/
xmlNodePtr MasterTarget::xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	cur = cur->children;
	while(cur != NULL)
	{
		if (!xmlStrcmp(cur->name, xmlEntry))
		{
			//Found the required child node
			break;
		}
		cur = cur->next;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return cur;
}

/* Finds the required Child node with given attribute match and returns its pointer
	Input - Parent Pointer , Value of Child node, Name of Attribute and Value of Attribute
	Output - Pointer to required Childe node (returns NULL if not found)
*/
xmlNodePtr MasterTarget::xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	cur = cur->children;
	while(cur != NULL)
	{
		if (!xmlStrcmp(cur->name, xmlEntry))
		{
			//Found the required child node. Now check the attribute
			xmlChar *attr_value_temp;
			attr_value_temp = xmlGetProp(cur,attrName);
			if ((attr_value_temp != NULL) && (! xmlStrcmp(attr_value_temp,attrValue) ))
			{
				break;
			}
		}
		cur = cur->next;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return cur;
}


//***************************************************************************************
// Finds the required Value for the given property for that node.
//	Input - current node pointer , property name
//	Output - Output value
//***************************************************************************************
bool MasterTarget::xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool rv = true;
	
	xmlNodePtr xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Parameter",(const xmlChar*)"Name", (const xmlChar*)xmlPropertyName.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Parameter with required Name=%s> entry not found\n", xmlPropertyName.c_str());
		rv = false;		
	}
	else
	{
		xmlChar *attr_value_temp = xmlGetProp(xChildNode,(const xmlChar*)"Value");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Parameter Value> entry not found\n");
			rv = false;
		}
		else
			attrValue = std::string((char *)attr_value_temp);
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}

/* Fetch retention details and process server IP from ESX.xml provided by UI.

	Sample Hierarchy of XML file
	<root CX_IP="10.0.15.153">
		<SRC_ESX>
			<host ....>					
				<process_server IP="10.0.0.109" />
				<retention retain_change_MB="0" retain_change_days="7"  log_data_dir="c:\logs" />
				.
				.
			</host>
		</SRC_ESX>
		.
		.
	</root>

	We need to fetch the process_server IP and retention details.
	Output - Vx Job Object with retention details and Process Server IP
	Return Value - true on success and false on failure
*/
bool MasterTarget::xGetValuesFromXML(VxJobXml &VmObj, string FileName, string HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(FileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <SRC_ESX> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if(xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required inmage_hostid> entry not found\n", FileName.c_str());
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required hostname> entry not found\n", FileName.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;			
		}
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"isSourceNatted");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.IsSourceNatted = string("yes");
		}
		else
		{
			VmObj.IsSourceNatted = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <isSourceNatted> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"isTargetNatted");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.IsTargetNatted = string("yes");
		}
		else
		{
			VmObj.IsTargetNatted = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <isTargetNatted> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"secure_src_to_ps");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.SecureSrcToPs = string("yes");
		}
		else
		{
			VmObj.SecureSrcToPs = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <secure_src_to_ps> entry not found\n");
		VmObj.SecureSrcToPs = "no";
	}
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"secure_ps_to_tgt");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.SecurePsToTgt = string("yes");
		}
		else
		{
			VmObj.SecurePsToTgt = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <secure_ps_to_tgt> entry not found\n");
		VmObj.SecurePsToTgt = "no";
	    
	}
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"compression");
	if (attr_value_temp != NULL)
	{
		VmObj.Compression = string((char *)attr_value_temp);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <compression> entry not found\n");
		VmObj.Compression = "";
	}
	
	xmlNodePtr xPsPtr = xGetChildNode(xCur,(const xmlChar*)"process_server");
	if (xPsPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <process_server> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}

	attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"IP");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <process_server IP> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	VmObj.ProcessServerIp = string((char *)attr_value_temp);

	//Get Retention details -
	//	<retention retain_change_MB="0" retain_change_days="7"  log_data_dir="c:\logs" />
	xmlNodePtr xRetentionPtr = xGetChildNode(xCur,(const xmlChar*)"retention");
	if (xRetentionPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retain_change_MB");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention retain_change_MB> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	VmObj.RetainChangeMB = string((char *)attr_value_temp);

	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retain_change_days");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention retain_change_days> entry not found\n");
	    VmObj.RetainChangeDays = "";
	}
	else
		VmObj.RetainChangeDays = string((char *)attr_value_temp);

	if(VmObj.RetainChangeDays.empty())
	{
		attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retain_change_hours");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention retain_change_hours> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		VmObj.RetainChangeHours = string((char *)attr_value_temp);
	}

	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"log_data_dir");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention log_data_dir> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	VmObj.RetentionLogPath = string((char *)attr_value_temp);

	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retention_drive");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention retention_drive> entry not found\n");
		VmObj.RetentionLogVolumeName = "";
	}
	else
		VmObj.RetentionLogVolumeName = string((char *)attr_value_temp);

	VmObj.RetTagType = "File System";

	xmlNodePtr xTimePtr = xGetChildNode(xCur,(const xmlChar*)"time");
	if (xTimePtr == NULL)
	{
		DebugPrintf(SV_LOG_INFO,"The ESX.xml document is not having the \"time\" tag. Sparse retention is not configurable\n");			
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Sparse Retention is selected.\n");	
		for(xmlNodePtr xChild = xTimePtr->children; xChild != NULL; xChild = xChild->next)
		{
			SparseRet_t objSparse;
			objSparse.strTimeWindow = string((char *)xChild->name);
			if(objSparse.strTimeWindow.find("time_interval_window") != string::npos)
			{
				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"unit");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <%s unit> entry not found\n", objSparse.strTimeWindow.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				objSparse.strUnit = string((char *)attr_value_temp);

				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"value");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <%s value> entry not found\n", objSparse.strTimeWindow.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				objSparse.strValue = string((char *)attr_value_temp);

				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"bookmark_count");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <%s bookmark_count> entry not found\n", objSparse.strTimeWindow.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				objSparse.strBookMark = string((char *)attr_value_temp);

				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"range");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_INFO,"The %s does not conatains the \"range\" tag\n", objSparse.strTimeWindow.c_str());
					objSparse.strTimeRange = "";
				}
				else					
					objSparse.strTimeRange = string((char *)attr_value_temp);
				VmObj.sparseTime.push_back(objSparse);
			}
			else
				DebugPrintf(SV_LOG_INFO,"Got the child node for \"time\" as %s\n", objSparse.strTimeWindow.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


bool MasterTarget::xGetHostNameFromHostId(const string strHostId, string& strHostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <SRC_ESX> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}

	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)strHostId.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required inmage_hostid> entry not found\n", m_strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	xmlChar *attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"hostname");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host hostname> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	strHostName = string((char *)attr_value_temp);
	DebugPrintf( SV_LOG_INFO , "Host name corresponding to host id %s is %s.", strHostId.c_str() , strHostName.c_str() );
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


VxJobXml::VxJobXml()
{	
    VmHostName = string("");
    SourceMachineIP = string("");
    TargetMachineIP = string("");
    SourceVolumeName = string("");
    TargetVolumeName = string("");
	ProcessServerIp = string("");
    RetentionLogVolumeName = string("");
	RetentionLogPath = string("");
	RetainChangeMB = string("");
	RetainChangeDays = string("");
	IsSourceNatted = string("");
	IsTargetNatted = string("");
	RetTagType = string("");
	SecureSrcToPs = string("");
	SecurePsToTgt = string("");
	Compression = string("");
	sparseTime.empty();
}


//Checks whether Esx.xml document exists and 
//		returns true  if exists 
//				false if does not exist.
bool MasterTarget::xCheckXmlFileExists()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	

    if(checkIfFileExists(m_strXmlFile))
    {
	    if (NULL == xmlParseFile(m_strXmlFile.c_str())) 
	    {
            DebugPrintf(SV_LOG_ERROR,"Failed to parse the file - %s.\n",m_strXmlFile.c_str());
	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		    return false;
	    }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"File - %s is not found.\n",m_strXmlFile.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

//Compare the files A and B byte by byte for the first noOfBytesToDeal and return true if equal.
bool MasterTarget::bCompareFiles(string strFisrtFile, string strSecondFile, unsigned int noOfBytesToDeal)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = false;

    unsigned char *chFisrtBuf;    
    chFisrtBuf = (unsigned char *)malloc(noOfBytesToDeal * (sizeof(unsigned char)));   

    FILE *firstptr;
	firstptr = fopen(strFisrtFile.c_str(),"rb");
    int inumberOfBytesRead = fread(chFisrtBuf, 1, noOfBytesToDeal, firstptr);
    if(noOfBytesToDeal != inumberOfBytesRead)
    {
        fclose(firstptr);
        free(chFisrtBuf);
        DebugPrintf(SV_LOG_ERROR,"bCompareFiles - Failed to read %s\n",strFisrtFile.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
        return false;
    }
    fclose(firstptr);

	unsigned char *chSecondBuf;
	chSecondBuf = (unsigned char *)malloc(noOfBytesToDeal * (sizeof(unsigned char)));
    FILE *secondptr;
	secondptr = fopen(strSecondFile.c_str(),"rb");
    inumberOfBytesRead = fread(chSecondBuf, 1, noOfBytesToDeal, secondptr);
    if(noOfBytesToDeal != inumberOfBytesRead)
    {
        fclose(secondptr);
        free(chFisrtBuf);
        free(chSecondBuf);
        DebugPrintf(SV_LOG_ERROR,"bCompareFiles - Failed to read %s\n",strSecondFile.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
        return false;
    }
    fclose(secondptr);

    bRetValue = bMyStrCmp(chFisrtBuf, chSecondBuf, noOfBytesToDeal);
    
    free(chFisrtBuf);
    free(chSecondBuf);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}

//Compare the strFirst and strSecond for the first noOfBytesToDeal and return true if equal.
bool MasterTarget::bMyStrCmp(const unsigned char *strFirst, const unsigned char *strSecond, unsigned int noOfBytesToDeal)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = true;

    for (unsigned int i = 0; i < noOfBytesToDeal; i++)
    {       
        if( *strFirst++ != *strSecond++ )
        {
            bRetValue = false;
            break;
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}

//If failover - directly write MBR
//If failback - fetch the MBR of that disk, compare to the to be written MBR and write if doesnt match
//Input - iDiskNumber : Disk number on the target, iBytesToSkip : number of bytes to skip on disk.
//        strInPutMbrFileName : MBR/EBR/GPT File, noOfBytesToDeal : Number of bytes to be written on disk
//Return TRUE if everything is successful.
bool MasterTarget::CompareAndWriteMbr(unsigned int iDiskNumber,LARGE_INTEGER iBytesToSkip,const string &strInPutMbrFileName,unsigned int noOfBytesToDeal)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = false;
    bool bWriteMbr = true;

    if (bIsFailbackEsx)
    {
        string strTempFileName = strInPutMbrFileName + string(".tempmbr");
        //Fetch the MBR on target disk to compare with source
        if( -1 == DumpMbrInFile(iDiskNumber, iBytesToSkip, strTempFileName, noOfBytesToDeal) )
        {
		    DebugPrintf(SV_LOG_DEBUG,"Failed to fetch the MBR/EBR/GPT failed for disk-%d on target.\n",iDiskNumber);
		    bWriteMbr = true;		    
    	}
        else
        {
            if( false == bCompareFiles(strInPutMbrFileName, strTempFileName, noOfBytesToDeal) )
            {
                DebugPrintf(SV_LOG_DEBUG,"MBR/EBR/GPT of source is not matching with target.\n");
                bWriteMbr = true;
            }
            else
            {            
                DebugPrintf(SV_LOG_DEBUG,"MBR/EBR/GPT of source is matching with target.Skipping the write.\n");
                bWriteMbr = false; //Skip WriteMbrToDisk function.
                bRetValue = true;
            }
        }

        if( checkIfFileExists(strTempFileName) ) //Delete tempmbr file which is created above.
            DeleteFile(strTempFileName.c_str());
    }
    
    if(bWriteMbr)
    {
        DebugPrintf(SV_LOG_DEBUG,"Updating Partition table on the target disk-%d.\n",iDiskNumber);
        if(-1 == WriteMbrToDisk(iDiskNumber, iBytesToSkip, strInPutMbrFileName, noOfBytesToDeal))
		{
            DebugPrintf(SV_LOG_ERROR,"WriteMbrToDisk Failed for disk-%d while writing %s.\n",iDiskNumber,strInPutMbrFileName.c_str());
			bRetValue = false;
		}
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Successfully written %s on disk-%d.\n",strInPutMbrFileName.c_str(),iDiskNumber);
            bRetValue = true;
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}

//Separate the volumes which are registered with Cx and which are not
//Input  - map of TargetVolume to its VxJobXml Attributes
//Output - vector of Registered Volumes
//Output - vector of Unregistered Volumes
//Returns true if "All is Well"
bool MasterTarget::SeparateVolumesSet(map<string,VxJobXml> m_mapOfTgtVolToVxJobObj, string strCxUrl,
                                      vector<VxJobXml> &vecRegVolumes,
                                      vector<VxJobXml> &vecUnregVolumes
                                      )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = true;

    map<string,VxJobXml>::iterator iterTemp = m_mapOfTgtVolToVxJobObj.begin();    
    //Check for each volume and add it in corresponding vector.
    for( ; iterTemp != m_mapOfTgtVolToVxJobObj.end(); iterTemp++)
    {
		string strOut;
		//Checks for the target volume is registered or not using CX cli call 8
        if(false == ProcessCxCliCall(strCxUrl,iterTemp->first, true, strOut))
		{
            DebugPrintf(SV_LOG_INFO,"Volume : %s is not registered.\n",iterTemp->first.c_str());
            vecUnregVolumes.push_back(iterTemp->second);
		}
        else
        {
            DebugPrintf(SV_LOG_INFO,"Volume : %s is registered.\n",iterTemp->first.c_str());
            vecRegVolumes.push_back(iterTemp->second);
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}

//Create the XML in new format for 5.5 to support Batch Resync feature
//Input - Vector of VxJob Attributes 
//Input - File Name for the XML to be created
//Returns true if "All is Well"
bool MasterTarget::MakeXmlForCxcli(vector<VxJobXml> vecVxVolInfo, string strVolFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
	//Fetching the plan name from Esx.xml
    string strPlanName;
    string strBatchResyncCount;
    if(false == FetchVxCommonDetails(strPlanName,strBatchResyncCount))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Plan Name required to set the volume pairs.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	
    ofstream outXml(strVolFileName.c_str(),std::ios::out);
    if(!outXml.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",strVolFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    string vx_xml_data_start1 = string("<plan><header><parameters><param name='name'>");
    string vx_xml_data_start2 = string("</param><param name='type'>DR Protection</param><param name='version'>1.1</param></parameters></header><application_options name=\"vContinuum\"><batch_resync>");
    string vx_xml_data_start3 = string("</batch_resync></application_options><failover_protection><replication>");    
    string vx_xml_data_start  = vx_xml_data_start1 + strPlanName + vx_xml_data_start2 + strBatchResyncCount + vx_xml_data_start3;
    outXml<<vx_xml_data_start<<endl; //Write the start part to XML file

    string vx_xml_data_job1, vx_xml_data_job2, vx_xml_data_job3;
	if(m_vConVersion >= 4.0)
	{
		vx_xml_data_job1 = string("<job type=\"vx\"><server_hostid type=\"source\">"); //Add source inmage hostid after this
		vx_xml_data_job2 = string("</server_hostid><server_hostid type=\"target\">");//Add Target inmage host id
		vx_xml_data_job3 = string("</server_hostid><source_device>");//Add source volume
	}
	else
	{
		vx_xml_data_job1 = string("<job type=\"vx\"><server_ip type=\"source\">"); //Add source IP after this
		vx_xml_data_job2 = string("</server_ip><server_ip type=\"target\">");//Add Target IP
		vx_xml_data_job3 = string("</server_ip><source_device>");//Add source volume
	}
    string vx_xml_data_job4 = string("</source_device><target_device>");//Add target volume
    string vx_xml_data_job5 = string("</target_device><process_server_ip use_nat_ip_for_src=\"");//Add strUseNatIpForSrc
    string vx_xml_data_job51 = string("\" use_nat_ip_for_trg=\"");//Add strUseNatIpForTgt
    string vx_xml_data_job52 = string("\">");//Add PS IP
    string vx_xml_data_job6 = string("</process_server_ip><options rawsize_required=\"Yes\" resync_required=\"Yes\" secure_src_to_ps=\"");
	string vx_xml_data_job61 = string("\" secure_ps_to_tgt=\"");
	string vx_xml_data_job62 = string("\" sync_option=\"fast\" compression=\"");
	string vx_xml_data_job63 = string("\"><retention retain_change_writes=\"all\" disk_space_alert_threshold=\"80\" insufficient_disk_space=\"purge_old_logs\" log_data_vol=\""); //Add log data dir volume name
    string vx_xml_data_job7 = string("\" log_data_path=\"");//Add retention log directory path
    string vx_xml_data_job8 = string("\"><space_in_mb>");//Add space in MB
    string vx_xml_data_job9 = string("</space_in_mb><retention_policy><retain_change_time unit=\"days\" value=\"");//Add retention time in days
	string vx_xml_data_job91 = string("</space_in_mb><retention_policy><retain_change_time unit=\"hours\" value=\"");//Add retention time in hours
    string vx_xml_data_job10 = string("\"></retain_change_time>");
	string vx_xml_data_job101 = string("<sparse_enabled>yes</sparse_enabled><time>"); 
	//string vx_xml_data_job102 = string("<time>");
	string vx_xml_data_job11 = string("</time></retention_policy></retention></options></job>");//End of Job
    
    //For each pair, add the job into XML.
    vector<VxJobXml>::iterator iterVxJob = vecVxVolInfo.begin();
    for( ; iterVxJob != vecVxVolInfo.end(); iterVxJob++)
    {
        //Removing the trailing characters from volumes (Ex: C:\ to C)
        string strTempSrcVol = iterVxJob->SourceVolumeName;
        string strTempTgtVol = iterVxJob->TargetVolumeName;
        RemoveTrailingCharactersFromVolume(strTempSrcVol);
        RemoveTrailingCharactersFromVolume(strTempTgtVol);

        string vx_xml_data_job;
		if(m_vConVersion >= 4.0)
			vx_xml_data_job = vx_xml_data_job1 + iterVxJob->SourceInMageHostId + vx_xml_data_job2 + iterVxJob->TargetInMageHostId;
		else
			vx_xml_data_job = vx_xml_data_job1 + iterVxJob->SourceMachineIP + vx_xml_data_job2 + iterVxJob->TargetMachineIP;

		vx_xml_data_job = vx_xml_data_job +
                          vx_xml_data_job3 + strTempSrcVol +
                          vx_xml_data_job4 + strTempTgtVol +
						  vx_xml_data_job5 + iterVxJob->IsSourceNatted +
						  vx_xml_data_job51 + iterVxJob->IsTargetNatted +
                          vx_xml_data_job52 + iterVxJob->ProcessServerIp +
						  vx_xml_data_job6 + iterVxJob->SecureSrcToPs +
						  vx_xml_data_job61 + iterVxJob->SecurePsToTgt +
						  vx_xml_data_job62 + iterVxJob->Compression +
                          vx_xml_data_job63 + iterVxJob->RetentionLogVolumeName +
                          vx_xml_data_job7 + iterVxJob->RetentionLogPath +
                          vx_xml_data_job8 + iterVxJob->RetainChangeMB;

		if(iterVxJob->RetainChangeDays.empty())
		{
			if(iterVxJob->RetainChangeHours.empty())
				vx_xml_data_job = vx_xml_data_job + vx_xml_data_job9 + string("3") + vx_xml_data_job10;
			else
				vx_xml_data_job = vx_xml_data_job + vx_xml_data_job91 + iterVxJob->RetainChangeHours + vx_xml_data_job10;
		}
		else
			vx_xml_data_job = vx_xml_data_job + vx_xml_data_job9 + iterVxJob->RetainChangeDays + vx_xml_data_job10;

		vx_xml_data_job = vx_xml_data_job + vx_xml_data_job101;

		list<SparseRet>::iterator sparseiter = iterVxJob->sparseTime.begin();
		for(; sparseiter != iterVxJob->sparseTime.end(); sparseiter++)
		{
			string vx_xml_data_job12 = string("<") + sparseiter->strTimeWindow + string(" unit=\"") + sparseiter->strUnit + 
										string("\" value=\"") + sparseiter->strValue + 
										string("\" bookmark_count=\"") + sparseiter->strBookMark;
			if(!sparseiter->strTimeRange.empty())
				vx_xml_data_job12 = vx_xml_data_job12 +	string("\" range=\"") + sparseiter->strTimeRange;
			vx_xml_data_job12 = vx_xml_data_job12 + string("\"></") + sparseiter->strTimeWindow + string(">");
			vx_xml_data_job = vx_xml_data_job + vx_xml_data_job12;
		}
		vx_xml_data_job = vx_xml_data_job + string("<application_name customtag=\"\">") + iterVxJob->RetTagType + string("</application_name>") + vx_xml_data_job11;

        outXml<<vx_xml_data_job<<endl;
    }
    
    
    string vx_xml_data_end = string("</replication></failover_protection></plan>");
    outXml<<vx_xml_data_end<<endl;//Write the end part to XML file

    outXml.close();

	//Assign Secure permission to the file
	AssignSecureFilePermission(strVolFileName);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Post the XML to CxCli using cxcli.exe
//Input - Cx Url, XML filename, Cxcli operation number
//Returns true if "All is Well"
bool MasterTarget::PostXmlToCxcli(string strCxUrl, string strXmlFile, int CxcliOperation)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::string strCxCliPath = m_strInstallDirectoryPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("cxcli.exe");

	string strPath;
	if(m_strDirPath.empty())
		strPath = m_strDataFolderPath;
	else
		strPath = m_strDirPath + string("\\");

	std::string strTempFileName = std::string("\"") + strXmlFile + std::string("\"");
	strXmlFile = strTempFileName;
    strTempFileName = std::string("\"") + strCxCliPath + std::string("\"");
    strCxCliPath = strTempFileName;
    string strResultXMLFile = std::string("\"") + strPath + string(RESULT_CXCLI_FILE) + std::string("\"");
    std::string strBatFile = strPath+ std::string("temp_im_cxcli.bat");
    ofstream outfile(strBatFile.c_str(),std::ios::out);	
	if(!outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s for writing.\n",strBatFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    outfile<<strCxCliPath<<" "<<strCxUrl<<" "<<strXmlFile<<" "<<CxcliOperation<<" > "<<strResultXMLFile<<endl;
    outfile.close();

    //create an ACE process to run the generated script
	ACE_Process_Options ace_options; 
    ace_options.handle_inheritance(false);
    ace_options.command_line("%s",strBatFile.c_str());
    ACE_Process_Manager* ace_pm = ACE_Process_Manager::instance();
	if (ace_pm == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get ACE Process Manager instance. Cannot continue further.Exiting...\n");
		return false;
	}

    pid_t pid = ace_pm->spawn(ace_options);
    if (pid == ACE_INVALID_PID)
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to create ACE Process for executing  file : %s\n",strCxCliPath.c_str());
        return false;
    }

	// wait for the process to finish
	ACE_exitcode ace_status = 0;
	pid_t rc = ace_pm->wait(pid, &ace_status);
	DebugPrintf(SV_LOG_INFO,"ACE Exit code status : [%d].\n",ace_status);
	if(ace_status == 0)
	{
		DebugPrintf(SV_LOG_INFO,"Successfully posted the XML file.\n");
	}
	else
	{
		int iError = ACE_OS::last_error();
        DebugPrintf(SV_LOG_ERROR,"Encountered an error in sending xmlFile to server. Error : %s\n",ACE_OS::strerror(iError));
		return false;
	}

    //delete temporary bat file created
    DeleteFile(strBatFile.c_str());

	//Assign Secure permission to the file
	AssignSecureFilePermission(strPath + string(RESULT_CXCLI_FILE));

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Update the Failed_Volumes_Names.txt so that user can set them later manually
//Input - vector of Unregistered volumes 
//Returns true if "All is Well"
bool MasterTarget::UpdateFailedVolumes(vector<VxJobXml> vecUnregVolumes)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = true;
    
    string strFailedVolumesFile = m_strDataFolderPath + string("Failed_Volumes_Names.txt");
    ofstream outFile(strFailedVolumesFile.c_str(), std::ios::app);
    if(!outFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s.\n",strFailedVolumesFile.c_str());
        bRetValue = false;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"-------------------------------------------------\n");
        DebugPrintf(SV_LOG_ERROR,"FAILED VOLUMES LIST - \n\n");
        vector<VxJobXml>::iterator iterVxJob = vecUnregVolumes.begin();
        for( ; iterVxJob != vecUnregVolumes.end(); iterVxJob++)
        {
            DebugPrintf(SV_LOG_ERROR,"%s : %s => %s\n",iterVxJob->VmHostName.c_str(),iterVxJob->SourceVolumeName.c_str(),iterVxJob->TargetVolumeName.c_str());
            if(bRetValue)
                outFile<<iterVxJob->VmHostName<<" : "<<iterVxJob->SourceVolumeName<<" => "<<iterVxJob->TargetVolumeName<<endl;
        }
        DebugPrintf(SV_LOG_ERROR,"-------------------------------------------------\n");
        if(bRetValue)
        {
            outFile<<"-------------------------------------------------\n";
            outFile.close();
        }
    }
    
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}

bool MasterTarget::UpdateDiskInfoFile(string strSrcVmName, map<string, dInfo_t> mapNewPairs, string strExtn)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_INFO, "Creating/Updating %s for machine %s.", strExtn.c_str(), strSrcVmName.c_str() );
	
	string strInfoFile	= m_strDataFolderPath + strSrcVmName + strExtn;
	string strTimestamp = GeneratreUniqueTimeStampString();
    string strBakupFile = "";
	map<string, dInfo_t> mapPairs;
	map<string, dInfo_t>::iterator iterMap;

	double vDlm = 0;
	if(m_mapOfHostIdDlmVersion.find(strSrcVmName) != m_mapOfHostIdDlmVersion.end())
		vDlm = m_mapOfHostIdDlmVersion[strSrcVmName];
	
	if( bIsAdditionOfDisk )
	{
		if ( true==checkIfFileExists(strInfoFile) )
		{
			strBakupFile = m_strDataFolderPath + strSrcVmName + string("_") + strTimestamp + strExtn + string("bak");
		}

		if (!strBakupFile.empty())
		{
			if(false == SVCopyFile(strInfoFile.c_str(),strBakupFile.c_str(),false))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to take the back up of file %s with Error [%lu].\n",strInfoFile.c_str(),GetLastError());
			}

			if(false == ReadDiskInfoFile(strInfoFile,mapPairs))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to fetch the map of disks for VM : %s.\n",strSrcVmName.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			if(strcmpi(strExtn.c_str(),".dpinfo") == 0)
			{
				double vDInfoFile = 1.0;
				iterMap = mapPairs.find("Version");
				//Get the version value if available and remove it from the map.
				if(iterMap != mapPairs.end())
				{
					stringstream ss;
					ss << iterMap->second.DiskNum;
					ss >> vDInfoFile;
					mapPairs.erase("Version");
				}

				if(1.0 == vDInfoFile && vDlm >= 1.2)
				{ 
					// Replace the disk numbers with its corresponding disk signatures.
					iterMap = mapPairs.begin();
					map<string, dInfo_t> mapTgtSignToSrcInfo;
					if(m_mapOfHostToDiskInfo.find(strSrcVmName) == m_mapOfHostToDiskInfo.end())
					{
						DebugPrintf(SV_LOG_ERROR,"Could not find the disk information for the host %s\n",strSrcVmName.c_str());
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					for(; iterMap != mapPairs.end(); iterMap++)
					{
						dInfo_t diskInfo;
						string strDiskName = "\\\\.\\PHYSICALDRIVE" + iterMap->second.DiskNum;
						DisksInfoMapIter_t iterDisks = m_mapOfHostToDiskInfo[strSrcVmName].find(strDiskName);
						if(m_mapOfHostToDiskInfo[strSrcVmName].end() == iterDisks)
						{
							DebugPrintf(SV_LOG_ERROR,"Could not find %s in the disk info of host %s\n",strDiskName.c_str(),strSrcVmName.c_str());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
						diskInfo.DiskSignature = iterDisks->second.DiskSignature;
						diskInfo.DiskDeviceId = iterDisks->second.DiskDeviceId;
						diskInfo.DiskNum = iterMap->second.DiskNum;
						mapTgtSignToSrcInfo[iterMap->first] = diskInfo;
					}
					mapPairs.clear();
					mapPairs.insert(mapTgtSignToSrcInfo.begin(),mapTgtSignToSrcInfo.end());
				}
			}
		}
	}

    //Add the new pairs(mapNewPairs) to mapPairs
    iterMap = mapNewPairs.begin();
    for(; iterMap != mapNewPairs.end(); iterMap++)
        mapPairs.insert(make_pair(iterMap->first,iterMap->second));

    if(!mapPairs.empty())
	{
		//Write the pairs data into Info file
		strInfoFile = m_strDataFolderPath + strSrcVmName + strExtn;
		ofstream outFile(strInfoFile.c_str());
		if(!outFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open File : %s.\n",strInfoFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		if((strcmpi(strExtn.c_str(),".dpinfo") == 0 || strcmpi(strExtn.c_str(),".dsinfo") == 0) && vDlm >= 1.2)
		{
			outFile << "1.1" << "!@!@!" << "" << "!@!@!" << "" << "!@!@!" << "Version" << endl;
		}
		iterMap = mapPairs.begin();
		for(; iterMap != mapPairs.end(); iterMap++)
		{
			DebugPrintf(SV_LOG_INFO,"SrcDiskNum=%s SrcDiskSig=%s(DWORD) SrcDeviceId=%s  TgtDiskSig=%s\n",
									iterMap->second.DiskNum.c_str(),
									iterMap->second.DiskSignature.c_str(), 
									iterMap->second.DiskDeviceId.c_str(), iterMap->first.c_str());
			outFile << iterMap->second.DiskNum;
			outFile << "!@!@!";
			outFile << iterMap->second.DiskSignature;
			outFile << "!@!@!";
			outFile << iterMap->second.DiskDeviceId;
			outFile << "!@!@!";
			outFile << iterMap->first << endl;
		}
		outFile.close();
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Update the Info file so that we can use this during recovery
//Info file inputs - VM Name, map of contents, extension of file to be created.
//Create an Info file with name as "strSrcVmName.strExtn" in Failover\Data
//Content will be written as  following per each line where !@!@! is token
//      mapFirstItem!@!@!mapFirstSecondItem
//If the same file already exists and its addition of disk, append the content.
//Else create a new file or overwrite old one.
bool MasterTarget::UpdateInfoFile(string strSrcVmName, map<string,string> mapNewPairs, string strExtn)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_INFO, "Creating/Updating %s for machine %s.", strExtn.c_str(), strSrcVmName.c_str() );
	
	string strHostVmName = "";
	if( m_vConVersion >= 4.0 )
	{
		if(false == xGetHostNameFromHostId( strSrcVmName, strHostVmName ) )
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the hostname for hostid. %s\n",strSrcVmName.c_str());
		}
	}

	string strInfoFile	= m_strDataFolderPath + strSrcVmName + strExtn;
	string strTimestamp = GeneratreUniqueTimeStampString();
    string strBakupFile = "";
	map<string,string> mapPairs;

	double vDlm = 0;
	if(m_mapOfHostIdDlmVersion.find(strSrcVmName) != m_mapOfHostIdDlmVersion.end())
		vDlm = m_mapOfHostIdDlmVersion[strSrcVmName];
	
	if( bIsAdditionOfDisk )
	{
		do
		{
			if ( true==checkIfFileExists(strInfoFile) )
			{
				strBakupFile = m_strDataFolderPath + strSrcVmName + string("_") + strTimestamp + strExtn + string("bak");
				break;
			}

			if ( strHostVmName.empty() )
			{
				break;
			}

			strInfoFile = m_strDataFolderPath + strHostVmName + strExtn;
			if ( true==checkIfFileExists(strInfoFile) )
			{
				strBakupFile = m_strDataFolderPath + strHostVmName + string("_") + strTimestamp + strExtn + string("bak");
				break;
			}
		}while (0);

		if ( ! strBakupFile.empty() )
		{
			if(false==SVCopyFile(strInfoFile.c_str(),strBakupFile.c_str(),false))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to take the back up of file %s with Error [%lu].\n",strInfoFile.c_str(),GetLastError());
			}
			if(false==ReadFileWith2Values(strInfoFile,mapPairs))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to fetch the map of Replicated Volumes for VM : %s.\n",strSrcVmName.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			if(strcmpi(strExtn.c_str(),".dpinfo") == 0)
			{
				double vDInfoFile = 1.0;
				//Get the version value if available and remove it from the map.
				if(mapPairs.find("Version")!=mapPairs.end())
				{
					stringstream ss;
					ss << mapPairs["Version"].c_str();
					ss >> vDInfoFile;
					mapPairs.erase("Version");
				}

				if(1.0 == vDInfoFile && vDlm >= 1.2)
				{ // Replace the disk numbers with its corresponding disk signatures.
					map<string,string>::iterator iterPair = mapPairs.begin();
					map<string,string> mapSrcTgtSign;
					if(m_mapOfHostToDiskInfo.find(strSrcVmName) == m_mapOfHostToDiskInfo.end())
					{
						DebugPrintf(SV_LOG_ERROR,"Could not find the disk information for the host %s\n",strSrcVmName.c_str());
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					for(; iterPair != mapPairs.end(); iterPair++)
					{
						string strDiskName = "\\\\.\\PHYSICALDRIVE" + iterPair->first;
						DisksInfoMapIter_t iterDisks = m_mapOfHostToDiskInfo[strSrcVmName].find(strDiskName);
						if(m_mapOfHostToDiskInfo[strSrcVmName].end() == iterDisks)
						{
							DebugPrintf(SV_LOG_ERROR,"Could not find %s in the disk info of host %s\n",strDiskName.c_str(),strSrcVmName.c_str());
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
						mapSrcTgtSign[iterDisks->second.DiskSignature] = iterPair->second;
					}
					mapPairs.clear();
					mapPairs.insert(mapSrcTgtSign.begin(),mapSrcTgtSign.end());
				}
			}
		}
	}

    //Add the new pairs(mapNewPairs) to mapPairs
    map<string,string>::iterator iterPair = mapNewPairs.begin();
    for(; iterPair != mapNewPairs.end(); iterPair++)
    {
		if(mapPairs.find(iterPair->first) != mapPairs.end())
			mapPairs.erase(iterPair->first);
		mapPairs.insert(make_pair(iterPair->first,iterPair->second));
	}

    if(!mapPairs.empty())
	{
		//Write the pairs data into Info file
		strInfoFile = m_strDataFolderPath + strSrcVmName + strExtn;
		ofstream outFile(strInfoFile.c_str());
		if(!outFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open File : %s.\n",strInfoFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		if((strcmpi(strExtn.c_str(),".dpinfo") == 0 || strcmpi(strExtn.c_str(),".dsinfo") == 0) && vDlm >= 1.2)
		{
			outFile <<"Version"
					<<"!@!@!"
					<<"1.1" << endl;
		}

		map<string,string>::iterator iterTemp = mapPairs.begin();
		for(; iterTemp!=mapPairs.end(); iterTemp++)
		{
			DebugPrintf(SV_LOG_INFO,"First=%s Second=%s\n",iterTemp->first.c_str(), iterTemp->second.c_str());
			outFile<<iterTemp->first;
			outFile<<"!@!@!";
			outFile<<iterTemp->second<<endl;
		}
		outFile.close();

		//Assign Secure permission
		AssignSecureFilePermission(strInfoFile);
	}

	if( m_vConVersion >= 4.0 )
	{
		if( ! strHostVmName.empty() )
		{
			string strHostInfoFile = m_strDataFolderPath + strHostVmName + strExtn;
			if(false==SVCopyFile(strInfoFile.c_str(),strHostInfoFile.c_str(),false))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s to %s with Error [%lu].\n",strInfoFile.c_str(),strHostInfoFile.c_str(), GetLastError());
			}
			//Assign Secure permission
			AssignSecureFilePermission(strHostInfoFile);
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Parse the Esx.xml and fetch the plan name from it.
//Output - Plan Name and Batch Resync Count
//Returns true if 'All is Well'
//Sample xml -
//  <root CX_IP="10.0.79.50" plan="PlanND" batchresync="3" isSourceNatted="False" isTargetNatted="True">
//  ..........
//  </root>
bool MasterTarget::FetchVxCommonDetails(std::string &strPlanName, 
                                        std::string &strBatchResyncCount)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = true;
    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(m_strXmlFile.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	        bRetValue = false;
            break;
	    }

	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
	        bRetValue = false;
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
	        bRetValue = false;
            break;
	    }

	    xmlChar *xPlan_value;
        xPlan_value = xmlGetProp(xCur,(const xmlChar*)"plan");
        if(NULL == xPlan_value)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root plan> entry not found\n");
            bRetValue = false;
            break;
        }
        else
        {
            strPlanName = string((char *)xPlan_value);
            if(strPlanName.empty())
            {
                DebugPrintf(SV_LOG_ERROR,"Found an empty Plan name in ESX.xml document.\n");
                bRetValue = false;
                break;
            }
        }

        //Go for batchresync which is an attribute in root.
	    xmlChar *xBR_value;
        xBR_value = xmlGetProp(xCur,(const xmlChar*)"batchresync");
        if(NULL == xBR_value)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root batchresync> entry not found\n");
            bRetValue = false;
            break;
        }
        else
        {
            strBatchResyncCount = std::string((char *)xBR_value);
            if(strBatchResyncCount.empty())
            {
                DebugPrintf(SV_LOG_ERROR,"Found an empty Batch Resync value in ESX.xml document.\n");
                bRetValue = false;
                break;
            }
        }
    }while(0);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}

//Parse the ESX.xml and fetch the value for Addition of disk from it.
//Output - Update flag for addition of disk. Default is "true".
//Only if the entry is found and if set to false, this flag will be false
// and "true" for all remaining cases.
//Sample xml -
//  <root CX_IP="10.0.79.50" AdditionOfDisk="true">
//  ..........
//  </root>
void MasterTarget::UpdateAoDFlag()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    bIsAdditionOfDisk = true;//default true to avoid overwriting previous entries.
    
    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(m_strXmlFile.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
            break;
	    }

	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
            break;
	    }

	    xmlChar *xAodFlag;
        xAodFlag = xmlGetProp(xCur,(const xmlChar*)"AdditionOfDisk");
        if(NULL == xAodFlag)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <AdditionOfDisk> entry not found\n");
            break;
        }
        else
        {
            if (! xmlStrcasecmp(xAodFlag,(const xmlChar*)"false")) 
	        {
                bIsAdditionOfDisk = false;
                break;
            }            
        }        
    }while(0);
    DebugPrintf(SV_LOG_INFO,"Addition of Disk = %d.\n",bIsAdditionOfDisk);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void MasterTarget::UpdateArrayBasedSnapshotFlag()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    m_bArrBasedSnapshot = false;//default false to avoid overwriting previous entries.
    
    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(m_strXmlFile.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
            break;
	    }

	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Snapshot.xml is empty. Cannot Proceed further.\n");
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    DebugPrintf(SV_LOG_ERROR,"The snapshot.xml document is not in expected format : <root> entry not found\n");
            break;
	    }

	    xmlChar *xAodFlag;
        xAodFlag = xmlGetProp(xCur,(const xmlChar*)"ArrayBasedDrDrill");
        if(NULL == xAodFlag)
        {
		    DebugPrintf(SV_LOG_ERROR,"The Snapshot.xml document is not in expected format : <ArrayBasedDrDrill> entry not found\n");
            break;
        }
        else
        {
            if (xmlStrcasecmp(xAodFlag,(const xmlChar*)"false")) 
			{
				m_bArrBasedSnapshot = true;
				break;
			}
        }        
    }while(0);
    DebugPrintf(SV_LOG_INFO,"Array based snapshot = %d.\n", m_bArrBasedSnapshot);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void MasterTarget::UpdateV2PFlag()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    m_bV2P = false;//default false to avoid overwriting previous entries.
    
    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(m_strXmlFile.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
            break;
	    }

	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
            break;
	    }

	    xmlChar *xAodFlag;
        xAodFlag = xmlGetProp(xCur,(const xmlChar*)"V2P");
        if(NULL == xAodFlag)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <V2P> entry not found\n");
            break;
        }
        else
        {
            if (xmlStrcasecmp(xAodFlag,(const xmlChar*)"false")) 
			{
				m_bV2P = true;
				break;
			}
        }        
    }while(0);
    DebugPrintf(SV_LOG_INFO,"V2P = %d.\n",m_bV2P);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


/* Searches for volume in the given filename which is expected in following format.
<xml>
<host id='052AD1D7-C93E-1845-B0FDF3AF57F14BB7'>
....
<logical_volumes>
<volume>
  <primary_volume>C:\ESX\w2k3-std01_C</primary_volume>
  ....
</volume>
<volume>
....
</volume>
</logical_volumes>
</host>
</xml>
    Returns true if volume exists , else false.
*/
bool MasterTarget::bSearchVolume(string FileName, string VolumeName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
	bool bResult = false;

	DebugPrintf(SV_LOG_INFO,"Volume to be searched - [%s].\n",VolumeName.c_str());

    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;

	    xDoc = xmlParseFile(FileName.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is not Parsed successfully.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is empty. Cannot Proceed further.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"xml")) 
	    {
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <xml> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"id",(const xmlChar*)m_strHostID.c_str());
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <host with required id> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;			
	    }

	    //go to logical_volumes child.
	    xmlNodePtr xLogVolsPtr = xGetChildNode(xCur,(const xmlChar*)"logical_volumes");
	    if (xLogVolsPtr == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <logical_volumes> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    xmlNodePtr xVolPtr = xLogVolsPtr->children;
	    while(xVolPtr != NULL)
	    {
		    if (!xmlStrcmp(xVolPtr->name, (const xmlChar*)"volume"))
		    {
			    xmlNodePtr xCurPtr = xVolPtr->children;
			    while(xCurPtr != NULL)
			    {
				    if (!xmlStrcmp(xCurPtr->name, (const xmlChar*)"primary_volume"))
				    {
					    xmlChar *key;
					    key = xmlNodeListGetString(xDoc,xCurPtr->children,1);
					    DebugPrintf(SV_LOG_INFO,"key = [%s]\n",key);
					    if(!xmlStrcasecmp(key,(const xmlChar*)VolumeName.c_str()))
					    {
						    DebugPrintf(SV_LOG_INFO,"Found the volume.\n");
						    bResult = true;
					    }
					    xmlFree(key);                    
					    break;
				    }				
				    xCurPtr = xCurPtr->next;				
			    }
			    if(bResult)
				    break;
		    }
		    xVolPtr = xVolPtr->next;
	    }
    }while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/*************************************************************************************************
Searches for HostName and its IP in the given filename which is expected in following format.
<hosts>
<host id='2a90357d-48d2-453c-86b2-fc0e46f32a3b'>
  <name>CENTOS-CS</name>
  <ip_address>10.0.24.2</ip_address>
  <ip_addresses>10.0.24.2</ip_addresses>
  <os_type>LINUX/UNIX</os_type>
  <vx_agent_path>/usr/local/InMage/Vx/</vx_agent_path>
  <fx_agent_path>/usr/local/InMage/Fx</fx_agent_path>
  <vx_patch_history></vx_patch_history>
  <fx_patch_history></fx_patch_history>
  <agent_time_zone>+0530</agent_time_zone>
</host>
................................
................................
</hosts>
*************************************************************************************************/
bool MasterTarget::GetHostIpFromXml(string FileName, string strHost, string &strHostIP)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"Host IP to be searched for Host - [%s].\n",strHost.c_str());

    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;

	    xDoc = xmlParseFile(FileName.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is not Parsed successfully.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is empty. Cannot Proceed further.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"hosts")) 
	    {
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <hosts> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    //we need hostname and host ip from host entry
	    xmlNodePtr xHostPtr = xCur->children;
	    while(xHostPtr != NULL)
	    {
		    if (!xmlStrcmp(xHostPtr->name, (const xmlChar*)"host"))
		    {
				string strHostName;
			    //go to logical_volumes child.
				xmlChar *attr_value_temp;
				xmlNodePtr xChildNode;
				
				xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"id",(const xmlChar*)strHost.c_str());
				if (xChildNode == NULL)
				{
					DebugPrintf(SV_LOG_INFO,"The XML document(%s) is not in expected format : <id> entry not found.\n",FileName.c_str());
					xChildNode = xGetChildNode(xHostPtr,(const xmlChar*)"name");
					if (xChildNode == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <name> entry not found.\n",FileName.c_str());
						bResult = false;
						break;
					}
				}

				attr_value_temp = xmlNodeListGetString(xDoc,xChildNode->children,1);
				strHostName = string((char *)attr_value_temp);

				if(0 == strcmpi(strHost.c_str(), strHostName.c_str()))
				{
					xChildNode = xGetChildNode(xHostPtr,(const xmlChar*)"ip_address");
					if (xChildNode == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <ip_address> entry not found.\n",FileName.c_str());
						bResult = false;
						break;
					}
					attr_value_temp = xmlNodeListGetString(xDoc,xChildNode->children,1);
					strHostIP = string((char *)attr_value_temp);
					if(strHostIP.empty())
					{
						DebugPrintf(SV_LOG_ERROR,"Empty IP value found for host : %s\n", strHostName.c_str());
						bResult = false;
						break;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Got the IP %s for host : %s\n", strHostIP.c_str(), strHostName.c_str());
						break;
					}
				}
		    }
		    xHostPtr = xHostPtr->next;
	    }
    }while(0);

	if(strHostIP.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"CXCLI call had not return the IP value for host : %s \n", strHost.c_str());
		bResult = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

// --------------------------------------------------------------------------
// Clear all the mount points present in a given volume.
// Return true if all mount points are successfully removed else return false.
// --------------------------------------------------------------------------
bool MasterTarget::ClearMountPoints(string DeviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
	
    std::string devname = DeviceName;
    bool rv = true;

    do 
    {
        FormatVolumeNameForMountPoint(devname);

        DebugPrintf(SV_LOG_DEBUG,"Searching Mount Points in Volume - %s to delete.\n",devname.c_str());

        char MountPointBuffer[ SV_MAX_PATH+1 ];
        HANDLE hMountPoint = FindFirstVolumeMountPoint( devname.c_str(), MountPointBuffer, sizeof( MountPointBuffer ) - 1 );

        if( INVALID_HANDLE_VALUE == hMountPoint )
        {
            DebugPrintf(SV_LOG_DEBUG,"No Mount Points are present in this Volume.\n");
            rv = true;
            break;
        }
        else
        {
			//If Mount Points are discovered, unmount them.
			//Even if one fails,proceed with remaining to clear as many as possible,but return as false.
			do
			{
                string strMntPoint = MountPointBuffer;
                strMntPoint = devname + strMntPoint;                   

                if(!DeleteVolumeMountPoint(strMntPoint.c_str()))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to delete the Mount Point - %s. Please delete this manually and rerun the job.\n",strMntPoint.c_str());
					rv = false;
				}
                else
                {
                    DebugPrintf(SV_LOG_DEBUG,"Successfully deleted Mount Point - %s.\n",strMntPoint.c_str());
                }                
			}while(FindNextVolumeMountPoint(hMountPoint,MountPointBuffer,sizeof( MountPointBuffer ) - 1 ));

            DWORD dwErrorValue = GetLastError();
            //if error is ERROR_NO_MORE_FILES, no more mount points else something went wrong.
            if(ERROR_NO_MORE_FILES == dwErrorValue)
            {
                FindVolumeMountPointClose(hMountPoint);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"FindNextVolumeMountPoint failed with Error : [%lu].\n",dwErrorValue);
                rv = false;
            }
            break;
        }
    } while (0);
    
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// --------------------------------------------------------------------------
// Clean all the files/folders starting with "_" present in  ESX folder of system drive
// Return false if fails to delete even one of them.
// But traverse for all i.e. do not break if you fail at one.
// Return true if no files/folder exist or all are deleted successfully.
// --------------------------------------------------------------------------
bool MasterTarget::CleanESXFolder()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
    
    do
    {
        //Fetch Root Volume first.
        CHAR szSysPath[MAX_PATH];
	    CHAR szRootVolumeName[MAX_PATH];	
        //System Directory Path
	    if(0 == GetSystemDirectory(szSysPath, MAX_PATH))
	    {
		    DebugPrintf(SV_LOG_ERROR,"Failed to get System Directory Path. ErrorCode : [%lu].\n",GetLastError());
	        rv = false;
            break;
        }
	    //Get the Root Volume
	    if( 0 == GetVolumePathName(szSysPath,szRootVolumeName,MAX_PATH))
	    {
		    DebugPrintf(SV_LOG_ERROR,"Failed to get the Root volume Name of the System Directory. ErrorCode : [%lu].\n",GetLastError());
            rv = false;
            break;
	    }

		string strSearchFolder;
		if(m_bPhySnapShot)
			strSearchFolder = string(szRootVolumeName) + string(ESX_DIRECTORY) + string("\\") + string(SNAPSHOT_DIRECTORY) + string("\\");
		else
			strSearchFolder = string(szRootVolumeName) + string(ESX_DIRECTORY) + string("\\");
        string strSearchPath = strSearchFolder + string("_*");

        WIN32_FIND_DATA FindFileData;
        HANDLE hFind;
        hFind = FindFirstFile(strSearchPath.c_str(), &FindFileData);

        if (hFind == INVALID_HANDLE_VALUE) 
        {
            DebugPrintf(SV_LOG_DEBUG,"No stale files or folders are present in %s\n",strSearchFolder.c_str());
            rv = true;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Deleting the stale files or folders present in %s\n",strSearchFolder.c_str());
            do
            {
                string strDeleteItem = strSearchFolder + string(FindFileData.cFileName);
                DebugPrintf(SV_LOG_DEBUG,"Deleting %s\n",strDeleteItem.c_str());               
                DWORD File_Attribute;
			    File_Attribute = GetFileAttributes(strDeleteItem.c_str());

                if(FILE_ATTRIBUTE_DIRECTORY & File_Attribute)
                {
                    if(false == RemoveDirectory(strDeleteItem.c_str()))
                    {
						m_PlaceHolder.Properties["DirectoryPath"] = strDeleteItem;
                        DebugPrintf(SV_LOG_ERROR,"Failed to delete the folder %s with error code : [%lu].\n",strDeleteItem.c_str(),GetLastError());
                        DebugPrintf(SV_LOG_ERROR,"Please delete this folder manually and rerun the job.\n");
                        rv = false;
                    }
                }
                else
                {
                    if(false == DeleteFile(strDeleteItem.c_str()))
                    {
						m_PlaceHolder.Properties["DirectoryPath"] = strDeleteItem;
                        DebugPrintf(SV_LOG_ERROR,"Failed to delete the file %s with error code : [%lu].\n",strDeleteItem.c_str(),GetLastError());
                        DebugPrintf(SV_LOG_ERROR,"Please delete this file manually and rerun the job.\n");
                        rv = false;
                    }
                }
            }while(FindNextFile(hFind, &FindFileData));
        }
        FindClose(hFind);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Stop the given service.
//Return true - if service is stopped or does not exist.
//Return false - if service is faile to stop.
bool MasterTarget::StopService(const string serviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = false;

	SC_HANDLE schService;        
    SERVICE_STATUS_PROCESS  serviceStatusProcess;
    SERVICE_STATUS serviceStatus;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;

    do
    {

	    SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schSCManager) 
        {
		    DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager.\n");    
            rv = false;
            break;
        }
	    
	    schService = OpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
        if (NULL != schService)
        { 
		    if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == serviceStatusProcess.dwCurrentState) 
            {
                // stop service
			    if (::ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus))
                {
                    // wait for it to actually stop
					DebugPrintf(SV_LOG_DEBUG, "Waiting for the service \"%s\" to stop.\n", serviceName.c_str());
				    int retry = 1;
                    do
                    {
                        Sleep(1000); 
				    } while(::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState && retry++ <= 600);

				    if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED == serviceStatusProcess.dwCurrentState) 
                    {
						DebugPrintf(SV_LOG_DEBUG, "Successfully stopped the service : \"%s\"\n", serviceName.c_str());
					    ::CloseServiceHandle(schService); 	    
					    rv = true;
                        break;
				    }
                }
		    }
		    else
		    {
				DebugPrintf(SV_LOG_DEBUG, "The service \"%s\" is stopped already.\n", serviceName.c_str());
			    ::CloseServiceHandle(schService); //close the service handle
			    rv = true;
                break;
		    }
		    // Collect the error ahead of other statement ::CloseServiceHandle()
		    dwRet = GetLastError();		    
		    ::CloseServiceHandle(schService); //close the service handle
	    }
    	
	    if( !dwRet )
		    dwRet = GetLastError();       		
    		
	    if(dwRet == ERROR_SERVICE_DOES_NOT_EXIST)
	    {         
			DebugPrintf(SV_LOG_DEBUG, "The service \"%s\" does not exist as an installed service.\n", serviceName.c_str());
		    rv = true;
            break;
	    }

        ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwRet, 0, (LPTSTR)&lpMsgBuf, 0, NULL);	
        if(dwRet)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Failed to stop the service \"%s\" with Error:[%d - %s].\n",serviceName.c_str(),dwRet,(LPCTSTR)lpMsgBuf);
	    }
	    LocalFree(lpMsgBuf); // Free the buffer
    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;	
}

// Function to check if any Vx Pair is not set 
// checking for following string in cxcli ouput.
//      "Failed to Create Volume Replication"
// But we need to change to correct way later.
// Return true only if all pairs are set and 
// Return false in all other cases.
bool MasterTarget::ProcessCxcliOutputForVxPairs()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    string strCxCliOutputXml;
	if(m_strDirPath.empty())
		strCxCliOutputXml = m_strInstallDirectoryPath + string("\\Failover") + string("\\data\\") + string(RESULT_CXCLI_FILE);
	else
		strCxCliOutputXml = m_strDirPath + string("\\") + string(RESULT_CXCLI_FILE);
    string strResultString; //fetch from xml

    //Parse the xml and get the result string.
    xmlDocPtr xDoc;	
	xmlNodePtr xCur;

    if(false == checkIfFileExists(strCxCliOutputXml))
    {
        DebugPrintf(SV_LOG_ERROR,"The file - %s does not exist.\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
    }

	xDoc = xmlParseFile(strCxCliOutputXml.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to parse the XML file - %s\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
        DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"plan")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The %s is not in expected format : <plan> entry not found\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    xCur = xGetChildNode(xCur,(const xmlChar*)"failover_protection");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s is not in expected format : <failover_protection> entry not found\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}

    xCur = xGetChildNode(xCur,(const xmlChar*)"vx");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s is not in expected format : <vx> entry not found\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}

    xmlChar *key;
    key = xmlNodeListGetString(xDoc,xCur->children,1);
    DebugPrintf(SV_LOG_INFO,"The CxCli Output string - \n\n");
    DebugPrintf(SV_LOG_INFO,"%s\n\n",key);
    strResultString = string((char *)key);
    if(string::npos != strResultString.find("Failed to Create Volume Replication") || string::npos != strResultString.find("Failed to create Volume Replication"))
    {
        DebugPrintf(SV_LOG_ERROR,"Some of the Volume pairs are not set.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
    }
	else if(string::npos != strResultString.find("job creation failed"))
    {
        DebugPrintf(SV_LOG_ERROR,"Some of the Volume pairs are not set.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//************************************************************************************
//  Bring all disks online (this is applicable to W2K8 and above)
//  First check if W2K8 or more else return true
//  1. Bring Initialized disks online
//  2. Bring Uninitialized disks online
//  3. Check whether are total number of disks processed in above 2 calls is total number (bug - 15627)
//  Repeat above 3 steps in loop of 3 in case of failure.
//************************************************************************************
bool MasterTarget::BringAllDisksOnline()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool rv = true;

	string strDiskNumber;
	string strDiskPartFile = m_strDataFolderPath + ONLINE_DISK;
	DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

    string strExeName = string(DISKPART_EXE);
	string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

	DebugPrintf(SV_LOG_DEBUG,"Command to online the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

	strExeName = strExeName + strParameterToDiskPartExe;
	string strOurPutFile = m_strDataFolderPath + string("online_output.txt");

	map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
	map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();
    
	for(; Iter_beg != Iter_end;Iter_beg++)
	{
		DebugPrintf(SV_LOG_DEBUG,"\n Onlining disks for VM %s \n",Iter_beg->first.c_str());

		map<int,int>::iterator iterDisk = Iter_beg->second.begin();

		string strSrcHostName = "";
		if (m_mapHostIdToHostName.find(Iter_beg->first) != m_mapHostIdToHostName.end())
			strSrcHostName = m_mapHostIdToHostName.find(Iter_beg->first)->second;

		for(; iterDisk != Iter_beg->second.end(); iterDisk++)
		{
			DebugPrintf(SV_LOG_DEBUG, "Online disk : %d\n", iterDisk->second);
			strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
			strDiskNumber = string("select disk ") + strDiskNumber;

			ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
			if(! outfile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
				m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				m_PlaceHolder.Properties[DISK_NAME] = strDiskNumber;
				UpdateErrorStatusStructure("Disk initialize failed on " + m_strMtHostName + " for " + strSrcHostName, "EA0510");

				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			outfile << strDiskNumber << endl;
			outfile << "online disk" << endl;
			outfile << "attribute disk clear readonly" << endl;
			outfile.flush();
			outfile.close();

            AssignSecureFilePermission(strDiskPartFile);

		    if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
		    {
			    DebugPrintf(SV_LOG_ERROR,"Failed to online the disks for the respective host\n");
				m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				m_PlaceHolder.Properties[DISK_NAME] = strDiskNumber;
				UpdateErrorStatusStructure("Disk initialize failed on " + m_strMtHostName + " for " + strDiskNumber, "EA0510");
			    rv = false;
		    }
		    Sleep(2000);
        }
	}
	
	Sleep(10000);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

// Process inmage_scsi_unit_disks.txt file and compare with disks found in system(m_mapOfDeviceIdAndControllerIds)
// If all required disks are discovered, return 0
// If all disks are not found, then return 1 ( retry can be done here )
// If it fails to fetch map of hostnames to source and target scsi ids, return 2 (need not retry)
int MasterTarget::CheckAllRequiredDisksFound()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int iRV = 0;

    do
    {
        map<string, map<string,string> > mapHostsToScsiIds;
        if(false == GetMapOfHostsToScsiIds(mapHostsToScsiIds)) 
        {
            DebugPrintf(SV_LOG_ERROR, "GetMapOfHostsToScsiIds failed.\n Please check if file  \"%s\" is in required format.\n", SCSI_DISKIDS_FILE_NAME);
            iRV = 2; break;
        }

        // Get vector of all target scsi ids expected from mapHostsToScsiIds
        vector<string> vecTgtScsiIdsReqd;
        map<string, map<string,string> >::iterator iTemp = mapHostsToScsiIds.begin();
        for(; iTemp != mapHostsToScsiIds.end(); iTemp++)
        {
            map<string,string>::iterator iter = iTemp->second.begin();
            for(; iter != iTemp->second.end(); iter++)
            {
                vecTgtScsiIdsReqd.push_back(iter->second);
            }
        }

        // Check whether all vecTgtScsiIdsReqd are present in m_mapOfDeviceIdAndControllerIds
        vector<string>::iterator iterReqd = vecTgtScsiIdsReqd.begin();                
        for(; iterReqd != vecTgtScsiIdsReqd.end(); iterReqd++)
        {            
            DebugPrintf(SV_LOG_DEBUG,"Disk's SCSI ID to be checked - %s\n",iterReqd->c_str());
            map<string,string>::iterator iterFound = m_mapOfDeviceIdAndControllerIds.find(*iterReqd);
            if(iterFound == m_mapOfDeviceIdAndControllerIds.end())
            {
                iRV = 1;
                DebugPrintf(SV_LOG_ERROR,"Disk(%s) is NOT present.\n", iterReqd->c_str());
                break;
            }
        }        
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return iRV;
}

// Get map of hostnames to source and target scsi ids from inmage_scsi_unit_disks.txt file
bool MasterTarget::GetMapOfHostsToScsiIds(map<string, map<string,string> > & mapHostsToScsiIds)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string strCommonPathName;
	if(m_strDirPath.empty())
		strCommonPathName = m_strDataFolderPath;
	else
		strCommonPathName = m_strDirPath + string("\\");
	string strFileName = strCommonPathName + string(SCSI_DISKIDS_FILE_NAME);
    string token("!@!@!");
	size_t index;
    string strVmName;
    string strLineRead;
    map<string,string> mapOfSrcScsiIdsAndTgtScsiIds; //map of src scsi ids to tgt scsi ids    

    if(false == checkIfFileExists(strFileName))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    ifstream inFile(strFileName.c_str());
    do
    {
        if(!inFile.is_open())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFileName.c_str(),GetLastError());
	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            rv = false;
            break;
        }
        while(getline(inFile,strLineRead))
        {            
            if(strLineRead.empty())
			{
				DebugPrintf(SV_LOG_DEBUG, "End of file reached...\n" );
				break;
			}
			
            strVmName= strLineRead;
            DebugPrintf(SV_LOG_DEBUG,"\n");
            DebugPrintf(SV_LOG_DEBUG,"VM Name : %s\n",strVmName.c_str());

            mapOfSrcScsiIdsAndTgtScsiIds.clear();

            if(getline(inFile,strLineRead))
            {
                stringstream sstream ;
                sstream << strLineRead;
                int iCount;
                sstream >> iCount;
                for(int i = 0; i < iCount; i++ )
                {
					string strSrcScsiId;
					string strTgtScsiId;
                    if(getline(inFile,strLineRead))
                    {
						//listOfScsiIds.push_back(strLineRead);
						index = strLineRead.find_first_of(token);
                        if(index != std::string::npos)
		                {
                            strSrcScsiId = strLineRead.substr(0,index);
                            strTgtScsiId = strLineRead.substr(index+(token.length()),strLineRead.length());
                            DebugPrintf(SV_LOG_DEBUG,"strSrcScsiId =  %s  <==>  ",strSrcScsiId.c_str());
                            DebugPrintf(SV_LOG_DEBUG,"strTgtScsiId =  %s\n",strTgtScsiId.c_str());
                            if ((!strSrcScsiId.empty()) && (!strTgtScsiId.empty()))
                                mapOfSrcScsiIdsAndTgtScsiIds.insert(make_pair(strSrcScsiId,strTgtScsiId));
                        }
                        else
		                {
                            DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strFileName.c_str());
			                rv = false;
                            break;
		                }
                    }
                    else
					{
                        inFile.close();
						DebugPrintf(SV_LOG_ERROR,"1.Cant read the disk mapping file.Cannot continue.\n");
                        rv = false;
                        break;
                    }
                }
                if(rv == false)
                    break;
            }
            else
            {
                inFile.close();
				DebugPrintf(SV_LOG_ERROR,"2.Cant read the disk mapping file.Cannot continue..\n");
                rv = false;
                break;
            }
            if(rv == false)
                break;

            mapHostsToScsiIds.insert(make_pair(strVmName, mapOfSrcScsiIdsAndTgtScsiIds));
        }
        inFile.close();
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool MasterTarget::DiscoverAndCollectAllDisks()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    DebugPrintf(SV_LOG_INFO,"**********************************************************\n");
    for(int i=1; i<=10; i++)                  //Trys for 10 times in each 30 secs if the required disks were not found. Bug - 1888563
    {
        if(FALSE == rescanIoBuses())
	    {
		    DebugPrintf(SV_LOG_ERROR,"rescanIoBuses() failed.\n");
		    rv = false; 
            break;
	    }           
        Sleep(10000);                          // 10 secs sleep after rescanning the disks using diskpart

        m_mapOfDeviceIdAndControllerIds.clear();
	    if(S_FALSE == InitCOM())
	    {
		    DebugPrintf(SV_LOG_ERROR,"InitCom() Failed.\n");
		    rv = false;
		    break;
	    }
	    if(S_FALSE == WMIQuery())
	    {
		    DebugPrintf(SV_LOG_ERROR,"WMIQuery() failed.\n");
		    rv = false;
		    break;
	    }
		CoUninitialize();;

		int iRV = m_bCloudMT ? VerifyAllRequiredDisks() : CheckAllRequiredDisksFound();
        if(0 == iRV)
        {
            DebugPrintf(SV_LOG_INFO,"Discovered all required disks on Master Target.\n");
            rv = true; // Found all disks.
            break;  
        }
        else if(1 == iRV)
        {
            DebugPrintf(SV_LOG_ERROR, "Attempt - %d : Failed to discover all disks.\n",i);
            if(i == 10)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to discover all disks on master target in 3 attempts.\n");
                DebugPrintf(SV_LOG_ERROR,"Rescan the disk managament console manually and rerun the job.\n");            
                rv = false;
                break;
            }
			Sleep(30000);                    // 30 secs sleep in case of required disks disocvery fails.
        }
        else 
        {
            DebugPrintf(SV_LOG_ERROR,"CheckAllRequiredDisksFound failed.\n");
		    rv = false;             
		    break;
        }

        DebugPrintf(SV_LOG_DEBUG,"----------------------------------\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"**********************************************************\n");    

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


void MasterTarget::CollectVsnapVols()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	VsnapMgr *Vsnap;
	WinVsnapMgr obj;
    Vsnap=&obj;

	std::vector<VsnapPersistInfo> vsnaps;

	if(!Vsnap->RetrieveVsnapInfo(vsnaps, "all"))
    {
		DebugPrintf(SV_LOG_INFO, "Unable to retrieve the vsnap info from persistent store.\n");
        return;
    }
	if(!vsnaps.empty())
    {
		DebugPrintf(SV_LOG_INFO, "Following is the list of virtual volumes mounted: \n");
		std::vector<VsnapPersistInfo>::iterator iter = vsnaps.begin();
		int nvols = 1;
        for(; iter != vsnaps.end(); ++iter)
        {
			std::string strVolName = iter->devicename;
			strVolName.find_last_of("\\");
			if(strVolName[strVolName.length()-1] != '\\')
			{
				strVolName.append("\\");
			}
			DebugPrintf(SV_LOG_INFO, "%d) %s\n", nvols, strVolName.c_str());
			m_setVsnapVols.insert(strVolName);
			nvols++;
		}
	}
	else
    {
		if(!Vsnap->VsnapQuit())
        {
		    DebugPrintf(SV_LOG_DEBUG, "There are NO virtual volumes mounted\n");
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}
/********************************************************************************************
It will take Source hostname as input and return the source Latest IP from CX.
For this it will do a CX CLI call(number 3). This will give some information for the source.
e.g. 
<host id='2a90357d-48d2-453c-86b2-fc0e46f32a3b'>
  <name>CENTOS-CS</name>
  <ip_address>10.0.24.2</ip_address>
  <ip_addresses>10.0.24.2</ip_addresses>
  <os_type>LINUX/UNIX</os_type>
  <vx_agent_path>/usr/local/InMage/Vx/</vx_agent_path>
  <fx_agent_path>/usr/local/InMage/Fx</fx_agent_path>
  <vx_patch_history></vx_patch_history>
  <fx_patch_history></fx_patch_history>
  <agent_time_zone>+0530</agent_time_zone>
</host>

We will filter out the ip for that host from this information.
**********************************************************************************************/
bool MasterTarget::GetSrcIPUsingCxCli(string strGuestOsName, string &strIPAddress)
{
	bool bRet = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	//Find CX Ip from Local Configurtaor
	string strCxIpAddress = getCxIpAddress();
    if (strCxIpAddress.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX IP Address.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	string strCXHttpPortNumber = getCxHttpPortNumber();
	if (strCXHttpPortNumber.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX HTTP Port Number.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	//Appending HTTP Port number to CX IP.
	strCxIpAddress += string(":") + strCXHttpPortNumber;
    string strCxUrl = m_strCmdHttp + strCxIpAddress + string("/cli/cx_cli_no_upload.php");

	if(false == ProcessCxCliCall(strCxUrl,strGuestOsName, false, strIPAddress))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the IP address using CX CLI for host %s\n",strGuestOsName.c_str());
		bRet = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}


/************************************************************************
This function will List all the host names which are going to protect
in this iteration.
This will list the hosts id from the "inmage_scsi_unti_disk.txt" file
*************************************************************************/
bool MasterTarget::ListHostForProtection()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strFileName;
	if(m_strDirPath.empty())
		strFileName = m_strDataFolderPath + string(SCSI_DISKIDS_FILE_NAME);
	else
		strFileName = m_strDirPath + string("\\") + string(SCSI_DISKIDS_FILE_NAME);
	string strLineRead;

	if(false == checkIfFileExists(strFileName))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    ifstream inFile(strFileName.c_str());
    do
    {
		string strInmHostNameToIdFile = m_strDataFolderPath + string("inmage_hostname_hostid.txt");
        if(!inFile.is_open())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFileName.c_str(),GetLastError());
	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        while(getline(inFile,strLineRead))
        {
        	if(strLineRead.empty())
			{
				DebugPrintf(SV_LOG_INFO, "End of file reached...\n" );
				break;
			}
			
			string strVmName, strVmHostId;
			size_t pos1 = strLineRead.find_first_of("(");
			size_t pos2 = strLineRead.find_last_of(")");
			if(pos1 != string::npos)
			{
				strVmHostId = strLineRead.substr(0, pos1);
				if(pos2 != string::npos)
					strVmName = strLineRead.substr(pos1+1, pos2-(pos1+1));
				else
					strVmName = strLineRead.substr(pos1+1);
			}
			else
				strVmHostId = strLineRead;

			DebugPrintf(SV_LOG_INFO,"VM Name : %s  And Vm InMage ID: %s\n",strVmName.c_str(), strVmHostId.c_str());

			m_listHost.push_back(strVmHostId);
			m_mapHostIdToHostName.insert(make_pair(strVmHostId, strVmName));
			strLineRead.clear();

			if(!m_bVolResize)
			{			
				if(getline(inFile,strLineRead))
				{
					stringstream sstream ;
					sstream << strLineRead;
					int iCount;
					sstream >> iCount;
					for(int i = 0; i < iCount; i++ )
					{
						strLineRead.clear();
						getline(inFile,strLineRead);
					}
				}
				else
				{
					inFile.close();
					DebugPrintf(SV_LOG_ERROR,"Cant read the disk mapping file.Cannot continue.\n");
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				strLineRead.clear();

				//this will Update The hostname and thier corresponding InMage Id. this one only for debug purpose 
				ofstream strInmfile(strInmHostNameToIdFile.c_str(), std::ios::app);
				if(!strInmfile.is_open())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strInmHostNameToIdFile.c_str());
				}
				else
				{
					//update inmage file
					strInmfile<<"VmName=\"";
					strInmfile<<strVmName<<"\"\t";
					strInmfile<<"VmInMageId=\"";
					strInmfile<<strVmHostId<<"\""<<endl;
					strInmfile.close();
				}
			}
		}
		AssignSecureFilePermission(strInmHostNameToIdFile);
		inFile.close();
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


/************************************************************************************
This function will findout the MBR file name and their corresponing MBR location in CX
from the xml file for the inputted host name. Also it collects the machine type.
Input : Host name for which the MBR details are required.
Input : xml file name from where need to fetch the required MBR info.
************************************************************************************/
bool MasterTarget::xGetMbrPathFromXmlFile(string strHostId, string& strMbrPath)
{
	bool bRet = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully. %s\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	do
	{
		//Found the doc. Now process it.
		xCur = xmlDocGetRootElement(xDoc);
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
			bRet = false;
			break;
		}

		if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
		{
			//root is not found
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
			bRet = false;
			break;
		}

		//If you are here root entry is found. Go for SRC_ESX entry.
		xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <SRC_ESX> entry not found\n", m_strXmlFile.c_str());
			bRet = false;
			break;			
		}
		
		//If you are here SRC_ESX entry is found. Go for host entry with required HostName.
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)strHostId.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required inmage_hostid> entry not found\n", m_strXmlFile.c_str());
			bRet = false;
			break;			
		}

		xmlChar *attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"mbr_path");
		if (attr_value_temp != NULL)
		{
			strMbrPath = string((char *)attr_value_temp);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"The %s file is not in expected format : <mbr_path> entry not found\n", m_strXmlFile.c_str());
			bRet = false;
			break;
		}

		//everything is fine till this point
		break;
	}while(0);

	xmlFreeDoc(xDoc);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool MasterTarget::GenerateSrcScsiIdToDiskSigFromXml(const string& strHostname, map<string,string>& mapOfSrcScsiIdsAndTgtScsiIds, map<string,string>& mapOfSrcDiskScsiIdToDiskSig)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	map<string, string>::iterator mapIter;
	map<string, string> mapOfSrcScsiIdAndSrcdevSign;

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully. %s\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <SRC_ESX> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;			
	}
	
	//If you are here SRC_ESX entry is found. Go for host entry with required HostName.
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)strHostname.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required inmage_hostid> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;			
	}

	//Parse through child nodes and if they are "nic" , fetch n/w details.     
    for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
        if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"disk"))
		{
            xmlChar *attr_value_temp;

			string strSrcDiskSign, strSrcDiskId;
			
			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"disk_signature");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk src_devicename> entry not found\n");
	            bRet = false;
				break;
            }
			strSrcDiskSign = string((char *)attr_value_temp);
			
			unsigned long dDiskSig;
			stringstream ss;
			ss << strSrcDiskSign;
			ss >> hex >> dDiskSig;

			stringstream ss1;
			ss1 << dDiskSig;
			ss1 >> strSrcDiskSign;		

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"scsi_mapping_host");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk scsi_mapping_host> entry not found\n");
	            bRet = false;
				break;
            }
			strSrcDiskId = string((char *)attr_value_temp);

			DebugPrintf(SV_LOG_INFO,"From Xml file got the Source Disk Signature : %s (DWORD)<==> Source ScsiId : %s\n", strSrcDiskSign.c_str(), strSrcDiskId.c_str());

			mapOfSrcScsiIdAndSrcdevSign.insert(make_pair(strSrcDiskId, strSrcDiskSign));
		}
	}

	map<string, string>::iterator iter;
	for(mapIter = mapOfSrcScsiIdsAndTgtScsiIds.begin();mapIter != mapOfSrcScsiIdsAndTgtScsiIds.end(); mapIter++)
	{
		iter = mapOfSrcScsiIdAndSrcdevSign.find(mapIter->first);
		if(iter == mapOfSrcScsiIdAndSrcdevSign.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the src_scsi_id %s from mapOfSrcScsiIdAndSrcdevSign\n", mapIter->first.c_str());
			bRet = false;
			break;
		}
		else
		{
			mapOfSrcDiskScsiIdToDiskSig.insert(make_pair(iter->first, iter->second));
		}
	}
	xmlFreeDoc(xDoc);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool MasterTarget::GenerateSrcDiskToTgtScsiIDFromXml(const string& strHostname, map<string,string>& mapOfSrcScsiIdsAndTgtScsiIds, map<string,string>& mapOfSrcDiskNmbrsAndTgtScsiIds)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	map<string, string>::iterator mapIter;
	string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE";

	map<string, string> mapOfSrcScsiIdAndSrcdevFromXml;

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully. %s\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;
	}

	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <SRC_ESX> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;			
	}
	
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)strHostname.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required inmage_hostid> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc);
		return false;			
	}
 
    for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
        if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"disk"))
		{
            xmlChar *attr_value_temp;

			string strSrcDevice, strSrcDeviceId, strSrcDiskSigInHex, strDiskSigInDec, strSrcDiskLayout;

			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"scsi_mapping_host");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk scsi_mapping_host> entry not found\n");
	            bRet = false;
				break;
            }
			strSrcDeviceId = string((char *)attr_value_temp);

			attr_value_temp = NULL;
			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"disk_signature");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk disk_signature> entry not found\n");
            }
			else
				strSrcDiskSigInHex = string((char *)attr_value_temp);

			attr_value_temp = NULL;
			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"disk_layout");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk disk_layout> entry not found\n");
            }
			else
				strSrcDiskLayout = string((char *)attr_value_temp);

			//Finds out the disk signature from xml and matches it with DLMMBR information
			//Considers the disk number which matches with the signature
			if(!strSrcDiskSigInHex.empty() && !strSrcDiskLayout.empty())
			{
				if(strSrcDiskLayout.compare("MBR") == 0)
					//Convert the disk signature to decimal
					strDiskSigInDec = ConvertHexToDec(strSrcDiskSigInHex);
				else
					strDiskSigInDec = strSrcDiskSigInHex;

				DebugPrintf(SV_LOG_DEBUG,"Disk signature from XML file is : %s\n", strDiskSigInDec.c_str());

				if(!strDiskSigInDec.empty())
				{
					map<string, DisksInfoMap_t>::iterator vmIter;
					vmIter = m_mapOfHostToDiskInfo.find(strHostname);
					if(vmIter == m_mapOfHostToDiskInfo.end())
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to find the vm [%s] info from the source mbr information. could not be able to proceed further \n", strHostname.c_str());
						bRet = false;
						break;
					}
					DisksInfoMapIter_t mapDiskIter = vmIter->second.begin();
					for(; mapDiskIter != vmIter->second.end(); mapDiskIter++)
					{
						if(strDiskSigInDec.compare(mapDiskIter->second.DiskSignature) == 0)
						{
							strSrcDevice = mapDiskIter->first;
							strSrcDevice = strSrcDevice.substr(strPhysicalDeviceName.size());
							DebugPrintf(SV_LOG_INFO,"From DLM MBR file got the Source Device : %s\n", strSrcDevice.c_str());
							break;
						}
					}
				}
			}

			//If disk number has not been found by the signature then going with the disk number available in XML
			if(strSrcDevice.empty())
			{
				attr_value_temp = NULL;
				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"src_devicename");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk src_devicename> entry not found\n");
					bRet = false;
					break;
				}

				strSrcDevice = string((char *)attr_value_temp);
				strSrcDevice = strSrcDevice.substr(strPhysicalDeviceName.size());
				DebugPrintf(SV_LOG_INFO,"From Xml file got the Source Device : %s\n", strSrcDevice.c_str());
			}

			DebugPrintf(SV_LOG_INFO,"Source Device : %s <==> Source ScsiId : %s\n", strSrcDevice.c_str(), strSrcDeviceId.c_str());

			mapOfSrcScsiIdAndSrcdevFromXml.insert(make_pair(strSrcDeviceId, strSrcDevice));
		}
	}

	map<string, string>::iterator iter;
	for(mapIter = mapOfSrcScsiIdsAndTgtScsiIds.begin();mapIter != mapOfSrcScsiIdsAndTgtScsiIds.end(); mapIter++)
	{
		iter = mapOfSrcScsiIdAndSrcdevFromXml.find(mapIter->first);
		if(iter == mapOfSrcScsiIdAndSrcdevFromXml.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the src_scsi_id %s from mapOfSrcScsiIdAndSrcdevFromXml\n", mapIter->first.c_str());
			bRet = false;
			break;
		}
		else
		{
			mapOfSrcDiskNmbrsAndTgtScsiIds.insert(make_pair(iter->second, mapIter->second));
		}
	}
	xmlFreeDoc(xDoc);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}


/****************************************************************************************************
This function created the map of source scsi id and their respective disk number for the given host.
This will fetch the information from the host disk info map which are fetched from mbrdata file.
Input: hostname/vmname
output: Map of source scsi id to their respective disk number.
*****************************************************************************************************/
bool MasterTarget::ReadSrcScsiIdAndDiskNmbrFromMap(string strVmName, map<string,string>& mapOfSrcScsiIdsAndSrcDiskNmbrs)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, DisksInfoMap_t>::iterator vmIter;
	vmIter = m_mapOfHostToDiskInfo.find(strVmName);
	if(vmIter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find the Scsi and disk number mapping for vm : %s \n", strVmName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	bool bp2v;
	map<string,bool>::iterator p2vIter= m_MapHostIdMachineType.find(strVmName);
	if(p2vIter != m_MapHostIdMachineType.end())
		bp2v = p2vIter->second;
	else
		bp2v = false;

	bool bClusterNode = false;
	if(IsClusterNode(strVmName))
	{
		bClusterNode = true;
	}
	string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE"; 
	DisksInfoMapIter_t mapDiskIter = vmIter->second.begin();
	for(;mapDiskIter != vmIter->second.end(); mapDiskIter++)
	{
		string strScsiId;
		string strDiskNumber = mapDiskIter->first.substr(strPhysicalDeviceName.size());
		if(bClusterNode)
		{
			strScsiId = mapDiskIter->second.DiskSignature;
		}
		else
		{
			stringstream temp;
			temp << mapDiskIter->second.DiskInfoSub.ScsiId.Channel;
			temp << ":";
			temp << mapDiskIter->second.DiskInfoSub.ScsiId.Target;
			
			if(bp2v)
			{
				temp << ":";
				temp << mapDiskIter->second.DiskInfoSub.ScsiId.Lun;
				temp << ":";
				temp << mapDiskIter->second.DiskInfoSub.ScsiId.Host;
				strScsiId = temp.str();
			}
			else
				strScsiId = temp.str();
		}
		DebugPrintf(SV_LOG_INFO,"strScsiId: %s strDiskNumber: %s\n",strScsiId.c_str(), strDiskNumber.c_str());
		mapOfSrcScsiIdsAndSrcDiskNmbrs.insert(make_pair(strScsiId, strDiskNumber));
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


bool MasterTarget::FillZerosInAttachedSrcDisks()
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();

	for(; Iter_beg != Iter_end;Iter_beg++)
	{
		string  hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_DEBUG,"Virtual Machine Name = %s.\n",hostname.c_str());

		map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
		if(iter == m_mapOfHostToDiskInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
			bResult = false;
			continue;
		}

		map<int, int>::iterator iterDisk = Iter_beg->second.begin();

		for(; iterDisk != Iter_beg->second.end(); iterDisk++)
		{
			DebugPrintf(SV_LOG_DEBUG,"Filling zeros in disk : %d\n", iterDisk->second);

			char tgtDisk[256];
			inm_sprintf_s(tgtDisk, ARRAYSIZE(tgtDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->second);

			
			char srcDisk[256];			
			inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
			DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
			if(iterDiskInfo == iter->second.end())
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
				bResult = false;
				continue;
			}

			SV_UCHAR * chDiskData;
			unsigned int numOfBytes;
			if((GPT == iterDiskInfo->second.DiskInfoSub.FormatType) &&  (DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type))
			{
				if(DLM_ERR_SUCCESS == OnlineOrOfflineDisk(string(tgtDisk), true))
				{
					DebugPrintf(SV_LOG_DEBUG,"Successfully onlined the GTP dynamic disk %s\n", tgtDisk);
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG,"Filed to online the GTP dynamic disk %s\n", tgtDisk);
				}

				Sleep(2000); //Sleeping for 2 secs.

				DebugPrintf(SV_LOG_DEBUG,"Cleaning disk : %d\n", iterDisk->second);
				string strDiskPartFile = m_strDataFolderPath + CLEAN_DISK;
				DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

				ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
				if(! outfile.is_open())
				{
					DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
					bResult = false;
					continue;
				}

				string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
				strDiskNumber = string("select disk ") + strDiskNumber; 
				outfile << strDiskNumber << endl;
				outfile << "clean" << endl;
				outfile << "offline disk" << endl;
				outfile << "attribute disk clear readonly" << endl;
				outfile.flush();
				outfile.close();

				AssignSecureFilePermission(strDiskPartFile);

				string strExeName = string(DISKPART_EXE);
				string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

				DebugPrintf(SV_LOG_DEBUG,"Command to clean the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

				strExeName = strExeName + strParameterToDiskPartExe;
				string strOurPutFile = m_strDataFolderPath + string("clean_output.txt");
				if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to clean the disk %d of host %s\n", iterDisk->second, hostname.c_str());
					bResult = false;
				}
				Sleep(2000); //Sleeping for 5 secs.
				continue;
			}
			else if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType)
			{
				numOfBytes = 17408;
				chDiskData = new SV_UCHAR[17408];
				ZeroMemory(chDiskData, 17408);
			}
			else
			{
				numOfBytes = 512;
				chDiskData = new SV_UCHAR[512];
				ZeroMemory(chDiskData, 512);
			}
			if(false == UpdateDiskWithMbrData(tgtDisk, 0, chDiskData, numOfBytes))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to zero the initial %d bytes of disk %s\n",numOfBytes, tgtDisk);
				bResult = false;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully Zeroed the initial %d bytes of disk %s\n",numOfBytes, tgtDisk);
			}
			delete [] chDiskData;			
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::ClearReadOnlyAttributeOfDisk()
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();

	for(; Iter_beg != Iter_end;Iter_beg++)
	{
		string  hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_DEBUG,"Virtual Machine Name = %s.\n",hostname.c_str());

		map<int, int>::iterator iterDisk = Iter_beg->second.begin();

		for(; iterDisk != Iter_beg->second.end(); iterDisk++)
		{
			DebugPrintf(SV_LOG_DEBUG,"Clearing the readonly disk attribute : %d\n", iterDisk->second);
			string strDiskPartFile = m_strDataFolderPath + CLEAR_DISK;
			DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

			ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
			if(! outfile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
			strDiskNumber = string("select disk ") + strDiskNumber; 
			outfile << strDiskNumber << endl;
			outfile << "attribute disk clear readonly" << endl;
			outfile.flush();
			outfile.close();

			AssignSecureFilePermission(strDiskPartFile);

			string strExeName = string(DISKPART_EXE);
			string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

			DebugPrintf(SV_LOG_DEBUG,"Command to clean the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

			strExeName = strExeName + strParameterToDiskPartExe;
			string strOurPutFile = m_strDataFolderPath + string("clear_output.txt");
			if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clean the disk %d of host %s\n", iterDisk->second, hostname.c_str());
				bResult = false;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::CleanAttachedSrcDisks(bool bOffline)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();

	string strDiskPartFile = m_strDataFolderPath + CLEAN_DISK;
	DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

	for(; Iter_beg != Iter_end;Iter_beg++)
	{
		string  hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_DEBUG,"Virtual Machine Name = %s.\n",hostname.c_str());

		map<int, int>::iterator iterDisk = Iter_beg->second.begin();

		for(; iterDisk != Iter_beg->second.end(); iterDisk++)
		{
			DebugPrintf(SV_LOG_DEBUG,"Cleaning disk : %d\n", iterDisk->second);
			ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
			if(! outfile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
			strDiskNumber = string("select disk ") + strDiskNumber; 
			outfile << strDiskNumber << endl;
			outfile << "clean" << endl;
			if(bOffline)
			{
				outfile << "offline disk" << endl;
				outfile << "attribute disk clear readonly" << endl;
			}
			outfile.flush();
			outfile.close();

			AssignSecureFilePermission(strDiskPartFile);

			string strExeName = string(DISKPART_EXE);
			string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

			DebugPrintf(SV_LOG_DEBUG,"Command to clean the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

			strExeName = strExeName + strParameterToDiskPartExe;
			string strOurPutFile = m_strDataFolderPath + string("clean_output.txt");
			if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clean the disk %d of host %s\n", iterDisk->second, hostname.c_str());
				bResult = false;
			}
			Sleep(5000); //Sleeping for 5 secs.
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::CleanSrcGptDynDisks()
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();

	for(; Iter_beg != Iter_end;Iter_beg++)
	{
		string  hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_DEBUG,"Virtual Machine Name = %s.\n",hostname.c_str());

		map<int, int>::iterator iterDisk = Iter_beg->second.begin();

		map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
		if(iter == m_mapOfHostToDiskInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		for(; iterDisk != Iter_beg->second.end(); iterDisk++)
		{

			char srcDisk[256];
			inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
			DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
			if(iterDiskInfo == iter->second.end())
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
			{
				DebugPrintf(SV_LOG_DEBUG,"Cleaning disk : %d\n", iterDisk->second);
				string strDiskPartFile = m_strDataFolderPath + string("gpt_dyn_") + CLEAN_DISK;
				DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

				ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
				if(! outfile.is_open())
				{
					DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}

				string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
				strDiskNumber = string("select disk ") + strDiskNumber; 
				outfile << strDiskNumber << endl;
				outfile << "clean" << endl;
				//outfile << "exit" << endl;
				outfile.flush();
				outfile.close();

				AssignSecureFilePermission(strDiskPartFile);

				string strExeName = string(DISKPART_EXE);
				string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

				DebugPrintf(SV_LOG_DEBUG,"Command to clean the disks: %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

				strExeName = strExeName + strParameterToDiskPartExe;
				string strOurPutFile = m_strDataFolderPath + string("gpt_dyn_clean_output.txt");
				if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to clean the disk %d of host %s\n", iterDisk->second, hostname.c_str());
					bResult = false;
				}
				Sleep(5000); //Sleeping for 5 secs.
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}



bool MasterTarget::InitialiseDisks(std::string& strErrorMsg)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();

	for(; Iter_beg != Iter_end;Iter_beg++)
	{		
		string hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_DEBUG,"Virtual Machine Name = %s.\n",hostname.c_str());

		string strSrcHostName = "";
		if (m_mapHostIdToHostName.find(hostname) != m_mapHostIdToHostName.end())
			strSrcHostName = m_mapHostIdToHostName.find(hostname)->second;

		map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
		if(iter == m_mapOfHostToDiskInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
			m_statInfo.ErrorMsg = "Internal error occured at Master Target " + m_strMtHostName + "Disk initialize failed for Source machine " + strSrcHostName;
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		map<int, int>::iterator iterDisk = Iter_beg->second.begin();

		for(; iterDisk != Iter_beg->second.end(); iterDisk++)
		{
			bool bGpt = false;
			bool bDyn = false;
			char srcDisk[256];
			char driveName[256];

			inm_sprintf_s(driveName, ARRAYSIZE(driveName), "\\\\.\\PHYSICALDRIVE%d", iterDisk->second);
			inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);

			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[DISK_NAME] = string(driveName);

			DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
			if(iterDiskInfo == iter->second.end())
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());				
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

				strErrorMsg = "Disk initialize failed on " + m_strMtHostName + " for " + strSrcHostName;
				return false;
			}

            Sleep(1000); //sleeping 1000 ms for regenration of uniq random id for disk signature. It is MUST TO have sleep of 1 seconds to get uniq id
			
			CREATE_DISK dsk;
			DWORD nDiskSign;
			srand(time(NULL));

			nDiskSign = rand() % 100000000 + 10000001;			
			dsk.PartitionStyle = PARTITION_STYLE_MBR;  
			dsk.Mbr.Signature = nDiskSign;
			DebugPrintf(SV_LOG_DEBUG,"Generated MBR disk signature : %lu \n", dsk.Mbr.Signature);

			if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType)
			{
				bGpt = true;
			}
			if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
			{
				bDyn = true;
			}

			DebugPrintf(SV_LOG_DEBUG, "Drive to initialize is %s.\n", driveName);
			strErrorMsg = "Disk initialize failed on " + m_strMtHostName + " for " + string(driveName);

			HANDLE diskHandle = SVCreateFile(driveName,GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);	
			if(diskHandle == INVALID_HANDLE_VALUE)
			{
				DebugPrintf(SV_LOG_ERROR,"Error in opening device. Error Code : [%lu].\n",GetLastError());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}			

			DWORD lpBytesReturned;
			bool flag = DeviceIoControl((HANDLE) diskHandle, IOCTL_DISK_CREATE_DISK, &dsk, sizeof(dsk), NULL, 0, (LPDWORD)&lpBytesReturned, NULL); 
			if (!bResult)
			{ 
				DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_CREATE_DISK Failed with Error Code : [%lu].\n",GetLastError());
				CloseHandle(diskHandle);				
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			
			flag = DeviceIoControl((HANDLE) diskHandle, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, (LPDWORD)&lpBytesReturned, (LPOVERLAPPED)NULL);
			if(!flag)
			{
				DebugPrintf(SV_LOG_ERROR,"IOCTL_DISK_UPDATE_PROPERTIES Failed with Error Code : [%lu].\n",GetLastError());
				CloseHandle(diskHandle);
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			CloseHandle(diskHandle);

			if(bGpt)
			{
				if(false == ConvertToMbrToGpt(iterDisk->second, bDyn))
				{
					DebugPrintf(SV_LOG_DEBUG,"Failed to convert the MBR partition disk to GPT partition style\n");
				}
				if(bDyn)
				{
					//Need to write code for collecting 2 KB data in case fo GPT dynamic disk
					if(false == CollectGPTData(hostname, (unsigned int)iterDisk->second))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to collect the 2KB GPT initial data for disk = %d\n",iterDisk->second);
						strErrorMsg = "Disk initialize failed on " + m_strMtHostName + " for " + string(driveName);
						bResult = false;
					}
				}
			}
			m_PlaceHolder.clear();
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::OfflineDiskAndCollectGptDynamicInfo()
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string,map<int,int>>::iterator iterHost;
	map<string,map<int,int>>::iterator Iter_beg = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
    map<string,map<int,int>>::iterator Iter_end = m_mapOfVmToSrcAndTgtDiskNmbrs.end();

	for(; Iter_beg != Iter_end;Iter_beg++)
	{
		map<int, int> mapDiskNumbers;
		map<int, int>::iterator iterDisk;
		string hostname = Iter_beg->first;
		DebugPrintf(SV_LOG_INFO,"Virtual Machine Name = %s.\n",hostname.c_str());

		map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
		if(iter == m_mapOfHostToDiskInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		if(bIsAdditionOfDisk)
		{
			map<string, bool>::iterator iterDyn = m_mapHostIdDynAddDisk.find(hostname);
			if(iterDyn != m_mapHostIdDynAddDisk.end())
			{
				if(false == iterDyn->second)
				{
					DebugPrintf(SV_LOG_INFO,"No dynamic disk is added for this host = %s.\n",hostname.c_str());
					continue;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in m_mapHostIdDynAddDisk Map\n", hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			iterHost = m_mapOfHostToDiskNumMap.find(hostname);
			if(iterHost != m_mapOfHostToDiskNumMap.end())
			{
				mapDiskNumbers = iterHost->second;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in m_mapOfHostToDiskNumMap Map\n", hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
		}
		else
		{
			mapDiskNumbers = Iter_beg->second;
		}

		iterDisk = mapDiskNumbers.begin();

		for(; iterDisk != mapDiskNumbers.end(); iterDisk++)
		{
			char srcDisk[256];
			inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);

			char tgtDisk[256];
			inm_sprintf_s(tgtDisk, ARRAYSIZE(tgtDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->second);

			DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
			if(iterDiskInfo == iter->second.end())
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
			{
				//Need to write code for collecting 2 KB data in case fo GPT dynamic disk
				if(false == CollectGPTData(hostname, (unsigned int)iterDisk->second))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to collect the 2KB GPT initial data for disk = %d\n",iterDisk->second);
					bResult = false;
				}
			}
			if(DLM_ERR_SUCCESS == OnlineOrOfflineDisk(string(tgtDisk), false))
			{
				DebugPrintf(SV_LOG_INFO,"Successfully offlined the disk %s\n", tgtDisk);
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to offline the disk %s\n", tgtDisk);
				bResult = false;
			}

			Sleep(2000); //Sleeping for 2 secs per disk.
		}
	}
	Sleep(10000); //Sleeping for 10 secs after completion offfline of all the disks.

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::CollectGPTData(string strHostName, unsigned int nDiskNum)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strDiskNmbr;
	stringstream strm;
	strm << nDiskNum;
	strm >> strDiskNmbr;

	string strGptFileName;
	if(m_strDirPath.empty())
		strGptFileName = m_strDataFolderPath + string("\\") + strHostName + string("_") + string("Disk_") + strDiskNmbr + (GPT_FILE_NAME);
	else
		strGptFileName = m_strDirPath + string("\\") + strHostName + string("_") + string("Disk_") + strDiskNmbr + (GPT_FILE_NAME);
					
	DebugPrintf(SV_LOG_INFO,"GPT file name is = %s\n",strGptFileName.c_str());
	
	LARGE_INTEGER nBytesToSkip;
	nBytesToSkip.HighPart = 0;
	nBytesToSkip.LowPart = 0;

	if(-1 == DumpMbrInFile(nDiskNum, nBytesToSkip, strGptFileName, 2048))
	{
		DebugPrintf(SV_LOG_ERROR,"Error: Dumping of GPT failed for disk = %d\n",nDiskNum);
		bResult = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}




/****************************************************************************************************
This function Will update the MBR/GPT/LDM/EBR to their corresponfing target disks for the given host.
Input : Hostname\Vmname
Input : Map of source and target disk number
*****************************************************************************************************/
bool MasterTarget::UpdateMbrOrGptInTgtDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	bool bResult=true;

	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<int, int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();

	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{	
		char srcDisk[256], tgtDisk[256];
		inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
		inm_sprintf_s(tgtDisk, ARRAYSIZE(tgtDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->second);

		DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
		if(iterDiskInfo == iter->second.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		
		SV_LONGLONG tgtDiskSize; //Need to get the exact size for updating LDM in case of P2V
		if(true == GetDiskSize(tgtDisk, tgtDiskSize))
		{
			DebugPrintf(SV_LOG_INFO,"Source disk : %s --> size : %lld\n", srcDisk, iterDiskInfo->second.DiskInfoSub.Size);
			DebugPrintf(SV_LOG_INFO,"Target disk : %s --> size : %lld\n", tgtDisk, tgtDiskSize);
			iterDiskInfo->second.DiskInfoSub.Size = tgtDiskSize;
		}

		if (tgtDiskSize < iterDiskInfo->second.DiskInfoSub.Size)
		{
			DebugPrintf(SV_LOG_ERROR, "Source disk size is higher then target disk size. Dumping disklayout failed.\n");
			DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
			return false;
		}

		DLM_ERROR_CODE RetVal = RestoreDisk(tgtDisk, srcDisk, iterDiskInfo->second);
		if(DLM_ERR_SUCCESS != RetVal)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Update MBR (or) GPT for disk %s of host %s. Error Code - %u\n", srcDisk, hostname.c_str(), RetVal);
			//Clearing the scsi 3 reservation if the respective host is part of cluster
			if(IsClusterNode(hostname))
			{
				if (false == ClrMscsScsiRsrv(iterDisk->second))
				{
					DebugPrintf(SV_LOG_ERROR,"Scsi reseravtion clearance failed on disk - %s, for host - %s\n", tgtDisk, hostname.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				DLM_ERROR_CODE RetVal = RestoreDisk(tgtDisk, srcDisk, iterDiskInfo->second);
				if(DLM_ERR_SUCCESS != RetVal)
				{
					DebugPrintf(SV_LOG_ERROR,"Retry of updating MBR (or) GPT failed for disk %s of host %s after clearing the scsi reservation. Error Code - %u\n", srcDisk, hostname.c_str(), RetVal);
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
		}
		
		if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			DebugPrintf(SV_LOG_DEBUG,"Updating Initially collected 2KB data in dynamic GPT disk - %s.\n",tgtDisk);

			string strDiskNum;
			stringstream temp;
			temp << iterDisk->second;
			temp >> strDiskNum;

			string strInPutGptFileName;

			if(m_strDirPath.empty())
				strInPutGptFileName = m_strDataFolderPath + string("\\") + hostname + string("_") + string("Disk_") + strDiskNum + (GPT_FILE_NAME);
			else
				strInPutGptFileName = m_strDirPath + string("\\") + hostname + string("_") + string("Disk_") + strDiskNum + (GPT_FILE_NAME);
			
			DebugPrintf(SV_LOG_DEBUG,"Considering the File %s for updating  2KB data in dynamic GPT disk - %s.\n", strInPutGptFileName.c_str(), tgtDisk);

			LARGE_INTEGER nNumOfBytesToSkip;
			nNumOfBytesToSkip.HighPart = 0;
			nNumOfBytesToSkip.LowPart = 0;			

			if(-1 == WriteMbrToDisk((unsigned int)iterDisk->second, nNumOfBytesToSkip, strInPutGptFileName, 2048))
			{
				DebugPrintf(SV_LOG_ERROR,"WriteMbrToDisk Failed for disk-%d while writing %s.\n",iterDisk->second, strInPutGptFileName.c_str());
				bResult = false;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"Successfully written %s on disk-%d.\n",strInPutGptFileName.c_str(), iterDisk->second);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}



bool MasterTarget::OnlineVolumeResizeTargetDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<int, int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();

	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{
		string strDiskPartFile = m_strDataFolderPath + "OnlineDisk.txt";
		DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

		ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
		if(! outfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
		strDiskNumber = string("select disk ") + strDiskNumber; 
		outfile << strDiskNumber << endl;
		outfile << "online disk" << endl;
		outfile.flush();
		outfile.close();

		AssignSecureFilePermission(strDiskPartFile);

		string strExeName = string(DISKPART_EXE);
		string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

		DebugPrintf(SV_LOG_DEBUG,"Command to online disk : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

		strExeName = strExeName + strParameterToDiskPartExe;
		string strOurPutFile = m_strDataFolderPath + string("Online_output.txt");
		if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to online the disks %d of host %s\n", iterDisk->second, hostname.c_str());
			bResult = false;
		}
		
		Sleep(2000); //Sleeping for 2 secs.
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/************************************************************************************************
This function will offline the basic disk and onlines it again for the given host.
Input : Hostname\Vmname
Input : Map of source and target disk number
*************************************************************************************************/
bool MasterTarget::OfflineOnlineBasicDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<int, int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();

	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{
		char srcDisk[256];
		inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
		DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
		if(iterDiskInfo == iter->second.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		if(BASIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			DebugPrintf(SV_LOG_DEBUG, "Got source Basic disk %s of host %s to offline and online for which target disk is %d\n", srcDisk, hostname.c_str(), iterDisk->second);

			string strDiskPartFile = m_strDataFolderPath + "OnlineOffline.txt";
			DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

			ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
			if(! outfile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
			strDiskNumber = string("select disk ") + strDiskNumber; 
			outfile << strDiskNumber << endl;
			outfile << "offline disk" << endl;
			outfile.flush();
			outfile.close();

			AssignSecureFilePermission(strDiskPartFile);

			string strExeName = string(DISKPART_EXE);
			string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

			DebugPrintf(SV_LOG_DEBUG,"Command to offline online basic disks : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

			strExeName = strExeName + strParameterToDiskPartExe;
			string strOurPutFile = m_strDataFolderPath + string("OfflineOnline_output.txt");
			if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to offline the basic disks %d of host %s and source disk %s\n", iterDisk->second, hostname.c_str(), srcDisk);
				bResult = false;
			}
			ofstream outfile1(strDiskPartFile.c_str(), ios_base::out);
			if(! outfile1.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			outfile1 << strDiskNumber << endl;
			outfile1 << "online disk" << endl;
			outfile1.flush();
			outfile1.close();

			AssignSecureFilePermission(strDiskPartFile);

			if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to online the basic disks %d of host %s and source disk %s\n", iterDisk->second, hostname.c_str(), srcDisk);
				bResult = false;
			}
		}
	}
	Sleep(15000); //To update the disks of all VM's

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/****************************************************************************************************
This function Will create the map of source disk and their volumes info map for the given host.
Input : Hostname\Vmname
Input : Map of source and target disk number
*****************************************************************************************************/
bool MasterTarget::CreateMapOfSrcDisktoVolInfo(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string clusName;
	bool bClusNode = IsClusterNode(hostname);
	if(bClusNode) clusName = GetNodeClusterName(hostname);

	double dlmVersion = 0;
	if(m_mapOfHostIdDlmVersion.find(hostname) != m_mapOfHostIdDlmVersion.end())
		dlmVersion = m_mapOfHostIdDlmVersion[hostname];

	map<int, int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();

	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{
		char srcDisk[256];
		inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
		DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
		if(iterDiskInfo == iter->second.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		if( bClusNode && dlmVersion>=1.2 && IsClusterDisk(hostname,iterDiskInfo->second.DiskSignature) )
		{
			if( m_mapClusNameSrcClusDiskSignature.find(clusName) == m_mapClusNameSrcClusDiskSignature.end() ||
				m_mapClusNameSrcClusDiskSignature[clusName].find(iterDiskInfo->second.DiskSignature) == m_mapClusNameSrcClusDiskSignature[clusName].end())
			{
				DebugPrintf(SV_LOG_DEBUG,"Collecting volumes of Cluster disk with disk-signature: %s\n",iterDiskInfo->second.DiskSignature);
				if(m_mapClusNameSrcClusDiskSignature.find(clusName) == m_mapClusNameSrcClusDiskSignature.end())
				{
					set<string> setClusDiskSign;
					setClusDiskSign.insert(iterDiskInfo->second.DiskSignature);
					m_mapClusNameSrcClusDiskSignature[clusName] = setClusDiskSign;
				}
				else
				{
					m_mapClusNameSrcClusDiskSignature[clusName].insert(iterDiskInfo->second.DiskSignature);
				}
				if(iterDiskInfo->second.VolumesInfo.empty())
					if(GetClusDiskWithPartitionInfoFromMap(hostname,iterDiskInfo))
					{
						DebugPrintf(SV_LOG_DEBUG,"Successfuly identified cluster disk with partition info. %s of %s\n",srcDisk,hostname.c_str());
					}
					else
					{
						DebugPrintf(SV_LOG_WARNING,"Clould not found the cluster disk with partition info. %s of %s\n",srcDisk,hostname.c_str());
					}
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"Volumes for this cluster disk is already collected. Disk Signatur: %s\n",iterDiskInfo->second.DiskSignature);
				continue;
			}
		}
		
		VolumesInfoVecIter_t iterVol = iterDiskInfo->second.VolumesInfo.begin();
		for(; iterVol != iterDiskInfo->second.VolumesInfo.end(); iterVol++)
		{
			volInfo volumeInfo;
			inm_strcpy_s(volumeInfo.strVolumeName, ARRAYSIZE(volumeInfo.strVolumeName), iterVol->VolumeName);
			volumeInfo.startingOffset.QuadPart = iterVol->StartingOffset;
			volumeInfo.partitionLength.QuadPart = iterVol->VolumeLength;
			volumeInfo.endingOffset.QuadPart = iterVol->EndingOffset;
			m_Disk_Vol_ReadFromBinFile.insert(make_pair(iterDisk->first, volumeInfo));
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

/**********************************************************************************************************************
This function will import all the dynamic disk of the given host in MT which were shown as foriegn disk afetr MBR dump
Input : Hostname\Vmname
Input : Map of source and target disk number
***********************************************************************************************************************/
bool MasterTarget::ImportDynamicDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<int, int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();

	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{
		char srcDisk[256];
		inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
		DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
		if(iterDiskInfo == iter->second.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		if(DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			DebugPrintf(SV_LOG_DEBUG, "Got source dynamic disk %s of host %s to import for which target disk is %d\n", srcDisk, hostname.c_str(), iterDisk->second);

			string strDiskPartFile = m_strDataFolderPath + IMPORT_DISK;
			DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

			ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
			if(! outfile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
			strDiskNumber = string("select disk ") + strDiskNumber; 
			outfile << strDiskNumber << endl;
			outfile << "import" << endl;
			//outfile << "exit" << endl;
			outfile.flush();
			outfile.close();

			AssignSecureFilePermission(strDiskPartFile);

			string strExeName = string(DISKPART_EXE);
			string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

			DebugPrintf(SV_LOG_DEBUG,"Command to import the foreign disks (Dynamic disks) : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

			strExeName = strExeName + strParameterToDiskPartExe;
			string strOurPutFile = m_strDataFolderPath + string("import_output.txt");
			if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to import the foreign disks %d of host %s and source disk %s\n", iterDisk->second, hostname.c_str(), srcDisk);
				bResult = false;
			}
			Sleep(10000);//This is recommended between successive Diskpart call to import foreign disks.
		}
		else
			DebugPrintf(SV_LOG_DEBUG, "Got source basic disk %s of host %s to import for which target disk is %d\n", srcDisk, hostname.c_str(), iterDisk->second);
	}
	//Sleep(30000); //30 seconds

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/***************************************************************************************
This Function is to convert a MBR partition style Disk to GPT Partitioned using diskpart
***************************************************************************************/
bool MasterTarget::ConvertToMbrToGpt(int nDiskNum, bool bDyn)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	char Disk[256];
	inm_sprintf_s(Disk, ARRAYSIZE(Disk), "\\\\.\\PHYSICALDRIVE%d", nDiskNum);
		
	DebugPrintf(SV_LOG_INFO, "Got Disk %s to convert GPT Partiton \n", Disk);

	string strDiskPartFile = m_strDataFolderPath + CONVERT_TO_GPT_DISK;
	DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

	ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
	if(! outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	string strDiskNumber = boost::lexical_cast<string>(nDiskNum);
	strDiskNumber = string("select disk ") + strDiskNumber; 
	outfile << strDiskNumber << endl;
	outfile << "convert gpt" << endl;
	if(bDyn)
		outfile << "convert dynamic" << endl;
	//outfile << "exit" << endl;
	outfile.flush();
	outfile.close();

	AssignSecureFilePermission(strDiskPartFile);

	string strExeName = string(DISKPART_EXE);
	string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

	DebugPrintf(SV_LOG_INFO,"Command to convert GPT partiton style disk : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

	strExeName = strExeName + strParameterToDiskPartExe;
	string strOurPutFile = m_strDataFolderPath + string("convert_gpt_output.txt");
	if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to cpnvert the mbr disk %d to gpt disk\n", nDiskNum);
		bResult = false;
	}
	Sleep(2000);//This is recommended between successive Diskpart call to convert gpt style	

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool MasterTarget::UpdateSignatureDiskIdentifierOfDisk(string hostname , map<int, int> mapOfSrcAndTgtDiskNmbr )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
		
	map<string, DisksInfoMap_t>::iterator iter = m_mapOfHostToDiskInfo.find(hostname);
	map<string, string >::iterator iterSig;
	if(iter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified host %s does not found in DiskInfo Map\n", hostname.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<int, int>::iterator iterDisk = mapOfSrcAndTgtDiskNmbr.begin();
	for(; iterDisk != mapOfSrcAndTgtDiskNmbr.end(); iterDisk++)
	{
		char srcDisk[256];
		inm_sprintf_s(srcDisk, ARRAYSIZE(srcDisk), "\\\\.\\PHYSICALDRIVE%d", iterDisk->first);
		DisksInfoMapIter_t iterDiskInfo = iter->second.find(string(srcDisk));
		if(iterDiskInfo == iter->second.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s of host %s\n", srcDisk, hostname.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		char tgtDisk[256];
		inm_sprintf_s(tgtDisk, ARRAYSIZE(tgtDisk), "\\\\?\\PhysicalDrive%d", iterDisk->second);
		DebugPrintf(SV_LOG_DEBUG,"Changing DiskSignature/DiskIdentifier for disk %s of host %s\n", tgtDisk, hostname.c_str());
		iterSig = m_diskNameToSignature.find( tgtDisk );
		if(iterSig == m_diskNameToSignature.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find DiskSignature/DiskIdentifier for disk %s of host %s\n", tgtDisk, hostname.c_str());
			continue;
		}

		//now we have to find the Layout of Disk, is it MBR or GPT?
		string diskId = iterSig->second;
		if(MBR == iterDiskInfo->second.DiskInfoSub.FormatType)
		{
			//in this case we need to truncate the String.
			diskId = iterSig->second.substr(0,iterSig->second.find_first_of('-'));
			diskId.erase(0, 1);
		}
		else if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType)
		{
			diskId = iterSig->second.erase(0, 1);
			int length = diskId.size();
			diskId.erase(length-1, 1);
		}
		DebugPrintf(SV_LOG_DEBUG,"DiskSignature/DiskIdentifier for disk %s of host %s to update : %s\n", tgtDisk, hostname.c_str(), diskId.c_str());
		
		string strDiskPartFile = m_strDataFolderPath + SET_DISK_ID;
		DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

		ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
		if(! outfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		string strDiskNumber = boost::lexical_cast<string>(iterDisk->second);
		strDiskNumber = string("select disk ") + strDiskNumber; 
		outfile << strDiskNumber << endl;
		outfile << "uniqueid disk id="<<diskId<< endl;
		outfile.flush();
		outfile.close();

		AssignSecureFilePermission(strDiskPartFile);

		DebugPrintf(SV_LOG_DEBUG,"uniqueid disk id=%s",diskId.c_str());			

		string strExeName = string(DISKPART_EXE);
		string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");
		DebugPrintf(SV_LOG_DEBUG,"Command to convert signature of disk (Dynamic disks) : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

		strExeName = strExeName + strParameterToDiskPartExe;
		string strOurPutFile = m_strDataFolderPath + string("ChangeDiskSignatureOutput.txt");
		if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to change disk signature of disks %d of host %s and source disk %s\n", iterDisk->second, hostname.c_str(), srcDisk);
			bResult = false;
		}
		Sleep(10000);//This is recommended between successive Diskpart call to change disk signature.
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/******************************************************************************************
This function will read the specified MBR file and display the disk and Volume information
Also stroes the MBR/GPT/LDM data in files with respect to disks.
*******************************************************************************************/
int MasterTarget::GetDiskAndMbrDetails(const string& strMbrFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;

	DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
	DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
	if(DLM_ERR_SUCCESS != RetVal)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile.c_str(), RetVal);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}

	DisksInfoMapIter_t diskIter;

	for(diskIter = srcMapOfDiskNamesToDiskInfo.begin(); diskIter != srcMapOfDiskNamesToDiskInfo.end(); diskIter++)
	{
		DISK_INFO diskInfo = diskIter->second;
		DebugPrintf(SV_LOG_INFO, "Disk Name = %s\n", diskIter->first.c_str());

		DebugPrintf(SV_LOG_INFO, "Name            = %s\n", diskInfo.DiskInfoSub.Name);
        DebugPrintf(SV_LOG_INFO, "BytesPerSector  = %llu\n", diskInfo.DiskInfoSub.BytesPerSector);
        DebugPrintf(SV_LOG_INFO, "EbrCount        = %llu\n", diskInfo.DiskInfoSub.EbrCount);
        DebugPrintf(SV_LOG_INFO, "Flag            = %llu\n", diskInfo.DiskInfoSub.Flag);
        DebugPrintf(SV_LOG_INFO, "FormatType      = %u\n", diskInfo.DiskInfoSub.FormatType);
        DebugPrintf(SV_LOG_INFO, "ScsiId          = %u:%u:%u:%u\n", diskInfo.DiskInfoSub.ScsiId.Host, diskInfo.DiskInfoSub.ScsiId.Channel, diskInfo.DiskInfoSub.ScsiId.Target, diskInfo.DiskInfoSub.ScsiId.Lun);
        DebugPrintf(SV_LOG_INFO, "Size            = %lld\n", diskInfo.DiskInfoSub.Size);
        DebugPrintf(SV_LOG_INFO, "Type            = %u\n", diskInfo.DiskInfoSub.Type);
        DebugPrintf(SV_LOG_INFO, "VolumeCount     = %llu\n", diskInfo.DiskInfoSub.VolumeCount);
		DebugPrintf(SV_LOG_INFO, "Disk Device Id  = %s\n", diskInfo.DiskDeviceId);
		if(diskInfo.DiskInfoSub.FormatType == MBR)
		{
			DebugPrintf(SV_LOG_INFO, "Disk Signature  = %s (Signature is in decimal format, convert to hexadecimal for extact signature)\n", diskInfo.DiskSignature);
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Disk Signature  = %s\n", diskInfo.DiskSignature);
		}

		if(!diskInfo.vecPartitionFilePath.empty())
		{
			std::vector<PARTITION_FILE>::iterator iterVecP = diskInfo.vecPartitionFilePath.begin();
			for(; iterVecP != diskInfo.vecPartitionFilePath.end(); iterVecP++)
			{
				DebugPrintf(SV_LOG_INFO, "PartitionFileName     = %s\n", iterVecP->Name);
			}
		}

		if(RAWDISK != diskInfo.DiskInfoSub.FormatType)
		{
            //read vol info of all  volumes
			VolumesInfoVecIter_t volIter= diskInfo.VolumesInfo.begin();
			for(; volIter != diskInfo.VolumesInfo.end(); volIter++)
			{                   
				DebugPrintf(SV_LOG_INFO, "\tVolumeName      = %s\n",volIter->VolumeName);
				DebugPrintf(SV_LOG_INFO, "\tVolumeGuid      = %s\n",volIter->VolumeGuid);
				DebugPrintf(SV_LOG_INFO, "\tVolumeLength    = %lld\n",volIter->VolumeLength);
				DebugPrintf(SV_LOG_INFO, "\tStartingOffset  = %lld\n",volIter->StartingOffset);
				DebugPrintf(SV_LOG_INFO, "\tEndingOffset    = %lld\n",volIter->EndingOffset);
			}
		}
		if(MBR == diskInfo.DiskInfoSub.FormatType && BASIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_mbr_basic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s. unable to write the MBR for basic disk %s\n",strMbrData.c_str(), diskIter->first.c_str());
			}
			else
			{
				fwrite(diskInfo.MbrSector, sizeof(SV_UCHAR)*MBR_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
		else if(MBR == diskInfo.DiskInfoSub.FormatType && DYNAMIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_mbr_dynamic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.MbrDynamicSector, sizeof(SV_UCHAR)*MBR_DYNAMIC_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}

			strMbrData = string("C:\\") + diskIter->first + string("_ldm_mbr_dynamic");
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.LdmDynamicSector, sizeof(SV_UCHAR)*LDM_DYNAMIC_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
		else if(GPT == diskInfo.DiskInfoSub.FormatType && BASIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_gpt_basic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.GptSector, sizeof(SV_UCHAR)*GPT_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
		else if(GPT == diskInfo.DiskInfoSub.FormatType && DYNAMIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_gpt_dynamic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.GptDynamicSector, sizeof(SV_UCHAR)*GPT_DYNAMIC_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}

int MasterTarget::GetDiskAndPartitionDetails(const string& strPartitionFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;

	DlmPartitionInfoMap_t srcMapOfDiskToPartitions;
	DLM_ERROR_CODE RetVal = ReadPartitionInfoFromFile(strPartitionFile.c_str(), srcMapOfDiskToPartitions);
	if(DLM_ERR_SUCCESS != RetVal)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strPartitionFile.c_str(), RetVal);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}

	DlmPartitionInfoIterMap_t diskIter;

	for(diskIter = srcMapOfDiskToPartitions.begin(); diskIter != srcMapOfDiskToPartitions.end(); diskIter++)
	{
		DebugPrintf(SV_LOG_INFO, "Disk Name = %s\n", diskIter->first.c_str());

		DlmPartitionInfoIterVec_t iterVec = diskIter->second.begin();
		for(; iterVec != diskIter->second.end(); iterVec++)
		{
			DebugPrintf(SV_LOG_INFO, "Partition Name            = %s\n", iterVec->PartitionInfoSub.PartitionName);
			DebugPrintf(SV_LOG_INFO, "Partition Type            = %s\n", iterVec->PartitionInfoSub.PartitionType);
			DebugPrintf(SV_LOG_INFO, "Partition Number          = %d\n", iterVec->PartitionInfoSub.PartitionNum);
			DebugPrintf(SV_LOG_INFO, "Partition Length          = %lld\n", iterVec->PartitionInfoSub.PartitionLength);
			DebugPrintf(SV_LOG_INFO, "Partition Starting Offset = %lld\n", iterVec->PartitionInfoSub.StartingOffset);

			stringstream s;
			s << iterVec->PartitionInfoSub.PartitionNum;
		
			string strPartitionData = string("C:\\") + diskIter->first + string("_partition_") + s.str();
			FILE *fp;
			fp = fopen(strPartitionData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strPartitionData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return 1;
			}
			else
			{
				fwrite(iterVec->PartitionSector, sizeof(SV_UCHAR)*(iterVec->PartitionInfoSub.PartitionLength), 1, fp);
				DebugPrintf(SV_LOG_INFO,"Successfully created the file : %s.\n",strPartitionData.c_str());
				fclose(fp);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}


int MasterTarget::UpdateDiskAndPartitionDetails(const string& strPartitionFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;

	std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames;
	MapSrcToTgtDiskNames.insert(make_pair("\\\\.\\PHYSICALDRIVE0", "\\\\.\\PHYSICALDRIVE1"));
	std::set<DiskName_t> RestoredSrcDiskNames;
	DLM_ERROR_CODE RetVal = RestoreDiskPartitions(strPartitionFile.c_str(), MapSrcToTgtDiskNames, RestoredSrcDiskNames);
	if(DLM_ERR_SUCCESS != RetVal)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strPartitionFile.c_str(), RetVal);
		iRetValue = 1;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully updated the disk partiton information\n");
		std::set<DiskName_t>::iterator iter = RestoredSrcDiskNames.begin();
		DebugPrintf(SV_LOG_INFO,"disk which is updated : \n");
		for(;iter != RestoredSrcDiskNames.end(); iter++)
		{
			DebugPrintf(SV_LOG_INFO,"\t %s \n", iter->c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}


bool MasterTarget::CopyDirectory(const std::string& src, const std::string& dest, bool recursive)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	ACE_stat ace_statBuff = {0};
	do
	{
		if( (sv_stat(getLongPathName(src.c_str()).c_str(), &ace_statBuff) != 0) )
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to stat file %s, error no: %d\n", src.c_str(),ACE_OS::last_error());
			rv = false;
			break;
		}
		if( (ace_statBuff.st_mode & S_IFDIR) != S_IFDIR )
		{
			DebugPrintf(SV_LOG_ERROR, "Given file %s is not a directory\n", src.c_str());
			rv = false;
			break;
		}
		if(SVMakeSureDirectoryPathExists(dest.c_str()).failed())
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to create %s, error no: %d\n", dest.c_str(),ACE_OS::last_error());
			rv = false;
			break;
		}

		ACE_DIR *pSrcDir = sv_opendir(src.c_str());
		if(pSrcDir == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "Unable to open directory %s, error no: %d\n", src.c_str(),ACE_OS::last_error());
			rv = false;
			break;
		}

		struct ACE_DIRENT *dEnt = NULL;
		while ((dEnt = ACE_OS::readdir(pSrcDir)) != NULL)
		{
			std::string dName = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
			if ( dName == "." || dName == ".." )
				continue;

			std::string srcPath = src + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
			std::string destPath = dest + ACE_DIRECTORY_SEPARATOR_STR_A + dName;
			ACE_stat ace_statbuf = {0};
			if( (sv_stat(getLongPathName(srcPath.c_str()).c_str(), &ace_statbuf) != 0))
			{
				DebugPrintf(SV_LOG_ERROR, "Given file %s does not exist.\n", srcPath.c_str());
				continue;
			}

			if((ace_statbuf.st_mode & S_IFDIR) == S_IFDIR )
			{
				if(!recursive)
					continue;
				if(!CopyDirectory(srcPath, destPath, true))
				{
					rv = false;
					break;
				}
			}
			else if((ace_statbuf.st_mode & S_IFREG) == S_IFREG)
			{
				//reducing the copy time by checking the the file existance in new location and comparing the size of the file
				ACE_stat NewFileStat ={0};
				if((sv_stat(getLongPathName(destPath.c_str()).c_str(), &NewFileStat) == 0) && 
					(NewFileStat.st_size == ace_statbuf.st_size))
				{
					continue;
				}
				
				if(false ==	SVCopyFile(srcPath.c_str(),destPath.c_str(),false))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to copy file %s to %s.\n", srcPath.c_str(),destPath.c_str());
					rv = false;
					break;
				}
			}
		}
		(void)ACE_OS::closedir(pSrcDir);

	} while(false);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


bool MasterTarget::RestartVdsService()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	string svcName = "vds";
	if(S_FALSE == restartService(svcName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to re-start the \"vds\" service \n");
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Successfully re-started the \"vds\" service \n");
		rv = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


bool MasterTarget::CheckOnlineStatusDisks(string hostname , map<int, int> mapOfSrcAndTgtDiskNmbr)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	CVdsHelper VdsObj;
	//This is for updating the dinfo file. required for rollback
	if(!VdsObj.InitilizeVDS())
	{
		DebugPrintf(SV_LOG_ERROR,"COM Initialisation failed while trying to use VDS Interface\n");
		DebugPrintf(SV_LOG_ERROR,"Failed to check the Status of the disks\n");
		VdsObj.unInitialize();
		rv = false;
	}
	else
	{
		map<string, bool> mapDiskToStatus;
		VdsObj.GetOnlineDisks(mapDiskToStatus);
		VdsObj.unInitialize();
		map<int, int>::iterator iter = mapOfSrcAndTgtDiskNmbr.begin();
		for(;iter != mapOfSrcAndTgtDiskNmbr.end(); iter++)
		{
			string tgtDisk;
			stringstream tgt;
			tgt << iter->second;
			tgt >> tgtDisk;
			tgtDisk = string("\\\\?\\PhysicalDrive") + tgtDisk;
			if(mapDiskToStatus.find(tgtDisk) == mapDiskToStatus.end())
			{
				DebugPrintf(SV_LOG_ERROR,"Disk %s is not Online in Master Target\n", tgtDisk.c_str());
				rv = false;
			}
		}
		if(!rv)
		{
			DebugPrintf(SV_LOG_ERROR,"Some of the disks of host %s are not online in master target\n", hostname.c_str());
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


bool MasterTarget::ProcessCxPath()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	string strInfoFile = m_strCxPath + "/input.txt";
	string strFile, strLineRead, strDir;
	if(m_strDirPath.empty())
		strFile = m_strDataFolderPath + string("\\input.txt");
	else
		strFile = m_strDirPath + string("\\input.txt");

	do
	{
		if(false == DownloadFileFromCx(strInfoFile, strFile))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to download the %s file from CX", strInfoFile.c_str());
			rv = false;
			break;
		}
		ifstream readFile(strFile.c_str());
		if (!readFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFile.c_str(),GetLastError());      
			rv = false;
			break;
		}
		while (getline(readFile,strLineRead))
		{
			if(strLineRead.empty())
				continue;
			DebugPrintf(SV_LOG_DEBUG,"strLineRead =  %s\n",strLineRead.c_str());
			if(strLineRead.find("Windows_2003_32") != string::npos)
			{
				strFile = m_strDataFolderPath + string("Windows_2003_32\\symmpi.sys");
				if(checkIfFileExists(strFile))
				{
					DebugPrintf(SV_LOG_DEBUG, "File %s already exist in MT, skpping downloading of this file", strFile.c_str());
					continue;
				}
				strDir = m_strDataFolderPath + string("Windows_2003_32");
				if(FALSE == CreateDirectory(strDir.c_str(),NULL))
				{
					if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
					{
						DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu].\n",strDir.c_str(),GetLastError());
					}
				}
			}
			else if(strLineRead.find("Windows_2003_64") != string::npos)
			{
				strFile = m_strDataFolderPath + string("Windows_2003_64\\symmpi.sys");
				if(checkIfFileExists(strFile))
				{
					DebugPrintf(SV_LOG_DEBUG, "File %s already exist in MT, skpping downloading of this file", strFile.c_str());
					continue;
				}
				strDir = m_strDataFolderPath + string("Windows_2003_64");
				if(FALSE == CreateDirectory(strDir.c_str(),NULL))
				{
					if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
					{
						DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu].\n",strDir.c_str(),GetLastError());
					}
				}
			}
			else if(strLineRead.find("Windows_XP_32") != string::npos)
			{
				strFile = m_strDataFolderPath + string("Windows_XP_32\\symmpi.sys");
				if(checkIfFileExists(strFile))
				{
					DebugPrintf(SV_LOG_DEBUG, "File %s already exist in MT, skpping downloading of this file", strFile.c_str());
					continue;
				}
				strDir = m_strDataFolderPath + string("Windows_XP_32");
				if(FALSE == CreateDirectory(strDir.c_str(),NULL))
				{
					if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
					{
						DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu].\n",strDir.c_str(),GetLastError());
					}
				}
			}
			else if(strLineRead.find("Windows_XP_64") != string::npos)
			{
				strFile = m_strDataFolderPath + string("Windows_XP_64\\symmpi.sys");
				if(checkIfFileExists(strFile))
				{
					DebugPrintf(SV_LOG_DEBUG, "File %s already exist in MT, skpping downloading of this file", strFile.c_str());
					continue;
				}
				strDir = m_strDataFolderPath + string("Windows_XP_64");
				if(FALSE == CreateDirectory(strDir.c_str(),NULL))
				{
					if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
					{
						DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu].\n",strDir.c_str(),GetLastError());
					}
				}
			}
			else
			{
				if(m_strDirPath.empty())
					strFile = m_strDataFolderPath + strLineRead;
				else
					strFile = m_strDirPath + string("\\") + strLineRead;
			}
			strInfoFile = m_strCxPath + "/" + strLineRead;
			if(false == DownloadFileFromCx(strInfoFile, strFile))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to download the %s file from CX", strInfoFile.c_str());
				rv = false;
				break;
			}
		}
		readFile.close();
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


//Prepares the xml data for CX API call to update task status
//We can make this Api generic, for this we need to pass all the values as required by storing it in a structure.
//currently all values are hard coded in it.
string MasterTarget::prepareXmlData()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strRqstId = GetRequestId();

    string xml_data = string("<Request Id=\"") + strRqstId + string("\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AuthMethod>ComponentAuth_V2015_01</AuthMethod>");
	xml_data = xml_data + string("<AccessKeyID>") + getInMageHostId() + string("</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature>") + GetAccessSignature("POST", "/ScoutAPI/CXAPI.php", m_TaskUpdateInfo.FunctionName, strRqstId, "1.0") + string("</AccessSignature>");
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"")+ m_TaskUpdateInfo.FunctionName + string("\" Include=\"N\">");
	xml_data = xml_data + string("<Parameter Name=\"PlanId\" Value=\"") + m_TaskUpdateInfo.PlanId + string("\" />");
	xml_data = xml_data + string("<Parameter Name=\"HostGUID\" Value=\"") + m_TaskUpdateInfo.HostId + string("\" />");

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for(; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		xml_data = xml_data + string("<ParameterGroup Id=\"") + iterStep->StepNum + string("\">");
		xml_data = xml_data + string("<Parameter Name=\"ExecutionStep\" Value=\"") + iterStep->ExecutionStep + string("\" />");
		xml_data = xml_data + string("<Parameter Name=\"Status\" Value=\"") + iterStep->StepStatus + string("\" />");
		xml_data = xml_data + string("<ParameterGroup Id=\"TaskList\">");

		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for(; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			xml_data = xml_data + string("<ParameterGroup Id=\"") + iterTask->TaskNum + string("\">");
			xml_data = xml_data + string("<Parameter Name=\"TaskName\" Value=\"") + iterTask->TaskName + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"Description\" Value=\"") + iterTask->TaskDesc + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"TaskStatus\" Value=\"") + iterTask->TaskStatus + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"ErrorMessage\" Value=\"") + iterTask->TaskErrMsg + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"FixSteps\" Value=\"") + iterTask->TaskFixSteps +  string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"LogPath\" Value=\"") + iterTask->TaskLogPath +  string("\" />");
			xml_data = xml_data + string("</ParameterGroup>");
		}
		xml_data = xml_data + string("</ParameterGroup>");   // For TaskList
		xml_data = xml_data + string("</ParameterGroup>");   // For Stepid
	}

	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");

	DebugPrintf(SV_LOG_DEBUG,"Xml Data to Post in CX \n\n%s\n\n",xml_data.c_str());
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}


/********************************************************************************************************
This function will process the input xml file using cx api and stroes the response in the ouput xml file
input -- chRequestXmlData - Prepared xml data for CX API in earlier call
output -- strOutPutXmlFile - Response xml file of CXAPI call
*********************************************************************************************************/
bool MasterTarget::processXmlData(string strXmlData, string& strOutPutXmlFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	//Find CX Ip from Local Configurtaor
	string strCxIpAddress = getCxIpAddress();
	DebugPrintf(SV_LOG_DEBUG,"Cx IP address = %s\n",strCxIpAddress.c_str());
    if (strCxIpAddress.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX IP Address.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	//Find CX port number
	string strCXHttpPortNumber = getCxHttpPortNumber();
	DebugPrintf(SV_LOG_DEBUG,"Cx HTTP Port Number = %s\n",strCXHttpPortNumber.c_str());
	if (strCXHttpPortNumber.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX HTTP Port Number.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
    string strCxUrl = string("/ScoutAPI/CXAPI.php");
	
	DebugPrintf(SV_LOG_DEBUG,"PHP URL is : %s\n",strCxUrl.c_str());
	
	bool useSecure = (m_strCmdHttp.compare("https://") == 0);
    DebugPrintf(SV_LOG_DEBUG,"Secure : %d\n",useSecure);

	DebugPrintf(SV_LOG_DEBUG,"Response file for Cx Api call is : %s\n",strOutPutXmlFile.c_str());
	
	// Post the XML to Cx.
	
	ResponseData_t Data = {0};
	string responsedata;
	
	curl_global_init(CURL_GLOBAL_ALL);
	

	DebugPrintf(SV_LOG_DEBUG,"Posting the xml data\n");

	std::string result;
	CurlOptions options(strCxIpAddress,boost::lexical_cast<int>(strCXHttpPortNumber),strCxUrl,useSecure);
	options.writeCallback( static_cast<void *>( &Data ),WriteCallbackFunction);

	Data.chResponseData = NULL; 
	Data.length = 0; /* no data at this point */

	try {
		CurlWrapper cw;
		cw.post(options, strXmlData);
		DebugPrintf(SV_LOG_DEBUG,"Posted the xml data Successfully\n");
		if( Data.length > 0)
		{
			responsedata = string(Data.chResponseData);
			if(Data.chResponseData != NULL)
				free( Data.chResponseData ) ;
		}
	} catch(ErrorException& exception )
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED : %s with error %s\n",FUNCTION_NAME, exception.what());
		bResult = false;
	}

	ofstream outXml(strOutPutXmlFile.c_str(),std::ios::out);
    if(!outXml.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",strOutPutXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	outXml << responsedata << endl;
	outXml.close();

	AssignSecureFilePermission(strOutPutXmlFile);

	DebugPrintf(SV_LOG_DEBUG,"Response : %s\n", responsedata.c_str());
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Modifys the task information for the running task and the task for whcih it will begin
void MasterTarget::ModifyTaskInfoForCxApi(taskInfo_t currentTask, taskInfo_t nextTask, bool bUpdateProtection)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for(; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for(; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			if(iterTask->TaskNum.compare(currentTask.TaskNum) == 0)
			{
				iterTask->TaskStatus = currentTask.TaskStatus;
				if(!currentTask.TaskErrMsg.empty())
				{
					for(int i=0; i < currentTask.TaskErrMsg.size(); i++)
					{
						if(currentTask.TaskErrMsg.find("\"") != string::npos)
							currentTask.TaskErrMsg.replace(currentTask.TaskErrMsg.find("\""), 1, "'");
					}
					iterTask->TaskErrMsg = currentTask.TaskErrMsg;
				}
				if(!currentTask.TaskFixSteps.empty())
				{
					for(int i=0; i < currentTask.TaskFixSteps.size(); i++)
					{
						if(currentTask.TaskFixSteps.find("\"") != string::npos)
							currentTask.TaskFixSteps.replace(currentTask.TaskFixSteps.find("\""), 1, "'");
					}
					iterTask->TaskFixSteps = currentTask.TaskFixSteps;
				}
			}
			else if((iterTask->TaskNum.compare(nextTask.TaskNum) == 0) && !bUpdateProtection)
			{
				iterTask->TaskStatus = nextTask.TaskStatus;
			}
		}
		if(bUpdateProtection)
		{
			if(iterStep->ExecutionStep.compare(nextTask.TaskNum) == 0)
			{
				iterStep->StepStatus = nextTask.TaskStatus;
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

void MasterTarget::UpdateTaskInfoToCX(bool bCheckResponse)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string xmlCxApiTaskInfo = prepareXmlData();

	string strCxApiResXmlFile;
	if(m_strDirPath.empty() && m_strDataFolderPath.empty())
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
	else if(m_strDirPath.empty())
		strCxApiResXmlFile = m_strDataFolderPath + "OutPutCxXml.xml";
	else
		strCxApiResXmlFile = m_strDirPath + "\\OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", m_TaskUpdateInfo.HostId.c_str());
		DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

/*****************************************************************************************
This function is to download the mbrdata_<timestamp> file from CX.
If successfully downloads then returns true, else false.
1st Input: Local path from where the file need to upload to CX
2nd input: CX URL with path in which the file need to upload.
*****************************************************************************************/
bool MasterTarget::UploadFileToCx(string strLocalPath, string strCxPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	std::string strCxIp = getCxIpAddress();
	string CxCmd = string("cxpsclient.exe -i " ) + strCxIp + string(" --cscreds --put \"") + strLocalPath + string("\" -d \"") + strCxPath + string("\" -C --secure -c \"") + m_strInstallDirectoryPath + string("\\transport\\client.pem\"");

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CX File Upload command : %s\n", CxCmd.c_str());

    if(false == CdpcliOperations(CxCmd))
    {
        DebugPrintf(SV_LOG_ERROR,"File Upload command failed\n");
        bRetStatus = false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool MasterTarget::DownloadPartitionFile(string strCxPath, string strLocalPath, DisksInfoMap_t& srcMapOfDiskNamesToDiskInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	set<string> setPartitionFile;
	DisksInfoMapIter_t iterDisk = srcMapOfDiskNamesToDiskInfo.begin();
	string strPartitionFile;

	for(; iterDisk != srcMapOfDiskNamesToDiskInfo.end(); iterDisk++)
	{
		if(!(iterDisk->second.vecPartitionFilePath.empty()))
		{
			std::vector<PARTITION_FILE>::iterator iterVec = iterDisk->second.vecPartitionFilePath.begin();
			for(; iterVec != iterDisk->second.vecPartitionFilePath.end(); iterVec++)
			{
				size_t pos = string(iterVec->Name).find_last_of("\\");
				strPartitionFile = string(iterVec->Name).substr(pos+1);
				setPartitionFile.insert(strPartitionFile);		//Currently only one file is there, so pushing that with out changing the name.
			}														//Need to change the logic if multiple files will introduce in future
		}
	}

	if(setPartitionFile.empty())
	{
		DebugPrintf(SV_LOG_DEBUG,"Partition files does not exist  for the respective host\n");
	}
	else
	{
		strLocalPath = strLocalPath + string("_partition");

		size_t pos = strCxPath.find_last_of("/");
		if(string::npos == pos)
			pos = strCxPath.find_last_of("\\"); // Windows CX compatability.

		string strPartitionCxPath;
		if(pos != string::npos)
			strPartitionCxPath = strCxPath.substr(0, pos+1);

		set<string>::iterator iterSet = setPartitionFile.begin();
		for(;iterSet != setPartitionFile.end(); iterSet++)
		{
			string strPartitionCxFile = strPartitionCxPath + (*iterSet);		//Need to write logic for multiple files generation case

			string strLocalPathResume = strLocalPath + string(".resume");
			if ((true == checkIfFileExists(strLocalPath)) && (m_strOperation.compare("resume") == 0))
			{
				if (false == SVCopyFile(strLocalPath.c_str(), strLocalPathResume.c_str(), false))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to take the back up of file %s with Error [%lu].\n", strLocalPath.c_str(), GetLastError());
				}
				else
					DeleteFile(strLocalPath.c_str());
			}
			if (false == DownloadFileFromCx(strPartitionCxFile, strLocalPath))
			{
				if (m_strOperation.compare("resume") == 0)
				{
					DebugPrintf(SV_LOG_DEBUG, "Partition File %s already exists, As this is Resume protection proceeding further.", strLocalPathResume.c_str());
					if (false == SVCopyFile(strLocalPathResume.c_str(), strLocalPath.c_str(), false))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to take the back up of file %s with Error [%lu].\n", strLocalPathResume.c_str(), GetLastError());
						bRetStatus = false;
					}
					else
						DeleteFile(strLocalPathResume.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to download the partition file %s  from CX path \n", strLocalPath.c_str(), strPartitionCxFile.c_str());
					bRetStatus = false;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully downloaded the partition file %s from cx path %s\n", strLocalPath.c_str(), strPartitionCxFile.c_str());
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}

bool MasterTarget::xGetClusNodesAndClusDisksInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = false;
	xmlDocPtr doc = NULL;
	xmlNodePtr cur_node = NULL;

	doc = xmlParseFile(m_strXmlFile.c_str());
	if(NULL == doc)
	{
		DebugPrintf(SV_LOG_ERROR,"XML Document (%s) is not parsed successfully.\n",m_strXmlFile.c_str());
		return bRet;
	}
	cur_node = xmlDocGetRootElement(doc);
	if(NULL == cur_node)
	{
		xmlFreeDoc(doc);
		DebugPrintf(SV_LOG_ERROR,"Document %s is empty\n",m_strXmlFile.c_str());
		return bRet;
	}
	if(xmlStrcmp(cur_node->name,(const xmlChar*)"root"))
	{
		xmlFreeDoc(doc);
		DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. Root tag is missing.\n",m_strXmlFile.c_str());
		return bRet;
	}
	//verify return status of cx-api xml response
	cur_node = xGetChildNode(cur_node,(const xmlChar*)"SRC_ESX");
	if(NULL == cur_node)
	{
		xmlFreeDoc(doc);
		DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. SRC_ESX tag is missing.\n",m_strXmlFile.c_str());
		return bRet;
	}
	bRet = true;
	cur_node = cur_node->children;
	for(; NULL != cur_node && bRet; cur_node = cur_node->next)
	{
		if(xmlStrcmp(cur_node->name,(const xmlChar*)"host") == 0)
		{
			xmlChar *pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"inmage_hostid");
			if(NULL == pAttrVal)
			{
				bRet = false;
				//xmlFreeDoc(doc);
				DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host inmage_hostid=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
				break;
			}
			string hostId = (const char*)pAttrVal;
			DebugPrintf(SV_LOG_DEBUG,"InMage Host ID : %s\n", hostId.c_str());
			xmlFree(pAttrVal);
			
			pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"machinetype");
			if(NULL == pAttrVal)
			{
				m_MapHostIdMachineType.insert(make_pair(hostId, false));
			}
			else
			{
				string strMachineType;
				strMachineType = (const char*)pAttrVal;
				DebugPrintf(SV_LOG_DEBUG,"Machine Type : %s\n", strMachineType.c_str());
				if(strMachineType.compare("PhysicalMachine") == 0)
					m_MapHostIdMachineType.insert(make_pair(hostId, true));
				else
					m_MapHostIdMachineType.insert(make_pair(hostId, false));
				xmlFree(pAttrVal);
			}

			string strOs;
			pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"operatingsystem");
			if(NULL == pAttrVal)
			{
				DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host operatingsystem=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
			}
			else
			{
				strOs = (const char*)pAttrVal;
				DebugPrintf(SV_LOG_DEBUG,"Operating System : %s\n", strOs.c_str());
				xmlFree(pAttrVal);
			}
			m_MapHostIdOsType.insert(make_pair(hostId, strOs));	
			m_mapOfClusNodeOS.insert(make_pair(hostId,strOs));

			pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"cluster");
			if(NULL == pAttrVal)
			{
				bRet = false;
				//xmlFreeDoc(doc);
				DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host cluster=\"yes/no\" > property is missing.\n",m_strXmlFile.c_str());
				break;
			}
			string strCluster = (const char*)pAttrVal;
			xmlFree(pAttrVal);

			if(strCluster.compare("yes") != 0) 
				continue;

			string clusname;
			pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"cluster_name");
			if(NULL == pAttrVal)
			{
				//bRet = false;
				//xmlFreeDoc(doc);
				DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host cluster_name=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
				//break;
			}
			else
			{
				clusname = (const char*)pAttrVal;
				xmlFree(pAttrVal);
			}

			m_mapClusterNameToNodes.insert(make_pair(clusname,hostId));

			std::string Separator = std::string(",");
			size_t pos = 0;
			size_t found;
			list<string> lstClusterNodes;
			list<string> lstClusterMbrFiles;

			string clusnodes;
			pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"clusternodes_inmageguids");
			if(NULL == pAttrVal)
			{
				//bRet = false;
				//xmlFreeDoc(doc);
				DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host clusternodes_inmageguids=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
				//break;
			}
			else
			{
				clusnodes = (const char*)pAttrVal;
				xmlFree(pAttrVal);

				found = clusnodes.find(Separator, pos);
				if(found == std::string::npos)
				{
					if(clusnodes.empty())
					{
						DebugPrintf(SV_LOG_INFO,"There are no cluster nodes information from XML file.\n");
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG,"Only single cluster node found.\n");
						lstClusterNodes.push_back(clusnodes);
					}
				}
				else
				{
					while(found != std::string::npos)
					{
						DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",clusnodes.substr(pos,found-pos).c_str());
						lstClusterNodes.push_back(clusnodes.substr(pos,found-pos));
						pos = found + 1; 
						found = clusnodes.find(Separator, pos);            
					}
					DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",clusnodes.substr(pos).c_str());
					lstClusterNodes.push_back(clusnodes.substr(pos));
				}
			}

			string clusnodembrfiles;
			pAttrVal = xmlGetProp(cur_node,(const xmlChar*)"clusternodes_mbrfiles");
			if(NULL == pAttrVal)
			{
				//bRet = false;
				//xmlFreeDoc(doc);
				DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <host clusternodes_mbrfiles=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
				//break;
			}
			else
			{
				clusnodembrfiles = (const char*)pAttrVal;
				xmlFree(pAttrVal);

				pos = 0;
				found = clusnodembrfiles.find(Separator, pos);
				if(found == std::string::npos)
				{
					if(clusnodembrfiles.empty())
					{
						DebugPrintf(SV_LOG_INFO,"Empty MBR file found for the cluster nodes.\n");
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG,"Only sinlge MBR file found for the cluster nodes : %s.\n", clusnodembrfiles.c_str());
						lstClusterMbrFiles.push_back(clusnodembrfiles);
					}
				}
				else
				{
					while(found != std::string::npos)
					{
						DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",clusnodembrfiles.substr(pos,found-pos).c_str());
						lstClusterMbrFiles.push_back(clusnodembrfiles.substr(pos,found-pos));
						pos = found + 1; 
						found = clusnodembrfiles.find(Separator, pos);            
					}
					DebugPrintf(SV_LOG_DEBUG,"Element - [%s]\n",clusnodembrfiles.substr(pos).c_str());
					lstClusterMbrFiles.push_back(clusnodembrfiles.substr(pos));
				}
			}

			list<string>::iterator iterNode, iterMbrFile;
			for(iterNode = lstClusterNodes.begin(); iterNode != lstClusterNodes.end(); iterNode++)
			{
				for(iterMbrFile = lstClusterMbrFiles.begin(); iterMbrFile != lstClusterMbrFiles.end(); iterMbrFile++)
				{
					if(iterMbrFile->find(*iterNode) != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Cluster Node : %s Cluster Mbr File : %s.\n",iterNode->c_str(), iterMbrFile->c_str());
						clusterMbrInfo_t clusInfo;
						clusInfo.bDownloaded = false;
						clusInfo.MbrFile = *iterMbrFile;
						clusInfo.NodeId = *iterNode;
						m_mapHostToClusterInfo.insert(make_pair(*iterNode, clusInfo));
					}
				}
			}

			//cluster disks
			map<string,string> mapDisknameSign;
			map<string,string> mapDiskSignToDiskType;
			xmlNodePtr node_disk = cur_node->children;
			for(; NULL != node_disk; node_disk = node_disk->next)
			{
				if(xmlStrcmp(node_disk->name,(const xmlChar*)"disk") == 0)
				{
					pAttrVal = xmlGetProp(node_disk,(const xmlChar*)"cluster_disk");
					if(NULL == pAttrVal)
					{
						bRet = false;
						//xmlFreeDoc(doc);
						DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <disk cluster_disk=\"yes/no\" > property is missing.\n",m_strXmlFile.c_str());
						break;
					}
					string strClusDisk = (const char*)pAttrVal;
					xmlFree(pAttrVal);

					string strDiskSign;
					pAttrVal = xmlGetProp(node_disk,(const xmlChar*)"disk_signature");
					if(NULL == pAttrVal)
					{
						//bRet = false;
						//xmlFreeDoc(doc);
						DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <disk disk_signature=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
						//break;
					}
					else
					{
						strDiskSign = (const char*)pAttrVal;
						xmlFree(pAttrVal);
					}

					string strSrcDiskLayout;
					pAttrVal = xmlGetProp(node_disk,(const xmlChar*)"disk_layout");
					if (NULL == pAttrVal)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <disk disk_layout> entry not found\n");
					}
					else
						strSrcDiskLayout = string((const char *)pAttrVal);

					string strDiskSigInDec;
					if(!strDiskSign.empty() && !strSrcDiskLayout.empty())
					{
						if(strSrcDiskLayout.compare("MBR") == 0)
							strDiskSigInDec = ConvertHexToDec(strDiskSign);
						else
							strDiskSigInDec = strDiskSign;

						mapDiskSignToDiskType.insert(make_pair(strDiskSign, strSrcDiskLayout));
					}
					xmlFree(pAttrVal);

					if(strClusDisk.compare("yes")!=0) 
						continue;

					//src_devicename
					pAttrVal = xmlGetProp(node_disk,(const xmlChar*)"src_devicename");
					if(NULL == pAttrVal)
					{
						bRet = false;
						//xmlFreeDoc(doc);
						DebugPrintf(SV_LOG_ERROR,"Document %s is not in expected format. <disk src_devicename=\"XXX\" \\> property is missing.\n",m_strXmlFile.c_str());
						break;
					}
					string strDiskName = (const char*)pAttrVal;
					xmlFree(pAttrVal);

					DebugPrintf(SV_LOG_DEBUG,"Making pair: Disk Number [%s] -- Disk Signature [%s].\n",strDiskName.c_str(), strDiskSigInDec.c_str());
					mapDisknameSign.insert(make_pair(strDiskSigInDec, strDiskName));
				}
			}
			m_mapNodeClusDiskSign[hostId] = mapDisknameSign;
			m_mapNodeDiskSignToDiskType[hostId] = mapDiskSignToDiskType;
		}
	}

	xmlFreeDoc(doc);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool MasterTarget::IsClusterNode(std::string hostName)
{
	MMIter iter = m_mapClusterNameToNodes.begin();
	while(iter != m_mapClusterNameToNodes.end())
	{
		if(hostName.compare(iter->second) == 0)
			return true;
		iter++;
	}
	return false;
}

bool MasterTarget::IsClusterDisk(std::string hostName,const string diskSignature)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s(%s,%s)\n",__FUNCTION__,hostName.c_str(),diskSignature.c_str());
	bool bRet = false;

	map<string, map<string,string> >::iterator clusDiskRange = m_mapNodeClusDiskSign.find(hostName);
	if(m_mapNodeClusDiskSign.end() != clusDiskRange)
	{
		if(clusDiskRange->second.find(diskSignature) != clusDiskRange->second.end())
		{
			DebugPrintf(SV_LOG_INFO,"Disk %s has been found under cluster disk information\n",diskSignature.c_str());
			bRet = true;
		}
		else
			DebugPrintf(SV_LOG_INFO,"Disk %s has not been found under cluster disk information\n",diskSignature.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}
string MasterTarget::GetSrcClusDiskSignature(const string& hostname, const string& diskname)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s(%s,%s)\n",__FUNCTION__,hostname.c_str(),diskname.c_str());
	string diskSing;

	map<string, map<string,string> >::iterator clusDiskRange = m_mapNodeClusDiskSign.find(hostname);
	if(m_mapNodeClusDiskSign.end() != clusDiskRange)
		if(clusDiskRange->second.find(diskname) != clusDiskRange->second.end())
		{
			diskSing = clusDiskRange->second[diskname];
			DebugPrintf(SV_LOG_DEBUG,"%s => %s\n",diskname.c_str(),diskSing.c_str());
		}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return diskSing;
}
void MasterTarget::GetClusterNodes(std::string clusterName, std::list<string> &nodes)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	nodes.clear();
	DebugPrintf(SV_LOG_INFO,"Cluster Name: %s\n",clusterName.c_str());

	MMIter mmIter;
	pair<MMIter,MMIter> clusNodesRange = m_mapClusterNameToNodes.equal_range(clusterName);
	for(mmIter = clusNodesRange.first; mmIter != clusNodesRange.second; mmIter++)
	{
		nodes.push_back(mmIter->second);
		DebugPrintf(SV_LOG_INFO,"Node %s\n",mmIter->second.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

string MasterTarget::GetNodeClusterName(std::string nodeName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string clusName;
	MMIter iter = m_mapClusterNameToNodes.begin();
	while(iter != m_mapClusterNameToNodes.end())
	{
		if(nodeName.compare(iter->second) == 0)
		{
			clusName = iter->first;
			DebugPrintf(SV_LOG_INFO,"Cluster Name %s\n",clusName.c_str());
			break;
		}
		iter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return clusName;
}

bool MasterTarget::GetClusDiskWithPartitionInfoFromMap(std::string nodeName, DisksInfoMapIter_t &diskIter)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFound = false;
	double dlmVersion = 0;
	if(m_mapOfHostIdDlmVersion.find(nodeName) != m_mapOfHostIdDlmVersion.end())
		dlmVersion = m_mapOfHostIdDlmVersion[nodeName];
	if(dlmVersion < 1.2)
	{
		DebugPrintf(SV_LOG_DEBUG,"Dlm version of %s MBR file is not compatible\n",nodeName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bFound;
	}
	string clusterName = GetNodeClusterName(nodeName);

	string diskSig = diskIter->second.DiskSignature;
	list<string> clusNodes;
	GetClusterNodes(clusterName,clusNodes);
	for(list<string>::iterator iterNode = clusNodes.begin();
		iterNode != clusNodes.end(); iterNode++ )
	{
		map<string, DisksInfoMap_t>::iterator iterNodeDisks = m_mapOfHostToDiskInfo.find(*iterNode);
		if(m_mapOfHostToDiskInfo.end() != iterNodeDisks)
		{
			DisksInfoMapIter_t iterDiskInfo = iterNodeDisks->second.begin();
			while(iterNodeDisks->second.end() != iterDiskInfo)
			{
				if(0 == diskSig.compare(iterDiskInfo->second.DiskSignature) &&
				   false == iterDiskInfo->second.VolumesInfo.empty() )
				{
					DebugPrintf(SV_LOG_DEBUG,"Node %s has disk-info with partition info available\n",
						iterDiskInfo->first.c_str());
					bFound = true;
					diskIter = iterDiskInfo;
					break;
				}
				iterDiskInfo++;
			}
		}
		
		if(bFound) break;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFound;
}

bool MasterTarget::GetClusterDiskMapIter(std::string nodeName, std::string strSrcDiskSign, DisksInfoMapIter_t & iterDisk)
{
	bool bFound = false;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	double dlmVersion = 0;
	if(m_mapOfHostIdDlmVersion.find(nodeName) != m_mapOfHostIdDlmVersion.end())
		dlmVersion = m_mapOfHostIdDlmVersion[nodeName];
	if(dlmVersion < 1.2)
	{
		DebugPrintf(SV_LOG_DEBUG,"Dlm version of %s MBR file is not compatible.\n",nodeName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bFound;
	}
	if(m_mapOfHostToDiskInfo.find(nodeName) != m_mapOfHostToDiskInfo.end())
	{
		DisksInfoMapIter_t iter = m_mapOfHostToDiskInfo[nodeName].begin();
		for(;iter != m_mapOfHostToDiskInfo[nodeName].end(); iter++)
		{
			if(strSrcDiskSign.compare(iter->second.DiskSignature)==0)
			{
				DebugPrintf(SV_LOG_INFO,"Found the disk in node %s.\n",nodeName.c_str());
				iterDisk = iter;
				bFound = true;
				break;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFound;
}

/****************************************************************************************************
This function created the map of source scsi id and their respective disk signatures for the given host.
This will fetch the information from the host disk info map which are fetched from mbrdata file.
Input: hostname/vmname
output: Map of source scsi id to their respective disk signature.
*****************************************************************************************************/
bool MasterTarget::ReadSrcScsiIdAndDiskSignFromMap(string strVmName, map<string,string>& mapOfSrcScsiIdsAndSrcDiskSigns)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, DisksInfoMap_t>::iterator vmIter;
	vmIter = m_mapOfHostToDiskInfo.find(strVmName);
	if(vmIter == m_mapOfHostToDiskInfo.end())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find the disk information for %s \n", strVmName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	double dlmVersion = 0;
	if(m_mapOfHostIdDlmVersion.find(strVmName) != m_mapOfHostIdDlmVersion.end())
		dlmVersion = m_mapOfHostIdDlmVersion[strVmName];
	if(dlmVersion < 1.2)
	{
		DebugPrintf(SV_LOG_DEBUG,"Dlm version of %s MBR file is not compatible.\n",strVmName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	bool bp2v;
	map<string,bool>::iterator p2vIter= m_MapHostIdMachineType.find(strVmName);
	if(p2vIter != m_MapHostIdMachineType.end())
		bp2v = p2vIter->second;
	else
		bp2v = false;

	string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE"; 
	DisksInfoMapIter_t mapDiskIter = vmIter->second.begin();
	for(;mapDiskIter != vmIter->second.end(); mapDiskIter++)
	{
		string strDiskSignature = mapDiskIter->second.DiskSignature;
		stringstream temp;
		temp << mapDiskIter->second.DiskInfoSub.ScsiId.Channel;
		temp << ":";
		temp << mapDiskIter->second.DiskInfoSub.ScsiId.Target;
		string strScsiId;
		if(bp2v)
		{
			temp << ":";
			temp << mapDiskIter->second.DiskInfoSub.ScsiId.Lun;
			temp << ":";
			temp << mapDiskIter->second.DiskInfoSub.ScsiId.Host;
			strScsiId = temp.str();
		}
		else
			strScsiId = temp.str();
		DebugPrintf(SV_LOG_INFO,"strScsiId: %s strDiskSignature: %s\n",strScsiId.c_str(), strDiskSignature.c_str());
		mapOfSrcScsiIdsAndSrcDiskSigns.insert(make_pair(strScsiId, strDiskSignature));
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool MasterTarget::IsWin2k3OS(std::string hostId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s(%s)\n",__FUNCTION__,hostId.c_str());
	bool bRet = false;
	if(m_mapOfClusNodeOS.find(hostId)!=m_mapOfClusNodeOS.end())
	{
		if( m_mapOfClusNodeOS[hostId].find("Microsoft Windows Server 2003") != string::npos ||
			m_mapOfClusNodeOS[hostId].find("Windows_2003") != string::npos )
			bRet = true;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Host Id not found\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

//Clears the scsi reservation from the input disk
bool MasterTarget::ClrMscsScsiRsrv(int nDiskNum)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;

	string strScsiCmd = m_strInstallDirectoryPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("scsicmdutil.exe");
	string strDiskNumber;
	stringstream ss;
	ss << nDiskNum;
	ss >> strDiskNumber;

	strScsiCmd = strScsiCmd + string(" debug -disk ") + strDiskNumber + string(" scsi3clear");
	string strOurPutFile = m_strDataFolderPath + string("scsi_output.txt");

	DebugPrintf(SV_LOG_DEBUG,"Scsi reservation clearance command : %s\n", strScsiCmd.c_str());

	if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strScsiCmd, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to clear the scsi reservation for disk : %d\n", nDiskNum);
		bRet = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool MasterTarget::PauseAllReplicationPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	list<string>::iterator iter = m_listHost.begin();
	for(; iter != m_listHost.end(); iter++)
	{
		if(false == PauseReplicationPairs((*iter), m_strMtHostID))
		{
			bRetStatus = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}

bool MasterTarget::PauseReplicationPairs(const string& strSrcHostId, const string& strTgtHostId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	CxApiInfo cxapi;
	cxapi.strFunction = "PauseReplicationPairs";
	cxapi.strValue = "No";
	cxapi.mapParameters.insert(make_pair("SourceHostGUID", strSrcHostId));
	cxapi.mapParameters.insert(make_pair("TargetHostGUID", strTgtHostId));
	cxapi.listParamGroup.clear();

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDirPath.empty() && m_strDataFolderPath.empty())
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
	else if(m_strDirPath.empty())
		strCxApiResXmlFile = m_strDataFolderPath + "OutPutCxXml.xml";
	else
		strCxApiResXmlFile = m_strDirPath + "\\OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", strSrcHostId.c_str());
		DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
		return false;
	}

	string strRequestId;
	if(!ReadResponseXmlFile(strCxApiResXmlFile, "PauseReplicationPairs", strRequestId))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", strSrcHostId.c_str());
		bRetStatus = false;
	}
	else
	{
		do
		{
			if(false == GetRequestStatus(strRequestId, strStatus))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed while processing \"GetRequestStatus\" for host : %s\n", strSrcHostId.c_str());
				bRetStatus = false;
				break;
			}

			if(strStatus.compare("success") == 0)
				break;
			else if(strStatus.compare("failed") == 0)
			{
				bRetStatus = false;
				break;
			}
			else if(strStatus.compare("pending") == 0)
				Sleep(10000);
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Unknown error occured while queryign the status for pause the replication for host : %s\n", strSrcHostId.c_str());
				bRetStatus = false;
				break;
			}
		}while(1);

		if(bRetStatus)
			DebugPrintf(SV_LOG_INFO,"successfully completed the pause replication for host : %s\n", strSrcHostId.c_str());
		else
			DebugPrintf(SV_LOG_ERROR,"Failed to pause the replication for host : %s\n", strSrcHostId.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}

bool MasterTarget::ResumeAllReplicationPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	list<string>::iterator iter = m_listHost.begin();
	for(; iter != m_listHost.end(); iter++)
	{
		if(false == ResumeReplicationPairs((*iter), m_strMtHostID))
		{
			bRetStatus = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool MasterTarget::ResumeReplicationPairs(const string& strSrcHostId, const string& strTgtHostId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	CxApiInfo cxapi;
	cxapi.strFunction = "ResumeReplicationPairs";
	cxapi.strValue = "No";
	cxapi.mapParameters.insert(make_pair("SourceHostGUID", strSrcHostId));
	cxapi.mapParameters.insert(make_pair("TargetHostGUID", strTgtHostId));
	cxapi.listParamGroup.clear();

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDirPath.empty() && m_strDataFolderPath.empty())
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
	else if(m_strDirPath.empty())
		strCxApiResXmlFile = m_strDataFolderPath + "OutPutCxXml.xml";
	else
		strCxApiResXmlFile = m_strDirPath + "\\OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", strSrcHostId.c_str());
		DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
		return false;
	}

	if(!ReadResponseXmlFile(strCxApiResXmlFile, "ResumeReplicationPairs", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", strSrcHostId.c_str());
		bRetStatus = false;
	}
	else
	{
		if(strStatus.compare("success") == 0)
			DebugPrintf(SV_LOG_INFO,"successfully completed the resume replication for host : %s\n", strSrcHostId.c_str());
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to resume the replication for host : %s\n", strSrcHostId.c_str());
			bRetStatus = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}

bool MasterTarget::RestartResyncforAllPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	list<string>::iterator iter = m_listHost.begin();
	for(; iter != m_listHost.end(); iter++)
	{
		if(false == RestartResyncPairs((*iter), m_strMtHostID))
		{
			bRetStatus = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool MasterTarget::RestartResyncPairs(const string& strSrcHostId, const string& strTgtHostId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	CxApiInfo cxapi;
	cxapi.strFunction = "RestartResyncReplicationPairs";
	cxapi.strValue = "No";
	cxapi.mapParameters.insert(make_pair("ResyncRequired", "Yes"));
	cxapi.mapParameters.insert(make_pair("SourceHostGUID", strSrcHostId));
	cxapi.mapParameters.insert(make_pair("TargetHostGUID", strTgtHostId));
	cxapi.listParamGroup.clear();

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDirPath.empty() && m_strDataFolderPath.empty())
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
	else if(m_strDirPath.empty())
		strCxApiResXmlFile = m_strDataFolderPath + "OutPutCxXml.xml";
	else
		strCxApiResXmlFile = m_strDirPath + "\\OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", strSrcHostId.c_str());
		DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
		return false;
	}

	if(!ReadResponseXmlFile(strCxApiResXmlFile, "RestartResyncReplicationPairs", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", strSrcHostId.c_str());
		bRetStatus = false;
	}
	else
	{
		if(strStatus.compare("success") == 0)
			DebugPrintf(SV_LOG_INFO,"successfully completed the restart resync for the replications of host : %s\n", strSrcHostId.c_str());
		else
			DebugPrintf(SV_LOG_ERROR,"Failed to restart resync the replications for host : %s . Manually restart the resync for the pairs.\n", strSrcHostId.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool MasterTarget::GetRequestStatus(string strRequestId, string& strStatus)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	CxApiInfo cxapi;
	cxapi.strFunction = "GetRequestStatus";
	cxapi.strValue = "No";
	cxapi.mapParameters.clear();

	CxApiParamGroup paramgrp;
	paramgrp.strId = "RequestIdList";
	paramgrp.mapParameters.insert(make_pair("RequestId", strRequestId));
	paramgrp.lstCxApiParam.clear();

	cxapi.listParamGroup.push_back(paramgrp);

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDirPath.empty() && m_strDataFolderPath.empty())
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
	else if(m_strDirPath.empty())
		strCxApiResXmlFile = m_strDataFolderPath + "OutPutCxXml.xml";
	else
		strCxApiResXmlFile = m_strDirPath + "\\OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API \"GetRequestStatus\"\n");
		bRetStatus = false;
	}

	if(!ReadResponseXmlFile(strCxApiResXmlFile, "GetRequestStatus", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file\n");
		bRetStatus = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"successfully read the response xml file\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool MasterTarget::SendAlertsToCX(map<string, SendAlerts>& sendAlertsInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	CxApiInfo cxapi;
	cxapi.strFunction = "SendAlerts";
	cxapi.strValue = "No";

	map<string, SendAlerts>::iterator iter;
	for (iter = sendAlertsInfo.begin(); iter != sendAlertsInfo.end(); iter++)
	{
		CxApiParamGroup paramgrp, prmgrp;
		paramgrp.strId = iter->first;
		paramgrp.mapParameters = iter->second.mapParameters;

		prmgrp.strId = iter->second.mapParamGrp.begin()->first;
		prmgrp.mapParameters = iter->second.mapParamGrp.begin()->second;

		paramgrp.lstCxApiParam.push_back(prmgrp);
		cxapi.listParamGroup.push_back(paramgrp);
	}

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if (m_strDirPath.empty() && m_strDataFolderPath.empty())
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
	else if (m_strDirPath.empty())
		strCxApiResXmlFile = m_strDataFolderPath + "OutPutCxXml.xml";
	else
		strCxApiResXmlFile = m_strDirPath + "\\OutPutCxXml.xml";

	if (false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to process the request XML data for CX API call \"UpdateStatusToCX\" \n");
		DebugPrintf(SV_LOG_INFO, "Proceeding further without stopping the work flow for this operation\n");
		return false;
	}

	if (!ReadResponseXmlFile(strCxApiResXmlFile, "SendAlerts", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to read the response xml file for CX API call \"UpdateStatusToCX\" \n");
		bRetStatus = false;
	}
	else
	{
		if (strStatus.compare("success") == 0)
			DebugPrintf(SV_LOG_INFO, "successfully completed the update status to CX.\n");
		else
			DebugPrintf(SV_LOG_ERROR, "Failed to update the status to CX.\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bRetStatus;
}


bool MasterTarget::GetMapOfDiskSigToNumFromDinfo(const string strVmName, string strDInfofile, map<string, string>& mapTgtDiskSigToSrcDiskNum)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string strLineRead;
	std::string token("!@!@!");
		
	std::ifstream inFile(strDInfofile.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the file - %s\n",strDInfofile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	while(getline(inFile,strLineRead))
	{
		if(strLineRead.empty())
			continue;
		std::string strSrcDiskNum;
		std::string strTgtDiskId;
		std::size_t index;		
        DebugPrintf(SV_LOG_INFO,"Line read =  %s \n",strLineRead.c_str());
		index = strLineRead.find_first_of(token);
		if(index != std::string::npos)
		{
			strSrcDiskNum = strLineRead.substr(0,index);
			strTgtDiskId = strLineRead.substr(index+(token.length()),strLineRead.length());			
            DebugPrintf(SV_LOG_INFO,"SourceDiskNumber =  %s  :  TargetDiskSignature =  %s\n",strSrcDiskNum.c_str(),strTgtDiskId.c_str());            
			if((!strSrcDiskNum.empty()) && (!strTgtDiskId.empty()))
			{
				mapTgtDiskSigToSrcDiskNum.insert(make_pair(strTgtDiskId,strSrcDiskNum));
			}
		}
		else
		{
            DebugPrintf(SV_LOG_ERROR,"Line read =  %s\n",strLineRead.c_str());
			DebugPrintf(SV_LOG_ERROR,"Failed to find token in line read. Data Format is incorrect in file - %s.\n",strDInfofile.c_str());            
			inFile.close();
			mapTgtDiskSigToSrcDiskNum.clear();
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}
	inFile.close();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return true;
}


bool MasterTarget::GenerateDisksMap(string HostName, map<int, int> & MapSrcToTgtDiskNumbers)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;    

    do
    {
        map<string, int> SrcMapVolNameToDiskNumber;
		if(false == GetMapVolNameToDiskNumberFromDiskBin(HostName, SrcMapVolNameToDiskNumber))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volumes to disk numbers from Disk Binary file.\n");
			rv = false; break;
		}
		//}

        map<string, int> TgtMapVolNameToDiskNumber;
        map<string, string> MapSrcVolToTgtVol;

		//search in pinfo file
        if(false == GetMapVolNameToDiskNumber(HostName, TgtMapVolNameToDiskNumber, MapSrcVolToTgtVol)) 
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volumes to disk numbers from pInfo file.\n");
            rv = false; break;
        }

        // Generate required map from above three maps
        map<string, string>::iterator iterVol = MapSrcVolToTgtVol.begin();
        for(; iterVol != MapSrcVolToTgtVol.end(); iterVol++)
        {
            // iterVol->first = source vol             
            map<string, int>::iterator iterSrcMap = SrcMapVolNameToDiskNumber.find(iterVol->first);
            if(iterSrcMap == SrcMapVolNameToDiskNumber.end())
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the source volume[%s] in source map of volume names to disk numbers.\n",iterVol->first.c_str());
                rv = false; break;
            }

            // iterVol->second = target vol
            map<string, int>::iterator iterTgtMap = TgtMapVolNameToDiskNumber.find(iterVol->second);
            if(iterTgtMap == TgtMapVolNameToDiskNumber.end())
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to find the target volume[%s] in target map of volume names to disk numbers.\n",iterVol->second.c_str());
                rv = false; break;
            }
            
            //(Key,Value) = (Source,Target) Disk Number
            MapSrcToTgtDiskNumbers.insert(make_pair(iterSrcMap->second, iterTgtMap->second));
        }   
        if(false == rv)
            break;

        if(MapSrcToTgtDiskNumbers.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "Map of source to target disk numbers is empty for VM - %s\n", HostName.c_str());
            rv = false; break;       
        }
        else
        {
            map<int, int>::iterator i = MapSrcToTgtDiskNumbers.begin();
            for(; i!=MapSrcToTgtDiskNumbers.end(); i++)
            {
                DebugPrintf(SV_LOG_INFO,"Source::Target(Disk Numbers) ==> %u :: %u\n", i->first, i->second);
            }
        }

    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool MasterTarget::GetMapVolNameToDiskNumberFromDiskBin(string HostName, map<string, int> & SrcMapVolNameToDiskNumber)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;    

    string DiskBinFileName = m_strDataFolderPath + HostName + string("_disk.bin");
    FILE *f = fopen(DiskBinFileName.c_str(),"rb");
	if(!f)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s with Error: %ld \n",DiskBinFileName.c_str(), GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    DiskInformation * pDiskInfo;
    pDiskInfo = new DiskInformation;
    memset(pDiskInfo,0,sizeof(*pDiskInfo));
    while(fread(pDiskInfo,sizeof(*pDiskInfo),1,f))
    {
        DebugPrintf(SV_LOG_DEBUG,"========================================================\n");                
        DebugPrintf(SV_LOG_DEBUG," Disk Id                      = %u\n",pDiskInfo->uDiskNumber);
        DebugPrintf(SV_LOG_DEBUG," Disk Type                    = %d\n",pDiskInfo->dt);
        DebugPrintf(SV_LOG_DEBUG," Disk No of Actual Partitions = %lu\n",pDiskInfo->dwActualPartitonCount);
        DebugPrintf(SV_LOG_DEBUG," Disk No of Partitions        = %lu\n",pDiskInfo->dwPartitionCount);
        DebugPrintf(SV_LOG_DEBUG," Disk Partition Style         = %d\n",pDiskInfo->pst);
        DebugPrintf(SV_LOG_DEBUG," Disk Signature               = %lu\n",pDiskInfo->ulDiskSignature);
        DebugPrintf(SV_LOG_DEBUG," Disk Size                    = %I64d\n",pDiskInfo->DiskSize.QuadPart);

        PartitionInfo * ArrPartInfo;
        ArrPartInfo = new PartitionInfo [pDiskInfo->dwActualPartitonCount];
        if(NULL == ArrPartInfo)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Cannot create Partition Information structures. Cannot continue!\n");
		    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            delete pDiskInfo;
		    return false;
	    }
        //Fetch partitions on the disk
	    for(unsigned int i = 0; i < pDiskInfo->dwActualPartitonCount; i++)
	    {			    
		    fread(&ArrPartInfo[i],sizeof(ArrPartInfo[i]),1,f);
            DebugPrintf(SV_LOG_DEBUG,"\n  Start of Partition : %d\n  ------------------------\n",i+1);
            DebugPrintf(SV_LOG_DEBUG,"  Partition Disk Number       = %u\n",ArrPartInfo[i].uDiskNumber);
            DebugPrintf(SV_LOG_DEBUG,"  Partition Partition Type    = %d\n",ArrPartInfo[i].uPartitionType);
            DebugPrintf(SV_LOG_DEBUG,"  Partition Boot Indicator    = %d\n",ArrPartInfo[i].iBootIndicator);
            DebugPrintf(SV_LOG_DEBUG,"  Partition No of Volumes     = %u\n",ArrPartInfo[i].uNoOfVolumesInParttion);
            DebugPrintf(SV_LOG_DEBUG,"  Partition Partition Length  = %I64d\n",ArrPartInfo[i].PartitionLength.QuadPart);
            DebugPrintf(SV_LOG_DEBUG,"  Partition Start Offset      = %I64d\n",ArrPartInfo[i].startOffset.QuadPart);
            DebugPrintf(SV_LOG_DEBUG,"  Partition Ending Offset     = %I64d\n",ArrPartInfo[i].EndingOffset.QuadPart);

	        volInfo * ArrVolInfo;
            ArrVolInfo = new volInfo [ArrPartInfo[i].uNoOfVolumesInParttion];
		    if(NULL == ArrVolInfo)
		    {
			    DebugPrintf(SV_LOG_ERROR,"Cannot create Volume Information Structures. Cannot continue!\n");
			    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                delete[] ArrPartInfo;
                delete pDiskInfo;
			    return false;
		    }
            //Fetch volumes on each partition.
		    for(unsigned int j = 0; j < ArrPartInfo[i].uNoOfVolumesInParttion; j++)
		    {
			    fread(&ArrVolInfo[j],sizeof(ArrVolInfo[j]),1,f);              
                DebugPrintf(SV_LOG_DEBUG,"\n   Start of Volume : %d\n   ---------------------\n",j+1);
	            DebugPrintf(SV_LOG_DEBUG,"   Volume Volume Name         = %s\n",ArrVolInfo[j].strVolumeName);
	            DebugPrintf(SV_LOG_DEBUG,"   Volume Partition Length    = %I64d\n",ArrVolInfo[j].partitionLength.QuadPart);
	            DebugPrintf(SV_LOG_DEBUG,"   Volume Starting Offset     = %I64d\n",ArrVolInfo[j].startingOffset.QuadPart);
	            DebugPrintf(SV_LOG_DEBUG,"   Volume Ending Offset       = %I64d\n",ArrVolInfo[j].endingOffset.QuadPart);	            
                SrcMapVolNameToDiskNumber.insert(make_pair(ArrVolInfo[j].strVolumeName, (int)pDiskInfo->uDiskNumber));
            }           
            delete[] ArrVolInfo;
	    }
        delete[] ArrPartInfo;
    }
    delete pDiskInfo;
    DebugPrintf(SV_LOG_INFO,"------------------------------------------------------------\n");

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


//Will get this info from Pinfo file
bool MasterTarget::GetMapVolNameToDiskNumber(string HostName,
                                             map<string, int> & TgtMapVolNameToDiskNumber,
                                             map<string, string> & MapSrcVolToTgtVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
    do
    {
		MapSrcVolToTgtVol = getProtectedDrivesMap(HostName);
        if(MapSrcVolToTgtVol.empty())
        {
            DebugPrintf(SV_LOG_ERROR,"Protected Pairs are empty for the VM - %s\n", HostName.c_str());
            rv = false; break;
        }

        map<string, string>::iterator i = MapSrcVolToTgtVol.begin();
        for(; i!= MapSrcVolToTgtVol.end(); i++)
        {
            // i->first  : Src Vol; (C:\)
            // i->second : Tgt Vol; (C:\ESX\HostName_C)
            int dn;
            if(false == GetDiskNumberOfVolume(i->second, dn))
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to get Disk Number of the volume - %s\n",i->second.c_str());
                rv = false; break;
            }
            DebugPrintf(SV_LOG_INFO,"dn = %u\n",dn);
            TgtMapVolNameToDiskNumber.insert(make_pair(i->second,dn));
        }
    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


std::map<std::string,std::string> MasterTarget::getProtectedDrivesMap(std::string &strVmName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::map<std::string,std::string> replicatedDrivesMap;
    std::string strReplicatedVmsInfoFile;
	std::string strLineRead;
	std::string token("!@!@!");

	strReplicatedVmsInfoFile = m_strDataFolderPath + strVmName + string(".pInfo");
	
	std::ifstream inFile(strReplicatedVmsInfoFile.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the file - %s\n",strReplicatedVmsInfoFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return replicatedDrivesMap;
	}
	while(getline(inFile,strLineRead))
	{
		if(strLineRead.empty())
			continue;
		std::string strSrcVolName;
		std::string strTgtVolName;
		std::size_t index;		
        DebugPrintf(SV_LOG_INFO,"Line read =  %s \n",strLineRead.c_str());
		index = strLineRead.find_first_of(token);
		if(index != std::string::npos)
		{
			strSrcVolName = strLineRead.substr(0,index);
			strTgtVolName = strLineRead.substr(index+(token.length()),strLineRead.length());			
            DebugPrintf(SV_LOG_INFO,"SrcVolName =  %s  :  Target volume =  %s\n",strSrcVolName.c_str(),strTgtVolName.c_str());            
			if((!strSrcVolName.empty()) && (!strTgtVolName.empty()))
			{
				replicatedDrivesMap.insert(make_pair(strSrcVolName,strTgtVolName));
			}
		}
		else
		{
            DebugPrintf(SV_LOG_ERROR,"Line read =  %s\n",strLineRead.c_str());
			DebugPrintf(SV_LOG_ERROR,"Failed to find token in line read. Data Format is incorrect in file - %s.\n",strReplicatedVmsInfoFile.c_str());            
			inFile.close();
			replicatedDrivesMap.clear();
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return replicatedDrivesMap;
		}
	}
	inFile.close();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return replicatedDrivesMap;
}


bool MasterTarget::GetDiskNumberOfVolume(string VolName, int & dn)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
    do
    {
        // Fetch volume GUID
        char VolGuid[MAX_PATH];
        memset(VolGuid,0,MAX_PATH);
        if(0 == GetVolumeNameForVolumeMountPoint(VolName.c_str(), VolGuid, MAX_PATH))
		{            
            DebugPrintf(SV_LOG_ERROR,"Failed to get volume GUID for the volume[%s] with Errorcode : [%lu].\n", VolName.c_str(), GetLastError());
			rv = false;	break;                   
		}

        DebugPrintf(SV_LOG_INFO,"Before : %s ==> %s\n", VolName.c_str(), VolGuid); 
        // Remove the trailing "\" from volume GUID
        std::string strTemp = string(VolGuid);        
        std::string::size_type pos = strTemp.find_last_of("\\");
        if(pos != std::string::npos)
        {
            strTemp.erase(pos);
        }
        DebugPrintf(SV_LOG_INFO,"After : %s ==> %s\n", VolName.c_str(), strTemp.c_str()); 

        HANDLE	hVolume;	
		hVolume = CreateFile(strTemp.c_str(),GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		if( hVolume == INVALID_HANDLE_VALUE ) 
		{
            //Note - in creating handle vol guid is passed but to understand error displaying vol name below.
			DebugPrintf(SV_LOG_ERROR,"Failed to open handle for volume[%s] with Error Code : [%lu].\n", VolName.c_str(), GetLastError());
			rv = false;	break;
		}

		ULONG	bytesWritten;
        UCHAR	DiskExtentsBuffer[0x400];
        PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
		if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer), &bytesWritten, NULL)) 
		{	            
			for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
                dn = (int)DiskExtents->Extents[extent].DiskNumber;                
			}                      
		}
		else
		{
            DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed for volume[%s] with Error code : [%lu].\n", VolName.c_str(), GetLastError());
			CloseHandle(hVolume);
            rv = false;	break;
		}
		CloseHandle(hVolume);
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


string MasterTarget::PrepareCXAPIXmlData(CxApiInfo& cxapi)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strRqstId = GetRequestId();

	string xml_data = string("<Request Id=\"") + strRqstId + string("\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AuthMethod>ComponentAuth_V2015_01</AuthMethod>");
	xml_data = xml_data + string("<AccessKeyID>") + getInMageHostId() + string("</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature>") + GetAccessSignature("POST", "/ScoutAPI/CXAPI.php", cxapi.strFunction, strRqstId, "1.0") + string("</AccessSignature>");
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"")+ cxapi.strFunction + string("\" Include=\"") + cxapi.strValue + string("\">");
	map<string, string>::iterator iter = cxapi.mapParameters.begin();
	for(; iter != cxapi.mapParameters.end(); iter++)
	{
		xml_data = xml_data + string("<Parameter Name=\"") + iter->first + string("\" Value=\"") + iter->second + string("\" />");
	}

	if(!cxapi.listParamGroup.empty())
	{
		xml_data = xml_data + ProcessParamGroup(cxapi.listParamGroup);
	}

	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");

	DebugPrintf(SV_LOG_DEBUG,"Xml Data to Post in CX \n\n%s\n\n",xml_data.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}


string MasterTarget::ProcessParamGroup(list<CxApiParamGroup>& listParamGroup)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string xml_data;
	
	list<CxApiParamGroup>::iterator iterList = listParamGroup.begin();
	
	for(; iterList != listParamGroup.end(); iterList++)
	{
		xml_data = xml_data + string("<ParameterGroup Id=\"") + iterList->strId + string("\">");
		map<string, string>::iterator iter = iterList->mapParameters.begin();
		for(;iter != iterList->mapParameters.end(); iter++)
		{
			xml_data = xml_data + string("<Parameter Name=\"") + iter->first + string("\" Value=\"") + iter->second + string("\" />");
		}
		if(!iterList->lstCxApiParam.empty())
		{
			xml_data = xml_data + ProcessParamGroup(iterList->lstCxApiParam);
		}
		xml_data = xml_data + string("</ParameterGroup>");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}


bool MasterTarget::ReadResponseXmlFile(string strCxApiResponseFile, string strCxApi, string& strStatus)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;

	do
	{
		xDoc = xmlParseFile(strCxApiResponseFile.c_str());
		if (xDoc == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
			bResult = false;
			break;
		}

		//Found the doc. Now process it.
		xCur = xmlDocGetRootElement(xDoc);
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is empty. Cannot Proceed further.\n");
			bResult = false;
			break;
		}
		if (xmlStrcasecmp(xCur->name,(const xmlChar*)"Response")) 
		{
			//Response is not found
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Response> entry not found\n");
			bResult = false;
			break;
		}

		xmlChar *attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"Message");
		if(attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out the <Response Message=\"\"> from xml file\n");
			bResult = false;
			break;
		}
		strStatus = std::string((char *)attr_value_temp);
		DebugPrintf(SV_LOG_DEBUG,"Status at <Response> : %s\n", strStatus.c_str());

		//If you are here Response entry is found. Go for Body entry.
		xCur = xGetChildNode(xCur,(const xmlChar*)"Body");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Body> entry not found\n");
			bResult = false;
			break;		
		}
		//If you are here Body entry is found. Go for Function entry with required Function name.
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Function",(const xmlChar*)"Name", (const xmlChar*)(strCxApi.c_str()));
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Function with required Name> entry not found\n");
			bResult = false;
			break;		
		}
		attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"Message");
		if(attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out the <Response Message=\"\"> from xml file\n");
			bResult = false;
			break;
		}
		strStatus = std::string((char *)attr_value_temp);
		DebugPrintf(SV_LOG_DEBUG,"Status at <Function> : %s\n", strStatus.c_str());

		xCur = xGetChildNode(xCur,(const xmlChar*)"FunctionResponse");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <FunctionResponse> entry not found\n");
			bResult = false;
			break;		
		}

		if(strCxApi.compare("PauseReplicationPairs") == 0)
		{
			if(false == xGetValueForProp(xCur, "RequestId", strStatus))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Parameter Name \"RequestId\"\n");
				bResult = false;
				break;
			}
			DebugPrintf(SV_LOG_DEBUG,"RequestId: %s for pause request\n",strStatus.c_str());
		}
		else
		{
			string strOption = "Status";
			if(strCxApi.compare("ResumeReplicationPairs") == 0)
			{
				strOption = "ResumeStatus";
			}
			else if(strCxApi.compare("RestartResyncReplicationPairs") == 0)
			{
				strOption = "ResyncStatus";
			}
			if(false == xGetValueForProp(xCur, strOption, strStatus))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to get the value for Paramater Name : %s\n", strOption.c_str());
				bResult = false;
				break;
			}
			DebugPrintf(SV_LOG_DEBUG,"Status: %s \n",strStatus.c_str());
		}
	}while(0);

	if(!bResult)
	{
		strStatus = "failed";
		xmlFreeDoc(xDoc);
	}

	for(size_t i = 0; i < strStatus.length(); i++)
		strStatus[i] = tolower(strStatus[i]);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool MasterTarget::CreateHostToDynDiskAddOption()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	bool bAddDyn = false;

	map<string, DisksInfoMap_t>::iterator iterDiskInfo;
	map<string, map<int, int> >::iterator iter = m_mapOfVmToSrcAndTgtDiskNmbrs.begin();
	for(; iter != m_mapOfVmToSrcAndTgtDiskNmbrs.end(); iter++)
	{
		bAddDyn = false;
		iterDiskInfo = m_mapOfHostToDiskInfo.find(iter->first);
		if(iterDiskInfo != m_mapOfHostToDiskInfo.end())
		{
			map<int, int>::iterator iterMap = iter->second.begin();
			for(; iterMap != iter->second.end(); iterMap++)
			{
				string strPhysicalDeviceName = "\\\\.\\PHYSICALDRIVE";
				string diskNum;
				stringstream s;
				s << iterMap->first;
				s >> diskNum;
				diskNum = strPhysicalDeviceName + diskNum;
				
				DisksInfoMap_t::iterator iterDisk = iterDiskInfo->second.find(diskNum);
				if(DYNAMIC == iterDisk ->second.DiskInfoSub.Type)
				{
					bAddDyn = true;
				}
			}
		}
		m_mapHostIdDynAddDisk.insert(make_pair(iter->first, bAddDyn));
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

void MasterTarget::UpdateSendAlerts(taskInfo_t& taskInfo, string& strCxPath, map<string, SendAlerts>& sendAlertsInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	map<string, string> mapGrp;
	SendAlerts sendAlert;

	sendAlert.mapParameters["Category"] = "VCON_ERROR";  //Current default value.
	sendAlert.mapParameters["HostId"] = getInMageHostId();
	mapGrp["XmlFilePath"] = strCxPath;

	if (taskInfo.TaskStatus.compare(INM_VCON_TASK_FAILED) == 0)
	{
		sendAlert.mapParameters["EventCode"] = "VE0703";
		sendAlert.mapParameters["Message"] = "Protection failed";
        sendAlert.mapParameters["Severity"] = "ERROR";
	}
	else
	{
		sendAlert.mapParameters["EventCode"] = "VA0601";
		sendAlert.mapParameters["Message"] = "Successfully completed the protection";
        sendAlert.mapParameters["Severity"] = "NOTICE";
	}
	sendAlert.mapParamGrp["PlaceHolders"] = mapGrp;
	sendAlertsInfo["1"] = sendAlert;

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void MasterTarget::UpdateSendAlerts(map<string, SendAlerts>& mapSendAlertsInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	
	int nCounter = 1;
	for (map<string, statusInfo_t>::iterator iter = m_mapHostIdToStatus.begin(); iter != m_mapHostIdToStatus.end(); iter++, nCounter++)
	{
		string strHostName = iter->first;
		map<string, string>::iterator iterMap = m_mapHostIdToHostName.find(iter->first);
		if (!iterMap->second.empty())
			strHostName = iterMap->second;

		map<string, string> mapGrp;
		SendAlerts sendAlertsInfo;
		sendAlertsInfo.mapParameters["Category"] = "VCON_ERROR";  //Current default value.
		sendAlertsInfo.mapParameters["HostId"] = iter->first;
		mapGrp["HostName"] = strHostName;
		if (iter->second.Status.compare(INM_VCON_TASK_FAILED) == 0)
		{
			sendAlertsInfo.mapParameters["EventCode"] = "VE0701";
			sendAlertsInfo.mapParameters["Message"] = "Protection failed for host : " + strHostName;
            sendAlertsInfo.mapParameters["Severity"] = "ERROR";
		}
		else
		{
			sendAlertsInfo.mapParameters["EventCode"] = "VA0601";
			sendAlertsInfo.mapParameters["Message"] = "Successfully completed the protection for host : " + strHostName;
            sendAlertsInfo.mapParameters["Severity"] = "NOTICE";
		}
		sendAlertsInfo.mapParamGrp["PlaceHolders"] = mapGrp;

		stringstream ss;
		ss << nCounter;
		mapSendAlertsInfo[ss.str()] = sendAlertsInfo;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void MasterTarget::UpdateSrcHostStaus()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	for (list<string>::iterator iter = m_listHost.begin(); iter != m_listHost.end(); iter++)
	{
		statusInfo_t statusInfo;
		statusInfo.HostId = *iter;
		statusInfo.Status = "Initiated";
		statusInfo.ErrorMsg = "";
		statusInfo.ExecutionStep = "";
		statusInfo.Resolution = "";
		statusInfo.Job = "Protection";

		m_mapHostIdToStatus[*iter] = statusInfo;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void MasterTarget::UpdateSrcHostStaus(taskInfo_t & taskInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for (; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for (; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			if (iterTask->TaskNum.compare(taskInfo.TaskNum) == 0)
			{
				if (!taskInfo.TaskErrMsg.empty())
				{
					for (int i = 0; i < taskInfo.TaskErrMsg.size(); i++)
					{
						if (taskInfo.TaskErrMsg.find("\"") != string::npos)
							taskInfo.TaskErrMsg.replace(taskInfo.TaskErrMsg.find("\""), 1, "'");
					}
				}
				for (map<string, statusInfo_t>::iterator iter = m_mapHostIdToStatus.begin(); iter != m_mapHostIdToStatus.end(); iter++)
				{
					iter->second.ErrorMsg = taskInfo.TaskErrMsg;
					iter->second.ExecutionStep = iterTask->TaskName;
					iter->second.Status = taskInfo.TaskStatus;
				}
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

bool MasterTarget::DownloadDlmMbrFileForSrcHost(string strHostId, string strMbrUrl)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	do
	{
		string strMbrLocalPath = m_strDataFolderPath + string("\\") + strHostId + string("_mbrdata");
		if(!m_bPhySnapShot)
		{
			size_t pos = strMbrUrl.find("SourceMBR");
			if(pos == string::npos)
			{
				DebugPrintf(SV_LOG_ERROR, "SourceMBR path does not found from the given MBR url : %s for host: %s", strMbrUrl.c_str(), strHostId.c_str());
				bResult = false;
				break;
			}
			string strMbrFileResume = strMbrLocalPath + string(".resume");
			if((true == checkIfFileExists(strMbrLocalPath)) && (m_strOperation.compare("resume") == 0))
			{
				if(false==SVCopyFile(strMbrLocalPath.c_str(),strMbrFileResume.c_str(),false))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to take the back up of file %s with Error [%lu].\n",strMbrLocalPath.c_str(),GetLastError());
				}
				else
					DeleteFile(strMbrLocalPath.c_str());
			}
			if(false == DownloadFileFromCx(strMbrUrl, strMbrLocalPath))
			{
				if(m_strOperation.compare("resume") == 0)
				{
					DebugPrintf(SV_LOG_INFO, "MBR File %s for host %s already exists, As this is Resume protection proceeding further...", strMbrLocalPath.c_str(), strHostId.c_str());
					if(false==SVCopyFile(strMbrFileResume.c_str(),strMbrLocalPath.c_str(),false))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to take the back up of file %s with Error [%lu].\n",strMbrFileResume.c_str(),GetLastError());
						bResult = false;
						break;
					}
					else
						DeleteFile(strMbrFileResume.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to download the MBR for host %s", strHostId.c_str());
					bResult = false;
					break;
				}
			}
		}
		
		if(true == checkIfFileExists(strMbrLocalPath))
		{
			DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
			DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrLocalPath.c_str(), srcMapOfDiskNamesToDiskInfo);
			if(DLM_ERR_SUCCESS != RetVal)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrLocalPath.c_str(), RetVal);
				bResult = false;
				break;
			}
			if(!m_bPhySnapShot)
			{
				if(false == DownloadPartitionFile(strMbrUrl, strMbrLocalPath, srcMapOfDiskNamesToDiskInfo))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to download the partition files for host %s", strHostId.c_str());
					bResult = false;
					break;
				}
			}
			m_mapOfHostToDiskInfo.insert(make_pair(strHostId, srcMapOfDiskNamesToDiskInfo));
			
			double dlmVersion = 0;
			RetVal = GetDlmVersion(strMbrLocalPath.c_str(), dlmVersion);
			if(DLM_ERR_SUCCESS != RetVal)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the dlm version.");
			}
			m_mapOfHostIdDlmVersion[strHostId] = dlmVersion;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "File %s does not exist, considering it as upgrade setup\n", strMbrLocalPath.c_str());
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool MasterTarget::UnmountProtectedVolumes()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bResult = true;

	for (list<string>::iterator iter = m_listHost.begin(); iter != m_listHost.end(); iter++)
	{
		map<string, string> mapPairs;
		string strInfoFile = m_strDataFolderPath + (*iter) + string(VOLUMESINFO_EXTENSION);
		if (false == ReadFileWith2Values(strInfoFile, mapPairs))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to fetch the map of Replicated Volumes for VM : %s. Can't unmount volumes for this host\n", iter->c_str());
			bResult = false;
		}
		else
		{
			map<string, string>::iterator iterMapPair = mapPairs.begin();
			for (; iterMapPair != mapPairs.end(); iterMapPair++)
			{
				DebugPrintf(SV_LOG_INFO, "Unmount = %s\n", iterMapPair->second.c_str());
				if (false == DeleteVolumeMountPoint(iterMapPair->second.c_str()))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to unmount the volume with above mount point with Error : [%lu].\n", GetLastError());
					bResult = false;
				}
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bResult;
}

bool MasterTarget::UpdatePrepareTargetStatusInFile(string strResultFile, statusInfo_t& statInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bReturn = true;
	
	VMsPrepareTargetUpdateInfo vmsPrepareTargetInfo;
	if (m_prepTgtVmsInfo.m_VmsInfo.empty())
	{
		vmsPrepareTargetInfo.m_updateProps[ERROR_MESSAGE] = statInfo.ErrorMsg;
		vmsPrepareTargetInfo.m_updateProps[ERROR_CODE] = statInfo.ErrorCode;

		PlaceHolder placeHolder;
		placeHolder.m_Props = statInfo.PlaceHolder.Properties;
		vmsPrepareTargetInfo.m_PlaceHolderProps[PLACE_HOLDER] = placeHolder;
	}
	else
	{
		vmsPrepareTargetInfo.m_updateProps[ERROR_MESSAGE] = "Protection couldn't be enabled.";
		map<string, SrcCloudVMDisksInfo>::const_iterator iterVM = m_prepTgtVmsInfo.m_VmsInfo.begin();
		while (iterVM != m_prepTgtVmsInfo.m_VmsInfo.end())
		{
			PrepareTargetVMInfo prepareTgtVmInfo;
			prepareTgtVmInfo.m_Props[ERROR_MESSAGE] = statInfo.ErrorMsg;
			prepareTgtVmInfo.m_Props[ERROR_CODE] = statInfo.ErrorCode;

			PlaceHolder placeHolder;
			placeHolder.m_Props = statInfo.PlaceHolder.Properties;
			prepareTgtVmInfo.m_PlaceHolderProps[PLACE_HOLDER] = placeHolder;
			vmsPrepareTargetInfo.m_MapProtectedVMs[iterVM->first] = prepareTgtVmInfo;
			iterVM++;
		}
	}

	

	if (!strResultFile.empty())
	{
		ofstream out(strResultFile.c_str());
		if (out.good())
		{
			try
			{
				out << CxArgObj<VMsPrepareTargetUpdateInfo>(vmsPrepareTargetInfo);
			}
			catch (std::exception &e)
			{
				DebugPrintf(SV_LOG_ERROR, "Not able to serialize Vms recovery information. Exception: %s\n", e.what());
				bReturn = false;
			}
			catch (...)
			{
				DebugPrintf(SV_LOG_ERROR, "Not able to serialize Vms recovery information. Unknown Exception\n");
				bReturn = false;
			}
			out.close();
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open file for reading. File : %s\n", strResultFile.c_str());
			bReturn = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Result file path is missing.");
		bReturn = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bReturn;
}

void MasterTarget::UpdateErrorStatusStructure(const std::string& strErrorMsg, const std::string& strErrorCode)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	m_statInfo.Status = "2";
	m_statInfo.ErrorMsg = strErrorMsg;
	m_statInfo.ErrorCode = strErrorCode;
	m_statInfo.PlaceHolder = m_PlaceHolder;
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

bool MasterTarget::RemoveRecoveryStatusFile()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bReturn = true;

	map<string, SrcCloudVMDisksInfo>::const_iterator iterVM = m_prepTgtVmsInfo.m_VmsInfo.begin();
	while (iterVM != m_prepTgtVmsInfo.m_VmsInfo.end())
	{
		string strFileName = m_strDataFolderPath + iterVM->first + RECOVERY_PROGRESS;
		if (!checkIfFileExists(strFileName))
		{
			DebugPrintf(SV_LOG_DEBUG, "File [%s] does not exist, ignoring the deletion.\n", strFileName.c_str());
		}
		else
		{
			if (0 == DeleteFile(strFileName.c_str()))
			{
				DebugPrintf(SV_LOG_WARNING, "DeleteFile failed - (%s). Error code - %lu\n", strFileName.c_str(), GetLastError());
				bReturn = false;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "SuccessFully deleted file - %s\n", strFileName.c_str());
			}
		}
		iterVM++;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bReturn;
}

// A2E failback support

// 
// Notes: This routine has been copied from PrepareTargetDisks 
//        and modified for A2E failback scenario
//

int MasterTarget::PrepareTargetDisksForFailback(const string & strConfFile, const string & strResultFile)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	int iResult = 0;

	do {

		m_vConVersion = 4.2;
		string strErrorMsg, strErrorCode;

		m_strMtHostID = getInMageHostId();
		m_strMtHostName = stGetHostName();

		if (m_strMtHostID.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to fetch InMage Host id of Master target.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0501";
			UpdateErrorStatusStructure("Error while parsing the drscout.conf file from the Master Target " + m_strMtHostName, "EA0501");
			iResult = 1;
			break;
		}

		if (0 != getAgentInstallPath())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to find the Agent Install Path.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0502";
			UpdateErrorStatusStructure("Failed to get Mobility service installation path from " + m_strMtHostName, "EA0502");
			iResult = 1;
			break;
		}

		if (!m_bPhySnapShot)
		{
			//disables the automount for newly created volumes.
			if (false == DisableAutoMount())
			{
				DebugPrintf(SV_LOG_ERROR, "Error: Failed to disable the automount for newly created volumes.\n");
				DebugPrintf(SV_LOG_INFO, "Info  :Manually delete the volume names at the Master Target if not required.\n");
			}
		}

		//Read configuration information from configfile
		if (strConfFile.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Config file path is missing. Can not prepare target disks for protection.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0503";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0503");
			iResult = 1;
			break;
		}

		if (false == GetVmDisksInfoFromConfigFile(strConfFile))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get the VM Disks information from config file.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0504";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0504");
			iResult = 1;
			break;
		}


		//Removes the recoveryprogress status file which was created in earlier recovery operaions
		RemoveRecoveryStatusFile();

		//Stop this process to avoid the format popups.
		if (false == StopService("ShellHWDetection"))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to stop the service - ShellHWDetection.\n");
			DebugPrintf(SV_LOG_WARNING, "Please cancel all the popups of format volumes if observed.\n");
		}



		// A2E failback specific code - start

		//
		// step 1 : rescan the bus
		// step 2 : remove missing disks
		// step 3 : collect all the disks on the system
		// step 4 : for each  source vm
		//  DetectAndInitializeTargetDisks
		//    find the corresponding target disks 
		//    initialize them i.e make them offline RW and convert to raw disk
		// step 5: remove missing disks
		//

		
		if (FALSE == rescanIoBuses())
		{
			DebugPrintf(SV_LOG_ERROR, "SCSI Bus rescan to detect attached disks failed.\n");
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0508";
			UpdateErrorStatusStructure("Master target server " + m_strMtHostName + " couldn't detect newly attached disks.", "EA0508");
			iResult = 1;
			break;
		}

		RemoveMissingDisks();

		DiskNamesToDiskInfosMap diskNamesToDiskInfosMap;
		CollectAllDiskInformation(diskNamesToDiskInfosMap);
		display_devlist(diskNamesToDiskInfosMap);
		
		std::map<std::string, SrcCloudVMDisksInfo>::iterator m_VmsIter = m_prepTgtVmsInfo.m_VmsInfo.begin();
		for (; m_VmsIter != m_prepTgtVmsInfo.m_VmsInfo.end(); ++m_VmsIter)
		{
			std::map<std::string, std::string> srctgtLunMap = m_VmsIter->second.m_vmSrcTgtDiskLunMap;
			std::map<std::string, std::string> srctgtDiskMap;
			if (DetectAndInitializeTargetDisks(diskNamesToDiskInfosMap, m_VmsIter->first, srctgtLunMap, srctgtDiskMap)){
				m_prepTgtVMsUpdate.m_volumeMapping.insert(std::make_pair(m_VmsIter->first, srctgtDiskMap));
			}
			else{
				//DebugPrintf(SV_LOG_ERROR, "Some of the disks are missing on MT.\n");
				//m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
				//m_PlaceHolder.Properties[ERROR_CODE] = "EA0508";
				//UpdateErrorStatusStructure("Master target server " + m_strMtHostName + " couldn't detect newly attached disks.", "EA0508");
				iResult = 1;
				break;
			}
		}


		RemoveMissingDisks();

		// A2E failback specific code - end


		//Persis the result to the result file.
		if (false == PersistPrepareTargetVMDisksResult(strResultFile))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to persist prepare target result to the file %s.\n", strResultFile.c_str());
			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0516";
			UpdateErrorStatusStructure("Protection failed with an internal error on Master Target " + m_strMtHostName, "EA0516");
			iResult = 1;
			break;
		}
		//Make a Register-Host to update the newly created volumes to CX.
		if (!m_bPhySnapShot)
		{
			DebugPrintf(SV_LOG_DEBUG, "Registering the the host with cx to update the newly created volumes\n");
			RegisterHostUsingCdpCli();
		}

	} while (false);

	if (1 == iResult)
	{
		if (!UpdatePrepareTargetStatusInFile(strResultFile, m_statInfo))
			DebugPrintf(SV_LOG_ERROR, "Serialization of Protected vms informaton failed.\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return iResult;
}


bool MasterTarget::RemoveMissingDisks()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	CVdsHelper objVds;
	try
	{
		if (objVds.InitilizeVDS())
			objVds.RemoveMissingDisks();
		objVds.unInitialize();
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed while trying to remove the missing disks\n");
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

bool MasterTarget::CollectAllDiskInformation(DiskNamesToDiskInfosMap & diskNamesToDiskInfosMap)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;


	if (S_FALSE == InitCOM())
	{
		DebugPrintf(SV_LOG_ERROR, "%s InitCom() Failed.\n", __FUNCTION__);
		return false;
	}

	WmiDiskRecordProcessor p(&diskNamesToDiskInfosMap);
	GenericWMI gwmi(&p);
	SVERROR sv = gwmi.init();
	if (sv != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR, "initializating generic wmi failed for disk drive\n");
	}
	else
	{
		gwmi.GetData("Win32_DiskDrive");
	}
	
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

bool MasterTarget::DetectAndInitializeTargetDisks(const DiskNamesToDiskInfosMap &diskNamesToDiskInfosMap,
	const std::string &srcVm, 
	const std::map<std::string, std::string> & srctgtLuns, 
	std::map<std::string, std::string> & srctgtDisks)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	bool rv = true;
	std::map<std::string, std::string>::const_iterator srctgtIter = srctgtLuns.cbegin();

	for (; srctgtIter != srctgtLuns.cend(); ++srctgtIter)
	{
		std::string srcdisk = srctgtIter->first;
		std::string tgtLun = srctgtIter->second;
		Diskinfo_t tgtdiskinfo;

		if (!FindMatchingDisk(diskNamesToDiskInfosMap, tgtLun, tgtdiskinfo)){

			DebugPrintf(SV_LOG_ERROR, "Unable to find target disk for VM:%s source disk:%s.\n",
				srcVm.c_str(), srcdisk.c_str());

			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0508";
			UpdateErrorStatusStructure("Master target server " + m_strMtHostName + " couldn't detect newly attached disks for " + srcVm, "EA0508");
			rv = false;
			break;
		}

		if (!InitializeDisk(tgtdiskinfo)){

			DebugPrintf(SV_LOG_ERROR, "Unable to initialize target disk for VM:%s source disk:%s.\n",
				srcVm.c_str(), srcdisk.c_str());

			m_PlaceHolder.Properties[MT_HOSTNAME] = m_strMtHostName;
			m_PlaceHolder.Properties[ERROR_CODE] = "EA0508";
			UpdateErrorStatusStructure("Master target server " + m_strMtHostName + " couldn't initialize the attached disks for " + srcVm, "EA0508");
			rv = false;
			break;
		}

		std::string tgtscsid = tgtdiskinfo.scsiId;
		srctgtDisks.insert(std::make_pair(srcdisk, tgtscsid));
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

bool MasterTarget::FindMatchingDisk(const DiskNamesToDiskInfosMap & diskNamesToDiskInfosMap,
	const std::string &tgtLun, Diskinfo_t & tgtdiskinfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	bool rv = false;
	DiskNamesToDiskInfosMap::const_iterator dIter = diskNamesToDiskInfosMap.cbegin();
	for (; dIter != diskNamesToDiskInfosMap.cend(); ++dIter)
	{
		std::string LunId = dIter->second.scsiPort + ":" + dIter->second.scsiTgt;
		if (tgtLun == LunId)
		{
			tgtdiskinfo = dIter->second;
			rv = true;
			break;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}


bool MasterTarget::InitializeDisk(const Diskinfo_t & tgtdiskinfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	Disk d(tgtdiskinfo.scsiId, tgtdiskinfo.displayName, VolumeSummary::SCSIID);

	BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	if (SV_SUCCESS != d.Open(openMode))
		return false;

	if (!d.OfflineRW())
		return false;

	if (!d.InitializeAsRawDisk())
		return false;

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return true;
}


WmiDiskRecordProcessor::WmiDiskRecordProcessor(DiskNamesToDiskInfosMap *pdiskNamesToDiskInfosMap)
: m_pdiskNamesToDiskInfosMap(pdiskNamesToDiskInfosMap)
{

}


bool WmiDiskRecordProcessor::Process(IWbemClassObject *precordobj)
{
	if (0 == precordobj)
	{
		m_ErrMsg = "Record object is NULL";
		return false;
	}

	const char NAMECOLUMNNAME[] = "Name";
	const char SCSIBUSCOLUMNNAME[] = "SCSIBus";
	const char SCSILUCOLUMNNAME[] = "SCSILogicalUnit";
	const char SCSIPORTCOLUMNNAME[] = "SCSIPort";
	const char SCSITARGETIDCOLUMNNAME[] = "SCSITargetId";

	std::string diskname;
	USES_CONVERSION;
	VARIANT vtProp;
	HRESULT hrCol;

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(NAMECOLUMNNAME), 0, &vtProp, 0, 0);
	if (!FAILED(hrCol))
	{
		if (VT_BSTR == V_VT(&vtProp))
		{
			diskname = W2A(V_BSTR(&vtProp));
		}
		VariantClear(&vtProp);
	}
	else
	{
		m_ErrMsg = "Could not find column: ";
		m_ErrMsg += NAMECOLUMNNAME;
		return false;
	}

	HANDLE h = SVCreateFile(diskname.c_str(),
		GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == h)
	{
		DWORD err = GetLastError();
		std::stringstream sserr;
		sserr << err;
		m_ErrMsg = "Failed to open disk ";
		m_ErrMsg += diskname;
		m_ErrMsg += " with error ";
		m_ErrMsg += sserr.str().c_str();
		return false;
	}
	ON_BLOCK_EXIT(&CloseHandle, h);

	Diskinfo_t vi;

	std::string storagetype;
	std::string diskGuid;

	vi.displayName = diskname;

	vi.scsiId = m_DeviceIDInformer.GetID(diskname);
	if (vi.scsiId.empty()){

		m_ErrMsg = "For disk device name: ";
		m_ErrMsg += diskname;
		m_ErrMsg += " could not find SCSI Id. Not collecting this disk.";
		return false;
	}
	
	if (m_scsiIds.find(vi.scsiId) == m_scsiIds.end())
	{
		m_scsiIds.insert(vi.scsiId);
	}
	else
	{
		m_ErrMsg = "For disk device name: ";
		m_ErrMsg += diskname;
		m_ErrMsg += " found multiple disks with same scsi id:";
        m_ErrMsg += vi.scsiId;
        m_ErrMsg += " Not collecting this disk.";


        // we do not want to report this disk
        // removing entry inserted in earlier processing 
        // as well
		m_pdiskNamesToDiskInfosMap->erase(vi.scsiId);
		return false;
	}


	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(SCSIBUSCOLUMNNAME), 0, &vtProp, 0, 0);
	if (!FAILED(hrCol))
	{
		if (VT_I4 == V_VT(&vtProp))
		{
			std::stringstream ss;
			ss << V_I4(&vtProp);
			vi.scsiBus = ss.str();
		}
		VariantClear(&vtProp);
	}
	else
	{

		m_ErrMsg = "For disk device name: ";
		m_ErrMsg += diskname;
		m_ErrMsg += " could not find ";
		m_ErrMsg += SCSIBUSCOLUMNNAME;
		m_ErrMsg += " Not collecting this disk.";
		return false;
	}

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(SCSILUCOLUMNNAME), 0, &vtProp, 0, 0);
	if (!FAILED(hrCol))
	{
		if (VT_I4 == V_VT(&vtProp))
		{
			std::stringstream ss;
			ss << V_I4(&vtProp);
			vi.scsiLun = ss.str();
		}
		VariantClear(&vtProp);
	}
	else
	{
		m_ErrMsg = "For disk device name: ";
		m_ErrMsg += diskname;
		m_ErrMsg += " could not find ";
		m_ErrMsg += SCSILUCOLUMNNAME;
		m_ErrMsg += " Not collecting this disk.";

		return false;
	}

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(SCSIPORTCOLUMNNAME), 0, &vtProp, 0, 0);
	if (!FAILED(hrCol))
	{
		if (VT_I4 == V_VT(&vtProp))
		{
			std::stringstream ss;
			ss << V_I4(&vtProp);
			vi.scsiPort = ss.str();
		}
		VariantClear(&vtProp);
	}
	else
	{
		m_ErrMsg = "For disk device name: ";
		m_ErrMsg += diskname;
		m_ErrMsg += " could not find ";
		m_ErrMsg += SCSIPORTCOLUMNNAME;
		m_ErrMsg += " Not collecting this disk.";

		return false;
	}

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(SCSITARGETIDCOLUMNNAME), 0, &vtProp, 0, 0);
	if (!FAILED(hrCol))
	{
		if (VT_I4 == V_VT(&vtProp))
		{
			std::stringstream ss;
			ss << V_I4(&vtProp);
			vi.scsiTgt = ss.str();
		}
		VariantClear(&vtProp);
	}
	else
	{
		m_ErrMsg = "For disk device name: ";
		m_ErrMsg += diskname;
		m_ErrMsg += " could not find ";
		m_ErrMsg += SCSITARGETIDCOLUMNNAME;
		m_ErrMsg += " Not collecting this disk.";

		return false;
	}

	std::string errMsg;
	vi.capacity = GetDiskSize(h, errMsg);
	if(0 == vi.capacity) {
		m_ErrMsg = "Failed to find size of disk ";
		m_ErrMsg += diskname;
		m_ErrMsg += " with error : ";
		m_ErrMsg += errMsg;
		m_ErrMsg += ". Not collecting this disk.";
		return false;
	}

	m_pdiskNamesToDiskInfosMap->insert(std::make_pair(vi.scsiId, vi));
	return true;
}


void MasterTarget::display_devlist(DiskNamesToDiskInfosMap &diskNamesToDiskInfosMap)
{
	DiskNamesToDiskInfosMap::const_iterator dIter = diskNamesToDiskInfosMap.cbegin();
	for (; dIter != diskNamesToDiskInfosMap.cend(); ++dIter)
	{

		Diskinfo_t di = dIter->second;

		DebugPrintf(SV_LOG_DEBUG, "\n---- Disk information for %s ----    \n", di.displayName.c_str());

		DebugPrintf(SV_LOG_DEBUG, "device           %s \n", di.displayName.c_str());
		DebugPrintf(SV_LOG_DEBUG, "id               %s \n", di.scsiId.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Bus              %s \n", di.scsiBus.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Port             %s \n", di.scsiPort.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Target           %s \n", di.scsiTgt.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Lun              %s \n", di.scsiLun.c_str());
		DebugPrintf(SV_LOG_DEBUG, "device           %llu \n", di.capacity);
	}
}
