#include "global.h"
#include "misc.h"

FileRawIOWrapper::FileRawIOWrapper()
    : m_fileHandle(NULL),
    m_rangeStartOffset(0),
    m_rangeEndOffset(0),
    m_syncEventHandle(NULL),
    m_syncEventObject(NULL),
    m_isLearningCompleted(false),
    m_sectorsPerCluster(0),
    m_bytesPerLogicalSector(0),
    m_clusterSizeBytes(0),
    m_lcnStartSectorOffset(0),
    m_acquiredDiskNumber(FALSE),
    m_diskNumber(ULONG_MAX)
{
    InitializeListHead(&m_diskOffsetMappingList);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
FileRawIOWrapper::~FileRawIOWrapper()
{
    this->CleanupLearntInfo();

    SafeObDereferenceObject(m_syncEventObject);
    SafeCloseHandle(m_syncEventHandle);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::LearnFileSystemInformation()
{
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb = { 0 };
    FILE_FS_SIZE_INFORMATION FsSizeInfo = { 0 };

    PAGED_CODE();

    // TODO-SanKumar-1805: Ensure the mark for no compression and no defrag is on!

    // TODO-SanKumar-1805: Check if the file was compressed already and in that case also, drop the bitmap.

    // TODO-SanKumar-1805: Fail this call with not supported, if EFS is enabled (file system encryption)
    // http://www.osronline.com/showThread.cfm?link=98741
    // https://www.osronline.com/showThread.CFM?link=205351

    // Query the cluster size of the file system
    Status = ZwQueryVolumeInformationFile(
        m_fileHandle,
        &Iosb,
        &FsSizeInfo,
        sizeof(FsSizeInfo),
        FileFsSizeInformation);

    if (Status != STATUS_SUCCESS) {

        InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_FS_SIZE_INFO,
            m_fileHandle,
            Status);

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in calling ZwQueryVolumeInformationFile() to get info on file system, where the file resides. Handle %#p. Status Code %#x.\n",
            m_fileHandle, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    NT_ASSERT(Iosb.Information == sizeof(FsSizeInfo));

    InVolDbgPrint(DL_TRACE_L1, DM_FILE_RAW_WRAPPER,
        (__FUNCTION__ ": File handle %#p. Sectors per Allocation unit (Cluster) = %lu. Bytes per logical sector = %lu.\n",
        m_fileHandle, FsSizeInfo.SectorsPerAllocationUnit, FsSizeInfo.BytesPerSector));

    m_sectorsPerCluster = FsSizeInfo.SectorsPerAllocationUnit;
    m_bytesPerLogicalSector = FsSizeInfo.BytesPerSector;
    m_clusterSizeBytes = m_sectorsPerCluster * m_bytesPerLogicalSector;

    if (m_rangeStartOffset % m_bytesPerLogicalSector != 0 ||
        m_rangeEndOffset % m_bytesPerLogicalSector != 0) {
        Status = STATUS_INVALID_OFFSET_ALIGNMENT;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Range offset(s) is/are not sector aligned. Start offset %lld. End offset %lld. Bytes per logical sector = %lu. Status Code %#x.\n",
            m_rangeStartOffset, m_rangeEndOffset, m_bytesPerLogicalSector, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    // TODO-SanKumar-1805: Should we validate if the range end offset lies within the EOF?
    // Currently, if the range end offset is within the last cluster size (size on disk),
    // this library supports writing to that area. But when read through file system,
    // that data would never be consumed.
    // This becomes a bigger issue on moving to complete Raw IO writes for SetBits().
    // In the Init of bitmap file, the file is expanded if necessary by issuing all 0
    // writes till EOF (a genuine case is disk resize and corrupt case is file truncated).
    // Care must be taken, while completely using Raw IO around these scenarios.
    // Adding the m_rangeEndOffset <= EOF check helps us catch such design issues.

Cleanup:

    return Status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::LearnDiskOffsetFromVolumeOffset(
_In_ PDEVICE_OBJECT     VolStackDevObj,
_In_ ULONGLONG          FileOffset,
_In_ ULONGLONG          VolumeOffset
)
{
    NTSTATUS Status;
    VOLUME_PHYSICAL_OFFSETS OutBuff = { 0 };
    VOLUME_LOGICAL_OFFSET InBuff = { 0 };
    PIRP Irp;
    IO_STATUS_BLOCK Iosb = { 0 };
    PDISK_OFFSET_MAPPING_ENTRY CurrMappingEntry;

    PAGED_CODE();

    Irp = NULL;
    CurrMappingEntry = NULL;

    // TODO-SanKumar-1806: Change all the offsets in this file to LONGLONG
    InBuff.LogicalOffset = (LONGLONG)VolumeOffset;

    if (m_syncEventObject == NULL) {
        Status = STATUS_INVALID_DEVICE_STATE;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Sync event object is NULL. Status Code %#x.\n",
            Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    // TODO-SanKumar-1806: Add a debug counter to assert single-threaded use.
    KeResetEvent(m_syncEventObject);

    // TODO-SanKumar-1805: What if there are filter drivers that remap the offsets
    // returned by VolMgr? (highly unlikely). Even if so, we are issuing this
    // IOCTL to the top-most object in the device stack, by which we expect that
    // the filter drivers should reflect the remapping (if any) in the output.
    Irp = IoBuildDeviceIoControlRequest(
        IOCTL_VOLUME_LOGICAL_TO_PHYSICAL,
        VolStackDevObj,
        &InBuff,
        sizeof(InBuff),
        &OutBuff,
        sizeof(OutBuff),
        FALSE,
        m_syncEventObject,
        &Iosb);

    // TODO-SanKumar-1805: Repeated allocations and deallocations. We should
    // instead reuse the IRP.

    if (Irp == NULL) {
        Status = STATUS_NO_MEMORY;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER | DM_MEM_TRACE,
            (__FUNCTION__ ": Allocation failure in building the IRP for IOCTL_VOLUME_LOGICAL_TO_PHYSICAL. Status Code %#x.\n",
            Status));

        goto Cleanup;
    }

    Status = IoCallDriver(VolStackDevObj, Irp);

    if (Status == STATUS_PENDING) {
        Status = KeWaitForSingleObject(m_syncEventObject, Executive, KernelMode, FALSE, NULL);

        if (Status == STATUS_SUCCESS) {
            Status = Iosb.Status;
        }
    }

    // Irp is freed by the IO Manager on Synchronous completion
    Irp = NULL;

    if (Status != STATUS_SUCCESS) {

        InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_LOG_TO_PHY_OFF,
            m_fileHandle,
            FileOffset,
            VolumeOffset,
            Status);

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in invoking IOCTL_VOLUME_LOGICAL_TO_PHYSICAL. File offset %llu. Volume offset %llu. Status Code %#x.\n",
            FileOffset, VolumeOffset, Status));

        goto Cleanup;
    }

    NT_ASSERT(Iosb.Information == sizeof(OutBuff));

    if (OutBuff.NumberOfPhysicalOffsets != 1) {
        // TODO-SanKumar-1805: What if in some configuration only a single disk
        // is used and more than physical offsets residing on the same disk is
        // returned. Should we handle that?
        Status = STATUS_DEVICE_CONFIGURATION_ERROR;

        InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_INV_PHY_OFF_CNT,
            m_fileHandle,
            FileOffset,
            VolumeOffset,
            OutBuff.NumberOfPhysicalOffsets);

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Unexpected error. More than one physical offset reported. NumberOfPhysicalOffsets %lu. Status Code %#x.\n",
            OutBuff.NumberOfPhysicalOffsets, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (m_acquiredDiskNumber) {
        if (OutBuff.PhysicalOffset[0].DiskNumber != m_diskNumber) {
            Status = STATUS_DEVICE_CONFIGURATION_ERROR;

            InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_TOO_MANY_DISKNUM,
                m_fileHandle,
                FileOffset,
                VolumeOffset,
                OutBuff.PhysicalOffset[0].Offset,
                m_diskNumber,
                OutBuff.PhysicalOffset[0].DiskNumber);

            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Unexpected error. More than one physical disk learnt. PrevDiskNumber %lu. CurrDiskNumber %lu. Status Code %#x.\n",
                OutBuff.PhysicalOffset[0].DiskNumber, m_diskNumber, Status));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }
    }
    else {
        m_diskNumber = OutBuff.PhysicalOffset[0].DiskNumber;
        m_acquiredDiskNumber = TRUE;
    }

    InVolDbgPrint(DL_TRACE_L2, DM_FILE_RAW_WRAPPER,
        (__FUNCTION__ ": File offset %llu. Volume offset %llu. Disk Num %lu. Disk offset %lld.\n",
        FileOffset,
        VolumeOffset,
        OutBuff.PhysicalOffset[0].DiskNumber,
        OutBuff.PhysicalOffset[0].Offset));

    if (!IsListEmpty(&m_diskOffsetMappingList)) {

        CurrMappingEntry = CONTAINING_RECORD(
            m_diskOffsetMappingList.Blink,
            DISK_OFFSET_MAPPING_ENTRY,
            ListEntry);

        if (CurrMappingEntry->FileOffset + CurrMappingEntry->Length != FileOffset) {
            InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_MAP_NON_CONSEC_FILE_OFF,
                m_fileHandle,
                CurrMappingEntry->FileOffset,
                CurrMappingEntry->Length,
                FileOffset,
                VolumeOffset,
                OutBuff.PhysicalOffset[0].Offset);

            Status = STATUS_INTERNAL_ERROR;

            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Internal error as the FileOffset %llu is not adjacent to the previous mapping entry (FileOffset %llu, Length %llu). Status Code %#x.\n",
                FileOffset, CurrMappingEntry->FileOffset, CurrMappingEntry->Length, Status));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }

        if (CurrMappingEntry->DiskOffset + CurrMappingEntry->Length == (ULONGLONG)OutBuff.PhysicalOffset[0].Offset) {
            CurrMappingEntry->Length += m_bytesPerLogicalSector;

            Status = STATUS_SUCCESS;
            goto Cleanup;
        }

        // Operational channel entry
        InDskFltWriteEvent(INDSKFLT_INFO_FILERAWIOWRAPPER_MAPPING_ENTRY,
            m_fileHandle,
            CurrMappingEntry->FileOffset,
            CurrMappingEntry->VolumeOffset,
            CurrMappingEntry->DiskOffset,
            CurrMappingEntry->Length);

        // Since this is not a consecutive sector to the last map entry's last
        // sector, this needs a new map entry.
        CurrMappingEntry = NULL;
    }

    NT_ASSERT(CurrMappingEntry == NULL);

    CurrMappingEntry = (PDISK_OFFSET_MAPPING_ENTRY)ExAllocatePoolWithTag(
        NonPagedPoolNx,
        sizeof(DISK_OFFSET_MAPPING_ENTRY),
        TAG_FILE_RAW_IO_WRAPPER);

    if (CurrMappingEntry == NULL) {

        Status = STATUS_NO_MEMORY;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER | DM_MEM_TRACE,
            (__FUNCTION__ ": Allocation failure on adding a disk offset mapping entry. Status Code %#x.\n",
            Status));

        goto Cleanup;
    }

    RtlZeroMemory(CurrMappingEntry, sizeof(*CurrMappingEntry));
    CurrMappingEntry->FileOffset = FileOffset;
    CurrMappingEntry->VolumeOffset = VolumeOffset;
    CurrMappingEntry->DiskOffset = (ULONGLONG)OutBuff.PhysicalOffset[0].Offset;
    CurrMappingEntry->Length = m_bytesPerLogicalSector;

    InsertTailList(&m_diskOffsetMappingList, &CurrMappingEntry->ListEntry);

    Status = STATUS_SUCCESS;

