#ifndef _INMAGE_VV_COMMON_H_
#define _INMAGE_VV_COMMON_H_

extern "C"
{
#include "ntifs.h"
// Use this if we are using string safe library.
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
}

#include "VVDebugDefines.h"

// For Windows NT -> _WIN32_WINNT is 0x0400
// For Windows 2000 -> _WIN32_WINNT is 0x0500
// For Windows XP -> _WIN32_WINNT is 0x0501
// For Windows Server 2003 -> _WIN32_WINNT IS 0x0502
// For Windows Longhorn -> _WIN32_WINNT is 0x0600
//#if (_WIN32_WINNT <= 0x0500)
//#define MmGetSystemAddressForMdlSafe(MDL, PRIORITY) MmGetSystemAddressForMdlPrettySafe(MDL)
//#endif // (_WIN32_WINNT <= 0x0500)
       
// MEMORY_TAGS
#define VVTAG_GENERIC_PAGED             'pGVV'
#define VVTAG_GENERIC_NONPAGED          'nGVV'
#define VVTAG_BTREE_NODE                'tBVV'
#define VVTAG_COMMAND_QUEUE             'QCVV'
#define VVTAG_VSNAP_IO_QUEUE            'OIVV'
#define VVTAG_COMMAND_STRUCT            'mCVV'
#define VVTAG_COMMAND_IO_STRUCT         'oIVV'
#define VVTAG_CTRLDEV_REMOVE_LOCK       'LCVV'
#define VVTAG_VVOLUME_DEV_REMOVE_LOCK   'LDVV'
#define VVTAG_VSNAP_IO_TASK             'nTVV'
#define VVTAG_VSNAP_UPDATE_COMMAND      'cUVV'
#define VVTAG_VSNAP_OFFSET_INFO         'iOVV'
#define VVTAG_VSNAP_STRUCT              'sVVV'
#define VVTAG_VVOLUME_STRUCT            'vVVV' 
#define VVTAG_THREAD_POOL               'pTVV'
#define VVTAG_SEC_CONTEXT               'sCVV'

extern ULONG    ulDebugLevelForAll;
extern ULONG    ulDebugLevel;
extern ULONG    ulDebugMask;

#define SECTOR_SIZE 512
#define MAX_DATA_READ                                   2000
#define MAX_RETENTION_DATA_FILENAME_LENGTH	            50     //this is refering to retention filename length(not path length). this is a null terminating string
#define MAX_FILE_EXT_LENGTH                             50
#define MAX_FILENAME_LENGTH					            2048  //Max path length for retention directory only. this is a null terminating string
#define MAX_NAME_LENGTH							        2048
#define FILE_NAME_CACHE_ENTRIES_COUNT                   1022
#define FILE_NAME_CACHE_TSHASH_ENTRIES_BUFFER_SIZE      8184
#define FILE_NAME_CACHE_FID_ENTRIES_BUFFER_SIZE         4088
#define FILE_NAME_CACHE_ENTRY_SIZE              (sizeof(ULONGLONG) + sizeof(ULONG))

//cdpv30_<end time>_<type>_<start time>_<seq no>.dat     sparse retention filename
//        12 char     1 char  12 char     variable
#define BASE_YEAR                           2009
#define BOOK_MARK_LENGTH                    4


#define END_TIME_STAMP_OFFSET               7
#define END_TIME_STAMP_LENGTH               12

#define FILE_TYPE_OFFSET                    20
#define FILE_TYPE_LENGTH                    1

#define START_TIME_STAMP_OFFSET             22
#define START_TIME_STAMP_LENGTH             12


#define SEQUENCE_NUMBER_OFFSET              35
#define GUID_SIZE_IN_BYTES                  0x48
// Below length includes GUID in braces and null-terminator
#define GUID_LEN_WITH_BRACE                 GUID_SIZE_IN_BYTES + (3 * sizeof(WCHAR))
#define DEV_LINK_GUID_OFFSET                10
typedef
VOID
(*PCOMMAND_ROUTINE) (
    IN struct _DEVICE_EXTENSION *DeviceExtension,
    IN struct _COMMAND_STRUCT   *pCommand
    );

#if DBG
#define InVolDbgPrint(Level, Mod, x)                            \
{                                                               \
    if ((ulDebugLevelForAll >= (Level)) ||                      \
        ((ulDebugMask & (Mod)) && (ulDebugLevel >= (Level))))   \
        DbgPrint x;                                             \
}
#else
#define InVolDbgPrint(Level, Mod, x)  
#endif // DBG



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
 *      VolumeSize              - Source Volume Raw size
 */
