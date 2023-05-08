// to enable compilation of code for GetVolumeNameForVolumeMountPointW...MSDN says this 
// API is available only on Windows versions after XP
#define _WIN32_WINNT 0x0501

#include<stdio.h>
#include<windows.h>
#include<time.h>
#include"InvolFlt.h"
#include"main.h"
#include"rapid_test.h"
#include"rand_gen.h"

DWORD WINAPI WriteStreamR(LPVOID pArg) {
	PWRITE_THREAD_PARAMS_R pstWriteThreadParams;
	HANDLE				   hVolume = INVALID_HANDLE_VALUE;
	ULARGE_INTEGER         ullFileSize;
	LARGE_INTEGER          temp;
	DWORD				   fileType;
	char*				   buff = NULL;
	VOLUME_DISK_EXTENTS	   vd;
	ULONG				   nios_done=0;
	ULONGLONG			   offset;
	ULONG				   size;
	ULONG				   ulLen;

	pstWriteThreadParams = (PWRITE_THREAD_PARAMS_R) pArg;

	// We will allocate the maximum-sized buffer we might use for this thread
	buff = (char *) malloc(pstWriteThreadParams->ulMaxIOSz);
	if(buff == NULL) {
		printf("Insufficient Memory for the thread..\n");
		return 0;
	}
	memset(buff,0,pstWriteThreadParams->ulMaxIOSz);

	// Get a handle on the volume
	hVolume = CreateFile(pstWriteThreadParams->strVolumeSymbolicLinkName,
						 GENERIC_READ | GENERIC_WRITE,
						 FILE_SHARE_WRITE | FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 FILE_FLAG_NO_BUFFERING,
						 NULL);
	printf("CreateFile:Error=%x\n",GetLastError());
	if(hVolume == INVALID_HANDLE_VALUE) {
		printf("Couldn't obtain handle on %ws, error=%x\n",pstWriteThreadParams->strVolumeSymbolicLinkName,GetLastError());
		free(buff);
		return 0;
	}

	if(!DeviceIoControl(hVolume,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,NULL,0,
		&vd,sizeof(VOLUME_DISK_EXTENTS),&fileType,NULL)) {
		printf("IOCTL_DISK_GET_LENGTH_INFO, error=%x\n",GetLastError());
		CloseHandle(hVolume);
		free(buff);
		return 0;
	}

	ullFileSize.QuadPart = vd.Extents[0].ExtentLength.QuadPart;
	
	srand((unsigned int)time(NULL));
	while(nios_done < pstWriteThreadParams->ulNMaxIOs) {
		size = pstWriteThreadParams->ulMinIOSz+
				GenRandSize(pstWriteThreadParams->ulMaxIOSz-pstWriteThreadParams->ulMinIOSz);
		size = size-(size%TEST_SECTOR_SIZE);
		offset = GenRandOffset(ullFileSize.QuadPart-size+1);
		offset = offset-(offset%TEST_SECTOR_SIZE);
		// do the write after seeking to the offset
		temp.QuadPart = offset;
		if(!SetFilePointerEx(hVolume,temp,NULL,FILE_BEGIN)) {
			printf("Couldnt set File Pointer\n");
			free(buff);
			CloseHandle(hVolume);
			return 0;
		}
		if(!WriteFile(hVolume,
						buff,
						size,
						&ulLen,
						NULL)) {
			printf("WriteFile, error=%x\n",GetLastError());
			CloseHandle(hVolume);
			free(buff);
			return 0;
		}
		pstWriteThreadParams->ullTotalData+=(size>>10);
		nios_done++;
	}
	// Free the local buffers we allocated
	CloseHandle(hVolume);
	free(buff);
	printf("Total Data Written by the Thread is %dkb\n",pstWriteThreadParams->ullTotalData);
	return 1;
}

