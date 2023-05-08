//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapKernel.cpp
//
// Description: Core APIs used by virtual volume driver to support Read/Write/Write-Tracking for
//				Point-In-Time and Recovery Virtual Snapshots.
//

#include "VsnapKernel.h"
#include "VVDriverContext.h"


LIST_ENTRY	*ParentVolumeListHead=NULL;
VsnapRWLock *ParentVolumeListLock=NULL;

#if (NTDDI_VERSION >= NTDDI_WS03)
NTSTATUS VVImpersonate(PIMPERSONATION_CONTEXT pImpersonationContext, PETHREAD pThreadToImpersonate) {
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    if (NULL == pImpersonationContext) {
        Status = STATUS_INVALID_PARAMETER;        
        return Status;
    }
    /* bSecurityContext indicates token is stored in given sturcture*/
    if (1 == pImpersonationContext->bSecurityContext) {
    	/* Impersonation should be done at passive level */
        if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
            Status = SeImpersonateClientEx(pImpersonationContext->pSecurityContext, pThreadToImpersonate);
        } else {
            Status = STATUS_ACCESS_VIOLATION;
        }
    }
    return Status;
}

NTSTATUS VVCreateClientSecurity(PIMPERSONATION_CONTEXT pImpersonationContext, PETHREAD pThread) {
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    SECURITY_QUALITY_OF_SERVICE SecQualitySrv;
    PSECURITY_CLIENT_CONTEXT pSecurityContext;
    /* Already token is stored. Currently changing token dynamically is not implemented*/
    if (1 == pImpersonationContext->bSecurityContext) {
        return STATUS_SUCCESS;
    }
    pSecurityContext = (PSECURITY_CLIENT_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(SECURITY_CLIENT_CONTEXT), VVTAG_SEC_CONTEXT);
    if (!pSecurityContext) {
       Status = STATUS_INSUFFICIENT_RESOURCES;
       return Status; 
    }
    SecQualitySrv.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
    SecQualitySrv.ImpersonationLevel = SecurityImpersonation;
    SecQualitySrv.ContextTrackingMode = SECURITY_STATIC_TRACKING;
    SecQualitySrv.EffectiveOnly = FALSE;
    if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
        Status = SeCreateClientSecurity(
                pThread,
                &SecQualitySrv,
                FALSE,
                pSecurityContext);
        InVirVolLogError(DriverContext.ControlDevice, NULL, INVIRVOL_IMPERSONATION_STATUS, Status,          
                (ULONG)Status);
	
    } else {
        Status = STATUS_ACCESS_VIOLATION;
    }
    if (!NT_SUCCESS(Status)) {
        ExFreePoolWithTag(pSecurityContext, VVTAG_SEC_CONTEXT);
	    pSecurityContext = NULL;
        return Status;
    }
    
    KeAcquireGuardedMutex(&pImpersonationContext->GuardedMutex);
    if (0 == pImpersonationContext->bSecurityContext) {
        pImpersonationContext->pSecurityContext =  pSecurityContext;
        pImpersonationContext->bSecurityContext = 1;
        pSecurityContext = NULL;
    }        
    KeReleaseGuardedMutex(&pImpersonationContext->GuardedMutex);
    
    if (NULL != pSecurityContext) {
        SeDeleteClientSecurity(pSecurityContext);
        pSecurityContext = NULL;
    }                                        
    return Status;
}
#endif

void PushSubRootNode( PSINGLE_LIST_ENTRY Head, VsnapRootNodeInfo *Entry)
{
	VsnapRootNodeInfo *subRootNode = NULL;
	subRootNode = (VsnapRootNodeInfo *)InPopEntryList(Head);
	if(subRootNode) {
		if(subRootNode->Node == Entry->Node ) {
			subRootNode->RefCount++;
			FREE_MEMORY(Entry);
			InPushEntryList( Head, (PSINGLE_LIST_ENTRY)subRootNode);
		} else {
			Entry->RefCount=1;
			InPushEntryList( Head, (PSINGLE_LIST_ENTRY)subRootNode);
			InPushEntryList( Head, (PSINGLE_LIST_ENTRY)Entry);
		}
	} else {
		Entry->RefCount=1;
		InPushEntryList( Head, (PSINGLE_LIST_ENTRY)Entry);
	}
}

PSINGLE_LIST_ENTRY PopSubRootNode(PSINGLE_LIST_ENTRY Head)
{
	VsnapRootNodeInfo *subRootNode = NULL, *temp;
	subRootNode = (VsnapRootNodeInfo *)InPopEntryList(Head);
	if(subRootNode) {
        if(subRootNode->RefCount >= 2) {
			subRootNode->RefCount--;
			InPushEntryList( Head, (PSINGLE_LIST_ENTRY) subRootNode);
        } else if(subRootNode->RefCount == 1) {
            subRootNode->RefCount--;
        }
	} 

    return (PSINGLE_LIST_ENTRY)subRootNode;

}
/* This function allocates and initializes VsnapBtree from a btree map file handle.
 * VERIFIED.
 */
NTSTATUS VsnapInitBtreeFromHandle(void *Handle, DiskBtree **VsnapBtree)
{
    NTSTATUS Status = STATUS_SUCCESS;
	*VsnapBtree = (DiskBtree*)ALLOC_MEMORY(sizeof(DiskBtree), NonPagedPool);
	if(!*VsnapBtree)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Insufficient memory to allocate Disk Tree structure in \
					VsnapUpdateMap\n"));
		return STATUS_NO_MEMORY;
	}
	
	(*VsnapBtree)->Init(sizeof(VsnapKeyData));
	
	if(!(*VsnapBtree)->InitFromMapHandle(Handle))
	{
		(*VsnapBtree)->Uninit();
		FREE_MEMORY(*VsnapBtree);
        *VsnapBtree = NULL;
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapInitBtreeFromHandle failed to init map from handle\n"));
        Status = STATUS_INTERNAL_ERROR;
	}
		
	return Status;
}

/* This function perform vsnap btree uninitialization and frees the memory.
 * VERIFIED.
 */
void VsnapUninitBtree(DiskBtree *VsnapBtree)
{
	VsnapBtree->Uninit();
	FREE_MEMORY(VsnapBtree);
}

/* This function allocates one VsnapOffsetSplitInfo structure and inserts it into the head list.
 * VERIFIED.
 */
int VsnapAllocAndInsertOffsetSplitInList(PSINGLE_LIST_ENTRY Head, ULONGLONG Offset, ULONG Length,
										ULONG FileId, ULONGLONG FileOffset, ULONG TrackingId, PSINGLE_LIST_ENTRY SubRootHead, void *Node)

{
	VsnapOffsetSplitInfo *OffsetSplit = NULL;

	OffsetSplit = (VsnapOffsetSplitInfo *)ALLOC_MEMORY(sizeof(VsnapOffsetSplitInfo), PagedPool);
	if(!OffsetSplit)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Insufficient memory to allocate Disk Tree structure in \
					VsnapAllocAndInsertOffsetSplitInList\n"));
		return false;
	}

	if(SubRootHead)
	{
		VsnapRootNodeInfoTag  *SubRootNode = NULL;
		SubRootNode = (VsnapRootNodeInfoTag *)ALLOC_MEMORY(sizeof(VsnapRootNodeInfoTag), PagedPool);
		if(!SubRootNode)
		{	
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Insufficient memory to allocate Disk Tree structure in \
					VsnapAllocAndInsertOffsetSplitInList\n"));
			FREE_MEMORY(OffsetSplit);
			return false;
		}

		SubRootNode->Node = Node;
		PushSubRootNode(SubRootHead, SubRootNode);
	}


	
	OffsetSplit->Offset		= Offset;
	OffsetSplit->Length		= Length;
	OffsetSplit->FileOffset = FileOffset;
	OffsetSplit->FileId		= FileId;
	OffsetSplit->TrackingId = TrackingId;

	PushEntryList(Head, (PSINGLE_LIST_ENTRY)OffsetSplit);


	return true;
}

/* This function adds new filename->file id translation information to the FileIdTable.
 * VERIFIED.
 */
bool VsnapAddFileIdToTheTable(VsnapInfo *Vsnap, VsnapFileIdTable *FileTable, bool IsWriteFile)
{
	int			ByteXfer = 0;
	ULONGLONG	DistanceMoved = 0;
	NTSTATUS    ErrStatus = STATUS_SUCCESS;
	ULONG       FileId;

	if(!SEEK_FILE(Vsnap->FileIdTableHandle, 0, &DistanceMoved, SEEK_END))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: SEEK Failed in VsnapAddFileIdToTheTable\n"));
		return false;
	}
	if (IsWriteFile)
		FileId = Vsnap->WriteFileId;
	else 		
	    FileId = Vsnap->DataFileId;

	if(!WRITE_FILE(Vsnap->FileIdTableHandle, FileTable, DistanceMoved, sizeof(VsnapFileIdTable), (int *)&ByteXfer, true, &ErrStatus))
	{
		UNICODE_STRING ErrUniStr = {0};
        ANSI_STRING    ErrAnsiStr = {0};	

		RtlInitAnsiString(&ErrAnsiStr, FileTable->FileName);
        ErrUniStr.MaximumLength = (USHORT)RtlAnsiStringToUnicodeSize(&ErrAnsiStr);
        ErrUniStr.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, ErrUniStr.MaximumLength, VVTAG_GENERIC_PAGED);
        if (NULL != ErrUniStr.Buffer) {
           if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&ErrUniStr, &ErrAnsiStr, false))) {
               ErrUniStr.Buffer[(ErrUniStr.Length) / sizeof(WCHAR)] = L'\0';
               InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_WRITE_TO_FILEID_TABLE_FAILED, ErrStatus,
                                &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
                                ErrUniStr.Buffer, ErrUniStr.Length,
                                Vsnap->SnapShotId);                                
           }
           ExFreePool(ErrUniStr.Buffer);
        } else {
           InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_WRITE_TO_FILEID_TABLE_FAILED, ErrStatus,
                            &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
                            FileId,
                            Vsnap->SnapShotId);
		}

		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Write Failed in VsnapAddFileIdToTheTable\n"));
		return false;
	}

    return true;
}

/* Given a FileId, this function returns true if the file id points to retention log,
 * returns false if it points to a file where private copy of the vsnap is stored. 
 * VERIFIED.
 */
inline bool VsnapDoesFileIdPointToRetentionLog(ULONG FileId)
{
	if(FileId & VSNAP_RETENTION_FILEID_MASK)
		return false;
	else
		return true;
}

/* This function reads the data from the volume if the offset/length are not
 * multiples of sector size. 
 * VERIFIED.
 */
int VsnapAlignedRead(PCHAR Buffer, ULONGLONG VolumeOffset, ULONG ReadLength, 
					 DWORD *TotalBytesRead, DWORD SectorSize, void *RawTgtHdl)
{
	PCHAR	AlignedBuffer = NULL;
	DWORD	BytesRead = 0;
	DWORD	BytesToCopy = 0;
	ULONG	BufferOffset = 0;
	
	AlignedBuffer = (PCHAR)ALLOC_MEMORY(SectorSize, NonPagedPool);
	if(!AlignedBuffer)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Insufficient Memory while allocating buffer in Aligned Read\n"));
		return 0;
	}

	*TotalBytesRead = 0;

	while(true)
	{
		if(!READ_RAW_FILE(RawTgtHdl, AlignedBuffer, VolumeOffset, SectorSize, (int *)&BytesRead))
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Read Failed in VsnapAlignedRead\n"));
			break;
		}
		
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

	FREE_MEMORY(AlignedBuffer);

	if(*TotalBytesRead)
		return true;
	else
		return false;
}

/* Generic function used by the Vsnap module to read data from the volume using Raw Volume Access method.
 * VERIFIED.
 */
bool VsnapReadFromVolume(char *VolumeName, DWORD VolumeSectorSize, PCHAR Buffer, ULONGLONG ByteOffset, ULONG Length, 
						 ULONG *BytesRead)
{
	ULONG	ByteRead = 0;
	void	*RawTgtHdl = NULL;
    bool Success = true;
	
	if(!ALLOC_RAW_HANDLE(&RawTgtHdl))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_RAW_HANDLE Failed in VsnapReadFromVolume\n"));
		return false;
	}

	if(!OPEN_RAW_FILE(VolumeName, RawTgtHdl))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_RAW_FILE Failed in VsnapReadFromVolume\n"));
		return false;
	}

	do
	{
		if(((ULONGLONG)Buffer % VolumeSectorSize) || (Length % VolumeSectorSize))
		{
			if(!VsnapAlignedRead((PCHAR)Buffer, ByteOffset, Length, &ByteRead, VolumeSectorSize, RawTgtHdl))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapAlignedRead() Failed in VsnapReadFromVolume\n"));
                Success = false;
				break;
			}
				
			*BytesRead += ByteRead;
		}
		else
		{
			if(!READ_RAW_FILE(RawTgtHdl, Buffer, ByteOffset, Length, (int *)&ByteRead))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: READ_RAW_FILE Failed in VsnapReadFromVolume\n"));
                Success = false;
				break;
			}
				
			*BytesRead += ByteRead;
		}
	} while (FALSE);

	CLOSE_RAW_FILE(RawTgtHdl);

	FREE_RAW_HANDLE(RawTgtHdl);
    return Success;
}

/* This update method called in write path 
 */
bool VsnapWriteUpdateMap(DiskBtree *VsnapBtree, VsnapKeyData *Key)
{
	SINGLE_LIST_ENTRY		DirtyBlockHead;
	VsnapOffsetSplitInfo	*OffsetSplit = NULL;
	enum BTREE_STATUS		Status = BTREE_SUCCESS;
	bool					Success = true;
	
	VsnapBtree->SetCallBack(&VsnapCallBack);
	VsnapBtree->SetCallBackThirdParam(&DirtyBlockHead);

	InitializeEntryList(&DirtyBlockHead);
	if(!VsnapAllocAndInsertOffsetSplitInList(&DirtyBlockHead, Key->Offset, Key->Length, Key->FileId, Key->FileOffset, 
			Key->TrackingId))
		return false;
		
	while(true)
	{
		if(IsSingleListEmpty(&DirtyBlockHead))
			break;

		OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&DirtyBlockHead);
		
		Status = VsnapBtree->BtreeInsert((VsnapKeyData *)&OffsetSplit->Offset, true); //Insertion with overwrite

		if(Status == BTREE_FAIL)
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Btree Insert Failed in VsnapWriteUpdateMap\n"));
            
			Success = false;
			break;
		}
				
		FREE_MEMORY(OffsetSplit);
		OffsetSplit = NULL;
	}
	
	while(!IsSingleListEmpty(&DirtyBlockHead))
	{
		OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&DirtyBlockHead);
		if(OffsetSplit)
		{
			FREE_MEMORY(OffsetSplit);
			OffsetSplit = NULL;
		}
	}

	return Success;
}

/* This function updates map file of a single Vsnap volume.
 * VERIFIED.
 */
void VsnapUpdateMap(VsnapInfo *Vsnap, ULONGLONG VolOffset, ULONG Length, ULONG ulFileId, ULONGLONG FileOffset, PCHAR FileName, ULONG ulIndex)
{
	SINGLE_LIST_ENTRY		DirtyBlockHead;
	VsnapOffsetSplitInfo	*OffsetSplit = NULL;
	int						InsertFileIdIfRequired = 0;
    DiskBtree				*VsnapBtree = Vsnap->VsnapBtree;
	enum BTREE_STATUS		Status = BTREE_SUCCESS;
	
    if(!VsnapBtree) {
        NTSTATUS Vsnap_Status = VsnapInitBtreeFromHandle(Vsnap->BtreeMapFileHandle, &VsnapBtree);
	    if(!NT_SUCCESS(Vsnap_Status))
		    return;
    }
    Vsnap->VsnapBtree = VsnapBtree;
	
	VsnapBtree->SetCallBack(&VsnapCallBack);
	VsnapBtree->SetCallBackThirdParam(&DirtyBlockHead);

	InitializeEntryList(&DirtyBlockHead);
	if(!VsnapAllocAndInsertOffsetSplitInList(&DirtyBlockHead, VolOffset, Length, ulFileId, FileOffset, 0))
	{
		//VsnapUninitBtree(VsnapBtree);
		return;
	}
	
	while(true)
	{
		if(IsSingleListEmpty(&DirtyBlockHead))
			break;

		OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&DirtyBlockHead);
		
		Status = VsnapBtree->BtreeInsert((VsnapKeyData *)&OffsetSplit->Offset, false); // No overwrite.

		if(Status == BTREE_FAIL)
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Btree Insert Failed in VsnapUpdateMap\n"));
            break;
		}

		if(Status == BTREE_SUCCESS_MODIFIED)
			InsertFileIdIfRequired = 1;
				
		FREE_MEMORY(OffsetSplit);
		OffsetSplit = NULL;
	}
	
	while(!IsSingleListEmpty(&DirtyBlockHead))
	{
		OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&DirtyBlockHead);
		if(OffsetSplit)
		{
			FREE_MEMORY(OffsetSplit);
			OffsetSplit = NULL;
		}
	}

    if(InsertFileIdIfRequired && !Vsnap->DataFileIdUpdated && (Vsnap->DataFileId == ulFileId))
	{
	    	VsnapFileIdTable *FileTable;
		    FileTable = (VsnapFileIdTable *)ALLOC_MEMORY(sizeof(VsnapFileIdTable), NonPagedPool);
            RtlZeroMemory(FileTable, sizeof(VsnapFileIdTable));
		    if(FileTable != NULL)
		    {
			    STRING_COPY(FileTable->FileName, FileName, 50);
			    VsnapAddFileIdToTheTable(Vsnap, FileTable);
			    Vsnap->DataFileIdUpdated = true;
                FREE_MEMORY(FileTable);
                if(Vsnap->IsSparseRetentionEnabled) {
                    Vsnap->ullFileSize += MAX_RETENTION_DATA_FILENAME_LENGTH;
                    InsertIntoCache(Vsnap, ConvertStringToULLHash(FileName), ulFileId, ulIndex);           
                    ASSERT((Vsnap->DataFileId * MAX_RETENTION_DATA_FILENAME_LENGTH) == Vsnap->ullFileSize);
					if ((Vsnap->DataFileId * MAX_RETENTION_DATA_FILENAME_LENGTH) != Vsnap->ullFileSize) {
						UNICODE_STRING ErrUniStr = {0};
						ANSI_STRING    ErrAnsiStr = {0};	

						RtlInitAnsiString(&ErrAnsiStr, FileName);
						ErrUniStr.MaximumLength = (USHORT)RtlAnsiStringToUnicodeSize(&ErrAnsiStr);
						ErrUniStr.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, ErrUniStr.MaximumLength, VVTAG_GENERIC_PAGED);
						if (NULL != ErrUniStr.Buffer) {
						   if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&ErrUniStr, &ErrAnsiStr, false))) {
							   ErrUniStr.Buffer[(ErrUniStr.Length) / sizeof(WCHAR)] = L'\0';
							   InVirVolLogError(DriverContext.ControlDevice, Vsnap->DataFileId, INVIRVOL_VSNAP_FILEID_TABLE_MISMATCH, STATUS_SUCCESS,
												&Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
												ErrUniStr.Buffer, ErrUniStr.Length,
												Vsnap->SnapShotId);
						   }
						   ExFreePool(ErrUniStr.Buffer);
						} else {
						   InVirVolLogError(DriverContext.ControlDevice, Vsnap->DataFileId, INVIRVOL_VSNAP_FILEID_TABLE_MISMATCH, STATUS_SUCCESS,
											&Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
											Vsnap->DataFileId,
											Vsnap->SnapShotId);											
						}
					}
                }
		    }
		    else
			    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapUpdateMap\n"));
            }
    
	Vsnap->RootNodeOffset = VsnapBtree->GetBtreeRootNodeOffset();

	VsnapBtree->ResetCallBack();
	VsnapBtree->ResetCallBackParam();

	//VsnapUninitBtree(VsnapBtree);
}

