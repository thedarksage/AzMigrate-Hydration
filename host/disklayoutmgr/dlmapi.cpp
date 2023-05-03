#include "common.h"
#include "dldiscover.h"
#include "dlrestore.h"
#include "dlmdiskinfo.h"


DLM_ERROR_CODE StoreDisksInfo(const std::string& binaryFileName, 
							std::list<std::string>& outFileNames, 
							std::list<std::string>& erraticVolumeGuids,
							const std::string& dlmFileFlag)
{
    DLDiscover obj;
	std::string dlmFlag;
	if(dlmFileFlag.compare("") == 0)
		dlmFlag = std::string(DLM_FILE_NORMAL);
	else
		dlmFlag = dlmFileFlag;
    return obj.StoreDisksInfo(binaryFileName, outFileNames, erraticVolumeGuids, dlmFlag);    
}


DLM_ERROR_CODE CollectDisksInfoAndConvert(const char * BinaryFileName,
                                         DisksInfoMap_t & SrcMapDiskNamesToDiskInfo,
                                         DisksInfoMap_t & TgtMapDiskNamesToDiskInfo
                                         )
{
    DLRestore obj;
    std::list<std::string> erraticVolumeGuids ;
    return obj.CollectDisksInfo(BinaryFileName, SrcMapDiskNamesToDiskInfo, 
                    TgtMapDiskNamesToDiskInfo, erraticVolumeGuids);    
}

DLM_ERROR_CODE CollectDisksInfoFromSystem(DisksInfoMap_t & TgtMapDiskNamesToDiskInfo)
{
    DLRestore obj;
    std::list<std::string> erraticVolumeGuids ;
	DlmPartitionInfoSubMap_t PartitionsInfoMapFromSys;
	bool bPartitionExist;
    return obj.GetDiskInfoMapFromSystem(TgtMapDiskNamesToDiskInfo, PartitionsInfoMapFromSys, erraticVolumeGuids, bPartitionExist);   
}


DLM_ERROR_CODE GetCorruptedDisks (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                  DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                  std::vector<DiskName_t> & CorruptedSrcDiskNames,
                                  std::map<DiskName_t, std::vector<DiskName_t> > & MapSrcDisksToTgtDisks
                                  )
{
    DLRestore obj;
    return obj.GetCorruptedDisks(SrcMapDiskNamesToDiskInfo, TgtMapDiskNamesToDiskInfo, CorruptedSrcDiskNames, MapSrcDisksToTgtDisks);    
}


DLM_ERROR_CODE RestoreDiskStructure (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                     DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                     std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
                                     std::vector<DiskName_t> & RestoredSrcDiskNames,
                                     bool restartVds
                                     )
{
    DLRestore obj;
    return obj.RestoreDiskStructure(SrcMapDiskNamesToDiskInfo, TgtMapDiskNamesToDiskInfo, MapSrcToTgtDiskNames, RestoredSrcDiskNames);    
}


DLM_ERROR_CODE RestoreDiskPartitions(const char * BinaryFileName,
                                     std::map<DiskName_t, DiskName_t>& MapSrcToTgtDiskNames,
                                     std::set<DiskName_t>& RestoredSrcDiskNames
                                     )
{
	DLRestore obj;
    return obj.RestoreDiskPartitions(BinaryFileName, MapSrcToTgtDiskNames, RestoredSrcDiskNames); 
}


DLM_ERROR_CODE GetMapSrcToTgtVol (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                  std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
                                  std::map<VolumeName_t, VolumeName_t> & MapSrcToTgtVolumeNames
                                  )
{
    DLRestore obj;
    std::list<std::string> erraticVolumeGuids ;
    return obj.GetMapSrcToTgtVol(SrcMapDiskNamesToDiskInfo, MapSrcToTgtDiskNames, 
                        MapSrcToTgtVolumeNames, erraticVolumeGuids);    
}

DLM_ERROR_CODE RestoreVolumeMountPoints (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                         std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
                                         int mountNameType
                                         )
{
    DLRestore obj;
    std::list<std::string> erraticVolumeGuids ;
    return obj.RestoreMountPoints(SrcMapDiskNamesToDiskInfo, MapSrcToTgtDiskNames, erraticVolumeGuids, mountNameType);
}

DLM_ERROR_CODE ReadDiskInfoMapFromFile(const char * BinaryFileName, DisksInfoMap_t & mapDiskNamesToDiskInfo)
{
	DLDiscover obj; 
	return obj.ReadDiskInfoMapFromFile(BinaryFileName, mapDiskNamesToDiskInfo);
}

DLM_ERROR_CODE GetDlmVersion(const char * FileName, double & dlmVersion)
{
	DLDiscover obj; 
	return obj.GetDlmVersion(FileName, dlmVersion);
}

DLM_ERROR_CODE ReadPartitionInfoFromFile(const char * BinaryFileName, DlmPartitionInfoMap_t & mapDiskToPartition)
{
	DLDiscover obj; 
	return obj.ReadPartitionInfoFromFile(BinaryFileName, mapDiskToPartition);
}