Cleanup:
    NT_ASSERT(Irp == NULL);

    return Status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::BuildFileExtentsList(
_In_ PDEVICE_OBJECT volStackDevObj
)
{
    NTSTATUS Status;
    LONGLONG LearntRangeEndOffset;
    PRETRIEVAL_POINTERS_BUFFER pRetrievalPtrsOutBuff;
    STARTING_VCN_INPUT_BUFFER StartingVcnInBuff;
    IO_STATUS_BLOCK Iosb = { 0 };
    ULONG CurrExtentInd;
    LONGLONG PrevRetrievedNextVcn;
    LONGLONG RoundingAdjustedVcnCount;
    ULONGLONG CurrClusterInd;
    ULONGLONG CurrSectorInd;
    LONGLONG CurrLcn;
    LONGLONG CurrNextVcn;
    ULONGLONG CurrExtentLength;
    LONGLONG CurrFileOffset;
    LONGLONG CurrVolOffset;
    bool HasReachedEndOfRange;
    PDISK_OFFSET_MAPPING_ENTRY FinalDiskMappingEntry;

    PAGED_CODE();

    Status = STATUS_SUCCESS;
    pRetrievalPtrsOutBuff = NULL;
    LearntRangeEndOffset = m_rangeStartOffset;
    HasReachedEndOfRange = false;

    if (m_syncEventHandle == NULL || m_syncEventObject == NULL) {
        Status = STATUS_INVALID_DEVICE_STATE;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Sync event handle (%#p) or Sync event object (%#p) is NULL. Status Code %#x.\n",
            m_syncEventHandle, m_syncEventObject, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    // TODO-SanKumar-1805: Do we need a bigger buffer with more entries? Generally
    // the bitmap header is really small and the fragmentation is minimal. But,
    // the number of extents must be exposed as a macro. TESTING REQUIRED, since
    // the loop for multiple extents isn't executed currently more than once.
    pRetrievalPtrsOutBuff = (PRETRIEVAL_POINTERS_BUFFER)ExAllocatePoolWithTag(
        PagedPool,
        sizeof(RETRIEVAL_POINTERS_BUFFER),
        TAG_FS_QUERY);

    // TODO-SanKumar-1805: Repeated small buffer allocations. This can be a stack variable.

    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        (__FUNCTION__ ": Allocation for PRETRIEVAL_POINTERS_BUFFER %#p.\n",
        pRetrievalPtrsOutBuff));

    if (pRetrievalPtrsOutBuff == NULL) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    while (LearntRangeEndOffset < m_rangeEndOffset)
    {
        StartingVcnInBuff.StartingVcn.QuadPart = LearntRangeEndOffset / m_clusterSizeBytes;

        RtlZeroMemory(pRetrievalPtrsOutBuff, sizeof(RETRIEVAL_POINTERS_BUFFER));
        RtlZeroMemory(&Iosb, sizeof(Iosb));

        KeResetEvent(m_syncEventObject);

        Status = ZwFsControlFile(
            m_fileHandle,
            m_syncEventHandle,
            NULL,
            NULL,
            &Iosb,
            // Supported on NTFS, FAT, exFAT, and UDF file systems (MSDN)
            FSCTL_GET_RETRIEVAL_POINTERS,
            &StartingVcnInBuff,
            sizeof(StartingVcnInBuff),
            pRetrievalPtrsOutBuff,
            sizeof(RETRIEVAL_POINTERS_BUFFER)
            );

        if (Status == STATUS_PENDING) {
            Status = ZwWaitForSingleObject(m_syncEventHandle, FALSE, NULL);

            if (Status == STATUS_SUCCESS) {
                Status = Iosb.Status;
            }
        }

        if (Status != STATUS_SUCCESS && Status != STATUS_BUFFER_OVERFLOW) {
            InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_GET_RETR_PTRS,
                m_fileHandle,
                StartingVcnInBuff.StartingVcn.QuadPart,
                Status);

            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Failed in invoking FSCTL_GET_RETRIEVAL_POINTERS. File Handle %#p. StartingVCN %lld. Status Code %#x.\n",
                m_fileHandle, StartingVcnInBuff.StartingVcn.QuadPart, Status));

            goto Cleanup;
        }

        Status = STATUS_SUCCESS;

        // StartingVcn is not necessarily the VCN requested by the function call,
        // as the file system driver may round down to the first VCN of the extent
        // in which the requested starting VCN is found.