/* This function creates new target structure and adds to the global list. 
 * VERIFIED.
 */
bool VsnapAllocAndInsertParent(char *ParentVolumeName, VsnapInfo *Vsnap, char *LogDir, MultiPartSparseFileInfo *SparseFileInfo)
{
	VsnapParentVolumeList *Parent = NULL;
	size_t ParentVolumeNameSize;

	if(!ParentVolumeName){return false;}

    if (!NT_SUCCESS(RtlStringCbLengthA(ParentVolumeName, MAX_NAME_LENGTH, &ParentVolumeNameSize))) {
	    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: RtlStringCbLengthA failed for ParentVolumeName %s\n", ParentVolumeName));
		return false;
	}
	
	Parent = (VsnapParentVolumeList *)ALLOC_MEMORY(sizeof(VsnapParentVolumeList), NonPagedPool);
	if(NULL == Parent)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed to allocate memory for Parent in \
								VsnapAllocAndInsertParent\n"));
		return false;
	}
	
	Parent->VolumeSectorSize = 0;
	Parent->IsFile = false;
	Parent->IsMultiPartSparseFile = false;

	Parent->VolumeName = (PCHAR)ALLOC_MEMORY(ParentVolumeNameSize+1, NonPagedPool);
	if(NULL == Parent->VolumeName)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed to allocate memory for VolumeName in 								VsnapAllocAndInsertParent\n"));
		FREE_MEMORY(Parent);
		return false;
	}

    Parent->RetentionDirectory = (PCHAR)ALLOC_MEMORY(MAX_FILENAME_LENGTH, NonPagedPool);

	if(NULL == Parent->RetentionDirectory)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed to allocate memory for RetentionDir in 								VsnapAllocAndInsertParent\n"));
		FREE_MEMORY(Parent->VolumeName);
		FREE_MEMORY(Parent);
		return false;
	}

	Parent->OffsetListLock = (VsnapRWLock *)ALLOC_MEMORY(sizeof(VsnapRWLock), NonPagedPool);
	//Parent->OffsetListLock = (ERESOURCE *)ALLOC_MEMORY(sizeof(ERESOURCE), NonPagedPool);
	if(NULL == Parent->OffsetListLock)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed to allocate memory for OffsetListLock in 								VsnapAllocAndInsertParent\n"));
		FREE_MEMORY(Parent->RetentionDirectory);
		FREE_MEMORY(Parent->VolumeName);
		FREE_MEMORY(Parent);
		return false;
	}

	Parent->TargetRWLock = (VsnapRWLock *)ALLOC_MEMORY(sizeof(VsnapRWLock), NonPagedPool);
	//Parent->OffsetListLock = (ERESOURCE *)ALLOC_MEMORY(sizeof(ERESOURCE), NonPagedPool);
	if(NULL == Parent->TargetRWLock)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed to allocate memory for OffsetListLock in 								VsnapAllocAndInsertParent\n"));
		FREE_MEMORY(Parent->RetentionDirectory);
        FREE_MEMORY(Parent->OffsetListLock);
		FREE_MEMORY(Parent->VolumeName);
		FREE_MEMORY(Parent);
		return false;
	}

    STRING_COPY(Parent->VolumeName, ParentVolumeName, ParentVolumeNameSize + 1);

    if (SparseFileInfo && SparseFileInfo->IsMultiPartSparseFile) {
        Parent->IsMultiPartSparseFile = true;
        Parent->ChunkSize = SparseFileInfo->ChunkSize;
        Parent->SparseFileCount = SparseFileInfo->SplitFileCount;
        Parent->MultiPartSparseFileHandle = (void **)ALLOC_MEMORY((Parent->SparseFileCount) * sizeof(void *), PagedPool);

        if (NULL == Parent->MultiPartSparseFileHandle) {
            InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed to allocate memory for MultiPartSparseFileHandle in     \
                                                     VsnapAllocAndInsertParent\n"));
            FREE_MEMORY(Parent->RetentionDirectory);
            FREE_MEMORY(Parent->OffsetListLock);
            FREE_MEMORY(Parent->VolumeName);
            FREE_MEMORY(Parent);
            return false;
        }
    }

    if('\0' == Parent->VolumeName[2] || (('\\' == Parent->VolumeName[2]) && ('\0' == Parent->VolumeName[3]))) {
        // volume
    } else if (Parent->IsMultiPartSparseFile) {
        // SparseFileCount is ULONG(4294967296) here.It can have upto 10 digits. 
        size_t MaxSparseFileLength = ParentVolumeNameSize + MAX_FILE_EXT_LENGTH + 11;
        char* SparseFilePartName = (char *)ALLOC_MEMORY(MaxSparseFileLength , NonPagedPool);
        
        for (ULONG index = 0; index < Parent->SparseFileCount; index++) {
            STRING_PRINTF((SparseFilePartName, MaxSparseFileLength, "%s%s%d", Parent->VolumeName, SparseFileInfo->FileExt, index));
            NTSTATUS ErrStatus = STATUS_SUCCESS;
            if(!OPEN_FILE(SparseFilePartName, O_RDONLY | O_BUFFER, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                          (void **)&(Parent->MultiPartSparseFileHandle[index]), 
                          true, 
                          &ErrStatus))
            {
                InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN failed for SparseFilePartName %s\n", SparseFilePartName));
                UNICODE_STRING ErrUniStr = {0};
                ANSI_STRING ErrAnsiStr = {0};

                RtlInitAnsiString(&ErrAnsiStr, SparseFilePartName);
                ErrUniStr.MaximumLength = (USHORT)RtlAnsiStringToUnicodeSize(&ErrAnsiStr);
                ErrUniStr.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, ErrUniStr.MaximumLength, VVTAG_GENERIC_PAGED);
                if (NULL != ErrUniStr.Buffer) {
                    if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&ErrUniStr, &ErrAnsiStr, false))) {
                        ErrUniStr.Buffer[(ErrUniStr.Length) / sizeof(WCHAR)] = L'\0';
                        InVirVolLogError(DriverContext.ControlDevice, NULL, INVIRVOL_VOLPACK_OPEN_FILE_FAILURE, STATUS_SUCCESS,
                                        ErrUniStr.Buffer,
                                        ErrUniStr.Length,
                                        (ULONGLONG)ErrStatus);
                   }
                   ExFreePool(ErrUniStr.Buffer);
                }
                FREE_MEMORY(Parent->RetentionDirectory);
                FREE_MEMORY(Parent->OffsetListLock);
                FREE_MEMORY(Parent->VolumeName);
                CloseAndFreeMultiSparseFileHandle(index, Parent->MultiPartSparseFileHandle);
                FREE_MEMORY(SparseFilePartName);
                FREE_MEMORY(Parent);
                return false;
            }
        }
        if (SparseFilePartName)
            FREE_MEMORY(SparseFilePartName);
    } else {
        
        HANDLE                    FileHandle;
        OBJECT_ATTRIBUTES         ObjectAttributes;
        UNICODE_STRING            UnicodeName;
        ANSI_STRING               AnsiString;
        IO_STATUS_BLOCK           IoStatusBlock;
        FILE_STANDARD_INFORMATION FileStandardInfo;
        NTSTATUS                  Status = STATUS_SUCCESS;
        PCHAR                     SparseFileStr = NULL;
		CONST PCHAR               pDosPathName = "\\DosDevices\\";
        size_t                    SparseFileStrLength, DosPathNameSize;


		if (!NT_SUCCESS(RtlStringCbLengthA(pDosPathName, MAX_NAME_LENGTH, &DosPathNameSize))) {
	    	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: RtlStringCbLengthA failed for DosPathNameSize %s\n", pDosPathName));
			FREE_MEMORY(Parent->RetentionDirectory);
            FREE_MEMORY(Parent->OffsetListLock);
            FREE_MEMORY(Parent->VolumeName);
            FREE_MEMORY(Parent);
            return false;
	    }

		SparseFileStrLength = ParentVolumeNameSize + DosPathNameSize + 1;

        SparseFileStr = (PCHAR)ALLOC_MEMORY(SparseFileStrLength, NonPagedPool);
        RtlZeroMemory(SparseFileStr, SparseFileStrLength);

        if(!SparseFileStr) {
            FREE_MEMORY(Parent->RetentionDirectory);
            FREE_MEMORY(Parent->OffsetListLock);
            FREE_MEMORY(Parent->VolumeName);
            FREE_MEMORY(Parent);
            return false;
        }

        STRING_PRINTF((SparseFileStr, SparseFileStrLength, "%s%s", pDosPathName, Parent->VolumeName));
        RtlInitAnsiString(&AnsiString, SparseFileStr);
        RtlAnsiStringToUnicodeString(&UnicodeName, &AnsiString, TRUE);
        Parent->SparseFileHandle = NULL;

        InitializeObjectAttributes ( &ObjectAttributes,
                                &UnicodeName,
                                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                NULL,
                                NULL );
        
        Status = ZwCreateFile(&FileHandle, FILE_READ_ATTRIBUTES, &ObjectAttributes, &IoStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, FILE_NO_INTERMEDIATE_BUFFERING, NULL, 0);

        if (NT_SUCCESS(Status)) {
            Status = ZwQueryInformationFile(FileHandle, &IoStatusBlock, &FileStandardInfo , sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);
            if (NT_SUCCESS(Status))
            {
                 if (FileStandardInfo.Directory == FALSE)
                    Parent->IsFile = true;
            }

            ZwClose(FileHandle);
        }

        if(Parent->IsFile) {

            if(!OPEN_FILE(Parent->VolumeName, O_RDONLY | O_BUFFER, FILE_SHARE_READ | FILE_SHARE_WRITE, &Parent->SparseFileHandle, true, &Status))
            {
                InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN failed for Sparse\n"));
                InVirVolLogError(DriverContext.ControlDevice, NULL, INVIRVOL_VOLPACK_OPEN_FILE_FAILURE, STATUS_SUCCESS,
                                 UnicodeName.Buffer,
                                 UnicodeName.Length,
                                (ULONGLONG)Status);

                FREE_MEMORY(SparseFileStr);
                FREE_MEMORY(Parent->RetentionDirectory);
                FREE_MEMORY(Parent->OffsetListLock);
                FREE_MEMORY(Parent->VolumeName);
                FREE_MEMORY(Parent);
                return false;
            }

        } 
        if (SparseFileStr)
           FREE_MEMORY(SparseFileStr);
        RtlFreeUnicodeString(&UnicodeName);
    }

    TrimLastBackSlashChar(Parent->VolumeName, ParentVolumeNameSize + 1);

    STRING_COPY(Parent->RetentionDirectory, LogDir, MAX_FILENAME_LENGTH);

    Parent->RetentionDirectory[MAX_FILENAME_LENGTH -1] = 0;

    
    InitializeVsnapRWLock(Parent->OffsetListLock);
    InitializeVsnapRWLock(Parent->TargetRWLock);
    //INIT_RW_LOCK(Parent->OffsetListLock);
        
    InitializeListHead(&Parent->OffsetListHead);
    InsertTailList(ParentVolumeListHead, (PLIST_ENTRY)Parent);

    InitializeListHead(&Parent->VsnapHead);
    InsertTailList(&Parent->VsnapHead, (PLIST_ENTRY)Vsnap);

    Vsnap->ParentPtr = Parent;
    return true;
}

/* This function searches the parent list structure and returns the structure if 
 * one is found, otherwise returns NULL. The caller of this function should acquire the
 * parent list lock. 
 * VERIFIED.
 */
VsnapParentVolumeList *VsnapFindParentFromVolumeName(char *ParentVolumeName)
{
	VsnapParentVolumeList *Parent = NULL;

	size_t ParentVolumeNameSize;

	if(NULL == ParentVolumeListHead)
		return NULL;

	if (!NT_SUCCESS(RtlStringCbLengthA(ParentVolumeName, MAX_NAME_LENGTH+1, &ParentVolumeNameSize))) {
	   InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("VsnapFindParentFromVolumeName: RtlStringCbLengthA failed for ParentVolumeName %s\n", ParentVolumeName));
	   return NULL;
	}

	Parent = (VsnapParentVolumeList *)ParentVolumeListHead->Flink;
	while((Parent != NULL) && (Parent != (VsnapParentVolumeList *)ParentVolumeListHead))
	{
		if( 0 == STRING_CMP(Parent->VolumeName, ParentVolumeName, ParentVolumeNameSize + 1))
			break;

		Parent = (VsnapParentVolumeList *)(Parent->LinkNext.Flink);
	}
	
	if((Parent == NULL)  || (Parent == (VsnapParentVolumeList *) ParentVolumeListHead))
		return NULL;
	else
		return Parent;
}

/* Given a offset/length, this function searches through the offset list structure and returns the 
 * match. VERIFIED.
 */
VsnapOffsetLockInfo *VsnapFindMatchingOffsetFromList(ULONGLONG Offset, ULONG Length, VsnapInfo *Vsnap)
{
	VsnapOffsetLockInfo *OffsetLock = NULL;
    ULONGLONG CurEnd = Offset + Length - 1;
	ULONGLONG ListEnd = 0;
	
	OffsetLock = (VsnapOffsetLockInfo *)Vsnap->ParentPtr->OffsetListHead.Flink;
	while((OffsetLock != NULL) && (OffsetLock != (VsnapOffsetLockInfo *)&Vsnap->ParentPtr->OffsetListHead))
	{
		ListEnd = OffsetLock->Offset + OffsetLock->Length - 1;	
		if(((Offset >= OffsetLock->Offset) && (Offset <= ListEnd)) ||
			((CurEnd >= OffsetLock->Offset) && (CurEnd <= ListEnd)) ||
			((OffsetLock->Offset >= Offset) && (OffsetLock->Offset <= CurEnd)) ||
			((ListEnd >= Offset) && (ListEnd <= CurEnd)))
			return OffsetLock;

		OffsetLock = (VsnapOffsetLockInfo *)(OffsetLock->LinkNext.Flink);
	}
	
	return NULL;
}

/* This function adds new Vsnap structure to the global list. It also creates parent structure
 * if one already doesn't exist. 
 * VERIFIED
 */
bool VsnapAddVolumeToList(char *ParentVolumeName, VsnapInfo *Vsnap, char *LogDir, MultiPartSparseFileInfo *SparseFileInfo)
{
	VsnapParentVolumeList	*Parent = NULL;
	bool					success = true;

	//ENTER_CRITICAL_REGION;
	AcquireVsnapWriteLock(ParentVolumeListLock);
	//ACQUIRE_EXCLUSIVE_WRITE_LOCK(ParentVolumeListLock);

	do	
	{
		Parent = VsnapFindParentFromVolumeName(ParentVolumeName);
		if(NULL == Parent)
		{
			if(!VsnapAllocAndInsertParent(ParentVolumeName, Vsnap, LogDir, SparseFileInfo))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapAllocAndInsertParent Failed in VsnapAddVolumeToList\n"));
				success = false;
				break;
			}
			else
				break;
		}
		else
		{
			Vsnap->ParentPtr = Parent;
			InsertTailList(&Parent->VsnapHead, (PLIST_ENTRY)Vsnap);
		}
	} while(FALSE);

	ReleaseVsnapWriteLock(ParentVolumeListLock);
	//RELEASE_RW_LOCK(ParentVolumeListLock);
	//LEAVE_CRITICAL_REGION;
	
	return success;
}

