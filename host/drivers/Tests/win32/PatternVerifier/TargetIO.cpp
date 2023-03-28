#include "StdAfx.h"
#include <string.h>
#include <stdlib.h>

TargetIO::TargetIO()
{
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    shutdownOverlapped.Offset = 0;
    shutdownOverlapped.OffsetHigh = 0;
    shutdownOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    SetLastError(0);

    pipeHandle = NULL;

}

void TargetIO::SetPath(char * path)
{
    strcpy_s(pathname, _countof(pathname), path);
}

BOOLEAN TargetIO::VerifyIsRAWVolume()
{
char fsName[512];
char fsVolume[512];
TCHAR dummyBuffer[256];

    // check it's a raw volume
    strcpy_s(fsVolume, _countof(fsVolume), pathname);
    strcat_s(fsVolume, _countof(fsVolume), "\\");
    if (GetVolumeInformation(fsVolume, dummyBuffer, sizeof(dummyBuffer), 
                (PDWORD)&dummyBuffer, (PDWORD)&dummyBuffer,(PDWORD)&dummyBuffer, fsName, sizeof(fsName))) {
        printf("ERROR:This utility will destroy the %s file system on volume %s\n",fsName,pathname);
        return FALSE;
    } else {
        printf("Writing to RAW volume\n");
    }

    return TRUE;
}

DWORD TargetIO::Open()
{
DWORD status;

    status = ERROR_SUCCESS;

    handle = CreateFile(pathname,
                        GENERIC_ALL,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
                        NULL);
    if (handle == INVALID_HANDLE_VALUE)
        status = GetLastError();

    DeviceIoControl(handle, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0, &status, NULL);

    SetLastError(ERROR_SUCCESS);

    return status;
}

DWORD TargetIO::Close()
{
DWORD status;

    status = ERROR_SUCCESS;

    CloseHandle(handle);
    SetLastError(0);
 
    return status;
}

DWORD TargetIO::Write(__int64 offset, int size, void * buffer)
{
DWORD status;
DWORD written;

    overlapped.Offset = (DWORD)(offset & 0xFFFFFFFFL);
    overlapped.OffsetHigh = (DWORD)(offset >> 32L);
    SetLastError(ERROR_SUCCESS);

    if (WriteFile(handle, buffer, size, &written, &overlapped)) {
            status = ERROR_SUCCESS;
    } else {
        status = GetLastError();
        SetLastError(ERROR_SUCCESS);
       if (status == ERROR_IO_PENDING) {
            if (GetOverlappedResult(handle, &overlapped, &written, TRUE))
                status = ERROR_SUCCESS;
            else
                status = GetLastError();
        }
    }

    SetLastError(ERROR_SUCCESS);

    return status;
}

DWORD TargetIO::Read(__int64 offset, int size, void * buffer, int * bytesRead)
{
DWORD status;
DWORD read;

    overlapped.Offset = (DWORD)(offset & 0xFFFFFFFFL);
    overlapped.OffsetHigh = (DWORD)(offset >> 32L);
    SetLastError(ERROR_SUCCESS);
    read = 0;

    if (ReadFile(handle, buffer, size, &read, &overlapped)) {
            status = ERROR_SUCCESS;
    } else {
        status = GetLastError();
        SetLastError(ERROR_SUCCESS);
       if (status == ERROR_IO_PENDING) {
            if (GetOverlappedResult(handle, &overlapped, &read, TRUE))
                status = ERROR_SUCCESS;
            else
                status = GetLastError();
        }
    }

    SetLastError(ERROR_SUCCESS);

    *bytesRead = read;

    return status;
}


