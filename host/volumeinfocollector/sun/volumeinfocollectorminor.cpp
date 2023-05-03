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
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/errno.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <iterator>
#include <functional>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/phoenix1_primitives.hpp>
#include <boost/spirit/include/phoenix1_functions.hpp>

#include "volumeinfocollectorinfo.h"
#include "volumeinfocollector.h"
#include "mountpointentry.h"
#include "executecommand.h"
#include "utilfdsafeclose.h"
#include "utildirbasename.h"
#include <sys/types.h>
#include <dirent.h>

#include <dlfcn.h>
#include <link.h>

#include <ace/Guard_T.h>
#include "ace/Thread_Mutex.h"
#include "ace/Recursive_Thread_Mutex.h"
#include "diskpartition.h"
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "devicemajorminordec.h"
#include "logicalvolumes.h"
#include "volumegroupsettings.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"

using namespace BOOST_SPIRIT_CLASSIC_NS;

/** 
* The below header file is not present in solaris 8
* and may not be present on solaris 9 but our build 
* machine has it. so using dlopen
*/
#ifndef SV_SUN_5DOT8
#include "sys/efi_partition.h"
#endif /* SV_SUN_5DOT8 */


ACE_Recursive_Thread_Mutex g_lockforefilibandreadvtoc;

bool VolumeInfoCollector::GetMountInfos()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // order is important so do not change it
    
    // start with the mount table
    // TODO:
    // note:  mount -m does not update the mount table so it
    // may be missing things.
    // at this time not sure how to find mount points that
    // are not listed in the mount table
    bool bgotmntent = GetMountVolumeInfos<MountTableEntry>(DEVICE_MOUNTED);

    // if there are any well known mount points
    // that are currently unmounted. this is done last
    // as only care about the ones that are not currently
    // mounted.
    bool bgotfsent = GetMountVolumeInfos<FsTableEntry>(!DEVICE_MOUNTED);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    bool bgotmountinfos = (bgotmntent && bgotfsent);

    return bgotmountinfos;
}


bool VolumeInfoCollector::GetDiskVolumeInfos(const bool &bneedonlynatives, const bool &bnoexclusions)
{
    bool bretval = false;
    if (bneedonlynatives || bnoexclusions)
    {
        DebugPrintf(SV_LOG_ERROR, "GetDiskVolumeInfos with only natives and no exclusions is not implemeted on solaris\n");
    }
    else
    {
        bretval = true;
        for (int i = 0; i < NELEMS(DiskDirs); i++)
        {
            bool bgetdiskinfos = GetDiskInfosInDir(DiskDirs[i]);
            if (!bgetdiskinfos)
            {
                bretval = false;
            }
        } 

        bool bgotdiddevs = GetDidDevices();
        if (bretval)
        {
            bretval = bgotdiddevs;
        }
    }

    return bretval;
}


bool VolumeInfoCollector::GetDiskInfosInDir(const char *pdiskdir)
{
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    bool bretval = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    dentresult = NULL;
    dirp = opendir(pdiskdir);
    if (dirp)
    {
       dentp = (struct dirent *)calloc(direntsize, 1);
       if (dentp)
       {
           while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
           {
               E_DISKTYPE edsktyp = E_S2ISFULL;
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, ".."))
               {
                   std::string device = pdiskdir;
                   device += UNIX_PATH_SEPARATOR;
                   device += dentp->d_name;
                   if (!CanBeFullDisk(device, &edsktyp))
                   {
                       continue;
                   }
                   DiskPartitions_t diskandpartitions;    
                   GetDiskAndItsPartitions(pdiskdir, dentp->d_name, edsktyp, 
                                           diskandpartitions, m_EfiInfo.GetEfiReadAddress(), 
                                           m_EfiInfo.GetEfiFreeAddress());
                   FillDiskVolInfos(diskandpartitions);
               } /* end of skipping . and .. */
               memset(dentp, 0, direntsize);
           } /* end of while readdir_r */
           free(dentp);
       } /* endif of if (dentp) */
       else
       {
           bretval = false;
       }
       closedir(dirp); 
    } /* end of if (dirp) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bretval;
}


bool VolumeInfoCollector::GetDidDevices(void)
{
    bool bgotdids = true;

    std::vector<std::string> diddevs;
    std::string scdidadmcmd(m_localConfigurator.getScdidadmCmd());
    std::string awkcmd(m_localConfigurator.getAwkCmd());
    std::string cmd(scdidadmcmd);
    cmd += " ";
    cmd += SCDIDADMARGS;
    cmd += " ";
    cmd += "|";
    cmd += " ";
    cmd += awkcmd;
    cmd += " ";
    cmd += DIDPATTERN;

    /*
    TODO: below man says cluster read permission and also in global zone;
          what about non global zone ? 
    -l
    Lists the local devices in the DID configuration.
    You can use this option only in the global zone.
    The output of this command can be customized using the -o option. When no -o options are specified, 
    the default listing displays the instance number, the local fullpath, and the fullname.
    You need solaris.cluster.device.read RBAC authorization to use this command option. See rbac(5).

    -bash-3.00# scdidadm -l | awk '{for (i = 1; i <= NF; i++) if ($i ~ /\/dev\/did/) print $i}'
    /dev/did/rdsk/d2
    /dev/did/rdsk/d4
    /dev/did/rdsk/d5
    /dev/did/rdsk/d6
    /dev/did/rdsk/d7
    */
    std::stringstream results;
    if (executePipe(cmd, results)) 
    {
        while (!results.eof())
        {
            std::string didtok;
            results >> didtok;
            if (didtok.empty())
            {
                break;
            }
            std::string didname = GetBlockDeviceName(didtok);
            didname += FULLDISKSLICE;
            diddevs.push_back(didname);
        }
    }
     
    E_DISKTYPE edsktyp = E_S2ISFULL;
    for (int i = 0; i < NELEMS(DidDirs); i++)
    {
        std::vector<std::string>::const_iterator iter = diddevs.begin();
        std::vector<std::string>::const_iterator end = diddevs.end();
        for ( /* empty */ ; iter != end; iter++)
        {
            std::string basename = BaseName(*iter);
            DiskPartitions_t diskandpartitions;
            GetDiskAndItsPartitions(DidDirs[i], basename, 
                                    edsktyp, diskandpartitions, 
                                    m_EfiInfo.GetEfiReadAddress(), m_EfiInfo.GetEfiFreeAddress()); 
            FillDiskVolInfos(diskandpartitions);
        }
    } 

    return bgotdids;
}


/**
*
* TODO: Remove
*
* This function is not getting used at all anywhere.
* Hence not adding dlopen changes and the lock
*
*/