/* Given FileId, this function returns the filename. VERIFIED.
 */
int VsnapGetFileNameFromFileId(VsnapInfo *Vsnap, LONG FileId, PCHAR FileName, bool EnableLog, NTSTATUS *ErrStatus)
{
	int			BytesRead = 0;
	ULONGLONG	FileOffset = 0;
    
	if(FileId == 0)
		FileOffset = 0;
	else
		FileOffset = (FileId-1)*sizeof(VsnapFileIdTable);

	if(!READ_FILE(Vsnap->FileIdTableHandle, FileName, FileOffset, sizeof(VsnapFileIdTable), (int *)&BytesRead, EnableLog, ErrStatus))
		return false;
	
	return true;
}

/* This functions opens and reads the data from the retention data file or from private data file for this vsnap
 * and place it in the appropriate position in the buffer. 
 * VERIFIED.
 */
bool VsnapOpenAndReadDataFromLog(void *Buffer, ULONGLONG BufferOffset, ULONGLONG FileOffset, ULONG SizeToRead, 
								ULONG FileId, VsnapInfo *Vsnap)
{
	void	*DataFileHandle = NULL;
	int		BytesRead = 0;
	char	*ActualFileName= NULL;
	char	*ActualBuffer = NULL;
	bool    close =true;
    bool    found = false;  
    ULONG   CacheEntryCounter = 0;
    ULONG   leastRefCount = ULONG_MAX;
    ULONG   leastRefIndex = 0;
    ULONG   ulIndex = 0;
    PFILE_HANDLE_CACHE FileHandleCache = NULL;
    NTSTATUS ErrStatus = STATUS_SUCCESS;

	if(VsnapDoesFileIdPointToRetentionLog(FileId))
	{
        FileHandleCache = Vsnap->FileHandleCache;
		ActualFileName = (char *)ALLOC_MEMORY(MAX_FILENAME_LENGTH + MAX_RETENTION_DATA_FILENAME_LENGTH, PagedPool);
		
		if(!ActualFileName)
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapOpenAndReadDataFromLog\n"));
			return false;
		}
        RtlZeroMemory(ActualFileName, MAX_FILENAME_LENGTH + MAX_RETENTION_DATA_FILENAME_LENGTH);

		if(VsnapDoesFileIdPointToRetentionLog(FileId))
			STRING_COPY(ActualFileName, Vsnap->ParentPtr->RetentionDirectory, MAX_FILENAME_LENGTH);
		else
			STRING_COPY(ActualFileName, Vsnap->PrivateDataDirectory, MAX_FILENAME_LENGTH);
			
		STRING_CAT(ActualFileName, "\\", MAX_FILENAME_LENGTH + MAX_RETENTION_DATA_FILENAME_LENGTH);         
            
        if(FileHandleCache)
        {
            ASSERT(FileHandleCache->ulNoOfEntriesInCache <= FileHandleCache->ulMaxRetFileHandles);
            AcquireVsnapWriteLock(FileHandleCache->FileHandleCacheLock);
            // Search in Cache
            for(CacheEntryCounter = 0; CacheEntryCounter < FileHandleCache->ulNoOfEntriesInCache; CacheEntryCounter++ )
            {
                // On Cache full, implemented LRU - here finding least referenced entry
                if(FileHandleCache->ulMaxRetFileHandles == FileHandleCache->ulNoOfEntriesInCache
                    && leastRefCount > FileHandleCache->CacheEntries[CacheEntryCounter].ulRefCount 
                    && !FileHandleCache->CacheEntries[CacheEntryCounter].ulUseCount)
                {
                    leastRefCount = FileHandleCache->CacheEntries[CacheEntryCounter].ulRefCount;
                    leastRefIndex = CacheEntryCounter;
                }
                if( FileId == FileHandleCache->CacheEntries[CacheEntryCounter].ulFileId)
                {
                    // On CacheHit
                    DataFileHandle = FileHandleCache->CacheEntries[CacheEntryCounter].pWinHdlTag;
                    FileHandleCache->CacheEntries[CacheEntryCounter].ulRefCount++;
                    FileHandleCache->CacheEntries[CacheEntryCounter].ulUseCount++;
                    FileHandleCache->ullCacheHit++;
                    ulIndex = CacheEntryCounter;

                    found = true;
                    close = false;
                    break;
                }
            }
        }

        if(!found){
			size_t ActualFileNameSize;
			if (!NT_SUCCESS(RtlStringCbLengthA(ActualFileName, MAX_FILENAME_LENGTH + MAX_RETENTION_DATA_FILENAME_LENGTH, &ActualFileNameSize))) {
	 		  	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("VsnapOpenAndReadDataFromLog: RtlStringCbLengthA failed for ActualFileName %s\n", ActualFileName));
	   			return false;
			}
            PCHAR FileNamePos = ActualFileName + ActualFileNameSize;
            if(!VsnapGetFileNameFromFileId(Vsnap, FileId & ~VSNAP_RETENTION_FILEID_MASK, 
                ActualFileName + ActualFileNameSize, true, &ErrStatus))
            {
                InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapGetFileNameFromFileId Failed in \
                                                         VsnapOpenAndReadDataFromLog\n"));
                InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_ERR_GET_FILENAME_FROM_FILEID, STATUS_SUCCESS,
                                 &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
                                 FileId,
                                 Vsnap->SnapShotId,
                                 ErrStatus);
                FREE_MEMORY(ActualFileName);
                if(FileHandleCache)
                    ReleaseVsnapWriteLock(FileHandleCache->FileHandleCacheLock);
                return false;
            }

            ActualFileName[MAX_FILENAME_LENGTH + MAX_RETENTION_DATA_FILENAME_LENGTH - 1] = '\0';	//Safe guard for corrupt file name
            
            UNICODE_STRING ErrUniStr = {0};
            ANSI_STRING ErrAnsiStr = {0};
            char FileName[50] = {0};
            STRING_COPY(FileName, FileNamePos, MAX_RETENTION_DATA_FILENAME_LENGTH);
            FileName[MAX_RETENTION_DATA_FILENAME_LENGTH-1] = '\0';
            ErrStatus = STATUS_SUCCESS;

            if(!OPEN_FILE(ActualFileName, O_RDONLY | O_BUFFER, FILE_SHARE_READ | FILE_SHARE_WRITE, &DataFileHandle, true, &ErrStatus))
            {
                InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_FILE Failed in VsnapOpenAndReadDataFromLog\n"));
                RtlInitAnsiString(&ErrAnsiStr, FileName);
                ErrUniStr.MaximumLength = (USHORT)RtlAnsiStringToUnicodeSize(&ErrAnsiStr);
                ErrUniStr.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, ErrUniStr.MaximumLength, VVTAG_GENERIC_PAGED);
                if (NULL != ErrUniStr.Buffer) {
                   if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&ErrUniStr, &ErrAnsiStr, false))) {
                       ErrUniStr.Buffer[(ErrUniStr.Length) / sizeof(WCHAR)] = L'\0';
                       InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_DATA_FILE_OPEN_FAILED, ErrStatus,
                                        &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
                                        ErrUniStr.Buffer, ErrUniStr.Length,
                                        Vsnap->SnapShotId);
                   }

                   ExFreePool(ErrUniStr.Buffer);
                }
                FREE_MEMORY(ActualFileName);
                if(FileHandleCache)
                    ReleaseVsnapWriteLock(FileHandleCache->FileHandleCacheLock);
                return false;
            }
            
            if(FileHandleCache)
            {
                close = false;
                // If Cache is not full. append the entry
                if( FileHandleCache->ulMaxRetFileHandles != FileHandleCache->ulNoOfEntriesInCache ){
                    FileHandleCache->CacheEntries[FileHandleCache->ulNoOfEntriesInCache].pWinHdlTag = DataFileHandle;
                    FileHandleCache->CacheEntries[FileHandleCache->ulNoOfEntriesInCache].ulFileId = FileId;
                    FileHandleCache->CacheEntries[FileHandleCache->ulNoOfEntriesInCache].ulRefCount = 1;
                    FileHandleCache->CacheEntries[FileHandleCache->ulNoOfEntriesInCache].ulUseCount = 1;
                    ulIndex = FileHandleCache->ulNoOfEntriesInCache;
                    FileHandleCache->ulNoOfEntriesInCache++;
                }
                else // Cache is full. 
                {
                    // Close the file handle having least referenced entry
                    CLOSE_FILE(FileHandleCache->CacheEntries[leastRefIndex].pWinHdlTag );
                    // Replace least rescently used entry
                    FileHandleCache->CacheEntries[leastRefIndex].pWinHdlTag = DataFileHandle;
                    FileHandleCache->CacheEntries[leastRefIndex].ulFileId = FileId;
                    FileHandleCache->CacheEntries[leastRefIndex].ulRefCount = 1;
                    FileHandleCache->CacheEntries[leastRefIndex].ulUseCount = 1;
                    ulIndex = leastRefIndex;
                    FileHandleCache->ullCacheMiss++;
                }
            }
		}
        if(FileHandleCache)
            ReleaseVsnapWriteLock(FileHandleCache->FileHandleCacheLock);
	}
	else
	{
		if(FileId == (Vsnap->WriteFileId | VSNAP_RETENTION_FILEID_MASK))
			DataFileHandle = Vsnap->WriteFileHandle;
		else
			DataFileHandle = Vsnap->FSDataHandle;

		close = false;
	}

	ActualBuffer = (char *)Buffer + BufferOffset;
	ErrStatus = STATUS_SUCCESS;

	if(!READ_FILE(DataFileHandle, ActualBuffer, FileOffset, SizeToRead, &BytesRead, true, &ErrStatus))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: READ_FILE Failed in VsnapOpenAndReadDataFromLog\n"));
        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_READ_FROM_DATA_FILE_FAILED, STATUS_SUCCESS,
			             &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
						 FileId,
						 Vsnap->SnapShotId,
						 ErrStatus);
		FILE_STANDARD_INFORMATION FileStandardInfo;
	    IO_STATUS_BLOCK IoStatus;
		NTSTATUS Status;

		Status = ZwQueryInformationFile(((WinHdl*)DataFileHandle)->Handle, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                                        FileStandardInformation);
		if (NT_SUCCESS(Status)) {
			if (FileOffset > (ULONGLONG)FileStandardInfo.EndOfFile.QuadPart) {
				InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_OFFSET_BEYOND_FILE_SIZE, STATUS_SUCCESS,
								 &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
								 FileOffset,
								 Vsnap->SnapShotId,
								 (ULONGLONG)FileStandardInfo.EndOfFile.QuadPart);
			}
		}
        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_OFFSET_READ_FAILURE, STATUS_SUCCESS,
						 &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
						 FileOffset,
                         (ULONGLONG)SizeToRead);

		if(ActualFileName)
			FREE_MEMORY(ActualFileName);
		if(close)
			CLOSE_FILE(DataFileHandle);
		return false;
	}
	
    if(FileHandleCache) {
        AcquireVsnapWriteLock(FileHandleCache->FileHandleCacheLock);

        FileHandleCache->CacheEntries[ulIndex].ulUseCount--;

        ReleaseVsnapWriteLock(FileHandleCache->FileHandleCacheLock);
    }
	
	if(ActualFileName)
		FREE_MEMORY(ActualFileName);

	if(close)
		CLOSE_FILE(DataFileHandle);

	return true;
}

/* This is the core API that does all the work of getting data either from parent volume or retention logs.
 *
 * Pseudocode:
 *		1. Create VsnapOffsetSplitInfo structure with Voloffset, Length that are passed to this function. Add this
 *		   to the global list of unsatisfied offset/length pair.
 *		2. while the global list is not empty
 *				1. Search the btree map
 *				2. If success
 *						1. Read the data of matched offset/length from retention logs and place it appropriately in the	 
 *						   buffer.
 *						2. If required add the unsatisfied offset/length to the global list.
 *				3. else
 *						1. Read the data directly from the parent volume. PARTIALLY DONE
 * VERIFIED.
 */ 
