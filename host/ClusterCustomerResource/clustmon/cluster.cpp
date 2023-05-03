//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : cluster.cpp
//
// Description:
//
#include <windows.h>
#include <WinIoCtl.h>
#include <clusapi.h>
#include <resapi.h>
#include <ctype.h>
#include <iostream>
#include <sstream>
#include <shlwapi.h>
#include <exception>
#include <cstdarg>
#include <set>
#include <map>
#include <cmath>
#include <cctype>
#include <iterator>
#include <vector>
#include <process.h>
#include <cstdio>
#include <iomanip>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "cluster.h"
#include "configurator2.h"
#include "configurevxagent.h"
#include "configurecxagent.h"
#include "devicefilter.h"
#include "hostagenthelpers.h"
#include "hostagenthelpers_ported.h"
#include "cluster.h"
#include "inmageex.h"
#include "localconfigurator.h"
#include "configwrapper.h"
#include "fiopipe.h"
#include "scopeguard.h"
#include "cdputil.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmalertdefs.h"

// ----------------------------------------------------------------------------
// global constants
// ----------------------------------------------------------------------------
static WCHAR const ClusterTrackingInfoKeyName[] = L"ClusterTracking";
static WCHAR const ClusterTrackingVersionValueName[] = L"Version";
static WCHAR const ClusterTrackingVolumesValueName[] = L"Volumes";
static WCHAR const ClusterTrackingDependenciesValueName[] = L"Dependencies";
static WCHAR const ClusterTrackingAvailableVolumesValueName[] = L"AvailableVolumes";
static WCHAR const ClusterTrackingNameValueName[] = L"Name";
static WCHAR const GuidPrefix[] = L"\\??\\Volume{";
static WCHAR const ResourceTypePhysicalDisk[] = L"Physical Disk";
static WCHAR const ResourceTypeExchangeSystemAttendant[] = L"Microsoft Exchange System Attendant";
static WCHAR const ResourceTypeSqlServer[] = L"SQL Server";
static WCHAR const ResourceTypeNetworkName[] = L"Network Name";
static WCHAR const VirtualServerSql[] = L"VirtualServerSql";
static WCHAR const VirtualServerExchange[] = L"VirtualServerExchange";
static WCHAR const NetworkName[] = L"NetworkName";
static WCHAR const NetworkNameId[] = L"NetworkNameId";
static WCHAR const VirtualServerName[] = L"VirtualServerName";
static WCHAR const ResourceTypeVeritasVolumeManagerDiskGroup[] = L"Volume Manager Disk Group";

static WCHAR const ClusDiskName[] = L"DiskName";

static char const DosDevicesPrefix[] = "\\DosDevices\\A:";

static DWORD const ClusterTrackingVersion = 2;

static DWORD const ValueLength = 255;

static DWORD const CLUSTER_NOTIFY_TIMEOUT_MILISECONDS = 5000;

// FIXME: don't use this, get it from some common place once we have 1 common place
static const WCHAR SV_ROOT_KEY_UNICODE[] = L"SOFTWARE\\SV Systems";

HCLUSTER g_hCluster = 0;

bool g_UseGuidAsSignatureOffset = false;

#ifdef CLUSTER_STAND_ALONE
#define CLUSTER_LOG(x) std::wcout << x;
#else
#define CLUSTER_LOG(x)
#endif

typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;

// ----------------------------------------------------------------------------
// for tracking mount points that maybe under the wrong group
//
// NOTE: From MSCS documantation
// "When you create a volume mount point on a server cluster, you need to
// take into consideration the following key items in regards to volume
// mount points:
// * They cannot go between clustered, and non-clustered disks.
// * You cannot create mount points on the Quorum disk.
// * If you have a mount point from one shared disk to another, you must
// make sure that they are in the same group, and that the mounted disk
// is dependent on the root disk."
//
// This means any mount points found under the wrong group still get reported
// and show up under that (wrong) group without any hint that the setup maybe
// invalid (if not using the mount point that is under the wrong group, then no
// probelms).
//
// This is done because starting with w2k8, there is a default group that shared
// disks are placed under when first added to the cluster or if removed from
// the "Service or Application" they were under but are still in the cluster.
// ----------------------------------------------------------------------------
typedef std::map<std::wstring, std::wstring> MountPointsUnderWrongGroup_t;
MountPointsUnderWrongGroup_t MountPointsUnderWrongGroup;

// ----------------------------------------------------------------------------
// register cluser info to cx
// ----------------------------------------------------------------------------
void RegisterClusterInfo(char * request, ClusterInfo const & info)
{
    DumpClusterInfo(request, info);

#ifndef CLUSTER_STAND_ALONE
    LocalConfigurator TheLocalConfigurator;
    if (TheLocalConfigurator.registerClusterInfoEnabled())
    {
        Configurator* TheConfigurator = NULL;
        if(!GetConfigurator(&TheConfigurator))
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to obtain configurator %s %d\n", FUNCTION_NAME, LINE_NO);
            return;
        }
		
		const int SECONDSRETRYINTERVAL = 10; 
		do {
			if (registerClusterInfo(*TheConfigurator, request, info)) {
				DebugPrintf(SV_LOG_DEBUG, "Cluster registration with request %s, performed successfully\n", request);
				break;
			} else
				DebugPrintf(SV_LOG_ERROR, "Cluster registration with request %s, failed. Retrying after %d seconds\n", request, SECONDSRETRYINTERVAL);
		} while (!CDPUtil::QuitRequested(SECONDSRETRYINTERVAL)); /* TODO: instead of cdputil quit, use the cluster quit */
    }
#endif
}

// ----------------------------------------------------------------------------
// dump all the data in the given clusterInfo if CLUSTER_STAND_ALONE dumps to
// std:::cout else if debug dumps to OutputDebugStringA
// ----------------------------------------------------------------------------
void DumpClusterInfo(char const * type, ClusterInfo const & info)
{
    std::stringstream msg;

    msg << "\n=============   " << type << "   =============\n"
        << "Cluster Name: " << info.name << '\n'
        << "Cluster Id  : " << info.id << '\n';

    ClusterGroupInfos_t::const_iterator gIter(info.groups.begin());
    ClusterGroupInfos_t::const_iterator gEndIter(info.groups.end());
    for (/* emnpty*/; gIter != gEndIter; ++gIter) {
        msg << "   Group Name             : " << (*gIter).name << '\n'
            << "   Group Id               : " << (*gIter).id << '\n'
            << "   Sql Virtual Server     : " << (*gIter).sqlVirtualServerName << '\n'
            << "   Exchange Virtual Server: " << (*gIter).exchangeVirtualServerName << '\n';

        ClusterResourceInfos_t::const_iterator rIter((*gIter).resources.begin());
        ClusterResourceInfos_t::const_iterator rEndIter((*gIter).resources.end());
        for (/* emnpty*/; rIter != rEndIter; ++rIter) {
            msg << "      Resource Name: " << (*rIter).name << '\n'
                << "      Resource Id  : " << (*rIter).id << '\n';


            ClusterVolumeInfos_t::const_iterator vIter((*rIter).volumes.begin());
            ClusterVolumeInfos_t::const_iterator vEndIter((*rIter).volumes.end());
            for (/* emnpty*/; vIter != vEndIter; ++vIter) {
                msg << "         Device Name: " << (*vIter).deviceName << '\n'
                    << "         Device Id  : " << (*vIter).deviceId << '\n';
            }

            msg << '\n';

            ClusterResourceDependencyInfos_t::const_iterator dIter((*rIter).dependencies.begin());
            ClusterResourceDependencyInfos_t::const_iterator dEndIter((*rIter).dependencies.end());
            for (/* emnpty*/; dIter != dEndIter; ++dIter) {
                msg << "         Dependency Id  : " << (*dIter).id << '\n';
            }
            msg << '\n';
        }
        msg << '\n';
    }
    msg << '\n';

    msg << "Available Volumes\n";

    ClusterAvailableVolumes_t::const_iterator availableIter(info.availableVolumes.begin());
    ClusterAvailableVolumes_t::const_iterator availableEndIter(info.availableVolumes.end());
    for (/* empty */; availableIter != availableEndIter; ++availableIter) {
        msg << "  Device Name: " << (*availableIter).name << '\n'
            << "  Device Id  : " << (*availableIter).id << '\n';
    }
    msg << '\n';

    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
}


// ----------------------------------------------------------------------------
// registery helper functions
// ----------------------------------------------------------------------------
// for querying string values
bool QueryString(HKEY hKey, WCHAR const * valueName, std::wstring & data,bool mustHaveValue = true)
{
    std::vector<WCHAR> value(ValueLength);

    DWORD valueLength = ValueLength;

    DWORD type = REG_SZ;

    LONG status;
    do {
        value.resize(valueLength);
        status = RegQueryValueExW(hKey, valueName, 0, &type,
                                  reinterpret_cast<BYTE*>(&value[0]), &valueLength);
    } while (ERROR_MORE_DATA == status);

    if (ERROR_SUCCESS != status) {
        if (mustHaveValue || ERROR_FILE_NOT_FOUND != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed QueryValue RegQueryValueExW %S: %d\n",
                        ClusterTrackingVersionValueName, status);
            return false;
        }
        return true;
    }

    data = &value[0];

    return true;
}

// for querying values that are treated as bool (1 = true, 0 = false)
bool QueryBool(HKEY hKey, WCHAR const * valueName, bool data, bool mustHaveValue = true)
{
    data = false;

    DWORD value;
    DWORD valueLength = sizeof(DWORD);
    DWORD type = REG_DWORD;

    LONG status;

    status = RegQueryValueExW(hKey, valueName, 0, &type, reinterpret_cast<BYTE*>(&value), &valueLength);
    if (ERROR_SUCCESS != status) {
        if (mustHaveValue || ERROR_FILE_NOT_FOUND != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed QueryValue RegQueryValueExW %S: %d\n",
                        ClusterTrackingVersionValueName, status);
            return false;
        }
        return true;
    }

    data = (1 == value);

    return true;
}

// for enumerating all the subkeys under a key
LONG EnumerateKey(HKEY hKey, DWORD index, std::wstring & name)
{
    std::vector<WCHAR> value(ValueLength);

    DWORD valueLength = ValueLength;

    LONG status;

    /* TODO: manual page does not say that when ERROR_MORE_DATA is status,
       valueLength is to be used */
    do {
        value.resize(valueLength);
        status = RegEnumKeyExW(hKey, index, &value[0], &valueLength, NULL, NULL, NULL, NULL);
    } while (ERROR_MORE_DATA == status);

    switch (status) {
        case ERROR_SUCCESS:
            name = &value[0];
            break;
        case ERROR_NO_MORE_ITEMS:
            break;
        default:
            DebugPrintf(SV_LOG_ERROR, "FAILED EnumKey RegEnumKeyExW: %d\n", status);
            return status;
    }

    return status;
}

// for getting the signature and offset for a drive letter, this is basically the data associated
// with the drive letter's mount point entry in the MountedDevices registry key
bool GetDriveLetterSignatureOffset(HKEY hKey, std::wstring const & name, WCHAR * signatureOffset, int signatureOffsetSize)
{
    std::wstring valueName(L"\\DosDevices\\");
    valueName += name;

    DWORD valueSize = SIGNATURE_OFFSET_LENGTH;

    byte value[SIGNATURE_OFFSET_LENGTH];

    DWORD type = REG_DWORD;

    long status = RegQueryValueExW(hKey, valueName.c_str(), 0, &type, value, &valueSize);
    switch (status) {
        case ERROR_SUCCESS:
            break;
        case ERROR_NO_MORE_ITEMS:
        case ERROR_MORE_DATA:
            valueSize = SIGNATURE_OFFSET_LENGTH;
            return false;
        default:
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDriveLetterSignatureOffset RegQueryValueEx %S: %d\n", valueName.c_str(), status);
            return false;
    }

    _snwprintf(signatureOffset, signatureOffsetSize,
               L"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
               value[0], value[1], value[2], value[3], value[4], value[5],
               value[6], value[7], value[8], value[9], value[10], value[11]);

    return true;
}

// for saving a tracking info's id and name
template<class TrackingInfo>
bool SaveNameId(HKEY hKey, TrackingInfo const & trackingInfo, HKEY & trackingKey)
{
    long status = RegCreateKeyExW(hKey, trackingInfo.Id().c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed SaveNameId creating/opening %S for %S: %d\n",
                    trackingInfo.Id().c_str(), trackingInfo.Name().c_str(), status);
        return false;
    }

    status = RegSetValueExW(trackingKey, ClusterTrackingNameValueName, 0,
                            REG_SZ, reinterpret_cast<const BYTE*>(trackingInfo.Name().c_str()),
                            static_cast<DWORD>(trackingInfo.Name().length() * sizeof(WCHAR)));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed SaveNameId RegSetValueEx %S=%S: %d\n",
                    ClusterTrackingNameValueName, trackingInfo.Name().c_str(), status);
        RegCloseKey(trackingKey);
        return false;
    }

    return true;
}

// for loading a tracking info's id and name
template <class TrackingInfo>
bool LoadNameId(HKEY hKey, std::wstring const & id, TrackingInfo & trackingInfo, HKEY & trackingKey)
{
    long status = RegOpenKeyW(hKey, id.c_str(), &trackingKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed LoadNameId RegOpenKeyW %S: %d\n", id.c_str(), status);
        return false;
    }

    std::wstring name;

    if (!::QueryString(trackingKey, ClusterTrackingNameValueName, name)) {
        RegCloseKey(trackingKey);
        return false;
    }

    trackingInfo.SetIdName(id, name);

    return true;
}

// ----------------------------------------------------------------------------
// mount point help functions
// ----------------------------------------------------------------------------
// forward decleration
bool GetMountPointSignatureOffsetForMountPoint(HKEY hKey, std::wstring const & name, SignatureOffsetMountPoints_t & somp);
bool GetMountPointSignatureOffset(HKEY hKey, std::wstring const& name, SignatureOffsetMountPoints_t& somp);
bool GetSignatureOffsetFromClusDisk(const std::wstring &volume, const WCHAR *guid, byte *signatureOffset, const size_t &nElemsInSignatureOffset);
bool FillSignatureOffsetFromClusDisk(const DWORD &disknumber, const LARGE_INTEGER &startingoffset, byte *signatureOffset, const size_t &nElemsInSignatureOffset);
void FillDiskNumberToSignatureFromClusDisk(std::map<DWORD, std::wstring> &disknumbertosignature);
void LoadDiskNumberToSignatureFromClusDisk(HKEY hKey, const std::wstring &signature, std::map<DWORD, std::wstring> &disknumbertosignature);

// gets all the mount points for the given name
// it will recurse (through the call to GetMountPointSignatureOffsetForMountPoint) to get mount points
// under a mount point. Because of this approach need to guard against cyclic mount points as that will
// cause an infinite loop.
// E.g. Assume some user does something like this
// has 2 physical disks (pd1 and pd2) set up as follows
//   D:\ -> pd1
//   D:\mntpnt1 -> pd2
//   D:\mntpnt1\mntpnt2 -> pd1
// that creates an infinite loop
//
// Since we only report 1 mount point per volume the loop is detected by using a map, inserting the same signaturoffset
// will fail, thus indicating a mount point cycle. in the future if we want to be able to report multiple mount points
// then this will have to be updated to allow mulitple mount points but detect a mount point cycle.
bool GetMountPointSignatureOffset(HKEY hKey, std::wstring const & name, SignatureOffsetMountPoints_t & somp)
{
    USES_CONVERSION;
    if (name.empty()) {
        return true;
    }

    DWORD type = REG_BINARY;
    DWORD valueSize;

    byte value[SIGNATURE_OFFSET_LENGTH];

    WCHAR mountedVolumeGuid[MAX_GUID];
    WCHAR guid[MAX_GUID];
    WCHAR nameGuid[MAX_GUID];
    WCHAR mountPointPath[MAX_PATH];

    std::wstring mountPoint;

    if (!GetVolumeNameForVolumeMountPointW(name.c_str(), nameGuid, sizeof(nameGuid))) {
        DWORD err = GetLastError();
        DebugPrintf(ERROR_FILE_NOT_FOUND == err ?  SV_LOG_WARNING : SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetMountPointSignatureOffset GetVolumeNameForVolumeMountPoint SYSTEM\\MountedDevices: %d\n", err);
        return false;
    }

    HANDLE hMountPoint = FindFirstVolumeMountPointW(name.c_str(), mountPointPath, sizeof(mountPointPath));
    if (INVALID_HANDLE_VALUE == hMountPoint) {
        DebugPrintf(SV_LOG_DEBUG, "For name %S, could not find any mountpoints\n", name.c_str());
        return true; // no mount points for this mount point
    }

    boost::shared_ptr<void> hMountPointGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &FindVolumeMountPointClose, hMountPoint));

    do {
		DebugPrintf(SV_LOG_DEBUG, "Got mount point path %S for name %S\n", mountPointPath, name.c_str());
        mountPoint = name + mountPointPath;
        if (!GetVolumeNameForVolumeMountPointW(mountPoint.c_str(), guid, sizeof(guid))) {
            DWORD err = GetLastError();
            DebugPrintf(ERROR_FILE_NOT_FOUND == err ?  SV_LOG_WARNING : SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetMountPointSignatureOffset GetVolumeNameForVolumeMountPoint SYSTEM\\MountedDevices: %d\n", err);
            return false;
        }
		DebugPrintf(SV_LOG_DEBUG, "For mountpoint %S, got guid %S\n", mountPoint.c_str(), guid);

        WCHAR signatureOffset[MAX_SIGNATURE_OFFSET_LENGTH + 1] = {L'\0'};
        if (g_UseGuidAsSignatureOffset) {
            _snwprintf(signatureOffset, NELEMS(signatureOffset),
                       L"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                       guid[11], guid[12], guid[13], guid[14], guid[15], guid[16], guid[17], guid[18],
                       guid[20], guid[21], guid[22], guid[23],
                       guid[25], guid[26], guid[27], guid[28],
                       guid[30], guid[31], guid[32], guid[33],
                       guid[35], guid[36], guid[37], guid[38], guid[39], guid[40], guid[41], guid[42], guid[43], guid[44], guid[45], guid[46]);
        } else {
            _snwprintf(mountedVolumeGuid, NELEMS(mountedVolumeGuid), L"\\??\\%s",  &guid[4]);
            mountedVolumeGuid[wcslen(mountedVolumeGuid) - 1] = L'\0'; // remove trailing backslash (\)
			DebugPrintf(SV_LOG_DEBUG, "Asking registry key for mounted volume Guid %S\n", mountedVolumeGuid);
			/* Needed here as valueSize will be updated by RegQueryValueExW in case of ERROR_MORE_DATA */
			valueSize = NELEMS(value);
            long status = RegQueryValueExW(hKey, mountedVolumeGuid, 0, &type, value, &valueSize);
            switch (status) {
                case ERROR_SUCCESS:
                    DebugPrintf(SV_LOG_DEBUG, "successfully got signature offset from SYSTEM\\MountedDevices devices for volume Guid %S\n", mountedVolumeGuid);
                    break;
				case ERROR_NO_MORE_ITEMS:
					DebugPrintf(SV_LOG_WARNING, "Got ERROR_NO_MORE_ITEMS for mounted volume Guid %S in SYSTEM\\MountedDevices\n", mountedVolumeGuid);
					continue;
				default:
					/* This can be a guid for gpt disk on windows 2k3 */
					DebugPrintf(SV_LOG_DEBUG, "Got error %d to find signature offset for mounted volume Guid %S in SYSTEM\\MountedDevices. "
						"Getting it from ClusDisk registry\n", status, mountedVolumeGuid);
					if (!GetSignatureOffsetFromClusDisk(mountPoint, guid, value, NELEMS(value))) {
						DebugPrintf(SV_LOG_ERROR, "Could not get signature offset for mountedVolumeGuid %S even from ClusDisk registry\n", 
							mountedVolumeGuid);
						continue;
					}
					break;
            }

            _snwprintf(signatureOffset, NELEMS(signatureOffset),
                       L"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                       value[0], value[1], value[2], value[3], value[4], value[5],
                       value[6], value[7], value[8], value[9], value[10], value[11]);
			DebugPrintf(SV_LOG_DEBUG, "Got signature offset %S for mounted volume Guid %S.\n", signatureOffset, mountedVolumeGuid);
        }

        if (L'\\' == mountPoint[mountPoint.length() - 1]) {
            mountPoint.erase(mountPoint.length() - 1);
        }

        DebugPrintf(SV_LOG_DEBUG, "creating pair of signatureOffset %S, mountPoint %S\n", signatureOffset, mountPoint.c_str());
        auto insertRs(somp.insert(std::make_pair(signatureOffset, mountPoint)));
        if (!insertRs.second) {
            DebugPrintf(SV_LOG_DEBUG, "For name %S, mountPoint %S, cycle detected\n", name.c_str(), mountPoint.c_str());
            return true; // mount point cycle detected
        }

        GetMountPointSignatureOffsetForMountPoint(hKey, mountPoint, somp);

    } while (FindNextVolumeMountPointW(hMountPoint, mountPointPath, sizeof(mountPointPath)));

    return true;
}