DWORD TargetIO::GetDirtyBlocks(PDIRTY_BLOCKS dirtyBlocks)
{
DWORD status;
DWORD returnedBytes;
VOLUME_STATS_DATA volumeStats;
COMMIT_TRANSACTION commitInfo;
    
    ZeroMemory((void *)dirtyBlocks, sizeof(DIRTY_BLOCKS));

    if (pipeHandle) {
        status = GetDirtyBlocksFromRemote(dirtyBlocks);
    } else {
        do {
            if (DeviceIoControl(handle, IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS, NULL, 0, dirtyBlocks, sizeof(DIRTY_BLOCKS), &returnedBytes, &overlapped)) {
                status = ERROR_SUCCESS;
            } else {
                status = GetLastError();
                SetLastError(ERROR_SUCCESS);
                if (status == ERROR_IO_PENDING) {
                    if (GetOverlappedResult(handle, &overlapped, &returnedBytes, TRUE))
                        status = ERROR_SUCCESS;
                    else
                        status = GetLastError();
                }
            }
            if (dirtyBlocks->cChanges > 0) { // returned some changes
                commitInfo.ulTransactionID.QuadPart = dirtyBlocks->uliTransactionID.QuadPart;
                if (DeviceIoControl(handle, IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS, &commitInfo, sizeof(COMMIT_TRANSACTION), NULL, 0, &returnedBytes, &overlapped)) {
                    status = ERROR_SUCCESS;
                } else {
                    status = GetLastError();
                    SetLastError(ERROR_SUCCESS);
                    if (status == ERROR_IO_PENDING) {
                        if (GetOverlappedResult(handle, &overlapped, &returnedBytes, TRUE))
                            status = ERROR_SUCCESS;
                        else
                            status = GetLastError();
                    }
                }
                break;
            }

            // no data yet, see if any data will be coming
            if (DeviceIoControl(handle, IOCTL_INMAGE_GET_VOLUME_STATS, NULL, 0, &volumeStats, sizeof(VOLUME_STATS_DATA), &returnedBytes, &overlapped)) {
                status = ERROR_SUCCESS;
            } else {
                status = GetLastError();
                SetLastError(ERROR_SUCCESS);
                if (status == ERROR_IO_PENDING) {
                    if (GetOverlappedResult(handle, &overlapped, &returnedBytes, TRUE))
                        status = ERROR_SUCCESS;
                    else
                        status = GetLastError();
                }
            }
            if ((!(volumeStats.VolumeArray[0].ulVolumeFlags & VSF_BITMAP_READ_COMPLETE)) ||
                (volumeStats.VolumeArray[0].ulPendingChanges > 0) ||
                (volumeStats.VolumeArray[0].ulChangesQueuedForWriting > 0)) {
                // there may be data coming
                Sleep(10);
            } else {
                break;
            }
        } while (dirtyBlocks->cChanges == 0);
    }

    SetLastError(ERROR_SUCCESS);

    return status;
}

DWORD TargetIO::GetDirtyBlocksFromRemote(PDIRTY_BLOCKS dirtyBlocks)
{
BOOLEAN fSuccess;
DWORD localSize;
DWORD xferSize;

    localSize = sizeof(DIRTY_BLOCKS);
    fSuccess = TransactNamedPipe( 
                    pipeHandle,
                    &localSize,
                    sizeof(localSize),
                    &dirtyBlocks,
                    sizeof(DIRTY_BLOCKS),
                    &xferSize,
                    NULL);

    return fSuccess;
}

DWORD TargetIO::OpenRemote(char * path)
{
DWORD status;
char fullPath[1024];
DWORD mode;

    status = ERROR_SUCCESS;

    strcpy_s(fullPath, _countof(fullPath), "\\\\");
    strcat_s(fullPath, _countof(fullPath), path);
    strcat_s(fullPath, _countof(fullPath), "\\pipe\\InVolFlt");

    pipeHandle = CreateFile(fullPath,
                        GENERIC_ALL,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        status = GetLastError();
    } else {
        mode = PIPE_READMODE_MESSAGE; 
        SetNamedPipeHandleState(pipeHandle, &mode, NULL, NULL); 
    }

    SetLastError(ERROR_SUCCESS);

    return status;
}

#define TEST_GET_EXTENTS 0
#define TEST_BINARY_SEARCH 0
#define TEST_COMPLETE_BINARY_SEARCH 0
#define PRINT_SIZES 0

