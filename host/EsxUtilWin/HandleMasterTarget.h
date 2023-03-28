#ifndef HANDLE_MASTER_TARGET
#define HANDLE_MASTER_TARGET

#include "Common.h"
#include "ace/Process.h"
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "ace/Process_Manager.h"
#include <Winsock2.h>
#include <shlwapi.h>
#include <assert.h>
#include "vds.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <sddl.h>
#include "applicationsettings.h"
#include "serializeapplicationsettings.h"

#include "genericwmi.h"
#include "volinfocollector.h"
#include "deviceidinformer.h"

#include <set>
//libraries required to link with VDS
#pragma comment(lib,"Ole32.lib")
#pragma comment(lib,"Advapi32.lib")

#define _SafeRelease(x) {if (NULL != x) { x->Release(); x = NULL; } }

#ifndef RAND_MAX
#define RAND_MAX	99999999
#endif

typedef multimap<string,string>::iterator MMIter;

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

struct SendAlerts
{
	map<string, string> mapParameters;  //Paramater inside the function request
	map<string, map<string, string> > mapParamGrp;
};

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
    string VmHostName;
    string SourceMachineIP;
    string TargetMachineIP;
	string SourceInMageHostId;
	string TargetInMageHostId;
    string SourceVolumeName;
    string TargetVolumeName;
	string ProcessServerIp;
    string RetentionLogVolumeName;
	string RetentionLogPath;
	string RetainChangeMB;
	string RetainChangeDays;
	string RetainChangeHours;
	string IsSourceNatted;
	string IsTargetNatted;
	list<SparseRet_t> sparseTime;
	string RetTagType;
	string SecureSrcToPs;
	string SecurePsToTgt;
	string Compression;
	VxJobXml();
};

typedef struct diskinfo_tag
{
	//public:
	diskinfo_tag() : capacity(0) {};

	std::string displayName;
	std::string scsiBus;
	std::string scsiPort;
	std::string scsiTgt;
	std::string scsiLun;
	std::string scsiId;
	unsigned long long capacity;

} Diskinfo_t;

typedef std::map<std::string, Diskinfo_t> DiskNamesToDiskInfosMap;

class WmiDiskRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
	WmiDiskRecordProcessor(DiskNamesToDiskInfosMap *pdiskNamesToDiskInfosMap);
	bool Process(IWbemClassObject *precordobj);
	std::string GetErrMsg(void)
	{
		return m_ErrMsg;
	}

private:

	DiskNamesToDiskInfosMap *m_pdiskNamesToDiskInfosMap;

	// list to track device names (ids)
	// used for removing devices with duplicate ids from getting reported
	std::set<std::string> m_scsiIds;

	std::string m_ErrMsg;
	DeviceIDInformer m_DeviceIDInformer;
};


class MasterTarget:public Common
{
public:	
	MasterTarget()
	{
		bIsFailbackEsx = false;
		m_strHostID = "";
		m_bPhySnapShot = false;
		m_strDirPath = "";
		m_strCxPath = "";
		m_strPlanId = "1";
		m_strOperation = "";
		m_bVolResize = false;
		m_bCloudMT = false;
		bIsAdditionOfDisk = false;
	}
	MasterTarget(bool bValue, string strHostID, bool bPhySnapShot, string strDirPath, string strCxPath, string strPlanId, string strOperation)
	{
        bIsFailbackEsx = bValue;
        m_strHostID = strHostID;
		m_bPhySnapShot = bPhySnapShot;
		m_strDirPath = strDirPath;
		m_strCxPath = strCxPath;
		m_strPlanId = strPlanId;
		m_strOperation = strOperation;
		m_bVolResize = false;
		m_bCloudMT = false;
		bIsAdditionOfDisk = false;
	}
	~MasterTarget()
	{
	}
	int initTarget(void);
	int GetDiskAndMbrDetails(const string& strMbrFile);
	int GetDiskAndPartitionDetails(const string& strPartitionFile);
	int UpdateDiskAndPartitionDetails(const string& strPartitionFile);
	bool CopyDirectory(const std::string& src, const std::string& dest, bool recursive);
	int PrepareTargetDisks(const std::string & strConfFile, const std::string & strResultFile);

