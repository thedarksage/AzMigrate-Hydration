#include <VVCommon.h>

#include <stdio.h>

#define INMAGE_KERNEL_MODE

#include "FileIO.h"
#include "Misc.h"
#include "common.h"
#include "VVDevControl.h"

#define TARGET_LIST_REMOVE_LOCK 'VVTL'
#define VSNAP_MAX_DATA_FILE_SIZE 0x60000000

LIST_ENTRY *VSTgtVolHead=NULL;
ERESOURCE *TgtListLock = NULL;

extern int BtreeSearch(HANDLE BtreeMapHandle, LONGLONG RootOffset, LONGLONG Offset, LONGLONG Length, BtreeMatchEntry *Entry);
extern void InsertInList(PSINGLE_LIST_ENTRY Head, ULONGLONG Offset, ULONGLONG Length,ULONGLONG FileId, ULONGLONG FileOffset);
extern int BtreeInsert(char *RootPtr, VirtualSnapShotInfo *vinfo, KeyAndData & NewKey, PSINGLE_LIST_ENTRY Head, bool flag, int *CopyData);
extern int BtreeGetRootNode(HANDLE BtreeMapHandle, LONGLONG RootOffset, char **RootPtr);

void VsnapAddFileIdToTheTable(VirtualSnapShotInfo *VsnapInfo, FileIdTable *FileTable)
{
	NTSTATUS Status;
	ULONG ByteXfer;
	LARGE_INTEGER DistanceToMove, DistanceMoved;

	DistanceToMove.QuadPart = 0;
	DistanceMoved.QuadPart = 0;
    Status = InSetFilePointer(VsnapInfo->FileIdTableHandle, DistanceToMove, &DistanceMoved, FILE_END);
	if(!IN_SUCCESS(Status))
		return;
	
	Status = InWriteFile(VsnapInfo->FileIdTableHandle, FileTable, sizeof(FileIdTable), &DistanceMoved , &ByteXfer, NULL );
	
	return;
}

int VsnapAlignedRead(char *Buffer, LONGLONG VolumeOffset, LONGLONG ReadLength, DWORD *TotalBytesRead, 
					 DWORD SectorSize, PRAW_READ_HANDLE RawTgtHdl)
{
	char *AlignedBuffer = NULL;
	DWORD BytesRead = 0;
	DWORD BytesToCopy = 0;
	unsigned long BufferOffset = 0;
	
	AlignedBuffer = (char *)InAllocateMemory(SectorSize);
	if(!AlignedBuffer)
	{
		DbgPrint("Failed to allocate buffer in Aligned Read\n");
		return 0;
	}

	*TotalBytesRead = 0;

	while(true)
	{
		LARGE_INTEGER DistanceToMove;
		DistanceToMove.QuadPart = VolumeOffset;

		INSTATUS Status;
		Status = InReadFileRaw(RawTgtHdl, DistanceToMove, SectorSize, AlignedBuffer, &BytesRead);
		if(!IN_SUCCESS(	Status ))
			break;
		
		if(SectorSize > ReadLength)
			BytesToCopy = (DWORD)ReadLength;
		else
			BytesToCopy = SectorSize;

		RtlCopyMemory((char *)Buffer+BufferOffset, (char *)AlignedBuffer, BytesToCopy);

		ReadLength -= BytesToCopy;
		BufferOffset += BytesToCopy;
		
		*TotalBytesRead += BytesToCopy;

		if(ReadLength == 0)
			break;

		VolumeOffset += SectorSize;
	}

	InFreeMemory(AlignedBuffer);

	if(*TotalBytesRead)
		return true;
	else
		return false;
}

