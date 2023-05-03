///
///  \file  involfltfeatures.h
///
///  \brief contains InVolFltFeatures class encapsulating volume filter driver features
///

#ifndef __INVOLFLT_DRV_FEATURES__H
#define __INVOLFLT_DRV_FEATURES__H

#include <boost/shared_ptr.hpp>

#include "devicefilterfeatures.h"

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#endif
#include "devicefilter.h"

#include "svdparse.h"

class InVolFltFeatures : public DeviceFilterFeatures
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<InVolFltFeatures> Ptr; ///< Pointer type

    /// \brief constructor
    InVolFltFeatures(const DRIVER_VERSION &driverVersion) ///< driver version
        : DeviceFilterFeatures(driverVersion.ulDrMinorVersion & SV_PER_IO_POS, ///< per io time stamp support
        driverVersion.ulDrMinorVersion2 & SV_DIRTYBLK_NUM_POS,                 ///< diff ordering support
        driverVersion.ulDrMinorVersion2 & SV_WO_STATE_POS,                     ///< explicit write order state support
        driverVersion.ulDrMinorVersion3 & SV_DEVICE_STATS_NUM_POS,             ///< device stats support
        driverVersion.ulDrMinorVersion3 & SV_MAX_TRANSFER_SIZE_NUM_POS,        ///< max transfer size support
        driverVersion.ulDrMinorVersion & SV_MIRROR_POS,                        ///< mirror support
        driverVersion.ulDrMinorVersion2 & SV_CX_EVENT_SUPPORT_POS,             ///< cx event support
        driverVersion.ulDrMinorVersion2 & SV_TAG_COMMIT_NOTIFY_SUPPORT_POS,    ///< tag commit notify
        driverVersion.ulDrMinorVersion3 & SV_TAG_BLOCK_DRAIN_SUPPORT_POS       ///< block drain tag
        )        
    {
    }
};

#endif /* __INVOLFLT_DRV_FEATURES__H */