unsigned int VolumeInfoCollector::GetSectorSize(int fdparam, std::string const & deviceName) const
{
    std::string sRawVolName = GetRawDeviceName(deviceName);
    int fd;
    int read_vtoc_retval = 0;
    struct vtoc vt;
    unsigned int sectorsize = 0;



    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    fd = open(sRawVolName.c_str(), O_RDONLY);

    if (-1 == fd)
    {
        DebugPrintf(SV_LOG_ERROR, "open() failed for raw volume %s at line %d in file %s with errno = %d\n", sRawVolName.c_str(),
        __LINE__, __FILE__, errno); 
    }
    else
    {
        read_vtoc_retval = read_vtoc(fd, &vt);

        if (read_vtoc_retval < 0)
        {
            DebugPrintf(SV_LOG_ERROR, "read_vtoc() failed for raw volume %s at line %d in file %s with errno = %d\n", sRawVolName.c_str(),
                                       __LINE__, __FILE__, errno);
        }
        else
        {
            if (0 == vt.v_sectorsz)
            {
                sectorsize = DEV_BSIZE;
            }
            else
            { 
                sectorsize = vt.v_sectorsz;
            }
        }
        close(fd);
    }
 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return sectorsize;
}


off_t VolumeInfoCollector::GetVolumeSizeInBytes(int fd, std::string const & deviceName)
{
    unsigned long long rawSize = 0;
    std::string sRawVolName = GetRawDeviceName(deviceName);
    int read_vtoc_retval = 0;
    struct vtoc vt;



    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(g_lockforefilibandreadvtoc);

    if (!guard.locked())
    {
        DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s unable to apply lock\n", __LINE__, __FILE__);
        return 0;    
    }

    read_vtoc_retval = read_vtoc(fd, &vt);

    if (read_vtoc_retval < 0)
    {
        #ifndef SV_SUN_5DOT8
        if (VT_ENOTSUP == read_vtoc_retval)
        {
            dk_gpt_t *vt = NULL;
            int (*ptr_efi_alloc_and_read)(int fd, dk_gpt_t **vtoc) = NULL;
            void (*ptr_efi_free)(dk_gpt_t *vtoc) = NULL;
            void *pv_efi_alloc_and_read = NULL, *pv_efi_free = NULL;

            pv_efi_alloc_and_read = m_EfiInfo.GetEfiReadAddress();
            pv_efi_free = m_EfiInfo.GetEfiFreeAddress();
            if (pv_efi_alloc_and_read && pv_efi_free)
            {
                ptr_efi_alloc_and_read = (int (*)(int fd, dk_gpt_t **vtoc))pv_efi_alloc_and_read;
                ptr_efi_free = (void (*)(dk_gpt_t *vtoc))pv_efi_free;
                int efi_alloc_and_read_rval = ptr_efi_alloc_and_read(fd, &vt);

                if ((efi_alloc_and_read_rval >= 0) && vt)
                {
                    /**
                    *
                    * If EFI raw disk is there or efi_alloc_and_read has succeeded and
                    * V_UNASSIGNED is value of p_tag, this means that is a full disk
                    *
                    */
                    if (FULLEFIIDX == efi_alloc_and_read_rval)
                    {
                        DebugPrintf(SV_LOG_DEBUG,"@ LINE %d in FILE %s, %s is a full efi label disk\n", LINE_NO, FILE_NAME, deviceName.c_str());
                        rawSize = GetValidSize(sRawVolName, vt->efi_last_lba + 1, vt->efi_lbasize, E_ATLESSTHANSIZE);
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"@ LINE %d in FILE %s, %s is a partition on efi label disk\n", LINE_NO, FILE_NAME, 
                                    deviceName.c_str());
                        rawSize = ((unsigned long long)vt->efi_parts[efi_alloc_and_read_rval].p_size) *
                                  ((unsigned long long)vt->efi_lbasize);
                    }
                    ptr_efi_free(vt);
                } /* end of if ((efi_alloc_and_read_rval >= 0) && vt) */
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "reading efi label failed for raw volume %s at line %d in file %s with errno = %d\n", 
                                              sRawVolName.c_str(), __LINE__, __FILE__, errno);
                }
            } /* end if of  if (pv_efi_alloc_and_read && pv_efi_free) */
            else
            {
                DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s efi_free or efi_alloc_and_read are not present in libefi.so\n", 
	     	           __LINE__, __FILE__);
            }
        }
        else
        {
        #endif /* SV_SUN_5DOT8 */
            DebugPrintf(SV_LOG_ERROR, "read_vtoc() failed for raw volume %s at line %d in file %s with errno = %d\n", sRawVolName.c_str(),
                                       __LINE__, __FILE__, errno);
        #ifndef SV_SUN_5DOT8
        }
        #endif /* SV_SUN_5DOT8 */
    }
    else
    {
        /* here not verifying the more size since this function is used in vsnaps, lvs and volpacks
         * which give correct size; Also may get called for disk/partition if their size is 0 from
         * calculate free space; which will never be the case since if size is non zero, we are 
         * sending disks / partitions ;
         * for vsnaps, the size is always got from source and should not be more */
        unsigned long long secsz = vt.v_sectorsz ? vt.v_sectorsz : DEV_BSIZE;
        rawSize = ((unsigned long long)vt.v_part[read_vtoc_retval].p_size) * ((unsigned long long)secsz);
    }
 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rawSize;
}


off_t VolumeInfoCollector::GetVolumeSizeInBytes(    
    std::string const & deviceName,
    off_t offset,
    off_t sectorSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string rawDeviceName = GetRawDeviceName(deviceName);
    int fd = open(rawDeviceName.c_str(), O_RDONLY);

    if (-1 == fd) {
        return 0;
    }

    boost::shared_ptr<void> fdGuard(static_cast<void*>(0), boost::bind(fdSafeClose, fd));
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return GetVolumeSizeInBytes(fd, deviceName);
}


//void VolumeInfoCollector::GetVsnapDevices(void)
//{
//    DIR *dirp;
//    struct dirent *dentp, *dentresult;
//    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
//    const std::string VsnapDskDir = "/dev/vs/dsk";

//    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//    dentresult = NULL;
//    dirp = opendir(VsnapDskDir.c_str());
//    if (dirp)
//    {
//       dentp = (struct dirent *)calloc(direntsize, 1);
//       if (dentp)
//       {
//           while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
//           {
//               if (strcmp(dentp->d_name, ".") &&
//                   strcmp(dentp->d_name, ".."))                /* skip . and .. */
//               {
//                   std::string vsnapdskdevice = VsnapDskDir + "/" + dentp->d_name;
//                   std::string vsnaprdskdevice = GetRawDeviceName(vsnapdskdevice);
//                   int fd = -1;
//                   struct stat64 volStat;

