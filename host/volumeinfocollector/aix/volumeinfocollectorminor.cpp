
#include <stdio.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <math.h>
#include <string.h>
#include <lvm.h>
#include <sys/devinfo.h>
#include <sys/types.h>
#include <string>
#include <sstream>
#include <fstream>
#include <errno.h>
#include <iostream>
#include <list>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/devinfo.h>
#include <sstream>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include "volumeinfocollectorinfo.h"
#include "volumeinfocollector.h"
#include "mountpointentry.h"
#include "executecommand.h"
#include "utilfdsafeclose.h"
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "utildirbasename.h"
#include <ace/Guard_T.h>
#include "ace/Thread_Mutex.h"
#include "ace/Recursive_Thread_Mutex.h"

ACE_Recursive_Thread_Mutex g_lockvolinfo;

bool getVgNameToPPSize(const std::list<std::string>& vgs, std::map<std::string, SV_ULONGLONG>& VGtoPPSizeMap)
{
    std::map<std::string, SV_ULONGLONG> unitstovalue;
    unitstovalue.insert(std::make_pair("kilobyte(s)", 1024ULL));
    unitstovalue.insert(std::make_pair("megabyte(s)", 1024ULL*1024ULL));
    unitstovalue.insert(std::make_pair("gigabyte(s)", 1024ULL*1024ULL*1024ULL));
    unitstovalue.insert(std::make_pair("terabyte(s)", 1024ULL*1024ULL*1024ULL*1024ULL));

	bool bRetvalue = true;
	std::list<std::string>::const_iterator vgsBegIter = vgs.begin();
	std::list<std::string>::const_iterator vgsEndIter = vgs.end();
	while(vgsBegIter != vgsEndIter)
	{
        std::string cmd("lsvg ");
        cmd += *vgsBegIter;
        cmd += " | grep \'PP SIZE:\'";
        std::stringstream results;

        if (executePipe(cmd, results))
        {
            std::string generictok, pptok, sizetok, ppsize, units;
            results >> generictok >> generictok >> generictok >> pptok >> sizetok >> ppsize >> units;
            std::map<std::string, SV_ULONGLONG>::const_iterator utviter = unitstovalue.find(units);
            std::stringstream ssppsize(ppsize);
            SV_ULONGLONG ppsizevalue = 0;
            ssppsize >> ppsizevalue;

            if (("PP" == pptok) && ("SIZE:" == sizetok) && (unitstovalue.end() != utviter) && ppsizevalue)
            {
                const SV_ULONGLONG &unitsvalue = utviter->second;
                SV_ULONGLONG ppsizewithunits = ppsizevalue*unitsvalue;
                VGtoPPSizeMap.insert(std::make_pair(*vgsBegIter, ppsizewithunits));
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to fork command %s to get ppsize of vg\n", cmd.c_str());
            bRetvalue = false;
        }
		
		vgsBegIter++;
	}

    DebugPrintf(SV_LOG_DEBUG, "vgs to ppsize values:\n");
    for (std::map<std::string, SV_ULONGLONG>::const_iterator it = VGtoPPSizeMap.begin(); it != VGtoPPSizeMap.end(); it++)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s --> " ULLSPEC "\n", it->first.c_str(), it->second);
    }

	return bRetvalue;
}

bool getPvNames(const std::string& vgName, const std::string& lspvOutput, std::list<std::string>& pvNameList)
{
	bool bRetvalue = true;
	std::stringstream lspvOutputStream(lspvOutput.c_str());
	while(!lspvOutputStream.eof())
    {
		std::string line;
		std::getline(lspvOutputStream, line);
		if(line.empty() == false)
		{
			std::stringstream linestream(line) ;
			std::string diskName, data, vg, state;
			linestream >> diskName >> data >> vg >> state;
			if(strcmp(vg.c_str(), vgName.c_str()) ==0)
			{
				pvNameList.push_back(diskName);
			}
		}
	}
	return bRetvalue;
}