DWORD TargetIO::GetVolumeSize(LONGLONG * volumeSize)
{
#define MAXIMUM_SECTORS_ON_VOLUME (1024i64 * 1024i64 * 1024i64 * 1024i64)
#define DISK_SECTOR_SIZE (512)

    PVOLUME_DISK_EXTENTS pDiskExtents; // dynamically sized allocation based on number of extents
    GET_LENGTH_INFORMATION  lengthInfo;
    DWORD bytesReturned;
    DWORD returnBufferSize;
    DWORD status;
    BOOLEAN fSuccess;
    LONGLONG currentGap;
    LONGLONG currentOffset;
    LARGE_INTEGER largeInt;
    PUCHAR sectorBuffer;

    // try a simple IOCTl to get size, should work on WXP and later
    fSuccess = DeviceIoControl(handle,
                               IOCTL_DISK_GET_LENGTH_INFO,
                               NULL, 
                               0,
                               &lengthInfo, 
                               sizeof(lengthInfo),
                               &bytesReturned,
                               NULL);
#if (PRINT_SIZES)
    printf("Volume size via IOCTL_DISK_GET_LENGTH_INFO is hex:%I64X or decimal:%I64i\n", lengthInfo.Length.QuadPart, lengthInfo.Length.QuadPart);
#endif

#if (TEST_GET_EXTENTS || TEST_BINARY_SEARCH || TEST_COMPLETE_BINARY_SEARCH)
    fSuccess = FALSE;
#endif
    if (fSuccess) {
         *volumeSize = lengthInfo.Length.QuadPart;
         return ERROR_SUCCESS;
    }

    // next attempt, see if the volume is only 1 extent, if yes use it
    returnBufferSize = sizeof(VOLUME_DISK_EXTENTS);
    pDiskExtents = (PVOLUME_DISK_EXTENTS) new UCHAR[returnBufferSize];
    fSuccess =DeviceIoControl(handle,
                              IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                              NULL,
                              0,
                              pDiskExtents,
                              returnBufferSize,
                              &bytesReturned,
                              NULL);
#if (PRINT_SIZES)
    printf("Volume size via IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS is hex:%I64X or decimal:%I64i\n", pDiskExtents->Extents[0].ExtentLength.QuadPart, pDiskExtents->Extents[0].ExtentLength.QuadPart);
#endif
#if (TEST_BINARY_SEARCH || TEST_COMPLETE_BINARY_SEARCH)
    fSuccess = FALSE;
#endif
    if (fSuccess) {
        // must only have 1 extent as the buffer only has space for one
        assert(pDiskExtents->NumberOfDiskExtents == 1);
        *volumeSize = pDiskExtents->Extents[0].ExtentLength.QuadPart;
        delete pDiskExtents;
        return ERROR_SUCCESS;
    }

    // use harder ways of finding the size
    status = GetLastError();
#if (TEST_BINARY_SEARCH)
    status = ERROR_MORE_DATA;
#endif
    if (ERROR_MORE_DATA != status) {
        // some error other than buffer too small happened
        delete pDiskExtents;
        return status;
    }

    // has multiple extents
    returnBufferSize = sizeof(VOLUME_DISK_EXTENTS) + ((pDiskExtents->NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT));
    delete pDiskExtents;
    pDiskExtents = (PVOLUME_DISK_EXTENTS) new UCHAR[returnBufferSize];
    fSuccess = DeviceIoControl(handle,
                               IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                               NULL, 0,
                               pDiskExtents, returnBufferSize,
                               &bytesReturned,
                               NULL);
#if (TEST_COMPLETE_BINARY_SEARCH)
    fSuccess = FALSE;
#endif
    if (!fSuccess) {
        currentOffset = MAXIMUM_SECTORS_ON_VOLUME; // if IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, do a binary search with a large limit
    } else {
        // must be multiple extents, so will have to binary search last valid sector
       
        currentOffset = 0;
        for(unsigned int i = 0;i < pDiskExtents->NumberOfDiskExtents;i++) {
            currentOffset += pDiskExtents->Extents[i].ExtentLength.QuadPart;
        }

        // scale things now so we don't need to divide on every read
        currentOffset += DISK_SECTOR_SIZE; // these two are needed to make the search algorithm work
        currentOffset /= DISK_SECTOR_SIZE;
    }

    delete pDiskExtents;

    // the search gap MUST be a power of two, so find an appropriate value
    currentGap = 1i64;
    while ((currentGap * 2i64) < currentOffset) {
        currentGap *= 2i64;
    }

    // we are all ready to binary search for the last valid sector
    sectorBuffer = (PUCHAR)VirtualAlloc(NULL, DISK_SECTOR_SIZE, MEM_COMMIT, PAGE_READWRITE); 
    do {
        largeInt.QuadPart = currentOffset * DISK_SECTOR_SIZE;
        SetFilePointerEx(handle,
                         largeInt,
                         NULL,
                         FILE_BEGIN);

        fSuccess = ReadFile(handle,
                            sectorBuffer,
                            DISK_SECTOR_SIZE,
                            &bytesReturned,
                            NULL);

        if (fSuccess && (bytesReturned == DISK_SECTOR_SIZE)) {
            currentOffset += currentGap;
            if (currentGap == 0) {
                *volumeSize = (currentOffset * DISK_SECTOR_SIZE) + DISK_SECTOR_SIZE;
                status = ERROR_SUCCESS;
                break;
            }
        } else {
            currentOffset -= currentGap;
            if (currentGap == 0) {
                *volumeSize =  currentOffset * DISK_SECTOR_SIZE;
                status = ERROR_SUCCESS;
                break;
            }
        }
        currentGap /= 2;

    } while (TRUE);

    VirtualFree(sectorBuffer, DISK_SECTOR_SIZE, MEM_DECOMMIT);

#if (PRINT_SIZES)
    printf("Volume size via binary search is hex:%I64X or decimal:%I64i\n", *volumeSize, *volumeSize);
#endif

    return status;
}


DWORD TargetIO::OpenShutdownNotify()
{
DWORD status;

    SetLastError(ERROR_SUCCESS);
    DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &shutdownHandle, GENERIC_ALL, FALSE, 0);
    SetLastError(ERROR_SUCCESS);

    if (DeviceIoControl(shutdownHandle, IOCTL_SVSYS_SERVICE_SHUTDOWN_NOTIFY, NULL, 0, NULL, 0, NULL, &shutdownOverlapped)) {
        status = ERROR_SUCCESS;
    } else {
        status = GetLastError();
    }

    SetLastError(ERROR_SUCCESS);

    return status;

}

DWORD TargetIO::CloseShutdownNotify()
{
DWORD status;

    SetLastError(ERROR_SUCCESS);
    if (CancelIo(shutdownHandle))
        status = ERROR_SUCCESS;
    else
        status = GetLastError();

    CloseHandle(shutdownHandle);

    SetLastError(ERROR_SUCCESS);

    return status;
}



