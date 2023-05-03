#pragma once
#ifndef DLDISCOVER__H
#define DLDISCOVER__H

#include "common.h"
#include "dlwindows.h"
#include "dllinux.h"
#include "dlvdshelper.h"


class DLDiscover : public DisksInfoCollector
{
public:
	DLM_ERROR_CODE StoreDisksInfo(const std::string & binaryFile, std::list<std::string>& outFileNames, std::list<std::string>& erraticVolumeGuids, const std::string& dlmFileFlag = DLM_FILE_NORMAL);    
    DLM_ERROR_CODE GetDiskInfoMapFromBlob(const char * FileName, DisksInfoMap_t & DisksInfoMapFromBlob); //collect disk info from BLOB file.
	DLM_ERROR_CODE ReadDiskInfoMapFromFile(const char * FileName, DisksInfoMap_t & d);
	DLM_ERROR_CODE ReadPartitionInfoFromFile(const char * FileName, DlmPartitionInfoMap_t & dp);
	DLM_ERROR_CODE ReadPartitionSubInfoFromFile(const char * FileName, DlmPartitionInfoSubMap_t & dp);
	DLM_ERROR_CODE GetDlmVersion(const char * FileName, double & dlmVersion);

private:
    DLM_ERROR_CODE WriteDiskInfoMapToFile(const char * FileName, DisksInfoMap_t & d);
	DLM_ERROR_CODE DlmMd5Checksum(DisksInfoMap_t & d, unsigned char* curCheckSum);
	DLM_ERROR_CODE WritePartitionInfoToFile(const char * FileName, DlmPartitionInfoSubMap_t & dp);
	bool IsDlmInfoChanged(DisksInfoMap_t & d, bool& bCreate, bool & bCreatePartitionFile, std::string & strCurChecksum, std::list<std::string>& prevDlmFiles);
};

#endif /* DLDISCOVER__H */