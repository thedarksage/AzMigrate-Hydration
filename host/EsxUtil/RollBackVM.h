#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <ostream>
#include <fstream>
#include <string>
#include <sstream>
#include "ace/Process.h"
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/OS_Dirent.h>
#include "ace/Process_Manager.h"
#include <boost/algorithm/string.hpp>

#include "cdpcli.h"
#include "cdputil.h"
#include "svtypes.h"
#include "VacpUtil.h"
#include "localconfigurator.h"
#include "LinuxReplication.h"

#define DISKBIN_EXTENSION           "_disk.bin"
#define SCSI_DISK_FILE_NAME         "_ScsiIDsandDiskNmbrs.txt"
#define PAIRINFO_EXTENSION          ".pInfo"
#define SNAPSHOTINFO_EXTENSION      ".sInfo"
#define MBR_FILE_NAME				".MBR"
#define GPT_FILE_NAME				".GPT"
#define TGT_VOLUMESINFO             ".tgtVolInfo"

#define SOURCE_HOSTNAME             "SourceHostName"
#define MT_HOSTNAME                 "MTHostName"

#define MT_ENV_VMWARE				"vmware"

//Windows specific.
#ifdef WIN32
 #include <boost\lexical_cast.hpp>
 #include <windows.h>
 #include "dlmapi.h"
#else
 #include <sys/stat.h> 
#endif

class ChangeVmConfig;

#ifdef WIN32

typedef struct _VolumeInformation
{
	char  strVolumeName[MAX_PATH + 1];
	LARGE_INTEGER startingOffset;
	LARGE_INTEGER partitionLength;
	LARGE_INTEGER endingOffset;
}volInfo;

