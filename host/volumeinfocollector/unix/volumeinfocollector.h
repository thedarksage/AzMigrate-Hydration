//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : volumeinfocollector.h
//
// Description: collects volume information for devices that can be used as a source or target
//

#ifndef VOLUMEINFOCOLLECTOR_H
#define VOLUMEINFOCOLLECTOR_H

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <ace/Mutex.h>
#include <ace/Thread_Mutex.h>

#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/phoenix1_primitives.hpp>
#include <boost/spirit/include/phoenix1_functions.hpp>

#include "volumegroupsettings.h"
#include "devicetracker.h"
#include "filesystemtracker.h"
#include "customdevices.h"
#include "systemmountpoints.h"
#include "swapvolumes.h"
#include "volumeinfocollectorinfo.h"
#include "dumpdevice.h"
#include "tempdevice.h"
#include "cachevolume.h"
#include "localconfigurator.h"
#include "logicalvolumes.h"
#include "volumegroups.h"
#include "mountinfos.h"
#include "efiinfos.h"
#include "bootdisk.h"
#include "utilfunctionsmajor.h"
#include "deviceidinformer.h"
#include "writecacheinformer.h"
#include "luncapacityinformer.h"
#include "scsicommandissuer.h"
#include "volumedefines.h"

using namespace BOOST_SPIRIT_CLASSIC_NS;

bool const DEVICE_MOUNTED = true;

class VolumeInfoCollector {
public:

    explicit VolumeInfoCollector(bool dumpToStdout = false);

    explicit VolumeInfoCollector(DeviceNameTypes_t deviceNameTypeToReport, bool dumpToStdout = false);

    void GetVolumeInfos(VolumeSummaries_t & volumeSummaries, VolumeDynamicInfos_t & volumeDynamicInfos, bool dump=false);
  
    void GetOsDiskInfos(VolumeSummaries_t & volumeSummaries, const bool &dump);
 
    /* TODO: remove this later */
    const VolumeInfoCollectorInfos_t& getVolumeInfos() const 
    { 
        return m_VolumeInfos ; 
    }

    void InsertDeviceHCTLPair(DeviceHCTLPair_t devicehctlpair);

protected:
    static off_t const ONE_KB = 1024;
    static off_t const ONE_MB = ONE_KB * ONE_KB;
    static off_t const ONE_GB = ONE_MB * ONE_KB;
    static off_t const ONE_TB = ONE_GB * ONE_KB;

    static std::string const KB_UNITS;
    static std::string const MB_UNITS;
    static std::string const GB_UNITS;
    static std::string const TB_UNITS;

