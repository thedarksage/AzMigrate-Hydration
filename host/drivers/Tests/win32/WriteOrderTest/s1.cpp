
// to enable compilation of code for GetVolumeNameForVolumeMountPointW...MSDN says this 
// API is available only on Windows versions after XP
#define _WIN32_WINNT 0x0501

#include<stdio.h>
#include<windows.h>
#include<time.h>
#include"InvolFlt.h"
#include"s1.h"

WRITE_ORDER_TEST_INFO stWriteOrderTestInfo;

// For now gets plain index in the PER_IO_INFO Circular List of the Volume
// Assumes that caller is holding the lock on the VolumeInfo
int GenerateIOSeqNumForVolume(PPER_VOLUME_INFO pstPerVolumeInfo) {
	int t;
	t = pstPerVolumeInfo->ulNextActiveIOIndex;
	return t;
}

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
	ULONG temp;
	ULONG i;
	PPER_VOLUME_INFO pstPerVolumeInfo;

	memcpy(buf+nBytesCopied,stWriteOrderTestInfo.TagBufferPrefix,stWriteOrderTestInfo.TagBufferPrefixLength);
	nBytesCopied += stWriteOrderTestInfo.TagBufferPrefixLength;

    memcpy(buf+nBytesCopied,&nTotalTags,sizeof(ULONG));
	nBytesCopied += sizeof(ULONG);

	/*temp = sizeof(STREAM_REC_HDR_4B);
	memcpy(buf+nBytesCopied,&temp,sizeof(USHORT));
	nBytesCopied += sizeof(USHORT);

	stStreamRec.ucFlags = TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;
	stStreamRec.ucLength = 
		(UCHAR)(stWriteOrderTestInfo.nVolumes*(sizeof(ulSeqNum)));
	stStreamRec.usStreamRecType = STREAM_REC_TYPE_USER_DEFINED_TAG;
	memcpy(buf+nBytesCopied,&stStreamRec,sizeof(STREAM_REC_HDR_4B));
	nBytesCopied += sizeof(STREAM_REC_HDR_4B);*/

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
						PPER_IO_INFO pstPerIOInfo) {
	ULONG nBytesCopied=0;
	pstPerIOInfo->ulLen = ulMinSize+GenRandSize(ulMaxSize-ulMinSize);
    // To make the length a multiple of 512, which is sizeof one sector
    pstPerIOInfo->ulLen = pstPerIOInfo->ulLen - (pstPerIOInfo->ulLen%512);
	pstPerIOInfo->ullOffset = GenRandOffset(ulMaxVolSize-pstPerIOInfo->ulLen+1);
    // to make Offset multiple of 512
    pstPerIOInfo->ullOffset = pstPerIOInfo->ullOffset - (pstPerIOInfo->ullOffset%512);
	pstPerIOInfo->ulPattern = GenRandPattern();
	memcpy(buff+nBytesCopied,&pstPerIOInfo->ulIOSeqNum,sizeof(pstPerIOInfo->ulIOSeqNum));
	nBytesCopied += sizeof(pstPerIOInfo->ulIOSeqNum);
	memcpy(buff+nBytesCopied,&pstPerIOInfo->ulThreadId,sizeof(pstPerIOInfo->ulThreadId));
	nBytesCopied += sizeof(pstPerIOInfo->ulThreadId);

    ULONG i;
    PULONG plBuff = (PULONG) (buff+nBytesCopied);
    for(i=0;i<(pstPerIOInfo->ulLen-nBytesCopied)/4;i++) {
	    plBuff[i] = pstPerIOInfo->ulPattern;
    }
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
	if(GenRandSize(nWritesPerTag-1) == 0)
		return 1;
	return 0;
}