int VsnapReadDataUsingMap(void *Buffer, ULONGLONG Offset, ULONG Length, ULONG *BytesRead, VsnapInfo *Vsnap, PSINGLE_LIST_ENTRY Head, bool *TargetRead)
{
	SINGLE_LIST_ENTRY		OffsetSplitListHead,SubRootNodeList;
	ULONGLONG				CurOffset = 0, EntryEnd = 0, CurEnd = 0, LogFileOffset = 0;
	ULONG					BufferOffset = 0, SizeToRead = 0, CurLength = 0;
	VsnapOffsetSplitInfo	*OffsetSplit = NULL,*OffsetInfo = NULL;
	VsnapRootNodeInfo       *subRootNode = NULL;
	int						Success = true;
	VsnapKeyData			*Entry = NULL;
	enum BTREE_STATUS		Status = BTREE_SUCCESS;
    DiskBtree				*VsnapBtree = Vsnap->VsnapBtree;
	void                    *NewRoot;
    int                     n = 0;

	if(!ACQUIRE_REMOVE_LOCK(&Vsnap->VsRemoveLock))
		return false;
	
	InitializeEntryList(&OffsetSplitListHead);
	InitializeEntryList(&SubRootNodeList);
	*BytesRead = 0;

	//ENTER_CRITICAL_REGION;
	AcquireVsnapReadLock(Vsnap->BtreeMapLock);
	//ACQUIRE_SHARED_READ_LOCK(Vsnap->BtreeMapLock);
    if(!VsnapBtree) {
        NTSTATUS Vsnap_Status = VsnapInitBtreeFromHandle(Vsnap->BtreeMapFileHandle, &VsnapBtree);
	    if(!NT_SUCCESS(Vsnap_Status))
	    {
		    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapInitBtreeFromHandle Failed in VsnapReadDataUsingMap\n"));
		    ReleaseVsnapReadLock(Vsnap->BtreeMapLock);
		    //RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
		    //LEAVE_CRITICAL_REGION;
		    RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
		    return false;
	    }
    }
    Vsnap->VsnapBtree = VsnapBtree;

	Entry = (VsnapKeyData *)ALLOC_MEMORY(sizeof(VsnapKeyData), NonPagedPool);
	if(!Entry)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapReadDataUsingMap\n"));
		//VsnapUninitBtree(VsnapBtree);
		ReleaseVsnapReadLock(Vsnap->BtreeMapLock);
		//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
		//LEAVE_CRITICAL_REGION;
		RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
		return false;
	}
	
	if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetSplitListHead, Offset, Length, 0, 0, 0, &SubRootNodeList,Vsnap->VsnapBtree->GetRootNodeFromDiskBtree()))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapAllocAndInsertOffsetSplitInList Failed in \
				VsnapReadDataUsingMap\n"));
		//VsnapUninitBtree(VsnapBtree);
		FREE_MEMORY(Entry);
		ReleaseVsnapReadLock(Vsnap->BtreeMapLock);
		//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
		//LEAVE_CRITICAL_REGION;
		RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
		return false;
	}

    while(!IsSingleListEmpty(&OffsetSplitListHead))
    {
        OffsetInfo = (VsnapOffsetSplitInfo *)ALLOC_MEMORY_WITH_TAG(sizeof(VsnapOffsetSplitInfo), PagedPool, VVTAG_VSNAP_OFFSET_INFO);
        if(!OffsetInfo) {
             Success = false;
             break;
        }
        n++;
        OffsetSplit = NULL;
        subRootNode = NULL;
        subRootNode = (VsnapRootNodeInfo *)PopSubRootNode(&SubRootNodeList);
        OffsetSplit = (VsnapOffsetSplitInfo *)InPopEntryList(&OffsetSplitListHead);
        
		if(NULL == OffsetSplit)
            break;

        CurOffset = OffsetSplit->Offset;
        CurLength = OffsetSplit->Length;

		Success = VsnapBtree->BtreeSearch(subRootNode->Node,(VsnapKeyData *)&OffsetSplit->Offset, (VsnapKeyData*)Entry, &NewRoot);
        
		if(Success == BTREE_SUCCESS)
        {
            CurEnd = CurOffset + CurLength - 1;
            EntryEnd = Entry->Offset + Entry->Length - 1;
            
            if((CurOffset >= Entry->Offset) && (CurEnd <= EntryEnd))
            {
                BufferOffset = (ULONG)OffsetSplit->FileOffset;
                LogFileOffset = Entry->FileOffset + (CurOffset - Entry->Offset);
				SizeToRead = CurLength;
                OffsetInfo->Offset = BufferOffset;
                OffsetInfo->FileOffset = LogFileOffset;
                OffsetInfo->FileId = Entry->FileId;
                OffsetInfo->Length = SizeToRead;
                InPushEntryList(Head, (SINGLE_LIST_ENTRY *)OffsetInfo);
                OffsetInfo = NULL;

            /*  if(!VsnapOpenAndReadDataFromLog(Buffer, BufferOffset, LogFileOffset, SizeToRead, 
                                                                            Entry->FileId, Vsnap))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapOpenAndReadDataFromLog Failed in \
                            VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
                */
                *BytesRead += SizeToRead;
                if(subRootNode) {
                    if(NewRoot != subRootNode->Node)
                        DiskBtree::FreeNode(NewRoot);
                    else if(!subRootNode->RefCount) {
                        if(subRootNode->Node != Vsnap->VsnapBtree->GetRootNodeFromDiskBtree()) 
                            DiskBtree::FreeNode(subRootNode->Node);
                        FREE_MEMORY(subRootNode);
                        subRootNode = NULL;
                    }
                }
            }
            else if((CurOffset < Entry->Offset) && (CurEnd > EntryEnd))
            {
                BufferOffset = (ULONG)(OffsetSplit->FileOffset + (Entry->Offset - CurOffset));
                LogFileOffset = Entry->FileOffset;
                SizeToRead = Entry->Length;
                OffsetInfo->Offset = BufferOffset;
                OffsetInfo->FileOffset = LogFileOffset;
                OffsetInfo->FileId = Entry->FileId;
                OffsetInfo->Length = SizeToRead;
                InPushEntryList(Head, (SINGLE_LIST_ENTRY *)OffsetInfo);
                OffsetInfo = NULL;
               /* if(!VsnapOpenAndReadDataFromLog(Buffer, BufferOffset, LogFileOffset, SizeToRead, 
                                                                            Entry->FileId, Vsnap))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapOpenAndReadDataFromLog Failed in \
                            VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
                */
                *BytesRead += SizeToRead;
                
                if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetSplitListHead, CurOffset, 
                                                    (ULONG)(Entry->Offset - CurOffset), 0, OffsetSplit->FileOffset, 0, &SubRootNodeList, NewRoot))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
                
                OffsetSplit->FileOffset += (EntryEnd - CurOffset + 1);
                if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetSplitListHead, EntryEnd+1, (ULONG)(CurEnd-EntryEnd), 
                                                0, OffsetSplit->FileOffset, 0, &SubRootNodeList, NewRoot))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
                
            }
            else if( (CurOffset < Entry->Offset) && (CurEnd >= Entry->Offset) && (CurEnd <= EntryEnd))
            {
                BufferOffset = (ULONG)(OffsetSplit->FileOffset + (Entry->Offset - CurOffset));
                LogFileOffset = Entry->FileOffset;
                SizeToRead = (ULONG) (CurEnd - Entry->Offset + 1);
                OffsetInfo->Offset = BufferOffset;
                OffsetInfo->FileOffset = LogFileOffset;
                OffsetInfo->FileId = Entry->FileId;
                OffsetInfo->Length = SizeToRead;
                InPushEntryList(Head, (SINGLE_LIST_ENTRY *)OffsetInfo);
                OffsetInfo = NULL;
                /*if(!VsnapOpenAndReadDataFromLog(Buffer, BufferOffset, LogFileOffset, SizeToRead, Entry->FileId, Vsnap))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapOpenAndReadDataFromLog Failed in \
                            VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }*/
                *BytesRead += SizeToRead;

                if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetSplitListHead, CurOffset, 
                                                    (ULONG)(Entry->Offset - CurOffset), 0, OffsetSplit->FileOffset, 0, &SubRootNodeList, NewRoot))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
            }
            else if( (CurOffset >= Entry->Offset) && (CurOffset <= EntryEnd) && (CurEnd > EntryEnd))
            {
                BufferOffset = (ULONG)OffsetSplit->FileOffset;
                LogFileOffset = Entry->FileOffset + (CurOffset - Entry->Offset);
                SizeToRead = (ULONG)(EntryEnd - CurOffset + 1);
                OffsetInfo->Offset = BufferOffset;
                OffsetInfo->FileOffset = LogFileOffset;
                OffsetInfo->FileId = Entry->FileId;
                OffsetInfo->Length = SizeToRead;
                InPushEntryList(Head, (SINGLE_LIST_ENTRY *)OffsetInfo);
                OffsetInfo = NULL;
                /*if(!VsnapOpenAndReadDataFromLog(Buffer, BufferOffset, LogFileOffset, SizeToRead, 
                                                                            Entry->FileId, Vsnap))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapOpenAndReadDataFromLog Failed in \
                                VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
            */
                *BytesRead += SizeToRead;

                OffsetSplit->FileOffset += (EntryEnd - CurOffset + 1);
                if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetSplitListHead, EntryEnd + 1, 
                                                (ULONG)(CurEnd - EntryEnd), 0, OffsetSplit->FileOffset, 0, &SubRootNodeList, NewRoot))
                {
                    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapReadDataUsingMap\n"));
                    Success = false;
                    break;
                }
            }
        }
        else
        {
            DWORD ByteRead = 0;
            char *ActualBuffer = ((char*)Buffer + OffsetSplit->FileOffset);
            FREE_MEMORY(OffsetInfo);
            *TargetRead = true;
                
			//VsnapReadFromVolume(Vsnap->ParentPtr->VolumeName, Vsnap->ParentPtr->VolumeSectorSize, ActualBuffer, 
			//	CurOffset, (ULONG)OffsetSplit->Length, BytesRead);
            *BytesRead = (ULONG)OffsetSplit->Length;
            }

           
			
			//VsnapReadFromVolume(Vsnap->ParentPtr->VolumeName, Vsnap->ParentPtr->VolumeSectorSize, ActualBuffer, 
			//	CurOffset, (ULONG)OffsetSplit->Length, BytesRead);

		
        
        
        if(subRootNode && !subRootNode->RefCount ) {
		    if((subRootNode->Node != NewRoot) && (subRootNode->Node != Vsnap->VsnapBtree->GetRootNodeFromDiskBtree()))
			    DiskBtree::FreeNode(subRootNode->Node);
            FREE_MEMORY(subRootNode);
            subRootNode = NULL;

        }
		NewRoot = NULL;
		FREE_MEMORY(OffsetSplit);
		OffsetSplit = NULL;
	}
	
	ReleaseVsnapReadLock(Vsnap->BtreeMapLock);

	if(OffsetSplit)
		FREE_MEMORY(OffsetSplit);

	while(!IsSingleListEmpty(&OffsetSplitListHead))
	{
		OffsetSplit = (VsnapOffsetSplitInfo *)InPopEntryList(&OffsetSplitListHead);
		FREE_MEMORY(OffsetSplit);
	}

    
	
	//VsnapUninitBtree(VsnapBtree);
	FREE_MEMORY(Entry);
	RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
	
	if(*BytesRead)
		return true;
	else
		return false;
}

/* This function sets the write file id for a vsnap volume. VERIFIED
 */
void  SetNextWriteFileId(VsnapInfo *Vsnap)
{
	if(!Vsnap->DataFileIdUpdated)
	{
		Vsnap->WriteFileId = Vsnap->DataFileId;
		Vsnap->DataFileId++;
		Vsnap->DataFileIdUpdated = false;
	}
	else
	{
		Vsnap->WriteFileId = (Vsnap->DataFileId + 1);
	}

	UpdateVsnapPersistentHdr(Vsnap,false);
}

/* This function sets the read file id for a vsnap volume. VERIFIED.
 */
void SetNextDataFileId(VsnapInfo *Vsnap, bool IsUpdateFirstFID)
{
    if(Vsnap->DataFileIdUpdated)
    {
        if((Vsnap->WriteFileId != -1) && (Vsnap->WriteFileId > Vsnap->DataFileId)){
            Vsnap->DataFileId = Vsnap->WriteFileId + 1;
        }
        else
        {
            Vsnap->DataFileId++;
        }
        // Setting it to False, as corresponding FileName yet to append.
        Vsnap->DataFileIdUpdated = false;

		UpdateVsnapPersistentHdr(Vsnap, IsUpdateFirstFID);
    } else if(IsUpdateFirstFID) {
        UpdateVsnapPersistentHdr(Vsnap, IsUpdateFirstFID);
    }
}

/* Given a file in the private data directory of a virtual volume, this function writes the data at the specified
 * offset. If the offset supplied is -1, the this function finds the free offset and writes the data there. If the file
 * exceeds VSNAP_MAX_DATA_FILE_SIZE, this function writes data in the new file.
 * VERIFIED.
 */
bool VsnapWrite(char *FileName, ULONGLONG & FileOffset, void *Buffer, int Length, ULONG *BytesWritten, ULONG & FileId,
				VsnapInfo *Vsnap)
{
	void		*Handle = NULL;
	ULONGLONG	NewOffset = 0;
	bool		Success = true;
	NTSTATUS    ErrStatus = STATUS_SUCCESS;

	//if(!OPEN_FILE(FileName, O_RDWR | O_CREAT | O_BUFFER, FILE_SHARE_READ | FILE_SHARE_WRITE, &Handle))
	//{
	//	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_FILE Failed in VsnapWrite\n"));
	//	return false;
	//}
	
	if(FileId == (Vsnap->WriteFileId | VSNAP_RETENTION_FILEID_MASK))
		Handle = Vsnap->WriteFileHandle;
	else
		Handle = Vsnap->FSDataHandle;

	do
	{
		if(FileOffset == -1) //Adding new data to the private file.
		{
			SEEK_FILE(Handle, 0, &NewOffset, SEEK_END);

			/*if( NewOffset >= VSNAP_MAX_DATA_FILE_SIZE )
			{
				CLOSE_FILE(Handle);

				Vsnap->WriteFileSuffix++;
				SetNextWriteFileId(Vsnap);
				FileId = Vsnap->WriteFileId | VSNAP_RETENTION_FILEID_MASK;
		
				STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%I64u_WriteData%d", Vsnap->SnapShotId, 
								Vsnap->WriteFileSuffix));
				VsnapAddFileIdToTheTable(Vsnap, (VsnapFileIdTable*)FileName);
		
				STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_WriteData%d", Vsnap->PrivateDataDirectory,
						Vsnap->SnapShotId, Vsnap->WriteFileSuffix));

				if(!OPEN_FILE(FileName, O_RDWR | O_CREAT | O_BUFFER, FILE_SHARE_READ | FILE_SHARE_WRITE, &Handle))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_FILE Failed in VsnapWrite\n"));
					return false;
				}

				NewOffset = 0;
			}*/
		}
		else
		{
			NewOffset = FileOffset;
		}

		if(!WRITE_FILE(Handle, Buffer, NewOffset, Length, (int *)BytesWritten, true, &ErrStatus))
		{   
	        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_WRITE_TO_DATA_FILE_FAILED, STATUS_SUCCESS,
                             &Vsnap->DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], GUID_LEN_WITH_BRACE,
                             Vsnap->WriteFileId,
                             Vsnap->SnapShotId,
                             ErrStatus);
            InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: WRITE_FILE Failed in VsnapWrite\n"));
			Success = false;
			break;
		}
		
		FileOffset = NewOffset;

	} while(FALSE);
	
	
	//CLOSE_FILE(Handle);
	return Success;
}

/* This function copies the data to private log for a vsnap volume.
 * VERIFIED.
 */
bool VsnapWriteToPrivateLog(void *Buffer, ULONGLONG & FileOffset, ULONG SizeToWrite, ULONG & FileId, 
							VsnapInfo *Vsnap, ULONG *BytesWritten)
{
	char *FileName = NULL;
	void *DataFileHandle;
	bool Success = true;

	if(Vsnap->WriteFileId == -1) //First write.
	{
		FileName = (PCHAR) ALLOC_MEMORY(MAX_FILENAME_LENGTH, NonPagedPool);
        RtlZeroMemory(FileName, sizeof(MAX_FILENAME_LENGTH));
		if(!FileName)
			return false;

		SetNextWriteFileId(Vsnap);
		FileId = Vsnap->WriteFileId | VSNAP_RETENTION_FILEID_MASK;

		//Add the file id to the file id mapping table.
		STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%I64u_WriteData%d", Vsnap->SnapShotId, Vsnap->WriteFileSuffix));
		//STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%I64u_WriteData", Vsnap->SnapShotId));
		VsnapAddFileIdToTheTable(Vsnap, (VsnapFileIdTable*)FileName, true);
        Vsnap->ullFileSize += MAX_RETENTION_DATA_FILENAME_LENGTH;
		
		/*STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_WriteData%d", Vsnap->PrivateDataDirectory,
						Vsnap->SnapShotId, Vsnap->WriteFileSuffix));*/
		
		FileOffset = -1; //Get the New File Offset
		Success = VsnapWrite(FileName, FileOffset, Buffer, SizeToWrite, BytesWritten, FileId, Vsnap); 
		FREE_MEMORY(FileName);
	}
	else
	{
		if((FileId == -1) || VsnapDoesFileIdPointToRetentionLog(FileId))
		{	
			FileId = Vsnap->WriteFileId | VSNAP_RETENTION_FILEID_MASK;
			FileOffset = -1;
		}
		
		/*STRING_COPY(FileName, Vsnap->PrivateDataDirectory, MAX_FILENAME_LENGTH);
		STRING_CAT(FileName, "\\", MAX_FILENAME_LENGTH);
		if(!VsnapGetFileNameFromFileId(Vsnap, FileId & ~VSNAP_RETENTION_FILEID_MASK, FileName + strlen(FileName)))
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapGetFileNameFromFileId Failed in VsnapWriteToPrivateLog\n"));
			FREE_MEMORY(FileName);
			return false;
		}*/

		Success = VsnapWrite(FileName, FileOffset, Buffer, SizeToWrite, BytesWritten, FileId, Vsnap);
	}

	return Success;
}

/* This function adds the new data key to a separate map file.
 */
//void AddKeyToTrackingMapFile(VsnapInfo *Vsnap, VsnapKeyData *Entry)
//{
//	char					*FileName = NULL;
//	DiskBtree				*VsnapBtree = NULL;
//	SINGLE_LIST_ENTRY		ListHead;
//	VsnapOffsetSplitInfo	*OffsetSplit = NULL;
//		
//	if(Vsnap->TrackingFileHandle == NULL)
//	{
//		FileName = (char *)ALLOC_MEMORY(2048);
//		if(!FileName)
//			return;
//
//		STRING_PRINTF((FileName, 2048, "%s\\%I64u_TrackingMap", Vsnap->PrivateDataDirectory, Vsnap->SnapShotId));
//		if(!OPEN_FILE(FileName, O_RDWR | O_CREAT, FILE_SHARE_READ, &Vsnap->TrackingFileHandle))
//		{
//			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_FILE Failed in AddKeyToTrackingMapFile\n"));
//			FREE_MEMORY(FileName);
//			FileName = NULL;
//			return;
//		}
//		FREE_MEMORY(FileName);
//	}
//	
//	if(!VsnapInitBtreeFromHandle(Vsnap->TrackingFileHandle, &VsnapBtree))
//			return;
//	
//	InitializeEntryList(&ListHead);
//	if(!VsnapAllocAndInsertOffsetSplitInList(&ListHead, Entry->Offset, Entry->Length, Entry->FileId, Entry->FileOffset, 0))
//	{
//		VsnapUninitBtree(VsnapBtree);
//		return;
//	}
//	
//	VsnapBtree->SetCallBack(&VsnapCallBack);
//	VsnapBtree->SetCallBackThirdParam(&ListHead);
//
//	while(true)
//	{
//		if(IsSingleListEmpty(&ListHead))
//			break;
//
//		OffsetSplit = (VsnapOffsetSplitInfo *)PopEntryList(&ListHead);
//		
//		VsnapBtree->BtreeInsert((VsnapKeyData *)&OffsetSplit->Offset, true); //Overwrite any existing entries
//				
//		FREE_MEMORY(OffsetSplit);
//	}
//
//	VsnapUninitBtree(VsnapBtree);
//	return;
//}

/* The core write API performing the bunch of creating the logs, map updation, etc.
 *	Pseudocode:
 *		1. Acquire Vsnap Remove lock
 *		2. Get btree map lock
 *		3. Search the map for offset,length pair.
 *			1. if found in the map
 *				1. if fileid points to retention log, then we have to store this data as a personal copy for 
 *				   this vsnap
 *				2. else fileid points to data file that is personal to this vsnap
 *					1. overwrite the old data with new data
 *			2. else
 *				1. Insert the entry in the map
 *				2. Add the data to the personal data file for this vsnap.
 *		4. Release btree map lock
 *		5. Release Vsnap Remove lock.
 */
