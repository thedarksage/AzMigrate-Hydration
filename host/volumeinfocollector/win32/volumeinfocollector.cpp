#define _WIN32_WINNT 0x0501

#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>
#include <winioctl.h>

#include "svdparse.h"
#include "devicefilter.h"
#include "VVDevControl.h"
#include "portablehelpers.h"
#include "volumeinfocollector.h"
#include "VsnapUser.h"
#include "portablehelpersmajor.h"
#include "inmstrcmp.h"

#include "diskinfocollector.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmalertdefs.h"
#include "configwrapper.h"
#include "deviceidinformer.h"
#include "VersionHelpers.h"

#include "securityutils.h"

static DWORD const ValueLength = 8192;

using namespace std;

extern bool IsUEFIBoot(void);
extern bool GetSystemDiskList(std::set<SV_ULONG>& diskIndices, std::string &err, DWORD &errcode);

static bool IsRepeatingSlash(char ch1, char ch2)
{
    return ((ACE_DIRECTORY_SEPARATOR_CHAR_A == ch1) && (ch2 == ch1));
}


void VolumeInfoCollector::GetVolumeInfos(VolumeSummaries_t& volumeSummaries, VolumeDynamicInfos_t& volumeDynamicInfos)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

}

void VolumeInfoCollector::GetVolumeInfos(VolumeSummaries_t& volumeSummaries, VolumeDynamicInfos_t& volumeDynamicInfos, bool dump)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    GetVolumeInfos(volumeSummaries, volumeDynamicInfos, dump, false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

}