DWORD WINAPI WriteStream(LPVOID pArg) {
#define ACTION_FROZEN 0
#define ACTION_DECIDE 1
#define ACTION_IO_DATA 2
#define ACTION_IO_TAG 3
	int                  action;
	ULONG                i;
	time_t               start_time,cur_time;
	ULONG                ulThreadId;
	char                 *buff = NULL;
	int                  nrunning;
	int                  bTag;
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
	while(!pstPerVolumeInfo->bReadStarted) {
		Sleep(2);
	}

	// We will allocate the maximum-sized buffer we might use for this thread
	buff = (char *) malloc(pstWriteThreadParams->ulMaxIOSz);
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
	printf("CreateFile:Error=%x\n",GetLastError());
	if(hVolume == INVALID_HANDLE_VALUE) {
		printf("Couldn't obtain handle on %ws, error=%x\n",pstWriteThreadParams->strVolumeSymbolicLinkName,GetLastError());
		free(buff);
		return 0;
	}
	
	VOLUME_DISK_EXTENTS vd;
	if(!DeviceIoControl(hVolume,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,NULL,0,
		&vd,sizeof(VOLUME_DISK_EXTENTS),&fileType,NULL)) {
		printf("IOCTL_DISK_GET_LENGTH_INFO, error=%x\n",GetLastError());
		free(buff);
		return 0;
	}

	ullFileSize.QuadPart = vd.Extents[0].ExtentLength.QuadPart;

    ulThreadId = pstWriteThreadParams->ulThreadIndex;
	// We will use the threadIndex as seed for Random number generator
	unsigned int rand_seed = time(NULL);
	printf("rand seed = %d\n",(unsigned int)rand_seed);
	srand(rand_seed);

	action = ACTION_FROZEN;

	// Now we can go ahead and issue writes
	start_time = time(NULL);
	while(1) {
		cur_time = time(NULL);
		// check to see if we are done.
		if((!pstWriteThreadParams->ulNMaxIOs) || 
		   ((ULONG)(cur_time - start_time) >= pstWriteThreadParams->ulNSecsToRun)||
		   (stWriteOrderTestInfo.bInConsistent)) {
			stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = 
												THREAD_STATUS_NONE;
			break;
		}
	    if(stWriteOrderTestInfo.bFrozen && (action != ACTION_FROZEN)) {
			stWriteOrderTestInfo.earrWriteThreadStatus[ulThreadId] = THREAD_STATUS_FROZEN;
			action = ACTION_FROZEN;
			continue;
		}
		switch(action) {
		case ACTION_DECIDE:
			bTag = DecideTagOrWrite(pstWriteThreadParams->ulNWritesPerTag);
			action = (bTag) ? ACTION_IO_TAG : ACTION_IO_DATA;
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
            
			GenerateTagBuffer(buff,&ulLen,ularTagSeqNums);
            for(i=0;i<MAX_VOLUMES;i++) {
				if(!stWriteOrderTestInfo.arActiveVolumes[i])
					continue;
                stPerIOInfo.ulIOSeqNum = ularTagSeqNums[i];
                AddNewIOInfoToVolume(&stWriteOrderTestInfo.arstPerVolumeInfo[i],&stPerIOInfo);
            }
			printf("Issuing in a Tag of Length.. %d\n",ulLen);
			if(!DeviceIoControl(stWriteOrderTestInfo.hFilterDriver,
								IOCTL_INMAGE_TAG_VOLUME,
								buff,
								ulLen,
								NULL,
								0,
								&fileType,
								NULL)) {
				printf("Error issuing Tag %x\n",GetLastError());
			}
			printf("Issued Tag\n");
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
							   &stPerIOInfo);
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
				printf("Couldnt set File Pointer\n");
				free(buff);
				return 0;
			}
			if(!WriteFile(hVolume,
					  buff,
					  stPerIOInfo.ulLen,
					  &ulLen,
					  NULL)) {
				printf("WriteFile, error=%x\n",GetLastError());
				free(buff);
				return 0;
			}
					  printf("Data Write: threadid=%d pattern=%d seqNum=%d offset=%I64d, length=%d\n",stPerIOInfo.ulThreadId,
			stPerIOInfo.ulPattern,stPerIOInfo.ulIOSeqNum,stPerIOInfo.ullOffset, stPerIOInfo.ulLen);
			pstWriteThreadParams->ulNMaxIOs--;
			action = ACTION_DECIDE;
			break;
		case ACTION_FROZEN:
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
			break;
		}
	}

	// Free the local buffers we allocated
	CloseHandle(hVolume);
	free(buff);
	return 0;
}

