#ifndef _SIMULATED_TEST_H_
#define _SIMULATED_TEST_H_

// to enable compilation of code for GetVolumeNameForVolumeMountPointW...MSDN says this 
// API is available only on Windows versions after XP
#define _WIN32_WINNT 0x0501

#include<stdio.h>
#include<windows.h>
#include<time.h>
#include"InvolFlt.h"
#include"main.h"
#include"simulated_test.h"
#include"rand_gen.h"
#include "svdparse.h"
#include "assert.h"
#include "limits.h"
#include <string.h>
#include <stdlib.h>

#define DEFAULT_DATA_RATE_TUNING_STEP_DEFAULT 1
#define DEFAULT_DATA_THRESHOLD_FOR_VOLUME_DEFAULT 64
#define DEFAULT_DATA_RATE_THRESHOLD_FOR_VOLUME 20
#define LOG_FILE_PATH   ".\\Log.txt"

WRITE_ORDER_TEST_INFO stWriteOrderTestInfo;

char *exit_reason_strings[EXIT_MAX_REASONS+1] = {
	"Simulated Test Completed Sucessfully",
	"Exited before polling to get DBs",
	"The IO with seq num %d came in out of order",
	"The offset is not matching for IO with seq num %d",
	"The IO with seq num %d containts corrupt data",
	"Not enough Buffers are returned for the IO with seq num %d"
	"Expecting Start of change whereas part or end of split change arrived",
	"Expecting Part or End of change whereas start of change arrived",
	"The split DB has more than one change",
	"Split change for IO with seq num %d has arrived totally in Start of Split change DB",
	"Split change for IO with seq num %d has arrived totally in Part of Split change DB",
	"Split change for IO with seq num %d hasn't completely arrived even in End of Split change DB",
	"Normal change not completely arrived in DB for io with seq num %d",
	"Exited after waiting enough time to get a DB with changes",
	"Exited because of switchover of mode to MetaData or Bitmap",
	"Exited because of failure in getting DB",
	"Exited because of an Unknown Reason"
};

// For now gets plain index in the PER_IO_INFO Circular List of the Volume
// Assumes that caller is holding the lock on the VolumeInfo
int GenerateIOSeqNumForVolume(PPER_VOLUME_INFO pstPerVolumeInfo) {
	int t;
	t = pstPerVolumeInfo->ulNextActiveIOIndex;
	return t;
}

void GenerateTagBufferPrefix(char* buf,
							ULONG *ulLen) {
	ULONG i;
	ULONG ulFlags=TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;
	ULONG nBytesCopied = 0;

    // Need to decide what these flags will be..TBD
	memcpy(buf,&ulFlags,sizeof(ULONG));
	nBytesCopied += sizeof(ULONG);

	memcpy(buf+nBytesCopied,&stWriteOrderTestInfo.nVolumes,sizeof(ULONG));
	nBytesCopied += sizeof(ULONG);

	for(i=0;i<MAX_VOLUMES;i++) {
		if(!stWriteOrderTestInfo.arActiveVolumes[i])
			continue;
		memcpy(buf+nBytesCopied,stWriteOrderTestInfo.arstPerVolumeInfo[i].volumeGUID,
                                    GUID_SIZE_IN_CHARS*sizeof(WCHAR));
        nBytesCopied += GUID_SIZE_IN_CHARS*sizeof(WCHAR);
	}
	*ulLen = nBytesCopied;
}

void GenerateTagBuffer(char             *buf,
					   ULONG            *ulLen,
                       PULONG           pulTagSeqNums) {
	//STREAM_REC_HDR_4B stStreamRec;
	ULONG ulSeqNum;
	ULONG nBytesCopied = 0;
	ULONG nTotalTags = 1;
	PPER_VOLUME_INFO pstPerVolumeInfo;
	int i;

	memcpy(buf+nBytesCopied,stWriteOrderTestInfo.TagBufferPrefix,stWriteOrderTestInfo.TagBufferPrefixLength);
	nBytesCopied += stWriteOrderTestInfo.TagBufferPrefixLength;

    memcpy(buf+nBytesCopied,&nTotalTags,sizeof(ULONG));
	nBytesCopied += sizeof(ULONG);

	for(i=0;i<MAX_VOLUMES;i++) {
		if(!stWriteOrderTestInfo.arActiveVolumes[i])
			continue;
		pstPerVolumeInfo = &stWriteOrderTestInfo.arstPerVolumeInfo[i];
		WaitForSingleObject(pstPerVolumeInfo->hActiveIOsMutex,INFINITE);
		ulSeqNum = GenerateIOSeqNumForVolume(pstPerVolumeInfo);
		ReleaseMutex(pstPerVolumeInfo->hActiveIOsMutex);
        pulTagSeqNums[i] = ulSeqNum;
		USHORT tagLen=sizeof(ULONG);
		memcpy(buf+nBytesCopied,&tagLen,sizeof(USHORT));
		nBytesCopied += sizeof(USHORT);
		memcpy(buf+nBytesCopied,&ulSeqNum,sizeof(ulSeqNum));
		nBytesCopied += sizeof(ulSeqNum);
	}
	*ulLen = nBytesCopied;
}

void GenerateDataBuffer(char *buff,
						ULONG ulMinSize,
						ULONG ulMaxSize,
						LONGLONG ulMaxVolSize,
						PWRITE_THREAD_PARAMS pstWriteThreadParams,
						PPER_IO_INFO pstPerIOInfo,
						BOOL Sequential = FALSE) {
	ULONG nBytesCopied=0;
	pstPerIOInfo->ulLen = ulMinSize+GenRandSize(ulMaxSize-ulMinSize);
    // To make the length a multiple of 512, which is sizeof one sector
    pstPerIOInfo->ulLen = pstPerIOInfo->ulLen - (pstPerIOInfo->ulLen%TEST_SECTOR_SIZE);
	pstPerIOInfo->ullOffset = Sequential? (pstWriteThreadParams->LastWrittenOffset + pstPerIOInfo->ulLen) % ulMaxVolSize : GenRandOffset(ulMaxVolSize-pstPerIOInfo->ulLen+1);
    // to make Offset multiple of 512
    pstPerIOInfo->ullOffset = pstPerIOInfo->ullOffset - (pstPerIOInfo->ullOffset%TEST_SECTOR_SIZE);
	pstPerIOInfo->ulPattern = GenRandPattern();
	memcpy(buff+nBytesCopied,&pstPerIOInfo->ulIOSeqNum,sizeof(pstPerIOInfo->ulIOSeqNum));
	nBytesCopied += sizeof(pstPerIOInfo->ulIOSeqNum);
	memcpy(buff+nBytesCopied,&pstPerIOInfo->ulThreadId,sizeof(pstPerIOInfo->ulThreadId));
	nBytesCopied += sizeof(pstPerIOInfo->ulThreadId);

	if(pstWriteThreadParams->bDataTest) {
		ULONG i;
		PULONG plBuff = (PULONG) (buff+nBytesCopied);
		for(i=0;i<(pstPerIOInfo->ulLen-nBytesCopied)/4;i++) {
			plBuff[i] = pstPerIOInfo->ulPattern;
		}
	}
	else {
		pstPerIOInfo->ulPattern = 0;
	}

	pstWriteThreadParams->LastWrittenOffset += pstPerIOInfo->ulLen;
}

// Will always be called while holding lock on the PervolumeInfo
void AddNewIOInfoToVolume(PPER_VOLUME_INFO pstPerVolumeInfo, 
						  PPER_IO_INFO	   pstPerIOInfo) {
    PPER_IO_INFO pstPerIOInfo1;
    pstPerIOInfo1 = &pstPerVolumeInfo->arstActiveIOs[pstPerVolumeInfo->ulNextActiveIOIndex];
	memcpy(pstPerIOInfo1,pstPerIOInfo,sizeof(PER_IO_INFO));
	pstPerIOInfo1->bArrived = 0;
	pstPerVolumeInfo->ulNActiveIOs++;
	pstPerVolumeInfo->ulNextActiveIOIndex = 
			(pstPerVolumeInfo->ulNextActiveIOIndex+1)%MAX_ACTIVE_IOS_PER_VOLUME;
}

int VerifyIOArrivalOrderForVolume(PPER_VOLUME_INFO pstPerVolumeInfo, 
								  ULONG			   bTag,
								  PPER_IO_INFO	   pstPerIOInfo) {
	int i;
    // Verify if the IoSeqNum for the IO is valid, should lie within the first active IO
    // To the last one
    // Taking care of circularity of the Array
    if(pstPerVolumeInfo->ulStartActiveIOIndex < pstPerVolumeInfo->ulNextActiveIOIndex) {
        if(pstPerIOInfo->ulIOSeqNum < pstPerVolumeInfo->ulStartActiveIOIndex)
            return 0;
        if(pstPerIOInfo->ulIOSeqNum >= pstPerVolumeInfo->ulNextActiveIOIndex)
            return 0;
    }
    else {
        if((pstPerIOInfo->ulIOSeqNum >= pstPerVolumeInfo->ulNextActiveIOIndex) &&
           (pstPerIOInfo->ulIOSeqNum < pstPerVolumeInfo->ulStartActiveIOIndex)) {
            return 0;
        }
    }

    if(bTag != pstPerVolumeInfo->arstActiveIOs[pstPerIOInfo->ulIOSeqNum].bTag) {
        printf("Tag/Data mismatch\n");
        return 0;
    }

	if(pstPerVolumeInfo->arstActiveIOs[pstPerIOInfo->ulIOSeqNum].bArrived) {
		printf("IO with seqnum %d has allready arrived\n",pstPerIOInfo->ulIOSeqNum);
		return 0;
	}

	// Iterate through all the IOS prior to this pstPerIOInfo->ulIOSeqNum and perform
	// Write Order check
	for(i=pstPerVolumeInfo->ulStartActiveIOIndex;
		i!=pstPerIOInfo->ulIOSeqNum;i=(i+1)%MAX_ACTIVE_IOS_PER_VOLUME) {
			if(pstPerVolumeInfo->arstActiveIOs[i].bArrived) {
			continue;
		}
		if(bTag) {
			// The one that arrived is a tag and this IO which is prior to tag
			// has not yet arrived...so out-of-order
			return 0;
		}
		// If this IO is from the same thread as pstPerIOInfo, it is supposed to have arrived
		// by now
		if(pstPerIOInfo->ulThreadId == 
			pstPerVolumeInfo->arstActiveIOs[i].ulThreadId) {
			return 0;
		}
	}

    // Need to give all the info about this IO to the caller
    memcpy(pstPerIOInfo,&pstPerVolumeInfo->arstActiveIOs[pstPerIOInfo->ulIOSeqNum],
                                sizeof(PER_IO_INFO));
	// This IO has arrived in proper order
	return 1;
}

