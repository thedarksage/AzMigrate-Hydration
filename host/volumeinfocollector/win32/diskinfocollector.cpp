#include <string>
#include <sstream>
#include <iomanip>

#include "portablehelpersmajor.h"

#include <atlbase.h>
#include <atlsafe.h>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include "diskinfocollector.h"

#include "dlmapi.h"
#include "logger.h"
#include "portable.h"
#include "service.h"

#include "scopeguard.h"

#include "InmFltInterface.h"
#include "InmFltIOCTL.h"

#include "DiskHelpers.h"

#define MAX_CHARS_IN_RES 256
#define GET_DISK_INFO_EXPECTED_SIZE 1024
#define CREATE_PAIR(x) std::make_pair(x, #x)

static std::map<MEDIA_TYPE, std::string>  s_MediaType = { 
    { CREATE_PAIR(Unknown) },
    { CREATE_PAIR(F5_1Pt2_512) },
    { CREATE_PAIR(F3_1Pt44_512) },
    { CREATE_PAIR(F3_2Pt88_512) },
    { CREATE_PAIR(F3_20Pt8_512) },
    { CREATE_PAIR(F3_720_512) },
    { CREATE_PAIR(F5_360_512) },
    { CREATE_PAIR(F5_320_512) },
    { CREATE_PAIR(F5_320_1024) },
    { CREATE_PAIR(F5_180_512) },
    { CREATE_PAIR(F5_160_512) },
    { CREATE_PAIR(RemovableMedia) },
    { CREATE_PAIR(FixedMedia) },
    { CREATE_PAIR(F3_120M_512) },
    { CREATE_PAIR(F3_640_512) },
    { CREATE_PAIR(F5_640_512) },
    { CREATE_PAIR(F5_720_512) },
    { CREATE_PAIR(F3_1Pt2_512) },
    { CREATE_PAIR(F3_1Pt23_1024) },
    { CREATE_PAIR(F5_1Pt23_1024) },
    { CREATE_PAIR(F3_128Mb_512) },
    { CREATE_PAIR(F3_230Mb_512) },
    { CREATE_PAIR(F8_256_128) },
    { CREATE_PAIR(F3_200Mb_512) },
    { CREATE_PAIR(F3_240M_512) },
    { CREATE_PAIR(F3_32M_512) }
};

static std::map<STORAGE_BUS_TYPE, std::string> s_InterfaceType = {
    { BusTypeUnknown, "UNKNOWN" },
    { BusTypeScsi, "SCSI" },
    { BusTypeAtapi, "ATAPI" },
    { BusTypeAta, "ATA" },
    { BusType1394, "1394" },
    { BusTypeSsa, "SSA" },
    { BusTypeFibre, "FIBRE" },
    { BusTypeUsb, "USB" },
    { BusTypeRAID, "RAID" },
    { BusTypeiScsi, "ISCSI" },
    { BusTypeSas, "SCSI" },
    { BusTypeSata, "SATA" },
    { BusTypeSd, "SD" },
    { BusTypeMmc, "MMC" },
    { BusTypeVirtual, "VIRTUAL" },
    { BusTypeFileBackedVirtual, "FILEBACKEDVIRTUAL" },
    { BusTypeSpaces, "SPACES" },
    { BusTypeNvme, "NVME" },
    { BusTypeMax, "MAX" },
    { BusTypeMaxReserved, "RESERVED" }
};

bool IsBusTypeUnsupported(STORAGE_BUS_TYPE busType)
{
    return  (IsWindows8OrGreater() && (BusTypeSpaces == busType));
}

WmiDiskDriveRecordProcessor::WmiDiskDriveRecordProcessor(
    DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,
    DeviceVgInfos *devicevginfos,
    WmiDiskPartitionRecordProcessor *wmidp,
    bool reportScsiIDforDevName,
    const bool &getscsiid,
    ULONG   ulDiscoveryFlags,
    const unsigned &clussvcretrytimeinseconds)
    : m_reportScsiIDforDevName(reportScsiIDforDevName),
    m_pdiskNamesToDiskVolInfosMap(pdiskNamesToDiskVolInfosMap),
    m_pDeviceVgInfos(devicevginfos),
    m_ulDiscoveryFlags(ulDiscoveryFlags),
    m_wmidp(wmidp)
{

    if (reportScsiIDforDevName && !getscsiid) {
        // todo: report error invalid combination
        // if reportScsiIDforDevName is true, then getscsiid should also be true
    }

    if (!getscsiid)
        m_DeviceIDInformer.DoNotTryScsiID();
    m_SharedDiskInformerPtr.reset(new MSCSSharedDisksInformer(clussvcretrytimeinseconds));
    if (!m_SharedDiskInformerPtr->LoadInformation()) {
        std::string errmsg = "Failed to recognize if the disks are shared with error " + m_SharedDiskInformerPtr->GetErrorMessage() +
            ". Protection will not be correct"
            ". Fix the cluster service for correct protection.";
        /* Due to circular library dependencies, not calling SendAlertToCx for now.
        * This needs to be called ultimately */
        /* SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_CLUSTER, errmsg.c_str()); */
        DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.c_str());
    }
}


