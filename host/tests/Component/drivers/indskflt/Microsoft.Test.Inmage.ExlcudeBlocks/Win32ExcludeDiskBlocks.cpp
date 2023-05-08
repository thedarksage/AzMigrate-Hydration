#include <Windows.h>
#include "stdafx.h"

#include "Microsoft.Test.Inmage.Win32ExcludeDiskBlocks.h"
#include <vector>
#include <sstream>

bool compare_extentInfo(const ExtentInfo& first, const ExtentInfo& second)
{
    return (first.m_liStartOffset < second.m_liStartOffset);
}

CWin32ExcludeDiskBlocks::CWin32ExcludeDiskBlocks(IPlatformAPIs* platformApis, ILogger* pLogger, CInmageIoctlController* pIoctlController) :
    m_platformApis(platformApis),
    m_inmageIoctlController(pIoctlController),
    m_pLogger(pLogger)
{
    std::vector<std::string> PageFileNames;
    std::vector<std::string> BitmapFileNames;
    std::vector<std::string>::iterator  it;
    SV_ULONG mask = 0x1;

    SV_ULONG pageVolumes = GetPagefilevol(&PageFileNames);

    if (0 == pageVolumes)   return;


    for (it = PageFileNames.begin(); PageFileNames.end() != it; it++) {
        UpdateExludeList(*it, true);
    }
    GetBitmapFilePaths(&BitmapFileNames);
    for (it = BitmapFileNames.begin(); BitmapFileNames.end() != it; it++) {
        if (m_platformApis->PathExists(it->c_str())) {
            UpdateExludeList(*it, false);
        }
    }

    //Sort the list
    std::map<std::string, std::list<ExtentInfo>>::iterator diskIt;
    for (diskIt = m_DiskExcludeBlocks.begin(); m_DiskExcludeBlocks.end() != diskIt; diskIt++)
    {
        m_DiskExcludeBlocks[diskIt->first].sort(compare_extentInfo);
    }

}

void CWin32ExcludeDiskBlocks::UpdateExcludeRanges(void* hVolume, void* pRetrievalPointer, SV_ULONG ulBytesPerCluster)
{
    CHAR                        volBuffer[1024];
    PRETRIEVAL_POINTERS_BUFFER  retBuf = (PRETRIEVAL_POINTERS_BUFFER)pRetrievalPointer;
    PVOLUME_PHYSICAL_OFFSETS    pVolumeOffsets;
    SV_ULONG                    dwBytes;
    BOOL                        bStatus;

    LARGE_INTEGER   liPrevLcn = retBuf->StartingVcn;
    pVolumeOffsets = (PVOLUME_PHYSICAL_OFFSETS)volBuffer;

    for (SV_ULONG i = 0; i < retBuf->ExtentCount; i++) {
        VOLUME_LOGICAL_OFFSET       volumeLogicalOffset = { 0 };

        SV_LONGLONG liStartOffset = retBuf->Extents[i].Lcn.QuadPart * ulBytesPerCluster;
        SV_LONGLONG liSize = (retBuf->Extents[i].NextVcn.QuadPart - liPrevLcn.QuadPart) * ulBytesPerCluster;
        liPrevLcn = retBuf->Extents[i].NextVcn;

        // Get Physical Disk Offsets
        volumeLogicalOffset.LogicalOffset = liStartOffset;
        bStatus = m_platformApis->DeviceIoControlSync(hVolume, IOCTL_VOLUME_LOGICAL_TO_PHYSICAL, &volumeLogicalOffset, sizeof(volumeLogicalOffset),
            pVolumeOffsets, 1024, &dwBytes);
        if (!bStatus) {
            m_platformApis->Close(hVolume);
            throw CInmageExcludeBlocksException("%s: IOCTL_VOLUME_LOGICAL_TO_PHYSICAL failed with error=0x%x", __FUNCTION__, m_platformApis->GetLastError());
        }

        for (int j = 0; j < pVolumeOffsets->NumberOfPhysicalOffsets; j++) {
            SV_ULONG ulDiskNum = pVolumeOffsets->PhysicalOffset[j].DiskNumber;
            CDiskDevice disk(ulDiskNum, m_platformApis);
            m_DiskExcludeBlocks[disk.GetDeviceId()].push_back(ExtentInfo(pVolumeOffsets->PhysicalOffset[j].Offset, pVolumeOffsets->PhysicalOffset[j].Offset + liSize));
            m_pLogger->LogInfo("DeviceId: %s Adding range starting: 0x%08x end: 0x%08x\n", disk.GetDeviceId().c_str(),
                pVolumeOffsets->PhysicalOffset[j].Offset,
                pVolumeOffsets->PhysicalOffset[j].Offset + liSize);
        }
    }

}

