//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : systemmountpoints.h
// 
// Description: 
// 

#ifndef SYSTEMMOUNTPOINTS_H
#define SYSTEMMOUNTPOINTS_H


#include <string>
#include <set>

class SystemMountPoints {
public:
    
    typedef std::set<std::string> SystemMountPoints_t;
    
    SystemMountPoints();

    bool IsSystemMountPoint(char const * mountPoint)
        {
            return (m_SystemMountPoints.end() != m_SystemMountPoints.find(mountPoint));
        }

    void SetBootMountPointIfSo(const std::string &mpt);

    bool IsBootMountPoint(const std::string &mpt)
        {
             return (mpt == m_BootMountPoint);
        }

protected:
    void AddHomeMountPoints();
    void SetDefaultBootMountPoint();

private:
    
    SystemMountPoints_t m_SystemMountPoints;

    std::string m_BootMountPoint;
};



#endif // ifndef SYSTEMMOUNTPOINTS_H