void VsnapReadFromVolume(VirtualSnapShotInfo *VsnapInfo, char *Buffer, LONGLONG ByteOffset, ULONG Length, ULONG *BytesRead)
{
	LARGE_INTEGER ReadOffset;
	ULONG ByteRead;
	INSTATUS Status = STATUS_SUCCESS;
	RAW_READ_HANDLE RawTgtHdl;
	
	ReadOffset.QuadPart = ByteOffset;

	if(!IN_SUCCESS(InCreateFileRaw(VsnapInfo->TgtPtr->TargetVolume, &RawTgtHdl)))
		return;
	
	do
	{
		if(((ULONGLONG)Buffer % VsnapInfo->VolumeSectorSize) || (Length % VsnapInfo->VolumeSectorSize))
		{
			if(!VsnapAlignedRead((char *)Buffer, ByteOffset, Length, &ByteRead, VsnapInfo->VolumeSectorSize, &RawTgtHdl))
				break;
				
			*BytesRead += ByteRead;
		}
		else
		{
			Status = InReadFileRaw(&RawTgtHdl, ReadOffset, Length, Buffer, &ByteRead);
			if(!IN_SUCCESS(Status))
				break;
	
			*BytesRead += ByteRead;
		}
	} while (FALSE);

	InCloseFileRaw(&RawTgtHdl);
}

void VsnapUpdateMap(VirtualSnapShotInfo *VsnapInfo, UPDATE_VSNAP_VOLUME_INPUT *LockInput)
{
	int CopyData = 0;
	SINGLE_LIST_ENTRY DirtyBlockHead;
	MapAttribute *Attrib = NULL;
	char *RootPtr = NULL;
	int InsertFileIdIfRequired = 0;

	if(!BtreeGetRootNode(VsnapInfo->BtreeMapFileHandle, VsnapInfo->RootNodeOffset, &RootPtr))
	{
		return;
	}

	InitializeEntryList(&DirtyBlockHead);

	InsertInList(&DirtyBlockHead, LockInput->VolOffset, LockInput->Length, VsnapInfo->DataFileId, LockInput->FileOffset);
		
	while(true)
	{
		if(IsSingleListEmpty(&DirtyBlockHead))
			break;

		KeyAndData NewKey;
		
		Attrib = (MapAttribute *)PopEntryList(&DirtyBlockHead);
			
		NewKey.Offset = Attrib->Offset;		
		NewKey.Length = Attrib->Length;
		NewKey.FileId = Attrib->FileId;
		NewKey.FileOffset = Attrib->FileOffset;
		
		BtreeInsert(RootPtr, VsnapInfo, NewKey, &DirtyBlockHead, true, &CopyData);
		if(CopyData)
			InsertFileIdIfRequired = 1;
		
		InFreeMemory(Attrib);
	}

	if(InsertFileIdIfRequired && !VsnapInfo->DataFileIdUpdated)
	{
		FileIdTable FileTable;
		FileTable.FileId = VsnapInfo->DataFileId;
		RtlStringCbCopyA(FileTable.FileName, 40, LockInput->DataFileName);				
		VsnapAddFileIdToTheTable(VsnapInfo, &FileTable);
		VsnapInfo->DataFileIdUpdated = true;
	}
		
	InFreeMemory(RootPtr);
}

bool VsnapAllocAndInsertTgt(char *TgtVolName, VirtualSnapShotInfo *VsnapInfo)
{
	TargetVolumeList *Tgt=NULL;
	
	Tgt = (TargetVolumeList *)InAllocateMemory(sizeof(TargetVolumeList), NonPagedPool);
	if(NULL == Tgt)
		return false;

	Tgt->TargetVolume[0] = TgtVolName[0];
	Tgt->TargetVolume[1] = TgtVolName[1];
	Tgt->TargetVolume[2] = TgtVolName[2];
	Tgt->Offset = -1;
	Tgt->Length = 0;

	KeInitializeMutex(&Tgt->hTgtMutex, 0);
	KeInitializeMutex(&Tgt->hTgtLockMutex, 0);
	InsertTailList(VSTgtVolHead, (PLIST_ENTRY)Tgt);
	
	InitializeListHead(&Tgt->VsnapHead);
	InsertTailList(&Tgt->VsnapHead, (PLIST_ENTRY)VsnapInfo);
	VsnapInfo->TgtPtr = Tgt;

	return true;
}