void CWin32ExcludeDiskBlocks::UpdateExludeList(std::string fileName, bool bPagefile)
{
    CHAR                        retBuffer[1024];

    PRETRIEVAL_POINTERS_BUFFER  retBuf;
    HANDLE                      hVolume;
    NTFS_VOLUME_DATA_BUFFER     volumeDataBuffer = { 0 };

    std::string  volumeName = "\\\\?\\";
    std::string  strVol = fileName.substr(0, fileName.find_first_of(':') + 1);

    volumeName.append(strVol.begin(), strVol.end());

    hVolume = m_platformApis->OpenDefault(volumeName.c_str());
    if (!m_platformApis->IsValidHandle(hVolume)) {
        throw CInmageExcludeBlocksException("%s: Failed to open handle to %s err=0x%x", __FUNCTION__, strVol);
    }

    // Get Bytes Per Cluster
    SV_ULONG   dwBytes;
    BOOL    bStatus;

    bStatus = m_platformApis->DeviceIoControlSync(hVolume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &volumeDataBuffer, sizeof(volumeDataBuffer), &dwBytes);
    if (!bStatus) {
        m_platformApis->Close(hVolume);
        throw CInmageExcludeBlocksException("%s: FSCTL_GET_NTFS_VOLUME_DATA failed with error=0x%x", __FUNCTION__, m_platformApis->GetLastError());
    }

    retBuf = (PRETRIEVAL_POINTERS_BUFFER)retBuffer;
    string filePath(fileName.begin(), fileName.end());

    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (!bPagefile) {
        filePath.insert(0, "\\\\?\\");
        hFile = m_platformApis->Open(filePath,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_WRITE,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL);

        if (!m_platformApis->IsValidHandle(hFile)) {
            SV_ULONG dwErr = m_platformApis->GetLastError();
            throw CInmageExcludeBlocksException("CreateFile for %s failed with err=0x%x", filePath.c_str(), m_platformApis->GetLastError());
        }
    }

    SV_LONGLONG     llStartingVcn = 0;
    bool            bContinue = false;

    do {
        if (bPagefile)
        {
            filePath.insert(0, "\\DosDevices\\");
            m_inmageIoctlController->GetLcn(filePath, retBuf, 1024, llStartingVcn);
        }
        else
        {
            m_platformApis->GetVolumeMappings(hFile, llStartingVcn, retBuf, 1024);
        }

        UpdateExcludeRanges(hVolume, retBuf, volumeDataBuffer.BytesPerCluster);

        llStartingVcn = retBuf->Extents[retBuf->ExtentCount - 1].NextVcn.QuadPart;
        bContinue = (ERROR_MORE_DATA == m_platformApis->GetLastError());
    } while (bContinue);

    m_platformApis->Close(hFile);
    m_platformApis->Close(hVolume);
}

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

void CWin32ExcludeDiskBlocks::GetBitmapFilePaths(std::vector<std::string>* pPageFileNames)
{
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\InDskFlt\Parameters
    HKEY hKey;
    CHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    SV_ULONG    cbName;                   // size of name string 
    CHAR    achClass[MAX_PATH] = "";  // buffer for class name 
    SV_ULONG    cchClassName = MAX_PATH;  // size of class string 
    SV_ULONG    cSubKeys = 0;               // number of subkeys 
    SV_ULONG    cbMaxSubKey;              // longest subkey size 
    SV_ULONG    cchMaxClass;              // longest class string 
    SV_ULONG    cValues;              // number of values for key 
    SV_ULONG    cchMaxValue;          // longest value name 
    SV_ULONG    cbMaxValueData;       // longest value data 
    SV_ULONG    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 

    SV_ULONG i, retCode;

    TCHAR  achValue[MAX_VALUE_NAME];
    SV_ULONG cchValue = MAX_VALUE_NAME;
    string BitmapRootKey = "SYSTEM\\CurrentControlSet\\Services\\InDskFlt\\Parameters";
    string BitmapFileKey;

    long status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, BitmapRootKey.c_str(), 0, KEY_ALL_ACCESS, &hKey);
    // Get the class name and the value count. 
    retCode = RegQueryInfoKeyA(
        hKey,                    // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        NULL,                    // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 

    if (0 == cSubKeys)
    {
        throw CInmageExcludeBlocksException("failed to open bitmap key");
    }

    for (i = 0; i < cSubKeys; i++)
    {
        BitmapFileKey = BitmapRootKey;
        cbName = MAX_KEY_LENGTH;
        retCode = RegEnumKeyExA(hKey, i,
            achKey,
            &cbName,
            NULL,
            NULL,
            NULL,
            &ftLastWriteTime);

        BitmapFileKey.append("\\").append(achKey);
        SV_ULONG dataSize = 0;

        status = RegGetValueA(hKey, achKey, "BitmapLogPathName", RRF_RT_REG_SZ, NULL, NULL, &dataSize);
        if (ERROR_SUCCESS != status) {
            continue;
        }

        vector<CHAR>	buffer(dataSize);
        status = RegGetValueA(hKey, achKey, "BitmapLogPathName", RRF_RT_REG_SZ, NULL, &buffer[0], &dataSize);
        if (ERROR_SUCCESS == status) {
            pPageFileNames->push_back(string(buffer.begin(), buffer.end()));
        }
    }
}