#pragma warning(push)
#pragma warning(disable:6102) // SDV check for using uninitialized out param on failure (ret val != STATUS_SUCCESS)
        if (pRetrievalPtrsOutBuff->StartingVcn.QuadPart <= StartingVcnInBuff.StartingVcn.QuadPart) {
#pragma warning(pop)

            // This could be only false on the first ever query, since all the
            // following iterations would be called with the NextVcn given by FS.
            NT_ASSERT(
                pRetrievalPtrsOutBuff->StartingVcn.QuadPart == StartingVcnInBuff.StartingVcn.QuadPart ||
                IsListEmpty(&m_diskOffsetMappingList));

            PrevRetrievedNextVcn = StartingVcnInBuff.StartingVcn.QuadPart;
            RoundingAdjustedVcnCount =
                StartingVcnInBuff.StartingVcn.QuadPart - pRetrievalPtrsOutBuff->StartingVcn.QuadPart;
        }
        else {
            InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_STARTVCN_GT_REQUESTED,
                m_fileHandle,
                pRetrievalPtrsOutBuff->StartingVcn.QuadPart,
                StartingVcnInBuff.StartingVcn.QuadPart);

            // There's a mistake in this retrieval logic.
            Status = STATUS_INTERNAL_ERROR;

            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Internal error as the returned starting VCN %lld is greater than the requested VCN %lld. Status Code %#x.\n",
                pRetrievalPtrsOutBuff->StartingVcn.QuadPart, StartingVcnInBuff.StartingVcn.QuadPart, Status));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }

        for (CurrExtentInd = 0; CurrExtentInd < pRetrievalPtrsOutBuff->ExtentCount; CurrExtentInd++)
        {
            CurrNextVcn = pRetrievalPtrsOutBuff->Extents[CurrExtentInd].NextVcn.QuadPart;
            CurrLcn = pRetrievalPtrsOutBuff->Extents[CurrExtentInd].Lcn.QuadPart;

            if (CurrLcn == -1) {
                InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_COMPR_OR_SPARSE_FILE,
                    m_fileHandle,
                    StartingVcnInBuff.StartingVcn.QuadPart,
                    PrevRetrievedNextVcn,
                    CurrNextVcn,
                    CurrLcn);

                // On the NTFS file system, the value(LONGLONG) –1 indicates either
                // a compression unit that is partially allocated, or an unallocated
                // region of a sparse file.
                Status = STATUS_COMPRESSED_FILE_NOT_SUPPORTED;

                InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                    (__FUNCTION__ ": Compressed file is not supported by FileRawIOWrapper. File handle %#p. Requested starting VCN %lld. CurrExtentInd %lu. Status Code %#x.\n",
                    m_fileHandle, StartingVcnInBuff.StartingVcn.QuadPart, CurrExtentInd, Status));

                NT_ASSERT(FALSE);
                goto Cleanup;
            }

            if (CurrLcn < 0) {
                InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_INV_LCN,
                    m_fileHandle,
                    StartingVcnInBuff.StartingVcn.QuadPart,
                    PrevRetrievedNextVcn,
                    CurrNextVcn,
                    CurrLcn);

                Status = STATUS_UNEXPECTED_IO_ERROR;

                InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                    (__FUNCTION__ ": Error in the current LCN %llu, which is negative. File handle %#p. Requested starting VCN %lld. CurrExtentInd %lu. Status Code %#x.\n",
                    CurrLcn, StartingVcnInBuff.StartingVcn.QuadPart, CurrExtentInd, Status));

                NT_ASSERT(FALSE);
                goto Cleanup;
            }

            CurrLcn += RoundingAdjustedVcnCount;
            CurrExtentLength = (ULONGLONG)(CurrNextVcn - PrevRetrievedNextVcn); // Length in clusters

            if ((LONGLONG)CurrExtentLength <= 0) {
                InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_INV_EXT_LENGTH_CALC,
                    m_fileHandle,
                    StartingVcnInBuff.StartingVcn.QuadPart,
                    PrevRetrievedNextVcn,
                    CurrNextVcn,
                    CurrLcn,
                    (LONGLONG)CurrExtentLength);

                // There's a mistake in this retrieval logic.
                Status = STATUS_INTERNAL_ERROR;

                InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                    (__FUNCTION__ ": Internal error in the current extent length (%lld) <= 0. CurrNextVcn %llu. PrevRetrievedNextVcn %llu. RoundingAdjustedVcnCount %llu. Status Code %#x.\n",
                    (LONGLONG)CurrExtentLength, CurrNextVcn, PrevRetrievedNextVcn, RoundingAdjustedVcnCount, Status));

                NT_ASSERT(FALSE);
                goto Cleanup;
            }

            for (CurrClusterInd = 0; CurrClusterInd < CurrExtentLength && !HasReachedEndOfRange; CurrClusterInd++) {

                for (CurrSectorInd = 0; CurrSectorInd < m_sectorsPerCluster && !HasReachedEndOfRange; CurrSectorInd++) {

                    CurrFileOffset =
                        m_clusterSizeBytes * (PrevRetrievedNextVcn + RoundingAdjustedVcnCount + CurrClusterInd) +
                        m_bytesPerLogicalSector * CurrSectorInd;

                    if (CurrFileOffset >= m_rangeEndOffset) {
                        // If this stop isn't present, we would end up learning
                        // unnecessary large portions of file/even the full file.

                        HasReachedEndOfRange = true;
                        break;
                    }

                    CurrVolOffset = m_lcnStartSectorOffset +
                        m_clusterSizeBytes * (CurrLcn + CurrClusterInd) +
                        m_bytesPerLogicalSector * CurrSectorInd;

                    InVolDbgPrint(DL_TRACE_L2, DM_FILE_RAW_WRAPPER,
                        (__FUNCTION__ ": File offset %lld. LCN %lld. Volume offset %lld.\n",
                        CurrFileOffset,
                        CurrLcn + (LONG)CurrClusterInd,
                        CurrVolOffset));

                    Status = this->LearnDiskOffsetFromVolumeOffset(volStackDevObj, CurrFileOffset, CurrVolOffset);

                    if (Status != STATUS_SUCCESS) {

                        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                            (__FUNCTION__ ": Error in learning disk offset from the volume offset %lld. CurrFileOffset %lld. volStackDevObj %#p. Status Code %#x.\n",
                            CurrVolOffset, CurrFileOffset, volStackDevObj, Status));

                        goto Cleanup;
                    }
                }
            }

            PrevRetrievedNextVcn = CurrNextVcn;
            RoundingAdjustedVcnCount = 0;

            NT_ASSERT(LearntRangeEndOffset + CurrExtentLength * m_clusterSizeBytes == (ULONGLONG)(CurrNextVcn * m_clusterSizeBytes));
            LearntRangeEndOffset = CurrNextVcn * m_clusterSizeBytes;
        }
    }

    if (!IsListEmpty(&m_diskOffsetMappingList)) {
        // Log the final entry

        FinalDiskMappingEntry = CONTAINING_RECORD(
            m_diskOffsetMappingList.Blink,
            DISK_OFFSET_MAPPING_ENTRY,
            ListEntry);

        // Operational channel entry
        InDskFltWriteEvent(INDSKFLT_INFO_FILERAWIOWRAPPER_MAPPING_ENTRY,
            m_fileHandle,
            FinalDiskMappingEntry->FileOffset,
            FinalDiskMappingEntry->VolumeOffset,
            FinalDiskMappingEntry->DiskOffset,
            FinalDiskMappingEntry->Length);
    }

