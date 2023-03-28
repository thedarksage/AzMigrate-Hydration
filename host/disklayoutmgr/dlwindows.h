#pragma once
#ifndef DLWINDOWS__H
#define DLWINDOWS__H

#define VOLUME_DUMMYNAME_PREFIX         "RAWVOLUME"

#ifdef WIN32

#include "common.h"
#include "dlcommon.h"
#include "dlmdiskinfo.h"

#include <windows.h>
#include <Winioctl.h>
#include <atlconv.h>
#include <Wbemidl.h>
#include <clusApi.h>

#pragma comment(lib, "wbemuuid.lib")

#include "VVDevControl.h"
#include "devicefilter.h"

#define VSNAP_UNIQUE_ID_PREFIX			"VS_"
#define WIN_DISKNAME_PREFIX             "\\\\.\\PHYSICALDRIVE"

// The functions in public exists in both windows and linux (and implementation is platform specific).
class DisksInfoCollector : public DLCommon
{
public:   
    DLM_ERROR_CODE GetDiskInfoMapFromSystem(DisksInfoMap_t & DisksInfoMapFromSys,
								DlmPartitionInfoSubMap_t & PartitionsInfoMapFromSys,
                                std::list<std::string>& ErraticVolumeGuids,
								bool& bPartitionExist,
								const std::string & BinaryFile = "",
								const std::string & DlmFileFlag = DLM_FILE_NORMAL);   
	BOOL GetVolumesPresentInSystem(std::map<std::string,std::string> & mapOfGuidsWithLogicalNames,
                            std::list<std::string>& erraticVolumeGuids);
    bool RefreshDisk(const char * DiskName);
    DLM_ERROR_CODE RestoreMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
                                      std::list<std::string>& erraticVolumeGuids,
                                      int mountNameType);
	DLM_ERROR_CODE RestoreMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
									  DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
									  std::map<std::string, std::string> mapOfSrcVolToTgtVol);
	bool GetDlmInfoFromReg(const std::string& strRegKey, std::string& strRegValue);
	bool SetDlmRegPrivileges(LPCSTR privilege, bool set);	
	DLM_ERROR_CODE WriteDlmInfoIntoReg(const std::string& strRegValue, const std::string& strRegKey);
	bool WritePartitionFileNameInReg(const std::string& strBinaryFileName);
	bool WriteDlmMbrInfoInReg(const std::string& strBinaryFileName, const std::string& Checksum);	
	DLM_ERROR_CODE OnlineOrOfflineDisk(const std::string& DiskName, const bool& bOnline);
	DLM_ERROR_CODE ConvertEfiToNormalPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp);
	DLM_ERROR_CODE ConvertNormalToEfiPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp);
	DLM_ERROR_CODE DeleteDlmRegitryEntries();
    DLM_ERROR_CODE PostDynamicDiskRestoreOperns(std::vector<std::string>& RestoredDisk, bool restartVds = true);
	bool GetSizeOfVolume(const std::string& strVolGuid, SV_ULONGLONG& volSize);
	bool MountEfiVolume(const std::string& strVolGuid, std::string& strMountPoint);
	void RemoveAndDeleteMountPoints(const std::string & strEfiMountPoint);
	bool RescanDiskMgmt();
	HRESULT restartService(const std::string& serviceName);
	DLM_ERROR_CODE CleanDisk(const std::string& DiskName);
	DLM_ERROR_CODE DiskCheckAndRectify();
	bool GetPartitionFileNameFromReg(std::string & strFileName);
	DLM_ERROR_CODE ImportDisks(std::set<std::string>& DiskNames);
	DLM_ERROR_CODE AutoMount(bool bFlag);
	DLM_ERROR_CODE ClearAttributeOfDisk(const std::string& DiskName, DISK_TYPE_ATTRIBUTE DiskAttribute = DISK_READ_ONLY);
	DLM_ERROR_CODE InitializeDisk(std::string DiskName, DISK_FORMAT_TYPE FormatType = MBR, DISK_TYPE DiskType = BASIC);

private:
    bool GetDiskNamesAndScsiIds(std::map<std::string, SCSIID> & mapDiskNamesToScsiIDs);    
    bool GetDiskSize(const char * DiskName, SV_LONGLONG & DiskSize);    
    bool GetVolumesInfo(VolumesInfoMultiMap_t & mmapDiskToVolInfo,
                    std::list<std::string>& erraticVolumeGuids);
    bool GetDiskBytesPerSector(const char * DiskName, SV_ULONG & BytesPerSector);
	HRESULT InitCOM();
	HRESULT GetWbemService(IWbemServices **pWbemService, IWbemLocator **pLoc);
    BOOL EnumerateVolumes(std::map<std::string,std::string> & mapOfGuidsWithLogicalNames, 
                HANDLE , TCHAR *, SV_ULONG , SV_INT &,
                std::list<std::string>& erraticVolumeGuids);
    bool GetMapOfDiskToVolInfo(std::map<std::string,std::string> mapOfGuidsWithLogicalNames, 
                               VolumesInfoMultiMap_t & mmapDiskToVolInfo);
    bool isVirtualVolume(std::string sVolumeName);
	bool RegGetValueOfTypeSzAndMultiSz(HKEY hKey, const std::string& KeyToFetch, std::string & Value);
	bool ChangeREG_SZ(HKEY hKey,const std::string& strName, const std::string& strTempRegValue);
	bool ExecuteProcess(const std::string &FullPathToExe, const std::string &Parameters, std::string& strOurPutFile);
	bool ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes);
	bool GetSystemVolume(std::string & strSysVol);
	DLM_ERROR_CODE DeleteDlmInfoFromReg(const std::string& strRegKey, const std::string& strRegValue);
	bool ImportDynDisks(std::list<std::string> & lstFrnDiskNums, bool & bImport);
	bool RecoverDynDisks(std::list<std::string> & listDiskNum);
	bool IsVolumeNotMounted(const std::string& strVolumeGuid , std::string & strMountPoint);
	bool ListAssignedDrives(std::set<std::string>& lstDrives);
	bool FindOutFreeDrive(std::string& strFreeDrive, std::set<std::string>& lstAssignedDrives);
	bool MountW2K8EfiVolume(std::string& strMountPoint, const std::string& strVolGuid, std::set<std::string>& lstAssignedDrives);
};

#endif  /* WIN32 */

#endif /* DLWINDOWS__H */
