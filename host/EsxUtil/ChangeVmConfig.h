#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include "RollBackVM.h"
#include "inmuuid.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "inmcommand.h"
#include <boost/algorithm/string.hpp>

#ifdef WIN32

	#include "boostsectfix.h"
	#include <windows.h>
	#include <algorithm>
	#include <limits.h>
	#include <initguid.h>
	#include <vds.h>
	#include <assert.h>
	#include <atlconv.h>
	#include <stdio.h>
	#include <atlbase.h>
	#include <netfw.h>

	#include "genericwmi.h"
	#include "volinfocollector.h"
	#include "deviceidinformer.h"
	#include "sharedalignedbuffer.h"

	#include <fdi.h>
	//libraries required to link with VDS
	#pragma comment(lib,"Ole32.Lib")
	#pragma comment( lib, "oleaut32.lib" )
	#define _SafeRelease(x) {if (NULL != x) { x->Release(); x = NULL; } }

	//WMI
	#include <Wbemidl.h>
	#pragma comment(lib, "wbemuuid.lib")

	#include <winsock2.h>
	#include <iphlpapi.h>
	#pragma comment(lib, "IPHLPAPI.lib")

	#include <netcon.h>		//API's used for network enable

	#include <vdslun.h>
	#include <vds.h>
	#include <vdserr.h>

	#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
	#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

	#define WIN2K8_SERVER_VER			0x6
	#define BUFSIZE						MAX_PATH
	#define MAX_KEY_LENGTH				MAX_PATH
	#define MAX_VALUE_NAME				16383

	#define NETWORK_FILE_NAME			"nw.txt"
	#define ONLINE_DISK					"inmage_online_disks.txt"
	#define	CLEAR_RO_DISK				"inmage_clear_ro_disk.txt"
	#define WINDOWS_TEMP_PATH           "\\Windows\\Temp\\"
	#define NETWORK_FILE_PATH           "\\Temp\\nw.txt"
	#define NETWORK_LOGFILE_PATH        "\\nwlog.txt"
	#define IMPORT_FILE_PATH			"\\inmage_import_disks.txt"
	#define STARTUP_EXE_PATH            "\\EsxUtil.exe"
	#define DISKPART_EXE                "diskpart.exe"
    #define RESCAN_DISKS_FILE			"\\inmage_rescan_disks.txt"
	#define RECOVER_FILE_PATH			"\\inmage_recover_disks.txt"
	#define DRIVE_FILE_NAME				"inmage_drive.txt"

	#define VM_SYSTEM_HIVE_NAME         "Inmage_Loaded_System_Hive"
	#define VM_SYSTEM_HIVE_PATH         "\\Windows\\system32\\config\\system"
	#define VM_SOFTWARE_HIVE_NAME       "Inmage_Loaded_Sw_Hive"
	#define VM_SOFTWARE_HIVE_PATH       "\\Windows\\system32\\config\\software"
	#define REGISTRY_EXPORT_CMD         "regedit.exe /E "
	#define RUNNING_VM_SYSTEM_HIVE      "SYSTEM"

	#define IP_ADDRESS_KEY              "IPAddress"
	#define SUBNET_MASK_KEY             "SubnetMask"
	#define DEFAULT_GATEWAY_KEY         "DefaultGateway"
	#define NAME_SERVER_KEY             "NameServer"
	#define ENABLE_DHCP_KEY             "EnableDHCP"

	#define CONTROL_SET_PREFIX          "ControlSet"
	#define REG_VALUE_BACKUP_SANPOLICY  "Backup.SanPolicy"
    #define REG_VALUE_BOOTUPSCRIPT_NUM  "BootupScriptNumber"
    #define REG_KEY_SV_SYSTEMS_X32      "\\SV Systems"
    #define REG_KEY_SV_SYSTEMS_X64      "\\Wow6432Node\\SV Systems"
    #define REG_KEY_SOFTEARE            "Software"

	#define ADAPTER_REG_PATH            "\\Services\\Tcpip\\Parameters\\Adapters"
	#define INTERFACE_REG_PATH          "\\Services\\Tcpip\\Parameters\\Interfaces"
	#define XENVBD_6_0_REG_KEY          "\\DriverDatabase\\DriverPackages\\xenvbd.inf_amd64_7bee277809baa4bd"
	#define XEN_TOOLS_REG_KEY           "\\Citrix\\XenTools"
	#define INM_SAFE_RELEASE(x) {if(NULL != x) { x->Release(); x=NULL; } }
	#define GUID_LEN	50

    #define MAX_MBR_LENGTH 17408
    #define OSDISK_MBR_FILE_PATH            "mbr.dat"

    #define WINDOWS_AZURE_GUEST_AGENT   "WindowsAzureGuestAgent"
    #define WINDOWS_AZURE_TELEMETRY_SVC "WindowsAzureTelemetryService"
    #define WINDOWS_AZURE_RDAGENT_SVC       "Rdagent"

	inline std::string GUIDString(const GUID & guid){
		USES_CONVERSION;
		WCHAR szGUID[GUID_LEN] = {0};
		if(!StringFromGUID2(guid,szGUID,GUID_LEN))
			szGUID[0] = L'\0'; //failure
		return W2A(szGUID);
	}