static SV_ULONGLONG SizeFromIocInfo(const int &fd, const std::string &sVolume)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with sVolume = %s\n",FUNCTION_NAME, sVolume.c_str());
    SV_ULONGLONG rawsize = 0;
    SV_ULONGLONG mulfactor;
    unsigned int low, high;

    struct devinfo devinfo;
    if (ioctl(fd, IOCINFO, &devinfo) != -1)
    {
        std::stringstream ss;
        switch(devinfo.devtype)
        {
            case DD_DISK:   /* disk */
                if (devinfo.flags & DF_LGDSK)
                {
                    ss << "For DD_DISK, DF_LGDSK is set. "
                       << " devinfo.flags = " << (unsigned int)devinfo.flags
                       << " bytpsec = " << devinfo.un.dk64.bytpsec
                       << " secptrk = " << devinfo.un.dk64.secptrk
                       << " trkpcyl = " << devinfo.un.dk64.trkpcyl
                       << " lo_numblks = " << devinfo.un.dk64.lo_numblks
                       << " segment_size = " << devinfo.un.dk64.segment_size
                       << " segment_count = " << devinfo.un.dk64.segment_count
                       << " byte_count = " << devinfo.un.dk64.byte_count
                       << " hi_numblks = " << devinfo.un.dk64.hi_numblks;

                    mulfactor = pow(2, sizeof(devinfo.un.dk64.lo_numblks)*8);
                    low = (unsigned int)devinfo.un.dk64.lo_numblks;
                    high = (unsigned int)devinfo.un.dk64.hi_numblks;
                    rawsize = ((mulfactor*high) + low) * devinfo.un.dk64.bytpsec;
                }
                else
                {
                    ss << "For DD_DISK, DF_LGDSK is not set. "
                       << " devinfo.flags = " << (unsigned int)devinfo.flags
                       << " bytpsec = " << devinfo.un.dk.bytpsec
                       << " secptrk = " << devinfo.un.dk.secptrk
                       << " trkpcyl = " << devinfo.un.dk.trkpcyl
                       << " numblks = " << devinfo.un.dk.numblks
                       << " segment_size = " << devinfo.un.dk.segment_size
                       << " segment_count = " << devinfo.un.dk.segment_count
                       << " byte_count = " << devinfo.un.dk.byte_count;
                    low = (unsigned int)devinfo.un.dk.numblks;
                    rawsize = low * devinfo.un.dk.bytpsec;
                }
                DebugPrintf(SV_LOG_DEBUG, "For device %s, %s\n", sVolume.c_str(), ss.str().c_str());
                break;
            case DD_SCDISK: /* scsi disk */
                if (devinfo.flags & DF_LGDSK)
                {
                    ss << "For DD_SCDISK, DF_LGDSK is set. "
                       << " devinfo.flags = " << (unsigned int)devinfo.flags
                       << " blksize = " << devinfo.un.scdk64.blksize
                       << " lo_numblks = " << devinfo.un.scdk64.lo_numblks
                       << " lo_max_request = " << devinfo.un.scdk64.lo_max_request
                       << " segment_size = " << devinfo.un.scdk64.segment_size
                       << " segment_count = " << devinfo.un.scdk64.segment_count
                       << " byte_count = " << devinfo.un.scdk64.byte_count
                       << " hi_numblks = " << devinfo.un.scdk64.hi_numblks
                       << " hi_max_request = " << devinfo.un.scdk64.hi_max_request;

                    mulfactor = pow(2, sizeof(devinfo.un.scdk64.lo_numblks)*8);
                    low = (unsigned int)devinfo.un.scdk64.lo_numblks;
                    high = (unsigned int)devinfo.un.scdk64.hi_numblks;
                    rawsize = ((mulfactor*high) + low) * devinfo.un.scdk64.blksize;
                }
                else
                {
                    ss << "For DD_SCDISK, DF_LGDSK is not set. "
                       << " devinfo.flags = " << (unsigned int)devinfo.flags
                       << " blksize = " << devinfo.un.scdk.blksize
                       << " numblks = " << devinfo.un.scdk.numblks
                       << " max_request = " << devinfo.un.scdk.max_request
                       << " segment_size = " << devinfo.un.scdk.segment_size
                       << " segment_count = " << devinfo.un.scdk.segment_count
                       << " byte_count = " << devinfo.un.scdk.byte_count;
                    low = (unsigned int)devinfo.un.scdk.numblks;
                    rawsize = low * devinfo.un.scdk.blksize;
                }
                DebugPrintf(SV_LOG_DEBUG, "For device %s, %s\n", sVolume.c_str(), ss.str().c_str());
                break;
            default:        /* unknown */
                break;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ioctl IOCINFO Failed volumepath: %s, error: %s\n", sVolume.c_str(), strerror(errno));
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for volume %s with rawsize = " ULLSPEC "\n",FUNCTION_NAME, sVolume.c_str(), rawsize);
    return rawsize;
}

static SV_ULONGLONG getDiskSize(const std::string &sVolume)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with sVolume = %s\n",FUNCTION_NAME, sVolume.c_str());
    SV_ULONGLONG rawsize = 0;

    if(sVolume.empty() == false)
    {
        ACE_Guard<ACE_Recursive_Thread_Mutex> guard(g_lockvolinfo);
        if(guard.locked())
        {
            std::string sRawVolName = GetRawDeviceName(sVolume);
            int fd;
            fd = open(sRawVolName.c_str(), O_NDELAY | O_RDONLY);
            if(fd != 0)
            {
                rawsize = SizeFromIocInfo(fd, sVolume);
                (void)close(fd);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "open() failed for raw volume %s at line %d in file %s with errno = %d\n", sRawVolName.c_str(),
            __LINE__, __FILE__, errno);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s unable to apply lock\n", __LINE__, __FILE__);
        }
    }
    else
    {
       DebugPrintf(SV_LOG_ERROR, "@ LINE %d in FILE %s, invalid arguement VolumeName\n",
            LINE_NO, FILE_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for volume %s with rawsize = " ULLSPEC "\n",FUNCTION_NAME, sVolume.c_str(), rawsize);
    return rawsize;
}
off_t VolumeInfoCollector::GetVolumeSizeInBytes(int fdparam, std::string const & deviceName) 
{
	ACE_stat s;
	std::string  sparsefile;
    bool isnewvirtualvolume = false;
	if(!IsVolPackDevice(deviceName.c_str() ,sparsefile,isnewvirtualvolume))
   		return SizeFromIocInfo(fdparam, deviceName);
    else {
        //Need to change for multi sparse file
	    if(sv_stat( getLongPathName(sparsefile.c_str()).c_str(), &s ) < 0 )
		    return 0;
	    else
		    return s.st_size;
    }
}



off_t VolumeInfoCollector::GetVolumeSizeInBytes(    
    std::string const & deviceName,
    off_t offset,
    off_t sectorSize) 
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_ERROR,"%s not implemented\n",FUNCTION_NAME);
    return offset;
}
off_t VolumeInfoCollector::CalculateFSFreeSpace(const struct statvfs64 &volStatvfs)
{
    off_t freeSpace = static_cast<off_t>(volStatvfs.f_bavail) * static_cast<off_t>(volStatvfs.f_frsize);
    return freeSpace;
}
std::string GetBootDisk()
{    
    /**
    *
    * TODO:
    *
    * Mentioning absolute paths of programs being forked.
    * Currently only for solaris. Need to do the same for
    * linux and unix directories since popen manual page
    * says absolute paths to be given.
    *
    */

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string cmd("getconf BOOT_DEVICE");

    std::stringstream results;

    std::string bootDisk;

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s executePipe failed with errno = %d\n", __LINE__, __FILE__, errno);
        return bootDisk;
    }

    results >> bootDisk;
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bootDisk;
}

