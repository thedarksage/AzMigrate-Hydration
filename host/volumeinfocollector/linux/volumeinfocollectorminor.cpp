//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : volumeinfocollectorminor.cpp
//
// Description:
//

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <string>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <functional>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <dirent.h>

#include "volumeinfocollectorinfo.h"
#include "volumeinfocollector.h"
#include "mountpointentry.h"
#include "executecommand.h"
#include "utilfdsafeclose.h"
#include "portablehelpersminor.h"
#include "portablehelpersmajor.h"
#include "diskpartition.h"
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "voldefs.h"
#include "devicemajorminordec.h"
#include "diskpartition.h"
#include "gpt.h"

bool VolumeInfoCollector::GetNextFullDeviceEntry(std::stringstream & results, std::string & field1, std::string & field2, std::string const & diskTok)
{
    char line[512];

    do {
        field1.clear();
        field2.clear();
        results.getline(line, sizeof(line));
        results >> field1 >> field2;
    } while (!results.eof() && results.good() && (field1 != diskTok || field2.empty()));

    return !results.eof() && results.good();
}

bool VolumeInfoCollector::GetDiskVolumeInfos(const bool &bneedonlynatives, const bool &bnoexclusions)
{
    bool bgotosnsdiskvolumeinfos = GetOSNameSpaceDiskInfos(bnoexclusions);

    bool bgotdmdiskvolumeinfos = true;
    bool bgotvxdmpdiskvolumeinfos = true;

    if (!bneedonlynatives)
    {
        bgotdmdiskvolumeinfos = GetDmDiskInfos();
        bgotvxdmpdiskvolumeinfos = GetVxDMPDiskInfos();
    }
 
    bool bretval = (bgotosnsdiskvolumeinfos && bgotdmdiskvolumeinfos && bgotvxdmpdiskvolumeinfos);
    return bretval;
}