#else
	#include <arpa/inet.h>
	#include <sys/mount.h>
	#define AGENT_SCRIPT_PATH                      "/scripts"
	#define CLOUD_SCRIPT_PATH                      "/Cloud"
	#define AWS_SCRIPT_PATH                        "/scripts/Cloud/AWS/"
	#define AZURE_SCRIPT_PATH                      "/scripts/azure/"
	#define LINUXP2V_PREPARETARGET_AZURE           "post-rec-script.sh"
	#define LINUXP2V_PREPARETARGET_AWS             "aws_recover.sh"
	#define LINUXP2V_PREPARETARGET_INFORMATION     "PrepareTarget.sh"
	#define VMWARE_CONFIGURE_NETWORK_SCRIPT			"vmware-configure-network.sh"

	const char ROOT_MOUNT_POINT[] = "/";
	const char ETC_MOUNT_POINT[] = "/etc";
	const char BOOT_MOUNT_POINT[] = "/boot";
	const char USR_MOUNT_POINT[] = "/usr";
	const char VAR_MOUNT_POINT[] = "/var";
	const char USR_LOCAL_MOUNT_POINT[] = "/usr/local";
	const char OPT_MOUNT_POINT[] = "/opt";
	const char HOME_MOUNT_POINT[] = "/home";
#endif /* WIN32 */

#define CASPIAN_V2_GA_PROD_VERSION	"9.0.0.0"


#define TRACE_ERROR(format,...)      DebugPrintf(SV_LOG_ERROR,format,__VA_ARGS__)

#define TRACE_WARNING(format,...)    DebugPrintf(SV_LOG_WARNING,format,__VA_ARGS__)

#define TRACE_INFO(format,...)       DebugPrintf(SV_LOG_INFO,format,__VA_ARGS__)

#define TRACE(format,...)            DebugPrintf(SV_LOG_DEBUG,format,__VA_ARGS__)

#define TRACE_FUNC_BEGIN      DebugPrintf(SV_LOG_DEBUG,"Entered into %s\n", __FUNCTION__)

#define TRACE_FUNC_END        DebugPrintf(SV_LOG_DEBUG,"Exited from %s\n", __FUNCTION__)


struct NetworkInfo
{
    SV_ULONG EnableDHCP;
	std::string OldIPAddress;
    std::string IPAddress;
    std::string SubnetMask;
    std::string DefaultGateway;
    std::string NameServer;
	std::string MacAddress;
	std::string VirtualIps;
    
    NetworkInfo()
    {
        EnableDHCP = 1; // Since default below values are empty, keep dhcp = 1
		OldIPAddress.clear();
		IPAddress.clear();
		SubnetMask.clear();
		DefaultGateway.clear();
		NameServer.clear();
		MacAddress.clear();
		VirtualIps.clear();
    }
};

struct IpInfo
{
	std::string IPAddress;
    std::string SubnetMask;
	std::string DefaultGateway;
	std::string DnsServer;
};

typedef std::string InterfaceName_t;
typedef std::vector<InterfaceName_t> InterfacesVec_t;
typedef std::vector<InterfaceName_t>::iterator InterfacesVecIter_t;
typedef std::map<InterfaceName_t, NetworkInfo> NetworkInfoMap_t;
typedef std::map<InterfaceName_t, NetworkInfo>::iterator NetworkInfoMapIter_t;

struct CabExtractInfo 
{
	std::string m_path;
	std::string m_name;
	std::string m_destination;
	std::set<string> m_extractFiles;
};


#ifdef WIN32

typedef struct diskinfo_tag
{
	//public:
	diskinfo_tag() : capacity(0) {};

	std::string displayName;
	std::string scsiId;
	unsigned long long capacity;

} Diskinfo_t;

typedef struct ProfileMapElement
{
	NET_FW_PROFILE_TYPE2 Id;
	LPCWSTR Name;
}FwProfileElement_t;


class WmiDiskRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
	WmiDiskRecordProcessor(std::vector<Diskinfo_t> *pdiskinfos);
	bool Process(IWbemClassObject *precordobj);
	std::string GetErrMsg(void)
	{
		return m_ErrMsg;
	}

private:

	std::vector<Diskinfo_t> * m_pdiskinfos;
	std::string m_ErrMsg;
	DeviceIDInformer m_DeviceIDInformer;
};

#endif

class ChangeVmConfig
{
public:
    ChangeVmConfig()
    {
        LinuxReplication obj;
		m_VxInstallPath = obj.getVxInstallPath();
        m_NumberOfDisksMadeOnline = 0;
		m_vConVersion = 1.2;
		m_bInit = false;
		m_bV2P = false;
		m_bReboot = false;
		m_bCloudMT = false;
		m_strCloudEnv = "";
		m_bTestFailover = false;
		m_bEnableRdpFireWall = false;
#ifdef WIN32
		m_sysHiveControlSets.clear();
#else
		m_lvs.clear();
#endif
    }
	~ChangeVmConfig()
	{
#ifdef WIN32
		UnInitCOM();
#endif
	}
	std::string m_VxInstallPath;
    SV_UINT m_NumberOfDisksMadeOnline;
	bool m_bDrDrill;
	bool m_bV2P;
	bool m_bReboot;
	bool m_bCloudMT;
	int m_nMaxCdpJobs;
	double m_vConVersion;
	std::string m_strDatafolderPath;
	std::string m_InMageCmdLogFile;
	std::string m_strCloudEnv;
	std::map<std::string, std::string> m_mapOfVmNameToId;
	std::map<std::string, std::string> m_mapOfVmToClusOpn;
	std::map<std::string, std::string> m_mapOfVmInfoForPostRollback;
	VMsRecoveryInfo m_RecVmsInfo;
	std::string m_newHostId;
	bool m_bTestFailover;
	bool m_bEnableRdpFireWall;
    