bool VolumeInfoCollector::GetMountInfos()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // order is important so do not change it

    // start with the mount table
    // note:  mount -m does not update the mount table so it
    // may be missing things.
    // at this time not sure how to find mount points that
    // are not listed in the mount table
    GetMountVolumeInfos<MountTableEntry>(DEVICE_MOUNTED);

    // if there are any well known mount points
    // that are currently unmounted. this is done last
    // as only care about the ones that are not currently
    // mounted.
    GetMountVolumeInfos<FsTableEntry>(!DEVICE_MOUNTED);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return true;

}
//void VolumeInfoCollector::GetVsnapDevices(void)
//{
//   DebugPrintf(SV_LOG_ERROR,"%s not implemented\n",FUNCTION_NAME);
//}

bool VolumeInfoCollector::GetDiskVolumeInfos(const bool &bneedonlynatives, const bool &bnoexclusions)
{
	bool bgotosnsdiskvolumeinfos = false, bgotvxdmpdiskvolumeinfos = false;
    if (bneedonlynatives || bnoexclusions)
    {
        DebugPrintf(SV_LOG_ERROR, "GetDiskVolumeInfos with only natives and no exclusions is not implemeted on aix\n");
    }
    else
    {
        bgotosnsdiskvolumeinfos = GetOSNameSpaceDiskInfos(false);
        bgotvxdmpdiskvolumeinfos = GetVxDMPDiskInfos();
    }

	return bgotosnsdiskvolumeinfos && bgotvxdmpdiskvolumeinfos;
}
bool VolumeInfoCollector::GetOSNameSpaceDiskInfos(const bool &bnoexclusions)
{
    if (bnoexclusions)
    {
        DebugPrintf(SV_LOG_ERROR, "GetOSNameSpaceDiskInfos with no exclusions is not implemeted on aix\n");
        return false;
    }

	std::stringstream stream ;
	std::string bootDisk(GetBootDisk()) ;

    bootDisk = "/dev/" + bootDisk;
	executePipe("lspv |awk -F' ' '{ print $1 }'", stream) ;
	std::list<std::string> pvs ;
	while( stream.good() && !stream.eof() )
	{
		std::string pv ;
		stream >> pv ;
		if( pv.empty() )
		{
			break ;
		}
		pvs.push_back(pv) ;
	}
	stream.str("") ;
	stream.clear() ;
	std::list<std::string>::iterator beginIter(pvs.begin()) ;
	std::list<std::string>::iterator endIter(pvs.end()) ;
	while( beginIter != endIter )
	{
        std::string vsnapDevName = "/dev/vs/";
        vsnapDevName += *beginIter;
        struct stat stbuf;
        if(lstat(vsnapDevName.c_str(),&stbuf) != 0)
        {

            std::string devName = "/dev/";
            devName += *beginIter;
            struct stat64 volStat ;
            if(stat64(devName.c_str(), &volStat) == 0 )
            {
                bool bisdevicetracked = true;
                int major ;
                major = major64(volStat.st_rdev) ;
                bisdevicetracked = IsDeviceTracked(major, devName);
                if( bisdevicetracked )
                {
                    VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
                    std::string realName ;
                    GetSymbolicLinksAndRealName(devName, symbolicLinks, realName);
                    VolumeInfoCollectorInfo volInfo;
                    volInfo.m_DeviceName = devName ;
                    volInfo.m_DeviceID = m_DeviceIDInformer.GetID(volInfo.m_DeviceName) ;
                    if (volInfo.m_DeviceID.empty())
                    {
                        DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", volInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
                    }
                    volInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(volInfo.m_DeviceName);
                    volInfo.m_RealName = realName;
                    volInfo.m_DeviceType = VolumeSummary::DISK ;
                    m_DeviceTracker.UpdateVendorType(volInfo.m_DeviceName, volInfo.m_DeviceVendor, volInfo.m_IsMultipath);
                    volInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(volInfo.m_DeviceName);
                    volInfo.m_Locked = IsDeviceLocked(devName);
                    volInfo.m_SymbolicLinks = symbolicLinks;
                    volInfo.m_Major = major64(volStat.st_rdev) ;
                    volInfo.m_Minor = minor64(volStat.st_rdev) ;
                    SV_LONGLONG size ;
                    size = getDiskSize(devName);
                    volInfo.m_RawSize = size;
                    volInfo.m_LunCapacity = volInfo.m_RawSize;
                    volInfo.m_BootDisk = (volInfo.m_DeviceName == bootDisk);
                    volInfo.m_SwapVolume = m_SwapVolumes.IsSwapVolume(volInfo);
                    UpdateMountDetails(volInfo);
                    UpdateSwapDetails(volInfo);
                    UpdateLvInsideDevts(volInfo);
                    volInfo.m_Attributes = GetAttributes(volInfo.m_DeviceName);
                    InsertVolumeInfo(volInfo);
                }
            }
        }
		beginIter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return true;
}

bool VolumeInfoCollector::GetOSVolumeManagerInfos(void)
{
   bool bgotvgs = GetLvmVgInfos();
   bool bgotvxvmvgs = GetVxVMVgs();

   return (bgotvgs && bgotvxvmvgs);
}

bool VolumeInfoCollector::GetLvmVgInfos(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool retVal = true ;
    std::list<std::string> vgs ;
    std::stringstream stream ;

    executePipe("lsvg -o", stream) ;
    while( stream.good() && !stream.eof() )
    {
        std::string str ;
        stream >> str ;
        if( str.empty() )
        {
            break ;
        }
        vgs.push_back(str) ;
    }
	std::map<std::string, SV_ULONGLONG> VGtoPPSizeMap;
	getVgNameToPPSize(vgs, VGtoPPSizeMap);
	std::stringstream lspvOutputStream;
	executePipe("lspv", lspvOutputStream) ;
	std::string lspvOutput = lspvOutputStream.str();
    std::list<std::string>::iterator vgBegIter(vgs.begin()) ;
    std::list<std::string>::iterator vgEndIter(vgs.end()) ;
    while( vgBegIter != vgEndIter )
    {
		DebugPrintf(SV_LOG_DEBUG, "Volume Group Name %s\n", vgBegIter->c_str()) ;
        stream.clear() ;
		Vg_t lvmVg;
        lvmVg.m_Vendor = VolumeSummary::LVM;
        lvmVg.m_Name = *vgBegIter ;
		SV_ULONGLONG ppsize = 0;
		if(VGtoPPSizeMap.find(lvmVg.m_Name) != VGtoPPSizeMap.end())
		{
			ppsize = VGtoPPSizeMap.find(lvmVg.m_Name)->second;;
		}
		std::list<std::string> pvNameList;
		getPvNames(*vgBegIter, lspvOutput, pvNameList);
		std::list<std::string>::iterator begIter = pvNameList.begin();
		std::list<std::string>::iterator endIter = pvNameList.end();
		while(begIter != endIter)
		{
			std::string pvName = *begIter;
			if(pvName.empty() == false)
			{
				DebugPrintf(SV_LOG_DEBUG, "PV Name %s\n", pvName.c_str()) ;
				
				pvName = "/dev/" + *begIter ;
				struct stat64 pvStat;
				if ((0 == stat64(pvName.c_str(), &pvStat)) && pvStat.st_rdev)
				{
					lvmVg.m_InsideDevts.insert(pvStat.st_rdev);
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "stat failed for %s\n", pvName.c_str());
				}
			}
			begIter++;
		}
		stream.clear() ;
		stream.str("") ;
        executePipe("/usr/sbin/lsvg -l " + *vgBegIter + " | tail +3" , stream) ;
        while( stream.good() && !stream.eof() )
        {
			std::string lvStateOk("Opened/syncd") ;
			char line[1024] ;
            std::string lvname, lvtype, lvstate, mountPt , lps, pps, pvs ;
			stream.getline(line, 1024) ;
			std::stringstream linestream(line) ;
            linestream >> lvname >> lvtype >> lps >> pps >> pvs >> lvstate >> mountPt ;
	
            if( lvname.empty() )
            {
                break ;
            }
            std::string devName( "/dev/" + lvname ) ;
            struct stat64 volStat ;
			Lv_t lv;
            lv.m_Vendor = lvmVg.m_Vendor;
            lv.m_VgName = *vgBegIter;
            lv.m_Name = devName;
			lv.m_Size = ppsize * boost::lexical_cast<SV_ULONGLONG>(pps);
            if(stat64(devName.c_str(), &volStat) == 0 )
            {
				DebugPrintf(SV_LOG_DEBUG, "Stat successful for %s\n", devName.c_str()) ;
	            struct stat64 lvStat;
	            if ((0 == stat64(devName.c_str(), &lvStat)) && lvStat.st_rdev)
	            {
	                lv.m_Devt = lvStat.st_rdev; 
					lv.m_Available = true ;
	                UpdateLvFromVgLvInfo(*vgBegIter, lv);
	                InsertLv(lvmVg.m_Lvs, lv);
	            }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Stat failed for %s\n" ,devName.c_str()) ;
            }
        }
		InsertVg(lvmVg);
        vgBegIter++ ;
    }
	return retVal ;
}


