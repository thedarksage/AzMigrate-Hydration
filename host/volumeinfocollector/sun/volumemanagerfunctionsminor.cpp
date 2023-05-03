#include <string>
#include <sstream>
#include <cctype>
#include <cstring>
#include <vector>
#include <map>
#include "executecommand.h"
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "utildirbasename.h"
#include "portable.h"
#include "portablehelpersmajor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "utilfunctionsmajor.h"
#include "boost/algorithm/string/replace.hpp"
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include "logicalvolumes.h"

#ifndef SV_SUN_5DOT8
#include "sys/efi_partition.h"
#endif /* SV_SUN_5DOT8 */


/* TODO: separate out this for LINUX where in, just remove last digit */
std::string GetOnlyDiskName(const std::string &FullDiskName)
{
    std::string onlydisk = FullDiskName;

    /*
    * It can be disk_12s[0-9]+ (or) emcpower<n>[a-p] (or) cXtYdZ (or) cXtYdZs<0-9>+
    */
    if (IsEmcPowerPathNode(FullDiskName))
    {
        RemLastCharIfReq(FullDiskName, onlydisk);
    }   
    else
    {
        RemSliceIfReq(FullDiskName, onlydisk);
    }

    return onlydisk;
}


void RemSliceIfReq(const std::string &FullDiskName, std::string &onlydisk)
{
    std::string::size_type idx = FullDiskName.rfind(SLICECHAR);
    size_t len = FullDiskName.length();

    if ((std::string::npos != idx) && (idx < len - 1))
    {
        std::string::size_type saveidx = idx;
        idx++;
        for ( /* empty */; idx < len; idx++)
        {
            if (!isdigit(FullDiskName[idx]))
            {
                break;
            }
        }
        if (len == idx)
        {
            onlydisk.clear();
            onlydisk.append(FullDiskName, 0, saveidx);
        }
    }
}


void RemLastCharIfReq(const std::string &FullDiskName, std::string &onlydisk)
{
    bool bhasremlastchar = false;
    std::string devbasename = BaseName(FullDiskName);
    const char *p = devbasename.c_str();
    const size_t emclen = strlen(EMCPOWER);
    size_t len = FullDiskName.length();
    const char &lc = FullDiskName[len - 1];

    if ((std::string::npos != SlicesChar.find(lc))&&
        (0 ==  strncmp(EMCPOWER, p, emclen)))
    {
        const char *q = p + emclen;
        if (*q && isdigit(*q))
        {
            q++;
            while (*q && isdigit(*q))
            {
                q++;
            }
            if (*q)
            {
                const char *save = q;
                q++;
                bhasremlastchar = ((*save == lc) && ('\0' == *q));
            }
        }
    }

    if (bhasremlastchar)
    {
        onlydisk.clear();
        onlydisk.append(FullDiskName, 0, len - 1);
    }
}


bool IsFullEFIDisk(const char *disk)
{
    bool bretval = false;


    if (disk)
    {
        size_t len = strlen(disk);
        const char *p = disk + (len - 1);

        while (p >= disk)
        {
            if (isdigit(*p))
            {
               p--;
            }
            else
            {
               bretval = ('d' == *p)?true:false;
               break;
            }
        }
    }

    return bretval;
}


/* If we are not able to get devt of disk because entry is not
   created, get all partition devts; this also handles absolute path */
void GetDiskSetDiskDevts(const std::string &drive, std::set<dev_t> &devts)
{
    if (drive.empty())
    {
        return;
    }

    std::string disk = drive;
    if (UNIX_PATH_SEPARATOR[0] != drive[0])
    {
        std::string basename = BaseName(drive);
        disk = IsDidDisk(basename)?DEVDID_PATH:DEVDSK_PATH;
        disk += UNIX_PATH_SEPARATOR;
        disk += basename;
    }

    /* stat and add all disks and partitions to this */
    GetDiskDevts(disk, devts);
}


