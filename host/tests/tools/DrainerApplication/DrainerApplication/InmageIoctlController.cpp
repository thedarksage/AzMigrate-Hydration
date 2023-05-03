// Microsoft.Test.Inmage.IoctlController.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "InmageIoctlController.h"
#include <strsafe.h>
#include <vector>
#include <map>

// Private constructor for CInmageIoctlController
CInmageIoctlController::CInmageIoctlController() 
{
    printf("Entering: %s", __FUNCTION__);

    s_hInmageCtrlDevice = CreateFile(
        INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
        );

    if (INVALID_HANDLE_VALUE == s_hInmageCtrlDevice)
    {
        throw CAgentException("failed to open inmage control device err=0x%x", GetLastError());
    }
    printf("Exiting: %s", __FUNCTION__);
}

CInmageIoctlController::~CInmageIoctlController()
{
    if (INVALID_HANDLE_VALUE != s_hInmageCtrlDevice)
    {
#ifdef WINDOWS
        ::CancelIo(s_hInmageCtrlDevice);
#endif
        CloseHandle(s_hInmageCtrlDevice);
    }
}

void CInmageIoctlController::ServiceShutdownNotify(ULONG ulFlags)
{
    m_shutdownNotify = CreateEvent(NULL, true, false, NULL);

    OVERLAPPED shutdownOverlapped = { 0 };
    shutdownOverlapped.hEvent = m_shutdownNotify;

    SHUTDOWN_NOTIFY_INPUT   inputData = { 0 };

    inputData.ulFlags = ulFlags;
    //inputData.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;

    DWORD dwBytes;

    bool bErr = DeviceIoControl(s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
        &inputData,
        sizeof(inputData),
        NULL,
        0,
        &dwBytes,
        &shutdownOverlapped);

    if (!bErr && (ERROR_IO_PENDING != GetLastError()))
    {
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }
}


void CInmageIoctlController::ProcessStartNotity(ULONG ulFlags)
{
    m_processNotify = CreateEvent(NULL, true, false, NULL);

    OVERLAPPED processNotifyOverlapped = { 0 };
    processNotifyOverlapped.hEvent = (void*) m_processNotify;

    PROCESS_START_NOTIFY_INPUT inputData = { 0 };
    DWORD                        dwBytes = 0;
    void*                     output = NULL;
    inputData.ulFlags = ulFlags;

    bool bErr = DeviceIoControl(s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_PROCESS_START_NOTIFY,
        &inputData,
        sizeof(inputData),
        NULL,
        0,
        &dwBytes,
        &processNotifyOverlapped);

    if (!bErr && (ERROR_IO_PENDING != GetLastError()))
    {
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }
}

void CInmageIoctlController::GetDriverVersion(void *lpOutBuffer, _In_ DWORD nOutBufferSize)
{
    DWORD                        dwBytes = 0;

    if (!DeviceIoControl(s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_GET_VERSION,
        NULL,
        0,
        lpOutBuffer,
        nOutBufferSize,
        &dwBytes,
        NULL))
    {
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }
}

void CInmageIoctlController::StartFiltering(std::string deviceId)
{
    DWORD                        dwBytes = 0;
    START_FILTERING_INPUT       startFilteringInput = { 0 };

    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());

    StringCchPrintf(startFilteringInput.DeviceId, ARRAYSIZE(startFilteringInput.DeviceId), L"%s", wcsDeviceId.c_str());

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
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
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
}

void CInmageIoctlController::StopFiltering(std::string deviceId, bool delBitmapFile, bool bStopAll)
{
    DWORD                        dwBytes = 0;
    STOP_FILTERING_INPUT       stopFilteringInput = { 0 };

    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());

    if (bStopAll){
        stopFilteringInput.ulFlags |= STOP_ALL_FILTERING_FLAGS;
    }

    if (delBitmapFile){
        stopFilteringInput.ulFlags |= STOP_FILTERING_FLAGS_DELETE_BITMAP;
    }

    if (!wcsDeviceId.empty()){
        StringCchPrintf(stopFilteringInput.DevID, ARRAYSIZE(stopFilteringInput.DevID), L"%s", wcsDeviceId.c_str());
    }

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_STOP_FILTERING_DEVICE,
        &stopFilteringInput,
        sizeof(stopFilteringInput),
        NULL,
        0,
        &dwBytes,
        NULL
        ))
    {
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }
}

