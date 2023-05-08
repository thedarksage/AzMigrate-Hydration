//                                       
// Copyright (c) 2016 Microsoft Corp.
// This file contains proprietary and confidential information and
// remains the unpublished property of Microsoft. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with Microsoft.
// 
// File       : tempdevice.cpp
// 
// Description: 
//

#include <stdio.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <string.h>
#include <boost/algorithm/string.hpp>

#include "tempdevice.h"

const char VMBUS_DIR_KEYWORD[] = "/sys/bus/vmbus/devices";
const char DEVICE_KEYWORD[] = "vmbus_";
const char DEVICE_ID_KEYWORD[] = "device_id";
const char HOST_KEYWORD[] = "host";
const char TARGET_KEYWORD[] = "target";
const char BLOCK_KEYWORD[] = "block";
const char SCSIDISK_KEYWORD[] = "sd";
const char TEMP_DEVICE_ID[] = "{00000000-0001-";
const char PATH_SEPERATOR[] = "/";
const char DEVICE_DIR[] = "/dev/";


std::vector<std::string> FindAllDirEntries(const std::string &DirPath, const std::string &Pattern)
{
    DIR             *dip = 0;
    struct dirent   *dit = 0;
    std::vector<std::string> dirEntryPaths;

    if ((dip = opendir(DirPath.c_str())) != NULL)
    {
        while ((dit = readdir(dip)) != NULL)
        {
            if (!Pattern.length() || 
                (Pattern.length() && strncmp(dit->d_name, Pattern.c_str(), Pattern.length()) == 0))
            {
                std::string entryPath = DirPath;
                entryPath += PATH_SEPERATOR;
                entryPath += dit->d_name;
                dirEntryPaths.push_back(entryPath);
            }
        }
        closedir(dip);
    }

    return dirEntryPaths;
}

std::vector<std::string> FindAllDirEntries(const std::string &DirPath)
{
    return FindAllDirEntries(DirPath, std::string());
}

std::string FindDirEntry(std::string DirPath, std::string Pattern)
{
    std::string     dirEntry;
    DIR             *dip = 0;
    struct dirent   *dit = 0;

    if ((dip = opendir(DirPath.c_str())) != NULL)
    {
        while ((dit = readdir(dip)) != NULL)
        {
            if (strncmp(dit->d_name, Pattern.c_str(), Pattern.length()) == 0)
            {
                dirEntry = DirPath;
                dirEntry += PATH_SEPERATOR;
                dirEntry += dit->d_name;
                break;
            }
        }
        closedir(dip);
    }

    return dirEntry;
}

// Find the Azure VM temp or resource disk that is mounted on /mnt/resource
// 1. In the /sys/bus/vmbus/devices/ find the vmbus device with device_id 00000000-0001-
// 2. Get the corresponding sd* device from 
// either /sys/bus/vmbus/devices/host*/target*/<h:c:t:l>/block/sd*
// or /sys/bus/vmbus/devices/host*/target*/<h:c:t:l>/block:sd*
//
std::string GetAzureVmTempDevice()
{
    std::string sdDevicePath;

    std::vector<std::string> vmbus_devices = FindAllDirEntries(VMBUS_DIR_KEYWORD);
    std::vector<std::string>::iterator deviceIt = vmbus_devices.begin();

    for (;deviceIt != vmbus_devices.end(); deviceIt++)
    {
        std::string deviceIdFile = FindDirEntry((*deviceIt).c_str(), DEVICE_ID_KEYWORD);
        if (deviceIdFile.empty())
            continue;

        std::ifstream deviceIdFileStream(deviceIdFile.c_str(), std::ifstream::in);
        if (!deviceIdFileStream)
            continue;

        std::string deviceId;
        deviceIdFileStream >> deviceId;

        if (0 != strncmp(deviceId.c_str(), TEMP_DEVICE_ID, strlen(TEMP_DEVICE_ID)))
            continue;
        
        std::string hostDir = FindDirEntry((*deviceIt).c_str(), HOST_KEYWORD);
        if (hostDir.empty())
            continue;

        std::string targetDir = FindDirEntry(hostDir.c_str(), TARGET_KEYWORD);
        if (targetDir.empty())
            continue;

        std::string targetNum;
        std::size_t found = targetDir.find_last_of(PATH_SEPERATOR);
        if (found == std::string::npos)
            continue;

        targetNum = targetDir.substr(found+1+strlen(TARGET_KEYWORD));
        if (targetNum.empty())
            continue;

        std::string hctPattern = targetNum;

        std::string hctlDir = FindDirEntry(targetDir.c_str(), hctPattern.c_str());
        if (hctlDir.empty())
            continue;

        std::string blockDir = FindDirEntry(hctlDir.c_str(), BLOCK_KEYWORD);
        if (blockDir.empty())
            continue;

        found = blockDir.find_last_of(PATH_SEPERATOR);
        if (found == std::string::npos)
            continue;

        std::string blockEnt = blockDir.substr(found+1);

        std::vector<std::string> tokens;
        boost::split(tokens, blockEnt, boost::is_any_of(":"), boost::token_compress_on);
        if (tokens.size() == 2)
        {
            sdDevicePath = DEVICE_DIR;
            sdDevicePath += tokens[1];
        }
        else
        {
            std::string sdDir = FindDirEntry(blockDir.c_str(), SCSIDISK_KEYWORD);
            if (sdDir.empty())
                continue;
            
            found = sdDir.find_last_of(PATH_SEPERATOR);
            if (found == std::string::npos)
                continue;

            sdDevicePath = DEVICE_DIR;
            sdDevicePath += sdDir.substr(found+1);
        }

        break;
    }

    return sdDevicePath;
}


TempDevice::TempDevice()
{
    std::string tempDevice = GetAzureVmTempDevice();

    if (!tempDevice.empty())
        m_TempDevices.insert(tempDevice);
}
