
//                                       
// File       : devmapperdockertracker.h
// 
// Description: tracking devmapper storage driver reported docker devices
// 


#ifndef _DEV_MAPPER_DOCKER__TRACKER__H_
#define _DEV_MAPPER_DOCKER__TRACKER__H_ 

#include <string>
#include <cstring>
#include "voldefs.h"

class DevMapperDockerTracker {
public:

    bool IsDevMapperDockerName(std::string const & name) const 
    {
        bool bisdevmapperdockername = false;
        if (!strncmp(name.c_str(), m_devMapperDockerPattern.c_str(), strlen(m_devMapperDockerPattern.c_str())))
        {
            bisdevmapperdockername = true;
        }
        return bisdevmapperdockername;
    }

    DevMapperDockerTracker()
    {
        m_devMapperDockerPattern = DEV_MAPPER_DOCKER_PATTERN;
    } 

private:
    std::string m_devMapperDockerPattern;
};

#endif 
