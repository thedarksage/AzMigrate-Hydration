#pragma once
#ifndef DLLINUX__H
#define DLLINUX__H

#ifndef WIN32

#include "common.h"
#include "dlcommon.h"

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>

#define LINUX_DEVNAME_DIR 			                "/dev/"
#define LINUX_SD_STARTING                           "sd"

// The functions in public exists in both windows and linux (and implementation is platform specific).
class DisksInfoCollector : public DLCommon
{
public:    
    DLM_ERROR_CODE GetDiskInfoMapFromSystem(DisksInfoMap_t & DisksInfoMapFromSys, 
											DlmPartitionInfoSubMap_t& srcPartitionInfoMap, 
											std::list<std::string>& erraticGuids,
											bool& bPartitionExist,
											const std::string & BinaryFile = "",
											const std::string & DlmFileFlag = DLM_FILE_NORMAL); 
    bool RefreshDisk(const char * DiskName);
    DLM_ERROR_CODE RestoreMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
									  std::list<std::string>& erraticVolumeGuids,
                                      int mountNameType);
	DLM_ERROR_CODE OnlineOrOfflineDisk(const std::string& DiskName, const bool& bOnline);
    DLM_ERROR_CODE PostDynamicDiskRestoreOperns(std::vector<std::string>& RestoredDisk, bool restartVds = true);
	DLM_ERROR_CODE ConvertEfiToNormalPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp);
	DLM_ERROR_CODE ConvertNormalToEfiPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp);
	DLM_ERROR_CODE DeleteDlmRegitryEntries();
	DLM_ERROR_CODE CleanDisk(const std::string& DiskName);
	DLM_ERROR_CODE DiskCheckAndRectify();

private:
    int GetDisksAndVolumesInfo(std::vector<std::string> & DiskNames, VolumesInfoMultiMap_t & mmapDiskToVolInfo);
    bool GetDiskSize(const char * DiskName, SV_LONGLONG & DiskSize);
    bool GetVolumeStartingSector(char * Name, SV_LONGLONG & StartingOffset);
    bool GetVolumeInfo(char * VolName, VOLUME_INFO & VolInfo); //Linux(Parition) is same as Windows(Volume)
    bool GetDiskBytesPerSector(const char * DiskName, SV_ULONG & BytesPerSector);
    char * GetParent(char * PartitionName);
    bool GetScsiId(const char * DiskName, SCSIID & ScsiId);
};

#endif /* WIN32 */

#endif /* DLLINUX__H */