	bool MakeChangesPostRollbackVmWare(const std::string & HostName);
	bool MakeChangesPostRollbackCloud(std::string HostName);
	bool MakeChangesPostRollback(std::string HostName, bool bP2VChanges, std::string& strOsType);
	bool MakeInMageSVCManual(std::string strHostName);
	bool checkIfFileExists(std::string strFileName);
	xmlNodePtr xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry);
	xmlNodePtr xGetNextChildNode(xmlNodePtr cur, const xmlChar* xmlEntry);
    /*xmlNodePtr xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue);
	bool xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue);*/
	bool xGetvConVersion();
	bool UpdateArraySnapshotFlag();
	bool RecoverDynDisks(list<string>& listDiskNum);
	bool ImportForeignDisks();
	bool OnlineAllOfflineDisks();
	bool xGetMachineAndOstypeAndV2P(std::string HostName, std::string & OsType, std::string & MachineType, std::string& Failback);//made this as common function.
	bool ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes);
	std::map<std::string,std::string> getProtectedOrSnapshotDrivesMap(std::string strVmName, bool bTakePinfo = false);
	bool xGetListOfSrvToSetManual(string& HostName, vector<string>& lstSrvManual);
	
#ifdef WIN32
    HRESULT InitCOM();
	bool InstallVmWareTools();    
	bool ResetReplicationState();
	bool ConfigureSettingsAtBooting(std::string NwInfoFile, bool bP2v, std::string strCloudEnv, bool bResetGUID );
    bool ConfigureNWSettings(std::string NwInfoFile);
	bool printBootSectorInfo(std::string&);
	bool ExecuteProcess(const std::string &FullPathToExe, const std::string &Parameters);
	HRESULT restartService(const string& serviceName);
	bool StopService(const string serviceName);
	BOOL rescanIoBuses();
	void Reboot();
	BOOL EnumerateAllVolumes(std::set<std::string>& setVolume);
	BOOL MountVolume(const string& strVolGuid, string& strMountPoint);
	void RemoveAndDeleteMountPoints(const string & strEfiMountPoint);	
	bool GetSizeOfVolume(const string& strVolGuid, SV_ULONGLONG& volSize);
	bool GetAllVolumesMapOfAllType(map<string, string>& mapVolGuidToNameFromSystem);
	bool ExtractW2k3Driver(string& strSysVol, string& strOsType);
	bool CopyFileUsingCmd(string& strSrcPath, string& strTgtPath);
	bool FlushFileUsingWindowsAPI(string& strFile);
	int GetDiskAndMbrDetails(const string& strMbrFile);
	int GetDiskAndPartitionDetails(const string& strPartitionFile);
	bool UpdateHostID( const string& );
	bool ResetGUID();
	bool RestartServices( std::vector<std::string>& );
	bool UpdateServicesStartupType(std::vector<std::string>&, DWORD dStartType);
	bool DownloadDataFromDisk(string strDiskName, string strNumOfBytes, bool bEndOfDisk, string strBytesToSkip, string& strFileName);
	bool UpdateDataOnDisk(string strDiskName, string strNumOfBytes, bool bEndOfDisk, string strBytesToSkip, const string& strBinFile);
	bool UpdateDiskLayoutInfo(const string& strDiskMapFile, const string& strDlmFile);
	bool PrepareTargetDiskForProtection(const string& strDiskMapFile, const string& strDlmFile);
	bool ConfigureTargetDisks(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDiskInfo, map<string, string>& mapDiskNameToSignature);
	bool PrepareDisksForConfiguration(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo);
	bool PrepareDynamicGptDisks(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo);
	bool CollectGPTData(string strDiskNmbr);
	bool UpdateGPTDataOnDisk(string strDiskNmbr);
	bool UpdateSignatureOfDisk(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo, map<string, string>& mapDiskNameToSignature);
	bool RegisterHostUsingCdpCli();
	bool UpdateDiskLayout(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo);

	//Added for RDP enable
	HRESULT    WFCOMInitialize(INetFwPolicy2** ppNetFwPolicy2);
	void       WFCOMCleanup(INetFwPolicy2* pNetFwPolicy2);
	HRESULT    GetFirewallStatus(__in INetFwPolicy2 *pNetFwPolicy2, FwProfileElement_t ProfileElements[], int size, map<string, bool>& fwStatus);
	HRESULT    EnableRuleGroup(__in INetFwPolicy2 *pNetFwPolicy2, FwProfileElement_t ProfileElements[], int size, wstring wstrGroupName);
	bool       EnabledRdpPublicFirewallRule();
	
	// Method converts the windows error code to message format
	std::string ConvertGetLastErrorToMessage(DWORD error);