void CInmageIoctlController::ResyncStartNotification(std::string deviceId)
{
    DWORD                        dwBytes = 0;
    RESYNC_START_INPUT  Input = { 0 };
    RESYNC_START_OUTPUT_V2 Output = { 0 };

    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());
    StringCchPrintf(Input.DevID, ARRAYSIZE(Input.DevID), L"%s", wcsDeviceId.c_str());

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_RESYNC_START_NOTIFICATION,
        &Input,
        sizeof(Input),
        &Output,
        sizeof(Output),
        &dwBytes,
        NULL
        ))
    {
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }
}

void CInmageIoctlController::ResyncEndNotification(std::string deviceId)
{
    DWORD                        dwBytes = 0;
    RESYNC_END_INPUT  Input = { 0 };
    RESYNC_END_OUTPUT_V2 Output = { 0 };

    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());
    StringCchPrintf(Input.DevID, ARRAYSIZE(Input.DevID), L"%s", wcsDeviceId.c_str());

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_RESYNC_END_NOTIFICATION,
        &Input,
        sizeof(Input),
        &Output,
        sizeof(Output),
        &dwBytes,
        NULL
        ))
    {
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }

}

void CInmageIoctlController::ClearDifferentials(std::string deviceId)
{
    DWORD                        dwBytes = 0;
    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
        (void*)wcsDeviceId.c_str(),
        wcsDeviceId.length() * sizeof(wchar_t),
        NULL,
        0,
        &dwBytes,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, GetLastError());
    }

}

void CInmageIoctlController::GetDirtyBlockTransaction(PCOMMIT_TRANSACTION pCommitTrans, PUDIRTY_BLOCK_V2  pDirtyBlock, bool bThowIfResync, std::string deviceId)
{
    DWORD dwReturn;

    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());

    if (NULL == pCommitTrans)
    {
        throw CAgentException("%s: IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS Invalid pCommitTrans", __FUNCTION__);
    }

    DWORD dwInLength = (NULL == pCommitTrans) ? 0 : sizeof(*pCommitTrans);

    ZeroMemory(pDirtyBlock, sizeof(*pDirtyBlock));

    StringCchPrintf(pCommitTrans->DevID, ARRAYSIZE(pCommitTrans->DevID), L"%s", wcsDeviceId.c_str());

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
        pCommitTrans,
        dwInLength,
        pDirtyBlock,
        sizeof(UDIRTY_BLOCK_V2),
        &dwReturn,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        throw CAgentException("%s: IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS failed with err=0x%x", __FUNCTION__, GetLastError());
    }

    if (sizeof(UDIRTY_BLOCK_V2) != dwReturn)
    {
        throw CAgentException("%s Err: IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS dwReturn %d != UDIRTY_BLOCK_V2 size = %d", __FUNCTION__, dwReturn, sizeof(UDIRTY_BLOCK_V2));
    }

    if (pDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)
    {
        throw CAgentResyncRequiredException(pDirtyBlock);
    }
}

void CInmageIoctlController::RegisterForSetDirtyBlockNotify(std::string deviceId, const void* hEvent)
{
    DWORD            dwReturn;

    VOLUME_DB_EVENT_INFO    EventInfo;
    ZeroMemory(&EventInfo, sizeof(EventInfo));

    std::wstring wcsDeviceId(deviceId.begin(), deviceId.end());
    StringCchPrintfW(EventInfo.DevID,
        ARRAYSIZE(EventInfo.DevID),
        L"%s", wcsDeviceId.c_str());

    EventInfo.hEvent = (void*) hEvent;

    // Created new event have to do the IOCTL to set the event.
    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT,
        &EventInfo,
        sizeof(VOLUME_DB_EVENT_INFO),
        NULL,
        0,
        &dwReturn,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        throw CAgentException("%s: IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT failed with err=0x%x", __FUNCTION__, GetLastError());
    }
}

