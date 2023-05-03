#ifndef VOLUME__CLUSTER__INFOIF__H_
#define VOLUME__CLUSTER__INFOIF__H_

#include <string>
#include "svtypes.h"
#include "volumeclusterinfo.h"

class VolumeClusterInfoIf
{
public:
    /* returns 0 on failure */
    virtual SV_ULONGLONG GetNumberOfClusters(void) = 0;    
    /* returns false on failure for now */
    virtual bool GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci) = 0;
   
    virtual ~VolumeClusterInfoIf() {}
};

#endif /* VOLUME__CLUSTER__INFOIF__H_ */