Cleanup:

    SafeExFreePoolWithTag(pRetrievalPtrsOutBuff, TAG_FS_QUERY);
    return Status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::IsNTFSFileSystem(_In_ HANDLE hVolume, _Out_ PBOOLEAN isNtfs)
{
    IO_STATUS_BLOCK                 Iosb = { 0 };
    ULONG                           Length;
    NTSTATUS                        Status = STATUS_SUCCESS;
    PFILE_FS_ATTRIBUTE_INFORMATION      pFsInformation;
    UNICODE_STRING                      ntfsString;
    UNICODE_STRING                      fsName;

    ASSERT(isNtfs != NULL);
    *isNtfs = FALSE;

    Length = sizeof(FILE_FS_ATTRIBUTE_INFORMATION) + MAX_LOG_PATHNAME;

    pFsInformation = (PFILE_FS_ATTRIBUTE_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, Length, TAG_FS_QUERY);
    if (NULL == pFsInformation) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    RtlZeroMemory(pFsInformation, Length);

    Status = ZwQueryVolumeInformationFile(hVolume, &Iosb, pFsInformation, Length, FileFsAttributeInformation);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_FSINFORMATION_QUERY_FAILED, Status);
        goto Cleanup;
    }

    RtlInitUnicodeString(&ntfsString, L"NTFS");

    if (pFsInformation->FileSystemNameLength == 4 * sizeof(WCHAR)) {
        RtlInitUnicodeString(&fsName, pFsInformation->FileSystemName);
        if (RtlEqualUnicodeString(&fsName, &ntfsString, TRUE)) {
            *isNtfs = TRUE;
            Status = STATUS_SUCCESS;
            goto Cleanup;
        }
    }

    InDskFltWriteEvent(INDSKFLT_ERROR_UNSUPPORTED_BITMAP_FS, pFsInformation->FileSystemName);