void UpdateCommitedIOForVolume(PPER_VOLUME_INFO pstPerVolumeInfo,
							   ULONG			ulSeqNum) {
	WaitForSingleObject(pstPerVolumeInfo->hActiveIOsMutex,INFINITE);
	
	pstPerVolumeInfo->arstActiveIOs[ulSeqNum].bArrived = 1;

	if(pstPerVolumeInfo->arstActiveIOs[ulSeqNum].bTag) {
		// Since this is Tag IO We can discard all the entries till this seq num,
		// as they have all arrived and were commited
		pstPerVolumeInfo->ulStartActiveIOIndex = 
			(ulSeqNum+1)%MAX_ACTIVE_IOS_PER_VOLUME;
	}

    pstPerVolumeInfo->ulNActiveIOs--;
	ReleaseMutex(pstPerVolumeInfo->hActiveIOsMutex);
}

int DecideTagOrWrite(int nWritesPerTag) {
	// we need to decide on a tag with (1/nWritesPerTag) probability
	// since 1 occurs with 1/nWritesPerTag probability in the range [0..nWritesPerTag), 
	// we vote for a tag when the random number generated is 1
	if(nWritesPerTag >= 1000) return 0;
	if(GenRandSize(nWritesPerTag-1) == 0)
		return 1;
	return 0;
}

#define ACTION_FROZEN 0
#define ACTION_DECIDE 1
#define ACTION_IO_DATA 2
#define ACTION_IO_TAG 3

bool CanPerformIO(PWRITE_THREAD_PARAMS WriteThreadParams)
{
    bool Result = FALSE;

    if(!stWriteOrderTestInfo.ControlledTest)
    {
        Result = TRUE;
    }
    else
    {
        ULONGLONG NetData = 0;
        PPER_VOLUME_INFO PerVolumeInfo = NULL;
        PerVolumeInfo = WriteThreadParams->pstPerVolumeInfo;

        if(!PerVolumeInfo->bFrozen)
        {
            WaitForSingleObject(PerVolumeInfo->VolumeLock, INFINITE);
			ULONG TimeElapsed = GetTickCount() - PerVolumeInfo->LastModeChangeTime;
			ULONG IoRate = 0;
			if(TimeElapsed)
			{
				IoRate = ((PerVolumeInfo->BytesWritten + WriteThreadParams->ulMaxIOSz) / TimeElapsed) * 1000;
				if(((PerVolumeInfo->BytesWritten + WriteThreadParams->ulMaxIOSz) / TimeElapsed) * 1000 > PerVolumeInfo->InputDataRate)
				{
					//_tprintf(_T("Io Rate Exceeded:%u  Mb/Sec:%u\n"), IoRate, IoRate / MEGA_BYTES);
					ReleaseMutex(PerVolumeInfo->VolumeLock);
					return Result;
				}
			}

            NetData = PerVolumeInfo->BytesWritten - PerVolumeInfo->BytesRead;
            //_tprintf(_T("BytesWritten:%I64u BytesRead:%I64u NetData:%I64u\n"), 
            //                                        PerVolumeInfo->BytesWritten, PerVolumeInfo->BytesRead, NetData);
            if(NetData <= PerVolumeInfo->DataThresholdPerVolume - WriteThreadParams->ulMaxIOSz)
            {
		        Result = TRUE;
	            PerVolumeInfo->BytesWritten += WriteThreadParams->ulMaxIOSz;				//Predictive increment = MaxIoSize
            }
            ReleaseMutex(PerVolumeInfo->VolumeLock);
        }
    }

    return Result;
}

int DecideThreadAction(PWRITE_THREAD_PARAMS pstWriteThreadParams)
{
    int action = ACTION_FROZEN;
    if(CanPerformIO(pstWriteThreadParams))
    {
	    int bTag = DecideTagOrWrite(pstWriteThreadParams->ulNWritesPerTag);
	    action = (bTag) ? ACTION_IO_TAG : ACTION_IO_DATA;
    }
    return action;
}

DWORD WINAPI WriteStream(LPVOID pArg) {
	int                  action;
	ULONG                i;
	ULONG                ulThreadId;
	char                 *buff = NULL;
	char				 tagBuf[4096];
	int                  nrunning;
	HANDLE               hVolume=INVALID_HANDLE_VALUE;
	PER_IO_INFO          stPerIOInfo;
	PPER_VOLUME_INFO     pstPerVolumeInfo;
	ULARGE_INTEGER        ullFileSize;
	ULONG                ulLen;
	LARGE_INTEGER        temp;
	PWRITE_THREAD_PARAMS pstWriteThreadParams;
    ULONG                ularTagSeqNums[MAX_VOLUMES];
	DWORD				 fileType;

	pstWriteThreadParams = (PWRITE_THREAD_PARAMS) pArg;

	pstPerVolumeInfo = pstWriteThreadParams->pstPerVolumeInfo;

	// Wait till the ReadThread starts or else we will end up dumping too much
	// to the driver and take it into bitmap Logging Mode
    printf("Writer Created\n");
	while(!pstPerVolumeInfo->bReadStarted) {
		Sleep(2);
	}

	// We will allocate the maximum-sized buffer we might use for this thread
	buff = (char *) malloc(pstWriteThreadParams->ulMaxIOSz);
	if(!pstWriteThreadParams->bDataTest)
		memset(buff,0,pstWriteThreadParams->ulMaxIOSz);
	if(buff == NULL) {
		printf("Insufficient Memory for the thread..\n");
		return 0;
	}

	// Get a handle on the volume
	hVolume = CreateFile(pstWriteThreadParams->strVolumeSymbolicLinkName,
						 GENERIC_READ | GENERIC_WRITE,
						 FILE_SHARE_WRITE | FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 FILE_FLAG_NO_BUFFERING,
						 NULL);
	if(hVolume == INVALID_HANDLE_VALUE) {
		printf("Couldn't obtain handle on %ws, error=%x\n",pstWriteThreadParams->strVolumeSymbolicLinkName,GetLastError());
		free(buff);
		return 0;
	}
	
	VOLUME_DISK_EXTENTS vd;
	if(!DeviceIoControl(hVolume,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,NULL,0,
		&vd,sizeof(VOLUME_DISK_EXTENTS),&fileType,NULL)) {
		printf("IOCTL_DISK_GET_LENGTH_INFO, error=%x\n",GetLastError());
		CloseHandle(hVolume);
		free(buff);
		return 0;
	}

	ullFileSize.QuadPart = vd.Extents[0].ExtentLength.QuadPart;

    ulThreadId = pstWriteThreadParams->ulThreadIndex;
	stWriteOrderTestInfo.ullTotalData[ulThreadId]=0;
	// We will use the threadIndex as seed for Random number generator
	unsigned int rand_seed = (int) time(NULL);
	//printf("rand seed = %d\n",(unsigned int)rand_seed);
	srand(rand_seed);

	action = ACTION_FROZEN;
    ULONG ulNMaxIOs = pstWriteThreadParams->ulNMaxIOs;

	// Now we can go ahead and issue writes
    //try{
	while(1) {
		// check to see if we are done.
        if(!stWriteOrderTestInfo.ControlledTest)
        {
		    if((!pstWriteThreadParams->ulNMaxIOs) || 
		    (stWriteOrderTestInfo.bInConsistent)) {
			    stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = 
												    THREAD_STATUS_NONE;
			    break;
		    }
        }
        else
        {
            if(stWriteOrderTestInfo.ExitTest)
            {
                stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = THREAD_STATUS_NONE;
                break;
            }

            if(pstWriteThreadParams->pstPerVolumeInfo->bFrozen) {
			    stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = THREAD_STATUS_FROZEN;
			    action = ACTION_FROZEN;
			    //continue;
		    }
        }

	    if(stWriteOrderTestInfo.bFrozen) {
			stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = THREAD_STATUS_FROZEN;
			action = ACTION_FROZEN;
			//continue;
		}
		switch(action) {
		case ACTION_DECIDE:
            action = DecideThreadAction(pstWriteThreadParams);
			break;
		case ACTION_IO_TAG:
			if(WaitForSingleObject(stWriteOrderTestInfo.hWriteThreadsMutex,2) != 
																WAIT_OBJECT_0) {
				break;
			}
			// Obtained Mutex
			stWriteOrderTestInfo.bFrozen = 1;

			// check if all but this thread have frozen
			do {
				nrunning=0;
				for(i=0;i<MAX_WRITE_THREADS;i++) {
					if(stWriteOrderTestInfo.earrWriteThreadStatus[i] == THREAD_STATUS_RUNNING) 
						nrunning++;
				}
			}while(nrunning > 1);

			// All other threads are frozen now...They will be in FROZEN state until we 
			// clear off the Frozen Flag
			
			// Now we can go ahead issue the Tag
            memset(&stPerIOInfo,0,sizeof(PER_IO_INFO));
            stPerIOInfo.bTag = 1;
            
			GenerateTagBuffer(tagBuf,&ulLen,ularTagSeqNums);
            for(i=0;i<MAX_VOLUMES;i++) {
				if(!stWriteOrderTestInfo.arActiveVolumes[i])
					continue;
                stPerIOInfo.ulIOSeqNum = ularTagSeqNums[i];
                AddNewIOInfoToVolume(&stWriteOrderTestInfo.arstPerVolumeInfo[i],&stPerIOInfo);
            }
			printf("Issuing in a Tag of Length.. %d\n",ulLen);
			if(!DeviceIoControl(stWriteOrderTestInfo.hFilterDriver,
								IOCTL_INMAGE_TAG_VOLUME,
								tagBuf,
								ulLen,
								NULL,
								0,
								&fileType,
								NULL)) {
				printf("Error issuing Tag %x\n",GetLastError());
			}
            if(stWriteOrderTestInfo.ControlledTest)
            {
                //Offset the predictive increment in CanPerformIO, In case of tag the IO size is Zero.
                WaitForSingleObject(pstPerVolumeInfo->VolumeLock, INFINITE);
                pstPerVolumeInfo->BytesWritten -= pstWriteThreadParams->ulMaxIOSz;
                ReleaseMutex(pstPerVolumeInfo->VolumeLock);
            }

			//printf("Issued Tag\n");
			// Now clear off the Frozen Flag
			pstWriteThreadParams->ulNMaxIOs--;
			stWriteOrderTestInfo.bFrozen = 0;
			ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
			action = ACTION_DECIDE;
			break;
		case ACTION_IO_DATA:
            memset(&stPerIOInfo,0,sizeof(PER_IO_INFO));
			stPerIOInfo.ulThreadId = ulThreadId;
			GenerateDataBuffer(buff,
							   pstWriteThreadParams->ulMinIOSz,
							   pstWriteThreadParams->ulMaxIOSz,
							   ullFileSize.QuadPart,
							   pstWriteThreadParams,
							   &stPerIOInfo,
							   pstWriteThreadParams->Sequential);
			WaitForSingleObject(pstPerVolumeInfo->hActiveIOsMutex,INFINITE);
            stPerIOInfo.ulIOSeqNum = GenerateIOSeqNumForVolume(pstPerVolumeInfo);
			*((PULONG)buff) = stPerIOInfo.ulIOSeqNum;
            // Update Per IO Info. IO Needs to be logged before write is actually done
            // because, it may arrive from the driver before the write is logged, if it
            // is written first
            AddNewIOInfoToVolume(pstPerVolumeInfo,&stPerIOInfo);
			ReleaseMutex(pstPerVolumeInfo->hActiveIOsMutex);

            // do the write after seeking to the offset
			temp.QuadPart = stPerIOInfo.ullOffset;
			if(!SetFilePointerEx(hVolume,temp,NULL,FILE_BEGIN)) {
				fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "Couldnt set File Pointer: ThreadID:%u Error:%d\n", GetCurrentThreadId(), GetLastError());
				continue;
			}
            //printf("Issuing in an IO of Length:%u IO Number%u\n",stPerIOInfo.ulLen, ulNMaxIOs - pstWriteThreadParams->ulNMaxIOs);

			if(!WriteFile(hVolume,
					  buff,
					  stPerIOInfo.ulLen,
					  &ulLen,
					  NULL)) {
				fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "Couldnt set File Pointer: ThreadID:%u Error:%d\n", GetCurrentThreadId(), GetLastError());
				continue;
			}
            
            if(stWriteOrderTestInfo.ControlledTest)
            {
                //Offset the predictive increment in CanPerformIO
                WaitForSingleObject(pstPerVolumeInfo->VolumeLock, INFINITE);
                pstPerVolumeInfo->BytesWritten -= pstWriteThreadParams->ulMaxIOSz - stPerIOInfo.ulLen;
                ReleaseMutex(pstPerVolumeInfo->VolumeLock);
            }

			stWriteOrderTestInfo.ullTotalData[ulThreadId]+=(ULONGLONG)stPerIOInfo.ulLen;
			//printf("Data Write: threadid=%d pattern=%d seqNum=%d offset=%I64d, length=%d\n",stPerIOInfo.ulThreadId,
			//stPerIOInfo.ulPattern,stPerIOInfo.ulIOSeqNum,stPerIOInfo.ullOffset, stPerIOInfo.ulLen);
			pstWriteThreadParams->ulNMaxIOs--;
			action = ACTION_DECIDE;
			break;
		case ACTION_FROZEN:
            
            if(pstWriteThreadParams->pstPerVolumeInfo->bFrozen) {
                Sleep(2);
			    break;
            }

			if(stWriteOrderTestInfo.bFrozen) {
				// Sleep for a while so that the thread issuing the tag will be done
				Sleep(2);
				break;
			}
			// Can come out of freeze now
			if(WaitForSingleObject(stWriteOrderTestInfo.hWriteThreadsMutex,2) != WAIT_OBJECT_0)
				break;
			// Thread can Run
			stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = 
												THREAD_STATUS_RUNNING;
			ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
			action = ACTION_DECIDE;
			break;
		default:
            assert(0);
			break;
		}
	}
    //}
    //catch(...)
    //{
    //    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "Exception");
    //}

	// Free the local buffers we allocated
	CloseHandle(hVolume);
	free(buff);
    printf("Exiting WriteStream\n");
	return 0;
}