bool VolumeInfoCollector::GetVolumeStartingSector(const char * Name, SV_ULONGLONG & StartingSector)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int fd;
    fd = open(Name, O_RDONLY);
    if (fd < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file descriptor for device(%s). Error code: %d\n", Name, errno);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    struct hd_geometry geom;
    if (ioctl(fd, HDIO_GETGEO, &geom) < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"IOCTL(HDIO_GETGEO) call failed for device(%s). Error code: %d\n", Name, errno);
        close(fd);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    close(fd);
    StartingSector = geom.start;
    DebugPrintf(SV_LOG_DEBUG,"device(%s) - Starting Sector = %lld\n", Name, StartingSector);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


bool VolumeInfoCollector::GetOSNameSpaceDiskInfos(const bool &bnoexclusions)
{
    // uses fdisk and grep to get disks assumes output similar to one of these
    //
    // Disk /dev/sda: 26.8 GB, 26843545600 bytes
    // /dev/sda1   *          63      401624      200781   83  Linux
    // /dev/sda2          401625    43407629    21503002+  8e  Linux LVM
    // /dev/sda3        43407630    47600594     2096482+  82  Linux swap / Solaris
    // /dev/sda4        47600595    52420094     2409750   83  Linux
    // Disk /dev/sdb: 19.3 GB, 19327352832 bytes

    //Disk /dev/sdb doesn't contain a valid partition table

    //
    // or (the difference is that the size of the full device is not directly specified)
    //
    // Disk /dev/sda: 255 heads, 63 sectors/track, 9726 cylinders
    // /dev/sda1   *           1        9725    78116031    7  HPFS/NTFS
    // /dev/sdb1   *           1         284     2281198+   7  HPFS/NTFS
    // /dev/sdb2             285         415     1052257+  82  Linux swap
    // /dev/sdb3             416        9725    74782575   83  Linux
    //
    // or even (note the extra space and text between the device name and the ':')
    // 
    // Disk /dev/sde1 (Sun disk label): 128 heads, 128 sectors, 1278 cylinders
    // /dev/sde1p1             0        16    131072    2  SunOS root
    // /dev/sde1p2  u         16        32    131072    3  SunOS swap
    // /dev/sde1p3  u          0      1278  10469376    5  Whole disk
    // /dev/sde1p7            32      1278  10207232    4  SunOS usr
    // 

    std::string cmd = m_localConfigurator.getLinuxDiskDiscoveryCommand();
    if (cmd.empty()) {
        DebugPrintf(SV_LOG_ERROR,"%s: disk discovery command is empty.\n",FUNCTION_NAME);
        return false;
    }

    std::stringstream results;

    if (!executePipe(cmd, results) || results.str().empty()) {
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"%s: output \n%s\n", FUNCTION_NAME, results.str().c_str());

    char const * const swapTok = "swap";

    char line[512];

    std::string const diskTok("Disk");
    PartitionNegAttrs_t partitionNegAttrs;
    partitionNegAttrs.push_back("Unknown");
    partitionNegAttrs.push_back("Whole disk");
    
    std::string field1;         // generic name because it can have different data based on the line
    std::string field2;         // generic name because it can have different data based on the line
    std::string fullDeviceName;
    std::string extendedPartitionName;
    std::string blocks;
    std::string gptPartDev;

    off_t startSector;
    off_t endSector;
    off_t extendedStartSector = 0;
    off_t extendedEndSector = 0;

    off_t sectorCount = 0;
    off_t sectorSize = 0;
    off_t cylinderCount = 0;
    off_t totalSectors = 0;
    off_t blockCount = 0;

    int partitionType = 0;

    results >> field1 >> field2;

    do {
        if (field1 != diskTok || field2.empty()) {
            // hmm... perhaps some extra output that shows up under certain conditions
            if (!GetNextFullDeviceEntry(results, field1, field2, diskTok)) {
                break;
            }
        }

        // either reading a line that looks like
        //   Disk /dev/sda:
        // or
        //   Disk /dev/sde1 (Sun disk label): 
        if (':' == field2[field2.length() - 1]) {
            // have -  Disk /dev/sda:
            // remove the ':' from the device name
            field2.erase(field2.length() - 1);
        } else {
            // have -  Disk /dev/sde1 (Sun disk label): 128 heads, 128 sectors, 1278 cylinders
            // skip up to and including the ':'
            char ch;
            do {
                results >> ch;
            } while (!results.eof() && results.good() && ':' != ch);

            if (results.eof() || !results.good()) {
                return results.good();
            }
        }

        bool bcollectdev = true;

        if (IsDeviceValidLv(field2))
        {
            DebugPrintf(SV_LOG_DEBUG, "disk %s already collected as valid logical volume\n", field2.c_str());
            bcollectdev = false;
        }
        
        /* fdisks list whole disk partitions as 
         * disks again; we dont collect whole disk partitions
        if (IsLinuxPartition(field2))
        {
            bcollectdev = false;
        } */

        struct stat64 volStat;
        if (bcollectdev)
        {
            if ((0 == stat64(field2.c_str(), &volStat)) && volStat.st_rdev)
            {
                bcollectdev = IsDeviceTracked(major(volStat.st_rdev), field2.c_str());
            }
            else 
            {
                bcollectdev = false;
            }
        }

        /* We do not want to search back; since if partitions are
         * displayed as disks, we filter them above;
         * TODO: We need to make sure that in custom devices, 
         *       we should never give already collected device. For 
         *       this reason only, we do not want to search back
        if (bcollectdev && 
            IsDeviceTracked(major(volStat.st_rdev), field2.c_str()))
        {
            bcollectdev = (!IsVolInfoPresent(field2));
        }
        */
       
        if (!bcollectdev)
        {
            if (!GetNextFullDeviceEntry(results, field1, field2, diskTok)) {
                break;
            }
            continue;
        }

        fullDeviceName = field2;

        // read rest of the line
        results.getline(line, sizeof(line));

        std::string realName;
        VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
        GetSymbolicLinksAndRealName(fullDeviceName, symbolicLinks, realName);
        VolumeInfoCollectorInfo dVolInfo;

        dVolInfo.m_DeviceName = fullDeviceName;
        dVolInfo.m_DeviceID = m_DeviceIDInformer.GetID(dVolInfo.m_DeviceName);
        if (dVolInfo.m_DeviceID.empty())
        {
            DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", dVolInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
        }

        dVolInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(dVolInfo.m_DeviceName);
        dVolInfo.m_RealName = realName;
        dVolInfo.m_DeviceType = VolumeSummary::DISK;

        m_DeviceTracker.UpdateVendorType(dVolInfo.m_DeviceName, dVolInfo.m_DeviceVendor, dVolInfo.m_IsMultipath);

        if (dVolInfo.m_DeviceVendor == VolumeSummary::DEVMAPPERDOCKER)
        {
            dVolInfo.m_DeviceType = VolumeSummary::DOCKER_DISK;
        }

        dVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(dVolInfo.m_DeviceName);

        sectorSize = GetSectorSize(fullDeviceName);
        dVolInfo.m_SectorSize = sectorSize;
        dVolInfo.m_RawSize = GetVolumeSizeInBytes(fullDeviceName, 0, sectorSize);
        dVolInfo.m_LunCapacity = dVolInfo.m_RawSize;

        dVolInfo.m_Locked = IsDeviceLocked(fullDeviceName);
        dVolInfo.m_SymbolicLinks = symbolicLinks;

        dVolInfo.m_Major = major(volStat.st_rdev);
        dVolInfo.m_Minor = minor(volStat.st_rdev);

        dVolInfo.m_BekVolume = IsBekVolume(fullDeviceName);

        UpdateMountDetails(dVolInfo);
        UpdateSwapDetails(dVolInfo);

        /* TODO: dm disks will have these as inside devts; CX
           should not have worry disks/partitions (dm and native), 
           having same VG. One more way is to not copy VG if dm is 
           disk or partition, but not needed now */
        UpdateLvInsideDevts(dVolInfo);
        dVolInfo.m_Attributes = GetAttributes(dVolInfo.m_DeviceName);

        if (IsIscsiDisk(dVolInfo))
            dVolInfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::IS_ISCSI_DISK, STRBOOL(true)));

        if (IsNvmeDisk(dVolInfo.m_DeviceName))
            dVolInfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::IS_NVME_DISK, STRBOOL(true)));

        if (!dVolInfo.m_DeviceID.empty())
            dVolInfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_UUID, dVolInfo.m_DeviceID));

        mbr_area_t mbrArea;
        bool bgptdev = validateMBRArea(dVolInfo.m_DeviceName, &mbrArea);
        if (!bgptdev)
            goto check_for_partitions;

        gpt_hdr_t gptHdr;
        if (readGPTHeader(dVolInfo.m_DeviceName, &gptHdr, sectorSize, 1)) {
            DebugPrintf(SV_LOG_ERROR, "Failed to read the main GPT header for device %s\n", dVolInfo.m_DeviceName.c_str());
            if (readGPTHeader(dVolInfo.m_DeviceName, &gptHdr, sectorSize, (dVolInfo.m_RawSize / sectorSize) - 1)) {
                DebugPrintf(SV_LOG_ERROR, "Failed to read the alternate GPT header for device %s\n", dVolInfo.m_DeviceName.c_str());
                goto check_for_partitions;
            }
        }

        gpt_part_entry_t *gptPartEntry;
        gptPartEntry = readGPTPartitionEntries(dVolInfo.m_DeviceName, &gptHdr, sectorSize);
        if (!gptPartEntry) {
            DebugPrintf(SV_LOG_ERROR, "Failed to read the GPT partition entries for device %s\n", dVolInfo.m_DeviceName.c_str());
            goto check_for_partitions;
        }

        for (int i = 0; i < gptHdr.numPartitions; i++) {
            gptPartDev.clear();

            if (!gptPartEntry[i].firstLBA)
                continue;

            gptPartDev = dVolInfo.m_DeviceName + boost::lexical_cast<std::string>(i + 1);

            struct stat64 pVolStat;
            if (-1 == stat64(gptPartDev.c_str(), &pVolStat)) {
                DebugPrintf(SV_LOG_ERROR, "Failed to stat the device %s\n", dVolInfo.m_DeviceName.c_str());
                continue;
            }

            if (0 == pVolStat.st_rdev)
                continue;

            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(gptPartDev, symbolicLinks, realName);
            VolumeInfoCollectorInfo pVolInfo;

            pVolInfo.m_DeviceName = gptPartDev;
            pVolInfo.m_DeviceID = GetPartitionID(dVolInfo.m_DeviceID, pVolInfo.m_DeviceName);
            pVolInfo.m_WCState = dVolInfo.m_WCState;
            pVolInfo.m_RealName = realName;
            pVolInfo.m_DeviceType = VolumeSummary::PARTITION;
            pVolInfo.m_DeviceVendor = dVolInfo.m_DeviceVendor;
            pVolInfo.m_IsMultipath = dVolInfo.m_IsMultipath;
            pVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(pVolInfo.m_DeviceName);

            blockCount = gptPartEntry[i].lastLBA - gptPartEntry[i].firstLBA + 1;
            pVolInfo.m_RawSize = GetVolumeSizeInBytes(gptPartDev, blockCount * sectorSize, sectorSize);
            pVolInfo.m_LunCapacity = pVolInfo.m_RawSize;
            pVolInfo.m_Locked = IsDeviceLocked(gptPartDev);
            pVolInfo.m_SymbolicLinks = symbolicLinks;
            pVolInfo.m_Major = major(pVolStat.st_rdev);
            pVolInfo.m_Minor = minor(pVolStat.st_rdev);
            pVolInfo.m_SystemVolume = false;
            pVolInfo.m_IsStartingAtBlockZero = false;

            pVolInfo.m_PhysicalOffset = bnoexclusions ? (gptPartEntry[i].firstLBA * sectorSize) : 0;
            pVolInfo.m_SectorSize = bnoexclusions ? sectorSize : 0;

            pVolInfo.m_BekVolume = IsBekVolume(pVolInfo.m_DeviceName);

            UpdateMountDetails(pVolInfo);
            UpdateSwapDetails(pVolInfo);
            UpdateLvInsideDevts(pVolInfo);

            InsertVolumeInfoChild(dVolInfo, pVolInfo);
        }

        free(gptPartEntry);