TargetVolumeList *VsnapFindTargetFromVolumeName(char *TgtVolName)
{
	TargetVolumeList *Temp = NULL;
	
	if(NULL == VSTgtVolHead)
		return NULL;

	Temp = (TargetVolumeList *)VSTgtVolHead->Flink;
	while((Temp != NULL) && (Temp != (TargetVolumeList *)VSTgtVolHead))
	{
		if((Temp->TargetVolume[0] == TgtVolName[0]) && (Temp->TargetVolume[1] == TgtVolName[1]))
			break;

		Temp = (TargetVolumeList *)(Temp->LinkNext.Flink);
	}
	
	if((Temp == NULL)  || (Temp == (TargetVolumeList *) VSTgtVolHead))
		return NULL;
	else
		return Temp;
}

bool VsnapAddVolumeToList(char *TgtVolName, VirtualSnapShotInfo *VsnapInfo, char *LogDir)
{
	TargetVolumeList *Tgt = NULL;

	Tgt = VsnapFindTargetFromVolumeName(TgtVolName);
	if(NULL == Tgt)
	{
		if(!VsnapAllocAndInsertTgt(TgtVolName, VsnapInfo))
			return false;
		else
		{
			RtlStringCbCopyA(VsnapInfo->TgtPtr->LogDir, 1024, LogDir);
			IoInitializeRemoveLock(&VsnapInfo->VsRemoveLock, NULL, 0, 0);	
			return true;
		}
	}
	else
	{
		InsertTailList(&Tgt->VsnapHead, (PLIST_ENTRY)VsnapInfo);
		IoInitializeRemoveLock(&VsnapInfo->VsRemoveLock, NULL, 0, 0);	
		VsnapInfo->TgtPtr = Tgt;
	}
	
	return true;
}

int VsnapGetFileNameFromFileId(FileIdTable *FileTable, HANDLE FileIdTableHandle)
{
	DWORD BytesRead = 0;
	LARGE_INTEGER FileOffset;
    
	FileOffset.QuadPart = (FileTable->FileId-1)*sizeof(FileIdTable);

	if(!IN_SUCCESS(InReadFile(FileIdTableHandle, FileTable, sizeof(FileIdTable), &FileOffset, 
						&BytesRead, NULL)))
	{
		return 0;
	}

	return 1;
}

bool VsnapOpenAndReadDataFromLog(char *LogDir, void *Buffer, LONGLONG BufferOffset, LONGLONG FileOffset,
							LONGLONG SizeToRead, int FileId, HANDLE FileIdTableHandle)
{
	HANDLE DataFileHandle;
	LARGE_INTEGER DataFileOffset;
	DWORD BytesRead = 0;
	char *ActualFileName= NULL;
	FileIdTable *Table =  NULL;
    INSTATUS Status;

	Table = (FileIdTable *)InAllocateMemory(sizeof(FileIdTable));
	if(!Table)
		return false;

	Table->FileId = FileId;
	if(!VsnapGetFileNameFromFileId(Table, FileIdTableHandle))
	{
		InFreeMemory(Table);
		return false;
	}

	ActualFileName = (char *)InAllocateMemory(1024);
	if(!ActualFileName)
	{
		InFreeMemory(Table);
		return false;
	}

	RtlStringCchCopyA(ActualFileName, 1024, LogDir);
	RtlStringCbCatA(ActualFileName, 1024, "\\");
	RtlStringCbCatA(ActualFileName, 1024, Table->FileName);

	Status = InCreateFileA(ActualFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL, NULL, NULL, &DataFileHandle);

    if(!IN_SUCCESS(Status))
	{
		InFreeMemory(Table);
		InFreeMemory(ActualFileName);
		return false;
	}

	DataFileOffset.QuadPart = FileOffset;
		
	//Read the data into the corresponding position in the buffer.
	char *ActualBuffer = NULL;
	ActualBuffer = (char *)Buffer + BufferOffset;
	if(!IN_SUCCESS(InReadFile(DataFileHandle, ActualBuffer, (DWORD)SizeToRead, &DataFileOffset, &BytesRead, NULL)))
	{
		InFreeMemory(Table);
		InFreeMemory(ActualFileName);
		InCloseHandle(DataFileHandle);
		return false;
	}

	InFreeMemory(Table);
	InFreeMemory(ActualFileName);
	InCloseHandle(DataFileHandle);
	return true;
}

