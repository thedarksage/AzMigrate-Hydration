#include <string>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <sstream>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>
#include <functional>
#include <list>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "voldefs.h"
#include "utilfunctionsmajor.h"
#include "logicalvolumes.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "utildirbasename.h"

/* should never be used on dev mapper , vxdmp disks since
 * they can have number at last since only scsi ID is populated */
std::string GetOnlyDiskName(const std::string &devname)
{
    std::string onlydiskname = devname;
    const char *p = NULL;
    const char *dp = devname.c_str();

    p = strpbrk(dp, NUMSTR);
    if (p)
    {
        bool bisdigit = true;
        const char *q = p;
        p++;
        while (*p)
        {
            bisdigit = isdigit(*p);
            if (!bisdigit)
            {
                break;
            }
            p++;
        }
        if (bisdigit)
        {
            onlydiskname = devname.substr(0, q - dp);
        }
    }
  
    return onlydiskname;
}


std::string GetRawDeviceName(const std::string &dskname)
{
    return dskname;
}


std::string GetBlockDeviceName(const std::string &rdskname)
{
    return rdskname;
}

/* for linux just stat on disk is enough; since we do not 
 * know number of partitions (dos fdisk says unlimited) and
 * also whether partition has what char ? */
bool GetDiskDevts(const std::string &onlydisk, std::set<dev_t> &devts)
{
    
    bool bfounddsktyp = false;
     
    struct stat64 volStat;
    if ((0 == stat64(onlydisk.c_str(), &volStat)) && volStat.st_rdev)
    {
        devts.insert(volStat.st_rdev);
        bfounddsktyp = true;
    }

    return bfounddsktyp;
}


bool ShouldCollectPartition(const std::string &partitionAttr, 
                            PartitionNegAttrs_t &partitionNegAttrs)
{
    bool bcollect = true;

    const PartitionNegAttrs_t::const_iterator begin = partitionNegAttrs.begin();
    const PartitionNegAttrs_t::const_iterator end = partitionNegAttrs.end();

    for (PartitionNegAttrs_t::const_iterator iter = begin; iter != end; iter++)
    {
        const std::string &eachattr = (*iter);
        if (strstr(partitionAttr.c_str(), eachattr.c_str()))
        {
            /* found negative attr */
            bcollect = false;
            break;
        }
    }

    return bcollect;
}


bool IsLinuxPartition(const std::string &device)
{
    const char &c = device[device.length() - 1];
    return isdigit(c);
}


std::string GetPartitionNumberFromNameText(const std::string &partition)
{
    std::string s;
    return s;
}


bool IsVxDmpPartition(const std::string &disk, const std::string &partition)
{
    bool bispartition = false;
    const char *d = disk.c_str();
    const char *p = partition.c_str();

    size_t dlen = strlen(d);
    if (0 == strncmp(d, p, dlen))
    {
        p += dlen;
        if (*p)
        {
            if (isdigit(*p))
            {
                p++;
                if (*p)
                    bispartition = AreDigits(p);
                else
                    bispartition = true;
            }
            else if(isalpha(*p))
            {
                p++;
                bispartition = AreDigits(p);
            }
        }
    }

    return bispartition;
}


