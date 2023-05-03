#ifndef INMAGE_UTIL__H
#define INMAGE_UTIL__H

#pragma once 

#include "stdafx.h"
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <Windows.h>
#include "VssRequestor.h"
#include "common.h"
#include "devicefilter.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "VacpErrorCodes.h"

#define SV_NTFS_MAGIC_OFFSET  3
#define SV_MAGIC_NTFS         "NTFS    "
#define SV_MAGIC_SVFS         "SVFS0001"
#define SV_FSMAGIC_LEN        8

#define MAXIPV4ADDRESSLEN 15

#define NELEMS(ARR) ((sizeof (ARR)) / (sizeof (ARR[0])))

// Helper macros to print a GUID using printf-style formatting
#define WSTR_GUID_FMT  L"{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"

#define GUID_PRINTF_ARG( X )                                \
    (X).Data1,                                              \
    (X).Data2,                                              \
    (X).Data3,                                              \
    (X).Data4[0], (X).Data4[1], (X).Data4[2], (X).Data4[3], \
    (X).Data4[4], (X).Data4[5], (X).Data4[6], (X).Data4[7]

typedef std::set<std::string> strset_t;

typedef struct _Writer2Application
{
	std::string VssWriterName;
	std::string ApplicationName;//this will be same as VssWriterName when we are populating this structure dynamically.
	USHORT usStreamRecType;//this cannot be populated when we are dynamically filling the structure by calling GatherVssAppsInfo!
	std::string VssWriterInstanceName;
	std::string VssWriterInstanceId;
	std::string VssWriterId;

}Writer2Application_t;

typedef enum ENUM_SCRIPT_TYPE
{
  ST_PRESCRIPT = 1,
  ST_POSTSCRIPT= 2
}SCRIPT_TYPE;
/**************************************************************************************************/
typedef struct _VacpBpb {
    unsigned char  BS_jmpBoot[3];  /* Jump instruction to the boot code */
    unsigned char  BS_OEMName[8];  /* The name of the system that formatted the volume */
    unsigned short BPB_BytsPerSec; /* How many bytes in a sector (should be 512) */
    unsigned char  BPB_SecPerClus; /* How many sectors are in a cluster (FAT-12 should be 1) */
    unsigned short BPB_RsvdSecCnt; /* Number of sectors that are reserved (FAT-12 should be 1) */
    unsigned char  BPB_NumFATs;    /* The number of FAT tables on the disk (should be 2) */
    unsigned short BPB_RootEntCnt; /* Maximum number of directory entries in the root directory */
    unsigned short BPB_TotSec16;   /* FAT-12 total number of sectors on the disk */
    unsigned char  BPB_Media;      /* Code for media type {fixed, removable, etc.} */
    unsigned short BPB_FATSz16;    /* FAT-12 number of sectors that each FAT table occupies (should be 9) */
    unsigned short BPB_SecPerTrk;  /* Number of sectors in one cylindrical track */
    unsigned short BPB_NumHeads;   /* Number of heads for this volume (2 heads on a 1.4Mb 3.5 inch floppy) */
    unsigned long  BPB_HiddSec;    /* Number of preceding hidden sectors (0 for non-partitioned media) */
    unsigned long  BPB_TotSec32;   /* FAT-32 number of sectors on the disk (0 for FAT-12) */
    unsigned char  BS_DrvNum;      /* A drive number for the media (OS specific) */
    unsigned char  BS_Reserved1;   /* Reserved space for Windows NT (when formatting, set to 0) */
    unsigned char  BS_BootSig;     /* Indicates that the following three fields are present (0x29) */
    unsigned long  BS_VolID;       /* Volume serial number (for tracking this disk) */
    unsigned char  BS_VolLab[11];  /* Volume label (matches label in the root directory, or "NO NAME    ") */
    unsigned char  BS_FilSysType[8];/* Deceptive FAT type Label that may or may not indicate the FAT type */
} VacpBpb;

const char FST_SCTR_OFFSET_510_VALUE = (char) 0x55;
const char FST_SCTR_OFFSET_511_VALUE = (char) 0xAA;

int const BS_JMP_BOOT_VALUE_0xEB = 0xEB;
int const BS_JMP_BOOT_VALUE_0xE9 = 0xE9;

int const FST_SCTR_OFFSET_510 = 510;
int const FST_SCTR_OFFSET_511 = 511;
/**************************************************************************************************/