void VsnapInsertInToTheList(PSINGLE_LIST_ENTRY Head, LONGLONG Offset, LONGLONG Length, LONGLONG FileOffset)
{
	MapAttribute *Attrib = NULL;

	Attrib = (MapAttribute *)InAllocateMemory(sizeof(MapAttribute));
	ASSERT(Attrib);
	
	Attrib->Offset = Offset;
	Attrib->Length = Length;
	Attrib->FileOffset = FileOffset;

	PushEntryList(Head, (PSINGLE_LIST_ENTRY)Attrib);
}


int VsnapReadDataUsingMap(void *Buffer, LONGLONG Offset, LONGLONG Length, ULONG *BytesRead, VirtualSnapShotInfo *VsnapInfo)
{
	SINGLE_LIST_ENTRY ListHead;
	LONGLONG CurOffset = 0;
	ULONG CurLength = 0;
	MapAttribute *Attrib = NULL;
	int ret_val = true;
	INSTATUS Status = STATUS_SUCCESS;
		
	//Ensure before calling this function VsnapMapFp points to correct map file.
	//fseek(VsnapMapFp, sizeof(VsnapMapHdr), SEEK_SET);

	// Here, we use STL's map class to dynamically store fragmented off, length pair
	// whoe location is yet to determined from the disk map. When we have found a partial
	// result, we delete the old entry in this map and insert new entried for which
	// a search is still required. This is operated in a while loop until either
	// all entries in the map exhausted or thid dynamic map becomes empty.

	//DbgPrint("Offset %I64u Length %I64u\n", Offset, Length);

	Status = IoAcquireRemoveLock(&VsnapInfo->VsRemoveLock, NULL);
	if(!IN_SUCCESS(Status))
		return false;

	InitializeEntryList(&ListHead);

	VsnapInsertInToTheList(&ListHead, Offset, Length, 0);

	*BytesRead = 0;

	while(!IsSingleListEmpty(&ListHead))
	{
		LONGLONG EntryEnd = 0;
		LONGLONG CurEnd = 0;
		LONGLONG BufferOffset = 0;
		LONGLONG LogFileOffset = 0;
		ULONG SizeToRead = 0;
		
		Attrib = (MapAttribute *)InPopEntryList(&ListHead);

		CurOffset = Attrib->Offset;
		CurLength = (ULONG)Attrib->Length;

		BtreeMatchEntry Entry;
		
		KeWaitForMutexObject(&VsnapInfo->BtreeMapLock, Executive, KernelMode, FALSE, NULL);
		ret_val = BtreeSearch(VsnapInfo->BtreeMapFileHandle, VsnapInfo->RootNodeOffset, CurOffset, CurLength, &Entry);
		KeReleaseMutex(&VsnapInfo->BtreeMapLock, FALSE);

		if(ret_val)
		{
			CurEnd = CurOffset + CurLength - 1;
			EntryEnd = Entry.Offset + Entry.Length - 1;
					
			if((CurOffset >= Entry.Offset) && (CurEnd <= EntryEnd))
			{
				BufferOffset = Attrib->FileOffset;
				LogFileOffset = Entry.DataFileOffset + (CurOffset - Entry.Offset);
				SizeToRead = CurLength;
				//ASSERT(BufferOffset + SizeToRead <= Length);
				if(!VsnapOpenAndReadDataFromLog(VsnapInfo->TgtPtr->LogDir, Buffer, BufferOffset, LogFileOffset, SizeToRead, 
																			Entry.FileId, VsnapInfo->FileIdTableHandle))
				{
					ret_val = false;
					break;
				}
				*BytesRead += SizeToRead;
			}
			else if((CurOffset < Entry.Offset) && (CurEnd > EntryEnd))
			{
				BufferOffset = Attrib->FileOffset + (Entry.Offset - CurOffset);
				LogFileOffset = Entry.DataFileOffset;
				SizeToRead = Entry.Length;
				//ASSERT(BufferOffset + SizeToRead <= Length);
				if(!VsnapOpenAndReadDataFromLog(VsnapInfo->TgtPtr->LogDir, Buffer, BufferOffset, LogFileOffset, SizeToRead, 
																			Entry.FileId, VsnapInfo->FileIdTableHandle))
				{
					ret_val = false;
					break;
				}
				*BytesRead += SizeToRead;

				VsnapInsertInToTheList(&ListHead, CurOffset, (Entry.Offset - CurOffset), Attrib->FileOffset);
				
				Attrib->FileOffset += (EntryEnd - CurOffset + 1);
				VsnapInsertInToTheList(&ListHead, EntryEnd+1, (CurEnd-EntryEnd), Attrib->FileOffset);
			}
			else if( (CurOffset < Entry.Offset) && (CurEnd >= Entry.Offset) && (CurEnd <= EntryEnd))
			{
				BufferOffset = Attrib->FileOffset + (Entry.Offset - CurOffset);
				LogFileOffset = Entry.DataFileOffset;
				SizeToRead = (ULONG) (CurEnd - Entry.Offset + 1);
				//ASSERT(BufferOffset + SizeToRead <= Length);
				if(!VsnapOpenAndReadDataFromLog(VsnapInfo->TgtPtr->LogDir, Buffer, BufferOffset, LogFileOffset, SizeToRead, 
																			Entry.FileId, VsnapInfo->FileIdTableHandle))
				{
					ret_val = false;
					break;
				}
				*BytesRead += SizeToRead;

				VsnapInsertInToTheList(&ListHead, CurOffset, (Entry.Offset - CurOffset), Attrib->FileOffset);
			}
			else if( (CurOffset >= Entry.Offset) && (CurOffset <= EntryEnd) && (CurEnd > EntryEnd))
			{
				BufferOffset = Attrib->FileOffset;
				LogFileOffset = Entry.DataFileOffset + (CurOffset - Entry.Offset);
				SizeToRead = (ULONG)(EntryEnd - CurOffset + 1);
				//ASSERT(BufferOffset + SizeToRead <= Length);
				if(!VsnapOpenAndReadDataFromLog(VsnapInfo->TgtPtr->LogDir, Buffer, BufferOffset, LogFileOffset, SizeToRead, 
																			Entry.FileId, VsnapInfo->FileIdTableHandle))
				{
					ret_val = false;
					break;
				}

				*BytesRead += SizeToRead;

				Attrib->FileOffset += (EntryEnd - CurOffset + 1);
				VsnapInsertInToTheList(&ListHead, EntryEnd + 1, CurEnd - EntryEnd, Attrib->FileOffset);
			}
		}
		else
		{
			//Read the data from the volume and fill the data in the buffer.
            LARGE_INTEGER ByteOffset;
			DWORD ByteRead = 0;
			bool Lock = false;
			
        	
			ByteOffset.QuadPart = CurOffset;
			char *ActualBuffer = ((char*)Buffer + Attrib->FileOffset);
			
			KeWaitForMutexObject(&VsnapInfo->TgtPtr->hTgtMutex, Executive, KernelMode, FALSE, NULL);
			if(VsnapInfo->TgtPtr->Offset == -1 && VsnapInfo->TgtPtr->Length == 0)
				Lock = false;
			else
			{
				LONGLONG TgtOffset = VsnapInfo->TgtPtr->Offset;
				LONGLONG TgtLength = VsnapInfo->TgtPtr->Length;
				LONGLONG TgtEOffset = VsnapInfo->TgtPtr->Offset + VsnapInfo->TgtPtr->Length;
				CurEnd = CurOffset + CurLength - 1;
				
				if(((CurOffset < TgtOffset) && (CurEnd < TgtOffset)) || (CurOffset > TgtEOffset))
					Lock = false;
				else
					Lock = true;

			}
			KeReleaseMutex(&VsnapInfo->TgtPtr->hTgtMutex, FALSE);

			if(Lock)
			{
				KeWaitForMutexObject(&VsnapInfo->TgtPtr->hTgtLockMutex, Executive, KernelMode, FALSE, NULL);
				KeReleaseMutex(&VsnapInfo->TgtPtr->hTgtLockMutex, FALSE);
				VsnapInsertInToTheList(&ListHead,CurOffset, Attrib->Length, Attrib->FileOffset);
			}
			else
			{
				//RtlZeroMemory(ActualBuffer, (SIZE_T)Attrib->Length);
				VsnapReadFromVolume(VsnapInfo, ActualBuffer, ByteOffset.QuadPart, (ULONG)Attrib->Length, BytesRead);
			}
		}

		InFreeMemory(Attrib);
	}

	IoReleaseRemoveLock(&VsnapInfo->VsRemoveLock, NULL);

	if(*BytesRead)
		return true;
	else
		return false;
}