//                  if ((-1 != stat64(vsnapdskdevice.c_str(), &volStat))) // && (major == GetVsnapMajorNumber())) 
//                   {
//                       fd = open(vsnaprdskdevice.c_str(), O_NDELAY | O_RDONLY);
//                       if (-1 != fd) 
//                       {
//                           std::string realName;
//                           VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
//                          VolumeInfoCollectorInfo volInfo;
//                           GetSymbolicLinksAndRealName(vsnapdskdevice, symbolicLinks, realName);
//                           volInfo.m_DeviceName = vsnapdskdevice;
//                           volInfo.m_RealName = realName;
//                           volInfo.m_Locked = IsDeviceLocked(vsnapdskdevice);
//                           volInfo.m_DeviceType = VolumeSummary::VSNAP_MOUNTED;
//                           volInfo.m_DeviceVendor = VolumeSummary::INMVSNAP;
//                           volInfo.m_RawSize = GetVolumeSizeInBytes(fd, vsnapdskdevice);
//                           volInfo.m_LunCapacity = volInfo.m_RawSize;
//                           volInfo.m_Major = major(volStat.st_rdev);
//                          volInfo.m_Minor = minor(volStat.st_rdev);
//                           volInfo.m_SymbolicLinks = symbolicLinks;
//                           UpdateMountDetails(volInfo);
    
//                           InsertVolumeInfo(volInfo);
//                           close(fd);
//                       }
//                   }
//               } /* end of skipping . and .. */
//               memset(dentp, 0, direntsize);
//           } /* end of while readdir_r */
//           free(dentp);
//       } /* end of if (dentp) */
//       closedir(dirp); 
//   } /* end of if (dirp) */
//    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
//}

off_t VolumeInfoCollector::CalculateFSFreeSpace(const struct statvfs64 &volStatvfs)
{
    off_t freeSpace = static_cast<off_t>(volStatvfs.f_bavail) * static_cast<off_t>(volStatvfs.f_frsize);
    return freeSpace;
}


bool VolumeInfoCollector::GetOSVolumeManagerInfos(void)
{
    /* NOTE: order is very important here. please do not change */
    bool bgotsvmlvsandvgs = GetSvmLvsAndVgs();
    bool bgotvxvmvgs = GetVxVMVgs();
    bool bgotzpools = GetZpools();

    bool bgotvminfos = (bgotsvmlvsandvgs && bgotvxvmvgs && bgotzpools);
    return bgotvminfos;
}


bool VolumeInfoCollector::GetSvmLvsAndVgs(void)
{
    bool bgotsharedds = GetSharedDiskSets();
    bool bgotlocalds = GetLocalDiskSetLvs();
 
    bool bgotsvms = (bgotsharedds && bgotlocalds);
    return bgotsvms;
}


bool VolumeInfoCollector::GetSharedDiskSets(void)
{
    bool bgotsharedds = true;

    std::string cmd("/usr/sbin/metaset 2> /dev/null | /usr/bin/grep -v \'^$\'");
    std::stringstream results;

    /* 
    -bash-3.00# metaset
    *
    * Set name = blue, Set number = 1
    *
    * Host                Owner
    *   Sol10-64VM_ONE     Yes
    *
    * Drive    Dbase
    *
    * c1t4d0   Yes
    *
    * c1t6d0   Yes
    *
    * Set name = myset, Set number = 2
    *
    * Host                Owner
    *   Sol10-64VM_ONE     Yes
    *
    * Drive    Dbase
    *
    * c1t5d0   Yes
    */

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR,"Unable to run metaset. executing it failed with errno = %d\n", errno);
        return false;
    }

    if (results.str().empty()) {
        DebugPrintf(SV_LOG_DEBUG,"metaset has returned empty output\n");
        return true;
    }
  
    const std::string SETTOK = "Set";
    const std::string SPACETOK = " ";
    const std::string NAMETOK = "name";
    /* TODO: should this be const ? */
    std::string setnametoken = SETTOK;
    setnametoken += SPACETOK;
    setnametoken += NAMETOK;;
    setnametoken += SPACETOK;
    setnametoken += EQUALS;

    size_t currentStartPos = 0;
    size_t offset = 0;
    while ((std::string::npos != currentStartPos) && ((currentStartPos + offset) < results.str().length()))
    {
        size_t nextStartPos = results.str().find(setnametoken, currentStartPos + offset);
        if (0 == offset)
        {
            offset = setnametoken.size();
        }
        else
        {
            size_t metasetstreamlen = (std::string::npos == nextStartPos) ? std::string::npos : (nextStartPos - currentStartPos);
            std::string metasetstr = results.str().substr(currentStartPos, metasetstreamlen);
            FillMetaset(metasetstr);
        }
        currentStartPos = nextStartPos;
    }

    return bgotsharedds;
}


bool VolumeInfoCollector::FillMetaset(const std::string &metasetstr)
{
    /* 
    * Below shows that metaset does not work with vxdmp disks
    * -bash-3.00# metaset -s orange -a /dev/vx/dmp/disk_5s2
    * metaset: Solaris10U8SRC: /dev/rvx/dmp/disk_5s2s0: No such file or directory
    */
    bool bfilled = true;
    std::stringstream metasetstream(metasetstr);
    std::string strfirstline;
    std::getline(metasetstream, strfirstline);
    const std::string DRIVETOK = "Drive";
    const std::string DBASETOK = "Dbase";
    std::stringstream firstline(strfirstline);
        
    std::string settok, nametok, eqtok, setname;
    firstline >> settok >> nametok >> eqtok >> setname;
    /* Also a set name may not have comma; its verified */
    const char SETNAMELASTCHAR = ',';
    size_t setnamelen = setname.length();
    if (SETNAMELASTCHAR == setname[setnamelen- 1])
    {
        setname.erase(setnamelen - 1);
    }

    Vg_t diskset;
    diskset.m_Vendor = VolumeSummary::SVM;
    diskset.m_Name = setname;

    while (!metasetstream.eof())
    {
        std::string eachline;
        std::getline(metasetstream, eachline);
        if (eachline.empty())
        {
            break;
        }
        std::stringstream line(eachline);
        std::string drivetok, dbasetok;
        line >> drivetok >> dbasetok; 
        /* added find instead of equals because:drivetok
           may not be fully printed (Driv)
         -bash-3.00# metaset

         Set name = oracle, Set number = 2

         Host                Owner
           Solaris10VMCL1     Yes
           Solaris10VMCL2
             
         Driv Dbase
             
         d8   Yes
             
         d9   Yes
        */

        if ((0 == DRIVETOK.find(drivetok)) && (DBASETOK == dbasetok))
        {
            break;
        }
    }
        
    while (!metasetstream.eof())
    {
        std::string eachline;
        std::getline(metasetstream, eachline);
        if (eachline.empty())
        {
            break;
        }
        std::stringstream line(eachline);
        std::string drive, dbpresent;
        line >> drive >> dbpresent;
        if (drive.empty())
        {
            break;
        }
        else
        {
            GetDiskSetDiskDevts(drive, diskset.m_InsideDevts);
        }
    }
    GetLvsInSharedDiskSet(diskset);
    /* do not collect diskset without lvs or inside disks/partitions */
    if (!diskset.m_Lvs.empty() || !diskset.m_InsideDevts.empty())
    {
        InsertVg(diskset);
    }

    return bfilled;
}