/*
Input Tag Stream Structure for disk devices
_____________________________________________________________________________________________________________________________________________________________
|  Context   |             |          |                |                 |    |              |              |         |          |     |         |           |
| GUID(UCHAR)|   Flags     |timeout   | No. of GUIDs(n)|  Volume GUID1   | ...| Volume GUIDn | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
|_(36 Bytes)_|____(4 Bytes)|_(4 Bytes)|_ (4 Bytes)_____|___(36 Bytes)____|____|______________|___(4 bytes)__|(2 bytes)|______________________________________|

*/
std::vector<char> CInmageIoctlController::GetAppTagBuffer(
    std::list<std::string>    diskIds,
    std::string          tagContext,
    std::list<std::string>    taglist,
    ULONG        ulFlags,
    ULONG        timeout,
    ULONG&       totalSize)
{
    int                 numTags = taglist.size();
    ULONG            numDisks = diskIds.size();
    std::vector<char>   Buffer;

    if ((0 <= numDisks) || (0 <= numTags)) {
        // return an empty vector
        return std::vector<char>();
    }

    totalSize = GUID_SIZE_IN_CHARS;
    totalSize += sizeof(ULONG); // ulFlags
    totalSize += sizeof(ULONG); // timeout
    totalSize += sizeof(ULONG); // Number of Disk Guid
    totalSize += numDisks * GUID_SIZE_IN_BYTES; // Total size for disk guids
    totalSize += sizeof(ULONG); // Tag Length

    for (auto tag : taglist) {
        totalSize += sizeof(USHORT);
        totalSize += tag.size();
    }

    Buffer.resize(totalSize);
    char*   pBuffer = &Buffer[0];

    ULONG    ulCurPosition = 0;
    StringCchPrintfA(pBuffer, GUID_SIZE_IN_CHARS, "%s", tagContext.c_str());
    ulCurPosition += GUID_SIZE_IN_CHARS;

    // Copy Flags
    *((ULONG *)(pBuffer + ulCurPosition)) = ulFlags;
    ulCurPosition += sizeof(ULONG);

    // Copy Timeout
    *((ULONG *)(pBuffer + ulCurPosition)) = timeout;
    ulCurPosition += sizeof(ULONG);

    // Copy number of disk guids
    *((ULONG *)(pBuffer + ulCurPosition)) = numDisks;
    ulCurPosition += sizeof(ULONG);

    // Copy each of disk guids
    for (auto diskguid : diskIds) {
        std::wstring    wcsId(diskguid.begin(), diskguid.end());
        StringCchPrintfW((WCHAR*) pBuffer, GUID_SIZE_IN_BYTES, L"%s", wcsId.c_str());
        ulCurPosition += GUID_SIZE_IN_BYTES;
    }

    // Copy Tag Length
    *((ULONG *)(pBuffer + ulCurPosition)) = taglist.size();
    ulCurPosition += sizeof(ULONG);

    for (auto tag : taglist) {
        *((USHORT *)(pBuffer + ulCurPosition)) = tag.size();
        ulCurPosition += sizeof(USHORT);

        StringCchPrintfA(pBuffer, tag.size(), "%s", tag.c_str());
        ulCurPosition += tag.size();
    }

    if (ulCurPosition != totalSize) {
        throw CAgentException("CInmageIoctlController::GetAppTagBuffer  ulCurPosition = %d != totalSize = %d", ulCurPosition, totalSize);
    }

    return Buffer;
}

