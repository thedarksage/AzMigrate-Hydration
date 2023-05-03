#ifndef INM_DISKINFO_COLLECTOR_H
#define INM_DISKINFO_COLLECTOR_H

#include <winioctl.h>
#include <vector>
#include <set>
#include <string>

#include <ClusApi.h>
#include <VersionHelpers.h>

#include "volumegroupsettings.h"
#include "genericwmi.h"
#include "volinfocollector.h"
#include "deviceidinformer.h"
#include "volumedefines.h"
#include "inmstrcmp.h"


#include "localconfigurator.h"

#define DISCOVERY_IGNORE_DISKS_WITHOUT_SCSI_ATTRIBS             0x00000001

#define TEST_FLAG(a, f)     (((a) & (f)) == (f))
#define SET_FLAG(a, f)      ((a) = ((a) | (f)))

// Sends IOCTL_DISK_GET_DRIVE_GEOMETRY ioctl to disk handle and fills disk_geometry 
extern BOOL GetDriveGeometry(const HANDLE &hDisk, const std::string & diskname, DISK_GEOMETRY & geometry, std::string& errormessage);

// Sends IOCTL_SCSI_GET_ADDRESS ioctl to disk handle and fills SCSI_ADDRESS
extern BOOL GetSCSIAddress(const HANDLE &hDisk, const std::string & diskname, SCSI_ADDRESS & scsiAddress, std::string& errormessage);

// Gets Disk Number
extern BOOL GetDeviceNumber(const HANDLE &hDisk, const std::string & diskname, dev_t& diskIndex, std::string& errormessage);

extern BOOL GetDeviceAttributes(const HANDLE& hDevice,
    const std::string & diskname,
    std::string& vendorId,
    std::string& productId,
    std::string& serialNumber,
    std::string& errormessage);

typedef enum{
    SpDriveCannotPoolReasonUnknown = 0,
    SpDriveCannotPoolReasonOther = 1,
    SpDriveCannotPoolReasonAlreadyPooled = 2,
    SpDriveCannotPoolReasonNotHealthy = 3,
    SpDriveCannotPoolReasonRemovableMedia = 4,
    SpDriveCannotPoolReasonClustered = 5,
    SpDriveCannotPoolReasonOffline = 6,
    SpDriveCannotPoolReasonInsufficientCapacity = 7
}SpDriveCannotReason;

namespace MSFT_PHYSICALDISK_ATTRIB{
    const char  INDEXCOLUMNNAME[] = "deviceId";
    const char  CANPOOLCOLUMNNAME[] = "CanPool";
    const char  CANNOTPOOLREASONCOLUMNNAME[] = "CannotPoolReason";
};

class WmiDiskPartitionRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
    WmiDiskPartitionRecordProcessor(const bool &shouldCollectScsiID, ///< collect scsi id instead of disk signature/GUID of the boot disks.
        const bool &getScsiID                                        ///< is getting scsi id allowed
        )
        : m_ShouldCollectScsiID(shouldCollectScsiID)
    {
        if (shouldCollectScsiID && !getScsiID) {
            // todo: report error invalid combination
            // if reportScsiIDforDevName is true, then getscsiid should also be true
        }

        if (!getScsiID)
            m_DeviceIDInformer.DoNotTryScsiID();
    }

    WmiDiskPartitionRecordProcessor(WmiDiskPartitionRecordProcessor & wmidp)
    {
        m_BootableDiskIDs.insert(wmidp.m_BootableDiskIDs.begin(), wmidp.m_BootableDiskIDs.end());
        m_ErrMsg = wmidp.m_ErrMsg;
        m_ShouldCollectScsiID = wmidp.m_ShouldCollectScsiID;
    }

    WmiDiskPartitionRecordProcessor& operator=(WmiDiskPartitionRecordProcessor& wmidp)
    {
        m_BootableDiskIDs.insert(wmidp.m_BootableDiskIDs.begin(), wmidp.m_BootableDiskIDs.end());
        m_ErrMsg = wmidp.m_ErrMsg;
        m_ShouldCollectScsiID = wmidp.m_ShouldCollectScsiID;
        return *this;
    }

    bool Process(IWbemClassObject *precordobj);
    bool CollectDiskIDAsBootable(const std::string &diskName);
    std::string GetErrMsg(void)
    {
        return m_ErrMsg;
    }
    bool IsDiskBootable(const std::string & diskID);

    std::set<std::string> m_BootableDiskIDs;

