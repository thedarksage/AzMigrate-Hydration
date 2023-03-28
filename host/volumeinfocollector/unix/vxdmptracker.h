
//                                       
// File       : vxdmptracker.h
// 
// Description: tracking Hitachi Dynamic Link (HDLM) devices
// 


#ifndef _VXDMP__TRACKER__H_
#define _VXDMP__TRACKER__H_ 

#include <string>
#include <cstring>
#include "voldefs.h"

class VxDmpTracker {
public:

    bool IsVxDmpName(std::string const & name) const 
    {
        bool bisvxdmpname = false;
        if (!strncmp(name.c_str(), m_Dir.c_str(), strlen(m_Dir.c_str())))
        {
            bisvxdmpname = true;
        }
        return bisvxdmpname;
    }

    VxDmpTracker()
    {
        m_Dir = VXDMP_PATH;
    } 

private:
    std::string m_Dir;
};

#endif 