#ifdef INMAGE_KERNEL_MODE

bool VsnapVolumesUpdateMapForTargetVolume(UPDATE_VSNAP_VOLUME_INPUT *LockInput)
{
	TargetVolumeList *tgt = NULL;
	char Target[3];
	int i = 0;
	INSTATUS Status = STATUS_SUCCESS;

	while(true)
	{
		if((LockInput->TargetVolume[i] >= 'a' && LockInput->TargetVolume[i] <= 'z') ||
				(LockInput->TargetVolume[i] >= 'A' && LockInput->TargetVolume[i] <= 'Z'))
		{
			Target[0] = LockInput->TargetVolume[i];
			Target[1] = ':';
			Target[2] = '\0';
			break;
		}
		else if(LockInput->TargetVolume[i] == '\0')
			break;
		
		i++;
	}
	
	if(NULL == TgtListLock)
		return true;

	/* Get Read Lock on the list */
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(TgtListLock, TRUE);
	tgt = VsnapFindTargetFromVolumeName(Target);
	if(NULL != tgt)
	{
		//We are here because there is atleast one active vsnap for this volume
		KeWaitForMutexObject(&tgt->hTgtMutex, Executive, KernelMode, FALSE, NULL);
		tgt->Offset = LockInput->VolOffset;
		tgt->Length = LockInput->Length;
		KeReleaseMutex(&tgt->hTgtMutex, FALSE);

		KeWaitForMutexObject(&tgt->hTgtLockMutex, Executive, KernelMode, FALSE, NULL);

		VirtualSnapShotInfo *vinfo = NULL;
		FileIdTable FileTable;
		vinfo = (VirtualSnapShotInfo *)tgt->VsnapHead.Flink;
		
		while((vinfo != NULL) && (vinfo != (VirtualSnapShotInfo *)&tgt->VsnapHead))
		{
			Status = IoAcquireRemoveLock(&vinfo->VsRemoveLock, NULL);
			if(!IN_SUCCESS(Status))
			{
				vinfo = (VirtualSnapShotInfo*)vinfo->LinkNext.Flink;
				continue;
			}

			KeWaitForMutexObject(&vinfo->BtreeMapLock, Executive, KernelMode, FALSE, NULL);
			if(vinfo->LastDataFileName[0] == '\0')
			{
				RtlStringCbCopyA(vinfo->LastDataFileName, 40, LockInput->DataFileName);
				vinfo->DataFileIdUpdated = false;
			}
			else if((0 != _strnicmp(vinfo->LastDataFileName, LockInput->DataFileName, 40)))
			{
				RtlStringCbCopyA(vinfo->LastDataFileName, 40, LockInput->DataFileName);
				if(vinfo->DataFileIdUpdated)
				{
					vinfo->DataFileId++;
					vinfo->DataFileIdUpdated = false;
				}
			}

			VsnapUpdateMap(vinfo, LockInput);
					
			KeReleaseMutex(&vinfo->BtreeMapLock, FALSE);
			IoReleaseRemoveLock(&vinfo->VsRemoveLock, NULL);

			vinfo = (VirtualSnapShotInfo*)vinfo->LinkNext.Flink;
		}

		KeReleaseMutex(&tgt->hTgtLockMutex, FALSE);
		
		KeWaitForMutexObject(&tgt->hTgtMutex, Executive, KernelMode, FALSE, NULL);
		tgt->Offset = -1;
		tgt->Length = 0;
		KeReleaseMutex(&tgt->hTgtMutex, FALSE);

	}

	ExReleaseResourceLite(TgtListLock);
	KeLeaveCriticalRegion();
	/* Release the read lock */

	return true;
}

