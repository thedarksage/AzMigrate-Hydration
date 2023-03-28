//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : customdevicecli.cpp
// 
// Description: command line tool customdevice used to allow customers to provide
//              custome device information
// 

#include <iostream>
#include <string>
#include <sstream>
#include <ace/OS_main.h>

#include "configurator2.h"
#include "logger.h"
#include "customdevices.h"

Configurator* TheConfigurator = 0; // singleton

void usage()
{
    std::cout << "\nusage: one of the following\n"
              << "  customdevice [-f <custom devices file name>]\n"
              << "  customdevice -a [-s <size>] [-m <mountpoint>] [-f <custom devices file name>] <device file name>\n"
              << "  customdevice -d [-f <custom devices file name>] <device file name>\n"
              << "  customdevice -h\n"
              << "\n"
              << "if no options (or only the -f option) specified, prints out the current\n"
              << "list of custom devices\n\n"
              << "  -f <custom devices file name>: use <custom devices file name> instead\n"
              << "     of the default one (optional)\n"
              << "  -a: add custom device information or update custom device information\n"
              << "      (if device file name exists in the custom devices list)\n"
              << "  -s <size>: device size (optional). Recommended if cx reports size of 0\n"
              << "  -m <mountpoint>: mount point for the custom device (optional)\n"
              << "  -d: delete custom device information\n"
              << "  -h: display this help\n"
              << "  <device file name>: device file name to delete (required)\n"
              << '\n';
}

bool getArgs(
    int argc, char* argv[],
    CustomDevices::CustomDevice & customDevice,
    bool& addDevice,
    bool& printDevices,
    std::string & customDevicesFileName)
{
    bool haveAdd = false;
    bool haveDelete = false;
    
    for (int i = 1; i < argc; ++i) {
        if ('-' == argv[i][0]) {
            switch (argv[i][1]) {
                case 'a':
                    if (haveDelete) {
                        std::cout << "ERROR: you can not add and delete at the same time\n";
                        usage();
                        return false;
                    }
                    haveAdd = true;
                    addDevice = true;
                    break;
                case 'd':
                    if (haveAdd) {
                        std::cout << "ERROR: you can not add and delete at the same time\n";
                        usage();
                        return false;
                    }
                    haveDelete = true;
                    addDevice = false;
                    break;
                case 'f':
                    ++i;
                    customDevicesFileName = argv[i];
                    break;
                case 'm':
                    ++i;
                    customDevice.m_Mount = argv[i];
                    break;
                case 's':
                    ++i;
                    {
                        // make sure it is a valid size
                        std::stringstream tmp;
                        unsigned long long size;
                        if ((tmp << argv[i]).fail() || !(tmp >> size).eof()) {
                            std::cout << "ERROR: size not valid number: " << argv[i] << '\n';
                            usage();
                            return false;
                        }

                        if (size > static_cast<unsigned long long>(std::numeric_limits<long long>::max())) {
                            std::cout << "ERROR: size (" << argv[i] << ") greater then max allowed size ("
                                      << std::numeric_limits<long long>::max() << ")\n";
                            usage();
                            return false;
                        }


                        customDevice.m_Size = static_cast<long long>(size); // safe as we checked that size was <= max long long
                    }
                    break;
                case 'h':
                    usage();
                    return false;
                    break;
                default:
                    usage();
                    return false;
            }
        } else {
            if (!customDevice.m_Name.empty()) {
                std::cout << "ERROR: you can only specifiy 1 device name\n";
                usage();
                return false;
            }
            customDevice.m_Name = argv[i];
        }
    }

    // make sure a valid set of options were specified 
    if (1 == argc || (3 == argc && !customDevicesFileName.empty())) {
        // OK, either no options or only -f just want to print the current custom devices
        printDevices = true;
    } else {
        printDevices = false;
        // must have at least -a or -d and a custom device name
        if (!(haveAdd || haveDelete) || customDevice.m_Name.empty()) {
            std::cout << "ERROR: missing arguments\n";
            usage();
            return false;
        }
    }

    return true;
}

int ACE_TMAIN(int argc, char* argv[])
{
    bool addDevice;
    bool printDevices;

    std::string customDevicesFileName;
    LocalConfigurator TheLocalConfigurator;
    std::string logFile = TheLocalConfigurator.getLogPathname() + "customdevicecli.log";
    SetLogFileName(logFile.c_str());

    CustomDevices customDevices(TheLocalConfigurator.getCacheDirectory());

    CustomDevices::CustomDevice customDevice;

    if (!getArgs(argc, argv, customDevice, addDevice, printDevices, customDevicesFileName)) {
        return -1;
    }

    customDevices.Load(customDevicesFileName);

    if (printDevices) {
         customDevices.Print();
         return 0;
    }
    
    if (addDevice) {
        if (!customDevices.AddCustomDevice(customDevice)) {
            return -1;
        
        }
    } else if (!customDevices.DeleteCustomDevice(customDevice)) {
        return -1;
    }

    return 0;
            
}