check_for_partitions:
        // check for partitions
        while (!results.eof()) {
            // now have one of the following
            //   Disk <devicename>
            // or
            //   /dev/sdb1   *           1         284     2281198+   7  HPFS/NTFS
            // or
            //   /dev/sdb2             285         415     1052257+  82  Linux swap
            field1.clear();
            field2.clear();
            results >> field1 >> field2;

            if (bgptdev || field1.empty() || field1 == diskTok) {
                // done with this device and its partitions
                break;
            }

            blocks.clear();
            startSector = 0;
            endSector = 0;
            partitionType = 0;
            std::string fileSystemType ;

            try {
                // TODO: need a better way to determine if field2 is the start sector or
                // a boot/flag value. this will work as long as a boot/flag value is not
                // a number so far have only seen "blank" "*" and "u"
                startSector = boost::lexical_cast<off_t>(field2);
                // if no exception then field2 has the start sector so had something like
                //   /dev/sdb2             285         415     1052257+  82  Linux swap
                // already read the first 2 fields. need next 3 fields
                results >> endSector >> blocks;
            } catch (boost::bad_lexical_cast& e) {
                // the second field was not the start sector so had something like
                //   /dev/sdb1   *           1         284     2281198+   7  HPFS/NTFS
                // or
                // /dev/sdb8p3  u          0       544  10444800    5  Whole disk
                // already read the first 2 fields. need next 4 fields to get the blocks
                results >> startSector >> endSector >> blocks;
            }
 
            // handle the new output format of the sfdisk where the partiton size is also
            // included in the output
            std::string strPartitionType;
            results >> strPartitionType;

            DebugPrintf(SV_LOG_DEBUG,"partition type is : %s\n", strPartitionType.c_str());

            if (strPartitionType.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos)
            {
                std::stringstream ss;
                ss << strPartitionType;
                ss >> std::hex >> partitionType >> std::dec;
                //   /dev/sdb2             285         415     1052257+  82  Linux swap
                //   or
                //   /dev/sdb2   *         285         415     1052257+  82  Linux swap
                results >> fileSystemType;
                DebugPrintf(SV_LOG_DEBUG,"partition type : %x , filesystem : %s\n",partitionType, fileSystemType.c_str());
            } else {
                //   /dev/sdb2             285         415     1052257+ 10G  82  Linux swap
                //   or
                //   /dev/sdb2   *         285         415     1052257+ 10G 82  Linux swap
                // the field read was a partition size, ignore it and read the partition type
                // already read the first 2 fields. need next 4 fields to get the blocks
                // partitionType is actually a hex value (read as hex and set back to dec)
                results >> std::hex >> partitionType >> std::dec >> fileSystemType;
                DebugPrintf(SV_LOG_DEBUG," partition type : %x , filesystem : %s\n",partitionType, fileSystemType.c_str());
            }
                           
            // read rest of the line from partition line
            results.getline(line, sizeof(line));

            // convert blocks to blockCount
            blockCount = 0;
            try {
                // need to remove trailing + or - if it exits else the lexical cast will fail
                if ('+' == blocks[blocks.length() -1] || '-' == blocks[blocks.length() -1]) {
                    blocks.erase(blocks.length() -1);
                }
                blockCount = boost::lexical_cast<off_t>(blocks);
            } catch (boost::bad_lexical_cast& e) {
                // TODO: need to report this
            }

            /* a partition should be reported as extended if unknown or   
             * whole disk is there; 
             * dev mapper multipath seems to be not 
             * creating partition entries for partitions 
             * which are unknown or whole disk; Hence we wont be
             * doing anything for dev mapper disks for now
             * But vxdmp is creating all partition entries;
             * Hence from dm and vxdmp, we should match ID back to
             * native device and get a list of reported partition 
             * numbers and for dev mapper, add <dmdisk>p<partition number>
             * , for vxdmp read the next entry in directory and know if it is
             * ending with p1, s1 or 1 since 1 to 15 entries are always present 
             * and from there on we know what to add at
             * last(s or p or nothing )
             * 
             * [root@imits216 ~]# fdisk -l /dev/sdd
             *
             * Disk /dev/sdd (Sun disk label): 128 heads, 32 sectors, 2558 cylinders
             * Units = cylinders of 4096 * 512 bytes
             *
             * Device Flag    Start       End    Blocks   Id  System
             * /dev/sdd3  u          0      2558   5238784    5  Whole disk
             * /dev/sdd8  u          0      2558   5238784    f  Unknown
             */


            struct stat64 pVolStat;

            if (-1 == stat64(field1.c_str(), &pVolStat)) {
                continue;
            }

            if (0 == pVolStat.st_rdev)
            {
                continue;
            }

            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(field1, symbolicLinks, realName);
            VolumeInfoCollectorInfo pVolInfo;

            pVolInfo.m_DeviceName = field1;
            pVolInfo.m_DeviceID = GetPartitionID(dVolInfo.m_DeviceID, pVolInfo.m_DeviceName);
            pVolInfo.m_WCState = dVolInfo.m_WCState;
            pVolInfo.m_RealName = realName;
            /* We try to report partitions that should not be collected,
             * as extended since from dev mapper and vxdmp, we may not 
             * know whether to collect or not; */
            pVolInfo.m_DeviceType = ShouldCollectPartition(line, partitionNegAttrs)?VolumeSummary::PARTITION:
                                    VolumeSummary::EXTENDED_PARTITION;
            pVolInfo.m_DeviceVendor = dVolInfo.m_DeviceVendor;
            pVolInfo.m_IsMultipath = dVolInfo.m_IsMultipath;
            pVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(pVolInfo.m_DeviceName);
            pVolInfo.m_RawSize = GetVolumeSizeInBytes(field1, blockCount * ONE_KB, sectorSize);
            pVolInfo.m_LunCapacity = pVolInfo.m_RawSize;
            pVolInfo.m_Locked = IsDeviceLocked(field1);
            pVolInfo.m_SymbolicLinks = symbolicLinks;
            pVolInfo.m_Major = major(pVolStat.st_rdev);
            pVolInfo.m_Minor = minor(pVolStat.st_rdev);
            pVolInfo.m_SystemVolume = (0 != strstr(line, swapTok));
            pVolInfo.m_IsStartingAtBlockZero = (0 == startSector);
            if(  strstr( fileSystemType.c_str(), "Linux") != NULL ) 
            {
                pVolInfo.m_PhysicalOffset = bnoexclusions ? (startSector * sectorSize) : 0;
                pVolInfo.m_SectorSize = bnoexclusions ? sectorSize : 0;
            }
            else
            {
                GetVolumeStartingSector( pVolInfo.m_DeviceName.c_str(), pVolInfo.m_PhysicalOffset ) ;
                pVolInfo.m_SectorSize = sectorSize ;
				pVolInfo.m_PhysicalOffset *= sectorSize ;
            }            

            pVolInfo.m_BekVolume = IsBekVolume(pVolInfo.m_DeviceName);

            UpdateMountDetails(pVolInfo);
            UpdateSwapDetails(pVolInfo);
            UpdateLvInsideDevts(pVolInfo);

            // if we already found an extended partition, need to check if this is part of that extended partition
            if (!extendedPartitionName.empty() && extendedStartSector <= startSector && extendedEndSector >= endSector) {
                pVolInfo.m_ExtendedPartitionName = extendedPartitionName;
            }

            bool bisextendedpartition = IsExtendedPartition(partitionType);
            // check if this is an extended partition
            if (bisextendedpartition) {
                extendedPartitionName = field1;
                pVolInfo.m_DeviceType = VolumeSummary::EXTENDED_PARTITION;
                extendedStartSector = startSector;
                extendedEndSector = endSector;
            }

            InsertVolumeInfoChild(dVolInfo, pVolInfo);
        }

        UpdateAttrsWithChildren(dVolInfo);
        InsertVolumeInfo(dVolInfo);
        // reset
        extendedStartSector = 0;
        extendedEndSector = 0;
        fullDeviceName.clear();
        extendedPartitionName.clear();

    } while (!results.eof() && results.good());

    return results.good();
}


bool VolumeInfoCollector::IsEncryptedDisk(std::string const & deviceName)
{
    std::string cmd;
    std::string out;
    std::stringstream results;

    cmd = "dmsetup info ";
    cmd += deviceName;
    cmd += " | grep \"UUID: CRYPT-LUKS\"";

    if (!executePipe(cmd, results) || results.str().empty())
        return false;

    DebugPrintf(SV_LOG_DEBUG, "Found an encrypted device %s\n", deviceName.c_str());
    return true;
}


bool VolumeInfoCollector::IsBekVolume(std::string const & deviceName)
{
    std::string cmd;
    std::string out;
    std::stringstream results;

    cmd = "blkid ";
    cmd += deviceName;
    cmd += " | awk -F'LABEL=' '{print $2}' | awk -F'\"' '{print $2}'";

    if (!executePipe(cmd, results) || results.str().empty())
        return false;

    out = results.str();
    boost::trim(out);
    if (boost::equals(out, "BEK VOLUME"))
    {
        DebugPrintf(SV_LOG_DEBUG, "Found BEK Volume %s\n", deviceName.c_str());
        return true;
    }

    return false;
}