ULONG ValidateDirtyBlockTags(PUDIRTY_BLOCK pstDirtyBlock,
							 PREAD_THREAD_STATE_INFO pstReadThreadStateInfo) {
	PSTREAM_REC_HDR_4B pstTag = NULL;
	PER_IO_INFO		   stPerIOInfo;
	PULONG			   pData;
	PPER_VOLUME_INFO   pstPerVolumeInfo = NULL;
    ULONG              ulHeaderLen;
	int				   nTags;

	pstPerVolumeInfo = pstReadThreadStateInfo->pstVolumeInfo;
	
	pstTag = &pstDirtyBlock->uTagList.TagList.TagEndOfList;
	nTags=0;
	// Iterate through the list of tags
	while(pstTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) {
		nTags++;
		if(pstTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) {
			// We just wrote 8 bytes, so this is wrong format
			return 0;
			pData = (PULONG)((PUCHAR)pstTag + sizeof(STREAM_REC_HDR_8B)+sizeof(USHORT));
            ulHeaderLen = sizeof(STREAM_REC_HDR_8B);
		}
		else {
			if(pstTag->ucLength-8 != sizeof(ULONG)) {
				// We have only 8-byte tags
				return 0;
			}
			pData = (PULONG)((PUCHAR)pstTag + sizeof(STREAM_REC_HDR_4B)+sizeof(USHORT));
            ulHeaderLen = sizeof(STREAM_REC_HDR_4B);
		}
		memset(&stPerIOInfo,0,sizeof(PER_IO_INFO));
		stPerIOInfo.ulIOSeqNum = pData[0];
		//printf("recvd tag = %d\n",*pData);

		if(!VerifyIOArrivalOrderForVolume(pstPerVolumeInfo,1,&stPerIOInfo)) {
			pstReadThreadStateInfo->ulCurIOSeqNum = stPerIOInfo.ulIOSeqNum;
			printf("recvd io with seq num %d out of order\n",stPerIOInfo.ulIOSeqNum);
			pstReadThreadStateInfo->ulExitReason = EXIT_IO_OUT_OF_ORDER;
			// Volume is inconsistent
			return 0;
		}
		// Update the IOsWaitingForCommit List
		UpdateCommitedIOForVolume(pstPerVolumeInfo,
						stPerIOInfo.ulIOSeqNum);
		/*pstPerVolumeInfo->arstIOsWaitingForCommit[pstPerVolumeInfo->ulNIOsWaitingForCommit++] = 
																		stPerIOInfo.ulIOSeqNum;*/
        pstReadThreadStateInfo->ulCurIOSeqNum = INVALID_SEQ_NUM;
		pstTag = (PSTREAM_REC_HDR_4B) ((PUCHAR)pstTag+pstTag->ucLength);
	}
	//printf("......NTags in this DBlock = %d Nchanges = %d\n",nTags,pstDirtyBlock->uHdr.Hdr.cChanges);
	// All the Tags in this Dirty Block are verified
	return 1;
}