typedef struct _TagData
{
	unsigned short len;
	char *data;
}TagData_t;


typedef struct _ServerDeviceID
{
	unsigned short len;
	char *data;
}ServerDeviceID_t;

// Used to automatically release the contents of a VSS_SNAPSHOT_PROP structure 
// (but not the structure itself)
// when the instance of this class goes out of scope
// (even if an exception is thrown)
#if (_WIN32_WINNT >= 0x502)
class SmartSnapshotPropMgr
{
public:
    SmartSnapshotPropMgr(VSS_SNAPSHOT_PROP * ptr): m_ptr(ptr) {};
    ~SmartSnapshotPropMgr() { ::VssFreeSnapshotProperties(m_ptr); }
private:
    VSS_SNAPSHOT_PROP * m_ptr;
};
#endif

unsigned long long GetTimeInSecSinceEpoch1970();

DWORD GetVolumeMountPoints(const std::string& volumeGuid, std::list<std::string>& mountPoints, std::string &errmsg);

// Get the volume's root path name for the given path
HRESULT  GetVolumeRootPath(const char *src, char *dst, unsigned int dstlen, bool validateMountPoint = true);
HRESULT  GetVolumeRootPathEx(const char *src, char *dst, unsigned int dstlen, bool& validateMountPoint);

bool IsValidVolume(const char *srcVol, bool validateMountPoint = true);
bool IsValidVolumeEx(const char *srcVol, bool bCheckIfItCanBeSnapshot = false, CComPtr<IVssBackupComponentsEx> ptrVssBackupComponentsEx = NULL);

HRESULT IsThisFATfsHiddenVolume(/* in */ const char *);
HRESULT IsThisNtfsHiddenVolume(/* in */ const char *);
bool IsVolumeLocked(const char * );
HRESULT OpenVolume( /* out */ HANDLE *,
                   /* in */  const char   *
                   );
HRESULT CloseVolume( HANDLE  );


HRESULT OpenVolumeExtended( /* out */ HANDLE *,
                           /* in */  const char   *,
                           /* in */  DWORD  
                           );

bool IsItVolumeMountPoint(const char* volumeName);

std::string  GetLocalSystemTime();
bool GetSystemUptimeInSec(unsigned long long& uptime);
bool ExtractAppsWithSR(std::string &strApps,std::string &strNewApps);
bool IsThisVolumeProtected(std::string strVol);
std::string GetFormattedFailMsg(const FailMsgList &failMsgList, const std::string &delm);

bool IsValidApplication(const char *appName);

HRESULT GetVolumeIndexFromVolumeName(const char *volumeName, DWORD *pVolumeIndex, bool validateMountPoint = true);
HRESULT GetVolumeIndexFromVolumeNameEx(const char *volumeName, DWORD *pVolumeIndex,bool validateMountPoint);

bool ValidateSnapshotVolumes(DWORD dwDriveMask);

const char *MapVssWriter2Application(const char *VssWriterName);

const char *MapApplication2VssWriter(const char *AppName);

#if (_WIN32_WINNT > 0x500)

void PrintVssWritersInfo(std::vector<VssAppInfo_t> &AppInfo);

void PrintFormattedVssWritersInfo(std::vector<VssAppInfo_t> &AppInfo, std::vector<AppParams_t> &CmdLineAppList);

DWORD MapApplication2VolumeBitMask(const char *AppName, std::vector<VssAppInfo_t> &AppInfo);

DWORD MapVssWriter2VolumeBitMask(const char *VssWriterName, std::vector<VssAppInfo_t> &AppInfo);

void MapApplication2VolumeList(const char *AppName, std::vector<VssAppInfo_t> &AppInfo,std::vector<std::string> &Vols);

void MapVssWriter2VolumeList(const char *VssWriterName, std::vector<VssAppInfo_t> &AppInfo, std::vector<std::string> &Vols);

bool GetApplicationComponentInfo(const char *AppName, const char *CompName,  std::vector<VssAppInfo_t> &AppInfo, ComponentInfo_t &AppComponent);

bool FillAppSummaryAllComponentsInfo(AppSummary_t &ASum, vector<VssAppInfo_t> &AppInfo );

void GetVolumeMountPointInfoForApp(const char *AppName, std::vector<VssAppInfo_t> &AppInfo, 
							   VOLMOUNTPOINT_INFO &volMountPoint_info);