void VolumeInfoCollector::GetVolumeInfos(VolumeSummaries_t& volumeSummaries, VolumeDynamicInfos_t& volumeDynamicInfos, bool dump, bool reportOnlyClusterVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    getEncryptionStatus();
    DiskNamesToDiskVolInfosMap diskNamesToDiskVolInfosMap;
    std::vector<volinfo> volinfos;
    DeviceVgInfos devicevginfos;
    GetDiskVolumeInfos(diskNamesToDiskVolInfosMap, devicevginfos);

    for (DiskNamesToDiskVolInfosMap::iterator it = diskNamesToDiskVolInfosMap.begin(); it != diskNamesToDiskVolInfosMap.end(); ++it) {
        volinfos.push_back(it->second);
        if (it->second.attributes.find(NsVolumeAttributes::IS_PART_OF_CLUSTER) != it->second.attributes.end() &&
            boost::iequals(it->second.attributes.at(NsVolumeAttributes::IS_PART_OF_CLUSTER), "true"))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Inserting volumegroup %s to cluster volumegroup set\n", FUNCTION_NAME, it->second.volumegroupname.c_str());
            m_clusterVolumeGroups.insert(it->second.volumegroupname);
        }
    }

    if (volinfos.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "disks not collected. This may be due to wmi service unavailable\n");
        SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_HOST_REGISTRATION, WmiServiceBadAlert());
    }

    bool isnewvirtualvolume = false;
    std::string sparsefilename;
    logicalDrives = getlogicaldrives();
    systemDrives = getsystemdrives();
    cacheVolume = getcacheddrives();
    vsnapDrives = getvsnapdrives();
    virtualDrives = getvirtualdrives();
    pagefileDrives = getpagefilevol();

    // addchildvolumes internally finds any drive letters with duplicate volume guids 
    for (int i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        //Bug: 7729
        TCHAR szVolume[4] = "X:\\";
        szVolume[0] = _T('A') + i;
        addchildvolumes(volinfos, szVolume);
    }

    for (int i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        DWORD dwDriveMask = 1 << i;

        volinfo vol;

        vol.devname = getvolname(i);
        if (logicalDrives &  dwDriveMask)
        {
            vol.mountpoint = vol.devname;
            vol.fstype = getdrivefstype(i);
            vol.locked = isdrivelocked(i);
            vol.mounted = true;
            if (vsnapDrives & dwDriveMask)
            {
                vol.voltype = VolumeSummary::VSNAP_MOUNTED;
                vol.vendor = VolumeSummary::INMVSNAP;
            }
            else if (virtualDrives & dwDriveMask)
            {
                vol.voltype = VolumeSummary::VOLPACK;
                vol.vendor = VolumeSummary::INMVOLPACK;
            }
            else
            {
                vol.voltype = VolumeSummary::LOGICAL_VOLUME;
                vol.vendor = VolumeSummary::NATIVE;
            }
            vol.systemvol = issystemdrive(dwDriveMask);
            vol.cachedirvol = iscachedirvol(i);
            vol.containpagefile = ispagefilevol(dwDriveMask);
            vol.rawcapacity = getrawvolumesizeUtf8(vol.devname);
            vol.capacity = getdrivecapacity(i);
            vol.freespace = getdrivefreespace(i);
            vol.writecachestate = getwritecachestate(i);
            vol.sectorsize = getdrivesectorsize(i);
            vol.volumelabel = getdrivelabel(i);
            updateEncryptionStatus(vol);
            //add it to list
            volinfos.push_back(vol);
        }
        else if (m_localConfigurator.IsVolpackDriverAvailable() || (!IsVolPackDevice(vol.devname.c_str(), sparsefilename, isnewvirtualvolume)))
        {
            if (canbevirtualvolume(vol.devname))
            {
                vol.mountpoint = "";
                vol.fstype = "";
                vol.locked = false;
                vol.mounted = false;
                vol.voltype = VolumeSummary::VSNAP_UNMOUNTED;
                /* TODO: should vendor be native ? */
                vol.vendor = VolumeSummary::INMVSNAP;
                vol.systemvol = false;
                vol.cachedirvol = false;
                vol.containpagefile = false;
                vol.capacity = 0;
                vol.freespace = 0;
                vol.writecachestate = VolumeSummary::WRITE_CACHE_DONTKNOW;
                vol.sectorsize = 0;
                vol.volumelabel = "";
                vol.rawcapacity = 0;
                //add it to list
                volinfos.push_back(vol);
            }
        }
    }
    getVirtualVolumeFromPersist(volinfos);
    bool registerSystemDrive = m_localConfigurator.getRegisterSystemDrive();

    MarkDisksOnBootDiskController(volinfos);

    for (vector<volinfo>::iterator i(volinfos.begin()); i != volinfos.end(); ++i) {
        if ((!(*i).systemvol && !(*i).cachedirvol) || registerSystemDrive) {
            VolumeSummary vol;
            VolumeDynamicInfo vdi;

            if (((*i).voltype == VolumeSummary::LOGICAL_VOLUME) &&
                (*i).vendor == VolumeSummary::NATIVE)
            {
                
                UpdateVolumeGroup(*i, devicevginfos);
                
                updateClusteredVolumeGroupInfo(*i);
            }
            
            vol.id = (*i).deviceid;
            vol.name = (*i).devname;
            vol.type = (*i).voltype;
            vol.vendor = (*i).vendor;
            vol.fileSystem = (*i).fstype;
            vol.mountPoint = (*i).mountpoint;
            vol.isMounted = (*i).mounted;
            vol.isSystemVolume = (*i).systemvol;
            vol.isCacheVolume = (*i).cachedirvol;
            vol.capacity = (*i).capacity;
            vol.locked = (*i).locked;
            vol.physicalOffset = 0;
            vol.sectorSize = (*i).sectorsize;
            vol.volumeLabel = (*i).volumelabel;
            vol.containPagefile = (*i).containpagefile;
            vol.volumeGroup = (*i).volumegroupname;
            vol.volumeGroupVendor = (*i).volumegroupvendor;
            vol.rawcapacity = (*i).rawcapacity;
            vol.writeCacheState = (*i).writecachestate;
            vol.formatLabel = (*i).formatlabel;
            vol.attributes = (*i).attributes;

            bool clusteredVol = false;
            if ((vol.attributes.find(NsVolumeAttributes::IS_PART_OF_CLUSTER) != vol.attributes.end()) &&
                boost::iequals(vol.attributes.at(NsVolumeAttributes::IS_PART_OF_CLUSTER), "true"))
            {
                clusteredVol = true;
            }

            if (reportOnlyClusterVolumes && !clusteredVol)
            {
                continue;
            }
            else
            {
                volumeSummaries.push_back(vol);
            }

            vdi.id = (*i).deviceid;
            vdi.name = (*i).devname;
            vdi.freeSpace = (*i).freespace;
            
            volumeDynamicInfos.push_back(vdi);
        }
    }

    if (dump)
    {
        display_devlist(volinfos);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VolumeInfoCollector::GetOsDiskInfos(VolumeSummaries_t & volumeSummaries, const bool &dump)
{
    /* No implementation yet */
}

unsigned long long VolumeInfoCollector::getdrivecapacity(unsigned int driveIndex)
{

    char szDrive[4] = "X:";
    szDrive[0] = 'A' + driveIndex;

    return getdrivecapacity(szDrive);
}

unsigned long long VolumeInfoCollector::getdrivecapacity(const char* volume)
{
    ULARGE_INTEGER uliQuota = { 0 };
    ULARGE_INTEGER uliTotalCapacity = { 0 };
    ULARGE_INTEGER uliFreeSpace = { 0 };
    unsigned long long capacity = 0;

    //	if(!isdrivelocked(driveIndex))
    //	{
    // PR#10815: Long Path support
    if (SVGetDiskFreeSpaceExUtf8(volume, &uliQuota, &uliTotalCapacity, &uliFreeSpace))
    {
        capacity = (unsigned long long)uliTotalCapacity.QuadPart;
    }
    else
    {
        capacity = 0;
        DebugPrintf(SV_LOG_WARNING, "FAILED: Unable to get volume capacity.(OK if volume is a hidden target or cluster volume offline).  \n");
    }
    // Do not get volume size from driver
    /*		ULONGLONG volCapacity = 0;
    if( !GetVolumeSize( &(szDrive[ 0 ]), &volCapacity ) )
    {
    DebugPrintf(SV_LOG_WARNING,"FAILED: Unable to get volume size.\n");
    }
    else
    {
    uliTotalCapacity.QuadPart = volCapacity;
    }
    */

    //	}
    return capacity;
}

unsigned long long VolumeInfoCollector::getdrivefreespace(unsigned int driveIndex)
{

    char szDrive[4] = "X:";
    szDrive[0] = 'A' + driveIndex;

    return getdrivefreespace(szDrive);
}

unsigned long long VolumeInfoCollector::getdrivefreespace(const char* volume)
{
    ULARGE_INTEGER uliQuota = { 0 };
    ULARGE_INTEGER uliTotalCapacity = { 0 };
    ULARGE_INTEGER uliFreeSpace = { 0 };
    unsigned long long volFreeSpace = 0;

    if (!isdrivelocked(volume))
    {
        // PR#10815: Long Path support
        if( !SVGetDiskFreeSpaceExUtf8( volume, &uliQuota, &uliTotalCapacity, &uliFreeSpace ) )
        {
            DebugPrintf(SV_LOG_WARNING, "FAILED: Unable to get free space on volume.\n");
        }
    }

    volFreeSpace = (unsigned long long)uliFreeSpace.QuadPart;

    return volFreeSpace;
}

std::string VolumeInfoCollector::getdrivefstype(unsigned int driveIndex)
{

    char szVolumeName[4];
    char szFileSystemType[BUFSIZ] = { 0 };
    std::string strFileSystemType;
    bool ret = false;
    std::string strOutVolumeName;
    std::string fsName = "Unknown";

    memset(szFileSystemType, '\000', sizeof(szFileSystemType));
    DWORD dwMaxFilenameLength = 0;
    DWORD dwFlags = 0;

    szVolumeName[0] = 'A' + driveIndex;
    szVolumeName[1] = ':';
    szVolumeName[2] = '\\';
    szVolumeName[3] = '\0';

    if (!isdrivelocked(driveIndex, sizeof(szFileSystemType), szFileSystemType))
    {
        // PR#10815: Long Path support
        if (!SVGetVolumeInformationUtf8(szVolumeName,
            strOutVolumeName,
            NULL,
            &dwMaxFilenameLength,
            &dwFlags,
            strFileSystemType))
        {
            if (ERROR_NOT_READY == GetLastError()) //Cluster offline drives
            {
                //TODO: need to check if we receive valid fstype or not
                //if we dont receive valid fstype, then change ret = false;
            }
        }
        else
        {
            fsName = strFileSystemType;
        }
    }
    else
    {
        fsName = szFileSystemType; //islocked would have returned the filesystem type
    }

    return fsName;
}


bool VolumeInfoCollector::isdrivelocked(unsigned int driveIndex, int fsTypeSize, char * fsType)
{

    char szVolumeName[4];

    szVolumeName[0] = 'A' + driveIndex;
    szVolumeName[1] = ':';
    szVolumeName[2] = '\\';
    szVolumeName[3] = '\0';

    return isdrivelocked(szVolumeName, fsTypeSize, fsType);
}

bool VolumeInfoCollector::isdrivelocked(const char* volume, int fsTypeSize, char * fsType)
{
    bool locked = false;
    if (m_localConfigurator.isMobilityAgent())
    {
        locked = false;
    }
    else if (IsVolumeLockedUtf8(volume, fsTypeSize, fsType))
    {
        locked = true;
    }
    else
    {
        // Check for volume availability
        //
        std::string sGuid = volume;
        FormatVolumeNameToGuidUtf8(sGuid);
        // PR#10815: Long Path support
        HANDLE hVolume = SVCreateFileUtf8(sGuid.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (INVALID_HANDLE_VALUE == hVolume)
        {
            locked = true;
        }
        else
        {
            locked = false;
            CloseHandle(hVolume);
        }
    }

    return locked;
}


VolumeSummary::WriteCacheState VolumeInfoCollector::getwritecachestate(unsigned int driveIndex)
{
    char szVolumeName[4];

    szVolumeName[0] = 'A' + driveIndex;
    szVolumeName[1] = ':';
    szVolumeName[2] = '\\';
    szVolumeName[3] = '\0';

    return getwritecachestate(szVolumeName);
}


VolumeSummary::WriteCacheState VolumeInfoCollector::getwritecachestate(const char* volume)
{
    VolumeSummary::WriteCacheState state = VolumeSummary::WRITE_CACHE_DONTKNOW;

    // Check for volume availability
    //
    std::string sGuid = volume;
    FormatVolumeNameToGuidUtf8(sGuid);
    /* file share read write is needed but generic read is enough */
    // PR#10815: Long Path support
    HANDLE hVolume = SVCreateFileUtf8( sGuid.c_str(),
        GENERIC_READ,
        (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD err;

    if (INVALID_HANDLE_VALUE != hVolume)
    {
        DWORD rr;
        DWORD returned;
        DISK_CACHE_INFORMATION info;
        memset(&info, 0, sizeof(info));

        rr = DeviceIoControl(hVolume, IOCTL_DISK_GET_CACHE_INFORMATION, NULL,
            0, (LPVOID)&info, (DWORD)sizeof(info), (LPDWORD)&returned, (LPOVERLAPPED)NULL);
        if (rr)
        {
            state = info.WriteCacheEnabled ? VolumeSummary::WRITE_CACHE_ENABLED :
                VolumeSummary::WRITE_CACHE_DISABLED;
        }
        else
        {
            err = GetLastError();
            std::stringstream errmsg;
            errmsg << "IOCTL_DISK_GET_CACHE_INFORMATION for " << sGuid
                << " failed with error = " << err
                << '\n';
            DebugPrintf(SV_LOG_WARNING, "%s", errmsg.str().c_str());
        }

        CloseHandle(hVolume);
    }
    else
    {
        err = GetLastError();
        std::stringstream errmsg;
        errmsg << "SVCreateFile for " << sGuid
            << " failed with error = " << err
            << '\n';
        DebugPrintf(SV_LOG_WARNING, "%s", errmsg.str().c_str());
    }

    return state;
}

string VolumeInfoCollector::getextendedvolumename(const char* volume)
{
    if (0 == volume[3])
    {
        char szExtendedVolumeName[BUFSIZ] = "\\\\.\\X:";
        szExtendedVolumeName[4] = volume[0];
        return string(szExtendedVolumeName);
    }

    // TODO: return extended volume name for mount point
    char szExtendedVolumeName[BUFSIZ] = "\\\\.\\X:";
    szExtendedVolumeName[4] = volume[0];
    return string(szExtendedVolumeName);
}

DWORD VolumeInfoCollector::getvsnapdrives()
{
    if (!m_localConfigurator.IsVsnapDriverAvailable())
        return 0;
    DWORD dwDrives = GetLogicalDrives();
    if (0 == dwDrives)
    {
        return dwDrives;
    }

    size_t countCopied = 0;

    DWORD dwLockedDrives = 0;
    DWORD dwUnusedDrives = 0;
    DWORD dwVsnapDrives = 0;
    int i;

    for (i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        DWORD dwDriveMask = 1 << i;

        char szVolumeName[4];
        szVolumeName[0] = 'A' + i;
        szVolumeName[1] = ':';
        szVolumeName[2] = '\\';
        szVolumeName[3] = '\0';

        if (DRIVE_FIXED == GetDriveType(szVolumeName))
        {
            if (isdrivelocked(i))
            {
                dwLockedDrives |= dwDriveMask;
            }
        }
    }
    HANDLE VVCtrlDevice = INVALID_HANDLE_VALUE;
    char vol[4];
    WCHAR   VolumeLink[256];
    DWORD   BytesReturned = 0;
    bool bResult;
    DWORD HiddenDrives = 0;
    vol[0] = 'A' - 1;
    vol[1] = ':';
    vol[2] = '\\';
    vol[3] = '\0';

    DWORD UnUsedDrivesMask = ~dwDrives;
    UnUsedDrivesMask &= 0x3FFFFFF; //Valid drives are from A-Z only.
    VVCtrlDevice = SVCreateFile(VV_CONTROL_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        NULL, NULL);

    if (INVALID_HANDLE_VALUE == VVCtrlDevice)
        return 0;
    for (i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        DWORD dwDriveMask = 1 << i;
        vol[0]++;

        if (dwDrives & dwDriveMask)
        {
            string	UniqueId;
            UniqueId.clear();

            UniqueId = VSNAP_UNIQUE_ID_PREFIX;
            UniqueId += vol;
            bResult = DeviceIoControl(VVCtrlDevice, IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT, (void *)UniqueId.c_str(),
                (SV_ULONG)(UniqueId.size() + 1), VolumeLink, (SV_ULONG) sizeof(VolumeLink), &BytesReturned, NULL);

            //This ioctl succeeds if the volume is virtual.
            if (bResult)
            {
                std::vector<CHAR> vVolumeLink(256);
                wcstombs_s(&countCopied, &vVolumeLink[0], vVolumeLink.size(), VolumeLink, vVolumeLink.size()-1);
                if (IsVolumeNameInGuidOrGlobalGuidFormat(vVolumeLink.data()))
                {
                    dwVsnapDrives |= dwDriveMask; // a vsnap volume
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "The IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT ioctl doesnt return a valid guid for volume %s. %s:%d\n", vol, FILE_NAME, LINE_NO);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "The IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT failed for volume %s.Could be a standard device %s:%d\n", vol, FILE_NAME, LINE_NO);
            }
        }
    }
    CloseHandle(VVCtrlDevice);
    return dwVsnapDrives;
}

DWORD VolumeInfoCollector::getvirtualdrives()
{
    if (!m_localConfigurator.IsVolpackDriverAvailable())
        return 0;
    DWORD dwDrives = GetLogicalDrives();
    if (0 == dwDrives)
    {
        return dwDrives;
    }
    
    size_t countCopied = 0;

    DWORD dwLockedDrives = 0;
    DWORD dwUnusedDrives = 0;
    DWORD dwVirtualDrives = 0;

    int i;

    for (i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        DWORD dwDriveMask = 1 << i;

        char szVolumeName[4];
        szVolumeName[0] = 'A' + i;
        szVolumeName[1] = ':';
        szVolumeName[2] = '\\';
        szVolumeName[3] = '\0';

        if (DRIVE_FIXED == GetDriveType(szVolumeName))
        {
            if (isdrivelocked(i))
            {
                dwLockedDrives |= dwDriveMask;
            }
        }
    }

    HANDLE VVCtrlDevice = INVALID_HANDLE_VALUE;
    char vol[4];
    WCHAR   VolumeLink[256];
    DWORD   BytesReturned = 0;
    bool bResult;
    DWORD HiddenDrives = 0;

    vol[0] = 'A' - 1;
    vol[1] = ':';
    vol[2] = '\\';
    vol[3] = '\0';

    DWORD UnUsedDrivesMask = ~dwDrives;
    UnUsedDrivesMask &= 0x3FFFFFF; //Valid drives are from A-Z only.

    // PR#10815: Long Path support
    VVCtrlDevice = SVCreateFile(VV_CONTROL_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        NULL, NULL);

    if (INVALID_HANDLE_VALUE == VVCtrlDevice)
        return 0;


    for (i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        DWORD dwDriveMask = 1 << i;
        vol[0]++;

        if (dwDrives & dwDriveMask)
        {
            string	UniqueId;
            UniqueId.clear();
            UniqueId = vol;
            UniqueId[0] = tolower(UniqueId[0]);
            FormatVolumeName(UniqueId);
            bResult = DeviceIoControl(VVCtrlDevice, IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT, (void *)UniqueId.c_str(),
                (SV_ULONG)(UniqueId.size() + 1), VolumeLink, (SV_ULONG) sizeof(VolumeLink), &BytesReturned, NULL);
            if (bResult)
            {
                CHAR szVolumeLink[256];
                memset((void*)szVolumeLink, 0, sizeof(szVolumeLink));
                wcstombs_s(&countCopied, szVolumeLink, ARRAYSIZE(szVolumeLink), VolumeLink, ARRAYSIZE(szVolumeLink)-1);
                if (IsVolumeNameInGuidOrGlobalGuidFormat(szVolumeLink))
                {
                    dwVirtualDrives |= dwDriveMask; // a virtual volume
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "The IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT ioctl doesnt return a valid guid for virtual volume %s. %s:%d\n", vol, FILE_NAME, LINE_NO);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "The IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT (volpack) failed for volume %s.Could be a standard device %s:%d\n", vol, FILE_NAME, LINE_NO);
            }
        }
    }

    CloseHandle(VVCtrlDevice);
    return dwVirtualDrives;

}

DWORD VolumeInfoCollector::getlogicaldrives()
{

    DWORD dwDrives = GetLogicalDrives();

    DebugPrintf(SV_LOG_DEBUG, "%s: Logical Drives : 0x%x\n", FUNCTION_NAME, dwDrives);

    if (dwDrives == 0)
    {
        //TODO: Log error msg
        return dwDrives;
    }

    for (int i = 0; i < MAX_LOGICAL_DRIVES; i++)
    {
        DWORD dwDriveMask = 1 << i;

        if (0 == (dwDrives &  dwDriveMask))
        {
            continue;
        }

        char szVolumeName[4];
        szVolumeName[0] = 'A' + i;
        szVolumeName[1] = ':';
        szVolumeName[2] = '\\';
        szVolumeName[3] = '\0';


        if (DRIVE_FIXED != GetDriveType(szVolumeName))
        {
            //reset non-fixed drives
            dwDrives &= ~dwDriveMask;
            nonreportableDrives |= dwDriveMask;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "%s: Non Reportable Drives : 0x%x\n", FUNCTION_NAME, nonreportableDrives);

    return dwDrives;
}


std::string VolumeInfoCollector::getcacheddrives()
{
    //TODO: need to add support for receiving cached drive from Configurator

    std::string cacheDir = m_localConfigurator.getCacheDirectory();
    std::vector<char> vCacheVolume(MAX_PATH + 1);
    // PR#10815: Long Path support
    SVGetVolumePathName(cacheDir.c_str(), &vCacheVolume[0], vCacheVolume.size());

    return vCacheVolume.data();
}
bool VolumeInfoCollector::iscachedirvol(std::string const& vol)
{
    return sameVolumeCheckUtf8(vol, cacheVolume);
}

bool VolumeInfoCollector::ispagefilevol(DWORD driveIndex)
{
    return (pagefileDrives & driveIndex);
}

bool VolumeInfoCollector::iscachedirvol(DWORD driveIndex)
{

    char szVolumeName[4];
    szVolumeName[0] = 'A' + static_cast<unsigned int>(driveIndex);
    szVolumeName[1] = ':';
    szVolumeName[2] = '\\';
    szVolumeName[3] = '\0';

    std::string volume = szVolumeName;
    return sameVolumeCheckUtf8(volume, cacheVolume);
}

//BUGBUG: need to add support for treating CacheDirectory as system drive
DWORD VolumeInfoCollector::getsystemdrives()
{

    DWORD dwSystemDrives;
    TCHAR rgtszSystemDirectory[MAX_PATH + 1];

    if (0 == GetSystemDirectory(rgtszSystemDirectory, MAX_PATH + 1))
    {
        //TODO: Log error msg to cx
        dwSystemDrives = 0;
    }
    else
    {
        dwSystemDrives = 1 << (toupper(rgtszSystemDirectory[0]) - 'A');
    }

    //dwSystemDrives |= getcacheddrives();

    return dwSystemDrives;
}

bool VolumeInfoCollector::issystemdrive(DWORD driveIndex)
{
    return (systemDrives & driveIndex);

}

std::string VolumeInfoCollector::getvolname(unsigned int driveIndex)
{
    char szVolumeName[2];

    szVolumeName[0] = 'A' + driveIndex;
    szVolumeName[1] = '\0';
    /*  We no longer need to have trailing : and \
    szVolumeName[ 1 ] = ':';
    szVolumeName[ 2 ] = '\\';
    szVolumeName[ 3 ] = '\0';
    */
    std::string sVolumeName = "";

    return  (sVolumeName = szVolumeName);
}

std::string VolumeInfoCollector::getdrivelabel(unsigned int driveIndex)
{
    char szVolumeName[4];

    szVolumeName[0] = 'A' + driveIndex;
    szVolumeName[1] = ':';
    szVolumeName[2] = '\\';
    szVolumeName[3] = '\0';

    return getdrivelabel(szVolumeName);
}

std::string VolumeInfoCollector::getdrivelabel(char const* volume)
{
    char szFileSystemType[BUFSIZ] = { 0 };
    std::string strFileSystemType;
    std::string strOutvolumeLabel;
    memset(szFileSystemType, '\0', sizeof(szFileSystemType));

    DWORD dwMaxFilenameLength = 0;
    DWORD dwFlags = 0;
    if (!isdrivelocked(volume, sizeof(szFileSystemType), szFileSystemType))
    {
        // PR#10815: Long Path support	
        if( !SVGetVolumeInformationUtf8(volume,
            strOutvolumeLabel,
            NULL,
            &dwMaxFilenameLength,
            &dwFlags,
            strFileSystemType) )
        {
            // volume is either locked or offlined(cluster);
        }
    }
    else
    {
        //islocked would have returned the filesystem type
    }

    return strOutvolumeLabel;
}

unsigned int VolumeInfoCollector::getdrivesectorsize(unsigned int driveIndex)
{

    char volName[4];
    volName[0] = 'A' + driveIndex;
    volName[1] = ':';
    volName[2] = '\\';
    volName[3] = '\0';

    return getdrivesectorsize(volName);
}

unsigned int VolumeInfoCollector::getdrivesectorsize(const char* volume)
{
    DWORD bytesPerSector;

    // PR#10815: Long Path support
    if (!SVGetDiskFreeSpaceUtf8(volume, NULL, &bytesPerSector, NULL, NULL)) {
        bytesPerSector = DEFAULT_SECTOR_SIZE;
    }

    return (unsigned int)bytesPerSector;
}

bool VolumeInfoCollector::canbevirtualvolume(const std::string& sVolumeName)
{
    std::string sVolume = sVolumeName;

    if (sVolume.empty())
        return false;

    if (IsDrive(sVolume))
    {
        UnformatVolumeNameToDriveLetter(sVolume);
        FormatVolumeNameForMountPoint(sVolume);
        if (DRIVE_NO_ROOT_DIR == GetDriveType(sVolume.c_str()))
        {
            return true;
        }
    }
    else if (IsMountPoint(sVolume))
        return true;
    return false;
}

bool VolumeInfoCollector::addchildvolumes(vector<volinfo> &vinfo, const char * szVolume)
{
    DebugPrintf(SV_LOG_DEBUG, "\n%s volume:%s\n", FUNCTION_NAME, szVolume);
    std::string strVolumeNameBuffer;
    DWORD MaxPathComponentLength = 0;
    DWORD FileSystemFlags = 0;
    std::string strFileSystemName;
	std::wstring wszVolume = convertUtf8ToWstring(szVolume);

    // PR#10815: Long Path support
    if (!SVGetVolumeInformationUtf8(
        szVolume,
        strVolumeNameBuffer,
        NULL /* serial number */,
        &MaxPathComponentLength,
        &FileSystemFlags,
        strFileSystemName))
    {
        return false;
    }

    if (!(FileSystemFlags & FILE_SUPPORTS_REPARSE_POINTS))
    {
        return false;
    }

    wchar_t MountPointBuffer[MAX_PATH + 1];
    HANDLE hMountPoint = FindFirstVolumeMountPointW(wszVolume.c_str(), MountPointBuffer, NELEMS(MountPointBuffer));
    if (INVALID_HANDLE_VALUE == hMountPoint)
    {
        return false;
    }
    //added for bug 7729
	std::wstring MountPoint = wszVolume + std::wstring(MountPointBuffer);
    addchildvolumes( vinfo, convertWstringToUtf8(MountPoint.c_str()).c_str() );

    HANDLE VVCtrlDevice = INVALID_HANDLE_VALUE;

    // PR#10815: Long Path support
    VVCtrlDevice = SVCreateFile(VV_CONTROL_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        NULL, NULL);

    if (INVALID_HANDLE_VALUE == VVCtrlDevice)
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to open vv control device.\n");
    }

    //This ioctl succeeds if the volume is virtual.

    // process mount point
    updateVolInfos(vinfo, VVCtrlDevice, szVolume, convertWstringToUtf8(MountPointBuffer).c_str());

    while (FindNextVolumeMountPointW(hMountPoint, MountPointBuffer, NELEMS(MountPointBuffer)))
    {
        // process mount point
        //added for bug 7729
        MountPoint = wszVolume + std::wstring(MountPointBuffer);
        addchildvolumes(vinfo, convertWstringToUtf8(MountPoint.c_str()).c_str());
        updateVolInfos(vinfo, VVCtrlDevice, szVolume, convertWstringToUtf8(MountPointBuffer).c_str());
    }

    if (INVALID_HANDLE_VALUE != VVCtrlDevice)
        CloseHandle(VVCtrlDevice);

    if (INVALID_HANDLE_VALUE != hMountPoint)
        FindVolumeMountPointClose(hMountPoint);

    return true;
}

void VolumeInfoCollector::updateVolInfos(vector<volinfo>& vinfo, HANDLE VVCtrlDevice, const char* szVolume, const char* MountPointBuffer)
{
    std::string strVolumeName;
    DWORD MaxPathComponentLength = 0;
    DWORD FileSystemFlags = 0;
    std::string strFileSystemName;
    string UniqueId;
    size_t countCopied = 0;

    UniqueId = VSNAP_UNIQUE_ID_PREFIX;

    volinfo vol;
    vol.devname = string(szVolume) + string(MountPointBuffer);
    vol.mountpoint = vol.devname;

    FileSystemFlags = 0;
    MaxPathComponentLength = 0;

    // PR#10815: Long Path support
    if (!SVGetVolumeInformationUtf8(
        vol.devname.c_str(),
        strVolumeName,
        NULL /* serial number */,
        &MaxPathComponentLength,
        &FileSystemFlags,
        strFileSystemName))
    {
        DebugPrintf(SV_LOG_DEBUG, "Failed to acquire volume information for volume %s.\n", vol.devname.c_str());
    }
    ACE_HANDLE handle = ACE_INVALID_HANDLE;
    SVERROR sve = OpenVolumeExtendedUtf8(&handle, vol.devname.c_str(), GENERIC_READ);
    if (sve.succeeded())
    {
        CloseVolume(handle);
        vol.fstype = strFileSystemName; // TODO: handle volume locked case as 'Unknown'
        vol.locked = IsVolumeLockedUtf8(vol.devname.c_str()); // TODO: handle volume locked case
        vol.mounted = true;  // NOTE: not obvious how to implement unmounted volumes on Windows
        /*
        * For now, compare c:\srv to report as system volume.
        * Correct approach is to use api / ioctl to find upfront
        * the GUID of boot partition.
        */
        vol.systemvol = false;
        if (0 == InmStrCmp<NoCaseCharCmp>(vol.devname, INM_BOOT_VOLUME_MOUNTPOINT))
        {
            vol.systemvol = true;
        }
        vol.cachedirvol = iscachedirvol(vol.devname);
        vol.capacity = getdrivecapacity(vol.mountpoint.c_str());
        vol.freespace = getdrivefreespace(vol.mountpoint.c_str());
        vol.writecachestate = getwritecachestate(vol.mountpoint.c_str());
        vol.sectorsize = getdrivesectorsize(vol.mountpoint.c_str());
        vol.volumelabel = getdrivelabel(vol.mountpoint.c_str());
        vol.rawcapacity = getrawvolumesizeUtf8(vol.devname);

        if (INVALID_HANDLE_VALUE != VVCtrlDevice)
        {
            //UniqueId += _strlwr(const_cast<char*>(vol.devname.c_str()));
            std::string sVolumeName = vol.devname;
            UniqueId += _strlwr(const_cast<char*>(sVolumeName.c_str()));

            WCHAR   VolumeLink[256];
            memset((void*)VolumeLink, 0, sizeof(VolumeLink)); //Init to null.
            DWORD   BytesReturned = 0;

            bool bResult = DeviceIoControl(VVCtrlDevice,
                IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
                (void *)UniqueId.c_str(),
                (SV_ULONG)(UniqueId.size() + 1),
                VolumeLink,
                (SV_ULONG) sizeof(VolumeLink),
                &BytesReturned,
                NULL);

            if (bResult)
            {
                CHAR szVolumeLink[256];
                memset((void*)szVolumeLink, 0, sizeof(szVolumeLink));
                wcstombs_s(&countCopied, szVolumeLink, ARRAYSIZE(szVolumeLink), VolumeLink, ARRAYSIZE(szVolumeLink)-1);
                if (IsVolumeNameInGuidOrGlobalGuidFormat(szVolumeLink))
                {
                    vol.voltype = VolumeSummary::VSNAP_MOUNTED; // a virtual volume
                    vol.vendor = VolumeSummary::INMVSNAP;
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "The IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT ioctl doesnt return a valid guid for volume %s. %s:%d", vol.devname.c_str(), FILE_NAME, LINE_NO);
                    vol.voltype = VolumeSummary::LOGICAL_VOLUME;
                    vol.vendor = VolumeSummary::NATIVE;
                }
            }
            else
            {
                UniqueId.clear();
                UniqueId = _strlwr(const_cast<char*>(sVolumeName.c_str()));
                memset((void*)VolumeLink, 0, sizeof(VolumeLink));
                bResult = DeviceIoControl(VVCtrlDevice,
                    IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT,
                    (void *)UniqueId.c_str(),
                    (SV_ULONG)(UniqueId.size() + 1),
                    VolumeLink,
                    (SV_ULONG) sizeof(VolumeLink),
                    &BytesReturned,
                    NULL);
                if (bResult)
                {
                    CHAR szVolumeLink[256];
                    memset((void*)szVolumeLink, 0, sizeof(szVolumeLink));
                    wcstombs_s(&countCopied, szVolumeLink, ARRAYSIZE(szVolumeLink), VolumeLink, ARRAYSIZE(szVolumeLink)-1);
                    //cout << OutputBuffer << endl;
                    if (IsVolumeNameInGuidOrGlobalGuidFormat(szVolumeLink))
                    {
                        vol.voltype = VolumeSummary::VOLPACK; // a virtual volume
                        vol.vendor = VolumeSummary::INMVOLPACK;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG, "The IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT ioctl doesnt return a valid guid for volume %s. %s:%d", vol.devname.c_str(), FILE_NAME, LINE_NO);
                        vol.voltype = VolumeSummary::LOGICAL_VOLUME;
                        vol.vendor = VolumeSummary::NATIVE;
                    }
                }
                else
                {
                    //DeviceIoControl failed.
                    DebugPrintf(SV_LOG_DEBUG, "DeviceIoControl failed for IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT. Reporting %s as physical drive.  %s:%d", vol.devname.c_str(), FILE_NAME, LINE_NO);
                    vol.voltype = VolumeSummary::LOGICAL_VOLUME;
                    vol.vendor = VolumeSummary::NATIVE;
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Unable to open handle to the VVControlDevice. Reporting %s volume as physical drive. %s:%d", vol.devname.c_str(), FILE_NAME, LINE_NO);
            vol.voltype = VolumeSummary::LOGICAL_VOLUME;
            vol.vendor = VolumeSummary::NATIVE;
        }
        updateEncryptionStatus(vol);

        vinfo.push_back(vol);
    }

    return;
}

void VolumeInfoCollector::getEncryptionStatus()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    const std::string wmiProvider = "root\\CIMV2\\Security\\MicrosoftVolumeEncryption";
    WmiEncryptableVolumeRecordProcessor wencrvolInfo(bitlockerProtectionStatusMap, bitlockerConversionStatusMap);
    GenericWMI gwmiboot(&wencrvolInfo);
    SVERROR sv = gwmiboot.init(wmiProvider);
    if (sv == SVS_OK)
    {
        gwmiboot.GetData("Win32_EncryptableVolume");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VolumeInfoCollector::updateEncryptionStatus(volinfo &vol)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string deviceID = vol.devname;
    FormatVolumeNameToGuidUtf8(deviceID);
    if (deviceID[deviceID.length() - 1] != '\\')
    {
        deviceID += '\\';
    }

    // TODO-SanKumar-1806: In case of EHDD, we can support the disk actually, since the encryption
    // happens below our stack.

    // TODO-SanKumar-1806: Keeping this value in 1805 for continued support. The CS has to decide
    // based on the below 2 attributes specific to bitlocker. This way we can defer the decision
    // making to CS as well as support more encryption methods (ex: EFS) in the future.
    vol.attributes.insert(std::make_pair(NsVolumeAttributes::ENCRYPTION, bitlockerProtectionStatusMap[deviceID]));

    std::map<std::string, std::string>::const_iterator itr;

    itr = bitlockerProtectionStatusMap.find(deviceID);
    if (itr != bitlockerProtectionStatusMap.cend())
        vol.attributes.insert(std::make_pair(NsVolumeAttributes::BITLOCKER_PROT_STATUS, itr->second));

    itr = bitlockerConversionStatusMap.find(deviceID);
    if (itr != bitlockerConversionStatusMap.cend())
        vol.attributes.insert(std::make_pair(NsVolumeAttributes::BITLOCKER_CONV_STATUS, itr->second));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool VolumeInfoCollector::getVirtualVolumeFromPersist(std::vector<volinfo> & volinfos)
{
    bool rv = true;

    if (m_localConfigurator.IsVolpackDriverAvailable())
        return rv;

    //Note: Instantiating LocalConfigurator here again as new virtual volumes might have been
    //created.
    LocalConfigurator localConfigurator;
    int counter = localConfigurator.getVirtualVolumesId();
    while (counter != 0)
    {
        stringstream regData;
        regData << "VirVolume" << counter;
        string data = localConfigurator.getVirtualVolumesPath(regData.str());
        std::string sparsefilename, volume;

        if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            counter--;
            continue;
        }
        else
        {
            SV_ULONGLONG size = 0;
            ACE_stat s;
            std::string sparsefile;
            bool newvirtualvolume = false;
            IsVolPackDevice(volume.c_str(), sparsefile, newvirtualvolume);
            if (newvirtualvolume)
            {
                int i = 0;
                std::stringstream sparsepartfile;
                while (true)
                {
                    sparsepartfile.str("");
                    sparsepartfile << sparsefilename << SPARSE_PARTFILE_EXT << i;
                    if (sv_stat(getLongPathName(sparsepartfile.str().c_str()).c_str(), &s) < 0)
                    {
                        break;
                    }
                    size += s.st_size;
                    i++;
                }
            }
            else
                size = (sv_stat(getLongPathName(sparsefilename.c_str()).c_str(), &s) < 0) ? 0 : s.st_size;
            if (IsDrive(volume))
            {
                FormatVolumeNameForCxReporting(volume);
                volume[0] = toupper(volume[0]);
            }
            else
            {
                volume.erase(unique(volume.begin(), volume.end(), IsRepeatingSlash), volume.end());
                volume[0] = toupper(volume[0]);
            }

            volinfo vol;
            vol.devname = volume;
            vol.mountpoint = volume;
            vol.fstype = "NTFS";
            vol.locked = false;
            vol.mounted = true;
            vol.voltype = VolumeSummary::VOLPACK;
            vol.vendor = VolumeSummary::INMVOLPACK;
            vol.systemvol = false;
            vol.cachedirvol = false;
            vol.containpagefile = false;
            vol.capacity = size;
            vol.freespace = 0;
            vol.writecachestate = VolumeSummary::WRITE_CACHE_DONTKNOW;
            vol.sectorsize = 512;
            vol.volumelabel = "";
            vol.rawcapacity = size;
            //add it to list
            volinfos.push_back(vol);
            counter--;
        }
    }
    return rv;
}


void VolumeInfoCollector::GetDiskVolumeInfos(DiskNamesToDiskVolInfosMap & diskNamesToDiskVolInfosMap, DeviceVgInfos & devicevginfos)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static std::string s_discoveryChecksum;

    std::stringstream   ssDiscoveryInfo;

    ULONG   ulDiscoveryFlags = 0;

    bool getscsiid = m_localConfigurator.getScsiId();
    bool bReportScsiIdForDevName = m_localConfigurator.shouldReportScsiIdAsDevName();

    WmiDiskPartitionRecordProcessor wdpInfo(bReportScsiIdForDevName, getscsiid);
    GenericWMI gwmiboot(&wdpInfo);
    SVERROR sv = gwmiboot.init();
    if (sv != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "initializating generic wmi failed for disk partition\n");
    }
    else
    {
        gwmiboot.GetData("Win32_DiskPartition");
    }

    std::stringstream ss;
    std::set<std::string>::iterator iter = wdpInfo.m_BootableDiskIDs.begin();
    for (; iter != wdpInfo.m_BootableDiskIDs.end(); iter++)
    {
        ss << (*iter) << ", ";
    }
    DebugPrintf(SV_LOG_DEBUG, "Boot disk IDs: %s\n", ss.str().c_str());

    // On Master target, we are reporting scsi id for disk name
    // so irrespective of value for getScsiId in config file, we always set it to true
    if (bReportScsiIdForDevName){
        getscsiid = true;
    }

    bool isAzure2Azure = m_localConfigurator.IsAzureToAzureReplication();
    if (isAzure2Azure) {
        SET_FLAG(ulDiscoveryFlags, DISCOVERY_IGNORE_DISKS_WITHOUT_SCSI_ATTRIBS);
    }

    // Check whether OS Version is above W2K8R2 or not as Storage pool is supported above WK8R2 
    if (!isAzure2Azure && IsWindows8OrGreater())
    {
        DebugPrintf(SV_LOG_DEBUG, "Collecting MSFT_PhysicalDisk details\n");

        MsftPhysicalDiskRecordProcessor d(&diskNamesToDiskVolInfosMap, 
                                          &devicevginfos,
                                          &wdpInfo,
                                          bReportScsiIdForDevName,
                                          ulDiscoveryFlags,
                                          getscsiid);

        // Set storage namespace
        GenericWMI gwmpd(&d);
        SVERROR sv = gwmpd.init("root\\Microsoft\\Windows\\Storage");

        if (sv != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "initializating generic wmi for MSFT_Physicaldisk failed for disk drive\n");
            return;
        }

        // Get physical disk information
        gwmpd.GetData("MSFT_PhysicalDisk");

        DebugPrintf(SV_LOG_DEBUG, "Disks collected from MSFT_PhysicalDisk\n");
        DebugPrintf(SV_LOG_DEBUG, "--------------------------------------\n");
        for (auto diskIter : diskNamesToDiskVolInfosMap) {
            DebugPrintf(SV_LOG_DEBUG, "devnum: %d devname: %s deviceId: %s\n",
                diskIter.second.devno,
                diskIter.first.c_str(),
                diskIter.second.deviceid.c_str());
        }
    }

    // Process win32_diskdrive namespace
    if (!isAzure2Azure)
    {
        DebugPrintf(SV_LOG_DEBUG, "Collecting Win32_DiskDrive details\n");

        WmiDiskDriveRecordProcessor p(&diskNamesToDiskVolInfosMap,
                                      &devicevginfos,
                                      &wdpInfo,
                                      bReportScsiIdForDevName,
                                      getscsiid,
                                      ulDiscoveryFlags,
                                      m_localConfigurator.getClusSvcRetryTimeInSeconds());
        GenericWMI gwmi(&p);

        sv = gwmi.init();
        if (sv != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "initializating generic wmi failed for disk drive\n");
        }
        else
        {
            gwmi.GetData("Win32_DiskDrive");
        }

        DebugPrintf(SV_LOG_DEBUG, "Disks collected after Win32_DiskDrive\n");
        DebugPrintf(SV_LOG_DEBUG, "--------------------------------------\n");

        for (auto diskIter : diskNamesToDiskVolInfosMap) {
            DebugPrintf(SV_LOG_DEBUG, "devnum: %d devname: %s deviceId: %s\n",
                diskIter.second.devno,
                diskIter.first.c_str(),
                diskIter.second.deviceid.c_str());
        }
    }

    // Process any missing disk from driver on source but skip on Master Target.
    bool isMobilityAgent = m_localConfigurator.isMobilityAgent();
    if (isMobilityAgent)
    {
        DebugPrintf(SV_LOG_DEBUG, "Collecting Driver disk details\n");

        DriverDiskRecordProcessor   d(&diskNamesToDiskVolInfosMap,
                                      &devicevginfos,
                                      &wdpInfo,
                                      bReportScsiIdForDevName,
                                      ulDiscoveryFlags,
                                      getscsiid);
        if (!d.Enumerate()) {
            DebugPrintf(SV_LOG_DEBUG, "driver disk enumeration failed\n");
        }
        else {
            if (d.Process()) {
                DebugPrintf(SV_LOG_DEBUG, "added disks from driver\n");
            }
            else {
                DebugPrintf(SV_LOG_ERROR, "Adding disks from driver failed\n");
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "Disks collected after Driver disk Query\n");
        DebugPrintf(SV_LOG_DEBUG, "---------------------------------------\n");

        for (auto diskIter : diskNamesToDiskVolInfosMap) {
            DebugPrintf(SV_LOG_DEBUG, "devnum: %d devname: %s deviceId: %s\n",
                diskIter.second.devno,
                diskIter.first.c_str(),
                diskIter.second.deviceid.c_str());
        }
    }

    // Process Disk from drscout.Conf
    {
        DebugPrintf(SV_LOG_DEBUG, "Collecting Configurator disk details\n");
        ConfigDiskProcessor   d(&diskNamesToDiskVolInfosMap,
            &devicevginfos,
            &wdpInfo,
            bReportScsiIdForDevName,
            ulDiscoveryFlags,
            getscsiid);
        if (!d.Enumerate()) {
            DebugPrintf(SV_LOG_DEBUG, "ConfigDiskProcessor disk enumeration failed\n");
        }
        else {
            if (d.Process()) {
                DebugPrintf(SV_LOG_DEBUG, "added disks from ConfigDiskProcessor\n");
            }
            else {
                DebugPrintf(SV_LOG_ERROR, "Adding disks from ConfigDiskProcessor failed\n");
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "Disks collected after Config disk Query\n");
        DebugPrintf(SV_LOG_DEBUG, "---------------------------------------\n");
        for (auto diskIter : diskNamesToDiskVolInfosMap) {
            DebugPrintf(SV_LOG_DEBUG, "devnum: %d devname: %s deviceId: %s\n",
                diskIter.second.devno,
                diskIter.first.c_str(),
                diskIter.second.deviceid.c_str());
        }
    }

    for (auto diskIter : diskNamesToDiskVolInfosMap) {
        ssDiscoveryInfo << "Added Disk" << diskIter.second.devno << " Name: " << diskIter.first.c_str()
            << " Id: " << diskIter.second.deviceid.c_str()
            << " Bus: " << diskIter.second.attributes[NsVolumeAttributes::INTERFACE_TYPE]
            << " Size: " << diskIter.second.rawcapacity 
            << " BytesPerSector: " << diskIter.second.sectorsize << std::endl;
    }

    std::string lastDiscoveredInfo = ssDiscoveryInfo.str();

    if (!lastDiscoveredInfo.empty()) {
        std::string checksum = securitylib::genSha256Mac(lastDiscoveredInfo.c_str(), lastDiscoveredInfo.length());

        if (!boost::iequals(checksum, s_discoveryChecksum)) {
            DebugPrintf(SV_LOG_ALWAYS, lastDiscoveredInfo.c_str());
            s_discoveryChecksum = checksum;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void VolumeInfoCollector::UpdateVolumeGroup(volinfo &vi, DeviceVgInfos &devicevginfos)
{
    std::string diskname;
    std::stringstream ss;
    std::string sGuid = vi.devname;
    FormatVolumeNameToGuidUtf8(sGuid);
    HANDLE  h = SVCreateFileUtf8(sGuid.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL
        );

    if (h == INVALID_HANDLE_VALUE)
    {
        ss << "Failed to open volume " << vi.devname << " with error " << GetLastError();
        DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
        return;
    }
    int EXPECTEDEXTENTS = 1;
    bool bexit = false;
    VOLUME_DISK_EXTENTS *vde = NULL;
    unsigned int volumediskextentssize;
    DWORD bytesreturned;
    DWORD err;

    do
    {
        if (vde)
        {
            free(vde);
            vde = NULL;
        }
        int nde;
        INM_SAFE_ARITHMETIC(nde = InmSafeInt<int>::Type(EXPECTEDEXTENTS) - 1, INMAGE_EX(EXPECTEDEXTENTS))
            INM_SAFE_ARITHMETIC(volumediskextentssize = sizeof (VOLUME_DISK_EXTENTS)+(nde * InmSafeInt<size_t>::Type(sizeof (DISK_EXTENT))), INMAGE_EX(sizeof (VOLUME_DISK_EXTENTS))(nde)(sizeof (DISK_EXTENT)))
            vde = (VOLUME_DISK_EXTENTS *)calloc(1, volumediskextentssize);

        if (NULL == vde)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to allocate %u bytes for %d expected extents, "
                "for IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS\n",
                volumediskextentssize, EXPECTEDEXTENTS);
            break;
        }

        bytesreturned = 0;
        err = 0;
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, volumediskextentssize, &bytesreturned, NULL))
        {
            for (DWORD i = 0; i < vde->NumberOfDiskExtents; i++)
            {
                DISK_EXTENT &de = vde->Extents[i];
                std::stringstream ssdiskindex;
                ssdiskindex << de.DiskNumber;
                diskname = DISKNAMEPREFIX;
                diskname += ssdiskindex.str();
                FillVolumeGroup(diskname, vi, devicevginfos);
            }
            bexit = true;
        }
        else if ((err = GetLastError()) == ERROR_MORE_DATA)
        {
            DebugPrintf(SV_LOG_DEBUG, "with EXPECTEDEXTENTS = %d, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS says error more data\n", EXPECTEDEXTENTS);
            EXPECTEDEXTENTS = vde->NumberOfDiskExtents;
        }
        else
        {
            ss << "IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed on disk " << vi.devname << " with error " << err;
            DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
            bexit = true;
        }
    } while (!bexit);

    if (vde)
    {
        free(vde);
        vde = NULL;
    }

    CloseHandle(h);
}


