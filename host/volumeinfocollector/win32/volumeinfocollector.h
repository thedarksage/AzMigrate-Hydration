
#ifndef VOLUMEINFOCOLLECTOR_H
#define VOLUMEINFOCOLLECTOR_H

#include <cstdio>
#include <vector>
#include <set>
#include <string>
#include <ace/Singleton.h>
#include <ace/Mutex.h>

#include "volumegroupsettings.h"
#include "volinfocollector.h"
#include "localconfigurator.h"
#include "configwrapper.h"

#define MAX_LOGICAL_DRIVES	26

typedef unsigned int dev_t;

#define DEFAULT_SECTOR_SIZE        (512)


using namespace std;

//typedef struct volinfo_tag
//{
////public:
//    volinfo_tag() : systemvol(false), mounted(false), cachedirvol(false),locked(false),voltype(0),rawsize(0ULL), freespace(0ULL), devno(0), sectorsize(DEFAULT_SECTOR_SIZE){};
//    std::string devname;
//    std::string mountpoint;
//    std::string fstype;
//    dev_t devno;
//    bool mounted;
//    bool systemvol;
//    bool cachedirvol;
//    bool locked;
//    int  voltype;
//    unsigned int  sectorsize;
//    unsigned long long rawsize;
//    unsigned long long freespace;
//    std::string volumelabel;
//} volinfo;


typedef enum deviceNameTypes {
    DEVICE_NAME_TYPE_DISK_SIGNATURE = 4
} DeviceNameTypes_t;

class VolumeInfoCollector
{

public:

    VolumeInfoCollector(){};
    VolumeInfoCollector(DeviceNameTypes_t notUsed): VolumeInfoCollector() {}
    ~VolumeInfoCollector(){};


    void GetVolumeInfos(VolumeSummaries_t& volumeSummaries, VolumeDynamicInfos_t& volumeDynamicInfos);
    void GetVolumeInfos(VolumeSummaries_t& volumeSummaries, VolumeDynamicInfos_t& volumeDynamicInfos, bool dump);
    void GetVolumeInfos(VolumeSummaries_t & volumeSummaries, VolumeDynamicInfos_t & volumeDynamicInfos,  bool dump, bool reportOnlyClusterVolumes);
    void GetOsDiskInfos(VolumeSummaries_t & volumeSummaries, const bool &dump);

    void MarkDisksOnBootDiskController(std::vector<volinfo> &vinfo);

    void display_devlist(std::vector<volinfo> &vinfo)
        {
            // TODO-SanKumar-1806: Can be optimized to exit at the beginning of
            // the method by checking the log level.

            for(unsigned int i = 0; i < vinfo.size(); i++)
            {
                volinfo di = vinfo[i];
    
                DebugPrintf(SV_LOG_DEBUG, "\n---- VOLUMEINFO OF %s ----    \n", di.devname.c_str());
    
                DebugPrintf(SV_LOG_DEBUG, "device           %s \n", di.devname.c_str());
                DebugPrintf(SV_LOG_DEBUG, "mount point      %s \n", di.mountpoint.c_str());
                DebugPrintf(SV_LOG_DEBUG, "fstype           %s \n", di.fstype.c_str());
                DebugPrintf(SV_LOG_DEBUG, "devno            0x%x \n", di.devno);
                DebugPrintf(SV_LOG_DEBUG, "mounted          %s \n", di.mounted ? "[MOUNTED]" : "[UNMOUNTED]");
                DebugPrintf(SV_LOG_DEBUG, "system volume    %s \n", di.systemvol ? "[SYSVOL]" : "[NONSYSVOL]");
                DebugPrintf(SV_LOG_DEBUG, "cache volume     %s \n", di.cachedirvol ? "[CACHEVOL]" : "[NONCACHEVOL]");
                DebugPrintf(SV_LOG_DEBUG, "has page file    %s \n", di.containpagefile ? "[PAGEFILEVOL]" : "[NONPAGEFILEVOL]");
                DebugPrintf(SV_LOG_DEBUG, "locked           %s \n", di.locked ? "[LOCKEDVOL]" : "[NOTLOCKEDVOL]");
                DebugPrintf(SV_LOG_DEBUG, "volume type      %s \n", StrDeviceType[di.voltype]);
                DebugPrintf(SV_LOG_DEBUG, "sectorsize       %u \n", di.sectorsize);
                DebugPrintf(SV_LOG_DEBUG, "raw size		    %I64u\n", di.rawcapacity);
                DebugPrintf(SV_LOG_DEBUG, "capacity         %I64u \n", di.capacity);
                DebugPrintf(SV_LOG_DEBUG, "freespace        %I64u \n", di.freespace);
                DebugPrintf(SV_LOG_DEBUG, "volume label     %s \n", di.volumelabel.c_str());
                DebugPrintf(SV_LOG_DEBUG, "device id        %s \n", di.deviceid.c_str());
                DebugPrintf(SV_LOG_DEBUG, "vendor           %s \n", StrVendor[di.vendor]);
                DebugPrintf(SV_LOG_DEBUG, "write cache      %s \n", StrWCState[di.writecachestate]);
                DebugPrintf(SV_LOG_DEBUG, "format Label     %s \n", StrFormatLabel[di.formatlabel]);
                DebugPrintf(SV_LOG_DEBUG, "vg vendor        %s \n", StrVendor[di.volumegroupvendor]);
                DebugPrintf(SV_LOG_DEBUG, "volume group     %s \n", di.volumegroupname.c_str());
                PrintAttributes(di.attributes);
                //printf("devname = %s  mountpt = %s%s%s fstype = %s  devno = 0x%x \n", di.devname.c_str(), di.mountpoint.c_str(), di.mounted ? "[MOUNTED]" : "", di.systemvol ? "[SYSVOL]" : "", di.fstype.c_str(), di.devno);
            }
        }

