//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : systemmountpointsmajor.cpp
// 
// Description: 
//

#include <string>

#include "systemmountpoints.h"

SystemMountPoints::SystemMountPoints()
{
    m_SystemMountPoints.insert(std::string("/"));
    m_SystemMountPoints.insert(std::string("/boot"));
    m_SystemMountPoints.insert(std::string("/etc"));
    m_SystemMountPoints.insert(std::string("/var"));
    m_SystemMountPoints.insert(std::string("/tmp"));
    m_SystemMountPoints.insert(std::string("/usr"));
    AddHomeMountPoints();

    SetDefaultBootMountPoint();
}