bool VolumeInfoCollector::GetLocalDiskSetLvs(void)
{
    bool bgotds = true;

    Vg_t diskset;
    diskset.m_Vendor = VolumeSummary::SVM;
    diskset.m_Name = SVMLOCALDISKSETNAME;

    GetMetadbInsideDevts(diskset);
    GetMetastatInfo(diskset);

    if (!diskset.m_Lvs.empty() || !diskset.m_InsideDevts.empty())
    {
        InsertVg(diskset);
    }
    return bgotds;
}


bool VolumeInfoCollector::GetZpools(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ZpoolsWithStorageDevices_t zpoolswithstoragedevices;

    bool gotzpools = GetZpoolsWithStorageDevices(zpoolswithstoragedevices);
    if (gotzpools) {
        for (ConstZpoolsWithStorageDevicesIter_t cit = zpoolswithstoragedevices.begin(); cit != zpoolswithstoragedevices.end(); cit++) {
            const std::string &poolname = cit->first;
            const svector_t &storagedevices = cit->second;
            Vg_t zpoolvg;
            zpoolvg.m_Vendor = VolumeSummary::ZFS;
            zpoolvg.m_Name = poolname;
            DebugPrintf(SV_LOG_DEBUG, "For zpool %s, the storage devices are:\n", poolname.c_str());
            for (constsvectoriter_t cvit = storagedevices.begin(); cvit != storagedevices.end(); cvit++) {
                const std::string &device = *cvit;
                struct stat st;
                if (0 == stat(device.c_str(), &st)) {
                    DebugPrintf(SV_LOG_DEBUG, "%s\n", device.c_str());
                    zpoolvg.m_InsideDevts.insert(st.st_rdev);
                }
            }
            InsertVg(zpoolvg);
        }
    } else
        DebugPrintf(SV_LOG_ERROR,"Unable to get zpools in volumeinfocollector.\n");

    DebugPrintf(SV_LOG_DEBUG,"EXITING %s\n",FUNCTION_NAME);
    return gotzpools;
}