/* 
 * 
 * Gets the disk number and starting offset of the volumeGUID using windows ioctl,
 * and forms a signatureoffset by getting signature for
 * the disk number got earlier from cluster svc key 
 * "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ClusDisk\Parameters\Signatures"
 *
 * NOTE: read signatureoffset as disk signature plus starting offset of volume in the disk.
 * 
*/
bool GetSignatureOffsetFromClusDisk(const std::wstring &volume, const WCHAR *guid, byte *signatureOffset, const size_t &nElemsInSignatureOffset)
{
	bool rval = false;

	std::wstring guidtoopen(guid);
	std::wstring::size_type len = guidtoopen.length();
	if (guidtoopen[len-1] == L'\\')
		guidtoopen.erase(len-1);

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with volume %S, guid %S, guid to open %S\n", FUNCTION_NAME, volume.c_str(), guid, guidtoopen.c_str());
	HANDLE  h = CreateFileW(guidtoopen.c_str(),GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if( h == INVALID_HANDLE_VALUE ) {
		DebugPrintf(SV_LOG_ERROR, "Failed to open volume %S with guid %S with error %d while trying to find signature offset\n", 
                                  volume.c_str(), guidtoopen.c_str(), GetLastError());
        return rval;
    }

    int EXPECTEDEXTENTS = 1;
    bool bexit = false;
    VOLUME_DISK_EXTENTS *vde = NULL;
    unsigned int volumediskextentssize;
    DWORD bytesreturned;
    DWORD err;

    do {
        if (vde) {
            free(vde);
            vde = NULL;
        }
        int nde;
        INM_SAFE_ARITHMETIC(nde = InmSafeInt<int>::Type(EXPECTEDEXTENTS) - 1, INMAGE_EX(EXPECTEDEXTENTS))
        INM_SAFE_ARITHMETIC(volumediskextentssize = sizeof (VOLUME_DISK_EXTENTS) + (nde * InmSafeInt<size_t>::Type(sizeof (DISK_EXTENT))), INMAGE_EX(sizeof (VOLUME_DISK_EXTENTS))(nde)(sizeof (DISK_EXTENT)))
        vde = (VOLUME_DISK_EXTENTS *) calloc(1, volumediskextentssize);
        if (NULL == vde) {
            DebugPrintf(SV_LOG_ERROR, "Failed to allocate %u bytes for %d expected extents, "
                                      "for IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS while finding signature offset\n",
                                      volumediskextentssize, EXPECTEDEXTENTS);
            break;
        }

        bytesreturned = 0;
        err = 0;
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, volumediskextentssize, &bytesreturned, NULL)) {
			std::set<DWORD> disknumbers;
			LARGE_INTEGER startingoffset;
			DWORD disknumber;
            for (DWORD i = 0; i < vde->NumberOfDiskExtents; i++) {
				DISK_EXTENT &de = vde->Extents[i];
				disknumbers.insert(de.DiskNumber);
				if (0 == i) {
					startingoffset = de.StartingOffset;
					disknumber = de.DiskNumber;
				}
			}
			if (disknumbers.size() == 1)
				rval = FillSignatureOffsetFromClusDisk(disknumber, startingoffset, signatureOffset, nElemsInSignatureOffset);
			else if (disknumbers.size() == 0)
				DebugPrintf(SV_LOG_ERROR, "volume %S, guid %S, has no disks\n", volume.c_str(), guidtoopen.c_str());
			else 
				DebugPrintf(SV_LOG_DEBUG, "volume %S cannot be in mscs as this is made from more than one disk\n", volume.c_str());
			
            bexit = true;
        } else if ((err=GetLastError()) == ERROR_MORE_DATA) {
            DebugPrintf(SV_LOG_DEBUG, "with EXPECTEDEXTENTS = %d, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS says error more data\n", EXPECTEDEXTENTS);
            EXPECTEDEXTENTS = vde->NumberOfDiskExtents;
        } else {
            DebugPrintf(SV_LOG_ERROR, "IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed on volume %S, guid %S, with error %d, " 
                                      "while trying to find signature offset\n", volume.c_str(), guidtoopen.c_str(), err);
            bexit = true;
        }
    } while (!bexit);

    if (vde) {
        free(vde);
        vde = NULL;
    }
	CloseHandle(h);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rval;
}


bool FillSignatureOffsetFromClusDisk(const DWORD &disknumber, const LARGE_INTEGER &startingoffset, byte *signatureOffset, const size_t &nElemsInSignatureOffset)
{
	bool rval = false;
	std::stringstream ss;
	ss << "disknumber = " << disknumber << ", startingoffset = " << startingoffset.QuadPart;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n", FUNCTION_NAME, ss.str().c_str());

	static std::map<DWORD, std::wstring> disknumbertosignature;
	if (0 == disknumbertosignature.size())
		FillDiskNumberToSignatureFromClusDisk(disknumbertosignature);

	/*
	(03-08-2013 17:32:01):   DEBUG  3152 2328 Adding key e70ebb640044000200000000, drive letter 
	(03-08-2013 17:32:01):   DEBUG  3152 2328 diskname to signature map:
	(03-08-2013 20:38:50):   DEBUG  3356 2224 
	1 --> 86CF88C2
	2 --> 86CF88C0
	3 --> FC5A92D3
	4 --> D3A28996
	5 --> 64BB0EE7   
	*/
	std::map<DWORD, std::wstring>::const_iterator it = disknumbertosignature.find(disknumber);
	if (disknumbertosignature.end() != it) {
		const std::wstring &signature = it->second;
		DebugPrintf("For %s, signature is %S\n", ss.str().c_str(), signature.c_str());
		union usignature {
			unsigned __int32 n;
			byte b[4];
		} usig;
		std::wistringstream wss(signature);
		wss >> std::hex;
		wss >> usig.n;
		DebugPrintf(SV_LOG_DEBUG, "signature in decimal is %u\n", usig.n);

		union ustartoffset {
			__int64 n;
			byte b[8];
		} uso;
		uso.n = startingoffset.QuadPart;

		/* copy signature */
		int i;
		for (i = 0; i < 4; i++)
			signatureOffset[i] = usig.b[i]; 

		/* copy start offset */
		for (int j = 0; j < 8; j++, i++)
			signatureOffset[i] = uso.b[j];

		rval = true;
	} else
		DebugPrintf(SV_LOG_DEBUG, "%s not found in ClusDisk\n", ss.str().c_str());

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rval;
}


void FillDiskNumberToSignatureFromClusDisk(std::map<DWORD, std::wstring> &disknumbertosignature)
{
	HKEY hKey;
	const WCHAR *clusdiskkey = L"SYSTEM\\CurrentControlSet\\Services\\ClusDisk\\Parameters\\Signatures";

    long status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, clusdiskkey, 0, KEY_READ, &hKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed to open key %S with error %d\n", clusdiskkey, status);
		return;
    }
    boost::shared_ptr<void> hKeyGguard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, hKey));

    DWORD index = 0;
    std::wstring signature;
    while (ERROR_SUCCESS == EnumerateKey(hKey, index, signature)) {
		DebugPrintf(SV_LOG_DEBUG, "Found signature from clus disk as %S\n", signature.c_str());
		LoadDiskNumberToSignatureFromClusDisk(hKey, signature, disknumbertosignature);
        ++index;
    }

	DebugPrintf(SV_LOG_DEBUG, "disk number to signature map:\n");
	std::wstringstream wss;
	for (std::map<DWORD, std::wstring>::const_iterator it = disknumbertosignature.begin(); it != disknumbertosignature.end(); it++)
		wss << it->first << " --> " << it->second << '\n';
	DebugPrintf(SV_LOG_DEBUG, "%S", wss.str().c_str());
}


void LoadDiskNumberToSignatureFromClusDisk(HKEY hKey, const std::wstring &signature, std::map<DWORD, std::wstring> &disknumbertosignature)
{
	HKEY signatureKey;
    LONG status = RegOpenKeyW(hKey, signature.c_str(), &signatureKey);
    if (ERROR_SUCCESS != status) {
		DebugPrintf(SV_LOG_ERROR, "Failed to open key %S with error %d in ClusDisk Key\n", signature.c_str(), status);
		return;
    }
    boost::shared_ptr<void> signatureKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, signatureKey));

    std::wstring diskname;
    if(!::QueryString(signatureKey, ClusDiskName, diskname)) {
		DebugPrintf(SV_LOG_ERROR, "Failed to query %S in ClusDisk signature key %S\n", ClusDiskName, signature.c_str());
        return;
    }

	if (!diskname.empty()) {
		DebugPrintf(SV_LOG_DEBUG, "Got diskname %S for ClusDisk signature %S\n", diskname.c_str(), signature.c_str());
		DWORD disknumber = 0;
		std::wstring::const_reverse_iterator it = diskname.rbegin();
		for (std::wstring::size_type i = 0; it != diskname.rend(); it++, i++) {
			if (iswdigit(*it))
				disknumber += (boost::lexical_cast<DWORD>(*it)) * ((DWORD)pow((double)10, (double)i));
			else
				break;
		}
		if (it != diskname.rbegin())
			disknumbertosignature.insert(std::make_pair(disknumber, signature));
		else
			DebugPrintf(SV_LOG_ERROR, "Got diskname %S for ClusDisk signature %S, is invalid\n", diskname.c_str(), signature.c_str());
	}
}


// for geting mount points under a mount point
bool GetMountPointSignatureOffsetForMountPoint(HKEY hKey, std::wstring const & name, SignatureOffsetMountPoints_t & somp)
{
    std::wstring checkName(name + L"\\");

    return GetMountPointSignatureOffset(hKey, checkName, somp);
}

// ----------------------------------------------------------------------------
//  ResourceTrackingInfo
// -----------------------------------------------------------------------------
bool ResourceTrackingInfo::operator==(ResourceTrackingInfo const & rhs) const
{
    return (m_ResourceType == rhs.m_ResourceType
            && m_Name == rhs.m_Name
            && m_Id == rhs.m_Id
            && m_Volumes == rhs.m_Volumes
            && m_Dependencies == rhs.m_Dependencies);
}

bool ResourceTrackingInfo::IsPhysicalDisk() const
{
    return (0 == wcscmp(ResourceTypePhysicalDisk, &m_ResourceType[0])
            || 0 == wcscmp(ResourceTypeVeritasVolumeManagerDiskGroup, &m_ResourceType[0]));
}

bool ResourceTrackingInfo::IsVeritasVolumeManagerDiskGroup()
{
    return (0 == wcscmp(ResourceTypeVeritasVolumeManagerDiskGroup, &m_ResourceType[0]));
}

bool ResourceTrackingInfo::IsSqlServer() const
{
    return (0 == wcscmp(ResourceTypeSqlServer, &m_ResourceType[0]));
}

bool ResourceTrackingInfo::IsExchangeSystemAttendant() const
{
    return (0 == wcscmp(ResourceTypeExchangeSystemAttendant, &m_ResourceType[0]));
}

bool ResourceTrackingInfo::IsNetworkName() const
{
    return (0 == wcscmp(ResourceTypeNetworkName, &m_ResourceType[0]));
}


bool ResourceTrackingInfo::GoingOffline() const
{
    HRESOURCE hResource = OpenClusterResource(g_hCluster, m_Name.c_str());

    if (0 == hResource) {
        return false;
    }

    boost::shared_ptr<void> hResourceGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &CloseClusterResource, hResource));

    DWORD ownerLength = 255;
    DWORD groupLength = 255;
    std::vector<WCHAR> owner(ownerLength);
    std::vector<WCHAR> group(groupLength);

    CLUSTER_RESOURCE_STATE resourceState = GetClusterResourceState(hResource, &owner[0], &ownerLength, &group[0], &groupLength);

    switch (resourceState) {
        case ClusterResourceOffline:
        case ClusterResourceFailed:
        case ClusterResourceOfflinePending:
        case ClusterResourceStateUnknown:
            return true;
        default:
            return false;
    }
}

bool ResourceTrackingInfo::Online() const
{
    HRESOURCE hResource = OpenClusterResource(g_hCluster, m_Name.c_str());

    if (0 == hResource) {
        return false;
    }

    boost::shared_ptr<void> hResourceGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &CloseClusterResource, hResource));

    DWORD ownerLength = 255;
    DWORD groupLength = 255;
    std::vector<WCHAR> owner(ownerLength);
    std::vector<WCHAR> group(groupLength);

    CLUSTER_RESOURCE_STATE resourceState = GetClusterResourceState(hResource, &owner[0], &ownerLength, &group[0], &groupLength);

    return (ClusterResourceOnline == resourceState);
}


// for performing ClusterResourceControl for the given request
bool ResourceTrackingInfo::ResourceControl(WCHAR_VECTOR& data, DWORD& size, DWORD request)
{
    DWORD bytesReturned = (0 == size ? MAX_CHARS : size);

    DWORD status;

    do {
        data.resize(bytesReturned);
        status = ClusterResourceControl(m_hResource, NULL, request, NULL, 0, &data[0], static_cast<DWORD>(data.size()), &bytesReturned);
    } while (ERROR_MORE_DATA == status);

    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::ResourceControl ClusterResourceControl: %d\n", status);
        return false;
    }

    size = bytesReturned;

    return true;
}

bool ResourceTrackingInfo::GetVirtualServerName(std::wstring & name)
{
    m_hResource = OpenClusterResource(g_hCluster, m_Name.c_str());

    if (0 == m_hResource) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetVirtualServerName OpenClusterResource: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(),&CloseClusterResource, m_hResource));

    DWORD size = MAX_CHARS;

    WCHAR_VECTOR virualServerName(size);

    ResourceControl(virualServerName, size, CLUSCTL_RESOURCE_GET_NETWORK_NAME);

    name = &virualServerName[0];

    return true;
}


void ResourceTrackingInfo::SetResourceType()
{
    DWORD size = MAX_CHARS;

    m_ResourceType.resize(size);

    ResourceControl(m_ResourceType, size, CLUSCTL_RESOURCE_GET_RESOURCE_TYPE);
}

bool ResourceTrackingInfo::GetDependsOn()
{
    DWORD enumResourceType = CLUSTER_RESOURCE_ENUM_DEPENDS;

    HRESENUM hResourceEnum = ClusterResourceOpenEnum(m_hResource, enumResourceType);
    if (0 == hResourceEnum) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDependsOn ClusterResourceOpenEnum: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> hResourceEnumGuard(static_cast<void*>(0), boost::bind(boost::type<DWORD>(), &ClusterResourceCloseEnum, hResourceEnum));

    DWORD resourceIndex = 0;
    DWORD dependsOnLength = MAX_CHARS;
    DWORD status;

    std::vector<WCHAR> dependsOn;

    do {
        dependsOn.resize(dependsOnLength);
        dependsOn[0] = '\0';
        status = ClusterResourceEnum(hResourceEnum, resourceIndex, &enumResourceType, &dependsOn[0], &dependsOnLength);
        switch (status) {
            case ERROR_SUCCESS:
                break;
            case ERROR_NO_MORE_ITEMS:
                return true;
            case ERROR_MORE_DATA:
                // returned length does not include NULL terminator, but needs to be included when calling ClusterResourceEnum
                ++dependsOnLength;
                status = ERROR_SUCCESS;
                continue;
            default:
                DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDependsOn ClusterResourceEnum: %d", status);
                return false;
        }

        ResourceTrackingInfo resource;

        bool bnameidtypeset = resource.SetNameIdType(&dependsOn[0]);
        if (false == bnameidtypeset)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDependsOn resource.SetNameIdType\n");
            return false;
        }

        if (resource.IsPhysicalDisk()) {
            m_Dependencies.insert(std::make_pair(resource.Id(), resource.Name()));
        }

        if (dependsOnLength < MAX_CHARS) {
            dependsOnLength = MAX_CHARS;
        }

        ++resourceIndex;
    } while (ERROR_SUCCESS == status);

    return true;
}

bool ResourceTrackingInfo::GetDiskInfo()
{
    // this will be set to false if either GetMountVolumeInfo or GetDiskVolumeInfo already did the veritas check
    bool needToCollectVeritasVolumes = IsVeritasVolumeManagerDiskGroup();
    if (GetMountVolumeInfo(needToCollectVeritasVolumes)) {
        DebugPrintf(SV_LOG_DEBUG, "For resource name %S, Id %S, GetMountVolumeInfo returned true\n", m_Name.c_str(), m_Id.c_str());
        return true;
    }
    g_UseGuidAsSignatureOffset = true;
    if (!GetDiskVolumeInfo(needToCollectVeritasVolumes)) {
        if (needToCollectVeritasVolumes) {
            return GetVeritasCluterVolumeInfo();
        }
        return false;
    }
    return true;        
}

std::string ResourceTrackingInfo::GetVeritasVxdgCmd()
{
    HKEY key;
    long result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\VERITAS\\Volume Manager", 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &key);
    if (ERROR_SUCCESS != result) {
        return std::string();
    }
    ON_BLOCK_EXIT(&RegCloseKey, key);    
    char buffer[MAX_PATH];
    DWORD size;
    DWORD type = REG_SZ;
    result = RegQueryValueExA(key, "INSTALLDIR", 0, &type, (BYTE*)buffer, &size);
    if (ERROR_SUCCESS != result) {
        return std::string();
    }
    std::string cmd(buffer);
    cmd += "vxdg.exe";
    return cmd;
}

