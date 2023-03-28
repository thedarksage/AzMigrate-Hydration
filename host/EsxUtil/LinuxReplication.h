#ifndef LINUX_REPLICATION_
#define LINUX_REPLICATION_

#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <ostream>
#include <fstream>
#include <string>
#include <sstream>
#ifndef WIN32
	#include <mntent.h>
	#include <sys/ioctl.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <net/if.h>
	#include <ifaddrs.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <cerrno>
	#include <ctime>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <cmath>
	#include <sys/utsname.h>
#endif
#include "ace/Process.h"
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "ace/Process_Manager.h"

#include "logger.h"
#include "localconfigurator.h"
#include "rpcconfigurator.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <curl/curl.h>
#include "applicationsettings.h"
#include "serializeapplicationsettings.h"
#include "inmsafeint.h"
#include "inmageex.h"

#ifndef WIN32
	#include "executecommand.h"
#endif

extern bool not_comma(char);
extern bool isComma(char);

#define RECOVERY_FILE_NAME			    "Recovery.xml"
#define VX_JOB_FILE_NAME			    "ESX.xml"
#define FAILOVER_DATA_FILE_NAME         "failover_data"
#define VOLUMES_CXCLI_FILE              "Volumes_CxCli.xml"
#define RESULT_CXCLI_FILE               "Result_CxCli.xml"
#define NETWORK_DETAILS_FILE			"_Network.xml"
#define REDHAT_RELEASE_FILE				"/etc/redhat-release"
#define SUSE_RELEASE_FILE				"/etc/SuSE-release"
#define SNAPSHOT_FILE_NAME				"Snapshot.xml"
#define VOL_RESIZE_FILE_NAME		    "Resize.xml"
#define VOL_DISK_REMOVE_FILE_NAME       "Remove.xml"
#define ROLLBACK_VM_FILE_NAME			"InMage_Recovered_Vms.rollback"
#define SNAPSHOT_VM_FILE_NAME			"InMage_Recovered_Vms.snapshot"
#define LINUXP2V_MAKE_INFORMATION		"DiscoverMachine.sh"
#define LINUXP2V_PREPARETARGET_INFORMATION "PrepareTarget.sh"
#define ROLLBACK_FAILED_VM_FILE         "failed_errormessage.rollback"
#define POST_CDPCLI_ROLLBACK_SLEEPTIME_IN_SEC  180