void VolumeInfoCollector::CollectScsiHCTL(void)
{
}


void VolumeInfoCollector::UpdateHCTLFromDevice(const std::string &devicename)
{
}


bool VolumeInfoCollector::GetVxDMPDiskInfos(void)
{
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    bool bretval = true;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;



    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    dentresult = NULL;

    dirp = opendir(VXDMP_PATH.c_str());

    if (dirp)
    {
        dentp = (struct dirent *)calloc(direntsize, 1);

        if (dentp)
        {
            while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
            {
                if (strcmp(dentp->d_name, ".") &&
                    strcmp(dentp->d_name, ".."))                /* skip . and .. */
                {
                    std::string vxdmpdisk = VXDMP_PATH + "/" + dentp->d_name;
                    struct stat64 volStat;
                    if ((0 == stat64(vxdmpdisk.c_str(), &volStat)) && volStat.st_rdev)
                    {
                        int major = major64(volStat.st_rdev) ;
                        if(IsDeviceTracked(major, vxdmpdisk))
                        {
                            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
                            std::string realName ;
                            GetSymbolicLinksAndRealName(vxdmpdisk, symbolicLinks, realName);
                            VolumeInfoCollectorInfo volInfo;
                            volInfo.m_DeviceName = vxdmpdisk ;
                            volInfo.m_DeviceID = m_DeviceIDInformer.GetID(volInfo.m_DeviceName) ;
                            if (volInfo.m_DeviceID.empty())
                            {
                                DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", volInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
                            }
                            volInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(volInfo.m_DeviceName);
                            volInfo.m_RealName = realName;
                            volInfo.m_DeviceType = VolumeSummary::DISK ;
                            m_DeviceTracker.UpdateVendorType(volInfo.m_DeviceName, volInfo.m_DeviceVendor, volInfo.m_IsMultipath);
                            volInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(volInfo.m_DeviceName);
                            volInfo.m_Locked = IsDeviceLocked(vxdmpdisk);
                            volInfo.m_SymbolicLinks = symbolicLinks;
                            volInfo.m_Major = major64(volStat.st_rdev) ;
                            volInfo.m_Minor = minor64(volStat.st_rdev) ;
                            SV_LONGLONG size ;
                            size = getDiskSize(vxdmpdisk);
                            volInfo.m_RawSize = size;
                            volInfo.m_LunCapacity = volInfo.m_RawSize;
                            volInfo.m_SwapVolume = m_SwapVolumes.IsSwapVolume(volInfo);
                            UpdateMountDetails(volInfo);
                            UpdateSwapDetails(volInfo);
                            UpdateLvInsideDevts(volInfo);
                            volInfo.m_Attributes = GetAttributes(volInfo.m_DeviceName);
                            InsertVolumeInfo(volInfo);
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s %s is not tracked.\n", __LINE__, __FILE__, vxdmpdisk.c_str());
                        }
                    } /* end of stat */
                    else 
                    {
                        DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s stat on %s failed with error %d\n", __LINE__, __FILE__, vxdmpdisk.c_str(), errno);
                    }
                } /* end of skipping . and .. */
                memset(dentp, 0, direntsize);
            } /* end of while readdir_r */
            free(dentp);
        }
        else
        {
            bretval = false;
        }
        closedir(dirp); 
    } /* end of if (dirp) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bretval;
}