SV_ULONG CWin32ExcludeDiskBlocks::GetPagefilevol(std::vector<std::string>* pPageFileNames)
{
    HKEY hKey;
    SV_ULONG pageDrives = 0;
    char* pageInfo = NULL;
    SV_ULONG pageInfoLength = 0;
    const int PAGEFILE_START_IN_EXISTING_KEY = 4;

    do
    {
        long status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", 0, KEY_QUERY_VALUE, &hKey);

        if (ERROR_SUCCESS != status) {
            throw CInmageExcludeBlocksException("Failed to get the pagefile info from registry %d\n", status);
        }

        // #15838 - Check if "ExistingPageFiles" entry is present in the registry. If so get the drive letter from this entry.  
        // The ExistingPageFiles entry will be present in the registry for version Windows 2007 and greater.
        if (RegQueryValueExA(hKey, "ExistingPageFiles", NULL, NULL, NULL, &pageInfoLength) == ERROR_SUCCESS)
        {
            pageInfo = (char*)calloc(pageInfoLength, 1);
            status = RegQueryValueExA(hKey, "ExistingPageFiles", NULL, NULL,
                reinterpret_cast<BYTE*>(pageInfo), &pageInfoLength);

            if (ERROR_SUCCESS != status) {
                throw CInmageExcludeBlocksException("Failed to get the pagefile info from registry %d\n", status);
            }

            std::string pagefileDrive;
            unsigned long size = 0;
            while (size < pageInfoLength)
            {
                pagefileDrive.clear();
                pagefileDrive = (pageInfo + size);
                size += (pagefileDrive.size() + 1);
                if (pagefileDrive.empty())
                    continue;

                std::stringstream sspg(pagefileDrive.c_str() + PAGEFILE_START_IN_EXISTING_KEY);
                if (pPageFileNames)
                {
                    std::string path;
                    sspg >> path;
                    pPageFileNames->push_back(path);
                }
                SV_ULONG dwSystemDrives = 0;
                dwSystemDrives = 1 << (toupper(pagefileDrive[PAGEFILE_START_IN_EXISTING_KEY]) - 'A');
                pageDrives |= dwSystemDrives;
            }

        }
        else
        {

            status = RegQueryValueExA(hKey, "PagingFiles", NULL, NULL, NULL, &pageInfoLength);

            if (ERROR_SUCCESS != status) {
                throw CInmageExcludeBlocksException("Failed to get the pagefile length from registry %d\n", status);
            }

            pageInfo = (char*)calloc(pageInfoLength, 1);
            status = RegQueryValueExA(hKey, "PagingFiles", NULL, NULL,
                reinterpret_cast<BYTE*>(pageInfo), &pageInfoLength);

            if (ERROR_SUCCESS != status) {
                throw CInmageExcludeBlocksException("Failed to get the pagefile info from registry %d\n", status);
            }

            std::string pagefileDrive;
            unsigned long size = 0;
            while (size < pageInfoLength)
            {
                pagefileDrive.clear();
                pagefileDrive = (pageInfo + size);
                size += (pagefileDrive.size() + 1);
                if (pagefileDrive.empty())
                    continue;
                std::stringstream sspg(pagefileDrive);
                if (pPageFileNames)
                {
                    std::string path;
                    sspg >> path;
                    pPageFileNames->push_back(path);
                }
                SV_ULONG dwSystemDrives = 0;
                dwSystemDrives = 1 << (toupper(pagefileDrive[0]) - 'A');
                pageDrives |= dwSystemDrives;
            }
        }
    } while (false);
    RegCloseKey(hKey);
    if (pageInfo)
    {
        free(pageInfo);
    }

    return pageDrives;
}

std::list<ExtentInfo> CWin32ExcludeDiskBlocks::GetExcludeBlocks(std::string diskId)
{
    std::list<ExtentInfo> emptyList;

    if (m_DiskExcludeBlocks.end() == m_DiskExcludeBlocks.find(diskId)) {
        return emptyList;
    }

    return m_DiskExcludeBlocks[diskId];
}