#else
	std::set<string> m_lvs;
	std::map<std::string,std::map<std::string, std::string> > m_mapOfVmToSrcAndTgtDisks;
	bool createDirectory(string mnt = "/mnt/inmage_mnt");
	bool deleteDirectory(string dir = "/mnt/inmage_mnt");
	bool GetFileNames( std::string , std::string , std::list<std::string> &listFile);
	bool RenameFile( std::string , std::string );
	bool FindAllPartitions(string strDevMapper, set<string>& setPartitions, bool bDevMapper=false);
	bool PostRollbackCloudVirtualConfig(std::string& strHostName);
#endif

private:
	bool m_bInit;
	bool m_p2v;
	bool m_bWinNT;
	bool xGetNWInfoFromXML(NetworkInfoMap_t& NwMap, std::string HostName);
#ifdef WIN32
	bool xGetSrcSystemVolume(std::string HostName, std::string & SrcSystemVolume);
	bool changeWindowsVmIpAddress();
	bool SetPrivileges(LPCSTR privilege, bool set);
    bool ChangeRegistry(std::string SystemHiveName, std::string strRegName, bool bRegDelete, string strPhyOsType);
	bool ChangeDWord(HKEY hKey,std::string strName,DWORD dwValue);
	bool ChangeREG_SZ(HKEY hKey,std::string strName,std::string strTempRegValue);
	bool ChangeREG_EXPAND_SZ(HKEY hKey,std::string strName,std::string strTempRegValue); 
	bool ChangeREG_MULTI_SZ(HKEY hKey,std::string strName,std::string strTempRegValue);
    //bool AddBatScriptToRunOnce(std::string strSrcSystemVolume, std::string strMntPtNameOfSrcSystemVolume);
    //bool CopyBatchFile(std::string strBatchFilePath);
    bool GetControlSets(std::vector<std::string> & vecControlSets);
	DWORD GetControlSets(std::string strSystemHive);
	bool ExportRegistryFile(string regPath, const string regFile);
    bool ReadNWInfoMapFromFile(NetworkInfoMap_t & NwMap, std::string FileName, int& nNumNics);
    bool SetNwUsingNetsh(NetworkInfoMap_t NwMap,int nNumNics);
    bool IsItW2k8Machine();
    bool BringAllDisksOnline();
	bool OnlineDisk( const std::string & );
	bool ClearReadonlyFlagOnDisk( const std::string &);
    void EnableDebugPriv( void );
    void CleanUpDiskProps(VDS_DISK_PROP &vdsDiskProperties);
    HRESULT ProcessVdsPacks(IN IEnumVdsObject **pEnumIVdsPack);
    HRESULT BringDisksOnline();
    HRESULT BringUnInitializedDisksOnline();
    bool GetSrcNTgtSystemVolumes(std::string HostName, std::string & SrcSystemVolume, std::string & TgtMntPtForSrcSystemVolume);
    bool MakeChangesForP2V(std::string HostName, std::string PhysicalMachineOsType, std::string SystemHiveName, std::string SrcSystemVolume, std::string TgtMntPtForSrcSystemVolume, std::string SoftwareHiveName);
	bool SetVMwareToolsStartType( std::string SystemHiveName , DWORD );
    bool MakeChangesForNW(std::string HostName, std::string OsType, std::string SystemHiveName, std::string SoftwareHiveName, std::string SrcSystemVolume, std::string TgtMntPtForSrcSystemVolume);
	bool RemoveRegistryPathFromSystemHiveControlSets(std::string SystemHiveName, std::string RegPathToDelete);
	bool ResetInvolfltParameters( std::string , std::string, std::string );
	bool MakeChangesForInmSvc(std::string SystemHiveName, std::string SoftwareHiveName, bool bDoNotSkipInmServices=true);
	bool MakeRegChangesForW2012AWS(std::string SystemHiveName);
	bool MakeChangesForBootDrivers(string TgtMntPtForSrcSystemVolume, string SystemHiveName, string SoftwareHiveName);
    bool PreRegistryChanges(std::string TgtMntPtForSrcSystemVolume,
                            std::string SystemHiveName, std::string SoftwareHiveName,
                            bool & bSystemHiveLoaded, bool & bSoftwareHiveLoaded);
    bool PostRegistryChanges(std::string TgtMntPtForSrcSystemVolume,
                             std::string SystemHiveName, std::string SoftwareHiveName,
                             bool bSystemHiveLoaded, bool bSoftwareHiveLoaded);
    bool GetSrcSystemVolume(std::string HostName, std::string & SrcSystemVolume);
	bool GetMapOfSrcToTgtVolumes(std::string HostName, std::map<std::string,std::string> & mapOfSrcToTgtVolumes);
    bool RegGetNWInfo(std::string SystemHiveName, std::string SoftwareHiveName,
                      std::string SystemDrivePathOnVm, std::string SystemDrivePathOnMT, 
                      NetworkInfoMap_t & NetworkInfoMap);
    bool GetCurrentControlSetValue(std::string SystemHiveName, DWORD & dwCCS);
    bool RegGetInterfaces(std::string SystemHiveName, string CCS, InterfacesVec_t & Interfaces);
    bool RegGetNwMap(std::string SystemHiveName, std::string CCS, InterfacesVec_t Interfaces, NetworkInfoMap_t & NetworkInfoMap);
    bool RegAddStartupExeOnBoot(std::string SoftwareHiveName, std::string SystemDrivePathOnVm, bool bW2K8, bool bW2K8R2);
    bool RegAddRegEntriesForStartupExe(std::string SystemDrivePathOnVm, std::string RegPathToOpen, std::string AgentInstallDir, bool bW2K8R2);
	bool RegDelStartupExePathEntries(bool bW2K3, DWORD dwStartupScriptOrder);
	bool RegDelRegEntriesForStartupExe(std::string RegPath, DWORD scriptOrderNumber);
	LONG RegGetSVSystemDWORDValue(const std::string& valueName,
		DWORD& valueData, const std::string& hiveRoot, const std::string& relativeKeyPath = "");
    bool RegDelGivenKey(std::string RegPath);
	bool RegDelGivenValue(string RegPath, string strRegValue);
    bool WriteNWInfoMapToFile(NetworkInfoMap_t NwMap, std::string FileName, std::string NwFileName, std::string ClusterNwFile);
    bool RegGetInstallDir(std::string SoftwareHiveName, std::string & InstallDir);
    bool RegGetSubFolders(std::string RegPathToOpen, std::vector<std::string> & SubFolders);
    bool RegGetValueOfTypeDword(HKEY hKey, std::string KeyToFetch, DWORD & Value);
    bool RegGetValueOfTypeSzAndMultiSz(HKEY hKey, std::string KeyToFetch, std::string & Value);
    void FormatCCS(DWORD dwCCS, std::string & CCS);
    void UpdateNwInfoMapWithNewValues(NetworkInfoMap_t NwMapFromReg, NetworkInfoMap_t NwMapFromXml, NetworkInfoMap_t & NwMapFinal);
    bool SetValueREG_DWORD(HKEY hKey, std::string Key, DWORD Value);
    bool SetValueREG_SZ(HKEY hKey, std::string Key, std::string Value);
	bool UpdateNetworkInfo(string strNetworkName, NetworkInfo nwInfo, string strResultFile);
	bool EnableConnection(const std::string& strNwAdpName, bool bEnable, const std::string& strResultFile);
	HRESULT WMINwInterfaceQuery(map<InterfaceName_t, unsigned int>& mapOfInterfaceStatus);
	HRESULT WMINwInterfaceQueryWithMAC( NetworkInfoMap_t , map<InterfaceName_t, unsigned int>& mapOfInterfaceStatus);
	HRESULT WMINicQueryForMac(string strMacAddress, string& strNetworkName);
    HRESULT GetWbemService(IWbemServices **pWbemService);
    bool GetInterfaceFriendlyNames(InterfacesVec_t & InterfaceNames);
    bool GetInterfaceFriendlyNamesUsingApi(InterfacesVec_t & InterfaceNames);
    bool GetSystemVolume(string & strSysVol);
	bool CdRomVolumeEnumeration(string& strCdRomDrive);
	BOOL FetchAllCdRomVolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, string& strCdRomDrive);
	bool DeleteRegistryTree( std::string );
	bool DoesRegistryPathExists( std::string );
	bool CheckServiceExist(const std::string& svcName);
	bool CreateMapOfIPandSubNet(string strIpAddress, string strSubNet, map<string, string>& mapOfIpAndSubnet);
	bool ExecuteNetshCmd(string strNetshCmd, string strResultFile);
	HRESULT WMIDiskPartitionQuery(map<string, string>& mapOfDiskNumberToType);
	bool RestartVdsService();
	BOOL FetchAllVolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, std::set<std::string>& stVolume);
	BOOL IsVolumeNotMounted(const string& strVolGuid, string & strMountPoint);
	bool WriteDriveInfoMapToFile(map<string, string> mapOfVolGuidToName, std::string localDriveFile, std::string recDriveFile);
	bool MakeChangesForDrive(std::string HostName, std::string SystemHiveName, std::string SoftwareHiveName, std::string SrcSystemVolume, std::string TgtMntPtForSrcSystemVolume);
	bool GetMapofVolGuidToVolNameFromMbrFile(std::string HostName, map<string, string>& mapOfSrcTgtVol, map<string, string>& mapOfVolGuidAndName);
	bool GetMapofVolGuidToVolName(std::string SystemHiveName, map<string, string> mapChecksumAndVolGUID, 
							  map<string, string> mapOfSrcTgtVol, map<string, string>& mapOfVolGuidAndName);
	bool GetMapVolumeGuidAndChecksum(std::string SystemHiveName, map<string, string>& mapChecksumAndVolGuid);
	bool AssignDriveLetters(string strDriveFile);
	bool GetMapOfVolGuidToNameFromDriveFile(map<string, string>& mapVolGuidToNameFromFile, string strDriveFile);
	BOOL FetchAllTypeVolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, map<string, string>& mapVolGuidToNameFromSystem);
	bool CompareAndAssignVolGuidToPath(map<string, string>& mapVolGuidToNameFromFile, map<string, string>& mapVolGuidToNameFromSystem);
	bool DiffPhysicalAndVirtualNw(NetworkInfo & NwInfo, NetworkInfo & PhysicalNw, NetworkInfo & VirtualNw);
	bool GetDriverFile(std::string& SystemVol, std::string& SoftwareHiveName, bool& bx64, std::string& strExtractPath);
	bool GetW2k3SpVesrion(std::string& SoftwareHiveName, int& w2k3SpVersion);
	bool ExtractDriverFile(std::string& strSystemVol, std::string strCabFile, bool& bx64, std::string strDriverFile, std::string& strExtractPath);
	bool GetCabFilePath(std::string& strSystemVol, std::string& strCabFile, bool& bx64, std::string& strCabFilePath);
	bool ExtractCabFile(CabExtractInfo* cabInfo);
	bool ChangeServiceStartType(string& SystemHiveName, const string strSrvName, DWORD dSrvType, string& strCtrlSet);
	bool ChangeDriverServicesStartType(string& SystemHiveName, const string strSrvName, DWORD dSrvType, string& strCtrlSet);
	bool SetServicesToManual(string& HostName, string& SystemHiveName, string& SoftwareHiveName);
	bool SetServicesStartType(string& SystemHiveName, string& SoftwareHiveName, map<string, DWORD> SrvWithNewStartType);
	bool RegSetKeyAtInstallDir(string& SoftwareHiveName, const string strKeyName, string& strKeyValue);
	bool RegGetKeyFromInstallDir(string& SoftwareHiveName, const string strKeyName, string& strKeyValue);
	bool RevertSrvStatusToOriginal(string& HostName);
	bool SetServicesToOriginalType(string& HostName, string& SystemHiveName, string& SoftwareHiveName);
	bool GetDiskMappingFromFile(const string& strDiskMapFile, map<string, string>& mapSrcAndTgtDiskNum);
	bool PrepareVolumesMapping(DisksInfoMap_t& srcDisksInfo, map<string, string>& mapSrcVolToTgtVol);
	void RemoveTrailingCharactersFromVolume(std::string& volumeName);
	bool CheckDriverFilesExistance(list<string>& listDriverFiles);	
	bool RestoreSanPolicy(const std::string& SystemHiveName, const std::string& SoftwareHiveName);
	void RestoreSanPolicy();

	void UnInitCOM()
	{
		if(m_bInit)
			CoUninitialize();
	}
	
	// A2E support
	// Member variable to store the names of discovered system hive control sets.
	std::vector<std::string> m_sysHiveControlSets;
	std::vector<Diskinfo_t> m_disksdetails;

	disk_extent m_srcOsVolDiskExtent;
	std::string m_sourceVMName;
	std::string m_OsType;
	std::string m_srcOSVolGuid;
	std::string m_srcOSVolumeName;
	std::string m_osVolMountPath;
	std::string m_osDiskMBRFilePath;
	Diskinfo_t m_srcBootDisk;

	bool m_bOsDiskOnlined;
	bool m_bOsVolMounted;
	bool m_bSystemHiveLoaded;
	bool m_bSoftwareHiveLoaded;

	SharedAlignedBuffer m_osMBR;

	void InitFailbackProperties(const std::string & srcVmName);
	void ParseBootDiskDetails();
	void ParseOsTypeInformation();
	void CollectAllDiskInformation();

	void FindSrcBootDisk();
	bool FindMatchingDisk(const std::string &scsiId, Diskinfo_t & diskinfo);

	void FetchOsDiskMBR();
	void ResetOsDiskMBR();

	void FetchPreviouslySavedMBR();
	void PersistOsDiskMBR();
	

	void OnlineRecoveredVMBootDisk();
	bool OnlineDisk(const Diskinfo_t & diskinfo);
	
	void OfflineRecoveredVMBootDisk();
	bool OfflineDisk(const Diskinfo_t & diskinfo);

	void OfflineOnlineRecoveredVMBootDisk();

	void PrepareSourceSystemPath();
	void PrepareSourceOSDriveWithRetries();
	void PrepareSourceOSDrive();

	DWORD GetSourceOSVolumeGuid(const disk_extent& osVolExtent,
		std::string& volumeGuid);

	DWORD GetVolumeDiskExtents(const std::string& volumeGuid, disk_extents_t& diskExtents);
	void GetDiskId(DWORD dwDiskNum, std::string& diskID);

	DWORD GetVolumeMountPoints(const std::string& volumeGuid,
		std::list<std::string>& mountPoints);
	
	DWORD AutoSetMountPoint(const std::string& volumeGuid, std::string& mountPoint);
	DWORD GenerateUniqueMntDirectory(std::string& mntDir);
	bool GenerateUuid(std::string& uuid);

	void PrepareSourceRegistryHivesWithRetries();
	void PrepareSourceRegistryHives();
	void SetServicesToOriginalType();

	void MakeChangesForInMageServices(bool bDoNotSkipInmServices);

	void ResetInvolfltParameters();
	void SetBootupScript();
	void SetBootupScriptOptions();
	bool IsRecoveredVmV2GA();
	bool IsBackwardCompatibleVcon()
	{
		return m_strCloudEnv.empty();
	}

	bool PerformCleanUpActions();
	void ReleaseSourceRegistryHives();

	void HideUnhideSourceVolFS();
	void LogDirtyBitValueForVolume(const string & volumeGuid);
	bool FlushFilesystemBuffers(const string & volumeGuid);


