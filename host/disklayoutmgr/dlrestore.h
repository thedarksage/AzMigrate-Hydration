#pragma once
#ifndef DLRESOTRE__H
#define DLRESOTRE__H

#include "common.h"
#include "dldiscover.h"


class DLRestore : public DLDiscover
{
public:
    DLM_ERROR_CODE CollectDisksInfo (const char * BinaryFileName,
                                     DisksInfoMap_t & SrcMapDiskNamesToDiskInfo,
                                     DisksInfoMap_t & TgtMapDiskNamesToDiskInfo,
                                     std::list<std::string>& erraticVolumeGuids
                                     );

    DLM_ERROR_CODE GetCorruptedDisks (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                      DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                      std::vector<std::string> & CorruptedSrcDiskNames,
                                      std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks
                                      );

    DLM_ERROR_CODE RestoreDiskStructure (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                         DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                         std::map<std::string, std::string> MapSrcToTgtDiskNames,
                                         std::vector<std::string>& RestoredSrcDiskNames,
                                         bool restartVds = true
                                         );
    DLM_ERROR_CODE ClearDynamicDiskMetaData(DisksInfoMap_t&  SrcMapDiskNamesToDiskInfo,
                                            std::map<std::string, std::string>& MapSrcToTgtDiskNames);

	DLM_ERROR_CODE RestoreDiskPartitions (const char * BinaryFileName,
                                         std::map<std::string, std::string>& MapSrcToTgtDiskNames,
                                         std::set<std::string>& RestoredSrcDiskNames
                                         );

    DLM_ERROR_CODE GetMapSrcToTgtVol (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                      std::map<std::string, std::string> MapSrcToTgtDiskNames,
                                      std::map<std::string, std::string> & MapSrcToTgtVolumeNames,
                                      std::list<std::string>& erraticVolumeGuids);

	DLM_ERROR_CODE RestoreDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo);	
	DLM_ERROR_CODE RestorePartitions(const char * TgtDiskName, DlmPartitionInfoVec_t& vecPartitions);
	DLM_ERROR_CODE RestoreW2K8Efi(const char * BinaryFileName, 
									std::map<std::string, std::string>& MapSrcToTgtDiskNames, 
									std::set<DiskName_t>& RestoredSrcDiskNames);

	DLM_ERROR_CODE GetFilteredCorruptedDisks(std::vector<DiskName_t>& SelectedSrcDiskNames,
										     std::vector<DiskName_t> & CorruptedSrcDiskNames);

private:
    DLM_ERROR_CODE RestoreMbrDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo);
    DLM_ERROR_CODE RestoreGptDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo);
    DLM_ERROR_CODE RestoreEBRs(const char * DiskName, SV_LONGLONG PartitionStartOffset, SV_ULONGLONG BytesPerSector, std::vector<EBR_SECTOR> vecEBRs);
    DLM_ERROR_CODE FilterDisksBySizes(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                      DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                      std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks                                      
                                      );
    DLM_ERROR_CODE FilterDisksByBootRecords(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                            DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                            std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks,
                                            std::vector<std::string> & IdentifiedSrcDisks,
                                            std::vector<std::string> & IdentifiedTgtDisks,
                                            std::vector<std::string> & GoodSrcDiskNames
                                            );        
    void RemoveTheIsolatedDisks(std::map<std::string, std::vector<std::string> > & MapSrcDisksToTgtDisks,
                                std::vector<std::string> IdentifiedSrcDisks,
                                std::vector<std::string> IdentifiedTgtDisks
                                );
    void FindCorruptedDisks(std::map<std::string, std::vector<std::string> > MapSrcDisksToTgtDisks,
                            std::vector<std::string> GoodSrcDiskNames,
                            std::vector<std::string> & CorruptedSrcDiskNames
                            );
                                      
    bool CompareBootRecords(std::string SrcDiskName,
                            std::string TgtDiskName,
                            DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                            DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                            std::vector<std::string> & GoodSrcDiskNames
                            );
    bool VectorFind(std::string Element , std::vector<std::string> VecElements);
    bool CompareGPTs(SV_UCHAR SrcGptSector[], SV_UCHAR TgtGptSector[]);
	bool CompareDynamicGPTs(SV_UCHAR SrcGptSector[], SV_UCHAR TgtGptSector[]);
    bool CompareMBRs(SV_UCHAR SrcMbrSector[], SV_UCHAR TgtMbrSector[]);
    bool CompareEBRs(DISK_INFO SrcDiskInfo, DISK_INFO TgtDiskInfo);
	bool CompareDynamicMBRs(SV_UCHAR SrcMbrSector[], SV_UCHAR TgtMbrSector[]);
	bool CompareDynamicLDMs(SV_UCHAR SrcLdmSector[], SV_UCHAR TgtLdmSector[]);
#ifdef WIN32
	bool ProcessW2K8EfiPartition(const std::string& strTgtDisk, DlmPartitionInfoSubVec_t & vecDp);
	void NewlyGeneratedVolume(std::map<std::string,std::string>& mapPrevVolumes, std::map<std::string,std::string>& mapCurVolumes, std::list<std::string>& lstNewVolume);
	void GetEfiVolume(std::list<std::string>& lstVolumes, DlmPartitionInfoSubVec_t & vecDp, std::string& strEfiVol);
	bool UpdateW2K8EfiChanges(std::string & strEfiMountPoint);
#endif
};

#endif /* DLRESOTRE__H */