void GetDataBufferAllocationLength(ULONG ulDataLength, ULONG *pOutAllocLength)
{
    if (pOutAllocLength == NULL)
        return;

    ULONG pTotalLength = 0;

    if (ulDataLength <= ((ULONG)0xFF - sizeof(STREAM_REC_HDR_4B))) {
        pTotalLength = ulDataLength + sizeof(STREAM_REC_HDR_4B);
    }
    else {
        pTotalLength = ulDataLength + sizeof(STREAM_REC_HDR_8B);
    }

    *pOutAllocLength = pTotalLength;
}

std::vector<char> CInmageIoctlController::GetTagBuffer(std::string tagContext, std::list<std::string> taglist, ULONG ulFlags, ULONG timeout, int& totalSize)
{
    int numTags = taglist.size();
    CRASHCONSISTENT_TAG_INPUT crashInput;

    StringCchPrintfA((STRSAFE_LPSTR)crashInput.TagContext, sizeof(crashInput.TagContext), "%s", tagContext.c_str());
    crashInput.ulFlags = ulFlags;
    crashInput.ulNumOfTags = numTags;



    totalSize = sizeof(CRASHCONSISTENT_TAG_INPUT);// +sizeof(ULONG);// numTags * sizeof(USHORT);

    std::list<std::string>::iterator it = taglist.begin();
    for (; taglist.end() != it; ++it)
    {
        totalSize +=  (it->length() + sizeof(USHORT));
    }

    std::vector<char>    buffer(totalSize);

    ZeroMemory(&buffer[0], totalSize);

    // Copy TagContext first
    memcpy(&buffer[0], &crashInput, sizeof(crashInput));

    char* pBuffer = &buffer[0] + sizeof(crashInput) + sizeof(ULONG);

    it = taglist.begin();
    for (; taglist.end() != it; ++it)
    {
        *(USHORT*)pBuffer = it->length();
        pBuffer += sizeof(USHORT);
        memcpy(pBuffer, it->c_str(), it->length());
        pBuffer += it->length();
    }

    return buffer;
}

bool CInmageIoctlController::LocalCrashConsistentTagIoctl(std::string tagContext, std::list<std::string> taglist, ULONG ulFlags, ULONG ulTimeoutInMS)
{
    bool status = true;
    int totalSize;
    DWORD dwBytes = 0;
    CRASHCONSISTENT_TAG_OUTPUT output = { 0 };

    std::vector<char> Buffer = GetTagBuffer(tagContext, taglist, ulFlags, ulTimeoutInMS, totalSize);

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_IOBARRIER_TAG_DISK,
        &Buffer[0],
        totalSize,
        &output,
        sizeof(output),
        &dwBytes,
        NULL
        ))
    {
        DWORD dwErr = GetLastError();
        printf("IOCTL_INMAGE_IOBARRIER_TAG_DISK failed with err=0x%x", GetLastError());

        status = false;
    }
    else
    {
        printf("IOCTL_INMAGE_IOBARRIER_TAG_DISK  : Success");
    }

    return status;
}