bool ResourceTrackingInfo::GetVeritaslDiskGroupInfo(std::string& dskGroupInfo)
{
    try {
        // FIXME: should use wide char but FioPipe does not support widechar yet
        std::string cmd(GetVeritasVxdgCmd());
        if (cmd.empty()) {
            // assume default location
            cmd = "C:\\Program Files\\Veritas\\Veritas Volume Manager\\vxdg.exe";
        }
        std::string dg("-g");
        std::stringstream name;
        std::locale loc;
        std::wstring::const_iterator it(m_Name.begin());
        std::wstring::const_iterator endIt(m_Name.end());
        for (/* empty */ ; it != endIt; ++it) {
            name << std::use_facet<std::ctype<wchar_t> >(loc).narrow(*it, '.');
        }
        dg += name.str();  
        FIO::cmdArgs_t vargs;
        vargs.push_back(cmd.c_str());
        vargs.push_back(dg.c_str());
        vargs.push_back("dginfo");
        vargs.push_back((char*)0);
        FIO::FioPipe fioPipe(vargs, true); // use true to tie stderr to stdout
        long bytes;
        char buffer[1024];
        do {
            while ((bytes = fioPipe.read(buffer, sizeof(buffer), false)) > 0) {
                dskGroupInfo.append(buffer, bytes);
            }
        } while (FIO::FIO_WOULD_BLOCK == fioPipe.error() && dskGroupInfo.empty());
        return true;
    } catch (ErrorException const& e) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get veritas group disk infor for resource name %S, Id %S: %S\n", m_Name.c_str(), m_Id.c_str(), e.what());
        return false;
    }
}

bool ResourceTrackingInfo::AddVeritasClusterInfo(std::string const& dskGroupInfo)
{
    boost::char_separator<char> sep("\r\f\n");
    tokenizer_t tokens(dskGroupInfo, sep);
    tokenizer_t::iterator iter(tokens.begin());
    tokenizer_t::iterator iterEnd(tokens.end());
    if (iter == iterEnd || !boost::algorithm::icontains((*iter), m_Name)) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get veritas group disk infor for resource name %S, Id %S: %S\n", m_Name.c_str(), m_Id.c_str(), dskGroupInfo.c_str());
        return false;
    }
    bool ok = false;
    std::string dskGroupId;
    // get the info we care about
    for (/* empty*/; iter != iterEnd; ++iter) {
        if (boost::algorithm::icontains((*iter), "DiskGroup ID")) {
            std::string::size_type idx = (*iter).find_first_of(":");
            if (std::string::npos != idx) {
                ++idx;
                while (idx < (*iter).size() && isspace((*iter)[idx])) {
                    ++idx;
                }
                dskGroupId = (*iter).substr(idx);
            }
        } else if (boost::algorithm::icontains((*iter), "Device") && boost::algorithm::icontains((*iter), m_Name)) {
            // get drive letter
            std::string::size_type idx = (*iter).find_last_of("(");
            if (std::string::npos != idx) {
                ++idx;
                std::wstringstream signatureOffset;
                signatureOffset <<  (*iter)[idx] << L"-" << dskGroupId.c_str();
                std::wstringstream drive;
                drive << (*iter)[idx];
                m_Volumes.insert(std::make_pair(signatureOffset.str(), drive.str()));
                ok = true;
            }
        }
    }
    if (!ok) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get veritas group disk infor for resource name %S, Id %S: %S\n", m_Name.c_str(), m_Id.c_str(), dskGroupInfo.c_str());
    }
    return ok;
}

bool ResourceTrackingInfo::GetVeritasCluterVolumeInfo()
{
    std::string dskGroupInfo;
    if (!GetVeritaslDiskGroupInfo(dskGroupInfo)) {
        return false;
    }
    if (dskGroupInfo.empty()) {
        DebugPrintf(SV_LOG_ERROR, "Failed to veritas disk group inforamtion for %S\n", m_Name.c_str());
        return false;
    }
    return AddVeritasClusterInfo(dskGroupInfo);
}

bool ResourceTrackingInfo::GetMountVolumeInfo(bool& needToCollectVeritasVolumes)
{
    DWORD size = 1024;
    WCHAR_VECTOR privateProperties(size);

    if (!ResourceControl(privateProperties, size, CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES)) {
        return false;
    }

    DWORD dataSize;
    byte* data;

    DWORD status = ResUtilFindBinaryProperty(&privateProperties[0], size, L"MountVolumeInfo", &data, &dataSize);
    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND == status) {
            DebugPrintf(SV_LOG_WARNING, "Warning ResourceTrackingInfo::GetDiskInfo ResUtilFindDwordProperty MountVolumeInfo not found for %S. This is OK only if this is a newly added resource and the disks have not yet been added\n",
                        m_Name.c_str());
        } else {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDiskInfo ResUtilFindDwordProperty MountVolumeInfo: %d\n", status);
        }
        return false;
    }
    if (0 == dataSize) {
        // veritas disk resource that does not add the disk info directly to cluster resource info
        if (IsVeritasVolumeManagerDiskGroup()) {
            needToCollectVeritasVolumes = false; // caller does not need collect veritas
            return GetVeritasCluterVolumeInfo();
        }
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDiskInfo ResUtilFindDwordProperty MountVolumeInfo found for %S, but has no data\n", m_Name.c_str());
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<HLOCAL>(),&LocalFree, data));

    MountVolumeInfoHeader* header = reinterpret_cast<MountVolumeInfoHeader*>(data);

    std::vector<WCHAR> signatureOffset((DISK_SIGNATURE_LENGTH + DISK_OFFSET_LENGTH) * 2 + 1);

    // HACK: temporary until we have full veritas support
    if (IsVeritasVolumeManagerDiskGroup()) {
        signatureOffset.resize((DISK_SIGNATURE_LENGTH + VERITAS_DISK_OFFSET_LENGTH) * 2 + 1);
    }

    _snwprintf(&signatureOffset[0], signatureOffset.size(),
               L"%2.2x%2.2x%2.2x%2.2x",
               header->m_Signature[0], header->m_Signature[1], header->m_Signature[2], header->m_Signature[3]);

    WCHAR driveLetter[2];

    MountVolumeInfo* volumeInfo = reinterpret_cast<MountVolumeInfo*>(&data[sizeof(MountVolumeInfoHeader)]);
    for (unsigned int i = 0; i < header->m_VolumeCount; ++i) {
        _snwprintf(&signatureOffset[DISK_SIGNATURE_LENGTH * 2], signatureOffset.size() - (DISK_SIGNATURE_LENGTH * 2),
                   L"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                   volumeInfo->m_StartOffset[0], volumeInfo->m_StartOffset[1], volumeInfo->m_StartOffset[2], volumeInfo->m_StartOffset[3],
                   volumeInfo->m_StartOffset[4], volumeInfo->m_StartOffset[5], volumeInfo->m_StartOffset[6], volumeInfo->m_StartOffset[7]);

        // HACK: temporary until we have full veritas support
        if (IsVeritasVolumeManagerDiskGroup()) {
            _snwprintf(&signatureOffset[(DISK_SIGNATURE_LENGTH + DISK_SIGNATURE_LENGTH) * 2], signatureOffset.size() - ((DISK_SIGNATURE_LENGTH + DISK_SIGNATURE_LENGTH) * 2),
                       L"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                       volumeInfo->m_Length[0], volumeInfo->m_Length[1], volumeInfo->m_Length[2], volumeInfo->m_Length[3],
                       volumeInfo->m_Length[4], volumeInfo->m_Length[5], volumeInfo->m_Length[6], volumeInfo->m_Length[7],
                       volumeInfo->m_Number[0], volumeInfo->m_Number[1], volumeInfo->m_Number[2], volumeInfo->m_Number[3]);
        }

        _snwprintf(driveLetter, NELEMS(driveLetter), L"%c", volumeInfo->m_DriveLetter);
        DebugPrintf(SV_LOG_DEBUG, "Adding key %S, drive letter %S\n", &signatureOffset[0], driveLetter);

        // NOTE: since Volumes_t is a map it is ok to just insert here with out checking if it exists
        // if it every gets switched to a multimap so that we can send both drive letters and mount points for
        // the same signature offset, then a test will be needed to make sure we don't add the same entry mulitple times
        m_Volumes.insert(std::make_pair(&signatureOffset[0], driveLetter));
        ++volumeInfo;
    }
    return true;
}

bool ResourceTrackingInfo::GetDiskSignatureForGuid(WCHAR const * guid, signatureOffset_t& signatureOffset)
{
    DWORD type = REG_BINARY;
    DWORD valueSize = 256;

    std::vector<byte> value(valueSize);

    HKEY hKey;

    long status = RegOpenKeyExW(HKEY_LOCAL_MACHINE , L"SYSTEM\\MountedDevices", 0, KEY_QUERY_VALUE, &hKey);

    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetMountPointSignatureOffset RegOpenKeyEx SYSTEM\\MountedDevices: %d\n", status);
        }
        return false;
    }

    boost::shared_ptr<void> hKeyGguard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, hKey));

    do {
        status = RegQueryValueExW(hKey, guid, 0, &type, &value[0], &valueSize);
        switch (status) {
            case ERROR_SUCCESS:
                break;

            case ERROR_MORE_DATA:
                value.resize(valueSize + 1);
                break;

            case ERROR_NO_MORE_ITEMS:
            default:
                DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetMountPointSignatureOffset RegQueryValueEx SYSTEM\\MountedDevices for %S: %d\n", guid, status);
                return false;
        }
    } while (ERROR_MORE_DATA == status);

    signatureOffset.resize(SIGNATURE_OFFSET_LENGTH*2);
	_snwprintf(&signatureOffset[0], SIGNATURE_OFFSET_LENGTH*2,
               L"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
               value[0], value[1], value[2], value[3], value[4], value[5],
               value[6], value[7], value[8], value[9], value[10], value[11]);

    return true;
}

bool ResourceTrackingInfo::GetDiskVolumeInfo(bool& needToCollectVeritasVolumes)
{
    DWORD size = 1024;
    WCHAR_VECTOR privateProperties(size);

    if (!ResourceControl(privateProperties, size, CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES)) {
        return false;
    }

    DWORD dataSize;
    byte* data;

    DWORD status = ResUtilFindBinaryProperty(&privateProperties[0], size, L"DiskVolumeInfo", &data, &dataSize);
    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND == status) {
            DebugPrintf(SV_LOG_WARNING,
                        "Warning ResourceTrackingInfo::GetDiskInfo ResUtilFindDwordProperty "
                        "DiskVolumeInfo not found for %S. This is OK only if this is a newly added resource "
                        "and the disks have not yet been added\n", m_Name.c_str());
        } else {
            DebugPrintf(SV_LOG_ERROR,
                        "Failed ResourceTrackingInfo::GetDiskInfo ResUtilFindBinaryProperty DiskVolumeInfo for %S: %d\n"
                        , m_Name.c_str(), status);
        }
        return false;
    }

	std::stringstream ssds;
	ssds << dataSize;
	DebugPrintf(SV_LOG_DEBUG, "dataSize = %s, sizeof DiskVolumeInfoHeader = %u, sizeof DiskVolumeInfo = %u\n", ssds.str().c_str(), 
		sizeof(DiskVolumeInfoHeader), sizeof(DiskVolumeInfo));

    if (0 == dataSize) {
        // veritas disk resource that does not add the disk info directly to cluster resource info
        if (IsVeritasVolumeManagerDiskGroup()) {
            needToCollectVeritasVolumes = false; // caller does not need to collect veritas
            return GetVeritasCluterVolumeInfo();
        }
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetDiskInfo ResUtilFindDwordProperty DiskVolumeInfo found for %S, but has no data\n", m_Name.c_str());
        return false;
    }


    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<HLOCAL>(),&LocalFree, data));

    DiskVolumeInfoHeader* header = reinterpret_cast<DiskVolumeInfoHeader*>(data);
    DebugPrintf(SV_LOG_DEBUG, "DiskVolumeInfoHeader:\n");
    PrintDiskVolumeInfoHeader(header);

	if (0 == header->m_VolumeCount) {
		DebugPrintf(SV_LOG_WARNING, "Number of volumes is zero for %S. No volumes collected for this.\n", m_Name.c_str());
		return true;
	}

    unsigned int diskInfoSize;
	unsigned int nBytesForDiskVolumeInfoEntries;
	nBytesForDiskVolumeInfoEntries = dataSize - sizeof(DiskVolumeInfoHeader);
	diskInfoSize = (unsigned int)(nBytesForDiskVolumeInfoEntries / header->m_VolumeCount);
    DebugPrintf(SV_LOG_DEBUG, "nBytesForDiskVolumeInfoEntries = %u, each diskInfoSize = %u\n", nBytesForDiskVolumeInfoEntries, diskInfoSize);

    WCHAR driveLetter[2] = { '\0', '\0' };

    // TODO: may need some changes here to support veritas (see verits hack in GetMountVolumeInfo)
    DiskVolumeInfo* diskInfo = reinterpret_cast<DiskVolumeInfo*>(&data[sizeof(DiskVolumeInfoHeader)]);
    for (unsigned int i = 0; i < header->m_VolumeCount; ++i) {
        DebugPrintf(SV_LOG_DEBUG, "DiskInfo:\n");
        PrintDiskVolumeInfo(diskInfo);
        // need the disk signature and have the disk guid in weird reverse order
        // given the following guid
        //    1bac6d6d-0b5d-11da-b162-00016c264d65
        // w2k8 cluster DiskVolumeInfo stores that guid as
        //    6d6dac1b-5d0b-da11-b162-00016c264d65
        //
        // note: how the frist 3 parts have are each in reverse order
        // while the last 2 parts in normal order
        // note sure why it is done that way but we need it in the correct order to get the disk signature
        std::vector<WCHAR> guid(strlen("\\??\\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}")+ 1);
        WCHAR signatureOffset[MAX_SIGNATURE_OFFSET_LENGTH + 1];
        _snwprintf(&signatureOffset[0], NELEMS(signatureOffset),
                   L"%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                   diskInfo->m_Guid[3], diskInfo->m_Guid[2], diskInfo->m_Guid[1], diskInfo->m_Guid[0],
                   diskInfo->m_Guid[5], diskInfo->m_Guid[4],
                   diskInfo->m_Guid[7], diskInfo->m_Guid[6],
                   diskInfo->m_Guid[8], diskInfo->m_Guid[9],
                   diskInfo->m_Guid[10], diskInfo->m_Guid[11], diskInfo->m_Guid[12], diskInfo->m_Guid[13], diskInfo->m_Guid[14], diskInfo->m_Guid[15]);

        char drive = ' ';
        if (0 != diskInfo->m_Drive) {
            for (unsigned int i = 0; i < 26; i++ ) {
                if (((1 << i) == diskInfo->m_Drive)) {
                    drive = 'A' + i;
                    break;
                }
            }
        }

        if (' ' != drive) {
            _snwprintf(driveLetter, NELEMS(driveLetter), L"%c", drive);
        }
        DebugPrintf(SV_LOG_DEBUG, "For drive %c, adding key %S, drive letter %S\n", drive,
                    signatureOffset, driveLetter);

        // NOTE: since Volumes_t is a map it is ok to just insert here with out checking if it exists
        // if it every gets switched to a multimap so that we can send both drive letters and mount points for
        // the same signature offset, then a test will be needed to make sure we don't add the same entry
        // mulitple times
        m_Volumes.insert(std::make_pair(signatureOffset, driveLetter));
        driveLetter[0] = L'\0';

        diskInfo = (DiskVolumeInfo*)(((char*)diskInfo) + diskInfoSize);
    }
    return true;
}

bool ResourceTrackingInfo::GetMountPointSignatureOffset(HKEY hKey, std::wstring const & name, GroupTrackingInfo* group)
{
    SignatureOffsetMountPoints_t somp;

    if (::GetMountPointSignatureOffset(hKey, name, somp)) {
        SignatureOffsetMountPoints_t::iterator iter(somp.begin());
        SignatureOffsetMountPoints_t::iterator endIter(somp.end());
        for (/* empty */; iter != endIter; ++iter) {
            group->AddMountPointInfo((*iter).first, (*iter).second);
        }
    }

    return true;
}

bool ResourceTrackingInfo::GetMountPointSignatureOffsetForVolumes(HKEY hKey, GroupTrackingInfo* group)
{
    std::wstring driveLetter;

    Volumes_t::iterator iter(m_Volumes.begin());
    Volumes_t::iterator endIter(m_Volumes.end());
    for (/* empty */; iter != endIter; ++iter) {
        // if there is no drive letter or it has already
        // been replaced with a mount point skip it
        if ((*iter).second.empty() || (*iter).second.length() > 1) {
            DebugPrintf(SV_LOG_DEBUG, "For resource name %S, Id %S, is volume name empty: %d for key: %S\n",
                        m_Name.c_str(), m_Id.c_str(),
                        (*iter).second.empty(), (*iter).first.c_str());
            continue;
        }

        driveLetter = (*iter).second + L":\\";

        GetMountPointSignatureOffset(hKey, driveLetter, group);
    }

    return true;
}

template <class Container>
bool ResourceTrackingInfo::SaveData(HKEY hKey, Container data)
{
    HKEY trackingKey;

    Container::iterator iter(data.begin());
    Container::iterator endIter(data.end());
    for (/* empty */; iter != endIter; ++iter) {
        long status = RegCreateKeyExW(hKey, (*iter).first.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
        if (ERROR_SUCCESS != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::SaveData() creating/opening %S for %S: %d\n",
                        (*iter).first.c_str(), (*iter).second.c_str(), status);
            return false;
        }

        boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

        status = RegSetValueExW(trackingKey, ClusterTrackingNameValueName, 0,
                                REG_SZ, reinterpret_cast<const BYTE*>((*iter).second.c_str()),
                                static_cast<DWORD>((*iter).second.length() * sizeof(WCHAR)));
        if (ERROR_SUCCESS != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::SaveData() RegSetValueEx %S=%S: %d\n",
                        ClusterTrackingNameValueName, (*iter).second.c_str(), status);
            return false;
        }
    }

    return true;
}

template <class Container>
bool ResourceTrackingInfo::DeleteData(HKEY hKey, Container data)
{
    Container::iterator iter(data.begin());
    Container::iterator endIter(data.end());
    for (/* empty */; iter != endIter; ++iter) {

        long status = RegDeleteKeyW(hKey, (*iter).first.c_str());
        if (ERROR_SUCCESS != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteData() deleteing %S for %S: %d\n",
                        (*iter).first.c_str(), (*iter).second.c_str(), status);
            return false;
        }
    }

    data.clear();

    return true;
}

bool ResourceTrackingInfo::SaveVolumes(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, ClusterTrackingVolumesValueName, 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::SaveVolumes() creating/opening %S for %S: %d\n",
                    ClusterTrackingVolumesValueName, m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    return SaveData(trackingKey, m_Volumes);
}

bool ResourceTrackingInfo::DeleteVolumes(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, ClusterTrackingVolumesValueName, 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteVolumes() opening %S for %S: %d\n",
                    ClusterTrackingVolumesValueName, m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DeleteData(trackingKey, m_Volumes);

    status = RegDeleteKeyW(hKey, ClusterTrackingVolumesValueName);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteVolumes() deleting %S for %S: %d\n",
                    ClusterTrackingVolumesValueName, m_Name.c_str(), status);
        return false;
    }

    return true;
}

bool ResourceTrackingInfo::SaveDependencies(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, ClusterTrackingDependenciesValueName, 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::SaveDependencies() creating/opening %S for %S: %d\n",
                    ClusterTrackingDependenciesValueName, m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    return SaveData(trackingKey, m_Dependencies);
}

bool ResourceTrackingInfo::DeleteDependencies(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, ClusterTrackingDependenciesValueName, 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteDependencies() opening %S for %S: %d\n",
                    ClusterTrackingDependenciesValueName, m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DeleteData(trackingKey, m_Dependencies);

    status = RegDeleteKeyW(hKey, ClusterTrackingDependenciesValueName);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteDependencies() deleteing %S for %S: %d\n",
                    ClusterTrackingDependenciesValueName, m_Name.c_str(), status);
        return false;
    }

    return true;
}