    void Init(bool dumpToStdout);
    void CopyVolumeInfoToVolumeSummary(VolumeInfoCollectorInfo & volumeInfo, VolumeSummary & volumeSummary);
    void CopyVolumeInfoToVolumeDynamicInfo(VolumeInfoCollectorInfo & volumeInfo, VolumeDynamicInfo & volumeDynamicInfo);
    void CopyVolumeInfosToVolumeSummaries(VolumeInfoCollectorInfos_t & volInfos, 
                                          VolumeSummaries_t & volumeSummaries,
                                          VolumeDynamicInfos_t & volumeDynamicInfos);
    void CopyAllVolumeInfosToVolumeSummaries(VolumeInfoCollectorInfos_t & volInfos,
                                             VolumeSummaries_t & volumeSummaries);
    void DumpVolumeInfos() const;
    void DumpVolumeSummaries(VolumeSummaries_t & volumeSummaries) const;
    void DumpVolumeDynamicInfos(VolumeDynamicInfos_t & volumeDynamicInfos) const;
 // void GetVsnapDevices();
    void GetVolPacks();
    void GetVirtualVolumes();
    void InsertVolumeInfo(VolumeInfoCollectorInfo const & volInfo);
    void InsertVolumeInfos(VolumeInfoCollectorInfos_t &volinfos, const VolumeInfoCollectorInfos_t &newvolinfos);
    void InsertVolumeInfoChild(VolumeInfoCollectorInfo &parentVolInfo, VolumeInfoCollectorInfo const & childVolInfo);
    void InsertLv(const Lv_t &lv);
    void InsertLv(Lvs_t &lvs, const Lv_t &lv);
    void InsertVg(const Vg_t &vg);
    void InsertMountInfo(const MountInfo_t &mntinfo);
    void UpdateVolumeManagerInfos(void);
    void GetVsnapInfos(void);
    bool GetLvsInVg(Vg_t &vg);
    bool GetCustomDevices();
    void UpdateLvInsideDevts(VolumeInfoCollectorInfo &volInfo);
    bool IsLvInsideDevtEncrypted(dev_t devt);
    bool GetNextFullDeviceEntry(std::stringstream & results, std::string & field1, std::string & field2, std::string const & diskTok);
    bool GetDiskVolumeInfos(const bool &bneedonlynatives, const bool &bnoexclusions);
    bool GetDiskInfosInDir(const char *pdiskdir);
	bool GetVolumeStartingSector( const char* name, SV_ULONGLONG& startingSector ) ;
    bool GetOSNameSpaceDiskInfos(const bool &bnoexclusions);
    bool GetDmDiskInfos(void);
    bool GetByIdDiskInfos(void);
    bool GetDmDiskInfo(Lv_pair_t &dmdiskpair);
    bool GetVxDMPDiskInfos(void);
    bool GetDmsetupTableInfo(void);
    void FillDmsetupTableInfos(const std::string &tabline, const std::set<std::string> &dmraiddisks);
    bool GetLvmVgInfos(void);
    bool GetLogicalVolumeInfos();
    bool GetMountInfos(void);
    bool GetVxVMVolumesInThisDiskGroup(
                                       const std::string &vgname,
                                       const std::string &diskgroupdir, 
                                       const std::set<dev_t> &vxvmvsets,
                                       const VolumeSummary::Vendor vendor,
                                       Lvs_t &lvs
                                      );
    void UpdateMountDetails(VolumeInfoCollectorInfo &volInfo);
    void UpdateLvToDisk(VolumeInfoCollectorInfos_t &volInfos, VolumeInfoCollectorInfo &volInfo);
    void UpdateSwapDetails(VolumeInfoCollectorInfo &volInfo);
    void CopyMountInfoToVolInfo(const MountInfo_t &mntInfo, VolumeInfoCollectorInfo &volInfo);
    void UpdateAttrsWithChildren(VolumeInfoCollectorInfo &parentVolInfo);
    void CopyAttrsToChildren(VolumeInfoCollectorInfo &parentVolInfo);
    void CopyAttrsToParents(VolumeInfoCollectorInfo &parentVolInfo);
    void UpdateLvFromVgLvInfo(const std::string &vgname, const Lv_t &lvinfo);
    void UpdateVgLvFromLvInfo(const std::string &vgname, Lv_t &lvinfo);
    void GetFreeSpace(const MountInfo_t &mntInfo, VolumeInfoCollectorInfo &volInfo);
    bool IsLogicalVolumeMountedOnRoot(Lv_t &lvinfo);
    void FindLogicalVolumeMountedOnRoot(void);
    bool GetLvs(void);
    bool GetVgLvs(void);
    bool GetVolumeManagerInfos(void);
    bool GetOSVolumeManagerInfos(void);
    bool GetCustomVolumeManagerInfos(void);
    bool GetSvmLvsAndVgs(void);
    bool GetZpools(void);
    void GetSvmLv(const std::string &firsttok, std::stringstream &ssline);
    bool GetVxVMVgs(void);
    bool GetSharedDiskSets(void);
    bool FillMetaset(const std::string &metasetstr);
    bool GetLocalDiskSetLvs(void);
    void GetVgsFromVxdisk(void);
    void GetVgInfoForDevice(
                            const std::set<dev_t> &vxvmvsets,
                            const std::string &line, 
                            Vgs_t &Vgs,
                            const int &grouppos,
                            const int &statuspos
                           );
    off_t CalculateFSFreeSpace(const struct statvfs64 &volStatvfs);
    void FillDiskVolInfos(const DiskPartitions_t &diskandpartition);
    void GetLvsInSharedDiskSet(Vg_t &diskset);
    void GetMetastatInfo(Vg_t &Vg);
    void GetMetastatLineInfo(const std::string &line, Vg_t &Vg);
    bool GetDidDevices(void);

    bool IsExtendedPartition(int partitionType)
        {
            return (0x5 == partitionType || 0xf == partitionType || 0x85 == partitionType);
        }

    bool IsDeviceLocked(std::string const & deviceName);
    bool IsDeviceTracked(int major, std::string const & deviceName);

