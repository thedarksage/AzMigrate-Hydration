#ifndef VOLUME__CLUSTER__INFOIMP__H_
#define VOLUME__CLUSTER__INFOIMP__H_

#include "inmdefines.h"
#include "volumeclusterinfo.h"
#include "volumeclusterinfoif.h"

class VolumeClusterInformerImp : public VolumeClusterInformer
{
public:
    VolumeClusterInformerImp();
    ~VolumeClusterInformerImp();
    /* returns false on failure for now */
    bool Init(const VolumeDetails &vd);
    /* returns false on failure for now */
    bool InitializeNoFsCluster(const SV_LONGLONG size, const SV_LONGLONG startoffset, const SV_UINT minimumclustersizepossible);
    /* returns 0 on failure */
    SV_ULONGLONG GetNumberOfClusters(void);    
    /* returns false on failure for now */
    bool GetVolumeClusterInfo(VolumeClusterInfo &vci);
    FeatureSupportState_t GetSupportState(void);

private:
    VolumeClusterInfoIf *m_pVciIf;
    FeatureSupportState_t m_featureSupportState;

private:
    bool InitClusterInfoIf(const VolumeDetails &vd);
};

#endif /* VOLUME__CLUSTER__INFOIMP__H_ */
