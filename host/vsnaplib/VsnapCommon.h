//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapCommon.h
//
// Description: Common Header file used by both User/Kernel mode Vsnap library.
//

#ifndef _VSNAP_COMMON_H
#define _VSNAP_COMMON_H

#include "DiskBtreeHelpers.h"
#include "svtypes.h"

#if defined(VSNAP_WIN_KERNEL_MODE)
#include "WinVsnapKernelHelpers.h"
#endif

#if defined(VSNAP_USER_MODE)
#if defined(SV_WINDOWS)
#include<windows.h>
#endif

#ifdef SV_UNIX
typedef unsigned long long ULONGLONG;
typedef  unsigned long ULONG;
typedef char CHAR ;
typedef unsigned long DWORD;
#endif

#ifndef SV_WINDOWS
typedef struct _SINGLE_LIST_ENTRY {
  struct _SINGLE_LIST_ENTRY *Next;
} SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;

extern void InitializeEntryList(PSINGLE_LIST_ENTRY);
extern bool IsSingleListEmpty(PSINGLE_LIST_ENTRY);
extern SINGLE_LIST_ENTRY *PopEntryList(PSINGLE_LIST_ENTRY);
//extern void InsertInList(PSINGLE_LIST_ENTRY, SV_ULONGLONG, SV_ULONG, SV_ULONG , SV_ULONGLONG, SV_ULONG);
extern void InsertInList(PSINGLE_LIST_ENTRY, SV_ULONGLONG, SV_UINT, SV_UINT , SV_ULONGLONG, SV_UINT);
#endif

extern void PushEntryList(PSINGLE_LIST_ENTRY, PSINGLE_LIST_ENTRY);
#endif

#define VSNAP_RETENTION_FILEID_MASK	0x80000000
#define DEFAULT_VOLUME_SECTOR_SIZE	512

#define SET_WRITE_BIT_FOR_FILEID(x)		(x | VSNAP_RETENTION_FILEID_MASK)
#define RESET_WRITE_BIT_FOR_FILEID(x)	(x &= ~VSNAP_RETENTION_FILEID_MASK)

enum VSNAP_VOLUME_ACCESS_TYPE	{ VSNAP_READ_ONLY, VSNAP_READ_WRITE, VSNAP_READ_WRITE_TRACKING, VSNAP_UNKNOWN_ACCESS_TYPE };
enum VSNAP_CREATION_TYPE		{ VSNAP_POINT_IN_TIME, VSNAP_RECOVERY, VSNAP_CREATION_UNKNOWN };

/* Structure used to pass Vsnap Mount information from User program
 * to the invirvol driver.
 *
 * Members:
 *		RetentionDirectory		- Directory where retention log data files are stored.
 *		ParentVolumeName		- Replicated Target Volume Name or a virutal volume this Vsnap is pointing to.
 *		SnapShotId				- Unique Id associated with each Vsnap.
 *		RootNodeOffset			- Root Node Offset of Btree map file for this Vsnap.
 *		ParentVolumeSize		- Target Volume Size in bytes. Used in creating virtual volume of certain size.
 *		ParentVolumeSectorSize	- Sectore Size for this volume. This parameter is useful for performing aligment checks.
 *		NextDataFileId			- This is the next id for subsequent data files.
 *		RecoveryTime			- Recovery time associated with this Vsnap.
 */
struct VsnapMountInfoTag 
{
	char						RetentionDirectory[2048];
	char						PrivateDataDirectory[2048];
	char						VirtualVolumeName[2048];
	char						VolumeName[2048];
	SV_ULONGLONG					VolumeSize;
	SV_UINT 						VolumeSectorSize;
	SV_UINT 					NextDataFileId;
	SV_ULONGLONG					SnapShotId;			
	SV_ULONGLONG					RootNodeOffset;
	SV_ULONGLONG					RecoveryTime;
	VSNAP_VOLUME_ACCESS_TYPE	AccessType;
	bool						IsMapFileInRetentionDirectory;
	bool						IsTrackingEnabled;
	bool                        IsSparseRetentionEnabled;
	bool                        IsFullDevice;
};
typedef struct VsnapMountInfoTag VsnapMountInfo;