    template <class MntPntEnt>  bool GetMountVolumeInfos(bool mounted)
        {
            MntPntEnt mpe;

            while (-1 != mpe.Next()) {
                const char *mountedres = mpe.MountedResource();
                if (mountedres)
                {
					DebugPrintf(SV_LOG_DEBUG, "Mounted Resource %s\n", mountedres) ;

                    const char *fs = mpe.FileSystem();
                    if (fs)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "File system %s\n", fs);
                        std::string strfs = fs;
                        /* For linux and solaris, nfs file system shows up as nfs,
                         * but for aix, it shows up as NFS3 */
                        bool isnfs = parse(strfs.begin(), 
                                           strfs.end(), 
                                           lexeme_d
                                           [
                                           as_lower_d["nfs"] >> !uint_p
                                           ] >> end_p,
                                           space_p).full;
                        if (isnfs)
                        {
                            DebugPrintf(SV_LOG_DEBUG, "For device %s, the file system %s is nfs based. Hence not collecting.\n", 
                                                      mountedres, fs);
                            continue;
                        }
                    }

                    MountInfo mntInfo;
                    mntInfo.m_MountedResource = mountedres;
                    mntInfo.m_Source = mpe.GetFileName();

                    struct stat64 devStat;
                    /* NOTE: dev_t can be zero; it is handled while comparing */
                    if (0 == stat64(mntInfo.m_MountedResource.c_str(), &devStat)) {
                        mntInfo.m_Devt = devStat.st_rdev;
                        DebugPrintf(SV_LOG_DEBUG,"dev stat is %d\n",mntInfo.m_Devt);
                        if(mntInfo.m_Devt==0)
                        {
                            if(devStat.st_mode & S_IFDIR)
                            {
                                m_DirMountPoints.push_back(mpe.MountPoint());
                                DebugPrintf(SV_LOG_DEBUG,"pushing %s in directory mount point vector\n",mpe.MountPoint());
                            }
                        }
                    }
	
                    if (0 != mpe.MountPoint()) {
                        mntInfo.m_MountPoint = mpe.MountPoint();
                        mntInfo.m_IsSystemMountPoint = m_SystemMountPoints.IsSystemMountPoint(mpe.MountPoint());
			}
					else
					{
						DebugPrintf(SV_LOG_DEBUG, "Mount point not found\n") ;
					}
                    if (mounted) {
                        mntInfo.m_IsMounted = DEVICE_MOUNTED;

                        if (0 != mpe.MountPoint()) {
                            struct stat64 mntStat;
                            if (-1 != stat64(mpe.MountPoint(), &mntStat)) {
                                mntInfo.m_IsCacheMountPoint = m_CacheVolume.IsCacheVolume(mntStat.st_dev);
                                m_SystemMountPoints.SetBootMountPointIfSo(mpe.MountPoint());
                            }
                        }
                    }
                    if (fs) {
                        mntInfo.m_FileSystem =  fs;
                        // make sure to check swap volume after setting the filesytem in case it is swap filesystem
                        mntInfo.m_IsSwapFileSystem = m_SwapVolumes.IsSwapFS(mntInfo.m_FileSystem);
                    }
                    InsertMountInfo(mntInfo);
                }
            }

            return true;
        }

    int GetVsnapMajorNumber();

    off_t GetVolumeSizeInBytes(std::string const & deviceName, off_t offset, off_t sectorSize = 0);
    off_t GetVolumeSizeInBytes(int fd, std::string const & deviceName);
    bool IsVolInfoPresent(const std::string &devname);
    off_t SizeUnitsToSizeBytes(float size, std::string const & units);
    bool IsDeviceValidLv(const std::string &device);
    unsigned int GetSectorSize(std::string const & deviceName) const;
    unsigned int GetSectorSize(int fd, std::string const & deviceName) const;
    Attributes_t GetAttributes(const std::string &devicename);
    void GetScsiHCTL(const std::string &devicename, std::string &h, std::string &c, std::string &t, std::string &l);
    void CollectScsiHCTL(void);
    void UpdateHCTLFromDevice(const std::string &devicename);
    bool IsEncryptedDisk(std::string const & deviceName);
    bool IsBekVolume(std::string const & deviceName);
    bool IsIscsiDisk(VolumeInfoCollectorInfo &volInfo);
    void MarkDisksOnBootDiskController();
    bool IsNvmeDisk(std::string const & deviceName);

private:
    LocalConfigurator m_localConfigurator;

    VolumeInfoCollectorInfos_t m_VolumeInfos;

    DeviceTracker m_DeviceTracker;

    FileSystemTracker m_FileSystemTracker;

    CustomDevices m_CustomDevices;

    SystemMountPoints m_SystemMountPoints;

    DumpDevice m_DumpDevice;

    TempDevice m_TempDevice;

    SwapVolumes m_SwapVolumes;

    CacheVolume m_CacheVolume;

    Lvs_t m_Lvs;

    Vgs_t m_Vgs;

    MountInfos_t m_MountInfos;
 
    EfiInfo_t m_EfiInfo;

    BootDiskInfo_t m_BootDiskInfo;

    bool m_RegisterSystemDevices;

    bool m_ReportFullDevicesOnly;

    DeviceToHCTL_t m_DeviceToHCTL;

    ScsiCommandIssuer m_ScsiCommandIssuer;

    DeviceIDInformer m_DeviceIDInformer;

    WriteCacheInformer m_WriteCacheInformer;

    LunCapacityInformer m_LunCapacityInformer;
   
    std::set<dev_t> m_VsnapDevts;

    std::vector<std::string> m_DirMountPoints;//this vector holds the directory mount points

    DeviceNameTypes_t m_deviceNameTypeToReport;
};

#endif // ifndef VOLUMEINFOCOLLECTOR_H