bool WmiDiskDriveRecordProcessor::Process(IWbemClassObject *precordobj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);
    std::stringstream sserr;

    if (0 == precordobj)
    {
        m_ErrMsg = "Record object is NULL";
        return false;
    }

    bool bprocessed = true;
    const char NAMECOLUMNNAME[] = "Name";
    const char IFTYPECOLUMNNAME[] = "InterfaceType";
    const char SCSIBUSCOLUMNNAME[] = "SCSIBus";
    const char SCSILUCOLUMNNAME[] = "SCSILogicalUnit";
    const char SCSIPORTCOLUMNNAME[] = "SCSIPort";
    const char SCSITARGETIDCOLUMNNAME[] = "SCSITargetId";
    const char TOTALCYLINDERSCOLUMNNAME[] = "TotalCylinders";
    const char TOTALHEADSCOLUMNNAME[] = "TotalHeads";
    const char TOTALSECTORSCOLUMNNAME[] = "TotalSectors";
    const char SECTORSPERTRACKCOLUMNNAME[] = "SectorsPerTrack";
    const char INDEXCOLUMNNAME[] = "Index";
    const char MANUFACTURERCOLUMNNAME[] = "Manufacturer";
    const char MODELCOLUMNNAME[] = "Model";
    const char MEDIATYPECOLUMNNAME[] = "MediaType";
    const char SERIALNUMBERCOLUMNNAME[] = "SerialNumber";

    SV_UINT     diskIndex;
    std::string diskname;
    USES_CONVERSION;
    VARIANT vtProp;
    HRESULT hrCol;

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(NAMECOLUMNNAME), 0, &vtProp, 0, 0);
    if (FAILED(hrCol) || (VT_BSTR != V_VT(&vtProp)))
    {
        m_ErrMsg = "Could not find column: ";
        m_ErrMsg += NAMECOLUMNNAME;
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }

    diskname = W2A(V_BSTR(&vtProp));
    VariantInit(&vtProp);

    hrCol = precordobj->Get(A2W(INDEXCOLUMNNAME), 0, &vtProp, 0, 0);
    if (FAILED(hrCol) || (VT_I4 != V_VT(&vtProp)))
    {
        sserr << "Failed to get column " << INDEXCOLUMNNAME << " for diskname " << diskname;
        m_ErrMsg = sserr.str();
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }

    diskIndex = V_I4(&vtProp);
    VariantClear(&vtProp);

    HANDLE h = SVCreateFile(diskname.c_str(),
        GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == h)
    {
        std::stringstream sserr;
        sserr << "Failed to open disk " << diskname << " error: " << GetLastError();
        m_ErrMsg = sserr.str();
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }
    ON_BLOCK_EXIT(&CloseHandle, h);

    STORAGE_BUS_TYPE    busType;
    if (!GetBusType(h, busType, m_ErrMsg)) {
        DebugPrintf(SV_LOG_ERROR, "Disk %s err=%s\n", diskname.c_str(), m_ErrMsg.c_str());
        return false;
    }

    DebugPrintf(SV_LOG_INFO, "Disk %s BusType: %d\n", diskname.c_str(), busType);

    if (IsBusTypeUnsupported(busType)) {
        DebugPrintf(SV_LOG_INFO, "Skipping disk %s as it is %s disk\n", diskname.c_str(), s_InterfaceType[busType].c_str());
        return true;
    }

    volinfo vi;

    std::string storagetype;
    std::string diskGuid;

    vi.devno = diskIndex;

    if (s_InterfaceType.end() ==  s_InterfaceType.find(busType)) {
        busType = BusTypeUnknown;
    }

    vi.attributes.insert(std::make_pair(NsVolumeAttributes::INTERFACE_TYPE, s_InterfaceType[busType]));
    DebugPrintf(SV_LOG_DEBUG, "%s: %s\n", NsVolumeAttributes::INTERFACE_TYPE, s_InterfaceType[busType].c_str());

    GetDiskAttributes(diskname, storagetype, vi.formatlabel, vi.volumegroupvendor, vi.volumegroupname, diskGuid);
    vi.deviceid = m_DeviceIDInformer.GetID(diskname);
    DebugPrintf(SV_LOG_DEBUG, "Device ID: %s\n", vi.deviceid.c_str());

    if (!vi.deviceid.empty())
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_UUID, vi.deviceid));
    }

    vi.devname = (m_reportScsiIDforDevName) ? vi.deviceid : diskGuid;

    if (vi.devname.empty())
    {
        m_ErrMsg = "For disk device name: ";
        m_ErrMsg += diskname;

        m_ErrMsg += (m_reportScsiIDforDevName) ?
            " could not find SCSI Id. Hence not reporting this disk." :
            " could not find MBR signature/GPT GUID. Hence not reporting this disk.";

        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "Device Name: %s\n", vi.devname.c_str());

    DiskNamesToDiskVolInfosMap::const_iterator it = m_pdiskNamesToDiskVolInfosMap->find(vi.devname);
    if (it != m_pdiskNamesToDiskVolInfosMap->end())
    {
        if (it->second.devno == vi.devno) {
            DebugPrintf(SV_LOG_INFO, "Disk number %d Disk ID: %s iterated twice. Skipping\n", it->second.devno, it->first.c_str());
            return true;
        }

        sserr << "For disk device name: " << diskname <<
            " found multiple disks with same id: " << vi.devname <<
            " with different disk numbers " << vi.devno << "," <<
            it->second.devno <<
            ". Hence not reporting any disk matching the id.";

        m_ErrMsg = sserr.str();
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());

        // we do not want to report this disk
        // removing entry inserted in earlier processing 
        // as well
        m_pdiskNamesToDiskVolInfosMap->erase(vi.devname);
        return false;
    }

    if (!storagetype.empty()) {
        DebugPrintf(SV_LOG_DEBUG, "Storage Type: %s\n", storagetype.c_str());
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::STORAGE_TYPE, storagetype));
    }

    DebugPrintf(SV_LOG_DEBUG, "DeviceName: %s\n", diskname.c_str());
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::DEVICE_NAME, diskname));

    if (m_wmidp->IsDiskBootable(vi.devname)) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::HAS_BOOTABLE_PARTITION, STRBOOL(true)));
        DebugPrintf(SV_LOG_DEBUG, "DeviceName: %s is bootable\n", diskname.c_str());
    }

    if (HasUefi(vi.devno)) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::HAS_UEFI, STRBOOL(true)));
        DebugPrintf(SV_LOG_DEBUG, "DeviceName: %s has uefi partition\n", diskname.c_str());
    }

    if (!diskGuid.empty()) {
        if (m_SharedDiskInformerPtr->IsShared(diskGuid)) {
            vi.attributes.insert(std::make_pair(NsVolumeAttributes::IS_SHARED, STRBOOL(true)));
            DebugPrintf(SV_LOG_DEBUG, "DeviceName: %s is shared disk\n", diskname.c_str());
        }
    }

    bool bIgnoreDisksWithoutScsiAttributes = TEST_FLAG(m_ulDiscoveryFlags, DISCOVERY_IGNORE_DISKS_WITHOUT_SCSI_ATTRIBS);

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(SCSIBUSCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_I4 == V_VT(&vtProp)))
    {
        std::stringstream ss;
        ss << V_I4(&vtProp);
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_BUS, ss.str()));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::SCSI_BUS, ss.str().c_str());
        VariantClear(&vtProp);
    }
    else {
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", SCSIBUSCOLUMNNAME, diskname.c_str());
        if (bIgnoreDisksWithoutScsiAttributes) {
            DebugPrintf(SV_LOG_INFO, "Skipping disk %s for A2A Disk enumeration\n", diskname.c_str());
            return false;
        }
    }

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(SCSILUCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_I4 == V_VT(&vtProp)))
    {
        std::stringstream ss;
        ss << V_I4(&vtProp);
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_LOGICAL_UNIT, ss.str()));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::SCSI_LOGICAL_UNIT, ss.str().c_str());
        VariantClear(&vtProp);
    }
    else{
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", SCSILUCOLUMNNAME, diskname.c_str());
        if (bIgnoreDisksWithoutScsiAttributes) {
            DebugPrintf(SV_LOG_INFO, "Skipping disk %s for A2A Disk enumeration\n", diskname.c_str());
            return false;
        }
    }

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(SCSIPORTCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_I4 == V_VT(&vtProp)))
    {
        std::stringstream ss;
        ss << V_I4(&vtProp);
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_PORT, ss.str()));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::SCSI_PORT, ss.str().c_str());
        VariantClear(&vtProp);
    }
    else{
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", SCSIPORTCOLUMNNAME, diskname.c_str());
        if (bIgnoreDisksWithoutScsiAttributes) {
            DebugPrintf(SV_LOG_INFO, "Skipping disk %s for A2A Disk enumeration\n", diskname.c_str());
            return false;
        }
    }

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(SCSITARGETIDCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_I4 == V_VT(&vtProp)))
    {
        std::stringstream ss;
        ss << V_I4(&vtProp);
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_TARGET_ID, ss.str()));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::SCSI_TARGET_ID, ss.str().c_str());
        VariantClear(&vtProp);
    }
    else{
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", SCSITARGETIDCOLUMNNAME, diskname.c_str());
        if (bIgnoreDisksWithoutScsiAttributes) {
            DebugPrintf(SV_LOG_INFO, "Skipping disk %s for A2A Disk enumeration\n", diskname.c_str());
            return false;
        }
    }

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(TOTALCYLINDERSCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_BSTR == V_VT(&vtProp)))
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::TOTAL_CYLINDERS, W2A(V_BSTR(&vtProp))));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::TOTAL_CYLINDERS, W2A(V_BSTR(&vtProp)));
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", TOTALCYLINDERSCOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(TOTALHEADSCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_I4 == V_VT(&vtProp)))
    {
        std::stringstream ss;
        ss << V_I4(&vtProp);
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::TOTAL_HEADS, ss.str()));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::TOTAL_HEADS, ss.str().c_str());
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", TOTALHEADSCOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(TOTALSECTORSCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_BSTR == V_VT(&vtProp)))
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::TOTAL_SECTORS, W2A(V_BSTR(&vtProp))));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::TOTAL_SECTORS, W2A(V_BSTR(&vtProp)));
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", TOTALSECTORSCOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(SECTORSPERTRACKCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_I4 == V_VT(&vtProp)))
    {
        std::stringstream ss;
        ss << V_I4(&vtProp);
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SECTORSPERTRACK, ss.str()));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::SECTORSPERTRACK, ss.str().c_str());
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", SECTORSPERTRACKCOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(MANUFACTURERCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_BSTR == V_VT(&vtProp)))
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::MANUFACTURER, W2A(V_BSTR(&vtProp))));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::MANUFACTURER, W2A(V_BSTR(&vtProp)));
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", MANUFACTURERCOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(MODELCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_BSTR == V_VT(&vtProp)))
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::MODEL, W2A(V_BSTR(&vtProp))));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::MODEL, W2A(V_BSTR(&vtProp)));
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", MODELCOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp); // Get MediaType
    hrCol = precordobj->Get(A2W(MEDIATYPECOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_BSTR == V_VT(&vtProp)))
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::MEDIA_TYPE, W2A(V_BSTR(&vtProp))));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::MEDIA_TYPE, W2A(V_BSTR(&vtProp)));
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", MEDIATYPECOLUMNNAME, diskname.c_str());

    VariantInit(&vtProp); // Get SerialNumber
    hrCol = precordobj->Get(A2W(SERIALNUMBERCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol) && (VT_BSTR == V_VT(&vtProp)))
    {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SERIAL_NUMBER, W2A(V_BSTR(&vtProp))));
        DebugPrintf(SV_LOG_DEBUG, "%s : %s\n", NsVolumeAttributes::SERIAL_NUMBER, W2A(V_BSTR(&vtProp)));
        VariantClear(&vtProp);
    }
    else
        DebugPrintf(SV_LOG_INFO, "Could not find column %s in wmi disk drive class, for disk device name %s\n", SERIALNUMBERCOLUMNNAME, diskname.c_str());

    vi.attributes.insert(std::make_pair(NsVolumeAttributes::NAME_BASED_ON, m_reportScsiIDforDevName ?
        NsVolumeAttributes::SCSIID :
        NsVolumeAttributes::SIGNATURE));

    DebugPrintf(SV_LOG_DEBUG, "Name Based On: %s\n", m_reportScsiIDforDevName ?
                                                        NsVolumeAttributes::SCSIID :
                                                        NsVolumeAttributes::SIGNATURE);

    /* TODO:
    1. get write cache state
    2. what should be mount point ?
    3. put if system, cache volume based on attributes from volumes residing on it
    4. sector size
    */
    vi.mounted = true;
    vi.voltype = VolumeSummary::DISK;
    vi.vendor = VolumeSummary::NATIVE;

    std::string errMsg;
    vi.capacity = GetDiskSize(h, errMsg);
    if (vi.capacity) {
        vi.rawcapacity = vi.capacity;
        m_pDeviceVgInfos->insert(std::make_pair(vi.devname, VgInfo(vi.volumegroupvendor, vi.volumegroupname)));
    }
    else {
        m_ErrMsg = "Failed to find size of disk with error: ";
        m_ErrMsg += errMsg;
        m_ErrMsg += ". Not collecting this disk.";
        bprocessed = false;
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
    }

    if (bprocessed) {
        m_pdiskNamesToDiskVolInfosMap->insert(std::make_pair(vi.devname, vi));
        DebugPrintf(SV_LOG_INFO, "Added Disk %s name: %s scsi id: %s bustype: %d\n", diskname.c_str(), vi.devname.c_str(), vi.deviceid.c_str(), busType);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
    return bprocessed;
}

