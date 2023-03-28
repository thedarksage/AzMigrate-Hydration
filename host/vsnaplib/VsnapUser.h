//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapUser.h
//
// Description: Vsnap User library header file.
//

#ifndef VSNAP_USER_H
#define VSNAP_USER_H

#define VSNAP_SUFFIX "in"

#if defined(VSNAP_USER_MODE)
#include<vector>
#endif

#include "VsnapCommon.h"
#include "DiskBtreeCore.h"

#ifdef SV_WINDOWS
#include "drvstatus.h"
#endif

#include "svdparse.h"
#include "localconfigurator.h"
#include "configurator2.h"
#include "configurevxagent.h"
#include <ace/Process_Manager.h>
#include "volinfocollector.h"
#ifndef SV_WINDOWS
#define MAX_STRING_SIZE    128
#endif


#include "cdpdatabase.h"

enum VSNAP_PROGRESS_STATUS	{ VSNAP_READY_STATUS = 1 , MAP_START_STATUS, MAP_PROGRESS_STATUS, VSNAP_COMPLETE_STATUS, 
VSNAP_FAILED_STATUS, MOUNT_START, MOUNT_PROGRESS, PRE_SCRIPT_STARTS,PRE_SCRIPT_ENDS,POST_SCRIPT_STARTS,POST_SCRIPT_ENDS,VSNAP_UNINIT_STATE} ;
enum VSNAP_OP_STATE			{ VSNAP_MOUNT = 1, VSNAP_UNMOUNT, VSNAP_REMOUNT, VSNAP_UNMOUNT_PRUNING, VSNAP_UNKNOWN } ;


enum VSNAP_FS_TYPE			{ VSNAP_FS_FAT = 0,VSNAP_FS_NTFS = 1, VSNAP_FS_ReFS, VSNAP_FS_EXFAT, VSNAP_FS_UNKNOWN };
//The enum defined above was altered to match the one defined in the portablehelpersmajor.cpp to simiplify the logic. 


enum VSNAP_SOURCE			{ VSNAP_ALL = 1, VSNAP_TARGET = 2, VSNAP_SELF = 3 };

#define UNMOUNT_SUCCESS 0
#define UNMOUNT_FAILED 1

#define VSNAP_UNIQUE_ID_PREFIX			"VS_"
#define VSNAP_MNTPT_OFFSET_OF_UNIQUE_ID	3

#define VSNAP_DEFAULT_SECTOR_SIZE		512 //Default Volume Sector Size to be used.
#define VSNAP_FS_DATA_SIZE				512 //Default FS boot sector size.
#define VSNAP_NTFS_BOOTSECTOR_SIZE		512 //First 512 bytes of the filesystem contains filesystem signature.
#define VSNAP_FAT_BOOTSECTOR_SIZE		512 //First 512 bytes of the filesystem contains filesystem signature.

//Vsnap Progress Codes
#define VSNAP_DONT_SEND_PROGRESS		0x0
#define VSNAP_SEND_PROGRESS				0x1
#define VSNAP_MAP_GENERATION_STARTED	0x2
#define VSNAP_MAP_GENERATION_INPROGRESS	0x3
#define VSNAP_MAP_GENERATION_COMPLETE	0x4
#define VSNAP_MAP_GENERATION_FAILED		0x5
#define VSNAP_MOUNT_OPERATION_FAILED	0x6
#define VSNAP_MOUNT_SUCCEEDED			0x7

//Vsnap Exit Codes
#define VSNAP_SUCCESS					0x0
#define VSNAP_UNKNOWN_FS_FOR_TARGET		0x1
#define VSNAP_TARGET_HANDLE_OPEN_FAILED	0x2
#define VSNAP_TARGET_VOLUMESIZE_UNKNOWN	0x3
#define VSNAP_READ_TARGET_FAILED		0x4
#define VSNAP_DB_INIT_FAILED			0x5
#define VSNAP_DB_PARSE_FAILED			0x6
#define VSNAP_MAP_INIT_FAILED			0x7
#define VSNAP_MAP_OP_FAILED				0x8
#define VSNAP_FILETBL_OP_FAILED			0x9
#define VSNAP_MEM_UNAVAILABLE			0x10
#define VSNAP_VIRVOL_OPEN_FAILED		0x11
#define VSNAP_MOUNT_IOCTL_FAILED		0x12
#define VSNAP_MOUNT_FAILED				0x13
#define VSNAP_UNMOUNT_FAILED			0x14
#define VSNAP_DRIVER_LOAD_FAILED		0x15
#define VSNAP_DRIVER_UNLOAD_FAILED		0x16
#define VSNAP_DRIVER_INSTALL_FAILED		0x17
#define VSNAP_INVALID_MOUNT_POINT		0x18
#define VSNAP_VOLUME_INFO_FAILED		0x19
#define VSNAP_MOUNTDIR_CREATION_FAILED	0x20
#define VSNAP_MOUNTPT_NOT_DIRECTORY		0x21
#define VSNAP_DIRECTORY_NOT_EMPTY		0x22
#define VSNAP_REPARSE_PT_NOT_SUPPORTED	0x23
#define VSNAP_MOUNTDIR_STAT_FAILED		0x24
#define VSNAP_INVALID_SOURCE_VOLUME		0x25
#define VSNAP_GET_SYMLINK_FAILED		0x26
#define VSNAP_OPEN_HANDLES				0x28
#define VSNAP_UNSUPPORTED_DB			0x29

