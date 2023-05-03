#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define SECTOR_SIZE 512
// First partition entry starts at 0x1BE (446)
#define FIRST_PARTITION_ENTRY_OFFSET    0x1BE

typedef struct _PARTITION_ENTRY {
    UCHAR   Bootable;   // Set to 0x80 for bootable
    UCHAR   BeginHead;
    UCHAR   BeginSector; // Bit 7, and Bit 6 are for cylinder and 5-0 are for sector
    UCHAR   BeginCylinder;
    UCHAR   PartitionType;
    UCHAR   EndHead;
    UCHAR   EndSector;
    UCHAR   EndCylinder;
    ULONG   SectorsPrecedingPartition;
    ULONG   PartitionSizeInSectors;
} PARTITION_ENTRY, *PPARTITION_ENTRY;

#define DRIVE_NAME_SIZE_IN_CHARS    512
#define MAX_SECTORS_IN_CHS          (1024 * 255 * 63)

#define PARTITION_TYPE_NTFS         0x07

#define SECTORS_PER_TRACK   63
#define NUM_HEADS           255
#define MAX_CYLINDERS       1024

#define BEGIN_CYLINDER      0
#define BEGIN_HEAD          1
#define BEGIN_SECTOR        1

#define CMDOPT_SET_ACTIVE   _T("-SetActive")
#define CMDOPT_SET_START_LOCATION _T("-SetStartLocation")
#define CMDOPT_SET_PARTITION_TYPE _T("-SetPartitionType")
#define CMDOPT_PARTITION_TYPE_NTFS _T("NTFS")

#define CMD_OPTIONS_FLAG_SET_ACTIVE             0x00000001
#define CMD_OPTIONS_FLAG_SET_START_LOCATION     0x00000002
#define CMD_OPTIONS_FLAG_SET_PARTITION_TYPE     0x00000004

typedef struct _CMD_OPTIONS {
    TCHAR       *PhysicalDriveName;
    ULONGLONG   ullPartitionSize;
    UCHAR       ucPartitionType;
    ULONG       Flags;
} CMD_OPTIONS, *PCMD_OPTIONS;

void
Usage(TCHAR *argv[])
{
    _tprintf(_T("Usage: %s [%s] [%s] [%s %s] PhysicalDriveX VolumeSizeInBytes\n"), argv[0],
        CMDOPT_SET_ACTIVE, CMDOPT_SET_START_LOCATION, CMDOPT_SET_PARTITION_TYPE, CMDOPT_PARTITION_TYPE_NTFS);
    return;
}

bool
ParseCommandOptions(PCMD_OPTIONS pCmdOptions, int argc, TCHAR *argv[])
{
    int index = 1;

    memset(pCmdOptions, 0, sizeof(CMD_OPTIONS));

    while (index < argc) {
        if (0 == _tcsicmp(argv[index], CMDOPT_SET_ACTIVE)) {
            pCmdOptions->Flags |= CMD_OPTIONS_FLAG_SET_ACTIVE;
        } else if (0 == _tcsicmp(argv[index], CMDOPT_SET_START_LOCATION)) {
            pCmdOptions->Flags |= CMD_OPTIONS_FLAG_SET_START_LOCATION;
        } else if (0 == _tcsicmp(argv[index], CMDOPT_SET_PARTITION_TYPE)) {
            index++;
            if (index >= argc) {
                _tprintf(_T("no filesystem specified for %s\n"), CMDOPT_SET_PARTITION_TYPE);
                return FALSE;
            }
            if (0 == _tcsicmp(argv[index], CMDOPT_PARTITION_TYPE_NTFS)) {
                pCmdOptions->ucPartitionType = PARTITION_TYPE_NTFS;
            } else {
                _tprintf(_T("Invalid filesystem type %s\n"), argv[index]);
                return FALSE;
            }

        } else if (NULL == pCmdOptions->PhysicalDriveName) {
            pCmdOptions->PhysicalDriveName = argv[index];
        } else if (0 == pCmdOptions->ullPartitionSize) {
            pCmdOptions->ullPartitionSize = _tcstoui64(argv[index], NULL, 0);
        } else {
            _tprintf(_T("Unrecognized option %s\n"), argv[index]);
            return FALSE;
        }
        index++;
    }

    if ((NULL == pCmdOptions->PhysicalDriveName) || (0 == pCmdOptions->ullPartitionSize)) {
        return FALSE;
    }

    return TRUE;
}

