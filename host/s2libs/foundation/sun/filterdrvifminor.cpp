#include <cstring>
#include <string>
#include <sstream>
#include "filterdrvifminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"


FilterDrvIfMinor::FilterDrvIfMinor()
{
}


int FilterDrvIfMinor::PassMaxTransferSizeIfNeeded(const DeviceFilterFeatures *pdevicefilterfeatures, const std::string &volume, DeviceStream &devstream,
                                                  const VolumeReporter::VolumeReport_t &vr)
{
    return SV_SUCCESS;
}


bool FilterDrvIfMinor::GetVolumeBlockSize(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &blksz, const int &errorfromgetattr, 
                                          std::string &reasonifcant)
{
    std::stringstream ss;
    int oserrcode = Error::UserToOS(errorfromgetattr);
    ss << "Failed to get blocks size from filter driver for volume " << devicename << " with error " << Error::Msg(oserrcode);
    reasonifcant = ss.str();
    return false;
}


bool FilterDrvIfMinor::GetNumberOfBlocksInVolume(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &nblks, const int &errorfromgetattr, 
                               std::string &reasonifcant)
{
    std::stringstream ss;
    int oserrcode = Error::UserToOS(errorfromgetattr);
    ss << "Failed to get number of blocks from filter driver for volume " << devicename << " with error " << Error::Msg(oserrcode);
    reasonifcant = ss.str();
    return false;
}