/* This structure stores some useful information that can be queried/modified from user through 
 * IOCTL calls.
 */
struct VsnapContextInfoTag 
{
	SV_ULONGLONG					SnapShotId;
	char						VolumeName[2048];
	SV_ULONGLONG					RecoveryTime;
	
	char						RetentionDirectory[2048];
	char						PrivateDataDirectory[2048];
	char						ParentVolumeName[2048];
	VSNAP_VOLUME_ACCESS_TYPE	AccessType;
	bool						IsTrackingEnabled;
	bool                        Reserved[3];
};
typedef struct VsnapContextInfoTag VsnapContextInfo;

/* This structure is used internally while inserting entries in the btree map and also while searching the map.
 */
struct VsnapOffsetSplitInfoTag {
	SINGLE_LIST_ENTRY	LinkNext;
	SV_ULONGLONG			Offset;
	SV_ULONGLONG			FileOffset;
	SV_UINT 					FileId;
	SV_UINT 					Length;
	SV_UINT 					TrackingId;
};
typedef struct VsnapOffsetSplitInfoTag VsnapOffsetSplitInfo;

#if defined(SV_SUN) || defined(SV_AIX)
    #pragma pack(1)
#else
    #pragma pack(push, 1)
#endif /* SV_SUN || SV_AIX */

/* FileIdTable file contains array of file name. For a particular id, the corresponding file name is found
 * using the formula (FileId-1)*sizeof(FileIdTable). This avoids searching in this table.
 */
struct VsnapFileIdTableTag
{
	char FileName[50];
};
typedef struct VsnapFileIdTableTag VsnapFileIdTable;

/* Structure to represent each entry in the node of a Btree map.
 */
struct VsnapKeyDataTag
{
	SV_ULONGLONG	Offset;
	SV_ULONGLONG	FileOffset;
	SV_UINT			FileId;
	SV_UINT			Length;
	SV_UINT			TrackingId;
};
typedef struct VsnapKeyDataTag VsnapKeyData;

#if defined(SV_SUN) || defined(SV_AIX)
    #pragma pack ()
#else
    #pragma pack (pop)
#endif /* SV_SUN  || SV_AIX */

struct VsnapPersistentHeaderInfoTag
{
	SV_ULONGLONG VsnapTime;
	SV_UINT		DataFileId;
	SV_UINT		WriteFileId;
	SV_UINT 	WriteFileSuffix;
	SV_UINT		BackwardFileId;
	SV_UINT		ForwardFileId;
};

typedef struct VsnapPersistentHeaderInfoTag VsnapPersistentHeaderInfo;

typedef struct _ADD_RETENTION_IOCTL_DATA {
    SV_USHORT UniqueVolumeIdLength;
    char  UniqueVolumeID[1022];
    char  VolumeInfo[1];
} ADD_RETENTION_IOCTL_DATA, *PADD_RETENTION_IOCTL_DATA;

/*
VsnapMountIoctlData structure added to support mouting vsnaps for Volpack target volumes split across Multiple files.MountIoctl for MultiFile
volpack is issued using new IOCTL. Driver relies on the size of VsnapMountIoctlData to differentiate the Mount IOCTL is whether old or new? 
User space and Kernel space has to maintain same size of structure by adding appropriate padding. 
*/

struct VsnapMountIoctlData 
{
	char						RetentionDirectory[2048];
	char						PrivateDataDirectory[2048];
	char						VirtualVolumeName[2048];
	char						VolumeName[2048];
	SV_ULONGLONG					VolumeSize;
	SV_UINT 						VolumeSectorSize;
	SV_UINT 					NextDataFileId;
	SV_ULONGLONG					SnapShotId;			
	SV_ULONGLONG					RootNodeOffset;
	SV_ULONGLONG					RecoveryTime;
	VSNAP_VOLUME_ACCESS_TYPE	AccessType;
	bool						IsMapFileInRetentionDirectory;
	bool						IsTrackingEnabled;
	bool                        IsSparseRetentionEnabled;
	bool                        IsFullDevice;
    SV_ULONGLONG                sparsefilechunksize;
    char                        fileextention[50];
    bool                        Reserved[6];
};
typedef struct VsnapMountIoctlData VsnapMountIoctlDataInfo;

#endif

