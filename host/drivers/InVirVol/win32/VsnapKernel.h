//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapKernel.h
//
// Description: Data Structures used in kernel mode by vsnap module.
//

#ifndef _VSNAP_KERNEL_H
#define _VSNAP_KERNEL_H
#include "VVDriverContext.h"
#if defined(VSNAP_WIN_KERNEL_MODE)
#include "VVDebugDefines.h"
#include "WinVsnapKernelHelpers.h"
#include "VVDevControl.h"
#include <VVCommon.h>
#include "invirvollog.h"
#include "VVDispatch.h"
#elif defined(VSNAP_UNIX_KERNEL_MODE)
#include "UnixVsnapKernelHelpers.h"
#endif

#include "DiskBtreeHelpers.h"
#include "DiskBtreeCore.h"
#include "VsnapCommon.h"

/* Maximum data file size for private vsnap data. When it exceeds this value, 
 * a new data file will be created.
 */

#define VSNAP_MAX_DATA_FILE_SIZE			0x1F400000
#define FLAG_FREE_VSNAP                     0x10000000
//#define VSNAP_MAX_DATA_FILE_SIZE			0x4000000

/* Retention data filenames don't exceed more than 50 bytes.
 */

#define ULONG_MAX     0xffffffffUL

/* This structure holds information about offset/length of the parent volume(target) that is undergoing
 * a change. From dataprotection, this info is received by invirvol driver through update ioctl. Or if parent
 * itself is a virtual volume(nested vsnaps), then invirvol directly gets these changes through Write API.
 * 
 * Members:
 *		LinkNext			- Head for the list of offsets that are undergoing a change on the parent volume.
 *		hWaitLockMutex		- Mutex Lock for read threads to pend on this lock.
 *		Offset, Length		- Offset/Length that needs to be protected.
 *					
 */
struct VsnapOffsetLockInfoTag
{
	LIST_ENTRY	LinkNext;
	VsnapMutex	hWaitLockMutex;
	ULONGLONG	Offset;
	ULONG		Length;
};
typedef struct VsnapOffsetLockInfoTag VsnapOffsetLockInfo;

/* This is a singly linked list of Target Volumes. Target Volume specific information is stored here.
 * Each structure in turn contains linked list of VsnapInfo structure associated with the same target volume.
 *
 * Members:
 *		LinkNext			- Pointer to next target volume in the list.
 *		VolumeName			- Pointer to Target Volume Name.
 *		RetentionDirectory	- Directory where retention data files are stored.
 *		OffsetListLock		- Read-Write Lock to protect the Offset List
 *		OffsetListHead		- Head for the offset list.
 *		VsnapHead			- Head pointer to the first vsnap mounted for this target volume.
 */
struct VsnapParentVolumeListTag
{
	LIST_ENTRY	LinkNext;
	PCHAR		VolumeName;
	DWORD		VolumeSectorSize;
	PCHAR		RetentionDirectory;
	VsnapRWLock	*OffsetListLock;
    VsnapRWLock  *TargetRWLock;
	LIST_ENTRY	OffsetListHead;
    LIST_ENTRY	VsnapHead;
    PVOID       SparseFileHandle;
    bool        IsFile;
    bool        IsMultiPartSparseFile;
    ULONGLONG   ChunkSize;
    PVOID       *MultiPartSparseFileHandle;
    ULONG       SparseFileCount;
};

typedef struct _FILE_HANDLE_CACHE_ENTRY
{
    ULONG   ulFileId;
    PVOID   pWinHdlTag;
    ULONG   ulRefCount;
    ULONG   ulUseCount;
}FILE_HANDLE_CACHE_ENTRY,*PFILE_HANDLE_CACHE_ENTRY;