ULONG ValidateDirtyBlockDiskChange(PUDIRTY_BLOCK           pstDirtyBlock,
                                   ULONG                   ulDiskChangeNum,
                                   ULONG                   bValidateOrder,
                                   PREAD_THREAD_STATE_INFO pstReadThreadStateInfo) {
    ULONG       i,j;
    ULONG       nLen;
    PULONG      pBuff;
    ULONG       ulBufferSize,ulStartBuffer,ulNBuffers;
    PDISK_CHANGE pstDiskChange;
    PER_IO_INFO stPerIOInfo;
    ULONG       ulDiskChangeLen;
    PVOID*      ppBufferArray;
    PPER_VOLUME_INFO pstPerVolumeInfo;

    pstDiskChange = &pstDirtyBlock->Changes[ulDiskChangeNum];
    ulBufferSize = pstDirtyBlock->uHdr.Hdr.ulBufferSize;
    ulStartBuffer = pstDiskChange->usBufferIndex;
    ulNBuffers = pstDiskChange->usNumberOfBuffers;
    ppBufferArray = pstDirtyBlock->uHdr.Hdr.ppBufferArray;
    pstPerVolumeInfo = pstReadThreadStateInfo->pstVolumeInfo;

    for(i=0;i<ulNBuffers;i++) {
        pBuff = (PULONG) ppBufferArray[ulStartBuffer+i];
        if(i == 0) {
            if(bValidateOrder){
                memset(&stPerIOInfo,0,sizeof(PER_IO_INFO));
                stPerIOInfo.ulIOSeqNum = pBuff[0];
                stPerIOInfo.ulThreadId = pBuff[1];
				//printf("new io threadid=%d, seqnum=%d\n",
					//stPerIOInfo.ulThreadId,stPerIOInfo.ulIOSeqNum);
                if(!VerifyIOArrivalOrderForVolume(pstPerVolumeInfo,0,&stPerIOInfo)) {
                    // IO Arrived Out of Order
					pstReadThreadStateInfo->ulCurIOSeqNum = stPerIOInfo.ulIOSeqNum;
					pstReadThreadStateInfo->ulExitReason = EXIT_IO_OUT_OF_ORDER;
                    return 0;
                }
                if(stPerIOInfo.ullOffset != pstDiskChange->ByteOffset.QuadPart) {
                    // Offsets doesn't match
					pstReadThreadStateInfo->ulExitReason = EXIT_OFFSET_MISMATCH;
                    return 0;
                }

                nLen = MIN_OF_2(pstDirtyBlock->Changes[ulDiskChangeNum].Length,
                                pstDirtyBlock->uHdr.Hdr.ulBufferSize);
                ulDiskChangeLen = pstDirtyBlock->Changes[ulDiskChangeNum].Length;
                                
                pstReadThreadStateInfo->ulCurIOSeqNum = stPerIOInfo.ulIOSeqNum;
                pstReadThreadStateInfo->ullByteOffset = pstDiskChange->ByteOffset.QuadPart;
                pstReadThreadStateInfo->ulLen = stPerIOInfo.ulLen;
                pstReadThreadStateInfo->ulPattern = stPerIOInfo.ulPattern;
                pBuff+=2; nLen -= 8; pstReadThreadStateInfo->ulLen -= 8;
                pstReadThreadStateInfo->ullByteOffset += 8;
				ulDiskChangeLen -= 8;
            }
            else {
                if(pstDiskChange->ByteOffset.QuadPart != pstReadThreadStateInfo->ullByteOffset) {
					pstReadThreadStateInfo->ulExitReason = EXIT_OFFSET_MISMATCH;
                    // Starting Offset doesn't match
                    return 0;
                }
                nLen = MIN_OF_3(pstReadThreadStateInfo->ulLen,
                                pstDirtyBlock->Changes[ulDiskChangeNum].Length,
                                pstDirtyBlock->uHdr.Hdr.ulBufferSize);
                ulDiskChangeLen = pstDirtyBlock->Changes[ulDiskChangeNum].Length;
            }
        }
		else {
			nLen = MIN_OF_2(ulDiskChangeLen,
                            pstDirtyBlock->uHdr.Hdr.ulBufferSize);
		}
		nLen /= 4;
		for(j=0;j<nLen;j++) {
			if(pBuff[j] != pstReadThreadStateInfo->ulPattern) {
				// Pattern didn't match
				printf("Data Corrupted\n");
				pstReadThreadStateInfo->ulExitReason = EXIT_IO_DATA_CORRUPTED;
				return 0;
			}
		}
		pstReadThreadStateInfo->ulLen -= nLen*4; 
		pstReadThreadStateInfo->ullByteOffset += nLen*4;
		ulDiskChangeLen -= nLen*4;
    }
	if(ulDiskChangeLen != 0) {
		printf("Not enough Buffers for the CHAnge in the DB\n");
		pstReadThreadStateInfo->ulExitReason = EXIT_NOT_ENOUGH_BUFFERS_FOR_THE_SIZE;
		return 0;
	}
    // All the changes validated 
    return 1;
}

ULONG ValidateDirtyBlockDiskChanges(PUDIRTY_BLOCK           pstDirtyBlock,
									PREAD_THREAD_STATE_INFO pstReadThreadStateInfo) {
    ULONG            i;
    PPER_VOLUME_INFO pstPerVolumeInfo=NULL;

    pstPerVolumeInfo = pstReadThreadStateInfo->pstVolumeInfo;

    switch(pstDirtyBlock->uHdr.Hdr.ulFlags&UDIRTY_BLOCK_FLAG_SPLIT_CHANGE) {
    case UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE:
        if(pstReadThreadStateInfo->ulState != TAG_OR_START_OF_DISK_CHANGE) {
            // Not expecting start of a disk change
			printf("Not expecting start of a Disk Change\n");
			pstReadThreadStateInfo->ulExitReason = EXIT_EXPECTING_PART_OR_END_OF_CHANGE;
            return 0;
        }
        if(pstDirtyBlock->uHdr.Hdr.cChanges != 1) {
            // Incorrect number of disk changes in a split dirty block
            printf("%d changes in a dirty block which contains split changes??!\n",
                                                            pstDirtyBlock->uHdr.Hdr.cChanges);
			pstReadThreadStateInfo->ulExitReason = EXIT_MORE_DISK_CHANGES_IN_SPLIT_DB;
            return 0;
        }
        for(i=0;i<pstDirtyBlock->uHdr.Hdr.cChanges;i++) {
            if(!ValidateDirtyBlockDiskChange(pstDirtyBlock,
                                             i,
                                             1,
                                             pstReadThreadStateInfo)) {
                return 0;
            }
            if(pstReadThreadStateInfo->ulLen <= 0) {
				printf("Split Change completed in first DB itself\n");
				pstReadThreadStateInfo->ulExitReason = EXIT_SPLIT_CHANGE_OVER_IN_FIRST_DB;
                // Split change completed in the START_SPLIT_BLOCK itself
                return 0;
            } 
        }
        pstReadThreadStateInfo->ulState = PART_OR_END_OF_SPLIT_DISK_CHANGE;
        break;
    case UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE:
        if(pstReadThreadStateInfo->ulState == TAG_OR_START_OF_DISK_CHANGE) {
			pstReadThreadStateInfo->ulExitReason = EXIT_EXPECTING_START_OF_CHANGE;
            // Expecting only start of a disk change
            return 0;
        }
        if(pstDirtyBlock->uHdr.Hdr.cChanges != 1) {
            // Incorrect number of disk changes in a split dirty block
            printf("%d changes in a dirty block which contains split changes??!\n",
                                                            pstDirtyBlock->uHdr.Hdr.cChanges);
			pstReadThreadStateInfo->ulExitReason = EXIT_MORE_DISK_CHANGES_IN_SPLIT_DB;
            return 0;
        }
 
        for(i=0;i<pstDirtyBlock->uHdr.Hdr.cChanges;i++) {
            if(!ValidateDirtyBlockDiskChange(pstDirtyBlock,
                                             i,
                                             0,
                                             pstReadThreadStateInfo)) {
                return 0;
            }
            if(pstReadThreadStateInfo->ulLen <= 0) {
                // Split change completed in the PART_SPLIT_BLOCK itself
				pstReadThreadStateInfo->ulExitReason = EXIT_SPLIT_CHANGE_OVER_IN_FIRST_DB;
                return 0;
            }
        }
        break;
    case UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE:
        if(pstReadThreadStateInfo->ulState == TAG_OR_START_OF_DISK_CHANGE) {
            // Expecting only part of a disk change
            printf("Expecting start of a disk change but got Part of Split change\n");
			pstReadThreadStateInfo->ulExitReason = EXIT_EXPECTING_START_OF_CHANGE;
            return 0;
        }
        if(pstDirtyBlock->uHdr.Hdr.cChanges != 1) {
            // Incorrect number of disk changes in a split dirty block
            printf("%d changes in a dirty block which contains split changes??!\n",
                                                            pstDirtyBlock->uHdr.Hdr.cChanges);
			pstReadThreadStateInfo->ulExitReason = EXIT_MORE_DISK_CHANGES_IN_SPLIT_DB;
            return 0;
        }

        for(i=0;i<pstDirtyBlock->uHdr.Hdr.cChanges;i++) {
            if(!ValidateDirtyBlockDiskChange(pstDirtyBlock,
                                             i,
                                             0,
                                             pstReadThreadStateInfo)) {
                return 0;
            }
        }
        if(pstReadThreadStateInfo->ulLen != 0) {
            // We have processed either lesser number of bytes or more number of bytes
            // but this is the last dirty block of split changes
			pstReadThreadStateInfo->ulExitReason = EXIT_SPLIT_CHANGE_NOT_OVER_IN_END_DB;
            return 0;
        }

        // All is well so we can commit this IO
		UpdateCommitedIOForVolume(pstPerVolumeInfo,
						pstReadThreadStateInfo->ulCurIOSeqNum);
        /*pstPerVolumeInfo->arstIOsWaitingForCommit[pstPerVolumeInfo->ulNIOsWaitingForCommit++] = 
                                                            pstReadThreadStateInfo->ulCurIOSeqNum;*/
        pstReadThreadStateInfo->ulCurIOSeqNum = INVALID_SEQ_NUM;
        pstReadThreadStateInfo->ulState = TAG_OR_START_OF_DISK_CHANGE;
        break;
    default:
        // All the disk changes are regular (non-split) ones
        if(pstReadThreadStateInfo->ulState != TAG_OR_START_OF_DISK_CHANGE) {
            // Not expecting start of a disk change
			pstReadThreadStateInfo->ulExitReason = EXIT_EXPECTING_PART_OR_END_OF_CHANGE;
            return 0;
        }
        for(i=0;i<pstDirtyBlock->uHdr.Hdr.cChanges;i++) {
            if(!ValidateDirtyBlockDiskChange(pstDirtyBlock,
                                             i,
                                             1,
                                             pstReadThreadStateInfo)) {
                return 0;
            }
            if(pstReadThreadStateInfo->ulLen != 0) {
                // We have processed either lesser number of bytes or more number of bytes
                // but this is the last dirty block of split changes
				pstReadThreadStateInfo->ulExitReason = EXIT_SPLIT_CHANGE_NOT_OVER_IN_END_DB;
                return 0;
             }   
			UpdateCommitedIOForVolume(pstPerVolumeInfo,
						pstReadThreadStateInfo->ulCurIOSeqNum);
            /*pstPerVolumeInfo->arstIOsWaitingForCommit[pstPerVolumeInfo->ulNIOsWaitingForCommit++] = 
                                                        pstReadThreadStateInfo->ulCurIOSeqNum;*/
            pstReadThreadStateInfo->ulCurIOSeqNum = INVALID_SEQ_NUM;
        }
        break;
    }
    return 1;
}

ULONG ValidateDirtyBlock(PUDIRTY_BLOCK pstDirtyBlock,
						 PREAD_THREAD_STATE_INFO pstReadThreadStateInfo) {
	if(!ValidateDirtyBlockTags(pstDirtyBlock,pstReadThreadStateInfo)) {
		return 0;
	}
	if(!ValidateDirtyBlockDiskChanges(pstDirtyBlock,pstReadThreadStateInfo)) {
		return 0;
	}
	return 1;
}

