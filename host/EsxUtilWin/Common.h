#ifndef COMMON_H
#define COMMON_H


#define  _WIN32_DCOM

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define _WIN32_WINNT	    0x0501
#define SECURITY_WIN32		1

#include "localconfigurator.h"
#include "rpcconfigurator.h"
#include <vector>
#include <list>
#include <set>
#include <map>
#include<iostream>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <atlconv.h>
#include <sstream>
#include <fstream>
#include <bitset>
#include <algorithm>
#include <windows.h>
#include <Winioctl.h>
#include <wininet.h>
#include <boost\lexical_cast.hpp>
#include <winsock2.h>
#include <Security.h>
#include "initguid.h"
#include <exception>
#include "portablehelpersmajor.h"
#include "ace/Process.h"
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "ace/Process_Manager.h"
#include <ace/OS_main.h>
#include "version.h"
#include "dlmapi.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <tchar.h>
#include <vdslun.h>
#include <vds.h>
#include <vdserr.h>
#include <atlconv.h>
#include <curl/curl.h>
#include "HttpFileTransfer.h"
#include "inmsafeint.h"
#include "inmageex.h"

#pragma comment(lib, "wbemuuid.lib")

typedef struct _VolumeInformation
{
	char  strVolumeName[MAX_PATH + 1];
	LARGE_INTEGER startingOffset;
	LARGE_INTEGER partitionLength;
	LARGE_INTEGER endingOffset;
}volInfo;

typedef struct _GETVERSIONOUTPARAMS
{
   BYTE bVersion;      // Binary driver version.
   BYTE bRevision;     // Binary driver revision.
   BYTE bReserved;     // Not used.
   BYTE bIDEDeviceMap; // Bit map of IDE devices.
   DWORD fCapabilities; // Bit mask of driver capabilities.
   DWORD dwReserved[4]; // For future use.
} GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;

typedef struct _DiskInfomartion
{
	unsigned int    uDiskNumber;
	LARGE_INTEGER   DiskSize;        //Totoal Disk Size
	ULONG		  	ulDiskSignature; //Disk Signature
	//enum			g_Disktype dt;      //DiskType
	enum			DISK_TYPE dt;
	//enum			g_PartitionStyle pst;
	enum			DISK_FORMAT_TYPE pst;
	DWORD			dwPartitionCount;
	DWORD			dwActualPartitonCount;
}DiskInformation;

typedef struct _PartitionInformation
{
	unsigned int    uDiskNumber;
	unsigned int    uNoOfVolumesInParttion;
    int             uPartitionType;
	int             iBootIndicator;
	LARGE_INTEGER   startOffset;
	LARGE_INTEGER   EndingOffset;
	LARGE_INTEGER   PartitionLength;
}PartitionInfo;

//	enum g_Disktype        {BASIC = 0,DYNAMIC} ;
//	enum g_PartitionStyle  {MBR = 0,GPT,RAW};

//libraries required to link with VDS
#pragma comment(lib,"Ole32.lib")
#pragma comment(lib,"Advapi32.lib")

   //  IOCTL commands
#define  DFP_GET_VERSION          0x00074080
#define  DFP_SEND_DRIVE_COMMAND   0x0007c084
#define  DFP_RECEIVE_DRIVE_DATA   0x0007c088
#define  FILE_DEVICE_SCSI              0x0000001b
#define  IOCTL_SCSI_MINIPORT_IDENTIFY  ((FILE_DEVICE_SCSI << 16) + 0x0501)
#define  IOCTL_SCSI_MINIPORT 0x0004D008  //  see NTDDSCSI.H for definition
//#define _WIN32_WINNT 0x0500
#define BUFSIZE						MAX_PATH
#define FILESYSNAMEBUFSIZE			MAX_PATH
#define MAX_DISK_IN_SYSTEM          256
#define FILE_REP                    "FileRep"
#define MBR_FILE_NAME				".MBR"
#define GPT_FILE_NAME				".GPT"
#define EBR_FILE_NAME				".EBR"
#define DISK_SIGNATURE_FILE			".sig"
#define OPTION_RETENTIONLOG_PATH    "-retentionPath"
#define MAX_VOLUME                  128
#define  _AFXDLL  
#define CONSISTENCY_DIRECTORY        "Consistency"
#define DEPENDENT_SERVICES_CONF_FILE "FailoverServices.conf"
#define DISK_COUNT                   "disk_count"
#define ACTIVE_MAC_TRANSPORT         "macAdd.txt";


