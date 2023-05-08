#include "drivermirrorerror.h"
#include <utility>
#include <iterator>


PRISM_VOLUME_INFO::MIRROR_ERROR DriverMirrorError::GetMirrorErrorFromDriver(const SV_LONGLONG drverr)
{
    DriverToMirrorError_t::const_iterator iter = m_DriverToMirrorError.find(drverr);
    PRISM_VOLUME_INFO::MIRROR_ERROR err = PRISM_VOLUME_INFO::MirrorErrorUnknown;
    if (m_DriverToMirrorError.end() != iter)
    {
        err = iter->second;
    }
    
    return err;
}

