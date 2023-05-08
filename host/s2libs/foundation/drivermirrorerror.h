#ifndef _DRV__MIRROR__ERROR__H_
#define _DRV__MIRROR__ERROR__H_

#include "svtypes.h"
#include "prismsettings.h"
#include <map>

class DriverMirrorError
{
    typedef std::map<SV_LONGLONG, PRISM_VOLUME_INFO::MIRROR_ERROR> DriverToMirrorError_t;
public:
    DriverMirrorError();
    PRISM_VOLUME_INFO::MIRROR_ERROR GetMirrorErrorFromDriver(const SV_LONGLONG drverr);
private:
    DriverToMirrorError_t m_DriverToMirrorError;
};

#endif /* _DRV__MIRROR__ERROR__H_ */
