
//                                       
// File       : vxdmptracker.h
// 
// Description: tracking Hitachi Dynamic Link (HDLM) devices
// 


#ifndef _DEVGLBL__TRACKER__H_
#define _DEVGLBL__TRACKER__H_ 

#include <string>
#include "voldefs.h"

class DevGlblTracker {
public:

    bool IsDevGlblName(std::string const & name) const ;

    DevGlblTracker()
    {
        m_Dir = DEVGLBL_PATH;
    } 

private:
    std::string m_Dir;
};

#endif 