void VolumeInfoCollector::FillDiskVolInfos(const DiskPartitions_t &diskandpartitions)
{
    bool bshallreportdisk;
    std::string strnameofdisk;
    struct stat64 dVolStat;
    bool bisdevicetracked = true;                  
    VolumeInfoCollectorInfo dVolInfo;
    std::string bootDisk = m_BootDiskInfo.GetBootDisk();
    bool getfsfromtracker = false;
    const std::string ZFS = "zfs";

    ConstDiskPartitionIter fulldiskiter;
    fulldiskiter = find_if(diskandpartitions.begin(), diskandpartitions.end(), DPEqDeviceType(VolumeSummary::DISK));
    if ((diskandpartitions.end() != fulldiskiter) && (!stat64((*fulldiskiter).m_name.c_str(), &dVolStat))
        && (dVolStat.st_rdev))
    {
        strnameofdisk = (*fulldiskiter).m_name;
        bisdevicetracked = IsDeviceTracked(major(dVolStat.st_rdev), strnameofdisk);
        if (bisdevicetracked)
        {
            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(strnameofdisk, symbolicLinks, realName);
            dVolInfo.m_DeviceName = strnameofdisk;
            dVolInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(dVolInfo.m_DeviceName);
            dVolInfo.m_DeviceID = m_DeviceIDInformer.GetID(dVolInfo.m_DeviceName);
            if (dVolInfo.m_DeviceID.empty())
            {
                DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", dVolInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
            }

            dVolInfo.m_RealName = realName;
            dVolInfo.m_DeviceType = (*fulldiskiter).m_devicetype;
            dVolInfo.m_FormatLabel = (*fulldiskiter).m_formatlabel;
            m_DeviceTracker.UpdateVendorType(dVolInfo.m_DeviceName, dVolInfo.m_DeviceVendor, dVolInfo.m_IsMultipath);
            dVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(dVolInfo.m_DeviceName);
            if (!bootDisk.empty())
            {
                dVolInfo.m_BootDisk = (std::string::npos != realName.find(bootDisk));
            }
            dVolInfo.m_RawSize = (*fulldiskiter).m_size; 
            dVolInfo.m_LunCapacity = m_LunCapacityInformer.GetCapacity(dVolInfo.m_DeviceName, dVolInfo.m_RawSize); 
            dVolInfo.m_Locked = IsDeviceLocked(strnameofdisk);
            dVolInfo.m_SymbolicLinks = symbolicLinks;
            dVolInfo.m_Major = major(dVolStat.st_rdev);
            dVolInfo.m_Minor = minor(dVolStat.st_rdev);
            UpdateMountDetails(dVolInfo);
            UpdateSwapDetails(dVolInfo);
            UpdateLvInsideDevts(dVolInfo);
            if (VolumeSummary::ZFS == dVolInfo.m_VolumeGroupVendor) 
            {
                if (VolumeSummary::EFI == dVolInfo.m_FormatLabel)
                    getfsfromtracker = true;
                else
                    dVolInfo.m_FileSystem = ZFS;
            }
            dVolInfo.m_Attributes = GetAttributes(dVolInfo.m_DeviceName);
        } /* End of if if (bisdevicetracked) */
        else
        {
            DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s device %s is not tracked\n", __LINE__, __FILE__, strnameofdisk.c_str());
        }
    }  /* End of if (diskandpartitions.end() != fulldiskiter) */

    if (bisdevicetracked)
    {
        bool bispartitiontracked = false;
        VolumeSummary::WriteCacheState wcstate = VolumeSummary::WRITE_CACHE_DONTKNOW;
        VolumeSummary::Vendor vendor = VolumeSummary::UNKNOWN_VENDOR;
        bool bismultipath = false;
        std::string diskid;
        VolumeInfoCollectorInfos_t pVolInfos;
        
        if (!dVolInfo.m_DeviceName.empty())
        {
            bispartitiontracked = true;
            wcstate = dVolInfo.m_WCState;
            vendor = dVolInfo.m_DeviceVendor;
            bismultipath = dVolInfo.m_IsMultipath;
            diskid = dVolInfo.m_DeviceID;
        }

        Attributes_t attributes;
        // now check for partitions
        for (ConstDiskPartitionIter iter = diskandpartitions.begin(); iter != diskandpartitions.end(); ++iter)
        {
            struct stat64 pVolStat;
            if ((iter != fulldiskiter) && (!stat64((*iter).m_name.c_str(), &pVolStat))
                && (pVolStat.st_rdev))
            {
                std::string partitionName = (*iter).m_name;
                if (false == bispartitiontracked)
                {
                    bispartitiontracked = IsDeviceTracked(major(pVolStat.st_rdev), partitionName);
                    if (bispartitiontracked)
                    {
                        /* partition answers all ioctls that a disk can; 
                         * hence we can get disk id, write cache state etc 
                         * from partitions too */
                        diskid = m_DeviceIDInformer.GetID(partitionName);
                        if (diskid.empty())
                        {
                            DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", partitionName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
                        }

                        wcstate = m_WriteCacheInformer.GetWriteCacheState(partitionName);
                        m_DeviceTracker.UpdateVendorType(partitionName, 
                                                         vendor, 
                                                         bismultipath);
                        attributes = GetAttributes(partitionName);
                    }
                }

                if (bispartitiontracked)
                {
                    std::string realName;
                    VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
                    GetSymbolicLinksAndRealName(partitionName, symbolicLinks, realName);
                    VolumeInfoCollectorInfo pVolInfo;
                    pVolInfo.m_DeviceName = partitionName;
                    pVolInfo.m_DeviceID = GetPartitionID(diskid, pVolInfo.m_DeviceName);
                    pVolInfo.m_WCState = wcstate;
                    pVolInfo.m_DeviceVendor = vendor;
                    pVolInfo.m_IsMultipath = bismultipath;
                    pVolInfo.m_RealName = realName;
                    pVolInfo.m_DeviceType = (*iter).m_devicetype;
                    pVolInfo.m_FormatLabel = (*iter).m_formatlabel;
                    pVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(pVolInfo.m_DeviceName);                
                    if (!bootDisk.empty())
                    {
                        pVolInfo.m_BootDisk = (std::string::npos != realName.find(bootDisk));
                    }
                    pVolInfo.m_RawSize = (*iter).m_size;
                    pVolInfo.m_LunCapacity = pVolInfo.m_RawSize + m_LunCapacityInformer.GetLengthForLunMakeup();
                    pVolInfo.m_Locked = IsDeviceLocked(partitionName);
                    pVolInfo.m_PhysicalOffset = (*iter).m_physicaloffset;
                    pVolInfo.m_SymbolicLinks = symbolicLinks;
                    pVolInfo.m_Major = major(pVolStat.st_rdev);
                    pVolInfo.m_Minor = minor(pVolStat.st_rdev);
                    pVolInfo.m_IsStartingAtBlockZero = (*iter).m_ispartitionatblockzero;
                    UpdateMountDetails(pVolInfo);
                    UpdateSwapDetails(pVolInfo);
                    UpdateLvInsideDevts(pVolInfo);
                    if (VolumeSummary::ZFS == pVolInfo.m_VolumeGroupVendor)
                        pVolInfo.m_FileSystem = ZFS;
                    else if (getfsfromtracker)
                        pVolInfo.m_FileSystem = m_FileSystemTracker.GetFileSystem(pVolInfo.m_DeviceName);
                    if (dVolInfo.m_DeviceName.empty())
                    {
                        pVolInfo.m_Attributes = attributes;
                    }

                    pVolInfos.insert(std::make_pair(pVolInfo.m_DeviceName, pVolInfo));
                }
                else
                {
                    DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s device %s is not tracked\n", __LINE__, __FILE__, partitionName.c_str());
                }
            } 
        } /* End of for */

        VolInfoIter dPIter = find_if(pVolInfos.begin(), pVolInfos.end(), EqDeviceType(VolumeSummary::DISK_PARTITION));
        if (pVolInfos.end() != dPIter)
        {
            VolumeInfoCollectorInfo &dPVolInfo = dPIter->second;
            dPVolInfo.m_DeviceID = m_DeviceIDInformer.GetID(dPVolInfo.m_DeviceName);
            if (dPVolInfo.m_DeviceID.empty())
            {
                DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", dPVolInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
            }
            for (ConstVolInfoIter pIter = pVolInfos.begin(); pIter != pVolInfos.end(); pIter++)
            {
                const VolumeInfoCollectorInfo &pVolInfo = pIter->second;
                if (VolumeSummary::DISK_PARTITION != pVolInfo.m_DeviceType)
                {
                    InsertVolumeInfoChild(dPVolInfo, pVolInfo);
                }
            }
            VolumeInfoCollectorInfo &topVolInfo = dPVolInfo;
            if (!dVolInfo.m_DeviceName.empty())
            {
                InsertVolumeInfoChild(dVolInfo, dPVolInfo);
                topVolInfo = dVolInfo;
            }
            UpdateAttrsWithChildren(topVolInfo);
            InsertVolumeInfo(topVolInfo);
        }
        else
        {
            if (!dVolInfo.m_DeviceName.empty())
            {
                InsertVolumeInfos(dVolInfo.m_Children, pVolInfos);
                UpdateAttrsWithChildren(dVolInfo);
                InsertVolumeInfo(dVolInfo);
            }
            else
            {
                InsertVolumeInfos(m_VolumeInfos, pVolInfos);
            }
        }
    } /* End if of if (bisdevicetracked) */
}


