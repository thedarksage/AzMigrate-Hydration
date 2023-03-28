///
///  \file  devicefilterfeatures.h
///
///  \brief contains DeviceFilterFeatures class which is interface for specific filter driver features classes
///

#ifndef DEVICE_FILTER_FEATURES_H
#define DEVICE_FILTER_FEATURES_H

#include <boost/shared_ptr.hpp>

class DeviceFilterFeatures
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<DeviceFilterFeatures> Ptr; ///< Pointer type

    /// \brief constructor
    DeviceFilterFeatures(const bool &isPerIOTimestampSupported,               ///< per io time stamp support
        const bool &isDiffOrderSupported,                                     ///< diff ordering support     
        const bool &isWriteOrderStateSupported,                               ///< explicit write order state support
        const bool &isDeviceStatsSupported,                                   ///< device stats support
        const bool &isMaxDeviceTransferSizeSupported,                         ///< max transfer size support
        const bool &isMirrorSupported,                                        ///< is mirroring supported (prism case)
        const bool &isCxEventSupported,                                       ///< is cx event supported
        const bool &isCommitTagNotifySupported,                               ///< is commit tag notify supported
        const bool &isBlockDrainTagSupported                                  ///< is block drain tag supported
        )
        : m_IsPerIOTimestampSupported(isPerIOTimestampSupported),
        m_IsDiffOrderSupported(isDiffOrderSupported),
        m_IsWriteOrderStateSupported(isWriteOrderStateSupported),
        m_IsDeviceStatsSupported(isDeviceStatsSupported),
        m_IsMaxDeviceTransferSizeSupported(isMaxDeviceTransferSizeSupported),
        m_IsMirrorSupported(isMirrorSupported),
        m_IsCxEventsSupported(isCxEventSupported),
        m_IsCommitTagNotifySupported(isCommitTagNotifySupported),
        m_IsBlockDrainTagSupported(isBlockDrainTagSupported)
    {
    }

    /// \return
    /// \li \c true: If device filter supports per io time stamp
    /// \li \c false: otherwise
    bool IsPerIOTimestampSupported(void) const
    {
        return m_IsPerIOTimestampSupported;
    }

    /// \return
    /// \li \c true: If device filter supports diff ordering i,e. current diff carries previous diff's last time stamp and sequence number
    /// \li \c false: otherwise
    bool IsDiffOrderSupported(void) const
    {
        return m_IsDiffOrderSupported;
    }

    /// \return
    /// \li \c true: If device filter supports explicit write order indicator
    /// \li \c false: otherwise
    bool IsWriteOrderStateSupported(void) const
    {
        return m_IsWriteOrderStateSupported;
    }

    /// \return
    /// \li \c true: If device filter supports querying of filtered device stats
    /// \li \c false: otherwise
    bool IsDeviceStatsSupported(void) const
    {
        return m_IsDeviceStatsSupported;
    }

    /// \return
    /// \li \c true: If device filter needs max transfer size of filtered device
    /// \li \c false: otherwise
    bool IsMaxDeviceTransferSizeSupported(void) const
    {
        return m_IsMaxDeviceTransferSizeSupported;
    }

    /// \return
    /// \li \c true: If mirror is supported
    /// \li \c false: otherwise
    bool IsMirrorSupported(void) const
    {
        return m_IsMirrorSupported;
    }

    /// \return
    /// \li \c true: If cx events are supported
    /// \li \c false: otherwise
    bool IsCxEventsSupported(void) const
    {
        return m_IsCxEventsSupported;
    }

    /// \return
    /// \li \c true: If commit tag notify is supported
    /// \li \c false: otherwise
    bool IsCommitTagNotifySupported(void) const
    {
        return m_IsCommitTagNotifySupported;
    }

    /// \return
    /// \li \c true: If block drain tag is supported
    /// \li \c false: otherwise
    bool IsBlockDrainTagSupported(void) const
    {
        return m_IsBlockDrainTagSupported;
    }

    virtual ~DeviceFilterFeatures()
    {
    }

protected:
    bool m_IsPerIOTimestampSupported;        ///< per io time stamp support

    bool m_IsDiffOrderSupported;             ///< diff ordering support

    bool m_IsWriteOrderStateSupported;       ///< explicit write order state support

    bool m_IsDeviceStatsSupported;           ///< device stats support

    bool m_IsMaxDeviceTransferSizeSupported; ///< max transfer size support

    bool m_IsMirrorSupported;                ///< mirror support

    bool m_IsCxEventsSupported;              ///< CX events for churn vs throughput support

    bool m_IsCommitTagNotifySupported;       ///< commit tag notify support

    bool m_IsBlockDrainTagSupported;         ///< block drain tag support
};

#endif /* DEVICE_FILTER_FEATURES_H */
