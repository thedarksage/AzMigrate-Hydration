#include <windows.h>
#include <atlbase.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include <iomanip>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include "ListEntry.h"
#include <InmFltInterface.h>
#include "drvutil.h"
#include "VFltCmdsExec.h"
#include "svtypes.h"
//#include "svsvolume.h"
#include "TagDetails.h"
#include "DBWaitEvent.h"
#include "SVDstream.h"
#include <time.h>
#include <iostream>
#include <winreg.h>

#define _WIN32_DCOM

#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

#define S2TIMEOUT 65000 // set timeout to 65 seconds to simulate S2 behavior
tGetSCISIDiskCapacityByName GetSCSICapacity = (tGetSCISIDiskCapacityByName)GetProcAddress(LoadLibrary(L"diskgeneric.dll"), "GetSCISIDiskCapacityByName");
tGetDriveGeometry GetDriveGeometry = (tGetDriveGeometry)GetProcAddress(LoadLibrary(L"diskgeneric.dll"), "GetDriveGeometry");
tReadSectorsFromDiskDriveAndWriteToFile RDiskSectorsWriteFile = (tReadSectorsFromDiskDriveAndWriteToFile)GetProcAddress(LoadLibrary(L"diskgeneric.dll"), "ReadSectorsFromDiskDriveAndWriteToFile");
bool GetFileSystemVolumeSize(char *vol, SV_ULONGLONG* size, unsigned int* sectorSize);
bool CanReadDriverSize(char *vol, SV_ULONGLONG offset, unsigned int sectorSize);

using namespace std;

extern LIST_ENTRY               PendingTranListHead;
extern COMMAND_LINE_OPTIONS     sCommandOptions;

extern STRING_TO_ULONG ModuleStringMapping[];
extern STRING_TO_ULONG LevelStringMapping[];

extern std::vector<CDBWaitEvent *> DBWaitEventVector;

static TSSNSEQID   PrevTSSequenceNumber   = {0,0,0};
static TSSNSEQIDV2 PrevTSSequenceNumberV2 = {0,0,0};
static bool        FirstCommit=0;

extern bool IsDiskFilterRunning;

bool bControlPrintStats = false;

TCHAR * ServiceStringArray[] = 
{
    _T("Service State Unknown"),
    _T("Service Not Started"),
    _T("Service Running"),
    _T("Service Shutdown")
};

TCHAR * BitMapStateStringArray[] = 
{
    _T("ecVBitmapStateUnInitialized"),
    _T("ecVBitmapStateOpened"),
    _T("ecVBitmapStateReadStarted"),
    _T("ecVBitmapStateReadPaused"),
    _T("ecVBitmapStateAddingChanges"),
    _T("ecVBitmapStateReadCompleted"),
    _T("ecVBitmapStateClosed"),
    _T("ecVBitmapStateReadError"),
    _T("ecVBitmapStateInternalError"),
};

const char *WriteOrderStateStringArray[] =
{
    "ecWriteOrderStateUnInitialized",
    "ecWriteOrderStateBitmap",
    "ecWriteOrderStateMetadata",
    "ecWriteOrderStateData"
};

const char *CaptureModeStringArray[] =
{
    "ecCaptureModeUninitialized",
    "ecCaptureModeMetaData",
    "ecCaptureModeData"
};

TCHAR * DataSourceStringArray[] =
{
    _T("DataSourceUnknown"),
    _T("DataSourceBitmap"),
    _T("DataSourceMetaData"),
    _T("DataSourceData")
};

const char *TagStatusStringArray[] = 
{
    "Commited",
    "Pending",
    "Deleted",
    "Dropped",
    "Invalid GUID",
    "Volume filtering stopped",
    "Unattempted",
    "Failure",
};

void
WriteStream(PCOMMAND_STRUCT pCommand)
{
    _tprintf(_T("Volume Write Command\n"));
    _tprintf(_T("MountPoint: %s\n"), pCommand->data.WriteStreamData.MountPoint);
    _tprintf(_T("MaxIO: %ld\n"), pCommand->data.WriteStreamData.nMaxIOs);
    _tprintf(_T("Io Size Upper Bound: %ld\n"), pCommand->data.WriteStreamData.nMaxIOSz);
    _tprintf(_T("Io Size Lower Bound: %ld\n"), pCommand->data.WriteStreamData.nMinIOSz);
    _tprintf(_T("Seconds To Run: %ld\n"), pCommand->data.WriteStreamData.nSecsToRun);
    _tprintf(_T("Writes Per Tag: %ld\n"), pCommand->data.WriteStreamData.nWritesPerTag);
}

void
ReadStream(HANDLE hVFCtrlDevice, PREAD_STREAM_DATA pReadStreamData)
{
    _tprintf(_T("Read Command\n"));
    _tprintf(_T("MountPoint: %s\n"), pReadStreamData->MountPoint);
}

TCHAR*
UlongToDebugLevelString(ULONG ulValue)
{
    int     i;

    for (i = 0; NULL != LevelStringMapping[i].pString; i++) {
        if (ulValue == LevelStringMapping[i].ulValue) {
            return LevelStringMapping[i].pString;
        }
    }

    return DL_STR_UNKNOWN;
}


TCHAR *
BitMapStateString(
    etVBitmapState eBitMapState
    )
{
    TCHAR *BitMapStateString = NULL;
    switch (eBitMapState) {
        case ecVBitmapStateUnInitialized:
        case ecVBitmapStateOpened:
        case ecVBitmapStateReadStarted:
        case ecVBitmapStateReadPaused:
        case ecVBitmapStateReadCompleted:
        case ecVBitmapStateAddingChanges:
        case ecVBitmapStateClosed:
        case ecVBitmapStateReadError:
        case ecVBitmapStateInternalError:
            BitMapStateString = BitMapStateStringArray[eBitMapState];
            break;
        default:
            BitMapStateString = _T("ecBitMapStateUnknown");
            break;
    }

    return BitMapStateString;
}

const char *
WriteOrderStateString(
    etWriteOrderState eWOState
    )
{
    const char *WOStateString = NULL;
    switch(eWOState) {
    case ecWriteOrderStateUnInitialized:
    case ecWriteOrderStateBitmap:
    case ecWriteOrderStateMetadata:
    case ecWriteOrderStateData:
        WOStateString = WriteOrderStateStringArray[eWOState];
        break;
    default:
        WOStateString = "ecWriteOrderStateUnknown";
        break;
    }

    return WOStateString;
}

const char *
WOStateStringShortForm(etWriteOrderState eWOState)
{
    switch(eWOState) {
    case ecWriteOrderStateUnInitialized:
        return "Un";
    case ecWriteOrderStateBitmap:
        return "Bi";
    case ecWriteOrderStateMetadata:
        return "MD";
    case ecWriteOrderStateData:
        return "Da";
        break;
    default:
        return "UnKn";
        break;
    }

    return "UnKn";
}

const char *
WOSchangeReasonShortForm(etWOSChangeReason eWOSChangeReason)
{
    switch (eWOSChangeReason) {
		case ecWOSChangeReasonUnInitialized :
            return "UnInit";
		case eCWOSChangeReasonServiceShutdown :
            return "SerShut";
		case ecWOSChangeReasonBitmapChanges :
            return "Bi-Ch";
		case ecWOSChangeReasonBitmapNotOpen :
            return "BiNotOp";
		case ecWOSChangeReasonBitmapState :
            return "BiState";
		case ecWOSChangeReasonCaptureModeMD :
            return "CaMo-MD";
		case ecWOSChangeReasonMDChanges :
            return "MD-Ch";
		case ecWOSChangeReasonDChanges :
            return "Da-Ch";
        case ecWOSChangeReasonDontPageFault :
            return "NoPageF";
        case ecWOSChangeReasonPageFileMissed :
            return "PfileM";
        default:
            return "UnKnown";
    }

    return "UnKnown";
}

const char *
CaptureModeString(
    etCaptureMode eCaptureMode
    )
{
    const char *CMstring = NULL;

    switch(eCaptureMode) {
    case ecCaptureModeUninitialized:
    case ecCaptureModeData:
    case ecCaptureModeMetaData:
        CMstring = CaptureModeStringArray[eCaptureMode];
        break;
    default:
        CMstring = "ecCaptureModeUnknown";
        break;
    }

    return CMstring;
}

TCHAR *
DataSourceToString(
    ULONG   ulDataSource
    )
{
    TCHAR *DataSourceString = NULL;
    switch(ulDataSource) {
        case INVOLFLT_DATA_SOURCE_BITMAP:
        case INVOLFLT_DATA_SOURCE_META_DATA:
        case INVOLFLT_DATA_SOURCE_DATA:
            DataSourceString = DataSourceStringArray[ulDataSource];
            break;
        case INVOLFLT_DATA_SOURCE_UNDEFINED:
        default:
            DataSourceString = DataSourceStringArray[INVOLFLT_DATA_SOURCE_UNDEFINED];
            break;
    }

    return DataSourceString;
}

TCHAR *
ServiceStateString(
    etServiceState eServiceState
    )
{
    TCHAR *ServiceString = NULL;

    switch (eServiceState) {
        case ecServiceNotStarted:
        case ecServiceRunning:
        case ecServiceShutdown:
            ServiceString = ServiceStringArray[eServiceState];
            break;
        default:
            ServiceString = ServiceStringArray[ecServiceUnInitiliazed];
            break;
    }
    
    return ServiceString;
}

void
ConvertSizeToString(
    ULONG   ulSize,
    TCHAR   *szSize,
    ULONG   ulszSize
    )
{
    double   dSize = ulSize;

    if (ulSize/(1024 * 1024)) {
        // Data is in Meg
        if (ulSize % (1024*1024)) {
            dSize = dSize/(1024 * 1024);
            _sntprintf(szSize, ulszSize, _T("%.2fM"), dSize);
        } else {
            ulSize = ulSize/(1024*1024);
            _sntprintf(szSize, ulszSize, _T("%dM"), ulSize);
        }
    } else if (ulSize / 1024) {
        if (ulSize % 1024) {
            dSize = dSize/1024;
            _sntprintf(szSize, ulszSize, _T("%.2fK"), dSize);
        } else {
            ulSize = ulSize/1024;
            _sntprintf(szSize, ulszSize, _T("%dK"), ulSize);
        }
    } else {
        _sntprintf(szSize, ulszSize, _T("%d"), ulSize);        
    }
}

std::string&
ConvertSizeToString(
    ULONGLONG       ullSize,
    std::string&    str
    )
{
    std::ostringstream OStr;
    ULONGLONG TerB, GigB, MegB, KilB;

    TerB =  ullSize / ((ULONGLONG)(1024 * 1024) * (ULONGLONG)(1024 * 1024));

    if (TerB) {
        OStr << TerB << "T";
    } else {
        GigB = ullSize / (1024 * 1024 * 1024);
        if (GigB) {
            OStr << GigB << "G";
        } else {
            MegB = ullSize / (1024 * 1024);
            if (MegB) {
                OStr << MegB << "M";
            } else {
                KilB = ullSize / 1024;
                if (KilB) {
                    OStr << KilB << "K";
                } else {
                    OStr << ullSize << "B";
                }                
            }
        }
    }

    str = OStr.str();
    return str;
}

// String format
// XXXG, XXXM, XXXK, XXXB - 24 characters string
std::string&
ConvertULLtoString(
    ULONGLONG   ullSize,
    std::string& str
    )
{
    std::ostringstream OStr;

    if (0 == ullSize) {
        OStr << "0";
        str = OStr.str();
        return str;
    }

    ULONG ulGB = (ULONG)(ullSize / (1024 * 1024 * 1024));

    ullSize = ullSize % (1024 * 1024 * 1024);

    ULONG ulMB = (ULONG)(ullSize / (1024 * 1024));

    ullSize = ullSize % (1024 * 1024);

    ULONG ulKB = (ULONG)(ullSize / 1024);

    ullSize = ullSize % 1024;

    if (ulGB) {
        OStr << ulGB << "G";
    }

    if (ulMB) {
        if (!OStr.str().empty()) {
            OStr << ", " << ulMB << "M";
        } else {
            OStr << ulMB << "M";
        }
    }

    if (ulKB) {
        if (!OStr.str().empty()) {
            OStr << ", " << ulKB << "K";
        } else {
            OStr << ulKB << "K";
        }
    }

    if (ullSize) {
        if (!OStr.str().empty()) {
            OStr << ", " << ullSize << "B";
        } else {
            OStr << ullSize << "B";
        }
    }

    str = OStr.str();
    return str;
}


void
ConvertULtoString(
    ULONG   ulSize,
    std::string& str
    )
{
    std::ostringstream OStr;

    if (0 == ulSize) {
        OStr << "0";
        str = OStr.str();
        return;
    }

    ULONG ulGB = (ulSize / (1024 * 1024 * 1024));

    ulSize = ulSize % (1024 * 1024 * 1024);

    ULONG ulMB = (ulSize / (1024 * 1024));

    ulSize = ulSize % (1024 * 1024);

    ULONG ulKB = (ulSize / 1024);

    ulSize = ulSize % 1024;

    if (ulGB) {
        OStr << ulGB << "G";
    }

    if (ulMB) {
        if (!OStr.str().empty()) {
            OStr << ", " << ulMB << "M";
        } else {
            OStr << ulMB << "M";
        }
    }

    if (ulKB) {
        if (!OStr.str().empty()) {
            OStr << ", " << ulKB << "K";
        } else {
            OStr << ulKB << "K";
        }
    }

    if (ulSize) {
        if (!OStr.str().empty()) {
            OStr << ", " << ulSize << "B";
        } else {
            OStr << ulSize << "B";
        }
    }

    str = OStr.str();
    return;
}

std::string&
ConvertFileTimeToString(
    FILETIME    *Time,
    std::string& str
    )
{
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;
    std::ostringstream      OStr;

    FileTimeToSystemTime(Time, &SystemTime);
    GetTimeZoneInformation(&TimeZone);
    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);

    // format is %02d/%02d/%04d %02d:%02d:%02d:%04d

    OStr << std::setfill('0') << std::setw(2) << LocalTime.wMonth << "/" << std::setw(2) << LocalTime.wDay << "/" << std::setw(4) << LocalTime.wYear;
    OStr << " " << std::setw(2) << LocalTime.wHour << ":" << std::setw(2) << LocalTime.wMinute << ":";
    OStr << std::setw(2) << LocalTime.wSecond << ":" << std::setw(4) << LocalTime.wMilliseconds;

    str = OStr.str();
    return str;
}

void
ConvertULMicroSecondsToString(
    ULONG   ul,
    std::string& str
    )
{
    std::ostringstream OStr;

    if (0 == ul) {
        OStr << "0 us";
        str = OStr.str();
        return;
    }

    ULONG ulmin = (ul / (1000 * 1000 * 60));

    ul = ul % (60 * 1000 * 1000);

    ULONG ulsec = (ul / (1000 * 1000));

    ul = ul % (1000 *1000);

    ULONG ulMilliSec = (ul / 1000);

    ul = ul % 1000;

    if (ulmin) {
        OStr << ulmin << "M";
    }

    if (ulsec) {
        if (!OStr.str().empty()) {
            OStr << ":" << ulsec << "S";
        } else {
            OStr << ulsec << "S";
        }
    }

    if (ulMilliSec) {
        if (!OStr.str().empty()) {
            OStr << ":" << ulMilliSec << "mS";
        } else {
            OStr << ulMilliSec << "mS";
        }
    }

    if (ul) {
        if (!OStr.str().empty()) {
            OStr << ":" << ul << "uS";
        } else {
            OStr << ul << "uS";
        }
    }

    str = OStr.str();
    return;
}

// MaxSize of String
// DDDdHHhMMmSSs 13 characters
std::string&
ConvertULSecondsToString(
    ULONG   ulSec,
    std::string& str
    )
{
    std::ostringstream OStr;

    if (0 == ulSec) {
        OStr << "0s";
        str = OStr.str();
        return str;
    }

    ULONG ulMin = ulSec / 60;
    ulSec = ulSec % 60;

    ULONG ulHr = ulMin / 60;
    ulMin = ulMin % 60;

    ULONG ulDa = ulHr / 24;
    ulHr = ulHr % 24;

    if (ulDa) {
        OStr << ulDa << "d";
    }

    if (ulHr) {
        if (!OStr.str().empty()) {
            OStr << ":" << ulHr << "h";
        } else {
            OStr << ulHr << "h";
        }
    }

    if (ulMin) {
        if (!OStr.str().empty()) {
            OStr << ":" << ulMin << "m";
        } else {
            OStr << ulMin << "m";
        }
    }

    if (ulSec) {
        if (!OStr.str().empty()) {
            OStr << ":" << ulSec << "s";
        } else {
            OStr << ulSec << "s";
        }
    }

    str = OStr.str();
    return str;
}

void
PrintStr(char *format, ULONGLONG ull)
{
    std::string str;
    ConvertULLtoString(ull, str);

    printf(format, str.c_str());
}

void
PrintStr(char *format, ULONG ul)
{
    std::string str;
    ConvertULtoString(ul, str);

    printf(format, str.c_str());
}

void
PrintMicroSeconds(char *format, ULONG ul)
{
    std::string str;

    ConvertULMicroSecondsToString(ul, str);

    printf(format, str.c_str());
}

void
ClearDiffs(HANDLE hVFCtrlDevice, PCLEAR_DIFFERENTIALS_DATA pClearDifferentialsData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
                     pClearDifferentialsData->VolumeGUID,
                     sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                     NULL,
                     0,
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (bResult)
        _tprintf(_T("Returned Success from Clear Bitmap DeviceIoControl call for volume %s\n"), pClearDifferentialsData->MountPoint);
    else
        _tprintf(_T("Returned Failed from Clear Bitmap DeviceIoControl call for volume %s %d\n"), 
        pClearDifferentialsData->MountPoint, GetLastError());

    return;
}

void
SetDiskClustered(HANDLE hVFCtrlDevice, PSET_DISK_CLUSTERED setDiskClustered)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;
    SET_DISK_CLUSTERED_INPUT	setClusterFiltering = { 0 };

    StringCchPrintfW(setClusterFiltering.DeviceId, sizeof(setClusterFiltering.DeviceId), setDiskClustered->VolumeGUID, sizeof(setDiskClustered->VolumeGUID));

    bResult = DeviceIoControl(
        hVFCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_DISK_CLUSTERED,
        &setClusterFiltering,
        sizeof(setClusterFiltering),
        NULL,
        0,
        &dwReturn,
        NULL        // Overlapped
    );
    if (bResult)
        _tprintf(_T("Returned Success from Set Disk Clustered call for volume %s\n"), setDiskClustered->VolumeGUID);
    else
        _tprintf(_T("Returned Failed from Clear Bitmap DeviceIoControl call for volume %s %d\n"),
            setDiskClustered->VolumeGUID, GetLastError());

    return;
}


int GetLcn(HANDLE hVFCtrlDevice, PDRIVER_GET_LCN pDriverGetLcn) {
    int ret = 1;
    BOOL    bResult = 0;
    DWORD   dwReturn;
    GET_LCN GetLcn;
    GetLcn.StartingVcn.QuadPart = pDriverGetLcn->StartingVcn;
    GetLcn.usFileNameLength = pDriverGetLcn->usLength * sizeof(WCHAR);
    memmove(GetLcn.FileName, pDriverGetLcn->FileName, pDriverGetLcn->usLength * sizeof(WCHAR));    
    GetLcn.FileName[pDriverGetLcn->usLength] = L'\0';
    _tprintf(_T("Input FileName %s\n"), GetLcn.FileName);
    
    while (!bResult) {
        RETRIEVAL_POINTERS_BUFFER RetPointerBuffer;
        _tprintf(_T("\nInput Vcn %#I64x\n"), GetLcn.StartingVcn.QuadPart);
        bResult = DeviceIoControl(
                hVFCtrlDevice,
                (DWORD)IOCTL_INMAGE_GET_LCN,
                &GetLcn,
                sizeof(GET_LCN),
                &RetPointerBuffer,
                sizeof(RetPointerBuffer),
                &dwReturn, 
                NULL        // Overlapped
                );
        if (bResult || (ERROR_MORE_DATA == GetLastError())) { //STATUS_BUFFER_OVERFLOW
            _tprintf(_T("ExtentCount %d Vcn %I64x\n"), RetPointerBuffer.ExtentCount, RetPointerBuffer.StartingVcn.QuadPart);

            for (DWORD i = 0; i < RetPointerBuffer.ExtentCount; i++) {
                if (((LONGLONG) - 1) == RetPointerBuffer.Extents[i].Lcn.QuadPart) {
                    bResult = 1;
                    _tprintf(_T("Skipping LCN as either a compression unit that is partially allocated, or an unallocated region of a sparse file\n"));
                    break;
                } else {
                    _tprintf(_T("Extent LCN %I64x, Length %I64x\n"), RetPointerBuffer.Extents[i].Lcn.QuadPart, 
                            ((i>0) ? (RetPointerBuffer.Extents[i].NextVcn.QuadPart-RetPointerBuffer.Extents[i-1].NextVcn.QuadPart) : 
                            (RetPointerBuffer.Extents[i].NextVcn.QuadPart-RetPointerBuffer.StartingVcn.QuadPart)));
                    GetLcn.StartingVcn.QuadPart = RetPointerBuffer.Extents[i].NextVcn.QuadPart;
                }		    
            }
	        if (bResult) {
                _tprintf(_T("Returned Success from GetLcn DeviceIoControl for FileName %s\n"), pDriverGetLcn->FileName);
            }		
        } else {
            //STATUS_END_OF_FILEE
            if (ERROR_HANDLE_EOF == GetLastError()) {
               _tprintf(_T("END of File\n"));
	    } else {
                _tprintf(_T("Returned Failed from GetLcn DeviceIoControl for FileName %s %d\n"), 
                          GetLcn.FileName, GetLastError());
                if ((ERROR_BAD_PATHNAME == GetLastError()) && 
                        (0 != _tcsncicmp(L"\\??\\", GetLcn.FileName, _tcslen(L"\\??\\")))) {
                    _tprintf(_T("Filename should be given in form of \\??\\DriverLeter:\\FileName \n"));
                }			
                ret = 1;
            }
            break;
        }
    }
    return ret;
}

int GetWMIComputerSystem()
{
    HRESULT hres;

    hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
    if (FAILED(hres))
    {
        cout << "Failed to initialize COM library. Error code = 0x" 
            << hex << hres << endl;
        return 1;                  // Program has failed.
    }

    hres =  CoInitializeSecurity(
        NULL, 
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities 
        NULL                         // Reserved
        );

                      
    if (FAILED(hres))
    {
        cout << "Failed to initialize security. Error code = 0x" 
            << hex << hres << endl;
        CoUninitialize();
        return 1;
    }
    
    IWbemLocator *pLoc = NULL;

    hres = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &pLoc);
 
    if (FAILED(hres))
    {
        cout << "Failed to create IWbemLocator object."
            << " Err code = 0x"
            << hex << hres << endl;
        CoUninitialize();
        return 1;
    }

    IWbemServices *pSvc = NULL;
 
    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(
         _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
         NULL,                    // User name. NULL = current user
         NULL,                    // User password. NULL = current
         0,                       // Locale. NULL indicates current
         NULL,                    // Security flags.
         0,                       // Authority (for example, Kerberos)
         0,                       // Context object 
         &pSvc                    // pointer to IWbemServices proxy
         );
    
    if (FAILED(hres))
    {
        cout << "Could not connect. Error code = 0x" 
             << hex << hres << endl;
        pLoc->Release();     
        CoUninitialize();
        return 1;
    }

    hres = CoSetProxyBlanket(
       pSvc,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name 
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities 
    );

    if (FAILED(hres))
    {
        cout << "Could not set proxy blanket. Error code = 0x" 
            << hex << hres << endl;
        pSvc->Release();
        pLoc->Release();     
        CoUninitialize();
        return 1;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"), 
        bstr_t("SELECT * FROM Win32_ComputerSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);
    
    if (FAILED(hres))
    {
        cout << "Query for Computer system name failed."
            << " Error code = 0x" 
            << hex << hres << endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;
   
    while (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
            &pclsObj, &uReturn);

        if(0 == uReturn)
        {
            break;
        }

        VARIANT vtProp;

        // Get the value of the Name property
        hr = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
        wcout << ",Manf : " << vtProp.bstrVal;

        hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
        wcout << ",Model : " << vtProp.bstrVal;

        hr = pclsObj->Get(L"TotalPhysicalMemory", 0, &vtProp, 0, 0);
        wcout << ",TotalPhyMem : " << vtProp.bstrVal;

        VariantClear(&vtProp);

        pclsObj->Release();
    }


    // Cleanup
    // ========
    
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}


void
GetDriverVersion(HANDLE hVFCtrlDevice)
{
    DRIVER_VERSION  DriverVersion = {0};
    BOOL    bResult;
    DWORD   dwReturn, dwInputSize = 0;

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_GET_VERSION,
                     NULL,
                     0,
                     &DriverVersion,
                     sizeof(DRIVER_VERSION),
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    
    if (bResult) {
        _tprintf(_T("Driver Version %hd.%hd.%hd.%hd\n"), DriverVersion.ulDrMajorVersion, DriverVersion.ulDrMinorVersion,
                    DriverVersion.ulDrMinorVersion2, DriverVersion.ulDrMinorVersion3);
        _tprintf(_T("Product Version %hd.%hd.%hd.%hd\n"), DriverVersion.ulPrMajorVersion, DriverVersion.ulPrMinorVersion,
                    DriverVersion.ulPrBuildNumber, DriverVersion.ulPrMinorVersion2);
    }
    else
        _tprintf(_T("Returned Failed from get driver version DeviceIoControl call: %d\n"), GetLastError());

    return;
}

void
ClearVolumeStats(HANDLE hVFCtrlDevice, PCLEAR_STATISTICS_DATA pClearStatistcsData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn, dwInputSize = 0;
    PVOID   pInput = NULL;

    if (NULL != pClearStatistcsData->MountPoint) {
        pInput = pClearStatistcsData->VolumeGUID;
        dwInputSize = sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
    }

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_CLEAR_VOLUME_STATS,
                     pInput,
                     dwInputSize,
                     NULL,
                     0,
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (NULL == pClearStatistcsData->MountPoint) {
        if (bResult)
            _tprintf(_T("Returned Success from Clear Stats DeviceIoControl call for all volumes\n"));
        else
            _tprintf(_T("Returned Failed from Clear Stats DeviceIoControl call for all volumes: %d\n"), GetLastError());
    } else {
        if (bResult)
            _tprintf(_T("Returned Success from Clear Stats DeviceIoControl call for volume %s\n"), pClearStatistcsData->MountPoint);
        else
            _tprintf(_T("Returned Failed from Clear Stats DeviceIoControl call for volume %s %d\n"), 
            pClearStatistcsData->MountPoint, GetLastError());
    }

    return;
}

void
SetIoSizeArray(HANDLE hVFCtrlDevice, PSET_IO_SIZE_ARRAY_INPUT_DATA pIoSizeArrayData)
{
    SET_IO_SIZE_ARRAY_INPUT Input = {0};
    BOOL    bResult;
    DWORD   dwReturn;

    Input.ulFlags = pIoSizeArrayData->Flags;

    if (pIoSizeArrayData->MountPoint) {
		inm_memcpy_s(Input.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pIoSizeArrayData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        Input.ulFlags |= SET_IO_SIZE_ARRAY_INPUT_FLAGS_VALID_GUID;
    }

    Input.ulArraySize = pIoSizeArrayData->ulNumValues;
    for (ULONG ul = 0; ul < Input.ulArraySize; ul++) {
        Input.ulIoSizeArray[ul] = pIoSizeArrayData->ulIoSizeArray[ul];
    }
    Input.ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS - 1] = 0x00;

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_SET_IO_SIZE_ARRAY,
                     &Input,
                     sizeof(SET_IO_SIZE_ARRAY_INPUT),
                     NULL,
                     0,
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (pIoSizeArrayData->MountPoint) {
        if (bResult)
            _tprintf(_T("Returned Success from set io sizes call for volume %s\n"), pIoSizeArrayData->MountPoint);
        else
            _tprintf(_T("Returned Failed from set io sizes call for volume %s %d\n"), 
            pIoSizeArrayData->MountPoint, GetLastError());
    } else {
        if (bResult)
            _tprintf(_T("Returned Success from set io sizes call\n"));
        else
            _tprintf(_T("Returned Failed from set io sizes call: %d\n"), GetLastError());
    }

    return;
}
//This function is added to handle for new start filtering IOCTL for disk filter
ULONG
StartFiltering_v2(HANDLE hVFCtrlDevice, PSTART_FILTERING_DATA pStartFilteringData)
{
	// Add one more char for NULL.
	BOOL    bResult;
	DWORD   dwReturn;
	START_FILTERING_INPUT StartFiltering = {0};

	// Fill the startfiltering structure for new IOCTL
	StartFiltering.DeviceSize.QuadPart = 0xFFFF;
	memcpy_s(&StartFiltering.DeviceId,
		sizeof(WCHAR)* GUID_SIZE_IN_CHARS,
		pStartFilteringData->VolumeGUID,
		sizeof(WCHAR)* GUID_SIZE_IN_CHARS);

	_tprintf(_T("%s"), &StartFiltering.DeviceId[0]);
	
	bResult = DeviceIoControl(
		hVFCtrlDevice,
		(DWORD)IOCTL_INMAGE_START_FILTERING_DEVICE,
		&StartFiltering,
		sizeof(START_FILTERING_INPUT),
		NULL,
		0,
		&dwReturn,
		NULL        // Overlapped
		);
	if (bResult) {
		_tprintf(_T("Returned Success from start filtering DeviceIoControl call for Disk %s\n"), pStartFilteringData->MountPoint);
	}
	else {
		_tprintf(_T("Returned Failed from start filtering DeviceIoControl call for Disk %s %d\n"),
			pStartFilteringData->MountPoint, GetLastError());
		return ERROR_GEN_FAILURE;
	}
	return ERROR_SUCCESS;
}
// DBF end
ULONG
StartFiltering(HANDLE hVFCtrlDevice, PSTART_FILTERING_DATA pStartFilteringData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_START_FILTERING_DEVICE,
                     pStartFilteringData->VolumeGUID,
                     sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                     NULL,
                     0,
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (bResult) {
        _tprintf(_T("Returned Success from start filtering DeviceIoControl call for volume %s\n"), pStartFilteringData->MountPoint);
    } else {
        _tprintf(_T("Returned Failed from start filtering DeviceIoControl call for volume %s %d\n"), 
        pStartFilteringData->MountPoint, GetLastError());
        return ERROR_GEN_FAILURE;
    }
    return ERROR_SUCCESS;
}

void
ResyncStartNotify(HANDLE hVFCtrlDevice, PRESYNC_START_NOTIFY_DATA pResyncStart)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;
    RESYNC_START_INPUT  Input = {0};
    RESYNC_START_OUTPUT Output = {0};

	inm_memcpy_s(Input.VolumeGUID, sizeof(WCHAR)* GUID_SIZE_IN_CHARS, pResyncStart->VolumeGUID, sizeof(WCHAR)* GUID_SIZE_IN_CHARS);

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_RESYNC_START_NOTIFICATION,
                     &Input,
                     sizeof(Input),
                     &Output,
                     sizeof(Output),
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (bResult) {
        TIME_ZONE_INFORMATION   TimeZone;
        SYSTEMTIME              SystemTime, LocalTime;

        if (sizeof(RESYNC_START_OUTPUT) != dwReturn) {
            _tprintf(_T("ResyncStartNotify: dwReturn(%#x) did not match RESYNC_START_OUTPUT size(%#x)\n"), dwReturn, sizeof(RESYNC_START_OUTPUT));
            return;
        }
        _tprintf(_T("Returned Success from resync start notification for volume %s:\n"), pResyncStart->MountPoint);

        // Print time stamp returned.
        GetTimeZoneInformation(&TimeZone);
        FileTimeToSystemTime((FILETIME *)&Output.TimeInHundNanoSecondsFromJan1601, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);

        _tprintf(_T("ResyncStart returned Timestamp : %02d/%02d/%04d %02d:%02d:%02d:%04d, SeqID : %#x\n"),
                            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
                            Output.ulSequenceNumber );
    } else {
        _tprintf(_T("Returned Failed from resync start DeviceIoControl call for volume %s: %d\n"), 
                 pResyncStart->MountPoint, GetLastError());
    }

    return;
}

void
ResyncEndNotify(HANDLE hVFCtrlDevice, PRESYNC_END_NOTIFY_DATA pResyncEnd)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;
    RESYNC_END_INPUT  Input = {0};
    RESYNC_END_OUTPUT Output = {0};

	inm_memcpy_s(Input.VolumeGUID, sizeof(WCHAR)* GUID_SIZE_IN_CHARS, pResyncEnd->VolumeGUID, sizeof(WCHAR)* GUID_SIZE_IN_CHARS);

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_RESYNC_END_NOTIFICATION,
                     &Input,
                     sizeof(Input),
                     &Output,
                     sizeof(Output),
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (bResult) {
        TIME_ZONE_INFORMATION   TimeZone;
        SYSTEMTIME              SystemTime, LocalTime;

        if (sizeof(RESYNC_END_OUTPUT) != dwReturn) {
            _tprintf(_T("ResyncEndNotify: dwReturn(%#x) did not match RESYNC_END_OUTPUT size(%#x)\n"), dwReturn, sizeof(RESYNC_END_OUTPUT));
            return;
        }
        _tprintf(_T("Returned Success from resync end notification for volume %s:\n"), pResyncEnd->MountPoint);

        // Print time stamp returned.
        GetTimeZoneInformation(&TimeZone);
        FileTimeToSystemTime((FILETIME *)&Output.TimeInHundNanoSecondsFromJan1601, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);

        _tprintf(_T("ResyncEnd returned Timestamp : %02d/%02d/%04d %02d:%02d:%02d:%04d, SeqID : %#x\n"),
                            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
                            Output.ulSequenceNumber );
    } else {
        _tprintf(_T("Returned Failed from resync end DeviceIoControl call for volume %s: %d\n"), 
                 pResyncEnd->MountPoint, GetLastError());
    }

    return;
}

ULONG
StopFiltering(HANDLE hVFCtrlDevice, PSTOP_FILTERING_DATA pStopFilteringData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;
    STOP_FILTERING_INPUT    Input;

	inm_memcpy_s(Input.VolumeGUID, sizeof(WCHAR)* GUID_SIZE_IN_CHARS, pStopFilteringData->VolumeGUID, sizeof(WCHAR)* GUID_SIZE_IN_CHARS);
    Input.ulFlags = 0;
    Input.ulReserved = 0;
	// By default, let' stop filtering to delete the stale bitmap file

        Input.ulFlags |= STOP_FILTERING_FLAGS_DELETE_BITMAP;
    if (pStopFilteringData->bStopAll) {
        Input.ulFlags |= STOP_ALL_FILTERING_FLAGS;
    }
    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_STOP_FILTERING_DEVICE,
                     &Input,
                     sizeof(STOP_FILTERING_INPUT),
                     NULL,
                     0,
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (bResult) {
        if (pStopFilteringData->bStopAll) {
            _tprintf(_T("Returned Success from stop All filtering\n"));
        } else {
            _tprintf(_T("Returned Success from stop filtering DeviceIoControl call for volume %s\n"), pStopFilteringData->MountPoint);		
	    }		
    } else {
        if (pStopFilteringData->bStopAll) {
             _tprintf(_T("Returned Failed from stop All filtering %d\n"), GetLastError());
        } else {
             _tprintf(_T("Returned Failed from stop filtering DeviceIoControl call for volume %s: %d\n"), 			     
             pStopFilteringData->MountPoint, GetLastError());		
        }		
        return ERROR_GEN_FAILURE;
    }
    return ERROR_SUCCESS;
}

void GetFsVolumeSize(PTCHAR vol, unsigned long long* fsSize, unsigned long long drvSize, bool* canReadDriverSize)
{        
    char drive[60];

    tcstombs(drive, 60, vol);
    drive[_tcslen(vol)] = '\0';
    
    unsigned int sectorSize;
    
   if (!GetFileSystemVolumeSize(drive, fsSize, &sectorSize)) {
        _tprintf(_T("*** Error getting filesystem volume size: %d\n"), drive, GetLastError());
        (*fsSize) = 0;
    }
    (*canReadDriverSize) = CanReadDriverSize(drive, drvSize, sectorSize);
}

void
GetDataMode(HANDLE hVFCtrlDevice, PDATA_MODE_DATA pDataModeData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn, ulNumVolumes, dwOutputSize;
    PVOLUMES_DM_DATA  pVolumesDMdata;

    pVolumesDMdata = (PVOLUMES_DM_DATA)malloc(sizeof(VOLUMES_DM_DATA));
    if (NULL == pVolumesDMdata) {
        printf("GetDataMode: Failed in memory allocation\n");
        return;
    }

    if(NULL == pDataModeData->MountPoint) {
        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
                        NULL,
                        0,
                        pVolumesDMdata,
                        sizeof(VOLUMES_DM_DATA),
                        &dwReturn,  
                        NULL        // Overlapped
                        );
    } else {
        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
                        pDataModeData->VolumeGUID,
                        sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                        pVolumesDMdata,
                        sizeof(VOLUMES_DM_DATA),
                        &dwReturn,  
                        NULL        // Overlapped
                        );
    }

    if (bResult) {
        if ((NULL == pDataModeData->MountPoint) && 
            (pVolumesDMdata->ulTotalVolumes > pVolumesDMdata->ulVolumesReturned)) 
        {
            // We asked for all volumes and buffer wasn't enough.
            ulNumVolumes = pVolumesDMdata->ulTotalVolumes;
            if (ulNumVolumes >= 1)
                ulNumVolumes--;

            free(pVolumesDMdata);
            dwOutputSize = sizeof(VOLUMES_DM_DATA) + (ulNumVolumes * sizeof(VOLUME_DM_DATA));
            pVolumesDMdata = (PVOLUMES_DM_DATA)malloc(dwOutputSize);
            bResult = DeviceIoControl(
                            hVFCtrlDevice,
                            (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
                            NULL,
                            0,
                            pVolumesDMdata,
                            dwOutputSize,
                            &dwReturn,  
                            NULL        // Overlapped
                            );
        }
    }

    if (bResult) {
        _tprintf(_T("Total Number of Volumes = %lu\n"), pVolumesDMdata->ulTotalVolumes);
        _tprintf(_T("Number of Volumes Data Returned = %lu\n"), pVolumesDMdata->ulVolumesReturned);

        ulNumVolumes = 0;
        while (ulNumVolumes < pVolumesDMdata->ulVolumesReturned) {
            PVOLUME_DM_DATA pVolumeDM = &pVolumesDMdata->VolumeArray[ulNumVolumes];

            _tprintf(_T("\nIndex = %lu\n"), ulNumVolumes);
            if (true == IsDiskFilterRunning) {
                wprintf(L"DiskID = %.*s.\n", GUID_SIZE_IN_CHARS, pVolumeDM->VolumeGUID);
            } else {
                wprintf(L"DriveLetter = %c: VolumeGUID = %.*s.\n", pVolumeDM->DriveLetter,
                    GUID_SIZE_IN_CHARS, pVolumeDM->VolumeGUID);
            }

            printf("Write order state  = %s\n", WriteOrderStateString(pVolumeDM->eWOState));
            printf("Prev write order state  = %s\n", WriteOrderStateString(pVolumeDM->ePrevWOState));
            printf("Capture mode = %s\n\n", CaptureModeString(pVolumeDM->eCaptureMode));
            switch(pVolumeDM->eWOState) {
            case ecWriteOrderStateBitmap:
                pVolumeDM->ulNumSecondsInBitmapWOState += pVolumeDM->ulNumSecondsInCurrentWOState;
                break;
            case ecWriteOrderStateMetadata:
                pVolumeDM->ulNumSecondsInMetaDataWOState += pVolumeDM->ulNumSecondsInCurrentWOState;
                break;
            case ecWriteOrderStateData:
                pVolumeDM->ulNumSecondsInDataWOState += pVolumeDM->ulNumSecondsInCurrentWOState;
                break;
            default:
                break;
            }

            switch(pVolumeDM->eCaptureMode) {
            case ecCaptureModeData:
                pVolumeDM->ulNumSecondsInDataCaptureMode += pVolumeDM->ulNumSecondsInCurrentCaptureMode;
                break;
            case ecCaptureModeMetaData:
                pVolumeDM->ulNumSecondsInMetadataCaptureMode += pVolumeDM->ulNumSecondsInCurrentCaptureMode;
                break;
            default:
                break;
            }

            printf("Number of times write order state is changed to data = %#lu\n", pVolumeDM->lNumChangeToDataWOState);
            printf("Number of times write order state is changed to meta-data = %#lu\n", pVolumeDM->lNumChangeToMetaDataWOState);
            printf("Number of times write order state is changed to meta-data on user request = %#lu\n", pVolumeDM->lNumChangeToMetaDataWOStateOnUserRequest);
            printf("Number of times write order state is changed to bitmap = %#lu\n", pVolumeDM->lNumChangeToBitmapWOState);
            printf("Number of times write order state is changed to bitmap on user request = %#lu\n", pVolumeDM->lNumChangeToBitmapWOStateOnUserRequest);

            printf("Number of seconds in current write order state = %lu\n", pVolumeDM->ulNumSecondsInCurrentWOState);
            printf("Number of seconds in data write order state = %#lu\n", pVolumeDM->ulNumSecondsInDataWOState);
            printf("Number of seconds in meta-data write order state = %#lu\n", pVolumeDM->ulNumSecondsInMetaDataWOState);
            printf("Number of seconds in bitmap write order state = %#lu\n\n", pVolumeDM->ulNumSecondsInBitmapWOState);

            printf("Number of times capture mode is changed to data = %lu\n", pVolumeDM->lNumChangeToDataCaptureMode);
            printf("Number of times capture mode is changed to meta-data = %lu\n", pVolumeDM->lNumChangeToMetaDataCaptureMode);
            printf("Number of times capture mode is changed to meta-data on user request = %#lu\n", pVolumeDM->lNumChangeToMetaDataCaptureModeOnUserRequest);

            printf("Number of seconds in current capture mode = %lu\n", pVolumeDM->ulNumSecondsInCurrentCaptureMode);
            printf("Number of seconds in data capture mode = %#lu\n", pVolumeDM->ulNumSecondsInDataCaptureMode);
            printf("Number of seconds in meta-data capture mode = %#lu\n\n", pVolumeDM->ulNumSecondsInMetadataCaptureMode);

            _tprintf(_T("Number of DirtyBlocks in Queue = %#ld\n"), pVolumeDM->lDirtyBlocksInQueue);
            _tprintf(_T("Number of Data files pending = %#lu\n"), pVolumeDM->ulDataFilesPending);
            _tprintf(_T("Number of Data changes pending = %#lu\n"), pVolumeDM->ulDataChangesPending);
            _tprintf(_T("Number of Meta-data changes pending = %#lu\n"), pVolumeDM->ulMetaDataChangesPending);
            _tprintf(_T("Number of Bitmap changes pending = %#lu\n"), pVolumeDM->ulBitmapChangesPending);
            _tprintf(_T("Number of Total changes pending = %#lu\n"), pVolumeDM->ulTotalChangesPending);

            PrintStr("Bytes of Data changes pending = %s\n", pVolumeDM->ullcbDataChangesPending);
            PrintStr("Bytes of Meta-data changes pending = %s\n", pVolumeDM->ullcbMetaDataChangesPending);
            PrintStr("Bytes of Bitmap changes pending = %s\n", pVolumeDM->ullcbBitmapChangesPending);
            PrintStr("Bytes of Total changes pending = %s\n\n", pVolumeDM->ullcbTotalChangesPending);

            ULONG ulLastSize = 0;
            printf("\nNotify latency distribution. (Latency in S2 getting dirty block after driver setting notification)\n");
            for (ULONG ul = 0; ul < USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS; ++ul) {
                if (0 == pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyDistSizeArray[ul]) {
                    PrintMicroSeconds(">  %-12s : ", ulLastSize);
                    printf("%#I64d\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ullNotifyLatencyDistribution[ul]);
                    break;
                } else {
                    ulLastSize = pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyDistSizeArray[ul];
                    PrintMicroSeconds("<= %-12s : ", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyDistSizeArray[ul]);
                    printf("%#I64d\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ullNotifyLatencyDistribution[ul]);
                }
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyLogSize) {
                printf("\nNotify latency log. (Latency in S2 getting dirty block after driver setting notification)\n");
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMinLoggedValue) {
                    PrintMicroSeconds("Minimum notify latency value logged = %s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMinLoggedValue);
                }
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMaxLoggedValue) {
                    PrintMicroSeconds("Maximum notify latency value logged = %s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMaxLoggedValue);
                }

                for (ULONG ul = 0; ul < pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyLogSize; ++ul) {
                    printf("%-5d: ", ul + 1);
                    PrintMicroSeconds("%-12s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyLogArray[ul]);
                }
                printf("\n");
            }

            ulLastSize = 0;
            printf("\nCommit latency distribution. (Latency in S2 commiting dirty block after retreived from driver)\n");
            for (ULONG ul = 0; ul < USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS; ++ul) {
                if (0 == pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyDistSizeArray[ul]) {
                    PrintMicroSeconds(">  %-12s : ", ulLastSize);
                    printf("%#I64d\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ullCommitLatencyDistribution[ul]);
                    break;
                } else {
                    ulLastSize = pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyDistSizeArray[ul];
                    PrintMicroSeconds("<= %-12s : ", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyDistSizeArray[ul]);
                    printf("%#I64d\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ullCommitLatencyDistribution[ul]);
                }
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyLogSize) {
                printf("\nCommit latency log. (Latency in S2 commiting dirty block after retreived from driver)\n");
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMinLoggedValue) {
                    PrintMicroSeconds("Minimum commit latency value logged = %s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMinLoggedValue);
                }
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMaxLoggedValue) {
                    PrintMicroSeconds("Maximum commit latency value logged = %s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMaxLoggedValue);
                }

                for (ULONG ul = 0; ul < pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyLogSize; ++ul) {
                    printf("%-5d: ", ul + 1);
                    PrintMicroSeconds("%-12s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyLogArray[ul]);
                }
                printf("\n");
            }

            ulLastSize = 0;
            printf("\nS2 data retrieval latency distribution. (Latency in S2 getting dirty block after filling last IO.)\n");
            for (ULONG ul = 0; ul < USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS; ++ul) {
                if (0 == pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyDistSizeArray[ul]) {
                    PrintMicroSeconds(">  %-12s : ", ulLastSize);
                    printf("%#I64d\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ullS2DataRetievalLatencyDistribution[ul]);
                    break;
                } else {
                    ulLastSize = pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyDistSizeArray[ul];
                    PrintMicroSeconds("<= %-12s : ", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyDistSizeArray[ul]);
                    printf("%#I64d\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ullS2DataRetievalLatencyDistribution[ul]);
                }
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyLogSize) {
                printf("\nS2 data retrieval latency log. (Latency in S2 getting dirty block after filling last IO.)\n");
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMinLoggedValue) {
                    PrintMicroSeconds("Minimum S2 data retrieval latency value logged = %s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMinLoggedValue);
                }
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMaxLoggedValue) {
                    PrintMicroSeconds("Maximum S2 data retrieval latency value logged = %s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMaxLoggedValue);
                }

                for (ULONG ul = 0; ul < pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyLogSize; ++ul) {
                    printf("%-5d: ", ul + 1);
                    PrintMicroSeconds("%-12s\n", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyLogArray[ul]);
                }
                printf("\n");
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulMuInstances) {
                _tprintf(_T("%-5.5s%-10.10s%-10.10s%-10.10s%-10s%-10.10s%-25.25s\n"), 
                        _T("Idx"), _T("DaToDisk"), _T("MemAlloc"), _T("MemRes"), _T("MemVCFree"), _T("MemFree"), _T("ChangeTime"));
                for (ULONG i = 0; i < pVolumesDMdata->VolumeArray[ulNumVolumes].ulMuInstances; i++) {
                    std::string strDataToDiskSize, strMemAlocated, strMemReserved, strMemFree, strMemTotalFree, strTS;

                    ConvertULLtoString(pVolumesDMdata->VolumeArray[ulNumVolumes].ullDuAtOutOfDataMode[i], strDataToDiskSize);
                    ConvertULtoString(pVolumesDMdata->VolumeArray[ulNumVolumes].ulMAllocatedAtOutOfDataMode[i], strMemAlocated);
                    ConvertULtoString(pVolumesDMdata->VolumeArray[ulNumVolumes].ulMReservedAtOutOfDataMode[i], strMemReserved);
                    ConvertULtoString(pVolumesDMdata->VolumeArray[ulNumVolumes].ulMFreeInVCAtOutOfDataMode[i], strMemFree);
                    ConvertULtoString(pVolumesDMdata->VolumeArray[ulNumVolumes].ulMFreeAtOutOfDataMode[i], strMemTotalFree);
                    ConvertFileTimeToString((FILETIME *)&pVolumesDMdata->VolumeArray[ulNumVolumes].liCaptureModeChangeTS[i].QuadPart, strTS);
                    printf("%-5d%-#10s%-#10s%-#10s%-#10s%-#10s%-#25s\n", i, 
                            strDataToDiskSize.c_str(), strMemAlocated.c_str(), strMemReserved.c_str(), strMemFree.c_str(), 
                            strMemTotalFree.c_str(), strTS.c_str());
                }
                _tprintf(_T("\n"));
            }

            _tprintf(_T("\n"));
            ulNumVolumes++;
        }
    }
    else
        _tprintf(_T("Returned Failed from GetDriverStats DeviceIoControl: %d\n"), GetLastError());

    free(pVolumesDMdata);

    return;
}

void
GetDataModeCompact(HANDLE hVFCtrlDevice, PDATA_MODE_DATA pDataModeData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn, ulNumVolumes, dwOutputSize;
    PVOLUMES_DM_DATA  pVolumesDMdata;

    pVolumesDMdata = (PVOLUMES_DM_DATA)malloc(sizeof(VOLUMES_DM_DATA));
    if (NULL == pVolumesDMdata) {
        printf("GetDataMode: Failed in memory allocation\n");
        return;
    }

    if (NULL == pDataModeData->MountPoint) {
        bResult = DeviceIoControl(
            hVFCtrlDevice,
            (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
            NULL,
            0,
            pVolumesDMdata,
            sizeof(VOLUMES_DM_DATA),
            &dwReturn,
            NULL        // Overlapped
            );
    }
    else {
        bResult = DeviceIoControl(
            hVFCtrlDevice,
            (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
            pDataModeData->VolumeGUID,
            sizeof(WCHAR)* GUID_SIZE_IN_CHARS,
            pVolumesDMdata,
            sizeof(VOLUMES_DM_DATA),
            &dwReturn,
            NULL        // Overlapped
            );
    }

    if (bResult) {
        if ((NULL == pDataModeData->MountPoint) &&
            (pVolumesDMdata->ulTotalVolumes > pVolumesDMdata->ulVolumesReturned))
        {
            // We asked for all volumes and buffer wasn't enough.
            ulNumVolumes = pVolumesDMdata->ulTotalVolumes;
            if (ulNumVolumes >= 1)
                ulNumVolumes--;

            free(pVolumesDMdata);
            dwOutputSize = sizeof(VOLUMES_DM_DATA)+(ulNumVolumes * sizeof(VOLUME_DM_DATA));
            pVolumesDMdata = (PVOLUMES_DM_DATA)malloc(dwOutputSize);
            bResult = DeviceIoControl(
                hVFCtrlDevice,
                (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
                NULL,
                0,
                pVolumesDMdata,
                dwOutputSize,
                &dwReturn,
                NULL        // Overlapped
                );
        }
    }

    if (bResult) {
        ulNumVolumes = 0;
        while (ulNumVolumes < pVolumesDMdata->ulVolumesReturned) {
            PVOLUME_DM_DATA pVolumeDM = &pVolumesDMdata->VolumeArray[ulNumVolumes];

            _tprintf(_T("\nIndex=%lu"), ulNumVolumes);
            if (true == IsDiskFilterRunning) {
                wprintf(L"DiskID=%.*s.", GUID_SIZE_IN_CHARS, pVolumeDM->VolumeGUID);
            }
            else {
                wprintf(L"DriveLetter = %c: VolumeGUID = %.*s.\n", pVolumeDM->DriveLetter,
                    GUID_SIZE_IN_CHARS, pVolumeDM->VolumeGUID);
            }

            ULONG ulLastSize = 0;
            printf(",NLD");
            for (ULONG ul = 0; ul < USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS; ++ul) {
                if (0 == pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyDistSizeArray[ul]) {
                    PrintMicroSeconds(",>%s :", ulLastSize);
                    printf("%#I64d", pVolumesDMdata->VolumeArray[ulNumVolumes].ullNotifyLatencyDistribution[ul]);
                    break;
                }
                else {
                    ulLastSize = pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyDistSizeArray[ul];
                    PrintMicroSeconds(",<=%s:", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyDistSizeArray[ul]);
                    printf("%#I64d", pVolumesDMdata->VolumeArray[ulNumVolumes].ullNotifyLatencyDistribution[ul]);
                }
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyLogSize) {
                printf(",NLLog");
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMinLoggedValue) {
                    PrintMicroSeconds(",MinNL=%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMinLoggedValue);
                }
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMaxLoggedValue) {
                    PrintMicroSeconds(",MaxNL=%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyMaxLoggedValue);
                }

                for (ULONG ul = 0; ul < pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyLogSize; ++ul) {
                    printf(",%d:", ul + 1);
                    PrintMicroSeconds("%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulNotifyLatencyLogArray[ul]);
                }
            }

            ulLastSize = 0;
            printf(",CLD");
            for (ULONG ul = 0; ul < USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS; ++ul) {
                if (0 == pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyDistSizeArray[ul]) {
                    PrintMicroSeconds(",>%s:", ulLastSize);
                    printf("%#I64d", pVolumesDMdata->VolumeArray[ulNumVolumes].ullCommitLatencyDistribution[ul]);
                    break;
                }
                else {
                    ulLastSize = pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyDistSizeArray[ul];
                    PrintMicroSeconds(",<=%s:", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyDistSizeArray[ul]);
                    printf("%#I64d", pVolumesDMdata->VolumeArray[ulNumVolumes].ullCommitLatencyDistribution[ul]);
                }
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyLogSize) {
                printf(",CLL");
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMinLoggedValue) {
                    PrintMicroSeconds(",MinCL=%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMinLoggedValue);
                }
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMaxLoggedValue) {
                    PrintMicroSeconds(",MaxCL=%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyMaxLoggedValue);
                }

                for (ULONG ul = 0; ul < pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyLogSize; ++ul) {
                    printf(",%d:", ul + 1);
                    PrintMicroSeconds("%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulCommitLatencyLogArray[ul]);
                }
            }

            ulLastSize = 0;
            printf(",S2 DRLD");
            for (ULONG ul = 0; ul < USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS; ++ul) {
                if (0 == pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyDistSizeArray[ul]) {
                    PrintMicroSeconds(",>%s:", ulLastSize);
                    printf("%#I64d", pVolumesDMdata->VolumeArray[ulNumVolumes].ullS2DataRetievalLatencyDistribution[ul]);
                    break;
                }
                else {
                    ulLastSize = pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyDistSizeArray[ul];
                    PrintMicroSeconds(",<=%s:", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyDistSizeArray[ul]);
                    printf("%#I64d", pVolumesDMdata->VolumeArray[ulNumVolumes].ullS2DataRetievalLatencyDistribution[ul]);
                }
            }

            if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyLogSize) {
                printf(",S2 DRLL");
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMinLoggedValue) {
                    PrintMicroSeconds(",MinS2DRL=%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMinLoggedValue);
                }
                if (pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMaxLoggedValue) {
                    PrintMicroSeconds(",MaxS2DRL= %s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyMaxLoggedValue);
                }

                for (ULONG ul = 0; ul < pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyLogSize; ++ul) {
                    printf(",%d:", ul + 1);
                    PrintMicroSeconds("%s", pVolumesDMdata->VolumeArray[ulNumVolumes].ulS2DataRetievalLatencyLogArray[ul]);
                }
            }
            ulNumVolumes++;
        }
    }
    else
        _tprintf(_T("Returned Failed from GetDriverStats DeviceIoControl: %d\n"), GetLastError());

    free(pVolumesDMdata);

    return;
}
void
GetWriteOrderState(HANDLE hVFCtrlDevice, PGET_FILTERING_STATE pGetFilteringState)
{
    BOOL    bResult;
    DWORD   dwReturn;
    etWriteOrderState  eWriteOrderState;

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_GET_VOLUME_WRITE_ORDER_STATE,
                    pGetFilteringState->VolumeGUID,
                    sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                    &eWriteOrderState,
                    sizeof(eWriteOrderState),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

    if(bResult && (dwReturn >=  sizeof(eWriteOrderState))) {
        printf("Mode of volume is %s\n", WriteOrderStateString(eWriteOrderState));
    } else {
        if (!bResult) {
            printf("GetWriteOrderState failed with error %d\n", GetLastError());
        } else {
            printf("GetWriteOrderState failed: dwReturn(%d) < sizeof(etWriteOrderState)(%d)\n", dwReturn, sizeof(eWriteOrderState));
        }
    }

    return;
}

void
GetVolumeFlags(HANDLE hVFCtrlDevice, PGET_VOLUME_FLAGS pGetVolumeFlags)
{
    BOOL    bResult;
    DWORD   dwReturn;
    ULONG   ulFlags;

    bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_VOLUME_FLAGS,
                        pGetVolumeFlags->VolumeGUID,
                        sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                        &ulFlags,
                        sizeof(ulFlags),
                        &dwReturn,  
                        NULL        // Overlapped
                        );

    
    if(bResult && (dwReturn >=  sizeof(ulFlags))) {    
        _tprintf(_T("Volume Flags are\n"));
        if (ulFlags & VSF_BITMAP_NOT_OPENED)
            _tprintf(_T("\tVSF_BITMAP_NOT_OPENED\n"));
        if (ulFlags & VSF_BITMAP_READ_NOT_STARTED)
            _tprintf(_T("\tVSF_BITMAP_READ_NOT_STARTED\n"));
        if (ulFlags & VSF_BITMAP_READ_IN_PROGRESS)
            _tprintf(_T("\tVSF_BITMAP_READ_IN_PROGRESS\n"));
        if (ulFlags & VSF_BITMAP_READ_PAUSED)
            _tprintf(_T("\tVSF_BITMAP_READ_PAUSED\n"));
        if (ulFlags & VSF_BITMAP_READ_COMPLETE)
            _tprintf(_T("\tVSF_BITMAP_READ_COMPLETE\n"));
        if (ulFlags & VSF_BITMAP_READ_ERROR)
            _tprintf(_T("\tVSF_BITMAP_READ_ERROR\n"));
        if (ulFlags & VSF_BITMAP_WRITE_ERROR)
            _tprintf(_T("\tVSF_BITMAP_WRITE_ERROR\n"));
        if (ulFlags & VSF_READ_ONLY)
            _tprintf(_T("\tVSF_READ_ONLY\n"));
        if (ulFlags & VSF_FILTERING_STOPPED)
            _tprintf(_T("\tVSF_FILTERING_STOPPED\n"));
        if (ulFlags & VSF_BITMAP_WRITE_DISABLED)
            _tprintf(_T("\tVSF_BITMAP_WRITE_DISABLED\n"));
        if (ulFlags & VSF_BITMAP_READ_DISABLED)
            _tprintf(_T("\tVSF_BITMAP_READ_DISABLED\n"));
        if (ulFlags & VSF_DATA_CAPTURE_DISABLED)
            _tprintf(_T("\tVSF_DATA_CAPTURE_DISABLED\n"));
        if (ulFlags & VSF_DATA_FILES_DISABLED)
            _tprintf(_T("\tVSF_DATA_FILES_DISABLED\n"));
        if (ulFlags & VSF_CLUSTER_VOLUME)
            _tprintf(_T("\tVSF_CLUSTER_VOLUME\n"));
        if (ulFlags & VSF_SURPRISE_REMOVED)
            _tprintf(_T("\tVSF_SURPRISE_REMOVED\n"));
        if (ulFlags & VSF_DISK_ID_CONFLICT)
            _tprintf(_T("\tVSF_DISK_ID_CONFLICT\n"));		
        if (ulFlags & VSF_VCFIELDS_PERSISTED)
            _tprintf(_T("\tVSF_VCFIELDS_PERSISTED\n"));
        if (ulFlags & VSF_PAGEFILE_WRITES_IGNORED)
            _tprintf(_T("\tVSF_PAGEFILE_WRITES_IGNORED\n"));
        if (ulFlags & VSF_PHY_DEVICE_INRUSH)
            _tprintf(_T("\tVSF_PHY_DEVICE_INRUSH\n"));
        if (ulFlags & VSF_VOLUME_ON_BASIC_DISK)
            _tprintf(_T("\tVSF_VOLUME_ON_BASIC_DISK\n"));
        if (ulFlags & VSF_VOLUME_ON_CLUS_DISK)
            _tprintf(_T("\tVSF_VOLUME_ON_CLUS_DISK\n"));
        if (ulFlags & VSF_CLUSTER_VOLUME_ONLINE)
            _tprintf(_T("\tVSF_CLUSTER_VOLUME_ONLINE\n"));

        if (ulFlags & VSF_DEVNUM_OBTAINED)
            _tprintf(_T("\tVSF_DEVNUM_OBTAINED\n"));
        if (ulFlags & VSF_DEVSIZE_OBTAINED)
            _tprintf(_T("\tVSF_DEVSIZE_OBTAINED\n"));
        if (ulFlags & VSF_BITMAP_DEV)
            _tprintf(_T("\tVSF_BITMAP_DEV\n"));
        if (ulFlags & VSF_DONT_PAGE_FAULT)
            _tprintf(_T("\tVSF_DONT_PAGE_FAULT\n"));
        if (ulFlags & VSF_LAST_IO)
            _tprintf(_T("\tVSF_LAST_IO\n"));
        if (ulFlags & VSF_BITMAP_DEV_OFF)
            _tprintf(_T("\tVSF_BITMAP_DEV_OFF\n"));
        if (ulFlags & VSF_PAGE_FILE_MISSED)
            _tprintf(_T("\tVSF_PAGE_FILE_MISSED\n"));
        if (ulFlags & VSF_EXPLICIT_NONWO_NODRAIN)
            _tprintf(_T("\tVSF_EXPLICIT_NONWO_NODRAIN\n"));

    } else {
        if (!bResult) {
            _tprintf(_T("GetVolumeFlags failed with error %d\n"), GetLastError());
        } else {
            _tprintf(_T("GetVolumeFlags failed: dwReturn(%d) < sizeof(ULONG)(%d)\n"), dwReturn, sizeof(ulFlags));
        }
    }

}
void
GetVolumeSize_v3(HANDLE hVFCtrlDevice, PGET_VOLUME_SIZE_V2 pGetVolumeSize)
{
	BOOL    bResult;
	DWORD   dwReturn;
	ULARGE_INTEGER   ulVolumeSize;

	//pGetVolumeSize->VolumeGUID = NULL;
	bResult = DeviceIoControl(
		hVFCtrlDevice,
		(DWORD)IOCTL_INMAGE_GET_DEVICE_TRACKING_SIZE,
		pGetVolumeSize->VolumeGUID,
		pGetVolumeSize->inputlen,
		&ulVolumeSize,
		sizeof(ULARGE_INTEGER),
		&dwReturn,
		NULL        // Overlapped
		);

	if (bResult && (dwReturn >= sizeof(ULARGE_INTEGER))) {
		PrintStr("Disk Size read from Driver = %s\n", ulVolumeSize.QuadPart);
	}
	else {
		if (!bResult) {
			_tprintf(_T("Disk Size IOCTL failed with error %d\n"), GetLastError());
		}
		else {
			_tprintf(_T("Disk Size IOCTL is failed: dwReturn(%d) < sizeof(ULARGE_INTEGER)(%d)\n"), dwReturn, sizeof(ULARGE_INTEGER));
		}
	}
}
//DBF start
void
GetVolumeSize_v2(HANDLE hVFCtrlDevice, PGET_VOLUME_SIZE pGetVolumeSize)
{
	BOOL    bResult;
	DWORD   dwReturn;
	ULARGE_INTEGER   ulVolumeSize;

	bResult = DeviceIoControl(
		hVFCtrlDevice,
		(DWORD)IOCTL_INMAGE_GET_DEVICE_TRACKING_SIZE,
		pGetVolumeSize->VolumeGUID,
		sizeof(WCHAR)* GUID_SIZE_IN_CHARS,
		&ulVolumeSize,
		sizeof(ULARGE_INTEGER),
		&dwReturn,
		NULL        // Overlapped
		);

	if (bResult && (dwReturn >= sizeof(ULARGE_INTEGER))) {
		PrintStr("Disk Size read from Driver = %s\n", ulVolumeSize.QuadPart);
	}
	else {
		if (!bResult) {
			_tprintf(_T("Disk Size IOCTL failed with error %d\n"), GetLastError());
		}
		else {
			_tprintf(_T("Disk Size IOCTL is failed: dwReturn(%d) < sizeof(ULARGE_INTEGER)(%d)\n"), dwReturn, sizeof(ULARGE_INTEGER));
		}
	}
}
//DBF end
void
GetVolumeSize(HANDLE hVFCtrlDevice, PGET_VOLUME_SIZE pGetVolumeSize)
{
    BOOL    bResult;
    DWORD   dwReturn;
    ULARGE_INTEGER   ulVolumeSize;

    bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_VOLUME_SIZE,
                        pGetVolumeSize->VolumeGUID,
                        sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                        &ulVolumeSize,
                        sizeof(ULARGE_INTEGER),
                        &dwReturn,  
                        NULL        // Overlapped
                        );

    if(bResult && (dwReturn >=  sizeof(ULARGE_INTEGER))) {
        PrintStr("Volume Size read from Driver = %s\n",ulVolumeSize.QuadPart);
    } else {
        if (!bResult) {
            _tprintf(_T("Volume Size IOCTL failed with error %d\n"), GetLastError());
        } else {
            _tprintf(_T("Volume Size IOCTL is failed: dwReturn(%d) < sizeof(ULARGE_INTEGER)(%d)\n"), dwReturn, sizeof(ULARGE_INTEGER));
        }
    }
}

void
PrintAdditionalGUIDs(
    ULONG   ulGUIDsReturned,
    PUCHAR  pBuffer)
{
    PWCHAR  GUID = (PWCHAR)pBuffer;

    for (ULONG i = 0; i < ulGUIDsReturned; i++) {
        wprintf(L"Index = %lu: VolumeGUID = %.*s.\n", i, 
                GUID_SIZE_IN_CHARS, GUID);
        GUID += GUID_SIZE_IN_CHARS;
    }

    return;
}

void
PrintVolumeStats(PVOLUME_STATS pVolumeStats, int IsOlderDriver)
{
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;
    unsigned long long      fsSize = 0;
    bool                    canReadDriverSize = false;

    GetTimeZoneInformation(&TimeZone);
    if (true == IsDiskFilterRunning) {
        wprintf(L"DiskID = %.*s.\n", GUID_SIZE_IN_CHARS, pVolumeStats->VolumeGUID);
    } else {
        wprintf(L"DriveLetter = %c: VolumeGUID = %.*s.\n", pVolumeStats->DriveLetter,
            GUID_SIZE_IN_CHARS, pVolumeStats->VolumeGUID);
    }
    if (false == IsDiskFilterRunning){
        printf("Additonal GUIDs = %ld\n", pVolumeStats->lAdditionalGUIDs);
        printf("Addtional GUIDs returned = %lu\n", pVolumeStats->ulAdditionalGUIDsReturned);
    }
    PrintStr("Driver known Disk size = %s\n", pVolumeStats->ulVolumeSize.QuadPart);
    //We are passing uniquevolumename instead of driveletter to work for mount point also 
    TCHAR UniqueVolumeName[60];
    GetUniqueVolumeName(UniqueVolumeName , pVolumeStats->VolumeGUID, sizeof(UniqueVolumeName));
//            GetFsVolumeSize(UniqueVolumeName , &fsSize,pVolumeStats->ulVolumeSize.QuadPart, &canReadDriverSize);
    PrintStr("File System Volume Size = %s\n", fsSize);
    _tprintf(_T("Can read driver volume size = %s\n"), (canReadDriverSize ? _T("Yes") : _T("No")));

    printf("Changes returned = %I64u\n", pVolumeStats->uliChangesReturnedToService.QuadPart);
    if (!IsOlderDriver)
        printf("Changes returned in Non-Data Mode = %I64u\n", pVolumeStats->liChangesReturnedToServiceInNonDataMode.QuadPart);
    _tprintf(_T("Data files returned = %#lu\n"), pVolumeStats->ullDataFilesReturned);
    printf("Changes read from bitmap = %I64u\n", pVolumeStats->uliChangesReadFromBitMap.QuadPart);
    printf("Changes writen to bitmap = %I64u\n", pVolumeStats->uliChangesWrittenToBitMap.QuadPart);
    printf("Changes queued for writing = %lu\n", pVolumeStats->ulChangesQueuedForWriting);

    PrintStr("Bytes of changes in bitmap = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesInBitmap.QuadPart);
    PrintStr("Bytes of changes Queued for writing = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesQueuedForWriting.QuadPart);
    PrintStr("Bytes of changes written to bitmap = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesWrittenToBitMap.QuadPart);
    PrintStr("Bytes of changes read from bitmap = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesReadFromBitMap.QuadPart);
    PrintStr("Bytes of changes returned = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesReturnedToService.QuadPart);
    if (!IsOlderDriver)
        PrintStr("Bytes of changes returned in Non-Data Mode = %s\n", (ULONGLONG)pVolumeStats->licbReturnedToServiceInNonDataMode.QuadPart);
    PrintStr("Bytes of changes pending commit = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesPendingCommit.QuadPart);
    PrintStr("Bytes of changes reverted from pending commit = %s\n", (ULONGLONG)pVolumeStats->ulicbChangesReverted.QuadPart);

    _tprintf(_T("Pending changes commit = %#lu\n"), pVolumeStats->ulChangesPendingCommit);
    _tprintf(_T("Changes reverted from pending commit = %lu\n"), pVolumeStats->ulChangesReverted);
    _tprintf(_T("Data files reverted = %lu\n"), pVolumeStats->ulDataFilesReverted);

    _tprintf(_T("\nNumber of DirtyBlocks in Queue = %#ld\n"), pVolumeStats->lDirtyBlocksInQueue);
    _tprintf(_T("Number of Data files pending = %#lu\n"), pVolumeStats->ulDataFilesPending);
    _tprintf(_T("Number of Data changes pending = %#lu\n"), pVolumeStats->ulDataChangesPending);
    _tprintf(_T("Number of Meta-data changes pending = %#lu\n"), pVolumeStats->ulMetaDataChangesPending);
    _tprintf(_T("Number of Bitmap changes pending = %#lu\n"), pVolumeStats->ulBitmapChangesPending);
    _tprintf(_T("Number of Total changes pending = %#lu\n"), pVolumeStats->ulTotalChangesPending);
    if (!IsOlderDriver)
        _tprintf(_T("Number of TSO drained = %#llu\n"), pVolumeStats->ullCounterTSODrained);

    PrintStr("Bytes of Data changes pending = %s\n", pVolumeStats->ullcbDataChangesPending);
    PrintStr("Bytes of Meta-data changes pending = %s\n", pVolumeStats->ullcbMetaDataChangesPending);
    PrintStr("Bytes of Bitmap changes pending = %s\n", pVolumeStats->ullcbBitmapChangesPending);
    PrintStr("Bytes of Total changes pending = %s\n\n", pVolumeStats->ullcbTotalChangesPending);
    printf("Cache hit %I64u",pVolumeStats->ullCacheHit);
    printf("      Cache Miss %I64u\n\n",pVolumeStats->ullCacheMiss);

    printf("Write order state  = %s\n", WriteOrderStateString(pVolumeStats->eWOState));
    printf("Prev write order state  = %s\n", WriteOrderStateString(pVolumeStats->ePrevWOState));
    printf("Capture mode = %s\n\n", CaptureModeString(pVolumeStats->eCaptureMode));
    switch(pVolumeStats->eWOState) {
    case ecWriteOrderStateBitmap:
        pVolumeStats->ulNumSecondsInBitmapWOState += pVolumeStats->ulNumSecondsInCurrentWOState;
        break;
    case ecWriteOrderStateMetadata:
        pVolumeStats->ulNumSecondsInMetaDataWOState += pVolumeStats->ulNumSecondsInCurrentWOState;
        break;
    case ecWriteOrderStateData:
        pVolumeStats->ulNumSecondsInDataWOState += pVolumeStats->ulNumSecondsInCurrentWOState;
        break;
    default:
        break;
    }

    switch(pVolumeStats->eCaptureMode) {
    case ecCaptureModeData:
        pVolumeStats->ulNumSecondsInDataCaptureMode += pVolumeStats->ulNumSecondsInCurrentCaptureMode;
        break;
    case ecCaptureModeMetaData:
        pVolumeStats->ulNumSecondsInMetadataCaptureMode += pVolumeStats->ulNumSecondsInCurrentCaptureMode;
        break;
    default:
        break;
    }

    printf("Number of times write order state is changed to data = %#lu\n", pVolumeStats->lNumChangeToDataWOState);
    printf("Number of times write order state is changed to meta-data = %#lu\n", pVolumeStats->lNumChangeToMetaDataWOState);
    printf("Number of times write order state is changed to meta-data on user request = %#lu\n", pVolumeStats->lNumChangeToMetaDataWOStateOnUserRequest);
    printf("Number of times write order state is changed to bitmap = %#lu\n", pVolumeStats->lNumChangeToBitmapWOState);
    printf("Number of times write order state is changed to bitmap on user request = %#lu\n", pVolumeStats->lNumChangeToBitmapWOStateOnUserRequest);

    printf("Number of seconds in current write order state = %lu\n", pVolumeStats->ulNumSecondsInCurrentWOState);
    printf("Number of seconds in data write order state = %#lu\n", pVolumeStats->ulNumSecondsInDataWOState);
    printf("Number of seconds in meta-data write order state = %#lu\n", pVolumeStats->ulNumSecondsInMetaDataWOState);
    printf("Number of seconds in bitmap write order state = %#lu\n\n", pVolumeStats->ulNumSecondsInBitmapWOState);

    printf("Number of times capture mode is changed to data = %lu\n", pVolumeStats->lNumChangeToDataCaptureMode);
    printf("Number of times capture mode is changed to meta-data = %lu\n", pVolumeStats->lNumChangeToMetaDataCaptureMode);
    printf("Number of times capture mode is changed to meta-data on user request = %#lu\n", pVolumeStats->lNumChangeToMetaDataCaptureModeOnUserRequest);

    printf("Number of seconds in current capture mode = %lu\n", pVolumeStats->ulNumSecondsInCurrentCaptureMode);
    printf("Number of seconds in data capture mode = %#lu\n", pVolumeStats->ulNumSecondsInDataCaptureMode);
    printf("Number of seconds in meta-data capture mode = %#lu\n\n", pVolumeStats->ulNumSecondsInMetadataCaptureMode);

    _tprintf(_T("VolumeReservedBlocks = %lu\n"), pVolumeStats->ulDataBlocksReserved);
    _tprintf(_T("NumberOfPageFiles = %lu\n"), pVolumeStats->ulNumOfPageFiles);
    if (pVolumeStats->bPagingFilePossible)
        _tprintf(_T("Device may be paging device\n"));

    if (pVolumeStats->IoCounterWithPagingIrpSet)
        _tprintf(_T("Writes With irp paging set : %#I64d\n"), pVolumeStats->IoCounterWithPagingIrpSet);

    if (pVolumeStats->IoCounterWithSyncPagingIrpSet)
        _tprintf(_T("Writes With sync irp paging set : %#I64d\n"), pVolumeStats->IoCounterWithSyncPagingIrpSet);

    if (!IsOlderDriver)
        _tprintf(_T("IO Counter with NULL fileobject : %#I64d\n"), pVolumeStats->IoCounterWithNullFileObject);

    if ((true == bControlPrintStats) && (!IsOlderDriver)) {
        _tprintf(_T("DevContext Allocs : %lu\t"), pVolumeStats->lCntDevContextAllocs);
        _tprintf(_T("DevContext Free : %lu\n"), pVolumeStats->lCntDevContextFree);
        _tprintf(_T("Data Pool size set : %#I64d\n"), pVolumeStats->ullDataPoolSize);

        if ((!IsOlderDriver) &&(pVolumeStats->liDriverLoadTime.QuadPart)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDriverLoadTime.QuadPart, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Driver Load time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) &&(pVolumeStats->liLastAgentStartTime.QuadPart)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastAgentStartTime.QuadPart, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Last Agent Start time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) &&(pVolumeStats->liLastAgentStopTime.QuadPart)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastAgentStopTime.QuadPart, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Last Agent Stop time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) &&(pVolumeStats->liLastS2StartTime.QuadPart)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastS2StartTime.QuadPart, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Last S2 Start time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) &&(pVolumeStats->liLastS2StopTime.QuadPart)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastS2StopTime.QuadPart, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Last S2 Stop time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) &&(pVolumeStats->liLastTagReq.QuadPart)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastTagReq.QuadPart, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Last tag request time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) && (pVolumeStats->llTimeJumpDetectedTS)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->llTimeJumpDetectedTS, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Time Jump Detected Approx. time : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }
        if ((!IsOlderDriver) && (pVolumeStats->llTimeJumpedTS)) {
            FileTimeToSystemTime((FILETIME *)&pVolumeStats->llTimeJumpedTS, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
            _tprintf(_T("Time Jumped(Forward or Backward) : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        }

        bControlPrintStats = false;
    }

    _tprintf(_T("Memory alloc failures = %lu\n"), pVolumeStats->ulNumMemoryAllocFailures);
    _tprintf(_T("Bit map set-bit errors = %lu\n"), pVolumeStats->lNumBitMapWriteErrors);
    _tprintf(_T("Bit map clear-bit errors = %lu\n"), pVolumeStats->lNumBitMapClearErrors);
    _tprintf(_T("Bit map read errors = %lu\n"), pVolumeStats->lNumBitMapReadErrors);
    _tprintf(_T("Bit map open errors = %lu\n"), pVolumeStats->lNumBitmapOpenErrors);
    _tprintf(_T("Number of tag points dropped(switching to bitmap) %u\n"), pVolumeStats->ulNumberOfTagPointsDropped);
    _tprintf(_T("Number of tag points in WriteOrder State = %u\n"), pVolumeStats->ulTagsinWOState);
    _tprintf(_T("Number of tag points dropped in Non-Write Order state = %u\n"), pVolumeStats->ulTagsinNonWOSDrop);
    _tprintf(_T("Number of tag points revoked = %u\n"), pVolumeStats->ulRevokeTagDBCount);
    if (pVolumeStats->ullLastCommittedTagTimeStamp) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->ullLastCommittedTagTimeStamp, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("TimeStamp of Last Tag : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (!IsOlderDriver) {
        _tprintf(_T("Sequence Number of Last Tag = %#I64x\n"), pVolumeStats->ullLastCommittedTagSequeneNumber);
        _tprintf(_T(" IRP with InCorrect Completion routine count : %I64d \n"), pVolumeStats->llInCorrectCompletionRoutineCount);
        _tprintf(_T(" Write IRP Count at Non Passive level : %I64d\n"), pVolumeStats->llIoCounterNonPassiveLevel);
        _tprintf(_T(" Write IRP Count seen Null MDL : %I64d\n"), pVolumeStats->llIoCounterWithNULLMdl);
        _tprintf(_T(" Write IRP Count with Invalid flags : %I64d \n"), pVolumeStats->llIoCounterWithInValidMDLFlags);
        _tprintf(_T(" MDL system VA Map fail count : %I64d \n"), pVolumeStats->llIoCounterMDLSystemVAMapFailCount);
    }
    _tprintf(_T("Bit map read state = %s\n"), BitMapStateString(pVolumeStats->eVBitmapState));

    if (0 != pVolumeStats->ulVolumeFlags) {
        _tprintf(_T("Volume Flags are\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_NOT_OPENED)
            _tprintf(_T("\tVSF_BITMAP_NOT_OPENED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_READ_NOT_STARTED)
            _tprintf(_T("\tVSF_BITMAP_READ_NOT_STARTED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_READ_IN_PROGRESS)
            _tprintf(_T("\tVSF_BITMAP_READ_IN_PROGRESS\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_READ_PAUSED)
            _tprintf(_T("\tVSF_BITMAP_READ_PAUSED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_READ_COMPLETE)
            _tprintf(_T("\tVSF_BITMAP_READ_COMPLETE\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_READ_ERROR)
            _tprintf(_T("\tVSF_BITMAP_READ_ERROR\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_WRITE_ERROR)
            _tprintf(_T("\tVSF_BITMAP_WRITE_ERROR\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_READ_ONLY)
            _tprintf(_T("\tVSF_READ_ONLY\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_FILTERING_STOPPED)
            _tprintf(_T("\tVSF_FILTERING_STOPPED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_WRITE_DISABLED)
            _tprintf(_T("\tVSF_BITMAP_WRITE_DISABLED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_READ_DISABLED)
            _tprintf(_T("\tVSF_BITMAP_READ_DISABLED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_DATA_CAPTURE_DISABLED)
            _tprintf(_T("\tVSF_DATA_CAPTURE_DISABLED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_DATA_FILES_DISABLED)
            _tprintf(_T("\tVSF_DATA_FILES_DISABLED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_CLUSTER_VOLUME)
            _tprintf(_T("\tVSF_CLUSTER_VOLUME\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_SURPRISE_REMOVED)
            _tprintf(_T("\tVSF_SURPRISE_REMOVED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_VCFIELDS_PERSISTED)
            _tprintf(_T("\tVSF_VCFIELDS_PERSISTED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_DISK_ID_CONFLICT)
            _tprintf(_T("\tVSF_DISK_ID_CONFLICT\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_DEVNUM_OBTAINED)
            _tprintf(_T("\tVSF_DEVNUM_OBTAINED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_DEVSIZE_OBTAINED)
            _tprintf(_T("\tVSF_DEVSIZE_OBTAINED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_DEV)
            _tprintf(_T("\tVSF_BITMAP_DEV\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_PHY_DEVICE_INRUSH)
            _tprintf(_T("\tVSF_PHY_DEVICE_INRUSH\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_DONT_PAGE_FAULT)
            _tprintf(_T("\tVSF_DONT_PAGE_FAULT\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_LAST_IO)
            _tprintf(_T("\tVSF_LAST_IO\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_BITMAP_DEV_OFF)
            _tprintf(_T("\tVSF_BITMAP_DEV_OFF\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_PAGE_FILE_MISSED)
            _tprintf(_T("\tVSF_PAGE_FILE_MISSED\n"));
        if (pVolumeStats->ulVolumeFlags & VSF_EXPLICIT_NONWO_NODRAIN)
            _tprintf(_T("\tVSF_EXPLICIT_NONWO_NODRAIN\n"));
    }

    _tprintf(_T("Number of Flushes observed till now = %#x\n"), pVolumeStats->ulFlushCount);
    if (pVolumeStats->ulFlushCount) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->llLastFlushTime, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("TimeStamp of last flush : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    
    if ((pVolumeStats->ucShutdownMarker > Uninitialized) && (pVolumeStats->ucShutdownMarker <= DirtyShutdown))
        _tprintf(_T("Disk Shutdown Marker:%d\n"), (int)pVolumeStats->ucShutdownMarker);

    if (0 == pVolumeStats->liDeleteBitmapTimeStamp.QuadPart) {
        if ((pVolumeStats->BitmapRecoveryState > uninit) && (pVolumeStats->BitmapRecoveryState <= resync))
            _tprintf(_T("Bitmap Recovery State:%d\n"), (int)pVolumeStats->BitmapRecoveryState);
    }
    if (pVolumeStats->liLastCommittedTimeStampReadFromStore) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastCommittedTimeStampReadFromStore, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Last committed Timestmap Read on persistent store open %#I64x, %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                            pVolumeStats->liLastCommittedTimeStampReadFromStore,
                            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liLastCommittedSequenceNumberReadFromStore)
        _tprintf(_T("Last Committed Sequence Number Read on persistent store open : %#I64x\n"), 
                            pVolumeStats->liLastCommittedSequenceNumberReadFromStore);

    if (pVolumeStats->liLastCommittedSplitIoSeqIdReadFromStore)
        _tprintf(_T("Last Committed Sequence Id for Split IO read on persistent store open : %#lu\n"), 
                            pVolumeStats->liLastCommittedSplitIoSeqIdReadFromStore);

    PrintStr("Data to disk limit = %s\n", pVolumeStats->ullcbDataToDiskLimit);
    PrintStr("Min Free Data to disk limit Before Writing Data Blocks Again = %s\n", pVolumeStats->ullcbMinFreeDataToDiskLimit);
    PrintStr("Data to disk used = %s\n", pVolumeStats->ullcbDiskUsed);
    _tprintf(_T("Filewriter thread configured priority = %#lu\n"), pVolumeStats->ulConfiguredPriority);
    _tprintf(_T("Filewriter thread running priority = %#lu\n"), pVolumeStats->ulEffectivePriority);
    _tprintf(_T("Filewriter thread id = %#x\n"), pVolumeStats->ulWriterId);
    PrintStr("Number of bytes allocated to buffer data = %s\n", pVolumeStats->ulcbDataBufferSizeAllocated);
    PrintStr("data bytes reserved for I/O = %s\n", pVolumeStats->ulcbDataBufferSizeReserved);
    PrintStr("data bytes free to use = %s\n", pVolumeStats->ulcbDataBufferSizeFree);
    if (!IsOlderDriver)
        wcout << "Total bytes tracked since boot = " << pVolumeStats->ullTotalTrackedBytes << endl;

    if (pVolumeStats->ulMuInstances) {
        _tprintf(_T("\n%-5.5s%-10.10s%-10.10s%-10.10s%-10s%-10.10s%-25.25s\n"), 
                 _T("Idx"), _T("DaToDisk"), _T("MemAlloc"), _T("MemRes"), _T("MemVCFree"), _T("MemFree"), _T("ChangeTime"));
        for (ULONG i = 0; i < pVolumeStats->ulMuInstances; i++) {
            std::string strDataToDiskSize, strMemAlocated, strMemReserved, strMemFree, strMemTotalFree, strTS;

            ConvertULLtoString(pVolumeStats->ullDuAtOutOfDataMode[i], strDataToDiskSize);
            ConvertULtoString(pVolumeStats->ulMAllocatedAtOutOfDataMode[i], strMemAlocated);
            ConvertULtoString(pVolumeStats->ulMReservedAtOutOfDataMode[i], strMemReserved);
            ConvertULtoString(pVolumeStats->ulMFreeInVCAtOutOfDataMode[i], strMemFree);
            ConvertULtoString(pVolumeStats->ulMFreeAtOutOfDataMode[i], strMemTotalFree);
            ConvertFileTimeToString((FILETIME *)&pVolumeStats->liCaptureModeChangeTS[i].QuadPart, strTS);
            printf("%-5d%-#10s%-#10s%-#10s%-#10s%-#10s%-#25s\n", i, 
                     strDataToDiskSize.c_str(), strMemAlocated.c_str(), strMemReserved.c_str(), strMemFree.c_str(), 
                     strMemTotalFree.c_str(), strTS.c_str());
        }
        _tprintf(_T("\n"));
    }

    if (pVolumeStats->ulWOSChInstances) {
        // Idx(4:4) Old (4:8) New (4:12) ChReas(9:21) NumSec(15:36) TimeStamp(26:62) BitmapChanges(6:68) MetaDataChanges(6:74) DataChanges(6:80)
        _tprintf(_T("%-4.4s%-4.4s%-4.4s%-9.9s%-15.15s%-26.26s%-6.6s%-6.6s%-6.6s\n"),
            _T("Idx"), _T("Old"), _T("New"), _T("ChReas"), _T("NumSec"), _T("TimeStamp"), _T("Bi-Ch"), _T("MD-Ch"), _T("Da-Ch"));
        for (ULONG i = 0; i < pVolumeStats->ulWOSChInstances; i++) {
            std::string strNumSec, strTS, strB_Ch, strMD_Ch, strDa_Ch;

            printf("%-4d%-#4s%-#4s%-#9s%-#15s%-#26s%-#6s%-#6s%-#6s\n",
                i, WOStateStringShortForm(pVolumeStats->eOldWOState[i]), WOStateStringShortForm(pVolumeStats->eNewWOState[i]),
                WOSchangeReasonShortForm(pVolumeStats->eWOSChangeReason[i]), 
                ConvertULSecondsToString(pVolumeStats->ulNumSecondsInWOS[i], strNumSec).c_str(),
                ConvertFileTimeToString((FILETIME *)&pVolumeStats->liWOstateChangeTS[i], strTS).c_str(), 
                ConvertSizeToString(pVolumeStats->ullcbBChangesPendingAtWOSchange[i], strB_Ch).c_str(),
                ConvertSizeToString(pVolumeStats->ullcbMDChangesPendingAtWOSchange[i], strMD_Ch).c_str(), 
                ConvertSizeToString(pVolumeStats->ullcbDChangesPendingAtWOSchange[i], strDa_Ch).c_str());
        }
        _tprintf(_T("\n"));
    }

    PrintStr("Data notify threshold is %s\n", pVolumeStats->ulcbDataNotifyThreshold);
    if (pVolumeStats->ulVolumeFlags & VSF_DATA_NOTIFY_SET) {
        _tprintf(_T("Data notify is set\n"));
    } else {
        _tprintf(_T("Data notify is not set\n"));
    }
  
    ULONG   ulLastIoSize = 0;
    _tprintf(_T("Write Counters: \n%-13s : Counter Value\n"), _T("IoSize"));
    for (ULONG ul = 0; ul < USER_MODE_MAX_IO_BUCKETS; ul++) {
        const unsigned long ulBufferSize = 80;
        TCHAR   szIoSize[ulBufferSize];
        if (0 == pVolumeStats->ulIoSizeArray[ul]) {
            ConvertSizeToString(ulLastIoSize, szIoSize, ulBufferSize);
            _tprintf(_T(">  %-6s : %#I64d\n"), szIoSize, pVolumeStats->ullIoSizeCounters[ul]);
            break;
        } else {
            ulLastIoSize = pVolumeStats->ulIoSizeArray[ul];
            ConvertSizeToString(ulLastIoSize, szIoSize, ulBufferSize);
            _tprintf(_T("<= %-6s : %#I64d\n"), szIoSize, pVolumeStats->ullIoSizeCounters[ul]);
        }
    }

    _tprintf(_T("Read Counters: \n%-13s : Counter Value\n"), _T("IoSize"));
    for (ULONG ul = 0; ul < USER_MODE_MAX_IO_BUCKETS; ul++) {
        const unsigned long ulBufferSize = 80;
        TCHAR   szIoSize[ulBufferSize];
        if (0 == pVolumeStats->ulIoSizeReadArray[ul]) {
            ConvertSizeToString(ulLastIoSize, szIoSize, ulBufferSize);
            _tprintf(_T(">  %-6s : %#I64d\n"), szIoSize, pVolumeStats->ullIoSizeReadCounters[ul]);
            break;
        } else {
            ulLastIoSize = pVolumeStats->ulIoSizeReadArray[ul];
            ConvertSizeToString(ulLastIoSize, szIoSize, ulBufferSize);
            _tprintf(_T("<= %-6s : %#I64d\n"), szIoSize, pVolumeStats->ullIoSizeReadCounters[ul]);
        }
    }

    if (pVolumeStats->liVolumeContextCreationTS.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liVolumeContextCreationTS.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume context creation time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStopFilteringTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStopFilteringTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last stop filtering event time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if ((!IsOlderDriver) &&(pVolumeStats->liStopFilteringTimestampByUser.QuadPart)) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStopFilteringTimestampByUser.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last stop filtering event time stamp by User : %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if ((!IsOlderDriver) &&(pVolumeStats->liStopFilteringAllTimeStamp.QuadPart)) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStopFilteringAllTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last stop filtering ALL event time stamp by User : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (pVolumeStats->liClearDiffsTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liClearDiffsTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last clear diffs event time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liClearStatsTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liClearStatsTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last clear stats event time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStartFilteringTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStartFilteringTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last start filtering event time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStartFilteringTimeStampByUser.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStartFilteringTimeStampByUser.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last start filtering event time stamp issued by User: %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liGetDBTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liGetDBTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last Get DB time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liCommitDBTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liCommitDBTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last Commit DB time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liResyncStartTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liResyncStartTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last resync start event time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liResyncEndTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liResyncEndTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("last resync end event time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liOutOfSyncIndicatedTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liOutOfSyncIndicatedTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume Out of sync flag is indicated on : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liOutOfSyncResetTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liOutOfSyncResetTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume Out of sync flag is reset on : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if(pVolumeStats->liLastOutOfSyncTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastOutOfSyncTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Last out of sync time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d,"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (!IsOlderDriver) {
        if (pVolumeStats->ullLastOutOfSyncSeqNumber) {
            _tprintf(_T("Sequence number : %I64x,"), pVolumeStats->ullLastOutOfSyncSeqNumber);
        }
        if (pVolumeStats->ulLastOutOfSyncErrorCode) {
            _tprintf(_T("Error Code : %x \n"), pVolumeStats->ulLastOutOfSyncErrorCode);
        }
    }
    if(pVolumeStats->liDeleteBitmapTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDeleteBitmapTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Last Delete Bitmap File time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if(pVolumeStats->liMountNotifyTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liMountNotifyTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume Mount Notify time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if(pVolumeStats->liDisMountNotifyTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDisMountNotifyTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume Unmount Notify time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if(pVolumeStats->liDisMountFailNotifyTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDisMountFailNotifyTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume Unmount Failed Notify time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if ((!IsOlderDriver) && (pVolumeStats->ulOutOfSyncTotalCount)) {
        _tprintf(_T("Total out of sync count from last boot : %x \n"), pVolumeStats->ulOutOfSyncTotalCount);
    }
    if (pVolumeStats->ulOutOfSyncCount) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liOutOfSyncTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Volume is Out of sync with count = %#x, ErrorCode = %#x, ErrorStatus = %#x\nlast time stamp = %02d/%02d/%04d %02d:%02d:%02d:%04d, seq number = %#I64x\n"),
                 pVolumeStats->ulOutOfSyncCount,
                 pVolumeStats->ulOutOfSyncErrorCode,
                 pVolumeStats->ulOutOfSyncErrorStatus,
                 LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                 LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
                 pVolumeStats->ullOutOfSyncSeqNumber);

        // look up the text from indskflt.sys
        if (pVolumeStats->ulOutOfSyncErrorCode > ERROR_TO_REG_MAX_ERROR) {
            HMODULE hModule = NULL;

            std::vector<WCHAR>      VolumeGUID(GUID_SIZE_IN_CHARS + 1, L'\0');
            std::vector<WCHAR>      VolumeDriveLetter(2,  L'\0');

            // Manifest events start from 10001 in the indskflt driver.
            const USHORT LegacyMCEventIDStart = 0, LegacyMCEventIDEnd = 10000;
            // Truncate and get the least significant 16-bits.
            USHORT outOfSyncEvtID = (USHORT)pVolumeStats->ulOutOfSyncErrorCode;


            std::vector<const WCHAR *>  Parameters(100, L"");
            std::vector<WCHAR>          errTxt(1024, 0);

            int ind = 0;

            // MC events will have parameters in the format string starting from %2, while
            // manifest events will have parameters in the format string starting from %1.
            if (outOfSyncEvtID >= LegacyMCEventIDStart && outOfSyncEvtID <= LegacyMCEventIDEnd) {
                Parameters[ind++] = L""; // %1 - MC - Dummy. Not used in formatting.
            }

            // provide proper stucture that the driver errmsg can use to display the drive
            Parameters[ind++] = &VolumeDriveLetter[0]; // %2 - MC | %1 - Man
            Parameters[ind++] = &VolumeGUID[0];        // %3 - MC | %2 - Man
            Parameters[ind++] = L""; // %4 - MC | %3 - Man - Additional parameter used in few messages.

            VolumeDriveLetter[0] = pVolumeStats->DriveLetter;
            inm_memcpy_s(&VolumeGUID[0], GUID_SIZE_IN_CHARS * sizeof(WCHAR), pVolumeStats->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

            std::vector<WCHAR>          involfltString(MAX_PATH, L'\0');
            std::vector<WCHAR>          systemRoot(MAX_PATH, L'\0');

            if (0 == GetSystemDirectoryW(&systemRoot[0], systemRoot.size() * sizeof(WCHAR))) {
                _tprintf(_T("Failed to get SystemDirectory with error: %d\n"), GetLastError());
            } else {
                StringCchPrintfW(&involfltString[0], involfltString.size(), L"%s\\drivers\\indskflt.sys", &systemRoot[0]);
                hModule = LoadLibraryExW(&involfltString[0], NULL, LOAD_LIBRARY_AS_DATAFILE);

                if (NULL == hModule) {
                    _tprintf(_T("Failed to load library with error: %d\n"), GetLastError());
                } else {
                    if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                        hModule,
                        pVolumeStats->ulOutOfSyncErrorCode,
                        0,
                        &errTxt[0],
                        errTxt.size(),
                        (va_list *)&Parameters[0]))
                    {
                        _tprintf(_T("Failed to FormatMessage with error: %d\n"), GetLastError());
                    } else {
                        wprintf(L"Out of sync error message : \"%s\"\n", errTxt);
                    }

                    FreeLibrary(hModule);
                }
            }
        } else {
            wprintf(L"Out of sync error message : \"%s\"\n", pVolumeStats->ErrorStringForResync);
        }

    }
    _tprintf(_T("\n"));

    return;
}
void
PrintVolumeStatsCompact(PVOLUME_STATS pVolumeStats, int IsOlderDriver)
{
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;
    unsigned long long      fsSize = 0;
    bool                    canReadDriverSize = false;

    GetTimeZoneInformation(&TimeZone);
    if (true == IsDiskFilterRunning) {
        wprintf(L",ID=%.*s.", GUID_SIZE_IN_CHARS, pVolumeStats->VolumeGUID);
    }
    else {
        wprintf(L"DriveLetter = %c: VolumeGUID = %.*s.\n", pVolumeStats->DriveLetter,
            GUID_SIZE_IN_CHARS, pVolumeStats->VolumeGUID);
    }
    if (false == IsDiskFilterRunning){
        printf("Additonal GUIDs = %ld\n", pVolumeStats->lAdditionalGUIDs);
        printf("Addtional GUIDs returned = %lu\n", pVolumeStats->ulAdditionalGUIDsReturned);
    }
    printf(",DDSb:%I64u", pVolumeStats->ulVolumeSize.QuadPart);
    PrintStr(",DDS%s", pVolumeStats->ulVolumeSize.QuadPart);
    _tprintf(_T("@ChRet"));
    printf(",%I64u", pVolumeStats->uliChangesReturnedToService.QuadPart);
    if (!IsOlderDriver)
        printf(",CRNonDataMode : %I64u", pVolumeStats->liChangesReturnedToServiceInNonDataMode.QuadPart);

    if ((true == bControlPrintStats) && (!IsOlderDriver)) {
        _tprintf(_T(",DevCA:%lu"), pVolumeStats->lCntDevContextAllocs);
        _tprintf(_T(",DevCF:%lu\n"), pVolumeStats->lCntDevContextFree);
        _tprintf(_T(",DPPset:%#I64d\n"), pVolumeStats->ullDataPoolSize);
        bControlPrintStats = false;
    }
    _tprintf(_T(",%#lu"), pVolumeStats->ullDataFilesReturned);
    printf(",%I64u", pVolumeStats->uliChangesReadFromBitMap.QuadPart);
    printf(",%I64u", pVolumeStats->uliChangesWrittenToBitMap.QuadPart);
    printf(",%lu", pVolumeStats->ulChangesQueuedForWriting);
    if (!IsOlderDriver)
        wcout << ",TotTrk," << pVolumeStats->ullTotalTrackedBytes;

    _tprintf(_T(",ByteCh"));
    // size is already separated by comma
    // @ indicates ignore comma till it finds this symbol
    PrintStr(",%s", (ULONGLONG)pVolumeStats->ulicbChangesInBitmap.QuadPart);
    PrintStr("@%s", (ULONGLONG)pVolumeStats->ulicbChangesQueuedForWriting.QuadPart);
    PrintStr("@%s", (ULONGLONG)pVolumeStats->ulicbChangesWrittenToBitMap.QuadPart);
    PrintStr("@%s", (ULONGLONG)pVolumeStats->ulicbChangesReadFromBitMap.QuadPart);
    PrintStr("@%s", (ULONGLONG)pVolumeStats->ulicbChangesReturnedToService.QuadPart);
    if (!IsOlderDriver)
        PrintStr("@%s", (ULONGLONG)pVolumeStats->licbReturnedToServiceInNonDataMode.QuadPart);

    PrintStr("@%s", (ULONGLONG)pVolumeStats->ulicbChangesPendingCommit.QuadPart);
    PrintStr("@%s", (ULONGLONG)pVolumeStats->ulicbChangesReverted.QuadPart);

    _tprintf(_T("@%#lu"), pVolumeStats->ulChangesPendingCommit);
    _tprintf(_T(",%lu"), pVolumeStats->ulChangesReverted);
    _tprintf(_T(",%lu"), pVolumeStats->ulDataFilesReverted);
    _tprintf(_T(",DBInfo"));
    _tprintf(_T(",%#ld"), pVolumeStats->lDirtyBlocksInQueue);
    _tprintf(_T(",%#lu"), pVolumeStats->ulDataFilesPending);
    _tprintf(_T(",%#lu"), pVolumeStats->ulDataChangesPending);
    _tprintf(_T(",%#lu"), pVolumeStats->ulMetaDataChangesPending);
    _tprintf(_T(",%#lu"), pVolumeStats->ulBitmapChangesPending);
    _tprintf(_T(",%#lu"), pVolumeStats->ulTotalChangesPending);
     if (!IsOlderDriver)
        _tprintf(_T(",TSO drained : %#llu"), pVolumeStats->ullCounterTSODrained);
    _tprintf(_T(",DBInfoCh"));
    PrintStr("@%s", pVolumeStats->ullcbDataChangesPending);
    PrintStr("@%s", pVolumeStats->ullcbMetaDataChangesPending);
    PrintStr("@%s", pVolumeStats->ullcbBitmapChangesPending);
    PrintStr("@%s", pVolumeStats->ullcbTotalChangesPending);
    printf("@%I64u", pVolumeStats->ullCacheHit);
    printf(",%I64u", pVolumeStats->ullCacheMiss);
    
    printf(",WOS%d", pVolumeStats->eWOState);// parser logic for Wo State
    printf(",PWOS%d", pVolumeStats->ePrevWOState);// parser logic for Wo State
    printf(",CM%d", pVolumeStats->eCaptureMode);// parser logic for capture mode
    switch (pVolumeStats->eWOState) {
    case ecWriteOrderStateBitmap:
        pVolumeStats->ulNumSecondsInBitmapWOState += pVolumeStats->ulNumSecondsInCurrentWOState;
        break;
    case ecWriteOrderStateMetadata:
        pVolumeStats->ulNumSecondsInMetaDataWOState += pVolumeStats->ulNumSecondsInCurrentWOState;
        break;
    case ecWriteOrderStateData:
        pVolumeStats->ulNumSecondsInDataWOState += pVolumeStats->ulNumSecondsInCurrentWOState;
        break;
    default:
        break;
    }

    switch (pVolumeStats->eCaptureMode) {
    case ecCaptureModeData:
        pVolumeStats->ulNumSecondsInDataCaptureMode += pVolumeStats->ulNumSecondsInCurrentCaptureMode;
        break;
    case ecCaptureModeMetaData:
        pVolumeStats->ulNumSecondsInMetadataCaptureMode += pVolumeStats->ulNumSecondsInCurrentCaptureMode;
        break;
    default:
        break;
    }
    _tprintf(_T(",WOS_ChangesN"));
    printf(",%#lu", pVolumeStats->lNumChangeToDataWOState);
    printf(",%#lu", pVolumeStats->lNumChangeToMetaDataWOState);
    printf(",%#lu", pVolumeStats->lNumChangeToMetaDataWOStateOnUserRequest);
    printf(",%#lu", pVolumeStats->lNumChangeToBitmapWOState);
    printf(",%#lu", pVolumeStats->lNumChangeToBitmapWOStateOnUserRequest);
    _tprintf(_T(",WOS_ChangesS"));
    printf(",%lu", pVolumeStats->ulNumSecondsInCurrentWOState);
    printf(",%#lu", pVolumeStats->ulNumSecondsInDataWOState);
    printf(",%#lu", pVolumeStats->ulNumSecondsInMetaDataWOState);
    printf(",%#lu", pVolumeStats->ulNumSecondsInBitmapWOState);
    _tprintf(_T(",CM_ChangesN"));
    printf(",%lu", pVolumeStats->lNumChangeToDataCaptureMode);
    printf(",%lu", pVolumeStats->lNumChangeToMetaDataCaptureMode);
    printf(",%#lu", pVolumeStats->lNumChangeToMetaDataCaptureModeOnUserRequest);
    _tprintf(_T(",CM_ChangesS"));
    printf(",%lu", pVolumeStats->ulNumSecondsInCurrentCaptureMode);
    printf(",%#lu", pVolumeStats->ulNumSecondsInDataCaptureMode);
    printf(",%#lu", pVolumeStats->ulNumSecondsInMetadataCaptureMode);

    _tprintf(_T(",RB%lu"), pVolumeStats->ulDataBlocksReserved);
    _tprintf(_T(",NPF%lu"), pVolumeStats->ulNumOfPageFiles);
    if (pVolumeStats->bPagingFilePossible)
        _tprintf(_T(",PaF"));

    if (pVolumeStats->IoCounterWithPagingIrpSet)
        _tprintf(_T(",PagingIRPSet : %#I64d"), pVolumeStats->IoCounterWithPagingIrpSet);

    if (pVolumeStats->IoCounterWithSyncPagingIrpSet)
        _tprintf(_T(",SyncPagingIRPSet : %#I64d"), pVolumeStats->IoCounterWithSyncPagingIrpSet);

    if (!IsOlderDriver)
        _tprintf(_T(",FileObj Set NULL: %#I64d"), pVolumeStats->IoCounterWithNullFileObject);

    _tprintf(_T(",MemF%lu"), pVolumeStats->ulNumMemoryAllocFailures);
    _tprintf(_T(",BitmapErr"));
    _tprintf(_T(",%lu"), pVolumeStats->lNumBitMapWriteErrors);
    _tprintf(_T(",%lu"), pVolumeStats->lNumBitMapClearErrors);
    _tprintf(_T(",%lu"), pVolumeStats->lNumBitMapReadErrors);
    _tprintf(_T(",%lu"), pVolumeStats->lNumBitmapOpenErrors);
    _tprintf(_T(",Tags"));
    _tprintf(_T(",%u"), pVolumeStats->ulNumberOfTagPointsDropped);
    _tprintf(_T(",%u"), pVolumeStats->ulTagsinWOState);
    _tprintf(_T(",%u"), pVolumeStats->ulTagsinNonWOSDrop);
    _tprintf(_T(",%u"), pVolumeStats->ulRevokeTagDBCount);
    if (pVolumeStats->ullLastCommittedTagTimeStamp) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->ullLastCommittedTagTimeStamp, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",TSLastTag : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (!IsOlderDriver) {
         _tprintf(_T(",SNLastTag : %I64x"), pVolumeStats->ullLastCommittedTagSequeneNumber);
        _tprintf(_T(", InCorrect Completion Count: %I64d"), pVolumeStats->llInCorrectCompletionRoutineCount);
        _tprintf(_T(", Write Count at NonPassiveLevel : %I64d"), pVolumeStats->llIoCounterNonPassiveLevel);
        _tprintf(_T(",IO Counter with NULL MDL : %I64d"), pVolumeStats->llIoCounterWithNULLMdl);
        _tprintf(_T(",IO Counter with Invalid MDL flags : %I64d"), pVolumeStats->llIoCounterWithInValidMDLFlags);
        _tprintf(_T(",MDL system VA Map fail count : %I64d"), pVolumeStats->llIoCounterMDLSystemVAMapFailCount);
    }
    _tprintf(_T(",BMS%d"), (int)pVolumeStats->eVBitmapState);// parser logic for bitmap state

    if ((true == bControlPrintStats) && (!IsOlderDriver)) {
        _tprintf(_T(",DevContext Allocs : %lu"), pVolumeStats->lCntDevContextAllocs);
        _tprintf(_T(",DevContext Free : %lu"), pVolumeStats->lCntDevContextFree);
        bControlPrintStats = false;
    }

    _tprintf(_T(",VF%#x"), pVolumeStats->ulVolumeFlags);

    _tprintf(_T(",NF%#x"), pVolumeStats->ulFlushCount);
    if (pVolumeStats->ulFlushCount) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->llLastFlushTime, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",LFTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if ((pVolumeStats->ucShutdownMarker > Uninitialized) && (pVolumeStats->ucShutdownMarker <= DirtyShutdown))
    _tprintf(_T(",DiskSDMarker : %d"), (int)pVolumeStats->ucShutdownMarker);
    if (0 == pVolumeStats->liDeleteBitmapTimeStamp.QuadPart) {
        if ((pVolumeStats->BitmapRecoveryState > uninit) && (pVolumeStats->BitmapRecoveryState <= resync))
            _tprintf(_T(",Bitmap Recovery State:%d"), (int)pVolumeStats->BitmapRecoveryState);
    }

    if (pVolumeStats->liLastCommittedTimeStampReadFromStore) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastCommittedTimeStampReadFromStore, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",LCTS%#I64x %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            pVolumeStats->liLastCommittedTimeStampReadFromStore,
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liLastCommittedSequenceNumberReadFromStore)
        _tprintf(_T(",LCSN%#I64x"), pVolumeStats->liLastCommittedSequenceNumberReadFromStore);

    if (pVolumeStats->liLastCommittedSplitIoSeqIdReadFromStore)
        _tprintf(_T(",LCSID%#lu"), pVolumeStats->liLastCommittedSplitIoSeqIdReadFromStore);

    PrintStr(",BY1%s", pVolumeStats->ulcbDataBufferSizeAllocated);//replace with \n
    PrintStr("@BY2%s", pVolumeStats->ulcbDataBufferSizeReserved);//replace with \n
    PrintStr("@BY3%s@", pVolumeStats->ulcbDataBufferSizeFree);//replace with \n

    if (pVolumeStats->ulMuInstances) {
        //_tprintf(_T("\n%-5.5s/*%-10.10s*/%-10.10s%-10.10s%-10s%-10.10s%-25.25s\n"),
        //    _T("Idx"),/* _T("DaToDisk"),*/ _T("MemAlloc"), _T("MemRes"), _T("MemVCFree"), _T("MemFree"), _T("ChangeTime"));
        _tprintf(_T("\n%-5.5s%-10.10s%-10.10s%-10s%-10.10s%-25.25s,"),
            _T("Idx"),_T("MemAlloc"), _T("MemRes"), _T("MemVCFree"), _T("MemFree"), _T("ChangeTime"));

        for (ULONG i = 0; i < pVolumeStats->ulMuInstances; i++) {
            std::string strMemAlocated, strMemReserved, strMemFree, strMemTotalFree, strTS;

            ConvertULtoString(pVolumeStats->ulMAllocatedAtOutOfDataMode[i], strMemAlocated);
            ConvertULtoString(pVolumeStats->ulMReservedAtOutOfDataMode[i], strMemReserved);
            ConvertULtoString(pVolumeStats->ulMFreeInVCAtOutOfDataMode[i], strMemFree);
            ConvertULtoString(pVolumeStats->ulMFreeAtOutOfDataMode[i], strMemTotalFree);
            ConvertFileTimeToString((FILETIME *)&pVolumeStats->liCaptureModeChangeTS[i].QuadPart, strTS);
            printf("%-5d%-#10s%-#10s%-#10s%-#10s%-#25s,", i,
                strMemAlocated.c_str(), strMemReserved.c_str(), strMemFree.c_str(),
                strMemTotalFree.c_str(), strTS.c_str());
        }
        _tprintf(_T("\n"));
    }

    if (pVolumeStats->ulWOSChInstances) {
        // Idx(4:4) Old (4:8) New (4:12) ChReas(9:21) NumSec(15:36) TimeStamp(26:62) BitmapChanges(6:68) MetaDataChanges(6:74) DataChanges(6:80)
        _tprintf(_T("%-4.4s%-4.4s%-4.4s%-9.9s%-15.15s%-26.26s%-6.6s%-6.6s%-6.6s,"),
            _T("Idx"), _T("Old"), _T("New"), _T("ChReas"), _T("NumSec"), _T("TimeStamp"), _T("Bi-Ch"), _T("MD-Ch"), _T("Da-Ch"));
        for (ULONG i = 0; i < pVolumeStats->ulWOSChInstances; i++) {
            std::string strNumSec, strTS, strB_Ch, strMD_Ch, strDa_Ch;

            printf("%-4d%-#4s%-#4s%-#9s%-#15s%-#26s%-#6s%-#6s%-#6s,",
                i, WOStateStringShortForm(pVolumeStats->eOldWOState[i]), WOStateStringShortForm(pVolumeStats->eNewWOState[i]),
                WOSchangeReasonShortForm(pVolumeStats->eWOSChangeReason[i]),
                ConvertULSecondsToString(pVolumeStats->ulNumSecondsInWOS[i], strNumSec).c_str(),
                ConvertFileTimeToString((FILETIME *)&pVolumeStats->liWOstateChangeTS[i], strTS).c_str(),
                ConvertSizeToString(pVolumeStats->ullcbBChangesPendingAtWOSchange[i], strB_Ch).c_str(),
                ConvertSizeToString(pVolumeStats->ullcbMDChangesPendingAtWOSchange[i], strMD_Ch).c_str(),
                ConvertSizeToString(pVolumeStats->ullcbDChangesPendingAtWOSchange[i], strDa_Ch).c_str());
        }
        _tprintf(_T("\n"));
    }

    PrintStr("DNT%s@", pVolumeStats->ulcbDataNotifyThreshold);
    if (pVolumeStats->ulVolumeFlags & VSF_DATA_NOTIFY_SET) {
        _tprintf(_T(",DNS")); // stop parsing based on the DNS
    }
    else {
        _tprintf(_T(",DNNS"));
    }

    ULONG   ulLastIoSize = 0;
    //_tprintf(_T("Write Counters: \n%-13s : Counter Value\n"), _T("IoSize"));
    _tprintf(_T(",WC"));
    for (ULONG ul = 0; ul < USER_MODE_MAX_IO_BUCKETS; ul++) {
        const unsigned long ulBufferSize = 80;
        TCHAR   szIoSize[ulBufferSize];
        if (0 == pVolumeStats->ulIoSizeArray[ul]) {
            ConvertSizeToString(ulLastIoSize, szIoSize, ulBufferSize);
            _tprintf(_T(",>%s:%#I64d"), szIoSize, pVolumeStats->ullIoSizeCounters[ul]);
            break;
        }
        else {
            ulLastIoSize = pVolumeStats->ulIoSizeArray[ul];
            ConvertSizeToString(ulLastIoSize, szIoSize, ulBufferSize);
            _tprintf(_T(",<=%s:%#I64d"), szIoSize, pVolumeStats->ullIoSizeCounters[ul]);
        }
    }

    if (pVolumeStats->liVolumeContextCreationTS.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liVolumeContextCreationTS.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",VCTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStopFilteringTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStopFilteringTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",StopFTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStopFilteringTimestampByUser.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStopFilteringTimestampByUser.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",StopFSU: %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (pVolumeStats->liClearDiffsTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liClearDiffsTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",CDTS %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (pVolumeStats->liClearStatsTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liClearStatsTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",CSTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStartFilteringTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStartFilteringTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",StartFTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liStartFilteringTimeStampByUser.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liStartFilteringTimeStampByUser.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",StartFSU: %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liGetDBTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liGetDBTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",GDBTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liCommitDBTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liCommitDBTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",CDBTS %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liResyncStartTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liResyncStartTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",RSTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liResyncEndTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liResyncEndTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",RETS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liOutOfSyncIndicatedTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liOutOfSyncIndicatedTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",OOSITS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liOutOfSyncResetTimeStamp.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liOutOfSyncResetTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",OOSRTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liLastOutOfSyncTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liLastOutOfSyncTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",Last Out of Sync TimeStamp = %02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (!IsOlderDriver) {
        if (pVolumeStats->ullLastOutOfSyncSeqNumber) {
            _tprintf(_T(",Last Out of Sync SequenceNumber : %#I64x"), pVolumeStats->ullLastOutOfSyncSeqNumber);
        }

        if (pVolumeStats->ulLastOutOfSyncErrorCode) {
            _tprintf(_T(",Last Out of Sync ErrCode : %#x,"), pVolumeStats->ulLastOutOfSyncErrorCode);
        }
    }
    if (pVolumeStats->liDeleteBitmapTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDeleteBitmapTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",DBiTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liMountNotifyTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liMountNotifyTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",VMNTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if (pVolumeStats->liDisMountNotifyTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDisMountNotifyTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",VUMNTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (pVolumeStats->liDisMountFailNotifyTimeStamp.QuadPart){
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liDisMountFailNotifyTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",VUMNTFTS%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }

    if ((!IsOlderDriver) && (pVolumeStats->ulOutOfSyncTotalCount)) {
        _tprintf(_T(",Total Out of sync : %lu "), pVolumeStats->ulOutOfSyncTotalCount);
    }

    if (pVolumeStats->ulOutOfSyncCount) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStats->liOutOfSyncTimeStamp.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",OutOfSyncSwitchCount:%#x ErrorCode : %#x ErrorStatus:%#x time stamp : %02d/%02d/%04d %02d:%02d:%02d:%04d, seq number = %#I64x\n"),
            pVolumeStats->ulOutOfSyncCount,
            pVolumeStats->ulOutOfSyncErrorCode,
            pVolumeStats->ulOutOfSyncErrorStatus,
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
            pVolumeStats->ullOutOfSyncSeqNumber);

        // look up the text from indskflt.sys
        if (pVolumeStats->ulOutOfSyncErrorCode > ERROR_TO_REG_MAX_ERROR) {
            HMODULE hModule = NULL;
            WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 1] = { 0 };
            WCHAR   VolumeDriveLetter[2] = { 0 };

            // Manifest events start from 10001 in the indskflt driver.
            const USHORT LegacyMCEventIDStart = 0, LegacyMCEventIDEnd = 10000;
            // Truncate and get the least significant 16-bits.
            USHORT outOfSyncEvtID = (USHORT)pVolumeStats->ulOutOfSyncErrorCode;

            const WCHAR * Parameters[4];
            WCHAR errTxt[1024] = { 0 };
            int ind = 0;

            // MC events will have parameters in the format string starting from %2, while
            // manifest events will have parameters in the format string starting from %1.
            if (outOfSyncEvtID >= LegacyMCEventIDStart && outOfSyncEvtID <= LegacyMCEventIDEnd) {
                Parameters[ind++] = L""; // %1 - MC - Dummy. Not used in formatting.
            }

            // provide proper stucture that the driver errmsg can use to display the drive
            Parameters[ind++] = VolumeDriveLetter; // %2 - MC | %1 - Man
            Parameters[ind++] = VolumeGUID;        // %3 - MC | %2 - Man
            Parameters[ind++] = L""; // %4 - MC | %3 - Man - Additional parameter used in few messages.

            VolumeDriveLetter[0] = pVolumeStats->DriveLetter;
            inm_memcpy_s(VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pVolumeStats->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

            WCHAR involfltString[MAX_PATH];
            WCHAR systemRoot[MAX_PATH];

            if (0 == GetSystemDirectoryW(systemRoot, sizeof(systemRoot))) {
                _tprintf(_T("Failed to get SystemDirectory with error: %d\n"), GetLastError());
            }
            else {
                StringCchPrintfW(involfltString, ARRAYSIZE(involfltString), L"%s\\drivers\\indskflt.sys", systemRoot);
                hModule = LoadLibraryExW(involfltString, NULL, LOAD_LIBRARY_AS_DATAFILE);

                if (NULL == hModule) {
                    _tprintf(_T("Failed to load library with error: %d\n"), GetLastError());
                }
                else {
                    if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                        hModule,
                        pVolumeStats->ulOutOfSyncErrorCode,
                        0,
                        errTxt,
                        ARRAYSIZE(errTxt),
                        (va_list *)Parameters))
                    {
                        _tprintf(_T("Failed to FormatMessage with error: %d\n"), GetLastError());
                    }
                    else {
                        wprintf(L"Out of sync error message : \"%s\"", errTxt);
                    }

                    FreeLibrary(hModule);
                }
            }
        }
        else {
            wprintf(L"Out of sync error message : \"%s\"\n", pVolumeStats->ErrorStringForResync);
        }

    }

    return;
}
void GetOSVersion()
{
    OSVERSIONINFOEX osviex;
    ZeroMemory(&osviex, sizeof(OSVERSIONINFOEX));

    osviex.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    // Get the build number, product type and service pack
    if (GetVersionEx((OSVERSIONINFO*) &osviex)) {
        printf("Operating System Version = %d.%d.%d\n", osviex.dwMajorVersion, osviex.dwMinorVersion, osviex.dwBuildNumber);
        printf("Service Pack = %d.%d \n", osviex.wServicePackMajor, osviex.wServicePackMinor);
        printf("Product Type = ");
        if (osviex.wProductType == (BYTE)0x0000003)
            printf ("Server Edition\n");
        else if(osviex.wProductType == (BYTE)0x0000001)
            printf("WorkStation edition \n");
        else
            printf("Domain Controller Edition\n " );
        if(0x00000080 == osviex.wSuiteMask)
            printf("Suite Type = Data Center Edition\n");
        else if(0x00000002 == osviex.wSuiteMask)
            printf("Suite Type = Enterprise Edition\n");

    } else {
        printf(" Failed to get the details about Operating System : %d", GetLastError());
    }
}

/*
This routine checks whether the driver version is less than 9.5(A2A). Earlier version does not zero out the reseved space and
sends random values in upgrade scenario. This change is part of 9.6 V2A release. 

*/
int
CheckForOlderDriverVersion(HANDLE hVFCtrlDevice) {

    DRIVER_VERSION  DriverVersion = {0};
    BOOL    bResult;
    DWORD   dwReturn, dwInputSize = 0;

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_GET_VERSION,
                     NULL,
                     0,
                     &DriverVersion,
                     sizeof(DRIVER_VERSION),
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    
    if (bResult) {
        _tprintf(_T("Driver Version %hd.%hd.%hd.%hd\n"), DriverVersion.ulDrMajorVersion, DriverVersion.ulDrMinorVersion,
                    DriverVersion.ulDrMinorVersion2, DriverVersion.ulDrMinorVersion3);
        _tprintf(_T("Product Version %hd.%hd.%hd.%hd\n"), DriverVersion.ulPrMajorVersion, DriverVersion.ulPrMinorVersion,
                    DriverVersion.ulPrBuildNumber, DriverVersion.ulPrMinorVersion2);

        if ((DriverVersion.ulPrMajorVersion <= 9) && (DriverVersion.ulPrMinorVersion < 5))
            return 1;
    }
    else
        _tprintf(_T("Returned Failed from get driver version DeviceIoControl call: %d\n"), GetLastError());

    return 0;
}

void GetOSVersionCompact()
{
    OSVERSIONINFOEX osviex;
    ZeroMemory(&osviex, sizeof(OSVERSIONINFOEX));

    osviex.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    // Get the build number, product type and service pack
    if (GetVersionEx((OSVERSIONINFO*)&osviex)) {
        printf(",OSV%d.%d.%d", osviex.dwMajorVersion, osviex.dwMinorVersion, osviex.dwBuildNumber);
        printf(",SP%d.%d", osviex.wServicePackMajor, osviex.wServicePackMinor);
    }
    else {
        printf(" Failed to get the details about Operating System : %d", GetLastError());
    }
}
void
GetDriverStats(HANDLE hVFCtrlDevice, PPRINT_STATISTICS_DATA pPrintStatisticsData)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn, ulNumVolumes, dwOutputSize;
    PVOLUME_STATS_DATA  pVolumeStatsData;
    TIME_ZONE_INFORMATION lpTimeZoneInfo;

    dwReturn = GetTimeZoneInformation(&lpTimeZoneInfo);
    if(dwReturn == TIME_ZONE_ID_STANDARD || dwReturn == TIME_ZONE_ID_UNKNOWN)
        wprintf(L" Time Zone : %s\n", lpTimeZoneInfo.StandardName);
    else if( dwReturn == TIME_ZONE_ID_DAYLIGHT )
        wprintf(L"Time Zone : %s\n", lpTimeZoneInfo.DaylightName);

     // Get Operating System details along with service pack and suite type
    if (sCommandOptions.bCompact)
        GetOSVersionCompact();
    else
        GetOSVersion();

    pVolumeStatsData = (PVOLUME_STATS_DATA)malloc(sizeof(VOLUME_STATS_DATA) + sizeof(VOLUME_STATS));
	ZeroMemory(pVolumeStatsData, (sizeof(VOLUME_STATS_DATA)+sizeof(VOLUME_STATS)));

    if (NULL == pVolumeStatsData) {
        printf("GetDriverStats: Failed in memory allocation\n");
        return;
    }

    if(NULL == pPrintStatisticsData->MountPoint) {
        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_VOLUME_STATS,
                        NULL,
                        0,
                        pVolumeStatsData,
                        sizeof(VOLUME_STATS_DATA) + sizeof(VOLUME_STATS),
                        &dwReturn,  
                        NULL        // Overlapped
                        );
    } else {
        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_VOLUME_STATS,
                        pPrintStatisticsData->VolumeGUID,
                        sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                        pVolumeStatsData,
                        sizeof(VOLUME_STATS_DATA) + sizeof(VOLUME_STATS),
                        &dwReturn,  
                        NULL        // Overlapped
                        );
    }

    if (bResult) {
        if ((NULL == pPrintStatisticsData->MountPoint) && 
            (pVolumeStatsData->ulTotalVolumes > pVolumeStatsData->ulVolumesReturned)) 
        {

            LONG lAdditionalGUIDs = 0;

            if (true == IsDiskFilterRunning)
                lAdditionalGUIDs = 0;

            // We asked for all volumes and buffer wasn't enough.
            ulNumVolumes = pVolumeStatsData->ulTotalVolumes;

            free(pVolumeStatsData);
            dwOutputSize = sizeof(VOLUME_STATS_DATA) + (ulNumVolumes * sizeof(VOLUME_STATS)) +
                            (lAdditionalGUIDs * GUID_SIZE_IN_CHARS * sizeof(WCHAR));
            pVolumeStatsData = (PVOLUME_STATS_DATA)malloc(dwOutputSize);			
            ZeroMemory(pVolumeStatsData, dwOutputSize);
            bResult = DeviceIoControl(
                            hVFCtrlDevice,
                            (DWORD)IOCTL_INMAGE_GET_VOLUME_STATS,
                            NULL,
                            0,
                            pVolumeStatsData,
                            dwOutputSize,
                            &dwReturn,  
                            NULL        // Overlapped
                            );
        }
    }

    if (bResult) {
        if ((pVolumeStatsData->usMajorVersion != VOLUME_STATS_DATA_MAJOR_VERSION) ||
            (pVolumeStatsData->usMinorVersion != VOLUME_STATS_DATA_MINOR_VERSION)) 
        {
            printf("VolumeStatsData version mismatch. DrvUtil Version %d.%d DriverVersion %d.%d\n",
                    VOLUME_STATS_DATA_MAJOR_VERSION, VOLUME_STATS_DATA_MAJOR_VERSION, pVolumeStatsData->usMajorVersion, pVolumeStatsData->usMinorVersion);
        } else if (sCommandOptions.bCompact) {
            int IsOlderDriver = 0;
            IsOlderDriver = CheckForOlderDriverVersion(hVFCtrlDevice);
            PrintDriverStatsCompact(pVolumeStatsData, dwReturn, IsOlderDriver);
        } else {
            int IsOlderDriver = 0;
            IsOlderDriver = CheckForOlderDriverVersion(hVFCtrlDevice);
            PrintDriverStats(pVolumeStatsData, dwReturn, IsOlderDriver);
        }
    }
	else {
        _tprintf(_T("Returned Failed from GetDriverStats DeviceIoControl: %d\n"), GetLastError());
    }

    free(pVolumeStatsData);

    return;
}

void 
GetGlobalTSSN(HANDLE hVFCtrlDevice)
{
    DWORD dwReturn;
    BOOL bResult;
    GLOBAL_TIMESTAMP GlobalTSSN;

    bResult = DeviceIoControl(
                hVFCtrlDevice,
                (DWORD)IOCTL_INMAGE_GET_GLOBAL_TIMESTAMP,
                NULL,
                NULL,
                &GlobalTSSN,
                sizeof(GLOBAL_TIMESTAMP),
                &dwReturn,
                NULL
                );
    if (bResult) {
        if (dwReturn >= sizeof(GLOBAL_TIMESTAMP))
            printf(" GetGlobalTSSN : Global Timestamp : %#I64x and Global Sequence Number: %#I64x \n", GlobalTSSN.TimeInHundNanoSecondsFromJan1601, 
            GlobalTSSN.ullSequenceNumber);
                    
    } else {
        printf(" GetGlobalTSSN failed with error : %x", GetLastError());

    }
    
}

void
GetAdditionalStats_v2(HANDLE hVFCtrlDevice, PADDITIONAL_STATS_INFO pAdditionalStatsInfo)
{
	DWORD dwReturn;
	BOOL bResult;
	VOLUME_STATS_ADDITIONAL_INFO VolumeAdditionalStats;

	bResult = DeviceIoControl(
		hVFCtrlDevice,
		(DWORD)IOCTL_INMAGE_GET_ADDITIONAL_DEVICE_STATS,
		&pAdditionalStatsInfo->VolumeGUID,
		sizeof(WCHAR)* GUID_SIZE_IN_CHARS,
		&VolumeAdditionalStats,
		sizeof(VOLUME_STATS_ADDITIONAL_INFO),
		&dwReturn,
		NULL
		);
	if (bResult) {
		if (dwReturn >= sizeof(VOLUME_STATS_ADDITIONAL_INFO))
			printf(" CurrentTimeStamp :  %#I64x  \nOldestChange Timestamp :  %#I64x  \nPending changes with Driver:  %#I64d \n",
			VolumeAdditionalStats.ullDriverCurrentTimeStamp,
			VolumeAdditionalStats.ullOldestChangeTimeStamp,
			VolumeAdditionalStats.ullTotalChangesPending);
		SV_ULONGLONG currentRPO = 0;
		if (VolumeAdditionalStats.ullTotalChangesPending > 0)
		{
			currentRPO = VolumeAdditionalStats.ullDriverCurrentTimeStamp - VolumeAdditionalStats.ullOldestChangeTimeStamp;
		}
		SV_ULONGLONG rpoInterval = 10 * 10 * 10 * 10 * 10 * 10 * 10 * 60;
		double rpo = (1.0 * currentRPO) / rpoInterval;
		printf("\nRPO in Mins: %f", rpo);
	}
	else {
		printf(" GetAdditionalStats failed with error : %x", GetLastError());

	}

}

// DBF end
void 
GetAdditionalStats(HANDLE hVFCtrlDevice, PADDITIONAL_STATS_INFO pAdditionalStatsInfo)
{
    DWORD dwReturn;
    BOOL bResult;
    VOLUME_STATS_ADDITIONAL_INFO VolumeAdditionalStats;

    bResult = DeviceIoControl(
                hVFCtrlDevice,
                (DWORD)IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS,
                &pAdditionalStatsInfo->VolumeGUID,
                sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                &VolumeAdditionalStats,
                sizeof(VOLUME_STATS_ADDITIONAL_INFO),
                &dwReturn,
                NULL
                );
    if (bResult) {
        if (dwReturn >= sizeof(VOLUME_STATS_ADDITIONAL_INFO))
            printf(" CurrentTimeStamp :  %#I64x  \nOldestChange Timestamp :  %#I64x  \nPending changes with Driver:  %#I64d \n", 
                                    VolumeAdditionalStats.ullDriverCurrentTimeStamp, 
                                    VolumeAdditionalStats.ullOldestChangeTimeStamp,
                                    VolumeAdditionalStats.ullTotalChangesPending);
            SV_ULONGLONG currentRPO = 0;
            if(VolumeAdditionalStats.ullTotalChangesPending > 0)
            {
                currentRPO =  VolumeAdditionalStats.ullDriverCurrentTimeStamp - VolumeAdditionalStats.ullOldestChangeTimeStamp;
            }
            SV_ULONGLONG rpoInterval = 10 * 10 * 10 * 10 * 10 * 10 * 10 * 60;
            double rpo = (1.0 * currentRPO)/ rpoInterval;
            printf("\nRPO in Mins: %f", rpo);
    } else {
        printf(" GetAdditionalStats failed with error : %x", GetLastError());

    }
    
}

void
CommitDBTrans(
    HANDLE                  hVFCtrlDevice,
    PCOMMIT_DB_TRANS_DATA   pCommitDBTran
    )
{
    COMMIT_TRANSACTION  CommitTrans;
    ULONG   ulError;
    BOOL    bResult;
    bool    bResetResync = false;
    DWORD   dwReturn;

    CommitTrans.ulFlags = 0;
	inm_memcpy_s(CommitTrans.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pCommitDBTran->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
    ulError = GetPendingTransactionID(&PendingTranListHead, pCommitDBTran->VolumeGUID, CommitTrans.ulTransactionID.QuadPart, bResetResync);
    if (bResetResync && pCommitDBTran->bResetResync) {
        CommitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
    }

    if (ERROR_SUCCESS == ulError) {
        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                        &CommitTrans,
                        sizeof(COMMIT_TRANSACTION),
                        NULL,
                        0,
                        &dwReturn,  
                        NULL        // Overlapped
                        );
        if (bResult) {
            wprintf(L"Commiting pending transaction on Volume %.*s Succeeded\n", GUID_SIZE_IN_CHARS, CommitTrans.VolumeGUID);
        } else {
            wprintf(L"Commiting pending transaction on Volume %.*s Failed: %d\n", GUID_SIZE_IN_CHARS, CommitTrans.VolumeGUID,
                GetLastError());
        }
    } else {
        wprintf(L"Failed to find pending transaction on Volume %.*s Failed\n", GUID_SIZE_IN_CHARS, CommitTrans.VolumeGUID);
    }

    return;
}

void
SetResyncRequired(
    HANDLE                      hVFCtrlDevice,
    PSET_RESYNC_REQUIRED_DATA   pSetResyncRequiredData
    )
{
    SET_RESYNC_REQUIRED SetResyncRequired;
    BOOL    bResult;
    DWORD   dwReturn;

	inm_memcpy_s(SetResyncRequired.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pSetResyncRequiredData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
    SetResyncRequired.ulOutOfSyncErrorCode = pSetResyncRequiredData->ulErrorCode;
    SetResyncRequired.ulOutOfSyncErrorStatus = pSetResyncRequiredData->ulErrorStatus;

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_RESYNC_REQUIRED,
                    &SetResyncRequired,
                    sizeof(SET_RESYNC_REQUIRED),
                    NULL,
                    0,
                    &dwReturn,  
                    NULL        // Overlapped
                    );
    if (bResult) {
        wprintf(L"Set resync required on Volume %.*s Succeeded\n", GUID_SIZE_IN_CHARS, SetResyncRequired.VolumeGUID);
    } else {
        wprintf(L"Set resync required on Volume %.*s Failed: %d\n", GUID_SIZE_IN_CHARS, SetResyncRequired.VolumeGUID,
            GetLastError());
    }

    return;
}

ULONG
SetVolumeFlags(
    HANDLE              hVFCtrlDevice,
    PVOLUME_FLAGS_DATA  pVolumeFlagsData
    )
{
    VOLUME_FLAGS_INPUT  VolumeFlagsInput;
    VOLUME_FLAGS_OUTPUT VolumeFlagsOutput;
    BOOL    bResult;
    DWORD   dwReturn;

	inm_memcpy_s(VolumeFlagsInput.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pVolumeFlagsData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
    VolumeFlagsInput.ulVolumeFlags = pVolumeFlagsData->ulVolumeFlags;
    VolumeFlagsInput.eOperation = pVolumeFlagsData->eBitOperation;
    if (VolumeFlagsInput.ulVolumeFlags & VOLUME_FLAG_DATA_LOG_DIRECTORY) {
        VolumeFlagsInput.DataFile.usLength = pVolumeFlagsData->DataFile.usLength;
        if (VolumeFlagsInput.DataFile.usLength > 0) {
            memmove(VolumeFlagsInput.DataFile.FileName, pVolumeFlagsData->DataFile.FileName,
                    VolumeFlagsInput.DataFile.usLength);
            VolumeFlagsInput.DataFile.FileName[VolumeFlagsInput.DataFile.usLength/sizeof(WCHAR)] = L'\0';
        }
    }


    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_VOLUME_FLAGS,
                    &VolumeFlagsInput,
                    sizeof(VOLUME_FLAGS_INPUT),
                    &VolumeFlagsOutput,
                    sizeof(VOLUME_FLAGS_OUTPUT),
                    &dwReturn,  
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetVolumeFlags: Set flags on Volume %s Succeeded\n"), pVolumeFlagsData->MountPoint);
    } else {
        _tprintf(_T("SetVolumeFlags: Set flags on Volume %s Failed: %d\n"),  pVolumeFlagsData->MountPoint, GetLastError());
        return ERROR_GEN_FAILURE;
    }

    if (sizeof(VOLUME_FLAGS_OUTPUT) != dwReturn) {
        _tprintf(_T("SetVolumeFlags: dwReturn(%#x) did not match VOLUME_FLAGS_OUTPUT size(%#x)\n"), dwReturn, sizeof(VOLUME_FLAGS_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (VolumeFlagsOutput.ulVolumeFlags) {
        _tprintf(_T("Following flags are set for volume %s \n"), pVolumeFlagsData->MountPoint);
    } else {
        _tprintf(_T("No flags are set for this volume %s \n"), pVolumeFlagsData->MountPoint);
        return ERROR_SUCCESS;
    }
    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
        _tprintf(_T("\tVOLUME_FLAG_IGNORE_PAGEFILE_WRITES\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DISABLE_BITMAP_READ)
        _tprintf(_T("\tVOLUME_FLAG_DISABLE_BITMAP_READ\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE)
        _tprintf(_T("\tVOLUME_FLAG_DISABLE_BITMAP_WRITE\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_READ_ONLY)
        _tprintf(_T("\tVOLUME_FLAG_READ_ONLY\n"));

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE) {
        _tprintf(_T("\tVOLUME_FLAG_DISABLE_DATA_CAPTURE\n"));
    }

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_FREE_DATA_BUFFERS) {
        _tprintf(_T("\tVOLUME_FLAG_FREE_DATA_BUFFERS\n"));
    }

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DISABLE_DATA_FILES) {
        _tprintf(_T("\tVOLUME_FLAG_DISABLE_DATA_FILES\n"));
    }

    if (VolumeFlagsOutput.ulVolumeFlags & VOLUME_FLAG_DATA_LOG_DIRECTORY) {
        _tprintf(_T("\tData Log Directory changed\n"));
    } else if (VolumeFlagsInput.ulVolumeFlags & VOLUME_FLAG_DATA_LOG_DIRECTORY) {
        _tprintf(_T("\tData Log Directory failed\n"));
    }

    return ERROR_SUCCESS;
}

ULONG
SetDriverFlags(
    HANDLE              hVFCtrlDevice,
    PDRIVER_FLAGS_DATA  pDriverFlagsData
    )
{
    DRIVER_FLAGS_INPUT  DriverFlagsInput;
    DRIVER_FLAGS_OUTPUT DriverFlagsOutput;
    BOOL    bResult;
    DWORD   dwReturn;

    DriverFlagsInput.ulFlags = pDriverFlagsData->ulDriverFlags;
    DriverFlagsInput.eOperation = pDriverFlagsData->eBitOperation;

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_DRIVER_FLAGS,
                    &DriverFlagsInput,
                    sizeof(DRIVER_FLAGS_INPUT),
                    &DriverFlagsOutput,
                    sizeof(DRIVER_FLAGS_OUTPUT),
                    &dwReturn,  
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetDriverFlags: Set driver flags Succeeded\n"));
    } else {
        _tprintf(_T("SetDriverFlags: Set driver flags failed: %d\n"),  GetLastError());
        return ERROR_GEN_FAILURE;
    }

    if (sizeof(DRIVER_FLAGS_OUTPUT) != dwReturn) {
        _tprintf(_T("SetDriverFlags: dwReturn(%#x) did not match DRIVER_FLAGS_OUTPUT size(%#x)\n"), dwReturn, sizeof(DRIVER_FLAGS_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (DriverFlagsOutput.ulFlags) {
        _tprintf(_T("Following driver flags are set\n"));
    } else {
        _tprintf(_T("No driver flags are set\n"));
        return ERROR_SUCCESS;
    }

    if (DriverFlagsOutput.ulFlags & DRIVER_FLAG_DISABLE_DATA_FILES) {
        _tprintf(_T("\tDRIVER_FLAG_DISABLE_DATA_FILES\n"));
    }

    if (DriverFlagsOutput.ulFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES) {
        _tprintf(_T("\tDRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES\n"));
    }

    return ERROR_SUCCESS;
}

#ifdef _CUSTOM_COMMAND_CODE_
ULONG
SendCustomCommand(
    HANDLE hVFCtrlDevice,
    PCUSTOM_COMMAND_DATA pCustomCommandData
    )
{
    DRIVER_CUSTOM_COMMAND  DriverInput = {0};
    BOOL    bResult;
    DWORD   dwReturn;

    DriverInput.ulParam1 = pCustomCommandData->ulParam1;
    DriverInput.ulParam2 = pCustomCommandData->ulParam2;

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_CUSTOM_COMMAND,
                    &DriverInput,
                    sizeof(DRIVER_CUSTOM_COMMAND),
                    NULL,
                    0,
                    &dwReturn,  
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetCustomCommand: Succeeded\n"));
    } else {
        _tprintf(_T("SetCustomCommand: failed with error %d\n"),  GetLastError());
        return ERROR_GEN_FAILURE;
    }

    return ERROR_SUCCESS;
}
#endif // _CUSTOM_COMMAND_CODE_

ULONG
SetDriverConfig(
    HANDLE              hVFCtrlDevice,
    PDRIVER_CONFIG_DATA pDriverConfig
    )
{
    DRIVER_CONFIG  DriverInput = {0}, DriverOutput = {0};
    BOOL    bResult;
    DWORD   dwReturn;

    DriverInput.ulFlags1 = pDriverConfig->ulFlags1;
    DriverInput.ulFlags2 = pDriverConfig->ulFlags2;

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING) {
        DriverInput.ullwmServiceRunning = pDriverConfig->ullwmServiceRunning;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED) {
        DriverInput.ulhwmServiceNotStarted = pDriverConfig->ulhwmServiceNotStarted;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING) {
        DriverInput.ulhwmServiceRunning = pDriverConfig->ulhwmServiceRunning;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN) {
        DriverInput.ulhwmServiceShutdown = pDriverConfig->ulhwmServiceShutdown;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM) {
        DriverInput.ulChangesToPergeOnhwm = pDriverConfig->ulChangesToPergeOnhwm;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG) {
        DriverInput.ulSecsBetweenMemAllocFailureEvents = pDriverConfig->ulSecsBetweenMemAllocFailureEvents;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER) {
        DriverInput.ulSecsMaxWaitTimeOnFailOver = pDriverConfig->ulSecsMaxWaitTimeForFailOver;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS) {
        DriverInput.ulMaxLockedDataBlocks = pDriverConfig->ulMaxLockedDataBlocks;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL) {
        DriverInput.ulMaxNonPagedPoolInMB = pDriverConfig->ulMaxNonPagedPoolInMB;
    }
    
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION) {
        DriverInput.ulMaxWaitTimeForIrpCompletionInMinutes = pDriverConfig->ulMaxWaitTimeForIrpCompletionInMinutes;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) {
        DriverInput.bDisableVolumeFilteringForNewClusterVolumes = pDriverConfig->bDisableVolumeFilteringForNewClusterVolumes;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) {
        DriverInput.bDisableVolumeFilteringForNewVolumes = pDriverConfig->bDisableVolumeFilteringForNewVolumes;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) {
        DriverInput.bEnableDataFilteringForNewVolumes = pDriverConfig->bEnableDataFilteringForNewVolumes;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE) {
        DriverInput.bEnableDataCapture = pDriverConfig->bEnableDataCapture;
    }
  
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT) {
        DriverInput.ulMaxDataAllocLimitInPercentage = pDriverConfig->ulMaxDataAllocLimitInPercentage;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT) {
        DriverInput.ulAvailableDataBlockCountInPercentage = pDriverConfig->ulAvailableDataBlockCountInPercentage;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE) {
        DriverInput.ulMaxCoalescedMetaDataChangeSize = pDriverConfig->ulMaxCoalescedMetaDataChangeSize;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL) {
        DriverInput.ulValidationLevel = pDriverConfig->ulValidationLevel;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) {
        DriverInput.ulDataPoolSize = pDriverConfig->ulDataPoolSize;        
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME) {
        DriverInput.ulMaxDataSizePerVolume = pDriverConfig->ulMaxDataSizePerVolume;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE) {
        DriverInput.ulDaBThresholdPerVolumeForFileWrite = pDriverConfig->ulDaBThresholdPerVolumeForFileWrite;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) {
        DriverInput.ulDaBFreeThresholdForFileWrite = pDriverConfig->ulDaBFreeThresholdForFileWrite;
    }
    
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) {
        DriverInput.ulMaxDataSizePerNonDataModeDirtyBlock = pDriverConfig->ulMaxDataSizePerNonDataModeDirtyBlock;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) {
        DriverInput.ulMaxDataSizePerDataModeDirtyBlock = pDriverConfig->ulMaxDataSizePerDataModeDirtyBlock;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) {
        DriverInput.DataFile.usLength = pDriverConfig->DataFile.usLength;
        if (DriverInput.DataFile.usLength > 0) {
            memmove(DriverInput.DataFile.FileName, pDriverConfig->DataFile.FileName,
                    DriverInput.DataFile.usLength);
            DriverInput.DataFile.FileName[DriverInput.DataFile.usLength/sizeof(WCHAR)] = L'\0';
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH) {
        DriverInput.bEnableFSFlushPreShutdown = pDriverConfig->bEnableFSFlushPreShutdown;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_DRIVER_CONFIGURATION,
                    &DriverInput,
                    sizeof(DRIVER_CONFIG),
                    &DriverOutput,
                    sizeof(DRIVER_CONFIG),
                    &dwReturn,  
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetDriverConfig: Set driver config Succeeded\n"));
    } else {
        _tprintf(_T("SetDriverConfig: Set driver config failed: %d\n"),  GetLastError());
        return ERROR_GEN_FAILURE;
    }

    if (sizeof(DRIVER_CONFIG) != dwReturn) {
        _tprintf(_T("SetDriverConfig: dwReturn(%#x) did not match DRIVER_CONFIG size(%#x)\n"), dwReturn, sizeof(DRIVER_FLAGS_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (DriverOutput.ulFlags1 || DriverOutput.ulFlags2) {
        _tprintf(_T("Following driver data is returned\n"));
    } else {
        _tprintf(_T("No driver data is returned\n"));
        return ERROR_SUCCESS;
    }

	if (DriverOutput.ulError) {
        _tprintf(_T("\n\tError occured \n\n"));
	}

    const unsigned long ulBufferSize = 80;
    TCHAR   szUlongInString[ulBufferSize];
    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED) {
            _tprintf(_T("\n\t Error occured during this opertion \n\n"));
        } else {
        	ConvertSizeToString(DriverOutput.ulhwmServiceNotStarted, szUlongInString, ulBufferSize);
            _tprintf(_T("\tHigh water mark for service NOT STARTED = %s\n"), szUlongInString);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            ConvertSizeToString(DriverOutput.ulhwmServiceRunning, szUlongInString, ulBufferSize);
            _tprintf(_T("\tHigh water mark for service RUNNING = %s\n"), szUlongInString);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            ConvertSizeToString(DriverOutput.ulhwmServiceShutdown, szUlongInString, ulBufferSize);
            _tprintf(_T("\tHigh water mark for service SHUTDOWN = %s\n"), szUlongInString);        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            ConvertSizeToString(DriverOutput.ullwmServiceRunning, szUlongInString, ulBufferSize);
            _tprintf(_T("\tLow water mark for service RUNNING = %s\n"), szUlongInString);        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	ConvertSizeToString(DriverOutput.ulChangesToPergeOnhwm, szUlongInString, ulBufferSize);
            _tprintf(_T("\tChanges to purge on High water mark reach = %s\n"), szUlongInString);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	_tprintf(_T("\tSecs between driver alloc failure events = %lu\n"), DriverOutput.ulSecsBetweenMemAllocFailureEvents);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	_tprintf(_T("\tMax Secs wait time to flush changes on fail over = %lu\n"), DriverOutput.ulSecsMaxWaitTimeOnFailOver);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS) {            
            if (DriverInput.ulMaxLockedDataBlocks > DriverOutput.ulDataPoolSize) {
                _tprintf(_T("\n\tError Max Lock Data Block %d can not be greater than Data Page Pool %d\n\n"), 
                        DriverInput.ulMaxLockedDataBlocks, 
                        DriverOutput.ulDataPoolSize);                
            } else {
                _tprintf(_T("\n\tError occured during this opertion \n\n"));            	
            }
        } else {
        	_tprintf(_T("\tMax locked data blocks for writes at DISPATCH_LEVEL = %lu\n"), DriverOutput.ulMaxLockedDataBlocks);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	_tprintf(_T("\tMax non paged pool limit = %dm\n"), DriverOutput.ulMaxNonPagedPoolInMB);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            _tprintf(_T("\tMax wait time for Irp completion = %d mins\n"), DriverOutput.ulMaxWaitTimeForIrpCompletionInMinutes);        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            if (DriverOutput.bDisableVolumeFilteringForNewClusterVolumes) {
                _tprintf(_T("\tDefault Volume Filtering for New Cluster Volumes is Disabled\n"));
            } else {
                _tprintf(_T("\tDefault Volume Filtering for New Cluster Volumes is Enabled\n"));
            }
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) {
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            if (DriverOutput.bDisableVolumeFilteringForNewVolumes) {
                _tprintf(_T("\tDefault Volume Filtering for New Volumes is Disabled\n"));
            } else {
                _tprintf(_T("\tDefault Volume Filtering for New Volumes is Enabled\n"));
            }        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) {        
        if (DriverOutput.ulError &  DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	if (DriverOutput.bEnableDataFilteringForNewVolumes) {
                _tprintf(_T("\tDefault Volume Data Filtering for New Volumes is Enabled\n"));
            } else {
                _tprintf(_T("\tDefault Volume Data Filtering for New Volumes is Disabled\n"));
            }
        }
    }
    
    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            if (DriverOutput.bEnableDataCapture) {
                _tprintf(_T("\tDefault Data Filtering is Enabled\n"));
            } else {
                _tprintf(_T("\tDefault Data Filtering is Disabled\n"));
            }        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH) {
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        }
        else {
            if (DriverOutput.bEnableFSFlushPreShutdown) {
                _tprintf(_T("\tDefault FS Filtering during pre shutdown is Enabled\n"));
            }
            else {
                _tprintf(_T("\tDefault FS Filtering during pre shutdown is Disabled\n"));
            }
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            _tprintf(_T("\tLimit on allocated paged pool for a volume below which it can switch to Data Capture mode = %d (In Percentage)\n"), 
                     DriverOutput.ulMaxDataAllocLimitInPercentage);        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	_tprintf(_T("\tLimit on total free and locked data blocks available above which a volume can switch to Data Capture Mode= %d (In Percentage)\n"),
                 DriverOutput.ulAvailableDataBlockCountInPercentage);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            _tprintf(_T("\tMax Coalesced Meta-Data Change Size = %d\n"), DriverOutput.ulMaxCoalescedMetaDataChangeSize);        	
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	_tprintf(_T("\tValidation Level = %d (0 = No Checks; 1 = Add to Event Log; 2 or higher = Cause BugCheck/BSOD + Add to Event Log)\n"), DriverOutput.ulValidationLevel);
        }
    }
    
    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_DATA_POOL_SIZE) {            
            if (DriverOutput.ulMaxLockedDataBlocks > DriverInput.ulDataPoolSize) {
                _tprintf(_T("\n\tError Data Page Pool %d can not be lesser than Max Lock Data Block %d. Set Max Lock Data Block less than %d and then try again\n\n"), 
                        DriverInput.ulDataPoolSize, DriverOutput.ulMaxLockedDataBlocks, DriverInput.ulDataPoolSize);                
            } else {
            	_tprintf(_T("\n\tError occured during this opertion \n\n"));
            }
        } else {
        	_tprintf(_T("\tData Pool Size = %dMB\n"), DriverOutput.ulDataPoolSize);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME) {            
            ULONG ulReqMaxDataSizePerVolume = (DriverOutput.ulDataPoolSize * MAX_DATA_SIZE_PER_VOL_PERC) / 100;
            if ((DriverInput.ulMaxDataSizePerVolume / 1024) > DriverOutput.ulDataPoolSize) {
                _tprintf(_T("\n\tError Max Data Size Per Volume %dKB is greater than DataPoolSize %dKB. Suggested %d\n\n"), 
                        DriverInput.ulMaxDataSizePerVolume, (DriverOutput.ulDataPoolSize * 1024),
                        ulReqMaxDataSizePerVolume * 1024);
            } else {
            	_tprintf(_T("\n\tError occured during this opertion \n\n"));
            }      
        } else if (DriverInput.ulFlags1 & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME) {
            if (DriverInput.ulMaxDataSizePerVolume != DriverOutput.ulMaxDataSizePerVolume) {
                _tprintf(_T("\n\tNote MaxDataSizePerVolume is reset to %dKB instead of given value %dKB\n\n"), 
                            DriverOutput.ulMaxDataSizePerVolume, DriverInput.ulMaxDataSizePerVolume);
            }
        } else {
        	_tprintf(_T("\tMax Data Pool Size Per Volume = %d KB\n"), DriverOutput.ulMaxDataSizePerVolume);
        }
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
        	_tprintf(_T("\tThreshold Per Volume For File Write = %d (In Percentage) \n"), DriverOutput.ulDaBThresholdPerVolumeForFileWrite);
        }
    }
    
    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) {        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            _tprintf(_T("\tFree Threshold Per Volume For File Write = %d (In Percentage)\n"), DriverOutput.ulDaBFreeThresholdForFileWrite);        	
        }
    }
    
    if (DriverOutput.ulFlags1 &  DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) {		        
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) {
			if(DriverInput.ulMaxDataSizePerNonDataModeDirtyBlock < DriverOutput.ulMaxDataSizePerDataModeDirtyBlock) {
                _tprintf(_T("\n\tError occured as MaxDataPerNonDataModeDirtyBlock %u should not be lesser than MaxDataPerDataModeDirtyBlock %u \n\n"), 
					    DriverInput.ulMaxDataSizePerNonDataModeDirtyBlock , DriverOutput.ulMaxDataSizePerDataModeDirtyBlock);                 
			} else {
                _tprintf(_T("\n\tError occured during this operation \n\n"));
			}
		} else {
			_tprintf(_T("\tMax Data Size Per Non Data Mode Dirty Block = %d MB\n"), DriverOutput.ulMaxDataSizePerNonDataModeDirtyBlock);
		}
    }

    if (DriverOutput.ulFlags1 &  DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) {		
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) {
			if(DriverInput.ulMaxDataSizePerDataModeDirtyBlock > DriverOutput.ulDataPoolSize / 8) {
                _tprintf(_T("\n\tError occured as MaxDataPerDataModeDirtyBlock %u cannot be greater than %u \n\n"), 
					    DriverInput.ulMaxDataSizePerDataModeDirtyBlock , DriverOutput.ulDataPoolSize / 8);                 
			} else {
                _tprintf(_T("\n\tError occured during this operation \n\n"));
			}
		} else {
            _tprintf(_T("\tMax Data Size Per Data Mode Dirty Block = %d MB\n"), DriverOutput.ulMaxDataSizePerDataModeDirtyBlock);
		}
    }

    if (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) {
        if (DriverOutput.ulError & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) {
            _tprintf(_T("\n\tError occured during this opertion \n\n"));
        } else {
            if (DriverOutput.DataFile.usLength == 0) {
                _tprintf(_T("\n\tError occured during this opertion as Length returned of DataLogDirectory string is zero \n\n"));
                _tprintf(_T("\tData Log Directory Path = %s\n"), DriverOutput.DataFile.FileName);
	        } else if (DriverOutput.DataFile.usLength >= UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY) {
                _tprintf(_T("\n\tError occured during this opertion as Length returned of DataLogDirectory string is %d greater than %d \n\n"),
                       DriverOutput.DataFile.usLength, UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY);
	        } else {
                _tprintf(_T("\tData Log Directory Path = %s\n"), DriverOutput.DataFile.FileName);
            }
        }
    }
    
    if ((DriverOutput.ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) && 
                (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL)) {
        if (DriverOutput.ulReqNonPagePool > DriverOutput.ulMaxNonPagedPoolInMB) {
            _tprintf(_T("\n\n\tWarning Max non paged pool limit = %dm, Required non paged pool limit = %dm\n\n"), 
                    DriverOutput.ulMaxNonPagedPoolInMB, 
                    DriverOutput.ulReqNonPagePool);
        }
    }

    if ((DriverOutput.ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) && 
                (DriverOutput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS)) {
        if (DriverOutput.ulMaxLockedDataBlocks > DriverOutput.ulDataPoolSize) {
            _tprintf(_T("\n\n\tWarning Max Lock Data Block %d Should not be greater than Data Page Pool %d\n\n"), 
                    DriverOutput.ulMaxLockedDataBlocks, 
                    DriverOutput.ulDataPoolSize);
        }
    }

    if ((DriverOutput.ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) && 
                (DriverOutput.ulFlags1 & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME)) {
        ULONG ulReqMaxDataSizePerVolume = (DriverOutput.ulDataPoolSize * MAX_DATA_SIZE_PER_VOL_PERC) / 100;
        if ((DriverOutput.ulMaxDataSizePerVolume / 1024) > DriverOutput.ulDataPoolSize) {
            _tprintf(_T("\n\n\tWarning Max Data Size Per Volume %dKB is greater than DataPoolSize %dKB. Suggested %d\n\n"), 
                    DriverOutput.ulMaxDataSizePerVolume, (DriverOutput.ulDataPoolSize * 1024),
                    ulReqMaxDataSizePerVolume * 1024);
        }
    }
    
    return ERROR_SUCCESS;
}

ULONG
TagVolume(
    HANDLE              hVFCtrlDevice,
    PTAG_VOLUME_INPUT_DATA pTagVolumeInputData
    )
{
    BOOL    bResult;
    DWORD   dwReturn;

    bResult = DeviceIoControl(
                    hVFCtrlDevice, 
                    (DWORD)IOCTL_INMAGE_TAG_VOLUME, 
                    pTagVolumeInputData->pBuffer, 
                    pTagVolumeInputData->ulLengthOfBuffer,
                    NULL, 
                    (DWORD)0, 
                    &dwReturn, 
                    NULL
                    );

    if(bResult)
    {
        _tprintf(_T("TagVolume: Successful\n"));
    }
    else
    {
        _tprintf(_T("TagVolume: Failed: %d\n"), GetLastError());
    }
    return 0;
}


ULONG
TagDisk(
HANDLE              hVFCtrlDevice,
PTAG_VOLUME_INPUT_DATA pTagVolumeInputData
)
{
	BOOL    bResult;
	DWORD   dwReturn;

	bResult = DeviceIoControl(
		hVFCtrlDevice,
		(DWORD)IOCTL_INMAGE_TAG_DISK,
		pTagVolumeInputData->pBuffer,
		pTagVolumeInputData->ulLengthOfBuffer,
		NULL,
		(DWORD)0,
		&dwReturn,
		NULL
		);

	if (bResult)
	{
		_tprintf(_T("TagDisk: App Consistency Tag Successful\n"));
	}
	else
	{
		_tprintf(_T("TagDisk: App Consistency Tag Failed: %d\n"), GetLastError());
	}
	return 0;
}
void
PrintTagStatusOutputData(PTAG_VOLUME_STATUS_OUTPUT_DATA StatusOutputData)
{
    printf("\nNumber of Volumes: %d\n", StatusOutputData->ulNoOfVolumes);
    if (StatusOutputData->Status < ecTagStatusMaxEnum)
        printf("Tag status: %s\n", TagStatusStringArray[StatusOutputData->Status]);
    else
        printf("Invalid Tag value: %d\n", StatusOutputData->Status);

    for(ULONG i = 0; i < StatusOutputData->ulNoOfVolumes ; i++) {
        if (StatusOutputData->VolumeStatus[i] < ecTagStatusMaxEnum) {
            printf("Status of the tag for %d volume is  %s\n", i, TagStatusStringArray[StatusOutputData->VolumeStatus[i]]);    
        } else {
            printf("Invalid Status value %d for volume %d\n", StatusOutputData->VolumeStatus[i], i);
        }
    }
}
ULONG
TagSynchronousToVolume(
    HANDLE              hVFCtrlDevice,
    PTAG_VOLUME_INPUT_DATA pTagVolumeInputData
    )
{
    BOOL    bResult;
    DWORD   dwReturn = 0,RetValue=0;;
    OVERLAPPED OverLapIO = { 0, 0, 0, 0, NULL /* No event. */ };
    OverLapIO.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    char ch='n';

    ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(pTagVolumeInputData->ulNumberOfVolumes);
    PTAG_VOLUME_STATUS_OUTPUT_DATA TagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);
    PTAG_VOLUME_STATUS_INPUT_DATA StatusInputData = (PTAG_VOLUME_STATUS_INPUT_DATA)malloc(sizeof(TAG_VOLUME_STATUS_INPUT_DATA));
    PTAG_VOLUME_STATUS_OUTPUT_DATA StatusOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);

    if ((NULL == StatusInputData)||(NULL == StatusOutputData) || (NULL == TagOutputData)) {
        return 0;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice, 
                    (DWORD)IOCTL_INMAGE_SYNC_TAG_VOLUME, 
                    pTagVolumeInputData->pBuffer, 
                    pTagVolumeInputData->ulLengthOfBuffer,
                    (PVOID)TagOutputData, 
                    ulOutputLength, 
                    &dwReturn, 
                    &OverLapIO
                    );

    if (0 == bResult) {
        ULONG   ulCount = pTagVolumeInputData->ulCount;
        bool    bPrompt = pTagVolumeInputData->bPrompt;
        bool    bCompleted = false;

        if (ERROR_IO_PENDING != GetLastError()) {
            _tprintf(_T("IOCTL_INMAGE_SYNC_TAG_VOLUME: DeviceIoControl failed with error = %d, BytesReturn = %d\n"), 
                        GetLastError(), dwReturn);
            if (dwReturn >= ulOutputLength) {
                PrintTagStatusOutputData(TagOutputData);
            }
        } else {
            do {
                RetValue = WaitForSingleObject(OverLapIO.hEvent, pTagVolumeInputData->ulTimeOut);
                
                if (WAIT_TIMEOUT == RetValue) {
					inm_memcpy_s(&StatusInputData->TagGUID, sizeof(GUID), &pTagVolumeInputData->TagGUID, sizeof(GUID));
                    bResult = DeviceIoControl(
                            hVFCtrlDevice, 
                            (DWORD)IOCTL_INMAGE_GET_TAG_VOLUME_STATUS, 
                            StatusInputData, 
                            sizeof(TAG_VOLUME_STATUS_INPUT_DATA),
                            (PVOID)StatusOutputData, 
                            ulOutputLength, 
                            &dwReturn, 
                            NULL
                        );
                    if (bResult && (dwReturn >= ulOutputLength)) {
                        PrintTagStatusOutputData(StatusOutputData);
                    }

                    if (ulCount) {
                        if (bPrompt) {
                            printf("Wait again for %d secs for tags to commit (y/n) : ", pTagVolumeInputData->ulTimeOut/1000);
                            fflush(stdout);
                            fflush(stdin);
                            char ch = getchar();
                            if ((ch != 'y') && (ch != 'Y')) {
                                break;
                            }
                        }
                        ulCount--;
                    }
                } else if (RetValue == WAIT_OBJECT_0) {
                    if (GetOverlappedResult(hVFCtrlDevice, &OverLapIO, &dwReturn, FALSE)) {
                        if (dwReturn >= ulOutputLength) {
                            PrintTagStatusOutputData(TagOutputData);
                        }
                    }
                    bCompleted = true;
                    break;
                }
            } while (ulCount);

            if (!bCompleted && (0 != CancelIo(hVFCtrlDevice))) {
                // Get overlapped result to get IO Status
                printf("Waiting for Tag IOCTL to be cancelled\n");
                RetValue = WaitForSingleObject(OverLapIO.hEvent, INFINITE);
                if (GetOverlappedResult(hVFCtrlDevice, &OverLapIO, &dwReturn, FALSE)) {
                    if (dwReturn >= ulOutputLength) {
                        PrintTagStatusOutputData(TagOutputData);
                    }
                } else {
                    printf("GetOverlappedResult returned error %d\n", GetLastError());
                }
            }
        }
    } else {
        _tprintf(_T("IOCTL_INMAGE_SYNC_TAG_VOLUME: Returned success, BytesReturn = %d\n"), dwReturn);
        if (dwReturn >= ulOutputLength) {
            PrintTagStatusOutputData(TagOutputData);
        }
    }

    _tprintf(_T("TagVolume: completed: \n") );
    return 0;
}


ULONG
SetDebugInfo(
    HANDLE              hVFCtrlDevice,
    PDEBUG_INFO_DATA    pDebugInfoData
    )
{
    DEBUG_INFO  DebugInfoOutput;
    BOOL    bResult;
    DWORD   dwReturn;

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_DEBUG_INFO,
                    &pDebugInfoData->DebugInfo,
                    sizeof(DEBUG_INFO),
                    &DebugInfoOutput,
                    sizeof(DEBUG_INFO),
                    &dwReturn,  
                    NULL        // Overlapped
                    );
    if (bResult) {
        _tprintf(_T("SetDebugInfo: Set debug info Succeeded\n"));
    } else {
        _tprintf(_T("SetDebugInfo: Set debug info Failed: %d\n"), GetLastError());
        return ERROR_GEN_FAILURE;
    }

    if (sizeof(DEBUG_INFO) != dwReturn) {
        _tprintf(_T("SetDebugInfo: dwReturn(%#x) did not match DEBUG_INFO size(%#x)\n"), dwReturn, sizeof(DEBUG_INFO));
        return ERROR_GEN_FAILURE;
    }

    // Print the current debug info.
    _tprintf(_T("DebugLevelForAll = %s\n"), UlongToDebugLevelString(DebugInfoOutput.ulDebugLevelForAll));
    _tprintf(_T("DebugLevelForModules = %s\n"), UlongToDebugLevelString(DebugInfoOutput.ulDebugLevelForMod));

    if (0 != DebugInfoOutput.ulDebugModules) {
        _tprintf(_T("Following are debug modules\n"));
    } else {
        _tprintf(_T("No Debug Modules are set\n"));
    }

    for (int i = 0; NULL != ModuleStringMapping[i].pString; i++) {
        if (ModuleStringMapping[i].ulValue == (DebugInfoOutput.ulDebugModules & ModuleStringMapping[i].ulValue)) {
            _tprintf(_T("\t\t%s\n"), ModuleStringMapping[i].pString);
        }
    }

    return ERROR_SUCCESS;
}

bool
VerifyDifferentialsOrderV2(PTSSNSEQIDV2 pCurrentStartTimeStamp, PTSSNSEQIDV2 pPrevTSSequenceNumber)
{
    if (0 == pCurrentStartTimeStamp->ullSequenceNumber) {
        if (pCurrentStartTimeStamp->ullTimeStamp <= pPrevTSSequenceNumber->ullTimeStamp) {
            printf("Error : Current Start TimeStamp should be greater than Previous TimeStamp");
            return false;
        }
    } else {
        if (pCurrentStartTimeStamp->ullTimeStamp < pPrevTSSequenceNumber->ullTimeStamp) {
            printf("Error: Current Start TimeStamp should be greater than or equal to Previous Time Stamp\n");
            return false;
        }
    }
    //Current Start Seq Num should be greater than or equal to Previous Seq num
    if ((pPrevTSSequenceNumber->ullSequenceNumber > pCurrentStartTimeStamp->ullSequenceNumber) && ((0xFFFFFFFFF8200000) >
            (pPrevTSSequenceNumber->ullSequenceNumber))) { 
        printf("%s: Last Dirty block change time stamp(%#I64x %#I64x) is greater than Current change time stamp(%#I64x %#I64x)\n",
                __FUNCTION__, pPrevTSSequenceNumber->ullTimeStamp, pPrevTSSequenceNumber->ullSequenceNumber, 
                pCurrentStartTimeStamp->ullTimeStamp, 
                pCurrentStartTimeStamp->ullSequenceNumber);
        printf("Error: Current Start Seq Num should be greater than or equal to Previous Seq num\n");
        return false;
    }
    return true;
}

bool
VerifyDifferentialsOrder(PTSSNSEQID pCurrentStartTimeStamp, PTSSNSEQID pPrevTSSequenceNumber)
{
    if (0 == pCurrentStartTimeStamp->ulSequenceNumber) {
        if (pCurrentStartTimeStamp->ullTimeStamp <= pPrevTSSequenceNumber->ullTimeStamp) {
            printf("Error : Current Start TimeStamp should be greater than Previous TimeStamp");
            return false;
        }
    } else {
        if (pCurrentStartTimeStamp->ullTimeStamp < pPrevTSSequenceNumber->ullTimeStamp) {
            printf("Error: Current Start TimeStamp should be greater than or equal to Previous Time Stamp\n");
            return false;
        }
    }
    return true;
}

//Defined for per io time stamp changes
ULONG
GetDBTransV2(
    HANDLE              hVFCtrlDevice,
    PGET_DB_TRANS_DATA  pDBTranData
    )
{
    PUDIRTY_BLOCK_V2  pDirtyBlock;
    COMMIT_TRANSACTION  CommitTrans;
    BOOL    bResult, bWaitForEvent;
    DWORD   dwReturn;
    ULONG   ulError, ulNumDirtyBlocksReturned;
    HANDLE  hEvent = NULL;
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime;
    TSSNSEQIDV2 tempPrevTSSequenceNumberV2 = {0,0,0};
    int iTsoCount = 0;
    int iStartTsoCount = 0;
	int iTagCount = 0;
    pDirtyBlock = (PUDIRTY_BLOCK_V2)malloc(sizeof(UDIRTY_BLOCK_V2));

    if (NULL == pDirtyBlock) {
        _tprintf(_T("GetDBTransV2: Memory allocation failed for DIRTY_BLOCKS\n"));
        return ERROR_OUTOFMEMORY;
    }

	inm_memcpy_s(CommitTrans.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

    bWaitForEvent = FALSE;
    ulNumDirtyBlocksReturned = 0;

    if (pDBTranData->bWaitIfTSO) {
        if (pDBTranData->bPollForDirtyBlocks) {
            hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        } else {
            std::wstring VolumeGUID(pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS);

            if (CreateNewDBWaitEventForVolume(VolumeGUID, hEvent)) {
                VOLUME_DB_EVENT_INFO    EventInfo;

                // Created new event have to do the IOCTL to set the event.
				inm_memcpy_s(EventInfo.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
                EventInfo.hEvent = hEvent;
                bResult = DeviceIoControl(
                                hVFCtrlDevice,
                                (DWORD)IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT,
                                &EventInfo,
                                sizeof(VOLUME_DB_EVENT_INFO),
                                NULL,
                                0,
                                &dwReturn,  
                                NULL        // Overlapped
                                );
                if (bResult) {
                    _tprintf(_T("GetDBTransV2: Set event info on Volume %s Succeeded\n"), pDBTranData->MountPoint);
                } else {
                    _tprintf(_T("GetDBTransV2: Set event info on Volume %s: Failed: %d\n"), pDBTranData->MountPoint, GetLastError());
                    return ERROR_GEN_FAILURE;
                }
            }
        }

        if (NULL == hEvent) {
            _tprintf(_T("GetDBTransV2: event creation for Volume %s: Failed: %d\n"), pDBTranData->MountPoint, GetLastError());
            return ERROR_GEN_FAILURE;
        }
    }

	DWORD   dwStartTime = GetTickCount();
    bool    bGetNextDirtyBlock = false;
	_tprintf(_T(" Inside GetDTransV2 loop\n"));

    do {
	
        memset(&(pDBTranData->FCSequenceNumberV2), 0, sizeof(TSSNSEQIDV2));
        memset(&(pDBTranData->LCSequenceNumberV2), 0, sizeof(TSSNSEQIDV2));	
        // Get the CommitTrans ID
        if (true == pDBTranData->PrintDetails) {
            _tprintf(_T("\n"));
        }
        CommitTrans.ulFlags = 0;
        if (pDBTranData->CommitPreviousTrans) {
            bool bResetResync = false;

            ulError = GetPendingTransactionID(&PendingTranListHead, pDBTranData->VolumeGUID, CommitTrans.ulTransactionID.QuadPart, bResetResync);
            if (ERROR_SUCCESS != ulError) {
                if (sCommandOptions.bVerbose) {
                    _tprintf(_T("GetDBTransV2: No pending transaction\n"));
                }
                CommitTrans.ulTransactionID.QuadPart = 0;
            } else {
                if (sCommandOptions.bVerbose) {
                    if (bResetResync) {
                        _tprintf(_T("GetDBTransV2: Commiting pending transaction with id %#I64x and ResetResync\n"), 
                                 CommitTrans.ulTransactionID.QuadPart);
                    } else {
                        _tprintf(_T("GetDBTransV2: Commiting pending transaction with id %#I64x\n"), 
                                 CommitTrans.ulTransactionID.QuadPart);
                    }
                }
                if (bResetResync && pDBTranData->bResetResync){
                    CommitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
                }
            }
        } else {
            CommitTrans.ulTransactionID.QuadPart = 0;
        }

        memset(pDirtyBlock, 0, sizeof(UDIRTY_BLOCK_V2));

        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
                        &CommitTrans,
                        sizeof(COMMIT_TRANSACTION),
                        pDirtyBlock,
                        sizeof(UDIRTY_BLOCK_V2),
                        &dwReturn,  
                        NULL        // Overlapped
                        );
        if (bResult) {
            if (sCommandOptions.bVerbose) {
                wprintf(L"GetDBTransV2: Get dirty blocks transaction on Volume %s Succeeded\n", pDBTranData->MountPoint);
            }

            if (sizeof(UDIRTY_BLOCK_V2) != dwReturn) {
                _tprintf(_T("GetDBTransV2: dwReturn(%#x) did not match DIRTY_BLOCKS size(%#x)\n"), dwReturn, sizeof(UDIRTY_BLOCK_V2));
                return ERROR_GEN_FAILURE;
            }
        } else {
            wprintf(L"GetDBTransV2: Get dirty blocks pending transaction on Volume %s Failed: %d\n", pDBTranData->MountPoint, GetLastError());
            return ERROR_GEN_FAILURE;
        }
        ulNumDirtyBlocksReturned++;
        if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED) {
            SYSTEMTIME  OutOfSyncTime;
            GetTimeZoneInformation(&TimeZone);
            FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uHdr.Hdr.liOutOfSyncTimeStamp, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &OutOfSyncTime);
             _tprintf(_T("GetDBTransV2: Resync required flag is set\n"));
            _tprintf(_T("GetDBTransV2: Resync count is %#x\n"), pDirtyBlock->uHdr.Hdr.ulOutOfSyncCount);
            _tprintf(_T("GetDBTransV2: Resync Error Code = %#x\n"), pDirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode);
            _tprintf(_T("GetDBTransV2: TimeStamp of resync request : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                OutOfSyncTime.wMonth, OutOfSyncTime.wDay, OutOfSyncTime.wYear,
                                OutOfSyncTime.wHour, OutOfSyncTime.wMinute, OutOfSyncTime.wSecond, OutOfSyncTime.wMilliseconds);
            printf("GetDBTransV2: Error String is \"%s\"\n", pDirtyBlock->uHdr.Hdr.ErrorStringForResync);
            // Resync Step1 is required.
            if (1 == pDBTranData->sync) {
                pDBTranData->resync_req = 1;
                break;
            }
        }
		//_tprintf(_T("DirtyBlock header Flags : %x \n"), pDirtyBlock->uHdr.Hdr.ulFlags);
        // if no changes and no tags are returned, wait for event.
        if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE) {
            iTsoCount++;
            
			_tprintf(_T(" TSO file  \n"));
            // There are no changes in this dirty block.
            if (sCommandOptions.bVerbose) {
                _tprintf(_T("GetDBTransV2: TSO dirty block is returned\n"));
                _tprintf(_T("GetDBTransV2: current data source = %s\n"), DataSourceToString(pDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
            }
            if (true == pDBTranData->PrintDetails) {
                printf("..................TSO...............\n");
                _tprintf(_T("GetDBTransV2: dirty block number %d\n"), ulNumDirtyBlocksReturned);
                _tprintf(_T("GetDBTransV2: ulTransactionID = %#I64x\n"), pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart);
                _tprintf(_T("GetDBTransV2: Last Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO);
                _tprintf(_T("GetDBTransV2: Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO);
                printf("%s: Write order state : %s\n", __FUNCTION__, WriteOrderStateString(pDirtyBlock->uHdr.Hdr.eWOState));
                printf("%s: Number of changes = %#x (%#d)\n", __FUNCTION__, pDirtyBlock->uHdr.Hdr.cChanges, pDirtyBlock->uHdr.Hdr.cChanges);

                _tprintf(_T("GetDBTransV2: Time Stamp Of Previous Change : SeqNumber = %#I64x, TimeStamp : %#I64x \n"),
                        pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber, pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp);

                _tprintf(_T("GetDBTransV2: Time Stamp Of Current Change : SeqNumber = %#I64x, TimeStamp : %#I64x \n"),
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);

                _tprintf(_T("GetDBTransV2: Time Stamp Of Last Change : SeqNumber = %#I64x, TimeStamp : %#I64x \n"),
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
            }
            pDBTranData->FCSequenceNumberV2.ullSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber;
            pDBTranData->FCSequenceNumberV2.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            pDBTranData->LCSequenceNumberV2.ullSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber;
            pDBTranData->LCSequenceNumberV2.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
        } else {
            iTsoCount = 0;
            iStartTsoCount = 1;
            if (true == pDBTranData->PrintDetails) {
                std::string             sCbChanges, sCbChangesPending;
                ConvertULLtoString(pDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart, sCbChanges);
                ConvertULLtoString(pDirtyBlock->uHdr.Hdr.ulicbTotalChangesPending.QuadPart, sCbChangesPending);

                _tprintf(_T("GetDBTransV2: dirty block number %d\n"), ulNumDirtyBlocksReturned);
                _tprintf(_T("GetDBTransV2: ulTransactionID = %#I64x\n"), pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart);
                _tprintf(_T("GetDBTransV2: Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO);
                _tprintf(_T("GetDBTransV2: Last Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO);
                printf("%s: Write order state : %s\n", __FUNCTION__, WriteOrderStateString(pDirtyBlock->uHdr.Hdr.eWOState));
                printf("%s: Number of changes = %#x (%#d)\n", __FUNCTION__, pDirtyBlock->uHdr.Hdr.cChanges, pDirtyBlock->uHdr.Hdr.cChanges);
                printf("%s: Bytes of changes = %#I64x (%s)\n", __FUNCTION__, pDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart, sCbChanges.c_str());
                printf("%s: Bytes of changes pending = %#I64x (%s)\n", __FUNCTION__, 
                         pDirtyBlock->uHdr.Hdr.ulicbTotalChangesPending.QuadPart, sCbChangesPending.c_str());
                _tprintf(_T("GetDBTransV2: Time Stamp Of Previous Change : SequenceNumber = %#I64x, TimeStamp : %#I64x \n"),
                    pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber, pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp);

                if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE) {
                    _tprintf(_T("GetDBTransV2: DirtyBlock is start of split change\n"));
                }
                if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE) {
                    _tprintf(_T("GetDBTransV2: DirtyBlock is part of split change\n"));
                }
                if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE) {
                    _tprintf(_T("GetDBTransV2: DirtyBlock is end of split change\n"));
                }   
            }
            if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) {
				_tprintf(_T(" Data Mode - Data file \n"));
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {			
                    wprintf(L"GetDBTransV2: Dirty block consists of data file \"%s\"\n", pDirtyBlock->uTagList.DataFile.FileName);
                }			
                if (0 == PrintSVDstreamInfoV2(pDBTranData, pDirtyBlock)) {
                    if (NULL != pDirtyBlock) {
                        free(pDirtyBlock);
                        pDirtyBlock = NULL;
                    }
                    printf("PrintSVDstreamInfoV2 returned error in data file mode\n");
                    return ERROR_GEN_FAILURE;
                }
            } else if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
				_tprintf(_T(" Data Mode - Stream \n"));
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {			
                    wprintf(L"GetDBTransV2: Dirty block consists of data in stream format.\n");
                }			
                if (0 == PrintSVDstreamInfoV2(pDBTranData, pDirtyBlock)) {
                    if (NULL != pDirtyBlock) {
                        free(pDirtyBlock);
                        pDirtyBlock = NULL;
                    }
                    printf("PrintSVDstreamInfoV2 returned error in data mode\n");
                    return ERROR_GEN_FAILURE;
				}
			}  else { // Following code depicts the behavior of S2, when no flags are set, usually it may be a tag or data shipped with source as MD/Bitmap
				_tprintf(_T("GetDBTransV2: Data source = %s\n"), DataSourceToString(pDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
				_tprintf(_T("GetDBTransV2: No User flags are set by Driver; Source may be MD/Bitmap or Tags(DB Header)\n"));
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                    SYSTEMTIME              FirstChangeLT, LastChangeLT;
                    GetTimeZoneInformation(&TimeZone);
                    FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                                    &SystemTime);
                    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &FirstChangeLT);
                    FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            &SystemTime);
                    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LastChangeLT);

                    _tprintf(_T("GetDBTransV2: Data source = %s\n"), DataSourceToString(pDirtyBlock->uHdr.Hdr.ulDataSource));
                    _tprintf(_T("GetDBTransV2: TimeStamp of first change : SeqID = %#I64x,  %#I64x %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                        pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                                        pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                                        FirstChangeLT.wMonth, FirstChangeLT.wDay, FirstChangeLT.wYear,
                                        FirstChangeLT.wHour, FirstChangeLT.wMinute, FirstChangeLT.wSecond, FirstChangeLT.wMilliseconds);
                    _tprintf(_T("GetDBTransV2: TimeStamp of last change  : SeqID = %#I64x, %#I64x %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                        pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
                                        pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                        LastChangeLT.wMonth, LastChangeLT.wDay, LastChangeLT.wYear,
                                        LastChangeLT.wHour, LastChangeLT.wMinute, LastChangeLT.wSecond, LastChangeLT.wMilliseconds);

                    
                    _tprintf(_T("GetDBTransV2: Printing change details\n"));
                    if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
                        _tprintf(_T("BufferArray = %#p\n"), pDirtyBlock->uHdr.Hdr.ppBufferArray);
                        _tprintf(_T("Max number of buffers = %#d\n"), pDirtyBlock->uHdr.Hdr.usMaxNumberOfBuffers);
                        _tprintf(_T("Number of buffers = %#d\n"), pDirtyBlock->uHdr.Hdr.usNumberOfBuffers);
                        if (NULL != pDirtyBlock->uHdr.Hdr.ppBufferArray) {
                            for (unsigned long i = 0; i < pDirtyBlock->uHdr.Hdr.usNumberOfBuffers; i++) {
                                _tprintf(_T("Index = %d, Address = %#p\n"), i, pDirtyBlock->uHdr.Hdr.ppBufferArray[i]);
                            }
                        }
                    }
                }
                pDBTranData->FCSequenceNumberV2.ullSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber;
                pDBTranData->FCSequenceNumberV2.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
     	        pDBTranData->LCSequenceNumberV2.ullSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber;
                pDBTranData->LCSequenceNumberV2.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;

                for (unsigned long i = 0; i < pDirtyBlock->uHdr.Hdr.cChanges; i++) {
                    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                        SYSTEMTIME              TimeDelta;
                        ULONGLONG               TotalTime = 0;
                        TotalTime = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 + pDirtyBlock->TimeDeltaArray[i];
                        FileTimeToSystemTime((FILETIME *)&TotalTime,&SystemTime);
                        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &TimeDelta);
                        _tprintf(_T("Index = %4d, Offset = %#12I64x, Len = %#6x,\n              TimeDelta = %#9x, SeqDelta = %x\n"), i, pDirtyBlock->ChangeOffsetArray[i],
                                pDirtyBlock->ChangeLengthArray[i],pDirtyBlock->TimeDeltaArray[i],pDirtyBlock->SequenceNumberDeltaArray[i]);
                        _tprintf(_T("              TimeDelta  = %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                TimeDelta.wMonth, TimeDelta.wDay, TimeDelta.wYear,
                                TimeDelta.wHour, TimeDelta.wMinute, TimeDelta.wSecond, TimeDelta.wMilliseconds);
                    }
					//METADATA/BIMAP : Read from disk directly in metadata/bitmap mode
                    if (pDBTranData->sync) {
                        if (DataSync (pDBTranData, NULL, pDirtyBlock->ChangeOffsetArray[i], pDirtyBlock->ChangeLengthArray[i])) {
                            printf("Error in DataSync  Offset = %#12I64x, Len = %#6x\n", pDirtyBlock->ChangeOffsetArray[i], pDirtyBlock->ChangeLengthArray[i]);
                            if (NULL != pDirtyBlock) {
                                free(pDirtyBlock);
                                pDirtyBlock = NULL;
                            }
                            return ERROR_GEN_FAILURE;
                        }
                    }
                }
                
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {		    
                    PSTREAM_REC_HDR_4B      pTag = NULL;
                    ULONG                   ulBytesProcessed = 0, ulMaxBytesToParse;
                    ULONG                   ulNumberOfUserDefinedTags = 0;

                    ulBytesProcessed = (ULONG) ((PUCHAR) &pDirtyBlock->uTagList.TagList.TagEndOfList - (PUCHAR) pDirtyBlock->uTagList.BufferForTags);
                    ulMaxBytesToParse = sizeof(pDirtyBlock->uTagList.BufferForTags) - ulBytesProcessed;
                    pTag = (PSTREAM_REC_HDR_4B) (pDirtyBlock->uTagList.BufferForTags + ulBytesProcessed);

                    while(pTag->usStreamRecType != STREAM_REC_TYPE_END_OF_TAG_LIST) {
                        ULONG ulLength = STREAM_REC_SIZE(pTag);
                        ULONG ulHeaderLength = (pTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ? sizeof(STREAM_REC_HDR_8B) : sizeof(STREAM_REC_HDR_4B);

                        if (pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG){
                            ULONG   ulLengthOfTag = 0;
                            PUCHAR   pTagData;

                            ulNumberOfUserDefinedTags++;
                            _tprintf(_T("User Defined Tag%ld:\n"), ulNumberOfUserDefinedTags);

                            ulLengthOfTag = *(PUSHORT)((PUCHAR)pTag + ulHeaderLength);
                            _tprintf(_T("Length of Tag:%d\n"), ulLengthOfTag);

                            pTagData = (PUCHAR)pTag + ulHeaderLength + sizeof(USHORT);
                            _tprintf(_T("Tag data is\n"));
                            PrintBinaryData(pTagData, ulLengthOfTag);
							_tprintf(_T(" Found a valid Tag"));
							_tprintf(_T(" Committing the DirtyBlock with tag assuming it has only just TAG\n" ));
							iTagCount++;
                        } else {
                            PrintUnidentifiedTag(pTag, ulMaxBytesToParse);
                        }

                        ulBytesProcessed += ulLength;
                        ulMaxBytesToParse -= ulLength;
                        pTag = (PSTREAM_REC_HDR_4B)(pDirtyBlock->uTagList.BufferForTags + ulBytesProcessed);
                    }

                    if(ulNumberOfUserDefinedTags > 0) {
                        _tprintf(_T("Total User Defined Tags Present: %ld\n"), ulNumberOfUserDefinedTags);
                    }                    
                }
            }
        }

        ULONG ret = ValidateTimeStampSeqV2(pDBTranData, pDirtyBlock, &tempPrevTSSequenceNumberV2);
        if (ret != ERROR_SUCCESS) {
            printf("Error in ValidateTimeStampSeqV2\n");
            return ret;
        }
        if (0 != pDBTranData->ulWaitTimeBeforeCurrentTransCommit) {
            Sleep(pDBTranData->ulWaitTimeBeforeCurrentTransCommit);
        }

        if ((0 != pDBTranData->ulWaitTimeBeforeCurrentTransCommit) || (true == pDBTranData->CommitCurrentTrans))
        {
            CommitTrans.ulFlags = 0;
            CommitTrans.ulTransactionID.QuadPart = pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart;
            if ((pDBTranData->bResetResync) && ( pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)) {
                CommitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
            }

            if (0 == CommitTrans.ulTransactionID.QuadPart) {
                _tprintf(_T("This is not a transaction, Skipping commit\n")); 
            } else {
                bResult = DeviceIoControl(
                                hVFCtrlDevice,
                                (DWORD)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                                &CommitTrans,
                                sizeof(COMMIT_TRANSACTION),
                                NULL,
                                0,
                                &dwReturn,  
                                NULL        // Overlapped
                                );
                if (bResult) {
                    if (true == pDBTranData->PrintDetails) {			    
                        wprintf(L"Commiting pending transaction on Volume %s Succeeded\n", pDBTranData->MountPoint);
                    }
                    PrevTSSequenceNumberV2.ulContinuationId = pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO;
                    PrevTSSequenceNumberV2.ullSequenceNumber = pDBTranData->LCSequenceNumberV2.ullSequenceNumber;
                    PrevTSSequenceNumberV2.ullTimeStamp = pDBTranData->LCSequenceNumberV2.ullTimeStamp;
                } else {
                    wprintf(L"Commiting pending transaction on Volume %s Failed: %d\n", pDBTranData->MountPoint, GetLastError());
                    PrevTSSequenceNumberV2.ulContinuationId = tempPrevTSSequenceNumberV2.ulContinuationId;
                    PrevTSSequenceNumberV2.ullSequenceNumber = tempPrevTSSequenceNumberV2.ullSequenceNumber;
                    PrevTSSequenceNumberV2.ullTimeStamp = tempPrevTSSequenceNumberV2.ullTimeStamp;
                }
            }
        } else {
            AddPendingTransaction(&PendingTranListHead, 
                                  pDBTranData->VolumeGUID, 
                                  pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart,
                                  pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED ? true : false);
        }
        if ((pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE) && (pDBTranData->bWaitIfTSO)) {
            DWORD   dwWaitInMilliSeconds;
            if (pDBTranData->bPollForDirtyBlocks) {
                dwWaitInMilliSeconds = pDBTranData->ulPollIntervalInMilliSeconds;
                if (sCommandOptions.bVerbose) {
                    wprintf(L"GetDBTransV2: Waiting to poll for dirty blocks for Volume %s\n", pDBTranData->MountPoint);
                }
            } else {
                // Wait for event to be set.
                if (pDBTranData->ulRunTimeInMilliSeconds) {
                    DWORD   dwTimeRun = GetTickCount() - dwStartTime;
                    if (pDBTranData->ulRunTimeInMilliSeconds > dwTimeRun) {
                        dwWaitInMilliSeconds = pDBTranData->ulRunTimeInMilliSeconds - dwTimeRun;
                    } else {
                        // Dont come out of loop even if wait time spent is 0
                        if (pDBTranData->sync) {
                            dwWaitInMilliSeconds = 1;
                        } else {
                            printf("Time elapsed\n");
                            // Time elapsed.
                            break;
                        }
                    }
                } else {
					dwWaitInMilliSeconds = S2TIMEOUT; // DBF - Changing to 65 seconds
                }
                if (sCommandOptions.bVerbose) {
                    wprintf(L"GetDBTransV2: Waiting on dirty block notify event for Volume %s\n", pDBTranData->MountPoint);
                }
            }

            DWORD   dwReturnValue;
			_tprintf(_T(" Wait time : %d seconds\n"), (dwWaitInMilliSeconds/1000));
            dwReturnValue = WaitForSingleObject(hEvent, dwWaitInMilliSeconds);
            // Dont come out of loop if sync flag is set
            //if (!pDBTranData->sync) {
                if (WAIT_ABANDONED == dwReturnValue) {
                    _tprintf(_T("GetDBTransV2: Wait on dirty block notify event is abandoned\n"));
                    break;
                }

                if (WAIT_TIMEOUT == dwReturnValue) {
                    if (sCommandOptions.bVerbose) {
                        _tprintf(_T("GetDBTransV2: Wait on dirty block notify event has timed out\n"));
                    }
					_tprintf(_T("GetDBTransV2: Notify Event Time Out\n"));
                    //if (!pDBTranData->bPollForDirtyBlocks) {
                    //    _tprintf(_T("GetDBTransV2: Wait on dirty block notify event has timed out and Polling is disabled\n"));
                    //    break;
                    //}
                }

                if (STATUS_WAIT_0 == dwReturnValue) {
                    if (sCommandOptions.bVerbose) {
                        _tprintf(_T("GetDBTransV2: Dirty block notify event signalled\n"));
                    }
					_tprintf(_T("GetDBTransV2: Notify Event Signalled\n"));
                }            
            //}
        }

        // Check if we have to run again.
        bGetNextDirtyBlock = true;

		if ((pDBTranData->bStopReplicationOnTag) && (iTagCount)) { // There are few tags and we would like to break the replication
			bGetNextDirtyBlock = false;
		}

        if (pDBTranData->ulRunTimeInMilliSeconds) {
            if ((GetTickCount() - dwStartTime) < pDBTranData->ulRunTimeInMilliSeconds) {
                // We stil have time.
                if (pDBTranData->ulNumDirtyBlocks) {
                    // User specified number of dirty blocks too
                    if (ulNumDirtyBlocksReturned < pDBTranData->ulNumDirtyBlocks) {
                        bGetNextDirtyBlock = true;
                    } else {
                        printf("User specified number of dirty blocks drained\n");
                    }
                } else {
                    bGetNextDirtyBlock = true;
                }
            } else {
                printf("Time elapsed\n");
            }
        } else {
            if (pDBTranData->ulNumDirtyBlocks) {
                // User specified number of dirty blocks
                if (ulNumDirtyBlocksReturned < pDBTranData->ulNumDirtyBlocks) {
                    bGetNextDirtyBlock = true;
                } else {
                    printf("User specified number of dirty blocks drained\n");
                }
            }
        }

    } while (bGetNextDirtyBlock);

    if (NULL != pDirtyBlock) {
        free(pDirtyBlock);
        pDirtyBlock = NULL;
    }
	if (false == bGetNextDirtyBlock)
		return ERROR_NOT_READY;
    return ERROR_SUCCESS;
}

ULONG ValidateTimeStampSeqV2 (PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK_V2 pDirtyBlock, PTSSNSEQIDV2 ptempPrevTSSequenceNumberV2) {
    bool bVerify = true;
    if ((0xFFFFFFFFFFFFFFFF == pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) && 
        (0xFFFFFFFFFFFFFFFF == pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber) && 
        (0xFFFFFFFF == pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO)) {
        if (true == pDBTranData->PrintDetails) {
            printf("GetDBTransV2 : Previous Values are MAX. No Ordering Check\n");
        }
        bVerify = false;
    }
    if ((0 == pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) && (0 == pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber) && 
        (0 == pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO)) {
        if (true == pDBTranData->PrintDetails) {
            printf("GetDBTransV2 : Previous Values are Zero. This is first file to drain. No ordering check\n");
        }
        bVerify = false;
    }
    // Validate the TimeStamp/SequenceNumber/Continuation id to make sure that No OOD from Driver
    if (bVerify) {
        ptempPrevTSSequenceNumberV2->ulContinuationId = pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO;
        ptempPrevTSSequenceNumberV2->ullSequenceNumber = pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber;
        ptempPrevTSSequenceNumberV2->ullTimeStamp = pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp;
        // Last stored seq number should match current dirtly block prev Number field
        if ((PrevTSSequenceNumberV2.ullSequenceNumber != pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber) && 
                (PrevTSSequenceNumberV2.ullTimeStamp !=0)) {
            if (PrevTSSequenceNumberV2.ullSequenceNumber < pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber) {
                printf("Prev Seq num is lesser than Curr Block Prev seq num.\nResync must have been issued. Pls Check\n");
            }
            printf("%s: Static prev Seq num (%#I64u) is not equal to Curr Dirty block prev Seq num (%#I64u)\n",
                    __FUNCTION__, PrevTSSequenceNumberV2.ullSequenceNumber, 
                    pDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber);
            return ERROR_GEN_FAILURE;
        }
        if ((PrevTSSequenceNumberV2.ullTimeStamp != pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) && 
                (PrevTSSequenceNumberV2.ullTimeStamp !=0)) {
            if (PrevTSSequenceNumberV2.ullTimeStamp < pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) {
                printf("Prev Seq timestamp is lesser than Curr Block Prev timestamp.\nResync must have been issued. Pls Check\n");
            }		
            printf("%s: Static prev TimeStamp (%#I64u) is not equal to Curr Dirty block prev TimeStamp (%#I64u)\n",
                    __FUNCTION__, PrevTSSequenceNumberV2.ullTimeStamp, 
                    pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp);
            return ERROR_GEN_FAILURE;
        }
        // ullSequenceNumber cannot be zero in any case.
        if (pDBTranData->FCSequenceNumberV2.ullSequenceNumber == 0) {
            printf("%s: Error TagTimeStampOfFirstChange.ullSequenceNumber set to zero" 
                    "Last change Seq num (%#I64u) first change Seq num (%#I64u)\n",
                    __FUNCTION__, pDBTranData->LCSequenceNumberV2.ullSequenceNumber, 
            pDBTranData->FCSequenceNumberV2.ullSequenceNumber);
            return ERROR_GEN_FAILURE;
        }

        if (pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO < pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO) {
            if (pDBTranData->LCSequenceNumberV2.ullTimeStamp != 
                    PrevTSSequenceNumberV2.ullTimeStamp) {
                printf("GetDBTransV2[Error]: Continuation Ids are increasing, Diffs should have same Last time Stamps\n");
                return ERROR_GEN_FAILURE;
            }
            if (pDBTranData->LCSequenceNumberV2.ullSequenceNumber != 
                    PrevTSSequenceNumberV2.ullSequenceNumber) {
                printf("GetDBTransV2[Error]: Continuation Ids are increasing, Diffs should have same Last Sequence Numbers\n");
                return ERROR_GEN_FAILURE;
            } 
        } else {
            bool bStatus = VerifyDifferentialsOrderV2(&pDBTranData->FCSequenceNumberV2,
                    ptempPrevTSSequenceNumberV2);
            if (true == bStatus) { 
                if (true == pDBTranData->PrintDetails) {
                    printf("GetDBTransV2 : Differentials are in Correct Order\n");
                }
            } else {
                printf("GetDBTransV2 : Differentials are not in Correct Order\n");
                return ERROR_GEN_FAILURE;
            }
        }
    }    
    if ((pDBTranData->FCSequenceNumberV2.ullTimeStamp == 0) ||
            (pDBTranData->LCSequenceNumberV2.ullTimeStamp == 0)) {
        printf("%s: Error TimeInHundNanoSecondsFromJan1601 set to zero" 
                "Last change TimeStamp (%#I64u) first change TimeStamp (%#I64u)\n",
                __FUNCTION__, pDBTranData->LCSequenceNumberV2.ullTimeStamp, 
        pDBTranData->FCSequenceNumberV2.ullTimeStamp);
        return ERROR_GEN_FAILURE;
    }
    // Last Change TimeStamp shd never be less than First Change TimeStamp
    if (pDBTranData->FCSequenceNumberV2.ullTimeStamp >
            pDBTranData->LCSequenceNumberV2.ullTimeStamp) {
        printf("%s: Last change TimeStamp (%#I64u) is lesser than First change TimeStamp (%#I64u)\n",
                __FUNCTION__, pDBTranData->LCSequenceNumberV2.ullTimeStamp, 
                pDBTranData->FCSequenceNumberV2.ullTimeStamp);
        return ERROR_GEN_FAILURE;
    }
    if ((pDBTranData->FCSequenceNumberV2.ullSequenceNumber >
            pDBTranData->LCSequenceNumberV2.ullSequenceNumber) && ((0xFFFFFFFFF8200000) > 
            pDBTranData->FCSequenceNumberV2.ullSequenceNumber)) {
        printf("%s: First Seq num (%#I64u) is greater than Last Seq num (%#I64u)\n",
                __FUNCTION__, pDBTranData->FCSequenceNumberV2.ullSequenceNumber, 
                pDBTranData->LCSequenceNumberV2.ullSequenceNumber);
        return ERROR_GEN_FAILURE;      
    }
    //Validate the last timestamp / seq no  = first  + last delta - Applicable of Non-data files
	if (!((pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) ||
            (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM))) {
    if (pDirtyBlock->uHdr.Hdr.cChanges > 0) {
        ULONG Change = pDirtyBlock->uHdr.Hdr.cChanges - 1;
        if ((pDBTranData->LCSequenceNumberV2.ullTimeStamp != 
                pDBTranData->FCSequenceNumberV2.ullTimeStamp + pDirtyBlock->TimeDeltaArray[Change])||
                (pDBTranData->LCSequenceNumberV2.ullSequenceNumber != 
                pDBTranData->FCSequenceNumberV2.ullSequenceNumber + pDirtyBlock->SequenceNumberDeltaArray[Change])){
            printf("\nError: Last time stamp tag is not matching the summation of first and the last delta\n");
            printf("Last timestamp %#I64x And according to delta %#I64x\n",
                    pDBTranData->LCSequenceNumberV2.ullTimeStamp,
                    pDBTranData->FCSequenceNumberV2.ullTimeStamp + 
                    pDirtyBlock->TimeDeltaArray[Change]);
            printf("Last Sequence  %#I64x And according to delta %#I64x\n",
                    pDBTranData->LCSequenceNumberV2.ullSequenceNumber,
                    pDBTranData->FCSequenceNumberV2.ullSequenceNumber + 
                    pDirtyBlock->SequenceNumberDeltaArray[Change]);
            return ERROR_GEN_FAILURE;
        }
    }
    for (unsigned long i = 1; i < pDirtyBlock->uHdr.Hdr.cChanges; i++) {
        ULONG   i_old = i - 1;
        //Compare prev delta value with curr delta value
        if ((((pDirtyBlock->SequenceNumberDeltaArray[i_old]) > 
                (pDirtyBlock->SequenceNumberDeltaArray[i])) && ((0xFFFFFFFFF8200000) >
                (pDirtyBlock->SequenceNumberDeltaArray[i_old]))) || ((pDirtyBlock->TimeDeltaArray[i_old]) > 
                (pDirtyBlock->TimeDeltaArray[i]))) {
            printf("%s: Prev delta change time stamp %#I64u (or seq num %#I64u) is greater than" 
                    "Current delta change time stamp %#I64u (or seq num %#I64u) at index %u\n",
                    __FUNCTION__, pDirtyBlock->TimeDeltaArray[i_old], 
                    pDirtyBlock->SequenceNumberDeltaArray[i_old], 
                    pDirtyBlock->TimeDeltaArray[i], 
                    pDirtyBlock->SequenceNumberDeltaArray[i], i);
            return ERROR_GEN_FAILURE;
        }
    }
	}
    return ERROR_SUCCESS;
}

ULONG ValidateTimeStampSeq (PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK pDirtyBlock, PTSSNSEQID ptempPrevTSSequenceNumber) {
    bool bVerify = true;
    if ((0xFFFFFFFFFFFFFFFF == pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) && 
        (0xFFFFFFFF == pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber) && 
        (0xFFFFFFFF == pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO)) {
        if (true == pDBTranData->PrintDetails) {
            printf("GetDBTrans : Previous Values are MAX. No Ordering Check\n");
        }
        bVerify = false;
    }
    if ((0 == pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) && (0 == pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber) && 
        (0 == pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO)) {
        if (true == pDBTranData->PrintDetails) {
            printf("GetDBTrans : Previous Values are Zero. This is first file to drain. No ordering check\n");
        }
        bVerify = false;
    }
    // Validate the TimeStamp/SequenceNumber/Continuation id to make sure that No OOD from Driver
    if (bVerify) {
        ptempPrevTSSequenceNumber->ulContinuationId = pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO;
        ptempPrevTSSequenceNumber->ulSequenceNumber = pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber;
        ptempPrevTSSequenceNumber->ullTimeStamp = pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp;
        // Last stored seq number should match current dirtly block prev Number field
        if ((PrevTSSequenceNumber.ulSequenceNumber != pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber) && 
                (PrevTSSequenceNumber.ullTimeStamp !=0)) {
            if (PrevTSSequenceNumber.ulSequenceNumber < pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber) {
                printf("Prev Seq num is lesser than Curr Block Prev seq num.\nResync must have been issued. Pls Check\n");
            }
            printf("%s: Static prev Seq num (%u) is not equal to Curr Dirty block prev Seq num (%u)\n",
                    __FUNCTION__, PrevTSSequenceNumber.ulSequenceNumber, 
                    pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber);
            return ERROR_GEN_FAILURE;
        }
        if ((PrevTSSequenceNumber.ullTimeStamp != pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) && 
                (PrevTSSequenceNumber.ullTimeStamp !=0)) {
            if (PrevTSSequenceNumber.ullTimeStamp < pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp) {
                printf("Prev Seq timestamp is lesser than Curr Block Prev timestamp.\nResync must have been issued. Pls Check\n");
            }		
            printf("%s: Static prev TimeStamp (%#I64u) is not equal to Curr Dirty block prev TimeStamp (%#I64u)\n",
                    __FUNCTION__, PrevTSSequenceNumber.ullTimeStamp, 
                    pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp);
            return ERROR_GEN_FAILURE;
        }

        if (pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO < pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO) {
            if (pDBTranData->LCSequenceNumber.ullTimeStamp != 
                    PrevTSSequenceNumber.ullTimeStamp) {
                printf("GetDBTrans[Error]: Continuation Ids are increasing, Diffs should have same Last time Stamps\n");
                return ERROR_GEN_FAILURE;
            }
            if (pDBTranData->LCSequenceNumber.ulSequenceNumber != 
                    PrevTSSequenceNumber.ulSequenceNumber) {
                printf("GetDBTrans[Error]: Continuation Ids are increasing, Diffs should have same Last Sequence Numbers\n");
                return ERROR_GEN_FAILURE;
            } 
        } else {
            bool bStatus = VerifyDifferentialsOrder(&pDBTranData->FCSequenceNumber,
                    ptempPrevTSSequenceNumber);
            if (true == bStatus) { 
                if (true == pDBTranData->PrintDetails) {
                    printf("GetDBTrans : Differentials are in Correct Order\n");
                }
            } else {
                printf("GetDBTrans : Differentials are not in Correct Order\n");
                return ERROR_GEN_FAILURE;
            }
        }
    }    
    if ((pDBTranData->FCSequenceNumber.ullTimeStamp == 0) ||
            (pDBTranData->LCSequenceNumber.ullTimeStamp == 0)) {
        printf("%s: Error TimeInHundNanoSecondsFromJan1601 set to zero" 
                "Last change TimeStamp (%#I64u) first change TimeStamp (%#I64u)\n",
                __FUNCTION__, pDBTranData->LCSequenceNumber.ullTimeStamp, 
        pDBTranData->FCSequenceNumber.ullTimeStamp);
        return ERROR_GEN_FAILURE;
    }
    // Last Change TimeStamp shd never be less than First Change TimeStamp
    if (pDBTranData->FCSequenceNumber.ullTimeStamp >
            pDBTranData->LCSequenceNumber.ullTimeStamp) {
        printf("%s: Last change TimeStamp (%#I64u) is lesser than First change TimeStamp (%#I64u)\n",
                __FUNCTION__, pDBTranData->LCSequenceNumber.ullTimeStamp, 
                pDBTranData->FCSequenceNumber.ullTimeStamp);
        return ERROR_GEN_FAILURE;
    } else if (pDBTranData->FCSequenceNumber.ullTimeStamp ==
            pDBTranData->LCSequenceNumber.ullTimeStamp) {
        if ((pDBTranData->FCSequenceNumber.ulSequenceNumber >
                pDBTranData->LCSequenceNumber.ulSequenceNumber) && ((0x7FFF0000) > 
                pDBTranData->FCSequenceNumber.ulSequenceNumber)) {
            printf("%s: First Seq num (%u) is greater than Last Seq num (%u)\n",
                    __FUNCTION__, pDBTranData->FCSequenceNumber.ulSequenceNumber, 
                    pDBTranData->LCSequenceNumber.ulSequenceNumber);
            return ERROR_GEN_FAILURE;      
        }
    }
    return ERROR_SUCCESS;
}


ULONG
GetDBTrans(
    HANDLE              hVFCtrlDevice,
    PGET_DB_TRANS_DATA  pDBTranData
    )
{
    PUDIRTY_BLOCK       pDirtyBlock;
    COMMIT_TRANSACTION  CommitTrans;
    BOOL    bResult, bWaitForEvent;
    DWORD   dwReturn;
    ULONG   ulError, ulNumDirtyBlocksReturned;
    HANDLE  hEvent = NULL;
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime;
    TSSNSEQID   tempPrevTSSequenceNumber   = {0,0,0};
    int iTsoCount = 0;
    int iStartTsoCount = 0;
    pDirtyBlock = (PUDIRTY_BLOCK)malloc(sizeof(UDIRTY_BLOCK));

    if (NULL == pDirtyBlock) {
        _tprintf(_T("GetDBTrans: Memory allocation failed for DIRTY_BLOCKS\n"));
        return ERROR_OUTOFMEMORY;
    }

	inm_memcpy_s(CommitTrans.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

    bWaitForEvent = FALSE;
    ulNumDirtyBlocksReturned = 0;

    if (pDBTranData->bWaitIfTSO) {
        if (pDBTranData->bPollForDirtyBlocks) {
            hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        } else {
            std::wstring VolumeGUID(pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS);

            if (CreateNewDBWaitEventForVolume(VolumeGUID, hEvent)) {
                VOLUME_DB_EVENT_INFO    EventInfo;

                // Created new event have to do the IOCTL to set the event.
				inm_memcpy_s(EventInfo.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
                EventInfo.hEvent = hEvent;
                bResult = DeviceIoControl(
                                hVFCtrlDevice,
                                (DWORD)IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT,
                                &EventInfo,
                                sizeof(VOLUME_DB_EVENT_INFO),
                                NULL,
                                0,
                                &dwReturn,  
                                NULL        // Overlapped
                                );
                if (bResult) {
                    _tprintf(_T("GetDBTrans: Set event info on Volume %s Succeeded\n"), pDBTranData->MountPoint);
                } else {
                    _tprintf(_T("GetDBTrans: Set event info on Volume %s: Failed: %d\n"), pDBTranData->MountPoint, GetLastError());
                    return ERROR_GEN_FAILURE;
                }
            }
        }

        if (NULL == hEvent) {
            _tprintf(_T("GetDBTrans: event creation for Volume %s: Failed: %d\n"), pDBTranData->MountPoint, GetLastError());
            return ERROR_GEN_FAILURE;
        }
    }

    DWORD   dwStartTime = GetTickCount();
    bool    bGetNextDirtyBlock = false;

    do {
        memset(&(pDBTranData->FCSequenceNumber), 0, sizeof(TSSNSEQID));
        memset(&(pDBTranData->LCSequenceNumber), 0, sizeof(TSSNSEQID));
	    
        // Get the CommitTrans ID
        if (true == pDBTranData->PrintDetails) {
            _tprintf(_T("\n"));
        }
        CommitTrans.ulFlags = 0;
        if (pDBTranData->CommitPreviousTrans) {
            bool bResetResync = false;

            ulError = GetPendingTransactionID(&PendingTranListHead, pDBTranData->VolumeGUID, CommitTrans.ulTransactionID.QuadPart, bResetResync);
            if (ERROR_SUCCESS != ulError) {
                if (sCommandOptions.bVerbose) {
                    _tprintf(_T("GetDBTrans: No pending transaction\n"));
                }
                CommitTrans.ulTransactionID.QuadPart = 0;
            } else {
                if (sCommandOptions.bVerbose) {
                    if (bResetResync) {
                        _tprintf(_T("GetDBTrans: Commiting pending transaction with id %#I64x and ResetResync\n"), 
                                 CommitTrans.ulTransactionID.QuadPart);
                    } else {
                        _tprintf(_T("GetDBTrans: Commiting pending transaction with id %#I64x\n"), 
                                 CommitTrans.ulTransactionID.QuadPart);
                    }
                }
                if (bResetResync && pDBTranData->bResetResync){
                    CommitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
                }
            }
        } else {
            CommitTrans.ulTransactionID.QuadPart = 0;
        }

        memset(pDirtyBlock, 0, sizeof(UDIRTY_BLOCK));

        bResult = DeviceIoControl(
                        hVFCtrlDevice,
                        (DWORD)IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
                        &CommitTrans,
                        sizeof(COMMIT_TRANSACTION),
                        pDirtyBlock,
                        sizeof(UDIRTY_BLOCK),
                        &dwReturn,  
                        NULL        // Overlapped
                        );

        if (bResult) {
            if (sCommandOptions.bVerbose) {
                wprintf(L"GetDBTrans: Get dirty blocks transaction on Volume %s Succeeded\n", pDBTranData->MountPoint);
            }

            if (sizeof(UDIRTY_BLOCK) != dwReturn) {
                _tprintf(_T("GetDBTrans: dwReturn(%#x) did not match DIRTY_BLOCKS size(%#x)\n"), dwReturn, sizeof(UDIRTY_BLOCK));
                return ERROR_GEN_FAILURE;
            }
        } else {
            wprintf(L"GetDBTrans: Get dirty blocks pending transaction on Volume %s Failed: %d\n", pDBTranData->MountPoint, GetLastError());
            return ERROR_GEN_FAILURE;
        }
        ulNumDirtyBlocksReturned++;
        if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED) {
            SYSTEMTIME  OutOfSyncTime;
            GetTimeZoneInformation(&TimeZone);
            FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uHdr.Hdr.liOutOfSyncTimeStamp, &SystemTime);
            SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &OutOfSyncTime);
             _tprintf(_T("GetDBTrans: Resync required flag is set\n"));
            _tprintf(_T("GetDBTrans: Resync count is %#x\n"), pDirtyBlock->uHdr.Hdr.ulOutOfSyncCount);
            _tprintf(_T("GetDBTrans: Resync Error Code = %#x\n"), pDirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode);
            _tprintf(_T("GetDBTrans: TimeStamp of resync request : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                OutOfSyncTime.wMonth, OutOfSyncTime.wDay, OutOfSyncTime.wYear,
                                OutOfSyncTime.wHour, OutOfSyncTime.wMinute, OutOfSyncTime.wSecond, OutOfSyncTime.wMilliseconds);
            printf("GetDBTrans: Error String is \"%s\"\n", pDirtyBlock->uHdr.Hdr.ErrorStringForResync);
            // Resync Step1 is required.
            if (1 == pDBTranData->sync) {
                pDBTranData->resync_req = 1;
                break;
            }
        }
        // if no changes and no tags are returned, wait for event.
        if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE) {
            iTsoCount++;

            // There are no changes in this dirty block.
            if (sCommandOptions.bVerbose) {
                _tprintf(_T("GetDBTrans: TSO dirty block is returned\n"));
                _tprintf(_T("GetDBTrans: current data source = %s\n"), DataSourceToString(pDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
            }
            if (true == pDBTranData->PrintDetails) {
                printf("..................TSO...............\n");
                _tprintf(_T("GetDBTrans: dirty block number %d\n"), ulNumDirtyBlocksReturned);
                _tprintf(_T("GetDBTrans: ulTransactionID = %#I64x\n"), pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart);
                printf("%s: Write order state : %s\n", __FUNCTION__, WriteOrderStateString(pDirtyBlock->uHdr.Hdr.eWOState));
                printf("%s: Number of changes = %#x (%#d)\n", __FUNCTION__, pDirtyBlock->uHdr.Hdr.cChanges, pDirtyBlock->uHdr.Hdr.cChanges);
                _tprintf(_T("GetDBTrans: Time Stamp Of Previous Change : SeqNumber = %x, TimeStamp : %#I64x \n"),
                        pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber, pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp);
                _tprintf(_T("GetDBTrans: Last Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO);

                _tprintf(_T("GetDBTrans: Time Stamp Of First Change : SeqNumber = %x , TimeStamp : %#I64x\n"),
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber,
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);

                _tprintf(_T("GetDBTrans: Time Stamp Of Last Change : SeqNumber = %x ,TimeStamp : %#I64x\n"),
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber,
                        pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
                _tprintf(_T("GetDBTrans: Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO);
            }
            pDBTranData->FCSequenceNumber.ulSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber;
            pDBTranData->FCSequenceNumber.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            pDBTranData->LCSequenceNumber.ulSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber;
            pDBTranData->LCSequenceNumber.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
        } else {
            iTsoCount = 0;
            iStartTsoCount = 1;
            if (true == pDBTranData->PrintDetails) {
                std::string             sCbChanges, sCbChangesPending;

                ConvertULLtoString(pDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart, sCbChanges);
                ConvertULLtoString(pDirtyBlock->uHdr.Hdr.ulicbTotalChangesPending.QuadPart, sCbChangesPending);

                _tprintf(_T("GetDBTrans: dirty block number %d\n"), ulNumDirtyBlocksReturned);
                _tprintf(_T("GetDBTrans: ulTransactionID = %#I64x\n"), pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart);
                _tprintf(_T("GetDBTrans: Last Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO);
                _tprintf(_T("GetDBTrans: Continuation ID = %#x\n"), pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO);
                printf("%s: Write order state : %s\n", __FUNCTION__, WriteOrderStateString(pDirtyBlock->uHdr.Hdr.eWOState));
                printf("%s: Number of changes = %#x (%#d)\n", __FUNCTION__, pDirtyBlock->uHdr.Hdr.cChanges, pDirtyBlock->uHdr.Hdr.cChanges);
                printf("%s: Bytes of changes = %#I64x (%s)\n", __FUNCTION__, pDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart, sCbChanges.c_str());
                printf("%s: Bytes of changes pending = %#I64x (%s)\n", __FUNCTION__, 
                         pDirtyBlock->uHdr.Hdr.ulicbTotalChangesPending.QuadPart, sCbChangesPending.c_str());

                if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE) {
                    _tprintf(_T("GetDBTrans: DirtyBlock is start of split change\n"));
                }
                if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE) {
                    _tprintf(_T("GetDBTrans: DirtyBlock is part of split change\n"));
                }
                if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE) {
                    _tprintf(_T("GetDBTrans: DirtyBlock is end of split change\n"));
                }
                _tprintf(_T("GetDBTrans: Time Stamp Of Previous Change : SeqNumber = %x, TimeStamp : %#I64x \n"),
                        pDirtyBlock->uHdr.Hdr.ulPrevEndSequenceNumber, pDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp);
            }
            if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) {
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                    wprintf(L"GetDBTrans: Dirty block consists of data file \"%s\"\n", pDirtyBlock->uTagList.DataFile.FileName);
                }		    
                if (0 == PrintSVDstreamInfo(pDBTranData, pDirtyBlock)) {
                    if (NULL != pDirtyBlock) {
                        free(pDirtyBlock);
                        pDirtyBlock = NULL;
                    }
                    printf("PrintSVDstreamInfo returned error in data file mode\n");
                    return ERROR_GEN_FAILURE;
                }
            } else if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {			
                    wprintf(L"GetDBTrans: Dirty block consists of data in stream format.\n");
                }			
                if (0 == PrintSVDstreamInfo(pDBTranData, pDirtyBlock)) {
                    if (NULL != pDirtyBlock) {
                        free(pDirtyBlock);
                        pDirtyBlock = NULL;
                    }
                    printf("PrintSVDstreamInfo returned error in data mode\n");
                    return ERROR_GEN_FAILURE;
                }
            } else {
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                    SYSTEMTIME              FirstChangeLT, LastChangeLT;
                    GetTimeZoneInformation(&TimeZone);
                    FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                                &SystemTime);
                    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &FirstChangeLT);

                    FileTimeToSystemTime((FILETIME *)&pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                &SystemTime);
                    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LastChangeLT);

                    _tprintf(_T("GetDBTrans: Data source = %s\n"), DataSourceToString(pDirtyBlock->uHdr.Hdr.ulDataSource));
                    _tprintf(_T("GetDBTrans: TimeStamp of first change : SeqID = %#x, TimeStamp : %#I64x ,%02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                    pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber,
                                    pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                                    FirstChangeLT.wMonth, FirstChangeLT.wDay, FirstChangeLT.wYear,
                                    FirstChangeLT.wHour, FirstChangeLT.wMinute, FirstChangeLT.wSecond, FirstChangeLT.wMilliseconds);
                    _tprintf(_T("GetDBTrans: TimeStamp of last change : SeqID = %#x, TimeStamp : %#I64x ,%02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
                                    pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber,
                                    pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                    LastChangeLT.wMonth, LastChangeLT.wDay, LastChangeLT.wYear,
                                    LastChangeLT.wHour, LastChangeLT.wMinute, LastChangeLT.wSecond, LastChangeLT.wMilliseconds);
                     
                    _tprintf(_T("GetDBTrans: Printing change details\n"));
                    if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
                        _tprintf(_T("BufferArray = %#p\n"), pDirtyBlock->uHdr.Hdr.ppBufferArray);
                        _tprintf(_T("Max number of buffers = %#d\n"), pDirtyBlock->uHdr.Hdr.usMaxNumberOfBuffers);
                        _tprintf(_T("Number of buffers = %#d\n"), pDirtyBlock->uHdr.Hdr.usNumberOfBuffers);
                        if (NULL != pDirtyBlock->uHdr.Hdr.ppBufferArray) {
                            for (unsigned long i = 0; i < pDirtyBlock->uHdr.Hdr.usNumberOfBuffers; i++) {
                                _tprintf(_T("Index = %d, Address = %#p\n"), i, pDirtyBlock->uHdr.Hdr.ppBufferArray[i]);
                            }
                        }
                    }                        
                }
                pDBTranData->FCSequenceNumber.ulSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber;
                pDBTranData->FCSequenceNumber.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                pDBTranData->LCSequenceNumber.ulSequenceNumber = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber;
                pDBTranData->LCSequenceNumber.ullTimeStamp = pDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
                for (unsigned long i = 0; i < pDirtyBlock->uHdr.Hdr.cChanges; i++) {
                    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                        _tprintf(_T("Index = %d, Offset = %#I64x, Len = %#x\n"), i, pDirtyBlock->ChangeOffsetArray[i],
                                    pDirtyBlock->ChangeLengthArray[i]);
                    }
                    if (pDBTranData->sync) {
                        if (DataSync (pDBTranData, NULL, pDirtyBlock->ChangeOffsetArray[i], pDirtyBlock->ChangeLengthArray[i])) {
                            printf("Error in DataSync  Offset = %#I64x, Len = %#x\n", pDirtyBlock->ChangeOffsetArray[i], pDirtyBlock->ChangeLengthArray[i]);
                            if (NULL != pDirtyBlock) {
                                free(pDirtyBlock);
                                pDirtyBlock = NULL;
                            }
                            return ERROR_GEN_FAILURE;
                        }
                    }
                } 
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {		    
                    PSTREAM_REC_HDR_4B      pTag = NULL;
                    ULONG                   ulBytesProcessed = 0, ulMaxBytesToParse;
                    ULONG                   ulNumberOfUserDefinedTags = 0;
                    ulBytesProcessed = (ULONG) ((PUCHAR) &pDirtyBlock->uTagList.TagList.TagEndOfList - (PUCHAR) pDirtyBlock->uTagList.BufferForTags);
                    ulMaxBytesToParse = sizeof(pDirtyBlock->uTagList.BufferForTags) - ulBytesProcessed;
                    pTag = (PSTREAM_REC_HDR_4B) (pDirtyBlock->uTagList.BufferForTags + ulBytesProcessed);

                    while(pTag->usStreamRecType != STREAM_REC_TYPE_END_OF_TAG_LIST) {
                        ULONG ulLength = STREAM_REC_SIZE(pTag);
                        ULONG ulHeaderLength = (pTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ? sizeof(STREAM_REC_HDR_8B) : sizeof(STREAM_REC_HDR_4B);

                        if (pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG){
                            ULONG   ulLengthOfTag = 0;
                            PUCHAR   pTagData;

                            ulNumberOfUserDefinedTags++;
                            _tprintf(_T("User Defined Tag%ld:\n"), ulNumberOfUserDefinedTags);

                            ulLengthOfTag = *(PUSHORT)((PUCHAR)pTag + ulHeaderLength);
                            _tprintf(_T("Length of Tag:%d\n"), ulLengthOfTag);

                            pTagData = (PUCHAR)pTag + ulHeaderLength + sizeof(USHORT);
                            _tprintf(_T("Tag data is\n"));
                            PrintBinaryData(pTagData, ulLengthOfTag);
                        } else {
                            PrintUnidentifiedTag(pTag, ulMaxBytesToParse);
                        }

                        ulBytesProcessed += ulLength;
                        ulMaxBytesToParse -= ulLength;
                        pTag = (PSTREAM_REC_HDR_4B)(pDirtyBlock->uTagList.BufferForTags + ulBytesProcessed);
                    }

                    if (ulNumberOfUserDefinedTags > 0) {
                        _tprintf(_T("Total User Defined Tags Present: %ld\n"), ulNumberOfUserDefinedTags);
                    }
                }
            }
        }
        ULONG ret = ValidateTimeStampSeq(pDBTranData, pDirtyBlock, &tempPrevTSSequenceNumber);
        if (ret != ERROR_SUCCESS) {
            printf("Error in ValidateTimeStampSeq\n");
            return ret;
        }	
        if (0 != pDBTranData->ulWaitTimeBeforeCurrentTransCommit) {
            Sleep(pDBTranData->ulWaitTimeBeforeCurrentTransCommit);
        }

        if ((0 != pDBTranData->ulWaitTimeBeforeCurrentTransCommit) || (true == pDBTranData->CommitCurrentTrans))
        {
            CommitTrans.ulFlags = 0;
            CommitTrans.ulTransactionID.QuadPart = pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart;
            if ((pDBTranData->bResetResync) && ( pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)) {
                CommitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
            }

            if (0 == CommitTrans.ulTransactionID.QuadPart) {
                _tprintf(_T("This is not a transaction, Skipping commit\n")); 
            } else {
                bResult = DeviceIoControl(
                                hVFCtrlDevice,
                                (DWORD)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
                                &CommitTrans,
                                sizeof(COMMIT_TRANSACTION),
                                NULL,
                                0,
                                &dwReturn,  
                                NULL        // Overlapped
                                );
                if (bResult) {
                    if (true == pDBTranData->PrintDetails) {
                        wprintf(L"Commiting pending transaction on Volume %s Succeeded\n", pDBTranData->MountPoint);
                    }
                    PrevTSSequenceNumber.ulContinuationId = pDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO;
                    PrevTSSequenceNumber.ulSequenceNumber = pDBTranData->LCSequenceNumber.ulSequenceNumber;
                    PrevTSSequenceNumber.ullTimeStamp = pDBTranData->LCSequenceNumber.ullTimeStamp;
                } else {
                    wprintf(L"Commiting pending transaction on Volume %s Failed: %d\n", pDBTranData->MountPoint, GetLastError());
                    PrevTSSequenceNumber.ulContinuationId = tempPrevTSSequenceNumber.ulContinuationId;
                    PrevTSSequenceNumber.ulSequenceNumber = tempPrevTSSequenceNumber.ulSequenceNumber;
                    PrevTSSequenceNumber.ullTimeStamp = tempPrevTSSequenceNumber.ullTimeStamp;
                }
            }
        } else {
            AddPendingTransaction(&PendingTranListHead, 
                                  pDBTranData->VolumeGUID, 
                                  pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart,
                                  pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED ? true : false);
        }
        if ((pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE) && (pDBTranData->bWaitIfTSO)) {
            DWORD   dwWaitInMilliSeconds;
            if (pDBTranData->bPollForDirtyBlocks) {
                dwWaitInMilliSeconds = pDBTranData->ulPollIntervalInMilliSeconds;
                if (sCommandOptions.bVerbose) {
                    wprintf(L"GetDBTrans: Waiting to poll for dirty blocks for Volume %s\n", pDBTranData->MountPoint);
                }
            } else {    
                // Wait for event to be set.
                if (pDBTranData->ulRunTimeInMilliSeconds) {
                    DWORD   dwTimeRun = GetTickCount() - dwStartTime;
                    if (pDBTranData->ulRunTimeInMilliSeconds > dwTimeRun) {
                        dwWaitInMilliSeconds = pDBTranData->ulRunTimeInMilliSeconds - dwTimeRun;
                    } else {
                        // Dont come out of loop even if wait time spent is 0
                        if (pDBTranData->sync) {
                            dwWaitInMilliSeconds = 1;	    
                        } else {
                            // Time elapsed.
                            printf("Time elapsed\n");
                            break;
                        }
                    }
                } else {
                    dwWaitInMilliSeconds = INFINITE;
                }
                if (sCommandOptions.bVerbose) {
                    wprintf(L"GetDBTrans: Waiting on dirty block notify event for Volume %s\n", pDBTranData->MountPoint);
                }
            }

            DWORD   dwReturnValue;
            dwReturnValue = WaitForSingleObject(hEvent, dwWaitInMilliSeconds);
            // Dont come out of loop if sync flag is set
            if (!pDBTranData->sync) {

                if (WAIT_ABANDONED == dwReturnValue) {
                    _tprintf(_T("GetDBTrans: Wait on dirty block notify event is abandoned\n"));
                    break;
                }

                if (WAIT_TIMEOUT == dwReturnValue) {
                    if (sCommandOptions.bVerbose) {
                        _tprintf(_T("GetDBTrans: Wait on dirty block notify event has timed out\n"));
                    }
                    if (!pDBTranData->bPollForDirtyBlocks) {
                        _tprintf(_T("GetDBTrans: Wait on dirty block notify event has timed out and Polling is disabled\n"));
                        break;
                    }
                }

                if (STATUS_WAIT_0 == dwReturnValue) {
                    if (sCommandOptions.bVerbose) {
                        _tprintf(_T("GetDBTrans: Dirty block notify event signalled\n"));
                    }
                }
            }
        }

        // Check if we have to run again.
        bGetNextDirtyBlock = false;
        // If sync flag set then look if data already started then accordingly check tso count
        if (pDBTranData->sync) {
            if (iStartTsoCount) {
                if (WAIT_TSO_DATA > iTsoCount) {
                    bGetNextDirtyBlock = true;
                }
            } else {
                if (WAIT_TSO_NODATA > iTsoCount) {
                    bGetNextDirtyBlock = true;
                }
            }
        } else {
        if (pDBTranData->ulRunTimeInMilliSeconds) {
            if ((GetTickCount() - dwStartTime) < pDBTranData->ulRunTimeInMilliSeconds) {
                // We stil have time.
                if (pDBTranData->ulNumDirtyBlocks) {
                    // User specified number of dirty blocks too
                    if (ulNumDirtyBlocksReturned < pDBTranData->ulNumDirtyBlocks) {
                        bGetNextDirtyBlock = true;
                    } else {
                        printf("User specified number of dirty blocks drained\n");
                    }
                } else {
                    bGetNextDirtyBlock = true;
                }
            } else {
                printf("Time elapsed\n");
            }
        } else {
            if (pDBTranData->ulNumDirtyBlocks) {
                // User specified number of dirty blocks
                if (ulNumDirtyBlocksReturned < pDBTranData->ulNumDirtyBlocks) {
                    bGetNextDirtyBlock = true;
                } else {
                    printf("User specified number of dirty blocks drained\n");
                }
            }
        }
        }	
    } while (bGetNextDirtyBlock);

    if (NULL != pDirtyBlock) {
        free(pDirtyBlock);
        pDirtyBlock = NULL;
    }

    return ERROR_SUCCESS;
}

void
WaitForKeyword(PWAIT_FOR_KEYWORD_DATA pKeywordData)
{
    TCHAR   Input[81];

    while (0 != _tcsicmp(pKeywordData->Keyword, Input)) {
        memset(Input, 0, sizeof(TCHAR) * 81);
        _tprintf(_T("Waiting for keyword %s\n"), pKeywordData->Keyword);
        _tscanf(_T("%80s"), Input);
    }

    return;
}

void WaitForThread(PCOMMAND_STRUCT pCommand)
{
    map<TString, HANDLE>::iterator iter;
    ULONG ulNumberOfThreads = (ULONG) pCommand->data.WaitForThreadData.pvThreadId->size();
    ULONG ulThreadHandlesFound = 0;
    PHANDLE phThread = (PHANDLE)calloc(ulNumberOfThreads, sizeof(HANDLE));
    //Wait till all the threads get created
    for(ULONG i = 1; ulThreadHandlesFound < ulNumberOfThreads; i++)
    {
        WaitForSingleObject(sCommandOptions.hThreadMutex, INFINITE);
        TCHAR *ptr = (*pCommand->data.WaitForThreadData.pvThreadId)[i - 1];
        iter = sCommandOptions.ThreadMap.find((*pCommand->data.WaitForThreadData.pvThreadId)[i - 1]);

        if(iter != sCommandOptions.ThreadMap.end())
        {
            if(phThread[i - 1] != iter->second)
            {
                phThread[i - 1] = iter->second;
                ulThreadHandlesFound++;
            }
        }

        ReleaseMutex(sCommandOptions.hThreadMutex);

        if(i == ulNumberOfThreads && ulThreadHandlesFound < ulNumberOfThreads)
            Sleep(10);

        i = i % ulNumberOfThreads;
    }

    WaitForMultipleObjects(ulNumberOfThreads, phThread, TRUE, INFINITE);
    free(phThread);
}

void
CreateCmdThread(PCOMMAND_STRUCT pCommand, PLIST_ENTRY ListHead)
{
    size_t stIdLength = 0;
    DWORD dwThreadId = 0;
    HANDLE hThread = 0;

    PLIST_ENTRY NewHead = pCommand->data.CreateThreadData.ListHead;

    hThread = CreateThread( 
                NULL,              // default security attributes
                0,                 // use default stack size  
                ExecuteCommands,        // thread function 
                NewHead,             // argument to thread function 
                0,                 // use default creation flags 
                &dwThreadId);   // returns the thread identifier
    
    WaitForSingleObject(sCommandOptions.hThreadMutex, INFINITE);
    sCommandOptions.ThreadMap.insert(make_pair(pCommand->data.CreateThreadData.ThreadId, hThread));
    ReleaseMutex(sCommandOptions.hThreadMutex);
}

/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle PENDING_TRANSACTIONS
 *-------------------------------------------------------------------------------------------------
 */
PPENDING_TRANSACTION
AllocatePendingTransaction()
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = (PPENDING_TRANSACTION)malloc(sizeof(PENDING_TRANSACTION));

    if (NULL != pPendingTrans) {
        memset(pPendingTrans, 0, sizeof(PENDING_TRANSACTION));
    }

    return pPendingTrans;
}

void
DeallocatePendingTransaction(PPENDING_TRANSACTION pPendingTrans)
{
    if (NULL != pPendingTrans) {
        free (pPendingTrans);
    }
}

PPENDING_TRANSACTION
GetPendingTransactionRef(PLIST_ENTRY ListHead, PWCHAR VolumeGUID)
{
    PLIST_ENTRY             pListEntry;

    pListEntry = ListHead->Flink;
    while (pListEntry != ListHead) {
        if (0 == _wcsnicmp(VolumeGUID, ((PPENDING_TRANSACTION)pListEntry)->VolumeGUID, GUID_SIZE_IN_CHARS))
            return (PPENDING_TRANSACTION) pListEntry;

        pListEntry = pListEntry->Flink;
    }

    return NULL;
}

PPENDING_TRANSACTION
GetPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransactionRef(ListHead, VolumeGUID);

    if (NULL != pPendingTrans) {
        RemoveEntryList(&pPendingTrans->ListEntry);
    }

    return pPendingTrans;
}

ULONG
GetPendingTransactionID(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId, bool &bResetResync)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransaction(ListHead, VolumeGUID);
    if (NULL != pPendingTrans) {
        ullTransactionId = pPendingTrans->ullTransactionID;
        bResetResync = pPendingTrans->bResetResync;
        DeallocatePendingTransaction(pPendingTrans);
        return ERROR_SUCCESS;
    }

    ullTransactionId = 0;
    return ERROR_NOT_FOUND;
}

ULONG
GetPendingTransactionIDRef(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG &ullTransactionId)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransactionRef(ListHead, VolumeGUID);
    if (NULL != pPendingTrans) {
        ullTransactionId = pPendingTrans->ullTransactionID;
        return ERROR_SUCCESS;
    }

    ullTransactionId = 0;
    return ERROR_NOT_FOUND;
}

ULONG
AddPendingTransaction(PLIST_ENTRY ListHead, PWCHAR VolumeGUID, ULONGLONG ullTransactionId, bool bResetResync)
{
    PPENDING_TRANSACTION    pPendingTrans = NULL;

    pPendingTrans = GetPendingTransactionRef(ListHead, VolumeGUID);
    if (NULL == pPendingTrans) {
        pPendingTrans = AllocatePendingTransaction();
		inm_memcpy_s(pPendingTrans->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        pPendingTrans->ullTransactionID = ullTransactionId;
        pPendingTrans->bResetResync = bResetResync;

        InsertHeadList(ListHead, &pPendingTrans->ListEntry);

        return ERROR_SUCCESS;
    } else {
        // There is a transaction for this GUID
        if (ullTransactionId == pPendingTrans->ullTransactionID) {
            // TransactionID matched so lets just leave it
            pPendingTrans->bResetResync = bResetResync;
            return ERROR_SUCCESS;
        } else {
            wprintf(L"Old TransactionID %#I64X for GUID %.*s does not match new transcation ID %#I64X\n",
                pPendingTrans->ullTransactionID, GUID_SIZE_IN_CHARS, VolumeGUID, ullTransactionId);
            pPendingTrans->ullTransactionID = ullTransactionId;
            pPendingTrans->bResetResync = bResetResync;
            return ERROR_ALREADY_EXISTS;
        }
    }
}

/**
 * Routines to add and retreive events for volume
 * */
bool
CreateNewDBWaitEventForVolume(wstring& VolumeGUID, HANDLE &hEvent)
{
    bool    bCreated = false;
    hEvent = NULL;

    for (std::vector<CDBWaitEvent *>::const_iterator iter = DBWaitEventVector.begin(); iter != DBWaitEventVector.end(); iter++) {
        if ((*iter)->IsVolumeGUID(VolumeGUID)) {
            hEvent = (*iter)->GetEventHandle();
            return false;
        }
    }

    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    CDBWaitEvent *DBWaitEvent = new CDBWaitEvent(hEvent, VolumeGUID);
    if (NULL == DBWaitEvent) {
        _tprintf(_T("Failed to allocate CDBWaitEvent\n"));
        CloseHandle(hEvent);
        return false;
    }

    DBWaitEventVector.push_back(DBWaitEvent);

    return true;
}

/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle SERVICE_START Notifications
 *-------------------------------------------------------------------------------------------------
 */
HANDLE
SendServiceStartNotification(HANDLE hVFCtrlDevice, PSERVICE_START_DATA pServiceStart)
{
    PSERVICE_START_NOTIFY_DATA  pServiceStartNotifyData = (PSERVICE_START_NOTIFY_DATA)INVALID_HANDLE_VALUE;
    PROCESS_START_NOTIFY_INPUT  InputData = {0};
    BOOL    bResult;
    DWORD   dwReturn;

    pServiceStartNotifyData = (PSERVICE_START_NOTIFY_DATA)malloc(sizeof(SERVICE_START_NOTIFY_DATA));
    if (NULL == pServiceStartNotifyData)
        return INVALID_HANDLE_VALUE;

    memset(pServiceStartNotifyData, 0, sizeof(SERVICE_START_NOTIFY_DATA));

    pServiceStartNotifyData->hVFCtrlDevice = hVFCtrlDevice;
    pServiceStartNotifyData->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == pServiceStartNotifyData->hEvent)
    {
        _tprintf(_T("SendServiceStartNotification: CreateEvent Failed with Error = %#x\n"), GetLastError());
        goto cleanup;
    }
    
    pServiceStartNotifyData->Overlapped.hEvent = pServiceStartNotifyData->hEvent;

    if (!pServiceStart->bDisableDataFiles) {
        InputData.ulFlags |= PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE;
    }

    _tprintf(_T("Sending Process start notify IRP\n"));
    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_PROCESS_START_NOTIFY,
                     &InputData,
                     sizeof(PROCESS_START_NOTIFY_INPUT),
                     NULL,
                     0,
                     &dwReturn, 
                     &pServiceStartNotifyData->Overlapped        // Overlapped
                     ); 
    if (0 == bResult) {
        if (ERROR_IO_PENDING != GetLastError())
        {
            _tprintf(_T("SendServiceStartNotification: DeviceIoControl failed with error = %d\n"), GetLastError());
            goto cleanup;
        }
    }

    pServiceStartNotifyData->ulFlags |= SERVICE_START_ND_FLAGS_COMMAND_SENT;

    return (HANDLE)pServiceStartNotifyData;
cleanup:
    if (NULL == pServiceStartNotifyData)
        return INVALID_HANDLE_VALUE;

    if (NULL != pServiceStartNotifyData->hEvent)
        CloseHandle(pServiceStartNotifyData->hEvent);

    free(pServiceStartNotifyData);

    return INVALID_HANDLE_VALUE;
}

bool
HasServiceStartNotificationCompleted(HANDLE hServiceStartNotification)
{
    PSERVICE_START_NOTIFY_DATA   pServiceStartNotifyData;

    if (INVALID_HANDLE_VALUE == hServiceStartNotification)
        return false;

    pServiceStartNotifyData = (PSERVICE_START_NOTIFY_DATA)hServiceStartNotification;

    if ((pServiceStartNotifyData->ulFlags & SERVICE_START_ND_FLAGS_COMMAND_SENT) &&
        ( 0 == (pServiceStartNotifyData->ulFlags & SERVICE_START_ND_FLAGS_COMMAND_COMPLETED)))
    {
        if (HasOverlappedIoCompleted(&pServiceStartNotifyData->Overlapped)) {
            pServiceStartNotifyData->ulFlags |= SERVICE_START_ND_FLAGS_COMMAND_COMPLETED;
        }
    }

    if (pServiceStartNotifyData->ulFlags & SERVICE_START_ND_FLAGS_COMMAND_COMPLETED)
        return true;

    return false;
}

void
CloseServiceStartNotification(HANDLE hServiceStartNotification)
{
    PSERVICE_START_NOTIFY_DATA   pServiceStartNotifyData;

    if (INVALID_HANDLE_VALUE == hServiceStartNotification)
        return;

    pServiceStartNotifyData = (PSERVICE_START_NOTIFY_DATA)hServiceStartNotification;

    if ((pServiceStartNotifyData->ulFlags & SERVICE_START_ND_FLAGS_COMMAND_SENT) &&
        ( 0 == (pServiceStartNotifyData->ulFlags & SERVICE_START_ND_FLAGS_COMMAND_COMPLETED)))
    {
        if (FALSE == HasOverlappedIoCompleted(&pServiceStartNotifyData->Overlapped)) {
            CancelIo(pServiceStartNotifyData->hVFCtrlDevice);
            pServiceStartNotifyData->ulFlags |= SERVICE_START_ND_FLAGS_COMMAND_COMPLETED;
        }
    }

    if (NULL != pServiceStartNotifyData->hEvent)
        CloseHandle(pServiceStartNotifyData->hEvent);

    free(pServiceStartNotifyData);

    return;
}

/*
 *-------------------------------------------------------------------------------------------------
 * Routines to handle SHUTDOWN Notifications
 *-------------------------------------------------------------------------------------------------
 */
HANDLE
SendShutdownNotification(HANDLE hVFCtrlDevice)
{
    PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)INVALID_HANDLE_VALUE;
    SHUTDOWN_NOTIFY_INPUT   InputData = {0};
    BOOL    bResult;
    DWORD   dwReturn;

    pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)malloc(sizeof(SHUTDOWN_NOTIFY_DATA));
    if (NULL == pShutdownNotifyData)
        return INVALID_HANDLE_VALUE;

    memset(pShutdownNotifyData, 0, sizeof(SHUTDOWN_NOTIFY_DATA));

    pShutdownNotifyData->hVFCtrlDevice = hVFCtrlDevice;
    pShutdownNotifyData->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == pShutdownNotifyData->hEvent)
    {
        _tprintf(_T("SendShutdownNotification: CreateEvent Failed with Error = %#x\n"), GetLastError());
        goto cleanup;
    }
    
    pShutdownNotifyData->Overlapped.hEvent = pShutdownNotifyData->hEvent;
    _tprintf(_T("Sending Shutdown Notify IRP\n"));
    InputData.ulFlags = 0;

    if (!sCommandOptions.bDisableDataFiltering) {
        InputData.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;
    }

    if (!sCommandOptions.bDisableDataFiles) {
        InputData.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES;
    }

    bResult = DeviceIoControl(
                     hVFCtrlDevice,
                     (DWORD)IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
                     &InputData,
                     sizeof(SHUTDOWN_NOTIFY_INPUT),
                     NULL,
                     0,
                     &dwReturn, 
                     &pShutdownNotifyData->Overlapped        // Overlapped
                     ); 
    if (0 == bResult) {
        if (ERROR_IO_PENDING != GetLastError())
        {
            _tprintf(_T("SendShutdownNotification: DeviceIoControl failed with error = %d\n"), GetLastError());
            goto cleanup;
        }
    }

    pShutdownNotifyData->ulFlags |= SND_FLAGS_COMMAND_SENT;

    return (HANDLE)pShutdownNotifyData;
cleanup:
    if (NULL == pShutdownNotifyData)
        return INVALID_HANDLE_VALUE;

    if (NULL != pShutdownNotifyData->hEvent)
        CloseHandle(pShutdownNotifyData->hEvent);

    free(pShutdownNotifyData);

    return INVALID_HANDLE_VALUE;
}

bool
HasShutdownNotificationCompleted(HANDLE hShutdownNotification)
{
    PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData;

    if (INVALID_HANDLE_VALUE == hShutdownNotification)
        return false;

    pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)hShutdownNotification;

    if ((pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_SENT) &&
        ( 0 == (pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_COMPLETED)))
    {
        if (HasOverlappedIoCompleted(&pShutdownNotifyData->Overlapped)) {
            pShutdownNotifyData->ulFlags |= SND_FLAGS_COMMAND_COMPLETED;
        }
    }

    if (pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_COMPLETED)
        return true;

    return false;
}

void
CloseShutdownNotification(HANDLE hShutdownNotification)
{
    PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData;

    if (INVALID_HANDLE_VALUE == hShutdownNotification)
        return;

    pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)hShutdownNotification;

    if ((pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_SENT) &&
        ( 0 == (pShutdownNotifyData->ulFlags & SND_FLAGS_COMMAND_COMPLETED)))
    {
        if (FALSE == HasOverlappedIoCompleted(&pShutdownNotifyData->Overlapped)) {
            CancelIo(pShutdownNotifyData->hVFCtrlDevice);
            pShutdownNotifyData->ulFlags |= SND_FLAGS_COMMAND_COMPLETED;
        }
    }

    if (NULL != pShutdownNotifyData->hEvent)
        CloseHandle(pShutdownNotifyData->hEvent);

    free(pShutdownNotifyData);

    return;
}

ULONG
SetDataToDiskSizeLimit(HANDLE hVFCtrlDevice, PSET_DATA_TO_DISK_SIZE_LIMIT_DATA pSetDataToDiskSizeLimit)
{
    SET_DATA_TO_DISK_SIZE_LIMIT_INPUT     Input;
    SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT    Output;
    DWORD   dwReturn = 0;
    BOOL    bResult;

    memset(&Input, 0, sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_INPUT));

    if ((0 == (pSetDataToDiskSizeLimit->ulFlags & SET_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID)) &&
        (0 == pSetDataToDiskSizeLimit->DefaultDataToDiskSizeLimit) &&
        (0 == pSetDataToDiskSizeLimit->DataToDiskSizeLimitForAllVolumes))
    {
        _tprintf(_T("SetDataToDiskSizeLimit: Flag is not set, default and all volumes value are zero\n"));
        return ERROR_SUCCESS;
    }

    if (pSetDataToDiskSizeLimit->ulFlags & SET_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID) {
        // User requested value to be set for a specific volume
		inm_memcpy_s(&Input.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pSetDataToDiskSizeLimit->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        Input.ulFlags |= SET_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID;
        Input.ulDataToDiskSizeLimit = pSetDataToDiskSizeLimit->ulDataToDiskSizeLimit;
    }

    if (pSetDataToDiskSizeLimit->DefaultDataToDiskSizeLimit) {
        Input.ulDefaultDataToDiskSizeLimit = pSetDataToDiskSizeLimit->DefaultDataToDiskSizeLimit;
    }

    if (pSetDataToDiskSizeLimit->DataToDiskSizeLimitForAllVolumes) {
        Input.ulDataToDiskSizeLimitForAllVolumes = pSetDataToDiskSizeLimit->DataToDiskSizeLimitForAllVolumes;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_DATA_SIZE_TO_DISK_LIMIT,
                    &Input,
                    sizeof(Input),
                    &Output,
                    sizeof(Output),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

    if (sCommandOptions.bVerbose) {
        if (bResult) {
            wprintf(L"SetDataToDiskSizeLimit: Setting disk size limit IOCTL for volumes Succeeded\n");
        } else {
            wprintf(L"SetDataToDiskSizeLimit: Setting disk size limit for volumes Failed: %d\n", GetLastError());
        }
    }

    if (sizeof(Output) != dwReturn) {
        _tprintf(_T("SetDataToDiskSizeLimit: dwReturn(%#x) did not match SET_DATA_FILE_THREAD_PRIORITY_OUTPUT size(%#x)\n"), 
                 dwReturn, sizeof(SET_DATA_FILE_THREAD_PRIORITY_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (Output.ulErrorInSettingForVolume) {
        _tprintf(_T("SetDataToDiskSizeLimit: Failed in setting disk size limit for volume %s with error %#x\n"),
                    pSetDataToDiskSizeLimit->MountPoint , Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingDefault) {
        _tprintf(_T("SetDataToDiskSizeLimit: Failed in setting default disk size limit to %#x with error %#x\n"),
                    Input.ulDefaultDataToDiskSizeLimit, Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingForAllVolumes) {
        _tprintf(_T("SetDataToDiskSizeLimit: Failed in setting disk size limit for all volumes to %#x with error %#x\n"),
                    Input.ulDataToDiskSizeLimitForAllVolumes, Output.ulErrorInSettingForVolume);
    }

    return ERROR_SUCCESS;
}



ULONG
SetMinFreeDataToDiskSizeLimit(HANDLE hVFCtrlDevice, PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA pSetMinFreeDataToDiskSizeLimit)
{
    SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT     Input;
    SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT    Output;
    DWORD   dwReturn = 0;
    BOOL    bResult;

    memset(&Input, 0, sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT));

    if ((0 == (pSetMinFreeDataToDiskSizeLimit->ulFlags & SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID)) &&
        (0 == pSetMinFreeDataToDiskSizeLimit->DefaultMinFreeDataToDiskSizeLimit) &&
        (0 == pSetMinFreeDataToDiskSizeLimit->MinFreeDataToDiskSizeLimitForAllVolumes))
    {
        _tprintf(_T("SetMinFreeDataToDiskSizeLimit: Flag is not set, default and all volumes value are zero\n"));
        return ERROR_SUCCESS;
    }

    if (pSetMinFreeDataToDiskSizeLimit->ulFlags & SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID) {
        // User requested value to be set for a specific volume
		inm_memcpy_s(&Input.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pSetMinFreeDataToDiskSizeLimit->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        Input.ulFlags |= SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_DATA_VALID_GUID;
        Input.ulMinFreeDataToDiskSizeLimit = pSetMinFreeDataToDiskSizeLimit->ulMinFreeDataToDiskSizeLimit;
    }

    if (pSetMinFreeDataToDiskSizeLimit->DefaultMinFreeDataToDiskSizeLimit) {
        Input.ulDefaultMinFreeDataToDiskSizeLimit = pSetMinFreeDataToDiskSizeLimit->DefaultMinFreeDataToDiskSizeLimit;
    }

    if (pSetMinFreeDataToDiskSizeLimit->MinFreeDataToDiskSizeLimitForAllVolumes) {
        Input.ulMinFreeDataToDiskSizeLimitForAllVolumes= pSetMinFreeDataToDiskSizeLimit->MinFreeDataToDiskSizeLimitForAllVolumes;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_MIN_FREE_DATA_SIZE_TO_DISK_LIMIT,
                    &Input,
                    sizeof(Input),
                    &Output,
                    sizeof(Output),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

    if (sCommandOptions.bVerbose) {
        if (bResult) {
            wprintf(L"SetMinFreeDataToDiskSizeLimit: Setting disk size limit IOCTL for volumes Succeeded\n");
        } else {
            wprintf(L"SetMinFreeDataToDiskSizeLimit: Setting disk size limit for volumes Failed: %d\n", GetLastError());
        }
    }

    if (sizeof(Output) != dwReturn) {
        _tprintf(_T("SetDataToDiskSizeLimit: dwReturn(%#x) did not match SET_DATA_TO_MIN_FREE_DISK_SIZE_LIMIT_OUTPUT size(%#x)\n"), 
                 dwReturn, sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (Output.ulErrorInSettingForVolume) {
        _tprintf(_T("SetMinFreeDataToDiskSizeLimit: Failed in setting min free data to disk size limit for volume %s with error %#x\n"),
                    pSetMinFreeDataToDiskSizeLimit->MountPoint , Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingDefault) {
        _tprintf(_T("SetMinFreeDataToDiskSizeLimit: Failed in setting default disk size limit to %#x with error %#x\n"),
                    Input.ulDefaultMinFreeDataToDiskSizeLimit, Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingForAllVolumes) {
        _tprintf(_T("SetMinFreeDataToDiskSizeLimit: Failed in setting disk size limit for all volumes to %#x with error %#x\n"),
                  Input.ulMinFreeDataToDiskSizeLimitForAllVolumes, Output.ulErrorInSettingForVolume);
    }

    return ERROR_SUCCESS;
}

ULONG
SetDataNotifySizeLimit(HANDLE hVFCtrlDevice, PSET_DATA_NOTIFY_SIZE_LIMIT_DATA pSetDataNotifySizeLimit)
{
    SET_DATA_NOTIFY_SIZE_LIMIT_INPUT     Input;
    SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT    Output;
    DWORD   dwReturn = 0;
    BOOL    bResult;

    memset(&Input, 0, sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_INPUT));

    if ((0 == (pSetDataNotifySizeLimit->ulFlags & SET_DATA_NOTIFY_SIZE_LIMIT_DATA_VALID_GUID)) &&
        (0 == pSetDataNotifySizeLimit->ulDefaultLimit) &&
        (0 == pSetDataNotifySizeLimit->ulLimitForAllVolumes))
    {
        _tprintf(_T("SetDataNotifySizeLimit: Flag is not set, default and all volumes value are zero\n"));
        return ERROR_SUCCESS;
    }

    if (pSetDataNotifySizeLimit->ulFlags & SET_DATA_NOTIFY_SIZE_LIMIT_DATA_VALID_GUID) {
        // User requested value to be set for a specific volume
		inm_memcpy_s(&Input.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pSetDataNotifySizeLimit->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        Input.ulFlags |= SET_DATA_NOTIFY_SIZE_LIMIT_FLAGS_VALID_GUID;
        Input.ulLimit = pSetDataNotifySizeLimit->ulLimit;
    }

    if (pSetDataNotifySizeLimit->ulDefaultLimit) {
        Input.ulDefaultLimit = pSetDataNotifySizeLimit->ulDefaultLimit;
    }

    if (pSetDataNotifySizeLimit->ulLimitForAllVolumes) {
        Input.ulLimitForAllVolumes = pSetDataNotifySizeLimit->ulLimitForAllVolumes;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_DATA_NOTIFY_LIMIT,
                    &Input,
                    sizeof(Input),
                    &Output,
                    sizeof(Output),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

    if (sCommandOptions.bVerbose) {
        if (bResult) {
            wprintf(L"SetDataNotifySizeLimit: Setting disk size limit IOCTL for volumes Succeeded\n");
        } else {
            wprintf(L"SetDataNotifySizeLimit: Setting disk size limit for volumes Failed: %d\n", GetLastError());
        }
    }

    if (sizeof(Output) != dwReturn) {
        _tprintf(_T("SetDataNotifySizeLimit: dwReturn(%#x) did not match SET_DATA_FILE_THREAD_PRIORITY_OUTPUT size(%#x)\n"), 
                 dwReturn, sizeof(SET_DATA_FILE_THREAD_PRIORITY_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (Output.ulErrorInSettingForVolume) {
        _tprintf(_T("SetDataNotifySizeLimit: Failed in setting disk size limit for volume %s with error %#x\n"),
                    pSetDataNotifySizeLimit->MountPoint , Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingDefault) {
        _tprintf(_T("SetDataNotifySizeLimit: Failed in setting default disk size limit to %#x with error %#x\n"),
                    Input.ulDefaultLimit, Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingForAllVolumes) {
        _tprintf(_T("SetDataNotifySizeLimit: Failed in setting disk size limit for all volumes to %#x with error %#x\n"),
                    Input.ulLimitForAllVolumes, Output.ulErrorInSettingForVolume);
    }

    return ERROR_SUCCESS;
}

ULONG
SetDataFileThreadPriority(
    HANDLE hVFCtrlDevice, 
    PSET_DATA_FILE_THREAD_PRIORITY_DATA pSetDataFileThreadPriority
    )
{
    SET_DATA_FILE_THREAD_PRIORITY_INPUT     Input;
    SET_DATA_FILE_THREAD_PRIORITY_OUTPUT    Output;
    DWORD   dwReturn = 0;
    BOOL    bResult;

    memset(&Input, 0, sizeof(SET_DATA_FILE_THREAD_PRIORITY_INPUT));

    if ((0 == (pSetDataFileThreadPriority->ulFlags & SET_DATA_FILE_THREAD_PRIORITY_DATA_VALID_GUID)) &&
        (0 == pSetDataFileThreadPriority->DefaultPriority) &&
        (0 == pSetDataFileThreadPriority->PriorityForAllVolumes))
    {
        _tprintf(_T("SetDataFileThreadPriority: Flag is not set, and default and all volumes value are zero\n"));
        return ERROR_SUCCESS;
    }

    if (pSetDataFileThreadPriority->ulFlags & SET_DATA_FILE_THREAD_PRIORITY_DATA_VALID_GUID) {
        // User requested value to be set for a specific volume
		inm_memcpy_s(&Input.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pSetDataFileThreadPriority->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        Input.ulFlags |= SET_DATA_FILE_THREAD_PRIORITY_FLAGS_VALID_GUID;
        Input.ulThreadPriority = pSetDataFileThreadPriority->ulPriority;
    }

    if (pSetDataFileThreadPriority->DefaultPriority) {
        Input.ulDefaultThreadPriority = pSetDataFileThreadPriority->DefaultPriority;
    }

    if (pSetDataFileThreadPriority->PriorityForAllVolumes) {
        Input.ulThreadPriorityForAllVolumes = pSetDataFileThreadPriority->PriorityForAllVolumes;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_DATA_FILE_THREAD_PRIORITY,
                    &Input,
                    sizeof(Input),
                    &Output,
                    sizeof(Output),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

    if (sCommandOptions.bVerbose) {
        if (bResult) {
            wprintf(L"SetDataFileThreadPriority: Setting data file thread priority IOCTL for volumes Succeeded\n");
        } else {
            wprintf(L"SetDataFileThreadPriority: Setting data file thread priority for volumes Failed: %d\n", GetLastError());
        }
    }

    if (sizeof(Output) != dwReturn) {
        _tprintf(_T("SetDataFileThreadPriority: dwReturn(%#x) did not match SET_DATA_FILE_THREAD_PRIORITY_OUTPUT size(%#x)\n"), 
                 dwReturn, sizeof(SET_DATA_FILE_THREAD_PRIORITY_OUTPUT));
        return ERROR_GEN_FAILURE;
    }

    if (Output.ulErrorInSettingForVolume) {
        _tprintf(_T("SetDataFileThreadPriority: Failed in setting priority for volume %s with error %#x\n"),
            pSetDataFileThreadPriority->MountPoint, Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingDefault) {
        _tprintf(_T("SetDataFileThreadPriority: Failed in setting default priority to %#x with error %#x\n"),
                    Input.ulDefaultThreadPriority, Output.ulErrorInSettingForVolume);
    }

    if (Output.ulErrorInSettingForAllVolumes) {
        _tprintf(_T("SetDataFileThreadPriority: Failed in setting priority for all volumes to %#x with error %#x\n"),
                    Input.ulThreadPriorityForAllVolumes, Output.ulErrorInSettingForVolume);
    }

    return ERROR_SUCCESS;
}

ULONG
SetWorkerThreadPriority(
    HANDLE hVFCtrlDevice, 
    PSET_WORKER_THREAD_PRIORITY_DATA pSetWorkerThreadPriority
    )
{
    SET_WORKER_THREAD_PRIORITY     Input, Output;
    DWORD   dwReturn = 0;
    BOOL    bResult;

    memset(&Input, 0, sizeof(SET_WORKER_THREAD_PRIORITY));

    if (pSetWorkerThreadPriority->ulFlags & SET_WORKER_THREAD_PRIORITY_DATA_VALID_PRIORITY) {
        Input.ulFlags |= SET_WORKER_THREAD_PRIORITY_VALID_PRIORITY;
        Input.ulThreadPriority = pSetWorkerThreadPriority->ulPriority;
    }

    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_SET_WORKER_THREAD_PRIORITY,
                    &Input,
                    sizeof(Input),
                    &Output,
                    sizeof(Output),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

    if (sCommandOptions.bVerbose) {
        if (bResult) {
            wprintf(L"SetWorkerThreadPriority: Setting worker thread priority IOCTL succeeded\n");
        } else {
            wprintf(L"SetWorkerThreadPriority: Setting worker thread priority failed: %d\n", GetLastError());
        }
    }

    if (sizeof(Output) != dwReturn) {
        _tprintf(_T("SetWorkerThreadPriority: dwReturn(%#x) did not match SET_WORKER_THREAD_PRIORITY size(%#x)\n"), 
                 dwReturn, sizeof(SET_WORKER_THREAD_PRIORITY));
        return ERROR_GEN_FAILURE;
    }

    if (Output.ulFlags & SET_WORKER_THREAD_PRIORITY_VALID_PRIORITY) {
        _tprintf(_T("SetWorkerThreadPriority: Current worker thread priority is %#d\n"), Output.ulThreadPriority);
    }

    return ERROR_SUCCESS;
}

// This function is added to work in mount point case also
void GetUniqueVolumeName(PTCHAR UniqueVolumeName, PWCHAR VolumeGUID, size_t UniqueVolNameSize)
{   
	inm_memcpy_s(UniqueVolumeName, UniqueVolNameSize, VOLUME_NAME_PREFIX, _tcslen(VOLUME_NAME_PREFIX) * sizeof(TCHAR));
	UniqueVolNameSize -= _tcslen(VOLUME_NAME_PREFIX) * sizeof(TCHAR);
	inm_memcpy_s(UniqueVolumeName + _tcslen(VOLUME_NAME_PREFIX), UniqueVolNameSize, VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
	UniqueVolNameSize -= GUID_SIZE_IN_CHARS * sizeof(WCHAR);
	inm_memcpy_s(UniqueVolumeName + GUID_SIZE_IN_CHARS + _tcslen(VOLUME_NAME_PREFIX), UniqueVolNameSize, VOLUME_NAME_POSTFIX,
        _tcslen(VOLUME_NAME_POSTFIX) * sizeof(TCHAR));          
	UniqueVolNameSize -= _tcslen(VOLUME_NAME_POSTFIX) * sizeof(TCHAR);
	UniqueVolumeName[GUID_SIZE_IN_CHARS + _tcslen(VOLUME_NAME_PREFIX) + _tcslen( VOLUME_NAME_POSTFIX )] = _T('\0');
}

/* Name :- GetSync
 * Input Parameter :- Handle of Involflt, Struct contaning info of source and dest
 * Return :- On Success zero
 * Describtion :- It sync the source and target on the same host. It does sync step1, step2 and will then maintain differential. 
 * 		  When writes get stop it will flush the source and complete the sync with target and make read only to source and dest
 * 		  Waits for exit keyword*/

int GetSync (HANDLE hVFCtrlDevice, PGET_DB_TRANS_DATA pDBTranData) {
    HANDLE hServiceStartNotification = INVALID_HANDLE_VALUE;
    HANDLE hShutdownNotification = INVALID_HANDLE_VALUE;
    DWORD dwErrorVal = 0;
    DWORD dwReturn = 0;
    BOOL bRetVal = 0;
    ULONG   ulRet = 0;
    BOOL bIsWow64 = FALSE;
    BOOL bDestVolLock = 0, bSrcVolLock = 0;
    ULONG   ulFlagsSrc, ulFlagsDest;
    VOLUME_FLAGS_DATA  SrcVolumeFlagsData;
    VOLUME_FLAGS_DATA  DestVolumeFlagsData;
    START_FILTERING_DATA StartFilteringData;
    STOP_FILTERING_DATA StopFilteringData;
    SERVICE_START_DATA ServiceStart;
    BOOL bSrcReadOnlyMade = 0, bFilterStarted = 0, bDestReadOnlyMade = 0, bFilterStop = 0;
    int iReturn = 1;
    bool skip_step_one;
    
    skip_step_one = pDBTranData->skip_step_one;
    // Initialization
    //

    memset(&StartFilteringData, 0, sizeof(START_FILTERING_DATA));
    memset(&StopFilteringData, 0, sizeof(STOP_FILTERING_DATA));
    memset(&SrcVolumeFlagsData, 0, sizeof(VOLUME_FLAGS_DATA));
    memset(&DestVolumeFlagsData, 0, sizeof(VOLUME_FLAGS_DATA));
    memset(&ServiceStart, 0, sizeof(SERVICE_START_DATA));
    

    pDBTranData->BufferSize = READ_BUFFER_SIZE;
    pDBTranData->chpBuff = NULL;
    pDBTranData->hFileSource = INVALID_HANDLE_VALUE;
    pDBTranData->hFileDest = INVALID_HANDLE_VALUE;

	inm_memcpy_s(SrcVolumeFlagsData.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
	inm_memcpy_s(StopFilteringData.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
	inm_memcpy_s(StartFilteringData.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
	inm_memcpy_s(DestVolumeFlagsData.VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR), pDBTranData->DestVolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

    StartFilteringData.MountPoint = pDBTranData->MountPoint;
    StopFilteringData.MountPoint = pDBTranData->MountPoint;
    SrcVolumeFlagsData.MountPoint = pDBTranData->MountPoint;
    DestVolumeFlagsData.MountPoint = pDBTranData->DestMountPoint;
    ServiceStart.bDisableDataFiles = FALSE;
	
    if (!RunningOn64bitVersion(bIsWow64)) {
        _tprintf(_T("Execute getdbtrans Failed as RunningOn64bitVersion() failed\n"));
        goto ERR1;
    }

    if (bIsWow64) {
        _tprintf(_T("DrvUtil is running on 64 bit Machine\n"));
    }

    // Get Src and Dest volume flags
    bRetVal = DeviceIoControl(
            hVFCtrlDevice,
            (DWORD)IOCTL_INMAGE_GET_VOLUME_FLAGS,
            pDBTranData->VolumeGUID,
            sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
            &ulFlagsSrc,
            sizeof(ulFlagsSrc),
            &dwReturn,  
            NULL        // Overlapped
            );
    if (!(bRetVal && (dwReturn >=  sizeof(ulFlagsSrc)))) {
        if (!bRetVal) {
            _tprintf(_T("GetVolumeFlags failed on Src with error %d\n"), GetLastError());
        } else {
            _tprintf(_T("GetVolumeFlags failed on Src: dwReturn(%d) < sizeof(ULONG)(%d)\n"), dwReturn, sizeof(ulFlagsSrc));
        }
        goto ERR1;
    }
    bRetVal = DeviceIoControl(
            hVFCtrlDevice,
            (DWORD)IOCTL_INMAGE_GET_VOLUME_FLAGS,
            pDBTranData->DestVolumeGUID,
            sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
            &ulFlagsDest,
            sizeof(ulFlagsDest),
            &dwReturn,  
            NULL        // Overlapped
            );
    if (!(bRetVal && (dwReturn >=  sizeof(ulFlagsDest)))) {
        if (!bRetVal) {
            _tprintf(_T("GetVolumeFlags failed on Dest with error %d\n"), GetLastError());
        } else {
            _tprintf(_T("GetVolumeFlags failed on Dest: dwReturn(%d) < sizeof(ULONG)(%d)\n"), dwReturn, sizeof(ulFlagsSrc));
        }
        goto ERR1;
    }

    // If Filtering is ON on target then return error.
    if (!(ulFlagsDest & VSF_FILTERING_STOPPED)) {
        _tprintf(_T("Error Destination Drive Filter is ON\n"));
        goto ERR1;
    }

    if (ulFlagsDest & VSF_READ_ONLY) {
        _tprintf(_T("Error Destination Drive is READ ONLY\n"));
        goto ERR1;
    }

    if (ulFlagsSrc & VSF_READ_ONLY) {
        _tprintf(_T("\tSource VSF_READ_ONLY\n"));
    }

    // If filtering is off on source turn it on
    if (ulFlagsSrc & VSF_FILTERING_STOPPED) {
        _tprintf(_T("\tSource VSF_FILTERING_STOPPED\n"));
        ulRet = StartFiltering(hVFCtrlDevice, &StartFilteringData);
        if (ERROR_SUCCESS != ulRet) {
            _tprintf(_T("Error in starting filter on Src\n"));
            goto ERR1;
        }
        _tprintf(_T("Filtering Started on source\n"));
        bFilterStarted = 1;
    } else {
        _tprintf(_T("Filtering already ON at source\n"));
    }

    // Open the handle on source volume
    pDBTranData->hFileSource = CreateFile ((LPCTSTR)pDBTranData->tcSourceName,
            GENERIC_READ,           // Open for reading
            FILE_SHARE_READ,        // Do not share
            NULL,                   // No security
            OPEN_EXISTING,          // Existing file only
            FILE_FLAG_NO_BUFFERING,  // Normal file
            NULL);                  // No template file
    if (INVALID_HANDLE_VALUE == pDBTranData->hFileSource) {
        dwErrorVal = GetLastError();
        _tprintf(_T("Error in opening the Src %s Errorcode 0x%x\n"), 
                pDBTranData->tcSourceName, dwErrorVal);
        goto ERR1;
    }

    // Open the handle on dest volume
    pDBTranData->hFileDest = CreateFile ((LPCTSTR)pDBTranData->tcDestName,
            GENERIC_READ | GENERIC_WRITE,           // Open for reading
            FILE_SHARE_READ | FILE_SHARE_WRITE,        // Do not share
            NULL,                   // No security
            OPEN_EXISTING,          // Existing file only
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,  // Normal file
            NULL);                  // No template file
    if (INVALID_HANDLE_VALUE == pDBTranData->hFileDest) {
        dwErrorVal = GetLastError();
        _tprintf(_T("Error in opening the Dest %s Errorcode 0x%x\n"),
                pDBTranData->tcDestName, dwErrorVal);
        goto ERR1;
    }

    // Lock and unmount the dest volume
    bRetVal = DeviceIoControl(
            pDBTranData->hFileDest,
            FSCTL_LOCK_VOLUME,
            NULL,                        // lpInBuffer
            0,                           // nInBufferSize
            NULL,                        // lpOutBuffer
            0,                           // nOutBufferSize
            &dwReturn,   // number of bytes returned
            0  // OVERLAPPED structure
            );
    if (0 == bRetVal) {
        dwErrorVal = GetLastError();
        _tprintf(_T("Error in locking the Dest %s Errorcode 0x%x\n"), pDBTranData->tcDestName, dwErrorVal);
        goto ERR1;
    }
    bDestVolLock = 1;

    bRetVal = DeviceIoControl(
            pDBTranData->hFileDest,
            FSCTL_DISMOUNT_VOLUME,
            NULL,                        // lpInBuffer
            0,                           // nInBufferSize
            NULL,                        // lpOutBuffer
            0,                           // nOutBufferSize
            &dwReturn,   // number of bytes returned
            0  // OVERLAPPED structure
            );
    if (0 == bRetVal) {
        dwErrorVal = GetLastError();
        _tprintf(_T("Error in Dismounting the Dest %s Errorcode 0x%x\n"), pDBTranData->tcDestName, dwErrorVal);
        goto ERR1;
    }

    // Allocate the buffer
    pDBTranData->chpBuff = (char *) malloc (READ_BUFFER_SIZE);
    if (NULL == pDBTranData->chpBuff) {
        _tprintf(_T("Memory could not allocate\n"));
        goto ERR1;
    }

    // Send shutdown and service start notification
    hShutdownNotification = SendShutdownNotification(hVFCtrlDevice);
    if (INVALID_HANDLE_VALUE == hShutdownNotification){
        _tprintf(_T("ExecuteCommand: Aborting processing of commands1\n"));
        goto ERR1;
    }

    if (true == HasShutdownNotificationCompleted(hShutdownNotification)) {
        _tprintf(_T("Overlapped IO has completed\n"));
        goto ERR1;
    }

    hServiceStartNotification = SendServiceStartNotification(hVFCtrlDevice, &ServiceStart);
    if (INVALID_HANDLE_VALUE == hServiceStartNotification) {
        _tprintf(_T("ExecuteCommand: Aborting processing of commands\n"));
        goto ERR1;
    }

    // Do Clear diffs, Sync Step1, Sync Step2 and make the src and dest volume sync
    // pDBTranData->resync_req is set in parsing by default
    do {
        if (!skip_step_one) {
        // Clear the diffs
        bRetVal = DeviceIoControl(
            hVFCtrlDevice,
            (DWORD)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
             pDBTranData->VolumeGUID,
             sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
             NULL,
             0,
             &dwReturn, 
             NULL        // Overlapped
        );
        if (bRetVal) {
            if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                _tprintf(_T("Returned Success from Clear Bitmap DeviceIoControl call for volume %s\n"), pDBTranData->MountPoint);
            }
        } else {
            _tprintf(_T("Returned Failed from Clear Bitmap DeviceIoControl call for volume %s %d\n"), pDBTranData->MountPoint, GetLastError());
            goto ERR1;
        }
        pDBTranData->resync_req = 0;
	// Sync Step1
	_tprintf(_T("Sync Step1 starting\n"));
        if (DataSync(pDBTranData, NULL, 0, -1) ) {
            printf("Error in DataSync\n");
            goto ERR1;
        }
        printf("Sync Step1 done\n");
        } else {
            printf("Sync Step1 Skipped\n");
            skip_step_one = 0;
            Sleep(2000);
        }

	//Sync Step2
        bool bSyncStep2 = 0;
        while (1) {

            // GetDb and write the data to disk and then commit
            if (bIsWow64) {
                if (ERROR_SUCCESS != GetDBTransV2(hVFCtrlDevice, pDBTranData)) {
                    _tprintf(_T("GetDBTrans failed\n"));
                    goto ERR1;
                }
            } else {
                if (ERROR_SUCCESS != GetDBTrans(hVFCtrlDevice, pDBTranData)) {
                    _tprintf(_T("GetDBTrans failed\n"));
                    goto ERR1;
                }
            }

            if (pDBTranData->resync_req) {
                _tprintf(_T("Resync required flag is set.\n"));		    
                break;
            }

            // Flag is set after src is flushed by unmounting it.
            if (bSyncStep2) {
                break;
            }
            if (!bSrcVolLock) {
                // Lock the src and the unmount it so that data get flushed
                bRetVal = DeviceIoControl(
                        pDBTranData->hFileSource,
                        FSCTL_LOCK_VOLUME,
                        NULL,                        // lpInBuffer
                        0,                           // nInBufferSize
                        NULL,                        // lpOutBuffer
                        0,                           // nOutBufferSize
                        &dwReturn,   // number of bytes returned
                        0  // OVERLAPPED structure
                        );
                if (0 == bRetVal) {
                    dwErrorVal = GetLastError();
                    _tprintf(_T("Error in locking the Src %s Errorcode 0x%x\n Close the drive\n"), pDBTranData->tcSourceName, dwErrorVal);
                    // If failes continue may be some write operation on source is continuing
                    continue;
                }
                bSrcVolLock = 1;
                bRetVal = DeviceIoControl(
                        pDBTranData->hFileSource,
                        FSCTL_DISMOUNT_VOLUME,
                        NULL,                        // lpInBuffer
                        0,                           // nInBufferSize
                        NULL,                        // lpOutBuffer
                        0,                           // nOutBufferSize
                        &dwReturn,   // number of bytes returned
                        0  // OVERLAPPED structure
                        );
                if (0 == bRetVal) {
                    dwErrorVal = GetLastError();
                    _tprintf(_T("Error in Dismounting the Src %s Errorcode 0x%x\n"), pDBTranData->tcSourceName, dwErrorVal);
                    goto ERR1;
                }
            }
            // Get all dirty blocks sync.
            bSyncStep2 = 1;
        }
    } while (pDBTranData->resync_req);
    _tprintf(_T("Sync Step2 done\n"));

    // Make Dest read only
    if (!(ulFlagsDest & VSF_READ_ONLY)) {    
        DestVolumeFlagsData.eBitOperation = ecBitOpSet;
        DestVolumeFlagsData.ulVolumeFlags = 0;
        DestVolumeFlagsData.ulVolumeFlags = DestVolumeFlagsData.ulVolumeFlags | VOLUME_FLAG_READ_ONLY;
        ulRet = SetVolumeFlags(hVFCtrlDevice, &DestVolumeFlagsData);
        if (ERROR_SUCCESS != ulRet) {
            _tprintf(_T("Error while making Dest volume read only\n"));
            goto ERR1;            
	} 
        bDestReadOnlyMade = 1;
    }   

    // Make src read only so stop the service and make it read only
    if ((!(ulFlagsSrc & VSF_READ_ONLY)) && (!pDBTranData->skip_step_one)) {
        ulRet = StopFiltering(hVFCtrlDevice, &StopFilteringData);
        if (ERROR_SUCCESS != ulRet) {
            printf("Error in stoping filter while making read only\n");
            goto ERR1;
        }
        bFilterStop = 1;        
        SrcVolumeFlagsData.eBitOperation = ecBitOpSet;
        SrcVolumeFlagsData.ulVolumeFlags = 0;
        SrcVolumeFlagsData.ulVolumeFlags = SrcVolumeFlagsData.ulVolumeFlags | VOLUME_FLAG_READ_ONLY;
        ulRet = SetVolumeFlags(hVFCtrlDevice, &SrcVolumeFlagsData);
        if (ERROR_SUCCESS != ulRet) {
            _tprintf(_T("Error while making src volume read only\n"));
            goto ERR1;
        }
        bSrcReadOnlyMade = 1;
    }

    iReturn = 0;
ERR1:
    // Unlock Src and dest
    if (bSrcVolLock) {
        bRetVal = DeviceIoControl(
                pDBTranData->hFileSource,
                FSCTL_UNLOCK_VOLUME,
                NULL,                        // lpInBuffer
                0,                           // nInBufferSize
                NULL,                        // lpOutBuffer
                0,                           // nOutBufferSize
                &dwReturn,   // number of bytes returned
                0  // OVERLAPPED structure
                );
        if (0 == bRetVal) {
            dwErrorVal = GetLastError();
            _tprintf(_T("Error in unlocking the Souce %s Errorcode 0x%x\n"), pDBTranData->tcSourceName, dwErrorVal);
        }
    }
    if (bDestVolLock) {
        bRetVal = DeviceIoControl(
                pDBTranData->hFileDest,
                FSCTL_UNLOCK_VOLUME,
                NULL,                        // lpInBuffer
                0,                           // nInBufferSize
                NULL,                        // lpOutBuffer
                0,                           // nOutBufferSize
                &dwReturn,   // number of bytes returned
                0  // OVERLAPPED structure
                );
        if (0 == bRetVal) {
            dwErrorVal = GetLastError();
            _tprintf(_T("Error in unlocking the Dest %s Errorcode 0x%x\n"), pDBTranData->tcDestName, dwErrorVal);        
        }
    }

    
    if (INVALID_HANDLE_VALUE != pDBTranData->hFileDest) {
        CloseHandle(pDBTranData->hFileDest);
    }
    if (INVALID_HANDLE_VALUE != pDBTranData->hFileSource) {
        CloseHandle(pDBTranData->hFileSource);
    }
    if (NULL != pDBTranData->chpBuff) {
        free(pDBTranData->chpBuff);
    }

    //Wait for user input. User can do DI
    if (0 == iReturn) {
        WAIT_FOR_KEYWORD_DATA KeywordData;
		_tcscpy_s(KeywordData.Keyword, TCHAR_ARRAY_SIZE(KeywordData.Keyword), _T("exit"));
        WaitForKeyword(&KeywordData);
    }

    // Revert the original state of dest
    if (bDestReadOnlyMade) {
        DestVolumeFlagsData.eBitOperation = ecBitOpReset;
        DestVolumeFlagsData.ulVolumeFlags = 0;
        DestVolumeFlagsData.ulVolumeFlags = DestVolumeFlagsData.ulVolumeFlags | VOLUME_FLAG_READ_ONLY;
        ulRet = SetVolumeFlags(hVFCtrlDevice, &DestVolumeFlagsData);
        if (ERROR_SUCCESS != ulRet) {
            _tprintf(_T("Error while reset read only flag on Dest\n"));
            iReturn = 1;
        }
    }

    // Revert the original state of Src
    if (bSrcReadOnlyMade) {
        if (!(ulFlagsSrc & VSF_FILTERING_STOPPED)) {
            ulRet = StartFiltering(hVFCtrlDevice, &StartFilteringData);
            if (ERROR_SUCCESS != ulRet) {
                _tprintf(_T("Error in starting filter on source before reseting read only flag\n"));
                iReturn = 1;
            }
        }
        SrcVolumeFlagsData.eBitOperation = ecBitOpReset;
        SrcVolumeFlagsData.ulVolumeFlags = 0;
        SrcVolumeFlagsData.ulVolumeFlags = SrcVolumeFlagsData.ulVolumeFlags | VOLUME_FLAG_READ_ONLY;
        ulRet = SetVolumeFlags(hVFCtrlDevice, &SrcVolumeFlagsData);
        if (ERROR_SUCCESS != ulRet) {
            _tprintf(_T("Error while reseting read only flag on Source\n"));
            iReturn = 1;
        }
    } else {
        if (bFilterStarted) {
            if (!bFilterStop) {
                ulRet = StopFiltering(hVFCtrlDevice, &StopFilteringData);
                if (ERROR_SUCCESS != ulRet) {
                    _tprintf(_T("Error in stopping the filter on Source\n"));
                    iReturn = 1;
                }
            }		
        } else if (bFilterStop) {
            ulRet = StartFiltering(hVFCtrlDevice, &StartFilteringData);
            if (ERROR_SUCCESS != ulRet) {
                _tprintf(_T("Error in starting the filter on Source\n"));
                iReturn = 1;
            }
        }
    }

    if (INVALID_HANDLE_VALUE != hServiceStartNotification) {
        CloseServiceStartNotification(hServiceStartNotification);
    }
    if (INVALID_HANDLE_VALUE != hShutdownNotification) {
        CloseServiceStartNotification(hShutdownNotification);
    }
    if (0 == iReturn) {
        printf("Success\n");
    } else {
        printf("Failed\n");
    }
    return iReturn;
}

/*
Description : This routine set up replication between src disk and target VHD.Routine wait for 65 sec on finding TSO 
              otherwise event is set on data notify
 Parameters : Handle of InDskFlt, Struct contaning info of source and dest
 Return     : Exits on seeing Resync flag otherwise runs in a loop 
 */
int GetSync_v2(HANDLE hVFCtrlDevice, PGET_DB_TRANS_DATA pDBTranData) {
    HANDLE hServiceStartNotification = INVALID_HANDLE_VALUE;
    HANDLE hShutdownNotification = INVALID_HANDLE_VALUE;
    DWORD dwErrorVal = 0;
    DWORD dwReturn = 0;
    BOOL bRetVal = 0;
    ULONG   ulRet = 0;
    BOOL bIsWow64 = FALSE;
    BOOL bDestVolLock = 0, bSrcVolLock = 0;
    ULONG   ulFlagsSrc;
    VOLUME_FLAGS_DATA  SrcVolumeFlagsData;
    //VOLUME_FLAGS_DATA  DestVolumeFlagsData;
    START_FILTERING_DATA StartFilteringData;
    STOP_FILTERING_DATA StopFilteringData;
    SERVICE_START_DATA ServiceStart;
    BOOL bSrcReadOnlyMade = 0, bFilterStarted = 0, bDestReadOnlyMade = 0, bFilterStop = 0;
    int iReturn = 1;
    bool skip_step_one;
    DWORD TotalSectors, BytesPerSector;

    skip_step_one = pDBTranData->skip_step_one;

    GetSCSICapacity(pDBTranData->tcSourceName, TotalSectors, BytesPerSector);

    memset(&StartFilteringData, 0, sizeof(START_FILTERING_DATA));
    memset(&StopFilteringData, 0, sizeof(STOP_FILTERING_DATA));
    memset(&SrcVolumeFlagsData, 0, sizeof(VOLUME_FLAGS_DATA));
    //memset(&DestVolumeFlagsData, 0, sizeof(VOLUME_FLAGS_DATA));
    memset(&ServiceStart, 0, sizeof(SERVICE_START_DATA));


    pDBTranData->BufferSize = READ_BUFFER_SIZE;
    pDBTranData->chpBuff = NULL;
    pDBTranData->hFileSource = INVALID_HANDLE_VALUE;
    pDBTranData->hFileDest = INVALID_HANDLE_VALUE;

    StringCchPrintfW(SrcVolumeFlagsData.VolumeGUID, GUID_SIZE_IN_CHARS,  L"%s", pDBTranData->VolumeGUID);
    StringCchPrintfW(StopFilteringData.VolumeGUID, GUID_SIZE_IN_CHARS, L"%s", pDBTranData->VolumeGUID);
    StringCchPrintfW(StartFilteringData.VolumeGUID, GUID_SIZE_IN_CHARS, L"%s", pDBTranData->VolumeGUID);

    StartFilteringData.MountPoint = pDBTranData->MountPoint;
    StopFilteringData.MountPoint = pDBTranData->MountPoint;
    SrcVolumeFlagsData.MountPoint = pDBTranData->MountPoint;
    //DestVolumeFlagsData.MountPoint = pDBTranData->DestMountPoint;
    ServiceStart.bDisableDataFiles = FALSE;

    if (!RunningOn64bitVersion(bIsWow64)) {
        _tprintf(_T("Execute getdbtrans Failed as RunningOn64bitVersion() failed\n"));
        goto ERR1;
    }

    if (bIsWow64) {
        _tprintf(_T("DrvUtil is running on 64 bit Machine\n"));
    }

    // Get Src and Dest volume flags
    bRetVal = DeviceIoControl(
        hVFCtrlDevice,
        (DWORD)IOCTL_INMAGE_GET_VOLUME_FLAGS,
        pDBTranData->VolumeGUID,
        sizeof(WCHAR)* GUID_SIZE_IN_CHARS,
        &ulFlagsSrc,
        sizeof(ulFlagsSrc),
        &dwReturn,
        NULL        // Overlapped
        );
    if (!(bRetVal && (dwReturn >= sizeof(ulFlagsSrc)))) {
        if (!bRetVal) {
            _tprintf(_T("GetVolumeFlags failed on Src with error %d\n"), GetLastError());
        }
        else {
            _tprintf(_T("GetVolumeFlags failed on Src: dwReturn(%d) < sizeof(ULONG)(%d)\n"), dwReturn, sizeof(ulFlagsSrc));
        }
        goto ERR1;
    }
    else {
        _tprintf(_T("GetVolumeFlags succeeded on src disk\n"));
    }


    if (ulFlagsSrc & VSF_READ_ONLY) {
        _tprintf(_T("\tSource VSF_READ_ONLY\n"));
    }

    // If filtering is off on source turn it on
    if (ulFlagsSrc & VSF_FILTERING_STOPPED) {
        _tprintf(_T("\tSource VSF_FILTERING_STOPPED\n"));
        ulRet = StartFiltering_v2(hVFCtrlDevice, &StartFilteringData);
        if (ERROR_SUCCESS != ulRet) {
            _tprintf(_T("Error in starting filter on Src\n"));
            goto ERR1;
        }
        _tprintf(_T("Filtering Started on source disk\n"));
        bFilterStarted = 1;
    }
    else {
        _tprintf(_T("Filtering is already started on the src disk\n"));
    }

    // Allocate the buffer
    pDBTranData->chpBuff = (char *)malloc(READ_BUFFER_SIZE);
    if (NULL == pDBTranData->chpBuff) {
        _tprintf(_T("Memory could not allocate\n"));
        goto ERR1;
    }
    if (pDBTranData->serviceshutdownnotify) {
    // Send shutdown and service start notification
    hShutdownNotification = SendShutdownNotification(hVFCtrlDevice);
    if (INVALID_HANDLE_VALUE == hShutdownNotification){
        _tprintf(_T("ExecuteCommand: Aborting processing of commands1\n"));
        goto ERR1;
    }

    if (true == HasShutdownNotificationCompleted(hShutdownNotification)) {
        _tprintf(_T("Overlapped IO has completed\n"));
        goto ERR1;
    }
 }
    if (pDBTranData->serviceshutdownnotify) {
        hServiceStartNotification = SendServiceStartNotification(hVFCtrlDevice, &ServiceStart);
        if (INVALID_HANDLE_VALUE == hServiceStartNotification) {
            _tprintf(_T("ExecuteCommand: Aborting processing of commands\n"));
            goto ERR1;
        }
    }
    // Do Clear diffs, Sync Step1, Sync Step2 and make the src and dest volume sync
    // pDBTranData->resync_req is set in parsing by default
    do {
        if (!skip_step_one) {
            
                // Clear the diffs
                bRetVal = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
                        pDBTranData->VolumeGUID,
                        sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                        NULL,
                        0,
                        &dwReturn, 
                        NULL        // Overlapped
                );
                if (bRetVal) {
                    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                        _tprintf(_T("Returned Success from Clear Bitmap DeviceIoControl call for volume %s\n"), pDBTranData->VolumeGUID);
                    }
                } else {
                    _tprintf(_T("Returned Failed from Clear Bitmap DeviceIoControl call for volume %s %d\n"), pDBTranData->VolumeGUID, GetLastError());
                    goto ERR1;
                }
                pDBTranData->resync_req = 0;
            // Sync Step1
            _tprintf(_T("+++++++++++++++++++++\n"));
            _tprintf(_T("Sync Step1 starting\n"));

            INT64 SectorsReadWritten = 0;

            SectorsReadWritten = RDiskSectorsWriteFile(pDBTranData->tcSourceName, 0, TotalSectors, pDBTranData->tcDestName,0);

            if( TotalSectors == (DWORD)SectorsReadWritten)
                {
                    wprintf(L"\nClone Drive to File Completed.Total Sectors written is 0x%x", TotalSectors);
                }else{
                    wprintf(L"\nClone Drive Completed but Lesser Sectors written than Actual- Could be Fatal! Aborting.");
                    goto ERR1;
                }
    
                printf(" \nSync Step1 done; waiting for 5 seconds\n");
                Sleep(5000);

        } else //end of skip_step_one true;
        {
            printf("Sync Step1 Skipped; Waiting for 5 seconds for \n");
            skip_step_one = 0;
            Sleep(5000);
        }
        _tprintf(_T("+++++++++++++++++++++\n"));
        _tprintf(_T("Enter : Diff sync\n"));
    
    //Open the handle on source disk
    pDBTranData->hFileSource = CreateFile ((LPCTSTR)pDBTranData->tcSourceName,
            GENERIC_READ,           // Open for reading
            FILE_SHARE_READ | FILE_SHARE_WRITE,        // Do not share
            NULL,                   // No security
            OPEN_EXISTING,          // Existing file only
            FILE_FLAG_NO_BUFFERING,  // Normal file
            NULL);                  // No template file
    if (INVALID_HANDLE_VALUE == pDBTranData->hFileSource) {
        dwErrorVal = GetLastError();
        _tprintf(_T("Error in opening the Src %s Errorcode 0x%x\n"), 
                pDBTranData->tcSourceName, dwErrorVal);
        goto ERR1;
    }
    
    // Open the handle on dest disk
    pDBTranData->hFileDest = CreateFile ((LPCTSTR)pDBTranData->tcDestName,
            GENERIC_READ | GENERIC_WRITE,           // Open for reading
            FILE_SHARE_READ | FILE_SHARE_WRITE,        // Do not share
            NULL,                   // No security
            OPEN_EXISTING,          // Existing file only
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,  // Normal file
            NULL);                  // No template file
    if (INVALID_HANDLE_VALUE == pDBTranData->hFileDest) {
        dwErrorVal = GetLastError();
        _tprintf(_T("Error in opening the Dest %s Errorcode 0x%x\n"),
                pDBTranData->tcDestName, dwErrorVal);
        goto ERR1;
    }
        while (1) {
            // GetDb and write the data to disk and then commit
            if (bIsWow64) {
                if (ERROR_SUCCESS != GetDBTransV2(hVFCtrlDevice, pDBTranData)) {
                    _tprintf(_T("GetDBTransV2 Either failed or exits on Tag\n"));
                    goto ERR1;
                }
            } else {
                if (ERROR_SUCCESS != GetDBTrans(hVFCtrlDevice, pDBTranData)) {
                    _tprintf(_T("GetDBTrans Either failed\n"));
                    goto ERR1;
                }
            }

            if (pDBTranData->resync_req) {
                _tprintf(_T("\n\nResync required flag is set.\n"));
                break;
            }
        }
    } while (0);
    _tprintf(_T("Exit : Diff sync\n"));

     iReturn = 0;
ERR1:
    
    if (NULL != pDBTranData->chpBuff) {
        free(pDBTranData->chpBuff);
    }

    if (INVALID_HANDLE_VALUE != hServiceStartNotification) {
        CloseServiceStartNotification(hServiceStartNotification);
    }
    if (INVALID_HANDLE_VALUE != hShutdownNotification) {
        CloseServiceStartNotification(hShutdownNotification);
    }

    if (INVALID_HANDLE_VALUE != pDBTranData->hFileSource) {
        printf("Closing Handle on source\n");
        CloseHandle(pDBTranData->hFileSource);
    }

    if (INVALID_HANDLE_VALUE != pDBTranData->hFileDest) {
        printf("Closing Handle on target\n");
        CloseHandle(pDBTranData->hFileDest);
    }

    if (0 == iReturn) {
        printf("Success\n");
    } else {
        printf("Failed\n");
    }
    return iReturn;
}
//DBF -END

//DBF -START
int ShowLastError()
{
	LPVOID lpMsgBuf;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
    wprintf(L"ERROR - %ws\n", (LPTSTR) lpMsgBuf);
	LocalFree( lpMsgBuf );
	return(0);
}
//DBF -END
/* Name :- DataSync
 * Input Parameter :- Struct contaning info of source and dest, SVD stream, offset, lenght
 * Return :- On Success zero
 * Describtion :- It sync the source and target on the same host. It does sync step1, step2 and will then maintain differential. 
 * 		  When writes get stop it will flush the source and complete the sync with target and make read only to source and dest
 * 		  Waits for exit keyword*/

bool DataSync (PGET_DB_TRANS_DATA  pDBTranData, PSOURCESTREAM pSourceStream, SV_LONGLONG ByteOffset, SV_LONGLONG length) {
    DWORD dwBytesRead = 0, dwBytesWritten = 0;
    DWORD dwRetVal = 0;
    DWORD dwErrorVal = 0;
    DWORD dwBytesReturned = 0;
    BOOL bRetVal = 0, bDestVolLock = 0;
    LARGE_INTEGER lSkipSrc;
    LARGE_INTEGER lSkipDest;
    SV_LONGLONG lWriteCount = 0;
    SV_LONGLONG lToRead = 0;
    size_t StreamOffset = 0;
    bool bSourceDisk = 1;
    bool bRet = 1;
    if (NULL == pDBTranData) {
        _tprintf(_T("Error in pDBTranData\n"));
        return bRet;
    }
    // Either input will be disk or stream. If disk then stream will be null
    if (NULL != pSourceStream) {
        bSourceDisk = 0;
    }

    // Length can be -1 if want to do sync step1. But it cannot be when source is not disk
    if ((length < 0) && (!bSourceDisk)) {
        _tprintf(_T("Error in pSourceStream\n"));
        return bRet;
    }

    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
        if (bSourceDisk) {
            _tprintf(_T("Source mode is disk\n"));
        } else {
            _tprintf(_T("Source mode is stream\n"));
	}
        if (length < 0) {
	   _tprintf(_T("++++++++++++++++++\n"));
	    _tprintf(_T("Sync Step 1\n"));
        } else {
       	    _tprintf(_T("Differential Sync\n"));
        }
    }
   
    if (bSourceDisk) {
        // if src is disk then seek	    
        lSkipSrc.QuadPart = ByteOffset;
        lSkipSrc.LowPart = SetFilePointer(
              pDBTranData->hFileSource,
              lSkipSrc.LowPart,
              &lSkipSrc.HighPart,
              FILE_BEGIN);
        if (INVALID_SET_FILE_POINTER == lSkipSrc.LowPart) {
            dwErrorVal = GetLastError();
            if (NO_ERROR != dwErrorVal) {
                 _tprintf(_T("Error in SetFilePointer Source %s Errorcode 0x%x\n"), pDBTranData->tcSourceName, dwErrorVal);
                return bRet;
            }
        }
    } else {
        StreamOffset = pSourceStream->StreamOffset;
    }

    // Seek dest volume
    lSkipDest.QuadPart = ByteOffset;
    lSkipDest.LowPart = SetFilePointer(
            pDBTranData->hFileDest,
            lSkipDest.LowPart,
            &lSkipDest.HighPart,
            FILE_BEGIN);

    if (INVALID_SET_FILE_POINTER == lSkipDest.LowPart) {
        dwErrorVal = GetLastError();
        if (NO_ERROR != dwErrorVal) {
             _tprintf(_T("Error in SetFilePointer Dest %s Errorcode 0x%x\n"), pDBTranData->tcDestName, dwErrorVal);
            return bRet;
	}
    }
    // Set success flag
    bRet = 0;
    lToRead = pDBTranData->BufferSize;
    do {
	if (length >= 0) {
            if (lWriteCount >= length) {
                break;
	    }
	    lToRead = length - lWriteCount;
            if (pDBTranData->BufferSize < lToRead) {
                lToRead = pDBTranData->BufferSize;
	    }
        }
        // Read from disk else from stream
        if (bSourceDisk) {
            dwBytesRead = 0;
            if (0 == ReadFile(pDBTranData->hFileSource, pDBTranData->chpBuff, (DWORD)lToRead, &dwBytesRead, NULL)) {
                // If More data was expected then return error else in sync step1 all the src must have been read
                if (length >= 0) {
                    _tprintf(_T("More data was expected to read. Read File return 0\n"));
                    bRet = 1;
	        }
                break;
            }
            if (dwBytesRead == 0) {                
                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
                     _tprintf(_T("BytesRead 0\n"));
                }
                // If More data was expected then return error else in sync step1 all the src must have been read
                if (length >= 0) {
                    printf("BytesRead 0 lToRead  %#I64x lWriteCount %#I64x BufferSize %u ByteOffset %#I64x length %#I64x\n", lToRead, lWriteCount, pDBTranData->BufferSize, ByteOffset, length);
                    bRet = 1;
                }
                break;
            }
            if (dwBytesRead != lToRead) {
                if (length >= 0) {
                    printf("Does not read as much expected. dwBytesRead %u lToRead  %#I64x lWriteCount %#I64x BufferSize %u ByteOffset %#I64x length %#I64x\n", dwBytesRead, lToRead, lWriteCount, pDBTranData->BufferSize, ByteOffset, length);
                    bRet = 1;
                }
                break;		    
            }
        } else {
            pSourceStream->DataStream->copy(StreamOffset, (size_t)lToRead, pDBTranData->chpBuff);
            dwBytesRead = (DWORD)lToRead;
	    StreamOffset = StreamOffset + dwBytesRead;
        }

	// Write to dest
        dwBytesWritten = 0;
        if (0 == WriteFile(pDBTranData->hFileDest, pDBTranData->chpBuff, dwBytesRead, &dwBytesWritten, NULL)) {
            dwErrorVal = GetLastError();
			ShowLastError();
             _tprintf(_T("Error in Writing Dest %s Errorcode 0x%x Bytesread 0x%x Byteswritten 0x%x\n"), pDBTranData->tcDestName, dwErrorVal, dwBytesRead, dwBytesWritten);
            bRet = 1;
            break;
	}
        if ((dwBytesWritten == 0) || (dwBytesRead != dwBytesWritten)) {
            _tprintf(_T("BytesWritten 0\n"));
            bRet = 1;
            break;
        }
        lWriteCount = lWriteCount + dwBytesWritten;
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
            _tprintf(_T("."));
        }
    } while(1);
    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {
        printf ("\n");
        if (length >= 0) {
            printf("Number of bytes expected to write 0x%x", length);
            printf(" Actual written 0x%x\n", lWriteCount);
        } else {
            printf("Number of blocks written written 0x%x of size 0x%x\n", lWriteCount, lToRead);
	}
    }
    return bRet;
}

void PrintDriverStats(PVOLUME_STATS_DATA  pVolumeStatsData, DWORD dwReturn, int IsOlderDriver)
{
    BOOL bIsWow64 = FALSE;
    DWORD   ulNumVolumes;
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;

    // Get HyperVisor Model and manufacturer
    GetWMIComputerSystem();

    //Get Filter keys
    GetFilterKeys(DiskClass, UpperFilters, "DiskUpperFilter");
    GetFilterKeys(VolumeClass, UpperFilters, "VolumeUpperFilter");
    GetFilterKeys(VolumeClass, LowerFilters, "VolumeLowerFilter");


    _tprintf(_T(",Total Number of Devices = %lu\n"), pVolumeStatsData->ulTotalVolumes);
    _tprintf(_T("Number of Devices Data Returned = %lu\n"), pVolumeStatsData->ulVolumesReturned);

    _tprintf(_T("Service State is %s\n"), ServiceStateString(pVolumeStatsData->eServiceState));
    _tprintf(_T("Number of Dirty blocks allocated = %ld\n"), pVolumeStatsData->lDirtyBlocksAllocated);
    _tprintf(_T("Number of Change nodes allocated = %ld\n"), pVolumeStatsData->lChangeNodesAllocated);
    _tprintf(_T("Number of Change blocks allocated = %ld\n"), pVolumeStatsData->lChangeBlocksAllocated);
    _tprintf(_T("Number of Bitmap work items allocated = %ld\n"), pVolumeStatsData->lBitmapWorkItemsAllocated);
    _tprintf(_T("Number of memory allocation failures = %lu\n"), pVolumeStatsData->ulNumMemoryAllocFailures);
    PrintStr("Non paged memory allocated = %s\n", (ULONG)pVolumeStatsData->lNonPagedMemoryAllocated);
    _tprintf(_T("Non paged memory limit = %ldm\n"), pVolumeStatsData->ulNonPagedMemoryLimitInMB);

    if (pVolumeStatsData->liNonPagedLimitReachedTSforFirstTime.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStatsData->liNonPagedLimitReachedTSforFirstTime.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Non paged memory limit is reached for first time at : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);

    }
    if (pVolumeStatsData->liNonPagedLimitReachedTSforLastTime.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStatsData->liNonPagedLimitReachedTSforLastTime.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Non paged memory limit is reached for last time at : %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if ((pVolumeStatsData->LastShutdownMarker > Uninitialized) && (pVolumeStatsData->LastShutdownMarker <= DirtyShutdown)) {
        if (pVolumeStatsData->LastShutdownMarker == DirtyShutdown)
            _tprintf(_T("Last Shutdown was NOT Clean\n"));
        else if (pVolumeStatsData->LastShutdownMarker == CleanShutdown)
            _tprintf(_T("Last Shutdown was Clean\n"));
        else
            _tprintf(_T("LastShutdownMarker returned by driver is corrupted\n"));

    }
    if (pVolumeStatsData->PersistentRegistryCreated == TRUE){
        _tprintf(_T("Persistent Registry Created on Boot\n"));
    }
    else if (pVolumeStatsData->PersistentRegistryCreated == FALSE){
        _tprintf(_T("Persistent Registry Read on Boot\n"));
    }
    else {
        _tprintf(_T("PersistentRegistryCreated Flag corrupted\n"));
    }

    if (!IsOlderDriver) {
        printf("System Boot Counter : %lu\n", pVolumeStatsData->ulCommonBootCounter);
        printf("Number of Protected disks : %d\n", pVolumeStatsData->ulNumProtectedDisk);
    }

    if (pVolumeStatsData->ullPersistedTimeStampAfterBoot) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStatsData->ullPersistedTimeStampAfterBoot, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T("Last Persistent Time Stamp written before last Boot: %#I64x, %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
            pVolumeStatsData->ullPersistedTimeStampAfterBoot, LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (RunningOn64bitVersion(bIsWow64)) {
        if (bIsWow64) {
            _tprintf(_T("Last Persistent Seq No written before last Boot: %#I64x\n"), pVolumeStatsData->ullPersistedSequenceNumberAfterBoot);
        }
    }

    PrintStr("Data Pool size allocated = %s\n", pVolumeStatsData->ullDataPoolSizeAllocated);
    PrintStr("Data Pool size free = %s\n", pVolumeStatsData->ullDataPoolSizeFree);
    PrintStr("Data pool size free and locked = %s\n", pVolumeStatsData->ullDataPoolSizeFreeAndLocked);
    printf("LockedDataBlockCounter = %d\n", pVolumeStatsData->LockedDataBlockCounter);
    printf("MaxLockedDataBlockCounter = %d\n", pVolumeStatsData->MaxLockedDataBlockCounter);
    printf("MappedDataBlockCounter = %d\n", pVolumeStatsData->MappedDataBlockCounter);
    printf("MaxMappedDataBlockCounter = %d\n", pVolumeStatsData->MaxMappedDataBlockCounter);
    printf("GlobalPageNodesAllocated = %lu\n", pVolumeStatsData->ulNumOfAllocPageNodes);
    printf("GlobalPageNodesAvailable = %lu\n", pVolumeStatsData->ulNumOfFreePageNodes);
    if (pVolumeStatsData->AsyncTagIOCTLCount)
        _tprintf(_T("Async Tag IOCTL Count = %lu\n"), pVolumeStatsData->AsyncTagIOCTLCount);
    if (pVolumeStatsData->ulDaBInOrphanedMappedDBList)
        _tprintf(_T("Total Orphaned Data Blocks = %lu\n"), pVolumeStatsData->ulDaBInOrphanedMappedDBList);
    if (pVolumeStatsData->ulDriverFlags & VSDF_DRIVER_DISABLED_DATA_CAPTURE) {
        _tprintf(_T("\tDriver has disabled data capture\n"));
    }
    if (pVolumeStatsData->ulDriverFlags & VSDF_DRIVER_DISABLED_DATA_FILES) {
        _tprintf(_T("\tDriver has disabled data files\n"));
    }
    if (pVolumeStatsData->ulDriverFlags & VSDF_SERVICE_DISABLED_DATA_MODE) {
        _tprintf(_T("\tServer has disabled data mode\n"));
    }
    if (pVolumeStatsData->ulDriverFlags & VSDF_SERVICE_DISABLED_DATA_FILES) {
        _tprintf(_T("\tServer has disabled data files\n"));
    }
    if (pVolumeStatsData->ulDriverFlags & VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_MODE) {
        _tprintf(_T("\tS2 has not registered for data mode\n"));
    }
    if (pVolumeStatsData->ulDriverFlags & VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_FILES) {
        _tprintf(_T("\tS2 has not registered for data files\n"));
    }
    if (pVolumeStatsData->ulDriverFlags & VSDF_DRIVER_FAILED_TO_ALLOCATE_DATA_POOL) {
        _tprintf(_T("\tDriver failed to allocate data pool\n"));
    }
    _tprintf(_T("\t Driver Mode : "));
    if (pVolumeStatsData->eDiskFilterMode == NoRebootMode) {
        _tprintf(_T("NoRebootMode\n "));
    }
    else if (pVolumeStatsData->eDiskFilterMode == RebootMode) {
        _tprintf(_T("RebootMode\n"));
    }
    if (0 == pVolumeStatsData->ulVolumesReturned)
        _tprintf(_T("Stats for the given volume does not exist, Since this is not a Physical Volume\n"));

    ulNumVolumes = 0;
    ULONG   ulBytesUsed = sizeof(VOLUME_STATS_DATA);
    bControlPrintStats = true;
    while (ulNumVolumes < pVolumeStatsData->ulVolumesReturned) {
        if (dwReturn < (ulBytesUsed + sizeof(VOLUME_STATS))) {
            _tprintf(_T("dwReturned = %d, ulBytesUsed = %d, sizeof(VOLUME_STATS) = %d, dwReturned < (ulBytesUsed + sizeof(VOLUME_STATS))\n"),
                dwReturn, ulBytesUsed, sizeof(VOLUME_STATS));
            break;
        }
        else {
            _tprintf(_T("\nIndex = %lu\n"), ulNumVolumes);
            PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)pVolumeStatsData + ulBytesUsed);
            PrintVolumeStats(pVolumeStats, IsOlderDriver);
            ulBytesUsed += sizeof(VOLUME_STATS);
            ulNumVolumes++;

            if (false == IsDiskFilterRunning){
                if (pVolumeStats->ulAdditionalGUIDsReturned) {
                    if (dwReturn >= (ulBytesUsed + (pVolumeStats->ulAdditionalGUIDsReturned * GUID_SIZE_IN_CHARS * sizeof(WCHAR)))) {
                        PrintAdditionalGUIDs(pVolumeStats->ulAdditionalGUIDsReturned, (PUCHAR)pVolumeStatsData + ulBytesUsed);
                        ulBytesUsed += pVolumeStats->ulAdditionalGUIDsReturned * GUID_SIZE_IN_CHARS * sizeof(WCHAR);
                    }
                    else {
                        printf("Inconsistent data: ulBytesUsed = %lu, GUIDsReturned = %lu, dwReturn = %ld\n",
                            ulBytesUsed, pVolumeStats->ulAdditionalGUIDsReturned, dwReturn);
                        break;
                    }
                }
            }
        }
    }
}

/*
This function retrieves the Filter keys 
If the filter key is empty, still returns the count of 6..But prints nothing
*/

void GetFilterKeys(LPCTSTR Class, LPCTSTR Value, char* PrintStr)
{
    DWORD retvalue;
    long RetVal;
    char *pData = NULL;

    printf(",%s : ", PrintStr);
    RetVal = RegGetValue(HKEY_LOCAL_MACHINE, Class, Value, RRF_RT_REG_MULTI_SZ, NULL, NULL, &retvalue);
    if ((RetVal != ERROR_SUCCESS) || (retvalue == 0)) {
        goto Exit;
    } else {
        pData = (char *)malloc(retvalue);
        if (NULL == pData) { printf(" Err"); goto Exit; }
        RetVal = RegGetValue(HKEY_LOCAL_MACHINE, Class, Value, RRF_RT_REG_MULTI_SZ, NULL, pData, &retvalue);
        if (RetVal != ERROR_SUCCESS) {
            goto Exit;
        } else {
            for (DWORD i = 0; i < retvalue; i++) {
                if (pData[i] == '\0')
                    continue;
                 printf("%c", pData[i]);
            }
        }
    }

Exit :
    if (NULL != pData) {
        free(pData);
    }
}
void
PrintDriverStatsCompact(PVOLUME_STATS_DATA  pVolumeStatsData, DWORD dwReturn, int IsOlderDriver)
{
    BOOL bIsWow64 = FALSE;
    DWORD   ulNumVolumes;
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;

    // Get HyperVisor Model and manufacturer
    GetWMIComputerSystem();

    // Get filter keys
    GetFilterKeys(DiskClass, UpperFilters, "DiskUpperFilter");
    GetFilterKeys(VolumeClass, UpperFilters, "VolumeUpperFilter");
    GetFilterKeys(VolumeClass, LowerFilters, "VolumeLowerFilter");

    _tprintf(_T(",TND%lu"), pVolumeStatsData->ulTotalVolumes);
    _tprintf(_T(",NumD%lu"), pVolumeStatsData->ulVolumesReturned);
    _tprintf(_T(",SS%d"), (int)pVolumeStatsData->eServiceState);// requires logic in parser
    _tprintf(_T(",DB%ld"), pVolumeStatsData->lDirtyBlocksAllocated);
    _tprintf(_T(",CN%ld"), pVolumeStatsData->lChangeNodesAllocated);
    _tprintf(_T(",CB%ld"), pVolumeStatsData->lChangeBlocksAllocated);
    _tprintf(_T(",BW%ld"), pVolumeStatsData->lBitmapWorkItemsAllocated);
    _tprintf(_T(",MeA%lu"), pVolumeStatsData->ulNumMemoryAllocFailures);
    PrintStr(",NPA%s", (ULONG)pVolumeStatsData->lNonPagedMemoryAllocated);
    _tprintf(_T(",NPL%ldm"), pVolumeStatsData->ulNonPagedMemoryLimitInMB);

    if (pVolumeStatsData->liNonPagedLimitReachedTSforFirstTime.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStatsData->liNonPagedLimitReachedTSforFirstTime.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",NPF%02d/%02d/%04d %02d:%02d:%02d:%04d"),
            LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);

    }
    if (pVolumeStatsData->liNonPagedLimitReachedTSforLastTime.QuadPart) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStatsData->liNonPagedLimitReachedTSforLastTime.QuadPart, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",NPL%02d/%02d/%04d %02d:%02d:%02d:%04d"),
                LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if ((pVolumeStatsData->LastShutdownMarker > Uninitialized) && (pVolumeStatsData->LastShutdownMarker <= DirtyShutdown))
        _tprintf(_T(",LS%d"), (int)pVolumeStatsData->LastShutdownMarker);

    if (!IsOlderDriver) {
        printf(",System Boot Counter:%lu", pVolumeStatsData->ulCommonBootCounter);
        printf(",NPD:%d", pVolumeStatsData->ulNumProtectedDisk);
    }

    //if (pVolumeStatsData->PersistentRegistryCreated == TRUE){
    //	_tprintf(_T("Persistent Registry Created on Boot\n"));
    //}
    //else if (pVolumeStatsData->PersistentRegistryCreated == FALSE){
    //	_tprintf(_T("Persistent Registry Read on Boot\n"));
    //}
    //else {
    //	_tprintf(_T("PersistentRegistryCreated Flag corrupted\n"));
    //}
    _tprintf(_T(",PRK%d"), (int)pVolumeStatsData->PersistentRegistryCreated);

    if (pVolumeStatsData->ullPersistedTimeStampAfterBoot) {
        FileTimeToSystemTime((FILETIME *)&pVolumeStatsData->ullPersistedTimeStampAfterBoot, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);
        _tprintf(_T(",LPTS%#I64x %02d/%02d/%04d %02d:%02d:%02d:%04d\n"),
            pVolumeStatsData->ullPersistedTimeStampAfterBoot, LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
            LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
    }
    if (RunningOn64bitVersion(bIsWow64)) {
        if (bIsWow64) {
            _tprintf(_T(",LPSN%#I64x"), pVolumeStatsData->ullPersistedSequenceNumberAfterBoot);
        }
    }
    _tprintf(_T(",GMemCounters"));
    PrintStr(",%s", pVolumeStatsData->ullDataPoolSizeAllocated);
    PrintStr(",%s", pVolumeStatsData->ullDataPoolSizeFree);
    PrintStr(",%s", pVolumeStatsData->ullDataPoolSizeFreeAndLocked);
    printf(",%d", pVolumeStatsData->LockedDataBlockCounter);
    printf(",%d", pVolumeStatsData->MaxLockedDataBlockCounter);
    printf(",%d", pVolumeStatsData->MappedDataBlockCounter);
    printf(",%d", pVolumeStatsData->MaxMappedDataBlockCounter);
    printf(",%lu", pVolumeStatsData->ulNumOfAllocPageNodes);
    printf(",%lu", pVolumeStatsData->ulNumOfFreePageNodes);
	
    if (pVolumeStatsData->AsyncTagIOCTLCount)
        _tprintf(_T(",ATIC%lu"), pVolumeStatsData->AsyncTagIOCTLCount);
    if (pVolumeStatsData->ulDaBInOrphanedMappedDBList)
        _tprintf(_T(",TODB%lu"), pVolumeStatsData->ulDaBInOrphanedMappedDBList);

	_tprintf(_T(",DrvF%#x"), pVolumeStatsData->ulDriverFlags);
    _tprintf(_T(",DrvM%lu"), pVolumeStatsData->eDiskFilterMode);

    if (0 == pVolumeStatsData->ulVolumesReturned)
        _tprintf(_T("\nStats for the given volume does not exist, Since this is not a Physical Volume\n"));

    ulNumVolumes = 0;
    ULONG   ulBytesUsed = sizeof(VOLUME_STATS_DATA);
    bControlPrintStats = true;
    while (ulNumVolumes < pVolumeStatsData->ulVolumesReturned) {
        if (dwReturn < (ulBytesUsed + sizeof(VOLUME_STATS))) {
            _tprintf(_T("dwReturned = %d, ulBytesUsed = %d, sizeof(VOLUME_STATS) = %d, dwReturned < (ulBytesUsed + sizeof(VOLUME_STATS))\n"),
                dwReturn, ulBytesUsed, sizeof(VOLUME_STATS));
            break;
        }
        else {
            _tprintf(_T("\nIndex=%lu\n"), ulNumVolumes);
            PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)pVolumeStatsData + ulBytesUsed);
            PrintVolumeStatsCompact(pVolumeStats,IsOlderDriver);
            ulBytesUsed += sizeof(VOLUME_STATS);
            ulNumVolumes++;

            if (false == IsDiskFilterRunning){
                if (pVolumeStats->ulAdditionalGUIDsReturned) {
                    if (dwReturn >= (ulBytesUsed + (pVolumeStats->ulAdditionalGUIDsReturned * GUID_SIZE_IN_CHARS * sizeof(WCHAR)))) {
                        PrintAdditionalGUIDs(pVolumeStats->ulAdditionalGUIDsReturned, (PUCHAR)pVolumeStatsData + ulBytesUsed);
                        ulBytesUsed += pVolumeStats->ulAdditionalGUIDsReturned * GUID_SIZE_IN_CHARS * sizeof(WCHAR);
                    }
                    else {
                        printf("Inconsistent data: ulBytesUsed = %lu, GUIDsReturned = %lu, dwReturn = %ld\n",
                            ulBytesUsed, pVolumeStats->ulAdditionalGUIDsReturned, dwReturn);
                        break;
                    }
                }
            }
        }
    }
}

LONG UpdateKeyIfNotExist(String keyName, String ValueName, DWORD Value)
{
    CRegKey     osVerKey;
    LONG        lStatus;

    lStatus = osVerKey.Open(HKEY_LOCAL_MACHINE, keyName.c_str());
    if (ERROR_SUCCESS != lStatus) {
        _tprintf(_T("Failed to open registry key %s err=%d\n"), keyName.c_str(), lStatus);
        return lStatus;
    }

    lStatus = osVerKey.QueryDWORDValue(ValueName.c_str(), Value);
    if ((ERROR_SUCCESS != lStatus) && (ERROR_FILE_NOT_FOUND != lStatus)) {
        _tprintf(_T("Failed to query %s registry key %s err=%d\n"), ValueName.c_str(), keyName.c_str(), lStatus);
        return lStatus;
    }


    lStatus = osVerKey.SetDWORDValue(ValueName.c_str(), Value);
    if (ERROR_SUCCESS != lStatus) {
        _tprintf(_T("Failed to write %s registry key %s err=%d\n"), ValueName.c_str(), keyName.c_str(), lStatus);
        return lStatus;
    }

    _tprintf(_T("Updated Registery Key: %s ValueName: %s Value=%d\n"), keyName.c_str(), ValueName.c_str(), Value);

    return lStatus;
}

DWORD EnumerateDisks(HANDLE hDriverControlDevice)
{
    _tprintf(__T("Enter: %s\n"), __TFUNCTION__);
    PDISKINDEX_INFO     pDiskIndexList = NULL;
    DWORD               size = 0, dwBytes;
    std::vector<UCHAR>  Buffer;
    bool bSuccess = false;
    size = sizeof(DISKINDEX_INFO) + MAX_NUM_DISKS_SUPPORTED * sizeof(ULONG);
    Buffer.resize(size);
    pDiskIndexList = (PDISKINDEX_INFO)&Buffer[0];
    ZeroMemory(pDiskIndexList, size);
    DWORD bRetVal = DeviceIoControl(
        hDriverControlDevice,
        (DWORD)IOCTL_INMAGE_GET_DISK_INDEX_LIST,
        NULL, 0, pDiskIndexList, size, &dwBytes, NULL);
    if (!bRetVal) 
    {
        _tprintf(__T("IOCTL_INMAGE_GET_DISK_INDEX_LIST failed with error: %d\n"), bRetVal);
        return CMD_STATUS_UNSUCCESSFUL;
    }
    _tprintf(__T("Number of disks enumerated by driver = %d size=%d\n"), pDiskIndexList->ulNumberOfDisks, size);
    return (pDiskIndexList->ulNumberOfDisks > 0)? CMD_STATUS_SUCCESS : CMD_STATUS_UNSUCCESSFUL;
}

LONG UpdateOSVersionForDisks()
{
    _tprintf(__T("Enter: %s\n"), __TFUNCTION__);

    CRegKey                 cregkey;
    LONG                    lStatus = 0;
    std::vector<_TCHAR>     subKeyBuffer(MAX_PATH);
    DWORD                   dwResult;
    int                     index = 0;
    StringStream            ss;
    OSVERSIONINFOEX         osVersionInfoEx = { 0 };

    osVersionInfoEx.dwOSVersionInfoSize = sizeof(osVersionInfoEx);
    if (!GetVersionEx((POSVERSIONINFO)&osVersionInfoEx)) {
        _tprintf(_T("GetVersionEx failed with error=%d\n"), GetLastError());
        return GetLastError();
    }

    lStatus = cregkey.Open(HKEY_LOCAL_MACHINE, INDSKFLT_KEY);
    if (ERROR_SUCCESS != lStatus) {
        _tprintf(_T("Failed to open registry key %s err=%d\n"), INDSKFLT_KEY, lStatus);
        return lStatus;
    }

    _tmemset(&subKeyBuffer[0], _T('\0'), subKeyBuffer.size());
    while (ERROR_SUCCESS == (dwResult = RegEnumKey(cregkey, index, &subKeyBuffer[0], subKeyBuffer.size()))) {
        ++index;

        if (0 == _tcscmp(KERNEL_TAGS, &subKeyBuffer[0])) {
            _tprintf(_T("Skkiping subkey: %s\n"), &subKeyBuffer[0]);
            _tmemset(&subKeyBuffer[0], _T('\0'), subKeyBuffer.size());
            continue;
        }

        ss.clear();
        ss.str(String());
        ss << INDSKFLT_KEY << _T('\\') << &subKeyBuffer[0];

        UpdateKeyIfNotExist(ss.str(), OS_MAJOR_VERSION_KEY, osVersionInfoEx.dwMajorVersion);

        UpdateKeyIfNotExist(ss.str(), OS_MINOR_VERSION_KEY, osVersionInfoEx.dwMinorVersion);

        _tmemset(&subKeyBuffer[0], _T('\0'), subKeyBuffer.size());
    }

    _tprintf(__T("Exit: %s\n"), __TFUNCTION__);
    return ERROR_SUCCESS;
}