bool VolumeInfoCollector::IsIscsiDisk(VolumeInfoCollectorInfo &volInfo)
{
   std::string scsiBus = volInfo.GetAttribute(volInfo.m_Attributes, NsVolumeAttributes::SCSI_BUS, false);
   std::string iscsiHostsPath = "/sys/class/iscsi_host/";
   std::string iscsiHost = "host" + scsiBus;
   bool bIscsiDisk = false;

   DIR *dir = opendir(iscsiHostsPath.c_str());
   if (!dir)
       return bIscsiDisk;

   struct dirent *entry;
   while (NULL != (entry = readdir(dir)))
   {
       if (boost::equals(entry->d_name, iscsiHost))
       {
           bIscsiDisk = true;
           break;
       }
   }

   if (dir)
       closedir(dir);

   return bIscsiDisk;
}

bool VolumeInfoCollector::IsNvmeDisk(std::string const & deviceName)
{
    std::string cmd;
    std::string out;
    std::stringstream results;

    cmd = "cat /sys/block/";
    cmd += deviceName;
    cmd += "/device/model";

    if (!executePipe(cmd, results) || results.str().empty()) {
        goto device_name_check;
    }

    out = results.str();
    boost::trim(out);
    if (boost::equals(out, "Microsoft NVMe Direct Disk"))
    {
        DebugPrintf(SV_LOG_DEBUG, "Found Microsoft NVMe Direct Disk %s\n", deviceName.c_str());
        return true;
    }

device_name_check:
    if (boost::starts_with(deviceName, "/dev/nvme"))
    {
        DebugPrintf(SV_LOG_DEBUG, "Found NVMe disk %s\n", deviceName.c_str());
        return true;
    }

    return false;
}