bool ResourceTrackingInfo::DeleteResourceDependency(HKEY hKey, ResourceTrackingInfo::Dependencies_t const & dependencies)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, m_Id.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteResourceDependency() opening %S for %S: %d\n",
                    m_Id.c_str(), m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    HKEY resourceKey;
    status = RegCreateKeyExW(trackingKey, ClusterTrackingDependenciesValueName, 0, 0, 0, KEY_ALL_ACCESS, 0, &resourceKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::DeleteResourceDependency() opening %S for %S: %d\n",
                    ClusterTrackingDependenciesValueName, m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> resourceKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, resourceKey));

    ResourceTrackingInfo::Dependencies_t::const_iterator iter(dependencies.begin());
    ResourceTrackingInfo::Dependencies_t::const_iterator endIter(dependencies.end());
    for (/* empty */; iter != endIter; ++iter) {
        status = RegDeleteKeyW(resourceKey, (*iter).first.c_str());
        m_Dependencies.erase((*iter).first);
    }

    return true;
}

bool ResourceTrackingInfo::Save(HKEY hKey)
{
    HKEY trackingKey;

    if (!::SaveNameId(hKey, *this, trackingKey)) {
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    SaveVolumes(trackingKey);
    SaveDependencies(trackingKey);

    return true;
}

bool ResourceTrackingInfo::Delete(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, m_Id.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::Delete() creating/opening %S for %S: %d\n",
                    m_Id.c_str(), m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DeleteVolumes(trackingKey);
    DeleteDependencies(trackingKey);

    status = RegDeleteKeyW(hKey, m_Id.c_str());

    return true;
}

template <class Container>
bool ResourceTrackingInfo::LoadData(HKEY hKey, std::wstring id, Container & data)
{
    HKEY trackingKey;
    LONG status = RegOpenKeyW(hKey, id.c_str(), &trackingKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::LoadData() opening %S: %d\n",
                    id.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    std::wstring name;

    if(!::QueryString(trackingKey, ClusterTrackingNameValueName, name)) {
        return false;
    }

    data.insert(std::make_pair(id, name));

    return true;

}
bool ResourceTrackingInfo::LoadVolumes(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegOpenKeyW(hKey, ClusterTrackingVolumesValueName, &trackingKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::LoadVolumes() opening %S: %d\n",
                    ClusterTrackingVolumesValueName, status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DWORD index = 0;

    std::wstring id;

    while (ERROR_SUCCESS == EnumerateKey(trackingKey, index, id)) {
        LoadData(trackingKey, id, m_Volumes);
        ++index;
    }

    return true;
}

bool ResourceTrackingInfo::LoadDependencies(HKEY hKey)
{
    HKEY trackingKey;

    long status = RegOpenKeyW(hKey, ClusterTrackingDependenciesValueName, &trackingKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::LoadDependencies() opening %S: %d\n",
                    ClusterTrackingDependenciesValueName, status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DWORD index = 0;

    std::wstring id;

    while (ERROR_SUCCESS == EnumerateKey(trackingKey, index, id)) {
        LoadData(trackingKey, id, m_Dependencies);
        ++index;
    }

    return true;
}

bool ResourceTrackingInfo::Load(HKEY hKey, std::wstring id)
{
    HKEY trackingKey;

    if (!::LoadNameId(hKey, id, *this, trackingKey)) {
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    if (!LoadVolumes(trackingKey)) {
        return false;
    }

    return LoadDependencies(trackingKey);
}

void ResourceTrackingInfo::FormatGuid(std::wstring const& guid, std::wstring& formattedGuid)
{
    formattedGuid += L"\\??\\Volume{";

    std::wstring::size_type i = 0;
    for ( /* empty*/; i < guid.size() && i < 8 ; ++i) {
        formattedGuid += guid[i];
    }
    formattedGuid += L'-';

    for ( /* empty*/; i < guid.size() && i < 12 ; ++i) {
        formattedGuid += guid[i];
    }
    formattedGuid += L'-';

    for ( /* empty*/; i < guid.size() && i < 16 ; ++i) {
        formattedGuid += guid[i];
    }
    formattedGuid += L'-';

    for ( /* empty*/; i < guid.size() && i < 20 ; ++i) {
        formattedGuid += guid[i];
    }
    formattedGuid += L'-';

    for ( /* empty*/; i < guid.size(); ++i) {
        formattedGuid += guid[i];
    }
    formattedGuid += L'}';
}

void ResourceTrackingInfo::DumpSignatureOffset(char* type, signatureOffset_t& signatureOffset)
{
    std::wstring s;

    signatureOffset_t::iterator iter(signatureOffset.begin());
    signatureOffset_t::iterator iterEnd(signatureOffset.end());
    for (/* empty*/; iter != iterEnd; ++iter) {
        s += *iter;
    }
    DebugPrintf(SV_LOG_INFO, "%s signatureoffset: '%S'\n", type, s.c_str());
}

bool ResourceTrackingInfo::AddMountPointInfoUsingGuid(std::wstring const & guid, std::wstring const & mountPoint)
{
    std::wstring  systemGuid;
    FormatGuid(guid, systemGuid);

    signatureOffset_t systemSignatureOffset;

    DebugPrintf(SV_LOG_INFO, "ResourceTrackingInfo::AddMountPointInfoUsingGuid system guid: '%S'\n", guid.c_str());

    if (!GetDiskSignatureForGuid(systemGuid.c_str(), systemSignatureOffset)) {
        DebugPrintf(SV_LOG_INFO, "ResourceTrackingInfo::AddMountPointInfoUsingGuid system sigoffset not found\n", guid.c_str());
        return false;
    }

    DumpSignatureOffset("system", systemSignatureOffset);

    std::wstring mscsGuid;
    signatureOffset_t mscsSignatureOffset;

    Volumes_t::iterator iter(m_Volumes.begin());
    Volumes_t::iterator endIter(m_Volumes.end());
    for (/* empty */; iter != endIter; ++iter) {
		mscsGuid.clear();
        FormatGuid((*iter).first, mscsGuid);
        DebugPrintf(SV_LOG_INFO, "ResourceTrackingInfo::AddMountPointInfoUsingGuid mscs guid: '%S'\n", mscsGuid.c_str());
        if (!GetDiskSignatureForGuid(mscsGuid.c_str(), mscsSignatureOffset)) {
            DebugPrintf(SV_LOG_INFO, "ResourceTrackingInfo::AddMountPointInfoUsingGuid MSCS sigoffset not found\n");
            continue;
        }

        DumpSignatureOffset("mscs", mscsSignatureOffset);
        if (mscsSignatureOffset == systemSignatureOffset) {
            DebugPrintf(SV_LOG_DEBUG, "For resource name %S, Id %S, in function %s, "
                        "overwriting existing volume %S with %S for key %S\n", m_Name.c_str(), m_Id.c_str(),
                        FUNCTION_NAME, (*iter).second.c_str(), mountPoint.c_str(), (*iter).first.c_str());
            (*iter).second = mountPoint; // override the drive letter
            DebugPrintf(SV_LOG_INFO, "ResourceTrackingInfo::AddMountPointInfoUsingGuid FOUND MATCH\n");
            return true;
        }
    }

    DebugPrintf(SV_LOG_INFO, "ResourceTrackingInfo::AddMountPointInfoUsingGuid no match found\n");

    return false;
}

bool ResourceTrackingInfo::AddMountPointInfo(std::wstring const & signatureOffset, std::wstring const & mountPoint)
{
    Volumes_t::iterator volumeIter = m_Volumes.find(signatureOffset);
    if (volumeIter == m_Volumes.end()) {
        if (g_UseGuidAsSignatureOffset) {
            // sometimes the MSCS guid is not the same as the system guid
            // need to try match the MSCS guid with the system guid
            return AddMountPointInfoUsingGuid(signatureOffset, mountPoint);
        }
        return false;
    }

    // TODO: if we ever want to send both drive letter and mount point then need to make sure that
    // m_Volumes is changed to a multimap and instead of just overriding the drive letter
    // need to check if it is a valid drive letter, if not then continue to override else insert
    // the mount point info e.g. note you may need to test for the existence of the mountpoint before inserting
    // if (InvalidDriveLetter == (*volumeIter).second) {
    //     (*volumeIter).second = mountPoint; // no drive letter so set it to the mount point
    // } else {
    //    m_Volumes.insert(std::make_pair(signatureOffset, mountPoint)); // drive letter exists insert mount point
    // }

    DebugPrintf(SV_LOG_DEBUG, "For resource name %S, Id %S, in function %s, "
		"overwriting existing volume %S with %S for key %S\n", m_Name.c_str(), m_Id.c_str(),
		FUNCTION_NAME, (*volumeIter).second.c_str(), mountPoint.c_str(), (*volumeIter).first.c_str());
    (*volumeIter).second = mountPoint; // override the drive letter

    return true;
}

bool ResourceTrackingInfo::SetId()
{
    DWORD size = MAX_GUID;

    WCHAR_VECTOR guid(size);

    if (!ResourceControl(guid, size, CLUSCTL_RESOURCE_GET_ID)) {
        return false;
    }

    m_Id = &guid[0];

    return true;

}

bool ResourceTrackingInfo::SetNameIdType(WCHAR const * name)
{
    m_hResource = OpenClusterResource(g_hCluster, name);

    if (0 == m_hResource) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::SetId OpenClusterResource: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(),&CloseClusterResource, m_hResource));

    m_Name = name;

    if (!SetId()) {
        return false;
    }

    SetResourceType();

    return true;

}

bool ResourceTrackingInfo::BuildInfo(WCHAR const * resourceName)
{
    m_hResource = OpenClusterResource(g_hCluster, resourceName);

    if (0 == m_hResource) {
        DWORD err = GetLastError();
        if (ERROR_RESOURCE_NOT_FOUND != err) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::BuildInfo OpenClusterResource: %d\n", err);
        }
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(),&CloseClusterResource, m_hResource));

    if (!SetId()) {
        return false;
    }

    m_Name = resourceName;

    SetResourceType();

    if (IsPhysicalDisk()) {
        if (!GetDiskInfo()) {
            return false;
        }
    }

    GetDependsOn();

    return true;
}

void ResourceTrackingInfo::GetInfoForCx(ClusterResourceInfo& info)
{
    USES_CONVERSION;

    info.name = W2CA(m_Name.c_str());
    info.id = W2CA(m_Id.c_str());

    Volumes_t::iterator iter(m_Volumes.begin());
    Volumes_t::iterator endIter(m_Volumes.end());
    for (/* empty */; iter != endIter; ++iter) {
        ClusterVolumeInfo volumeInfo;
        volumeInfo.deviceId = W2CA((*iter).first.c_str());
        volumeInfo.deviceName = W2CA((*iter).second.c_str());
        info.volumes.push_back(volumeInfo);
    }

    ClusterResourceDependencyInfo dependencyInfo;

    Dependencies_t::iterator depIter(m_Dependencies.begin());
    Dependencies_t::iterator depEndIter(m_Dependencies.end());
    for (/* empty */; depIter != depEndIter; ++depIter) {
        dependencyInfo.id = W2CA((*depIter).first.c_str());
        dependencyInfo.name = W2CA((*depIter).second.c_str());
        info.dependencies.push_back(dependencyInfo);
    }
}

bool ResourceTrackingInfo::MissingVolumeNames() const
{
    Volumes_t::const_iterator iter(m_Volumes.begin());
    Volumes_t::const_iterator endIter(m_Volumes.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.empty()) {
            DebugPrintf(SV_LOG_DEBUG, "For resource name %S, Id %S, the volume name is empty for key: %S\n",
                        m_Name.c_str(), m_Id.c_str(), (*iter).first.c_str());
            return true;
        }
    }

    return false;
}

bool ResourceTrackingInfo::ResourceNameChanged(ClusterInfo & clusterInfo, ClusterGroupInfo & group, ResourceTrackingInfo const & resource)
{
    USES_CONVERSION;

    if (m_Name == resource.Name()) {
        return false;
    }

    m_Name = resource.Name();
    ClusterResourceInfo resourceInfo;
    resourceInfo.id = W2CA(m_Id.c_str());
    resourceInfo.name = W2CA(m_Name.c_str());
    group.resources.push_back(resourceInfo);
    clusterInfo.groups.push_back(group);
    RegisterClusterInfo("resource_renamed", clusterInfo);

    return true;
}

bool ResourceTrackingInfo::UpdateResourceDependencyName(ResourceTrackingInfo const & resource)
{
    USES_CONVERSION;

    Dependencies_t::iterator iter(m_Dependencies.find(resource.Id()));
    if (iter == m_Dependencies.end()) {
        return false;
    }

    (*iter).second = resource.Name();

    return true;
}

bool ResourceTrackingInfo::ResourceDependencyChanged(
    ClusterInfo & clusterInfo,
    ClusterGroupInfo & group,
    ResourceTrackingInfo const & resource,
    ResourceTrackingInfo::Dependencies_t & deletedDependencies,
    bool & dependenciesAdded)
{
    USES_CONVERSION;

    Dependencies_t::const_iterator addIter(resource.m_Dependencies.begin());
    Dependencies_t::const_iterator addEndIter(resource.m_Dependencies.end());
    for (/* empty */; addIter != addEndIter; ++ addIter) {
        Dependencies_t::iterator iter(m_Dependencies.find((*addIter).first));
        if (iter == m_Dependencies.end()) {
            ClusterResourceInfo resourceInfo;
            resourceInfo.id = W2CA(m_Id.c_str());
            resourceInfo.name = W2CA(m_Name.c_str());
            ClusterResourceDependencyInfo dependencyInfo;
            dependencyInfo.id = W2CA((*addIter).first.c_str());
            dependencyInfo.name = W2CA((*addIter).second.c_str());
            resourceInfo.dependencies.push_back(dependencyInfo);
            group.resources.push_back(resourceInfo);
            dependenciesAdded = true;
        }
    }

    if (dependenciesAdded) {
        clusterInfo.groups.push_back(group);
        RegisterClusterInfo("dependency_added", clusterInfo);
    }

    group.resources.clear();
    clusterInfo.groups.clear();

    Dependencies_t::iterator delIter(m_Dependencies.begin());
    Dependencies_t::iterator delEndIter(m_Dependencies.end());
    for (/* empty */; delIter != delEndIter; ++ delIter) {
        Dependencies_t::const_iterator iter(resource.m_Dependencies.find((*delIter).first));
        if (iter == resource.m_Dependencies.end()) {
            ClusterResourceInfo resourceInfo;
            resourceInfo.id = W2CA(m_Id.c_str());
            resourceInfo.name = W2CA(m_Name.c_str());
            ClusterResourceDependencyInfo dependencyInfo;
            dependencyInfo.id = W2CA((*delIter).first.c_str());
            dependencyInfo.name = W2CA((*delIter).second.c_str());
            resourceInfo.dependencies.push_back(dependencyInfo);
            group.resources.push_back(resourceInfo);
            deletedDependencies.insert(std::make_pair((*delIter).first, (*delIter).second));
        }
    }

    if (!deletedDependencies.empty()) {
        clusterInfo.groups.push_back(group);
        RegisterClusterInfo("dependency_deleted", clusterInfo);
    }

    if (dependenciesAdded || !deletedDependencies.empty()) {
        m_Dependencies = resource.m_Dependencies;
        return true;
    }

    return false;
}

bool ResourceTrackingInfo::ResourceAdded(ClusterInfo & clusterInfo, ClusterGroupInfo & group)
{
    USES_CONVERSION;

    ClusterResourceInfo resourceInfo;
    resourceInfo.id = W2CA(m_Id.c_str());
    resourceInfo.name = W2CA(m_Name.c_str());

    Volumes_t::iterator volIter(m_Volumes.begin());
    Volumes_t::iterator volEndIter(m_Volumes.end());
    for (/* empty */; volIter != volEndIter; ++volIter) {
        ClusterVolumeInfo volumeInfo;
        volumeInfo.deviceId = W2CA((*volIter).first.c_str());
        volumeInfo.deviceName = W2CA((*volIter).second.c_str());
        resourceInfo.volumes.push_back(volumeInfo);
    }

    Dependencies_t::iterator depIter(m_Dependencies.begin());
    Dependencies_t::iterator depEndIter(m_Dependencies.end());
    for (/* empty */; depIter != depEndIter; ++depIter) {
        ClusterResourceDependencyInfo dependencyInfo;
        dependencyInfo.id = W2CA((*depIter).first.c_str());
        dependencyInfo.name = W2CA((*depIter).second.c_str());
        resourceInfo.dependencies.push_back(dependencyInfo);
    }

    group.resources.push_back(resourceInfo);
    clusterInfo.groups.push_back(group);
    RegisterClusterInfo("resource_added", clusterInfo);

    return true;
}

bool ResourceTrackingInfo::HasVolume(std::wstring const & volumeName) const
{
    Volumes_t::const_iterator iter(m_Volumes.begin());
    Volumes_t::const_iterator endIter(m_Volumes.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second == volumeName) {
            return true;
        }
    }

    return false;
}

void ResourceTrackingInfo::UpdateMountPointsUnderWrongGroup()
{
    MountPointsUnderWrongGroup_t::iterator missingIter(MountPointsUnderWrongGroup.begin());
    MountPointsUnderWrongGroup_t::iterator missingEndIter(MountPointsUnderWrongGroup.end());
    while (missingIter != missingEndIter) {
        Volumes_t::iterator iter(m_Volumes.find((*missingIter).first));
        if (m_Volumes.end() != iter) {
            DebugPrintf(SV_LOG_DEBUG, "For resource name %S, Id %S, in function %s, "
                        "overwriting existing volume %S with %S for key %S\n", m_Name.c_str(), m_Id.c_str(),
                        FUNCTION_NAME, (*iter).second.c_str(), (*missingIter).second.c_str(), (*iter).first.c_str());
            (*iter).second = (*missingIter).second;
            MountPointsUnderWrongGroup.erase(missingIter++);
        } else {
            ++missingIter;
        }
    }
}

// ----------------------------------------------------------------------------
//  GroupTrackingInfo
// -----------------------------------------------------------------------------
bool GroupTrackingInfo::AddMountPointInfo(std::wstring const & signatureOffset, std::wstring const & mountPoint)
{
    ResourceTrackingInfos_t::iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.AddMountPointInfo(signatureOffset, mountPoint)) {
            return true;
        }
    }

    DebugPrintf(SV_LOG_WARNING, "FAILED GroupTrackingInfo::AddMountPointInfo could not find the root volume for mount point %S under this group\n", mountPoint.c_str());

    MountPointsUnderWrongGroup.insert(std::make_pair(signatureOffset, mountPoint));

    return true;
}

bool GroupTrackingInfo::GetMountPointSignatureOffset()
{
    HKEY hKey;

    long status = RegOpenKeyExW(HKEY_LOCAL_MACHINE , L"SYSTEM\\MountedDevices", 0, KEY_QUERY_VALUE, &hKey);

    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetMountPointSignatureOffset RegOpenKeyEx SYSTEM\\MountedDevices: %d\n", status);
            return false;
        }
        return true;
    }

    boost::shared_ptr<void> hKeyGguard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, hKey));

    ResourceTrackingInfos_t::iterator pdIter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator pdEndIter(m_Resources.end());
    for (/* empty */; pdIter != pdEndIter; ++pdIter) {
        (*pdIter).second.GetMountPointSignatureOffsetForVolumes(hKey, this);
    }

    return true;
}

