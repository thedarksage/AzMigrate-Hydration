#include<windows.h>
#include<stdio.h>
#include"Involflt.h"
#include"main.h"

extern ULONG bSendProcessStartNotify;

int PrintNextDB(HANDLE hDriver,PPRINT_NEXT_DB_CMD pstPrintNextDBCmd) {
	WCHAR volGuid[GUID_SIZE_IN_CHARS+1];
	UDIRTY_BLOCK stDirtyBlock;

	printf("PrintNextDB: drive=%c: commit=%d printdataios=%d printtagios=%d\n",
			pstPrintNextDBCmd->DriveLetter,pstPrintNextDBCmd->bCommit,
			pstPrintNextDBCmd->bPrintDataIOs,pstPrintNextDBCmd->bPrintTagIOs);
	if(!GetVolumeGUIDFromDriveLetter(pstPrintNextDBCmd->DriveLetter,volGuid,(GUID_SIZE_IN_CHARS+1)*sizeof(WCHAR))) {
		printf("PrintNextDB::Couldnt get GUID for drive Letter %c\n",pstPrintNextDBCmd->DriveLetter);
		return 0;
	}
	if(!GetNextDB(hDriver,0,volGuid,&stDirtyBlock)){
		printf("PrintNextDB::Couldnt get next dirty block, error = %d\n",GetLastError());
		return 0;
	}

	printf("PrintNextDB::The Data source of this Dirty Block is:");

	if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_UNDEFINED) {
		printf("INVOLFLT_DATA_SOURCE_UNDEFINED\n");
	}
	else if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_BITMAP) {
		printf("INVOLFLT_DATA_SOURCE_BITMAP\n");
	}
	else if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_META_DATA) {
		printf("INVOLFLT_DATA_SOURCE_META_DATA\n");
	}
	else if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
		printf("INVOLFLT_DATA_SOURCE_DATA\n");
	}
	else {
		printf("UNRECOGNIZED\n");
		if(pstPrintNextDBCmd->bCommit) goto perform_commit;
	}

	if(pstPrintNextDBCmd->bPrintTagIOs) {
		PSTREAM_REC_HDR_4B pstTag = NULL;
		char *			   pData;
		int				   nTags;
		
		pstTag = &stDirtyBlock.uTagList.TagList.TagEndOfList;
		nTags=0;
		// Iterate through the list of tags
		while(pstTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG) {
			if(pstTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) {
				pData = (char *)((PUCHAR)pstTag + sizeof(STREAM_REC_HDR_8B)+sizeof(ULONG));
			}
			else {
				pData = (char *)((PUCHAR)pstTag + sizeof(STREAM_REC_HDR_4B)+sizeof(USHORT));
			}
			nTags++;
			pstTag = (PSTREAM_REC_HDR_4B) ((PUCHAR)pstTag+pstTag->ucLength);
			printf("Tag%d = %s\n",nTags,pData);
		}
	}

	if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_UNDEFINED) {
		if(pstPrintNextDBCmd->bCommit)
			goto perform_commit;
		else return 1;
	}

	ULONG i;

	printf("This DB has %d changes\n",stDirtyBlock.uHdr.Hdr.cChanges);

	if(stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {	
		if(stDirtyBlock.uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE) {
			printf("This DB has a UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE\n");
		}
		else if(stDirtyBlock.uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE) {
			printf("This DB has a UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE\n");
		}
		else if(stDirtyBlock.uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE) {
			printf("This DB has a UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE\n");
		}
		else {
			printf("This DB doesnt have split changes\n");
		}
	}

#define MAX_PATTERNS_TO_REPORT 3
	ULONG nPatterns;
	ULONG Patterns[MAX_PATTERNS_TO_REPORT+1];
	ULONG PatStart[MAX_PATTERNS_TO_REPORT+1];
	ULONG PatLen[MAX_PATTERNS_TO_REPORT+1];
	ULONG nLen,nBytesRemaining,nTotalBytes,ulBufferSize;
	PULONG pulBuf;
	USHORT j;
	ULONG k;
	
	if(pstPrintNextDBCmd->bPrintDataIOs) {
		for(i=0;i<stDirtyBlock.uHdr.Hdr.cChanges;i++) {
			if((stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_META_DATA) || 
			   (stDirtyBlock.uTagList.TagList.TagDataSource.ulDataSource == INVOLFLT_DATA_SOURCE_BITMAP) ||
			   (!bSendProcessStartNotify)) {
				printf("DB#%d offset=%I64d len=%d\n",i,stDirtyBlock.Changes[i].ByteOffset,
						stDirtyBlock.Changes[i].Length);
			}
			else {
				printf("DB#%d offset=%I64d len=%d startbuffer=%d numbuffers=%d\n",i,stDirtyBlock.Changes[i].ByteOffset,
						stDirtyBlock.Changes[i].Length,stDirtyBlock.Changes[i].usBufferIndex,
						stDirtyBlock.Changes[i].usNumberOfBuffers);
				nPatterns=0;
				nTotalBytes = stDirtyBlock.Changes[i].Length;
				nBytesRemaining = nTotalBytes; 
				ulBufferSize = stDirtyBlock.uHdr.Hdr.ulBufferSize;
				for(j=stDirtyBlock.Changes[i].usBufferIndex;
					j<stDirtyBlock.Changes[i].usBufferIndex+stDirtyBlock.Changes[i].usNumberOfBuffers;j++) {
					pulBuf = (PULONG)stDirtyBlock.uHdr.Hdr.ppBufferArray[j];
					nLen = (nBytesRemaining < ulBufferSize) ? nBytesRemaining:ulBufferSize;
					for(k=0;k<nLen/4;k++) {
						if((nPatterns == 0) || (pulBuf[k] != Patterns[nPatterns-1])) {
							Patterns[nPatterns] = pulBuf[k];
							PatStart[nPatterns] = nTotalBytes-nBytesRemaining;
							PatLen[nPatterns] = sizeof(ULONG);
							nPatterns++;
							if(nPatterns > MAX_PATTERNS_TO_REPORT) {
								break;
							}
						}
						else {
							PatLen[nPatterns-1]+=sizeof(ULONG);
						}
					}
					if(k < nLen/4) {
						// Its time we breaked out as we encountered more than 3 different patterns
						break;
					}
					nBytesRemaining-=nLen;
				}
				if(pstPrintNextDBCmd->bPrintPatterns) {
					for(j=0;j<nPatterns;j++) {
						printf("Pattern=%d start=%d len=%d\n",Patterns[j],PatStart[j],PatLen[j]);
					}
				}
			}
		}
	}
	if(!pstPrintNextDBCmd->bCommit) {
		return 1;
	}
perform_commit:
	CommitDb(hDriver,stDirtyBlock.uHdr.Hdr.uliTransactionID.QuadPart,volGuid);
	return 1;
}