bool WmiDiskPartitionRecordProcessor::Process(IWbemClassObject *precordobj)
{
    if (0 == precordobj)
    {
        m_ErrMsg = "Record object is NULL";
        return false;
    }

    bool bprocessed = true;
    const char INDEXCOLUMNNAME[] = "DiskIndex";
    const char BOOTABLECOLUMNNAME[] = "Bootable";

    USES_CONVERSION;
    VARIANT vtProp;
    HRESULT hrCol;
    SV_UINT diskIndex;

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(INDEXCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
        if (VT_I4 == V_VT(&vtProp))
        {
            diskIndex = V_I4(&vtProp);
        }
        VariantClear(&vtProp);
    }
    else
    {
        bprocessed = false;
        m_ErrMsg = "Could not find column: ";
        m_ErrMsg += INDEXCOLUMNNAME;
        return bprocessed;
    }

    std::stringstream ssIndex;
    ssIndex << diskIndex;
    std::string diskName = DISKNAMEPREFIX;
    diskName += ssIndex.str();
    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(BOOTABLECOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
        if (VT_BOOL == V_VT(&vtProp))
        {
            if (V_BOOL(&vtProp))
            {
                bprocessed = CollectDiskIDAsBootable(diskName);
            }
        }
        VariantClear(&vtProp);
    }
    else
    {
        bprocessed = false;
        m_ErrMsg = "Could not find column: ";
        m_ErrMsg += BOOTABLECOLUMNNAME;
        m_ErrMsg += ", for disk device name ";
        m_ErrMsg += diskName;
    }

    return bprocessed;
}


bool WmiDiskPartitionRecordProcessor::CollectDiskIDAsBootable(const std::string &diskName)
{
    bool iscollected = true;

    VolumeSummary::FormatLabel format = VolumeSummary::LABEL_RAW;
    VolumeSummary::Vendor vgVendor = VolumeSummary::UNKNOWN_VENDOR;
    std::string storageType, vgName, diskId;

    if (m_ShouldCollectScsiID)
        diskId = m_DeviceIDInformer.GetID(diskName);
    else
        GetDiskAttributes(diskName, storageType, format, vgVendor, vgName, diskId);

    if (!diskId.empty())
        m_BootableDiskIDs.insert(diskId);
    else {
        iscollected = false;
        m_ErrMsg = "Failed to get id for disk ";
        m_ErrMsg += diskName;
    }

    return iscollected;
}


bool WmiDiskPartitionRecordProcessor::IsDiskBootable(const std::string & diskID)
{
    return (m_BootableDiskIDs.end() != m_BootableDiskIDs.find(diskID));
}


// MSCSSharedDisksInformer
MSCSSharedDisksInformer::MSCSSharedDisksInformer(const unsigned &retrytimeinseconds)
    : m_hCluster(0)
{
    m_MaxTries = (retrytimeinseconds / RETRY_INTERVAL_IN_SECONDS) + 1;
}


MSCSSharedDisksInformer::~MSCSSharedDisksInformer()
{
    CloseClusterHandle();
}


bool MSCSSharedDisksInformer::LoadInformation(void)
{
    bool isloaded = true;
    InmService is("ClusSvc");
    is.m_svcStartType = INM_SVCTYPE_NONE;
    SVERROR sve = QuerySvcConfig(is);
    if (sve.succeeded()) {
        std::stringstream ss;
        ss << "starttype = " << is.m_svcStartType << '(' << is.typeAsStr() << ')';
        DebugPrintf(SV_LOG_DEBUG, "%s\n", ss.str().c_str());
        bool isauto = (INM_SVCTYPE_AUTO == is.m_svcStartType);
        if (isauto || (INM_SVCTYPE_MANUAL == is.m_svcStartType))
            isloaded = RetryLoadInformation(isauto);
    }
    else {
        isloaded = false;
        m_ErrorMessage = "Failed to find configuration of cluster service to gather mscs shared disks with error " + std::string(sve.toString());
    }
    return isloaded;
}


