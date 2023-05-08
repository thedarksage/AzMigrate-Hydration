//                                       
// Copyright (c) 2018 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : gpt.h
// 
// Description: 
// 

#ifndef GPT_H
#define GPT_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "inmsafeint.h"
#include "inmageex.h"

#define MBR_BOOT_SIGNATURE_BYTE_1 0x55
#define MBR_BOOT_SIGNATURE_BYTE_2 0xAA

#define GPT_HEADER_PARTITION_LBA  0x01
#define SECTOR_SIZE               512

#define GPT_HEADER_MIN_SIZE       92
#define GPT_HEADER_SIGNATURE      0x5452415020494645

#pragma pack(1)
typedef struct mbr_partition_entry
{
    uint8_t    status;
    uint8_t    startSector[3];
    uint8_t    partitionType;
    uint8_t    endSector[3];
    uint32_t   lbaStart;
    uint32_t   lbaSize;
} mbr_part_entry_t;

typedef struct mbr_area
{
    uint8_t          bootCode[440];
    uint32_t         diskSignature;
    uint16_t         unused;
    mbr_part_entry_t partitions[4];
    uint8_t          bootSignature[2];
} mbr_area_t;

typedef struct gpt_header
{
    uint64_t      signature;
    uint32_t      revision;
    uint32_t      headerSize;
    uint32_t      headerCRC32;
    uint32_t      reserved;
    uint64_t      currentLBA;
    uint64_t      backupLBA;
    uint64_t      firstUsableLBA;
    uint64_t      lastUsableLBA;
    unsigned char diskGUID[16];
    uint64_t      partitionEntriesStartingLBA;
    uint32_t      numPartitions;
    uint32_t      partitionEntrySize;
    uint32_t      partitionEntriesCRC32;
    unsigned char reserved2[SECTOR_SIZE - GPT_HEADER_MIN_SIZE];
} gpt_hdr_t;

typedef struct gpt_partition_entry
{
    uint8_t    partitionType[16];
    uint8_t    uniquePartitionGUID[16];
    uint64_t   firstLBA;
    uint64_t   lastLBA;
    uint64_t   attributeFlags;
    uint8_t    partitionName[72];
} gpt_part_entry_t;

#define CRC32_POLYNOMIAL 0xEDB88320L

uint32_t calculate_crc32(unsigned char *bytes, unsigned int numBytes)
{
    unsigned long crc32 = 0xFFFFFFFF;
    int i;
    int j;
    unsigned long temp;
    unsigned long temp_2;

    for (i = 0; i < numBytes; i++) {
        temp = ((crc32 >> 8) & 0x00FFFFFF);
        temp_2 = (crc32 ^ *bytes++) & 0xFF;
        for (j = 8; j > 0; j--)
            temp_2 = (temp_2 & 1) ? ((temp_2 >> 1) ^ CRC32_POLYNOMIAL) : (temp_2 >> 1);

        crc32 = temp ^ temp_2;
    }

    return (crc32 ^ 0xFFFFFFFF);
}

bool isLittleEndian()
{
    bool bLittleEndian = false;

    union {
        uint32_t      num;
        unsigned char bytes[sizeof(uint32_t)];
    } endian;

    endian.num = 1;
    if (endian.bytes[0] == 1)
        bLittleEndian = true;

    return bLittleEndian;
}

void reverseBytes(void *data, int numBytes)
{
    char temp;
    int i;

    for (i = 0; i < (numBytes / 2); i++) {
        temp = ((char *) data)[i];
        ((char *) data)[i] = ((char *) data)[numBytes - i - 1];
        ((char *) data)[numBytes - i - 1] = temp;
    }
}

