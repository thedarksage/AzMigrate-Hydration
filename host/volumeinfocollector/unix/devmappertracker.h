
//                                       
// File       : DevMapperTracker.h
// 
// Description: tracking dev mapper devices
// 


#ifndef _DEVMAPPER__TRACKER__H_
#define _DEVMAPPER__TRACKER__H_ 

#include <string>
#include <cstring>
#include <voldefs.h>

/* DevMapperTracker class based on directory "/dev/mapper" */
class DevMapperTracker {
public:

    bool IsDevMapperName(std::string const & name) const 
    {
        bool bisdmname = false;
        /* assumes that on all unices, "/dev/mapper" is the path of dev mappers */ 
        if (0 == strncmp(name.c_str(), m_Dir.c_str(), strlen(m_Dir.c_str())))
        {
            bisdmname = true;
        }
        return bisdmname;
    }

    DevMapperTracker()
    {
        m_Dir = DEV_MAPPER_PATH;
    } 

private:
    std::string m_Dir;
};

#endif 