#define VSNAP_TARGET_NOT_SPECIFIED		0x30
#define VSNAP_VIRTUAL_NOT_SPECIFIED		0x31
#define VSNAP_VIRTUAL_DRIVELETTER_INUSE	0x32
#define VSNAP_RECOVERYPOINT_CONFLICT	0x33
#define VSNAP_DB_UNKNOWN				0x34
#define VSNAP_PRIVATE_DIR_NOT_SPECIFIED 0x35
#define VSNAP_GET_DB_VERSION_FAILED		0x36
#define VSNAP_APPNAMETOTYPE_FAILED		0x37
#define VSNAP_GETMARKER_FAILED			0x38
#define VSNAP_MATCHING_EVENT_NOT_FOUND	0x39
#define VSNAP_MATHING_EVENT_NUMBER_NOT_FOUND 0x40
#define VSNAP_GET_VOLUME_INFO_FAILED	0x41
#define VSNAP_INVALID_MOUNT_PT			0x42
#define VSNAP_TRACK_IOCTL_FAILED		0x43
#define VSNAP_TRACK_IOCTL_ERROR			0x44
#define VSNAP_MAP_FILE_NOT_FOUND		0x45
#define VSNAP_GETVOLUMENAME_API_FAILED	0x46
#define VSNAP_DELETEMNTPT_API_FAILED	0x47
#define VSNAP_TARGET_OPEN_FAILED		0x48
#define VSNAP_SETMNTPT_API_FAILED		0x49
#define VSNAP_TARGET_OPENEX_FAILED		0x50
#define VSNAP_ID_VIRTUALINFO_UNKNOWN	0x51
#define VSNAP_ID_VOLNAME_CONFLICT		0x52
#define VSNAP_ID_TGTVOLNAME_EMPTY		0x53
#define VSNAP_MAP_LOCATION_UNKNOWN		0x54
#define VSNAP_PRESCRIPT_EXEC_FAILED		0x55
#define VSNAP_POSTSCRIPT_EXEC_FAILED	0x56
#define VSNAP_DATADIR_CREATION_FAILED	0x57
#define VSNAP_INVALID_VIRTUAL_VOLUME	0x58
#define VSNAP_DELETE_LOG_FAILED			0x59
#define VSNAP_TARGET_UNHIDDEN_RW		0x60
#define VSNAP_PIT_INIT_FAILED			0x61
#define L_VSNAP_MOUNT_FAILED			0x62
#define VSNAP_UNMOUNT_FAILED_FS_PRESENT         0x63
#define VSNAP_MKDIR_FAILED		        0x64
#define VSNAP_LINUX_FS_MOUNT_FAILED		0x65
#define  VSNAP_MAJOR_NUMBER_FAILED      0x66	
#define  VSNAP_MKNOD_FAILED		        0x67
#define  VSNAP_LINUX_FSMOUNT_FAILED 	0x68
#define VSANP_NOTBLOCKDEVICE_FAILED	0x69
#define VSANP_MAJOR_MINOR_CONFLICT		0x70
#define VSNAP_RMDIR_FAILED			0x71
#define VSNAP_TARGET_SECTORSIZE_UNKNOWN	0x72 

// Bug #5527 - Added new error code for vsnap log deletion
#define VSNAP_DELETE_LOGFILES_FAILED	0x73
// End of change
//Added new error code for virtual volume supplied  not exist
#define VSNAP_VIRTUAL_VOLUME_NOTEXIST 	0x74
#define DATALOG_VOLNAME_CONFLICT 		0x75
#define VSNAP_UNIQUE_ID_FAILED			0x76

//Bug #6133

#define VSNAP_NOT_A_TARGET_VOLUME		0x77
#define VSNAP_TARGET_UNHIDDEN			0x78
#define VSNAP_TARGET_RESYNC				0x79
#define VSNAP_TARGET_HIDDEN				0x80
#define VSNAP_CONTAINS_MOUNTED_VOLUMES	0x81

//Bug: 4147
#define VSNAP_DIR_SAME_AS_PWD			0x82

#define VSNAP_VOLNAME_SAME_AS_DATALOG	0x83
#define VSNAP_OPEN_LINVSNAP_BLOCK_DEV_FAILED 0x84
#define VSNAP_CLOSE_LINVSNAP_BLOCK_DEV_FAILED 0x85
#define VSNAP_ZPOOL_DESTROY_EXPORT_FAILED 0x86

