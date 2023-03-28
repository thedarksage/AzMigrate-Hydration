#ifndef _MPXIO__TRACKER__H_
#define _MPXIO__TRACKER__H_

#include <string>
#include <vector>
#include <iterator>
#include "utilfunctionsmajor.h"

class MpxioTracker {
public:
    bool IsMpxioName(std::string const & name);
    MpxioTracker();
private:
    void LoadMpxioDrvNames(void);
    std::vector<std::string> m_DrvNames;
};

#endif /* _MPXIO__TRACKER__H_ */