void VolumeInfoCollector::GetLvsInSharedDiskSet(Vg_t &diskset)
{
    /*
    *
    * bash-3.00# metastat -s myset -p
    * myset/d35 -r /dev/md/myset/rdsk/d30 /dev/md/myset/rdsk/d31 /dev/md/myset/rdsk/d33 -k -i 32b
    * myset/d30 -p c2t4d0s0 -o 1 -b 524288
    * myset/d31 -p c2t4d0s0 -o 524290 -b 524288
    * myset/d33 -p c2t4d0s0 -o 1048579 -b 524288
    * The above is the reason we need to use
    * the base name
    */

    /*
    * -bash-3.00# metaset 
    *
    * Set name = blue, Set number = 1
    *
    * Host                Owner
    *   Solaris10VMCL1     Yes
    *
    * Driv Dbase
    *
    * d7   Yes  
    * -bash-3.00# metastat -s blue -p
    * blue/d20 1 1 /dev/did/rdsk/d7s0
    */

    /*
     * -bash-3.00# metastat -s oracle -p
     * oracle/d5 -r /dev/md/oracle/rdsk/d108 /dev/md/oracle/rdsk/d100 /dev/md/oracle/rdsk/d99 -k -i 32b
     * oracle/d108 -p /dev/md/oracle/rdsk/d76 -o 409696 -b 1048576
     * oracle/d76 1 1 c1t1d0s0
     * oracle/d100 -p /dev/md/oracle/rdsk/d76 -o 204864 -b 204800
     * oracle/d99 -p /dev/md/oracle/rdsk/d76 -o 32 -b 204800
     * oracle/d87 -p /dev/md/oracle/rdsk/d25 -o 409696 -b 204800
     * oracle/d25 -m oracle/d11 oracle/d12 1
     * oracle/d11 1 1 c1t4d0s0
     * oracle/d12 1 1 c1t5d0s0
     * oracle/d77 -p /dev/md/oracle/rdsk/d25 -o 204864 -b 204800
     * oracle/d70 -p /dev/md/oracle/rdsk/d25 -o 32 -b 204800
    */

    /*
    -bash-3.00# /usr/sbin/metastat -s oracle -p
    oracle/d5 -r /dev/md/oracle/rdsk/d108 /dev/md/oracle/rdsk/d100 /dev/md/oracle/rdsk/d99 -k -i 32b
    oracle/d108 -p /dev/md/oracle/rdsk/d76 -o 409696 -b 1048576
    oracle/d76 1 1 c1t1d0s0
    oracle/d100 -p /dev/md/oracle/rdsk/d76 -o 204864 -b 204800
    oracle/d99 -p /dev/md/oracle/rdsk/d76 -o 32 -b 204800
    oracle/d22 2 1 c1t4d0s0 \
             1 c1t5d0s0
    -bash-3.00#
    */
 
    /* 
    * -bash-3.00# metastat -s oracle -p
    * oracle/d5 -r /dev/md/oracle/rdsk/d108 /dev/md/oracle/rdsk/d100 /dev/md/oracle/rdsk/d99 -k -i 32b
    * oracle/d108 -p /dev/md/oracle/rdsk/d76 -o 409696 -b 1048576
    * oracle/d76 1 1 c1t1d0s0
    * oracle/d100 -p /dev/md/oracle/rdsk/d76 -o 204864 -b 204800
    * oracle/d99 -p /dev/md/oracle/rdsk/d76 -o 32 -b 204800
    * oracle/hsp004 c1t5d0s0
    * oracle/hsp002 c1t4d0s0
    */

    std::string cmd = "/usr/sbin/metastat -s";
    cmd += " ";
    cmd += diskset.m_Name;
    cmd += " ";
    cmd += "-p";

    std::stringstream results;
     
    if (executePipe(cmd, results)) 
    {
        while (!results.eof()) 
        {
            std::string line;
            std::getline(results, line);
            if (line.empty())
            {
                break;
            }

            /* although never seen continue character for
            *  svm lvs inside svm lvs, but to be on safe 
            *  side */
            while (CONTINUECHAR == line[line.length() - 1])
            {
                std::string nextline;
                std::getline(results, nextline);
                if (!nextline.empty())
                {
                    line += SPACE;
                    line += nextline;
                }
            }

            std::stringstream ssline(line);
            std::string lvtok;
            ssline >> lvtok;
            if (lvtok.empty())
            {
                break;
            }
            std::string lvname = MDONLYPATH;
            lvname += UNIX_PATH_SEPARATOR;
            lvname += diskset.m_Name;
            lvname += UNIX_PATH_SEPARATOR;
            lvname += DSK;
            lvname += UNIX_PATH_SEPARATOR;
            lvname += BaseName(lvtok);
            struct stat64 lvstat;
            /* This stat should avoid hsps inside a diskset */
            if ((0 == stat64(lvname.c_str(), &lvstat)) && lvstat.st_rdev)
            {
                Lv_t lv;
                lv.m_Name = lvname;
                lv.m_Devt = lvstat.st_rdev;
                lv.m_Vendor = diskset.m_Vendor;
                std::string rawname = GetRawDeviceName(lv.m_Name);
                int fd = open(rawname.c_str(), O_RDONLY);
                if (-1 != fd)
                {
                    lv.m_Size = GetVolumeSizeInBytes(fd, lv.m_Name);
                    close(fd);
                }
                lv.m_VgName = diskset.m_Name;
                while (!ssline.eof())
                {
                    std::string insidelv;
                    ssline >> insidelv;
                    if (insidelv.empty())
                    {
                        break;
                    }
                   
                    /* comparision with MDONLYPATH should avoid any dids; 
                     * store only mds since dids and native disks/partitions 
                     * are already inside by GetDiskDevts */
                    /* Could have added diskset name to MDONLYPATH for comparision
                     * but this is enough as we are trying to capture inside devts */
                    std::string insidelvtostat = insidelv;
                    bool btrystatoninsidelv = GetValidDiskSetInsideLv(diskset.m_Name, insidelv, insidelvtostat);
                    if (btrystatoninsidelv)
                    {
                        std::string dskinsidelvtostat = GetBlockDeviceName(insidelvtostat);
                        struct stat64 insidelvstat;
                        if ((0 == stat64(dskinsidelvtostat.c_str(), &insidelvstat))
                            && insidelvstat.st_rdev)
                        {
                            lv.m_InsideDevts.insert(insidelvstat.st_rdev);
                        }     
                    }
                }
                InsertLv(diskset.m_Lvs, lv);
            }
        }
    }
}


void VolumeInfoCollector::GetMetastatInfo(Vg_t &Vg)
{
    std::string cmd("/usr/sbin/metastat -p");
    std::stringstream results;

    /*
    * -bash-3.00# metastat -p
    * d22 -m d21 d20 1
    * d21 1 1 c1t1d0s3
    * d20 2 1 c1t1d0s0 \
    *        1 c1t1d0s1
    */

    /*
    * -bash-3.00# metastat -p
    * d22 -m d21 d20 1
    * d21 1 1 /dev/did/rdsk/d7s3
    * d20 2 1 /dev/did/rdsk/d7s0 \
    *          1 /dev/did/rdsk/d7s1
    * hsp998 /dev/did/rdsk/d7s4
    */

    /* even though rdsk is shown as inside devts, rdsk and dsk have
     * same devt, major and minor */
    /* -bash-3.00# ./stat /dev/dsk/c1t0d0s0 /dev/rdsk/c1t0d0s0
    * For device /dev/dsk/c1t0d0s0
    * devt = 137438953472
    * major = 32
    * minor = 0
    * For device /dev/rdsk/c1t0d0s0
    * devt = 137438953472
    * major = 32
    * minor = 0
    * -bash-3.00# ./stat /dev/dsk/c1t0d0s2 /dev/rdsk/c1t0d0s2
    * For device /dev/dsk/c1t0d0s2
    * devt = 137438953474
    * major = 32
    * minor = 2
    * For device /dev/rdsk/c1t0d0s2
    * devt = 137438953474
    * major = 32
    * minor = 2
    * -bash-3.00# ./stat /dev/did/dsk/d7s0 /dev/did/rdsk/d7s0
    * For device /dev/did/dsk/d7s0
    * devt = 1026497183968
    * major = 239
    * minor = 224
    * For device /dev/did/rdsk/d7s0
    * devt = 1026497183968
    * major = 239
    * minor = 224
    * -bash-3.00# ./stat /dev/md/dsk/d2
    * d20  d21  
    * -bash-3.00# ./stat /dev/md/dsk/d20 /dev/md/rdsk/d20
    * For device /dev/md/dsk/d20
    * devt = 365072220180
    * major = 85
    * minor = 20
    * For device /dev/md/rdsk/d20
    * devt = 365072220180
    * major = 85
    * minor = 20
    * -bash-3.00# 
    */

    if (executePipe(cmd, results))
    {
        while(!results.eof())
        {
            std::string line;
            std::getline(results, line);
            if (line.empty())
            {
                break;
            }
            while (CONTINUECHAR == line[line.length() - 1])
            {
                std::string nextline;
                std::getline(results, nextline);
                if (!nextline.empty())
                {
                    line += SPACE;
                    line += nextline;
                }
            }
            GetMetastatLineInfo(line, Vg);
        }
    }
}