void LogErrorReport(PPER_VOLUME_INFO		pstPerVolumeInfo,
			        PREAD_THREAD_STATE_INFO  pstReadThreadStateInfo) {
	int i;
	int bReportPendingIOs=0;
    WaitForSingleObject(stWriteOrderTestInfo.hWriteThreadsMutex,INFINITE);
    if(stWriteOrderTestInfo.bInConsistent) {
        // Error Report is Logged already
        ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
        return;
    }
	stWriteOrderTestInfo.bInConsistent = 1;
	FILE *fp = fopen(stWriteOrderTestInfo.strLogFileName,"w");
	printf("Error Report Logging Started...\n");
	switch(pstReadThreadStateInfo->ulExitReason) {
		case EXIT_SUCCESFUL_COMPLETION:
		case EXIT_GET_DBS_NOT_STARTED:
		case EXIT_EXPECTING_START_OF_CHANGE:
		case EXIT_EXPECTING_PART_OR_END_OF_CHANGE:
		case EXIT_MORE_DISK_CHANGES_IN_SPLIT_DB:
		case EXIT_TIMEOUT_WAITING_FOR_A_DB_WITH_CHANGES:
		case EXIT_MODE_CHANGED_TO_BITMAP_OR_META_DATA:
			fprintf(fp,exit_reason_strings[pstReadThreadStateInfo->ulExitReason]);
			break;
		case EXIT_IO_OUT_OF_ORDER:
		case EXIT_OFFSET_MISMATCH:
		case EXIT_IO_DATA_CORRUPTED:
		case EXIT_NOT_ENOUGH_BUFFERS_FOR_THE_SIZE:
		case EXIT_SPLIT_CHANGE_OVER_IN_FIRST_DB:
		case EXIT_SPLIT_CHANGE_OVER_IN_PART_DB:
		case EXIT_SPLIT_CHANGE_NOT_OVER_IN_END_DB:
		case EXIT_CHANGE_NOT_COMPLETE:
			fprintf(fp,exit_reason_strings[pstReadThreadStateInfo->ulExitReason],
				pstReadThreadStateInfo->ulCurIOSeqNum);
			bReportPendingIOs=1;
			break;
	}
	printf("\n");
	
	if(bReportPendingIOs) {
		fprintf(fp,"This is the list of pending IOs....\n");
		for(i=pstPerVolumeInfo->ulStartActiveIOIndex;
			i!=pstPerVolumeInfo->ulNextActiveIOIndex;
			i=(i+1)%MAX_ACTIVE_IOS_PER_VOLUME) {
			fprintf(fp,"%d %s %s %d %x %I64x %x\n",
				i,
				((pstPerVolumeInfo->arstActiveIOs[i].bTag)?"TAG":"DATA"),
				((pstPerVolumeInfo->arstActiveIOs[i].bArrived)?"ARRIVED":"NOT ARRIVED"),
				pstPerVolumeInfo->arstActiveIOs[i].ulThreadId,
				pstPerVolumeInfo->arstActiveIOs[i].ulPattern,
				pstPerVolumeInfo->arstActiveIOs[i].ullOffset,
				pstPerVolumeInfo->arstActiveIOs[i].ulLen);
		}
	}
	fclose(fp);
    ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
	printf("Error Report Logging Done.\n");
}

void PauseAllWritesOnVolume(PPER_VOLUME_INFO PerVolumeInfo)
{
    PerVolumeInfo->bFrozen = TRUE;
    // check if all but this thread have frozen
    int nrunning=0;
	do {
        nrunning=0;
		for(int i=0;i<MAX_WRITE_THREADS;i++) {
            if(PerVolumeInfo == stWriteOrderTestInfo.arstWriteThreadParams[i].pstPerVolumeInfo)
            {
			    if(stWriteOrderTestInfo.earrWriteThreadStatus[i] == THREAD_STATUS_RUNNING) 
				    nrunning++;
            }
		}
	}while(nrunning);
}

void LogChangeEvent(PPER_VOLUME_INFO PerVolumeInfo, int EventType)
{
    const char EventMessage[3][128] = {"MODE_CHANGE", "CONSISTENY_LOSS", "USER_EXIT"};
    ULONG LogSize = 0, BytesWritten = 0, TestDurationSecond = 0;
    ULONGLONG DataRateInBytes = 0;
    ULONGLONG DrainRateInBytes = 0;

    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "VolumeGUID=%ws\n", PerVolumeInfo->volumeGUID);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "EventType=%s\n", EventMessage[EventType]);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "BytesWritten=%I64u\n", PerVolumeInfo->BytesWritten);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer , "BytesRead=%I64u\n", PerVolumeInfo->BytesRead);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer , "ModeChangeTime=%u\n", GetTickCount() / 1000);
    
    TestDurationSecond = (GetTickCount() - PerVolumeInfo->LastModeChangeTime) / 1000;
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "Duration=%u\n", TestDurationSecond);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "DataThresholdPerVolume=%u\n", PerVolumeInfo->DataThresholdPerVolume);

    DataRateInBytes = PerVolumeInfo->BytesWritten;
    
    if(TestDurationSecond)
        DataRateInBytes  = DataRateInBytes / TestDurationSecond;
    
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "DataRate (Bytes / Sec)=%I64u\n", DataRateInBytes);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "DataRate (KBytes / Sec)=%I64u\n", DataRateInBytes / KILO_BYTES);
    fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "DataRate (MBytes / Sec)=%I64u\n\n", DataRateInBytes / MEGA_BYTES);

    fflush(stWriteOrderTestInfo.ChangeEventLogFilePointer);
}

VOID CleanupVolumeAndThreadState(PPER_VOLUME_INFO PerVolumeInfo)
{
    memset(PerVolumeInfo->arstActiveIOs, 0, sizeof(PerVolumeInfo->arstActiveIOs));
    PerVolumeInfo->ulNActiveIOs = 0;
    PerVolumeInfo->ulNextActiveIOIndex = 0;
    PerVolumeInfo->ulStartActiveIOIndex = 0;
}

void NotifyModeChange(PPER_VOLUME_INFO PerVolumeInfo, int EventType)
{
    _tprintf(_T("NotifyModeChange:\n"));
    PauseAllWritesOnVolume(PerVolumeInfo);
    LogChangeEvent(PerVolumeInfo, EventType);
    ClearExistingData(stWriteOrderTestInfo.hFilterDriver, PerVolumeInfo->volumeGUID);
    PerVolumeInfo->BytesRead = 0;
    PerVolumeInfo->BytesWritten = 0;
    PerVolumeInfo->LastModeChangeTime = GetTickCount();
    PerVolumeInfo->DataThresholdPerVolume -= stWriteOrderTestInfo.DataRateTuningStep;
    _tprintf(_T("DataThresholdPerVolume:%I64u\n"),PerVolumeInfo->DataThresholdPerVolume);
    CleanupVolumeAndThreadState(PerVolumeInfo);
    PerVolumeInfo->bFrozen = FALSE;
}

DWORD WINAPI ReadStream(LPVOID pArg) {
#define MAX_TRIES_FOR_DB_WITH_CHANGES 40
	UDIRTY_BLOCK		   stDirtyBlock;
	PPER_VOLUME_INFO	   pstPerVolumeInfo = NULL;
	READ_THREAD_STATE_INFO stReadThreadStateInfo;
	ULONG				   ntries;

	pstPerVolumeInfo = (PPER_VOLUME_INFO) pArg;
    pstPerVolumeInfo->bReadStarted = 1;

	stReadThreadStateInfo.ulState = TAG_OR_START_OF_DISK_CHANGE;
	stReadThreadStateInfo.pstVolumeInfo = pstPerVolumeInfo;
	stReadThreadStateInfo.ulCurIOSeqNum = INVALID_SEQ_NUM;
	stReadThreadStateInfo.ulExitReason = EXIT_GET_DBS_NOT_STARTED;
	stReadThreadStateInfo.ullByteOffset = 0;
	stReadThreadStateInfo.ulLen = 0;
	stReadThreadStateInfo.ulPattern = 0;

	ntries=0;
	memset(&stDirtyBlock,0,sizeof(UDIRTY_BLOCK));
	while(1) {
        if(!stWriteOrderTestInfo.ControlledTest)
        {
            if(stWriteOrderTestInfo.bInConsistent)
                break;
        }
        else 
        {
            if(stWriteOrderTestInfo.bInConsistent)
            {
                NotifyModeChange(pstPerVolumeInfo, 1);
                
                WaitForSingleObject(stWriteOrderTestInfo.hWriteThreadsMutex,INFINITE);
                stWriteOrderTestInfo.bInConsistent = 0;
                ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
            }

            if(stWriteOrderTestInfo.ExitTest)
                break;
        }

		if(!GetNextDB(stWriteOrderTestInfo.hFilterDriver,
					  stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart,
					  pstPerVolumeInfo->volumeGUID,
					  &stDirtyBlock)) {
			printf("ReadStream::Failed getting DB\n");
			stReadThreadStateInfo.ulExitReason = EXIT_FAILURE_GETTING_DB;
			LogErrorReport(pstPerVolumeInfo,&stReadThreadStateInfo);
			break;
		}

		if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_DATA && 
            stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart != 0) {
			ntries=0;
			if(!ValidateDirtyBlock(&stDirtyBlock,&stReadThreadStateInfo)) {
				// Mark the Volume as Inconsistent
				LogErrorReport(pstPerVolumeInfo,&stReadThreadStateInfo);
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

				if(!stWriteOrderTestInfo.ControlledTest)
					break;
				else
					continue;
			}
            
            if(stWriteOrderTestInfo.ControlledTest)
                    pstPerVolumeInfo->BytesRead += stDirtyBlock.uHdr.Hdr.ulicbChanges.QuadPart;

            //_tprintf(_T("BytesRead: %I64u\n"), pstPerVolumeInfo->BytesRead);

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

			Sleep(1000);
		}
        else if(stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart == 0) {
			ntries++;
			if(ntries == MAX_TRIES_FOR_DB_WITH_CHANGES) {
				stReadThreadStateInfo.ulExitReason = EXIT_SUCCESFUL_COMPLETION;
                _tprintf(_T("Exceeding Max Tries\n"));
                
                if(!stWriteOrderTestInfo.ControlledTest)
                    break;

				continue;
			}
			Sleep(100);
			continue;
		}
		else {
            if(stWriteOrderTestInfo.ControlledTest)
            {
                NotifyModeChange(pstPerVolumeInfo, 0);
            }
            else
            {
                _tprintf(_T("Should Not come here\n"));
			    if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_META_DATA) {
				    printf("Quitting due to switching of mode to meta-data\n");
			    }
			    else if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_BITMAP) {
				    printf("Quitting due to switching of mode to Bitmap\n");
			    }
			    else {
				    printf("Quitting due to switching of mode to UNKNOWN\n");
			    }
			    stReadThreadStateInfo.ulExitReason = EXIT_MODE_CHANGED_TO_BITMAP_OR_META_DATA;
			    LogErrorReport(pstPerVolumeInfo,&stReadThreadStateInfo);
                break;
            }
		}
	}

    printf("Log Exit Event\n");
    LogChangeEvent(pstPerVolumeInfo, 2);
    printf("Exiting Read thread\n");
    return 0;
}

