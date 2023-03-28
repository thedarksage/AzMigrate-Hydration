#define _WIN32_WINNT 0x0501

#include<windows.h>
#include<stdio.h>
#include"Involflt.h"
#include"main.h"

void
ClearDiffs(HANDLE hDriver,WCHAR *volGUID);

int GetVolumeGUIDFromDriveLetter(char DriveLetter,WCHAR* volumeGUID,ULONG ulBufLen) {
	WCHAR MountPoint[4];
	WCHAR UniqueVolumeName[60];

	if(ulBufLen < (GUID_SIZE_IN_CHARS*sizeof(WCHAR))) {
		return 0;
	}

	MountPoint[0] = (WCHAR) DriveLetter; MountPoint[1] = L':';
	MountPoint[2] = L'\\'; MountPoint[3] = L'\0';

	if(!GetVolumeNameForVolumeMountPointW((WCHAR *)MountPoint,
										(WCHAR *)UniqueVolumeName,
										60)) {
		printf("GUIDFromDriveLetter..Failed\n");
		return 0;
	}

	memcpy(volumeGUID,&UniqueVolumeName[11],(GUID_SIZE_IN_CHARS*sizeof(WCHAR)));

	return 1;
}

void GenerateTagPrefixBuffer(char *buf,
							 ULONG *ulLen,
							 ULONG bAtomic,
							 int nVolumes,
							 WCHAR *volGuids) {
	ULONG ulFlags;
	ULONG nBytesCopied = 0;

	if(bAtomic) 
		ulFlags = TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;
	else 
		ulFlags = 0;
    // Need to decide what these flags will be..TBD
	memcpy(buf,&ulFlags,sizeof(ULONG));
	nBytesCopied += sizeof(ULONG);

	memcpy(buf+nBytesCopied,&nVolumes,sizeof(ULONG));
	nBytesCopied += sizeof(ULONG);

	memcpy(buf+nBytesCopied,volGuids,nVolumes*GUID_SIZE_IN_CHARS*sizeof(WCHAR));
    nBytesCopied += nVolumes*GUID_SIZE_IN_CHARS*sizeof(WCHAR);
	*ulLen = nBytesCopied;
}

ULONG SendShutdownNotify(HANDLE hDriver,LPOVERLAPPED povl) {
	SHUTDOWN_NOTIFY_INPUT stShutdownNotifyInput;
	ULONG bResult;
	DWORD dwReturn;
	memset(&stShutdownNotifyInput, 0, sizeof(SHUTDOWN_NOTIFY_INPUT));
	stShutdownNotifyInput.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;
	stShutdownNotifyInput.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES;

	bResult = DeviceIoControl(hDriver,
							 (DWORD)IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY,
							 &stShutdownNotifyInput,
							 sizeof(SHUTDOWN_NOTIFY_INPUT),
							 NULL,
							 0,
							 &dwReturn, 
							 povl); 
    if (0 == bResult) {
        if (ERROR_IO_PENDING != GetLastError())
        {
            _tprintf(_T("SendShutdownNotification: DeviceIoControl failed with error = %d\n"), GetLastError());
			return 0;
        }
    }
	return 1;
}

ULONG SendProcessStartNotify(HANDLE hDriver,LPOVERLAPPED povl) {
	ULONG bResult;
	DWORD dwReturn;
    PROCESS_START_NOTIFY_INPUT pInput;

    #define PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE   0x0001
    pInput.ulFlags = PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE;

	bResult = DeviceIoControl(
                     hDriver,
                     (DWORD)IOCTL_INMAGE_PROCESS_START_NOTIFY,
                     &pInput,
                     sizeof(pInput),
                     NULL,
                     0,
                     &dwReturn, 
                     povl     // Overlapped
                     ); 
    if (0 == bResult) {
        if (ERROR_IO_PENDING != GetLastError())
        {
            _tprintf(_T("SendServiceStartNotification: DeviceIoControl failed with error = %d\n"), GetLastError());
			return 0;
        }
    }
	return 1;
}