bool GroupTrackingInfo::GetResources()
{
    DWORD enumType = CLUSTER_GROUP_ENUM_CONTAINS;

    HGROUPENUM hGroupEnum = ClusterGroupOpenEnum(m_hGroup, enumType);

    if (0 == hGroupEnum) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo:GetResources ClusterGroupOpenEnum: %d\n", GetLastError());
        return false;
    }
    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<DWORD>(), &ClusterGroupCloseEnum, hGroupEnum));

    WCHAR resourceName[MAX_CHARS];

    DWORD resourceNameLength;
    DWORD status;

    int index = 0;

    do {
        resourceName[0] = '\0';
        resourceNameLength = sizeof(resourceName) / sizeof(WCHAR);
        status = ClusterGroupEnum(hGroupEnum, index, &enumType, resourceName, &resourceNameLength);
        if (ERROR_SUCCESS != status) {
            if (ERROR_NO_MORE_ITEMS != status) {
                // log error
                DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo:GetResources ClusterGroupEnum: %d\n", status);
                return false;
            }
            break;
        }
        if ('\0' != resourceName[0]) {
            ResourceTrackingInfo resource;

            if (!resource.BuildInfo(resourceName)) {
                return false;
            }

            if (resource.IsSqlServer()) {
                m_SqlVirtualServer = true;
            } else if (resource.IsExchangeSystemAttendant()) {
                m_ExchangeVirtualServer = true;
            } else if (resource.IsNetworkName()) {
                resource.GetVirtualServerName(m_VirtualServerName);
                m_NetworkName = resource.Name();
                m_NetworkNameId = resource.Id();
            }

            if (resource.Tracked()) {
                m_Resources.insert(std::make_pair(resource.Id(), resource));
            }
        }
        ++index;
    } while (ERROR_SUCCESS == status);

    return GetMountPointSignatureOffset();
}

bool GroupTrackingInfo::SetNameId(WCHAR const * name)
{
    DWORD status;
    m_hGroup = OpenClusterGroup(g_hCluster, name);

    if (NULL == m_hGroup) {
        status = GetLastError();
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SetId OpenClusterGroup: %d\n", status);
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseClusterGroup, m_hGroup));

    m_Name = name;

    return SetId();
}

bool GroupTrackingInfo::SetId()
{
    WCHAR guid[MAX_GUID];

    DWORD bytesReturned = 0;

    DWORD status = ClusterGroupControl(m_hGroup, NULL, CLUSCTL_GROUP_GET_ID, NULL, 0, guid, sizeof(guid), &bytesReturned);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SetId ClusterGroupControl: %d\n", status);
        return false;
    }

    m_Id = guid;

    return true;
}

bool GroupTrackingInfo::BuildInfo()
{
    DWORD status;
    m_hGroup = OpenClusterGroup(g_hCluster, m_Name.c_str());

    if (NULL == m_hGroup) {
        status = GetLastError();
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::BuildInfo OpenClusterGroup: %d\n", status);
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseClusterGroup, m_hGroup));

    if (!SetId()) {
        return false;
    }

    return GetResources();
}

void GroupTrackingInfo::GetInfoForCx(ClusterGroupInfo& info)
{
    USES_CONVERSION;
    info.name = W2CA(m_Name.c_str());
    info.id = W2CA(m_Id.c_str());

    if (m_SqlVirtualServer) {
        info.sqlVirtualServerName = W2CA(m_VirtualServerName.c_str());
    }

    if (m_ExchangeVirtualServer) {
        info.exchangeVirtualServerName = W2CA(m_VirtualServerName.c_str());
    }

    ResourceTrackingInfos_t::iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        ClusterResourceInfo resource;
        (*iter).second.GetInfoForCx(resource);
        info.resources.push_back(resource);
    }
}

bool GroupTrackingInfo::ResourceAdded(ClusterInfo & clusterInfo, ResourceTrackingInfo const & resource)
{
    USES_CONVERSION;

    ResourceTrackingInfos_t::iterator iter(m_Resources.find(resource.Id()));
    if (iter != m_Resources.end()) {
        ClusterGroupInfo group;
        group.id = W2CA(m_Id.c_str());
        group.name = W2CA(m_Name.c_str());

        if (m_SqlVirtualServer) {
            group.sqlVirtualServerName = W2CA(m_VirtualServerName.c_str());
        }

        if (m_ExchangeVirtualServer) {
            group.exchangeVirtualServerName = W2CA(m_VirtualServerName.c_str());
        }

        if ((*iter).second.ResourceAdded(clusterInfo, group)) {
            return true;
        }
    }

    return false;
}

bool GroupTrackingInfo::ResourceNameChanged(ClusterInfo & clusterInfo, ResourceTrackingInfo const & resource)
{
    USES_CONVERSION;

    ResourceTrackingInfos_t::iterator iter(m_Resources.find(resource.Id()));
    if (iter != m_Resources.end()) {
        ClusterGroupInfo group;
        group.id = W2CA(m_Id.c_str());
        group.name = W2CA(m_Name.c_str());
        if ((*iter).second.ResourceNameChanged(clusterInfo, group, resource)) {
            // update any resources that depend on the resource with the new name
            ResourceTrackingInfos_t::iterator resIter(m_Resources.begin());
            ResourceTrackingInfos_t::iterator resEndIter(m_Resources.end());
            for (/* empty*/; resIter != resEndIter; ++resIter) {
                (*resIter).second.UpdateResourceDependencyName(resource);
            }
            return true;
        }
    }

    return false;
}

bool GroupTrackingInfo::ResourceDependencyChanged(
    ClusterInfo & clusterInfo,
    ResourceTrackingInfo const & resource,
    ResourceTrackingInfo::Dependencies_t & deletedDependencies,
    bool & dependenceisAdded)
{
    USES_CONVERSION;

    ResourceTrackingInfos_t::iterator iter(m_Resources.find(resource.Id()));
    if (iter != m_Resources.end()) {
        ClusterGroupInfo group;
        group.id = W2CA(m_Id.c_str());
        group.name = W2CA(m_Name.c_str());
        if ((*iter).second.ResourceDependencyChanged(clusterInfo, group, resource, deletedDependencies, dependenceisAdded)) {
            return true;
        }
    }

    return false;
}


bool GroupTrackingInfo::ResourceDeleted(ClusterInfo & clusterInfo, std::wstring const & name)
{
    USES_CONVERSION;

    bool deleted = false;

    ResourceTrackingInfos_t::iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        if (name == (*iter).second.Name()) {
            ClusterResourceInfo resource;
            resource.id = W2CA((*iter).second.Id().c_str());
            resource.name = W2CA((*iter).second.Name().c_str());
            ClusterGroupInfo group;
            group.id = W2CA(m_Id.c_str());
            group.name = W2CA(m_Name.c_str());

            if ((*iter).second.IsSqlServer()) {
                group.sqlVirtualServerName = W2CA(m_VirtualServerName.c_str());
            }

            if ((*iter).second.IsExchangeSystemAttendant()) {
                group.exchangeVirtualServerName = W2CA(m_VirtualServerName.c_str());
            }

            group.resources.push_back(resource);
            clusterInfo.groups.push_back(group);
            RegisterClusterInfo("resource_deleted", clusterInfo);
            return true;
        }
    }

    return false;
}

bool GroupTrackingInfo::NetworkNameChanged(ResourceTrackingInfo const & resource)
{
    if (m_NetworkNameId == resource.Id() && m_NetworkName != resource.Name()) {
        m_NetworkName = resource.Name();
        return true;
    }

    return false;
}

bool GroupTrackingInfo::VirtualServerNameChanged(ClusterInfo & clusterInfo, ResourceTrackingInfo & resource)
{
    USES_CONVERSION;

    if (m_NetworkNameId == resource.Id()) {
        std::wstring name;
        resource.GetVirtualServerName(name);
        if (m_VirtualServerName != name) {
            m_VirtualServerName = name;
            if (m_SqlVirtualServer || m_ExchangeVirtualServer) {
                ClusterGroupInfo group;
                group.id = W2CA(m_Id.c_str());
                group.name = W2CA(m_Name.c_str());

                if (m_SqlVirtualServer) {
                    group.sqlVirtualServerName = W2CA(m_VirtualServerName.c_str());
                }

                if (m_ExchangeVirtualServer) {
                    group.exchangeVirtualServerName = W2CA(m_VirtualServerName.c_str());
                }

                clusterInfo.groups.push_back(group);
                RegisterClusterInfo("virtual_server_renamed", clusterInfo);
            }
            return true;
        }
    }

    return false;
}

bool GroupTrackingInfo::GroupNameChanged(ClusterInfo & clusterInfo, std::wstring const & name)
{
    USES_CONVERSION;

    if (name == m_Name) {
        return false;
    }

    m_Name = name;
    ClusterGroupInfo group;
    group.name = W2CA(m_Name.c_str());
    group.id = W2CA(m_Id.c_str());
    clusterInfo.groups.push_back(group);
    RegisterClusterInfo("group_renamed", clusterInfo);

    return true;
}

bool GroupTrackingInfo::SaveVirtualServerInfo(HKEY hKey)
{
    DWORD value = (m_SqlVirtualServer ? 1 : 0);
    DWORD status = RegSetValueExW(hKey, VirtualServerSql, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveVirtualServerInfo RegSetValueEx %S=%d: %d\n", VirtualServerSql, value, status);
        return false;
    }

    value = (m_ExchangeVirtualServer ? 1 : 0);
    status = RegSetValueExW(hKey, VirtualServerExchange, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveVirtualServerInfo RegSetValueEx %S=%S: %d\n", VirtualServerExchange, value, status);
        return false;
    }

    status = RegSetValueExW(hKey, NetworkName, 0, REG_SZ, reinterpret_cast<const BYTE*>(m_NetworkName.c_str()),
                            static_cast<DWORD>(m_NetworkName.length() * sizeof(WCHAR)));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveVirtualServerInfo RegSetValueEx %S=%S: %d\n", NetworkName, m_NetworkName.c_str(), status);
        return false;
    }

    status = RegSetValueExW(hKey, NetworkNameId, 0, REG_SZ, reinterpret_cast<const BYTE*>(m_NetworkNameId.c_str()),
                            static_cast<DWORD>(m_NetworkNameId.length() * sizeof(WCHAR)));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveVirtualServerInfo RegSetValueEx %S=%S: %d\n", NetworkNameId, m_NetworkNameId.c_str(), status);
        return false;
    }

    status = RegSetValueExW(hKey, VirtualServerName, 0, REG_SZ, reinterpret_cast<const BYTE*>(m_VirtualServerName.c_str()),
                            static_cast<DWORD>(m_VirtualServerName.length() * sizeof(WCHAR)));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveVirtualServerInfo RegSetValueEx %S=%S: %d\n", VirtualServerName, m_VirtualServerName.c_str(), status);
        return false;
    }

    return true;
}

bool GroupTrackingInfo::Save(HKEY hKey)
{
    HKEY trackingKey;

    if (!::SaveNameId(hKey, *this, trackingKey)) {
        return false;
    }

    if (!SaveVirtualServerInfo(trackingKey)) {
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    ResourceTrackingInfos_t::iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        (*iter).second.Save(trackingKey);
    }

    return true;
}

bool GroupTrackingInfo::MissingVolumeNames() const
{
    ResourceTrackingInfos_t::const_iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::const_iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.MissingVolumeNames())
            return true;
    }

    return false;
}

bool GroupTrackingInfo::DeleteResource(HKEY hKey, std::wstring const & resourceName)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, m_Id.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::DeleteResource() creating/opening %S for %S: %d\n",
                    m_Id.c_str(), m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    ResourceTrackingInfos_t::iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        bool saveVirtualServerInfo = false;
        if (resourceName.empty() || resourceName == (*iter).second.Name()) {
            (*iter).second.Delete(trackingKey);
            if (!resourceName.empty()) {
                if (m_NetworkName == resourceName) {
                    m_NetworkName.clear();
                    m_NetworkNameId.clear();
                    m_VirtualServerName.clear();
                    saveVirtualServerInfo = true;
                }

                if ((*iter).second.IsExchangeSystemAttendant()) {
                    m_ExchangeVirtualServer = false;
                    saveVirtualServerInfo = true;
                }

                if ((*iter).second.IsSqlServer()) {
                    m_SqlVirtualServer = false;
                    saveVirtualServerInfo = true;
                }

                if (saveVirtualServerInfo) {
                    SaveVirtualServerInfo(trackingKey);
                }

                m_Resources.erase(iter);
                return true;
            }
        }
    }

    // NOTE: if we get here then it means either resoureName is empty and we want to delete
    // all the resources and return true (resourceName.empty() will be true)
    // or resourceName is not empty and we were trying to delete a specific resource
    // and it was not in this group so we need to return false to check other groups
    // (resourceName.empty() will be false)
    if (!resourceName.empty()) {
        return false;
    }

    m_Resources.clear();

    return true;
}

bool GroupTrackingInfo::DeleteResourceDependency(HKEY hKey, ResourceTrackingInfo resource, ResourceTrackingInfo::Dependencies_t const & dependencies)
{
    HKEY trackingKey;

    long status = RegCreateKeyExW(hKey, m_Id.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::DeleteResourceDependency() creating/opening %S for %S: %d\n",
                    m_Id.c_str(), m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    ResourceTrackingInfos_t::iterator iter(m_Resources.find(resource.Id()));
    if (m_Resources.end() == iter) {
        return false;
    }

    if ((*iter).second.DeleteResourceDependency(trackingKey, dependencies)) {
        if (!(*iter).second.IsPhysicalDisk() && (*iter).second.DependenciesEmpty()) {
            (*iter).second.Delete(trackingKey);
            m_Resources.erase(iter);
        }
    }

    return true;
}

bool GroupTrackingInfo::Delete(HKEY hKey)
{
    if (!DeleteResource(hKey, std::wstring())) {
        return false;
    }

    LONG status = RegDeleteKeyW(hKey, m_Id.c_str());
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::Delete() creating/opening %S for %S: %d\n",
                    m_Id.c_str(), m_Name.c_str(), status);
        return false;
    }

    return true;
}

bool GroupTrackingInfo::Load(HKEY hKey, std::wstring id)
{
    HKEY trackingKey;

    if (!::LoadNameId(hKey, id, *this, trackingKey)) {
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));


    ::QueryBool(trackingKey, VirtualServerSql, m_SqlVirtualServer, false);
    ::QueryBool(trackingKey, VirtualServerExchange, m_ExchangeVirtualServer, false);
    ::QueryString(trackingKey, NetworkName, m_NetworkName, false);
    ::QueryString(trackingKey, NetworkNameId, m_NetworkNameId, false);
    ::QueryString(trackingKey, VirtualServerName, m_VirtualServerName, false);

    DWORD index = 0;

    std::wstring resourceId;

    while (ERROR_SUCCESS == EnumerateKey(trackingKey, index, resourceId)) {
        ResourceTrackingInfo resource;
        if (resource.Load(trackingKey, resourceId)) {
            m_Resources.insert(std::make_pair(resource.Id(), resource));
        }
        ++index;
    }
    return true;
}

GroupTrackingInfo::FindResourceResult_t GroupTrackingInfo::FindResourceNameForVolume(std::wstring const & volume) const
{
    ResourceTrackingInfos_t::const_iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::const_iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.HasVolume(volume)) {
            return std::make_pair(true, (*iter).second.Name());
        }
    }

    return std::make_pair(false, std::wstring());
}

void GroupTrackingInfo::UpdateMountPointsUnderWrongGroup()
{
    ResourceTrackingInfos_t::iterator iter(m_Resources.begin());
    ResourceTrackingInfos_t::iterator endIter(m_Resources.end());
    for (/* empty */; iter != endIter; ++iter) {
        if (MountPointsUnderWrongGroup.empty()) {
            break;
        }
        (*iter).second.UpdateMountPointsUnderWrongGroup();
    }
}

// ----------------------------------------------------------------------------
//  ClusterTrackingInfo
// -----------------------------------------------------------------------------
bool ClusterTrackingInfo::GetNameId()
{
    /* since doing ++ below */
    DWORD nameSize = MAX_CHARS - 1;
    DWORD status;

    std::vector<WCHAR> name;
    do {
        /* manpage says that nameSize excludes null charater if
         * ERROR_MORE_DATA */
        nameSize++;
        name.resize(nameSize);
        status = GetClusterInformation(g_hCluster, &name[0], &nameSize, 0);
    } while (ERROR_MORE_DATA == status);

    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "ClusterTrackingInfo::GetNameId GetClusterInformation failed: %d\n", status);
        return false;
    }

    // we need to get the unique id for this cluster, however there doesn't seem to be a cluster API
    // that will return it, so we have to go directly to the registry for it
    m_Name = &name[0];

    HKEY hKey;
    status = RegOpenKeyExW(HKEY_LOCAL_MACHINE , L"Cluster", 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "ClusterTrackingInfo::GetNameId RegOpenKeyEx Cluster failed: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> hKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, hKey));

    if (!::QueryString(hKey, L"ClusterInstanceID", m_Id)) {
        // ClusterInstanceId is not present in w2k, so let's try
        // ClusterNameResource guid instead that will still be unique and
        // should not change
        return ::QueryString(hKey, L"ClusterNameResource", m_Id);
    }
    return true;
}

void ClusterTrackingInfo::ParseAvailableDisks(BYTE_VECTOR & disks, DWORD listSize)
{
    CLUSPROP_BUFFER_HELPER valueList;

    DWORD position = 0;
    DWORD diskLength = 0;

    DWORD signature = 0;
    DWORD diskNumber = 0;


    HKEY hKey;

    long status = RegOpenKeyExW(HKEY_LOCAL_MACHINE , L"SYSTEM\\MountedDevices", 0, KEY_QUERY_VALUE, &hKey);

    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::GetMountPointSignatureOffset RegOpenKeyEx SYSTEM\\MountedDevices: %d\n", status);
        }
        return;
    }

    boost::shared_ptr<void> hKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, hKey));

    while (true) {
        valueList.pb = reinterpret_cast<BYTE*>(&disks[position]);
        if (CLUSPROP_SYNTAX_ENDMARK == valueList.pSyntax->dw) {
            break;
        }

        if (CLUSPROP_SYNTAX_PARTITION_INFO == valueList.pSyntax->dw) {
            SignatureOffsetMountPoints_t somp;
            std::wstring name = reinterpret_cast<CLUSPROP_PARTITION_INFO*>(valueList.pb)->szDeviceName;
            if (!name.empty()) {
                // TODO: we could miss some available mount points if the cluster has
                // the volume that the drive point is on but not the volume the mount point
                // is for. e.g. assume 2 volumes
                // M:\ -> volume 1 is in the cluster
                // M:\mntpnt -> volume 2 is not in the cluster
                // we won't report M:\mntpnt as available since we only look for mount points based on
                // drive letters. In general this is not a valid way to set up the cluster as M:\mntpnt is dependent on M:\
                // being available. For now we won't worry about this

                // only drive lettters end in ':'
                if (L':' == name[name.length() - 1]) {
                    WCHAR signatureOffset[(DISK_SIGNATURE_LENGTH + DISK_OFFSET_LENGTH ) * 2 + 1] = { 0 };
					GetDriveLetterSignatureOffset(hKey, name, signatureOffset, NELEMS(signatureOffset));
                    name += L"\\";
                    if (GetMountPointSignatureOffset(hKey, name, somp)) {
                        SignatureOffsetMountPoints_t::iterator iter(somp.begin());
                        SignatureOffsetMountPoints_t::iterator endIter(somp.end());
                        for (/* empty */; iter != endIter; ++iter) {
                            if (!IsClusterVolume((*iter).second)) {
                                m_AvailableVolumes.insert(std::make_pair((*iter).first, (*iter).second));
                            }
                        }
                    }
                    if (signatureOffset[0] != L'\0' && !IsClusterVolume(name.substr(0, 1))) {
                        m_AvailableVolumes.insert(std::make_pair(signatureOffset, name.substr(0, 1)));
                    }
                }
            }
        }

        position += sizeof(CLUSPROP_VALUE) + ALIGN_CLUSPROP(valueList.pValue->cbLength);
        if ((position + sizeof(DWORD)) >= listSize) {
            break;
        }
    }
}