int VsnapWriteDataUsingMap(void *Buffer, ULONGLONG Offset, ULONG Length, ULONG *BytesWritten, VsnapInfo *Vsnap, bool Cow)
{
	SINGLE_LIST_ENTRY		OffsetListHead,SubRootListHead;
	ULONGLONG				CurOffset = 0, EntryEnd = 0, CurEnd = 0, LogFileOffset = 0;
	ULONG					CurLength = 0, BufferOffset = 0, SizeToWrite = 0;
	VsnapOffsetSplitInfo	*pOffsetSplit = NULL;
	int						Success = true;
	enum BTREE_STATUS		Status = BTREE_SUCCESS;
    DiskBtree				*VsnapBtree = Vsnap->VsnapBtree;
	char					*datafilename = NULL;
	ULONG					TrackingId = 0;
	bool					ShouldUpdate = false;
	void                    *Node = NULL;
	
    if(!ACQUIRE_REMOVE_LOCK(&Vsnap->VsRemoveLock)){
      		return false;
    }
	
	//ENTER_CRITICAL_REGION;
	AcquireVsnapWriteLock(Vsnap->BtreeMapLock);
	//ACQUIRE_EXCLUSIVE_WRITE_LOCK(Vsnap->BtreeMapLock);
    if(!VsnapBtree) {
        NTSTATUS Vsnap_Status = VsnapInitBtreeFromHandle(Vsnap->BtreeMapFileHandle, &VsnapBtree);
	    if(!NT_SUCCESS(Vsnap_Status))
	    {
		    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapInitBtreeFromHandle Failed in \
			    			VsnapWriteDataUsingMap\n"));
		    ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
		    //RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
		    //LEAVE_CRITICAL_REGION;
		    RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
      	    return false;
	    }
    }
    Vsnap->VsnapBtree = VsnapBtree;
	
	InitializeEntryList(&OffsetListHead);
	*BytesWritten = 0;
	
	if(Cow)
		TrackingId = 0;
	else
	{
		if(Vsnap->IsTrackingEnabled)
			TrackingId = Vsnap->TrackingId;	
		else
			TrackingId = 0;
	}

	VsnapKeyData *Entry = (VsnapKeyData *)ALLOC_MEMORY(sizeof(VsnapKeyData), NonPagedPool);
	if(!Entry)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Entry: Failed in VsnapWriteDataUsingMap\n"));
		//VsnapUninitBtree(VsnapBtree);
		ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
		//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
		//LEAVE_CRITICAL_REGION;
		RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);

		return false;
	}

	if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetListHead, Offset, Length, 0, 0, TrackingId))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapWriteDataUsingMap\n"));
		//VsnapUninitBtree(VsnapBtree);
		FREE_MEMORY(Entry);
		ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
		//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
		//LEAVE_CRITICAL_REGION;
		RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);

		return false;
	}

	while(!IsSingleListEmpty(&OffsetListHead))
	{
		ULONG	NumberOfBytesWritten = 0;
		pOffsetSplit = NULL;
		pOffsetSplit = (VsnapOffsetSplitInfo *)InPopEntryList(&OffsetListHead);

		CurOffset = pOffsetSplit->Offset;
		CurLength = pOffsetSplit->Length;
		
		Status = VsnapBtree->BtreeSearch((VsnapKeyData *)&pOffsetSplit->Offset, Entry);
		if(Status == BTREE_SUCCESS)
		{
			CurEnd = CurOffset + CurLength - 1;
			EntryEnd = Entry->Offset + Entry->Length - 1;
			
			if((CurOffset >= Entry->Offset) && (CurEnd <= EntryEnd))
			{
				BufferOffset = (ULONG)pOffsetSplit->FileOffset;
				LogFileOffset = Entry->FileOffset + (CurOffset - Entry->Offset);
				SizeToWrite = CurLength;

				if(!Cow && !VsnapWriteToPrivateLog((char *)Buffer+BufferOffset, LogFileOffset, SizeToWrite, 
																			Entry->FileId, Vsnap, &NumberOfBytesWritten))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapWriteToPrivateLog Failed in \
												VsnapWriteDataUsingMap\n"));

					Success = false;

					break;
				}
				
				Entry->Offset = CurOffset;
				Entry->Length = CurLength;
				Entry->FileOffset = LogFileOffset;
				Entry->TrackingId = pOffsetSplit->TrackingId;
			}
			else if((CurOffset < Entry->Offset) && (CurEnd > EntryEnd))
			{
				BufferOffset = (ULONG)(pOffsetSplit->FileOffset + (Entry->Offset - CurOffset));
				LogFileOffset = Entry->FileOffset;
				SizeToWrite = Entry->Length;
				
				if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetListHead, CurOffset, (ULONG)(Entry->Offset - CurOffset), 
							0, pOffsetSplit->FileOffset, pOffsetSplit->TrackingId))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapWriteDataUsingMap\n"));
					Success = false;

					break;
				}

				pOffsetSplit->FileOffset += (EntryEnd - CurOffset + 1);

				if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetListHead, EntryEnd+1, (ULONG)(CurEnd-EntryEnd), 0, 
												pOffsetSplit->FileOffset, pOffsetSplit->TrackingId))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapWriteDataUsingMap\n"));

					Success = false;
					break;
				}

				if(!Cow && !VsnapWriteToPrivateLog((char *)Buffer+BufferOffset, LogFileOffset, SizeToWrite, 
																			Entry->FileId, Vsnap, &NumberOfBytesWritten))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapWriteToPrivateLog Failed in \
													VsnapWriteDataUsingMap\n"));

					Success = false;
					break;
				}				

				//Offset and Length remains as it is.
				Entry->FileOffset = LogFileOffset;
				Entry->TrackingId = pOffsetSplit->TrackingId;
				
			}
			else if( (CurOffset < Entry->Offset) && (CurEnd >= Entry->Offset) && (CurEnd <= EntryEnd))
			{
				BufferOffset = (ULONG)(pOffsetSplit->FileOffset + (Entry->Offset - CurOffset));
				LogFileOffset = Entry->FileOffset;
				SizeToWrite = (ULONG) (CurEnd - Entry->Offset + 1);
				
				if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetListHead, CurOffset, (ULONG)(Entry->Offset - CurOffset), 0,
														pOffsetSplit->FileOffset, pOffsetSplit->TrackingId))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapWriteDataUsingMap\n"));
					Success = false;

					break;
				}

				if(!Cow && !VsnapWriteToPrivateLog((char *)Buffer+BufferOffset, LogFileOffset, SizeToWrite, 
																			Entry->FileId, Vsnap, &NumberOfBytesWritten))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapWriteToPrivateLog Failed in		\
																VsnapWriteDataUsingMap\n"));

					Success = false;
					break;
				}

				Entry->Length = SizeToWrite;
				Entry->FileOffset = LogFileOffset;
				Entry->TrackingId = pOffsetSplit->TrackingId;
			}
			else if( (CurOffset >= Entry->Offset) && (CurOffset <= EntryEnd) && (CurEnd > EntryEnd))
			{
				BufferOffset = (ULONG)pOffsetSplit->FileOffset;
				LogFileOffset = Entry->FileOffset + (CurOffset - Entry->Offset);
				SizeToWrite = (ULONG)(EntryEnd - CurOffset + 1);
				
				pOffsetSplit->FileOffset += (EntryEnd - CurOffset + 1);
				if(!VsnapAllocAndInsertOffsetSplitInList(&OffsetListHead, EntryEnd + 1, (ULONG)(CurEnd - EntryEnd), 0,
														pOffsetSplit->FileOffset, pOffsetSplit->TrackingId))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ALLOC_MEMORY Failed in VsnapWriteDataUsingMap\n"));

					Success = false;
					break;
				}

				if(!Cow && !VsnapWriteToPrivateLog((char *)Buffer+BufferOffset, LogFileOffset, SizeToWrite, 
																			Entry->FileId, Vsnap, &NumberOfBytesWritten))
				{
					InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapWriteToPrivateLog Failed in \
														VsnapWriteDataUsingMap\n"));

					Success = false;
					break;
				}
				
				Entry->Offset = CurOffset;
				Entry->Length = SizeToWrite;
				Entry->FileOffset = LogFileOffset;
				Entry->TrackingId = pOffsetSplit->TrackingId;
			}

			if(Cow)
				ShouldUpdate = false;
			else
				ShouldUpdate = true;
		}
		else
		{
			Entry->Offset = CurOffset;
			Entry->Length = CurLength;
			Entry->FileId = -1;
			Entry->TrackingId = pOffsetSplit->TrackingId;

			if(!VsnapWriteToPrivateLog((char *)Buffer + pOffsetSplit->FileOffset, Entry->FileOffset, Entry->Length, 
																		Entry->FileId, Vsnap, &NumberOfBytesWritten))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapWriteToPrivateLog Failed in \
														VsnapWriteDataUsingMap\n"));

				Success = false;
				break;
			}

			ShouldUpdate = true;
		}
		
		*BytesWritten += NumberOfBytesWritten;

		/*InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Insert Keys: %I64u %I64u %lu %lu\n", Entry->Offset, 
						Entry->FileOffset, Entry->Length, Entry->FileId));*/

		if(ShouldUpdate && !VsnapWriteUpdateMap(VsnapBtree, Entry))
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapWriteUpdateMap Failed in VsnapWriteDataUsingMap\n"));
			Success = false;

			break;
		}

		FREE_MEMORY(pOffsetSplit);
		pOffsetSplit = NULL;
	
	}
	
	ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
	//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
	//LEAVE_CRITICAL_REGION;

	if(pOffsetSplit)
		FREE_MEMORY(pOffsetSplit);

	while(!IsSingleListEmpty(&OffsetListHead))
	{
		pOffsetSplit = (VsnapOffsetSplitInfo *)InPopEntryList(&OffsetListHead);
		if(pOffsetSplit)
		{
			FREE_MEMORY(pOffsetSplit);
			pOffsetSplit = NULL;
		}
	}

	//VsnapUninitBtree(VsnapBtree);
	FREE_MEMORY(Entry);
		
	RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);

	return Success;
}

/* This function allocates a new VsnapOffsetLockInfo structure, initializes it and adds to the parent's list of
 * Offset's to be in locked state. 
 * VERIFIED.
 */
bool VsnapAllocAndInsertOffsetLock(VsnapParentVolumeList *Parent, ULONGLONG VolOffset, ULONG Length, 
																VsnapOffsetLockInfo **OffsetLock )
{
	*OffsetLock = (VsnapOffsetLockInfo *)ALLOC_MEMORY(sizeof(VsnapOffsetLockInfo), NonPagedPool);
	if(NULL == *OffsetLock)
		return false;
	
	(*OffsetLock)->Offset = VolOffset;
	(*OffsetLock)->Length = Length;
	
	InitializeVsnapMutex(&((*OffsetLock)->hWaitLockMutex));
	//INIT_MUTEX(&((*OffsetLock)->hWaitLockMutex));
	
	AcquireVsnapWriteLock(Parent->OffsetListLock);
	//ACQUIRE_EXCLUSIVE_WRITE_LOCK(Parent->OffsetListLock);
	InsertTailList(&Parent->OffsetListHead,(PLIST_ENTRY)(*OffsetLock));
	ReleaseVsnapWriteLock(Parent->OffsetListLock);
	//RELEASE_RW_LOCK(Parent->OffsetListLock);
	
	return true;
}

/* This function removes the Offset Lock from the parent structure. 
 * VERIFIED.
 */
void VsnapReleaseAndRemoveOffsetLock(VsnapParentVolumeList *Parent, VsnapOffsetLockInfo *OffsetLock)
{
	AcquireVsnapWriteLock(Parent->OffsetListLock);
	//ACQUIRE_EXCLUSIVE_WRITE_LOCK(Parent->OffsetListLock);
	RemoveEntryList((PLIST_ENTRY)OffsetLock);
	ReleaseVsnapWriteLock(Parent->OffsetListLock);
	//RELEASE_RW_LOCK(Parent->OffsetListLock);
	
	FREE_MEMORY(OffsetLock);
}

/* This function is the core API to perform btree map updates to the child vsnap volumes when its
 * parent volume gets modified.
 * VERIFIED.
 */
bool VsnapVolumesUpdateMapForParentVolume(ULONGLONG VolOffset, ULONG Length, ULONGLONG FileOffset,
										  char *DataFileName, char *ParentVolumeName, ULONG Cow)
{
	VsnapParentVolumeList	*Parent = NULL;
	int						i = 0;
	char					*Buffer = NULL;
	ULONG					BytesRead = 0;
	ULONG					BytesWritten = 0;
	
    if(NULL == ParentVolumeListLock)
		return true;
	
	//ENTER_CRITICAL_REGION;
	AcquireVsnapReadLock(ParentVolumeListLock);
    //ACQUIRE_SHARED_READ_LOCK(ParentVolumeListLock);

	Parent = VsnapFindParentFromVolumeName(ParentVolumeName);
	if(NULL != Parent)
	{
		if(Cow)
		{
			Buffer = (char *)ALLOC_MEMORY(Length, NonPagedPool);
			if(!Buffer)
			{
				ReleaseVsnapReadLock(ParentVolumeListLock);
				//RELEASE_RW_LOCK(ParentVolumeListLock);
				//LEAVE_CRITICAL_REGION;
				Buffer = NULL;
				return false;
			}
			
			VsnapReadFromVolume(ParentVolumeName, Parent->VolumeSectorSize, Buffer, VolOffset, Length, &BytesRead);
			if(BytesRead != Length)
			{
				FREE_MEMORY(Buffer);
				ReleaseVsnapReadLock(ParentVolumeListLock);
				//RELEASE_RW_LOCK(ParentVolumeListLock);
				//LEAVE_CRITICAL_REGION;
				Buffer = NULL;
				return false;
			}
		}

		VsnapOffsetLockInfo *OffsetLock = NULL;

		if(!VsnapAllocAndInsertOffsetLock(Parent, VolOffset, Length, &OffsetLock))
		{
			ReleaseVsnapReadLock(ParentVolumeListLock);
			//RELEASE_RW_LOCK(ParentVolumeListLock);
			//LEAVE_CRITICAL_REGION;
			return false;
		}
		
		AcquireVsnapMutex(&OffsetLock->hWaitLockMutex);
		//ACQUIRE_MUTEX(&OffsetLock->hWaitLockMutex);
		
		VsnapInfo			*Vsnap = NULL;
		VsnapFileIdTable	FileTable;

		Vsnap = (VsnapInfo *)Parent->VsnapHead.Flink;
		
		while((Vsnap != NULL) && (Vsnap != (VsnapInfo *)&Parent->VsnapHead))
		{
			if(!Cow)
			{
				if(!ACQUIRE_REMOVE_LOCK(&Vsnap->VsRemoveLock))
				{
					Vsnap = (VsnapInfo*)Vsnap->LinkNext.Flink;
					continue; 
				}

				AcquireVsnapWriteLock(Vsnap->BtreeMapLock);
				//ACQUIRE_EXCLUSIVE_WRITE_LOCK(Vsnap->BtreeMapLock);

				if(Vsnap->LastDataFileName == NULL)
				{
					Vsnap->LastDataFileName = (char *) ALLOC_MEMORY(MAX_RETENTION_DATA_FILENAME_LENGTH, NonPagedPool);
					if(!Vsnap->LastDataFileName)
					{
						InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Memory alloc failed in VsnapVolumesUpdateMapForParentVolume\n"));
						ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
						//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
						RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
						continue;
					}
					STRING_COPY(Vsnap->LastDataFileName, DataFileName, MAX_RETENTION_DATA_FILENAME_LENGTH);
				}
				else if((0 != _strnicmp(Vsnap->LastDataFileName, DataFileName, 
										MAX_RETENTION_DATA_FILENAME_LENGTH)))
				{
					STRING_COPY(Vsnap->LastDataFileName, DataFileName, MAX_RETENTION_DATA_FILENAME_LENGTH);
					SetNextDataFileId(Vsnap,false);
				}
				
				//VsnapUpdateMap(Vsnap, VolOffset, Length, FileOffset, DataFileName);

				ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
				//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
				RELEASE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
			}
			else
				VsnapWriteDataUsingMap(Buffer, VolOffset, Length, &BytesWritten, Vsnap, true);
						
			Vsnap = (VsnapInfo*)Vsnap->LinkNext.Flink;
		}

		ReleaseVsnapMutex(&OffsetLock->hWaitLockMutex);
		//RELEASE_MUTEX(&OffsetLock->hWaitLockMutex);
		
		VsnapReleaseAndRemoveOffsetLock(Parent, OffsetLock);
		OffsetLock = NULL;
	}

	ReleaseVsnapReadLock(ParentVolumeListLock);
	//RELEASE_RW_LOCK(ParentVolumeListLock);
	//LEAVE_CRITICAL_REGION;
	
	if(Buffer)
		FREE_MEMORY(Buffer);

	return true;
}

/* This is the vsnap volume read API exported to the invirvol driver.
 * VERIFIED.
 */

int VsnapVolumeRead(void *Buffer, ULONGLONG Offset, ULONG BufferLength, ULONG *BytesRead, PVOID VolumeContext, PSINGLE_LIST_ENTRY  Head, bool *TargetRead)
{
	VsnapInfo *Vsnap = (VsnapInfo *) VolumeContext;
    PVOID      pReadBuffer = NULL;
    int        RetVal = 0;

    RetVal = VsnapReadDataUsingMap(Buffer, Offset, BufferLength, BytesRead, Vsnap, Head, TargetRead);
    
    return RetVal;


}

/* This is the vsnap volume write API exported to the invirvol driver.
 * VERIFIED.
 */
int VsnapVolumeWrite(void *Buffer, ULONGLONG Offset, ULONG BufferLength, ULONG *BytesWritten, PVOID VolumeContext)
{
	VsnapInfo *Vsnap = (VsnapInfo *) VolumeContext;

	if(Vsnap->AccessType == VSNAP_READ_ONLY)
	{
		*BytesWritten = BufferLength;
		return 1;
	}

    int returnValue = VsnapWriteDataUsingMap(Buffer, Offset, BufferLength, BytesWritten, Vsnap, false);

	return returnValue;
}

/* This function is called by the driver during a new mount for Virtual Recovery or Virtual Point-In-Time Snapshot.
 * Here, we initialize the vsnap structure, create a target volume structure if one doesn't exist already. During init,
 * we cache the handles of btree map file, file id to file name conversion file, FS data file for faster access. 
 * VERIFIED.
 */