#define VSANP_NOTCHARDEVICE_FAILED 0x87
#define VSANP_MKDEV_FAILED 0x88
#define VSANP_RMDEV_FAILED 0x89
#define VSANP_RELDEVNO_FAILED 0x90
#define VSNAP_TARGET_SPARSEFILE_CHUNK_SIZE_FAILED 0x91
#define VSNAP_ACQUIRE_RECOVERY_LOCK_FAILED 0x92

typedef dev_t sv_dev_t;

struct VsnapErrorInfoTag
{
    int		VsnapErrorStatus;
    SV_ULONG	VsnapErrorCode;
    std::string VsnapErrorMessage;
};

typedef struct VsnapErrorInfoTag VsnapErrorInfo;

// This structure holds information specific to a virtual volume.
struct VsnapVirtualVolInfoTag
{
    std::string					VolumeName;
    std::string					DeviceName;
    std::string					PrivateDataDirectory;
    VSNAP_VOLUME_ACCESS_TYPE	AccessMode;
    VSNAP_CREATION_TYPE			VsnapType;
    SV_ULONGLONG				TimeToRecover;
    std::string					AppName;
    std::string					EventName;
    int							EventNumber;
    std::string					PreScript;
    std::string					PostScript;
    bool						ReuseDriveLetter;
	bool						MountVsnap;

    SV_ULONGLONG				SnapShotId;
    int							NextFileId;
    VSNAP_PROGRESS_STATUS		VsnapProgress;
    VsnapErrorInfo				Error;
    VSNAP_OP_STATE				State;
    int							PreScriptExitCode;
    int							PostScriptExitCode;
    SV_ULONGLONG                SequenceId;
    bool						NoFail;
	std::string					VolumeGuid;
    VsnapVirtualVolInfoTag();
};

typedef struct VsnapVirtualVolInfoTag VsnapVirtualVolInfo;
typedef std::vector<VsnapVirtualVolInfo*> VsnapVirtualInfoList;

// This structure holds information specific to the source volume of vsnap. The source should be either
// replicated target volume(locked) or a virtual volume. Any other volume is not a candidate for taking
// virtual snapshots.

struct VsnapSourceVolInfoTag
{
    std::string				vsnap_id;
    std::string				VolumeName;
    std::string				RetentionDbPath;
    VsnapVirtualInfoList	VirtualVolumeList;

    SV_ULONG				SectorSize;
    SV_ULONGLONG            ActualSize;
    SV_ULONGLONG			UserVisibleSize;
    char					*FsData;
    VSNAP_FS_TYPE			FsType;
    bool skiptargetchecks;
    VsnapSourceVolInfoTag();
};

typedef struct VsnapSourceVolInfoTag VsnapSourceVolInfo;
typedef std::vector<VsnapSourceVolInfo*> VsnapSourceInfoList;

typedef VsnapSourceInfoList::iterator VsnapSrcIter;
typedef VsnapVirtualInfoList::iterator VsnapVirtualIter;

struct VsnapPairInfoTag
{
    VsnapSourceVolInfo	*SrcInfo;
    VsnapVirtualVolInfo	*VirtualInfo;
};

typedef struct VsnapPairInfoTag VsnapPairInfo;


// New structures for storing persistent information
// about vsnaps which will be stored/retrieved from
// files

// Bug #8543


#define VSNAP_VOLUME_LENGTH 2048
#define VSNAP_TARGET_VOLUME_LENGTH 2048

#define VSNAP_VER_HDR_SIZE 128
#define VSNAP_TAG_LENGTH 512

#define VSNAP_BYTEMASK(ch)    ((ch) & (0xFF))
#define VSNAP_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
    (VSNAP_BYTEMASK(ch0) | (VSNAP_BYTEMASK(ch1) << 8) |   \
    (VSNAP_BYTEMASK(ch2) << 16) | (VSNAP_BYTEMASK(ch3) << 24 ))

#define VSNAP_MAGIC VSNAP_MAKEFOURCC( 'V', 'S', 'N', 'P' )
#define VSNAP_VERSION(major,minor)     (((major) << 8) + (minor))

#define VSNAP_MAJOR_VERSION 1
#define VSNAP_MINOR_VERSION 0

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack(1)
#else
#pragma pack( push, 1 )
#endif /* SV_SUN || SV_AIX */

struct VsnapHeaderInfo
{
    union
    {
        struct  
        {
            SV_UINT magic;
            SV_UINT version;
        }Hdr;
        unsigned char rsvdbytes[VSNAP_VER_HDR_SIZE];
    }uHdr;
};



struct DiskVsnapPersistInfo
{
    SV_ULONGLONG volumesize;
    SV_ULONGLONG recoverytime;
    SV_ULONGLONG snapshot_id;
    SV_UINT sectorsize;
    SV_UINT accesstype;
    SV_UINT mapfileinretention;
    SV_UINT vsnapType;
    SV_UINT mtpt_len;
    SV_UINT dev_len;
    SV_UINT tgt_len;
    SV_UINT tag_len;
    SV_UINT datalog_len;
    SV_UINT ret_len;
    SV_UINT time_len;	
};

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack()
#else
#pragma pack( pop )
#endif /* SV_SUN || SV_AIX */