DWORD WINAPI ReadStreamR(LPVOID pArg) {
#define MAX_TRIES_FOR_DB_WITH_CHANGES 40
	UDIRTY_BLOCK		   stDirtyBlock;
	ULONG				   ntries;
	PREAD_THREAD_PARAMS_R  pstReadThreadParams;

	pstReadThreadParams = (PREAD_THREAD_PARAMS_R) pArg;
	ntries=0;
	memset(&stDirtyBlock,0,sizeof(UDIRTY_BLOCK));
	while(1) {
		if(!GetNextDB(pstReadThreadParams->hDriver,
					  stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart,
					  pstReadThreadParams->volGUID,
					  &stDirtyBlock)) {
			printf("ReadStream::Failed getting DB\n");
			break;
		}
		/*printf("tnid=%I64d changes=%d\n",stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart,
			stDirtyBlock.uHdr.Hdr.cChanges);*/
		if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource != INVOLFLT_DATA_SOURCE_UNDEFINED) {
			Sleep(20);
			ntries=0;
		}
		else {
			ntries++;
			if(ntries == MAX_TRIES_FOR_DB_WITH_CHANGES) {
				printf("Exceed maximum tries to get a DB with changes\n");
				break;
			}
			Sleep(100);
			continue;
		}
	}
    return 1;
}

int RapidTest(HANDLE hDriver, PRAPID_TEST_CMD pstRapidTestParams) {
	READ_THREAD_PARAMS_R  stReadThreadParams;
	WRITE_THREAD_PARAMS_R stWriteThreadParams;
	WCHAR				  volGUID[sizeof(WCHAR)*(1+GUID_SIZE_IN_CHARS)];
	HANDLE				  hThreads[2];
	DWORD				  ret;
	time_t				  s,e;
	
	s = time(NULL);
	if(!GetVolumeGUIDFromDriveLetter(pstRapidTestParams->stWriteStreamParams.DriveLetter,volGUID,
								sizeof(WCHAR)*(1+GUID_SIZE_IN_CHARS))) {
		printf("couldnt obtain GUID for %c:\n",pstRapidTestParams->stWriteStreamParams.DriveLetter);
		return 0;
	}

	/* clearing existing data for the volume */
	if(!ClearExistingData(hDriver,volGUID)) {
		printf("Error while clearing existing data for %c:\n",pstRapidTestParams->stWriteStreamParams.DriveLetter);
		return 0;
	}

	printf("Cleared existing data for %c:\n",pstRapidTestParams->stWriteStreamParams.DriveLetter);

	memset(&stWriteThreadParams,0,sizeof(WRITE_THREAD_PARAMS_R));
	stWriteThreadParams.strVolumeSymbolicLinkName[0] = L'\\';
	stWriteThreadParams.strVolumeSymbolicLinkName[1] = L'\\';
	stWriteThreadParams.strVolumeSymbolicLinkName[2] = L'.';
	stWriteThreadParams.strVolumeSymbolicLinkName[3] = L'\\';
	stWriteThreadParams.strVolumeSymbolicLinkName[4] = (WCHAR)pstRapidTestParams->stWriteStreamParams.DriveLetter;
	stWriteThreadParams.strVolumeSymbolicLinkName[5] = L':';
	stWriteThreadParams.strVolumeSymbolicLinkName[6] = L'\0';
	stWriteThreadParams.ullTotalData = 0;
	stWriteThreadParams.ulMaxIOSz = pstRapidTestParams->stWriteStreamParams.ulMaxIOSz;
	stWriteThreadParams.ulMinIOSz = pstRapidTestParams->stWriteStreamParams.ulMinIOSz;
	stWriteThreadParams.ulNMaxIOs = pstRapidTestParams->stWriteStreamParams.ulNMaxIOs;

	memset(&stReadThreadParams,0,sizeof(READ_THREAD_PARAMS_R));
	stReadThreadParams.hDriver = hDriver;
	memcpy(stReadThreadParams.volGUID,volGUID,GUID_SIZE_IN_CHARS*sizeof(WCHAR));

	hThreads[0] = CreateThread(NULL,
							    0,
							    WriteStreamR,
							    &stWriteThreadParams,
							    0,
							    NULL);

	hThreads[1] = CreateThread(NULL,
							   0,
							   ReadStreamR,
							   &stReadThreadParams,
							   0,
							   NULL);
	ret = WaitForMultipleObjects(2,hThreads,TRUE,INFINITE);
	printf("RapidTest::WaitForMultipleObjects %d\n",ret);
	e = time(NULL);
	printf("the test took %dsec\n",e-s-4);
	return 1;
}