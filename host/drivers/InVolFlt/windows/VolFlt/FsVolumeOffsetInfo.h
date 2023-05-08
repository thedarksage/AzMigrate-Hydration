/*
 * FsVolumeOffset.h
 *
 * Murali Kartha.
 *
 * Fix for Bug 27337
 * This fix is to address an inherent design issue where a volume read length obtained from bitmap is by default
 * adjusted with grunalarity attribute. When there happens a volume write to the boundary cluster of FS then
 * the length calculation based on granularity could lead to FS IO beyond the FS aware volume space and thus
 * leading to a resync.
 *
 */
 
#pragma once

#include "global.h"

#ifndef FSCTL_GET_RETRIEVAL_POINTER_BASE
#define FSCTL_GET_RETRIEVAL_POINTER_BASE    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 141, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef FSCTL_GET_REFS_VOLUME_DATA
#define FSCTL_GET_REFS_VOLUME_DATA          CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 182, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

typedef enum {FSTYPE_UNKNOWN, FSTYPE_NTFS, FSTYPE_FAT, FSTYPE_REFS} FileSystemTypeList;
  
#define AlignQuad(n) (((n) + (sizeof(LONGLONG) - 1)) & ~(sizeof(LONGLONG) - 1))
#define CONSTRUCT(n, type, cchname)                                       \
        (((n) * AlignQuad(sizeof(type) + (cchname) * sizeof(WCHAR)) +   \
          sizeof(type) - 1) / sizeof(type))


#if (_WIN32_WINNT < 0x0602) //It should be converted to NTDDI_WIN8(NTDDI_VERSION)

typedef struct _REFS_VOLUME_DATA_BUFFER{

    ULONG 			ByteCount;
    ULONG 			MajorVersion;
    ULONG 			MinorVersion;
    ULONG 			BytesPerPhysicalSector;
    LARGE_INTEGER 	VolumeSerialNumber;
    LARGE_INTEGER 	NumberSectors;
    LARGE_INTEGER 	TotalClusters;
    LARGE_INTEGER 	FreeClusters;
    LARGE_INTEGER 	TotalReserved;
    ULONG 			BytesPerSector;
    ULONG 			BytesPerCluster;
    LARGE_INTEGER 	MaximumSizeOfResidentFile;
    
    LARGE_INTEGER 	Reserved[10];

} REFS_VOLUME_DATA_BUFFER, *PREFS_VOLUME_DATA_BUFFER;

#endif /* _WIN32_WINNT < 0x0602 */

//FAT STRUCTURES START HERE........

/////PACKED OnDisk Bios Parameter Block -Kartha
typedef struct _ONDISK_FAT32_BPB {
    UCHAR   BytesPerSector[2];
    UCHAR   SectorsPerCluster[1]; 
    UCHAR   ReservedSectors[2]; 
    UCHAR   NumberOfFats[1];
    UCHAR   RootEntries[2];
    UCHAR   NumberOfSectors[2];
    UCHAR   MediaDescriptor[1];
    UCHAR   SectorsPerFat[2];
    UCHAR   SectorsPerTrack[2]; 
    UCHAR   HeadsPerCylinder[2];
    UCHAR   HiddenSectors[4];
    UCHAR   BigNumberOfSectors[4];
    UCHAR   BigSectorsPerFat[4];
    UCHAR   ExtFlags[2];
    UCHAR   FsVersion[2];
    UCHAR   RootDirectoryStart[4];
    UCHAR   FsInfoSector[2];
    UCHAR   BackupBootSector[2];
    UCHAR   Reserved[12];                
}ONDISK_FAT32_BPB, *PONDISK_FAT32_BPB;
//
// this is a processor friendly version of the FAT32 BPB
// UNPCKED.
typedef struct _FAT32_BPB {
    USHORT  BytesPerSector;
    UCHAR   SectorsPerCluster; 
    USHORT  ReservedSectors; 
    UCHAR   NumberOfFats;
    USHORT  RootEntries;
    USHORT  NumberOfSectors;
    UCHAR   MediaDescriptor;
    USHORT  SectorsPerFat;
    USHORT  SectorsPerTrack; 
    USHORT  HeadsPerCylinder;
    ULONG   HiddenSectors;
    ULONG   BigNumberOfSectors;
    ULONG   BigSectorsPerFat;
    USHORT  ExtFlags;
    USHORT  FsVersion;
    ULONG   RootDirectoryStart;
    USHORT  FsInfoSector;
    USHORT  BackupBootSector;

}FAT32_BPB, *PFAT32_BPB;

// this provides a sector sized object
// for reading the boot sector off of the volume,
// and getting access to the OndiskFat32Bpb.
//
typedef struct _FAT32_BOOT_SECTOR {
    union {
        UCHAR sector[512];
        struct {
            UCHAR reserved[11];
            ONDISK_FAT32_BPB ondiskBpb;
        } bpb;
    };
}FAT32_BOOT_SECTOR, *PFAT32_BOOT_SECTOR;

