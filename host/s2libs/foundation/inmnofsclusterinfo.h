#ifndef INMNOFS__CLUSTER__INFO__H_
#define INMNOFS__CLUSTER__INFO__H_

#include <string>
#include "svtypes.h"
#include "volumeclusterinfo.h"
#include "volumeclusterinfoif.h"

class InmNoFsClusterInfo : public VolumeClusterInfoIf
{
public:
    InmNoFsClusterInfo();
    InmNoFsClusterInfo(const SV_LONGLONG size, const SV_LONGLONG startoffset, const SV_UINT minimumclustersizepossible);
    ~InmNoFsClusterInfo();
    /* returns 0 on failure */
    SV_ULONGLONG GetNumberOfClusters(void);    
    /* returns false on failure for now */
    bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci);
private:
    void DetermineClusterSize(const SV_UINT candidateclustersize);
    bool AllocateBitmapIfReq(const size_t &outsize, VolumeClusterInformer::VolumeClusterInfo &vci);
    SV_UINT GetAdjustedMaxClusterSize(const SV_UINT &candidateclustersize, const SV_LONGLONG startoffset);
private:
    SV_LONGLONG m_Size;
    SV_UINT m_clusterSize;
    SV_UINT m_MaxClusterSizeToTry;
    SV_UINT m_MinClusterSizePossible;
};

#endif /* INMNOFS__CLUSTER__INFO__H_ */