void VolumeInfoCollector::GetMetastatLineInfo(const std::string &line, Vg_t &Vg)
{
    std::stringstream ssline(line);
 
    if (!ssline.eof())
    {
        std::string firsttok;
        ssline >> firsttok;
        if (firsttok.empty())  
        {
            return;
        }
        if (IsSvmLv(firsttok))
        {
            GetSvmLv(firsttok, ssline);
        }
        else if(IsSvmHsp(firsttok))
        {
            GetSvmHotSpares(firsttok, ssline, Vg);
        }
        else
        {
            /*TODO: no action yet */
        }
    }
}


void VolumeInfoCollector::GetSvmLv(const std::string &firsttok, std::stringstream &ssline)
{
    std::string lvname = MDPATH;
    lvname += UNIX_PATH_SEPARATOR;
    lvname += BaseName(firsttok);
    struct stat64 lvstat;
    if ((0 == stat64(lvname.c_str(), &lvstat)) && lvstat.st_rdev)
    {
        Lv_t lv;
        lv.m_Name = lvname;
        lv.m_Devt = lvstat.st_rdev;
        lv.m_Vendor = VolumeSummary::SVM;
        std::string rawname = GetRawDeviceName(lv.m_Name);
        int fd = open(rawname.c_str(), O_RDONLY);
        if (-1 != fd)
        {
            lv.m_Size = GetVolumeSizeInBytes(fd, lv.m_Name);
            close(fd);
        }
        while (!ssline.eof())
        {
            std::string insidevol;
            ssline >> insidevol;
            if (insidevol.empty())
            {
                break;
            }
            std::string devtostat = insidevol;
            if (UNIX_PATH_SEPARATOR[0] != insidevol[0])
            {
                std::string baseinsidevol = BaseName(insidevol);
                devtostat = IsSvmLv(baseinsidevol)?MDPATH:DEVDSK_PATH;
                devtostat += UNIX_PATH_SEPARATOR;
                devtostat += baseinsidevol;
            }
            std::string dskdevtostat = GetBlockDeviceName(devtostat);
            struct stat64 insidevolstat;
            if ((0 == stat64(dskdevtostat.c_str(), &insidevolstat)) && insidevolstat.st_rdev)
            {
                lv.m_InsideDevts.insert(insidevolstat.st_rdev);
            }
        }
        lv.m_VgName = lv.m_Name;
        InsertLv(lv);
    }
}