int VsnapVolumeRead(void *Buffer, LONGLONG Offset, ULONG BufferLength, ULONG *BytesRead, PVOID VolumeContext)
{
	VirtualSnapShotInfo *VsnapInfo = (VirtualSnapShotInfo *) VolumeContext;

	return VsnapReadDataUsingMap(Buffer, Offset, BufferLength, BytesRead, VsnapInfo);
}

bool VsnapMount(void *Buffer, int BufferLength,	LONGLONG *VolumeSize, PVOID *VolumeContext)
{
	char TargetVolume[3];
	char *FileName=NULL;
	VirtualSnapShotInfo *VsnapInfo = NULL;
	bool Status = true;

	VsnapInfo = (VirtualSnapShotInfo *)InAllocateMemory(sizeof(VirtualSnapShotInfo), NonPagedPool);
	if(NULL == VsnapInfo)
		return false;

	VsnapInfo->BtreeMapFileHandle = NULL;
	VsnapInfo->FileIdTableHandle = NULL;
	
	RetentionMountIoctlData *MountData = (RetentionMountIoctlData *)Buffer;

	do
	{
		//RtlStringCbCopyA(VsnapInfo->LogDir, 1024, MountData->LogDir);
		
		TargetVolume[0] = MountData->TargetVolume[0];
		TargetVolume[1] = ':';
		TargetVolume[2] = '\0';

		FileName = (char *)InAllocateMemory(2048);
		if(!FileName)
		{
			Status = false;
			break;
		}

		RtlStringCchPrintfA(FileName, 2048, "%s\\%I64u_VsnapMap", MountData->LogDir, MountData->SnapShotId);

		if(!IN_SUCCESS(InCreateFileA(FileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE , OPEN_EXISTING, 
					FILE_ATTRIBUTE_NORMAL, NULL, NULL, &VsnapInfo->BtreeMapFileHandle)))
		{
			Status = false;
			break;
		}
		
		RtlStringCchPrintfA(FileName, 2048, "%s\\%I64u_FileIdTable", MountData->LogDir, MountData->SnapShotId);

		if(!IN_SUCCESS(InCreateFileA(FileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE , OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL, NULL, NULL, &VsnapInfo->FileIdTableHandle)))
		{
			Status = false;
			break;
		}
		
		InFreeMemory(FileName);
		FileName = NULL;
		
		VsnapInfo->RootNodeOffset = MountData->RootNodeOffset;
		VsnapInfo->SnapShotId = MountData->SnapShotId;
		VsnapInfo->DataFileId = MountData->NextFileId;
		VsnapInfo->DataFileIdUpdated = false;
		VsnapInfo->NoOfDataFiles = 0;
		VsnapInfo->LastDataFileName[0] ='\0';
		VsnapInfo->RecoveryTime = MountData->RecoveryTime;

		KeInitializeMutex(&VsnapInfo->BtreeMapLock, 0);

		if(0 == MountData->VolumeSectorSize)
			VsnapInfo->VolumeSectorSize = 512;
		else
			VsnapInfo->VolumeSectorSize = MountData->VolumeSectorSize;
		    
		*VolumeSize = MountData->VolumeSize;

	} while (FALSE);

	if(!Status)
	{
		if(VsnapInfo->BtreeMapFileHandle)
			InCloseHandle(VsnapInfo->BtreeMapFileHandle);
		if(VsnapInfo->FileIdTableHandle)
			InCloseHandle(VsnapInfo->FileIdTableHandle);
		if(FileName)
			InFreeMemory(FileName);
		InFreeMemory(VsnapInfo);
	}
	else
	{
		*VolumeContext = VsnapInfo;
		
		/* Get Exclusive target list lock here */
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite(TgtListLock, TRUE);
		if(!VsnapAddVolumeToList(TargetVolume, VsnapInfo, MountData->LogDir))
		{
			if(VsnapInfo->BtreeMapFileHandle)
				InCloseHandle(VsnapInfo->BtreeMapFileHandle);
			if(VsnapInfo->FileIdTableHandle)
				InCloseHandle(VsnapInfo->FileIdTableHandle);
			if(FileName)
				InFreeMemory(FileName);
			InFreeMemory(VsnapInfo);
			Status = false;
		}
		ExReleaseResourceLite(TgtListLock);
		KeLeaveCriticalRegion();
		/* Release Exclusive target list lock */
	}

    return Status;
}