// Virtual Snapshot Class.
class VsnapMgr
{
public:
    VsnapMgr();
    virtual long OpenInVirVolDriver(ACE_HANDLE &, VsnapErrorInfo*)=0;
    bool StartTracking(VsnapVirtualVolInfo *);
    bool StopTracking(VsnapVirtualVolInfo *);
    virtual void GetAllVirtualVolumes(std::string & Volumes)= 0;

    //added the second argument for fixing the bug #5569.
    //It contains the vsnap device names of vsnaps which failed to unmount.
    void UnmountAllVirtualVolumes(std::vector<VsnapPersistInfo> & passedvsnaps, std::vector<VsnapPersistInfo> & failedvsnaps, bool deletemapfiles = true,bool bypassdriver = false);

    void Mount(VsnapSourceVolInfo *);
    //Bug #4044
    virtual bool Unmount(VsnapVirtualInfoList &, bool deletelogs, bool SendStatus, std::string& output, std::string& error,bool bypassdriver = false  ) = 0;
    virtual bool Unmount(VsnapVirtualInfoList &, bool, bool SendStatus = true,bool bypassdriver = false ) = 0;
    bool ApplyTrackLogs(VsnapSourceVolInfo *);
    virtual void StartTick(SV_ULONG &) = 0;
    virtual void EndTick(SV_ULONG) = 0;
    void Remount(VsnapSourceVolInfo *);
    void SetServerAddress(const char *);
    void SetServerId(const char *);
    void SetServerPort(int);
    void VsnapGetErrorMsg(const VsnapErrorInfo &, std::string &);
    void SetSnapshotObj(void *);
    void DeleteLogs(VsnapSourceVolInfo *);
    void Sync(VsnapVirtualVolInfo *);
    void SetCXSettings();
    OS_VAL getSourceOSVal(const std::string & deviceName);
    bool isSourceFullDevice(const std::string & deviceName);
	bool getCdpDbName(const std::string& input, std::string& dbpath);
    bool updateBookMarkLockStatus(const std::string &dbname, const std::string &bkmkname,SV_USHORT oldstatus, SV_USHORT newstatus);
    SV_ULONG getOtherSiteCompartibilityNumber(const std::string & deviceName);
    virtual bool fill_fsdata_from_retention(const std::string & fileName, SV_ULONGLONG fileoffset, VsnapPairInfo *PairInfo) = 0;

    bool isTargetVolume(const std::string & volumename)const;
    bool isResyncing(const std::string & volumename)const;
    std::string getFSType(const std::string & volumename) const; // Bug# 7227

    bool containsMountedVolumes(const std::string & volumename, std::string & mountedvolume) const;

    virtual	ACE_HANDLE OpenVirtualVolumeControlDevice() = 0;
    bool UnMountVsnapVolumesForTargetVolume(std::string, bool nofail = false);
    virtual	void VsnapGetVirtualInfoDetaisFromVolume(VsnapVirtualVolInfo *, VsnapContextInfo*)=0;
    //bug# 7934
    bool VsnapQuit(int waitSeconds = 0);
    //Bug #8543
    bool PersistVsnap(VsnapMountInfo* MountData, VsnapVirtualVolInfo* VirtualInfo);
    bool PersistPendingVsnap(const std::string & deviceName, const std::string & target);
    bool RetrieveVsnapInfo(std::vector<VsnapPersistInfo>& vsnaps, const std::string& match, const std::string& tgtvol = "", const std::string& vsnap = "");
    bool RetrievePendingVsnapInfo(std::vector<VsnapPersistInfo>& createdvsnaps, std::vector<VsnapDeleteInfo>& deletedVsnap,SV_UINT nBatchRequest=256);
    bool deletePendingPersistentVsnap(const std::string& targetvol);
    void PrintVsnapInfo(std::ostringstream& out, const VsnapPersistInfo &vsnapinfo );
    bool RemountVsnapsFromPersistentStore();
    bool RemountVsnapsFromCX();
    virtual	bool UnmountVirtualVolume(char*, const size_t, VsnapVirtualVolInfo *, std::string& output, std::string& error,bool bypassdriver = false ) = 0;

    bool IsConfigAvailable ;	// Ecase 2934
    bool notifyCx; //to control error logging

protected:
    virtual void PerformMountOrRemount(VsnapSourceVolInfo *) = 0;
    virtual bool RemountV2(const VsnapPersistInfo& remountInfo) = 0;