typedef union _UCHAR1 {
    UCHAR  Uchar[1];
    UCHAR  ForceAlignment;
} UCHAR1, *PUCHAR1;

typedef union _UCHAR2 {
    UCHAR  Uchar[2];
    USHORT ForceAlignment;
} UCHAR2, *PUCHAR2;

typedef union _UCHAR4 {
    UCHAR  Uchar[4];
    ULONG  ForceAlignment;
} UCHAR4, *PUCHAR4;

//  This macro copies an unaligned src byte to an aligned dst byte
#define CopyUchar1(Dst,Src) {                                \
    *((UCHAR1 *)(Dst)) = *((UNALIGNED UCHAR1 *)(Src)); \
    }

//  This macro copies an unaligned src word to an aligned dst word
#define CopyUchar2(Dst,Src) {                                \
    *((UCHAR2 *)(Dst)) = *((UNALIGNED UCHAR2 *)(Src)); \
    }

//  This macro copies an unaligned src longword to an aligned dsr longword
#define CopyUchar4(Dst,Src) {                                \
    *((UCHAR4 *)(Dst)) = *((UNALIGNED UCHAR4 *)(Src)); \
    }
	
#define CopyU4char(Dst,Src) {                                \
    *((UNALIGNED UCHAR4 *)(Dst)) = *((UCHAR4 *)(Src)); \
    }
	
#define Fat32UnpackBios(Bios,Pbios) {                                    \
    CopyUchar2(&(Bios)->BytesPerSector,    &(Pbios)->BytesPerSector[0]   ); \
    CopyUchar1(&(Bios)->SectorsPerCluster, &(Pbios)->SectorsPerCluster[0]); \
    CopyUchar2(&(Bios)->ReservedSectors,   &(Pbios)->ReservedSectors[0]  ); \
    CopyUchar1(&(Bios)->NumberOfFats,      &(Pbios)->NumberOfFats[0]     ); \
    CopyUchar2(&(Bios)->RootEntries,       &(Pbios)->RootEntries[0]      ); \
    CopyUchar2(&(Bios)->NumberOfSectors,   &(Pbios)->NumberOfSectors[0]  ); \
    CopyUchar1(&(Bios)->MediaDescriptor,   &(Pbios)->MediaDescriptor[0]  ); \
    CopyUchar2(&(Bios)->SectorsPerFat,     &(Pbios)->SectorsPerFat[0]    ); \
    CopyUchar2(&(Bios)->SectorsPerTrack,   &(Pbios)->SectorsPerTrack[0]  ); \
    CopyUchar2(&(Bios)->HeadsPerCylinder,  &(Pbios)->HeadsPerCylinder[0] ); \
    CopyUchar4(&(Bios)->HiddenSectors,     &(Pbios)->HiddenSectors[0]    ); \
    CopyUchar4(&(Bios)->BigNumberOfSectors,&(Pbios)->BigNumberOfSectors[0]); \
	CopyUchar4(&(Bios)->BigSectorsPerFat,  &(Pbios)->BigSectorsPerFat[0]); \
	CopyUchar2(&(Bios)->ExtFlags,          &(Pbios)->ExtFlags[0]   ); \
	CopyUchar2(&(Bios)->FsVersion,         &(Pbios)->FsVersion[0]   ); \
	CopyUchar4(&(Bios)->RootDirectoryStart,&(Pbios)->RootDirectoryStart[0]); \
	CopyUchar2(&(Bios)->FsInfoSector,      &(Pbios)->FsInfoSector[0]   ); \
	CopyUchar2(&(Bios)->BackupBootSector,  &(Pbios)->BackupBootSector[0]   ); \
}
//FAT STRUCTURES END HERE.........


typedef struct _FSINFORMATION {

 ULONG 			SectorsPerCluster;
 ULONG 			BytesPerSector;
 ULONG 			SizeOfFsBlockInBytes;
 LARGE_INTEGER 	TotalClustersInFS;
 LARGE_INTEGER 	SectorOffsetOfFirstDataLCN;
 ULONG64 		MaxVolumeByteOffsetForFs;
 
}FSINFORMATION, *PFSINFORMATION;

typedef struct _FSINFOWRAPPER {
	
 FSINFORMATION 	FsInfo;
 NTSTATUS		FsInfoNtStatus;
 LONG   		FsInfoFetchRetries;
  
}FSINFOWRAPPER, *PFSINFOWRAPPER;

//Function declaration starts here.
NTSTATUS 
InMageFltFetchFsEndClusterOffset(
 __in PUNICODE_STRING pUniqueVolumeName,
 __inout PFSINFORMATION pFsInfo
 );
