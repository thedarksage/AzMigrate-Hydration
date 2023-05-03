#ifndef DLM_DISK_INFO_H
#define DLM_DISK_INFO_H

#include "common.h"
#include "dlvdshelper.h"
#include "time.h"

class DlmDiskInfo : public DLVdsHelper
{
public:
	static DlmDiskInfo& GetInstance();
    static bool Destroy();
	bool Init();
    bool HasUEFI(const std::string &diskID);
	void RefreshDiskLayoutManager();
	void GetPartitionInfo(const std::string& diskName, DlmPartitionInfoSubVec_t& vecPartitions);

private:
	static DlmPartitionInfoSubMap_t m_DiskPartitionInfo;
	static std::map<std::string, std::string> m_DisksHavingUEFI;
    static boost::mutex m_DlmDiskInfoLock;
    static DlmDiskInfo* theDlmDiskInfo;
	static time_t m_dlmLastRefreshTime;

	DlmDiskInfo();
	virtual ~DlmDiskInfo();
	bool RefreshDiskInfo(bool bNotUefi = true);
};

#endif /* DLM_DISK_INFO_H */
