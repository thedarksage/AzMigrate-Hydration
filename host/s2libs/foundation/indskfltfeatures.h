///
///  \file  indskfltfeatures.h
///
///  \brief contains InDskFltFeatures class encapsulating disk filter driver features
///

#ifndef __INDSKFLT_DRV_FEATURES__H
#define __INDSKFLT_DRV_FEATURES__H

#include <boost/shared_ptr.hpp>

#include "devicefilterfeatures.h"

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#endif
#include "devicefilter.h"

#include "svdparse.h"

class InDskFltFeatures : public DeviceFilterFeatures
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<InDskFltFeatures> Ptr; ///< Pointer type

    /// \brief constructor
    InDskFltFeatures(const DRIVER_VERSION &driverVersion)                   ///< driver version
        : DeviceFilterFeatures(true,                                        ///< per io time stamp support
        true,                                                               ///< diff ordering support
        true,                                                               ///< explicit write order state support
        true,                                                               ///< device stats support
        false,                                                              ///< max transfer size support
        false,                                                              ///< mirror support
        driverVersion.ulDrMinorVersion2 & SV_CX_EVENT_SUPPORT_POS,          ///< cx event support
        driverVersion.ulDrMinorVersion2 & SV_TAG_COMMIT_NOTIFY_SUPPORT_POS, ///< tag commit notify
        driverVersion.ulDrMinorVersion2 & SV_TAG_BLOCK_DRAIN_SUPPORT_POS    ///< tag commit notify
        )
    {
    }
};

#endif /* __INDSKFLT_DRV_FEATURES__H */