DWORD WINAPI WaitForExit(LPVOID Context)
{
    TCHAR Buffer[128];
    do
    {
        _tscanf(_T("%s"), Buffer);
        if(0 == _tcsicmp(Buffer, _T("Exit")))
        {
            stWriteOrderTestInfo.ExitTest = TRUE;
            break;
        }
    }
    while(1);
    return 0;
}






int SimulatedTest(HANDLE hDriver, PSIMULATED_TEST_CMD pstWriteOrderTestParams) {
	HANDLE hThreads[MAX_WRITE_THREADS+MAX_VOLUMES+1];
	PWRITE_STREAM_PARAMS pstWriteStreamParams = NULL;
	PPER_VOLUME_INFO pstPerVolumeInfo = NULL;
	PWRITE_THREAD_PARAMS pstWriteThreadParams = NULL;
	ULONG nThreads=0;
	ULONG i;
	DWORD ret;
	time_t s,e;
	char c;

	s = time(NULL);
	//memset(&stWriteOrderTestInfo,0,sizeof(WRITE_ORDER_TEST_INFO));

    CreateThread(NULL, 0, WaitForExit, NULL, 0, NULL);

    strcpy_s(stWriteOrderTestInfo.ChangeEventLogFileName, _countof(stWriteOrderTestInfo.ChangeEventLogFileName), LOG_FILE_PATH);
    if(0 == stWriteOrderTestInfo.DataRateTuningStep)
        stWriteOrderTestInfo.DataRateTuningStep = DEFAULT_DATA_RATE_TUNING_STEP_DEFAULT * MEGA_BYTES;

    if(0 == stWriteOrderTestInfo.DataThresholdForVolume)
        stWriteOrderTestInfo.DataThresholdForVolume = DEFAULT_DATA_THRESHOLD_FOR_VOLUME_DEFAULT * MEGA_BYTES;

	if(0 == stWriteOrderTestInfo.InputDataRate)
		stWriteOrderTestInfo.InputDataRate = DEFAULT_DATA_RATE_THRESHOLD_FOR_VOLUME * MEGA_BYTES;

    stWriteOrderTestInfo.ExitTest = FALSE;

	////Open Log File
 //   stWriteOrderTestInfo.ChangeEventLogFileHandle = CreateFile(stWriteOrderTestInfo.ChangeEventLogFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
 //   if(INVALID_HANDLE_VALUE == stWriteOrderTestInfo.ChangeEventLogFileHandle)
 //   {
 //       _tprintf(_T("SimulatedTest: Could not create Log file Error:%d\n"), GetLastError());
 //   }
    stWriteOrderTestInfo.ChangeEventLogFilePointer = fopen(stWriteOrderTestInfo.ChangeEventLogFileName, "wt");
    if(NULL == stWriteOrderTestInfo.ChangeEventLogFilePointer)
    {
        printf("SimulatedTest: Could not create Log file Error\n");
    }

	// Initialize the Mutex that will be used to synchorize all the threads
    stWriteOrderTestInfo.hWriteThreadsMutex = CreateMutex(NULL,FALSE,NULL);
    stWriteOrderTestInfo.strLogFileName = ERROR_LOG_FILE_NAME;
	stWriteOrderTestInfo.hFilterDriver = hDriver;
	stWriteOrderTestInfo.nWriteThreads = pstWriteOrderTestParams->ulNWriteThreads;
	// Get all the Drive Letters we are writing to
	for(i=0;i<pstWriteOrderTestParams->ulNWriteThreads;i++) {
		pstWriteStreamParams = &pstWriteOrderTestParams->arstWriteStreamParams[i];
		if(stWriteOrderTestInfo.arActiveVolumes[pstWriteStreamParams->DriveLetter-'a'])
			continue;
		stWriteOrderTestInfo.nVolumes++;
		stWriteOrderTestInfo.arActiveVolumes[pstWriteStreamParams->DriveLetter-'a'] = 1;
		pstPerVolumeInfo = &stWriteOrderTestInfo.arstPerVolumeInfo[pstWriteStreamParams->DriveLetter-'a'];
        memset(pstPerVolumeInfo,0,sizeof(PER_VOLUME_INFO));
		pstPerVolumeInfo->DriveLetter = pstWriteStreamParams->DriveLetter;
        pstPerVolumeInfo->hActiveIOsMutex = CreateMutex(NULL,FALSE,NULL);
		if(!GetVolumeGUIDFromDriveLetter(pstPerVolumeInfo->DriveLetter,
										pstPerVolumeInfo->volumeGUID,
										sizeof(pstPerVolumeInfo->volumeGUID))) {
			printf("Couldn't obtain GUID for %c:\n",pstPerVolumeInfo->DriveLetter);
			return 0;
		}
		pstPerVolumeInfo->volumeGUID[GUID_SIZE_IN_CHARS] = '\0';
        pstPerVolumeInfo->VolumeLock = CreateMutex(NULL, FALSE, NULL);
        pstPerVolumeInfo->LastModeChangeTime = GetTickCount();
        pstPerVolumeInfo->DataThresholdPerVolume = stWriteOrderTestInfo.DataThresholdForVolume;
		pstPerVolumeInfo->InputDataRate = stWriteOrderTestInfo.InputDataRate;
        printf("%ws\n",pstPerVolumeInfo->volumeGUID);
	}

	/* clearing existing data for all the volumes */
	for(i=0;i<MAX_VOLUMES;i++) {
		if(!stWriteOrderTestInfo.arActiveVolumes[i])
			continue;
		pstPerVolumeInfo = &stWriteOrderTestInfo.arstPerVolumeInfo[i];
		/* clearing existing data for the volume */

		if(!ClearExistingData(hDriver,pstPerVolumeInfo->volumeGUID)) {
			printf("Error while clearing existing data for %c:\n",pstPerVolumeInfo->DriveLetter);
			return 0;
		}
	}
	printf("SimulatedTest:sucessfully cleared off existing data for all the participating volumes\n");

	GenerateTagBufferPrefix(stWriteOrderTestInfo.TagBufferPrefix,
		&stWriteOrderTestInfo.TagBufferPrefixLength);
#ifdef _DEBUG
    for(i=0;i<stWriteOrderTestInfo.TagBufferPrefixLength;i++) {
        printf("%c",stWriteOrderTestInfo.TagBufferPrefix[i]);
    }
    printf("\n");
#endif

	// Create all the write threads
	for(i=0;i<pstWriteOrderTestParams->ulNWriteThreads;i++) {
        pstWriteStreamParams = &pstWriteOrderTestParams->arstWriteStreamParams[i];
        pstWriteThreadParams = &stWriteOrderTestInfo.arstWriteThreadParams[i];
		// Construct the Volume Symbolic Name for this Volume
		c = pstWriteOrderTestParams->arstWriteStreamParams[i].DriveLetter;
        pstWriteThreadParams->pstPerVolumeInfo = &stWriteOrderTestInfo.arstPerVolumeInfo[c-'a'];
		pstWriteThreadParams->strVolumeSymbolicLinkName[0] = L'\\';
		pstWriteThreadParams->strVolumeSymbolicLinkName[1] = L'\\';
		pstWriteThreadParams->strVolumeSymbolicLinkName[2] = L'.';
		pstWriteThreadParams->strVolumeSymbolicLinkName[3] = L'\\';
		pstWriteThreadParams->strVolumeSymbolicLinkName[4] = (WCHAR)c;
		pstWriteThreadParams->strVolumeSymbolicLinkName[5] = L':';
		pstWriteThreadParams->strVolumeSymbolicLinkName[6] = L'\0';
		pstWriteThreadParams->pstPerVolumeInfo = 
			&stWriteOrderTestInfo.arstPerVolumeInfo[c-'a'];
		pstWriteThreadParams->ulNWritesPerTag = pstWriteOrderTestParams->ulNWritesPerTag;
		pstWriteThreadParams->bDataTest = pstWriteOrderTestParams->bDataTest;
        pstWriteThreadParams->ulMaxIOSz = pstWriteStreamParams->ulMaxIOSz;
        pstWriteThreadParams->ulMinIOSz = pstWriteStreamParams->ulMinIOSz;
        pstWriteThreadParams->ulNMaxIOs = pstWriteStreamParams->ulNMaxIOs;
		pstWriteThreadParams->ulThreadIndex = i;
		pstWriteThreadParams->Sequential = pstWriteStreamParams->Random? 0 : 1;
        
		hThreads[nThreads++] = CreateThread(NULL,
											0,
											WriteStream,
											pstWriteThreadParams,
											0,
											NULL);

		ULONG Ret = SetThreadPriority(hThreads[nThreads], THREAD_PRIORITY_HIGHEST);
	}

	// Start ReadThread for all the Active Volumes
	for(i=0;i<MAX_VOLUMES;i++) {
		if(!stWriteOrderTestInfo.arActiveVolumes[i])
			continue;

		// Construct ReadStreamParams for this ReadThread
		hThreads[nThreads++] = CreateThread(NULL,
											0,
											ReadStream,
											&stWriteOrderTestInfo.arstPerVolumeInfo[i],
											0,
											NULL);
	}

	// Wait till all the threads complete their job
	ret = WaitForMultipleObjects(nThreads,hThreads,TRUE,INFINITE);
	printf("WaitForMultipleObjects: %d\n",ret);
	e = time(NULL);
	ULONGLONG sum=0;
	for(i=0;i<stWriteOrderTestInfo.nWriteThreads;i++) {
		sum += stWriteOrderTestInfo.ullTotalData[i];
	}
	printf("please calculate the thruput yourself, timetaken=%dsec,totaldata=%I64dbytes\n",e-s-4,sum);
	if(stWriteOrderTestInfo.bInConsistent) {
		printf("Some error has been reported. Pls refer to c:\\results.txt for details\n");
	}
	fclose(stWriteOrderTestInfo.ChangeEventLogFilePointer);
	memset(&stWriteOrderTestInfo,0,sizeof(WRITE_ORDER_TEST_INFO));
    printf("Test Complete\n");
	return 1;
}