    virtual bool MountVol(VsnapVirtualVolInfo*, VsnapMountInfo *, char*,bool&) = 0;
    virtual bool StatAndMount(const VsnapPersistInfo& remountInfo) = 0;
    virtual bool StatAndWait(const std::string &device) = 0;
    virtual bool PerformZpoolDestroyIfReq(const std::string &devicefile, bool bshoulddeletevsnaplogs) = 0;
    virtual bool RetryZpoolDestroyIfReq(const std::string &device, const std::string &target, bool bshoulddeletevsnaplogs) = 0;
    virtual bool CloseLinvsnapSyncDev(const std::string &device, int &fd) = 0;
    virtual bool OpenLinvsnapSyncDev(const std::string &device, int &fd) = 0;
    virtual bool NeedToRecreateVsnap(const std::string &vsnapdevice) = 0;
    virtual	bool OpenMountPointExclusive(ACE_HANDLE*, char *, const size_t, char *) = 0;
    virtual	bool CloseMountPointExclusive(ACE_HANDLE, char *, char *) = 0;
    //Bug #4044
    virtual	bool UnmountVirtualVolume(char*, const size_t, VsnapVirtualVolInfo *, bool bypassdriver = false) = 0;
    virtual void GetOrUnmountAllVirtualVolumes(std::string & MountPoints, bool) = 0;	
    virtual void ProcessVolumeAndMountPoints(std::string &, char *, char *, int, bool) = 0;
    virtual	bool IsVolumeVirtual(char const * Vol) = 0;

    virtual bool GetVsnapContextFromVolumeLink(ACE_HANDLE, wchar_t* , VsnapVirtualVolInfo*, VsnapContextInfo *) = 0;
    virtual bool UnmountFileSystem(const wchar_t *, VsnapVirtualVolInfo *) = 0;
    virtual void DeleteDrivesForTargetVolume(const char* , VsnapVirtualVolInfo *) = 0;
    virtual bool ValidateSourceVolumeOfVsnapPair(VsnapPairInfo*) = 0;
    virtual bool GetVsnapSourceVolumeDetails(char *,VsnapPairInfo*) = 0;
    virtual bool UpdateMap(DiskBtree *, SV_UINT FileId, SVD_DIRTY_BLOCK_INFO *Block) = 0;
    virtual bool ValidateVirtualVolumeOfVsnapPair(VsnapPairInfo*,bool&) = 0;
    virtual void DeleteAutoAssignedDriveLetter(ACE_HANDLE hDevice, char* MountPoint, bool IsMountPoint) = 0;
    virtual bool SetMountPointForVolume(char* MountPoint, char* VolumeSymLink, char* VolumeName) = 0;
    virtual bool CheckForVirtualVolumeReusability(VsnapPairInfo*,bool&)=0;
    bool DeleteVsnapLogs(VsnapPersistInfo const &);
    void VsnapErrorPrint(VsnapPairInfo*, int SendToCx = 1);
    void TruncateTrailingBackSlash(char *str);
    bool SendUnmountStatus(int, VsnapVirtualVolInfo*, VsnapPersistInfo const &);

    void VsnapPrint(const char *);
    virtual size_t wcstotcs(char* tstring, const size_t tstringlen, const wchar_t* wcstr) = 0;
    virtual void DeleteMountPointsForTargetVolume(char*, char*, VsnapVirtualVolInfo *)=0;
    virtual void DeleteMountPoints(char*, VsnapVirtualVolInfo *)=0;
    bool SendStatus(VsnapPairInfo*, std::string s = "");
#ifdef SV_FABRIC
    void SendDeviceNameToCx(const VsnapPairInfo *PairInfo);
#endif
    bool ValidateMountArgumentsForPair(VsnapPairInfo *);
    int RunPreScript(VsnapVirtualVolInfo *, VsnapSourceVolInfo *);
    int RunPostScript(VsnapVirtualVolInfo *, VsnapSourceVolInfo *);
    int RunPreScript(VsnapVirtualVolInfo *, VsnapSourceVolInfo *, std::string& output, std::string& error);
    int RunPostScript(VsnapVirtualVolInfo *, VsnapSourceVolInfo *, std::string& output, std::string& error);
    void SendProgressToCx(VsnapPairInfo*, const SV_ULONG&, const SV_LONGLONG&);
    unsigned long long GetUniqueVsnapId();
    bool InitializePointInTimeVsnap(std::string, VsnapPairInfo*,SV_ULONGLONG&);
    bool getTimeStampFromLastAppliedFile(const std::string targetDevice,SV_ULONGLONG & timeToRecover);
    bool InitializeRemountVsnap(std::string, VsnapPairInfo*);
    void CleanupFiles(VsnapPairInfo*);
    bool ParseCDPDatabase(std::string &MapFileDir, VsnapPairInfo*);
    bool RecoverPartiallyAppliedChanges(CDPDatabase & db, SV_UINT& cdpversion, unsigned int & FileId, std::string const & MapFileDir,
        VsnapPairInfo * PairInfo, VsnapFileIdTable & FileTable,  DiskBtree* & VsnapBtree,std::vector<std::string> &filenames,
        SV_UINT& backwardFileId, SV_ULONGLONG& prevDrtdTimeStamp, unsigned int& fileIdsWritten);
    virtual	bool IsMountPointValid(char *, VsnapPairInfo*,bool&)=0;
    virtual SV_UCHAR FileDiskUmount(char* VolumeName)=0;
    virtual void DisconnectConsoleOutAndErrorHdl()=0;
    virtual void ConnectConsoleOutAndErrorHdl()=0;
    virtual bool VolumeSupportsReparsePoints(char* VolumeName)=0;
    virtual	size_t mbstotcs(char* tstring, const size_t tstringlen, const char* mbstr) = 0;
    virtual size_t tcstombs(char* mbstr, const size_t mbstrlen, const char* tstring) = 0;
    virtual	bool DirectoryEmpty(char *dir)=0;
    virtual bool TraverseAndApplyLogs(VsnapSourceVolInfo *)=0;
    bool ParseMap(ACE_HANDLE, ACE_HANDLE, VsnapVirtualVolInfo *);
    void VsnapDebugPrint(VsnapPairInfo *, SV_LOG_LEVEL);
    bool PollRetentionData(std::string, SV_ULONGLONG &);
    int WaitForProcess(ACE_Process_Manager *, pid_t);
    ACE_HANDLE	StdOutHdl;
    ACE_HANDLE	StdErrHdl;
    ACE_HANDLE	NulHdl;
    Configurator* m_TheConfigurator; // singleton