bool VolumeInfoCollector::GetDmDiskInfos(void)
{
    bool bgotinfos = true;

    LvsIter begin = m_Lvs.begin();
    LvsIter end = m_Lvs.end();
    
    do
    {
        LvsIter iter = find_if(begin, end, IsDmDiskToCollect);
        if (end != iter)
        {
            /* DebugPrintf(SV_LOG_DEBUG, "check dm disk %s tracked or not\n", iter->second.m_Name.c_str()); */
            /* we want to directly track these but call is needed to filter removables like cdroms etc,. */
            if (IsDeviceTracked(major(iter->second.m_Devt), iter->second.m_Name))
            {
                /* DebugPrintf(SV_LOG_DEBUG, "collecting dm disk %s\n", iter->second.m_Name.c_str()); */
                GetDmDiskInfo(*iter);
            }
            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);

    return bgotinfos;
}
  

bool VolumeInfoCollector::GetDmDiskInfo(Lv_pair_t &dmdiskpair)
{
    bool bgotinfo = true;
    Lv_t &dmdisk = dmdiskpair.second;

    std::string realName;
    VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
    GetSymbolicLinksAndRealName(dmdisk.m_Name, symbolicLinks, realName);

    VolumeInfoCollectorInfo dVolInfo;
    dVolInfo.m_DeviceName = dmdisk.m_Name;
    dVolInfo.m_DeviceID = m_DeviceIDInformer.GetID(dVolInfo.m_DeviceName);
    if (dVolInfo.m_DeviceID.empty())
    {
        DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", dVolInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
    }
    dVolInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(dVolInfo.m_DeviceName);
    dVolInfo.m_RealName = realName;
    dVolInfo.m_DeviceType = VolumeSummary::DISK;
    m_DeviceTracker.UpdateVendorType(dVolInfo.m_DeviceName, dVolInfo.m_DeviceVendor, dVolInfo.m_IsMultipath);
    /* UpdateVendorType will not set m_IsMultipath for dm since it may or may not be */
    dVolInfo.m_IsMultipath = (dmdisk.m_Type == DMMULTIPATHDISKTYPE);
    dVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(dVolInfo.m_DeviceName);
    dVolInfo.m_RawSize = dmdisk.m_Size;
    dVolInfo.m_LunCapacity = dVolInfo.m_RawSize;
    dVolInfo.m_Locked = IsDeviceLocked(dVolInfo.m_DeviceName);
    dVolInfo.m_SymbolicLinks = symbolicLinks;
    dVolInfo.m_Major = major(dmdisk.m_Devt);
    dVolInfo.m_Minor = minor(dmdisk.m_Devt);
    UpdateMountDetails(dVolInfo);
    UpdateSwapDetails(dVolInfo);
    UpdateLvInsideDevts(dVolInfo);
    dVolInfo.m_Attributes = GetAttributes(dVolInfo.m_DeviceName);

    LvsIter begin = m_Lvs.begin();
    LvsIter end = m_Lvs.end();
    do
    {
        LvsIter iter = find_if(begin, end, LvEqPartitionToCollect(&dmdisk));
        if (end != iter)
        {
            Lv_t &dmpart = iter->second;
            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(dmpart.m_Name, symbolicLinks, realName);
            VolumeInfoCollectorInfo pVolInfo;
    
            pVolInfo.m_DeviceName = dmpart.m_Name;
            pVolInfo.m_DeviceID = GetPartitionID(dVolInfo.m_DeviceID, pVolInfo.m_DeviceName);
            pVolInfo.m_WCState = dVolInfo.m_WCState;
            pVolInfo.m_RealName = realName;
            pVolInfo.m_DeviceType = VolumeSummary::PARTITION;
            pVolInfo.m_DeviceVendor = dVolInfo.m_DeviceVendor;
            pVolInfo.m_IsMultipath = dVolInfo.m_IsMultipath;
            pVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(pVolInfo.m_DeviceName);
            pVolInfo.m_RawSize = dmpart.m_Size;
            pVolInfo.m_LunCapacity = pVolInfo.m_RawSize;
            pVolInfo.m_Locked = IsDeviceLocked(pVolInfo.m_DeviceName);
            pVolInfo.m_SymbolicLinks = symbolicLinks;
            pVolInfo.m_Major = major(dmpart.m_Devt);
            pVolInfo.m_Minor = minor(dmpart.m_Devt);
            UpdateMountDetails(pVolInfo);
            UpdateSwapDetails(pVolInfo);
            UpdateLvInsideDevts(pVolInfo);
              
            dmpart.m_Collected = true;
            InsertVolumeInfoChild(dVolInfo, pVolInfo);
            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);

    dmdisk.m_Collected = true;
    UpdateAttrsWithChildren(dVolInfo);
    InsertVolumeInfo(dVolInfo);

    return bgotinfo;
}

unsigned int VolumeInfoCollector::GetSectorSize(std::string const & deviceName) const
{
    int fd = open(deviceName.c_str(), O_RDONLY);

    if (-1 == fd) {
        DebugPrintf(SV_LOG_ERROR,"Failed to open device %s for finding sector size.\n",deviceName.c_str());
        return 0;
    }

    boost::shared_ptr<void> fdGuard(static_cast<void*>(0), boost::bind(fdSafeClose, fd));

    return GetSectorSize(fd, deviceName);
}


unsigned int VolumeInfoCollector::GetSectorSize(int fd, std::string const & deviceName) const
{
    unsigned int sectorSize = 512;
    if (ioctl(fd, BLKSSZGET, &sectorSize) < 0)
    {
        DebugPrintf(SV_LOG_ERROR,"ioctl BLKSSZGET returned %d.\n", errno);
        DebugPrintf(SV_LOG_ERROR,"Failed to find sector size for device %s.\n",deviceName.c_str());
    }
    return sectorSize;
}


off_t VolumeInfoCollector::GetVolumeSizeInBytes(int fd, std::string const & deviceName)
{
    unsigned long long rawSize = 0;
    unsigned long nrsectors;

    // This returns number of sectors of 512 byte
    // But the actual sector size may be different from 512 bytes
    if (ioctl(fd, BLKGETSIZE, &nrsectors))
        nrsectors = 0;

    //Get rawsize. This ioctl is may not be supported on all linux versions
    if (ioctl(fd, BLKGETSIZE64, &rawSize))
        rawSize = 0;

    /*
    * If BLKGETSIZE64 was unknown or broken, use longsectors.
    * (Kernels 2.4.15-2.4.17 had a broken BLKGETSIZE64
    * that returns sectors instead of bytes.)
    */

    if (rawSize == 0 || rawSize == nrsectors)
        rawSize = ((unsigned long long) nrsectors) * 512;

    return rawSize;
}

off_t VolumeInfoCollector::GetVolumeSizeInBytes(
    std::string const & deviceName,
    off_t offset,
    off_t sectorSize)
{
    int fd = open(deviceName.c_str(), O_RDONLY);

    if (-1 == fd) {
        return 0;
    }

    boost::shared_ptr<void> fdGuard(static_cast<void*>(0), boost::bind(fdSafeClose, fd));

    // first see if we can do it the easy way
    off_t sizeBytes = GetVolumeSizeInBytes(fd, deviceName);

    if (sizeBytes > 0) {
        return sizeBytes;
    }

    // looks like we have to do it the hard way
    if (0 == offset) {
        // for now we won't try to figure the size if starting
        // at 0 becaues it could take a long time
        return offset;
    }

    if (0 == sectorSize) {
        sectorSize = GetSectorSize(fd, deviceName);
    }


    std::vector<char> buffer(sectorSize);

    sizeBytes = lseek(fd, offset, SEEK_SET);

    if (-1 == sizeBytes) {
        // looks like the starting offset was too big, back up
        // try a limited number so that we don't spin too long
        // if still not the end switch to binary search as that
        // should be faster if the size if off by some large factor
        off_t curOffset = offset;

        for (int i = 0; i < 5; ++i) {
            if (-1 != (sizeBytes = lseek(fd, curOffset, SEEK_SET))) {
                curOffset = sizeBytes;
                break;
            } else {
                curOffset -= sectorSize;
            }
        }

        if (-1 == sizeBytes) {
            off_t minOffset = 0;
            off_t maxOffset = curOffset;
            curOffset = ((minOffset + maxOffset) >> 1)  & ~(sectorSize - 1);
            do {
                sizeBytes = lseek(fd, curOffset, SEEK_SET);
                if (-1 == sizeBytes) {
                    maxOffset = curOffset;
                } else {
                    minOffset = curOffset;
                }
                curOffset = ((minOffset + maxOffset) >> 1)  & ~(sectorSize - 1);
            } while (maxOffset - minOffset > sectorSize && curOffset > 0);

            // ok should be with in a sectorSize of the end
            // make sure we stopped at a readable point
            while (-1 == (sizeBytes = lseek(fd, curOffset, SEEK_SET))) {
                curOffset -= sectorSize;
            }
        }
    } else {
        // lets see if device larger then offset and hope if
        // it is, it is not too much larger
        // (note we could imploy some type of binary search here too)
        do {
            offset += sectorSize;
            sizeBytes = lseek(fd, offset, SEEK_SET); // should be faster then reading
        } while (-1 != sizeBytes);
        sizeBytes = offset - sectorSize; // went 1 to many
    }

    off_t bytesRead;

    // finally read until error or eof to get the exact size
    // this should normaly be only 1 read but just in case
    // our previous cacluations were off keep reading to eof
    do {
        bytesRead = read(fd, &buffer[0], sectorSize);
        if (-1 == bytesRead) {
            break;
        }
        sizeBytes += bytesRead;
    } while (0 != bytesRead);

    return sizeBytes;
}

int  VolumeInfoCollector::GetVsnapMajorNumber()
{
    static int major = 0;

    if (0 != major) {
        return major;
    }
    
    std::string cmd("cat /proc/devices | grep vsnap");
    cmd += " 2> /dev/null";

    std::stringstream results;

    if (!executePipe(cmd, results)) {
        return false;
    }

    if (!results.str().empty()) {
        results >> major;
    }

    return major;    
}

//void VolumeInfoCollector::GetVsnapDevices()
//{
//    std::string cmd("cat /proc/partitions | grep vs");
//    std::stringstream results;

//    if (!executePipe(cmd, results)) {
//        return;
//    }

//    if (!results.str().empty()) {
//        std::string vsnap;
//        std::string device;

//        int major;
//        int minor;

//        long long size;

//        char buffer[255];

//        struct stat64 vStat;
//        while (!results.eof()) {
//            vsnap.clear();
//            device.clear();
//           major = 0;
//            minor = 0;
//            size = 0;
//           results >> major >> minor >> size >> vsnap;
//            results.getline(buffer, sizeof(buffer));

//            if (!vsnap.empty()) {
//               device = "/dev/";
//                device += vsnap;
                // make sure custom device exists
//                if (-1 == stat64(device.c_str(), &vStat) || major != GetVsnapMajorNumber()) {
//                    continue;
//                }

//                VolumeInfoCollectorInfo volInfo;
//                volInfo.m_DeviceName = device;
//                volInfo.m_RealName = device;
//                volInfo.m_Locked = IsDeviceLocked(device);
//                volInfo.m_DeviceType = VolumeSummary::VSNAP_MOUNTED;
//                volInfo.m_DeviceVendor = VolumeSummary::INMVSNAP;
//                volInfo.m_RawSize = GetVolumeSizeInBytes(device, size * ONE_KB, 0);
//                volInfo.m_LunCapacity = volInfo.m_RawSize;
//                volInfo.m_Major = major(vStat.st_rdev);
//                volInfo.m_Minor = minor(vStat.st_rdev);
//                UpdateMountDetails(volInfo);

//                InsertVolumeInfo(volInfo);
//           }
//        }
//    }
//}


off_t VolumeInfoCollector::CalculateFSFreeSpace(const struct statvfs64 &volStatvfs)
{
    off_t freeSpace = static_cast<off_t>(volStatvfs.f_bavail) * static_cast<off_t>(volStatvfs.f_bsize);
    return freeSpace;
}


bool VolumeInfoCollector::GetVxDMPDiskInfos(void)
{
    bool bretval = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    /* A set is taken so as to avoid duplicate entries 
       which can come in vxdmpadm. */
    std::set<std::string> vxdmpnodes;
    GetVxDmpNodes(vxdmpnodes);
    DevtToDeviceName_t devttoname;
    if (!vxdmpnodes.empty())
    {
        GetDevtToDeviceName(VXDMP_PATH, devttoname);
    }
    unsigned int npartitions = m_localConfigurator.getLinuxNumPartitions();
    
    std::set<std::string>::iterator iter = vxdmpnodes.begin();
    for ( /* empty */; iter != vxdmpnodes.end(); iter++)
    {
        std::string disk = VXDMP_PATH;
        disk += UNIX_PATH_SEPARATOR;
        disk += (*iter);
        struct stat64 volStat;
        if (0 != stat64(disk.c_str(), &volStat))
        {
            continue;
        }
        if (0 == volStat.st_rdev)
        {
            continue;
        }
        if (false == IsDeviceTracked(major(volStat.st_rdev), disk))
        {
            continue;
        }

        off_t drawsize = 0;
        std::string rawdisk = GetRawDeviceName(disk);
        int fd = open(rawdisk.c_str(), O_RDONLY);
        if (-1 != fd)
        {
            drawsize = GetVolumeSizeInBytes(fd, disk);
            close(fd);
        }
        if (0 == drawsize)
        {
            continue;
        }
        VolumeInfoCollectorInfo dVolInfo;
        std::string realName;
        VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
        GetSymbolicLinksAndRealName(disk, symbolicLinks, realName);

        dVolInfo.m_DeviceName = disk;
        dVolInfo.m_DeviceID = m_DeviceIDInformer.GetID(dVolInfo.m_DeviceName);
        if (dVolInfo.m_DeviceID.empty())
        {
            DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", dVolInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
        }
        dVolInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(dVolInfo.m_DeviceName);
        dVolInfo.m_RealName = realName;
        dVolInfo.m_DeviceType = VolumeSummary::DISK;
        m_DeviceTracker.UpdateVendorType(dVolInfo.m_DeviceName, dVolInfo.m_DeviceVendor, dVolInfo.m_IsMultipath);
        dVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(dVolInfo.m_DeviceName);
        dVolInfo.m_RawSize = drawsize;
        dVolInfo.m_LunCapacity = dVolInfo.m_RawSize;
        dVolInfo.m_Locked = IsDeviceLocked(disk);
        dVolInfo.m_SymbolicLinks = symbolicLinks;
        dVolInfo.m_Major = major(volStat.st_rdev);
        dVolInfo.m_Minor = minor(volStat.st_rdev);
        UpdateMountDetails(dVolInfo);
        UpdateSwapDetails(dVolInfo);
        UpdateLvInsideDevts(dVolInfo);
        dVolInfo.m_Attributes = GetAttributes(dVolInfo.m_DeviceName);

        /*
        *
        *  vxdmp is creating 1 to 15 partitions for disk with minor 
        *  numbers increasing, major being same as disk 
        *
        * [root@imits216 ~]# ls -l /dev/vx/dmp/disk_0*
        * brw------- 1 root root 201, 128 Sep 23 10:47 /dev/vx/dmp/disk_0
        * brw------- 1 root root 201, 129 Sep 23 10:47 /dev/vx/dmp/disk_0s1
        * brw------- 1 root root 201, 138 Sep 23 10:47 /dev/vx/dmp/disk_0s10
        * brw------- 1 root root 201, 139 Sep 23 10:47 /dev/vx/dmp/disk_0s11
        * brw------- 1 root root 201, 140 Sep 23 10:47 /dev/vx/dmp/disk_0s12
        * brw------- 1 root root 201, 141 Sep 23 10:47 /dev/vx/dmp/disk_0s13
        * brw------- 1 root root 201, 142 Sep 23 10:47 /dev/vx/dmp/disk_0s14
        * brw------- 1 root root 201, 143 Sep 23 10:47 /dev/vx/dmp/disk_0s15
        * brw------- 1 root root 201, 130 Sep 23 10:47 /dev/vx/dmp/disk_0s2
        * brw------- 1 root root 201, 131 Sep 23 10:47 /dev/vx/dmp/disk_0s3
        * brw------- 1 root root 201, 132 Sep 23 10:47 /dev/vx/dmp/disk_0s4
        * brw------- 1 root root 201, 133 Sep 23 10:47 /dev/vx/dmp/disk_0s5
        * brw------- 1 root root 201, 134 Sep 23 10:47 /dev/vx/dmp/disk_0s6
        * brw------- 1 root root 201, 135 Sep 23 10:47 /dev/vx/dmp/disk_0s7
        * brw------- 1 root root 201, 136 Sep 23 10:47 /dev/vx/dmp/disk_0s8
        * brw------- 1 root root 201, 137 Sep 23 10:47 /dev/vx/dmp/disk_0s9
        * 
        * [root@imits216 ~]# ls -l /dev/vx/dmp/sda*
        * brw------- 1 root root 201, 16 Sep 23 10:47 /dev/vx/dmp/sda
        * brw------- 1 root root 201, 17 Sep 23 10:47 /dev/vx/dmp/sda1
        * brw------- 1 root root 201, 26 Sep 23 10:47 /dev/vx/dmp/sda10
        * brw------- 1 root root 201, 27 Sep 23 10:47 /dev/vx/dmp/sda11
        * brw------- 1 root root 201, 28 Sep 23 10:47 /dev/vx/dmp/sda12
        * brw------- 1 root root 201, 29 Sep 23 10:47 /dev/vx/dmp/sda13
        * brw------- 1 root root 201, 30 Sep 23 10:47 /dev/vx/dmp/sda14
        * brw------- 1 root root 201, 31 Sep 23 10:47 /dev/vx/dmp/sda15
        * brw------- 1 root root 201, 18 Sep 23 10:47 /dev/vx/dmp/sda2
        * brw------- 1 root root 201, 19 Sep 23 10:47 /dev/vx/dmp/sda3
        * brw------- 1 root root 201, 20 Sep 23 10:47 /dev/vx/dmp/sda4
        * brw------- 1 root root 201, 21 Sep 23 10:47 /dev/vx/dmp/sda5
        * brw------- 1 root root 201, 22 Sep 23 10:47 /dev/vx/dmp/sda6
        * brw------- 1 root root 201, 23 Sep 23 10:47 /dev/vx/dmp/sda7
        * brw------- 1 root root 201, 24 Sep 23 10:47 /dev/vx/dmp/sda8
        * brw------- 1 root root 201, 25 Sep 23 10:47 /dev/vx/dmp/sda9
        *
        */

        for (int i = 1; i < npartitions; i++)
        {
            dev_t partitiondevt = makedev(dVolInfo.m_Major, dVolInfo.m_Minor + i);
            ConstDevtToDeviceNameIter_t dtiter = devttoname.find(partitiondevt);
            if (devttoname.end() == dtiter)
            {
                continue;
            }

            const std::string &partitionName = dtiter->second;
            off_t prawsize = 0;
            std::string rawpartition = partitionName;
            int fd = open(rawpartition.c_str(), O_RDONLY);
            if (-1 != fd)
            {
                prawsize = GetVolumeSizeInBytes(fd, partitionName);
                close(fd);
            }
            if (0 == prawsize)
            {
                continue;
            }

            VolumeInfoCollectorInfo pVolInfo;
            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(partitionName, symbolicLinks, realName);
            
            pVolInfo.m_DeviceName = partitionName;
            pVolInfo.m_DeviceID = GetPartitionID(dVolInfo.m_DeviceID, pVolInfo.m_DeviceName);
            pVolInfo.m_WCState = dVolInfo.m_WCState;
            pVolInfo.m_RealName = realName;
            pVolInfo.m_DeviceType = VolumeSummary::PARTITION;
            pVolInfo.m_DeviceVendor = dVolInfo.m_DeviceVendor;
            pVolInfo.m_IsMultipath = dVolInfo.m_IsMultipath;
            pVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(pVolInfo.m_DeviceName);                
            pVolInfo.m_RawSize = prawsize;
            pVolInfo.m_LunCapacity = pVolInfo.m_RawSize;
            pVolInfo.m_Locked = IsDeviceLocked(partitionName);
            pVolInfo.m_SymbolicLinks = symbolicLinks;
            pVolInfo.m_Major = major(partitiondevt);
            pVolInfo.m_Minor = minor(partitiondevt);
            UpdateMountDetails(pVolInfo);
            UpdateSwapDetails(pVolInfo);
            UpdateLvInsideDevts(pVolInfo);

            InsertVolumeInfoChild(dVolInfo, pVolInfo);
        }

        UpdateAttrsWithChildren(dVolInfo);
        InsertVolumeInfo(dVolInfo);
    } /* end of for */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bretval;
}


bool VolumeInfoCollector::GetDmsetupTableInfo(void)
{
    bool bgotdminfo = true;

    std::set<std::string> dmraiddisks;
    std::string dmraidcmd("dmraid -s -s | grep \'name *:\'");
    std::stringstream dmraidres;
    if (executePipe(dmraidcmd, dmraidres))
    {
        while (!dmraidres.eof())
        {
            std::string line;
            std::getline(dmraidres, line);
            if (line.empty())
            {
                break;
            }
            std::string nametok, raiddisk;
            std::stringstream ssline(line);
            std::getline(ssline, nametok, ':');
            ssline >> raiddisk;
            if (!raiddisk.empty())
            {
                dmraiddisks.insert(raiddisk);
            }
        }
        DebugPrintf(SV_LOG_DEBUG, "Below are the raid disks:\n");
        for_each(dmraiddisks.begin(), dmraiddisks.end(), PrintString);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to find dmraid disks\n");
    }
 
    /* 
     * OUTPUT:
     * 222850001550545fc: 0 20971520 multipath 0 0 1 1 round-robin 0 2 1 65:240 100 65:48 100
     * mpath88: 0 2592000 multipath 0 0 1 1 round-robin 0 1 1 67:288 100
     * mpath73: 0 1416000 multipath 0 0 1 1 round-robin 0 1 1 71:80 100
    */
    std::string cmd("dmsetup table | grep -v \'No devices found\'");
    std::stringstream results;

    if (executePipe(cmd, results) /* && !results.eof() */ )
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: output \n%s\n", FUNCTION_NAME, results.str().c_str());
        while (!results.eof())
        {
            std::string line;
            std::getline(results, line);
            if (line.empty())
            {
                break;
            }
            FillDmsetupTableInfos(line, dmraiddisks);
        }
    }
    else
    {
        /* TODO: convert to warning ? need to ask. but should not come here normally */
        DebugPrintf(SV_LOG_ERROR, "cannot collect dev mapper devices since dmsetup table command failed\n");
        bgotdminfo = false;
    }

    return bgotdminfo;
}


void VolumeInfoCollector::FillDmsetupTableInfos(const std::string &tabline, const std::set<std::string> &dmraiddisks)
{
    std::stringstream line(tabline);
    std::string device, type;
    off_t firstblk = 0;
    off_t nblks = 0;

    std::getline(line, device, ':');

    line >> firstblk >> nblks >> type;
    if (device.empty())
    {
        //DebugPrintf(SV_LOG_DEBUG, "End of line in FillDmsetupTableInfos\n");
    }
    else
    {
        size_t devicelen = device.length();
        if (DMLASTCHAR == device[devicelen - 1])
        {
            device.erase(devicelen - 1);
        }
        std::string dmname = DEV_MAPPER_PATH;
        dmname += UNIX_PATH_SEPARATOR;
        dmname += device;

        struct stat64 dmstat;
        if ((stat64(dmname.c_str(), &dmstat) == 0) && dmstat.st_rdev)
        {
            Lv_t dmlv;
            dmlv.m_Vendor = VolumeSummary::DEVMAPPER;
            dmlv.m_Devt = dmstat.st_rdev;
            dmlv.m_Name = dmname;
            dmlv.m_Encrypted = IsEncryptedDisk(dmname);
            dmlv.m_BekVolume = IsBekVolume(dmname);
            if (dmraiddisks.end() != dmraiddisks.find(device))
            {
                type = DMRAIDDISKTYPE;
            }
            dmlv.m_Type = type;
            if ((DMMULTIPATHDISKTYPE == dmlv.m_Type) || (DMRAIDDISKTYPE == dmlv.m_Type))
            {
                /* DebugPrintf(SV_LOG_DEBUG, "for device %s, setting valid to false\n", dmlv.m_Name.c_str()); */
                dmlv.m_IsValid = false;
            }
            dmlv.m_Size = GetVolumeSizeInBytes(dmname, nblks * 512);
            while (!line.eof())
            {
                std::string insidedevt;
                line >> insidedevt;
                if (insidedevt.empty())
                {
                    break;
                }
                std::string::const_iterator begin = insidedevt.begin();
                std::string::const_iterator end = insidedevt.end();
                std::string::const_iterator delimiter = find(begin, end, DMMAJORMINORSEP);
                if (insidedevt.end() != delimiter)
                {
                    std::string dmmajor(begin, delimiter);
                    std::string dmminor(delimiter + 1, end);
                    int major = 0;
                    int minor = 0;
                    std::stringstream strmajor(dmmajor);
                    std::stringstream strminor(dmminor);
                    strmajor >> major;
                    strminor >> minor;
                    dev_t devt = makedev(major, minor);
                    if (devt)
                    {
                        dmlv.m_InsideDevts.insert(devt);
                    }
                }
            }
            /* any lv having no dg, should have its name as vg name 
             * so that this will be copied to underlying disks and partitions
             * lvm lvs always have dg. so need not copy name */
            dmlv.m_VgName = dmlv.m_Name;
            InsertLv(dmlv);
        }
    }
}


bool VolumeInfoCollector::GetLvmVgInfos(void)
{
    // get lvm info: expected output if exits is something like
    //  VG Name               gjzvg1
    //  LV Name                /dev/gjzvg1/gjzlv1
    //  VG Name                gjzvg1
    //  LV Status              available
    //  LV Size                34.18 GB
    //  LV Name                /dev/gjzvg1/gjzlv2
    //  VG Name                gjzvg1
    //  LV Status              available
    //  LV Size                24.41 GB
    //  LV Name                /dev/gjzvg1/gjzlv3
    //  VG Name                gjzvg1
    //  LV Status              available
    //  LV Size                19.53 GB
    //  PV Name               /dev/sdc2
    //  PV Name               /dev/sdc3
    //  PV Name               /dev/sdc5
    //  VG Name               gjzvg2
    //  LV Name                /dev/gjzvg2/gjzlv1
    //  VG Name                gjzvg2
    //  LV Status              available
    //  LV Size                34.18 GB
    //  LV Name                /dev/gjzvg2/gjzlv2
    //  VG Name                gjzvg2
    //  LV Status              available
    //  LV Size                24.41 GB
    //  LV Name                /dev/gjzvg2/gjzlv3
    //  VG Name                gjzvg2
    //  LV Status              available
    //  LV Size                19.53 GB
    //  PV Name               /dev/sdc2
    //  PV Name               /dev/sdc3
    //  PV Name               /dev/sdc5

    std::string cmd("vgdisplay -v 2> /dev/null | grep -e 'VG Name' -e 'LV Name' -e 'LV Size' -e 'LV Status' -e 'PV Name'");
    std::stringstream results;

    if (!executePipe(cmd, results)) {
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG, "%s: output \n%s\n", FUNCTION_NAME, results.str().c_str());
    if (results.str().empty()) {
        return true;
    }

    // tokens for parsing output
    std::string vgNameTok("VG Name");
    std::string vgTok("VG");
    std::string pvTok("PV");
    std::string pvNameTok("PV Name");
    std::string lvTok("LV");
    std::string nameTok("Name");
    std::string lvSizeTok("Size");
    std::string lvStatusTok("Status");

    std::string lvStatusOk("available");

    char line[128];

    std::string tok1;
    std::string tok2;
    std::string deviceName;
    std::string units;
    std::string pvName;
    std::string lvStatus;
    std::string vgName;
    std::string unused;
    std::string strSize;

    std::string::size_type beginIdx = 0;

    do {
        std::string::size_type idx = results.str().find(pvNameTok, beginIdx);
        if (std::string::npos == idx) {
            // is this ok or an error?
            // I.e. does this just mean we are at the end of the buffer but didn't realize yet
            // or did we not get the correct output? there should always be at least 1 PV Name
            // per VG Name (might not be any LV Names per VG Name as a volume group gets
            // created before creating logical volumes so someone may have created the group
            // but decied not to create the logical volumes and didn't remove the valume group)
            break;
        }

        idx = results.str().find(vgNameTok, idx);
        if (std::string::npos == idx) {
            idx = results.str().length() + beginIdx;
        }

        std::stringstream vgStream(std::string(results.str().substr(beginIdx, idx - beginIdx)));
        beginIdx = idx;

        float size;

        off_t sizeBytes;

        tok1.clear();
        tok2.clear();

        // get VG Name that starts this group
        vgStream >> tok1 >> tok2 >> vgName;
        vgStream.getline(line, sizeof(line));
        if (tok1 != vgTok || tok2 != nameTok) {
            break;
        }
  
        Vg_t lvmVg;
        lvmVg.m_Vendor = VolumeSummary::LVM;
        lvmVg.m_Name = vgName;

        // first get the PV Names for this VG
        // note these are at the end of each VG info but we need them before
        // we process the LV info with in the VG group
        idx = vgStream.str().find(pvNameTok);
        if (std::string::npos == idx) {
            break; // is this ok or an error?
        }
        std::stringstream pvStream(std::string(vgStream.str().substr(idx)));

        do {
            tok1.clear();
            tok2.clear();
            pvName.clear();
            pvStream >> tok1 >> tok2 >> pvName;
            pvStream.getline(line, sizeof(line));
            if (tok1 != pvTok && tok2 != nameTok) {
                break;
            }

            if (!pvName.empty()) {
                struct stat64 pvStat;
                if ((0 == stat64(pvName.c_str(), &pvStat)) && pvStat.st_rdev)
                {
                    lvmVg.m_InsideDevts.insert(pvStat.st_rdev);
                }
            }
        } while (!pvStream.eof());
         

        do {
            tok1.clear();
            tok2.clear();
            deviceName.clear();

            // get LV Name
            vgStream >> tok1 >> tok2 >> deviceName;
            vgStream.getline(line, sizeof(line));
            if (tok1 != lvTok || tok2 != nameTok) {
                break;
            }

            // skip over this VG Name but make sure it is there
            vgStream >> tok1 >> tok2 >> unused;
            vgStream.getline(line, sizeof(line));
            if (tok1 != vgTok || tok2 != nameTok) {
                break;
            }

            // get LV Status
            lvStatus.clear();
            vgStream >> tok1 >> tok2 >> lvStatus;
            vgStream.getline(line, sizeof(line));
            if (tok1 != lvTok || tok2 != lvStatusTok) {
                break;
            }

            // get LV Size
            size = 0;
            strSize.clear();
            units.clear();
            vgStream >> tok1 >> tok2 >> strSize >> units;
            vgStream.getline(line, sizeof(line));
            if (tok1 != lvTok || tok2 != lvSizeTok) {
                break;
            }

            /* handle the cases where the size has additional symbols
             *   VG Name                centos_ubuntumt
             *   LV Status              available
             *   LV Size                <17.00 GiB
             */
            boost::replace_all(strSize, "<", "");
            boost::replace_all(strSize, ">", "");
            boost::replace_all(strSize, ",", ".");
            try {
                size = boost::lexical_cast<float>(strSize);
            } 
            catch (const boost::bad_lexical_cast& e) {
                DebugPrintf(SV_LOG_ERROR,"%s: size %s units %s error %s\n",
                __FUNCTION__, strSize.c_str(), units.c_str(), e.what());
                break;
            }

            Lv_t lv;
            if (!deviceName.empty() && (deviceName[0] != '/'))
            {
                std::string prefix = "/dev/";
                prefix += vgName;
                prefix += '/';
                lv.m_Name = prefix+deviceName;
            }
            else
            {
                lv.m_Name = deviceName;
            }

            sizeBytes = GetVolumeSizeInBytes(lv.m_Name,SizeUnitsToSizeBytes(size, units));
            lv.m_Vendor = lvmVg.m_Vendor;
            lv.m_VgName = vgName;
            lv.m_Size = sizeBytes;
            if (0 != lvStatus.find(lvStatusOk))
            {
                lv.m_Available = false;
            }
            int statRet = 0;
            struct stat64 lvStat;
            statRet = stat64(lv.m_Name.c_str(), &lvStat);
            int staterrno = (0 == statRet) ? 0 : errno;

            if ((0 == statRet) && lvStat.st_rdev)
            {
                lv.m_Devt = lvStat.st_rdev;
                UpdateVgLvFromLvInfo(vgName, lv);
                InsertLv(lvmVg.m_Lvs, lv);
            }
            else if (staterrno == ENOENT)
            {
                DebugPrintf(SV_LOG_DEBUG,"stat64 for %s returned %d, errno %d.\n",
                    lv.m_Name.c_str(), statRet, staterrno);
                // retry /dev/mapper/<vgname>-<lvname>
                std::string prefix = "/dev/mapper/";
                prefix += vgName;
                prefix += '-';
                lv.m_Name = prefix+deviceName;

                statRet = stat64(lv.m_Name.c_str(), &lvStat);
                staterrno = (0 == statRet) ? 0 : errno;
                if ((0 == statRet) && lvStat.st_rdev)
                {
                    lv.m_Devt = lvStat.st_rdev;
                    UpdateVgLvFromLvInfo(vgName, lv);
                    InsertLv(lvmVg.m_Lvs, lv);
                }
            }
            if (0 != statRet)
            {
                DebugPrintf(SV_LOG_DEBUG,"stat64 for %s returned %d, errno %d.\n",
                    lv.m_Name.c_str(), statRet, staterrno);
            }
        } while (!vgStream.eof());

        InsertVg(lvmVg);
    } while (!results.eof());

    return true;
}


bool VolumeInfoCollector::GetMountInfos(void)
{
    bool bgotinfo = true;

    // order is important so do not change it

    // start with the mount table
    // note:  mount -n does not update the mount table so it
    // may be missing things. we start out not mounted
    // in case a umount was called with out updating MOUNT_TABLE
    // we will mark it mounted when we process /proc/mounts
    bool bgotmtabs = GetMountVolumeInfos<MountTableEntry>(!DEVICE_MOUNTED);

    // next look at /proc/mounts.
    // this is to pick up any mounts not in the mount table
    // one would think just use this and not the mount table
    // since it will contain all current mounts. unfortunately
    // testing revealed that some of the mount names are of a
    // form that we can't validate if we really want it. testing
    // (thus far) has shown that if we process the mount table
    // first and then /proc/mounts we find all the mounts that
    // we care about.
    bool bgotprocmounts = GetMountVolumeInfos<ProcMountsEntry>(DEVICE_MOUNTED);

    // finally see if there are any well known mount points
    // that are currently unmounted. this is done last
    // as only care about the ones that are not currently
    // mounted.
    bool bgotfstabs = GetMountVolumeInfos<FsTableEntry>(!DEVICE_MOUNTED);

    bgotfstabs = (bgotmtabs && bgotprocmounts && bgotfstabs);
    return bgotinfo;
}

bool VolumeInfoCollector::GetOSVolumeManagerInfos(void)
{
    /* NOTE: order is very important here. please do not change */
    bool bgotlvs = GetDmsetupTableInfo();
    bool bgotvgs = GetLvmVgInfos();
    bool bgotvxvmvgs = GetVxVMVgs();

    bool bgotvminfos = (bgotlvs && bgotvgs && bgotvxvmvgs);
    return bgotvminfos;
}


void VolumeInfoCollector::CollectScsiHCTL(void)
{
    GetDeviceHCTL_t g[2];

    /**
     * From testing, 2.39 uek kernels do not have
     * hctl entries in sys class, rather have entries 
     * in sys block(like /sys/block/sda/device/scsi_disk/0:0:0:0), 
     * where as other lesser uek kernels like 2.32 have sys class entries.
     * Assuming that next higher versions of kernels might
     * continue not have entries in sys class, for any kerenl
     * having uek, first preference is to check for sys block.
     * Although it is observed from testing that uek kernels that
     * do not have entry in sys block (like 2.32 uek), have entries
     * like /sys/block/sda/device/scsi_disk\:0\:0\:0\:0/ from where
     * hctl can be picked up. For nonuek kernels, the sys class is given
     * first priority.
     */
    if (ShouldPreferSysBlockForHCTL()) {
        g[0] = &GetScsiHCTLFromSysBlock;
        g[1] = &GetScsiHCTLFromSysClass;
    } else {
        g[0] = &GetScsiHCTLFromSysClass;
        g[1] = &GetScsiHCTLFromSysBlock;
    }
    
    CollectDeviceHCTLPair_t c = std::bind1st(std::mem_fun(&VolumeInfoCollector::InsertDeviceHCTLPair), this);
    g[0](c);
    if (m_DeviceToHCTL.empty())
        g[1](c);

    /*
    DebugPrintf(SV_LOG_DEBUG, "device name --> h:c:t:l\n");
    for (DeviceToHCTL_t::const_iterator cit = m_DeviceToHCTL.begin(); cit != m_DeviceToHCTL.end(); cit++)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", cit->first.c_str(), cit->second.c_str());
    }
    */
}

void VolumeInfoCollector::UpdateHCTLFromDevice(const std::string &devicename)
{
}
