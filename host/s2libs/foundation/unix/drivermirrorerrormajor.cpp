#include "drivermirrorerror.h"
#include "devicefilter.h"
#include <utility>

DriverMirrorError::DriverMirrorError()
{
    m_DriverToMirrorError.insert(std::make_pair(SRC_LUN_INVALID,             PRISM_VOLUME_INFO::SrcLunInvalid));
    m_DriverToMirrorError.insert(std::make_pair(ATLUN_INVALID,               PRISM_VOLUME_INFO::AtlunInvalid));
    m_DriverToMirrorError.insert(std::make_pair(DRV_MEM_ALLOC_ERR,           PRISM_VOLUME_INFO::DrvMemAllocErr));
    m_DriverToMirrorError.insert(std::make_pair(DRV_MEM_COPYIN_ERR,          PRISM_VOLUME_INFO::DrvMemCopyinErr));
    m_DriverToMirrorError.insert(std::make_pair(DRV_MEM_COPYOUT_ERR,         PRISM_VOLUME_INFO::DrvMemCopyoutErr));
    m_DriverToMirrorError.insert(std::make_pair(SRC_NAME_CHANGED_ERR,        PRISM_VOLUME_INFO::SrcNameChangedErr));
    m_DriverToMirrorError.insert(std::make_pair(ATLUN_NAME_CHANGED_ERR,      PRISM_VOLUME_INFO::AtlunNameChangedErr));
    m_DriverToMirrorError.insert(std::make_pair(MIRROR_STACKING_ERR,         PRISM_VOLUME_INFO::MirrorStackingErr));
    m_DriverToMirrorError.insert(std::make_pair(RESYNC_CLEAR_ERROR,          PRISM_VOLUME_INFO::ResyncClearError));
    m_DriverToMirrorError.insert(std::make_pair(RESYNC_NOT_SET_ON_CLEAR_ERR, PRISM_VOLUME_INFO::ResyncNotSetOnClearErr));
    m_DriverToMirrorError.insert(std::make_pair(SRC_DEV_LIST_MISMATCH_ERR,   PRISM_VOLUME_INFO::SrcDevListMismatchErr));
    m_DriverToMirrorError.insert(std::make_pair(ATLUN_DEV_LIST_MISMATCH_ERR, PRISM_VOLUME_INFO::AtlunDevListMismatchErr));
    m_DriverToMirrorError.insert(std::make_pair(SRC_DEV_SCSI_ID_ERR,         PRISM_VOLUME_INFO::SrcDevScsiIdErr));
    m_DriverToMirrorError.insert(std::make_pair(DST_DEV_SCSI_ID_ERR,         PRISM_VOLUME_INFO::AtlunDevScsiIdErr));
    m_DriverToMirrorError.insert(std::make_pair(MIRROR_NOT_SETUP,            PRISM_VOLUME_INFO::MirrorNotSetup));
    m_DriverToMirrorError.insert(std::make_pair(MIRROR_NOT_SUPPORTED,        PRISM_VOLUME_INFO::MirrorNotSupported));
}