DLM_ERROR_CODE ReadPartitionSubInfoFromFile(const char * BinaryFileName, DlmPartitionInfoSubMap_t & mapDiskToPartition)
{
	DLDiscover obj; 
	return obj.ReadPartitionSubInfoFromFile(BinaryFileName, mapDiskToPartition);
}

DLM_ERROR_CODE RestoreDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo)
{
	DLRestore obj; 
	return obj.RestoreDisk(TgtDiskName, SrcDiskName, SrcDiskInfo);
}

DLM_ERROR_CODE RestorePartitions(const char * TgtDiskName, DlmPartitionInfoVec_t& vecPartitions)
{
	DLRestore obj; 
	return obj.RestorePartitions(TgtDiskName, vecPartitions);
}

bool HasUEFI(const std::string& DiskID)
{
	return DlmDiskInfo::GetInstance().HasUEFI(DiskID);
}

void RefreshDiskLayoutManager()
{
	DlmDiskInfo::GetInstance().RefreshDiskLayoutManager();
}

void GetPartitionInfo(const std::string& DiskName, DlmPartitionInfoSubVec_t& vecPartitions)
{
	DlmDiskInfo::GetInstance().GetPartitionInfo(DiskName, vecPartitions);
}

DLM_ERROR_CODE OnlineOrOfflineDisk(const std::string& DiskName, const bool& bOnline)
{
	DLDiscover obj;
	return obj.OnlineOrOfflineDisk(DiskName, bOnline);
}

DLM_ERROR_CODE PostDynamicDiskRestoreOperns(std::vector<std::string>& RestoredDisk, bool restartVds)
{
	DLDiscover obj;
	return obj.PostDynamicDiskRestoreOperns(RestoredDisk, restartVds);
}

DLM_ERROR_CODE ConvertEfiToNormalPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp)
{
	DLDiscover obj;
	return obj.ConvertEfiToNormalPartition(DiskName, vecDp);
}

DLM_ERROR_CODE ConvertNormalToEfiPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp)
{
	DLDiscover obj;
	return obj.ConvertNormalToEfiPartition(DiskName, vecDp);
}

DLM_ERROR_CODE DeleteDlmRegitryEntries()
{
	DLDiscover obj;
	return obj.DeleteDlmRegitryEntries();
}

DLM_ERROR_CODE RestoreW2K8Efi(const char * BinaryFileName,
                                     std::map<DiskName_t, DiskName_t>& MapSrcToTgtDiskNames,
                                     std::set<DiskName_t>& RestoredSrcDiskNames
                                     )
{
	DLRestore obj;
    return obj.RestoreW2K8Efi(BinaryFileName, MapSrcToTgtDiskNames, RestoredSrcDiskNames); 
}

DLM_ERROR_CODE CleanDisk(const std::string& DiskName)
{
	DLDiscover obj;
	return obj.CleanDisk(DiskName);
}

DLM_ERROR_CODE GetFilteredCorruptedDisks (std::vector<DiskName_t>& SelectedSrcDiskNames,
										std::vector<DiskName_t> & CorruptedSrcDiskNames
										)
{
    DLRestore obj;
    return obj.GetFilteredCorruptedDisks(SelectedSrcDiskNames, CorruptedSrcDiskNames);    
}

bool DownloadFromDisk(const char *DiskName, SV_LONGLONG BytesToSkip, size_t BytesToRead, SV_UCHAR *DiskData)
{
	DLDiscover obj;
	return obj.ReadFromDisk(DiskName, BytesToSkip, BytesToRead, DiskData);
}

bool UpdateOnDisk(const char *DiskName, SV_LONGLONG BytesToSkip, size_t BytesToWrite, SV_UCHAR *DiskData)
{
	DLDiscover obj;
	return obj.WriteToDisk(DiskName, BytesToSkip, BytesToWrite, DiskData);
}

#ifdef WIN32
DLM_ERROR_CODE RestoreVConVolumeMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
										 DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                         std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
										 std::map<std::string, std::string> mapOfSrcVolToTgtVol
                                         )
{
    DLRestore obj;
    return obj.RestoreMountPoints(SrcMapDiskNamesToDiskInfo, TgtMapDiskNamesToDiskInfo, MapSrcToTgtDiskNames, mapOfSrcVolToTgtVol);
}

DLM_ERROR_CODE ImportDisks(std::set<std::string>& DiskNames)
{
	DLDiscover obj;
	return obj.ImportDisks(DiskNames);
}

DLM_ERROR_CODE AutoMount(bool bEnableFlag)
{
	DLDiscover obj;
	return obj.AutoMount(bEnableFlag);
}

DLM_ERROR_CODE ClearReadOnlyAttrOfDisk(const std::string& DiskName)
{
	DLDiscover obj;
	return obj.ClearAttributeOfDisk(DiskName);
}

DLM_ERROR_CODE InitializeDisk(std::string DiskName, DISK_FORMAT_TYPE FormatType, DISK_TYPE DiskType)
{
	DLDiscover obj;
	return obj.InitializeDisk(DiskName, FormatType, DiskType);
}

DLM_ERROR_CODE DiskCheckAndRectify()
{
	DLDiscover obj;
	return obj.DiskCheckAndRectify();
}
#endif