bool MSCSSharedDisksInformer::IsShared(const std::string &diskid)
{
    return (m_SharedDiskIds.end() != m_SharedDiskIds.find(diskid));
}


std::string MSCSSharedDisksInformer::GetErrorMessage(void)
{
    return m_ErrorMessage;
}


bool MSCSSharedDisksInformer::RetryLoadInformation(const bool &isclussvcauto)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    unsigned nTries = 0;
    bool isloaded = false;
    unsigned maxtries = isclussvcauto ? m_MaxTries : 1;
    /* Retry is from start (opening cluster handle) because handle can become stale */
    do {
        Reset();
        InmServiceStatus st = INM_SVCSTAT_NONE;
        SVERROR sve = getServiceStatus("ClusSvc", st);
        if (sve.succeeded()) {
            InmService is(st);
            std::string strst = " : Cluster service state is " + is.statusAsStr();
            DebugPrintf(SV_LOG_DEBUG, "%s\n", strst.c_str());
            if (INM_SVCSTAT_RUNNING == st) {
                isloaded = Load();
                if (isloaded)
                    break;
            }
            m_ErrorMessage += strst;
            nTries++;
            if (nTries < maxtries) {
                DebugPrintf(SV_LOG_DEBUG, "Shared disks collection failed with error %s. "
                    "This may be due to cluster service in starting state. "
                    "Waiting for %u seconds before retry (Max tries: %u).\n",
                    m_ErrorMessage.c_str(), RETRY_INTERVAL_IN_SECONDS, maxtries);
                Sleep(RETRY_INTERVAL_IN_SECONDS * 1000);
            }
            else {
                isloaded = GetStatusOnRetryMaxout(isclussvcauto);
                break;
            }
        }
        else {
            m_ErrorMessage = "Failed to find status cluster service to gather mscs shared disks with error " + std::string(sve.toString());
            break;
        }
    } while (true);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isloaded;
}


bool MSCSSharedDisksInformer::Load(void)
{
    bool isloaded = false;
    m_hCluster = OpenCluster(0);
    if (m_hCluster) {
        DebugPrintf(SV_LOG_DEBUG, "Cluster service handle opened successfully.\n");
        isloaded = Collect();
        CloseClusterHandle();
    }
    else {
        DWORD err = GetLastError();
        std::stringstream emsg;
        emsg << "FAILED to open mscs cluster handle, in cluster info provider with error: " << err;
        m_ErrorMessage = emsg.str();
    }
    return isloaded;
}


bool MSCSSharedDisksInformer::Collect(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool arecollected = true;
    DWORD enumType = CLUSTER_ENUM_RESOURCE;
    HCLUSENUM hClusterEnum = ClusterOpenEnum(m_hCluster, enumType);
    if (hClusterEnum) {
        boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<DWORD>(), &ClusterCloseEnum, hClusterEnum));
        WCHAR resName[MAX_CHARS_IN_RES];
        DWORD resNameLength;
        DWORD status;
        DWORD index = 0;
        do {
            resName[0] = L'\0';
            resNameLength = sizeof(resName) / sizeof(WCHAR);
            status = ClusterEnum(hClusterEnum, index, &enumType, resName, &resNameLength);
            if (ERROR_SUCCESS != status) {
                if (ERROR_NO_MORE_ITEMS != status) {
                    std::stringstream emsg;
                    emsg << "Failed to enumerate resources in mscs with error code " << status << ", from api ClusterEnum, having index " << index;
                    m_ErrorMessage = emsg.str();
                    /* TODO: fail even if one resource is not enumerated ?
                    * For now yes, as we do not know what resource we did not get */
                    arecollected = false;
                }
                break;
            }
            if (L'\0' != resName[0]) {
                arecollected = CollectDiskResource(resName);
                if (!arecollected)
                    break;
            }
            ++index;
        } while (ERROR_SUCCESS == status);
    }
    else {
        DWORD err = GetLastError();
        std::stringstream sserr;
        sserr << err;
        m_ErrorMessage = std::string("Failed to enumerate resources in mscs with error ") + sserr.str() + " from api ClusterOpenEnum";
        arecollected = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return arecollected;
}


bool MSCSSharedDisksInformer::CollectDiskResource(const std::wstring &resource)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with resource name \"%S\"\n", FUNCTION_NAME, resource.c_str());

    bool iscollected = true;
    HRESOURCE hResource = OpenClusterResource(m_hCluster, resource.c_str());
    if (hResource) {
        boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseClusterResource, hResource));
        DWORD bytesReturned = GET_DISK_INFO_EXPECTED_SIZE;
        std::vector<BYTE> data(bytesReturned);
        DWORD status;
        do {
            data.resize(bytesReturned);
            /* TODO: check if anything has to be added for veritas volumes ? */
            status = ClusterResourceControl(hResource, NULL, CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO, NULL, 0, &data[0], static_cast<DWORD>(data.size()), &bytesReturned);
        } while (ERROR_MORE_DATA == status);
        if (ERROR_SUCCESS != status) {
            std::stringstream emsg;
            emsg << "ClusterResourceControl CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO failed with " << status << " for resource " << std::string(resource.begin(), resource.end());
            /* ERROR_INVALID_FUNCTION is returned for resources that are non physical disk resources like ip address, name etc */
            if (ERROR_INVALID_FUNCTION != status) {
                m_ErrorMessage = emsg.str();
                iscollected = false;
            }
            else
                DebugPrintf(SV_LOG_DEBUG, "%s. (OK for non disk resource)\n", emsg.str().c_str(), resource.c_str());
        }
        else if (bytesReturned) {
            iscollected = CollectDisk(resource, data, bytesReturned);
        }
        else
            DebugPrintf(SV_LOG_DEBUG, "CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO gave success for resource \"%S\", but with no disks\n", resource.c_str());
    }
    else {
        DWORD err = GetLastError();
        std::stringstream emsg;
        emsg << "OpenClusterResource failed with " << err << " for resource " << std::string(resource.begin(), resource.end());
        m_ErrorMessage = emsg.str();
        iscollected = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with resource name \"%S\"\n", FUNCTION_NAME, resource.c_str());
    return iscollected;
}