NTSTATUS VsnapMount(void *Buffer, int BufferLength,	LONGLONG *VolumeSize, PDEVICE_EXTENSION DevExtension, MultiPartSparseFileInfo *SparseFileInfo)
{
	char		*FileName = NULL;
	VsnapInfo	*Vsnap = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    PFILE_HANDLE_CACHE FileHandleCache = NULL;
    PFILE_HANDLE_CACHE_ENTRY CacheEntries = NULL;
    PFILE_NAME_CACHE   FileNameCache = NULL;
    bool Eventlog = true;
	size_t dwSize;

	VsnapMountInfo *MountData = (VsnapMountInfo *)Buffer;

	if(STATUS_INVALID_PARAMETER == ValidateMountInfo(MountData))
		return STATUS_INVALID_PARAMETER;

	if(sizeof(VsnapMountInfo) > BufferLength )
		return STATUS_OBJECT_TYPE_MISMATCH;

	Vsnap = (VsnapInfo *)ALLOC_MEMORY_WITH_TAG(sizeof(VsnapInfo), NonPagedPool, VVTAG_VSNAP_STRUCT);
	if(NULL == Vsnap)
		return STATUS_NO_MEMORY;

    RtlZeroMemory(Vsnap, sizeof(VsnapInfo));

    Vsnap->IsSparseRetentionEnabled = MountData->IsSparseRetentionEnabled;
    
    if(Vsnap->IsSparseRetentionEnabled) {

        if(!InitCache(Vsnap)) {
            FREE_MEMORY(Vsnap);
		    return STATUS_NO_MEMORY;
        }
    }
    // Allocate FILE_HANDLE_CACHE first
    Vsnap->FileHandleCache = NULL;
    FileHandleCache = (PFILE_HANDLE_CACHE)ALLOC_MEMORY(sizeof(FILE_HANDLE_CACHE), PagedPool);
    
    // Only on successful memory allocation for FileHandleCache and CacheEntries.
    // We set file handle caching on the VSNAP
    if(FileHandleCache)
    {
        // Initialise FILE_HANDLE_CACHE
        KeWaitForSingleObject(&DriverContext.FileHandleCacheMutex, Executive, KernelMode, FALSE, NULL);
        RtlZeroMemory(FileHandleCache, sizeof(FILE_HANDLE_CACHE));
        FileHandleCache->ulMaxRetFileHandles = (DriverContext.ulMemoryForFileHandleCache / sizeof(FILE_HANDLE_CACHE_ENTRY));
       	FileHandleCache->FileHandleCacheLock = (VsnapRWLock *)ALLOC_MEMORY(sizeof(VsnapRWLock), NonPagedPool);
        if( NULL == FileHandleCache->FileHandleCacheLock)
	    {
            FREE_MEMORY(Vsnap->FileHandleCache);
        } else {
            InitializeVsnapRWLock(FileHandleCache->FileHandleCacheLock);
            // Allocate FILE_HANDLE_CACHE_ENTRY
            CacheEntries = (PFILE_HANDLE_CACHE_ENTRY)ALLOC_MEMORY(DriverContext.ulMemoryForFileHandleCache, PagedPool);

            if(!CacheEntries){
                if(FileHandleCache){
                    FREE_MEMORY(FileHandleCache->FileHandleCacheLock);
                    FREE_MEMORY(FileHandleCache);
                }
            }else{          
				RtlZeroMemory(CacheEntries, sizeof(FILE_HANDLE_CACHE_ENTRY)*FileHandleCache->ulMaxRetFileHandles);
                FileHandleCache->CacheEntries = CacheEntries;
                Vsnap->FileHandleCache = FileHandleCache;
            }
        }
        KeReleaseMutex(&DriverContext.FileHandleCacheMutex, FALSE);
    }

	Vsnap->BtreeMapFileHandle = NULL;
	Vsnap->FileIdTableHandle  = NULL;
	Vsnap->LastDataFileName   = NULL;
	Vsnap->PrivateDataDirectory = NULL;
	Vsnap->VolumeName = NULL;
	Vsnap->WriteFileHandle = NULL;
	Vsnap->FSDataHandle = NULL;
    Vsnap->IOPending = 0;
    Vsnap->IOsInFlight = 0;
    Vsnap->IOWriteInProgress = false;
    Vsnap->DevExtension = DevExtension;
    ExInitializeFastMutex(&Vsnap->hMutex);
    KeInitializeEvent(&Vsnap->KillThreadEvent, SynchronizationEvent, FALSE);
	
	Vsnap->BtreeMapLock = (VsnapRWLock *)ALLOC_MEMORY(sizeof(VsnapRWLock), NonPagedPool);
	if( NULL == Vsnap->BtreeMapLock)
	{
        FREE_MEMORY(Vsnap->FileHandleCache->CacheEntries);
        FREE_MEMORY(Vsnap->FileHandleCache->FileHandleCacheLock);
        FREE_MEMORY(Vsnap->FileHandleCache);
		FREE_MEMORY(Vsnap);
		return STATUS_NO_MEMORY;
	}

	do
	{
		FileName = (char *)ALLOC_MEMORY(MAX_FILENAME_LENGTH, PagedPool);
		if(!FileName)
		{
		    Status = STATUS_NO_MEMORY;
			break;
		}
		
		if(MountData->IsMapFileInRetentionDirectory)
		{
			if(MountData->RetentionDirectory[0] == '\0')
			{
				Status =  STATUS_BUFFER_ALL_ZEROS;
				break;
			}

			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_VsnapMap", MountData->RetentionDirectory, 
							MountData->SnapShotId));
			if(!OPEN_FILE(FileName, O_RDWR | O_BUFFER, FILE_SHARE_READ, &Vsnap->BtreeMapFileHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN file of map failed in VsnapMount\n"));
				break;
			}

			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_FileIdTable", MountData->RetentionDirectory, 
						MountData->SnapShotId));
			if(!OPEN_FILE(FileName, O_RDWR | O_BUFFER, FILE_SHARE_READ, &Vsnap->FileIdTableHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN file of FileIdTable failed in VsnapMount\n"));
				break;
			}

			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_FSData", MountData->RetentionDirectory, 
						MountData->SnapShotId));
			if(!OPEN_FILE(FileName, O_RDWR | O_BUFFER, FILE_SHARE_READ, &Vsnap->FSDataHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN file for FSData\n"));
				break;
			}
		}
		else
		{
			if(MountData->PrivateDataDirectory[0] == '\0')
			{
				Status =  STATUS_BUFFER_ALL_ZEROS;
				break;
			}

			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_VsnapMap", MountData->PrivateDataDirectory, 
							MountData->SnapShotId));
			if(!OPEN_FILE(FileName, O_RDWR | O_BUFFER, FILE_SHARE_READ , &Vsnap->BtreeMapFileHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN file of map failed in VsnapMount\n"));
				break;
			}

			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_FileIdTable", MountData->PrivateDataDirectory, 
						MountData->SnapShotId));
			if(!OPEN_FILE(FileName, O_RDWR | O_BUFFER, FILE_SHARE_READ , &Vsnap->FileIdTableHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN file of FileIdTable failed in VsnapMount\n"));
				break;
			}

			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_FSData", MountData->PrivateDataDirectory, 
						MountData->SnapShotId));
			if(!OPEN_FILE(FileName, O_RDWR | O_BUFFER, FILE_SHARE_READ , &Vsnap->FSDataHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN file for FSData\n"));
				break;
			}
		}

        Status = VsnapInitFileIds(Vsnap);
		if(!NT_SUCCESS(Status))
        {
            break;
		}

        if(Vsnap->IsSparseRetentionEnabled && !ReadAndFillCache(Vsnap))
        {
            Status = STATUS_INTERNAL_ERROR;
            break;
		}

		Vsnap->RootNodeOffset = MountData->RootNodeOffset;
		Vsnap->SnapShotId = MountData->SnapShotId;
		//Vsnap->RecoveryTime = MountData->RecoveryTime;
		Vsnap->IsMapFileInRetentionDirectory = MountData->IsMapFileInRetentionDirectory;
		Vsnap->AccessType = MountData->AccessType;
		
		//Create the data file here.
		if(MountData->IsMapFileInRetentionDirectory)
		{
			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_WriteData%d", MountData->RetentionDirectory, 
				MountData->SnapShotId, Vsnap->WriteFileSuffix));

			if(!OPEN_FILE(FileName, O_RDWR | O_CREAT | O_BUFFER, FILE_SHARE_READ , &Vsnap->WriteFileHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_FILE Failed to create data file\n"));
				break;
			}

		}
		else
		{
			STRING_PRINTF((FileName, MAX_FILENAME_LENGTH, "%s\\%I64u_WriteData%d", MountData->PrivateDataDirectory, 
				MountData->SnapShotId, Vsnap->WriteFileSuffix));

			if(!OPEN_FILE(FileName, O_RDWR | O_CREAT | O_BUFFER, FILE_SHARE_READ , &Vsnap->WriteFileHandle, true, &Status, Eventlog))
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: OPEN_FILE Failed to create data file\n"));
				break;
			}

		}

		FREE_MEMORY(FileName);
        FileName = NULL;
		
		if(MountData->IsTrackingEnabled)
		{
			Vsnap->IsTrackingEnabled = true;
			Vsnap->TrackingId = 1;
		}
		else
		{
			Vsnap->IsTrackingEnabled = false;
			Vsnap->TrackingId = 0;
		}
		
		if(MountData->VirtualVolumeName[0] != '\0')
		{
		    if (!NT_SUCCESS(RtlStringCbLengthA(MountData->VirtualVolumeName, MAX_NAME_LENGTH, &dwSize))) {
	 		  	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("VsnapMount: RtlStringCbLengthA failed for VirtualVolumeName %s\n", MountData->VirtualVolumeName));
				Status = STATUS_INVALID_PARAMETER;
	   			break;
			}
			Vsnap->VolumeName = (PCHAR)ALLOC_MEMORY(dwSize + 1, NonPagedPool);
			if(NULL == Vsnap->VolumeName)
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapMount\n"));
				Status = STATUS_NO_MEMORY;
				break;
			}
			STRING_COPY(Vsnap->VolumeName, MountData->VirtualVolumeName, dwSize + 1);
		}

		if(MountData->PrivateDataDirectory[0] != '\0')
		{
		    if (!NT_SUCCESS(RtlStringCbLengthA(MountData->PrivateDataDirectory, MAX_NAME_LENGTH, &dwSize))) {
	 		  	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("VsnapMount: RtlStringCbLengthA failed for PrivateDataDirectory %s\n", MountData->PrivateDataDirectory));
				Status = STATUS_INVALID_PARAMETER;
	   			break;
			}
			Vsnap->PrivateDataDirectory = (PCHAR)ALLOC_MEMORY(dwSize + 1, NonPagedPool);
			if(NULL == Vsnap->PrivateDataDirectory)
			{
				InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapMount\n"));
				Status = STATUS_NO_MEMORY;
				break;
			}
			
			STRING_COPY(Vsnap->PrivateDataDirectory, MountData->PrivateDataDirectory, dwSize + 1);
		}

		InitializeVsnapRWLock(Vsnap->BtreeMapLock);
		//INIT_RW_LOCK(Vsnap->BtreeMapLock);
		INIT_REMOVE_LOCK(&Vsnap->VsRemoveLock);
		*VolumeSize = MountData->VolumeSize;
		
        if (SparseFileInfo && SparseFileInfo->IsMultiPartSparseFile) {
            SparseFileInfo->SplitFileCount = (ULONG)(MountData->VolumeSize / SparseFileInfo->ChunkSize);
            if ((MountData->VolumeSize % SparseFileInfo->ChunkSize))
                SparseFileInfo->SplitFileCount++;
        }
		/* Init done, add the vsnap structure to the global list. Create new target structure if required.
		 */
		if(!VsnapAddVolumeToList(MountData->VolumeName, Vsnap, MountData->RetentionDirectory, SparseFileInfo))
		{
            Status = STATUS_INTERNAL_ERROR;
			break;
		}
		else
		{
			if(Vsnap->ParentPtr->VolumeSectorSize == 0)
				Vsnap->ParentPtr->VolumeSectorSize = MountData->VolumeSectorSize?MountData->VolumeSectorSize:		
													 DEFAULT_VOLUME_SECTOR_SIZE;

			DevExtension->ulSectorSize = Vsnap->ParentPtr->VolumeSectorSize; 
			//*VolumeContext = Vsnap;
            DevExtension->VolumeContext = Vsnap;
		}


	} while (FALSE);

	if(!NT_SUCCESS(Status))
		goto cleanup_exit;
	
    return Status;

cleanup_exit:

	if(Vsnap->BtreeMapFileHandle)
		CLOSE_FILE(Vsnap->BtreeMapFileHandle);
	if(Vsnap->FileIdTableHandle)
		CLOSE_FILE(Vsnap->FileIdTableHandle);
	if(Vsnap->WriteFileHandle)
		CLOSE_FILE(Vsnap->WriteFileHandle);
	if(Vsnap->FSDataHandle)
		CLOSE_FILE(Vsnap->FSDataHandle);
    if(Vsnap->FileNameCache) {
        if(Vsnap->FileNameCache->TSHash)
            FREE_MEMORY(Vsnap->FileNameCache->TSHash);
        if(Vsnap->FileNameCache->FileId)
            FREE_MEMORY(Vsnap->FileNameCache->FileId);
    
        FREE_MEMORY(Vsnap->FileNameCache);
    }

    if(Vsnap->FileHandleCache) {
        if(Vsnap->FileHandleCache->FileHandleCacheLock)
            FREE_MEMORY(Vsnap->FileHandleCache->FileHandleCacheLock);

        if(Vsnap->FileHandleCache->CacheEntries)
            FREE_MEMORY(Vsnap->FileHandleCache->CacheEntries);

        FREE_MEMORY(Vsnap->FileHandleCache);
    }
	if(FileName)
		FREE_MEMORY(FileName);
	if(Vsnap->PrivateDataDirectory)
		FREE_MEMORY(Vsnap->PrivateDataDirectory);
	if(Vsnap->VolumeName)
		FREE_MEMORY(Vsnap->VolumeName);
	if(Vsnap->BtreeMapLock)
		FREE_MEMORY(Vsnap->BtreeMapLock);

	FREE_MEMORY(Vsnap);

	return Status;
}

void CleanupFileHandleCache(VsnapInfo *Vsnap)
{
    PFILE_HANDLE_CACHE FileHandleCache = Vsnap->FileHandleCache;
    ULONG CacheEntryCounter;

    if(FileHandleCache)
    {
        ASSERT(FileHandleCache->ulNoOfEntriesInCache <= FileHandleCache->ulMaxRetFileHandles);
                
        for(CacheEntryCounter = 0; CacheEntryCounter < FileHandleCache->ulNoOfEntriesInCache; CacheEntryCounter++ )
             CLOSE_FILE(FileHandleCache->CacheEntries[CacheEntryCounter].pWinHdlTag );

        FREE_MEMORY(FileHandleCache->FileHandleCacheLock);
        FREE_MEMORY(Vsnap->FileHandleCache->CacheEntries);
        FREE_MEMORY(Vsnap->FileHandleCache);            
    }
}
/* This function is called to cleanup Vsnap data structure associated with the virtual volume.
 * VERIFIED.
 */