private:
    std::string m_ErrMsg;                ///< error message

    bool m_ShouldCollectScsiID;          ///< collect scsi id instead of disk signature/GUID of the boot disks.

    DeviceIDInformer m_DeviceIDInformer; ///< device id informer
};


/* TODO: move into its own file */
class MSCSSharedDisksInformer : public SharedDisksInformer
{
public:
    MSCSSharedDisksInformer(const unsigned &retrytimeinseconds);
    ~MSCSSharedDisksInformer();
    bool LoadInformation(void);
    bool IsShared(const std::string &diskid);
    std::string GetErrorMessage(void);

private:
    enum { RETRY_INTERVAL_IN_SECONDS = 60 };

private:
    bool RetryLoadInformation(const bool &isclussvcauto);
    bool Load(void);
    bool Collect(void);
    bool CollectDiskResource(const std::wstring &resource);
    bool CollectDisk(const std::wstring &resource, std::vector<BYTE> & disks, DWORD listSize);
    void CollectDiskHavingSignature(const DWORD &signature, const std::wstring &resource, std::vector<BYTE> & disks, DWORD listSize);
    void Reset(void);
    void CloseClusterHandle(void);
    bool GetStatusOnRetryMaxout(const bool &isclussvcauto);

private:
    std::set<std::string, InmLessNoCase> m_SharedDiskIds;
    std::string m_ErrorMessage;
    HCLUSTER m_hCluster;
    unsigned m_MaxTries;
};


class WmiDiskDriveRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
    WmiDiskDriveRecordProcessor(
        DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,
        DeviceVgInfos *devicevginfos,
        WmiDiskPartitionRecordProcessor *wmidp,
        bool reportScsiIDforDevName,
        const bool &getscsiid,
        ULONG ulDiscoveryFlags,
        const unsigned &clussvcretrytimeinseconds);
    bool Process(IWbemClassObject *precordobj);
    std::string GetErrMsg(void)
    {
        return m_ErrMsg;
    }

private:
    bool    m_reportScsiIDforDevName;
    ULONG   m_ulDiscoveryFlags;
    // key: device name (guid on source and scsi-id on MT)
    // value: volume information
    DiskNamesToDiskVolInfosMap * m_pdiskNamesToDiskVolInfosMap;

    // container to track device names (ids)

    DeviceVgInfos * m_pDeviceVgInfos;
    std::string m_ErrMsg;
    DeviceIDInformer m_DeviceIDInformer;
    WmiDiskPartitionRecordProcessor * m_wmidp;
    SharedDisksInformer::Ptr m_SharedDiskInformerPtr;
};

class NativeDiskRecordProcessor
{
protected:
    bool                              m_reportScsiIDforDevName;        ///< Report scsi id instead of disk signature/GUID
    ULONG                             m_ulDiscoveryFlags;             ///< Flags for discovery like ignore disks without scsi attribs
    std::string                       m_ErrMsg;                        ///< Error Message used in case of failure
    DeviceVgInfos                     *m_pDeviceVgInfos;               ///< device volume group information
    DeviceIDInformer                  m_DeviceIDInformer;              ///< device id informer
    WmiDiskPartitionRecordProcessor   *m_wmidp;                        ///< processor for Win32_DiskPartition class
    DiskNamesToDiskVolInfosMap        *m_pdiskNamesToDiskVolInfosMap;  ///< key: device name (guid on source and scsi-id on MT),

public:
    NativeDiskRecordProcessor(
        DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,  ///< Map from device name to volume information
        DeviceVgInfos *devicevginfos,                             ///< Device volume group information
        WmiDiskPartitionRecordProcessor *wmidp,                   ///< Processor for win32_diskpartition class
        bool reportScsiIDforDevName,                              ///< Report scsi-id instead of disk signature/GUID
        ULONG ulDiscoveryFlags,                                   ///< Flags for discovery like ignore disks without scsi attribs
        const bool &getscsiid);                                   ///< Flag to fetch scsi-id