	int PrepareTargetDisksForFailback(const std::string & strConfFile, const std::string & strResultFile);

	string m_strLogFile;

private:

	// A2E Failback support routines - start

	bool RemoveMissingDisks();
	bool CollectAllDiskInformation(DiskNamesToDiskInfosMap & diskNamesToDiskInfosMap);
	bool DetectAndInitializeTargetDisks(const DiskNamesToDiskInfosMap &diskNamesToDiskInfosMap, const std::string &srcVm, const std::map<std::string, std::string> & srctgtLuns, std::map<std::string, std::string> & srctgtDisks);
	bool FindMatchingDisk(const DiskNamesToDiskInfosMap & diskNamesToDiskInfosMap, const std::string &tgtLun, Diskinfo_t & tgtdiskinfo);
	bool InitializeDisk(const Diskinfo_t & tgtdiskinfo);
	void display_devlist(DiskNamesToDiskInfosMap & diskNamesToDiskInfosMap);

	// A2E failback support routines - end


	BOOL rescanIoBuses();
    bool createMapOfVmToSrcAndTgtDiskNmbrs();
	bool createMapOfVmToSrcAndTgtDiskNmbrs(bool bVolResize); //Used in case of Volume resize
    bool ReadFileWith2Values(string, map<string,string> & );
    bool ReadDiskInfoFile(string strInfoFile, map<string, dInfo_t>& mapPairs);
    bool ConvertMapofStringsToInts(map<string,string>, map<int,int> & );
    bool GetVectorFromMap(map<int,int>, vector<int> &, string );
	HRESULT ConfigureTgtDisksSameAsSrcDisk();
	BOOL FetchAllTargetvolumes (HANDLE , TCHAR *, int);
	int startTargetVolumeEnumeration(bool ,bool );
	HRESULT createMountPointForVolumeGuid(const string &,int );
	int WriteMbrToDisk(unsigned int,LARGE_INTEGER ,const string &,unsigned int);
    HRESULT ReadDiskInfo(int, FILE *, DiskInformation*, vector<pair<PartitionInfo,volInfo*>>&);
	HRESULT processDisk(const string &,unsigned int ,DiskInformation* ,vector<pair<PartitionInfo,volInfo*> >& );
	int processTargetSidedisks();
	bool DumpVolumesOrMntPtsExtentOnDisk( );
	HRESULT restartService(const string& );
	bool mapSrcAndTgtVolumes();
	BOOL IsThisMasterTgtVol(const string &);
	HRESULT setReplicationPair(string, map<int,int>, map<string,string> & );
    string  FetchHostIPAddress();
	string FetchInMageHostId();
	void RemoveTrailingCharactersFromVolume(std::string& volumeName);
    map<string,string>readPersistedIPaddress();
    bool ProcessCxCliCall(const string, string, bool, string &);
    bool persistReplicatedTgtVolume(const std::string &);
    bool IsGuidAlreadyMounted(const std::string &);
    bool IsItMtPrivateDisk(const unsigned int &);
    bool GetMasterTargetVolumesExtent();
    //HRESULT MasterTarget::findAllvolumesPresentInAllSource();
    xmlNodePtr xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry);
	xmlNodePtr xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue); 
	bool xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue);
	bool xGetValuesFromXML(VxJobXml &VmObj, string FileName, string HostName);
	bool xCheckXmlFileExists();
    bool bCompareFiles(string, string, unsigned int);
    bool bMyStrCmp(const unsigned char *, const unsigned char *, unsigned int);
    bool CompareAndWriteMbr(unsigned int, LARGE_INTEGER, const string &, unsigned int);
    bool SetVxJobs(string &strErrorMsg, string &strFixStep);
    bool SeparateVolumesSet(map<string,VxJobXml>, string, vector<VxJobXml> &, vector<VxJobXml> &);
	bool readIpAddressfromEsxXmlfile(string &HostName,string &HostIp,bool bsrc);
	bool readInMageHostIdfromEsxXmlfile(string HostName, string &strHostInMageId,bool bsrc);
    bool MakeXmlForCxcli(vector<VxJobXml>, string);
    bool PostXmlToCxcli(string, string, int);
    bool UpdateFailedVolumes(vector<VxJobXml>);
    bool UpdateInfoFile(string, map<string,string>, string);
	bool UpdateDiskInfoFile(string strSrcVmName, map<string, dInfo_t> mapNewPairs, string strExtn);
	bool FetchVxCommonDetails(std::string &strPlanName, std::string &strBatchResyncCount);
    void UpdateAoDFlag();
	void UpdateV2PFlag();
	void UpdateArrayBasedSnapshotFlag();
    bool bSearchVolume(string FileName, string VolumeName);
    bool ClearMountPoints(string DeviceName);
    bool CleanESXFolder();
    bool StopService(const string serviceName);
    bool ProcessCxcliOutputForVxPairs();
    bool BringAllDisksOnline();
    int CheckAllRequiredDisksFound();
    bool GetMapOfHostsToScsiIds(map<string, map<string,string> > & mapHostsToScsiIds);
    bool DiscoverAndCollectAllDisks();
	void CollectVsnapVols();
	bool GetHostIpFromXml(string strTempXmlFile, string strInPut, string &strOutPut);
	bool GetSrcIPUsingCxCli(string strGuestOsName, string &strIPAddress);
	bool ListHostForProtection();
	bool xGetMbrPathFromXmlFile(string strHostId, string& strMbrPath);
	bool GenerateSrcDiskToTgtScsiIDFromXml(const string& strHostname, map<string,string>& mapOfSrcScsiIdsAndTgtScsiIds, map<string,string>& mapOfSrcDiskNmbrsAndTgtScsiIds);
	bool GenerateSrcScsiIdToDiskSigFromXml(const string& strHostname, map<string,string>& mapOfSrcScsiIdsAndTgtScsiIds, map<string,string>& mapOfSrcDiskScsiIdToDiskSig);
	bool ReadSrcScsiIdAndDiskNmbrFromMap(string strVmName, map<string,string>& mapOfSrcScsiIdsAndSrcDiskNmbrs);
	bool UpdateMbrOrGptInTgtDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr);
	bool CreateMapOfSrcDisktoVolInfo(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr);
	bool ImportDynamicDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmb);
	bool CleanAttachedSrcDisks(bool bOffline = true);
	bool ClearReadOnlyAttributeOfDisk();
	bool FillZerosInAttachedSrcDisks();
	bool CleanSrcGptDynDisks();
	bool InitialiseDisks(std::string& strErrorMsg);
	bool xGetHostNameFromHostId(const string strHostId, string& strHostName);
	bool UpdateSignatureDiskIdentifierOfDisk(string hostname , map<int, int> mapOfSrcAndTgtDiskNmbr);
	bool OfflineOnlineBasicDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr);
	bool ConvertToMbrToGpt(int nDiskNum, bool bDyn = false);
	bool RestartVdsService();
	bool CheckOnlineStatusDisks(string hostname , map<int, int> mapOfSrcAndTgtDiskNmbr);
	bool CollectGPTData(string strHostName, unsigned int nDiskNum);
	bool OfflineDiskAndCollectGptDynamicInfo();
	bool UpdateDiskWithMbrData(const char* chTgtDisk, SV_LONGLONG iBytesToSkip, const SV_UCHAR * chDiskData, unsigned int noOfBytesToDeal);
	bool ProcessCxPath();
	void InitProtectionTaskUpdate();
	void InitVolResizeTaskUpdate();
	void InitSnapshotTaskUpdate();
	string prepareXmlData();
	string PrepareCXAPIXmlData(CxApiInfo& cxapi);
	string ProcessParamGroup(list<CxApiParamGroup>& listParamGroup);
	bool processXmlData(string strXmlData, string& strOutPutXmlFile);
	void ModifyTaskInfoForCxApi(taskInfo_t currentTask, taskInfo_t nextTask, bool bUpdateProtection = false);
	void UpdateTaskInfoToCX(bool bCheckResponse = false);
	bool SendAlertsToCX(map<string, SendAlerts>& sendAlertsInfo);
	bool UploadFileToCx(string strLocalPath, string strCxPath);
	bool DownloadPartitionFile(string strCxPath, string strLocalPath, DisksInfoMap_t& srcMapOfDiskNamesToDiskInfo);
	bool RestartResyncforAllPairs();
	bool ResumeAllReplicationPairs();
	bool PauseAllReplicationPairs();
	bool PauseReplicationPairs(const string& strHostId, const string& strTgtHostId);
	bool ResumeReplicationPairs(const string& strSrcHostId, const string& strTgtHostId);
	bool RestartResyncPairs(const string& strSrcHostId, const string& strTgtHostId);
	bool OnlineVolumeResizeTargetDisk(string hostname, map<int, int> mapOfSrcAndTgtDiskNmbr);
	bool GetMapOfDiskSigToNumFromDinfo(const string strVmName, string strDInfofile, map<string, string>& mapTgtDiskSigToSrcDiskNum);
	bool GenerateDisksMap(string HostName, map<int, int> & MapSrcToTgtDiskNumbers);
	bool GetMapVolNameToDiskNumberFromDiskBin(string HostName, map<string, int> & SrcMapVolNameToDiskNumber);
	bool GetMapVolNameToDiskNumber(string HostName, map<string, int> & TgtMapVolNameToDiskNumber, map<string, string> & MapSrcVolToTgtVol);
	std::map<std::string,std::string> getProtectedDrivesMap(std::string &strVmName);
	bool GetDiskNumberOfVolume(string VolName, int & dn);
	bool ReadResponseXmlFile(string strCxApiResponseFile, string strCxApi, string& strStatus);
	bool GetRequestStatus(string strRequestId, string& strStatus);
	bool CreateHostToDynDiskAddOption();
	bool GetVmDisksInfoFromConfigFile(const std::string & strFileName);
	bool PersistPrepareTargetVMDisksResult(const std::string & strFileName);
	bool DowloadAndReadDiskInfoFromBlobFiles(std::string& strErrorMsg, std::string& strErrorCode);
	int VerifyAllRequiredDisks();
	bool CreateSrcTgtDiskMapForVMs();
	bool CreateSrcTgtVolumeMapForVMs(std::string& strErrorMsg);
	void UpdateSendAlerts(map<string, SendAlerts>& sendAlertsInfo);
	void UpdateSendAlerts(taskInfo_t& taskInfo, string& strCxPath, map<string, SendAlerts>& sendAlertsInfo);
    void UpdateSrcHostStaus();
	void UpdateSrcHostStaus(taskInfo_t & taskInfo);
	bool DownloadDlmMbrFileForSrcHost(string strHostId, string strMbrUrl);
	bool UnmountProtectedVolumes();
	bool UpdatePrepareTargetStatusInFile(string strResultFile, statusInfo_t& statInfo);
	void UpdateErrorStatusStructure(const std::string& strErrorMsg, const std::string& strErrorCode);
	bool RemoveRecoveryStatusFile();
	
	                        /******* MSCS Specifi member functions *******/
	bool xGetClusNodesAndClusDisksInfo();
	bool IsClusterNode(string hostName);
	bool IsClusterDisk(string hostName, const string diskSignature);
	bool IsWin2k3OS(string hostId);
	void GetClusterNodes(string clusterName, list<string> & nodes);
	string GetNodeClusterName(string nodeName);
	string GetSrcClusDiskSignature(const string& hostname, const string& diskname);
	//Make sure the diskIter is a valied iterator otherwise we may see unexpected behaviour.
	//   So before calling this funtion verify the diskIter is initialized and not pointing to end.
	//If cluster disk entry with full information found then the diskIter pointing to that new disk entry,
	//   Otherwise there will be no change in iterator.
	//Return value: If disk info found with volume information then true is returned otherwise false.
	bool GetClusDiskWithPartitionInfoFromMap(string nodeName, DisksInfoMapIter_t & diskIter);
	bool ReadSrcScsiIdAndDiskSignFromMap(string strVmName, map<string,string>& mapOfSrcScsiIdsAndSrcDiskNmbrs);
	bool GetClusterDiskMapIter(string nodeName, string strSrcDiskSign, DisksInfoMapIter_t & iterDisk);
	bool ClrMscsScsiRsrv(int nDiskNum);

	/************** variable ********************/
    //This map contains VM name to another map of source to target Disk Numbers
    std::map<string,map<int,int>> m_mapOfVmToSrcAndTgtDiskNmbrs;
	//This map contains the volume GUIDS and the Corresponding the volumes's Logical names
	map<std::string,std::string> m_mapOfGuidsWithLogicalNames;
	set<std::string> m_setVsnapVols;

	map<string,string> m_mapOfAlreadyPresentMasterVolumes;
	vector<string> m_vectorOfAlreadyMountedVolGuids;
	vector<pair<PartitionInfo,volInfo*> > m_partitionVolumePairVector;
	multimap<unsigned int,volInfo > m_Disk_Vol_ReadFromBinFile;
    map<string,multimap<unsigned int,volInfo>> m_mapOfVmToDiskVolReadFromBinFile;
	string m_strMasterTgtvolName;
	multimap<int ,volInfo> m_mapVolInfo;
	string strDirectoryPathName;
	std::map<std::string,std::string> m_FailedVolumesMap;
	fstream m_fileDescriptor;
    string m_strXmlBuffer ;
	string m_strCommandToExecute;
	map<std::string,std::string> m_mapLockedVolumes;
	//unsigned int uTotalNoOfSourceVolumes;
    bool bIsFailbackEsx;
    map<string,VxJobXml> m_mapOfTgtVolToVxJobObj;
    bool bIsAdditionOfDisk;//Flag for new protection/addition of disk to overwrite/append pInfo file.
	bool m_bVolResize;
	bool m_bVolPairPause;
    string m_strHostID;
	string m_strCxPath;
	string m_strPlanId;
	string m_strOperation;
    SV_UINT m_DiskCountProcessedToMakeOnline;
	bool m_bPhySnapShot;
	list<string> m_listHost;
	map<string, DisksInfoMap_t> m_mapOfHostToDiskInfo;
	map<string, bool> m_MapHostIdMachineType;
	map<string, string> m_diskNameToSignature;
	map<string, string> m_MapHostIdOsType;
	map<string, string> m_mapHostIdToHostName;
	map<string, bool> m_mapHostIdDynAddDisk;
	map<string, map<int, int> > m_mapOfHostToDiskNumMap;
	string m_strMtHostID;
	string m_strMtHostName;
	SrcCloudVMsInfo m_prepTgtVmsInfo;
	VMsCloneInfo m_drDrillVmsInfo;
	CloudVMsDisksVolsMap m_prepTgtVMsUpdate;
	map<string, double> m_mapOfHostIdDlmVersion;
    map<string, statusInfo_t> m_mapHostIdToStatus;

	//Cluster related data-members
	multimap<string, string> m_mapClusterNameToNodes;
	map<string, string> m_mapOfClusNodeOS;
	map<string, set<string> > m_mapClusNameSrcClusDiskSignature;
	//The disk singnature is collecting from xml files. Currently the disk signature is not using anywhere.
	map<string, map<string,string> > m_mapNodeClusDiskSign;
	map<string, map<string,string> > m_mapNodeDiskSignToDiskType;
	map<string, clusterMbrInfo_t> m_mapHostToClusterInfo;
};



#endif