bool MSCSSharedDisksInformer::CollectDisk(const std::wstring &resource, std::vector<BYTE> & disks, DWORD listSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool iscollected = true;
    CLUSPROP_BUFFER_HELPER valueList;
    valueList.pb = reinterpret_cast<BYTE*>(&disks[0]);
    std::stringstream sssyntax;
    sssyntax << "first value list syntax = " << std::hex << valueList.pSyntax->dw;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", sssyntax.str().c_str());
    if (CLUSPROP_SYNTAX_DISK_SIGNATURE == valueList.pSyntax->dw) {
        PCLUSPROP_DISK_SIGNATURE pds = reinterpret_cast<PCLUSPROP_DISK_SIGNATURE>(valueList.pb);
        DWORD signature = pds->dw;
        std::stringstream sssignature;
        sssignature << signature;
        DebugPrintf(SV_LOG_DEBUG, "numeric mscs signature of shared disk = %s\n", sssignature.str().c_str());
        /* W2K3 MSCS supports GPT disks by giving its own MSCS signature which is MBR like.
        * Hence collect the GPT guid for GPT disks instead of signature */
        CollectDiskHavingSignature(signature, resource, disks, listSize);
    }
    else if (CLUSPROP_SYNTAX_DISK_GUID == valueList.pSyntax->dw) {
        PCLUSPROP_SZ psz = reinterpret_cast<PCLUSPROP_SZ>(valueList.pb);
        std::wstring wstrguid = psz->sz;
        DebugPrintf(SV_LOG_DEBUG, "GUID of shared disk = %S\n", wstrguid.c_str());
        std::string strguid(wstrguid.begin(), wstrguid.end());
        DebugPrintf(SV_LOG_DEBUG, "Disk with id %s is a shared disk\n", strguid.c_str());
        m_SharedDiskIds.insert(strguid);
    }
    else {
        m_ErrorMessage = std::string("For resource ") + std::string(resource.begin(), resource.end()) +
            "CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO gave neither mbr signature nor gpt GUID. " +
            sssyntax.str();
        iscollected = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return iscollected;
}


void MSCSSharedDisksInformer::CollectDiskHavingSignature(const DWORD &signature, const std::wstring &resource, std::vector<BYTE> & disks, DWORD listSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    CLUSPROP_BUFFER_HELPER valueList;
    DWORD position = 0;
    std::string id = ConvertDWordToString(signature);

    while (true) {
        valueList.pb = reinterpret_cast<BYTE*>(&disks[position]);
        std::stringstream sssyntax;
        sssyntax << "value list syntax = " << std::hex << valueList.pSyntax->dw;
        DebugPrintf(SV_LOG_DEBUG, "%s\n", sssyntax.str().c_str());
        if (CLUSPROP_SYNTAX_ENDMARK == valueList.pSyntax->dw)
            break;
        if (CLUSPROP_SYNTAX_DISK_NUMBER == valueList.pSyntax->dw) {
            PCLUSPROP_DISK_NUMBER pdn = reinterpret_cast<PCLUSPROP_DISK_NUMBER>(valueList.pb);
            DWORD number = pdn->dw;
            std::stringstream ssnumber;
            ssnumber << number;
            DebugPrintf(SV_LOG_DEBUG, "disk number of shared disk = %s\n", ssnumber.str().c_str());
            std::string diskname = DISKNAMEPREFIX;
            diskname += ssnumber.str();
            VolumeSummary::FormatLabel format = VolumeSummary::LABEL_RAW;
            VolumeSummary::Vendor vgvendor = VolumeSummary::UNKNOWN_VENDOR;
            std::string storagetype, vgname, diskGuid;
            GetDiskAttributes(diskname, storagetype, format, vgvendor, vgname, diskGuid);
            if (VolumeSummary::GPT == format) {
                DebugPrintf(SV_LOG_DEBUG, "Above disk is a GPT disk with mscs signature %s. "
                    "Hence collecting GPT Guid %s instead of signature.\n",
                    id.c_str(), diskGuid.c_str());
                id = diskGuid;
            }
            break;
        }
        position += sizeof(CLUSPROP_VALUE) + ALIGN_CLUSPROP(valueList.pValue->cbLength);
        if ((position + sizeof(DWORD)) >= listSize) {
            break;
        }
    }
    m_SharedDiskIds.insert(id);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void MSCSSharedDisksInformer::Reset(void)
{
    m_SharedDiskIds.clear();
    m_ErrorMessage.clear();
    CloseClusterHandle();
}


void MSCSSharedDisksInformer::CloseClusterHandle(void)
{
    if (m_hCluster) {
        CloseCluster(m_hCluster);
        m_hCluster = 0;
    }
}


bool MSCSSharedDisksInformer::GetStatusOnRetryMaxout(const bool &isclussvcauto)
{
    bool isloaded = false;
    if (!isclussvcauto) {
        DebugPrintf(SV_LOG_DEBUG, "Could not find shared disks with error %s. OK as cluster service is not automatic.\n", m_ErrorMessage.c_str());
        isloaded = true;
        m_ErrorMessage.clear();
    }
    return isloaded;
}

bool WmiEncryptableVolumeRecordProcessor::Process(IWbemClassObject *precordobj)
{
    USES_CONVERSION;

    VARIANT vtProp;
    HRESULT hrCol;
    std::string deviceID;
    VolumeSummary::BitlockerProtectionStatus protectionStatus;
    VolumeSummary::BitlockerConversionStatus conversionStatus;
    UINT32 encryptionPercentage;
    bool isVolumeLocked;
    bool bprocessed = false;

    const wchar_t DEVICEID[] = L"DeviceID";

    VariantInit(&vtProp);

    if (NULL == precordobj)
    {
        m_ErrMsg = "Record object is NULL";
        goto Cleanup;
    }

    hrCol = precordobj->Get(DEVICEID, 0, &vtProp, 0, 0);
    if (FAILED(hrCol))
    {
        m_ErrMsg = "Could not find column: ";
        m_ErrMsg += W2A(DEVICEID);
        goto Cleanup;
    }

    if (VT_BSTR == V_VT(&vtProp))
    {
        try
        {
            deviceID = W2A(V_BSTR(&vtProp));
        }
        catch (std::bad_alloc)
        {
            goto Cleanup;
        }
    }
    else
    {
        m_ErrMsg = "Type of DeviceID isn't B_STR but ";
        m_ErrMsg += std::to_string(V_VT(&vtProp));
        goto Cleanup;
    }

    if (GetProtectionStatus(precordobj, protectionStatus) != SVS_OK)
    {
        m_ErrMsg = "Failed in invoking GetProtectionStatus()";
        goto Cleanup;
    }

    try
    {
        m_bitlockerProtStatusMap.emplace(deviceID, std::to_string(protectionStatus));
    }
    catch (std::bad_alloc)
    {
        goto Cleanup;
    }

    if (GetConversionStatus(precordobj, isVolumeLocked, conversionStatus, encryptionPercentage) != SVS_OK)
    {
        m_ErrMsg = "Failed in invoking GetConversionStatus()";
        goto Cleanup;
    }

    BOOST_ASSERT(isVolumeLocked ==
        (protectionStatus == VolumeSummary::BitlockerProtectionStatus::PROTECTION_UNKNOWN));

    // Can't determine conversion status, if the volume is locked. User has to
    // manually unlock the volume in the bitlocker.
    if (!isVolumeLocked)
    {
        try
        {
            m_bitlockerConvStatusMap.emplace(deviceID, std::to_string(conversionStatus));
        }
        catch (std::bad_alloc)
        {
            goto Cleanup;
        }
    }

    // TODO-SanKumar-1806: Although we can't determine the exact conversion status
    // of the volume if locked, we could understand that the data disk is "not
    // fully decrypted" - since you can only lock an encrypted volume. So, should
    // we say conversion status = "partially encrypted"?

    bprocessed = true;

Cleanup:
    VariantClear(&vtProp);

    return bprocessed;
}

SVERROR WmiEncryptableVolumeRecordProcessor::GetProtectionStatus(
    IWbemClassObject *encryptableVolObj,
    VolumeSummary::BitlockerProtectionStatus &protectionStatus)
{
    SVERROR err;
    std::vector<WmiProperty> inParams;
    std::vector<WmiProperty> outParams;

    try
    {
        // NOTE-SanKumar-1805: Although the mof documentation says it's UInt32,
        // the variants are using type Int32.
        outParams.emplace_back(L"ReturnValue", VT_I4); // 0
        outParams.emplace_back(L"ProtectionStatus", VT_I4); // 1
    }
    catch (std::bad_alloc)
    {
        return SVE_OUTOFMEMORY;
    }

    auto genericWmiObj = this->GetGenericWMI();
    if (genericWmiObj == NULL)
        return SVE_INVALIDARG;

    err = genericWmiObj->ExecuteMethod(encryptableVolObj, L"GetProtectionStatus", inParams, outParams);
    if (err != SVS_OK)
        return err;

    if (V_I4(&outParams[0].Value) != S_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "GetProtectionStatus() returned %#x", V_I4(&outParams[0].Value));
        return SVE_FAIL;
    }

    protectionStatus = static_cast<VolumeSummary::BitlockerProtectionStatus>(V_I4(&outParams[1].Value));
    return SVS_OK;
}

SVERROR WmiEncryptableVolumeRecordProcessor::GetConversionStatus(
    IWbemClassObject *encryptableVolObj,
    bool &isVolumeLocked,
    VolumeSummary::BitlockerConversionStatus &conversionStatus,
    UINT32 &encryptionPercentage)
{
    SVERROR err;
    std::vector<WmiProperty> inParams;
    std::vector<WmiProperty> outParams;

    try
    {
        // NOTE-SanKumar-1805: Although the mof documentation says it's UInt32,
        // the variants are using type Int32.

        // NOTE-SanKumar-1805: The WMI function doesn't take this parameter in
        // Win7. In that case, the percentages would always be without decimal
        // places.
        // Number of decimal digits appended to the UINT.
        // Ex: 98.62318% is returned as 986231 for 4 and 98 for 0.

        //inParams.emplace_back(L"PrecisionFactor", VT_I4); // 0
        
        outParams.emplace_back(L"ReturnValue", VT_I4); // 0
        outParams.emplace_back(L"ConversionStatus", VT_I4); // 1
        outParams.emplace_back(L"EncryptionPercentage", VT_I4); // 2
        // NOTE-SanKumar-1805: The WMI function doesn't return these values in
        // Win7.
        //outParams.emplace_back(L"EncryptionFlags", VT_I4); // 3
        //outParams.emplace_back(L"WipingStatus", VT_I4); // 4
        //outParams.emplace_back(L"WipingPercentage", VT_I4); // 5
    }
    catch (std::bad_alloc)
    {
        return SVE_OUTOFMEMORY;
    }

    auto genericWmiObj = this->GetGenericWMI();
    if (genericWmiObj == NULL)
        return SVE_INVALIDARG;

    err = genericWmiObj->ExecuteMethod(encryptableVolObj, L"GetConversionStatus", inParams, outParams);
    if (err != SVS_OK)
        return err;

    if (V_I4(&outParams[0].Value) == FVE_E_LOCKED_VOLUME)
    {
        isVolumeLocked = true;
        return SVS_OK;
    }

    isVolumeLocked = false;

    if (V_I4(&outParams[0].Value) != S_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "GetConversionStatus() returned %#x", V_I4(&outParams[0].Value));
        return SVE_FAIL;
    }

    conversionStatus = static_cast<VolumeSummary::BitlockerConversionStatus>(V_I4(&outParams[1].Value));
    encryptionPercentage = V_I4(&outParams[2].Value);

    return SVS_OK;
}