#define WIN2K8_SERVER_VER			   0x6
#define RESCAN_DISKS_FILE			   "inmage_rescan_disks.txt"
#define ONLINE_DISK					   "inmage_online_disks.txt"
#define IMPORT_DISK                    "inmage_import_disks.txt"
#define SET_DISK_ID					   "ChangeDiskSignature.txt"
#define CLEAN_DISK					   "inmage_clean_disks.txt"
#define CLEAR_DISK					   "inmage_clear_disks.txt"
#define CONVERT_TO_GPT_DISK            "inmage_convert_gptdisk.txt"							
#define RESCAN_DISK_SWITCH             "rescan"
#define DISKPART_EXE                   "diskpart.exe"
#define VOL_CNT_FILE				   ".volcnt"
#define VX_JOB_FILE_NAME			   "ESX.xml"
#define SCSI_DISK_FILE_NAME            "_ScsiIDsandDiskNmbrs.txt"
#define VM_CONSISTENCY_SCRIPT          "VM_consistency.bat"
#define OPTION_CRASH_CONSISTENCY       "-crashconsistency"
#define SCSI_DISKIDS_FILE_NAME         "Inmage_scsi_unit_disks.txt"
#define SCSI_DISKIDS_SUCCESS_FILE_NAME "Inmage_scsi_unit_disks.success"
#define ESX_DIRECTORY                  "ESX"
#define SNAPSHOT_DIRECTORY             "SNAPSHOT"
#define VOLUMESINFO_EXTENSION          ".pInfo"
#define DISKINFO_EXTENSION             ".dInfo"
#define SNAPSHOTINFO_EXTENSION         ".sInfo"
#define RESULT_CXCLI_FILE              "Result_CxCli.xml"
#define DRVUTIL_EXE                    "drvutil.exe"
#define SNAPSHOT_FILE_NAME             "Snapshot.xml"
#define DISABLE_MOUNTPOINT_FILE_PATH   "automount.txt"
#define VOL_RESIZE_FILE_NAME		   "Resize.xml"
#define TGT_VOLUMESINFO                ".tgtVolInfo"
#define RECOVERY_PROGRESS              "_recoveryprogress"

#define MBR_BOOT_SECTOR_LENGTH                      512
#define EBR_SECOND_ENTRY_STARTING_INDEX             470
#define EBR_SECOND_ENTRY_SIZE                       4

#define INM_SAFE_RELEASE(x) {if(NULL != x) { x->Release(); x=NULL; } }
#define GUID_LEN	50

/*
	EsxUtilWin Task update to CX related changes.
*/
#define INM_VCON_TASK_1						"Task1"
#define INM_VCON_TASK_2						"Task2"
#define INM_VCON_TASK_3						"Task3"
#define INM_VCON_TASK_4						"Task4"
#define INM_VCON_TASK_5						"Task5"
#define INM_VCON_TASK_6						"Task6"
#define INM_VCON_TASK_7						"Task7"

#define INM_VCON_PROTECTION_TASK_1			"Initializing Protection Plan"
#define INM_VCON_PROTECTION_TASK_2			"Downloading Configuration Files"
#define INM_VCON_PROTECTION_TASK_3			"Preparing Master Target Disks"
#define INM_VCON_PROTECTION_TASK_4			"Updating Disk Layouts"
#define INM_VCON_PROTECTION_TASK_5			"Activating Protection Plan"