std::string GetDeviceNameFromBlockEntry(const std::string &blockentry)
{
    std::string device;
    std::string bname = BaseName(blockentry);

    if (bname == BLOCKENTRY)
    {
        std::string realname = blockentry;
        GetDeviceNameFromSymLink(realname);
        if (blockentry != realname)
        {
            /* rhel 4 up 7: 
             * [root@rhel4u7-32-101 ~]# ls -l /sys/class/scsi_device/0\:0\:0\:0/device/
             * lrwxrwxrwx  1 root root    0 Jul 19 09:42 block -> ../../../../../../block/sda
             */
            device = NATIVEDISKDIR;
            device += UNIX_PATH_SEPARATOR;
            device += BaseName(realname);
        }
        else
        {
            /* sles11-sp1: 
             * sles11-sp1-64bit:/usr/local/InMage/Vx/bin # ls -l /sys/class/scsi_device/0\:0\:0\:0/device/block
             * total 0
             * drwxr-xr-x 8 root root 0 2012-08-06 12:19 sda
             * sles11-sp1-64bit:/usr/local/InMage/Vx/bin #
             */
             DIR *dirp;
             struct dirent *dentp, *dentresult;
             size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;

             dentresult = NULL;
             dirp = opendir(blockentry.c_str());
             if (dirp)
             {
                dentp = (struct dirent *)calloc(direntsize, 1);
                if (dentp)
                {
                    while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
                    {
                        if (strcmp(dentp->d_name, ".") &&
                            strcmp(dentp->d_name, ".."))
                        {
                            /* NOTE: expecting only one entry, the disk name */
                            device = NATIVEDISKDIR;
                            device += UNIX_PATH_SEPARATOR;
                            device += dentp->d_name;
                        }
                    }
                    free(dentp);
                }
                closedir(dirp); 
             }
        }
    }
    else
    {
        /* rhel 5 and above: 
         *  [root@TST-BLD-CENTOS5U5-64-10 host]# ls -l /sys/class/scsi_device/0\:0\:0\:0/device/block\:sda/
         * total 0
         * -r--r--r-- 1 root root 4096 Jul 20 16:00 dev
         */
        size_t colonpos = bname.find(':');
        if ((std::string::npos != colonpos) && (std::string::npos != (colonpos+1)))
        {
            device = NATIVEDISKDIR;
            device += UNIX_PATH_SEPARATOR;
            device += bname.substr(colonpos+1); 
        }
    }
   
    return device;
}


void GetScsiHCTLFromSysClass(CollectDeviceHCTLPair_t c)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string devicename;
    std::string devicedir;
    std::string blockentry;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    const char *pdir = "/sys/class/scsi_device/";
    dentresult = NULL;
    dirp = opendir(pdir);
    if (dirp)
    {
       dentp = (struct dirent *)calloc(direntsize, 1);
       if (dentp)
       {
           while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
           {
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, ".."))
               {
                   devicedir = pdir;
                   devicedir += dentp->d_name;
                   devicedir += "/device/";
           
                   DIR *ndirp;
                   struct dirent *ndentp, *ndentresult;
                   size_t ndirentsize = sizeof(struct dirent) + PATH_MAX + 1;
                   ndentresult = NULL;
                   ndirp = opendir(devicedir.c_str());
                   if (ndirp)
                   {
                       ndentp = (struct dirent *)calloc(ndirentsize, 1);
                       if (ndentp)
                       {
                           while ((0 == readdir_r(ndirp, ndentp, &ndentresult)) && ndentresult)
                           {
                               if (strcmp(ndentp->d_name, ".") &&
                                   strcmp(ndentp->d_name, "..") && 
                                   (0 == strncmp(ndentp->d_name, BLOCKENTRY, strlen(BLOCKENTRY))))
                               {
                                   blockentry = devicedir;
                                   blockentry += ndentp->d_name;
      
                                   devicename = GetDeviceNameFromBlockEntry(blockentry);
                                   DebugPrintf(SV_LOG_DEBUG, "block entry = %s has devicename %s\n", blockentry.c_str(), devicename.c_str());
                                   if (!devicename.empty())
                                   {
                                       std::string hctl = GetHCTLWithDelimiter(dentp->d_name, ":");
                                       if (!hctl.empty())
                                           c(std::make_pair(devicename, hctl));
                                   }
                               }
                           }
                           free(ndentp);
                       }
                       closedir(ndirp);
                   } 
               }
           }
           free(dentp);
       }
       closedir(dirp); 
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void GetScsiHCTLFromSysBlock(CollectDeviceHCTLPair_t c)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    /**
     * oracle uek kernels have hctl as:
     * ls -l /sys/block/sda/device/scsi_disk/0\:0\:0\:0
     * or 
     * sys/block/sda/device/scsi_disk\:0\:0\:0\:0
     */

    std::string devicename;
    std::string devicedir;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    const char *pdir = "/sys/block/";
    dentresult = NULL;
    dirp = opendir(pdir);
    if (dirp)
    {
       dentp = (struct dirent *)calloc(direntsize, 1);
       if (dentp)
       {
           while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
           {
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, ".."))
               {
                   devicedir = pdir;
                   devicedir += dentp->d_name;
                   devicedir += "/device/";

                   devicename = NATIVEDISKDIR;
                   devicename += UNIX_PATH_SEPARATOR;
                   devicename += dentp->d_name;

                   std::string hctl = GetScsiHCTLFromSysBlockDevice(devicedir);
                   if (!hctl.empty())
                       c(std::make_pair(devicename, hctl));
               }
           }
           free(dentp);
       }
       closedir(dirp); 
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