typedef struct _FILE_HANDLE_CACHE
{
    // Performance Counters for cache
    ULONGLONG     ullCacheMiss;
    ULONGLONG     ullCacheHit;
    ULONG         ulMaxRetFileHandles;
    ULONG         ulNoOfEntriesInCache;
    
    VsnapRWLock	  *FileHandleCacheLock;
    
    //ULONG  ulLRUIndex;
    PFILE_HANDLE_CACHE_ENTRY    CacheEntries;
}FILE_HANDLE_CACHE,*PFILE_HANDLE_CACHE;


typedef struct _FILE_NAME_CACHE
{
    
    ULONG                     ulMaxEntriesInCache;//maximum entries cache can hold
    ULONG                     ulNoOfEntriesInCache;//currently tells about the number of entries in Cache
    ULONGLONG                 ullCurrentTS;//current hour time stamp this is a 12 char number. first two bytes for year, next two for month, next two for date, next four for tags
    ULONGLONG                 ullCreationTS;//start time when vsnap was created. same as ullCurrentTS
    ULONG                     ulBackwardFileId;//how many entries to read from the start of file id table at the time of mounting Vsanp
    ULONG                     ulForwardFileId;//How many files to read from the end. This applies at remount case
    ULONGLONG                 *TSHash; //cache to hold hash
    ULONG                     *FileId;//cache to hold fileid
    

}FILE_NAME_CACHE,*PFILE_NAME_CACHE;




typedef struct VsnapParentVolumeListTag VsnapParentVolumeList;

/* This structure holds Vsnap specific information, some are passed from user program and some others
 * are used for internal purpose. One Vsnap structure per virtual volume is created.
 *
 * Members:
 *		LinkNext			 - Pointer to next VsnapInfo structure for the same target volume.
 *		PrivateDataDirectory - Valid for writeable vsnaps. Points to directory where private data files for this vsnap 
 *							   are stored.
 *		BtreeMapLock		 - Mutex to provide synchronization to Btree Map File.
 *		VsRemoveLock		 - This lock avoids this structure getting freed through unmount path while other
 *							   are accessing this structure.
 *		BtreeMapFileHandle	 - Cached Btree map file's handle.
 *		RootNodeOffset		 - Cached Root Node Offset of the btree map.
 *		FileIdTableHandle	 - Cached FileIdTable's handle. This file contains the translation between FileId and File Name.
 *		DataFileId			 - Next/Current Data File Id to be used for newer data files.	
 *		DataFileIdUpdated	 - This boolean tells whether File Name associated with DataFileId member in this
 *							   structure has been updated in FileIdTable file or not.
 *		SnapShotId			 - Unique Id associated with this vsnap.
 *		TargetPtr			 - Back Pointer to Target Volume Structure.
 *		LastDataFileName	 - File Name of the most recent data file in the retention log.
 *		RecoveryTime		 - Recovery Time associated with this Vsnap.
 */
struct VsnapInfoTag 
{
	LIST_ENTRY					LinkNext;
	PCHAR						VolumeName;
	PCHAR						PrivateDataDirectory;
	VsnapRWLock					*BtreeMapLock;
	REMOVE_LOCK					VsRemoveLock;
	void						*BtreeMapFileHandle;
	ULONGLONG					RootNodeOffset;
	void						*FileIdTableHandle;
	ULONG						DataFileId;
	int							DataFileIdUpdated;
	ULONG						WriteFileId;
	int							WriteFileSuffix;
	ULONGLONG					SnapShotId;
	VsnapParentVolumeList		*ParentPtr;
    PFILE_NAME_CACHE             FileNameCache;
	PCHAR						LastDataFileName;
	LARGE_INTEGER	    		RecoverySystemTime;
    VSNAP_VOLUME_ACCESS_TYPE	AccessType;
	bool						IsMapFileInRetentionDirectory;
	bool						IsTrackingEnabled;
	ULONG						TrackingId;
	void						*WriteFileHandle;
	void						*FSDataHandle;
    PFILE_HANDLE_CACHE          FileHandleCache;
    DiskBtree                   *VsnapBtree;
    LONG                        IOsInFlight;
    LONG                        IOPending;
    bool                        IOWriteInProgress;
    ULONG                       ulFlags;
    PDEVICE_EXTENSION           DevExtension;
    FAST_MUTEX                  hMutex;
    KEVENT                      KillThreadEvent;
    PKTHREAD                    UpdateThread;
    ULONGLONG                   ullFileSize;
#if DBG
    ULONGLONG                   ullNumberOfReads;
    ULONGLONG                   ullNumberOfWrites;
    ULONGLONG                   ullNumberOfUpdates;
#endif
    bool                        IsSparseRetentionEnabled;
};
typedef struct VsnapInfoTag VsnapInfo;