#define INM_VCON_PROTECTION_TASK_1_DESC		"This will initializes the Protection Plan.\
											It starts the EsxUtilWin.exe binary for Protection"
#define INM_VCON_PROTECTION_TASK_2_DESC		"The files which are going to download from CX are \
											1. Esx.xml \
											2. Inmage_scsi_unit_disks.txt \
											3. Respective MBR file"
#define INM_VCON_PROTECTION_TASK_3_DESC		"The following operations going to perform in this task:\
											1. Discovers all the source respective target disks in MT \
											2. Offline and clean the disks\
											3. initialise and online the disks"
#define INM_VCON_PROTECTION_TASK_4_DESC		"The following operations going to perform in this task:\
											1. Deploys the source disk layout on respective target disk\
											2. Creates the Source volumes at Target disks."
#define INM_VCON_PROTECTION_TASK_5_DESC		"Creates the replication pairs between source volume and newly created target volumes."


#define INM_VCON_VOL_RESIZE_TASK_1			"Initializing volume resize protection plan"
#define INM_VCON_VOL_RESIZE_TASK_2			"Downloading Configuration Files"
#define INM_VCON_VOL_RESIZE_TASK_3          "Pausing the protection pairs"
#define INM_VCON_VOL_RESIZE_TASK_4			"Preparing Master Target Disks"
#define INM_VCON_VOL_RESIZE_TASK_5			"Updating Disk Layouts"
#define INM_VCON_VOL_RESIZE_TASK_6			"Resuming the protection pairs"

#define INM_VCON_VOL_RESIZE_TASK_1_DESC		"This will initializes the Volume Resize Protection Plan.\
											It starts the EsxUtilWin.exe binary for Volume Resize operation"
#define INM_VCON_VOL_RESIZE_TASK_2_DESC		"The files which are going to download from CX are \
											1. Resize.xml \
											2. Inmage_scsi_unit_disks.txt \
											3. Respective Latest MBR files"
#define INM_VCON_VOL_RESIZE_TASK_3_DESC     "Pauses all the pairs with respect to volume resized host"
#define INM_VCON_VOL_RESIZE_TASK_4_DESC		"The following operations going to perform in this task:\
											1. Discovers all the source respective target disks in MT \
											2. Offlines and clears the readonly attribute of the disks"
#define INM_VCON_VOL_RESIZE_TASK_5_DESC		"The following operations going to perform in this task:\
											1. Deploys the source disk layout on respective target disks\
											2. Creates the Source volumes at Target disks."
#define INM_VCON_VOL_RESIZE_TASK_6_DESC		"Resumes all the pairs with respect to volume resized host"


#define INM_VCON_SNAPSHOT_TASK_1			"Initializing Dr-Drill Plan"
#define INM_VCON_SNAPSHOT_TASK_2			"Downloading Configuration Files"
#define INM_VCON_SNAPSHOT_TASK_3			"Preparing Master Target Disks"
#define INM_VCON_SNAPSHOT_TASK_4			"Updating Disk Layouts"
#define INM_VCON_SNAPSHOT_TASK_5			"Initializing Physical Snapshot Operation"
#define INM_VCON_SNAPSHOT_TASK_6			"Starting Physical Snapshot For Selected VM(s)"
#define INM_VCON_SNAPSHOT_TASK_7			"Powering on the dr drill VM(s)"

#define INM_VCON_SNAPSHOT_TASK_1_DESC		"This will initializes the Dr-Drill Plan.\
											It starts the EsxUtilWin.exe binary for taking physical snapshot"
#define INM_VCON_SNAPSHOT_TASK_2_DESC		"The files which are going to download from CX are \
											1. snapshot.xml \
											2. Inmage_scsi_unit_disks.txt"