void CInmageIoctlController::CommitTransaction(PCOMMIT_TRANSACTION pCommitTransaction)
{
    DWORD                        dwBytes = 0;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
        pCommitTransaction,
        sizeof(COMMIT_TRANSACTION),
        NULL,
        0,
        &dwBytes,
        NULL
        ))
    {
        DWORD    dwErr = GetLastError();
        throw CAgentException("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
}

/*void CInmageIoctlController::HoldWrites(std::string tagContext, ULONG ulFlags, ULONG ulTimeoutInMS)
{
    CRASH_CONSISTENT_HOLDWRITE_INPUT    holdWritesIn;
    OVERLAPPED                            overlapped = { 0 };
    std::map<std::string, boost::shared_ptr<Event>>::iterator it;
    DWORD    dwBytes;

    ZeroMemory(&holdWritesIn, sizeof(holdWritesIn));

    if (tagContext.length() < ARRAYSIZE(holdWritesIn.TagContext)) {
        throw CAgentException("%s: Invalid input tag context %s length: %d", __FUNCTION__, tagContext.c_str(), tagContext.length());
    }

    memcpy(holdWritesIn.TagContext, tagContext.c_str(), ARRAYSIZE(holdWritesIn.TagContext));
    holdWritesIn.ulFlags = ulFlags;
    holdWritesIn.ulTimeout = ulTimeoutInMS;

    it = m_distCrashEvent.find(tagContext);
    if (m_distCrashEvent.end() != it){
        if (it->second){
            SafeClose((void*) it->second->Self());
        }
        m_distCrashEvent.erase(tagContext);
    }

    m_distCrashEvent[tagContext].reset(new Event(false, true, std::string()));
    overlapped.hEvent = (void *) m_distCrashEvent[tagContext]->Self();

    BOOL bStatus = DeviceIoControl(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_HOLD_WRITES,
        &holdWritesIn,
        sizeof(holdWritesIn),
        NULL,
        0,
        &dwBytes,
        &overlapped
        );

    DWORD dwErr = GetLastError();
    
    if (bStatus || (ERROR_IO_PENDING != dwErr)){        
        throw CAgentException("%s: IOCTL_INMAGE_HOLD_WRITES failed with error = 0x%x", __FUNCTION__, dwErr);
    }
}*/

void CInmageIoctlController::InsertDistTag(std::string tagContext, ULONG ulFlags, std::list<std::string> taglist)
{
    PCRASHCONSISTENT_TAG_INPUT    crashTagIn;
    std::list<std::string>::iterator        it;
    ULONG                    ulSize;
    DWORD                        dwBytes;
    CRASHCONSISTENT_TAG_OUTPUT    tagOutput;
    char*                        pBuffer;

    ulSize = sizeof(CRASHCONSISTENT_TAG_INPUT)+sizeof(USHORT)*taglist.size();
    for (it = taglist.begin(); it != taglist.end(); it++){
        ulSize += it->length();
    }

    std::vector<char>    buffer(ulSize);
    crashTagIn = (PCRASHCONSISTENT_TAG_INPUT)&buffer[0];

    ZeroMemory(crashTagIn, ulSize);

    if (tagContext.length() < ARRAYSIZE(crashTagIn->TagContext)) 
    {
        throw CAgentException("%s: Invalid input tag context %s length: %d", __FUNCTION__, tagContext.c_str(), tagContext.length());
    }

    memcpy(crashTagIn->TagContext, tagContext.c_str(), ARRAYSIZE(crashTagIn->TagContext));
    crashTagIn->ulFlags = ulFlags;
    crashTagIn->ulNumOfTags = taglist.size();

    pBuffer = (char*) &crashTagIn->TagBuffer[0];

    for (it = taglist.begin(); taglist.end() != it; ++it)
    {
        *(USHORT*)pBuffer = it->length();
        pBuffer += sizeof(USHORT);
        memcpy(pBuffer, it->c_str(), it->length());
        pBuffer += it->length();
    }

    BOOL bStatus = DeviceIoControl(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG,
        crashTagIn,
        ulSize,
        &tagOutput,
        sizeof(tagOutput),
        &dwBytes,
        NULL
        );

    if (!bStatus){
        DWORD dwErr = GetLastError();
        throw CAgentException("%s: IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG failed with error = 0x%x", __FUNCTION__, dwErr);
    }
}

void CInmageIoctlController::ReleaseWrites(std::string tagContext, ULONG ulFlags)
{
    CRASH_CONSISTENT_INPUT    releaseWritesIn;
    ULONG                dwBytes;

    if (tagContext.length() < ARRAYSIZE(releaseWritesIn.TagContext)) {
        throw CAgentException("%s: Invalid input tag context %s length: %d", __FUNCTION__, tagContext.c_str(), tagContext.length());
    }
    
    memcpy(releaseWritesIn.TagContext, tagContext.c_str(), ARRAYSIZE(releaseWritesIn.TagContext));
    releaseWritesIn.ulFlags = ulFlags;

    BOOL bStatus = DeviceIoControl(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_RELEASE_WRITES,
        &releaseWritesIn,
        sizeof(releaseWritesIn),
        NULL,
        0,
        &dwBytes,
        NULL
        );

    if (!bStatus){
        DWORD dwErr = GetLastError();
        throw CAgentException("%s: IOCTL_INMAGE_RELEASE_WRITES failed with error = 0x%x", __FUNCTION__, dwErr);
    }
}

void CInmageIoctlController::CommitTag(std::string tagContext, ULONG ulFlags)
{
    CRASH_CONSISTENT_INPUT    commitTagIn;
    DWORD                    dwBytes;

    if (tagContext.length() < ARRAYSIZE(commitTagIn.TagContext)) {
        throw CAgentException("%s: Invalid input tag context %s length: %d", __FUNCTION__, tagContext.c_str(), tagContext.length());
    }

    memcpy(commitTagIn.TagContext, tagContext.c_str(), ARRAYSIZE(commitTagIn.TagContext));
    commitTagIn.ulFlags = ulFlags;

    BOOL bStatus = DeviceIoControl(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG,
        &commitTagIn,
        sizeof(commitTagIn),
        NULL,
        0,
        &dwBytes,
        NULL
        );

    if (!bStatus){
        DWORD dwErr = GetLastError();
        throw CAgentException("%s: IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG failed with error = 0x%x", __FUNCTION__, dwErr);
    }
}

void CInmageIoctlController::GetLcn(std::string fileName, void* retBuff, int size)
{
    DWORD   dwBytes;

    std::wstring wcsFileName(fileName.begin(), fileName.end());

    GET_LCN getLcn = { 0 };
    StringCchPrintf(getLcn.FileName, ARRAYSIZE(getLcn.FileName), L"%s", wcsFileName.c_str());
    getLcn.usFileNameLength = wcsFileName.length() * sizeof(WCHAR);

    BOOL bStatus = DeviceIoControl(
        s_hInmageCtrlDevice,
        IOCTL_INMAGE_GET_LCN,
        &getLcn,
        sizeof(getLcn),
        retBuff,
        size,
        &dwBytes,
        NULL);

    if (!bStatus){
        DWORD dwErr = GetLastError();
        throw CAgentException("%s: IOCTL_INMAGE_GET_LCN failed with error = 0x%x", __FUNCTION__, dwErr);
    }
}

void CInmageIoctlController::SetDriverFlags(ULONG ulFlags, etBitOperation flag)
{
    DWORD                        dwBytes = 0;
    DRIVER_FLAGS_INPUT  DriverFlagsInput;
    DRIVER_FLAGS_OUTPUT DriverFlagsOutput;

    DriverFlagsInput.ulFlags = ulFlags;
    DriverFlagsInput.eOperation = flag;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_DRIVER_FLAGS,
        &DriverFlagsInput,
        sizeof(DRIVER_FLAGS_INPUT),
        &DriverFlagsOutput,
        sizeof(DRIVER_FLAGS_OUTPUT),
        &dwBytes,
        NULL
        ))
    {
        DWORD    dwErr = GetLastError();
        throw new CAgentException("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
}

void CInmageIoctlController::SetDriverConfig(DRIVER_CONFIG DriverInConfig, DRIVER_CONFIG &DriverOutConfig)
{
    DWORD                        dwBytes = 0;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_DRIVER_CONFIGURATION,
        &DriverInConfig,
        sizeof(DriverInConfig),
        &DriverOutConfig,
        sizeof(DriverOutConfig),
        &dwBytes,
        NULL
        ))
    {
        DWORD    dwErr = GetLastError();
        printf("%s failed with error=0x%x", __FUNCTION__, dwErr);
        throw new CAgentException("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
    else
    {
        printf("IOCTL_INMAGE_SET_DRIVER_CONFIGURATION  : Success");
    }
}

void CInmageIoctlController::SetReplicationState(PVOID buffer, int size)
{
    REPLICATION_STATE   replicationState = { 0 };
 
    DWORD               dwBytes = 0;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_REPLICATION_STATE,
        buffer,
        size,
        NULL,
        0,
        &dwBytes,
        NULL)) {
        DWORD    dwErr = GetLastError();
        throw new CAgentException("%s failed with error=0x%x", __FUNCTION__, dwErr);

    }
}