    std::string	m_SVServer;

private:
    bool CreateFileIdTableFile(const std::string & MapFileDir, VsnapPairInfo *PairInfo);
    bool updateFileIdTableFile(std::string MapFileDir, VsnapPairInfo *PairInfo, std::vector<std::string>& filenames);
    bool InsertFSData(std::string MapFileDir, VsnapPairInfo*, std::vector<std::string> & filenames, unsigned int& fileIdsWritten);
    bool InsertVsnapWriteData(const std::string & MapFileDir, VsnapPairInfo *PairInfo,
        std::vector<std::string>&filenames,SV_ULONGLONG size, unsigned int& fileIdsWritten);
    bool PopulateLogDirInfo(VsnapMountInfo **MountData, int & size);
    unsigned long GetNextFileId(unsigned long long, const char *);
    void UnmountDrive(char *SnapshotDrive);	
    bool StartOrStopTracking(bool, VsnapVirtualVolInfo *);
    bool HideOrUnhideVirtualVolume(char *, bool);
    int  RunScript(std::string, VsnapVirtualVolInfo *, VsnapSourceVolInfo *);
    int  RunScript(std::string, VsnapVirtualVolInfo *, VsnapSourceVolInfo *, std::string& output, std::string& error);
    bool VsnapUpdatePersistentHdrInfo(DiskBtree *, VsnapPersistentHeaderInfo *);

    //bool ParseMap(ACE_HANDLE, ACE_HANDLE, VsnapVirtualVolInfo *);
    bool ParseNodeAndApply(ACE_HANDLE, void *, VsnapVirtualVolInfo *, DiskBtree *);
    void VsnapDebugPrint(VsnapVirtualVolInfo *, SV_LOG_LEVEL);
    //	int WaitForProcess(ACE_Process_Manager *, pid_t);

    void	*SnapShotObj;
    std::string	m_HostId;
    int		m_HttpPort;
    std::string m_err;




};
class WinVsnapMgr:public VsnapMgr
{
public:

    WinVsnapMgr():VsnapMgr()
    {
    }	
    long OpenInVirVolDriver(ACE_HANDLE &, VsnapErrorInfo*);
    void GetAllVirtualVolumes(std::string & Volumes);

    //added the second argument for fixing the bug #5569