/* For sun, we need to get all disks and partitions because,
   we do not know full disk until for SMI, V_BACKUP is known
   on which slice; s2 may not be always full. For EFI, disk 
   entry itself may not be there */
/* Here we are not doing anything for p0, since if p0 is full
 * disk, it will get volume group from one of its child (s0 to s16);
 * Also we know there can be max 16 slices */
bool GetDiskDevts(const std::string &onlydisk, std::set<dev_t> &devts)
{
    /* Added since from vxdisk list, s2 might be 
     * already added */
    E_DISKTYPE edsktyp = IsEmcPowerPathNode(onlydisk) ? E_CISFULL : E_S2ISFULL;
    std::string onlydiskname = GetOnlyDiskName(onlydisk);
    std::string dsk = GetBlockDeviceName(onlydiskname);
    bool bfounddsktyp = false;
     
    for (int i = -1; (i < NPARTITIONS); i++)
    {
        std::string slice = dsk;
        if (i >= 0)
        {
            slice += Slices[edsktyp][i];
        }

        struct stat64 volStat;
        if ((0 == stat64(slice.c_str(), &volStat)) && volStat.st_rdev) 
        {
            devts.insert(volStat.st_rdev);
            bfounddsktyp = true;
        }
    }
    /* currently keep commented since for efi, p0 is stale; 
     * and we do not want to do readvtoc here 
    if (bfounddsktyp)
    {
        std::string p0disk = dsk;
        p0disk += FdiskPartitions[0];
        struct stat64 volStat;
        if ((0 == stat64(p0disk.c_str(), &volStat)) && volStat.st_rdev)
        {
            devts.insert(volStat.st_rdev);
        }
    }
    */

    return bfounddsktyp;
}

bool IsDidDisk(const std::string &drive)
{
    bool bretval = false;
    std::string basename = BaseName(drive);
    const char * const disk = basename.c_str();
    const char DIDDISKCHAR = 'd';

    if (disk)
    {
        size_t len = strlen(disk);
        const char *p = disk + (len - 1);

        while (p >= disk)
        {
            if (isdigit(*p))
            {
               p--;
            }
            else if (DIDDISKCHAR == *p)
            {
                bretval = ((len > 1) && (p == disk));
                break;
            }
            else
            {
                break;
            }
        }
    }

    return bretval;
}
 

bool CanBeFullDisk(const std::string &device, E_DISKTYPE *pedsktyp)
{
    bool bisfulldevice = false;
    const std::string s2full = "s2";
    std::string::size_type idx = device.rfind(s2full);     
    size_t devicelen = device.length();



    if (std::string::npos != idx)
    {
        size_t endlen = s2full.length();
       
        if ((idx + endlen) == devicelen)
        {
            bisfulldevice = true;
            *pedsktyp = E_S2ISFULL;
        }
    }

    if (!bisfulldevice)
    {
        std::string devbasename = BaseName(device);
        const char *p = devbasename.c_str();
        const size_t emclen = strlen(EMCPOWER);
        size_t len = device.length();
        const char &lc = device[len - 1];

        if ((CFULL == lc) && 
           (IsEmcPowerPathNode(device)))
        {
            const char *q = p + emclen;
            if (*q && isdigit(*q))
            {
                q++;
                while (*q && isdigit(*q))
                {
                    q++;
                }
                if (*q)
                {
                    const char *save = q;
                    q++;
                    bisfulldevice = ((*save == lc) && ('\0' == *q));
                }
            }
        }
        if (bisfulldevice)
        {
            *pedsktyp = E_CISFULL;
        }
    }

    return bisfulldevice;
}


bool DoesLunNumberMatch(const std::string &device, const SV_ULONGLONG *plunnmuber, E_DISKTYPE *pedsktyp)
{
    bool bfound = false;
    const std::string s2full = "s2";
    std::string::size_type idx = device.rfind(s2full);     
    size_t devicelen = device.length();

    if (std::string::npos != idx)
    {
        size_t endlen = s2full.length();
       
        if ((idx + endlen) == devicelen)
        {
            *pedsktyp = E_S2ISFULL;
            std::string basename = BaseName(device);
            SV_ULONGLONG lunnumber = 0;
            if ((1 == sscanf(basename.c_str(), "%*[^d]d" ULLSPEC, &lunnumber)) && 
                (lunnumber == *plunnmuber))
            {
                bfound = true;
            }
        }
    }

    return bfound;
}

