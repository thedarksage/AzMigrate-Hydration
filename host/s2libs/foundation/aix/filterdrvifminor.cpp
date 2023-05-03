#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include "filterdrvifminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "executecommand.h"


FilterDrvIfMinor::FilterDrvIfMinor()
{
}


int FilterDrvIfMinor::FillMaxTransferSize(const std::string &volume, inm_user_max_xfer_sz_t &mxs,
                                          const VolumeReporter::VolumeReport_t &vr)
{
    /* Place holder function */
    return SV_SUCCESS;
}


void FilterDrvIfMinor::PrintMaxTransferSize(const inm_user_max_xfer_sz_t &mxs)
{
    std::stringstream ss;
    ss << "mxs.mxs_flag = " << mxs.mxs_flag
       << ", mxs.mxs_devname = " << mxs.mxs_devname
       << ", mxs.mxs_max_xfer_sz = " << mxs.mxs_max_xfer_sz;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", ss.str().c_str());
}


int FilterDrvIfMinor::PassMaxTransferSizeIfNeeded(const DeviceFilterFeatures *pdevicefilterfeatures, const std::string &volume, DeviceStream &devstream, 
                                                  const VolumeReporter::VolumeReport_t &vr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    if (pdevicefilterfeatures->IsMaxDeviceTransferSizeSupported())
    {
        DebugPrintf(SV_LOG_DEBUG, "Need to give maximum transfer size to involflt for volume %s\n", volume.c_str());
        inm_user_max_xfer_sz_t mxs;
        iStatus = FillMaxTransferSize(volume, mxs, vr);
        if (iStatus == SV_SUCCESS)
        {
            PrintMaxTransferSize(mxs);
            iStatus = devstream.IoControl(IOCTL_INMAGE_MAX_XFER_SZ, &mxs, sizeof mxs);
            if (SV_SUCCESS != iStatus)
            {
                DebugPrintf(SV_LOG_ERROR, "ioctl to pass max transfer size for volume %s failed\n", volume.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "could not fill maximum transfer size for volume %s\n", volume.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return iStatus;
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


std::string FilterDrvIfMinor::GetVolumeNameForLsAttr(const std::string &volume, const VolumeReporter::VolumeReport_t &vr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with input volume name %s\n", FUNCTION_NAME, volume.c_str());
    std::string v = volume;
     
    if ((VolumeSummary::VXDMP == vr.m_Vendor) && 
        vr.m_bIsMultipathNode) 
    {
        DebugPrintf(SV_LOG_DEBUG, "%s is a vxdmp multipath node. Getting its enabled path\n", volume.c_str());
        std::string volumebasename = basename_r(volume.c_str(), '/');
        std::string cmd("vxdisk list ");
        cmd += volumebasename;
        cmd += " | grep \'state=enabled\' | awk \'{print $1}\'";
        std::stringstream results;

        std::string nativenode;
        if (executePipe(cmd, results))
        {
            results >> nativenode;
            if (!nativenode.empty())
                v = "/dev/"+nativenode;
            else
                DebugPrintf(SV_LOG_ERROR, "could not find native path for vxdmp node %s\n", volume.c_str());
        }
        else 
            DebugPrintf(SV_LOG_ERROR, "Failed to fork command %s to find native path for vxdmp\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with output volume name %s\n", FUNCTION_NAME, v.c_str());
    return v;
}