bool VsnapUnmount(PVOID VolumeContext)
{
	char *FileName = NULL;
	VirtualSnapShotInfo *VsnapInfo = (VirtualSnapShotInfo *)(VolumeContext);

	/* Get Exclusive access */
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(TgtListLock, TRUE);
	IoAcquireRemoveLock(&VsnapInfo->VsRemoveLock, NULL);
	IoReleaseRemoveLockAndWait(&VsnapInfo->VsRemoveLock, NULL);

	InCloseHandle(VsnapInfo->BtreeMapFileHandle);
	InCloseHandle(VsnapInfo->FileIdTableHandle);
	
	FileName = (char *)InAllocateMemory(2048);
	if(NULL != FileName)
	{
		RtlStringCchPrintfA(FileName, 2048, "%s\\%I64u_VsnapMap", VsnapInfo->TgtPtr->LogDir, VsnapInfo->SnapShotId);
		InDeleteFileA(FileName);
		RtlStringCchPrintfA(FileName, 2048, "%s\\%I64u_FileIdTable", VsnapInfo->TgtPtr->LogDir, VsnapInfo->SnapShotId);
		InDeleteFileA(FileName);
		RtlStringCchPrintfA(FileName, 2048, "%s\\%I64u_FSData", VsnapInfo->TgtPtr->LogDir, VsnapInfo->SnapShotId);
		InDeleteFileA(FileName);
		InFreeMemory(FileName);
	}
	
	RemoveEntryList(&VsnapInfo->LinkNext);
	if(IsListEmpty(&(VsnapInfo->TgtPtr->VsnapHead)))
	{
		RemoveEntryList(&(VsnapInfo->TgtPtr->LinkNext));
		InFreeMemory(VsnapInfo->TgtPtr);
		ExReleaseResourceLite(TgtListLock);
	}
	else
		ExReleaseResourceLite(TgtListLock);
	/* Release exclusive access */

	KeLeaveCriticalRegion();
	InFreeMemory(VsnapInfo);

	return true;
}