void reverseGPTHeaderBytes(gpt_hdr_t *gptHdr)
{
    reverseBytes(&gptHdr->signature, 8);
    reverseBytes(&gptHdr->revision, 4);
    reverseBytes(&gptHdr->headerSize, 4);
    reverseBytes(&gptHdr->headerCRC32, 4);
    reverseBytes(&gptHdr->reserved, 4);
    reverseBytes(&gptHdr->currentLBA, 8);
    reverseBytes(&gptHdr->backupLBA, 8);
    reverseBytes(&gptHdr->firstUsableLBA, 8);
    reverseBytes(&gptHdr->lastUsableLBA, 8);
    reverseBytes(&gptHdr->partitionEntriesStartingLBA, 8);
    reverseBytes(&gptHdr->numPartitions, 4);
    reverseBytes(&gptHdr->partitionEntrySize, 4);
    reverseBytes(&gptHdr->partitionEntriesCRC32, 4);
    reverseBytes(&gptHdr->reserved2, SECTOR_SIZE - GPT_HEADER_MIN_SIZE);
}

void reverseGPTPartitionEntryBytes(gpt_hdr_t *gptHdr, gpt_part_entry_t *gptPartEntry)
{
    int i;

    for (i = 0; i < gptHdr->numPartitions; i++) {
        reverseBytes(&gptPartEntry[i].firstLBA, 8);
        reverseBytes(&gptPartEntry[i].lastLBA, 8);
    }
}

bool validateMBRArea(std::string const &deviceName, mbr_area_t *mbrArea)
{
    bool bgptdev = false;
    int retval;
    int fd;
    int i;

    fd = open(deviceName.c_str(), O_RDONLY);
    if (-1 == fd) {
        DebugPrintf(SV_LOG_ERROR,"Failed to open device %s for reading MBR area. Error is %d\n",
                    deviceName.c_str(), errno);
        goto out;
    }

    lseek(fd, 0, SEEK_SET);
    retval = read(fd, (void *)mbrArea, SECTOR_SIZE);
    if (SECTOR_SIZE != retval) {
        DebugPrintf(SV_LOG_ERROR,"Failed to read MBR area of device %s. Error is %d\n",
                    deviceName.c_str(), errno);
        goto close_fd;
    }

    if (mbrArea->bootSignature[0] != MBR_BOOT_SIGNATURE_BYTE_1 ||
            mbrArea->bootSignature[1] != MBR_BOOT_SIGNATURE_BYTE_2) {
        DebugPrintf(SV_LOG_DEBUG,"MBR boot signature mismatch for device %s\n", deviceName.c_str());
        goto close_fd;
    }

    retval = 0;
    for (i = 0; i < 4; i++) {
        if (0xEE == mbrArea->partitions[i].partitionType) {
            retval = 1;
            if (mbrArea->partitions[i].lbaStart != GPT_HEADER_PARTITION_LBA) {
                DebugPrintf(SV_LOG_ERROR,"Start LBA of GPT header is %u instead of 1 for device %s\n",
                            mbrArea->partitions[i].lbaStart, deviceName.c_str());
                goto close_fd;
            }

            break;
        }
    }

    if (retval)
        bgptdev = true;

close_fd:
    close(fd);

out:
    return bgptdev;
}

int checkGPTHeaderCrc(gpt_hdr_t *gptHdr, uint32_t *crc32)
{
    uint32_t crc32_backup;
    uint32_t hdrSize;

    crc32_backup = gptHdr->headerCRC32;
    gptHdr->headerCRC32 = 0;

    hdrSize = gptHdr->headerSize;

    if (!isLittleEndian())
        reverseGPTHeaderBytes(gptHdr);

    *crc32 = calculate_crc32((unsigned char*) gptHdr, hdrSize);

    if (!isLittleEndian())
        reverseGPTHeaderBytes(gptHdr);

    gptHdr->headerCRC32 = crc32_backup;

    return (*crc32 != crc32_backup);
}