bool VsnapUnmount(PVOID VolumeContext)
{
	ULONG StringOffset = 0;
	VsnapInfo *Vsnap = (VsnapInfo *)(VolumeContext);

	//ENTER_CRITICAL_REGION;
	AcquireVsnapWriteLock(ParentVolumeListLock);
	//ACQUIRE_EXCLUSIVE_WRITE_LOCK(ParentVolumeListLock);
	ACQUIRE_REMOVE_LOCK(&Vsnap->VsRemoveLock);
	RELEASE_REMOVE_WAIT_LOCK(&Vsnap->VsRemoveLock);

	CLOSE_FILE(Vsnap->BtreeMapFileHandle);
	CLOSE_FILE(Vsnap->FileIdTableHandle);

	if(Vsnap->WriteFileHandle)
		CLOSE_FILE(Vsnap->WriteFileHandle);

	if(Vsnap->FSDataHandle)
		CLOSE_FILE(Vsnap->FSDataHandle);
	
    if(Vsnap->FileHandleCache)
        CleanupFileHandleCache(Vsnap);

	RemoveEntryList(&Vsnap->LinkNext);
	if(IsListEmpty(&(Vsnap->ParentPtr->VsnapHead)))
	{
        if(Vsnap->ParentPtr->IsFile && Vsnap->ParentPtr->SparseFileHandle)
            CLOSE_FILE(Vsnap->ParentPtr->SparseFileHandle);
        else if(Vsnap->ParentPtr->IsMultiPartSparseFile && Vsnap->ParentPtr->MultiPartSparseFileHandle)
            CloseAndFreeMultiSparseFileHandle(Vsnap->ParentPtr->SparseFileCount, Vsnap->ParentPtr->MultiPartSparseFileHandle);

		RemoveEntryList(&(Vsnap->ParentPtr->LinkNext));
		if(Vsnap->ParentPtr->VolumeName)
			FREE_MEMORY(Vsnap->ParentPtr->VolumeName);
		if(Vsnap->ParentPtr->RetentionDirectory)
			FREE_MEMORY(Vsnap->ParentPtr->RetentionDirectory);

		//DELETE_RW_LOCK(Vsnap->ParentPtr->OffsetListLock);
		FREE_MEMORY(Vsnap->ParentPtr->OffsetListLock);
        FREE_MEMORY(Vsnap->ParentPtr->TargetRWLock);
		FREE_MEMORY(Vsnap->ParentPtr);
		ReleaseVsnapWriteLock(ParentVolumeListLock);
		//RELEASE_RW_LOCK(ParentVolumeListLock);
	}
	else
	{
		ReleaseVsnapWriteLock(ParentVolumeListLock);
		//RELEASE_RW_LOCK(ParentVolumeListLock);
	}
	
	//Kill update thread per vsnap
    if(Vsnap->UpdateThread) {
        KeSetEvent(&Vsnap->KillThreadEvent, EVENT_INCREMENT, FALSE);
        KeWaitForSingleObject(Vsnap->UpdateThread, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(Vsnap->UpdateThread);
        Vsnap->UpdateThread = NULL;
    }
        

	//LEAVE_CRITICAL_REGION;
	if(Vsnap->VolumeName)
		FREE_MEMORY(Vsnap->VolumeName);
	if(Vsnap->LastDataFileName)
		FREE_MEMORY(Vsnap->LastDataFileName);
	if(Vsnap->PrivateDataDirectory)
		FREE_MEMORY(Vsnap->PrivateDataDirectory);
    if(Vsnap->VsnapBtree) {
        Vsnap->VsnapBtree->DestroyCache();
        VsnapUninitBtree(Vsnap->VsnapBtree);
    }
    if(Vsnap->FileNameCache) {
        if(Vsnap->FileNameCache->TSHash)
            FREE_MEMORY(Vsnap->FileNameCache->TSHash);

        if(Vsnap->FileNameCache->FileId)
            FREE_MEMORY(Vsnap->FileNameCache->FileId);

        FREE_MEMORY(Vsnap->FileNameCache);
    }



	//DELETE_RW_LOCK(Vsnap->BtreeMapLock);
    FREE_MEMORY(Vsnap->BtreeMapLock);
	FREE_MEMORY(Vsnap);

	return true;
}

/* This function called from UPDATE ioctl path to update the child vsnap maps belonging to a particular source volume.
 * VERIFIED.
 */
bool VsnapVolumesUpdateMaps(PVOID InputBuffer,PIRP Irp, bool *IsIOComplete)
{
    bool returnValue = true;
	UPDATE_VSNAP_VOLUME_INPUT *UpdateInfo = (UPDATE_VSNAP_VOLUME_INPUT *)InputBuffer;
	
	if(STATUS_INVALID_PARAMETER ==  ValidateUpdateInfo(UpdateInfo))
		return false;
    
    returnValue = VsnapQueueUpdateTask(UpdateInfo, Irp, IsIOComplete);
	
 //   returnValue = VsnapVolumesUpdateMapForParentVolume(UpdateInfo->VolOffset, UpdateInfo->Length, UpdateInfo->FileOffset,
//		UpdateInfo->DataFileName, UpdateInfo->ParentVolumeName, UpdateInfo->Cow);

    
	
	return returnValue;
}

/* This function returns length of bytes in Vsnap Volume Context.
 * VERIFIED.
 */
ULONG VsnapGetVolumeInformationLength(PVOID VolumeContext)
{
	return sizeof(VsnapContextInfo);
}

/* This function retrieves the Vsnap Volume information.
 * VERIFIED.
 */
VOID VsnapGetVolumeInformation(PVOID VolumeContext, PVOID Buffer, ULONG Size)
{
	VsnapInfo			*Vsnap = (VsnapInfo *) VolumeContext;
	VsnapContextInfo	*VsnapContext = (VsnapContextInfo *)Buffer;

	if(Size < sizeof(VsnapContextInfo))
		return;

	VsnapContext->RecoveryTime = Vsnap->RecoverySystemTime.QuadPart;
	VsnapContext->SnapShotId = Vsnap->SnapShotId;
	VsnapContext->AccessType = Vsnap->AccessType;
	VsnapContext->IsTrackingEnabled = Vsnap->IsTrackingEnabled;
	STRING_COPY(VsnapContext->ParentVolumeName, Vsnap->ParentPtr->VolumeName, MAX_FILENAME_LENGTH);
	STRING_COPY(VsnapContext->PrivateDataDirectory, Vsnap->PrivateDataDirectory, MAX_FILENAME_LENGTH);
	STRING_COPY(VsnapContext->RetentionDirectory, Vsnap->ParentPtr->RetentionDirectory, MAX_FILENAME_LENGTH);
	STRING_COPY(VsnapContext->VolumeName, Vsnap->VolumeName, MAX_FILENAME_LENGTH);

	return;
}

/* This function is called by the invirvol driver during its init to initialize Vsnap's
 * global data structures.
 * VERIFIED
 */
int VsnapInit()
{
	ParentVolumeListHead = (PLIST_ENTRY)ALLOC_MEMORY(sizeof(LIST_ENTRY), NonPagedPool);
	if(NULL == ParentVolumeListHead)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapInit\n"));
		return false;
	}

	ParentVolumeListLock = (VsnapRWLock *)ALLOC_MEMORY(sizeof(VsnapRWLock), NonPagedPool);
	if(NULL == ParentVolumeListLock)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapInit\n"));
		FREE_MEMORY(ParentVolumeListHead);
		ParentVolumeListHead = NULL;
		return false;
	}

	InitializeVsnapRWLock(ParentVolumeListLock);
	//INIT_RW_LOCK(ParentVolumeListLock);
	InitializeListHead(ParentVolumeListHead);
	
	return true;
}

/* This function is called during driver's exit.
 * VERIFIED.
 */
int VsnapExit()
{
	//DELETE_RW_LOCK(ParentVolumeListLock);

	FREE_MEMORY(ParentVolumeListHead);
	FREE_MEMORY(ParentVolumeListLock);
	
	ParentVolumeListHead = NULL;
	ParentVolumeListLock = NULL;

	return true;
}

bool VsnapSetOrResetTracking(PVOID Context, bool Enable, int * TrackingId)
{
	VsnapInfo			*Vsnap = (VsnapInfo *) Context;
	
	//ENTER_CRITICAL_REGION;
	AcquireVsnapWriteLock(Vsnap->BtreeMapLock);
	//ACQUIRE_EXCLUSIVE_WRITE_LOCK(Vsnap->BtreeMapLock);

	if(Enable)
	{
		Vsnap->IsTrackingEnabled = true;
		Vsnap->TrackingId = 1;
	}
	else
	{
		Vsnap->IsTrackingEnabled = false;
		Vsnap->TrackingId = 0;
	}
	
	ReleaseVsnapWriteLock(Vsnap->BtreeMapLock);
	//RELEASE_RW_LOCK(Vsnap->BtreeMapLock);
	//LEAVE_CRITICAL_REGION;
	return true;
}

NTSTATUS VsnapInitFileIds(VsnapInfo *Vsnap)
{
    DiskBtree					*VsnapBtree = Vsnap->VsnapBtree;
	char						*Hdr = NULL;
	VsnapPersistentHeaderInfo	*VsnapHdrInfo;
	char						*FileName = NULL;
    LARGE_INTEGER                FileSize;
    NTSTATUS                     Status = STATUS_SUCCESS;
    ULONG                       EntriesCount = 0;
    TIME_FIELDS                 TimeFields;
    ULONGLONG                   ullVsnapTS = 0;

    if(Vsnap->IsSparseRetentionEnabled) {
        Status = QueryFileSize(Vsnap->FileIdTableHandle,&FileSize);
        if (!NT_SUCCESS(Status))
            return Status;
    
        Vsnap->ullFileSize = FileSize.QuadPart ;
        EntriesCount = (ULONG)(Vsnap->ullFileSize / MAX_RETENTION_DATA_FILENAME_LENGTH);    
    
    }
	Hdr = (char *)ALLOC_MEMORY(BTREE_HEADER_SIZE, PagedPool);
	if(!Hdr)
		return STATUS_NO_MEMORY;

	FileName = (char *)ALLOC_MEMORY(MAX_RETENTION_DATA_FILENAME_LENGTH, PagedPool);
	if(!FileName)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapInitFileIds\n"));
		FREE_MEMORY(Hdr);
		return STATUS_NO_MEMORY;
	}
    RtlZeroMemory(FileName, sizeof(MAX_RETENTION_DATA_FILENAME_LENGTH));
	VsnapHdrInfo = (VsnapPersistentHeaderInfo *) (Hdr + BTREE_HEADER_OFFSET_TO_USE);

	if( !VsnapBtree ) {
        Status = VsnapInitBtreeFromHandle(Vsnap->BtreeMapFileHandle, &VsnapBtree);
	    if(!NT_SUCCESS(Status))        
		{
			InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapInitBtreeFromHandle failed in VsnapInitFileIds\n"));
			FREE_MEMORY(Hdr);
			FREE_MEMORY(FileName);
			return Status;
		}
	}

    Vsnap->VsnapBtree = VsnapBtree;

	VsnapBtree->GetHeader(Hdr);
	
	Vsnap->DataFileId = VsnapHdrInfo->DataFileId;
    Vsnap->WriteFileId = VsnapHdrInfo->WriteFileId;
	Vsnap->WriteFileSuffix = VsnapHdrInfo->WriteFileSuffix;
	Vsnap->RecoverySystemTime.QuadPart = VsnapHdrInfo->VsnapTime; //Did for PIT VSNAP

    if(!Vsnap->IsSparseRetentionEnabled) {
	    if(!VsnapGetFileNameFromFileId(Vsnap, Vsnap->DataFileId, FileName))
		    Vsnap->DataFileIdUpdated = false;
	    else
	    {
		    if(Vsnap->LastDataFileName == NULL)
		    {
			    Vsnap->LastDataFileName = (char *) ALLOC_MEMORY(MAX_RETENTION_DATA_FILENAME_LENGTH, NonPagedPool);
			    if(!Vsnap->LastDataFileName)
			    {
				    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in VsnapInitFileIds\n"));
				    FREE_MEMORY(Hdr);
				    FREE_MEMORY(FileName);
                    VsnapUninitBtree(VsnapBtree);
                    return STATUS_NO_MEMORY;
			    }
                RtlZeroMemory(Vsnap->LastDataFileName, sizeof(MAX_RETENTION_DATA_FILENAME_LENGTH));

		    }
		    STRING_COPY(Vsnap->LastDataFileName, FileName, MAX_RETENTION_DATA_FILENAME_LENGTH);		
		    Vsnap->DataFileIdUpdated = true;
	    }
    } else {
        Vsnap->FileNameCache->ulBackwardFileId = VsnapHdrInfo->ulBackwardFileId;
        Vsnap->FileNameCache->ulForwardFileId = VsnapHdrInfo->ulForwardFileId;
        RtlTimeToTimeFields(&Vsnap->RecoverySystemTime, &TimeFields);
        ullVsnapTS = (((TimeFields.Year - BASE_YEAR) * 100 + TimeFields.Month) * 100 + TimeFields.Day) * 100 + TimeFields.Hour;
        Vsnap->FileNameCache->ullCreationTS = ullVsnapTS;
         Vsnap->DataFileIdUpdated = true;
        if((EntriesCount + 1)== Vsnap->DataFileId) {
            Vsnap->DataFileId--;
        }
    }
	
    Vsnap->VsnapBtree = VsnapBtree;
	FREE_MEMORY(Hdr);
	FREE_MEMORY(FileName);
	//VsnapUninitBtree(VsnapBtree);

	return Status;
}

void UpdateVsnapPersistentHdr(VsnapInfo *Vsnap, bool IsUpdateFirstFId)
{
	char		*Hdr = NULL;
    DiskBtree	*VsnapBtree = Vsnap->VsnapBtree;
    if(!VsnapBtree) {
        NTSTATUS Vsnap_Status = VsnapInitBtreeFromHandle(Vsnap->BtreeMapFileHandle, &VsnapBtree);
        if(!NT_SUCCESS(Vsnap_Status))         
	    {
		    InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: VsnapInitBtreeFromHandle failed in UpdateVsnapPersistentHdr\n"));
		    return;
	    }
    }
    Vsnap->VsnapBtree = VsnapBtree;
	
	Hdr = (char *)ALLOC_MEMORY(BTREE_HEADER_SIZE, NonPagedPool);
	if(!Hdr)
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: Alloc memory failed in UpdateVsnapPersistentHdr\n"));
		//VsnapUninitBtree(VsnapBtree);
		return;
	}
		
	VsnapPersistentHeaderInfo *temp = (VsnapPersistentHeaderInfo *) (Hdr + BTREE_HEADER_OFFSET_TO_USE);

	if(!VsnapBtree->GetHeader(Hdr))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: GetHeader failed in UpdateVsnapPersistentHdr\n"));
		//VsnapUninitBtree(VsnapBtree);
		FREE_MEMORY(Hdr);
		return;
	}
	
	temp->DataFileId = Vsnap->DataFileId;
	temp->WriteFileId = Vsnap->WriteFileId;
	temp->WriteFileSuffix = Vsnap->WriteFileSuffix;

    if(IsUpdateFirstFId) {
        temp->ulBackwardFileId = Vsnap->FileNameCache->ulBackwardFileId; 
        temp->ulForwardFileId = Vsnap->FileNameCache->ulForwardFileId;
    }

	if(!VsnapBtree->WriteHeader(Hdr))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: WriteHeader failed in UpdateVsnapPersistentHdr\n"));
		//VsnapUninitBtree(VsnapBtree);
		FREE_MEMORY(Hdr);
		return;
	}

	FREE_MEMORY(Hdr);
	//VsnapUninitBtree(VsnapBtree);
}

