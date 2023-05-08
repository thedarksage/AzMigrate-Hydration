#ifndef _MIRROR__ERROR__H_
#define _MIRROR__ERROR__H_

#include <string>
#include <map>
#include "prismsettings.h"

class MirrorError
{
    typedef std::map<PRISM_VOLUME_INFO::MIRROR_ERROR, const char *> MirrorErrorToMsg_t;
public:
    MirrorError();
    std::string GetMirrorErrorMessage(const PRISM_VOLUME_INFO::MIRROR_ERROR mirrorerror);
private:
    MirrorErrorToMsg_t m_MirrorErrorToMsg;
};

#endif /* _MIRROR__ERROR__H_ */