typedef enum _etSearchResult 	
{
    ecStatusNotFound, ecStatusNotFoundInCache, ecStatusFound 
}etSearchResult, *petSearchResult;

bool VsnapVolumesUpdateMapForParentVolume(ULONGLONG, ULONG, ULONGLONG, char *, char *, ULONG);
int VsnapAllocAndInsertOffsetSplitInList(PSINGLE_LIST_ENTRY, ULONGLONG, ULONG,ULONG, ULONGLONG, ULONG, SINGLE_LIST_ENTRY *node=NULL,void *Ptr = NULL);
bool VsnapAddFileIdToTheTable(VsnapInfo *, VsnapFileIdTable *, bool IsWriteFile = false);
inline bool VsnapDoesFileIdPointToRetentionLog(ULONG);
int VsnapAlignedRead(PCHAR , ULONGLONG, ULONG, DWORD *, DWORD, void *);
bool VsnapReadFromVolume(char *, DWORD, PCHAR, ULONGLONG, ULONG, ULONG *);
void VsnapUpdateMap(VsnapInfo *, ULONGLONG, ULONG, ULONG , ULONGLONG, PCHAR, ULONG);
bool VsnapAllocAndInsertParent(char *, VsnapInfo *, char *, ULONGLONG, char *, MultiPartSparseFileInfo *);
VsnapParentVolumeList *VsnapFindParentFromVolumeName(char *);
VsnapOffsetLockInfo *VsnapFindMatchingOffsetFromList(ULONGLONG, ULONG, VsnapInfo *);
bool VsnapAddVolumeToList(char *, VsnapInfo *, char *, ULONGLONG, char *, MultiPartSparseFileInfo *);
int VsnapGetFileNameFromFileId(VsnapInfo *, LONG, PCHAR, bool EnableLog = false, NTSTATUS *Status = STATUS_SUCCESS);
bool VsnapOpenAndReadDataFromLog(void *, ULONGLONG, ULONGLONG, ULONG, ULONG, VsnapInfo *);
int VsnapReadDataUsingMap(void *, ULONGLONG, ULONG, ULONG *, VsnapInfo *, PLIST_ENTRY, bool *);
void  SetNextWriteFileId(VsnapInfo *);
void SetNextDataFileId(VsnapInfo * , bool );
bool VsnapWrite(char *, ULONGLONG &, void *, int, ULONG *, ULONG &,	VsnapInfo *);
bool VsnapWriteToPrivateLog(void *, ULONGLONG & , ULONG, ULONG &, VsnapInfo *,ULONG *);
void AddKeyToTrackingMapFile(VsnapInfo *, VsnapKeyData *);
int VsnapWriteDataUsingMap(void *, ULONGLONG, ULONG, ULONG *, VsnapInfo *, bool);
bool VsnapAllocAndInsertOffsetLock(VsnapParentVolumeList *, ULONGLONG, ULONG, VsnapOffsetLockInfo **);
void VsnapReleaseAndRemoveOffsetLock(VsnapParentVolumeList *, VsnapOffsetLockInfo *);
bool VsnapVolumesUpdateMapForParentVolume(UPDATE_VSNAP_VOLUME_INPUT *);
int VsnapVolumeRead(void *, ULONGLONG, ULONG, ULONG *, PVOID, PSINGLE_LIST_ENTRY, bool *);
int VsnapVolumeWrite(void *, ULONGLONG, ULONG, ULONG *, PVOID);
NTSTATUS VsnapMount(void *, int,	LONGLONG *, PDEVICE_EXTENSION, MultiPartSparseFileInfo*);
bool VsnapUnmount(PVOID);
void CleanupFileHandleCache(VsnapInfo *Vsnap);
bool VsnapVolumesUpdateMaps(PVOID,PIRP,bool*);
bool VsnapSetOrResetTracking(PVOID, bool, int*);
ULONG VsnapGetVolumeInformationLength(PVOID);
VOID VsnapGetVolumeInformation(PVOID, PVOID, ULONG);
NTSTATUS VsnapInitBtreeFromHandle(void *, DiskBtree **);
void VsnapUninitBtree(DiskBtree*);
NTSTATUS VsnapInitFileIds(VsnapInfo *Vsnap);
void UpdateVsnapPersistentHdr(VsnapInfo *, bool);
NTSTATUS ValidateMountInfo( VsnapMountInfo *MountData);
NTSTATUS ValidateUpdateInfo( UPDATE_VSNAP_VOLUME_INPUT *UpdateInfo);
int IsStringNULLTerminated(PCHAR String , int len);
void TrimLastBackSlashChar(PCHAR String, SIZE_T len);
bool InitCache(VsnapInfo *Vsnap);
void DestroyCache(VsnapInfo *Vsnap);

