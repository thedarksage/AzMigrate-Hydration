//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : test-volumeinfocollector.cpp
// 
// Description: 
// 

#include <boost/algorithm/string.hpp>
#include <iostream>
#include "volumeinfocollector.h"
#include "logger.h"
#include "configurator2.h"

Configurator* TheConfigurator = 0; // singleton

int main(int, char**)
{    
    try {
        SetLogFileName("testvolinfo.log");

        VolumeSummaries_t volumeSummaries;
        VolumeDynamicInfos_t volumeDynamicInfos;

        VolumeInfoCollector volumeCollector((DeviceNameTypes_t)GetDeviceNameTypeToReport(), true);
        volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, true);
        
    } catch (std::exception& e) {
        std::cout << "caught " << e.what() << '\n';
    } catch (...) {
        std::cout << "caught unknow exception\n";
    }

    return 0;
}