    bool ProcessDisk(std::string diskName, volinfo&    vi);
    bool ProcessDisk(std::string diskName);
    bool ProcessDisk(SV_UINT index);
    std::string GetErrMsg(void);                                  ///< Gets error messages in case of failures
};

// Get physical disks information using MSFT_PHYSICALDISK class
class MsftPhysicalDiskRecordProcessor : public GenericWMI::WmiRecordProcessor, NativeDiskRecordProcessor
{
public:
    MsftPhysicalDiskRecordProcessor(
        DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,  ///< Map from device name to volume information
        DeviceVgInfos *devicevginfos,                             ///< Device volume group information
        WmiDiskPartitionRecordProcessor *wmidp,                   ///< Processor for win32_diskpartition class
        bool reportScsiIDforDevName,                              ///< Report scsi-id instead of disk signature/GUID
        ULONG ulDiscoveryFlags,                                   ///< Flags for discovery like ignore disks without scsi attribs
        const bool &getscsiid);                                   ///< Flag to fetch scsi-id

    bool Process(IWbemClassObject *precordobj);                   ///< Processes MSFT_Physicaldisk class records

    std::string GetErrMsg(void)
    {
        return m_ErrMsg;
    }

};

class DriverDiskRecordProcessor : public NativeDiskRecordProcessor
{
public:
    DriverDiskRecordProcessor(
        DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,  ///< Map from device name to volume information
        DeviceVgInfos *devicevginfos,                             ///< Device volume group information
        WmiDiskPartitionRecordProcessor *wmidp,                   ///< Processor for win32_diskpartition class
        bool reportScsiIDforDevName,                              ///< Report scsi-id instead of disk signature/GUID
        ULONG ulDiscoveryFlags,                                   ///< Flags for discovery like ignore disks without scsi attribs
        const bool &getscsiid)                                   ///< Flag to fetch scsi-id

        : NativeDiskRecordProcessor(pdiskNamesToDiskVolInfosMap, devicevginfos, wmidp, reportScsiIDforDevName, ulDiscoveryFlags, getscsiid)
        {}

    bool Enumerate();
    bool Process();

private:
    std::set<ULONG>     m_diskIndexSet;
};

class ConfigDiskProcessor : public NativeDiskRecordProcessor
{
public:
    ConfigDiskProcessor(
        DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,  ///< Map from device name to volume information
        DeviceVgInfos *devicevginfos,                             ///< Device volume group information
        WmiDiskPartitionRecordProcessor *wmidp,                   ///< Processor for win32_diskpartition class
        bool reportScsiIDforDevName,                              ///< Report scsi-id instead of disk signature/GUID
        ULONG ulDiscoveryFlags,                                   ///< Flags for discovery like ignore disks without scsi attribs
        const bool &getscsiid)                                   ///< Flag to fetch scsi-id

        : NativeDiskRecordProcessor(pdiskNamesToDiskVolInfosMap, devicevginfos, wmidp, reportScsiIDforDevName, ulDiscoveryFlags, getscsiid)
    {}

    bool Enumerate();
    bool Process();

private:
    std::set<ULONG>     m_diskIndexSet;
};

class WmiEncryptableVolumeRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
    WmiEncryptableVolumeRecordProcessor(
        BitlockerProtectionStatusMap &bitlockerProtStatusMap,
        BitlockerConversionStatusMap &bitlockerConvStatusMap
    ) : m_bitlockerProtStatusMap(bitlockerProtStatusMap),
        m_bitlockerConvStatusMap(bitlockerConvStatusMap)
    {}

    bool Process(IWbemClassObject *precordobj);

    std::string GetErrMsg(void)
    {
        return m_ErrMsg;
    }

private:
    std::string m_ErrMsg;
    BitlockerProtectionStatusMap &m_bitlockerProtStatusMap;
    BitlockerConversionStatusMap &m_bitlockerConvStatusMap;

    SVERROR GetProtectionStatus(
        IWbemClassObject *encryptableVolObj,
        VolumeSummary::BitlockerProtectionStatus &protectionStatus);

    SVERROR GetConversionStatus(
        IWbemClassObject *encryptableVolObj,
        bool &isVolumeLocked,
        VolumeSummary::BitlockerConversionStatus &conversionStatus,
        UINT32 &encryptionPercentage);
};

#endif /* INM_DISKINFO_COLLECTOR_H */