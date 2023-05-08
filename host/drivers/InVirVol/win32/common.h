//#include "ListEntry.h"

struct MountRetentionTag {
	char LogDir[2048];
	char TargetVolume[4];
	ULONGLONG SnapShotId;
	ULONG RootNodeOffset;
	ULONGLONG VolumeSize;
	DWORD VolumeSectorSize;
	LONG NextFileId;
	ULONGLONG RecoveryTime;
};

typedef struct MountRetentionTag RetentionMountIoctlData;

struct TargetVolumeListTag
{
	LIST_ENTRY LinkNext;
	char TargetVolume[3];
	char LogDir[1024];
	KMUTEX hTgtMutex;
	KMUTEX hTgtLockMutex;
	LONGLONG Offset;
	LONG Length;
	LIST_ENTRY VsnapHead;
};

typedef struct TargetVolumeListTag TargetVolumeList;

struct VirtualSnapShotInfoTag {
	LIST_ENTRY LinkNext;
	KMUTEX BtreeMapLock;
	IO_REMOVE_LOCK VsRemoveLock;
	HANDLE BtreeMapFileHandle;
	unsigned long RootNodeOffset;
	HANDLE FileIdTableHandle;
	int NoOfDataFiles;
	LONG DataFileId;
	int DataFileIdUpdated;
	DWORD VolumeSectorSize;
	LONGLONG SnapShotId;
	TargetVolumeList *TgtPtr;
	char LastDataFileName[40];
	ULONGLONG RecoveryTime;
};

typedef struct VirtualSnapShotInfoTag VirtualSnapShotInfo;

struct VsnapContextInfoTag {
	LONGLONG SnapShotId;
	char TargetVolume;
	ULONGLONG RecoveryTime;
};

typedef struct VsnapContextInfoTag VsnapContextInfo;

struct MapAttributeTag {
	SINGLE_LIST_ENTRY LinkNext;
	LONGLONG Offset;
	LONGLONG FileOffset;
	LONGLONG Length;
	int FileId;
};
typedef struct MapAttributeTag MapAttribute;

#pragma pack(push, 1)
struct KeyAndDataTag
{
	LONGLONG Offset;
	LONGLONG FileId;
	LONGLONG FileOffset;
	LONGLONG Length;
};
typedef struct KeyAndDataTag KeyAndData;

struct FileIdTableTag
{
	int FileId;
	char FileName[50];
};
typedef struct FileIdTableTag FileIdTable;

struct BtreeMatchEntryTag
{
	LONGLONG Offset;
	unsigned long FileId;
	unsigned long Length;
	unsigned long DataFileOffset;
};

typedef struct BtreeMatchEntryTag BtreeMatchEntry;
#pragma pack (pop, 1)