Cleanup:
    SafeExFreePoolWithTag(pFsInformation, TAG_FS_QUERY);

    return Status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::LearnVolumeOffsets()
{
#if (_WIN32_WINNT < 0x0601)
    NTSTATUS    Status = STATUS_SUCCESS;
    BOOLEAN     isNtfs = FALSE;

    Status = IsNTFSFileSystem(m_fileHandle, &isNtfs);
    if (NT_SUCCESS(Status) && !isNtfs) {
        Status = STATUS_NOT_SUPPORTED;
    }

    if (!NT_SUCCESS(Status)) {
        return Status;
    }
#endif

    return GetRetrievalPointerBase();
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::GetRetrievalPointerBase()
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PFILE_OBJECT        FileObject = NULL;
    PDEVICE_OBJECT      VolStackDeviceObject = NULL;
    HANDLE              VolHandle = NULL;
    PDEVICE_OBJECT      FsStackDeviceObject = NULL;

#if (_WIN32_WINNT >= 0x0601)
    IO_STATUS_BLOCK             Iosb = { 0 };
    RETRIEVAL_POINTER_BASE      RetrPtrBase;
#endif

    PAGED_CODE();

    m_lcnStartSectorOffset = 0;

    if (m_syncEventHandle == NULL || m_syncEventObject == NULL) {
        Status = STATUS_INVALID_DEVICE_STATE;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Sync event handle (%#p) or Sync event object (%#p) is NULL. Status Code %#x.\n",
            m_syncEventHandle, m_syncEventObject, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    Status = ObReferenceObjectByHandle(
        m_fileHandle,
        0,
        *IoFileObjectType,
        KernelMode,
        (PVOID*)&FileObject,
        NULL);

    if (Status != STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_REF_FILEOBJ,
            m_fileHandle,
            Status);

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in getting the file object of the file. File handle %#p. Status Code %#x.\n",
            m_fileHandle, Status));

        goto Cleanup;
    }

    FsStackDeviceObject = IoGetRelatedDeviceObject(FileObject);
    Status = GetVolumeHandle(FsStackDeviceObject, false, &VolHandle, &VolStackDeviceObject);
    FsStackDeviceObject = NULL;

    if (Status != STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in getting the volume handle from the FS dev object %#p. File handle %#p. Status Code %#x.\n",
            FsStackDeviceObject, m_fileHandle, Status));

        goto Cleanup;
    }

#if (_WIN32_WINNT >= 0x0601)
    KeResetEvent(m_syncEventObject);

    Status = ZwFsControlFile(
        VolHandle,
        m_syncEventHandle,
        NULL,
        NULL,
        &Iosb,
        FSCTL_GET_RETRIEVAL_POINTER_BASE,
        NULL,
        0,
        &RetrPtrBase,
        sizeof(RetrPtrBase));

    if (Status == STATUS_PENDING) {
        Status = ZwWaitForSingleObject(m_syncEventHandle, FALSE, NULL);

        if (Status == STATUS_SUCCESS) {
            Status = Iosb.Status;
        }
    }

    if (Status != STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_RETR_PTR_BASE,
            m_fileHandle,
            FsStackDeviceObject,
            VolHandle,
            Status);

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Invoking FSCTL_GET_RETRIEVAL_POINTER_BASE failed on volume handle %#p. FS dev obj %#p. File handle %#p. Status Code %#x.\n",
            VolHandle, FsStackDeviceObject, m_fileHandle, Status));

        goto Cleanup;
    }

    NT_ASSERT(Iosb.Information == sizeof(RetrPtrBase));
    m_lcnStartSectorOffset = RetrPtrBase.FileAreaOffset.QuadPart;
    // NOTE-SanKumar-1804: This is always 0 apart from FAT and exFAT.

    InVolDbgPrint(DL_TRACE_L2, DM_FILE_RAW_WRAPPER,
        (__FUNCTION__ ": Retrieved the volume relative first sector offset for cluster allocation %lld for volume dev object %#p. FS dev object %#p. File handle %#p.\n",
        m_lcnStartSectorOffset, VolStackDeviceObject, FsStackDeviceObject, m_fileHandle));