/*
	EsxUtil Task update to CX related changes.
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
#define INM_VCON_PROTECTION_TASK_4			"Activating Protection Plan"

#define INM_VCON_PROTECTION_TASK_1_DESC		"This will initializes the Protection Plan.\
											It starts the EsxUtil.exe binary for Protection"
#define INM_VCON_PROTECTION_TASK_2_DESC		"The files which are going to download from CX are \
											1. Esx.xml \
											2. Inmage_scsi_unit_disks.txt"
#define INM_VCON_PROTECTION_TASK_3_DESC		"The following operations going to perform in this task:\
											1. Discovers all the source respective target disks in MT \
											2. Creat mapping between the source and target disks"
#define INM_VCON_PROTECTION_TASK_4_DESC		"Creats the replication pairs between source disk to Target disk in MT"


#define INM_VCON_VOL_RESIZE_TASK_1			"Initializing volume resize protection plan"
#define INM_VCON_VOL_RESIZE_TASK_2			"Downloading Configuration Files"
#define INM_VCON_VOL_RESIZE_TASK_3			"Pausing the protection pairs"
#define INM_VCON_VOL_RESIZE_TASK_4			"Preparing Master Target Disks"
#define INM_VCON_VOL_RESIZE_TASK_5			"Resuming the protection pairs"

#define INM_VCON_VOL_RESIZE_TASK_1_DESC		"This will initializes the Protection Plan.\
											It starts the EsxUtilWin.exe binary for Protection"
#define INM_VCON_VOL_RESIZE_TASK_2_DESC		"The files which are going to download from CX are \
											1. Inmage_scsi_unit_disks.txt"
#define INM_VCON_VOL_RESIZE_TASK_3_DESC     "Pauses all the pairs with respect to volume resized host"
#define INM_VCON_VOL_RESIZE_TASK_4_DESC		"The following operations going to perform in this task:\
											1. Discovers all the source respective target disks in MT \
											2. Re-updates the target disks"
#define INM_VCON_VOL_RESIZE_TASK_5_DESC		"Resumes all the pairs with respect to volume resized host"


#define INM_VCON_RECOVERY_TASK_1			"Initializing Recovery Plan"
#define INM_VCON_RECOVERY_TASK_2			"Downloading Configuration Files"
#define INM_VCON_RECOVERY_TASK_3			"Starting Recovery For Selected VM(s)"
#define INM_VCON_RECOVERY_TASK_4			"Powering on the recovered VM(s)"

#define INM_VCON_RECOVERY_TASK_1_DESC		"This will initializes the Recovery Plan.\
											It starts the EsxUtil.exe binary for Recovery"
#define INM_VCON_RECOVERY_TASK_2_DESC		"The files which are going to download from CX are \
											1. Recovery.xml"
#define INM_VCON_RECOVERY_TASK_3_DESC		"The following operations going to perform in this task:\
											1. Remove pairs for all the selected VMs \
											2. Completes network related changes for all VMs \
											3. Deploys the source disk layout on respective target disk(in case of windows)"
#define INM_VCON_RECOVERY_TASK_4_DESC		"This will power-on all the recovered VMs \
											1. It will detach all the recoverd disks from MT \
											2. Power-on the recovered VMs"

#define INM_VCON_SNAPSHOT_TASK_1			"Initializing Dr-Drill Plan"
#define INM_VCON_SNAPSHOT_TASK_2			"Downloading Configuration Files"
#define INM_VCON_SNAPSHOT_TASK_3			"Preparing Master Target Disks"
#define INM_VCON_SNAPSHOT_TASK_4			"Updating Disk Layouts"
#define INM_VCON_SNAPSHOT_TASK_5			"Initializing Physical Snapshot Operation"
#define INM_VCON_SNAPSHOT_TASK_6			"Starting Physical Snapshot For Selected VM(s)"
#define INM_VCON_SNAPSHOT_TASK_7			"Powering on the dr drill VM(s)"

#ifdef WIN32
#define INM_VCON_SNAPSHOT_TASK_1_DESC		"This will initializes the Dr-Drill Plan.\
											It starts the EsxUtilWin.exe binary for taking physical snapshot"
#else
#define INM_VCON_SNAPSHOT_TASK_1_DESC		"This will initializes the Dr-Drill Plan.\
											It starts the EsxUtil.exe binary for taking physical snapshot"
#endif
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
											Starts the Physical snapshot using EsxUtil.exe binary"
#define INM_VCON_SNAPSHOT_TASK_6_DESC		"The following operations going to perform in this task:\
											1. takes physical snapshot for all the selected VMs \
											2. Completes network related changes for all VMs \
											3. Deploys the source disk layout on respective target disk(in case of windows)"
#define INM_VCON_SNAPSHOT_TASK_7_DESC		"This will power-on all the dr-drilled VMs \
											1. It will detach all the snapshot disks from MT \
											2. Power-on the Dr-driiled VMs"

#define INM_VCON_REMOVE_PAIR_TASK_1			"Initializing remove replication pairs Plan"
#define INM_VCON_REMOVE_PAIR_TASK_2			"Downloading Configuration Files"
#define INM_VCON_REMOVE_PAIR_TASK_3			"Deleting replication pairs"
#define INM_VCON_REMOVE_PAIR_TASK_4			"Detaching disks from MT and DR VM(s)"

#define INM_VCON_REMOVE_PAIR_TASK_1_DESC	"This will initializes the Remove replication plan.\
											It starts the EsxUtil.exe binary for Protection"
#define INM_VCON_REMOVE_PAIR_TASK_2_DESC	"The files which are going to download from CX are \
											1. Inmage_scsi_unit_disks.txt"
#define INM_VCON_REMOVE_PAIR_TASK_3_DESC	"The following operations going to perform in this task:\
											1. Discovers all the source volumes/disks for which pairs to be removed \
											2. Removes the replication pairs"
#define INM_VCON_REMOVE_PAIR_TASK_4_DESC	"Removes the disks from MT and and DR vms for which pairs were removed"

#define INM_VCON_TASK_COMPLETED				"Completed"
#define INM_VCON_TASK_FAILED				"Failed"
#define INM_VCON_TASK_QUEUED				"Queued"
#define INM_VCON_TASK_INPROGRESS			"InProgress"
#define INM_VCON_TASK_WARNING				"Warning"

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

struct RecoveryPlaceHolder
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
	string Tag;
	RecoveryPlaceHolder PlaceHolder;

	StatusInfo()
	{
		HostId = "";
		Status = "0";
		ErrorCode = "";
		ErrorMsg = "";
		ExecutionStep = "";
		Resolution = "";
		Job = "Recovery";
		ErrorReason = "";
		Tag = "";
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

struct SendAlerts
{
	map<string, string> mapParameters;  //Paramater inside the function request
	map<string, map<string, string> > mapParamGrp;
};

struct CxApiParamGroup
{
	string strId;
	map<string, string> mapParameters;
	list<CxApiParamGroup> lstCxApiParam;
};

struct CxApiInfo
{
	string strFunction;
	string strValue;
	map<string, string> mapParameters;  //Paramater inside the function request
	list<CxApiParamGroup> listParamGroup;
};

struct MbrDiskInfo
{
	string DiskNum;
	string DiskSignature;
	string DiskDeviceId;
};
typedef MbrDiskInfo dInfo_t;

#ifndef WIN32
	#include <libxml/parser.h>
	#include <libxml/tree.h>
	
	struct SparseRet
	{
		string strTimeWindow;
		string strUnit;
		string strValue;
		string strBookMark;
		string strTimeRange;
	};
	typedef SparseRet SparseRet_t;

	class VxJobXml
    {
    public:
        std::string VmHostName;
        std::string SourceMachineIP;
        std::string TargetMachineIP;
		string SourceInMageHostId;
		string TargetInMageHostId;
        std::string SourceVolumeName;
        std::string TargetVolumeName;
        std::string ProcessServerIp;
        std::string RetentionLogVolumeName;
        std::string RetentionLogPath;
        std::string RetainChangeMB;
        std::string RetainChangeDays;
		std::string RetainChangeHours;
		string IsSourceNatted;
		string IsTargetNatted;
		list<SparseRet_t> sparseTime;
		std::string RetTagType;
		string SecureSrcToPs;
		string SecurePsToTgt;
		string Compression;
	    VxJobXml();
    };
	
	struct StorageInfo
	{
		std::string strRootVolType;
		std::string strVgName;
		std::string strLvName;
		std::string strIsETCseperateFS;
		std::string strPartition;
		std::string strFSType;
		std::string	strMountPoint;
		bool bMounted;
		bool bVgActive;
		std::list<std::string> listDiskName;
		void reset()
		{
			strRootVolType.clear();
			strVgName.clear();
			strLvName.clear();
			strIsETCseperateFS.clear();
			strPartition.clear();
			listDiskName.clear();
			strMountPoint.clear();
			bMounted = false;
			bVgActive = false;
		}
		~StorageInfo()
		{
			reset();
		}
	};
	typedef StorageInfo StorageInfo_t;
	
	struct NwInfo
	{
		std::string strNicName;
		std::string strIsDhcp;
		std::string strIpAddress;
		std::string strNetMask;
		std::string strGateWay;
		std::string strMacAddress;
		std::string strDnsServer;
		void reset()
		{
			strNicName.clear();
			strIsDhcp.clear();
			strIpAddress.clear();
			strNetMask.clear();
			strGateWay.clear();
			strMacAddress.clear();
			strDnsServer.clear();
		}
		~NwInfo()
		{
			reset();
		}
	};
	typedef NwInfo NetworkInfo_t;
#endif


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
		inm_memcpy_s(&(ResData->chResponseData[ResData->length]), resplen,ptr, realsize);
		ResData->length += realsize;
		ResData->chResponseData[ResData->length] = 0;
	}	
	return realsize;
}

xmlNodePtr xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue);
bool xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue);

class LinuxReplication
{
public:
	LinuxReplication(bool bPhySnapShot,std::string strHostID, std::string strLogDir, std::string strCxPath, std::string strPlanId, std::string strOperation):m_strVxInstallPath(""),m_strCxIpAddress(""),m_MasterTaregtIpAddres(""),m_LocalHostIpAddress("")
	{
		m_bPhySnapShot = bPhySnapShot;
		m_strHostID = strHostID;
		m_strLogDir = strLogDir; 
		m_strCxPath = strCxPath;
		m_strPlanId = strPlanId;
		m_strOperation = strOperation;
		m_bVolResize = false;
		m_strCmdHttps = "http://";
#ifndef WIN32
		m_RedHatReleaseList.push_back("release 4");
		m_RedHatReleaseList.push_back("release 5");
		m_SuseReleaseList.push_back("Server 9");
		m_SuseReleaseList.push_back("Server 10");
		m_CentOsReleaseList.push_back("release 4");
		m_CentOsReleaseList.push_back("release 5");
		m_InitrdOsMap.insert(make_pair("Red Hat" , m_RedHatReleaseList ));
		m_InitrdOsMap.insert(make_pair("SUSE" , m_SuseReleaseList ));
		m_InitrdOsMap.insert(make_pair("CentOS" , m_CentOsReleaseList ));
#endif
	}
	LinuxReplication():m_strVxInstallPath(""),m_strCxIpAddress(""),m_MasterTaregtIpAddres(""),m_LocalHostIpAddress("")
	{
		m_bPhySnapShot = false;
		m_strHostID = "";
		m_strLogDir = "";
		m_strCxPath = "";
		m_strPlanId = "";
		m_strOperation = "";
		m_bVolResize = false;
		m_strCmdHttps = "http://";
#ifndef WIN32
		m_RedHatReleaseList.push_back("release 4");
		m_RedHatReleaseList.push_back("release 5");
		m_SuseReleaseList.push_back("Server 9");
		m_SuseReleaseList.push_back("Server 10");
		m_CentOsReleaseList.push_back("release 4");
		m_CentOsReleaseList.push_back("release 5");
		m_InitrdOsMap.insert(make_pair("Red Hat" , m_RedHatReleaseList ));
		m_InitrdOsMap.insert(make_pair("SUSE" , m_SuseReleaseList ));
		m_InitrdOsMap.insert(make_pair("CentOS" , m_CentOsReleaseList ));
#endif
	}
	
	std::string GetHostName();
	std::string getVxInstallPath() const;
    bool readDiskMappingFile();
    std::string FetchCxIpAddress();
    std::string FetchCxHttpPortNumber();
	std::string getInMageHostId();
	std::map<std::string,std::string> FetchProtectedSrcVmsAddress();
	const std::string FetchLocalIpAddress();
	bool initLinuxTarget();
	bool GenerateTagForSrcVol(std::string strTgtHostName, std::string strTagName, bool bCrashConsistency, Configurator* TheConfigurator);
	//bool GenerateInitrdImg( Configurator* );
	std::string prepareXmlData();
	bool processXmlData(string strXmlData, string& strOutPutXmlFile);
	void ModifyTaskInfoForCxApi(taskInfo_t currentTask, taskInfo_t nextTask, bool bUpdateProtection = false);
	void UpdateTaskInfoToCX(bool bCheckResponse = false);
	bool DownloadFileFromCx(string strCxPath, string strLocalPath);
	bool UploadFileToCx(string strLocalPath, string strCxPath);
	bool ProcessCxPath();
	bool ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes);
	bool RestartResyncforAllPairs();
	bool ResumeAllReplicationPairs();
	bool PauseAllReplicationPairs();
	bool RemoveReplicationPair(string strTgtHostId, list<string>& lstTgtDevices);
	bool GetRequestStatus(std::string strRequestId, std::string& strStatus);
	bool GetCommonRecoveryPoint(string strHostName, string& strValue);
	bool ReadResponseXmlFile(std::string strCxApiResponseFile, std::string strCxApi, std::string& strStatus);
	std::string PrepareCXAPIXmlData(CxApiInfo& cxapi);	
	std::string ProcessParamGroup(list<CxApiParamGroup>& listParamGroup);
	xmlNodePtr xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry);
	bool readScsiFile();
	void AssignSecureFilePermission(const string& strFile, bool bDir = false);
	std::string GetAccessSignature(const string& strRqstMethod, const string& strAccessFile, const string& strFuncName, const string& strRqstId, const string& strVersion);
	std::string GetFingerPrint();
	std::string GetRequestId();
	std::string GeneratreUniqueTimeStampString();
	bool checkIfFileExists(std::string strFileName);
	bool checkIfDirExists(string strDirName);
    std::string prepareXmlForGetHostInfo(const std::string strHostUuid);
	bool GetHostInfoFromCx(const string& strHostName, string& strGetHostInfoXmlFile);
	bool ReadDlmFileFromCxApiXml(const string& strXmlFile, string& strDlmFile);
	bool SendAlertsToCX(map<string, SendAlerts>& sendAlertsInfo);
	void UpdateSrcHostStaus();
    void UpdateSrcHostStaus(taskInfo_t & taskInfo);
	void UpdateSrcHostStaus(taskInfo_t & taskInfo, const string& strHostId);
	void UpdateSendAlerts(taskInfo_t & taskInfo, string& strCxPath, const string strPlan, map<string, SendAlerts>& sendAlertsInfo);
	void UpdateSendAlerts(map<string, SendAlerts>& sendAlertsInfo, const string strPlan);
	bool UpdateRecoveryStatusInFile(string strResultFile, statusInfo_t& statInfoOveral, bool bUpdateOveralStatus = false);

	taskUpdateInfo_t m_TaskUpdateInfo;
	string m_strCmdHttps;
	bool m_bUpdateStep;
	double m_vConVersion;
	std::string m_strDataDirPath;
	std::string m_strCxPath;
	std::string m_strLogFile;
	std::string m_strOperation;
	std::map<std::string, std::string> m_mapVmIdToVmName;
	std::map<std::string, std::map<std::string, std::string> > m_mapVmToTgtScsiIdAndSrcDisk;
	std::map<std::string, std::map<std::string, std::string> > m_mapVmToSrcandTgtVolOrDiskPair;
	map<string, statusInfo_t> m_mapHostIdToStatus;
	RecoveryPlaceHolder m_PlaceHolder;

#ifndef WIN32
	bool xGetValuesFromXML(VxJobXml &VmObj, std::string FileName, std::string HostName);
	bool xCheckXmlFileExists();
	bool xGetvConVersion();
    bool SetVxJobs(string &strErrorMsg, string &strFixStep);
    bool MakeXmlForCxcli(std::vector<VxJobXml> vecVxVolInfo, std::string strVolFileName);
    bool ExecuteCmdWithOutputToFile(const std::string Command, const std::string OutputFile);
    bool ProcessCxcliOutputForVxPairs(std::string strCxCliOutputXml);
    bool FetchVxCommonDetails(std::string &strPlanName, std::string &strBatchResyncCount, std::string &strUseNatIpForSrc, std::string &strUseNatIpForTgt);
    bool readIpAddressfromEsxXmlfile(std::string HostName, std::string & HostIp, bool bsrc);
	bool readInMageHostIdfromEsxXmlfile(string HostName, string &strHostInMageId,bool bsrc);
    void RescanTargetDisk(std::string TgtDisk);
	bool restartLinuxService(std::string strService);
	bool startLinuxService(std::string strService);
	bool stopLinuxService(std::string strService);
	bool getLinSrcNetworkInfo();
	bool CreateInitrdImgForOS( std::string );
	bool MakeInformation( Configurator* );
	bool WriteProtectedDiskInformationInOrder( std::string , bool );
	bool removeLsscsiId(std::string strLsscsiID);
	bool addLsscsiId(std::string strLsscsiID);
	bool reloadMultipathDevice();
	bool GetDiskUsage();
	bool GetMemoryInfo();

#endif
	
private:
	std::string m_strLogDir;
	std::string m_strHostID;
	std::string m_strPlanId;
	bool m_bPhySnapShot;
	bool m_bVolResize;
	bool restartVxAgent();
	bool createMappingFile();
	bool parseLsscsiOutput();
	bool PostXmlFileToServer(const char *url,std::string xmlFile,int operation);
	std::vector<std::string> parseCsvLine(const std::string &);
	std::string GetDiskNameForNumber(int disk_number) const;
	bool mapSrcDisksInMt();
	bool convertTokenIntoProperScsiFormat(std::string &);
	bool convertDeviceIntoMultiPath(std::list<std::string> listOfPhysicaldeviceNames, std::list<std::string>& listOfMultiPathNames);
	bool convertToMultiPath(std::string strPhysicalDeviceName, std::string& strMultiPathName);
	bool rescanScsiBus();
	bool executeScript(const std::string &);
	bool createSrcTgtDeviceMapping();
	bool editMultiPathFile();
	std::string filterScsiId(std::string strScsiDiskId);
	std::map<std::string ,std::string>readProtectedVmsIpAddresses ()const;
	
	std:: map<std::string,std::string> mapOfScsiCont;
	std::map<const std::string,std::vector<int> >m_mapOfVmAndItsDiskVector;
	std::string m_strVxInstallPath ;
	std::string m_strCxIpAddress;
	std::string m_MasterTaregtIpAddres;
	std::string m_XmlFileForCxCli;
	std::string m_LocalHostIpAddress;
	std::map<std::string,std::list<std::string> >m_mapOfVmsAndPhysicalDevices;
	std::map<std::string,std::map<std::string,std::string> >m_mapOfSrcTgtPhysicaldevices;
	static char posix_device_names[60][10];
    LocalConfigurator m_obj;
	std::map<std::string,std::list<std::string> >m_mapOfVmAndScsiIds;
	std::map<std::string,std::string>m_mapOfScsiCont;
	bool m_bIsAdditionOfDisk;
	
#ifndef WIN32
	bool editLvmConfFile();
	bool getEtcMntDevicePath(std::string& strEtcMntPath, bool& bEtcMounted);
	bool isLvOrPartition(std::string strEtcMntPath);
	void getVgLvName(std::string strEtcMntPath, std::string& strLvName, std::string& strVgName);
	bool getPvOfVg(std::string strVgName, std::list<std::string>& listDiskName);
	bool getNetworkDetails();
	bool getNetworkDevices();
	bool writeNwInfoIntoXmlFile(std::string strNwXmlFile, StorageInfo_t stgInfo);
	bool swapFile(std::string strCurFile, std::string strSwapFile, std::string strTempFile);
	bool getOsType(std::string strOsType);
	bool RegisterHostUsingCdpCli();
	bool ProcessCxCliCall(const std::string strCxCliPath, const std::string strCxUrl, std::string strInput, bool isTargetVol, std::string &strOutput);
	bool bSearchVolume(std::string FileName, std::string VolumeName);
	bool GetSrcIPUsingCxCli(std::string strGuestOsName, std::string &strIPAddress);
	bool GetHostIpFromXml(std::string FileName, std::string strHost, std::string &strHostIP);
	void addAndRemoveScsiEntry();
	bool cretaeMapOfVmToSrcAndTgtDevice();
	bool createMapperPathMap(std::map<std::string, std::string> mapSrcDiskToTgtDisk, std::map<std::string, std::string>& mapTgtMultipathToSrcDisk);
	std::string ConstructXmlForCxApi( std::string , std::string , std::string );
	bool ValidateExistingInitrdImg( const char* );
	std::string GetKernelVersion();
	bool CreateInitrdImg( std::string );
	bool xmlGetOsName( std::string , std::string& );
	bool PreCreateInitrdImg( std::string , std::string );
	bool PostCreateInitrdImg( std::string , std::string );
	bool ConstructInitrdImgNameAndCmd( std::string , std::string& , std::string& );
	bool GetSrcReplicatedVolumes(std::string, std::string&, Configurator*);
	void UpdateAoDFlag();
	std::string FetchInMageHostId();
	void InitialiseTaskUpdate();
	void InitialiseSnapshotTaskUpdate();
	void InitialiseVolResizeTaskUpdate();
	bool createDir(string Dir);
	bool SrcTgtDiskMapFromScsiFileV2P();
	bool getMapOfMultipathToScsiId(map<string, string>& mapOfMultipathToScsiId);
	bool reloadDevices(map<string, string>& mapOfMultipathToScsiId);
	bool GetListOfTargetDevice(string hostName, list<string>& listTargetDevice);

	bool m_bRedHat;
	bool m_bSuse;
	bool m_bV2P;
	std::string m_strLsscsiID;
	std::string m_strOsFlavor;
	std::string m_strMntFS;
	std::string m_strOsType;
	std::map<std::string, std::string> m_mapLsScsiID; 
    std::vector<VxJobXml> m_vecVxJobObj;
	std::list<std::string> m_listScsiId;
	std::list<NetworkInfo_t> m_NetworkInfo;
	std::map<std::string, std::string> m_mapMacToEth;
	std::string m_strXmlFile;
	std::map<std::string,std::list<std::string> > m_InitrdOsMap;
	std::list<std::string> m_RedHatReleaseList;
	std::list<std::string> m_SuseReleaseList;
	std::list<std::string> m_CentOsReleaseList;
#endif
};

#endif