void CInmageIoctlController::GetReplicationStats(std::string deviceId, REPLICATION_STATS& stats)
{
    DWORD               dwBytes = 0;
    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_SET_REPLICATION_STATE,
        NULL,
        0,
        &stats,
        sizeof(stats),
        &dwBytes,
        NULL)) 
    {
        DWORD    dwErr = GetLastError();
        throw new CAgentException("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
}


DWORD CInmageIoctlController::AppConsistentTagIoctl(std::list<std::string> diskIds, std::string tagContext, std::list<std::string> taglist, ULONG ulFlags, ULONG ulTimeoutInMS)
{
    std::vector<char>   tagBuffer;
    ULONG            ulTotalSize = 0;
    ULONG            ulTotalOutput = SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(diskIds.size());


    // GetAppTagBuffer(std::list<std::string> diskIds, std::string tagContext, std::list<std::string> taglist, ULONG ulFlags, ULONG timeout, int& totalSize)
    tagBuffer = GetAppTagBuffer(diskIds, tagContext, taglist, ulFlags, ulTimeoutInMS, ulTotalSize);

    if (tagBuffer.empty()) {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    DWORD                   dwBytes = 0;
    std::vector<UCHAR>      outBuffer(ulTotalOutput);
    DWORD    dwErr;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_TAG_DISK,
        &tagBuffer[0],
        ulTotalSize,
        &outBuffer[0],
        ulTotalOutput,
        &dwBytes,
        NULL))
    {
        dwErr = GetLastError();
       printf("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
    else {
        dwErr = ERROR_SUCCESS;
    }

    return dwErr;
}

DWORD CInmageIoctlController::AppConsistentTagCommitRevokeIoctl(std::string tagContext, ULONG ulFlags, std::list<std::string> diskIds, bool commit, std::vector<ULONG>& output)
{
    TAG_DISK_COMMIT_INPUT   input;

    StringCchPrintfA((char*)input.TagContext, sizeof(input.TagContext), "%s", tagContext.c_str());
    input.ulFlags = ulFlags;

    if (commit) {
        input.ulFlags |= TAG_INFO_FLAGS_COMMIT_TAG;
    }

    output.resize(SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(diskIds.size()));

    DWORD   dwBytes;
    DWORD    dwErr;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_COMMIT_TAG_DISK,
        &input,
        sizeof(input),
        &output[0],
        output.size(),
        &dwBytes,
        NULL))
    {
        dwErr = GetLastError();
       printf("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
    else {
        dwErr = GetLastError();
    }
    return dwErr;
}

DWORD CInmageIoctlController::AppConsistentTagIoctl(void* input, ULONG ulInSize, void *output, ULONG ulOutSize, ULONG& ulOutBytes)
{
    DWORD    dwErr;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_TAG_DISK,
        input,
        ulInSize,
        output,
        ulOutSize,
        &ulOutBytes,
        NULL))
    {
        dwErr = GetLastError();
       printf("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
    else {
        dwErr = GetLastError();
    }
    return dwErr;
}

DWORD CInmageIoctlController::AppConsistentTagCommitRevokeIoctl(void* input, ULONG ulInSize, void *output, ULONG ulOutSize, ULONG& ulOutBytes)
{
    DWORD    dwErr;

    if (!DeviceIoControl(
        s_hInmageCtrlDevice,
        (DWORD)IOCTL_INMAGE_COMMIT_TAG_DISK,
        input,
        ulInSize,
        output,
        ulOutSize,
        &ulOutBytes,
        NULL))
    {
        dwErr = GetLastError();
       printf("%s failed with error=0x%x", __FUNCTION__, dwErr);
    }
    else {
        dwErr = GetLastError();
    }
    return dwErr;
}