bool ClusterTrackingInfo::GetClusterAvailableVolumes()
{
    if (!GetClusterAvailableVolumes(ResourceTypePhysicalDisk))
    {
        return false;
    }

    if (!GetClusterAvailableVolumes(ResourceTypeVeritasVolumeManagerDiskGroup))
    {
        return false;
    }

    return true;
}

bool ClusterTrackingInfo::GetClusterAvailableVolumes(LPCWSTR resourceType)
{
    DWORD bytesReturned = AVAILABLE_DISK_SIZE;
    DWORD status;

    BYTE_VECTOR data(bytesReturned);

    do {
        data.resize(bytesReturned);
        status = ClusterResourceTypeControl(g_hCluster,
                                            resourceType,
                                            NULL,
                                            CLUSCTL_RESOURCE_TYPE_STORAGE_GET_AVAILABLE_DISKS,
                                            NULL,
                                            0,
                                            &data[0],
                                            static_cast<DWORD>(data.size()),
                                            &bytesReturned);
    } while ((ERROR_MORE_DATA == status) && (0 != bytesReturned));

    if (ERROR_SUCCESS != status) {
        if (ERROR_CLUSTER_RESOURCE_TYPE_NOT_FOUND == status) {
            // OK if not found as some of the resource types that indicate
            // disks may not be present (e.g. the veritas one is not
            // always going to be there)
            return true;
        }
        if ((ERROR_MORE_DATA == status) && (0 == bytesReturned)) {
            DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::GetClusterAvailableVolumes ClusterResourceTypeControl: "
                        "ERROR_MORE_DATA returned with bytesReturned == 0\n");
        } else {
            DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::GetClusterAvailableVolumes ClusterResourceTypeControl: %d\n", status);
        }
        return false;
    }

    ParseAvailableDisks(data, bytesReturned);

    return true;
}

bool ClusterTrackingInfo::GetGroups()
{
    DWORD enumType = CLUSTER_ENUM_GROUP;

    HCLUSENUM hClusterEnum = ClusterOpenEnum(g_hCluster, enumType);

    if (0 == hClusterEnum) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterGroup::GetGroups ClusterOpenEnum: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> guard(static_cast<void*>(0), boost::bind(boost::type<DWORD>(), &ClusterCloseEnum, hClusterEnum));

    WCHAR groupName[MAX_CHARS];

    DWORD groupNameLength;
    DWORD status;

    int index = 0;

    do {
        groupName[0] = '\0';
        groupNameLength = sizeof(groupName) / sizeof(WCHAR);
        status = ClusterEnum(hClusterEnum, index, &enumType, groupName, &groupNameLength);
        if (ERROR_SUCCESS != status) {
            if (ERROR_NO_MORE_ITEMS != status) {
                DebugPrintf(SV_LOG_ERROR, "Failed ClusterGroup::GetGroups ClusterEnum: %d\n", status);
                return false;
            }
            break;
        }
        if ('\0' != groupName[0]) {
            GroupTrackingInfo groupTrackingInfo(groupName);
            if (!groupTrackingInfo.BuildInfo()) {
                return false;
            }

            if (groupTrackingInfo.Track()) {
                m_Groups.insert(std::make_pair(groupTrackingInfo.Id(), groupTrackingInfo));
            }
        }
        ++index;
    } while (ERROR_SUCCESS == status);

    return true;
}

bool ClusterTrackingInfo::SaveAvailableVolumes(HKEY hKey)
{
    HKEY availableKey;

    long status = RegCreateKeyExW(hKey, ClusterTrackingAvailableVolumesValueName, 0, 0, 0, KEY_ALL_ACCESS, 0, &availableKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ResourceTrackingInfo::SaveAvailableVolumes() creating/opening %S for %S: %d\n",
                    ClusterTrackingDependenciesValueName, m_Name.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> availableKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, availableKey));

    HKEY trackingKey;

    ClusterAvailableVolumeTrackingInfos_t::iterator iter(m_AvailableVolumes.begin());
    ClusterAvailableVolumeTrackingInfos_t::iterator endIter(m_AvailableVolumes.end());
    for (/* empty */; iter != endIter; ++iter) {
        long status = RegCreateKeyExW(availableKey, (*iter).first.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
        if (ERROR_SUCCESS != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveAvailableVolumes() creating/opening %S for %S: %d\n",
                        (*iter).second.c_str(), (*iter).first.c_str(), status);
            return false;
        }

        status = RegSetValueExW(trackingKey, ClusterTrackingNameValueName, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>((*iter).second.c_str()),
                                static_cast<DWORD>((*iter).second.length() * sizeof(WCHAR)));

        RegCloseKey(trackingKey);

        if (ERROR_SUCCESS != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed GroupTrackingInfo::SaveAvailableVolumes() RegSetValueEx %S=%S: %d\n",
                        ClusterTrackingNameValueName, (*iter).second.c_str(), status);
            return false;
        }
    }

    return true;
}

bool ClusterTrackingInfo::LoadAvailableVolumes(HKEY hKey)
{
    HKEY availableKey;

    long status = RegOpenKeyW(hKey, ClusterTrackingAvailableVolumesValueName, &availableKey);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed LoadAvailableVolumes::Load() opening %S: %d\n", ClusterTrackingAvailableVolumesValueName, status);
        return false;
    }

    boost::shared_ptr<void> availableKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, availableKey));

    DWORD index = 0;

    std::wstring id;

    while (ERROR_SUCCESS == EnumerateKey(availableKey, index, id)) {
        HKEY trackingKey;
        long status = RegOpenKeyW(availableKey, id.c_str(), &trackingKey);
        if (ERROR_SUCCESS != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed LoadAvailableVolumes::Load() opening %S: %d\n", id.c_str(), status);
            return false;
        }

        std::wstring name;

        bool result = ::QueryString(trackingKey, ClusterTrackingNameValueName, name);

        RegCloseKey(trackingKey);

        if (!result) {
            return false;
        }

        m_AvailableVolumes.insert(std::make_pair(id, name));
        ++index;
    }
    return true;
}

bool ClusterTrackingInfo::Save()
{
    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::Save() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    status = RegSetValueExW(trackingKey, ClusterTrackingVersionValueName, 0,
                            REG_DWORD, reinterpret_cast<const BYTE*>(&ClusterTrackingVersion),
                            sizeof(ClusterTrackingVersion));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTackingInfo::Save() RegSetValueEx %S: %d\n",
                    ClusterTrackingVersionValueName, status);
        return false;
    }

    DWORD len = static_cast<DWORD>(m_Name.length() * sizeof(WCHAR));

    status = RegSetValueExW(trackingKey, ClusterTrackingNameValueName, 0,
                            REG_SZ, reinterpret_cast<const BYTE*>(m_Name.c_str()),
                            static_cast<DWORD>(m_Name.length() * sizeof(WCHAR)));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTackingInfo::Save() RegSetValueEx %S: %d\n",
                    ClusterTrackingNameValueName, status);
        return false;
    }

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        (*iter).second.Save(trackingKey);
    }

    SaveAvailableVolumes(trackingKey);

    return true;
}

bool ClusterTrackingInfo::SaveClusterName()
{
    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::SaveName() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DWORD len = static_cast<DWORD>(m_Name.length() * sizeof(WCHAR));

    status = RegSetValueExW(trackingKey, ClusterTrackingNameValueName, 0,
                            REG_SZ, reinterpret_cast<const BYTE*>(m_Name.c_str()),
                            static_cast<DWORD>(m_Name.length() * sizeof(WCHAR)));
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTackingInfo::SaveName() RegSetValueEx %S: %d\n",
                    ClusterTrackingNameValueName, status);
        return false;
    }

    return true;
}

bool ClusterTrackingInfo::DeleteGroup(std::wstring const & name)
{
    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::DeleteGroup() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    return DeleteGroup(trackingKey, name);
}

bool ClusterTrackingInfo::DeleteGroup(HKEY hKey, std::wstring const & name)
{
    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        if (name == (*iter).second.Name()) {
            (*iter).second.Delete(hKey);
            m_Groups.erase(iter);
            return true;
        }
    }

    return false;
}

bool ClusterTrackingInfo::DeleteResource(std::wstring const & name)
{
    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::DeleteResource() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.DeleteResource(trackingKey, name)) {
            if (!name.empty()) {
                if ((*iter).second.ResourcesEmpty()) {
                    DeleteGroup(trackingKey,(*iter).second.Name());
                }
                break;
            }
        }
    }

    return true;
}

bool ClusterTrackingInfo::DeleteResourceDependency(ResourceTrackingInfo const & resource, ResourceTrackingInfo::Dependencies_t const & dependencies)
{
    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::DeleteResourceDependency() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.DeleteResourceDependency(trackingKey, resource, dependencies)) {
            if ((*iter).second.ResourcesEmpty()) {
                DeleteGroup(trackingKey, (*iter).second.Name());
            }
            break;
        }
    }

    return true;
}

bool ClusterTrackingInfo::DeleteTrackingInfo(HKEY hKey, std::wstring const & keyName)
{
    std::wstring id;

    HKEY trackingKey;

    long status;

    status = RegCreateKeyExW(hKey, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);

    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::DeleteTrackingInfo creating/opening %S: %d\n", id.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    while (ERROR_SUCCESS == EnumerateKey(trackingKey, 0, id)) {
        DeleteTrackingInfo(trackingKey, id);
    }

    status = RegDeleteKeyW(hKey, keyName.c_str());
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::DeleteTrackingInfo() RegDeleteKeyW %S: %d\n", keyName.c_str(), status);
        return false;
    }

    return true;
}

bool ClusterTrackingInfo::DeleteTrackingInfo()
{
    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    return DeleteTrackingInfo(HKEY_LOCAL_MACHINE, keyName);
}

bool ClusterTrackingInfo::DeleteNode()
{
    DeleteTrackingInfo();
    m_Groups.clear();
    return true;
}

bool ClusterTrackingInfo::DeleteGroupResourceMoved(GroupTrackingInfo & group, DeleteResourceIds_t const & deleteResourceIds)
{
    USES_CONVERSION;

    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &trackingKey, 0);
    if (ERROR_SUCCESS != status) {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::DeleteGroupResourceMoved() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    GroupTrackingInfos_t::iterator grpIter(m_Groups.find(group.Id()));

    if (m_Groups.end() == grpIter) {
        return false;
    }

    DeleteResourceIds_t::const_iterator iter(deleteResourceIds.begin());
    DeleteResourceIds_t::const_iterator endIter(deleteResourceIds.end());
    for (/* empty */; iter != endIter; ++iter) {
        (*grpIter).second.DeleteResource(trackingKey, (*iter));
    }

    if ((*grpIter).second.ResourcesEmpty()) {
        (*grpIter).second.Delete(trackingKey);
        m_Groups.erase(grpIter);
    }

    return true;
}

bool ClusterTrackingInfo::Load()
{
    HKEY trackingKey;

    std::wstring keyName;
    keyName = SV_ROOT_KEY_UNICODE; // FIXME: use a common one once it exists
    keyName += L'\\';
    keyName += ClusterTrackingInfoKeyName;

    long status = RegOpenKeyW(HKEY_LOCAL_MACHINE, keyName.c_str(), &trackingKey);
    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND == status) {
            return true;
        }

        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::Load() creating/opening %S: %d\n", keyName.c_str(), status);
        return false;
    }

    boost::shared_ptr<void> trackingKeyGuard(static_cast<void*>(0), boost::bind(boost::type<LONG>(), &RegCloseKey, trackingKey));

    DWORD value;
    DWORD valueType = REG_DWORD;

    DWORD valueLength = sizeof(value);

    status = RegQueryValueExW(trackingKey, ClusterTrackingVersionValueName, 0, &valueType, reinterpret_cast<BYTE*>(&value), &valueLength);
    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND == status) {
            m_Upgrade = true;
            return true;
        }

        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTackingInfo::Load() RegQueryValueEx %S: %d\n",
                    ClusterTrackingVersionValueName, status);
        return false;
    }

    if (ClusterTrackingVersion != value) {
        // not the correct version so don't bother to continue loading
        // a register_upgrade will be done which should get everything in sync
        m_Upgrade = true;
        return true;
    }

    if (!::QueryString(trackingKey, ClusterTrackingNameValueName, m_Name)) {
        return false;
    }

    DWORD index = 0;

    std::wstring groupId;

    while (ERROR_SUCCESS == EnumerateKey(trackingKey, index, groupId)) {
        if (ClusterTrackingAvailableVolumesValueName == groupId) {
            LoadAvailableVolumes(trackingKey);
        } else {
            GroupTrackingInfo group;
            if (group.Load(trackingKey, groupId)) {
                m_Groups.insert(std::make_pair(group.Id(), group));
            }
        }
        ++index;
    }

    return true;
}

void ClusterTrackingInfo::GetInfoForCx(ClusterInfo& info)
{
    USES_CONVERSION;

    info.name = W2CA(m_Name.c_str());
    info.id= W2CA(m_Id.c_str());

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        ClusterGroupInfo group;
        (*iter).second.GetInfoForCx(group);
        info.groups.push_back(group);
    }

    ClusterAvailableVolumeTrackingInfos_t::iterator availableIter(m_AvailableVolumes.begin());
    ClusterAvailableVolumeTrackingInfos_t::iterator availableEndIter(m_AvailableVolumes.end());
    for (/* empty */; availableIter != availableEndIter; ++availableIter) {
        ClusterAvailableVolume av;
        av.id = W2CA((*availableIter).first.c_str());
        av.name = W2CA((*availableIter).second.c_str());
        info.availableVolumes.push_back(av);
    }
}

void ClusterTrackingInfo::RegisterMovedResources(ClusterInfo  & trackedInfo, ClusterInfo & clusterInfo)
{
    ClusterInfo registerInfo;

    registerInfo.name = clusterInfo.name;
    registerInfo.id= clusterInfo.id;

    bool moved = false;

    ClusterGroupInfos_t::iterator searchEndIter(clusterInfo.groups.end());

    ClusterGroupInfos_t::iterator iter(trackedInfo.groups.begin());
    ClusterGroupInfos_t::iterator endIter(trackedInfo.groups.end());
    for (/*empty */; iter != endIter; ++iter) {
        ClusterResourceInfos_t resInfosToDel;
        ClusterResourceInfos_t::iterator resIter((*iter).resources.begin());
        ClusterResourceInfos_t::iterator resEndIter((*iter).resources.end());
        for (/*empty */; resIter != resEndIter; ++resIter) {
            for (ClusterGroupInfos_t::iterator searchIter = clusterInfo.groups.begin(); searchIter != searchEndIter; ++searchIter) {
                if ((*iter).id != (*searchIter).id) { // no need to check agains self
                    ClusterResourceInfos_t::iterator resSearchEndIter((*searchIter).resources.end());
                    ClusterResourceInfos_t::iterator resSearchIter((*searchIter).resources.begin());
                    for (/* empty */; resSearchIter != resSearchEndIter; ++resSearchIter) {
                        if ((*resIter).id == (*resSearchIter).id) {
                            ClusterResourceInfo reseourceInfo;
                            reseourceInfo.id = (*resSearchIter).id;
                            reseourceInfo.name = (*resSearchIter).name;

                            ClusterGroupInfo groupInfo;
                            groupInfo.id = (*searchIter).id;
                            groupInfo.name = (*searchIter).name;
                            groupInfo.resources.push_back(reseourceInfo);
                            registerInfo.groups.push_back(groupInfo);

                            // remove from both so that we don't do a delete nor add on these
                            resInfosToDel.push_back(*resIter);
                            //(*iter).resources.erase(std::remove((*iter).resources.begin(), (*iter).resources.end(), (*resIter)), (*iter).resources.end());
                            /* TODO: For searchIter.resources, although iterators resSearchIter, ... become invalid, do not
                             * worry since there is break on 2 levels */
                            (*searchIter).resources.erase(std::remove((*searchIter).resources.begin(), (*searchIter).resources.end(), (*resSearchIter)), (*searchIter).resources.end());
                            moved = true;
                            break;
                        }
                    }
                    if (moved) {
                        // done with this resource onto next tracked resource
                        moved = false;
                        break;
                    }
                }
            }
        }
        for (ClusterResourceInfos_t::iterator delIter = resInfosToDel.begin(); delIter != resInfosToDel.end(); delIter++)
        {
            (*iter).resources.erase(std::remove((*iter).resources.begin(), (*iter).resources.end(), (*delIter)), (*iter).resources.end());
        }
    }

    if (!registerInfo.groups.empty()) {
        RegisterClusterInfo("resource_moved", registerInfo);
    }

}

void ClusterTrackingInfo::RegisterDeletedGroups(ClusterInfo  & trackedInfo, ClusterInfo const & clusterInfo)
{
    ClusterInfo registerInfo;
    ClusterGroupInfos_t groupInfosToDel;

    registerInfo.name = clusterInfo.name;
    registerInfo.id = clusterInfo.id;

    ClusterGroupInfos_t::const_iterator searchEndIter(clusterInfo.groups.end());

    ClusterGroupInfos_t::iterator iter(trackedInfo.groups.begin());
    ClusterGroupInfos_t::iterator endIter(trackedInfo.groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        ClusterGroupInfos_t::const_iterator searchIter(clusterInfo.groups.begin());
        for (/* empty */; searchIter != searchEndIter; ++searchIter) {
            if ((*iter).id == (*searchIter).id) {
                break;
            }
        }
        if (searchIter == searchEndIter) {
            ClusterGroupInfo groupInfo;
            groupInfo.id = (*iter).id;
            groupInfo.name = (*iter).name;
            registerInfo.groups.push_back(groupInfo);
            groupInfosToDel.push_back(*iter);
            //trackedInfo.groups.erase(std::remove(trackedInfo.groups.begin(), trackedInfo.groups.end(), (*iter)), trackedInfo.groups.end());
        }
    }

    for (ClusterGroupInfos_t::iterator delIter = groupInfosToDel.begin(); delIter != groupInfosToDel.end(); delIter++)
    {
        trackedInfo.groups.erase(std::remove(trackedInfo.groups.begin(), trackedInfo.groups.end(), (*delIter)), trackedInfo.groups.end());
    }

    if (!registerInfo.groups.empty()) {
        RegisterClusterInfo("group_deleted", registerInfo);
    }
}