#define INM_VCON_SNAPSHOT_TASK_3_DESC		"The following operations going to perform in this task:\
											1. Discovers all the source respective target disks in MT \
											2. Offline and clean the disks\
											3. initialise and online the disks"
#define INM_VCON_SNAPSHOT_TASK_4_DESC		"The following operations going to perform in this task:\
											1. Deploys the source disk layout on respective target disk\
											2. Creates the Source volumes at Target disks."
#define INM_VCON_SNAPSHOT_TASK_5_DESC		"This will initializes the Physical snapshot.\
											It starts the EsxUtil.exe binary for taking physical snapshot"
#define INM_VCON_SNAPSHOT_TASK_6_DESC		"The following operations going to perform in this task:\
											1. takes physical snapshot for all the selected VMs \
											2. Completes network related changes for all VMs \
											3. Deploys the source disk layout on respective target disk(in case of windows)"
#define INM_VCON_SNAPSHOT_TASK_7_DESC		"This will power-on all the dr-drilled VMs \
											1. It will detach all the snapshot disks from MT \
											2. Power-on the Dr-driiled VMs"

#define INM_VCON_TASK_COMPLETED				"Completed"
#define INM_VCON_TASK_FAILED				"Failed"
#define INM_VCON_TASK_QUEUED				"Queued"
#define INM_VCON_TASK_INPROGRESS			"InProgress"
#define INM_VCON_TASK_WARNING				"Warning"

//Error messaging
#define PREPARE_TARGET_STATUS               "PrepareTargetStatus"
#define ERROR_CODE                          "ErrorCode"
#define ERROR_MESSAGE                       "ErrorMessage"
#define ERROR_LOG_FILE                      "LogFile"
#define PLACE_HOLDER                        "PlaceHolder"
#define MT_HOSTNAME                         "MTHostName"
#define SOURCE_HOSTNAME                     "SourceIP"
#define CS_HOST_NAME                        "CsIP"
#define REGISTRY_KEY                        "RegistryKey"
#define DISK_NAME                           "DiskName"


struct ClusterMbrInfo
{
	string NodeId;
	string MbrFile;
	bool bDownloaded;
};
typedef ClusterMbrInfo clusterMbrInfo_t;


struct MbrDiskInfo
{
	string DiskNum;
	string DiskSignature;
	string DiskDeviceId;
};
typedef MbrDiskInfo dInfo_t;


struct TaskInfo
{
	string TaskNum;
	string TaskName;
	string TaskDesc;
	string TaskStatus;
	string TaskErrMsg;
	string TaskFixSteps;
	string TaskLogPath;
};
typedef TaskInfo taskInfo_t;

struct StepInfo
{
	string StepNum;
	string ExecutionStep;
	string StepStatus;
	list<taskInfo_t> TaskInfoList;
};
typedef StepInfo stepInfo_t;

struct TaskUpdateInfo
{
	string FunctionName;
	string PlanId;
	string HostId;
	list<stepInfo_t> StepInfoList;
};
typedef TaskUpdateInfo taskUpdateInfo_t;

struct ProtectionPlaceHolder
{
	map<string, string> Properties;

	void clear()
	{
		Properties.clear();
	}
};

struct StatusInfo
{
	string HostId;            //Source host id
	string Status;            //Initiated/Completed/Failed
	string ErrorCode;
	string ErrorMsg;          //Error message in case of failure
	string ExecutionStep;     //Failure step
	string Resolution;        //Recommendation steps to user
	string Job;               //Job for which it failed (Protection\PrepareTarget)
	string ErrorReason;       //Reason for the issue
	ProtectionPlaceHolder PlaceHolder;

	StatusInfo()
	{
		HostId = "";
		Status = "0";
		ErrorCode = "";
		ErrorMsg = "";
		ExecutionStep = "";
		Resolution = "";
		Job = "PrepareTarget";
		ErrorReason = "";
		PlaceHolder.clear();
	}