// Function prototypes
int ValidateSVFile      (const char *fileName, bool verboseFlag);
int OnHeader            (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock);
int OnDirtyBlocksData   (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock);
int OnDirtyBlocks       (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock);
int OnDRTDChanges       (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock);
int OnTimeStampInfo     (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock);
int OnUDT               (FILE *in, unsigned long size, unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock, ULONG &EndOfListTagOffset);

BOOLEAN FillDirtyBlockFromFile(PUDIRTY_BLOCK DirtyBlock)
{
    char fileName[4096];
    bool verboseFlag = false;
    
    wcstombs(fileName, DirtyBlock->uTagList.DataFile.FileName, DirtyBlock->uTagList.DataFile.usLength + sizeof(WCHAR));
    
    assert (NULL != fileName);
    
    bool        eof = false;
    int         readin = 0;
    int         chunkerr = 1;
    int         retVal = 0;
    char        *tagname = NULL;
    FILE        *in = NULL;
    SVD_PREFIX  prefix = {0};
    
    //__asm int 3
    // Open and verify that the existance of the requested file
    in = fopen (fileName, "rb");

	printf ("Processing file:  %s \n", fileName );

    if (NULL == in)
    {
        printf ("No such file %s, dwErr = %d\n", fileName , errno);
		fprintf(stWriteOrderTestInfo.ChangeEventLogFilePointer, "Cound Not open file:%s\n", fileName);
        return (retVal);
    }
    
	memset(DirtyBlock->Changes,0,sizeof(DirtyBlock->Changes));

    ULONG EndOfListTagOffset = ULONG((PUCHAR)&DirtyBlock->uTagList.TagList.TagEndOfList - DirtyBlock->uTagList.BufferForTags);
    DirtyBlock->uHdr.Hdr.ulBufferSize = 0x1000; //This value can be read from registry.
    FILL_STREAM_HEADER(&DirtyBlock->uTagList.TagList.TagDataSource, STREAM_REC_TYPE_DATA_SOURCE, sizeof(DATA_SOURCE_TAG));
    DirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = INVOLFLT_DATA_SOURCE_DATA;
    FILL_STREAM_HEADER(&DirtyBlock->uTagList.TagList.TagStartOfList, STREAM_REC_TYPE_START_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
    FILL_STREAM_HEADER(&DirtyBlock->uTagList.TagList.TagPadding, STREAM_REC_TYPE_PADDING, sizeof(STREAM_REC_HDR_4B));
    FILL_STREAM_HEADER(&DirtyBlock->uTagList.TagList.TagEndOfList, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));

    ULONG TotalChanges = DirtyBlock->uHdr.Hdr.cChanges;
    DirtyBlock->uHdr.Hdr.cChanges = 0;
    DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart = 0;
    // Scan through the file one chunk at a time
    while (!eof)
    {
        // Read the chunk prefix
        /* NOTE: The record size and record number fields have intentionally    *
        *   been swapped.  This way, we can distinguish between a full prefix   *
        *   being read (result = sizeof (prefix)), an end-of-file marker in the *
        *   middle of the prefix (0 < result < sizeof (prefix), the only error  *
        *   condition), and a proper end-of-file at the end of the last chunk   *
        *   (result = 0)                                                        */
        readin = (int)fread (&prefix, 1, sizeof (prefix), in);
        
        if (0 == readin)
        {
            retVal = 1;
            eof = true;
        }
        else if (sizeof (prefix) != readin)
        {
            printf ("Couldn't read prefix from file %s\n", fileName);
            retVal = 0;
            eof = true;
        }
        else
        {
            // Now that we have the prefix, determine the tag and branch accordingly
            switch (prefix.tag)
            {
            // File header
            case SVD_TAG_HEADER1:
                chunkerr = OnHeader (in, prefix.count, prefix.Flags, verboseFlag, DirtyBlock);
                tagname = "SVD1";
                break;
            
            // Dirty block with data
            case SVD_TAG_DIRTY_BLOCK_DATA:
                chunkerr = OnDirtyBlocksData (in, prefix.count, prefix.Flags, verboseFlag, DirtyBlock);
                tagname = "DRTD";
                break;
            
            // Dirty block
            case SVD_TAG_DIRTY_BLOCKS:
                chunkerr = OnDirtyBlocks (in, prefix.count, prefix.Flags, verboseFlag, DirtyBlock);
                tagname = "DIRT";
                break;
			
			//Time Stamp First Change
			case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE:
				chunkerr = OnTimeStampInfo (in, prefix.count, prefix.tag, verboseFlag, DirtyBlock);
				tagname = "TOFC";
				break;
			
			//Time Stamp Last Change
			case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE:
				chunkerr = OnTimeStampInfo (in, prefix.count, prefix.tag, verboseFlag, DirtyBlock);
				tagname = "TOLC";
				break;

			//DRTD Changes
			case SVD_TAG_LENGTH_OF_DRTD_CHANGES:
				chunkerr = OnDRTDChanges (in, prefix.count, prefix.Flags, verboseFlag, DirtyBlock);
				tagname = "LODC";
				break;
			
			//User Defined Tag
			case SVD_TAG_USER:
				chunkerr = OnUDT(in, prefix.count, prefix.Flags, verboseFlag, DirtyBlock, EndOfListTagOffset);
				tagname = "USER";
				break;
            
            // Unkown chunk.  Error condition
            default:
                printf ("FAILED: Encountered an unknown tag 0x%lX at offset %ld\n", prefix.tag, ftell (in));
                retVal = 0;
                eof = true;
            }
            
            // If the individual chunk reader failed, so should we
            if (chunkerr == 0)
            {
                printf ("FAILED: Encountered error in chunk \"%s\" at offset %ld\n", tagname, ftell (in));
                retVal = 0;
                eof = true;
            }
        }
    }
    
    // Close the file
    fclose (in);
    
    // Exit
    return (retVal);
}

//
// OnHeader (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnHeader (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock)
{
    assert (in);
    assert (size > 0);
    
    int retVal = 0;
    int readin = 0;
    SVD_HEADER1 *head = new SVD_HEADER1 [size];
    if (NULL == head)
    {
        return (0);
    }
    
    // Read the header chunk
    readin = (int)fread (head, sizeof (SVD_HEADER1), size, in);
    
    // If there is no discrepancy between what we read and what we expected to read, we succeded
    if (size == (unsigned) readin)
    {
        retVal = 1;
        printf ("SVD1 Header\n");
    }
    
    delete [] head;
    return retVal;
}

