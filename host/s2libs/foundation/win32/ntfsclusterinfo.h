#ifndef NTFS__CLUSTER__INFO__H_
#define NTFS__CLUSTER__INFO__H_

#include <string>
#include <windows.h>
#include <winioctl.h>
#include "svtypes.h"
#include "volumeclusterinfo.h"
#include "volumeclusterinfoif.h"

class NtfsClusterInfo : public VolumeClusterInfoIf
{
public:
    NtfsClusterInfo();
    NtfsClusterInfo(const VolumeClusterInformer::VolumeDetails &vd);
    ~NtfsClusterInfo();
    /* returns 0 on failure */
    SV_ULONGLONG GetNumberOfClusters(void);    
    /* returns false on failure for now */
    bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci);
private: 
    VolumeClusterInformer::VolumeDetails m_Vd;
    STARTING_LCN_INPUT_BUFFER m_StartingLcn;
private:
    bool AllocateBitmapIfReq(const size_t &outsize, VolumeClusterInformer::VolumeClusterInfo &vci);
};

#endif /* NTFS__CLUSTER__INFO__H_ */