VOID VsnapVolumesUpdateMaps(PVOID InputBuffer)
{
	UPDATE_VSNAP_VOLUME_INPUT *LockInput = (UPDATE_VSNAP_VOLUME_INPUT *)InputBuffer;

	VsnapVolumesUpdateMapForTargetVolume(LockInput);

	return;
}

ULONG VsnapGetVolumeInformationLength(PVOID VolumeContext)
{
	return sizeof(VsnapContextInfo);
}

VOID VsnapGetVolumeInformation(PVOID VolumeContext, PVOID Buffer, ULONG Size)
{
	VirtualSnapShotInfo* Vinfo = (VirtualSnapShotInfo *) VolumeContext;
	VsnapContextInfo VsnapContext;

	VsnapContext.RecoveryTime = Vinfo->RecoveryTime;
	VsnapContext.SnapShotId = Vinfo->SnapShotId;
	VsnapContext.TargetVolume = Vinfo->TgtPtr->TargetVolume[0];

	RtlCopyMemory(Buffer, &VsnapContext, sizeof(VsnapContextInfo));
	return;
}

int VsnapInit()
{
	VSTgtVolHead = (PLIST_ENTRY)InAllocateMemory(sizeof(LIST_ENTRY));
	if(NULL == VSTgtVolHead)
	{
		return false;
	}

	TgtListLock = (ERESOURCE*)InAllocateMemory(sizeof(ERESOURCE), NonPagedPool);
	if(NULL == TgtListLock)
	{
		InFreeMemory(VSTgtVolHead);
		VSTgtVolHead = NULL;
		return false;
	}

	ExInitializeResourceLite(TgtListLock);
	InitializeListHead(VSTgtVolHead);
	
	return true;
}

int VsnapExit()
{
	ExDeleteResource(TgtListLock);

	InFreeMemory(VSTgtVolHead);
	InFreeMemory(TgtListLock);
	VSTgtVolHead = NULL;
	TgtListLock = NULL;

	return true;
}

#endif	