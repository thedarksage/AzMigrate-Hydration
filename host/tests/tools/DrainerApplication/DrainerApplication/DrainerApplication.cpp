// DrainerApplication.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>

#include <list>
#include <string>
#include <vector>
#include <strsafe.h>

#include "InmFltInterface.h"
#include "InmFltIOCTL.h"
#include "InmageIoctlController.h"

#include <time.h>

std::string g_TagGuid = "89d17f1a-21e7-5bf6-afc5-11eb166289f1";

using namespace std;
#define TEST_FLAG(_a, _f)	((_f) == ((_a) & (_f)))

HANDLE m_hInmageCtrlDevice = INVALID_HANDLE_VALUE;

std::vector<std::wstring>    g_protectedDeviceIds;

const char *WriteOrderStateStringArray[] =
{
    "ecWriteOrderStateUnInitialized",
    "ecWriteOrderStateBitmap",
    "ecWriteOrderStateMetadata",
    "ecWriteOrderStateData"
};

const char *
WriteOrderStateString(
etWriteOrderState eWOState
)
{
    const char *WOStateString = NULL;
    switch (eWOState) {
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

const char *CaptureModeStringArray[] =
{
    "ecCaptureModeUninitialized",
    "ecCaptureModeMetaData",
    "ecCaptureModeData"
};


const char *
CaptureModeString(
etCaptureMode eCaptureMode
)
{
    const char *CMstring = NULL;

    switch (eCaptureMode) {
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

void
GetDiskStats(HANDLE hCtrlDevice, wstring wscDeviceId)
{
    PVOLUMES_DM_DATA  pVolumesDMdata;
    DWORD               dwReturn;

    pVolumesDMdata = (PVOLUMES_DM_DATA)malloc(sizeof(VOLUMES_DM_DATA));
    if (NULL == pVolumesDMdata) {
        printf("GetDataMode: Failed in memory allocation\n");
        return;
    }

    if (!DeviceIoControl(
        m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_GET_DATA_MODE_INFO,
        (LPVOID) wscDeviceId.c_str(),
        sizeof(WCHAR)* GUID_SIZE_IN_CHARS,
        pVolumesDMdata,
        sizeof(VOLUMES_DM_DATA),
        &dwReturn,
        NULL        // Overlapped
        ))
    {
        wprintf(L"IOCTL_INMAGE_GET_DATA_MODE_INFO failed with error=0x%x\n", GetLastError());
        free(pVolumesDMdata);
        return;
    }

    PVOLUME_DM_DATA pVolumeDM = &pVolumesDMdata->VolumeArray[0];
    printf("Write order state  = %s\n", WriteOrderStateString(pVolumeDM->eWOState));
    printf("Prev write order state  = %s\n", WriteOrderStateString(pVolumeDM->ePrevWOState));
    printf("Capture mode = %s\n\n", CaptureModeString(pVolumeDM->eCaptureMode));

    free(pVolumesDMdata);
}

std::string ProcessTagsV2(PUDIRTY_BLOCK_V2 udbp)
{
    PSTREAM_REC_HDR_4B tgp = NULL;
    char tag_guid[256] = { 0 };
    char tag_name[256] = { 0 };
    int tg_len;
    int ret = 0;
    list<string>        taglist;

    tgp = &udbp->uTagList.TagList.TagEndOfList;
    while (tgp->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) {

        unsigned int tg_hdr_len = 0;
        if ((tgp->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) == STREAM_REC_FLAGS_LENGTH_BIT)
            tg_hdr_len = sizeof(STREAM_REC_HDR_8B);
        else
            tg_hdr_len = sizeof(STREAM_REC_HDR_4B);

        tg_len = *(unsigned short *)((char*)tgp + tg_hdr_len);

        char *tg_ptr = (char *)((char *)tgp + tg_hdr_len + sizeof(unsigned short));

        PSTREAM_REC_HDR_4B tag_guid_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
        unsigned int tg_guid_hdr_len = 0;
        if ((tag_guid_hdr_p->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) == STREAM_REC_FLAGS_LENGTH_BIT)
        {
            tg_guid_hdr_len = sizeof(STREAM_REC_HDR_8B);
            PSTREAM_REC_HDR_8B tag_guid_hdr_p = (PSTREAM_REC_HDR_8B)tg_ptr;
            memcpy_s(tag_guid, ARRAYSIZE(tag_guid), (char*)tag_guid_hdr_p + tg_guid_hdr_len, tag_guid_hdr_p->ulLength - tg_guid_hdr_len);
            tg_ptr = (char *)((char *)tg_ptr + tag_guid_hdr_p->ulLength);
            //printf("tag guid: len %d - %s\n", tag_guid_hdr_p->ulLength - tg_guid_hdr_len, tag_guid);
            taglist.push_back(tag_guid);
        }
        else
        {
            tg_guid_hdr_len = sizeof(STREAM_REC_HDR_4B);
            PSTREAM_REC_HDR_4B tag_guid_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
            memcpy_s(tag_guid, ARRAYSIZE(tag_guid), (char*)tag_guid_hdr_p + tg_guid_hdr_len, tag_guid_hdr_p->ucLength - tg_guid_hdr_len);
            tg_ptr = (char *)((char *)tg_ptr + tag_guid_hdr_p->ucLength);
            //printf("tag guid: len %d - %s\n", tag_guid_hdr_p->ucLength - tg_guid_hdr_len, tag_guid);
            taglist.push_back(tag_guid);
        }

        PSTREAM_REC_HDR_4B tag_name_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
        unsigned int tg_name_hdr_len = 0;
        if ((tag_name_hdr_p->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) == STREAM_REC_FLAGS_LENGTH_BIT)
        {
            tg_name_hdr_len = sizeof(STREAM_REC_HDR_8B);
            PSTREAM_REC_HDR_8B tag_name_hdr_p = (PSTREAM_REC_HDR_8B)tg_ptr;
            memcpy_s(tag_name, ARRAYSIZE(tag_name), (char*)tag_name_hdr_p + tg_name_hdr_len, tag_name_hdr_p->ulLength - tg_name_hdr_len);
            tg_ptr = (char *)((char *)tg_ptr + tag_name_hdr_p->ulLength);
            //printf("tagtype: %0x tag name: len %d - %s\n", tag_name_hdr_p->usStreamRecType, tag_name_hdr_p->ulLength - tg_name_hdr_len, tag_name);
        }
        else
        {
            tg_name_hdr_len = sizeof(STREAM_REC_HDR_4B);
            PSTREAM_REC_HDR_4B tag_name_hdr_p = (PSTREAM_REC_HDR_4B)tg_ptr;
            memcpy_s(tag_name, ARRAYSIZE(tag_name), (char*)tag_name_hdr_p + tg_name_hdr_len, tag_name_hdr_p->ucLength - tg_name_hdr_len);
            tg_ptr = (char *)((char *)tg_ptr + tag_name_hdr_p->ucLength);
            //printf("tagtype: %0x tag name: len %d - %s\n", tag_name_hdr_p->usStreamRecType, tag_name_hdr_p->ucLength - tg_name_hdr_len, tag_name);
        }

        tgp = (PSTREAM_REC_HDR_4B)((char *)tgp +
            GET_STREAM_LENGTH(tgp));

    }    
    return tag_guid;
}

void StartFiltering(HANDLE hCtrlDevice, wstring wscDeviceId)
{
    DWORD                       dwBytes = 0;
    START_FILTERING_INPUT       startFilteringInput = { 0 };

    StringCchPrintfW(startFilteringInput.DeviceId, 
        ARRAYSIZE(startFilteringInput.DeviceId)+2, wscDeviceId.c_str());
    if (!DeviceIoControl(
        m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_START_FILTERING_DEVICE,
        &startFilteringInput,
        sizeof(startFilteringInput),
        NULL,
        0,
        &dwBytes,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        wprintf(L"IOCTL_INMAGE_START_FILTERING_DEVICE failed with err=0x%x\n", dwErr);
        exit(-1);
    }

    wprintf(L"%s deviceId: %s\n", __FUNCTIONW__, wscDeviceId.c_str());
}

void RegisterForDbNotify(HANDLE hEvent, wstring wscDeviceId)
{
    DWORD                       dwBytes = 0;

    VOLUME_DB_EVENT_INFO        eventInfo = { 0 };
    StringCchPrintf(eventInfo.DevID, ARRAYSIZE(eventInfo.DevID)+2, L"%s", wscDeviceId.c_str());
    eventInfo.hEvent = hEvent;

    if (!DeviceIoControl(
        m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT,
        (LPVOID)&eventInfo,
        sizeof(eventInfo),
        NULL,
        0,
        &dwBytes,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        wprintf(L"IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT failed with err=0x%x\n", dwErr);
        exit(-1);
    }


}

void ClearDifferentials(HANDLE hCtrlDevice, wstring wscDeviceId)
{
    DWORD   dwBytes = 0;

    if (!DeviceIoControl(
        m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
        (LPVOID) wscDeviceId.c_str(),
        wscDeviceId.length()*sizeof(WCHAR),
        NULL,
        0,
        &dwBytes,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        wprintf(L"IOCTL_INMAGE_START_FILTERING_DEVICE failed with err=0x%x\n", dwErr);
        exit(-1);
    }

    wprintf(L"%s deviceId: %s\n", __FUNCTIONW__, wscDeviceId.c_str());
}

wstring GetDiskId(int diskNum)
{
    WCHAR   wszDisk[MAX_PATH];
    wstring DiskName;
    WCHAR m_diskId[MAX_PATH];

    StringCchPrintfW(wszDisk, MAX_PATH, L"\\\\.\\PhysicalDrive%d", diskNum);
    DiskName = wszDisk;

    ZeroMemory(m_diskId, sizeof(m_diskId));

    HANDLE hDisk = CreateFile(DiskName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hDisk)
    {
        wprintf(L"%s Failed to open Handle %s err=0x%x\n", __FUNCTIONW__, DiskName.c_str(), GetLastError());
        exit(-1);
    }

    DWORD dwBytes = 0;
    DWORD dwErr;

    PDRIVE_LAYOUT_INFORMATION_EX layoutEx = (PDRIVE_LAYOUT_INFORMATION_EX)malloc(1024);

    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, layoutEx, 1024, &dwBytes, NULL))
    {
        dwErr = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER != dwErr)
        {
            CloseHandle(hDisk);
            wprintf(L"%s: IOCTL_DISK_GET_DRIVE_LAYOUT_EX on Disk %s failed with error=0x%x\n", __FUNCTIONW__, DiskName.c_str(), dwErr);
            exit(-1);
        }
    }

    if (layoutEx->PartitionStyle == PARTITION_STYLE_MBR)
    {
        if (0 == layoutEx->Mbr.Signature)
        {
            return L"";
        }
        StringCchPrintf(m_diskId, MAX_PATH, L"%lu", layoutEx->Mbr.Signature);
    }
    else if (layoutEx->PartitionStyle == PARTITION_STYLE_GPT)
    {
        ::StringFromGUID2(layoutEx->Gpt.DiskId, m_diskId, MAX_PATH);
    }
    else
    {
        return L"";
    }
    CloseHandle(hDisk);

    return m_diskId;
}

void ProcessTags(PUDIRTY_BLOCK_V2 udbp)
{
    PSTREAM_REC_HDR_4B pTag = &udbp->uTagList.TagList.TagEndOfList;
    list<string>        taglist;

    ULONG   ulBytesProcessed = 0;
    ULONG   ulNumberOfUserDefinedTags = 0;

    while (pTag->usStreamRecType != STREAM_REC_TYPE_END_OF_TAG_LIST)
    {
        ULONG ulLength = STREAM_REC_SIZE(pTag);
        ULONG ulHeaderLength = (pTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ? sizeof(STREAM_REC_HDR_8B) : sizeof(STREAM_REC_HDR_4B);

        if (pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG)
        {
            ULONG   ulTagLength = 0;
            PCHAR   pTagData;

            ulNumberOfUserDefinedTags++;

            ulTagLength = *(PUSHORT)((PUCHAR)pTag + ulHeaderLength);

            // This is tag name
            pTagData = (PCHAR)pTag + ulHeaderLength + sizeof(USHORT);

            taglist.push_back(pTagData);
        }

        ulBytesProcessed += ulLength;
        pTag = (PSTREAM_REC_HDR_4B)((PUCHAR)pTag + ulBytesProcessed);
    }
    list<string>::iterator it = taglist.begin();
    for (; taglist.end() != it; it++)
    {
        printf("\t%s", it->c_str());
    }
    printf("\n");
}

DWORD GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans,
    PUDIRTY_BLOCK_V2  pDirtyBlock,
    std::string deviceId)
{
    DWORD dwReturn;
    DWORD dwInLength = (NULL == pCommitTrans) ? 0 : sizeof(*pCommitTrans);

    std::wstring    wcsDeviceId(deviceId.begin(), deviceId.end());

    ZeroMemory(pDirtyBlock, sizeof(*pDirtyBlock));
    ZeroMemory(pCommitTrans, sizeof(*pCommitTrans));

    StringCchPrintfW(pCommitTrans->DevID, sizeof(pCommitTrans->DevID), L"%s", wcsDeviceId.c_str());

    bool isSuccessful = DeviceIoControl(
        m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
        pCommitTrans,
        dwInLength,
        pDirtyBlock,
        sizeof(UDIRTY_BLOCK_V2),
        &dwReturn,
        NULL
    );

    wprintf(L"%s: Device: %s IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS Status=%d\n", __FUNCTION__,
        wcsDeviceId.c_str(), isSuccessful ? 0 : GetLastError());

    DWORD dwErr = 0;
    if (!isSuccessful)
    {
        dwErr = GetLastError();
    }

    if (sizeof(UDIRTY_BLOCK_V2) != dwReturn)
    {
        dwErr = -1;
    }

    if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)
    {
        dwErr = -2;
    }

    return dwErr;
}

DWORD CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction)
{
    DWORD                        dwBytes = 0;
    DWORD               dwErr = ERROR_SUCCESS;

    if (!DeviceIoControl(
        m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
        pCommitTransaction,
        sizeof(COMMIT_TRANSACTION),
        NULL,
        0,
        &dwBytes,
        NULL
    ))
    {
        wprintf(L"Device: %s TransId: %lu IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS failed with err=%d\n", 
                    pCommitTransaction->DevID,
                    pCommitTransaction->ulTransactionID.QuadPart,
                    GetLastError());
        dwErr = GetLastError();
    }
    return dwErr;
}

DWORD CommitCurrentTransaction(std::string deviceId, ULONG ulNumExpectedDiryBlocks = 1)
{
    bool    bContinue = false;
    ULONG   ulNumDataBlocks = 0;
    ULONG   ulNumTags = 0;
    ULONG   ulNumDrainedDiryBlocks = 0;
    COMMIT_TRANSACTION  commitTrans = { 0 };
    UDIRTY_BLOCK_V2     uDirtyBlockV2 = { 0 };

    do
    {
        commitTrans.ulTransactionID.QuadPart = 0;
        GetDirtyBlockTransaction(&commitTrans,
            &uDirtyBlockV2, deviceId);


        if (TEST_FLAG(uDirtyBlockV2.uHdr.Hdr.ulFlags, UDIRTY_BLOCK_FLAG_SVD_STREAM)) {
        }
        else if (!TEST_FLAG(uDirtyBlockV2.uHdr.Hdr.ulFlags, UDIRTY_BLOCK_FLAG_TSO_FILE))
        {
            if (0 == uDirtyBlockV2.uHdr.Hdr.cChanges) {
                ProcessTags(&uDirtyBlockV2);
                +ulNumTags;
            }
            else {
                ++ulNumDataBlocks;
                for (unsigned long l = 0; l < uDirtyBlockV2.uHdr.Hdr.cChanges; l++) {
                    if (0 == uDirtyBlockV2.ChangeLengthArray[l]) {
                        printf("ERROR: uDirtyBlockV2.ChangeLengthArray %d is Zero\n", l);
                    }
                }
            }
        }

        commitTrans.ulTransactionID = uDirtyBlockV2.uHdr.Hdr.uliTransactionID;
        CommitTransaction(&commitTrans);
        //bContinue = (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
        ulNumDrainedDiryBlocks++;
    } while (bContinue);

    if (ulNumDrainedDiryBlocks != ulNumExpectedDiryBlocks) {
        printf("%lu != %lu\n", ulNumDrainedDiryBlocks, ulNumExpectedDiryBlocks);
    }
    return ERROR_SUCCESS;
}

DWORD WINAPI DrainThread(
    _In_ LPVOID lpParameter
    )
{
    DWORD diskNum = _wtol((WCHAR*) lpParameter);

    WCHAR   wszDisk[MAX_PATH];

    DWORD threadId = GetCurrentThreadId();

    StringCchPrintfW(wszDisk, MAX_PATH, L"\\\\.\\PhysicalDrive%d", diskNum);
    wstring DiskName = wszDisk;
    wstring deviceGuid = GetDiskId(diskNum);

    if (deviceGuid.empty())
    {
        wprintf(L"%s: DiskNum: %d Is Uninitialized\n", __FUNCTIONW__, diskNum);
        return 0;
    }
    if (deviceGuid.at(0) == L'{') {
        deviceGuid = deviceGuid.substr(1);
        deviceGuid.erase(deviceGuid.length() - 1);
    }

    wprintf(L"%s: DiskNum: %d DiskId: %s\n", __FUNCTIONW__, diskNum, deviceGuid.c_str());
    g_protectedDeviceIds.push_back(deviceGuid);

    StartFiltering(INVALID_HANDLE_VALUE, deviceGuid);
    Sleep(30 * 100);

    ClearDifferentials(INVALID_HANDLE_VALUE, deviceGuid);
    Sleep(30 * 100);

    // Get Disk State
    GetDiskStats(INVALID_HANDLE_VALUE, deviceGuid);

    PUDIRTY_BLOCK_V2  	pDirtyBlock;
    COMMIT_TRANSACTION  CommitTrans;

    ZeroMemory(&CommitTrans, sizeof(CommitTrans));

    pDirtyBlock = (PUDIRTY_BLOCK_V2)malloc(sizeof(UDIRTY_BLOCK_V2));
    DWORD dwReturn = 0;
    ULARGE_INTEGER  lastCommitedId = { 0 };

    StringCchPrintf(CommitTrans.VolumeGUID, ARRAYSIZE(CommitTrans.VolumeGUID)+2, L"%s", deviceGuid.c_str());

    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    RegisterForDbNotify(hEvent, deviceGuid);

    while (1)
    {
        time_t rawtime;
        struct tm timeinfo;
        char timebuf[26];

        time(&rawtime);
        localtime_s(&timeinfo, &rawtime);
        asctime_s(timebuf, 26, &timeinfo);
        timebuf[strlen(timebuf) - 1] = '\0';

        CommitTrans.ulTransactionID.QuadPart = 0;

        if (!DeviceIoControl(
            m_hInmageCtrlDevice,
            (DWORD)IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
            &CommitTrans,
            sizeof(CommitTrans) ,
            pDirtyBlock,
            sizeof(UDIRTY_BLOCK_V2),
            &dwReturn,
            NULL
            ))
        {
            wprintf(L"\n%s: Disk%d IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS failed with err=0x%x\n", timebuf, diskNum, GetLastError());
            WaitForSingleObject(hEvent, 65 * 1000);
            continue;
        }

        if (pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart == lastCommitedId.QuadPart)
        {
            printf("Transaction Id %llu is repeated\n", lastCommitedId);
        }

        CommitTrans.ulFlags = 0;
        CommitTrans.ulTransactionID.QuadPart = pDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart;

        // Commit this transaction
        // IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS
        if (UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED & pDirtyBlock->uHdr.Hdr.ulFlags)
        {
            printf("\n%s: T: %d Resync Needed ", timebuf, diskNum);
            CloseHandle(m_hInmageCtrlDevice);
            exit(0);
        }

        if (UDIRTY_BLOCK_FLAG_TSO_FILE != (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE))
        {
            if (pDirtyBlock->uHdr.Hdr.cChanges == 0)
            {
                auto tag = ProcessTagsV2(pDirtyBlock);
                printf("Disk%d: Recieved tag: %s\n", diskNum, tag.c_str());
            }
            else{
                //printf("\n%s: Data Received: changes = %d", timebuf, pDirtyBlock->uHdr.Hdr.cChanges);
            }
        }
       

        if (!DeviceIoControl(
            m_hInmageCtrlDevice,
            (DWORD)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
            &CommitTrans,
            sizeof(CommitTrans),
            NULL,
            0,
            &dwReturn,
            NULL
            ))
        {
            wprintf(L"\n%s: Disk%d IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS failed with err=0x%x\n", timebuf, diskNum, GetLastError());
        }

        Sleep(10 * 1000);
    }
    return 0;
}

void* GetTagCommitNotifyBuffer(std::vector<std::wstring> protectedDiskIds, 
                                ULONG& ulInputSize,
                                std::string TagGuid)
{

    uint32_t ulOutputSize = 0, ulTotalSize = 0;
    uint32_t ulNumDiskForDriverOutput = protectedDiskIds.size();

    ulInputSize = FIELD_OFFSET(TAG_COMMIT_NOTIFY_INPUT, DeviceId[ulNumDiskForDriverOutput]);

    PUCHAR  Buffer = (PUCHAR)malloc(ulInputSize);
    if (NULL == Buffer) {
        ulInputSize = 0;
        return NULL;
    }
    ZeroMemory(Buffer, ulInputSize);


    TAG_COMMIT_NOTIFY_INPUT *ptrCommitTagNotifyInput = (PTAG_COMMIT_NOTIFY_INPUT)Buffer;

    StringCchPrintfA((CHAR*)ptrCommitTagNotifyInput->TagGUID, ARRAYSIZE(ptrCommitTagNotifyInput->TagGUID), "%s", TagGuid.c_str());
    ptrCommitTagNotifyInput->ulNumDisks = ulNumDiskForDriverOutput;


    std::vector<std::wstring>::iterator devIter = protectedDiskIds.begin();
    int devIndex = 0;
    for (/*empty*/; devIter != protectedDiskIds.end(); devIter++, devIndex++)
    {
        StringCchPrintfW(ptrCommitTagNotifyInput->DeviceId[devIndex],
            ARRAYSIZE(ptrCommitTagNotifyInput->DeviceId[devIndex]), L"%s", devIter->c_str());
    }

    return Buffer;
}

void InitializeService(HANDLE hEvent)
{
    m_hInmageCtrlDevice = CreateFile(
        INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
        );

    // m_platformAPIs->OpenDefault(m_wszInmageCtrlDevice);
    if (INVALID_HANDLE_VALUE == m_hInmageCtrlDevice)
    {
        wprintf(L"%s failed to open device err=0x%x\n", INMAGE_DISK_FILTER_DOS_DEVICE_NAME, GetLastError());
        exit(-1);
    }

    SHUTDOWN_NOTIFY_INPUT   inputData = { 0 };
    OVERLAPPED              overlapped = { 0 };
    overlapped.hEvent = hEvent;

    inputData.ulFlags |= ~SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES;
    inputData.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;

    DWORD           dwBytes;
    DWORD           dwErr;

    if (!DeviceIoControl(m_hInmageCtrlDevice, 
                        (DWORD)IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY, 
                        &inputData, 
                        sizeof(inputData), NULL, 0, &dwBytes, &overlapped))
    {
        dwErr = GetLastError();
        if (ERROR_IO_PENDING != dwErr)
        {
            wprintf(L"IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY failed with err=0x%x\n", dwErr);
            CloseHandle(m_hInmageCtrlDevice);
            exit(-1);
        }
    }

    wprintf(L"Registered for service shutdown notify\n");
    PROCESS_START_NOTIFY_INPUT input = { 0 };
    OVERLAPPED              overlapped1 = { 0 };
    overlapped1.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!DeviceIoControl(m_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_PROCESS_START_NOTIFY,
        &input,
        sizeof(input), NULL, 0, &dwBytes, &overlapped1))
    {
        dwErr = GetLastError();
        if (ERROR_IO_PENDING != dwErr)
        {
            wprintf(L"IOCTL_INMAGE_PROCESS_START_NOTIFY failed with err=0x%x\n", dwErr);
            CloseHandle(m_hInmageCtrlDevice);
            exit(-1);
        }
    }

    wprintf(L"Registered for process start notify\n");
}

DWORD __stdcall TestCommitNotify(_In_ LPVOID lpParameter)
{
    ULONG   ulInputSize = 0;
    void *Buffer = GetTagCommitNotifyBuffer(g_protectedDeviceIds, ulInputSize, g_TagGuid);
    if (NULL == Buffer) {
        printf("Failed to get Tag Buffer");
        return -1;
    }

    ULONG   ulNumDisks = g_protectedDeviceIds.size();

    std::vector<char>       outBuffer(FIELD_OFFSET(TAG_COMMIT_NOTIFY_OUTPUT, TagStatus[ulNumDisks]));
    DWORD       dwBytes;
    OVERLAPPED  overlapped = { 0 };
    HANDLE  hEvent = CreateEvent(NULL, true, false, NULL);
    overlapped.hEvent = hEvent;
    PTAG_COMMIT_NOTIFY_OUTPUT   pCommitNotifyOutput = (PTAG_COMMIT_NOTIFY_OUTPUT)&outBuffer[0];

    if (DeviceIoControl(m_hInmageCtrlDevice,
        IOCTL_INMAGE_TAG_COMMIT_NOTIFY,
        Buffer,
        ulInputSize,
        &outBuffer[0],
        FIELD_OFFSET(TAG_COMMIT_NOTIFY_OUTPUT, TagStatus[ulNumDisks]),
        &dwBytes,
        &overlapped
    )) {
        printf("IOCTL_INMAGE_TAG_COMMIT_NOTIFY failed with error=%d\n", GetLastError());
        return -1;
    }

    DWORD dwError = GetLastError();
    if (ERROR_IO_PENDING != dwError) {
        printf("IOCTL_INMAGE_TAG_COMMIT_NOTIFY failed with error=%d\n", GetLastError());
        return dwError;
    }

    printf("Waiting for commit for tag: %s\n", g_TagGuid.c_str());
    DWORD dwStatus = WaitForSingleObject(hEvent, INFINITE);
    if (WAIT_OBJECT_0 != dwStatus) {
        switch (dwStatus) {
        case WAIT_TIMEOUT:
            printf("Wait Timedout for commit notify Tag\n");
            break;
        case WAIT_ABANDONED:
            printf("Wait Abandoned for commit notify Tag\n");
            break;
        case WAIT_FAILED:
            printf("Wait failed for commit notify Tag\n");
            break;
        default:
            printf("Wait failed for commit notify Tag with unknown status=%d\n", dwStatus);
        }

        CloseHandle(hEvent);
        return 0;
    }

    BOOL bResult = GetOverlappedResult(m_hInmageCtrlDevice,
        &overlapped,
        &dwBytes,
        FALSE);
    CloseHandle(hEvent);

    if (!bResult) {
        dwError = GetLastError();
        printf("**FAILED**************error: %d out: %d\n", dwError, pCommitNotifyOutput->ulNumDisks);
        return 0;
    }

    if (dwBytes != FIELD_OFFSET(TAG_COMMIT_NOTIFY_OUTPUT, TagStatus[ulNumDisks])) {
        printf("**FAILED**************\
                    Error = %d\
                    output: out: %d Expected Size: %lu size: %lu\n", 
                    overlapped.Internal,
                    pCommitNotifyOutput->ulNumDisks,
                    FIELD_OFFSET(TAG_COMMIT_NOTIFY_OUTPUT, TagStatus[ulNumDisks]),
                    dwBytes);
        return 0;
    }

    printf("error = %d\n", overlapped.Internal);
    printf("**SUCCESS**************output: out: %d\n", pCommitNotifyOutput->ulNumDisks);

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        wprintf(L"Usage: %s [<disk-id1> <disk-id 2.....]\n", argv[0]);
        return 1;
    }

    PHANDLE  hThreadArray = new HANDLE[argc - 1];
    PDWORD  hThreadIds = new DWORD[argc - 1];
    if (NULL == hThreadArray || NULL == hThreadIds) {
        printf("Failed to allocate handle\n");
        return -1;
    }

    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    InitializeService(hEvent);

    for (int i = 0; i < argc-1; i++)
    {
        int diskNum = _wtol(argv[i+1]);
        wprintf(L"argv: %s %d\n", argv[i+1], diskNum);
        hThreadArray[i] = CreateThread(NULL, 0, DrainThread, argv[i+1], 0, &hThreadIds[i]);
        if (hThreadArray[i] == NULL)
        {
            wprintf(L"CreateThread failed with err=0x%x\n", GetLastError());
            exit(3);
        }
    }
    Sleep(30 * 1000);
    DWORD id;
    HANDLE hThread = CreateThread(NULL, 0, TestCommitNotify, NULL, 0, &id);
    if (hThread == NULL)
    {
        wprintf(L"CreateThread TestCommitNotify failed with err=0x%x\n", GetLastError());
        exit(3);
    }
    Sleep(30 * 1000);

    std::list<std::string> taglist;
    taglist.push_back(g_TagGuid);

    //CInmageIoctlController *pIoctlController = new CInmageIoctlController();
    //pIoctlController->LocalCrashConsistentTagIoctl(g_TagGuid, taglist, (ULONG)TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, 100);

    DWORD dwStatus = WaitForSingleObject(hThread, INFINITE);
    if (WAIT_OBJECT_0 != dwStatus) {
        wprintf(L"WaitForSingleObject TestCommitNotify failed with err=0x%x\n", GetLastError());
        exit(3);

    }

    dwStatus = WaitForMultipleObjects(argc - 1, hThreadArray, FALSE, INFINITE);
    if (WAIT_FAILED == dwStatus) {
        printf("Wait failed for %d threads error=%d\n", argc-1,GetLastError());
        return 1;
    }
    if (dwStatus != (WAIT_OBJECT_0 + argc - 1))
        printf("dwStatus = %d\n", dwStatus);

    for (int i = 1; i < argc; i++)
    {
        CloseHandle(hThreadArray[i - 1]);
    }

    delete hThreadArray;
    delete hThreadIds;
	return 0;
}