#endif

    Status = this->BuildFileExtentsList(VolStackDeviceObject);
    if (Status != STATUS_SUCCESS) {

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in building the file extents list for volume dev object %#p. FS dev object %#p. File handle %#p. Status Code %#x.\n",
            VolStackDeviceObject, FsStackDeviceObject, m_fileHandle, Status));

        goto Cleanup;
    }

Cleanup:
    NT_ASSERT(FsStackDeviceObject == NULL);
    SafeCloseHandle(VolHandle);
    SafeObDereferenceObject(VolStackDeviceObject);
    SafeObDereferenceObject(FileObject);

    return Status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::LearnDiskOffsets(
_In_ HANDLE FileHandle,
_In_ LONGLONG RangeStartOffset,
_In_ LONGLONG RangeEndOffset)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES EventOA;

    if (m_isLearningCompleted) {
        Status = STATUS_INVALID_DEVICE_STATE;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Learning has already been completed. Status Code %#x.\n",
            Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (FileHandle == NULL) {
        Status = STATUS_INVALID_PARAMETER_1;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": File handle passed is NULL. Status Code %#x.\n",
            Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (RangeStartOffset < 0 || RangeEndOffset < 0) {
        Status = STATUS_INVALID_PARAMETER_2;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Range start offset %lld and Range end offset %lld shouldn't be negative. Status Code %#x.\n",
            RangeStartOffset, RangeEndOffset, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (RangeStartOffset >= RangeEndOffset) {
        Status = STATUS_INVALID_PARAMETER_3;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Range end offset %lld should be larger than Range start offset %lld. Status Code %#x.\n",
            RangeEndOffset, RangeStartOffset, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    m_fileHandle = FileHandle;
    m_rangeStartOffset = RangeStartOffset;
    m_rangeEndOffset = RangeEndOffset;

    if ((m_syncEventObject == NULL) != (m_syncEventHandle == NULL)) {
        Status = STATUS_INVALID_DEVICE_STATE;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Sync event object(%#p) and Sync event handle (%#p) are not in the same state. Status Code %#x.\n",
            m_syncEventObject, m_syncEventHandle, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (m_syncEventHandle == NULL) {
        InitializeObjectAttributes(&EventOA, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

        Status = ZwCreateEvent(
            &m_syncEventHandle, NULL, &EventOA, SynchronizationEvent, FALSE);

        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Failed to create synchronization event. Status Code %#x.\n",
                Status));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }

        Status = ObReferenceObjectByHandle(
            m_syncEventHandle, NULL, *ExEventObjectType, KernelMode, (PVOID*)&m_syncEventObject, NULL);

        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Failed to obtain event object for synchronization event %#p. Status Code %#x.\n",
                m_syncEventHandle, Status));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }
    }

    Status = this->LearnFileSystemInformation();
    if (Status != STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in learning the file system information. File handle %#p. Status Code %#x.\n",
            m_fileHandle, Status));

        goto Cleanup;
    }

    Status = this->LearnVolumeOffsets();
    if (Status != STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in learning the volume and disk offsets. File handle %#p. Range start offset %lld. Range end offset %lld. Status Code %#x.\n",
            m_fileHandle, m_rangeStartOffset, m_rangeEndOffset, Status));

        goto Cleanup;
    }

    NT_ASSERT(m_acquiredDiskNumber);
    m_isLearningCompleted = true;