//    bool bIgnoreDisksWithoutScsiAttributes = TEST_FLAG(m_ulDiscoveryFlags, DISCOVERY_IGNORE_DISKS_WITHOUT_SCSI_ATTRIBS);

NativeDiskRecordProcessor::NativeDiskRecordProcessor(
    DiskNamesToDiskVolInfosMap      *pdiskNamesToDiskVolInfosMap,  ///< Map from device name to volume information
    DeviceVgInfos                   *devicevginfos,                             ///< Device volume group information
    WmiDiskPartitionRecordProcessor *wmidp,                   ///< Processor for win32_diskpartition class
    bool                            reportScsiIDforDevName,                              ///< Report scsi-id instead of disk signature/GUID
    ULONG                           ulDiscoveryFlags,
    const bool                      &getscsiid)
    : m_reportScsiIDforDevName(reportScsiIDforDevName),
    m_pdiskNamesToDiskVolInfosMap(pdiskNamesToDiskVolInfosMap),
    m_pDeviceVgInfos(devicevginfos),
    m_ulDiscoveryFlags(ulDiscoveryFlags),
    m_wmidp(wmidp)
{
    if (!getscsiid) m_DeviceIDInformer.DoNotTryScsiID();
}
// Constructor for MsftPhysicalDiskRecordProcessor class
MsftPhysicalDiskRecordProcessor::MsftPhysicalDiskRecordProcessor(
    DiskNamesToDiskVolInfosMap *pdiskNamesToDiskVolInfosMap,      ///< Map from device name to volume information
    DeviceVgInfos *devicevginfos,                                 ///< Device volume group information
    WmiDiskPartitionRecordProcessor *wmidp,                       ///< Processor for win32_diskpartition class
    bool reportScsiIDforDevName,                                  ///< Report scsi-id instead of disk signature/GUID
    ULONG                           ulDiscoveryFlags,
    const bool &getscsiid)                                        ///< Flag to fetch scsi-id
    : NativeDiskRecordProcessor(pdiskNamesToDiskVolInfosMap, devicevginfos, wmidp, reportScsiIDforDevName, ulDiscoveryFlags, getscsiid)
{}

inline void SetSCSIAttributes(volinfo& vi, SCSI_ADDRESS scsiAddress)
{
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_BUS, std::to_string(scsiAddress.PathId)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_LOGICAL_UNIT, std::to_string(scsiAddress.Lun)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_PORT, std::to_string(scsiAddress.PortNumber)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::SCSI_TARGET_ID, std::to_string(scsiAddress.TargetId)));
}

inline void SetGeometryAttributes(volinfo& vi, DISK_GEOMETRY geometry)
{
    ULONGLONG totalSectors = geometry.Cylinders.QuadPart * geometry.TracksPerCylinder * geometry.SectorsPerTrack;

    vi.attributes.insert(std::make_pair(NsVolumeAttributes::TOTAL_CYLINDERS, std::to_string(geometry.Cylinders.QuadPart)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::TOTAL_HEADS, std::to_string(geometry.TracksPerCylinder)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::TOTAL_SECTORS, std::to_string(totalSectors)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::SECTORSPERTRACK, std::to_string(geometry.SectorsPerTrack)));
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::MEDIA_TYPE, s_MediaType[geometry.MediaType]));
}

inline void SetVendorProductAttributes(const HANDLE& h, const std::string& diskname, volinfo& vi)
{
    std::string     vendorId;
    std::string     productId;
    std::string     serialNumber;
    std::string     errormessage;

    if (!GetDeviceAttributes(h, diskname, vendorId, productId, serialNumber, errormessage)) {
        return;
    }

    if (!vendorId.empty()) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::MANUFACTURER, vendorId));
    }

    if (!productId.empty()) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::MODEL, productId));
    }

    if (!serialNumber.empty()) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::SERIAL_NUMBER, serialNumber));
    }
}

bool NativeDiskRecordProcessor::ProcessDisk(std::string diskName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    volinfo    vi;

    bool bSuccess = ProcessDisk(diskName, vi);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bSuccess;
}

bool NativeDiskRecordProcessor::ProcessDisk(SV_UINT index)
{
    std::stringstream       ssDiskName;
    volinfo                 vi;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ssDiskName << DISKNAMEPREFIX << index;

    bool bSuccess = ProcessDisk(ssDiskName.str(), vi);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bSuccess;
}

