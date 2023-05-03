#pragma once
#ifndef DLCOMMON__H
#define DLCOMMON__H

#include "common.h"

class DLCommon
{
public:
    bool ReadFromDisk(const char * DiskName, SV_LONGLONG BytesToSkip, size_t BytesToRead, SV_UCHAR * DiskData);
    bool WriteToDisk(const char * DiskName, SV_LONGLONG BytesToSkip, size_t BytesToWrite, const SV_UCHAR * DiskData);    
    void GetBytesFromMBR(const SV_UCHAR * MbrArr, size_t StartIndex, size_t BytesToFetch, SV_UCHAR * OutData);
    void ConvertArrayFromHexToDec(SV_UCHAR * HexArray, size_t NoOfBytes, SV_LONGLONG & DecNumber);
    bool isExtendedPartition(SV_UCHAR * Sector, size_t Index);
    void GetEBRStartingOffset(SV_UCHAR * BootRecord, size_t BootRecordOffset, SV_LONGLONG & Offset);
    void GetVolumesPerGivenDisk(const char * DiskName,
                                VolumesInfoMultiMap_t mmapDiskToVolInfo, 
                                VolumesInfoVec_t & vecVolsInfo);

    bool ReadFromBinaryFile(SV_UCHAR * FileData, size_t BytesToRead, const char * FileName);
    bool WriteToBinaryFile(const SV_UCHAR * FileData, size_t BytesToWrite, const char * FileName);
    
    bool isValidMBR(SV_UCHAR * Sector);
    bool isGptDisk(SV_UCHAR * Sector);
    bool isGptDiskDynamic(SV_UCHAR * Sector);
    void GetGUIDFromGPT(SV_UCHAR * Sector, size_t Offset, GPT_GUID & PartitionTypeGuid);
    bool isMbrDiskDynamic(SV_UCHAR * Sector);
    bool isUninitializedDisk(SV_UCHAR * Sector);
    DLM_ERROR_CODE CollectEBRs(const char * DiskName, SV_LONGLONG PartitionStartOffset, SV_ULONGLONG BytesPerSector, std::vector<EBR_SECTOR> & vecEBRs);
	DLM_ERROR_CODE ProcessMbrDisk(DISK_INFO & d, const char * DiskName, SV_UCHAR * MbrSector, std::string& strDiskType);
	DLM_ERROR_CODE ProcessGptDisk(DISK_INFO & d, const char * DiskName, std::string& strDiskType);
	DLM_ERROR_CODE ProcessDiskAndUpdateDiskInfo(DISK_INFO & d, const char * DiskName, std::string& strDiskType);    
    
    void SetDiskFlag(DISK_INFO & d, SV_ULONGLONG f);
    void SetDiskName(DISK_INFO & d, const char * DiskName);
    void SetDiskScsiId(DISK_INFO & d, SCSIID s);
    void SetDiskSize(DISK_INFO & d, SV_LONGLONG ds);
	void SetDiskSignature(DISK_INFO & d, const char * strDiskSignature);
	void SetDiskDeviceId(DISK_INFO & d, const char * strDeviceId);
    void SetDiskVolInfo(DISK_INFO & d, const char * DiskName, VolumesInfoMultiMap_t mmapDiskToVolInfo);
	void SetDiskBytesPerSector(DISK_INFO & d, SV_ULONGLONG bytesPerSector);
    void SetDiskType(DISK_INFO & d, DISK_TYPE t);
    void SetDiskFormatType(DISK_INFO & d, DISK_FORMAT_TYPE ft);
    void SetDiskEbrCount(DISK_INFO & d, SV_ULONGLONG count);    
    void SetDiskEbrSectors(DISK_INFO & d, std::vector<EBR_SECTOR> vecEBRs);
    void SetDiskMbrSector(DISK_INFO & d, SV_UCHAR MbrSector[]);
    void SetDiskGptSector(DISK_INFO & d, SV_UCHAR GptSector[]);
	void SetDynamicDiskMbrSector(DISK_INFO & d, SV_UCHAR MbrSector[]);
    void SetDynamicDiskGptSector(DISK_INFO & d, SV_UCHAR GptSector[]);
	void SetDynamicDiskLdmSector(DISK_INFO & d, SV_UCHAR GptSector[]);
};

#endif  /* DLCOMMON__H */