std::string GetRawDeviceName(const std::string &dskname)
{
    std::string rdskname = dskname;
    boost::algorithm::replace_first(rdskname, DSKDIR, RDSKDIR);
    boost::algorithm::replace_first(rdskname, DMPDIR, RDMPDIR);
    return rdskname;
}


std::string GetBlockDeviceName(const std::string &rdskname)
{
    std::string dskname = rdskname;
    boost::algorithm::replace_first(dskname, RDSKDIR, DSKDIR);
    boost::algorithm::replace_first(dskname, RDMPDIR, DMPDIR);
    return dskname;
}


void GetMetadbInsideDevts(Vg_t &localds)
{
    /*
    * -bash-3.00# /usr/sbin/metadb | /usr/bin/sed -e '1d' | /usr/bin/awk '{ print $NF }' | /usr/bin/sort -k1 -u
    * /dev/dsk/c2d0s0
    * /dev/dsk/c2d0s1
    * /dev/dsk/c2d0s3
    */
    std::string cmd("/usr/sbin/metadb | /usr/bin/sed -e \'1d\' | /usr/bin/awk \'{ print $NF }\' | /usr/bin/sort -k1 -u");
    std::stringstream results;

    if (executePipe(cmd, results)) 
    {
        while (!results.eof()) 
        {
            std::string dbslice;
            results >> dbslice;
            if (dbslice.empty()) 
            {
                break;
            }
            std::string dskdbslice = GetBlockDeviceName(dbslice);
            /* metadb gives absolute path; hence
             * stat directly */
            struct stat64 dbslicestat;
            if ((0 == stat64(dskdbslice.c_str(), &dbslicestat)) && dbslicestat.st_rdev)
            {
                localds.m_InsideDevts.insert(dbslicestat.st_rdev);
            }
        }
    }
}


void GetSvmHotSpares(const std::string &firsttok, std::stringstream &ssline, Vg_t &localds)
{
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
            devtostat = DEVDSK_PATH;
            devtostat += UNIX_PATH_SEPARATOR;
            devtostat += baseinsidevol;
        }
        std::string dskdevtostat = GetBlockDeviceName(devtostat);
        struct stat64 insidevolstat;
        if ((0 == stat64(dskdevtostat.c_str(), &insidevolstat)) && insidevolstat.st_rdev)
        {
            localds.m_InsideDevts.insert(insidevolstat.st_rdev);
        }
    }
}


bool IsSvmLv(const std::string &lv)
{
    bool bretval = false;
    std::string lvbasename = BaseName(lv);
    const char * const plv = lvbasename.c_str();
    const char SVMCHAR = 'd';

    if (plv)
    {
        size_t len = strlen(plv);
        const char *p = plv + (len - 1);

        while (p >= plv)
        {
            if (isdigit(*p))
            {
               p--;
            }
            else if (SVMCHAR == *p)
            {
                bretval = ((len > 1) && (p == plv));
                break;
            }
            else
            {
                break;
            }
        }
    }

    return bretval;
}


bool IsSvmHsp(const std::string &hsp)
{
    bool bissvmhsp = false;
    const char *p = hsp.c_str();
    const char *hspstart = 0;

    hspstart = strstr(p, HSP); 
    if (hspstart == p)
    {
        const char *q = hspstart + strlen(HSP);
        bool bisdig = false;
        while (*q)
        {
            bisdig = isdigit(*q);
            if (!bisdig)
            {
                break;
            }
            q++;
        }
        bissvmhsp = bisdig;
    }
    
    return bissvmhsp;
}