//
// OnDirtyBlocksData (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnDirtyBlocksData (FILE *in, const unsigned long size, const unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock)
{
    assert (in);
    assert (size > 0);
    
    unsigned long   i = 0;
    unsigned long   offset = 0;
    int             retVal = 1;
    SVD_DIRTY_BLOCK header = {0};
	unsigned long   eof_offset = 0;
    USHORT usBufferIndex = 0;
    USHORT BuffersRequired = 0;

    // For silent mode - commented out the following
	if (verboseFlag) {
		printf ("DRTD\tRecords: %ld\tFile Offset: %ld\n", size, ftell (in));
	}

	//Store the current offset
	offset = ftell (in);

	//Compute the file length
	(void)fseek(in, 0, SEEK_END);
	eof_offset = ftell(in);

	//Restore to the original offset
	(void)fseek(in, offset, SEEK_SET);
	
	// Cycle through all records, reading each one
    for (i = 0; i < size; i++)
    {
        
        // Read the record header
        if (fread (&header, sizeof (SVD_DIRTY_BLOCK), 1, in) == 0)
        {
            printf ("FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
            retVal = 0;
            break;
        }
        
		offset = ftell(in);

// A workaround for our one known bug.  If we're compiling on Windows we can use MS's printf so we will

// For silent mode- commented out the following
		if (verboseFlag) {
#ifndef SV_WINDOWS
        // at least get the full hex values printed
            printf ("[%lu] file offset: %lu, data length: %lu(0x%lu%8.8lu), vol. offset: %lu(0x%lu%8.8lu)\n", 
                i, offset, (unsigned long)header.Length, (unsigned long)(header.Length >> 32), (unsigned long)header.Length,
                (unsigned long)header.ByteOffset, (unsigned long)(header.ByteOffset >> 32), (unsigned long)header.ByteOffset);
#else
        printf ("[%lu] file offset: %lu(0x%x), data length: %I64u(0x%I64x), vol. offset: %I64u(0x%I64x)\n", 
            i, offset, offset, header.Length, header.Length, header.ByteOffset, header.ByteOffset);
#endif // SV_WINDOWS
		}
        
        // If, for some reason we're biting off more than we can chew we stop
        if (header.Length > ULONG_MAX)
        {
            printf ("FAILED: Block header too long (%lu)\n", (unsigned long)header.Length);
            retVal = 0;
            break;
        }

		//Check if we are crossing an end-of-file mark.
		if ( (offset + (unsigned long) header.Length) > eof_offset) {
            printf ("FAILED: Encountered an end-of-file at offset %lu, While trying to seek %lu bytes from offset %lu \n", eof_offset, (unsigned long)header.Length, offset);
            retVal = 0;
            break;

		}

        PUCHAR Buffer = (PUCHAR)malloc((size_t)header.Length);
        if(fread(Buffer, 1, (size_t)header.Length, in) != header.Length)
        {
            printf ("FAILED: Could not read to end of chunk (%lu bytes from offset %lu)\n", (unsigned long)header.Length, offset);
            retVal = 0;
            break;
        }

        DirtyBlock->Changes[DirtyBlock->uHdr.Hdr.cChanges].ByteOffset.QuadPart = header.ByteOffset;
        DirtyBlock->Changes[DirtyBlock->uHdr.Hdr.cChanges].Length = (ULONG)header.Length;
        
        DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart += (ULONG)header.Length;
        
        usBufferIndex = DirtyBlock->Changes[DirtyBlock->uHdr.Hdr.cChanges].usBufferIndex;
        BuffersRequired = (USHORT)((header.Length + DirtyBlock->uHdr.Hdr.ulBufferSize - 1)/ DirtyBlock->uHdr.Hdr.ulBufferSize);

        DirtyBlock->Changes[DirtyBlock->uHdr.Hdr.cChanges].usBufferIndex = usBufferIndex;
        
        DirtyBlock->uHdr.Hdr.ppBufferArray = 
                        (PVOID*)realloc(DirtyBlock->uHdr.Hdr.ppBufferArray, (BuffersRequired + usBufferIndex) * sizeof(PULONG));

        assert(NULL != DirtyBlock->uHdr.Hdr.ppBufferArray);

        for(int i =  0; i < BuffersRequired; i++)
        {
            DirtyBlock->uHdr.Hdr.ppBufferArray[usBufferIndex + i] = Buffer + i * DirtyBlock->uHdr.Hdr.ulBufferSize;
        }

        DirtyBlock->Changes[DirtyBlock->uHdr.Hdr.cChanges].usNumberOfBuffers = BuffersRequired;
        DirtyBlock->uHdr.Hdr.cChanges++;

        //Calculate next buffer free index
        if(DirtyBlock->uHdr.Hdr.cChanges < MAX_UDIRTY_CHANGES)
        {
            DirtyBlock->Changes[DirtyBlock->uHdr.Hdr.cChanges].usBufferIndex = usBufferIndex + BuffersRequired;
        }
    }
    
    printf ("End DRTD\n");
    return retVal;
}

//
// OnDirtyBlocks (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnDirtyBlocks (FILE *in, unsigned long size, unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock)
{
    assert (in);
    assert (size > 0);
    
    unsigned long   i = 0;
    unsigned long   offset = 0;
    int             retVal = 1;
    SVD_DIRTY_BLOCK header;
    
    //For silent mode - commented out the following  
	if (verboseFlag) {
		printf ("DIRT\tRecords: %ld\tFile Offset: %ld\n", size, ftell (in));
	}

	// Cycle through all records, reading each one
    for (i = 0; i < size; i++)
    {
        offset = ftell (in);
        
        // Read the record header
        if (fread (&header, sizeof (SVD_DIRTY_BLOCK), 1, in) == 0)
        {
            printf ("FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
            retVal = 0;
            break;
        }
        
// A workaround for our one known bug.  If we're compiling on Windows we can use MS's printf so we will
//For silent mode - comment it out the following
        if (verboseFlag) {
#ifndef SV_WINDOWS
            // at least get the full hex values printed
            printf ("[%lu] file offset: %lu, data length: %lu(0x%lu%8.8lu), vol. offset: %lu(0x%lu%8.8lu)\n", 
                i, offset, (unsigned long)header.Length, (unsigned long)(header.Length >> 32), (unsigned long)header.Length,
                (unsigned long)header.ByteOffset, (unsigned long)(header.ByteOffset >> 32), (unsigned long)header.ByteOffset);
#else
            printf ("[%lu] file offset: %lu(0x%x), data length: %I64u(0x%I64x), vol. offset: %I64u(0x%I64x)\n", 
                i, offset, offset, header.Length, header.Length, header.ByteOffset, header.ByteOffset);
#endif // SV_WINDOWS
        }
        
        // If, for some reason we're biting off more than we can chew we stop
        if (header.Length > ULONG_MAX)
        {
            printf ("FAILED: Block header too long (%lu)\n", (unsigned long)header.Length);
            retVal = 0;
            break;
        }
    }
    
    printf ("End DIRT\n");
    return retVal;
}

// OnTimeStmapInfo (FILE *in, unsigned int size)
//  Scans a block with a SVD_TIME_STAMP tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags Tag that tells whether it is a TOFC or TOLC
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnTimeStampInfo (FILE *in, unsigned long size, unsigned long tag, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock)
{
    assert (in);
    assert (size > 0);
	unsigned long int i;
	int retVal = 1;
	SVD_TIME_STAMP timestamp;

	if(tag == SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE)
		printf("TOFC\n");
	else
		printf("TOLC\n");

	for(i = 0 ; i < size ; i++ )
	{
		if (fread (&timestamp, sizeof (SVD_TIME_STAMP), 1, in) == 0)
        {
            printf ("FAILED: Record to read record %lu while reading timstamp Info\n", i+1);
            retVal = 0;
            break;
        }

        if(tag == SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE)
            DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange = *(TIME_STAMP_TAG *)&timestamp;
        else
            DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange = *(TIME_STAMP_TAG *)&timestamp;
		
        if(verboseFlag)
		{
			printf("Rec Type %d Flag %d length %d Timestamp %llu Seqno %lu\n", timestamp.Header.usStreamRecType, timestamp.Header.ucFlags, timestamp.Header.ucLength, timestamp.TimeInHundNanoSecondsFromJan1601, timestamp.ulSequenceNumber);
		}
	}
	
	return retVal;
}

// OnDRTDChanges (FILE *in, unsigned int size)
//  Scans a block with a SVD_TAG_LENGTH_OF_DRTD_CHANGES tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnDRTDChanges (FILE *in, unsigned long size, unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock)
{
    assert (in);
    assert (size > 0);
	unsigned long int i;
	unsigned long offset, eof_offset;
	SV_ULONGLONG Changes;
	SVD_PREFIX  prefix = {0};	

	for(i = 0 ; i < size ; i++ )
	{
		if (fread (&Changes, sizeof (SV_ULONGLONG), 1, in) == 0)
        {
            printf ("FAILED: Record to read record %lu while reading timstamp Info", i+1);
			return 0;
        }

		//Store the current offset
		offset = ftell(in);

		(void)fseek(in, 0, SEEK_END);
		eof_offset = ftell(in);

		if((offset + (unsigned long)Changes) > eof_offset)
		{
			printf("FAILED: Encountered size change that is greater than the end offset of the file cur_offset %lu, End offset %lu, Size of ChangesInfo %llu\n", offset, eof_offset, Changes);
			return 0;
		}

		//Restore the offset.
		fseek(in, offset, SEEK_SET);

		//Seek to the end of LODC chunk
		if (fseek (in, (unsigned long)Changes, SEEK_CUR) != 0)
        {
			printf ("FAILED: Could not seek to end of LODC chunk (%lu bytes from offset %lu)\n", (unsigned long)Changes, offset);
            return 0;
        }
		
		//After LODC, we expect a SVD_PREFIX with SVD_TAG_TIME_STAMP_OF_LAST_CHANGE tag. Validate it.
		if (fread (&prefix, sizeof (SVD_PREFIX), 1, in) == 0)
        {
            printf ("FAILED: Record to read svd prefix\n");
            return 0;
        }

		if(prefix.tag != SVD_TAG_TIME_STAMP_OF_LAST_CHANGE)
		{	
			printf("FAILED: After DRTD changes, expecting TOLC but got tag %lu\n", prefix.tag);
			return 0;
		}

		//Restore offset
		fseek(in, offset, SEEK_SET);
		
		if(verboseFlag)
			printf("size of change %llu", Changes);
	}

	printf("End LODC\n");
	return 1;
}

//  OnUDT
//  Scans a block with a SVD_TAG_USER tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//		bool				verbose flag
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnUDT (FILE *in, unsigned long size, unsigned long flags, bool verboseFlag, PUDIRTY_BLOCK DirtyBlock, ULONG &EndOfListTagOffset)
{
	unsigned long TagOffset = EndOfListTagOffset;

	printf("On UDT\n");

	//We store the actual length of UDT in flags field. Seek the file pointer past this
	//UDT
	if(verboseFlag)
	{
		printf("\tLength of UDT: %lu\n", flags);
	}
	
    STREAM_REC_HDR_4B* pUserDefinedTagHeader = (STREAM_REC_HDR_4B*)(DirtyBlock->uTagList.BufferForTags + TagOffset);

    FILL_STREAM_HEADER(pUserDefinedTagHeader, STREAM_REC_TYPE_USER_DEFINED_TAG, 
                        GET_ALIGNMENT_BOUNDARY((sizeof(STREAM_REC_HDR_4B) + flags + sizeof(USHORT)), sizeof(ULONG)));
    
    TagOffset += pUserDefinedTagHeader->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT?sizeof(STREAM_REC_HDR_8B):sizeof(STREAM_REC_HDR_4B);

    *(PUSHORT)(DirtyBlock->uTagList.BufferForTags + TagOffset) = (USHORT)flags;
    
    TagOffset += sizeof(USHORT);

    assert(fread(DirtyBlock->uTagList.BufferForTags + TagOffset, 1, flags, in) == flags);
    
    EndOfListTagOffset += pUserDefinedTagHeader->ucLength;      //Increment the length including padding

    FILL_STREAM_HEADER(&DirtyBlock->uTagList.BufferForTags + EndOfListTagOffset, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));

    printf("UDT: LengthInHeader%d TagLength:%d\n", pUserDefinedTagHeader->ucLength, flags);
	printf("End UDT\n");
	return 1;
}

#endif