Cleanup:

    if (Status != STATUS_SUCCESS) {
        // Ensuring that this method is atomic, such that it can be invoked again
        // on any error in learning.
        this->CleanupLearntInfo();

        // There could've been some issues with the synchronization event too.
        SafeObDereferenceObject(m_syncEventObject);
        SafeCloseHandle(m_syncEventHandle);
    }

    return Status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::RecheckLearntInfo()
{
    NTSTATUS Status;
    BOOLEAN bIsMatch;
    FileRawIOWrapper compWrapper;
    PLIST_ENTRY lhsListEntry;
    PLIST_ENTRY rhsListEntry;
    PCDISK_OFFSET_MAPPING_ENTRY lhsDiskMappingEntry;
    PCDISK_OFFSET_MAPPING_ENTRY rhsDiskMappingEntry;

    Status = STATUS_SUCCESS;
    bIsMatch = FALSE;

    if (!m_isLearningCompleted || !m_acquiredDiskNumber) {
        Status = STATUS_DEVICE_NOT_READY;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Incorrect initialization. Learning completed %d. Acquired disk number %d. Status Code %#x.\n",
            m_isLearningCompleted, m_acquiredDiskNumber, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    Status = compWrapper.LearnDiskOffsets(m_fileHandle, m_rangeStartOffset, m_rangeEndOffset);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in learning the disk offsets. File handle %#p. Range start offset %lld. Range end offset %lld. Status Code %#x.\n",
            m_fileHandle, m_rangeStartOffset, m_rangeEndOffset, Status));

        goto Cleanup;
    }

    if (!compWrapper.m_isLearningCompleted || !compWrapper.m_acquiredDiskNumber) {
        Status = STATUS_INTERNAL_ERROR;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Incorrect initialization in the comparison object. Learning completed %d. Acquired disk number %d. Status Code %#x.\n",
            compWrapper.m_isLearningCompleted, compWrapper.m_acquiredDiskNumber, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (m_sectorsPerCluster != compWrapper.m_sectorsPerCluster ||
        m_bytesPerLogicalSector != compWrapper.m_bytesPerLogicalSector ||
        m_clusterSizeBytes != compWrapper.m_clusterSizeBytes ||
        m_lcnStartSectorOffset != compWrapper.m_lcnStartSectorOffset ||
        m_diskNumber != compWrapper.m_diskNumber) {

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Comparison failed between the basic elements of the objects.\n"
            "element, this, compObj\n"
            "-------  ----  -------\n"
            "Sectors per cluster, %lu, %lu\n"
            "Bytes per logical sector, %lu, %lu\n"
            "Bytes per cluster, %lld, %lld\n"
            "LCN start sector offset, %lld, %lld\n"
            "Disk number, %lu, %lu\n",
            m_sectorsPerCluster, compWrapper.m_sectorsPerCluster,
            m_bytesPerLogicalSector, compWrapper.m_bytesPerLogicalSector,
            m_clusterSizeBytes, compWrapper.m_clusterSizeBytes,
            m_lcnStartSectorOffset, compWrapper.m_lcnStartSectorOffset,
            m_diskNumber, compWrapper.m_diskNumber));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    lhsListEntry = m_diskOffsetMappingList.Flink;
    rhsListEntry = compWrapper.m_diskOffsetMappingList.Flink;

    while (lhsListEntry != &m_diskOffsetMappingList && rhsListEntry != &compWrapper.m_diskOffsetMappingList) {
        lhsDiskMappingEntry = CONTAINING_RECORD(
            lhsListEntry,
            DISK_OFFSET_MAPPING_ENTRY,
            ListEntry);

        rhsDiskMappingEntry = CONTAINING_RECORD(
            rhsListEntry,
            DISK_OFFSET_MAPPING_ENTRY,
            ListEntry);

        if (*lhsDiskMappingEntry != *rhsDiskMappingEntry) {
            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Comparison failed between the mapping entries of the objects.\n"));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }

        lhsListEntry = lhsListEntry->Flink;
        rhsListEntry = rhsListEntry->Flink;
    }

    if (lhsListEntry != &m_diskOffsetMappingList || rhsListEntry != &compWrapper.m_diskOffsetMappingList) {
        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": More entries found in one of the objects. this %d. comparison object %d.\n",
            lhsListEntry != &m_diskOffsetMappingList, rhsListEntry != &compWrapper.m_diskOffsetMappingList));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    bIsMatch = TRUE;

Cleanup:

    if (NT_SUCCESS(Status) && !bIsMatch) {
        Status = STATUS_CONTEXT_MISMATCH;
    }

    return Status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