bool ShouldCollectSMISlice(struct partition *p_part)
{
    bool bshouldcollectslice = true;

    switch (p_part->p_tag)
    {
        case V_UNASSIGNED:
             if (p_part->p_size <= 0)
             {
                 bshouldcollectslice = false;
             }
             break;
        /* boot, alternate sector, reserved are present
        * on every x86 disk atleast. should not report these */
        case V_BOOT:
        case V_ALTSCTR: 
#ifndef SV_SUN_5DOT8
        case V_RESERVED: /* Fall Through */
#endif /* SV_SUN_5DOT8 */
             bshouldcollectslice = false;
             break; 
        /* 
         * swap tag generally is where swap is present
        case V_SWAP:
             pvolattrs->m_SwapVolume = true;
             break; 
        */
        case V_SWAP:
        case V_ROOT:
        case V_USR:
        case V_VAR:
        case V_HOME: /* Fall Through */
        default:
             break;
    }    

    return bshouldcollectslice;
}


#ifndef SV_SUN_5DOT8
bool ShouldCollectEFISlice(ushort_t ptag)
{
    bool bshouldcollectslice = true;
    switch (ptag)
    {
        /* For efi, unassigned slice is always zero size */
        case V_UNASSIGNED:
        /* boot, alternate sector, reserved are present
        * on every x86 disk atleast. should not report these */
        case V_BOOT:
        case V_ALTSCTR: 
        case V_RESERVED: /* Fall Through */
             bshouldcollectslice = false;
             break;
        /* 
        * swap tag generally is where swap is present
        case V_SWAP:
             pvolattrs->m_SwapVolume = true;
             break; 
        */
        case V_SWAP:
        case V_ROOT:
        case V_USR:
        case V_VAR:
        case V_HOME: /* Fall Through */
        default:
             break;
    }    

    return bshouldcollectslice;
}
#endif /* SV_SUN_5DOT8 */


bool IsBackUpDevice(struct partition *p_part)
{
    return (V_BACKUP == p_part->p_tag);
}


int GetVTOCBackUpDevIdx(struct vtoc *pvt)
{
    int idx = -1;
    for (int i = 0; i < pvt->v_nparts; i++)
    {
        if (V_BACKUP == pvt->v_part[i].p_tag)
        {
            idx = i;
            break;
        }
    }

    return idx;
}


void FillFdiskP0(const std::string &onlydskname, DiskPartitions_t &diskandpartitions)
{
    std::string fdiskpartition = onlydskname;
    fdiskpartition += FdiskPartitions[0];
    std::string fdiskrawpartition = GetRawDeviceName(fdiskpartition);
    int fd = open(fdiskrawpartition.c_str(), O_RDONLY);
    if (-1 != fd)
    {
        struct dk_minfo mi;
        struct part_info pi;
        memset(&pi, 0, sizeof pi);
        memset(&mi, 0, sizeof mi);
        unsigned long long secsize = NBPSCTR;
        int mirval = ioctl(fd, DKIOCGMEDIAINFO, &mi);
        if (!mirval)
        {
            secsize = mi.dki_lbsize;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "For %s, ioctl DKIOCGMEDIAINFO failed with errno = %d\n", fdiskpartition.c_str(), errno);
        }
        unsigned long long size = 0;
        unsigned long long nsecs = 0;
        int pirval = ioctl(fd, DKIOCPARTINFO, &pi);
        if (!pirval)
        {
            nsecs = pi.p_length;
            size = nsecs * secsize;
            if (size)
            {
                DiskPartition_t stdiskandpartition(fdiskpartition, 
                                                   size, 0,
                                                   VolumeSummary::DISK, false, VolumeSummary::SMI);
                diskandpartitions.push_back(stdiskandpartition);
            }
            else
            {
                std::stringstream msg;
                msg << "For device " << fdiskpartition
                    << ", nsecs = " << nsecs 
                    << ", secsize = " << secsize
                    << ", size = " << size;
                DebugPrintf(SV_LOG_WARNING, "%s\n", msg.str().c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "For %s, ioctl DKIOCPARTINFO failed with errno = %d\n", fdiskpartition.c_str(), errno);
        }
        close(fd);
    }
}