    //Bug #4044
    bool Unmount(VsnapVirtualInfoList &, bool, bool SendStatus = true,bool bypassdriver = false);
    bool Unmount(VsnapVirtualInfoList &, bool deletelogs, bool SendStatus, std::string& output, std::string& error ,bool bypassdriver = false );
    bool fill_fsdata_from_retention(const std::string & fileName, SV_ULONGLONG fileoffset, VsnapPairInfo *PairInfo);
    void StartTick(SV_ULONG &);
    void EndTick(SV_ULONG);
    ACE_HANDLE OpenVirtualVolumeControlDevice();
    void VsnapGetVirtualInfoDetaisFromVolume(VsnapVirtualVolInfo *, VsnapContextInfo*);
    bool UnmountVirtualVolume(char*, const size_t, VsnapVirtualVolInfo *, std::string& output, std::string& error, bool bypassdriver = false);


protected:
    void PerformMountOrRemount(VsnapSourceVolInfo *);
    bool RemountV2(const VsnapPersistInfo& remountInfo);
    bool MountVol(VsnapVirtualVolInfo*, VsnapMountInfo *, char*,bool&);
    bool StatAndMount(const VsnapPersistInfo& remountInfo);
    bool StatAndWait(const std::string &device);
    bool PerformZpoolDestroyIfReq(const std::string &devicefile, bool bshoulddeletevsnaplogs);
    bool CloseLinvsnapSyncDev(const std::string &device, int &fd);
    bool OpenLinvsnapSyncDev(const std::string &device, int &fd);
    bool RetryZpoolDestroyIfReq(const std::string &device, const std::string &target, bool bshoulddeletevsnaplogs);
    bool NeedToRecreateVsnap(const std::string &vsnapdevice);
    bool OpenMountPointExclusive(ACE_HANDLE*, char *, const size_t, char *);
    bool CloseMountPointExclusive(ACE_HANDLE, char *, char *);
    //Bug #4044
    bool UnmountVirtualVolume(char*, const size_t, VsnapVirtualVolInfo *, bool bypassdriver = false);
    void GetOrUnmountAllVirtualVolumes(std::string & MountPoints, bool);	
    void ProcessVolumeAndMountPoints(std::string &, char *, char *, int, bool);
    bool IsVolumeVirtual(char const *  Vol);
    bool GetVsnapContextFromVolumeLink(ACE_HANDLE, wchar_t* , VsnapVirtualVolInfo*, VsnapContextInfo *);
    bool UnmountFileSystem(const wchar_t *, VsnapVirtualVolInfo *);
    void DeleteDrivesForTargetVolume(const char* , VsnapVirtualVolInfo *);
    bool ValidateSourceVolumeOfVsnapPair(VsnapPairInfo*);
    bool GetVsnapSourceVolumeDetails(char *,VsnapPairInfo*);
    bool UpdateMap(DiskBtree *, SV_UINT FileId, SVD_DIRTY_BLOCK_INFO *Block);
    bool ValidateVirtualVolumeOfVsnapPair(VsnapPairInfo*,bool&);
    void DeleteAutoAssignedDriveLetter(ACE_HANDLE hDevice, char* MountPoint, bool IsMountPoint);
    bool SetMountPointForVolume(char* MountPoint, char* VolumeSymLink, char* VolumeName);
    void DeleteMountPointsForTargetVolume(char*, char*, VsnapVirtualVolInfo *);
    SV_UCHAR FileDiskUmount(char* VolumeName);
    void DisconnectConsoleOutAndErrorHdl();
    void ConnectConsoleOutAndErrorHdl();
    bool VolumeSupportsReparsePoints(char* VolumeName);
    void DeleteMountPoints(char*, VsnapVirtualVolInfo *);
    size_t mbstotcs(char* tstring, const size_t tstringlen, const char* mbstr);
    size_t tcstombs(char* mbstr, const size_t mbstrlen, const char* tstring);
    size_t wcstotcs(char* tstring, const size_t tstringlen, const wchar_t* wcstr);
    bool IsMountPointValid(char *, VsnapPairInfo*,bool&);
    bool DirectoryEmpty(char *dir);
    bool CheckForVirtualVolumeReusability(VsnapPairInfo*,bool&);

    bool TraverseAndApplyLogs(VsnapSourceVolInfo *);
	bool UnmountAllMountPointInVsnap(const std::string & devicename);

    //bool DeleteVsnapLogs(VsnapContextInfo *);
    //bool PollRetentionData(string, SV_ULONGLONG &);
};

class UnixVsnapMgr:public VsnapMgr
{
public:
    UnixVsnapMgr():VsnapMgr()
    {}
    long OpenInVirVolDriver(ACE_HANDLE &, VsnapErrorInfo*);
    void GetAllVirtualVolumes(std::string & Volumes);

