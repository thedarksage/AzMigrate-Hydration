#ifndef _FILTERDRVMINOR__IF__H_
#define _FILTERDRVMINOR__IF__H_

#include <vector>
#include <string>
#include "devicefilter.h"
#include "volumegroupsettings.h"
#include "devicefilterfeatures.h"
#include "filterdrvifmajor.h"
#include "devicestream.h"
#include "volumereporter.h"

class FilterDrvIfMinor : public FilterDrvIfMajor
{
public:
    FilterDrvIfMinor();
    int PassMaxTransferSizeIfNeeded(const DeviceFilterFeatures *pdevicefilterfeatures, const std::string &volume, DeviceStream &devstream,
                                    const VolumeReporter::VolumeReport_t &vr);
    int FillMaxTransferSize(const std::string &volume, inm_user_max_xfer_sz_t &mxs, const VolumeReporter::VolumeReport_t &vr);
    void PrintMaxTransferSize(const inm_user_max_xfer_sz_t &mxs);
    bool GetVolumeBlockSize(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &blksz, const int &errorfromgetattr, 
                            std::string &reasonifcant);
    bool GetNumberOfBlocksInVolume(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &nblks, const int &errorfromgetattr, 
                                   std::string &reasonifcant);
    std::string GetVolumeNameForLsAttr(const std::string &volume, const VolumeReporter::VolumeReport_t &vr);
};

#endif /* _FILTERDRVMINOR__IF__H_ */