#else 
	bool moveFileToDir(string file, string dir);
	bool copyFileToTarget(string srcFile, string destFile);
	bool mountFstab(string strRootMnt, string strTgtPartition);
	bool mountPartition(string strRootMnt, string strTgtPartition, string mnt);
	bool unmountPartition(string strTgtPartition);
	void tokeniseString(std::string strAllDisks, std::list<std::string>& listDisks, std::string strdel=",");
	bool getNwInfoFromXmlFile(std::string HostName);
	bool parseStorageInfo(std::string xmlFile);
	bool parseNetworkInfo(std::string xmlFile);
	bool processPartition(std::string hostName, std::string strPartition, std::string& strTargetDisk);
	bool setNetworkConfig(string hostName, string strMntDirectory);
	bool setNetworkConfigRedHat(string hostName, string strMntDirectory);
	bool setNetworkConfigSuse(string hostName, string strMntDirectory);
	bool listFilesInDir(std::string strDirectory, std::string strFile);
	bool GetMacAddressFile(string &strMacAddFile);
	bool UpdateGateWay(string strNwDirectory, string strNwDir);
	bool UpdateMacAddress(string strMacAddFile, string strNwDir);
	bool checkFSofDevice(string strTgtPartitionOrLV);
	void MakeInMageSVCStop(string strMntDirectory);
	bool UpdateDnsServerIP(string hostName, string strDnsFilePath, set<string> setDnsServer, string strNwDir);
	bool UpdateNsSwitchFile(string strNwSwitchFilePath, string nwDir);
	bool UpdateHostsFile(string strHostFilePath, string strNwDir, string strHostName, string strHostIp);
	bool getBroadCastandNWAddr(string strIpAddress, string strNetMask, string &strBroadCastAdd, string &strNetworkAdd);
	bool getHostInfoUsingCXAPI(const string strHostUuid , std::string strMountPoint = std::string("/") , bool fetchNetworkInfo = true , bool bGetDisks = false);
	bool parseCxApiXmlFile(string strCxXmlFile, const string strHostUuid , std::string , bool , bool);
	bool xGetStorageInfo(xmlNodePtr xCur , std::string , bool);
	bool xGetNetworkInfo(xmlNodePtr xCur);
	bool activateLV(string& strLvName);
	bool deActivateLV(string& strLvName);
	bool activateVolGrp(string strVgName);
	bool deActivateVolGrp(string strVgName);
	bool createMapOfOldIPToIPInfo(string OldIPAddress, string strIpAddress, string strSubNet, 
									string strGateway, string strDnsServer, map<string, IpInfo>& mapOfOldIpToIpAndSubnet);
	bool PrepareTargetVirtualConfiguration( std::string );
	bool ModifyConfiguration( std::string , std::string );
	bool InstallGRUB( std::string , std::string );
	bool RemoveInVolFltFiles(string strMntDirectory);
	bool NetrworkConfigForAWS(string strMntDirectory);
	bool FsTabConfigForAWS(string strMntDirectory);
	bool CreateProtectedDisksFile(string strHostId);
	bool ModifyConfigForAzure(string strHostId, string strModifyFor, string strMountPath);
	bool ModifyConfigForVMWare(string strHostId, string strModifyFor, string strMountPath);
	bool ModifyNetworkConfigForVMWare(const string& rootMntPath);
	bool AssignFilePermission(string strFirstFile, string& strSecondFile);
	bool ListAllPIDs(set<string>& stPids);
	bool ListPIDsHoldsMntPath(const string& strPath, set<string>& stPids);
	bool MountSourceLinuxPartition(string strRootMnt, string strHostName, string strPartitionName, string strMountDir, map<string, StorageInfo_t>& mapMountedPartitonInfo);
	bool GetDiskNameForDiskGuid(const std::string& strDiskGuid, std::string& strDiskName);
	
	std::string m_strOsFlavor;
	std::string m_strOsType;
	std::list<NetworkInfo_t> m_NetworkInfo;
	StorageInfo_t m_storageInfo;
	std::map<string, string> m_mapOfOldAndNewMacAdd;
	std::string m_strGateWay;
	std::string m_strHostName;

    // 
    // DiskGuid and DiskName as in XML doc that EsxUtil receives
    // For V2A failback, before the persistent name changes
    // DiskGuild and DiskName are same, like /dev/sdc
    // For V2A failback, after the persistent name changes
    // DiskGuid will be like DataDisk0 and DiskName is /dev/sda 
    //
	std::map<std::string, std::string> m_mapOfDiskGuidToDiskName;