NTSTATUS ValidateMountInfo( VsnapMountInfo *MountData)
{	

	size_t dwSize;

	if(-1 == IsStringNULLTerminated(MountData->PrivateDataDirectory , MAX_FILENAME_LENGTH ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo() Private Data Directory name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}
        
	if(-1 == IsStringNULLTerminated (MountData->RetentionDirectory , MAX_FILENAME_LENGTH ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo() Retention Directory name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}	

	if(-1 == IsStringNULLTerminated (MountData->VirtualVolumeName , MAX_FILENAME_LENGTH ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo() Virtual Volume Name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}	

	if(-1 == IsStringNULLTerminated (MountData->VolumeName , MAX_FILENAME_LENGTH ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo() Target Volume Name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}

	if (!NT_SUCCESS(RtlStringCbLengthA(MountData->VolumeName, MAX_NAME_LENGTH, &dwSize))) {
	  	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo::RtlStringCbLengthA failed for MountData->VolumeName %s\n", MountData->VolumeName));
		return STATUS_INVALID_PARAMETER;
	}

    if( (dwSize == 3 && (MountData->VolumeName[1] != ':' && MountData->VolumeName[2] != '\\'))
    || (dwSize == 2 && MountData->VolumeName[1] != ':') 
    || (dwSize >= 4 && (MountData->VolumeName[1] != ':' ||MountData->VolumeName[2] != '\\')  &&
    (MountData->VolumeName[0] != '\\' || MountData->VolumeName[1] != '\\')  ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo() Target Volume name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}

	if (!NT_SUCCESS(RtlStringCbLengthA(MountData->VirtualVolumeName, MAX_NAME_LENGTH, &dwSize))) {
	  	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo::RtlStringCbLengthA failed for MountData->VirtualVolumeName %s\n", MountData->VirtualVolumeName));
		return STATUS_INVALID_PARAMETER;
	}

	if( (dwSize == 3 && (MountData->VirtualVolumeName[1] != ':' && MountData->VirtualVolumeName[2] != '\\'))
	|| (dwSize == 2 && MountData->VirtualVolumeName[1] != ':') 
	|| (dwSize >= 4 && (MountData->VirtualVolumeName[1] != ':' ||
	MountData->VirtualVolumeName[2] != '\\' )  ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo() Virtual Volume name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}

	return STATUS_SUCCESS;
}


NTSTATUS ValidateUpdateInfo( UPDATE_VSNAP_VOLUME_INPUT *UpdateInfo)
{	
	size_t dwSize;
	
	if(-1 == IsStringNULLTerminated (UpdateInfo->DataFileName, MAX_RETENTION_DATA_FILENAME_LENGTH ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateUpdateInfo() Data File  name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}	

	if(-1 == IsStringNULLTerminated (UpdateInfo->ParentVolumeName, MAX_FILENAME_LENGTH ))
	{
		InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateUpdateInfo() Parent Volume name is not correct \n"));
		return STATUS_INVALID_PARAMETER;	
	}	

	if (!NT_SUCCESS(RtlStringCbLengthA(UpdateInfo->ParentVolumeName, MAX_NAME_LENGTH, &dwSize))) {
	  	InVolDbgPrint(DL_VV_ERROR, DM_VV_VSNAP, ("Vsnap: ValidateMountInfo::RtlStringCbLengthA failed for UpdateInfo->ParentVolumeName %s\n", UpdateInfo->ParentVolumeName));
		return STATUS_INVALID_PARAMETER;
	}

    TrimLastBackSlashChar(UpdateInfo->ParentVolumeName, dwSize + 1);

	return STATUS_SUCCESS;
}

int IsStringNULLTerminated(PCHAR String , int len)
{	
	for(int i=0 ; i < len ; i++)
	{
		if(String[i] == '\0')
			return i;
	}

	return -1;
}

void TrimLastBackSlashChar(PCHAR String, SIZE_T len)
{
	size_t dwSize;

	if (!NT_SUCCESS(RtlStringCbLengthA(String, MAX_NAME_LENGTH, &dwSize))) {
	  	return;
	}

    
    if(dwSize <= 3)
        return;

    for(SIZE_T i=3 ; i < len ; i++)
	{
        if(String[i] == 0) {
            if(String[--i] == '\\')
                String[i] = 0;
            break;
        }
        
	}


}

bool InitCache(VsnapInfo *Vsnap)
{
    
    Vsnap->FileNameCache = (PFILE_NAME_CACHE)ALLOC_MEMORY(sizeof(FILE_NAME_CACHE), PagedPool);
    if(!Vsnap->FileNameCache) {
        return false;
    }

    RtlZeroMemory(Vsnap->FileNameCache , sizeof(FILE_NAME_CACHE));

    return true;
}

bool ReadFileIdTable(VsnapInfo *Vsnap,char (*FileName)[MAX_RETENTION_DATA_FILENAME_LENGTH], ULONGLONG  Offset, ULONG  EntriesToRead, ULONG StartFileId)
{
    ULONG                 ulStartFileid = 0,ulCounter = 0, ulLength = 0;
    ULONGLONG             ullSTS, ullCurrentTS;
    int                   iDataRead = 0;
    bool                  bFirst = true, Success = true;

    if(!EntriesToRead)
        return true;

     do
        {
            if(EntriesToRead > (MAX_DATA_READ/MAX_RETENTION_DATA_FILENAME_LENGTH)) {
                EntriesToRead -= (MAX_DATA_READ/MAX_RETENTION_DATA_FILENAME_LENGTH);
                ulLength = MAX_DATA_READ;
            } else {
                ulLength = EntriesToRead * MAX_RETENTION_DATA_FILENAME_LENGTH;
                EntriesToRead     = 0;
            }


            if(!READ_FILE(Vsnap->FileIdTableHandle, FileName, Offset, ulLength, &iDataRead))
            {
                Success = false;
                break;
            }

    

            for(ulCounter = 0 ; ulCounter < (ULONG)(iDataRead/MAX_RETENTION_DATA_FILENAME_LENGTH); ulCounter++) {
                
                if(StartFileId == Vsnap->WriteFileId) {
                    StartFileId++;                    
                    continue;
                }

                ullCurrentTS = ConvertStringToULLETS(FileName[ulCounter], true);
                if(!Vsnap->FileNameCache->ullCurrentTS) {
                    Vsnap->FileNameCache->ullCurrentTS = ullCurrentTS;
                }

                ullSTS = ConvertStringToULLSTS(FileName[ulCounter], false);
                if(ullSTS && (Vsnap->FileNameCache->ullCreationTS < ullSTS)) {
                    StartFileId++;                    
                    continue;
                }
                   

                if(Vsnap->FileNameCache->ullCurrentTS != ullCurrentTS) {
                    if(StartFileId <= Vsnap->FileNameCache->ulBackwardFileId) {
                        Vsnap->FileNameCache->ulBackwardFileId = StartFileId -1;
                        SetNextDataFileId(Vsnap, true);
                        EntriesToRead = 0;
                        break;
                    } else {
                        Vsnap->FileNameCache->ulBackwardFileId = 0;
                        Vsnap->FileNameCache->ulForwardFileId = StartFileId;
                        DestroyCache(Vsnap);
                        InsertIntoCache(Vsnap, ConvertStringToULLHash(FileName[ulCounter]), StartFileId, 0);
                        Vsnap->FileNameCache->ullCurrentTS = ullCurrentTS ;
                        SetNextDataFileId(Vsnap, true);
                        StartFileId++;
                    }

                    
                    
                } else {
                    InsertIntoCache(Vsnap, ConvertStringToULLHash(FileName[ulCounter]), StartFileId, 0);
                    StartFileId++;
                }
            }            

            Offset += iDataRead;
            if(ulLength != iDataRead) {
                Success = false;
                break;
            }

        }while(EntriesToRead && Success);
    

return Success;



}


bool ReadAndFillCache(VsnapInfo *Vsnap)
{
    LARGE_INTEGER         StartLoc;
    int                   iDataRead = 0 ;
    ULONG                 ulEntriesToReadFromStart = 0, ulEntriesToReadFromEnd = 0, ulNumberOfEntryFromStart = 0,ulNumberOfEntryFromEnd = 0;
    ULONG                 ulStartFileid = 0,ulCounter = 0, ulLength = 0, ulTotalOfEntry = 0,ulCurrentFid = 0;;
    NTSTATUS              Status =  STATUS_SUCCESS;
    char                  (*FileName)[MAX_RETENTION_DATA_FILENAME_LENGTH] = NULL;
    bool                  bFirst = true, Success = true;

    Vsnap->FileNameCache->TSHash = (ULONGLONG *)ALLOC_MEMORY(FILE_NAME_CACHE_TSHASH_ENTRIES_BUFFER_SIZE, PagedPool);
    Vsnap->FileNameCache->FileId = (ULONG *)ALLOC_MEMORY(FILE_NAME_CACHE_FID_ENTRIES_BUFFER_SIZE, PagedPool);
    Vsnap->FileNameCache->ulMaxEntriesInCache = FILE_NAME_CACHE_ENTRIES_COUNT;
 

    if(!Vsnap->FileNameCache->TSHash && !Vsnap->FileNameCache->FileId) {
        return false;
    }

    
    ulCurrentFid = (ULONG)(Vsnap->ullFileSize / MAX_RETENTION_DATA_FILENAME_LENGTH);

    if(ulCurrentFid < Vsnap->FileNameCache->ulBackwardFileId)
        return false;

    if(ulCurrentFid < (Vsnap->FileNameCache->ulForwardFileId - 1))
        return false;


    Vsnap->FileNameCache->ulNoOfEntriesInCache = 0;
    ulNumberOfEntryFromEnd = (ULONG)(Vsnap->ullFileSize / MAX_RETENTION_DATA_FILENAME_LENGTH - Vsnap->FileNameCache->ulForwardFileId + 1);

    if(Vsnap->FileNameCache->ulBackwardFileId) { 
       ulNumberOfEntryFromStart = Vsnap->FileNameCache->ulBackwardFileId;        
    }

    
    if(ulNumberOfEntryFromEnd >= FILE_NAME_CACHE_ENTRIES_COUNT) {
            ulEntriesToReadFromEnd = FILE_NAME_CACHE_ENTRIES_COUNT;
            ulEntriesToReadFromStart = 0;
    } else {
            ulEntriesToReadFromEnd = ulNumberOfEntryFromEnd;
            if((FILE_NAME_CACHE_ENTRIES_COUNT - ulEntriesToReadFromEnd) >= ulNumberOfEntryFromStart)
                ulEntriesToReadFromStart = ulNumberOfEntryFromStart;
            else
                ulEntriesToReadFromStart = FILE_NAME_CACHE_ENTRIES_COUNT - ulEntriesToReadFromEnd;

    }

    ulStartFileid = ulCurrentFid - ulEntriesToReadFromEnd + 1;

        
    FileName = (char (*)[MAX_RETENTION_DATA_FILENAME_LENGTH])ALLOC_MEMORY(MAX_DATA_READ, PagedPool);
    if(!FileName) {
        return false;
    }

    do 
    {

        if(!ReadFileIdTable(Vsnap, FileName, 0, ulEntriesToReadFromStart, 1))
        {
            Success = false;
            break;
        }
       
        StartLoc.QuadPart = Vsnap->ullFileSize - ulEntriesToReadFromEnd * MAX_RETENTION_DATA_FILENAME_LENGTH;
        if(!ReadFileIdTable(Vsnap, FileName, StartLoc.QuadPart, ulEntriesToReadFromEnd, ulStartFileid))
        {
            Success = false;
            break;
        }
    }while(false);

    FREE_MEMORY(FileName);

    return Success;
}

void InsertIntoCache(VsnapInfo *Vsnap, ULONGLONG TimeStamp, ULONG  fid, ULONG ulIndex)
{
    int                               low  = -1, high , mid =0;
    ULONG                             ulFileId, leastFidIndex,Count;
    ULONGLONG                         *FileNameCache = NULL;
    bool                              bRemoveOldestEntry = false;

    /*
    if(Vsnap->FileNameCache->ulNoOfEntriesInCache >= Vsnap->FileNameCache->ulMaxEntriesInCache) {
        if(Vsnap->FileNameCache->ulMaxEntriesInCache == MAX_FILE_NAME_CACHE_ENTRIES) {
            bRemoveOldestEntry = true;        
        } else {

            FileNameCache = (ULONGLONG *)ALLOC_MEMORY(MAX_FILE_NAME_CACHE_ENTRIES_SIZE, PagedPool);
            if(!FileNameCache) {
                bRemoveOldestEntry = true;
            } else {
                RtlCopyMemory(FileNameCache, Vsnap->FileNameCache->TSHash, MIN_FILE_NAME_CACHE_ENTRIES * sizeof(ULONGLONG));
                RtlCopyMemory((char *)FileNameCache + MAX_FILE_NAME_CACHE_ENTRIES * sizeof(ULONGLONG) , Vsnap->FileNameCache->FileId, MIN_FILE_NAME_CACHE_ENTRIES * sizeof(ULONG));

                FREE_MEMORY(Vsnap->FileNameCache->TSHash);
                Vsnap->FileNameCache->TSHash = FileNameCache;
                Vsnap->FileNameCache->FileId = (ULONG *)((char *)FileNameCache + MAX_FILE_NAME_CACHE_ENTRIES * sizeof(ULONGLONG));
                Vsnap->FileNameCache->ulMaxEntriesInCache = MAX_FILE_NAME_CACHE_ENTRIES;
            }

        }
    }

*/
//if cache gets full, remove the oldest entry to accomodate new entry
    /*
    if(bRemoveOldestEntry) {

        ulFileId = Vsnap->FileNameCache->FileId[0];
        leastFidIndex = 0;
        for(Count =1 ; Count < Vsnap->FileNameCache->ulNoOfEntriesInCache; Count++) {
            if(ulFileId < Vsnap->FileNameCache->FileId[Count]) {
                ulFileId = Vsnap->FileNameCache->FileId[Count];
                leastFidIndex = Count;
            }
        }

        for(Count = leastFidIndex; Count < Vsnap->FileNameCache->ulNoOfEntriesInCache ; Count++) {
            Vsnap->FileNameCache->TSHash[Count] = Vsnap->FileNameCache->TSHash[Count + 1];
            Vsnap->FileNameCache->FileId[Count] = Vsnap->FileNameCache->FileId[Count + 1];

        }
        Vsnap->FileNameCache->ulNoOfEntriesInCache--;
    }
    */

//Actual Insertion logic starts here
    
        if(Vsnap->FileNameCache->ulNoOfEntriesInCache == 0) {
            Vsnap->FileNameCache->TSHash[0] = TimeStamp;
            Vsnap->FileNameCache->FileId[0] = fid;
            Vsnap->FileNameCache->ulNoOfEntriesInCache++;
            return;
        } else {
            if(0 == ulIndex) {
                high = Vsnap->FileNameCache->ulNoOfEntriesInCache;
                for(;;) {
                    mid = (low + high)/2;
                    if((mid == high) || (low == mid))
                        break;
                    if(Vsnap->FileNameCache->TSHash[mid] > TimeStamp){
                        high = mid;
                    } else {
                        low = mid;
                    }
        
                }
            } else {
                high = ulIndex;
            }
            if(Vsnap->FileNameCache->ulNoOfEntriesInCache < Vsnap->FileNameCache->ulMaxEntriesInCache) {
                for( Count = Vsnap->FileNameCache->ulNoOfEntriesInCache ; Count > (ULONG)high ; Count-- ){
                    Vsnap->FileNameCache->TSHash[Count] = Vsnap->FileNameCache->TSHash[Count -1];
                    Vsnap->FileNameCache->FileId[Count] = Vsnap->FileNameCache->FileId[Count -1];
                
                }
                Vsnap->FileNameCache->ulNoOfEntriesInCache++;
            } else {
                if(high == Vsnap->FileNameCache->ulMaxEntriesInCache)
                    high--;
            }
            
            Vsnap->FileNameCache->TSHash[high] = TimeStamp;
            Vsnap->FileNameCache->FileId[high] = fid;        
            
        }


    

 
}

etSearchResult SearchIntoCache(VsnapInfo *Vsnap, ULONGLONG  TimeStamp, ULONG *fid, ULONG *ulIndex)
{
    etSearchResult   eResult = ecStatusNotFound;
    int low = -1, high = Vsnap->FileNameCache->ulNoOfEntriesInCache, mid = (low + high) /2;
    for(;;) {
        mid = (low + high)/2;
        if((mid == high) || (low == mid))
                break;
            
        if(Vsnap->FileNameCache->TSHash[mid] > TimeStamp){
            high = mid;
        } else if(Vsnap->FileNameCache->TSHash[mid] < TimeStamp) {
            low = mid;
        }else {
            (*fid) = Vsnap->FileNameCache->FileId[mid];
            return ecStatusFound;
        }

    }

/*    if(Vsnap->DataFileIdUpdated) {
        if((Vsnap->FileNameCache->ulCHFirstFileid + Vsnap->FileNameCache->ulMaxEntriesInCache - 1) >= Vsnap->DataFileId)
            eResult = ecStatusNotFound;
    } else {
        if((Vsnap->FileNameCache->ulCHFirstFileid + Vsnap->FileNameCache->ulMaxEntriesInCache ) >= Vsnap->DataFileId)
            eResult = ecStatusNotFound; 
    }
*/
    (*ulIndex) = high;
    return eResult;
}

void DestroyCache(VsnapInfo *Vsnap)
{
    ULONGLONG          *FileNameCache = NULL;
    Vsnap->FileNameCache->ulNoOfEntriesInCache = 0;
/*
    if(MAX_FILE_NAME_CACHE_ENTRIES == Vsnap->FileNameCache->ulMaxEntriesInCache) {
        FileNameCache = (ULONGLONG *)ALLOC_MEMORY(MIN_FILE_NAME_CACHE_ENTRIES_SIZE, PagedPool);
        if(!FileNameCache) {
            return;
        }

        FREE_MEMORY(Vsnap->FileNameCache->TSHash);
        Vsnap->FileNameCache->TSHash = FileNameCache;
        Vsnap->FileNameCache->FileId = (ULONG *)((char *)Vsnap->FileNameCache->TSHash + MIN_FILE_NAME_CACHE_ENTRIES * sizeof(ULONGLONG) );
        Vsnap->FileNameCache->ulMaxEntriesInCache = MIN_FILE_NAME_CACHE_ENTRIES;

        

    }
*/
}
ULONGLONG StrToInt(char *str,int count ,bool *err)
{
    ULONGLONG   ullRetNumber = 0;
    ULONG       ulTmpNum = 0;

    *err = false;

    if(str == NULL)
        return 0;

    for(int i = 0 ; i < count; i++) {
        ulTmpNum = (str[i] - '0');
        ASSERT(ulTmpNum <= 9);
        if(ulTmpNum > 9) {
            *err = true;
            return 0;
        }
        ullRetNumber += ulTmpNum;
        ullRetNumber *= 10;
    }

    ullRetNumber /= 10;

    return ullRetNumber;
    
}

ULONGLONG ConvertStringToULLHash(char *str)
{
    char          *temp = str;
    ULONG         SeqNoLen = 0;
    ULONGLONG     ullRetNum = 0,ullTmpNum = 0;
    bool          err = true;

    if(str == NULL)
        return 0;
    temp = temp + FILE_TYPE_OFFSET;
    ullTmpNum = StrToInt(temp, FILE_TYPE_LENGTH, &err);
    if(err){
        DbgPrint("String is %s",str);
        return 0;
    }

    ullRetNum += ullTmpNum;

    temp = temp - FILE_TYPE_OFFSET + START_TIME_STAMP_OFFSET;
    for(int i = 0; i < START_TIME_STAMP_LENGTH ; i++)
        ullRetNum = ullRetNum * 10;
    
    ullTmpNum = StrToInt(temp, START_TIME_STAMP_LENGTH, &err);
    if(err){
        DbgPrint("String is %s",str);
        return 0;
    }

    ullRetNum += ullTmpNum;

    temp = temp - START_TIME_STAMP_OFFSET + SEQUENCE_NUMBER_OFFSET;
    for(int i = 0 ; i < 7 ; i++) {
        if(temp[i] != '.')
            SeqNoLen++;
        else
            break;
    }

    for(ULONG i = 0; i < SeqNoLen ; i++)
        ullRetNum = ullRetNum * 10;

    ullTmpNum = StrToInt(temp, SeqNoLen, &err);
    if(err){
        DbgPrint("String is %s",str);
        return 0;
    }

    ullRetNum += ullTmpNum;

    return ullRetNum;



}

ULONGLONG  ConvertStringToULLETS(char *str, bool WithBookMark)
{
    char *temp = str;
    bool  err = false;
    
    if(str == NULL)
        return 0;
    temp = temp + END_TIME_STAMP_OFFSET;
    if(WithBookMark) {
        return StrToInt(temp, END_TIME_STAMP_LENGTH,&err);
    } else {
        return StrToInt(temp, END_TIME_STAMP_LENGTH - BOOK_MARK_LENGTH,&err);
    }
}

ULONGLONG  ConvertStringToULLSTS(char *str, bool WithBookMark)
{
    
    char *temp = str;
    bool  err = false;
    
    if(str == NULL)
        return 0;
    temp = temp + START_TIME_STAMP_OFFSET;
    if(WithBookMark) {
        return StrToInt(temp, START_TIME_STAMP_LENGTH,&err);
    } else {
        return StrToInt(temp, START_TIME_STAMP_LENGTH - BOOK_MARK_LENGTH,&err);
    }
}

void CloseAndFreeMultiSparseFileHandle(ULONG index, void **MultiPartSparseHandle)
{
    for (ULONG indx = 0; indx < index; indx++)
        CLOSE_FILE(MultiPartSparseHandle[indx]);
    FREE_MEMORY(MultiPartSparseHandle);
}
