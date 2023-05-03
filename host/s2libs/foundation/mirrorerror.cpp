#include <utility>
#include <iterator>
#include "mirrorerror.h"

MirrorError::MirrorError()
{
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::MirrorErrorUnknown,      "Unknown mirror error"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunDiscoveryFailed,    "AT Lun discovery failed"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunIdFailed,           "Failed to get AT Lun Device ID"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AgentMemErr,             "Memory related error at Agent"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunDeletionFailed,     "AT Lun deletion failed"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcLunInvalid,           "Source LUN(s) given in settings are invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::DrvMemAllocErr,          "Memory allocation error at driver"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::DrvMemCopyoutErr,        "Driver Memory copyout error"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::DrvMemCopyinErr,         "Driver Memory copyin error"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunInvalid,            "AT Lun is invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcNameChangedErr,       "Source name(s) changed"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunNameChangedErr,     "AT Lun name(s) changed"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::MirrorStackingErr,       "Mirror stacking error"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::ResyncClearError,        "Resync clear error"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::ResyncNotSetOnClearErr,  "Resync not set when asked for clear"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcDevListMismatchErr,   "Source device list does not match"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunDevListMismatchErr, "AT Lun device list does not match"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcDevScsiIdErr,         "Source device ID invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::AtlunDevScsiIdErr,       "AT Lun device ID invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::MirrorNotSetup,          "Mirror not setup"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::MirrorDeleteIoctlFailed, "Mirror delete ioctl failed"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::MirrorNotSupported,      "Mirror not supported"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcsTypeMismatch,        "Sources types mismatch"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcsIsMulpathMismatch,   "Sources is multipath attribute mismatch"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcsVendorMismatch,      "Sources vendor mismatch"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcNotReported,          "Source not discovered"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::SrcCapacityInvalid,      "Source capacity is invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::MirrorStateInvalid,      "Mirror state is invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::ATLunNumberInvalid,      "AT Lun number invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::ATLunNameInvalid,        "AT Lun name invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::PIPwwnsInvalid,          "PI pwwns invalid"));
    m_MirrorErrorToMsg.insert(std::make_pair(PRISM_VOLUME_INFO::ATPwwnsInvalid,          "AT pwwns invalid"));
}


std::string MirrorError::GetMirrorErrorMessage(const PRISM_VOLUME_INFO::MIRROR_ERROR mirrorerror)
{
    MirrorErrorToMsg_t::const_iterator iter = m_MirrorErrorToMsg.find(mirrorerror);
    std::string errmsg = m_MirrorErrorToMsg[PRISM_VOLUME_INFO::MirrorErrorUnknown];
    if (m_MirrorErrorToMsg.end() != iter)
    {
        errmsg = iter->second;
    }

    return errmsg;
}