struct VsnapMountInfoTag 
{
	CHAR						RetentionDirectory[MAX_NAME_LENGTH];
	CHAR						PrivateDataDirectory[MAX_NAME_LENGTH];
	CHAR						VirtualVolumeName[MAX_NAME_LENGTH];
	CHAR						VolumeName[MAX_NAME_LENGTH];
	ULONGLONG					VolumeSize;
	ULONG						VolumeSectorSize;
	ULONG						NextDataFileId;
	ULONGLONG					SnapShotId;			
	ULONGLONG					RootNodeOffset;
	ULONGLONG					RecoveryTime;
	VSNAP_VOLUME_ACCESS_TYPE	AccessType;
	bool						IsMapFileInRetentionDirectory;
	bool						IsTrackingEnabled;
    bool                        IsSparseRetentionEnabled;
	bool                        Reserved;
};
typedef struct VsnapMountInfoTag VsnapMountInfo;

struct MultipleSparseFileInfoTag
{
    bool      IsMultiPartSparseFile;
    ULONGLONG ChunkSize;
    CHAR      FileExt[50];
    ULONG     SplitFileCount;
};
typedef MultipleSparseFileInfoTag MultiPartSparseFileInfo;

/*
VsnapMountInfoTag_V2 structure added to support mouting vsnaps for Volpack target volumes split across Multiple files.MountIoctl for MultiFile
volpack is issued using new IOCTL. Driver relies on the size of VsnapMountInfoTag_V2 to differentiate the Mount IOCTL is whether old or new? 
User space and Kernel space has to maintain same size of structure by adding appropriate padding. 
*/

struct VsnapMountInfoTag_V2 
{
    CHAR                        RetentionDirectory[2048];
    CHAR                        PrivateDataDirectory[2048];
    CHAR                        VirtualVolumeName[2048];
    CHAR                        VolumeName[2048];
    ULONGLONG                   VolumeSize;
    ULONG                       VolumeSectorSize;
    ULONG                       NextDataFileId;
    ULONGLONG                   SnapShotId;
    ULONGLONG                   RootNodeOffset;
    ULONGLONG                   RecoveryTime;
    VSNAP_VOLUME_ACCESS_TYPE    AccessType;
    bool                        IsMapFileInRetentionDirectory;
    bool                        IsTrackingEnabled;
    bool                        IsSparseRetentionEnabled;
    bool                        IsFullDevice;
    ULONGLONG                   SparseFileChunkSize;
    CHAR                        FileExtension[50]; // '.' prefixes extension name
    bool                        Reserved[6];
};
typedef struct VsnapMountInfoTag_V2 VsnapMountInfo_V2;


/* This structure stores some useful information that can be queried/modified from user through 
 * IOCTL calls.
 */
struct VsnapContextInfoTag 
{
	ULONGLONG					SnapShotId;
	CHAR						VolumeName[2048];
	ULONGLONG					RecoveryTime;
	
	CHAR						RetentionDirectory[2048];
	CHAR						PrivateDataDirectory[2048];
	CHAR						ParentVolumeName[2048];
	VSNAP_VOLUME_ACCESS_TYPE	AccessType;
	bool						IsTrackingEnabled;
	bool                        Reserved[3];
};
typedef struct VsnapContextInfoTag VsnapContextInfo;

/* This structure is used internally while inserting entries in the btree map and also while searching the map.
 */
struct VsnapOffsetSplitInfoTag {
	SINGLE_LIST_ENTRY	LinkNext;
	ULONGLONG			Offset;
	ULONGLONG			FileOffset;
	ULONG				FileId;
	ULONG				Length;
	ULONG				TrackingId;
};
typedef struct VsnapOffsetSplitInfoTag VsnapOffsetSplitInfo;

struct VsnapRootNodeInfoTag {
	SINGLE_LIST_ENTRY	LinkNext;
	void               *Node;
	ULONG               RefCount;
};

typedef struct VsnapRootNodeInfoTag VsnapRootNodeInfo;

#pragma pack(push, 1)

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
	ULONGLONG	Offset;
	ULONGLONG	FileOffset;
	ULONG		FileId;
	ULONG		Length;
	ULONG		TrackingId;
};
typedef struct VsnapKeyDataTag VsnapKeyData;

#pragma pack (pop)

struct VsnapPersistentHeaderInfoTag
{
	ULONGLONG VsnapTime;
	ULONG	DataFileId;
	ULONG	WriteFileId;
	ULONG	WriteFileSuffix;
    ULONG   ulBackwardFileId;
    ULONG   ulForwardFileId;
};

typedef struct VsnapPersistentHeaderInfoTag VsnapPersistentHeaderInfo;


typedef struct _ADD_RETENTION_IOCTL_DATA {
    USHORT UniqueVolumeIdLength;
    char  UniqueVolumeID[1022];
    char  VolumeInfo[1];
} ADD_RETENTION_IOCTL_DATA, *PADD_RETENTION_IOCTL_DATA;


#endif // _INMAGE_VV_COMMON_H_