#endif

BOOL GetVolumesList(std::vector<const char *> &LogicalVolumes);

BOOL ProcessVolMntPntsForGivenVol (HANDLE hVol, char *volGuidBuf, int iBufSize,vector<const char *> &LogicalVolumes);
BOOL GetMountPointsList(vector<const char *> &LogicalVolumes);

void GetApplicationsList(std::vector<std::string> &Applications);


HRESULT MapApplication2StreamRecType(const char *AppName, USHORT &usStreamRecType);

HRESULT MapStreamRecType2Application(const char **tagString, USHORT usStreamRecType);

HRESULT ValidateTagData(TagData_t *pTagData, bool bPrintTagDataAsString = true);

bool IsVolMountPointAlreadyExist(VOLMOUNTPOINT_INFO& targetVolMP_info, std::string volMountPoint);

bool IsThisDiskProtected(const std::string &strVol, const std::map<std::wstring, std::string> &diskGuidMap);

bool IsItValidDiskName(const char *srcDiskName);
HRESULT GetDiskNamesForVolume(const char *volName, vector<string> &diskNames);
HRESULT GetDiskInfo();

HRESULT GetBootableVolumes(std::vector<std::string> &bootableVolumeGuids);

HRESULT getAgentInstallPath(string &strInstallPath);

std::string convertWstringToUtf8(const std::wstring &wstr);
std::wstring convertUtf8ToWstring(const std::string &str);
const std::string GetHostName();
void GetIPAddressInAddrInfo(const std::string &hostName, strset_t &ipsInAddrInfo, std::string &errMsg);
std::string ValidateAndGetIPAddress(const std::string &ipAddress, std::string &errMsg);
HRESULT GetLocalIpFromWmi(std::vector<std::string> &ipAddress);

//Generates 8 digit unique string.
std::string getUniqueSequence();
bool MountPointToGUID(PTCHAR VolumeMountPoint, WCHAR *VolumeGUID, ULONG ulBufLen);
HRESULT GetSyncTagVolumeStatus();
HRESULT GetTagVolumeStatus();

#if(_WIN32_WINNT >=0x502)
DWORD MapVssWriter2VolumeBitMaskEx(const char *VssWriterName, const char *VssWriterInstanceName, std::vector<VssAppInfo_t> &AppInfo,bool bWriterInstanceName);
DWORD MapApplication2VolumeBitMaskEx(const char *AppName, const char *WriterInstanceName, std::vector<VssAppInfo_t> &AppInfo,bool bWriterInstanceName);
const char *MapWriterName2VssWriter(const char *AppName);
void MapApplication2VolumeListEx(const char *AppName,const char *VssWriterInstanceName, std::vector<VssAppInfo_t> &AppInfo, std::vector<std::string> &Vols, bool bWriterInstanceName);
void MapVssWriter2VolumeListEx(const char *VssWriterName,const char *VssWriterInstanceName, std::vector<VssAppInfo_t> &AppInfo, std::vector<std::string> &Vols, bool bWriterInstanceName);
void GetVolumeMountPointInfoForAppEx(const char *AppName,const char *VssWriterInstanceName, std::vector<VssAppInfo_t> &AppInfo, VOLMOUNTPOINT_INFO &volMountPoint_info,bool bWriterInstanceName);

#endif
bool IsDriverLoaded(_TCHAR* szDriverName);
DWORD CheckDriverStat(HANDLE hVFCtrlDevice, WCHAR* volumeGUID, etWriteOrderState *pDriverWriteOrderState, DWORD *dwLastError, DWORD dwVolumeGUIDSize = 0);
//Bug# 4853 -vacp wrongly issuing tag to base drive of mount point
//Stop using IS_MOUNT_POINT as it is not always give the right answer  
//For example F:M where M is a mount mount but this macro will not consider this as mount point
//Some Windows API accept mounpoint path as F:M. NO need to pass "\" after :( F:\M)
//Since Windows API is accepting this as valid path, VACP sohuld not reject this path
//hence using IsItVolumeMountPoint() instead of the macro
//#define IS_MOUNT_POINT(x)       ((_tcslen(x) > 3) && x[1] == _T(':') && x[2] == _T('\\'))

DWORD ExecuteScript(std::string strScript,SCRIPT_TYPE scriptType);
#endif INMAGE_UTIL__H