std::string GetScsiHCTLFromSysBlockDevice(const std::string &devicedir, bool bWithHctl)
{
    /* DebugPrintf(SV_LOG_DEBUG, "ENTERTED %s with devicedir %s\n", FUNCTION_NAME, devicedir.c_str()); */
    std::string hctl;
    std::string scsidiskentry;
    bool isscsidiskdir = false;
    const char SCSI_DISK_STRING[] = "scsi_disk";

    DIR *ndirp;
    struct dirent *ndentp, *ndentresult;
    size_t ndirentsize = sizeof(struct dirent) + PATH_MAX + 1;
    ndentresult = NULL;
    ndirp = opendir(devicedir.c_str());
    if (ndirp)
    {
        ndentp = (struct dirent *)calloc(ndirentsize, 1);
        if (ndentp)
        {
            while ((0 == readdir_r(ndirp, ndentp, &ndentresult)) && ndentresult)
            {
                if (strcmp(ndentp->d_name, ".") &&
                    strcmp(ndentp->d_name, ".."))
                {
                    if (0 == strncmp(ndentp->d_name, SCSI_DISK_STRING, strlen(SCSI_DISK_STRING))) {
                        isscsidiskdir = (0==strcmp(SCSI_DISK_STRING,ndentp->d_name));
                        scsidiskentry = ndentp->d_name;
                        break;
                    }
                }
            }
            free(ndentp);
        }
        closedir(ndirp);
    }

    DebugPrintf(SV_LOG_DEBUG, "In sysblock, devicedir = %s, scsidiskentry = %s, isscsidiskdir = %s\n", 
                              devicedir.c_str(), scsidiskentry.c_str(), STRBOOL(isscsidiskdir));
    if (scsidiskentry.empty()) {
        /* DebugPrintf(SV_LOG_DEBUG, "could not find scsidiskentry in sys block\n"); */
    } else if (isscsidiskdir) {
        DIR *ndirp;
        struct dirent *ndentp, *ndentresult;
        size_t ndirentsize = sizeof(struct dirent) + PATH_MAX + 1;
        ndentresult = NULL;
        std::string scsidiskdir = devicedir;
        scsidiskdir += SCSI_DISK_STRING;
        scsidiskdir += UNIX_PATH_SEPARATOR;
        ndirp = opendir(scsidiskdir.c_str());
        if (ndirp)
        {
            ndentp = (struct dirent *)calloc(ndirentsize, 1);
            if (ndentp)
            {
                while ((0 == readdir_r(ndirp, ndentp, &ndentresult)) && ndentresult)
                {
                    if (strcmp(ndentp->d_name, ".") &&
                        strcmp(ndentp->d_name, ".."))
                    {
                        hctl = bWithHctl ? GetHCTLWithHCTL(ndentp->d_name, ":") : GetHCTLWithDelimiter(ndentp->d_name, ":");
                        if (!hctl.empty())
                            break;
                    }
                }
                free(ndentp);
            }
            closedir(ndirp);
        }
    } else {
        size_t colonpos = scsidiskentry.find(':');
        if ((std::string::npos != colonpos) && (std::string::npos != (colonpos+1)))
            hctl = bWithHctl ? GetHCTLWithHCTL(scsidiskentry.substr(colonpos+1), ":") : GetHCTLWithDelimiter(scsidiskentry.substr(colonpos+1), ":");
    }

    /* DebugPrintf(SV_LOG_DEBUG, "EXITED %s with hctl %s\n", FUNCTION_NAME, hctl.c_str()); */
    return hctl;
}