bool ReadAndFillCache(VsnapInfo *Vsnap);
bool ReadFileIdTable(VsnapInfo *Vsnap, char (*FileName)[MAX_RETENTION_DATA_FILENAME_LENGTH], ULONGLONG  Offset, ULONG  EntriesToRead, ULONG StartFileId);
//bool SearchInFileIdTable(VsnapInfo *Vsnap,char *str, ULONG *ulFileid);
etSearchResult SearchIntoCache(VsnapInfo *Vsnap, ULONGLONG  TimeStamp, ULONG *fid, ULONG *Index);
void InsertIntoCache(VsnapInfo *Vsnap, ULONGLONG TimeStamp, ULONG  fid, ULONG Index);
ULONGLONG StrToInt(char *str,int count);
ULONGLONG  ConvertStringToULLHash(char *str);
ULONGLONG  ConvertStringToULLETS(char *str, bool WithBookMark);
ULONGLONG  ConvertStringToULLSTS(char *str, bool WithBookMark);
void CloseAndFreeMultiSparseFileHandle(ULONG, void**);

extern bool VsnapQueueUpdateTask(struct _UPDATE_VSNAP_VOLUME_INPUT *UpdateInfo, PIRP   Irp,bool *IsIOComplete);

extern void VsnapCallBack(void *, void *, void *, bool);
extern void InitializeEntryList(PSINGLE_LIST_ENTRY);
extern bool IsSingleListEmpty(PSINGLE_LIST_ENTRY);
extern PSINGLE_LIST_ENTRY InPopEntryList(PSINGLE_LIST_ENTRY);
extern void InPushEntryList(PSINGLE_LIST_ENTRY, PSINGLE_LIST_ENTRY);
extern bool operator<(VsnapKeyData &, VsnapKeyData &);
extern bool operator>(VsnapKeyData &, VsnapKeyData &);
extern bool operator==(VsnapKeyData &, VsnapKeyData &);
#if (NTDDI_VERSION >= NTDDI_WS03)
NTSTATUS VVImpersonate(PIMPERSONATION_CONTEXT pImpersonationContext, PETHREAD pThreadToImpersonate);
NTSTATUS VVCreateClientSecurity(PIMPERSONATION_CONTEXT pImpersonationContext, PETHREAD pThread);
#endif
#endif