unsigned long long GetPhysicalOffset(const std::string &disk)
{
    unsigned long long physicaloffset = 0;
    std::string rawdisk = GetRawDeviceName(disk);
    int fd = open(rawdisk.c_str(), O_RDONLY);
    if (-1 != fd)
    {
        struct dk_minfo mi;
        struct part_info pi;
        memset(&pi, 0, sizeof pi);
        memset(&mi, 0, sizeof mi);
        unsigned long long secsize = NBPSCTR;
        int mirval = ioctl(fd, DKIOCGMEDIAINFO, &mi);
        if (0 == mirval)
        {
            secsize = mi.dki_lbsize;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "For %s, ioctl DKIOCGMEDIAINFO failed with errno = %d\n", disk.c_str(), errno);
        }
        unsigned long long startoffset = 0;
        int pirval = ioctl(fd, DKIOCPARTINFO, &pi);
        if (0 == pirval)
        {
            startoffset = pi.p_start;
            physicaloffset = startoffset * secsize;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "For %s, ioctl DKIOCPARTINFO failed with errno = %d\n", disk.c_str(), errno);
        }
        close(fd);
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "For %s, failed to open with errno = %d\n", disk.c_str(), errno);
    }

    return physicaloffset;
}


std::string GetPartitionNumberFromNameText(const std::string &partition)
{
    std::string num;
    bool bhasremlastchar = false;
    std::string devbasename = BaseName(partition);
    const char *p = devbasename.c_str();
    const size_t emclen = strlen(EMCPOWER);
    size_t len = partition.length();
    const char &lc = partition[len - 1];
    size_t idx = SlicesChar.find(lc);

    if ((std::string::npos != idx) && IsEmcPowerPathNode(partition))
    {
        const char *q = p + emclen;
        if (*q && isdigit(*q))
        {
            q++;
            while (*q && isdigit(*q))
            {
                q++;
            }
            if (*q)
            {
                const char *save = q;
                q++;
                bhasremlastchar = ((*save == lc) && ('\0' == *q));
            }
        }
    }

    if (bhasremlastchar)
    {
        std::stringstream numstr;
        numstr << idx;
        num = numstr.str();
    }
    return num;
}


bool GetValidDiskSetInsideLv(const std::string &diskset, const std::string &insidetext, std::string &insidelvtostat)
{
    bool bisvalid = false;
    std::string disksetdir = diskset;
    disksetdir += UNIX_PATH_SEPARATOR;
    const char *pd = disksetdir.c_str();
    const char *pi = insidetext.c_str();
    size_t disksetdirlen = strlen(pd);
 
    if ((UNIX_PATH_SEPARATOR[0] == insidetext[0]) && 
       (0 == strncmp(pi, MDONLYPATH.c_str(), strlen(MDONLYPATH.c_str()))))
    {
        bisvalid = true;
        insidelvtostat = insidetext;
    }
    else if (0 == strncmp(pi, pd, disksetdirlen))
    {
        if (*(pi + disksetdirlen))
        {
            insidelvtostat = MDONLYPATH;
            insidelvtostat += UNIX_PATH_SEPARATOR;
            insidelvtostat += diskset;
            insidelvtostat += UNIX_PATH_SEPARATOR;
            insidelvtostat += DSK;
            insidelvtostat += UNIX_PATH_SEPARATOR;
            insidelvtostat += BaseName(insidetext);
            bisvalid = true;
        }
    }

    return bisvalid;
}


bool IsEmcPowerPathNode(const std::string &device)
{
    std::string blockdev = GetBlockDeviceName(device);
    std::string emcpowerprefix = NATIVEDISKDIR;
    emcpowerprefix += UNIX_PATH_SEPARATOR;
    emcpowerprefix += EMCPOWER;
    const size_t emcpowerprefixlen = strlen(emcpowerprefix.c_str());

    return (0 == strncmp(emcpowerprefix.c_str(), blockdev.c_str(), emcpowerprefixlen));
}