BOOL WriteDirtyBlockToFile(PUDIRTY_BLOCK pDirtyBlock)
{
    ULONG ChangesInStream = pDirtyBlock->uHdr.Hdr.ulcbChangesInStream;
    ULONG i = 0;
    ULONG ChangesToWrite = 0, ChangesWritten = 0;
    HANDLE hDataFile;

    hDataFile = CreateFileW(pDirtyBlock->uTagList.DataFile.FileName, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    
    if(INVALID_HANDLE_VALUE == hDataFile)
        return FALSE;

    while(i < pDirtyBlock->uHdr.Hdr.usNumberOfBuffers)
    {
        PVOID BufferAddress = pDirtyBlock->uHdr.Hdr.ppBufferArray[i++];
        if(ChangesInStream > pDirtyBlock->uHdr.Hdr.ulBufferSize)
        {
            ChangesInStream -= pDirtyBlock->uHdr.Hdr.ulBufferSize;
            ChangesToWrite = pDirtyBlock->uHdr.Hdr.ulBufferSize;
        }
        else
        {
            ChangesToWrite = ChangesInStream;
            ChangesInStream = 0;
        }

        WriteFile(hDataFile, BufferAddress, ChangesToWrite, &ChangesWritten, NULL);
    }

    CloseHandle(hDataFile);
    
    pDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_DATA_FILE;

    return TRUE;
}

ULONG GetNextDB(HANDLE hDriver,ULONGLONG ulTransId,WCHAR *volGuid,PUDIRTY_BLOCK pstDirtyBlock) {
	COMMIT_TRANSACTION	stCommitTrans;
	ULONG				bResult;
	DWORD				dwReturn;

	stCommitTrans.ulTransactionID.QuadPart = ulTransId;
	memcpy(stCommitTrans.VolumeGUID,volGuid,GUID_SIZE_IN_CHARS*sizeof(WCHAR));
	bResult = DeviceIoControl(hDriver,
							  IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
							  &stCommitTrans,
							  sizeof(COMMIT_TRANSACTION),
							  pstDirtyBlock,
							  sizeof(UDIRTY_BLOCK),
							  &dwReturn,
							  NULL);

    if(0 == pstDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart)
        return true;

	if(UDIRTY_BLOCK_FLAG_SVD_STREAM & pstDirtyBlock->uHdr.Hdr.ulFlags)
    {
        if(!WriteDirtyBlockToFile(pstDirtyBlock))
            return FALSE;
		pstDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_DATA_FILE;
		pstDirtyBlock->uHdr.Hdr.ppBufferArray = NULL;
    }

    if(pstDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE)
    {
        FillDirtyBlockFromFile(pstDirtyBlock);
    }

    _tprintf(_T("TransactionID:%4u"), pstDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart);
    if(pstDirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE)
        _tprintf(_T(":File Block:"));

    switch(pstDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource)
    {
    case INVOLFLT_DATA_SOURCE_DATA:
        printf("Data Source:Data Block\n");
        break;
    case INVOLFLT_DATA_SOURCE_META_DATA:
        printf("Data Source:Meta Data\n");
        break;
    case INVOLFLT_DATA_SOURCE_BITMAP:
        printf("Data Source:Bitmap\n");
        break;
    case INVOLFLT_DATA_SOURCE_UNDEFINED:
        printf("Data Source:Undefined\n");
        break;
    default:
        printf("Data Source:Error\n");
    }

	return bResult;
}

ULONG CommitDb(HANDLE hDriver,ULONGLONG ulTransId,WCHAR *volGuid) {
	COMMIT_TRANSACTION	stCommitTrans;
	ULONG				bResult;
	DWORD				dwReturn;

	stCommitTrans.ulTransactionID.QuadPart = ulTransId;
	memcpy(stCommitTrans.VolumeGUID,volGuid,GUID_SIZE_IN_CHARS*sizeof(WCHAR));
	bResult = DeviceIoControl(hDriver,
							  IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS,
							  &stCommitTrans,
							  sizeof(COMMIT_TRANSACTION),
							  NULL,
							  0,
							  &dwReturn,
							  NULL);
	return bResult;
}

//ULONG ClearExistingData(HANDLE hDriver,WCHAR *volGUID) {
//	UDIRTY_BLOCK stDirtyBlock;
//
//	memset(&stDirtyBlock,0,sizeof(UDIRTY_BLOCK));
//	while(1) {
//		if(!GetNextDB(hDriver,
//					  stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart,
//					  volGUID,
//					  &stDirtyBlock)) {
//			printf("ClearExistingData::Failed getting DB\n");
//			return 0;
//		}
//
//		if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource != INVOLFLT_DATA_SOURCE_UNDEFINED) {
//			continue;
//		}
//		break;
//	}
//	return 1;
//}

ULONG ClearExistingData(HANDLE hDriver,WCHAR *volGUID) {
	UDIRTY_BLOCK stDirtyBlock;

	memset(&stDirtyBlock,0,sizeof(UDIRTY_BLOCK));
    //ClearDiffs(hDriver, volGUID);
	while(1) {
		
        if(!GetNextDB(hDriver,
					  stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart,
					  volGUID,
					  &stDirtyBlock)) {
			printf("ClearExistingData::Failed getting DB\n");
			return 0;
		}

		if(stDirtyBlock.uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE)
        {
            //Free dirty block buffer array
            for(ULONG i = 0; i < stDirtyBlock.uHdr.Hdr.cChanges; i++)
            {
                free(stDirtyBlock.uHdr.Hdr.ppBufferArray[stDirtyBlock.Changes[i].usBufferIndex]);
            }

            free(stDirtyBlock.uHdr.Hdr.ppBufferArray);
            stDirtyBlock.uHdr.Hdr.ppBufferArray = NULL;
        }

		if(stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart != 0) {
			continue;
		}
		break;
	}
	return 1;
}

void
ClearDifferentials(HANDLE hDriver,WCHAR *volGUID)
{
    // Add one more char for NULL.
    BOOL    bResult;
    DWORD   dwReturn;

    bResult = DeviceIoControl(
                     hDriver,
                     (DWORD)IOCTL_INMAGE_CLEAR_DIFFERENTIALS,
                     volGUID,
                     sizeof(WCHAR) * GUID_SIZE_IN_CHARS,
                     NULL,
                     0,
                     &dwReturn, 
                     NULL        // Overlapped
                     );
    if (bResult)
        printf("Returned Success from Clear Bitmap DeviceIoControl call for volume %ws:\n", volGUID);
    else
        printf("Returned Failed from Clear Bitmap DeviceIoControl call for volume %ws: %d\n", 
                 volGUID, GetLastError());

    return;
}