void GetDiskAndItsPartitions(const char *pdskdir,
                             const std::string &diskentryname, 
                             const E_DISKTYPE edsktyp, 
                             DiskPartitions_t &diskandpartitions,
                             void *pv_efi_alloc_and_read, 
                             void *pv_efi_free)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with devdir = %s and diskentryname = %s\n",FUNCTION_NAME, pdskdir, 
                diskentryname.c_str());
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(g_lockforefilibandreadvtoc);

    if (!guard.locked())
    {
        DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s unable to apply lock\n", __LINE__, __FILE__);
        return;    
    }

    std::string dskdevice = pdskdir; 
    dskdevice += "/";
    dskdevice += diskentryname;
    std::string rdskdevice = GetRawDeviceName(dskdevice);
    int fd = -1;
    int readvtoc_retval = -1;  
    struct vtoc vt;

    #ifndef SV_SUN_5DOT8
    dk_gpt_t *efivt = NULL;
    int efi_alloc_and_read_rval = -1;
    int (*ptr_efi_alloc_and_read)(int fd, dk_gpt_t **vtoc) = NULL;
    void (*ptr_efi_free)(dk_gpt_t *vtoc) = NULL;
    #endif
                        
    struct stat64 volStat;
    if ((-1 != stat64(rdskdevice.c_str(), &volStat))) 
    {
        fd = open(rdskdevice.c_str(), O_NONBLOCK | O_RDONLY);
        if (-1 != fd) 
        {
            readvtoc_retval = read_vtoc(fd, &vt);
            #ifndef SV_SUN_5DOT8
            if (VT_ENOTSUP == readvtoc_retval)
            {
                if (pv_efi_alloc_and_read && pv_efi_free)
                {
                    ptr_efi_alloc_and_read = (int (*)(int fd, dk_gpt_t **vtoc))pv_efi_alloc_and_read;
                    ptr_efi_free = (void (*)(dk_gpt_t *vtoc))pv_efi_free;
                    efi_alloc_and_read_rval = ptr_efi_alloc_and_read(fd, &efivt);
                }
            }
            #endif /* ifndef SV_SUN_5DOT8 */
            close(fd);
        }
    }

    bool trypartitionsandonlydisk = false;
    if (readvtoc_retval < 0)
    {
        #ifndef SV_SUN_5DOT8
        if (VT_ENOTSUP == readvtoc_retval)
        {
            if (efi_alloc_and_read_rval < 0)
            {
                trypartitionsandonlydisk = true; 
            }
        }
        else
        {
        #endif /* ifndef SV_SUN_5DOT8 */
            trypartitionsandonlydisk = true; 
        #ifndef SV_SUN_5DOT8
        }
        #endif 
    }

    if (trypartitionsandonlydisk)
    {
        std::string rdsk = GetOnlyDiskName(rdskdevice);
        bool breakout = false;
        for (int i = -1; (i < NPARTITIONS) && (!breakout) ; i++)
        {
            std::string slice = rdsk;
            if (i >= 0)
            {
                slice += Slices[edsktyp][i];
            }

            if ((-1 != stat64(slice.c_str(), &volStat))) 
            {
                fd = open(slice.c_str(), O_NONBLOCK | O_RDONLY);
                if (-1 != fd) 
                {
                    readvtoc_retval = read_vtoc(fd, &vt);
                    if (readvtoc_retval >= 0)
                    {
                        breakout = true;
                    }
                    #ifndef SV_SUN_5DOT8
                    else if (VT_ENOTSUP == readvtoc_retval)
                    {
                        if (pv_efi_alloc_and_read && pv_efi_free)
                        {
                            ptr_efi_alloc_and_read = (int (*)(int fd, dk_gpt_t **vtoc))pv_efi_alloc_and_read;
                            ptr_efi_free = (void (*)(dk_gpt_t *vtoc))pv_efi_free;
                            efi_alloc_and_read_rval = ptr_efi_alloc_and_read(fd, &efivt);
                            if (efi_alloc_and_read_rval >= 0)
                            {
                                breakout = true;
                            }
                        }
                    }
                    #endif 
                    close(fd);
                }
            }
        } /* End of for */
    } /* End of trypartitionsandonlydisk */

    std::string onlydskname = GetOnlyDiskName(dskdevice);
    
    if ((readvtoc_retval >= 0)
        #ifndef SV_SUN_5DOT8
        || ((efi_alloc_and_read_rval >= 0) && efivt)
        #endif
       )
    {
        /* Start filling in the structure */
        if (readvtoc_retval >= 0)
        {
            int backupdeviceindex = (V_BACKUP == vt.v_part[readvtoc_retval].p_tag)?
                                     readvtoc_retval:GetVTOCBackUpDevIdx(&vt);
            if (vt.v_nparts == NPARTITIONS)
            {
                FillFdiskP0(onlydskname, diskandpartitions);
            }

            for (int i = 0;(i < vt.v_nparts); ++i)
            {
                if ((i != backupdeviceindex) && ShouldCollectSMISlice(vt.v_part + i))
                {
                    std::stringstream partitionName;
                    partitionName << onlydskname << Slices[edsktyp][i];
                    bool ispartitionatblockzero = !(vt.v_part[i].p_start);
                    /* unsigned long long size = GetValidSize(partitionName.str(), vt.v_part[i].p_size, vt.v_sectorsz, E_ATMORETHANSIZE); */
                    DiskPartition_t stdiskandpartition(partitionName.str(), 
                                                       vt.v_part[i].p_size * vt.v_sectorsz, 0,
                                                       VolumeSummary::PARTITION, ispartitionatblockzero, VolumeSummary::SMI);
                    diskandpartitions.push_back(stdiskandpartition);
                }
            } 
            if (backupdeviceindex >= 0)
            {
                std::stringstream dskName;
                dskName << onlydskname << Slices[edsktyp][backupdeviceindex];
                /* sparc: s2 comes as disk with physicaloffset as 0 */
                VolumeSummary::Devicetype type = VolumeSummary::DISK;
                unsigned long long physicaloffset = 0;
                if (vt.v_nparts == NPARTITIONS)
                {
                    /* hence in case of emcpp, vxdmp, where there is no p0,
                     * on solaris x86, s2 comes as DISK_PARTITION */
                    physicaloffset = GetPhysicalOffset(dskName.str()); 
                    type = VolumeSummary::DISK_PARTITION;
                }
                /* unsigned long long size = GetValidSize(dskName.str(), vt.v_part[backupdeviceindex].p_size, vt.v_sectorsz, E_ATMORETHANSIZE); */
                DiskPartition_t stdiskandpartition(dskName.str(),
                                                   vt.v_part[backupdeviceindex].p_size * vt.v_sectorsz, physicaloffset,
                                                   type, false, VolumeSummary::SMI);
                diskandpartitions.push_back(stdiskandpartition);
            } 
        } 
        #ifndef SV_SUN_5DOT8
        else if ((efi_alloc_and_read_rval >= 0) && efivt)
        {
            /* TODO: size calculation logic has to be added for efi s7 and p0 in 
             * common/sun/portablehelpersminor.cpp. Also need to add return value 
             * checking of readvtoc, efiread return values for partitions fall below 
             * the max number of partitions */
            std::string disk = onlydskname;
            int statrval = -1;
            struct stat64 diskstat;
            statrval = stat64(disk.c_str(), &diskstat);
            if (0 != statrval)
            {
                disk += Slices[edsktyp][FULLEFIIDX];
                statrval = stat64(disk.c_str(), &diskstat);
            }
            if (0 == statrval)
            {
                std::string rdisk = GetRawDeviceName(disk);
                unsigned long long size = GetValidSize(rdisk, efivt->efi_last_lba + 1, efivt->efi_lbasize, E_ATLESSTHANSIZE);
                if (size)
                {
                    DiskPartition_t stdiskandpartition(disk, 
                                                       size, 0,
                                                       VolumeSummary::DISK, false, VolumeSummary::EFI);
                    diskandpartitions.push_back(stdiskandpartition);
                }
            }
            
            /* TODO: verify that efi_nparts is never > 6. since s7 is treated as
             * full disk incase efi full disk entry is not present */
            for (int i = 0; (i < efivt->efi_nparts); ++i) 
            {
                if (ShouldCollectEFISlice(efivt->efi_parts[i].p_tag))
                {
                    std::stringstream partitionName;
                    partitionName << onlydskname << Slices[edsktyp][i];
        
                    DiskPartition_t stdiskandpartition(partitionName.str(),
                                                       efivt->efi_parts[i].p_size * efivt->efi_lbasize, 0,
                                                       VolumeSummary::PARTITION, false, VolumeSummary::EFI);
                    diskandpartitions.push_back(stdiskandpartition);
                }
            }
            ptr_efi_free(efivt); 
        }     
        #endif
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s read_vtoc and efi read on %s failed\n", __LINE__, __FILE__, dskdevice.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void VolumeInfoCollector::CollectScsiHCTL(void)
{
}


void VolumeInfoCollector::UpdateHCTLFromDevice(const std::string &devicename)
{
    unsigned int c, d;
    std::string t;
    std::string bname = BaseName(devicename);
    bool parsed = parse(
        bname.begin(),
        bname.end(),
        lexeme_d
        [
        'c' >> uint_p [phoenix::var(c) = phoenix::arg1]
        >> !('t' >> (+(~ch_p('d'))) [phoenix::var(t) = phoenix::construct_<std::string>(phoenix::arg1,phoenix::arg2)])
        >> 'd' >> uint_p [phoenix::var(d) = phoenix::arg1]
        >> (*anychar_p)
        ] >> end_p, 
        space_p).full;

    if (parsed)
    {
        std::pair<DeviceToHCTLIter_t, bool> pr = m_DeviceToHCTL.insert(std::make_pair(devicename, std::string()));
        DeviceToHCTLIter_t &it = pr.first;
        std::string &hctl = it->second;

		hctl += HOSTCHAR;
		hctl += HCTLLABELVALUESEP;
		hctl += boost::lexical_cast<std::string>(c);
		hctl += HCTLSEP;

        if (!t.empty())
        {
		    hctl += TARGETCHAR;
		    hctl += HCTLLABELVALUESEP;
		    hctl += t;
		    hctl += HCTLSEP;
        }

		hctl += LUNCHAR;
		hctl += HCTLLABELVALUESEP;
		hctl += boost::lexical_cast<std::string>(d);
    }
    else
    {
        /* should never happen */
    }
}