int WriteData(HANDLE hDriver,PWRITE_DATA_CMD pstWriteDataCmd) {
	HANDLE hVolume = INVALID_HANDLE_VALUE;
	WCHAR  strVolumeName[10];
	DWORD fileType;
	ULONG ulLen;
	char *buff = NULL;
	LARGE_INTEGER ullFileSize;
	buff = (char *)malloc(pstWriteDataCmd->ulSize);
	if(buff == NULL) {
		printf("WriteData::couldnt allocate memory\n");
		return 0;
	}
	strVolumeName[0] = L'\\';
	strVolumeName[1] = L'\\';
	strVolumeName[2] = L'.';
	strVolumeName[3] = L'\\';
	strVolumeName[4] = (WCHAR)pstWriteDataCmd->DriveLetter;
	strVolumeName[5] = L':';
	strVolumeName[6] = L'\0';
	// Get a handle on the volume
	hVolume = CreateFile(strVolumeName,
						 GENERIC_READ | GENERIC_WRITE,
						 FILE_SHARE_WRITE | FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 FILE_FLAG_NO_BUFFERING,
						 NULL);
	if(hVolume == INVALID_HANDLE_VALUE) {
		printf("Couldn't obtain handle on %ws, error=%x\n",strVolumeName,GetLastError());
		goto done;
	}
	
	VOLUME_DISK_EXTENTS vd;
	if(!DeviceIoControl(hVolume,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,NULL,0,
		&vd,sizeof(VOLUME_DISK_EXTENTS),&fileType,NULL)) {
		printf("IOCTL_DISK_GET_LENGTH_INFO, error=%x\n",GetLastError());
		goto done;
	}

	ullFileSize.QuadPart = vd.Extents[0].ExtentLength.QuadPart;

	if((pstWriteDataCmd->ulOffset > ullFileSize.QuadPart) || (pstWriteDataCmd->ulOffset+pstWriteDataCmd->ulSize > ullFileSize.QuadPart)) {
		printf("WriteData::Invalid offset and size specified\n");
		goto done;
	}

	ULONG i;
    PULONG plBuff = (PULONG) buff;
    for(i=0;i<pstWriteDataCmd->ulSize/4;i++) {
	    plBuff[i] = pstWriteDataCmd->ulPattern;
    }

	LARGE_INTEGER temp;
	// do the write after seeking to the offset
	temp.QuadPart = pstWriteDataCmd->ulOffset;
	if(!SetFilePointerEx(hVolume,temp,NULL,FILE_BEGIN)) {
		printf("Couldnt set File Pointer\n");
		free(buff);
		return 0;
	}

	if(!WriteFile(hVolume,
				  buff,
				  pstWriteDataCmd->ulSize,
				  &ulLen,
				  NULL)) {
		printf("WriteData::WriteFile, error=%x\n",GetLastError());
		goto done;
	}
	else 
		printf("WriteData::Succesful\n");
done:
	if(hVolume != INVALID_HANDLE_VALUE)
		CloseHandle(hVolume);
	if(buff != NULL)
		free(buff);
	return 1;
}

