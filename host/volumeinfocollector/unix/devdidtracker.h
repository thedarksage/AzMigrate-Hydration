
//                                       
// File       : vxdmptracker.h
// 
// Description: tracking Hitachi Dynamic Link (HDLM) devices
// 


#ifndef _DEVDID__TRACKER__H_
#define _DEVDID__TRACKER__H_ 

#include <string>
#include "voldefs.h"

class DevDidTracker {
public:

    bool IsDevDidName(std::string const & name) const;

    DevDidTracker()
    {
        m_Dir = DEVDID_PATH;
    } 

private:
    std::string m_Dir;
};

#endif /* */