void VolumeInfoCollector::FillVolumeGroup(const std::string &diskname, volinfo &vi, DeviceVgInfos &devicevginfos)
{
    VolumeSummary::FormatLabel format = VolumeSummary::LABEL_RAW;
    VolumeSummary::Vendor vgVendor = VolumeSummary::UNKNOWN_VENDOR;
    std::string storageType, vgName, diskId;
    GetDiskAttributes(diskname, storageType, format, vgVendor, vgName, diskId);
	if (m_localConfigurator.shouldReportScsiIdAsDevName() && m_localConfigurator.getScsiId()) {
		DeviceIDInformer ifr;
		diskId = ifr.GetID(diskname);
	}

    ConstDeviceVgInfosIter it = devicevginfos.find(diskId);
    if (it != devicevginfos.end())
    {
        const VgInfo &vginfo = it->second;
        DebugPrintf(SV_LOG_DEBUG, "volume %s is made from disk %s with ID %s, having vg vendor %s, vg name %s\n",
            vi.devname.c_str(), diskname.c_str(), diskId.c_str(),
            StrVendor[vginfo.Vendor], vginfo.Name.c_str());
        vi.volumegroupvendor = vginfo.Vendor;
        vi.volumegroupname = vginfo.Name;
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "volume %s is not made of collected disks.\n", vi.devname.c_str());
    }
}