	void clear()
	{
		ErrorCode.clear();
		ErrorMsg.clear();
		ExecutionStep.clear();
		Resolution.clear();
		ErrorReason.clear();
		PlaceHolder.clear();
	}
};
typedef StatusInfo statusInfo_t;

struct ResponseData
{
	char *chResponseData;
	size_t length;
};

typedef ResponseData ResponseData_t;

static size_t WriteCallbackFunction(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize;
    INM_SAFE_ARITHMETIC(realsize = InmSafeInt<size_t>::Type(size) * nmemb, INMAGE_EX(size)(nmemb))
	ResponseData_t *ResData = (ResponseData_t *)data;
	
    size_t resplen;
    INM_SAFE_ARITHMETIC(resplen = InmSafeInt<size_t>::Type(ResData->length) + realsize + 1, INMAGE_EX(ResData->length)(realsize))
	ResData->chResponseData = (char *)realloc(ResData->chResponseData, resplen);
	if(ResData->chResponseData)
	{
		inm_memcpy_s(&(ResData->chResponseData[ResData->length]), resplen, ptr, realsize);
		ResData->length += realsize;
		ResData->chResponseData[ResData->length] = 0;
	}
	
	return realsize;
}

inline std::string GUIDString(const GUID & guid){
	USES_CONVERSION;
	WCHAR szGUID[GUID_LEN] = {0};
	if(!StringFromGUID2(guid,szGUID,GUID_LEN))
		szGUID[0] = L'\0'; //failure
	return W2A(szGUID);
}

//Returns disk number for give device name //?/PhysicalDrive or //./PHYSICALDRIVE
inline int GetDiskNumFromName(const std::string &strDeviceName)
{
	int iRet = -1;
	size_t pos = 17; //szie of "//?/PhysicalDrive" or "//./PHYSICALDRIVE"
	std::string strDiskNum = strDeviceName.substr(pos,strDeviceName.length()-pos);
	
	//Verify the content of strDiskNum
	if(!strDiskNum.empty() && strDiskNum.find_first_not_of("0123456789") == std::string::npos)
		iRet = atoi(strDiskNum.c_str());
	else
		DebugPrintf(SV_LOG_ERROR,"Invalid input : [%s]\n",strDeviceName.c_str());

	return iRet;
}
using namespace std;
class Common
{
public:
	Common()
	{
		m_bV2P = false;
        bP2Vmachine = false;
		m_bArrBasedSnapshot = false;
		m_vConVersion = 1.2;
		m_bCloudMT = false;
		m_strCmdHttp = "http://";
	}
	~Common()
	{
	}
	void SetCloudEnv(bool bCloudEnv = true, std::string strCloudEnv = "")
	{
		m_bCloudMT = bCloudEnv;
		m_strCloudEnv = strCloudEnv;
	}
	string m_strCmdHttp;
	int getAgentInstallPath();
	HRESULT WMIQuery();
	HRESULT InitCOM();
	HRESULT GetWbemService(IWbemServices **pWbemService);
	string stGetHostName();
	string ConvertHexToDec(const string& strHexVal);
	string strConvetIntegetToString(const int);
	BOOL ExecuteProcess(const std::string &, const std::string &);
    int DumpMbrInFile(unsigned int, LARGE_INTEGER, const string&, unsigned int);
    bool checkIfFileExists(string );
	bool checkIfDirExists(string strDirName);
    bool isAnInteger(const char * );
    void FormatVolumeNameForMountPoint(std::string & volumeName);
    void FormatVolumeNameForCxReporting(std::string & volumeName);
    bool ExecuteCommandUsingAce(const std::string FullPathToExe, const std::string Parameters);
    int ReadDiskData(const char *disk_name, ACE_LOFF_T bytes_to_skip, size_t bytes_to_read, unsigned char *disk_data);
    int WriteToFileInBinary(const unsigned char *buffer, size_t bytes_to_write, const char *file_name);
    void GetBytesFromMBR(unsigned char *buffer, size_t start_index, size_t bytes_to_fetch, unsigned char *out_data);
    void ConvertHexArrayToDec(unsigned char *hex_array, size_t n, ACE_LOFF_T & number);
    int WriteDiskData(const char *disk_name, ACE_LOFF_T bytes_to_skip, size_t bytes_to_write, unsigned char *disk_data);
    int ReadFromFileInBinary(unsigned char *out_buffer, size_t bytes_to_read, const char *file_name);
    std::string GeneratreUniqueTimeStampString();
	bool CdpcliOperations(const string& cdpcli_cmd);
	bool RegisterHostUsingCdpCli();
	bool MountVolCleanup();
	bool ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes);
	bool DownloadFileFromCx(string strMbrUrl, string strMbrPath/*, bool bMbr = false*/);
	bool RenameDiskInfoFile(string strOldFile, string strNewFile);
	bool xGetvConVersion(string FileName);
	bool GetDiskSize(const char *DiskName, SV_LONGLONG & DiskSize);
	bool DisableAutoMount();
    void AssignSecureFilePermission(const string& strFile, bool bDir = false);
    std::string GetAccessSignature(const string& strRqstMethod, const string& strAccessFile, const string& strFuncName, const string& strRqstId, const string& strVersion);
	std::string GetFingerPrint();
	std::string GetRequestId();
	string getInMageHostId();
	string getCxIpAddress();
	string getCxHttpPortNumber();

	statusInfo_t m_statInfo;
	ProtectionPlaceHolder m_PlaceHolder;

