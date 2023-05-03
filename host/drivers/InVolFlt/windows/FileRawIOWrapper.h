#pragma once

#include <wdm.h>

class FileRawIOWrapper
{
public:
    FileRawIOWrapper();

    _IRQL_requires_max_(PASSIVE_LEVEL)
        ~FileRawIOWrapper();

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        LearnDiskOffsets(
        _In_ HANDLE FileHandle,
        _In_ LONGLONG RangeStartOffset,
        _In_ LONGLONG RangeEndOffset);

    _IRQL_requires_max_(DISPATCH_LEVEL)
        void
        CleanupLearntInfo();

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        RawReadFile(
        _In_ PDEVICE_OBJECT DiskDevObj,
        _Out_writes_bytes_(Length) PVOID Buffer,
        _In_ LONGLONG FileOffset,
        _In_ ULONG Length);

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        RawWriteFile(
        _In_ PDEVICE_OBJECT DiskDevObj,
        _In_reads_bytes_(Length) PVOID Buffer,
        _In_ LONGLONG FileOffset,
        _In_ ULONG Length);

    _Must_inspect_result_
        NTSTATUS
        GetDiskDeviceNumber(
        _Out_    ULONG   *DeviceNumber
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        RecheckLearntInfo();

private:
    typedef struct _DISK_OFFSET_MAPPING_ENTRY
    {
        LIST_ENTRY ListEntry;
        ULONGLONG FileOffset;
        ULONGLONG VolumeOffset;
        ULONGLONG DiskOffset;
        ULONGLONG Length;

        _DISK_OFFSET_MAPPING_ENTRY()
            : FileOffset(0),
            VolumeOffset(0),
            DiskOffset(0),
            Length(0)
        {
            InitializeListHead(&ListEntry);
        }

        bool operator!=(const _DISK_OFFSET_MAPPING_ENTRY &rhs) const
        {
            const _DISK_OFFSET_MAPPING_ENTRY &lhs = *this;

            return
                lhs.DiskOffset != rhs.DiskOffset ||
                lhs.FileOffset != rhs.FileOffset ||
                lhs.Length != rhs.Length ||
                lhs.VolumeOffset != rhs.VolumeOffset;
        }
    } DISK_OFFSET_MAPPING_ENTRY, *PDISK_OFFSET_MAPPING_ENTRY;

    typedef const DISK_OFFSET_MAPPING_ENTRY *PCDISK_OFFSET_MAPPING_ENTRY;

    HANDLE m_fileHandle;
    LONGLONG m_rangeStartOffset;
    LONGLONG m_rangeEndOffset;
    HANDLE m_syncEventHandle;
    PKEVENT m_syncEventObject;

    bool m_isLearningCompleted;
    ULONG m_sectorsPerCluster;
    ULONG m_bytesPerLogicalSector;
    LONGLONG m_clusterSizeBytes;
    LONGLONG m_lcnStartSectorOffset;
    LIST_ENTRY m_diskOffsetMappingList;
    BOOLEAN m_acquiredDiskNumber;
    ULONG m_diskNumber;

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        LearnFileSystemInformation();

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        LearnVolumeOffsets();

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        GetRetrievalPointerBase();

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        LearnDiskOffsetFromVolumeOffset(
        _In_ PDEVICE_OBJECT     VolStackDevObj,
        _In_ ULONGLONG          FileOffset,
        _In_ ULONGLONG          VolumeOffset);

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        BuildFileExtentsList(_In_ PDEVICE_OBJECT volStackDevObj);

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        NTSTATUS
        RawIoInternal(
        _In_ PDEVICE_OBJECT DiskDevObj,
        _When_(IsRead == TRUE, _Out_writes_bytes_(Length))
        _When_(IsRead == FALSE, _In_reads_bytes_(Length))
        PVOID Buffer,
        _In_ LONGLONG FileOffset,
        _In_ ULONG Length,
        _In_ BOOLEAN IsRead);

    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS
        IsNTFSFileSystem(_In_ HANDLE hVolume, _Out_ PBOOLEAN isNtfs);
};