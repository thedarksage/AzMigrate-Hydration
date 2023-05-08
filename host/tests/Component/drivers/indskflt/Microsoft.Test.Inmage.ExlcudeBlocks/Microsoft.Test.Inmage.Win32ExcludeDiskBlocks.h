#ifndef _MICROSOFTTESTINMAGEEXLCUDEBLOCKSWIN32_H_
#define _MICROSOFTTESTINMAGEEXLCUDEBLOCKSWIN32_H_

#include "Microsoft.Test.Inmage.ExlcudeBlocks.h"
#include "PlatformAPIs\win\PlatformAPIs.h"
#include "InmageTestException.h"
#include "InmageIoctlController.h"
#include "IBlockDevice.h"
#include "CDiskDevice.h"

#include "boost/shared_ptr.hpp"

#include <Ntddvol.h>
#include <map>

#define BUFFER_SIZE_LCN     (1024 * 2 * sizeof(LARGE_INTEGER))

class MICROSOFTTESTINMAGEEXLCUDEBLOCKS_API CWin32ExcludeDiskBlocks : public IInmageExlcudeBlocks {
private:
    CInmageIoctlController* m_inmageIoctlController;
    IPlatformAPIs* m_platformApis;
    std::map<std::string, std::list<ExtentInfo>>        m_DiskExcludeBlocks;
    ILogger* m_pLogger;

    SV_ULONG GetPagefilevol(std::vector<std::string>* pPageFileNames);
    void GetBitmapFilePaths(std::vector<std::string>* pPageFileNames);
    SV_LONGLONG GetBytesPerCluster();
    void UpdateExludeList(std::string fileName, bool bPagefile);
    void UpdateExcludeRanges(void* volhandle, void* pRetrievalPointer, SV_ULONG ulBytesPerCluster);
public:
    CWin32ExcludeDiskBlocks(IPlatformAPIs* platformApis, ILogger* pLogger, CInmageIoctlController* pIoctlController);
    std::list<ExtentInfo> GetExcludeBlocks(std::string diskId);
};

#endif