typedef struct _DiskInfomartion
{
	unsigned int    uDiskNumber;
	LARGE_INTEGER   DiskSize;        //Totoal Disk Size
	ULONG		  	ulDiskSignature; //Disk Signature
	enum			g_Disktype dt;      //DiskType
	enum			g_PartitionStyle pst;
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

/*enum g_Disktype        {BASIC = 0,DYNAMIC};
enum g_PartitionStyle  {MBR = 0,GPT,RAW};*/

#endif /* WIN32 */

typedef multimap<string,string>::iterator MMIter;

/* This structure is for implementing the cdpcli parallelisation execution */

typedef struct _CdpcliJob
{
	string uniqueid;
	string prescript;
	string postscript;
	string cdpclicmd;
	string cmdinfile;
	string cmdoutfile;
	string cmderrfile;
	string cmdstatusfile;

	_CdpcliJob()
	{
		uniqueid.clear();
		prescript.clear();
		postscript.clear();
		cdpclicmd.clear();
		cmdinfile.clear();
		cmdoutfile.clear();
		cmderrfile.clear();
		cmdstatusfile.clear();
	}
}CdpcliJob;

enum RecoveryStatus 
{
	RECOVERY_STARTED = 0,             //Marks as recovery started for the host (default)
	TAG_VERIFIED,                     //Marks as Tag verified for that particular host
	CDPCLI_ROLLBACK,                  //Marks as Cdpcli Rollback completed for that host
	POST_ROLLBACK,                    //Marks as post rollback operation completed for that host
	DISK_SIGNATURE_UPDATE,            //Marks as disk signature update completed for this host
	RECOVERY_COMPLETED                //Marks as recovery completed for the host
};

typedef struct _RecoveryFlowStatus
{
	RecoveryStatus recoverystatus;
	string tagname;                    //For time basedcase , it is filled with time.
	string tagtype;                    //For time bassed, this is filled as "TIME"

	_RecoveryFlowStatus()
	{
		recoverystatus = RECOVERY_STARTED;
		tagname.clear();
		tagtype.clear();
	}
}RecoveryFlowStatus;







/********************************************************************/

#include "cdpplatform.h"
#include "hostagenthelpers_ported.h"
//#include "hostagenthelpers.h"
#include "rpcconfigurator.h"
#include "configwrapper.h"
/********************************************************************/

extern bool not_comma(char);
extern bool isComma(char);

class RollBackVM
{
public:
    RollBackVM(bool bExtraChecks, bool bFetchDiskNumMap, bool bPhySnapShot, string strDirPath, string strCxPath, string strPlanId, string strOperation)
	{        
        m_bExtraChecks = bExtraChecks;
        m_bFetchDiskNumMap = bFetchDiskNumMap;
		m_bPhySnapShot = bPhySnapShot;
		m_strDatafolderPath = strDirPath;
		m_strCxPath = strCxPath;
		m_strPlanId = strPlanId;
		m_strOperation = strOperation;
		m_bClus = false;
		m_bCloudMT = false;
		m_strCloudEnv = "";
		m_strConfigFile = "";
		m_strResultFile = "";
		m_vDInfoVersion = 1.0;
		m_strHostName = "";
		m_protectionUnit = cdp_volume_t::CDP_REAL_VOLUME_TYPE;
	}
	RollBackVM()
	{
        m_bExtraChecks = false;
        m_bFetchDiskNumMap = false;
		m_bPhySnapShot = false;
		m_strCxPath = "";
		m_strPlanId = "";
		m_strOperation = "";
		m_bClus = false;
		m_bCloudMT = false;
		m_strCloudEnv = "";
		m_strConfigFile = "";
		m_strResultFile = "";
		m_vDInfoVersion = 1.0;
		m_strHostName = "";
		m_protectionUnit = cdp_volume_t::CDP_REAL_VOLUME_TYPE;
	}   

	string m_strLogFile;
	LinuxReplication m_objLin;
	bool m_bClus;
	bool m_bP2V;
	bool startRollBack();
	bool startRollBackCloud();
    bool checkIfFileExists(std::string &strFileName);
    bool startVxServices(const std::string &);
	bool stopVxServices(const std::string &);
	bool CdpcliOperations(const string& cdpcli_cmd);
	bool RemoveReplicationPairs();
	bool m_bSkipCdpcli;
	string m_strHostName;
	void SetCloudEnv(bool bCloudEnv = false, string strCloudEnv = "", string strConfigFile = "", string strResultFile = "")
	{
		m_bCloudMT = bCloudEnv;
		m_strCloudEnv = strCloudEnv;
		m_strConfigFile = strConfigFile;
		m_strResultFile = strResultFile;
	}

	std::map< const std::string, RecoveryFlowStatus > map_hostIDRecoveryFlowStatus;
	std::map<std::string,std::map<std::string, std::string> > m_mapOfVmToSrcAndTgtDisks;
	std::map<std::string,std::map<std::string, std::string> > m_mapOfVmToSrcAndTgtDiskSig;
#ifdef WIN32
    std::map<std::string,std::map<unsigned int,unsigned int> > m_mapOfVmToSrcAndTgtDiskNmbrs;
	bool GetDiskSize(const char *DiskName, SV_LONGLONG & DiskSize);
	bool RestoreTargetDisks(string& strHostName);
    bool ConfigureMountPoints(std::string hostInfoFile);
#else
	bool rescanDisk(std::string disk);
#endif

private:
    bool xGetHostDetailsFromXML();
	bool GenerateSrcDiskToTgtScsiIDFromXml(map<string, map<string, string> >& mapHostTomapOfDiskDevIdToNum);
	bool GetRollbackDetailsFromConfigFile(StatusInfo& statInfoOveral);
	bool GetHostNameHostIdFromRecoveryInfo(std::map<std::string,std::map<std::string, std::string> > & recoveryInfo);
    std::string getVxAgentInstallPath();
	std::vector<std::string> parseCsvFile(const std::string &);
	bool CreateSnapshotPair(std::map<std::string,std::string> mapProtectedPairs,std::map<std::string,std::string> mapSnapshotPairs,std::map<std::string,std::string>& mapProtectedToSnapshotVol);
	bool writeRecoveredVmsName(std::vector<std::string>);
	bool removeRollbackFile();
    bool getRetentionSettings(const std::string& volName, std::string& retLogLocation);
	bool RollBackVMs(std::string HostName, std::string Value, std::string Type, std::string& strCdpCliCmd, StatusInfo& statInfo , RecoveryFlowStatus& );
	bool IsRetentionDBPathExist(const std::string& strVolume, const std::string& strValue, const std::string& strType);
    bool GetRetentionDBPaths(std::map<std::string,std::string> mapProtectedPairs, 
                             std::vector<std::string> & vRetDbPaths,
							 std::map<std::string, std::string>& mTgtVolToRetDb);
    bool ValidateAndGetRollbackOptions(std::vector<std::string> vRetDbPaths, 
                                       std::string Value, 
                                       std::string Type,
                                       std::string & LatestTagOrTime);
    bool RollbackPairsOfVM(std::map<std::string, std::string> mapProtectedPairs,
                             std::string Value,
                             std::string Type,
                             std::string LatestTagOrTime,
							 std::string& strCdpCliCmd);
	bool SnapshotOnPairsOfVM(std::map<std::string, std::string> mapProtectedPairs,
                                   std::string Value,
                                   std::string Type,
                                   std::string LatestTagOrTime,
								   std::string& strCdpCliCmd);
	bool ArraySnapshotOnPairsOfVM(std::map<std::string, std::string> mapProtectedToSnapshotVol,
                                   std::string Value,
                                   std::string Type,
                                   std::string LatestTagOrTime,
								   std::string& strCdpCliCmd,
								   std::map<std::string, std::string> mTgtVolToRetDb);
    bool ValidateLatestTag(std::vector<std::string> vRetDbPaths, std::string TagType, std::string & LatestTagOrTime);
    bool ValidateLatestTime(std::vector<std::string> vRetDbPaths, std::string & LatestTagOrTime);
    bool ValidateSpecificTag(std::vector<std::string> vRetDbPaths, std::string TagType, std::string TagValue);
    bool ValidateSpecificTime(std::vector<std::string> vRetDbPaths, std::string TimeValue);
	bool GetMapOfSrcDiskToTgtDiskNumFromDinfo(const string strVmName, string strDInfofile, map<string, dInfo_t>& mapSrcToTgtDiskNum);
	void InitRecoveryTaskUpdate();
	void InitDrdrillTaskUpdate();
	void InitRemovePairTaskUpdate();
	bool PrpareInmExecFile(list<CdpcliJob>& stCdpCliJobs, const string& FileName);
	bool RunInmExec(const string& strFileName, const string& strOutputFileName);
	bool PreparePinfoFile(map<string, map<string, string> >& mapOfVmIdToTgtNewPairList);
	bool RemoveReplicationPairs(map<string, set<string> >& mapOfVmIdToTgtRemovePairList);
	bool UpdateRecoveryProgress(const std::string& strHost, const std::string& strDirPath, RecoveryFlowStatus& RecStatus);
	bool ReadRecoveryProgress(const std::string& strHost, const std::string& strDirPath, RecoveryFlowStatus& RecStatus);
	bool DeleteRecoveryProgressFiles(const std::string& strHost, const std::string& strDirPath);
	bool GenerateProtectionPairList(const std::string& strHost, std::map<std::string, std::string>& mapNewPairs, const std::string& strValue, const std::string& strType);
	bool BookMarkTheTag(const std::string& strTagName, const std::string& strTgtVol, const std::string& strTagType);
	bool getVxPairs(const std::string& srcHost, std::map<std::string, std::string> & mapVxPairs);
	std::map< std::string, std::string> GetProtectedPairsForHostID( const std::string &);
	bool MapPolicySettingsToFMA( RecoveryFlowStatus&, RecoveryFlowStatus&, StatusInfo & );
	void ListRecoveryStatusOfPolicy(std::map< const std::string, RecoveryFlowStatus >&);
	bool CollectCdpCliListEventLogs(const std::string& strTgtVol);
	bool ReadLogFileAndPrint(const string& strFileName);
	bool PreProcessRecoveryPlan(std::map<std::string, StatusInfo >&);
	std::string GetCurrentLocalTime();

#ifdef WIN32
    void UnmountAndDeleteMtPoints(vector<std::string> vecRecoveredVMs);
    void PollForVolumePair(std::string devicename, std::string srcHostName);
    bool SearchKeyMapSecond(std::string key, map<std::string,std::string> mapPairs);
    bool createMapOfVmToSrcAndTgtDiskNmbrs();
	bool createMapOfVmToSrcAndTgtDiskNmbrs(string& strHostName);
	bool generateMapOfVmToSrcAndTgtDiskNmbrs(string& strErrorMsg);
	bool GenerateDisksMap(string HostName, map<unsigned int, unsigned int> & MapSrcToTgtDiskNumbers);
    bool GetMapVolNameToDiskNumberFromDiskBin(string HostName, 
                                              map<string, unsigned int> & SrcMapVolNameToDiskNumber);
	bool GetMapVolNameToDiskNumber(string HostName,
                                   map<string, unsigned int> & TgtMapVolNameToDiskNumber,
                                   map<string, string> & MapSrcVolToTgtVol);
    bool GetDiskNumberOfVolume(string VolName, unsigned int & dn);
    bool ReadFileWith2Values(string strFileName, map<string,string> &mapOfStrings);
    bool ConvertMapofStringsToInts(map<string,string> mapOfStrings, map<int,int> &mapOfIntegers);
    bool isAnInteger(const char *pcInt);
    void FormatVolumeNameForCxReporting(std::string&);
    bool RestoreDiskSignature(const std::string& strHostName);
	bool UpdateDiskWithMbrData(const char* chTgtDisk, SV_LONGLONG iBytesToSkip, const SV_UCHAR * chDiskData, unsigned int noOfBytesToDeal);
    bool UpdateDiskSignature(unsigned int iDiskNumber, LARGE_INTEGER iBytesToSkip, const string strFileName, unsigned int noOfBytesToDeal);
    bool ChkdskVolumes(std::map<std::string,std::string> mapProtectedPairs);
	bool GetSystemDrive(std::string& systemDrive);
	bool GetMapVolNameToDiskNumberFromDiskInfoMap(string HostName, map<string, unsigned int> & SrcMapVolNameToDiskNumber);
	bool UpdateMbrOrGptInTgtDisk(string hostname, map<unsigned int, unsigned int> mapOfSrcAndTgtDiskNmbr);
	void CleanupDiskInfoOfAllVm();
	bool ConvertMapofStringsToInts(map<string,string> mapOfStrings, map<unsigned int,unsigned int> &mapOfIntegers);
	bool CleanTargetDisk(unsigned int nDiskNum);
	bool OfflineTargetDisk(unsigned int nDiskNum);
	bool UpdatePartitionInfo(const string& HostName, map<unsigned int, unsigned int>& mapOfSrcAndTgtDiskNmbr);
	bool ProcessW2K8EfiPartition(const string& strTgtDisk, DlmPartitionInfoSubVec_t & vecDp);
	void NewlyGeneratedVolume(set<string>& stPrevVolumes, set<string>& stCurVolumes, list<string>& strNewVolume);
	void GetEfiVolume(list<string>& lstVolumes, DlmPartitionInfoSubVec_t & vecDp, string& strEfiVol);
	bool UpdateW2K8EfiChanges(string & strEfiMountPoint);
	bool PrepareDinfoFile(map<string, set<string> >& mapOfVmIdToSrcDiskList);
	bool CreateSnapshotPairForClusNode(std::string node, std::map<std::string,std::string> mapProtectedPairs,std::map<std::string,std::string> mapSnapshotPairs,std::map<std::string,std::string>& mapProtectedToSnapshotVol);
	bool GetClusterDiskMapIter(std::string nodeName, std::string strSrcDiskSign, DisksInfoMapIter_t & iterDisk);
	bool IsClusterNode(string hostName);
	bool IsWin2k3OS(string hostId);
	void GetClusterNodes(string clusterName, list<string> & nodes);
	string GetNodeClusterName(string nodeName);
	bool DownloadDlmFile(string strHostName, string& strDlmFile);
	bool AssignMountPointToTgtVols(const std::string& strHostName);
	bool GetMappingFromFile(std::string& strFile, std::map<std::string, std::string>& strMap);

	map<string, DisksInfoMap_t> m_mapOfHostToDiskInfo;
#else
	void runFlushBufAndKpartx(const string& hostName);
	bool vgScan();
	bool pvScan();
	bool activateVg(string hostName);
	bool deActivateVg(string hostName);
	bool blockDevFlushBuffer(string disk);
	bool ShowPartitionsMultipath(string disk);
	bool RemovePartitionsMultipath(string disk);
	bool addEntryMpathConfigFile(string hostName);
	bool editLVMconfFile(string strLvmFile);
	bool createMapOfVmToSrcAndTgtDisk(string& strErrorMsg);
	bool generateDisksMap(string hostName, map<string, string>& mapSrcToTgtDisks);
	bool repalceOldLVMconfFile(string hostName);
	bool repalceOldMultiPathconfFile(string hostName);
	bool checkOStypeFromXml(string xmlFileName);
	void removeMultipathDevices(const string& hostName);
	void removeLogicalVolumes(std::set<std::string>& lvs);
	void findLscsiDevice(const string& hostName, list<string>& lstLsscsiId);
	void removeLsscsiDevice(list<string>& lstLsscsiId);
	void collectMessagesLog();
	void collectDmsegLog();
	bool removeMultipath(const string& strMultipathDev, set<string>& setPartitions);
	bool getLsscsiId(const string& strMultipathDev, string& strLsscsiId);
	bool getStandardDevice(const string& strMultipathDev, string& strStandardDevice);

#endif

	bool m_bArraySnapshot;
	std::string m_VmName;
	std::string m_TagValue;
	std::string m_TagType;
	std::string m_OsFlavor;
	std::string m_strXmlFile;
	std::string m_strCloudEnv;
	std::string m_strConfigFile;
	std::string m_strResultFile;
	//std::string m_strRollBackInfoFile;
	bool m_bCloudMT;
    bool m_bExtraChecks;
    bool m_bFetchDiskNumMap;
	bool m_bPhySnapShot;
	bool m_bV2P;
	std::pair<std::string,std::map<std::string,std::string> >m_RollBackinfoForVM;
    map<string, pair<string,string> > rollbackDetails;
	map<string,vector<string> >m_mapIPChangeDetails;
	LocalConfigurator m_obj;
	double m_vConVersion;
	std::string m_strDatafolderPath;
	std::string m_InMageCmdLogFile;
	std::string m_strCxPath;
	std::string m_strPlanId;
	std::string m_strOperation;
	std::map<std::string, std::string> m_mapOfVmNameToId;
	std::map<std::string, std::string> m_mapHostToType;
	std::map<std::string, std::string> m_mapOfVmToClusOpn;
	VMsRecoveryInfo m_RecVmsInfo;
	VMsCloneInfo m_SnapshotVmsInfo;
	double m_vDInfoVersion;
	std::multimap<std::string, std::string> m_mapClusterNameToNodes;
	map<string, double> m_mapOfHostIdDlmVersion;
	map<string, string> m_mapOfClusNodeOS;
	std::map<std::string, RecoveryFlowStatus> m_mapOfHostToRecoveryProgress;

	// A2E failback support

	cdp_volume_t::CDP_VOLUME_TYPE m_protectionUnit;


	bool isFailback()
	{
		return boost::algorithm::iequals(m_strCloudEnv, "vmware");
	}

	bool isVirtualizationTypeVmWare()
	{
		return boost::algorithm::iequals(m_strCloudEnv, "vmware");
	}

	bool usingDiskBasedProtection()
	{
		return (m_protectionUnit == cdp_volume_t::CDP_DISK_TYPE);
	}

	void SetFailbackProperties()
	{
#ifdef WIN32
		m_protectionUnit = cdp_volume_t::CDP_DISK_TYPE;
#else
		m_protectionUnit = cdp_volume_t::CDP_REAL_VOLUME_TYPE;
#endif
	}

#ifdef WIN32
	bool ParseSourceOsVolumeDetails(const std::string &srcVMId, ChangeVmConfig & VMObj);
	bool ParseSourceOsVolumeDetailsForForwardProtection(const std::string &srcVMId, ChangeVmConfig & VMObj);
	bool ParseSourceOsVolumeDetailsForFailbackProtection(const std::string &srcVMId, ChangeVmConfig & VMObj);

#endif

};