void ClusterTrackingInfo::RegisterDeletedResources(ClusterInfo & trackedInfo, ClusterInfo const & clusterInfo)
{
    ClusterInfo registerInfo;

    registerInfo.name = clusterInfo.name;
    registerInfo.id = clusterInfo.id;

    ClusterGroupInfos_t::const_iterator searchEndIter(clusterInfo.groups.end());

    ClusterGroupInfos_t::iterator iter(trackedInfo.groups.begin());
    ClusterGroupInfos_t::iterator endIter(trackedInfo.groups.end());
    for (/*empty */; iter != endIter; ++iter) {
        ClusterResourceInfos_t resInfosToDel;
        for (ClusterGroupInfos_t::const_iterator searchIter = clusterInfo.groups.begin(); searchIter != searchEndIter; ++searchIter) {
            if ((*iter).id == (*searchIter).id) {
                ClusterGroupInfo groupInfo;
                groupInfo.id = (*iter).id;
                groupInfo.name = (*iter).name;

                ClusterResourceInfos_t::const_iterator resSearchEndIter((*searchIter).resources.end());

                ClusterResourceInfos_t::iterator resIter((*iter).resources.begin());
                ClusterResourceInfos_t::iterator resEndIter((*iter).resources.end());
                for (/*empty */; resIter != resEndIter; ++resIter) {
                    ClusterResourceInfos_t::const_iterator resSearchIter((*searchIter).resources.begin());
                    for (/* empty */; resSearchIter != resSearchEndIter; ++resSearchIter) {
                        if ((*resIter).id == (*resSearchIter).id) {
                            break;
                        }
                    }
                    if (resSearchIter == resSearchEndIter) {
                        ClusterResourceInfo resourceInfo;
                        resourceInfo.id = (*resIter).id;
                        resourceInfo.name = (*resIter).name;
                        groupInfo.resources.push_back(resourceInfo);
                        resInfosToDel.push_back(*resIter);
                        //(*iter).resources.erase(std::remove((*iter).resources.begin(), (*iter).resources.end(), (*resIter)), (*iter).resources.end());
                    }
                }

                if (!groupInfo.resources.empty()) {
                    registerInfo.groups.push_back(groupInfo);
                }
                break;
            }
        }
        for (ClusterResourceInfos_t::iterator delIter = resInfosToDel.begin(); delIter != resInfosToDel.end(); delIter++)
        {
            (*iter).resources.erase(std::remove((*iter).resources.begin(), (*iter).resources.end(), (*delIter)), (*iter).resources.end());
        }
    }

    if (!registerInfo.groups.empty()) {
        RegisterClusterInfo("resource_deleted", registerInfo);
    }
}

void ClusterTrackingInfo::RegisterDeletedResourceDependencies(ClusterInfo & trackedInfo, ClusterInfo const & clusterInfo)
{
    ClusterInfo registerInfo;

    registerInfo.name = clusterInfo.name;
    registerInfo.id = clusterInfo.id;

    ClusterGroupInfos_t::const_iterator searchEndIter(clusterInfo.groups.end());

    ClusterGroupInfos_t::iterator iter(trackedInfo.groups.begin());
    ClusterGroupInfos_t::iterator endIter(trackedInfo.groups.end());
    for (/*empty */; iter != endIter; ++iter) {
        for (ClusterGroupInfos_t::const_iterator searchIter = clusterInfo.groups.begin(); searchIter != searchEndIter; ++searchIter) {
            if ((*iter).id == (*searchIter).id) {
                ClusterGroupInfo groupInfo;
                groupInfo.id = (*iter).id;
                groupInfo.name = (*iter).name;

                ClusterResourceInfos_t::const_iterator resSearchEndIter((*searchIter).resources.end());

                ClusterResourceInfos_t::iterator resIter((*iter).resources.begin());
                ClusterResourceInfos_t::iterator resEndIter((*iter).resources.end());
                for (/*empty */; resIter != resEndIter; ++resIter) {
                    ClusterResourceDependencyInfos_t depsToDel;
                    ClusterResourceInfos_t::const_iterator resSearchIter((*searchIter).resources.begin());
                    for (/* empty */; resSearchIter != resSearchEndIter; ++resSearchIter) {
                        ClusterResourceInfo resourceInfo;
                        if ((*resIter).id == (*resSearchIter).id) {
                            resourceInfo.id = (*resIter).id;
                            resourceInfo.name = (*resIter).name;

                            ClusterResourceDependencyInfos_t::const_iterator depSearchEndIter((*resSearchIter).dependencies.end());

                            ClusterResourceDependencyInfos_t::iterator depIter((*resIter).dependencies.begin());
                            ClusterResourceDependencyInfos_t::iterator depEndIter((*resIter).dependencies.end());
                            for (/* empty */; depIter != depEndIter; ++depIter) {
                                ClusterResourceDependencyInfos_t::const_iterator depSearchIter = (*resSearchIter).dependencies.begin();
                                for (; depSearchIter != depSearchEndIter; ++depSearchIter) {
                                    if ((*depIter).id == (*depSearchIter).id) {
                                        break;
                                    }
                                }
                                if (depSearchIter == depSearchEndIter) {
                                    ClusterResourceDependencyInfo depInfo;
                                    depInfo.id = (*depIter).id;
                                    depInfo.name = (*depIter).name;
                                    resourceInfo.dependencies.push_back(depInfo);
                                    depsToDel.push_back(*depIter);
                                    //(*resIter).dependencies.erase(std::remove((*resIter).dependencies.begin(), (*resIter).dependencies.end(), (*depIter)), (*resIter).dependencies.end());
                                }
                            }
                            if (!resourceInfo.dependencies.empty()) {
                                groupInfo.resources.push_back(resourceInfo);
                            }
                            break;
                        }
                    }
                    for (ClusterResourceDependencyInfos_t::iterator delIter = depsToDel.begin(); delIter != depsToDel.end(); delIter++)
                    {
                        (*resIter).dependencies.erase(std::remove((*resIter).dependencies.begin(), (*resIter).dependencies.end(), (*delIter)), (*resIter).dependencies.end());
                    }
                }

                if (!groupInfo.resources.empty()) {
                    registerInfo.groups.push_back(groupInfo);
                }
                break;
            }
        }
    }

    if (!registerInfo.groups.empty()) {
        RegisterClusterInfo("dependency_deleted", registerInfo);
    }
}

void ClusterTrackingInfo::RegisterDeletedAvailableVolumes(ClusterTrackingInfo const & trackedInfo)
{
    USES_CONVERSION;

    ClusterInfo registerInfo;
    registerInfo.name = W2CA(m_Name.c_str());
    registerInfo.id = W2CA(m_Id.c_str());

    ClusterAvailableVolumeTrackingInfos_t::const_iterator iter(trackedInfo.m_AvailableVolumes.begin());
    ClusterAvailableVolumeTrackingInfos_t::const_iterator endIter(trackedInfo.m_AvailableVolumes.end());
    for (/* empty */; iter != endIter; ++iter) {
        if (m_AvailableVolumes.end() == m_AvailableVolumes.find((*iter).first)) {
            ClusterAvailableVolume av;
            av.id = W2CA((*iter).first.c_str());
            av.name = W2CA((*iter).second.c_str());
            registerInfo.availableVolumes.push_back(av);
        }
    }

    if (!registerInfo.availableVolumes.empty()) {
        RegisterClusterInfo("available_volume_deleted", registerInfo);
    }
}

bool ClusterTrackingInfo::Register()
{
    LocalConfigurator TheLocalConfigurator;
    if (TheLocalConfigurator.registerClusterInfoEnabled()) {
        ClusterTrackingInfo currentTrackedInfo;
        currentTrackedInfo.Load();

        ClusterInfo clusterInfo;
        GetInfoForCx(clusterInfo);

        if (currentTrackedInfo.Upgrade()) {
            RegisterClusterInfo("register_upgrade", clusterInfo);
        } else {
            ClusterInfo trackedInfo;
            currentTrackedInfo.GetInfoForCx(trackedInfo);
            DumpClusterInfo("Previously Collected Tracked Info", trackedInfo);

            // keep things in this order
            RegisterMovedResources(trackedInfo, clusterInfo);
            RegisterDeletedGroups(trackedInfo, clusterInfo);
            RegisterDeletedResources(trackedInfo, clusterInfo);
            RegisterDeletedResourceDependencies(trackedInfo, clusterInfo);
            RegisterDeletedAvailableVolumes(currentTrackedInfo);
            RegisterClusterInfo("register", clusterInfo);  // register what ever is left
        }

#ifndef CLUSTER_STAND_ALONE
        DeleteTrackingInfo();
        Save();
#endif
    }

    return true;
}

void ClusterTrackingInfo::UpdateMountPointsUnderWrongGroup()
{
    if (!MountPointsUnderWrongGroup.empty()) {
        GroupTrackingInfos_t::iterator iter(m_Groups.begin());
        GroupTrackingInfos_t::iterator endIter(m_Groups.end());
        for (/* empty */; iter != endIter; ++iter) {
            if (MountPointsUnderWrongGroup.empty()) {
                break;
            }
            (*iter).second.UpdateMountPointsUnderWrongGroup();
        }
        MountPointsUnderWrongGroup.clear();
    }
}

bool ClusterTrackingInfo::BuildClusterTrackingInfo()
{
    m_Groups.clear();
    m_AvailableVolumes.clear();
    m_Name.clear();
    m_Id.clear();

    if (!GetNameId()) {
        return false;
    }

    if (!GetGroups()) {
        return false;
    }

    if (!GetClusterAvailableVolumes()) {
        return false;
    }

    UpdateMountPointsUnderWrongGroup();

    return true;
}

void ClusterTrackingInfo::RegisterIfNeeded()
{
    // because of the way we get installed we may miss
    // mount point info initially, so we just check if
    // any of the volume entries are missing names, if so
    // just re-build everything and re-register
    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.MissingVolumeNames()) {
            BuildClusterTrackingInfo();
            Register();
            return;
        }
    }
}

void ClusterTrackingInfo::ResourceStateChanged(WCHAR const * name)
{
    USES_CONVERSION;

    ResourceTrackingInfo resource;

    bool bnameidtypeset = resource.SetNameIdType(name);
    if (false == bnameidtypeset)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed ClusterTrackingInfo::ResourceStateChanged resource.SetNameIdType\n");
        return;
    }

    if (resource.Online()) {
        RegisterIfNeeded();
        return;
    }

    if (resource.GoingOffline()) {
        if (resource.IsPhysicalDisk() && 0 != m_OfflineCallback) {
            (*m_OfflineCallback)();
        }
        return;
    }

    if (resource.Online()) {
        // NOTE: network name changes don't take effect until the network name resource
        // is brought on line, so if the resource is online we need to check
        // if it is a network name resource and if the name changed. also note that we
        // do get ResourcePropertyChanged notification when the name is actually changed,
        // but doing a look up at that time returns the old name (yeah a little confusing)

        if (resource.IsNetworkName()) {
            ClusterInfo info;

            info.name = W2CA(m_Name.c_str());
            info.id = W2CA(m_Id.c_str());

            GroupTrackingInfos_t::iterator iter(m_Groups.begin());
            GroupTrackingInfos_t::iterator endIter(m_Groups.end());
            for (/* empty */; iter != endIter; ++iter) {
                if ((*iter).second.VirtualServerNameChanged(info, resource)){
                    Save();
                    return;
                }
            }
        }
    }
}

void ClusterTrackingInfo::ResourceAdded(WCHAR const * name)
{
    USES_CONVERSION;

    ResourceTrackingInfo resource;

    if (resource.BuildInfo(name))
    {
        ResourceAdded(resource);
    }
}

void ClusterTrackingInfo::ResourceAdded(ResourceTrackingInfo const & resource)
{
    USES_CONVERSION;
    if (resource.Tracked()) {

        BuildClusterTrackingInfo();

        ClusterInfo info;

        info.name = W2CA(m_Name.c_str());
        info.id = W2CA(m_Id.c_str());

        GroupTrackingInfos_t::iterator iter(m_Groups.begin());
        GroupTrackingInfos_t::iterator endIter(m_Groups.end());
        for (/* empty */; iter != endIter; ++iter) {
            if ((*iter).second.ResourceAdded(info, resource)) {
                Save();
                return;
            }
        }
    }
}

void ClusterTrackingInfo::ResourceDeleted(WCHAR const * name)
{
    USES_CONVERSION;

    ClusterInfo info;

    info.name = W2CA(m_Name.c_str());
    info.id = W2CA(m_Id.c_str());

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        if ((*iter).second.ResourceDeleted(info, name)) {
            DeleteResource(name);
            return;
        }
    }
}

void ClusterTrackingInfo::ResourcePropertyChanged(WCHAR const * name)
{
    USES_CONVERSION;

    // since we track network names for virtual servers and the cluster name is also
    // stored as a network name we need a special check for cluster rename
    // which will be the result of a ResourcePropertyChanged
    if (ClusterRenamed()) {
        return;
    }

    bool resourceExists = false;

    ResourceTrackingInfo resource;

    if (!resource.BuildInfo(name)) {
        // resource probably deleted just return as nothing more can be done at this time
        return;
    }


    ResourceTrackingInfo::Dependencies_t deletedDependencies;

    bool dependenciesAdded = false;

    ClusterInfo info;

    info.name = W2CA(m_Name.c_str());
    info.id = W2CA(m_Id.c_str());

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {

        // NOTE: do these in this order
        if ((resource.Tracked() && (*iter).second.ResourceNameChanged(info, resource))
            || (resource.IsNetworkName() && (*iter).second.NetworkNameChanged(resource))) {
            Save();
            return;
        }

        if ((*iter).second.ResourceDependencyChanged(info, resource, deletedDependencies, dependenciesAdded)) {
            if (!deletedDependencies.empty()) {
                DeleteResourceDependency(resource, deletedDependencies);
            }
            if (dependenciesAdded) {
                Save();
            }
            return;
        }

        if (resource.Tracked() && !resourceExists && (*iter).second.GroupResourceExists(resource.Id())) {
            resourceExists = true;
        }
    }

    if (!resourceExists) {
        // if we get here then it could this is really the result of a resource
        // being added and the volumes or dependencies didn't show up until now
        ResourceAdded(resource);
    }
}

void ClusterTrackingInfo::GroupDeleted(WCHAR const * name)
{
    USES_CONVERSION;

    GroupTrackingInfos_t::iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        if (name == (*iter).second.Name()) {
            ClusterGroupInfo group;
            group.name = W2CA((*iter).second.Name().c_str());
            group.id = W2CA((*iter).second.Id().c_str());
            ClusterInfo clusterInfo;
            clusterInfo.name = W2CA(m_Name.c_str());
            clusterInfo.groups.push_back(group);
            RegisterClusterInfo("group_deleted", clusterInfo);
            DeleteGroup(name);
            return;
        }
    }
}

bool ClusterTrackingInfo::GroupResourceMoved(ClusterInfo & clusterInfo, GroupTrackingInfo & group)
{
    USES_CONVERSION;

    ClusterGroupInfo groupInfo;

    DeleteResourceIds_t deleteResourceIds;

    // more then one resource can be moved at a time based on dependencies
    // so we have to check all the resources in group against all the other groups
    GroupTrackingInfo::ResourceTrackingInfos_t const resources = group.Resources();

    GroupTrackingInfo::ResourceTrackingInfos_t::const_iterator iter;
    GroupTrackingInfo::ResourceTrackingInfos_t::const_iterator endIter(resources.end());

    GroupTrackingInfos_t::iterator grpIter(m_Groups.begin());
    GroupTrackingInfos_t::iterator grpEndIter(m_Groups.end());
    for (/* empty */; grpIter != grpEndIter; ++grpIter) {
        if (group.Id() != (*grpIter).first) { // no need to check against itself
            for (iter = resources.begin(); iter != endIter; ++iter) {
                if ((*iter).second.Tracked() && (*grpIter).second.GroupResourceExists((*iter).first.c_str())) {
                    ClusterResourceInfo resourceInfo;
                    resourceInfo.id = W2CA((*iter).second.Id().c_str());
                    resourceInfo.name = W2CA((*iter).second.Name().c_str());
                    groupInfo.resources.push_back(resourceInfo);
                    deleteResourceIds.push_back((*iter).second.Name());
                }
            }

            if (!deleteResourceIds.empty()) {
                groupInfo.id = W2CA(group.Id().c_str());
                groupInfo.name = W2CA(group.Name().c_str());
                clusterInfo.groups.push_back(groupInfo);
                RegisterClusterInfo("resource_moved", clusterInfo);
                DeleteGroupResourceMoved((*grpIter).second, deleteResourceIds);
                return true;
            }
        }
    }

    return false;
}

bool ClusterTrackingInfo::GroupResourceMovedToNonTrackedGroup(ClusterInfo & clusterInfo, GroupTrackingInfo & nonTrackedGroup)
{
    // a reource moved will show up first as GroupPropertyChanged for the group that the resource was
    // moved to and then as a GroupPropertyChanged for the group that resource was moved from. In this
    // case the move could have been to a group we are not currently tracking so we just get the
    // resource and then try to find that resource in some other currently tracked group. if found, then
    // the resource was "moved" if not found, then it is a plain resource add which we don't care about
    // at this time. we will pick it up as part of the resource added property changed
    if (GroupResourceMoved(clusterInfo, nonTrackedGroup)) {
        // we want to track this now as it has resources we care about
        m_Groups.insert(std::make_pair(nonTrackedGroup.Id(), nonTrackedGroup));
        Save();
        return true;
    }

    return false;
}

bool ClusterTrackingInfo::GroupResourceMoved(ClusterInfo & clusterInfo, GroupTrackingInfo & group, GroupTrackingInfo & changedGroup)
{
    // a reource moved will show up first as GroupPropertyChanged for the group that the resource was
    // moved to and then as a GroupPropertyChanged for the group that resource was moved from In this
    // case we check if the changedGroup has more resources then its currently tracked group's resources
    // if no, then not a move and we are done if yes we try to find that resource in some other currently
    // trakced group if found, then the resource was "moved" if not found, then it is a plain resource add
    // which we don't care about at this time. we will pick it up as part of the resource added property change
    if (group.GroupResourceAdded(changedGroup) && GroupResourceMoved(clusterInfo, changedGroup)) {
        group = changedGroup;
        Save();
        return true;
    }

    return false;
}

bool ClusterTrackingInfo::ClusterRenamed()
{
    USES_CONVERSION;

    std::wstring name = m_Name;

    GetNameId();

    if (m_Name == name) {
        return false;
    }

    ClusterInfo clusterInfo;

    clusterInfo.name = W2CA(m_Name.c_str());
    clusterInfo.id = W2CA(m_Id.c_str());

    RegisterClusterInfo("cluster_renamed", clusterInfo);

    SaveClusterName();

    return true;
}

void ClusterTrackingInfo::GroupPropertyChanged(WCHAR const * name)
{
    USES_CONVERSION;

    ClusterInfo info;

    info.name = W2CA(m_Name.c_str());
    info.id = W2CA(m_Id.c_str());

    GroupTrackingInfo group;

    group.SetNameId(name);


    GroupTrackingInfos_t::iterator iter(m_Groups.find(group.Id()));
    if (iter != m_Groups.end()) {
        if ((*iter).second.GroupNameChanged(info, name)) {
            Save();
            return;
        }

        group.BuildInfo();
        if (GroupResourceMoved(info, (*iter).second, group)) {
            return;
        }
    }

    group.BuildInfo();
    GroupResourceMovedToNonTrackedGroup(info, group);
}

void ClusterTrackingInfo::NodeAdded(WCHAR const * name)
{
    // TODO: is there anything that really needs to be done?
}

void ClusterTrackingInfo::NodePropertyChanged(WCHAR const * name)
{
    // TODO: is there anything that really needs to be done?
}

void ClusterTrackingInfo::NodeDeleted(WCHAR const * name)
{
    USES_CONVERSION;

    ClusterInfo clusterInfo;

    // quick fix to make sure the proper node is deleted
    // when a node is evicted the remaining nodes report the
    // eviction, so to prevent the remaining nodes from being
    // deleted use the evicted node name instead of the cluster
    // name (m_Name) for cluserInfo.name. the cx epects this
    // as well so it is OK to do it.
    clusterInfo.name = W2CA(name);
    clusterInfo.id = W2CA(m_Id.c_str());

    RegisterClusterInfo("node_deleted", clusterInfo);

    // note registry clean up will happen later since this
    // notification only comes to nodes still in the cluster
    // we know this node is not being deleted
}