int WriteTag(HANDLE hDriver,PWRITE_TAG_CMD pstWriteTagCmd) {
	WCHAR volGuids[MAX_VOLUMES*GUID_SIZE_IN_CHARS];
	int nVolumes=0;
	ULONG ulTagLen = 0;
	int ulNTotalTags = 1;
	for(int i=0;i<MAX_VOLUMES;i++) {
		if((pstWriteTagCmd->DrivesBitmap & (1<<i))) {
			GetVolumeGUIDFromDriveLetter('a'+i,volGuids+(nVolumes*GUID_SIZE_IN_CHARS),GUID_SIZE_IN_CHARS*sizeof(WCHAR));
			nVolumes++;
		}
	}
	char buff[MAX_TAG_SIZE];
	GenerateTagPrefixBuffer(buff,&ulTagLen,pstWriteTagCmd->bAtomic,nVolumes,volGuids);
	memcpy(buff+ulTagLen,&ulNTotalTags,sizeof(ULONG));
	ulTagLen += sizeof(ULONG);
	USHORT tagNameLen = (USHORT)strlen(pstWriteTagCmd->TagName);
	memcpy(buff+ulTagLen,&tagNameLen,sizeof(USHORT));
	ulTagLen += sizeof(USHORT);
	memcpy(buff+ulTagLen,pstWriteTagCmd->TagName,strlen(pstWriteTagCmd->TagName));
	ulTagLen += (ULONG)strlen(pstWriteTagCmd->TagName);
	printf("Issuing in a Tag of Length.. %d\n",ulTagLen);
	DWORD fileType;
	if(!DeviceIoControl(hDriver,
						IOCTL_INMAGE_TAG_VOLUME,
						buff,
						ulTagLen,
						NULL,
						0,
						&fileType,
						NULL)) {
		printf("Error issuing Tag %x\n",GetLastError());
		return 0;
	}
	printf("Issued Tag Succesfully\n");
	return 1;
}