// on Azure VM the following logic tries fo find the temp disk
// If it is a Gen 1 VM, the temp disk is detected by using the IDE controller logic. See IsTempDisk()
// if it is a Gen 2 VM, any disk on the same controller as the boot disk is considered temp disk
void VolumeInfoCollector::MarkDisksOnBootDiskController(std::vector<volinfo> &volinfos)
{
    static std::string s_resourceDiskReason;

    if (!IsAzureVirtualMachine() && !IsAzureStackVirtualMachine())
        return;

    std::set<SV_ULONG> diskIndices;
    std::string err;
    DWORD errcode;

    if (!GetSystemDiskList(diskIndices, err, errcode))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get system disk err=%s errCode=%d\n", err.c_str(), errcode);
        return;
    }

    if (diskIndices.size() == 0) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get system disk.\n");
        return;
    }

    std::stringstream   ssError;
    if (diskIndices.size() > 1) {
        ssError << "More than one system disks:";
        for (auto diskIndex : diskIndices) {
            ssError << " " << diskIndex;
        }
        DebugPrintf(SV_LOG_ERROR, "%s\n", ssError.str().c_str());
        return;
    }

    std::stringstream       ssSystemDiskName;
    ssSystemDiskName << "\\\\.\\PhysicalDrive" << *(diskIndices.begin());

    int32_t bootdiskscsiport = -1;
    int32_t bootdiskscsibus = -1;
    int32_t bootdiskscsitarget = -1;
    int32_t bootdiskscsilun = -1;
    
    std::string bootdiskinterface;
    std::string bootdiskname;
    uint32_t numbootdisks = 0;

    std::vector<volinfo>::iterator diter(volinfos.begin());

    for ( /* empty */; diter != volinfos.end(); ++diter)
    {
        volinfo &dvolinfo = *diter;
        std::string strbootpart = dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::HAS_BOOTABLE_PARTITION, false);
        bool IsBootDisk = boost::iequals(strbootpart, "true");
        std::string deviceName = dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::DEVICE_NAME, false);

        if ((VolumeSummary::NATIVE == dvolinfo.vendor) &&
            (VolumeSummary::DISK == dvolinfo.voltype) &&
            IsBootDisk &&
            boost::iequals(ssSystemDiskName.str(), deviceName))
        {
            dvolinfo.systemvol = true;

            int32_t scsibus = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_BUS, true).c_str(), NULL, 10);
            int32_t scsiport = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_PORT, true).c_str(), NULL, 10);
            int32_t scsitarget = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_TARGET_ID, true).c_str(), NULL, 10);
            int32_t scsilun = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_LOGICAL_UNIT, true).c_str(), NULL, 10);
            std::string strInterfaceType = dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::INTERFACE_TYPE, true);

            numbootdisks++;

            if (bootdiskname.empty())
            {
                bootdiskname = dvolinfo.devname;
                bootdiskscsiport = scsiport;
                bootdiskscsibus = scsibus;
                bootdiskscsitarget = scsitarget;
                bootdiskscsilun = scsilun;
                bootdiskinterface = strInterfaceType;

                DebugPrintf(SV_LOG_DEBUG, "Found h:c:t:l %d:%d:%d:%d on %s interface for boot disk %s\n",
                    bootdiskscsibus, bootdiskscsiport, bootdiskscsitarget, bootdiskscsilun, bootdiskinterface.c_str(), bootdiskname.c_str());
            }
            else
            {
                if (bootdiskscsiport == scsiport &&
                    bootdiskscsibus == scsibus &&
                    bootdiskscsitarget == scsitarget &&
                    bootdiskscsilun == scsilun &&
                    bootdiskinterface == strInterfaceType)
                {
                    DebugPrintf(SV_LOG_WARNING, "Found same h:c:t:l %d:%d:%d:%d, interface %s for boot disk %s and %s\n",
                        bootdiskscsibus, bootdiskscsiport, bootdiskscsitarget, bootdiskscsilun,
                        bootdiskinterface.c_str(),
                        bootdiskname.c_str(), dvolinfo.devname.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Found different boot disks at h:c:t:l %d:%d:%d:%d on %s interface for boot disk %s and at h:c:t:l %d:%d:%d:%d on %s interface for %s\n",
                        bootdiskscsibus, bootdiskscsiport, bootdiskscsitarget, bootdiskscsilun,
                        bootdiskinterface.c_str(),
                        bootdiskname.c_str(),
                        scsibus, scsiport, scsitarget, scsilun,
                        strInterfaceType.c_str(),
                        dvolinfo.devname.c_str());

                    return;
                }

            }
        }
    }

    if (!numbootdisks)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Found no boot disks\n", FUNCTION_NAME);
        return;
    }

    if (numbootdisks > 1)
        DebugPrintf(SV_LOG_WARNING, "Found %d boot disks\n", numbootdisks);

    bool isUEFIBoot = IsUEFIBoot();

    uint32_t numtempdisks = 0;
    diter = volinfos.begin();
    for ( /* empty */; diter != volinfos.end(); ++diter)
    {
        volinfo &dvolinfo = *diter;
        std::string strbootpart = dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::HAS_BOOTABLE_PARTITION, false);
        bool IsBootDisk = boost::iequals(strbootpart, "true");
        std::string deviceName = dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::DEVICE_NAME, false);

        if ((VolumeSummary::NATIVE == dvolinfo.vendor) &&
            (VolumeSummary::DISK == dvolinfo.voltype) &&
            !dvolinfo.systemvol)
        {
            int32_t scsibus = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_BUS, true).c_str(), NULL, 10);
            int32_t scsiport = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_PORT, true).c_str(), NULL, 10);
            int32_t scsitarget = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_TARGET_ID, true).c_str(), NULL, 10);
            int32_t scsilun = strtol(dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::SCSI_LOGICAL_UNIT, true).c_str(), NULL, 10);
            std::string strInterfaceType = dvolinfo.GetAttribute(dvolinfo.attributes, NsVolumeAttributes::INTERFACE_TYPE, true);

            bool isScsiInterface = boost::iequals(strInterfaceType, "scsi");
            bool isAtaInterface = boost::iequals(strInterfaceType, "ata") || boost::iequals(strInterfaceType, "ide");

            bool isGen2VmTempDisk = isUEFIBoot &&
                isScsiInterface &&
                strInterfaceType == bootdiskinterface &&
                bootdiskscsiport == scsiport &&
                bootdiskscsibus == scsibus &&
                bootdiskscsitarget == scsitarget &&
                bootdiskscsilun != scsilun;

            bool isGen1VmTempDisk = !isUEFIBoot &&
                isAtaInterface &&
                strInterfaceType == bootdiskinterface; // TBD is there a chance that one can be ata and another ide???

            if (isGen2VmTempDisk || isGen1VmTempDisk)
            {
                std::stringstream ssReason;
                if (isGen2VmTempDisk)
                    ssReason << "Found temp disk "
                    << dvolinfo.devname << " "
                    << "at lun " << scsilun << " "
                    << "on same h:c:t "
                    << scsibus << ":"
                    << scsiport << ":"
                    << scsitarget << " "
                    << "as boot disk " << bootdiskname;

                else if (isGen1VmTempDisk)
                    ssReason << "Found temp disk "
                    << dvolinfo.devname << " "
                    << "on " << strInterfaceType << " interface.";

                if (s_resourceDiskReason != ssReason.str())
                {
                    DebugPrintf(SV_LOG_ALWAYS, "Marking resource disk. %s\n", ssReason.str().c_str());
                    s_resourceDiskReason = ssReason.str();
                }

                dvolinfo.attributes.insert(std::make_pair(NsVolumeAttributes::IS_RESOURCE_DISK, STRBOOL(true)));
                numtempdisks++;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Found %d disks on Boot Disk controller\n", numtempdisks);

    return;
}

void VolumeInfoCollector::updateClusteredVolumeGroupInfo(volinfo& vi)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string volumeGroupName = vi.volumegroupname;

    if (volumeGroupName.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Empty volumegroupname for the volume %s\n", FUNCTION_NAME, vi.devname.c_str());
        return;
    }
    std::set<std::string>::const_iterator itr;

    itr = m_clusterVolumeGroups.find(volumeGroupName);
    
    if (itr != m_clusterVolumeGroups.end())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s Marking volume %s as a part of cluster\n", FUNCTION_NAME, vi.devname.c_str());
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::IS_PART_OF_CLUSTER, "true"));
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

/*
int main()
{
vector<volinfo> vinfolist;
VolumeInfoCollector vc;

vc.getvolumeinfo(vinfolist);

printf("\n\n\n **** volumes information **** \n");
vc.display_devlist(vinfolist);
}
*/