protected:
	string m_strInstallDirectoryPath;
	string m_strDataFolderPath;
	map< string,string> m_mapOfDeviceIdAndControllerIds;
	map<string,string> m_mapOfLunIdAndDiskName;
    bool bP2Vmachine;
	double m_vConVersion;
	string m_strDirPath;
	string m_strXmlFile;
	taskUpdateInfo_t m_TaskUpdateInfo;
	bool m_bUpdateStep;
	bool m_bV2P;
	bool m_bArrBasedSnapshot;
	std::string m_strCloudEnv;
	bool m_bCloudMT;
private:
    char m_currentHost[MAX_PATH];
};

class CopyrightNotice
{ 
public:
    CopyrightNotice()
    {
        std::cout <<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
    }
};

class CVdsHelper
{
	IVdsService* m_pIVdsService;

	enum {
		INM_OBJ_UNKNOWN,
		INM_OBJ_MISSING_DISK,
		INM_OBJ_DISK_ONLINE,
		INM_OBJ_FAILED_VOL,
		INM_OBJ_DISK_SCSI_ID,
		INM_OBJ_DISK_SIGNATURE,
		INM_OBJ_HIDDEN_VOL
	} m_objType;

	std::map<std::string,std::string> m_helperMap;
	std::map<std::string,GUID> m_mapDiskToSignature;
	std::map<std::string,bool> m_mapDiskToStatus;
	bool m_bAvailable;
	
	HRESULT ProcessProviders();
	HRESULT ProcessPacks(IVdsSwProvider *pSwProvider);
	HRESULT RemoveMissingDisks(IVdsPack *pPack);
	HRESULT CollectOnlineDisks(IVdsPack *pPack);
	HRESULT CheckUnKnownDisks(IVdsPack *pPack);
	HRESULT DeleteFailedVolumes(IVdsPack *pPack);
	HRESULT DeleteVolume(const VDS_OBJECT_ID & objId);
	HRESULT ClearFlagsOfVolume(IVdsPack *pPack);
	HRESULT ProcessPlexes(IVdsVolume *pIVolume, std::list<VDS_DISK_EXTENT> & lstExtents);
	bool IsDiskMissing(const VDS_OBJECT_ID & diskId);
	bool AllDisksMissig(std::list<VDS_DISK_EXTENT> & lstExtents);
	void DeleteFailedVolumes(std::list<VDS_OBJECT_ID> & lstFailedVols);
	void CollectDiskScsiIDs(IVdsPack *pPack);
	void CollectDiskSignatures(IVdsPack *pPack);
	
public:
	CVdsHelper(void);
	~CVdsHelper(void);