FileRawIOWrapper::CleanupLearntInfo()
{
    PLIST_ENTRY ToDelListItem;
    PDISK_OFFSET_MAPPING_ENTRY ToDelMappingEntry;
    static const LARGE_INTEGER ZeroTimeout = { 0 };

    m_isLearningCompleted = false;

    m_sectorsPerCluster = 0;
    m_bytesPerLogicalSector = 0;
    m_clusterSizeBytes = 0;
    m_lcnStartSectorOffset = 0;

    m_acquiredDiskNumber = FALSE;
    m_diskNumber = ULONG_MAX;

    m_fileHandle = NULL;
    m_rangeStartOffset = 0;
    m_rangeEndOffset = 0;

    while (!IsListEmpty(&m_diskOffsetMappingList)) {

        ToDelListItem = RemoveHeadList(&m_diskOffsetMappingList);
        ToDelMappingEntry = CONTAINING_RECORD(
            ToDelListItem,
            DISK_OFFSET_MAPPING_ENTRY,
            ListEntry);

        ExFreePoolWithTag(ToDelMappingEntry, TAG_FILE_RAW_IO_WRAPPER);
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
FileRawIOWrapper::RawIoInternal(
_In_ PDEVICE_OBJECT DiskDevObj,
_When_(IsRead == TRUE, _Out_writes_bytes_(Length))
_When_(IsRead == FALSE, _In_reads_bytes_(Length))
PVOID Buffer,
_In_ LONGLONG FileOffset,
_In_ ULONG Length,
_In_ BOOLEAN IsRead)
{
    PCDISK_OFFSET_MAPPING_ENTRY CurrMappingEntry;
    PLIST_ENTRY CurrListItem;
    NTSTATUS Status;
    ULONG BufferOffset;
    ULONGLONG CurrIoFileOffset;
    ULONGLONG CurrIoDiskOffset;
    ULONGLONG CurrToExcludeStartingLength;
    ULONG CurrIoLength;
    AsyncIORequest *IoRequest;

    CurrListItem = m_diskOffsetMappingList.Flink;
    BufferOffset = 0;
    IoRequest = NULL;

    if (Buffer == NULL || FileOffset < 0 || Length == 0 || (FileOffset + Length) < FileOffset) {
        Status = STATUS_INVALID_PARAMETER;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Invalid parameter. Buffer %#p. FileOffset %lld. Length %lu. Status Code %#x.\n",
            Buffer, FileOffset, Length, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (!m_isLearningCompleted) {
        Status = STATUS_DEVICE_NOT_READY;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Learning hasn't been completed for this object. Status Code %#x.\n",
            Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (FileOffset < m_rangeStartOffset ||
        FileOffset + Length > m_rangeEndOffset) {
        Status = STATUS_RANGE_NOT_FOUND;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Requested range [%lld, %lld) is not part of the learnt range [%lld, %lld). Status Code %#x.\n",
            FileOffset, FileOffset + Length, m_rangeStartOffset, m_rangeEndOffset, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (FileOffset % m_bytesPerLogicalSector != 0 ||
        Length % m_bytesPerLogicalSector != 0) {
        Status = STATUS_INVALID_OFFSET_ALIGNMENT;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": The requested range is not logical sector aligned %lu. FileOffset %lld. Length %lu. Status Code %#x.\n",
            m_bytesPerLogicalSector, FileOffset, Length, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    while (BufferOffset < Length && CurrListItem != &m_diskOffsetMappingList) {

        NT_ASSERT(IoRequest == NULL);
        // TODO-SanKumar-1805: We should reuse AsyncIORequest instead of repeated allocations.
        IoRequest = new AsyncIORequest(DiskDevObj, NULL);
        InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
            (__FUNCTION__ ": Allocation for AsyncIORequest %p\n", IoRequest));

        if (IoRequest == NULL) {
            Status = STATUS_NO_MEMORY;
            goto Cleanup;
        }

        CurrMappingEntry = CONTAINING_RECORD(
            CurrListItem,
            DISK_OFFSET_MAPPING_ENTRY,
            ListEntry);

        /*
        CurrMappingEntry->                                  CurrMappingEntry->
        FileOffset                  CurrIoFileOffset        Length
        ---------------------------------------------------
        |                               |                   |
        |   CurrToExcludeStartingLength |                   |
        |                               |                   |
        ---------------------------------------------------
        */

        CurrIoFileOffset = FileOffset + BufferOffset;
        CurrToExcludeStartingLength = CurrIoFileOffset - CurrMappingEntry->FileOffset;

        // Apart from the first iteration, the remaining IOs should be performed
        // from the start of the mapping entry.
        NT_ASSERT(BufferOffset == 0 || CurrToExcludeStartingLength == 0);

        if (CurrMappingEntry->FileOffset > CurrIoFileOffset) {
            Status = STATUS_HASH_NOT_PRESENT;

            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Expected file offset %lld is not present in the mapping entries list. CurrMappingEntry file offset %lld. Status Code %#x.\n",
                CurrIoFileOffset, CurrMappingEntry->FileOffset, Status));

            NT_ASSERT(FALSE);
            goto Cleanup;
        }

        if (CurrMappingEntry->FileOffset < CurrIoFileOffset &&
            CurrToExcludeStartingLength >= CurrMappingEntry->Length) {

            // The expected range is not present in this entry
            CurrListItem = CurrListItem->Flink;
            continue;
        }

        // Disk offset for current IO request
        CurrIoDiskOffset = CurrMappingEntry->DiskOffset + CurrToExcludeStartingLength;

        // Length of the current IO request
        // Note that there's no need for RtlULongLongToULong() safe conversion,
        // since BufferOffset(ULONG) < Length(ULONG).
        CurrIoLength = (ULONG)min(
            Length - BufferOffset, // Total remaining bytes to read / write
            CurrMappingEntry->Length - CurrToExcludeStartingLength // Max operable length in the current entry
            );

        // SL_FORCE_DIRECT_WRITE recommended for raw read / write
        if (IsRead) {

            // Initiate the read

            Status = IoRequest->Read(
                (PCHAR)Buffer + BufferOffset,
                CurrIoLength,
                CurrIoDiskOffset,
                SL_FORCE_DIRECT_WRITE);
        } else {

            // Initiate the write

            // TODO-SanKumar-1807: Should issue SL_WRITE_THROUGH too.
            Status = IoRequest->Write(
                (PCHAR)Buffer + BufferOffset,
                CurrIoLength,
                CurrIoDiskOffset,
                SL_FORCE_DIRECT_WRITE);
        }

        if (Status == STATUS_PENDING) {
            Status = IoRequest->Wait(NULL);
        }

        if (Status != STATUS_SUCCESS) {
            InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
                (__FUNCTION__ ": Raw IO failed. Buffer %#p. BufferOffset %lu. CurrIOLength %lu. CurrIODiskOffset %llu. Status Code %#x.\n",
                Buffer, BufferOffset, CurrIoLength, CurrIoDiskOffset, Status));

            goto Cleanup;
        }

        BufferOffset += CurrIoLength;
        CurrListItem = CurrListItem->Flink;

        IoRequest->Release();
        IoRequest = NULL;
    }

    if (BufferOffset != Length) {
        // Either the buffer was overrrun or the end of mapping list is hit.
        Status = STATUS_INTERNAL_ERROR;

        InDskFltWriteEvent(INDSKFLT_ERROR_RAWIOWRAP_IO_NOT_FINISHED,
            m_fileHandle,
            FileOffset,
            Length,
            IsRead,
            BufferOffset);

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": The processed buffer length %lu is not as requested IO length %lu at the end of all IOs. Status Code %#x.\n",
            BufferOffset, Length, Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    Status = STATUS_SUCCESS;

Cleanup:

    if (IoRequest != NULL) {
        IoRequest->Release();
    }

    return Status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::RawReadFile(
_In_ PDEVICE_OBJECT DiskDevObj,
_Out_writes_bytes_(Length) PVOID Buffer,
_In_ LONGLONG FileOffset,
_In_ ULONG Length
)
{
    return this->RawIoInternal(DiskDevObj, Buffer, FileOffset, Length, TRUE);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FileRawIOWrapper::RawWriteFile(
_In_ PDEVICE_OBJECT DiskDevObj,
_In_reads_bytes_(Length) PVOID Buffer,
_In_ LONGLONG FileOffset,
_In_ ULONG Length
)
{
    return this->RawIoInternal(DiskDevObj, Buffer, FileOffset, Length, FALSE);
}

_Must_inspect_result_
NTSTATUS
FileRawIOWrapper::GetDiskDeviceNumber(
_Out_    ULONG   *DeviceNumber
)
{
    NTSTATUS Status;

    if (!m_isLearningCompleted) {
        Status = STATUS_DEVICE_NOT_READY;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Learning isn't completed. Status Code %#x.\n",
            Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (!m_acquiredDiskNumber) {
        Status = STATUS_INTERNAL_ERROR;

        InVolDbgPrint(DL_ERROR, DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Disk number hasn't been acquired at the end of learning operation. Status Code %#x.\n",
            Status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    NT_ASSERT(m_diskNumber != ULONG_MAX);

    *DeviceNumber = m_diskNumber;
    Status = STATUS_SUCCESS;

Cleanup:
    return Status;
}