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
    bool GetVolumeBlockSize(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &blksz, const int &errorfromgetattr, 
                            std::string &reasonifcant);
    bool GetNumberOfBlocksInVolume(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &nblks, const int &errorfromgetattr, 
                                   std::string &reasonifcant);
};

#endif /* _FILTERDRVMINOR__IF__H_ */