    //Bug #4044
    bool Unmount(VsnapVirtualInfoList &, bool, bool SendStatus = true,bool bypassdriver = false );
    bool Unmount(VsnapVirtualInfoList &, bool deletelogs, bool SendStatus, std::string& output, std::string& error,bool bypassdriver = false );
    bool fill_fsdata_from_retention(const std::string & fileName, SV_ULONGLONG fileoffset, VsnapPairInfo *PairInfo){ return true;}
    void StartTick(SV_ULONG &);
    void EndTick(SV_ULONG);
    ACE_HANDLE OpenVirtualVolumeControlDevice();
    void VsnapGetVirtualInfoDetaisFromVolume(VsnapVirtualVolInfo *, VsnapContextInfo*);
    bool UnmountVirtualVolume(char*, const size_t, VsnapVirtualVolInfo *, std::string& output, std::string& error, bool bypassdriver = false);

protected:
    void PerformMountOrRemount(VsnapSourceVolInfo *);
    bool RemountV2(const VsnapPersistInfo& remountInfo);
    bool MountVol(VsnapVirtualVolInfo*, VsnapMountInfo *, char*,bool&);
    bool StatAndMount(const VsnapPersistInfo& remountInfo);
    bool StatAndWait(const std::string &device);
    bool PerformZpoolDestroyIfReq(const std::string &devicefile, bool bshoulddeletevsnaplogs);
    bool CloseLinvsnapSyncDev(const std::string &device, int &fd);
    bool OpenLinvsnapSyncDev(const std::string &device, int &fd);
    bool RetryZpoolDestroyIfReq(const std::string &device, const std::string &target, bool bshoulddeletevsnaplogs);
    bool NeedToRecreateVsnap(const std::string &vsnapdevice);
    bool OpenMountPointExclusive(ACE_HANDLE*, char *, const size_t, char *);
    bool CloseMountPointExclusive(ACE_HANDLE, char *, char *);
    //Bug #4044
    bool UnmountVirtualVolume(char*, const size_t, VsnapVirtualVolInfo *, bool bypassdriver = false);
    void GetOrUnmountAllVirtualVolumes(std::string & MountPoints, bool);	
    void ProcessVolumeAndMountPoints(std::string &, char *, char *, int, bool);
    bool IsVolumeVirtual(char const * Vol);
    //bool IsVolumePresent(char *Vol);
    bool IsValidVsnapMountPoint(char const *Vol); //Equivalent to IsVolumePresent
    bool IsValidVsnapDeviceFile(char const *Vol); //Equivalent to IsVolumeVirtual
    bool GetVsnapContextFromVolumeLink(ACE_HANDLE, wchar_t* , VsnapVirtualVolInfo*, VsnapContextInfo *);
    bool UnmountFileSystem(const wchar_t *, VsnapVirtualVolInfo *);
    void DeleteDrivesForTargetVolume(const char* , VsnapVirtualVolInfo *);
    bool ValidateSourceVolumeOfVsnapPair(VsnapPairInfo*);
    bool GetVsnapSourceVolumeDetails(char *,VsnapPairInfo*);
    bool UpdateMap(DiskBtree *, SV_UINT FileId, SVD_DIRTY_BLOCK_INFO *Block);
    bool ValidateVirtualVolumeOfVsnapPair(VsnapPairInfo*,bool&);
    void DeleteAutoAssignedDriveLetter(ACE_HANDLE hDevice, char* MountPoint, bool IsMountPoint);
    bool SetMountPointForVolume(char* MountPoint, char* VolumeSymLink, char* VolumeName);
    void DeleteMountPointsForTargetVolume(char*, char*, VsnapVirtualVolInfo *);
    SV_UCHAR FileDiskUmount(char* VolumeName);
    void DisconnectConsoleOutAndErrorHdl();
    void ConnectConsoleOutAndErrorHdl();
    bool VolumeSupportsReparsePoints(char* VolumeName);
    void DeleteMountPoints(char*, VsnapVirtualVolInfo *);
    size_t mbstotcs(char* tstring, const size_t tstringlen, const char* mbstr);
    size_t tcstombs(char* mbstr, const size_t mbstrlen, const char* tstring);
    size_t wcstotcs(char* tstring, const size_t tstringlen, const wchar_t* wcstr);
    bool IsMountPointValid(char *, VsnapPairInfo*,bool&);
    bool DirectoryEmpty(char *dir);
    bool CheckForVirtualVolumeReusability(VsnapPairInfo*,bool&);

    bool TraverseAndApplyLogs(VsnapSourceVolInfo *);
    void process_proc_mounts(std::vector<volinfo> &fslist);
    /* only unix specific functions */
    bool CreateRequiredVsnapDIR();
    bool CreateVsnapDevice(VsnapVirtualVolInfo* VirtualInfo,VsnapMountInfo *MountData,char * Buffer,SV_ULONG& ByteOffset, SV_ULONG BufferSize);
    void CleanUpIncompleteVsnap(VsnapVirtualVolInfo* VirtualInfo);
    bool DeleteVsnapDevice(VsnapVirtualVolInfo* VirtualInfo,const std::string & devicefile,std::string & error);
    bool MountVol_Platform_Specific(VsnapVirtualVolInfo *VirtualInfo, int minornumber, sv_dev_t MkdevNumber);
	bool Unmount_Platform_Specific(const std::string & devicefile);
};

class VsnapUtil
{
public:

    static bool construct_persistent_vsnap_filename(const std::string& devicename, const std::string& target, std::string& filename);
    static bool construct_pending_persistent_vsnap_filename(const std::string& devicename, const std::string& target, std::string& filename );
    static std::string construct_vsnap_filename( const std::string& devicename, const std::string& target);
    static std::string construct_vsnap_persistent_name(const std::string& name);
    static bool parse_persistent_vsnap_filename(const std::string& filename, std::string& target, std::string& devicename);
    static bool match_persistent_vsnap_filename(const std::string& parent, const std::string& vsnapname, const std::string& match,
        const std::string& target_to_match, const std::string& devicename_to_match);
    static bool create_persistent_vsnap_file_handle(const std::string& devicename, const std::string& target, ACE_HANDLE& handle);
    static bool create_pending_persistent_vsnap_file_handle( const std::string& devicename, const std::string& target, ACE_HANDLE& handle);
    static bool create_vsnap_file_handle( const std::string& filename, ACE_HANDLE& handle);
    static bool delete_pending_persistent_vsnap_filename(const std::string& devicename, const std::string& target);
    static bool write_persistent_vsnap_info(ACE_HANDLE& handle, VsnapVirtualVolInfo* VirtualInfo, VsnapMountInfo* MountData);
    static bool read_persistent_vsnap_info(const std::string& filename, VsnapPersistInfo& vsnapinfo);
};




#endif