        /* to detect drive letters having duplicate mount point */
        struct DriveInfo
        {
           string volguid;
           bool ignore;
        };
        typedef map<int, DriveInfo> DriveInfoMap;
        typedef map<int, DriveInfo>::iterator DriveInfoIterator;

        /* to detect duplicate mount points */
        /* guid of the collected mount points */
        typedef std::set<std::string> MountPoints_t;
        typedef MountPoints_t::iterator MountPointsIter_t;
        typedef MountPoints_t::const_iterator ConstMountPointsIter_t;
private:

    DWORD getsystemdrives();
    std::string getcacheddrives();
    DWORD getlogicaldrives();
    DWORD getvirtualdrives();
    DWORD getvsnapdrives();
    bool issystemdrive(DWORD driveIndex);
    bool iscachedirvol(DWORD driveIndex);
    bool iscachedirvol(std::string const& vol);
    bool ispagefilevol(DWORD driveIndex);
    //bool isdrivelocked(DWORD driveIndex);
    bool isdrivelocked(unsigned int driveIndex, int fsTypeSize = 0, char * fsType = NULL);
    std::string getdrivefstype(unsigned int driveIndex);
    std::string getvollabel(DWORD driveIndex);
    unsigned long long getdrivecapacity(unsigned int driveIndex);
    unsigned long long getdrivefreespace(unsigned int driveIndex);
    std::string getdrivelabel(unsigned int driveIndex);
    std::string getvolname(unsigned int driveIndex);
    unsigned int getdrivesectorsize(unsigned int driveIndex);
    bool addchildvolumes( std::vector<volinfo> &vinfo, const char * volume );
    unsigned long long getdrivecapacity( const char* volume );
    unsigned long long getdrivefreespace( const char* volume );
    unsigned int getdrivesectorsize( const char* volume );
    bool isdrivelocked( const char* volume, int fsTypeSize = 0, char * fsType = NULL );
    VolumeSummary::WriteCacheState getwritecachestate(const char* volume);
    VolumeSummary::WriteCacheState getwritecachestate(unsigned int driveIndex);
    std::string getextendedvolumename( const char* volume );
    std::string getdrivelabel( const char* volume );
    bool canbevirtualvolume(const std::string&);
    bool getVirtualVolumeFromPersist(std::vector<volinfo> & volinfos);
    void GetDiskVolumeInfos(DiskNamesToDiskVolInfosMap & diskNamesToDiskVolInfosMap, DeviceVgInfos & devicevginfos);
    void UpdateVolumeGroup(volinfo &vi, DeviceVgInfos &devicevginfos);
    void FillVolumeGroup(const std::string &diskname, volinfo &vi, DeviceVgInfos &devicevginfos);

    void updateVolInfos( vector<volinfo>& vinfo, HANDLE VVCtrlDevice, const char * szVolume, const char* MountPointBuffer ) ;
    void getEncryptionStatus();
    void updateEncryptionStatus(volinfo &vol);

    void updateClusteredVolumeGroupInfo(volinfo &vol);

    BitlockerProtectionStatusMap bitlockerProtectionStatusMap;
    BitlockerConversionStatusMap bitlockerConversionStatusMap;
    DWORD systemDrives;
    DWORD logicalDrives;
    DWORD pagefileDrives;
    DWORD nonreportableDrives;
    std::string cacheVolume;
    DWORD virtualDrives;
    DWORD vsnapDrives;
    LocalConfigurator m_localConfigurator;
    
    std::set<std::string> m_clusterVolumeGroups;

};

#endif // ifndef VOLUMEINFOCOLLECTOR_H