bool ShouldPreferSysBlockForHCTL(void)
{
    bool should = false;
    struct utsname u;

    if (uname(&u) == 0) {
        DebugPrintf(SV_LOG_DEBUG, "kernel release version %s\n", u.release);
        should = (NULL != strstr(u.release, "uek"));
    } else
        DebugPrintf(SV_LOG_ERROR, "uname failed with error %d\n", errno);
     
    return should;
}

std::string GetNameBasedOnScsiHCTLFromSysClass(const std::string &device)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string devicename;
    std::string devicedir;
    std::string blockentry;
    std::string hctl;
    bool bfound=false;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    const char *pdir = "/sys/class/scsi_device/";
    dentresult = NULL;
    dirp = opendir(pdir);
    if (dirp)
    {
       dentp = (struct dirent *)calloc(direntsize, 1);
       if (dentp)
       {
           while (!bfound && (0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
           {
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, ".."))
               {
                   devicedir = pdir;
                   devicedir += dentp->d_name;
                   devicedir += "/device/";
           
                   DIR *ndirp;
                   struct dirent *ndentp, *ndentresult;
                   size_t ndirentsize = sizeof(struct dirent) + PATH_MAX + 1;
                   ndentresult = NULL;
                   ndirp = opendir(devicedir.c_str());
                   if (ndirp)
                   {
                       ndentp = (struct dirent *)calloc(ndirentsize, 1);
                       if (ndentp)
                       {
                           while ((0 == readdir_r(ndirp, ndentp, &ndentresult)) && ndentresult)
                           {
                               if (strcmp(ndentp->d_name, ".") &&
                                   strcmp(ndentp->d_name, "..") && 
                                   (0 == strncmp(ndentp->d_name, BLOCKENTRY, strlen(BLOCKENTRY))))
                               {
                                   blockentry = devicedir;
                                   blockentry += ndentp->d_name;
      
                                   devicename = GetDeviceNameFromBlockEntry(blockentry);
                                   DebugPrintf(SV_LOG_DEBUG, "block entry = %s has devicename %s\n", blockentry.c_str(), devicename.c_str());
                                   if (boost::iequals(devicename, device))
                                   {
                                       hctl = GetHCTLWithHCTL(dentp->d_name, ":");
                                       bfound=true;
                                   }
                               }
                           }
                           free(ndentp);
                       }
                       closedir(ndirp);
                   }
               }
           }
           free(dentp);
       }
       closedir(dirp); 
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return hctl;
}

std::string GetNameBasedOnScsiHCTLFromSysBlock(const std::string &device)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    /**
     * oracle uek kernels have hctl as:
     * ls -l /sys/block/sda/device/scsi_disk/0\:0\:0\:0
     * or 
     * sys/block/sda/device/scsi_disk\:0\:0\:0\:0
     */

    std::string devicename;
    std::string devicedir;
    std::string hctl;
    bool bfound = false;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    const char *pdir = "/sys/block/";
    dentresult = NULL;
    dirp = opendir(pdir);
    if (dirp)
    {
        dentp = (struct dirent *)calloc(direntsize, 1);
        if (dentp)
        {
            while (!bfound && (0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
            {
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, ".."))
               {
                   devicedir = pdir;
                   devicedir += dentp->d_name;
                   devicedir += "/device/";

                   devicename = NATIVEDISKDIR;
                   devicename += UNIX_PATH_SEPARATOR;
                   devicename += dentp->d_name;

                   if (boost::iequals(devicename, device))
                   {
                       hctl = GetScsiHCTLFromSysBlockDevice(devicedir, true);
                   }
               }
            }
           free(dentp);
        }
        closedir(dirp); 
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return hctl;
}

std::string GetNameBasedOnScsiHCTL(const std::string &device)
{
    std::string hctlDeviceName;
    
    if (ShouldPreferSysBlockForHCTL())
    {
        hctlDeviceName = GetNameBasedOnScsiHCTLFromSysClass(device);
    }
    else
    {
        hctlDeviceName = GetNameBasedOnScsiHCTLFromSysBlock(device);
    }

    return hctlDeviceName;
}