#endif /* WIN32 */
};


#ifdef WIN32

class CVdsHelper
{
	IVdsService* m_pIVdsService;

	enum {
		INM_OBJ_UNKNOWN,
		INM_OBJ_MISSING_DISK,
		INM_OBJ_FAILED_VOL,
		INM_OBJ_DISK_SCSI_ID,
		INM_OBJ_FOREIGN_DISK,
		INM_OBJ_DYNAMIC_DISK,
		INM_OBJ_HIDDEN_VOL,
		INM_OBJ_DISK_SIGNATURE,
		INM_OBJ_OFFLINE_DISK
	} m_objType;

	map<string, string> m_helperMap;
	list<string> m_listDisk;
	bool m_bAvailable;

	HRESULT ProcessProviders();
	HRESULT ProcessPacks(IVdsSwProvider *pSwProvider);
	HRESULT RemoveMissingDisks(IVdsPack *pPack);
	HRESULT DeleteFailedVolumes(IVdsPack *pPack);
	HRESULT DeleteVolume(const VDS_OBJECT_ID & objId);
	HRESULT ProcessPlexes(IVdsVolume *pIVolume, std::list<VDS_DISK_EXTENT> & lstExtents);
	bool IsDiskMissing(const VDS_OBJECT_ID & diskId);
	bool AllDisksMissig(std::list<VDS_DISK_EXTENT> & lstExtents);
	void DeleteFailedVolumes(std::list<VDS_OBJECT_ID> & lstFailedVols);
	void CollectDiskScsiIDs(IVdsPack *pPack);
	void CollectDiskSignatures(IVdsPack *pPack);
	HRESULT CollectForeignDisks(IVdsPack *pPack);
	HRESULT ClearFlagsOfVolume(IVdsPack *pPack);
	HRESULT CheckUnKnownDisks(IVdsPack *pPack);
	HRESULT CollectDynamicDisks(IVdsPack *pPack);
	HRESULT CollectOfflineDisks(IVdsPack *pPack);

public:
	CVdsHelper(void);
	~CVdsHelper(void);