ULONG ValidateDirtyBlockTags(PUDIRTY_BLOCK pstDirtyBlock,
							 PREAD_THREAD_STATE_INFO pstReadThreadStateInfo) {
	PSTREAM_REC_HDR_4B pstTag = NULL;
	PER_IO_INFO		   stPerIOInfo;
	PULONG			   pData;
	PPER_VOLUME_INFO   pstPerVolumeInfo = NULL;
    ULONG              ulHeaderLen;
	int nTags;

	pstPerVolumeInfo = pstReadThreadStateInfo->pstVolumeInfo;
	
	pstTag = &pstDirtyBlock->uTagList.TagList.TagEndOfList;
	if((pstTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) &&
	   (pstReadThreadStateInfo->ulState == 
			PART_OR_END_OF_SPLIT_DISK_CHANGE)) {
		// We are not expecting tags now
		return 0;
	}
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
		printf("recvd tag = %d\n",*pData);

		if(!VerifyIOArrivalOrderForVolume(pstPerVolumeInfo,1,&stPerIOInfo)) {
			pstReadThreadStateInfo->ulCurIOSeqNum = stPerIOInfo.ulIOSeqNum;
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
	printf("......NTags in this DBlock = %d Nchanges = %d\n",nTags,pstDirtyBlock->uHdr.Hdr.cChanges);
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
	if(pstDiskChange->usNumberOfBuffers == 0) {
		printf("DChange without a buffer..invalid\n");
		return 0;
	}
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
				printf("new io threadid=%d, seqnum=%d\n",
					stPerIOInfo.ulThreadId,stPerIOInfo.ulIOSeqNum);
                if(!VerifyIOArrivalOrderForVolume(pstPerVolumeInfo,0,&stPerIOInfo)) {
                    // IO Arrived Out of Order
					pstReadThreadStateInfo->ulCurIOSeqNum = stPerIOInfo.ulIOSeqNum;
                    return 0;
                }
                if(stPerIOInfo.ullOffset != pstDiskChange->ByteOffset.QuadPart) {
                    // Offsets doesn't match
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
                    // Starting Offset doesn't match
                    return 0;
                }
                nLen = MAX_OF_3(pstReadThreadStateInfo->ulLen,
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
				return 0;
			}
		}
		pstReadThreadStateInfo->ulLen -= nLen*4; 
		pstReadThreadStateInfo->ullByteOffset += nLen*4;
		ulDiskChangeLen -= nLen*4;
    }
	if(ulDiskChangeLen != 0) {
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
            return 0;
        }
        if(pstDirtyBlock->uHdr.Hdr.cChanges != 1) {
            // Incorrect number of disk changes in a split dirty block
            printf("%d changes in a dirty block which contains split changes??!\n",
                                                            pstDirtyBlock->uHdr.Hdr.cChanges);
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
                // Split change completed in the START_SPLIT_BLOCK itself
                return 0;
            } 
        }
        pstReadThreadStateInfo->ulState = PART_OR_END_OF_SPLIT_DISK_CHANGE;
        break;
    case UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE:
        if(pstReadThreadStateInfo->ulState == TAG_OR_START_OF_DISK_CHANGE) {
            // Expecting only start of a disk change
            return 0;
        }
        if(pstDirtyBlock->uHdr.Hdr.cChanges != 1) {
            // Incorrect number of disk changes in a split dirty block
            printf("%d changes in a dirty block which contains split changes??!\n",
                                                            pstDirtyBlock->uHdr.Hdr.cChanges);
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
                return 0;
            }
        }
        break;
    case UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE:
        if(pstReadThreadStateInfo->ulState == TAG_OR_START_OF_DISK_CHANGE) {
            // Expecting only part of a disk change
            printf("Expecting start of a disk change but got Part of Split change\n");
            return 0;
        }
        if(pstDirtyBlock->uHdr.Hdr.cChanges != 1) {
            // Incorrect number of disk changes in a split dirty block
            printf("%d changes in a dirty block which contains split changes??!\n",
                                                            pstDirtyBlock->uHdr.Hdr.cChanges);
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

void LogErrorReport(PPER_VOLUME_INFO pstPerVolumeInfo,
			        ULONG       	 ulIOSeqNum) {
	int i;
    WaitForSingleObject(stWriteOrderTestInfo.hWriteThreadsMutex,INFINITE);
    if(stWriteOrderTestInfo.bInConsistent) {
        // Error Report is Logged already
        ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
        return;
    }
	FILE *fp = fopen(stWriteOrderTestInfo.strLogFileName,"w");
	printf("Error Report Logging Started...\n");
	fprintf(fp,"SEQ.NUM\tIO-Type\tArrived\?\tThreadID\tPatternWritten\tOffset\tLength\n");
	fprintf(fp,"This IO with seqNum %d came out of Order\n",ulIOSeqNum);

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
	fclose(fp);
	stWriteOrderTestInfo.bInConsistent = 1;
    ReleaseMutex(stWriteOrderTestInfo.hWriteThreadsMutex);
	printf("Error Report Logging Done.\n");
}

DWORD WINAPI ReadStream(LPVOID pArg) {
	UDIRTY_BLOCK		   stDirtyBlock;
	PPER_VOLUME_INFO	   pstPerVolumeInfo = NULL;
	COMMIT_TRANSACTION	   stCommitTrans;
	READ_THREAD_STATE_INFO stReadThreadStateInfo;
	ULONG				   i;
	DWORD				   nbr;
	DWORD				   ntries;

	pstPerVolumeInfo = (PPER_VOLUME_INFO) pArg;
    pstPerVolumeInfo->bReadStarted = 1;

	stReadThreadStateInfo.ulState = TAG_OR_START_OF_DISK_CHANGE;
	stReadThreadStateInfo.pstVolumeInfo = pstPerVolumeInfo;
	ntries=0;
	memset(&stDirtyBlock,0,sizeof(UDIRTY_BLOCK));
	while(1) {
        if(stWriteOrderTestInfo.bInConsistent)
            break;
		if(ntries == 50)
			break;
		// Get DBTrans
		stCommitTrans.ulTransactionID.QuadPart = stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart;
        memcpy(stCommitTrans.VolumeGUID,pstPerVolumeInfo->volumeGUID,GUID_SIZE_IN_CHARS*sizeof(WCHAR));
		if(!DeviceIoControl(stWriteOrderTestInfo.hFilterDriver,
							IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
							&stCommitTrans,
							sizeof(stCommitTrans),
							&stDirtyBlock,
							sizeof(stDirtyBlock),
							&nbr,
							NULL)) {
			printf("MisCompletion by the Driver..\n");
			Sleep(500);
		}

		if(!ValidateDirtyBlock(&stDirtyBlock,&stReadThreadStateInfo)) {
			// Mark the Volume as Inconsistent
            LogErrorReport(pstPerVolumeInfo,stReadThreadStateInfo.ulCurIOSeqNum);
			break;
		}

		if(stDirtyBlock.uHdr.Hdr.cChanges == 0) {
			ntries++;
			if(ntries == 50) {
				printf("Waited for quite long before getting a change I/O\n");
				break;
			}
		}
		else { 
			ntries = 0;
		}

		pstPerVolumeInfo->ulNIOsWaitingForCommit = 0;
		Sleep(100);
	}
    return 0;
}

void WriteOrderTest(PWRITE_ORDER_TEST_PARAMS pstWriteOrderTestParams) {
	HANDLE hThreads[MAX_WRITE_THREADS+MAX_VOLUMES+1];
	PWRITE_STREAM_PARAMS pstWriteStreamParams = NULL;
	PPER_VOLUME_INFO pstPerVolumeInfo = NULL;
	PWRITE_THREAD_PARAMS pstWriteThreadParams = NULL;
	ULONG nThreads=0;
	ULONG i;
	DWORD ret;
	char c;

	// Get all the Drive Letters we are writing to
	for(i=0;i<pstWriteOrderTestParams->ulNWriteThreads;i++) {
		pstWriteStreamParams = &pstWriteOrderTestParams->arstWriteStreamParams[i];
		if(stWriteOrderTestInfo.arActiveVolumes[pstWriteStreamParams->DriveLetter-'A'])
			continue;
		stWriteOrderTestInfo.nVolumes++;
		stWriteOrderTestInfo.arActiveVolumes[pstWriteStreamParams->DriveLetter-'A'] = 1;
		pstPerVolumeInfo = &stWriteOrderTestInfo.arstPerVolumeInfo[pstWriteStreamParams->DriveLetter-'A'];
        memset(pstPerVolumeInfo,0,sizeof(PER_VOLUME_INFO));
		pstPerVolumeInfo->DriveLetter = pstWriteStreamParams->DriveLetter;
        pstPerVolumeInfo->hActiveIOsMutex = CreateMutex(NULL,FALSE,NULL);
		if(!GetVolumeGUIDFromDriveLetter(pstPerVolumeInfo->DriveLetter,
										pstPerVolumeInfo->volumeGUID,
										sizeof(pstPerVolumeInfo->volumeGUID))) {
			printf("Couldn't obtain GUID for %c:\n",pstPerVolumeInfo->DriveLetter);
		}
		pstPerVolumeInfo->volumeGUID[GUID_SIZE_IN_CHARS] = '\0';
        printf("%ws\n",pstPerVolumeInfo->volumeGUID);
	}

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
        pstWriteThreadParams->pstPerVolumeInfo = &stWriteOrderTestInfo.arstPerVolumeInfo[c-'A'];
		pstWriteThreadParams->strVolumeSymbolicLinkName[0] = L'\\';
		pstWriteThreadParams->strVolumeSymbolicLinkName[1] = L'\\';
		pstWriteThreadParams->strVolumeSymbolicLinkName[2] = L'.';
		pstWriteThreadParams->strVolumeSymbolicLinkName[3] = L'\\';
		pstWriteThreadParams->strVolumeSymbolicLinkName[4] = (WCHAR)c;
		pstWriteThreadParams->strVolumeSymbolicLinkName[5] = L':';
		pstWriteThreadParams->strVolumeSymbolicLinkName[6] = L'\0';
		pstWriteThreadParams->pstPerVolumeInfo = 
			&stWriteOrderTestInfo.arstPerVolumeInfo[c-'A'];
        pstWriteThreadParams->ulNWritesPerTag = DEFAULT_NWRITES_PER_TAG;
        pstWriteThreadParams->ulMaxIOSz = pstWriteStreamParams->ulMaxIOSz;
        pstWriteThreadParams->ulMinIOSz = pstWriteStreamParams->ulMinIOSz;
        pstWriteThreadParams->ulNSecsToRun = pstWriteStreamParams->ulNSecsToRun;
        pstWriteThreadParams->ulNMaxIOs = pstWriteStreamParams->ulNMaxIOs;
		pstWriteThreadParams->ulThreadIndex = i;
        
		hThreads[nThreads++] = CreateThread(NULL,
											0,
											WriteStream,
											pstWriteThreadParams,
											0,
											NULL);
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
}

int GetNumber(char *argv) {
    int n=0;
    while(*argv++ != '=');
    while(*argv != '\0') {
        n = (n*10) + (*argv-'0');
        argv++;
    }
    return n;
}

int main(int argc,char *argv[]) {
#define EXPECTING_SUB_COMMAND 0
#define EXPECTING_SUB_COMMAND_OR_SUB_COMMAND_OPTIONS 1
	int i;
	ULONG state;
    ULONG bDriveLetterRead = 1;
	WRITE_ORDER_TEST_PARAMS stWriteOrderTestParams;
	DWORD bResult, dwReturn;
    PWRITE_STREAM_PARAMS pstWriteStreamParams = NULL;
	char *WriteDataSubOptions[] = {"/miniosz","/maxiosz","/drive","/nsecstorun","/nios"};
	char *WriteOrderTestSubCommands[]= {"-writedata"};
	SHUTDOWN_NOTIFY_INPUT   InputData = {0};
	PSERVICE_START_NOTIFY_DATA   pServiceStartNotifyData = (PSERVICE_START_NOTIFY_DATA)INVALID_HANDLE_VALUE;
	PSHUTDOWN_NOTIFY_DATA   pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)INVALID_HANDLE_VALUE;

	memset(&stWriteOrderTestInfo,0,sizeof(WRITE_ORDER_TEST_INFO));

	// Initialize the Mutex that will be used to synchorize all the threads
    stWriteOrderTestInfo.hWriteThreadsMutex = CreateMutex(NULL,FALSE,NULL);

    stWriteOrderTestInfo.strLogFileName = ERROR_LOG_FILE_NAME;

    memset(&stWriteOrderTestParams,0,sizeof(WRITE_ORDER_TEST_PARAMS));
    state = EXPECTING_SUB_COMMAND;
	for(i=1;i<argc;i++) {
		switch(state){
		case EXPECTING_SUB_COMMAND:
            printf("EXPECTING_SUB_COMMAND %s\n",argv[i]);
			if(!strncmp(argv[i],"-writedata",strlen("-writedata"))) {
                if(!bDriveLetterRead) {
                    printf("Please specify Drive Letter for the WriteData command\n");
                    return 0;
                }
                pstWriteStreamParams = 
                    &stWriteOrderTestParams.arstWriteStreamParams[stWriteOrderTestParams.ulNWriteThreads];
                pstWriteStreamParams->ulNMaxIOs = DEFAULT_MAX_NIOS;
                pstWriteStreamParams->ulMinIOSz = DEFAULT_MIN_IO_SIZE;
                pstWriteStreamParams->ulMaxIOSz = DEFAULT_MAX_IO_SIZE;
                pstWriteStreamParams->ulNSecsToRun = DEFAULT_NSECS_TO_RUN;
                bDriveLetterRead = 0;
                stWriteOrderTestParams.ulNWriteThreads++;
                state = EXPECTING_SUB_COMMAND_OR_SUB_COMMAND_OPTIONS;
				break;
			}
			printf("Invalid Command..Quitting\n");
            return 0;
			break;
		case EXPECTING_SUB_COMMAND_OR_SUB_COMMAND_OPTIONS:
            printf("EXPECTING_SUB_COMMAND_OR_SUB_COMMAND_OPTIONS %s\n",argv[i]);
            if(!strncmp(argv[i],"-writedata",strlen("-writedata"))) {
                if(!bDriveLetterRead) {
                    printf("Please specify Drive Letter for the WriteData command\n");
                    return 0;
                }
                if(pstWriteStreamParams->ulMinIOSz > pstWriteStreamParams->ulMaxIOSz) {
                    printf("miniosz should be less then or equal to maxiosz\n");
                    return 0;
                }
                pstWriteStreamParams = 
                    &stWriteOrderTestParams.arstWriteStreamParams[stWriteOrderTestParams.ulNWriteThreads];
                pstWriteStreamParams->ulNMaxIOs = DEFAULT_MAX_NIOS;
                pstWriteStreamParams->ulMinIOSz = DEFAULT_MIN_IO_SIZE;
                pstWriteStreamParams->ulMaxIOSz = DEFAULT_MAX_IO_SIZE;
                pstWriteStreamParams->ulNSecsToRun = DEFAULT_NSECS_TO_RUN;
                bDriveLetterRead = 0;
                stWriteOrderTestParams.ulNWriteThreads++;
                state = EXPECTING_SUB_COMMAND_OR_SUB_COMMAND_OPTIONS;
				break;
            }
            if(!strncmp(argv[i],"/miniosz",strlen("/miniosz"))) {
                pstWriteStreamParams->ulMinIOSz = GetNumber(argv[i]);
                break;
            }
            if(!strncmp(argv[i],"/maxiosz",strlen("/maxiosz"))) {
                pstWriteStreamParams->ulMaxIOSz = GetNumber(argv[i]);
                break;
            }
            if(!strncmp(argv[i],"/drive",strlen("/drive"))) {
                pstWriteStreamParams->DriveLetter = argv[i][7];
                bDriveLetterRead = 1;
                if((argv[i][7] >= 'A') && (argv[i][7] <= 'Z')) break;
                if((argv[i][7] >= 'a') &&(argv[i][7] <= 'z')) { 
                    pstWriteStreamParams->DriveLetter += 'A'-'a';
                    break;
                }
                printf("Invalid Drive Letter Specified\n");
                return 0;
            }
            if(!strncmp(argv[i],"/nsecstorun",strlen("/nsecstorun"))) {
                pstWriteStreamParams->ulNSecsToRun = GetNumber(argv[i]);
                break;
            }
            if(!strncmp(argv[i],"/nios",strlen("/nios"))) {
                pstWriteStreamParams->ulNMaxIOs = GetNumber(argv[i]);
                break;
            }
            printf("Invalid Command Quitting\n");
			return 0;
		}
	}
    if(stWriteOrderTestParams.ulNWriteThreads == 0) {
        printf("No threads specified \n");
    }


    // Perform the test
	stWriteOrderTestInfo.hFilterDriver = INVALID_HANDLE_VALUE;
	stWriteOrderTestInfo.hFilterDriver = CreateFile (
                            INMAGE_FILTER_DOS_DEVICE_NAME,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED,
                            NULL
                            );
	if( INVALID_HANDLE_VALUE == stWriteOrderTestInfo.hFilterDriver ) {
            _tprintf(_T("CreateFile %s Failed with Error = %#x\n"), INMAGE_FILTER_DOS_DEVICE_NAME, GetLastError());
            return 0;
    }

	pShutdownNotifyData = (PSHUTDOWN_NOTIFY_DATA)malloc(sizeof(SHUTDOWN_NOTIFY_DATA));
    if (NULL == pShutdownNotifyData)
        return 0;

    memset(pShutdownNotifyData, 0, sizeof(SHUTDOWN_NOTIFY_DATA));

    pShutdownNotifyData->hVFCtrlDevice = stWriteOrderTestInfo.hFilterDriver;
    pShutdownNotifyData->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == pShutdownNotifyData->hEvent)
    {
        _tprintf(_T("SendShutdownNotification: CreateEvent Failed with Error = %#x\n"), GetLastError());
		CloseHandle(stWriteOrderTestInfo.hFilterDriver);
        return 0;
    }
    
    pShutdownNotifyData->Overlapped.hEvent = pShutdownNotifyData->hEvent;
    _tprintf(_T("Sending Shutdown Notify IRP\n"));
    InputData.ulFlags = 0;
	InputData.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;

    bResult = DeviceIoControl(
                     stWriteOrderTestInfo.hFilterDriver,
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
            CloseHandle(stWriteOrderTestInfo.hFilterDriver);
			return 0;
        }
    }

	pServiceStartNotifyData = (PSERVICE_START_NOTIFY_DATA)malloc(sizeof(SERVICE_START_NOTIFY_DATA));
    if (NULL == pServiceStartNotifyData)
        return 0;

    memset(pServiceStartNotifyData, 0, sizeof(SERVICE_START_NOTIFY_DATA));

    pServiceStartNotifyData->hVFCtrlDevice = stWriteOrderTestInfo.hFilterDriver;
    pServiceStartNotifyData->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == pServiceStartNotifyData->hEvent)
    {
        _tprintf(_T("SendServiceStartNotification: CreateEvent Failed with Error = %#x\n"), GetLastError());
        CloseHandle(stWriteOrderTestInfo.hFilterDriver);
		return 0;
    }
    
    pServiceStartNotifyData->Overlapped.hEvent = pServiceStartNotifyData->hEvent;
    _tprintf(_T("Sending Process start notify IRP\n"));
    bResult = DeviceIoControl(
                     stWriteOrderTestInfo.hFilterDriver,
                     (DWORD)IOCTL_INMAGE_PROCESS_START_NOTIFY,
                     NULL,
                     0,
                     NULL,
                     0,
                     &dwReturn, 
                     &pServiceStartNotifyData->Overlapped        // Overlapped
                     ); 
    if (0 == bResult) {
        if (ERROR_IO_PENDING != GetLastError())
        {
            _tprintf(_T("SendServiceStartNotification: DeviceIoControl failed with error = %d\n"), GetLastError());
            CloseHandle(stWriteOrderTestInfo.hFilterDriver);
			return 0;
        }
    }
    WriteOrderTest(&stWriteOrderTestParams);
	CancelIo(stWriteOrderTestInfo.hFilterDriver);
	printf("Cancelled all IO\n");
	return 0;
}