bool NativeDiskRecordProcessor::ProcessDisk(std::string diskName, volinfo&    vi)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (diskName.empty()) {
        m_ErrMsg = "Disk Name is empty";
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "Processing Disk Name: %s\n", diskName.c_str());

    std::stringstream   ssError;

    HANDLE hDisk = SVCreateFile(diskName.c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

    if (INVALID_HANDLE_VALUE == hDisk)
    {
        ssError << "Failed to open disk " << diskName << " error: " << GetLastError();
        m_ErrMsg = ssError.str();
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }
    ON_BLOCK_EXIT(&CloseHandle, hDisk);

    vi.attributes.insert(std::make_pair(NsVolumeAttributes::DEVICE_NAME, diskName));

    STORAGE_BUS_TYPE    busType;
    if (!GetBusType(hDisk, busType, m_ErrMsg)) {
        DebugPrintf(SV_LOG_ERROR, "Disk %s err=%s\n", diskName.c_str(), m_ErrMsg.c_str());
        return false;
    }

    if (s_InterfaceType.end() == s_InterfaceType.find(busType)) {
        busType = BusTypeUnknown;
    }

    DebugPrintf(SV_LOG_INFO, "Disk %s BusType: %d\n", diskName.c_str(), busType);

    if (IsBusTypeUnsupported(busType)) {
        DebugPrintf(SV_LOG_INFO, "Skipping disk %s as it is %s disk\n", diskName.c_str(), s_InterfaceType[busType].c_str());
        return true;
    }

    // Get Device Number
    dev_t   deviceNumber;

    if (!GetDeviceNumber(hDisk, diskName, deviceNumber, m_ErrMsg)) {
        return false;
    }

    vi.devno = deviceNumber;

    bool bIgnoreDisksWithoutScsiAttributes = TEST_FLAG(m_ulDiscoveryFlags, DISCOVERY_IGNORE_DISKS_WITHOUT_SCSI_ATTRIBS);

    // Get SCSI Details
    SCSI_ADDRESS scsiAddress = { 0 };
    if (GetSCSIAddress(hDisk, diskName, scsiAddress, m_ErrMsg)) {
        SetSCSIAttributes(vi, scsiAddress);
    }
    else if (bIgnoreDisksWithoutScsiAttributes) {
        DebugPrintf(SV_LOG_INFO, "Skipping disk %s as it is not reporting scsi address\n", diskName.c_str());
        return false;
    }

    // Get Drive Geometry
    DISK_GEOMETRY geometry = { 0 };
    if (GetDriveGeometry(hDisk, diskName, geometry, m_ErrMsg)) {
        SetGeometryAttributes(vi, geometry);
    }


    DebugPrintf(SV_LOG_DEBUG, "Reporting Device Name : %s\n", diskName.c_str());

    std::string storagetype;
    std::string diskGuid;

    GetDiskAttributes(diskName, storagetype, vi.formatlabel, vi.volumegroupvendor, vi.volumegroupname, diskGuid);

    vi.deviceid = m_DeviceIDInformer.GetID(diskName);
    vi.devname = (m_reportScsiIDforDevName) ? vi.deviceid : diskGuid;

    if (vi.devname.empty())
    {
        ssError << "For disk device name: " << diskName << " could not find "
            << (m_reportScsiIDforDevName ? "SCSI Id" : "MBR signature or, GPT GUID")
            << ". Hence not reporting this disk.";

        DebugPrintf(SV_LOG_ERROR, "%s\n", ssError.str().c_str());

        m_ErrMsg = ssError.str();
        return false;
    }

    DiskNamesToDiskVolInfosMap::iterator it = m_pdiskNamesToDiskVolInfosMap->find(vi.devname);
    if (it != m_pdiskNamesToDiskVolInfosMap->end())
    {
        std::string dupDeviceName;

        volinfo vin = it->second;
        if (vin.attributes.find(NsVolumeAttributes::DEVICE_NAME) != vin.attributes.end())
        {
            dupDeviceName = vin.attributes.find(NsVolumeAttributes::DEVICE_NAME)->second;
        }

        // 1. check if the disk names of the two devices same
        // ex. PhysicalDrive1
        if (0 == diskName.compare(dupDeviceName))
        {
            DebugPrintf(SV_LOG_INFO, "Found 2 disks with same id: %s and name: %s. Skipping second one\n", vi.devname.c_str(), diskName.c_str());
            return true;
        }

        ssError << "Found multiple disks " << diskName << "," << dupDeviceName <<
            " with same id: " << vi.devname <<
            ". Hence not reporting any disk matching the id.";

        m_ErrMsg = ssError.str();

        DebugPrintf(SV_LOG_ERROR, "%s : %s\n", FUNCTION_NAME, m_ErrMsg.c_str());

        // we do not want to report this disk and duplicate disk
        // removing entry inserted in earlier processing as well
        m_pdiskNamesToDiskVolInfosMap->erase(vi.devname);
        return false;
    }

    if (!storagetype.empty()) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::STORAGE_TYPE, storagetype));
    }

    if (m_wmidp->IsDiskBootable(vi.devname)) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::HAS_BOOTABLE_PARTITION, STRBOOL(true)));
    }

    if (HasUefi(vi.devno)) {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::HAS_UEFI, STRBOOL(true)));
    }

    // Populate Interface Type
    std::string interface_type = ToLower(s_InterfaceType[busType]);
    vi.attributes.insert(std::make_pair(NsVolumeAttributes::INTERFACE_TYPE, interface_type.c_str()));

    // to identify if device name is fetched based on signature or scsi id
    if (m_reportScsiIDforDevName){
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::NAME_BASED_ON, NsVolumeAttributes::SCSIID));
    }
    else {
        vi.attributes.insert(std::make_pair(NsVolumeAttributes::NAME_BASED_ON, NsVolumeAttributes::SIGNATURE));
    }

    vi.mounted = true;
    vi.voltype = VolumeSummary::DISK;
    vi.vendor = VolumeSummary::NATIVE;

    std::string errMsg;
    vi.capacity = GetDiskSize(hDisk, errMsg);

    if (vi.capacity) {
        vi.rawcapacity = vi.capacity;
        m_pDeviceVgInfos->insert(std::make_pair(vi.devname, VgInfo(vi.volumegroupvendor, vi.volumegroupname)));
    }
    else {
        m_ErrMsg = "Failed to find size of disk with error: ";
        m_ErrMsg += errMsg;
        m_ErrMsg += ". Not collecting this disk.";
        return false;
    }

    SetVendorProductAttributes(hDisk, diskName, vi);

    bool bClustered = false;
    std::string err_Msg;

    if (IsDiskClustered(diskName, bClustered, err_Msg))
    {
        if (bClustered)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Disk %s is a clustered disk\n", FUNCTION_NAME, diskName.c_str());
            vi.attributes.insert(std::make_pair(NsVolumeAttributes::IS_PART_OF_CLUSTER, "true"));
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Disk %s is not a clustered disk\n", FUNCTION_NAME, diskName.c_str());
        }
    }
    else
    {
        m_ErrMsg = "IsDiskClustered() failed with error: " + err_Msg;
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, m_ErrMsg.c_str());
    }

    m_pdiskNamesToDiskVolInfosMap->insert(std::make_pair(vi.devname, vi));
    DebugPrintf(SV_LOG_INFO, "Added Disk %s name: %s scsi id: %s bustype: %d\n",
                                            diskName.c_str(),
                                            vi.devname.c_str(),
                                            vi.deviceid.c_str(),
                                            busType);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