void
_tmain(int argc, TCHAR *argv[])
{
    HANDLE  hDrive = INVALID_HANDLE_VALUE;
    BOOL    bReturn;
    DWORD   dwBytesRead = 0, dwReturn = 0, dwBytesWritten = 0;
    UCHAR   *pBuffer = NULL;
    PPARTITION_ENTRY    pPartitionEntry = NULL;
    TCHAR   DriveName[DRIVE_NAME_SIZE_IN_CHARS] = {0};
    size_t  ccRemaining = DRIVE_NAME_SIZE_IN_CHARS - 1;
    ULONGLONG   ullVolumeSize, ullPartitionStartOffset, ullPartitionEndSectorOffset;
    ULONGLONG   ullMaxSizeInCHSFormat;
    ULARGE_INTEGER  ullVolumeSizeInSectors, ullEndOfParititionInLBA;
    CMD_OPTIONS CmdOptions;

    if (FALSE == ParseCommandOptions(&CmdOptions, argc, argv)) {
        Usage(argv);
        return;
    }

    // Open \PhysicalDrive0
    ullMaxSizeInCHSFormat = 1024;
    ullMaxSizeInCHSFormat = ullMaxSizeInCHSFormat * 255 * 63 * 512;
    
    _tcsncat_s(DriveName, ARRAYSIZE(DriveName), _T("\\\\.\\"), ccRemaining);
    ccRemaining -= _tcslen(DriveName);
    _tcsncat_s(DriveName, ARRAYSIZE(DriveName), CmdOptions.PhysicalDriveName, ccRemaining);
    
    ullVolumeSize = CmdOptions.ullPartitionSize;
    _tprintf(_T("Opening drive %s to set VolumeSize %I64u\n"), DriveName, ullVolumeSize);
    ullVolumeSizeInSectors.QuadPart = (ullVolumeSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (0 != ullVolumeSizeInSectors.HighPart) {
        _tprintf(_T("Volume size greater than 1 Tera byte. Failing..\n"));
        return;
    }

    hDrive = CreateFile(DriveName,
        FILE_READ_DATA | FILE_WRITE_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0, //FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Failed to open drive %s\n"),CmdOptions.PhysicalDriveName);
        return;
    }
    
    _tprintf(_T("Success in opening %s\n"), CmdOptions.PhysicalDriveName);

    pBuffer = (UCHAR *)malloc(2*4096);
    if (NULL == pBuffer)
        return;

    pBuffer = (UCHAR *)(((ULONG_PTR)pBuffer + 0xFFF) & 0xFFFFF000);

    dwReturn = SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
    bReturn = ReadFile(hDrive, pBuffer, SECTOR_SIZE, &dwBytesRead, NULL);
    if (FALSE == bReturn) {
        _tprintf(_T("ReadFile failed with error %#x\n"), GetLastError());
        CloseHandle(hDrive);
        return;
    }
    
    if (SECTOR_SIZE != dwBytesRead) {
        _tprintf(_T("Number of bytes read are %d\n"), dwBytesRead);
        return;
    }

    _tprintf(_T("Number of bytes read are %d\n"), dwBytesRead);

    pPartitionEntry = (PPARTITION_ENTRY) (pBuffer + FIRST_PARTITION_ENTRY_OFFSET);
    _tprintf(_T("First partition size in sectors = %d(0x%x), changing to %d(0x%x)\n"), pPartitionEntry->PartitionSizeInSectors,
            pPartitionEntry->PartitionSizeInSectors, ullVolumeSizeInSectors.LowPart, ullVolumeSizeInSectors.LowPart);

    if (CmdOptions.Flags & CMD_OPTIONS_FLAG_SET_ACTIVE) {
        pPartitionEntry->Bootable = 0x80;
    }

    if (CmdOptions.Flags & CMD_OPTIONS_FLAG_SET_START_LOCATION) {
        pPartitionEntry->BeginCylinder = (BEGIN_CYLINDER & 0xFF);
        pPartitionEntry->BeginHead = BEGIN_HEAD;
        pPartitionEntry->BeginSector = (BEGIN_SECTOR & 0x3F) | ((BEGIN_CYLINDER & 0x300) >> 2);
        pPartitionEntry->SectorsPrecedingPartition = 0x3F;
    }

    if (CmdOptions.ucPartitionType)
        pPartitionEntry->PartitionType = CmdOptions.ucPartitionType;

    pPartitionEntry->PartitionSizeInSectors = ullVolumeSizeInSectors.LowPart;
    ullPartitionStartOffset = pPartitionEntry->SectorsPrecedingPartition;
    ullPartitionStartOffset = ullPartitionStartOffset * SECTOR_SIZE;
    // ullPartitionEndSectorOffset is the sector offset of last sector in the partition.
    // This value will be a SECTOR_SIZE less than the volume boundary.
    ullPartitionEndSectorOffset = ullPartitionStartOffset + ullVolumeSize - SECTOR_SIZE;
    if (ullPartitionEndSectorOffset >=  ullMaxSizeInCHSFormat) {
        pPartitionEntry->EndHead = 0xFE;
        pPartitionEntry->EndSector = 0xFF;
        pPartitionEntry->EndCylinder = 0xFF;
        _tprintf(_T("C = 1024, H = 254, S = 63\n"));
    } else {
        ULONG   ulNumTracks, ulCylinderInCHS, ulSectorInCHS;

        ullEndOfParititionInLBA.QuadPart = ullPartitionEndSectorOffset / SECTOR_SIZE;
        ulSectorInCHS = ullEndOfParititionInLBA.LowPart % SECTORS_PER_TRACK;
        pPartitionEntry->EndSector = (UCHAR)ulSectorInCHS + 1;

        ulNumTracks = ullEndOfParititionInLBA.LowPart / SECTORS_PER_TRACK;
        pPartitionEntry->EndHead = (UCHAR) (ulNumTracks % NUM_HEADS);

        ulCylinderInCHS = ulNumTracks / NUM_HEADS;
        pPartitionEntry->EndCylinder = (UCHAR)(ulCylinderInCHS & 0xFF);

        pPartitionEntry->EndSector |= (UCHAR)((ulCylinderInCHS & 0x0300) >> 2);

        _tprintf(_T("C = %d, H = %d, S = %d\n"), ulCylinderInCHS, pPartitionEntry->EndHead,
                    pPartitionEntry->EndSector);
    }

    dwReturn = SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
    dwReturn = WriteFile(hDrive, pBuffer, SECTOR_SIZE, &dwBytesWritten, NULL);
    if (FALSE == bReturn) {
        _tprintf(_T("WriteFile failed with error %#x\n"), GetLastError());
        CloseHandle(hDrive);
        return;
    }
    _tprintf(_T("Number of bytes written are %d\n"), dwBytesWritten);

    if (hDrive != INVALID_HANDLE_VALUE)
        CloseHandle(hDrive);

    return;
}