	bool InitilizeVDS()
	{
		bool bInit = false;
		for(int i=0; i<3; i++)
		{
			if(!InitVDS())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS\n");
				unInitialize();
				Sleep(15000);
				continue;
			}
			else
			{
				bInit = true;
				break;
			}
		}
		if(bInit)
		{
			DebugPrintf(SV_LOG_DEBUG,"Successfully initialized the VDS\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS in all three tries.\n");
		}
		return bInit;
	}

	void unInitialize()
	{
		DebugPrintf(SV_LOG_DEBUG,"Uninitializing VDS instance\n");
		INM_SAFE_RELEASE(m_pIVdsService);
		CoUninitialize();
	}

	void GetDisksScsiIDs(std::map<std::string,std::string> & disksScsiIds)
	{
		m_helperMap.clear();
		m_objType = INM_OBJ_DISK_SCSI_ID;
		ProcessProviders();

		disksScsiIds.clear();
		//disksScsiIds.insert(m_helperMap.begin(),m_helperMap.end());
		std::map<std::string,std::string>::iterator iter = m_helperMap.begin();
		for(; iter != m_helperMap.end(); iter++)
		{
			DebugPrintf(SV_LOG_INFO,"Disk : %s Scsi ID : %s \n",iter->first.c_str(), iter->second.c_str());
			disksScsiIds.insert(make_pair(iter->first, iter->second));
		}
	}

	void GetDisksSignature( std::map<std::string, std::string> & diskNameToSignature )
	{
		m_mapDiskToSignature.clear();
		m_objType = INM_OBJ_DISK_SIGNATURE;
		ProcessProviders();
		diskNameToSignature.clear();
		std::map<std::string,GUID>::iterator iterSig = m_mapDiskToSignature.begin();
		for(; iterSig != m_mapDiskToSignature.end(); iterSig++)
		{
			USES_CONVERSION;
			wchar_t strGUID[128] = {0};
			StringFromGUID2(iterSig->second,strGUID,128);
			DebugPrintf(SV_LOG_INFO,"Disk : %s , DiskSignature : %s \n", iterSig->first.c_str(), W2A(strGUID) );
			diskNameToSignature.insert(make_pair(iterSig->first,string(W2A(strGUID))));
		}
	}

	bool InitVDS();
	void RemoveMissingDisks()
	{
		//1-> delete failed volumes
		m_objType = INM_OBJ_FAILED_VOL;
		ProcessProviders();

		//2-> remove missing disks
		m_objType = INM_OBJ_MISSING_DISK;
		ProcessProviders();
	}

	void ClearFlagsOfVolume()
	{
		m_objType = INM_OBJ_HIDDEN_VOL;
		ProcessProviders();
	}

	bool CheckUnKnowndisks()
	{
		m_bAvailable = false;
		m_objType = INM_OBJ_UNKNOWN;
		ProcessProviders();
		return m_bAvailable;
	}

	void GetOnlineDisks(std::map<std::string, bool> & diskToStatus)
	{
		m_mapDiskToStatus.clear();
		m_objType = INM_OBJ_DISK_ONLINE;
		ProcessProviders();

		diskToStatus.clear();
		//disksScsiIds.insert(m_helperMap.begin(),m_helperMap.end());
		std::map<std::string, bool>::iterator iter = m_mapDiskToStatus.begin();
		for(; iter != m_mapDiskToStatus.end(); iter++)
		{
			DebugPrintf(SV_LOG_DEBUG,"Disk : %s Disk Status : %d \n",iter->first.c_str(), iter->second);
			diskToStatus.insert(make_pair(iter->first, iter->second));
		}
	}
};

#endif