int readGPTHeader(std::string const &deviceName, gpt_hdr_t *gptHdr, off_t sectorSize, int numSectors)
{
    int retval = 1;
    int fd;
    int bytes_read;
    off_t seek = ((off_t) numSectors) * sectorSize;
    uint32_t hdrSize;
    uint32_t crc32;

    fd = open(deviceName.c_str(), O_RDONLY);
    if (-1 == fd) {
        DebugPrintf(SV_LOG_ERROR,"Failed to open device %s for reading GPT header. Error is %d\n",
                    deviceName.c_str(), errno);
        goto out;
    }

    lseek(fd, seek, SEEK_SET);
    bytes_read = read(fd, (void *)gptHdr, sectorSize);
    if (sectorSize != bytes_read) {
        DebugPrintf(SV_LOG_ERROR,"Failed to read GPT header of device %s. Error is %d\n",
                    deviceName.c_str(), errno);
        goto close_fd;
    }

    if (!isLittleEndian())
        reverseGPTHeaderBytes(gptHdr);

    hdrSize = gptHdr->headerSize;
    if (hdrSize > sectorSize || hdrSize < GPT_HEADER_MIN_SIZE) {
        DebugPrintf(SV_LOG_ERROR,"Device = %s, Header Size = %u, Sector Size = %d, Minimum Header Size = %d\n",
                    deviceName.c_str(), hdrSize, sectorSize, GPT_HEADER_MIN_SIZE);
        goto close_fd;
    }

    if (checkGPTHeaderCrc(gptHdr, &crc32)) {
        DebugPrintf(SV_LOG_ERROR,"GPT header CRC32 mismatch, Actual = %u, Calculated = %u for device %s\n",
                    gptHdr->headerCRC32, crc32, deviceName.c_str());
        goto close_fd;
    }

    if (gptHdr->signature != GPT_HEADER_SIGNATURE) {
        DebugPrintf(SV_LOG_ERROR,"GPT header signature mismatch, actual signature = %x for device %s\n",
                    gptHdr->signature, deviceName.c_str());
        goto close_fd;
    }

    if (gptHdr->revision != 0x00010000) {
        DebugPrintf(SV_LOG_ERROR,"GPT header revision mismatch, actual revision = %x for device %s\n",
                    gptHdr->revision, deviceName.c_str());
        goto close_fd;
    }

    retval=0;

close_fd:
    close(fd);

out:
    return retval;
}

gpt_part_entry_t *readGPTPartitionEntries(std::string const &deviceName, gpt_hdr_t *gptHdr, off_t sectorSize)
{
    gpt_part_entry_t *gptPartEntry = NULL;
    int fd;
    int bytes_read;
    uint32_t partsSize;
    uint32_t crc32;
    int i;

    INM_SAFE_ARITHMETIC(partsSize = InmSafeInt<size_t>::Type(gptHdr->numPartitions) * gptHdr->partitionEntrySize,
                        INMAGE_EX(gptHdr->numPartitions) (gptHdr->partitionEntrySize))
    gptPartEntry = (gpt_part_entry_t *)calloc(1, partsSize);
    if (!gptPartEntry) {
        DebugPrintf(SV_LOG_ERROR,"Failed to allocate memory to read the partition entries for device %s\n",
                    deviceName.c_str(), errno);
        goto out;
    }

    fd = open(deviceName.c_str(), O_RDONLY);
    if (-1 == fd) {
        DebugPrintf(SV_LOG_ERROR,"Failed to open device %s for reading partition entries. Error is %d\n",
                    deviceName.c_str(), errno);
        goto out;
    }

    lseek(fd, gptHdr->partitionEntriesStartingLBA * sectorSize, SEEK_SET);
    bytes_read = read(fd, (void *)gptPartEntry, partsSize);
    if (partsSize != bytes_read) {
        DebugPrintf(SV_LOG_ERROR,"Failed to read GPT partition entries of device %s. Error is %d\n",
                    deviceName.c_str(), errno);
        goto close_fd;
    }

    crc32 = calculate_crc32((unsigned char*) gptPartEntry, partsSize);
    if (crc32 != gptHdr->partitionEntriesCRC32) {
        DebugPrintf(SV_LOG_ERROR,"GPT partition entries CRC32 mismatch for device %s, actual = %u and calculated = %u\n",
                    deviceName.c_str(), gptHdr->partitionEntriesCRC32, crc32);
        goto close_fd;
    }

    if (!isLittleEndian())
        reverseGPTPartitionEntryBytes(gptHdr, gptPartEntry);

    close(fd);
    return gptPartEntry;

close_fd:
    close(fd);

out:
    if (gptPartEntry) {
        free(gptPartEntry);
        gptPartEntry = NULL;
    }

    return gptPartEntry;
}

#endif // ifndef GPT_H