/*
* FUNCTION NAME     : Process
*
* DESCRIPTION       : Process MSFT_PhysicalDisk class records to fetch all the physical disks information
*
* INPUT PARAMETERS  : Record Object
*
* OUTPUT PARAMETERS : NONE
*
* return value      : Returns false in below cases otherwise returns true
*                     1. Record object is null
*                     2. Disk index is not found
*                     3. Open disk handle failure
*                     4. Disk signature/scsi-id is not found
*                     5. Found multiple disks with same signature/scsi-id
*                     6. Disk size is not found
*
*/
bool MsftPhysicalDiskRecordProcessor::Process(IWbemClassObject *precordobj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (NULL == precordobj)
    {
        m_ErrMsg = "Record object is NULL";
        return false;
    }

    bool  bprocessed = true;

    std::string         diskIndex;
    volinfo             vi;
    VARIANT             vtProp;
    HRESULT             hrCol;
    VARIANT_BOOL        bVarCanPool;
    BOOLEAN             bCanPool;
    std::stringstream   ssError;

    USES_CONVERSION;

    // Get Disk Index
    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(MSFT_PHYSICALDISK_ATTRIB::INDEXCOLUMNNAME), 0, &vtProp, 0, 0);
    if (FAILED(hrCol) || (VT_BSTR != V_VT(&vtProp)))
    {
        ssError << "Could not find column: " << MSFT_PHYSICALDISK_ATTRIB::INDEXCOLUMNNAME;
        m_ErrMsg = ssError.str();
        return false;
    }

    diskIndex = W2A(V_BSTR(&vtProp));
    VariantClear(&vtProp);

    std::stringstream ssDiskName;
    ssDiskName << DISKNAMEPREFIX << diskIndex;
    std::string diskname = ssDiskName.str();

    vi.attributes.insert(std::make_pair(NsVolumeAttributes::DEVICE_NAME, diskname));

    // Collect only storage pool disks
    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(MSFT_PHYSICALDISK_ATTRIB::CANPOOLCOLUMNNAME), 0, &vtProp, 0, 0);
    if (FAILED(hrCol) || (VT_BOOL != V_VT(&vtProp))) {
        m_ErrMsg = "Could not find column: ";
        m_ErrMsg += MSFT_PHYSICALDISK_ATTRIB::CANPOOLCOLUMNNAME;
        return false;
    }

    bVarCanPool = V_BOOL(&vtProp);
    bCanPool = (VARIANT_TRUE == bVarCanPool);

    VariantClear(&vtProp);

    if (bCanPool) {
        DebugPrintf(SV_LOG_INFO, "Skipping disk %s as it is not part of storage pool\n", diskname.c_str());
        return true;
    }

    CComVariant         varCannotPoolReason;
    hrCol = precordobj->Get(A2W(MSFT_PHYSICALDISK_ATTRIB::CANNOTPOOLREASONCOLUMNNAME), 0, &varCannotPoolReason, 0, 0);
    if (FAILED(hrCol) || ((VT_ARRAY | VT_I4) != V_VT(&varCannotPoolReason)))
    {
        m_ErrMsg = "Could not find column: ";
        m_ErrMsg += MSFT_PHYSICALDISK_ATTRIB::CANNOTPOOLREASONCOLUMNNAME;
        return false;
    }

    BOOL bStoragePoolDisk = FALSE;
    try{
        CComSafeArray<int> cannotPoolReasons(varCannotPoolReason.parray);
        for (LONG l = 0; l < cannotPoolReasons.GetCount(); l++) {
            int iCannotPoolReason = static_cast<int> (cannotPoolReasons[l]);
            if (SpDriveCannotPoolReasonAlreadyPooled == iCannotPoolReason) {
                bStoragePoolDisk = TRUE;
                break;
            }
        }
    }
    catch (CAtlException const& e)
    {
        ssError << "Failed to get column: " << MSFT_PHYSICALDISK_ATTRIB::CANNOTPOOLREASONCOLUMNNAME << " hr=" << std::hex << e.m_hr;
        m_ErrMsg = ssError.str();
        DebugPrintf(SV_LOG_ERROR, "%s\n", m_ErrMsg.c_str());
        return false;
    }

    if (!bStoragePoolDisk) {
        DebugPrintf(SV_LOG_INFO, "Skipping disk %s as it is not part of storage pool with cannotPoolReason\n", diskname.c_str());
        return true;
    }

    bprocessed = ProcessDisk(diskname, vi);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bprocessed;
}

/*
* FUNCTION NAME     : GetErrMsg
*
* DESCRIPTION       : Returns error messages
*
* INPUT PARAMETERS  : NONE
*
* OUTPUT PARAMETERS : NONE
*
* return value      : error message
*
*/
std::string NativeDiskRecordProcessor::GetErrMsg(void)
{
    return m_ErrMsg;
}

bool DriverDiskRecordProcessor::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    for (auto index : m_diskIndexSet) {
        ProcessDisk(index);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool DriverDiskRecordProcessor::Enumerate()
{
    PDISKINDEX_INFO     pDiskIndexList = NULL;
    DWORD               size = 0, dwBytes;
    std::vector<UCHAR>  Buffer;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    HANDLE hInmage = SVCreateFile(INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (INVALID_HANDLE_VALUE == hInmage) {
        DebugPrintf(SV_LOG_ERROR, "Failed to open device %s err=%d\n", INMAGE_DISK_FILTER_DOS_DEVICE_NAME, GetLastError());
        return false;
    }

    ON_BLOCK_EXIT(&CloseHandle, hInmage);

    bool bSuccess = false;

    size = sizeof(DISKINDEX_INFO) + MAX_NUM_DISKS_SUPPORTED * sizeof(ULONG);
    Buffer.resize(size);
    pDiskIndexList = (PDISKINDEX_INFO)&Buffer[0];

    ZeroMemory(pDiskIndexList, size);

    bSuccess = DeviceIoControl(hInmage, IOCTL_INMAGE_GET_DISK_INDEX_LIST, NULL, 0, pDiskIndexList, size, &dwBytes, NULL);

    if (bSuccess){
        for (ULONG ulIndex = 0; ulIndex < pDiskIndexList->ulNumberOfDisks; ulIndex++) {
            DebugPrintf(SV_LOG_DEBUG, "%s: Adding disk %d\n", FUNCTION_NAME, pDiskIndexList->aDiskIndex[ulIndex]);
            m_diskIndexSet.insert(pDiskIndexList->aDiskIndex[ulIndex]);
        }
    }
    else {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_DISK_INDEX_LIST failed for device %s err=%d\n",
                                    INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
                                    GetLastError());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bSuccess;
}

bool ConfigDiskProcessor::Enumerate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    LocalConfigurator       lConfig;

    SV_UINT uiMaxDiskIndex = lConfig.GetMaximumDiskIndex();

    SV_UINT uiMaxConsMissingDiskIndex = lConfig.GetMaximumConsMissingDiskIndex();

    bool bSuccess = false;

    if (0 == uiMaxDiskIndex) {
        DebugPrintf(SV_LOG_DEBUG, "Invalid value for max disk in config file. Skipping\n");
        return bSuccess;
    }

    std::set<ULONG>     diskIndices = GetAvailableDiskIndices(uiMaxDiskIndex, uiMaxConsMissingDiskIndex);
    if (diskIndices.empty()) {
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return bSuccess;
    }

    bSuccess = true;

    m_diskIndexSet.insert(diskIndices.begin(), diskIndices.end());

    if (m_pdiskNamesToDiskVolInfosMap) {
        for (auto diskIt : *m_pdiskNamesToDiskVolInfosMap) {
            if (m_diskIndexSet.find(diskIt.second.devno) != m_diskIndexSet.end()) {
                m_diskIndexSet.erase(diskIt.second.devno);
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bSuccess;
}

bool ConfigDiskProcessor::Process()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    for (auto index : m_diskIndexSet) {
        DebugPrintf(SV_LOG_DEBUG, "Processing Disk Index: %d\n", index);
        ProcessDisk(index);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}