bool ClusterTrackingInfo::VolumeAvailableForNode(std::wstring const & vol) const
{
    GroupTrackingInfos_t::const_iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::const_iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        GroupTrackingInfo::FindResourceResult_t resResult = (*iter).second.FindResourceNameForVolume(vol);
        if (resResult.first) {
            return VolumeAvailable(resResult.second);
        }
    }

    // if we get here then the volume is not being tracked in the cluster so assume it is available
    return true;
}

bool ClusterTrackingInfo::IsClusterVolume(std::wstring const & vol) const
{
    GroupTrackingInfos_t::const_iterator iter(m_Groups.begin());
    GroupTrackingInfos_t::const_iterator endIter(m_Groups.end());
    for (/* empty */; iter != endIter; ++iter) {
        GroupTrackingInfo::FindResourceResult_t resResult = (*iter).second.FindResourceNameForVolume(vol);
        if (resResult.first) {
            return true;
        }
    }

    return false;
}

bool ClusterTrackingInfo::VolumeAvailable(std::wstring const & resourceName) const
{
    DWORD length = 128;
    std::vector<WCHAR> nodeName(length);

    if (!GetComputerNameW(&nodeName[0], &length)) {
        std::stringstream msg;
        msg << "FAILED ClusterTrackingInfo::VolumeAvailable GetComputerName: " <<  GetLastError() << '\n';
        DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
        throw std::exception(msg.str().c_str());
    }

    HRESOURCE hResource = OpenClusterResource(g_hCluster, resourceName.c_str());

    if (0 == hResource) {
        return false;
    }

    DWORD ownerLength = 255;
    DWORD groupLength = 255;
    std::vector<WCHAR> owner(ownerLength);
    std::vector<WCHAR> group(groupLength);

    CLUSTER_RESOURCE_STATE resourceState = GetClusterResourceState(hResource, &owner[0], &ownerLength, &group[0], &groupLength);

    CloseClusterResource(hResource);

    return (ClusterResourceOnline == resourceState && 0 == wcsicmp(&nodeName[0], &owner[0]));
}

bool ClusterTrackingInfo::CleanUpIfNeeded()
{
    HKEY hKey;
    long status = RegOpenKeyExW(HKEY_LOCAL_MACHINE , L"Cluster", 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS == status) {
        RegCloseKey(hKey);
        return true; // continue to check
    }

    DeleteNode();
    return false; // no need to check anymore
}


// ----------------------------------------------------------------------------
//  Cluster
// -----------------------------------------------------------------------------
Cluster::Cluster()
  : m_MonitorEvents((DWORD)(CLUSTER_CHANGE_GROUP_PROPERTY    |
                            CLUSTER_CHANGE_GROUP_DELETED     |
                            CLUSTER_CHANGE_RESOURCE_ADDED    |
                            CLUSTER_CHANGE_RESOURCE_DELETED  |
                            CLUSTER_CHANGE_RESOURCE_PROPERTY |
                            CLUSTER_CHANGE_RESOURCE_STATE    |
                            CLUSTER_CHANGE_NODE_ADDED        |
                            CLUSTER_CHANGE_NODE_DELETED      |
                            CLUSTER_CHANGE_NODE_PROPERTY     |
                            CLUSTER_CHANGE_CLUSTER_STATE     |
                            CLUSTER_CHANGE_REGISTRY_VALUE    |
                            CLUSTER_CHANGE_REGISTRY_SUBTREE  |
                            CLUSTER_CHANGE_HANDLE_CLOSE)),
    m_MonitorHandle(0),
    m_MonitorThreadId(0),
    m_CollectAndExit(false),
    m_Exited(false)
{
    InitializeCriticalSection(&m_CriticalSection);
}

bool Cluster::HandleClusterNotifications()
{
    DWORD filterType;
    DWORD notifyKey;
    DWORD nameSize;
    DWORD status;

    WCHAR name[MAX_CHARS];

    HCHANGE hChange = CreateClusterNotifyPort((HCHANGE)INVALID_HANDLE_VALUE, g_hCluster, m_MonitorEvents, m_MonitorEvents);

    if (0 == hChange) {
        status = GetLastError();
        DebugPrintf(SV_LOG_ERROR, "Failed Cluster::HandleClusterNotifications CreateClusterNotifyPort: %d\n", status);
        return false;
    }

    boost::shared_ptr<void> hChangeGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseClusterNotifyPort, hChange));

    while (!QuitRequested(0)) {

        nameSize = MAX_CHARS;

        status = GetClusterNotify(hChange, &notifyKey, &filterType, name, &nameSize, CLUSTER_NOTIFY_TIMEOUT_MILISECONDS);

        if (ERROR_SUCCESS == status) {
            switch(filterType ) {
                case CLUSTER_CHANGE_CLUSTER_STATE:
#ifndef CLUSTER_STAND_ALONE
                    SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_CLUSTER, ClusSvcShutdownAlert());
#endif
                    return false;

                case CLUSTER_CHANGE_REGISTRY_VALUE:
                case CLUSTER_CHANGE_REGISTRY_SUBTREE:
                    RegistryValueChanged(name);
                    break;

                case CLUSTER_CHANGE_RESOURCE_STATE:
                    ResourceStateChanged(name);
                    break;

                case CLUSTER_CHANGE_RESOURCE_ADDED:
                    ResourceAdded(name);
                    break;

                case CLUSTER_CHANGE_NODE_ADDED:
                    NodeAdded(name);
                    break;

                case CLUSTER_CHANGE_RESOURCE_DELETED:
                    ResourceDeleted(name);
                    break;

                case CLUSTER_CHANGE_NODE_DELETED:
                    NodeDeleted(name);
                    break;

                case CLUSTER_CHANGE_GROUP_PROPERTY:
                    GroupPropertyChanged(name);
                    break;

                case CLUSTER_CHANGE_GROUP_DELETED:
                    GroupDeleted(name);
                    break;

                case CLUSTER_CHANGE_RESOURCE_PROPERTY:
                    ResourcePropertyChanged(name);
                    break;

                case CLUSTER_CHANGE_NODE_PROPERTY:
                    NodePropertyChanged(name);
                    break;

                case CLUSTER_CHANGE_HANDLE_CLOSE:
                    // FIXME: should not exit, should instead reset things back as if calling monitor directly.
#ifndef CLUSTER_STAND_ALONE
                    SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_CLUSTER, ClusSvcShutdownAlert());
#endif
                    return false;

                default:
                    break;
            }
        } else if (WAIT_TIMEOUT != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed Cluster::HandleClusterNotifications GetclusterNotify: %d\n", status);
            return false;
        }
    }

    return true;
}

bool Cluster::VolumeAvailableForNode(std::string const &name)
{
    USES_CONVERSION;

    if (!SetClusterHandle(g_hCluster)) {
        return false;
    }

    std::wstringstream volumeName;

    volumeName << A2CW(name.c_str());

    ClusterTrackingInfo clusterTrackingInfo;
    clusterTrackingInfo.BuildClusterTrackingInfo();
    return clusterTrackingInfo.VolumeAvailableForNode(volumeName.str());
}

bool Cluster::IsClusterVolume(std::string const &name)
{
    USES_CONVERSION;

    if (!SetClusterHandle(g_hCluster)) {
        return false;
    }

    std::wstringstream volumeName;

    volumeName << A2CW(name.c_str());

    ClusterTrackingInfo clusterTrackingInfo;
    clusterTrackingInfo.BuildClusterTrackingInfo();
    return clusterTrackingInfo.IsClusterVolume(volumeName.str());
}

bool Cluster::VerifyInCDskFlInstall()
{
    HKEY hKey;

    WCHAR* keyName = L"SYSTEM\\CurrentControlSet\\Services\\InCDskFl";

    DWORD status = RegOpenKeyExW(HKEY_LOCAL_MACHINE , keyName, 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS != status) {
        if (ERROR_FILE_NOT_FOUND != status) {
            DebugPrintf(SV_LOG_ERROR, "Failed Cluster::VerifyInCDskFlInstall RegOpenKeyEx %S: %d\n", keyName, status);
        } else {
            DebugPrintf(SV_LOG_ERROR, "FAILED Cluster::VerifyInCDskFlInstall InCDskFl not installed\n");
        }
        return false;
    }
    return true;
}

bool Cluster::VerifyInCDskFlLoaded()
{
    SC_HANDLE scMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (0 == scMan) {
        DebugPrintf(SV_LOG_ERROR, "Failed Cluster::VerifyInCDskFlLoaded OpenSCManager: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> scManGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseServiceHandle, scMan));

    SC_HANDLE  schService = OpenServiceW(scMan, L"InCDskFl", SERVICE_ALL_ACCESS);
    if (0 == schService) {
        DebugPrintf(SV_LOG_ERROR, "Failed Cluster::VerifyInCDskFlLoaded OpenService InCDskFl: %d\n", GetLastError());
        return false;
    }

    boost::shared_ptr<void> schServiceGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseServiceHandle, schService));

    SERVICE_STATUS serviceStatus;
    if (!QueryServiceStatus(schService, &serviceStatus)) {
        DebugPrintf(SV_LOG_ERROR, "Failed Cluster::VerifyInCDskFlLoaded QueryServiceStatus InCDskFl: %d\n", GetLastError());
        return false;
    }

    if (SERVICE_RUNNING != serviceStatus.dwCurrentState) {
        DebugPrintf(SV_LOG_ERROR, "Failed Cluster::VerifyInCDskFlLoaded InCDskFl not running current state: %d\n", GetLastError());
        return false;
    }

    return true;
}

bool Cluster::VerifyInCDskFl()
{
    if (!VerifyInCDskFlInstall()) {
        return false;
    }

    return VerifyInCDskFlLoaded();
}

bool Cluster::QuitRequested(DWORD waitSeconds)
{
    if (WAIT_OBJECT_0 == MsgWaitForMultipleObjects(0, NULL, FALSE, static_cast<unsigned>(waitSeconds * 1000), QS_ALLINPUT)) {
        MSG msg;
        if (0 == GetMessage(&msg, NULL, 0, 0)) {
            return true; // WM_QUIT
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return false;
}

bool Cluster::WaitForMscs()
{
    bool checkCleanUp = true;

    SC_HANDLE scMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (0 == scMan) {
        DebugPrintf(SV_LOG_ERROR, "Cluster::WaitForMscs OpenSCManager failed: %d\n",  GetLastError());
        return false;
    }

    boost::shared_ptr<void> scManGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseServiceHandle, scMan));

    // loop until MSCS installed or told to quit
    SC_HANDLE  schService = 0;
    SERVICE_STATUS serviceStatus;
    do {
        schService = OpenServiceW(scMan, L"ClusSvc", SERVICE_ALL_ACCESS);
        if (NULL != schService) {
            boost::shared_ptr<void> schServiceGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseServiceHandle, schService));
            // MSCS installed loop until it is started or we are told to quit
            do {
                if (QueryServiceStatus(schService, &serviceStatus)) {
                    if (SERVICE_RUNNING == serviceStatus.dwCurrentState) {
                        return true;
                    }
                }

                if (checkCleanUp) {
                    // this is done here beacuse when a node is evicted, the cluster
                    // service is actually stopped for that node, so it does not
                    // get the delete node notification
                    checkCleanUp = m_ClusterTrackingInfo.CleanUpIfNeeded();
                }

                if (QuitRequested(5)) {
                    return false; // want caller to exit so return false
                }
            } while (true);
        }
        if (checkCleanUp) {
            // this is done here beacuse when a node is evicted, the cluster
            // service is actually stopped for that node, so it does not
            // get the delete node notification
            checkCleanUp = m_ClusterTrackingInfo.CleanUpIfNeeded();
        }
    } while (!QuitRequested(5));

    return false; // only get here if quit requested want caller to exit so return false
}

bool Cluster::SetClusterHandle(HCLUSTER & hCluster)
{
    bool clusterOpened = true;

    EnterCriticalSection(&m_CriticalSection);
    if (0 == hCluster) {
        hCluster = OpenCluster(0);
        if (0 == hCluster) {
            clusterOpened = false;
        }
    }
    LeaveCriticalSection(&m_CriticalSection);

    return clusterOpened;
}

bool Cluster::Monitor()
{
    do {
        try {
            std::stringstream msg;

            if (!WaitForMscs()) {
                return false;
            }

			if (!VerifyInCDskFl()) {
                return false;
            }

            g_hCluster = OpenCluster(0);
            if (0 == g_hCluster) {
                DebugPrintf(SV_LOG_WARNING, "FAILED Cluster::Monitor OpenCluster: %d (OK if cluster not setup)\n", GetLastError());
                if (QuitRequested(30)) {
                    return true;
                }
                continue;
            }

            // on restart, cluster information might not be fully populated before it is read
            // BuildClusterTrackingInfo Failed in 'ClusterGroup::GetGroups ClusterOpenEnum:'
            // with error 70 which seems to indicate that the cluster service has started but
            // not yet finished populating the cluster database.
            if (!m_ClusterTrackingInfo.BuildClusterTrackingInfo()) {
				DebugPrintf(SV_LOG_ERROR, "Building cluster tracking information failed. Trying with a refreshed cluster service handle.\n");
				CloseCluster(g_hCluster);
                if (QuitRequested(10)) {
                    return true;
                }
				continue;
            }

			boost::shared_ptr<void> hClusterGuard(static_cast<void*>(0), boost::bind(boost::type<BOOL>(), &CloseCluster, g_hCluster));

            m_ClusterTrackingInfo.Register();

#ifdef CLUSTER_STAND_ALONE
            if (m_CollectAndExit) {
                break;
            }
#endif

#ifndef CLUSTER_STAND_ALONE
			/* TODO: instead of cdputil quit, use the cluster quit 
			 * do not do expensive register host when going down,
			 * in case of register cluster retries */
			if (!CDPUtil::QuitRequested(0))
				RegisterHost(); // this should get cluster info registered correctly for the pasive nodes
#endif

			/* If this returns, means that cluster thread is exiting */
            if (HandleClusterNotifications()) {
                return true;
            }

        } catch (ContextualException const & ie) {
            DebugPrintf(SV_LOG_ERROR, "%s", ie.what());
        } catch (std::exception const & e) {
            DebugPrintf(SV_LOG_ERROR, "%s", e.what());
        } catch (...) {
            DebugPrintf(SV_LOG_ERROR, "Failed Cluster::Monitor unknown exception\n");
        }

#ifdef CLUSTER_STAND_ALONE
        if (m_CollectAndExit) {
            break;
        }
#endif
    } while (!QuitRequested(5));

    m_Exited = true;

    return true;
}

unsigned __stdcall Cluster::MonitorThread(LPVOID pParam)
{
    try {
        Cluster* cluster = (Cluster*)pParam;
        if (!cluster->Monitor()) {
            return ERROR_DEPENDENT_SERVICES_RUNNING;
        }
        return ERROR_SUCCESS;
    } catch (std::exception const & e) {
        DebugPrintf(SV_LOG_ERROR, "Failed Cluster::MonitorThread %s\n", e.what());
    }

    return ERROR_FUNCTION_FAILED;
}

DWORD Cluster::StartMonitor()
{
    DWORD result = ERROR_SUCCESS;
    m_MonitorHandle = (HANDLE)_beginthreadex(0, 0, Cluster::MonitorThread, (void*)this, 0, &m_MonitorThreadId);
    if (0 == m_MonitorHandle) {
        result = errno;
        DebugPrintf(SV_LOG_ERROR, "FAILED Cluster::Monitor _beginthreadex: \n", result);
    }
    return result;
}

void Cluster::StopMonitor()
{
    if (0 != m_MonitorThreadId) {
        PostThreadMessage(m_MonitorThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(m_MonitorHandle, INFINITE);//60 * 1000); // give the thread 1 minute to shut
    }
}

void Cluster::RegistryValueChanged(WCHAR const * name)
{
    // FIXME: implment this will allows to detect if drive letters change
}

void Cluster::ResourceStateChanged(WCHAR const * name)
{
    m_ClusterTrackingInfo.ResourceStateChanged(name);
}

void Cluster::ResourceAdded(WCHAR const * name)
{
    m_ClusterTrackingInfo.ResourceAdded(name);
}

void Cluster::ResourceDeleted(WCHAR const * name)
{
    m_ClusterTrackingInfo.ResourceDeleted(name);
}

void Cluster::ResourcePropertyChanged(WCHAR const * name)
{
    m_ClusterTrackingInfo.ResourcePropertyChanged(name);
}

void Cluster::GroupDeleted(WCHAR const * name)
{
    m_ClusterTrackingInfo.GroupDeleted(name);
}

void Cluster::GroupPropertyChanged(WCHAR const * name)
{
    m_ClusterTrackingInfo.GroupPropertyChanged(name);
}

void Cluster::NodeAdded(WCHAR const * name)
{
    m_ClusterTrackingInfo.NodeAdded(name);
}

void Cluster::NodePropertyChanged(WCHAR const * name)
{
    m_ClusterTrackingInfo.NodePropertyChanged(name);
}

void Cluster::NodeDeleted(WCHAR const * name)
{
    m_ClusterTrackingInfo.NodeDeleted(name);
}

void PrintDiskVolumeInfoHeader(DiskVolumeInfoHeader *p)
{
    std::stringstream ss;
    ss << "DiskVolumeInfoHeader: m_VolumeCount = " << p->m_VolumeCount;
    DebugPrintf(SV_LOG_DEBUG, "%s\n",ss.str().c_str());
}

void PrintDiskVolumeInfo(DiskVolumeInfo *p)
{
    DebugPrintf(SV_LOG_DEBUG, "===== DiskVolumeInfo: start =====\n");
    std::string s;

    for (int i = 0; i < DISK_OFFSET_LENGTH; i++)
    {
        std::stringstream ss;
        ss << std::hex;
        ss << std::setfill('0') << std::setw(2);
        ss << (unsigned int)(p->m_StartOffset[i]);
        s += ss.str();
        s += ' ';
    }
    DebugPrintf(SV_LOG_DEBUG, "m_StartOffset: %s\n", s.c_str());

    s.clear();
    for (int i = 0; i < DISK_LENGTH_LENGTH; i++)
    {
        std::stringstream ss;
        ss << std::hex;
        ss << std::setfill('0') << std::setw(2);
        ss << (unsigned int)(p->m_Length[i]);
        s += ss.str();
        s += ' ';
    }
    DebugPrintf(SV_LOG_DEBUG, "m_Length: %s\n", s.c_str());

    s.clear();
    for (int i = 0; i < DISK_NUMBER_LENGTH; i++)
    {
        std::stringstream ss;
        ss << std::hex;
        ss << std::setfill('0') << std::setw(2);
        ss << (unsigned int)(p->m_Number[i]);
        s += ss.str();
        s += ' ';
    }
    DebugPrintf(SV_LOG_DEBUG, "m_Number: %s\n", s.c_str());

    DebugPrintf(SV_LOG_DEBUG, "m_Drive: %u\n", p->m_Drive);

    s.clear();
    for (int i = 0; i < CLUSTERDISK_GUID_LENGTH; i++)
    {
        std::stringstream ss;
        ss << std::hex;
        ss << std::setfill('0') << std::setw(2);
        ss << (unsigned int)(p->m_Guid[i]);
        s += ss.str();
        s += ' ';
    }
    DebugPrintf(SV_LOG_DEBUG, "m_Guid: %s\n", s.c_str());
    DebugPrintf(SV_LOG_DEBUG, "===== DiskVolumeInfo: end =====\n");
}

