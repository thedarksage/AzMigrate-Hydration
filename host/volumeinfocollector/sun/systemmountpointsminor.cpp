//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : systemmountpointsminor.cpp
// 
// Description: 
// 

#include <string>

#include "systemmountpoints.h"

void SystemMountPoints::AddHomeMountPoints()
{
    m_SystemMountPoints.insert(std::string("/export/home"));
    m_SystemMountPoints.insert(std::string("/home"));
}


void SystemMountPoints::SetDefaultBootMountPoint()
{
}


void SystemMountPoints::SetBootMountPointIfSo(const std::string &mpt)
{
}
