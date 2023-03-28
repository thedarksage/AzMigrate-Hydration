//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : devicetrackerminor.cpp
// 
// Description: DeviceTracker class implementation see devicetracker.h for more details
// 

#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>

#include <boost/algorithm/string.hpp>

#include "devicetracker.h"
#include "configurator2.h"
#include "localconfigurator.h"
#include "mountpointentry.h"


// TODO: remove all code that uses the devices.txt file as we now have
// a reliable way using sysfs to figure this out
// it is left here for now in case something was missed but will removed
// eventually

DeviceTracker::DeviceTracker()
{
    // NOTE: these come directly from linux devices.txt file
    // should be kept up to date with the lastest one
    m_MajorNumbers.insert(MajorNumber(3, REQUIRES_CHECK));                       // First MFM, RLL and IDE hard disk/CD-ROM interface
    m_MajorNumbers.insert(MajorNumber(7, REQUIRES_CHECK));                       // Loopback devices (we use them for volpacks)
    m_MajorNumbers.insert(MajorNumber(8));                                       // scsi disk
    m_MajorNumbers.insert(MajorNumber(9));                                       // metadisk raid 
    m_MajorNumbers.insert(MajorNumber(13));                                      // 8-bit MFM/RLL/IDE controller
    m_MajorNumbers.insert(MajorNumber(14));                                      // BIOS harddrive callback support {2.6}
    m_MajorNumbers.insert(MajorNumber(19));                                      // "Double" compressed disk
    m_MajorNumbers.insert(MajorNumber(21));                                      // Acorn MFM hard drive interface
    m_MajorNumbers.insert(MajorNumber(22, REQUIRES_CHECK));                      // second ide hard disk or cd-rom 
    m_MajorNumbers.insert(MajorNumber(28, REQUIRES_CHECK));                      // ACSI disk (68k/Atari)/Fourth Matsushita (Panasonic/SoundBlaster) CD-ROM
    m_MajorNumbers.insert(MajorNumber(33, REQUIRES_CHECK));                      // third ide hard disk or cd-rom 
    m_MajorNumbers.insert(MajorNumber(34, REQUIRES_CHECK));                      // fourth ide hard disk or cd-rom 
    m_MajorNumbers.insert(MajorNumber(36));                                      // MCA ESDI hard disk
    m_MajorNumbers.insert(MajorNumber(45));                                      // Parallel port IDE disk devices
    m_MajorNumbers.insert(MajorNumber(47));                                      // Parallel port ATAPI disk devices
    m_MajorNumbers.insert(MajorNumber(48));                                      // Mylex DAC960 PCI RAID controller; first controller
    m_MajorNumbers.insert(MajorNumber(49));                                      // Mylex DAC960 PCI RAID controller; second controller
    m_MajorNumbers.insert(MajorNumber(50));                                      // Mylex DAC960 PCI RAID controller; third controller
    m_MajorNumbers.insert(MajorNumber(51));                                      // Mylex DAC960 PCI RAID controller; fourth controller
    m_MajorNumbers.insert(MajorNumber(52));                                      // Mylex DAC960 PCI RAID controller; fifth controller
    m_MajorNumbers.insert(MajorNumber(53));                                      // Mylex DAC960 PCI RAID controller; sixth controller
    m_MajorNumbers.insert(MajorNumber(54));                                      // Mylex DAC960 PCI RAID controller; seventh controller
    m_MajorNumbers.insert(MajorNumber(55));                                      // Mylex DAC960 PCI RAID controller; eighth controller
    m_MajorNumbers.insert(MajorNumber(56, REQUIRES_CHECK));                      // fifth ide hard disk or cd-rom 
    m_MajorNumbers.insert(MajorNumber(57, REQUIRES_CHECK));                      // sixth ide hard disk or cd-rom 
    m_MajorNumbers.insert(MajorNumber(58));                                      // LVM (note: 253 could also be used LVM2?)
    m_MajorNumbers.insert(MajorNumber(64));                                      // Scramdisk/DriveCrypt encrypted devices
    m_MajorNumbers.insert(MajorNumber(65));                                      // SCSI disk devices (16-31)
    m_MajorNumbers.insert(MajorNumber(66));                                      // SCSI disk devices (32-47)
    m_MajorNumbers.insert(MajorNumber(67));                                      // SCSI disk devices (48-63)
    m_MajorNumbers.insert(MajorNumber(68));                                      // SCSI disk devices (64-79)
    m_MajorNumbers.insert(MajorNumber(69));                                      // SCSI disk devices (80-95)
    m_MajorNumbers.insert(MajorNumber(70));                                      // SCSI disk devices (96-111)
    m_MajorNumbers.insert(MajorNumber(71));                                      // SCSI disk devices (112-127)
    m_MajorNumbers.insert(MajorNumber(72));                                      // Compaq Intelligent Drive Array, first controller
    m_MajorNumbers.insert(MajorNumber(73));                                      // Compaq Intelligent Drive Array, second controller
    m_MajorNumbers.insert(MajorNumber(74));                                      // Compaq Intelligent Drive Array, third controller
    m_MajorNumbers.insert(MajorNumber(75));                                      // Compaq Intelligent Drive Array, fourth controller
    m_MajorNumbers.insert(MajorNumber(76));                                      // Compaq Intelligent Drive Array, fifth controller
    m_MajorNumbers.insert(MajorNumber(77));                                      // Compaq Intelligent Drive Array, sixth controller
    m_MajorNumbers.insert(MajorNumber(78));                                      // Compaq Intelligent Drive Array, seventh controller
    m_MajorNumbers.insert(MajorNumber(79));                                      // Compaq Intelligent Drive Array, eighth controller
    m_MajorNumbers.insert(MajorNumber(80));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(81));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(82));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(83));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(84));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(85));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(86));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(87));                                      // I2O hard disk
    m_MajorNumbers.insert(MajorNumber(88, REQUIRES_CHECK));                      // Seventh IDE hard disk/CD-ROM interface
    m_MajorNumbers.insert(MajorNumber(89, REQUIRES_CHECK));                      // Eighth IDE hard disk/CD-ROM interface
    m_MajorNumbers.insert(MajorNumber(90, REQUIRES_CHECK));                      // Ninth IDE hard disk/CD-ROM interface
    m_MajorNumbers.insert(MajorNumber(91, REQUIRES_CHECK));                      // Tenth IDE hard disk/CD-ROM interface
    m_MajorNumbers.insert(MajorNumber(92));                                      // PPDD encrypted disk driver
    m_MajorNumbers.insert(MajorNumber(94));                                      // IBM S/390 DASD block storage
    m_MajorNumbers.insert(MajorNumber(99));                                      // JavaStation flash disk
    m_MajorNumbers.insert(MajorNumber(101));                                     // AMI HyperDisk RAID controller
    m_MajorNumbers.insert(MajorNumber(102));                                     // Compressed block device
    m_MajorNumbers.insert(MajorNumber(104));                                     // Compaq Next Generation Drive Array, first controller
    m_MajorNumbers.insert(MajorNumber(105));                                     // Compaq Next Generation Drive Array, second controller
    m_MajorNumbers.insert(MajorNumber(106));                                     // Compaq Next Generation Drive Array, third controller
    m_MajorNumbers.insert(MajorNumber(107));                                     // Compaq Next Generation Drive Array, fourth controller
    m_MajorNumbers.insert(MajorNumber(108));                                     // Compaq Next Generation Drive Array, fifth controller
    m_MajorNumbers.insert(MajorNumber(109));                                     // Compaq Next Generation Drive Array, sixth controller
    m_MajorNumbers.insert(MajorNumber(110));                                     // Compaq Next Generation Drive Array, seventh controller
    m_MajorNumbers.insert(MajorNumber(111));                                     // Compaq Next Generation Drive Array, eighth controller
    m_MajorNumbers.insert(MajorNumber(112));                                     // IBM iSeries virtual disk
    m_MajorNumbers.insert(MajorNumber(114));                                     // IDE BIOS powered software RAID interfaces such as the Promise Fastrak
    m_MajorNumbers.insert(MajorNumber(115));                                     // NetWare (NWFS) Devices (0-255)
    m_MajorNumbers.insert(MajorNumber(117));                                     // Enterprise Volume Management System (EVMS)
    m_MajorNumbers.insert(MajorNumber(120, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(121, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(122, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(123, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(124, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(125, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(126, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(127, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(128));                                     // SCSI disk devices (128-143)
    m_MajorNumbers.insert(MajorNumber(129));                                     // SCSI disk devices (144-159)
    m_MajorNumbers.insert(MajorNumber(130));                                     // SCSI disk devices (160-175)
    m_MajorNumbers.insert(MajorNumber(131));                                     // SCSI disk devices (176-191)
    m_MajorNumbers.insert(MajorNumber(132));                                     // SCSI disk devices (192-207)
    m_MajorNumbers.insert(MajorNumber(133));                                     // SCSI disk devices (208-223)
    m_MajorNumbers.insert(MajorNumber(134));                                     // SCSI disk devices (224-239)
    m_MajorNumbers.insert(MajorNumber(135));                                     // SCSI disk devices (240-255)
    m_MajorNumbers.insert(MajorNumber(136));                                     // Mylex DAC960 PCI RAID controller; ninth controller
    m_MajorNumbers.insert(MajorNumber(137));                                     // Mylex DAC960 PCI RAID controller; tenth controller
    m_MajorNumbers.insert(MajorNumber(138));                                     // Mylex DAC960 PCI RAID controller; eleventh controller
    m_MajorNumbers.insert(MajorNumber(139));                                     // Mylex DAC960 PCI RAID controller; twelfth controller
    m_MajorNumbers.insert(MajorNumber(140));                                     // Mylex DAC960 PCI RAID controller; thirteenth controller
    m_MajorNumbers.insert(MajorNumber(141));                                     // Mylex DAC960 PCI RAID controller; fourteenth controller
    m_MajorNumbers.insert(MajorNumber(142));                                     // Mylex DAC960 PCI RAID controller; fifteenth controller
    m_MajorNumbers.insert(MajorNumber(143));                                     // Mylex DAC960 PCI RAID controller; sixteenth controller
    m_MajorNumbers.insert(MajorNumber(153));                                     // Enhanced Metadisk RAID (EMD) storage units
    m_MajorNumbers.insert(MajorNumber(160));                                     // Carmel 8-port SATA Disks on First Controller
    m_MajorNumbers.insert(MajorNumber(161));                                     // Carmel 8-port SATA Disks on Second Controller
    m_MajorNumbers.insert(MajorNumber(162));                                     // Raw block device interface
    m_MajorNumbers.insert(MajorNumber(180));                                     // USB block devices
    m_MajorNumbers.insert(MajorNumber(199));                                     // Veritas volume manager (VxVM) volumes
    m_MajorNumbers.insert(MajorNumber(201));                                     // Veritas VxVM dynamic multipathing driver
    m_MajorNumbers.insert(MajorNumber(202));                                     // Xen Virtual Block Device
    m_MajorNumbers.insert(MajorNumber(228));                                     // IBM 3270 terminal block-mode access
    m_MajorNumbers.insert(MajorNumber(240, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(241, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(242, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(243, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(244, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(245, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(246, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(247, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(248, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(249, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(250, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(251, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(252, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(253, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE (device-mapper/LVM2)
    m_MajorNumbers.insert(MajorNumber(254, REQUIRES_CHECK, EXPERIMENTAL_CHECK)); // LOCAL/EXPERIMENTAL USE
    m_MajorNumbers.insert(MajorNumber(257));                                     // SSFDC Flash Translation Layer filesystem
    m_MajorNumbers.insert(MajorNumber(258));                                     // ROM/Flash read-only translation layer
}

bool DeviceTracker::LoadDevices()
{
    std::ifstream devices("/proc/devices"); // for linux they are in /proc/devices
    if (!devices.good()) {
        return false;
    }

    char line[128];
    std::string name;

    int major;

    // get to the block devices section
    std::string blockTok("Block");  // indicates the start of the block devices in the /proc/devices file
    
    while (!devices.eof()) {
        devices >> name;
        devices.getline(line, sizeof(line));
        if (name == blockTok) { 
            break;
        }
    }

    MajorNumber majorNumber;
        
    // process block devices
    MajorNumbers_t::iterator it;
    while (!devices.eof()) {
        devices >> major;
        devices >> name;
        devices.getline(line, sizeof(line));
        majorNumber.Major(major);
        it = m_MajorNumbers.find(majorNumber);
        if (m_MajorNumbers.end() != it) {
            m_TrackedNumbers.insert(*it);
        }            
    }

    return true;
}

bool DeviceTracker::IsTracked(int major, char const * deviceFile) const
{
    if (IsVolPack(deviceFile)) {
        return true;
    }

    MajorNumber majorNumber(major);
    
    MajorNumbers_t::iterator it;
    
    LocalConfigurator theLocalConfigurator;
        
    if (theLocalConfigurator.useLinuxDeviceTxt()) {
        it = m_TrackedNumbers.find(majorNumber);
        
        if (m_TrackedNumbers.end() == it) {
            return false;
        }
        
        if (!(*it).RequiresCheck()) {
            return true;
        }
        
        
        std::string media;
        if (IsHardDisk(deviceFile, media)) {
            return true;
        }
        
        if (IsCDRom(deviceFile)) {
            return false;
        }
    } else {
        if (IsCDRom(deviceFile)) {
            return false;
        }

        switch(IsFixedDisk(deviceFile)) {
            case -1:
                return false;  // don't track
            case 0:
                break;         // needs more checks
            case 1:
                return true;   // track
        }
    }
    
    /* NOTE: please do not change the order; 
     * since not to give vxdmp or dm to check
     * for hdlm or emcpp */
    if (m_DevMapperTracker.IsDevMapperName(deviceFile)) {
        return true;
    }

    if (m_VxDmpTracker.IsVxDmpName(deviceFile)) {
        return true;
    } 

    if (m_HdlmTracker.IsHdlm(deviceFile)) {
        return true;
    }

    if (m_EMCPPTracker.IsEMCPP(deviceFile)) {
        return true;
    } 

    if (m_DevMapperDockerTracker.IsDevMapperDockerName(deviceFile)) {
        return true;
    }

    if (theLocalConfigurator.useLinuxDeviceTxt()) {
        if (m_TrackedNumbers.end() != it) {
            return ((*it).IsExperimental() && theLocalConfigurator.trackExperimentalDeviceNumbers());
        }
    }
    
    return false;
}

std::string DeviceTracker::GetSysfsBlockDirectory() const
{
    if (m_SysFs.empty()) {
    
        // it's almost always here so let's try it first
        std::string sysfsBlock("/sys/block/");
        DIR* dir = opendir(sysfsBlock.c_str());
        if (0 != dir) {
            closedir(dir);
            m_SysFs = sysfsBlock;
        } else {
            // looks like sysfs is somewhere else find it 
            ProcMountsEntry  mpe;
            while (-1 != mpe.Next()) {
                if (0 == strcmp(mpe.FileSystem(),"sysfs")) {
                    sysfsBlock = mpe.MountPoint();
                    sysfsBlock += "/block/";
                    dir = opendir(sysfsBlock.c_str());
                    if (0 != dir) {
                        closedir(dir);
                        m_SysFs = sysfsBlock;
                    }
                }
            }
        }
    }

    return m_SysFs;    
}

bool DeviceTracker::IsIdeDevice(char const * deviceFile) const 
{
     std::string device(deviceFile + strlen("/dev/"));
     std::replace_if(device.begin(), device.end(), std::bind2nd(std::equal_to<int>(),'/'), '!');
     std::string devName(GetSysfsBlockDirectory() + device + "/dev");
     std::ifstream devFile(devName.c_str());
     if (devFile.good()) {
         std::string major;
         std::getline(devFile, major, ':');
         return (major == "3" || major == "22"); // 3 and 22 are linux IDE major device numbers
     }
     return false;
}

// only used if theLocalConfigurator.useLinuxDeviceTxt() returns false (default)
int DeviceTracker::IsFixedDisk(char const * deviceFile) const
{    
    std::string device(deviceFile + strlen("/dev/"));

    std::replace_if(device.begin(), device.end(),
                    std::bind2nd(std::equal_to<int>(),'/'),
                    '!');

    // make sure it has a device subdirectory
    std::string isDevice(GetSysfsBlockDirectory() + device + "/device");

    std::ifstream deviceCheck(isDevice.c_str());

    if (!(deviceCheck.good() || IsIdeDevice(deviceFile))) {
        return 0; // needs more checking
    }

    // make sure it is fixed
    std::string deviceRemoveable(GetSysfsBlockDirectory() + device + "/removable");
    
    deviceCheck.close();
    deviceCheck.open(deviceRemoveable.c_str());
    if (deviceCheck.good()) {
        
        /* commenting this out 
         * as if removable file
         * is present is enough 
         * because some internal fixed
         * disks on IBM xserver sles
         * are coming up as removable
        int removable;
        
        deviceCheck >> removable;
        if (0 == removable) {
            return 1; // fixed disk we should track
        } 
        return -1; // not anything we care about
        */

        return 1; // lets track
    }
    
    return 0; // needs more checking
}

bool DeviceTracker::IsCDRom(char const * deviceFile) const
{
    std::string deviceName = strrchr(deviceFile, '/');
    std::string cmd;
    std::string out;
    std::stringstream results;

    cmd = "grep 'DRIVER=' /sys/block/";
    cmd += deviceName;
    cmd += "/device/uevent";

    if (!executePipe(cmd, results) || results.str().empty()) {
        return false;
    }

    out = results.str();
    boost::trim(out);
    if (boost::equals(out, "DRIVER=sr")) {
        DebugPrintf(SV_LOG_DEBUG, "Found CDRom Disk %s\n", deviceName.c_str());
        return true;
    }

    return false;
}

// only used if theLocalConfigurator.useLinuxDeviceTxt() returns true
bool DeviceTracker::IsHardDisk(char const * deviceFile, std::string & media) const
{
    // TODO: currently only tests for ide disk
    // should add some checks for scsis disk too

    media.clear(); 

    const char* deviceName = strrchr(deviceFile, '/');
    if (0 == deviceName) {
        return false;
    }

    std::string path("/proc/ide/"); // where linix keeps the ide devices
    path += deviceName;
    path += "/media";               // holds the type of ide device disk, cdrom, etc.

    std::ifstream f(path.c_str());

    if (!f) {
        return false;
    }

    f >> media;
    return (media == "disk"); // we only want disk media 
}