	bool InitilizeVDS()
	{
		bool bInit = false;
		for(int i=0; i<3; i++)
		{
			try
			{
				if(!InitVDS())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS\n");
					unInitialize();
					Sleep(2000);
					continue;
				}
				else
				{
					bInit = true;
					break;
				}
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS\n");
				unInitialize();
				continue;
			}
		}
		if(bInit)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully initialized the VDS\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize the VDS in all three tries.\n");
		}
		return bInit;
	}

	void unInitialize()
	{
		DebugPrintf(SV_LOG_INFO,"Uninitializing VDS instance\n");
		INM_SAFE_RELEASE(m_pIVdsService);
		CoUninitialize();
	}

	bool InitVDS();

	void GetDisksScsiIDs(std::map<std::string,std::string> & disksScsiIds)
	{
		string strDisk = "\\\\?\\PhysicalDrive";
		m_helperMap.clear();
		m_objType = INM_OBJ_DISK_SCSI_ID;
		ProcessProviders();

		disksScsiIds.clear();
		map<string, string>::iterator iter = m_helperMap.begin();
		for(; iter != m_helperMap.end(); iter++)
		{
			string strDiskNum = iter->first.substr(strDisk.size());
			disksScsiIds.insert(make_pair(iter->second, strDiskNum));
		}
	}

	void GetDisksSignature( std::map<std::string, std::string> & diskNameToSignature )
	{
		string strDisk = "\\\\?\\PhysicalDrive";
		m_helperMap.clear();
		m_objType = INM_OBJ_DISK_SIGNATURE;
		ProcessProviders();

		diskNameToSignature.clear();
		map<string, string>::iterator iter = m_helperMap.begin();
		for(; iter != m_helperMap.end(); iter++)
		{
			string strDiskNum = iter->first.substr(strDisk.size());
			diskNameToSignature.insert(make_pair(iter->second, strDiskNum));
		}
	}

	void RemoveMissingDisks()
	{
		//1-> delete failed volumes
		m_objType = INM_OBJ_FAILED_VOL;
		ProcessProviders();

		//2-> remove missing disks
		m_objType = INM_OBJ_MISSING_DISK;
		ProcessProviders();
	}

	void CollectForeignDisks(std::list<string>& listDiskNumb)
	{
		m_listDisk.clear();
		m_objType = INM_OBJ_FOREIGN_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			list<string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				string phyDrive = "\\\\?\\PhysicalDrive";
				string strDiskNum = iter->substr(phyDrive.size());
				listDiskNumb.push_back(strDiskNum);
			}
		}
	}

	void CollectDynamicDisks(std::list<string>& listDiskNumb)
	{
		m_listDisk.clear();
		m_objType = INM_OBJ_DYNAMIC_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			list<string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				string phyDrive = "\\\\?\\PhysicalDrive";
				string strDiskNum = iter->substr(phyDrive.size());
				listDiskNumb.push_back(strDiskNum);
			}
		}
	}

	void CollectOfflineDisks(std::list<string>& listDiskNumb)
	{
		m_listDisk.clear();
		m_objType = INM_OBJ_OFFLINE_DISK;
		ProcessProviders();

		if(!m_listDisk.empty())
		{
			list<string>::iterator iter = m_listDisk.begin();
			for(; iter != m_listDisk.end(); iter++)
			{
				string phyDrive = "\\\\?\\PhysicalDrive";
				string strDiskNum = iter->substr(phyDrive.size());
				listDiskNumb.push_back(strDiskNum);
			}
		}
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
};

#endif /*WIN32*/
