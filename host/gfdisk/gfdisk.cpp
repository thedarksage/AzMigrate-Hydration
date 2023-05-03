//                                       
// Copyright (c) 2018 Microsoft.
// This file contains proprietary and confidential information and
// remains the unpublished property of Microsoft. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with Microsoft.
// 
// File       : gfdisk.cpp
// 
// Description: 
// 

#include <sys/mount.h>
#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include "logger.h"
#include "gpt.h"

using namespace std;

unsigned int getSectorSize(string const & deviceName)
{
    unsigned int sectorSize = 512;
    int fd = open(deviceName.c_str(), O_RDONLY);
    if (-1 == fd) {
        DebugPrintf(SV_LOG_ERROR,"Failed to open device %s for finding sector size.\n",deviceName.c_str());
        return 0;
    }

    if (ioctl(fd, BLKSSZGET, &sectorSize) < 0) {
        DebugPrintf(SV_LOG_ERROR,"ioctl BLKSSZGET returned %d.\n", errno);
        DebugPrintf(SV_LOG_ERROR,"Failed to find sector size for device %s.\n",deviceName.c_str());
    }

    close(fd);
    return sectorSize;
}

unsigned long long getVolumeSizeInBytes(string const & deviceName, unsigned int sectorSize)
{
    unsigned long long rawSize = 0;
    unsigned long nrsectors;

    int fd = open(deviceName.c_str(), O_RDONLY);
    if (-1 == fd) {
        DebugPrintf(SV_LOG_ERROR,"Failed to open device %s for finding raw size.\n",deviceName.c_str());
        return 0;
    }

    if (ioctl(fd, BLKGETSIZE, &nrsectors))
        nrsectors = 0;

    if (ioctl(fd, BLKGETSIZE64, &rawSize))
        rawSize = 0;

    if (rawSize == 0 || (rawSize == (unsigned long long)nrsectors))
        rawSize = ((unsigned long long) nrsectors) * ((unsigned long long)sectorSize);

    close(fd);
    return rawSize;
}

int main(int argc, char **argv)
{
    SetLogFileName("gfdiskinfo.log");

    if (argc != 2) {
        DebugPrintf(SV_LOG_ERROR, "Usage: ./gfdisk disk\n");
        return 2;
    }
 
    string deviceName = argv[1];
    struct stat64 pVolStat;

    if (-1 == stat64(deviceName.c_str(), &pVolStat)) {
        DebugPrintf(SV_LOG_ERROR, "Failed to stat the device %s\n", deviceName.c_str());
        return 1;
    }

    if (0 == pVolStat.st_rdev)
        return 1;

    unsigned int sectorSize = getSectorSize(deviceName);
    unsigned long long rawSize = getVolumeSizeInBytes(deviceName, sectorSize);

    mbr_area_t mbrArea;
    bool bgptdev = validateMBRArea(deviceName, &mbrArea);
    if (!bgptdev)
        return 1;

    gpt_hdr_t gptHdr;
    if (readGPTHeader(deviceName, &gptHdr, sectorSize, 1)) {
        DebugPrintf(SV_LOG_ERROR, "Failed to read the main GPT header for device %s\n", deviceName.c_str());
        if (readGPTHeader(deviceName, &gptHdr, sectorSize, (rawSize / sectorSize) - 1)) {
            DebugPrintf(SV_LOG_ERROR, "Failed to read the alternate GPT header for device %s\n", deviceName.c_str());
            return 1;
        }
    }

    gpt_part_entry_t *gptPartEntry;
    gptPartEntry = readGPTPartitionEntries(deviceName, &gptHdr, sectorSize);
    if (!gptPartEntry) {
        DebugPrintf(SV_LOG_ERROR, "Failed to read the GPT partition entries for device %s\n", deviceName.c_str());
        return 1;
    }

    for (int i = 0; i < gptHdr.numPartitions; i++) {
        if (!gptPartEntry[i].firstLBA)
            continue;

        cout << deviceName.c_str() << i + 1 << endl;
    }

    free(gptPartEntry);
    return 0;
}