unsigned long long GetValidSize(const std::string &device, const unsigned long long nsecs, 
                                const unsigned long long secsz, const E_VERIFYSIZEAT everifysizeat)
{
    unsigned long long size = 0;

    int fd = open(device.c_str(), O_RDONLY);
    if (-1 != fd)
    {
        bool bisreadable = IsReadable(fd, device, nsecs, secsz, everifysizeat);
        bool bshouldbrute = bisreadable ? (E_ATMORETHANSIZE == everifysizeat) : (E_ATLESSTHANSIZE == everifysizeat);
        if (bshouldbrute)
        {
             DebugPrintf(SV_LOG_WARNING, "For device: %s, size: " ULLSPEC " is invalid. Getting size by reading\n", device.c_str(), nsecs * secsz);
             size = ReadAndFindSize(fd, device);
             DebugPrintf(SV_LOG_DEBUG, "For device: %s, size is " ULLSPEC " is got by reading\n", device.c_str(), size);
        }
        else
        {
            size = nsecs * secsz;
        }
        close(fd);
    }

    return size;
}


unsigned long long ReadAndFindSize(int fd, const std::string &device)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with fd = %d and device = %s\n", FUNCTION_NAME, fd, device.c_str());
    char            buf[DEV_BSIZE];
    diskaddr_t      read_failed_from_top = 0;
    diskaddr_t      read_succeeded_from_bottom = 0;
    diskaddr_t      current_file_position;
    unsigned long long sizefromread = 0;
    off_t lseekrval = 0;
    int fd_dsk = -1; 
    
    if (((lseek(fd, (offset_t)0, SEEK_SET)) == -1) || ((read(fd, buf, DEV_BSIZE)) == -1))
    {
        DebugPrintf(SV_LOG_ERROR,"@ LINE %d in FILE %s, lseek failed for device %s with errno = %d\n", LINE_NO, FILE_NAME, device.c_str(), errno);
        return 0;  
    }

    for (current_file_position = NUM_SECTORS_PER_TB * 4; read_failed_from_top == 0 && current_file_position < SIZE_LIMIT_TOP; current_file_position += 4 * NUM_SECTORS_PER_TB) 
    {
        if (((lseek(fd, (offset_t)(current_file_position * DEV_BSIZE), SEEK_SET)) == -1) || ((read(fd, buf, DEV_BSIZE)) != DEV_BSIZE))
        {
            read_failed_from_top = current_file_position;
        }
        else
        {
            read_succeeded_from_bottom = current_file_position;
        }
    }

    if (read_failed_from_top == 0)
    {
        DebugPrintf(SV_LOG_ERROR,"@ LINE %d in FILE %s, read failed for device %s with errno = %d\n", LINE_NO, FILE_NAME, device.c_str(), errno);
        return 0;
    }

    while (read_failed_from_top - read_succeeded_from_bottom > 1) 
    {
        current_file_position = read_succeeded_from_bottom + (read_failed_from_top - read_succeeded_from_bottom)/2;
        if (((lseek(fd, (offset_t)(current_file_position * DEV_BSIZE), SEEK_SET)) == -1) || ((read(fd, buf, DEV_BSIZE)) != DEV_BSIZE))
        {
            read_failed_from_top = current_file_position;
        }
        else
        {
            read_succeeded_from_bottom = current_file_position;
        }
    }

    sizefromread = (((unsigned long long)(read_succeeded_from_bottom + 1)) * ((unsigned long long)DEV_BSIZE));

    /*
    DebugPrintf(SV_LOG_DEBUG,"@ LINE %d in FILE %s, size of device %s from read = %llu\n", 
                             LINE_NO, FILE_NAME, device.c_str(), sizefromread);
    */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with size = %llu for device %s\n",FUNCTION_NAME, sizefromread, device.c_str()); 
    return sizefromread;
}
