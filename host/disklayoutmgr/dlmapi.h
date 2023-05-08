#ifndef DLMAPI__H
#define DLMAPI__H

#include <iostream>
#include <svtypes.h>
#include <list>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <string.h>
#include "inmsafecapis.h"

#define DLM_MBR_FILE_REG							"DlmMbrFile"
#define DLM_DISK_PARTITION_FILE						"_partition.dat"
#define DLM_PARTITION_FILE_REG						"PartitionFile"
#define DLM_CHECKSUM_REG							"DlmChecksum"
#define DLM_SOFTWARE_HIVE							"SOFTWARE"
#define MAX_PATH_COMMON                             260
#define MAX_DISK_SIGNATURE_LENGTH                   40
#define DISK_BYTES_PER_SECTOR                       512

#define MBR_BOOT_SECTOR_LENGTH                      512
#define GPT_BOOT_SECTOR_LENGTH                      17408
#define MBR_DYNAMIC_BOOT_SECTOR_LENGTH				1048576
#define GPT_DYNAMIC_BOOT_SECTOR_LENGTH				1065984
#define LDM_DYNAMIC_BOOT_SECTOR_LENGTH				1048576

#define DLM_FILE_NORMAL								"dlm_normal"
#define DLM_MBR_FILE_ONLY							"dlm_mbronly"
#define DLM_FILE_ALL								"dlm_all"

// mount name types - used to tell RestoreMountPoints which types it should restore
// these can be OR'd together
int const MOUNT_NAME_TYPE_DRIVE_LETTER = 0x0001;
int const MOUNT_NAME_TYPE_MOUNT_POINT  = 0x0002;

//Disk Flags - Gives info which part is missing in DISK_INFO structure
enum DISK_FLAG
{
    FLAG_COMPLETE = 0,
    FLAG_USELESS = 1,
    FLAG_NO_DISKSIZE = 2,
    FLAG_NO_VOLUMESINFO = 4,
    FLAG_NO_DISK_BYTESPERSECTOR = 8,
    FLAG_NO_BOOTRECORD = 16,    
    FLAG_NO_EBR = 32,
    FLAG_NO_SCSIID = 64,
	FLAG_NO_DISKSIGNATURE = 65,
	FLAG_NO_DEVICEID = 66
};

//define all error codes here
enum DLM_ERROR_CODE 
{
    DLM_ERR_SUCCESS = 0,
    DLM_ERR_FAILURE,
    DLM_ERR_INCOMPLETE,
    DLM_ERR_DISK_READ,
    DLM_ERR_DISK_WRITE,
    DLM_ERR_WMI,
    DLM_ERR_FETCH_VOLUMES,
    DLM_ERR_DYNAMICDISK,  
    DLM_ERR_RAWDISK,
    DLM_ERR_FETCH_EBR,
    DLM_ERR_WRITE_EBR,
    DLM_ERR_FILE_CREATE,		
    DLM_ERR_FILE_OPEN,
    DLM_ERR_FILE_WRITE,
    DLM_ERR_FILE_READ,
	DLM_FILE_CREATED,	//File created need to upload
	DLM_FILE_NOT_CREATED,		//File not created, use old files to upload
    DLM_ERR_UNABLE_TO_ASSIGN_SAME_MOUNT_POINT,
	DLM_ERR_DISK_DISAPPEARED
};

enum DISK_FORMAT_TYPE
{
    MBR = 0,
    GPT,
    RAWDISK
};

enum DISK_TYPE
{
    BASIC = 0,
    DYNAMIC
};

enum DISK_TYPE_ATTRIBUTE{
        DISK_READ_ONLY,
		DISK_BOOT,
		DISK_PAGEFILE,
		DISK_CLUSTER,
		DISK_CRASH_DUMP
	};

struct SCSIID
{
	SV_UINT Host;
	SV_UINT Channel;
	SV_UINT Target;
	SV_UINT Lun;
    SCSIID()
    {
        Host    = 0;
        Channel = 0;
        Target  = 0;
        Lun     = 0;
    }
};

struct PARTITION_FILE
{
	char	Name[MAX_PATH_COMMON + 1];
	PARTITION_FILE()
	{
		memset(Name, '\0', MAX_PATH_COMMON+1);
	}
};

struct EBR_SECTOR
{
    SV_UCHAR EbrSector[MBR_BOOT_SECTOR_LENGTH];
};

struct DISK_INFO_SUB
{
	char                Name[MAX_PATH_COMMON + 1];
    SV_ULONGLONG        Flag;
	SV_LONGLONG         Size;	
	DISK_TYPE           Type;
    SCSIID              ScsiId;
	DISK_FORMAT_TYPE    FormatType;
    SV_ULONGLONG        BytesPerSector;
	SV_ULONGLONG        VolumeCount;
    SV_ULONGLONG        EbrCount;
    DISK_INFO_SUB()
    {
		memset(Name, '\0', MAX_PATH_COMMON+1);
        Flag            = FLAG_COMPLETE;
        Size            = 0;
        Type            = BASIC;
        FormatType      = RAWDISK;
        BytesPerSector  = 0; //or we can assign DISK_BYTES_PER_SECTOR. chk this.
        VolumeCount     = 0;
        EbrCount        = 0;
    }
};

struct VOLUME_INFO
{
	char                VolumeName[MAX_PATH_COMMON + 1];
    char                VolumeGuid[MAX_PATH_COMMON + 1];
	SV_LONGLONG         VolumeLength;
	SV_LONGLONG         StartingOffset;
	SV_LONGLONG         EndingOffset;
    VOLUME_INFO()
    {
        memset(VolumeName, '\0', MAX_PATH_COMMON+1);
        memset(VolumeGuid, '\0', MAX_PATH_COMMON+1);
        VolumeLength    = 0;
        StartingOffset  = 0;
        EndingOffset    = 0;
    }
};

typedef std::vector<VOLUME_INFO> VolumesInfoVec_t;
typedef std::vector<VOLUME_INFO>::iterator VolumesInfoVecIter_t;

struct DLM_VERSION_INFO
{
	SV_UINT	    Major;
    SV_UINT 	Minor;
	DLM_VERSION_INFO()
	{
		Major = 1;
		Minor = 0;
	}
};

struct DISK_INFO
{
    DISK_INFO_SUB                   DiskInfoSub;
	char                            DiskDeviceId[MAX_PATH_COMMON + 1];
	char                            DiskSignature[MAX_DISK_SIGNATURE_LENGTH + 1];	
    VolumesInfoVec_t                VolumesInfo;
    SV_UCHAR                        MbrSector[MBR_BOOT_SECTOR_LENGTH];
    SV_UCHAR                        GptSector[GPT_BOOT_SECTOR_LENGTH];
	SV_UCHAR						*MbrDynamicSector;
	SV_UCHAR                        *GptDynamicSector;
	SV_UCHAR						*LdmDynamicSector;
    std::vector<EBR_SECTOR>         EbrSectors;
	std::vector<PARTITION_FILE>		vecPartitionFilePath;	//Took Vector because in future we may collect more no.of partition files per disk

	DISK_INFO()
	{
		memset(DiskDeviceId, 0, sizeof(DiskDeviceId));
		memset(DiskSignature, 0, sizeof(DiskSignature));
		MbrDynamicSector = GptDynamicSector = LdmDynamicSector = NULL;
		MbrDynamicSector = new SV_UCHAR[MBR_DYNAMIC_BOOT_SECTOR_LENGTH];
		GptDynamicSector = new SV_UCHAR[GPT_DYNAMIC_BOOT_SECTOR_LENGTH];
		LdmDynamicSector = new SV_UCHAR[LDM_DYNAMIC_BOOT_SECTOR_LENGTH];
		vecPartitionFilePath.clear();
	}

	DISK_INFO(const DISK_INFO & D)
	{
		DiskInfoSub = D.DiskInfoSub;
		inm_memcpy_s(DiskDeviceId, sizeof (DiskDeviceId), D.DiskDeviceId, MAX_PATH_COMMON+1);
		inm_memcpy_s(DiskSignature, sizeof (DiskSignature), D.DiskSignature, MAX_DISK_SIGNATURE_LENGTH+1);		
		VolumesInfo = D.VolumesInfo;
		inm_memcpy_s(MbrSector, sizeof (MbrSector), D.MbrSector, MBR_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(GptSector, sizeof (GptSector), D.GptSector, GPT_BOOT_SECTOR_LENGTH);
		MbrDynamicSector = GptDynamicSector = LdmDynamicSector = NULL;
		MbrDynamicSector = new SV_UCHAR[MBR_DYNAMIC_BOOT_SECTOR_LENGTH];
		GptDynamicSector = new SV_UCHAR[GPT_DYNAMIC_BOOT_SECTOR_LENGTH];
		LdmDynamicSector = new SV_UCHAR[LDM_DYNAMIC_BOOT_SECTOR_LENGTH];
		inm_memcpy_s(MbrDynamicSector, MBR_DYNAMIC_BOOT_SECTOR_LENGTH, D.MbrDynamicSector, MBR_DYNAMIC_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(GptDynamicSector, GPT_DYNAMIC_BOOT_SECTOR_LENGTH, D.GptDynamicSector, GPT_DYNAMIC_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(LdmDynamicSector, LDM_DYNAMIC_BOOT_SECTOR_LENGTH, D.LdmDynamicSector, LDM_DYNAMIC_BOOT_SECTOR_LENGTH);
		EbrSectors = D.EbrSectors;
		vecPartitionFilePath = D.vecPartitionFilePath;
	}

	DISK_INFO& operator=(DISK_INFO& D) 
	{
		DiskInfoSub = D.DiskInfoSub;
		inm_memcpy_s(DiskDeviceId, sizeof(DiskDeviceId), D.DiskDeviceId, MAX_PATH_COMMON+1);
		inm_memcpy_s(DiskSignature, sizeof(DiskSignature), D.DiskSignature, MAX_DISK_SIGNATURE_LENGTH+1);		
		VolumesInfo = D.VolumesInfo;
		inm_memcpy_s(MbrSector, sizeof(MbrSector), D.MbrSector, MBR_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(GptSector, sizeof(GptSector), D.GptSector, GPT_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(MbrDynamicSector, MBR_DYNAMIC_BOOT_SECTOR_LENGTH, D.MbrDynamicSector, MBR_DYNAMIC_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(GptDynamicSector, GPT_DYNAMIC_BOOT_SECTOR_LENGTH, D.GptDynamicSector, GPT_DYNAMIC_BOOT_SECTOR_LENGTH);
		inm_memcpy_s(LdmDynamicSector, LDM_DYNAMIC_BOOT_SECTOR_LENGTH, D.LdmDynamicSector, LDM_DYNAMIC_BOOT_SECTOR_LENGTH);
		EbrSectors = D.EbrSectors;
		vecPartitionFilePath = D.vecPartitionFilePath;
		return *this;
	}

	~DISK_INFO()
	{
		if(NULL != MbrDynamicSector)
			delete [] MbrDynamicSector;
		if(NULL != GptDynamicSector)
			delete [] GptDynamicSector;
		if(NULL != LdmDynamicSector)
			delete [] LdmDynamicSector;	
	}
};

typedef std::string DiskName_t;
typedef std::map<DiskName_t, DISK_INFO> DisksInfoMap_t;
typedef std::map<DiskName_t, DISK_INFO>::iterator DisksInfoMapIter_t;
typedef std::string VolumeName_t;
typedef std::multimap<VolumeName_t, VOLUME_INFO> VolumesInfoMultiMap_t;
typedef std::multimap<VolumeName_t, VOLUME_INFO>::iterator VolumesInfoMultiMapIter_t;
typedef std::map<std::string, std::string>::iterator StringArrMapIter_t;



//New structure implementation for Disk layout information

#if defined(SV_SUN) || defined(SV_AIX)
    #pragma pack( 1 )
#else
    #pragma pack( push, 1 )
#endif /* SV_SUN  || SV_AIX */

struct DLM_HEADER
{
	DLM_VERSION_INFO	Version;
	SV_UCHAR			MD5Checksum[16];
	DLM_HEADER()
	{
		memset(MD5Checksum, '\0', 16);
	}
};

struct DLM_PREFIX
{
	SV_UCHAR		MD5Checksum[16];	//Checksum for structures (future use)
	std::string		strIdentifier;		//Identifier for the structure
	SV_UINT			size;				//Size of structure
	SV_UINT			numOfStructure;		//Number of structures going to write in file	
	DLM_PREFIX()
	{
		memset(MD5Checksum, '\0', 16);
		strIdentifier	= "";
		size			= 0;
		numOfStructure	= 0;
	}
	void clear()
	{
		memset(MD5Checksum, '\0', 16);
		strIdentifier.clear();
		size			= 0;
		numOfStructure	= 0;
	}
};

struct DLM_PREFIX_U1
{
	SV_UCHAR		MD5Checksum[16];	//Checksum for structures (future use)
	char			Identifier[30];		//Identifier for the structure (changed this parameter to earlier to teh current once because 64 bit parsing issue came if we take c++ string object)
	SV_UINT			size;				//Size of structure
	SV_UINT			numOfStructure;		//Number of structures going to write in file	
	DLM_PREFIX_U1() : size(0),numOfStructure(0) 
	{
		memset(MD5Checksum, '\0', 16);
		memset(Identifier, '\0', 30);
	}
	void clear()
	{
		memset(MD5Checksum, '\0', 16);
		memset(Identifier, '\0', 30);
		size			= 0;
		numOfStructure	= 0;
	}
};

struct DLM_PARTITION_INFO_SUB
{
	char					DiskName[MAX_PATH_COMMON + 1];
	char					PartitionName[MAX_PATH_COMMON + 1];
	char					PartitionType[MAX_PATH_COMMON + 1];
	SV_UCHAR				MD5Checksum[16];	//Checksum for structures (future use)
	SV_UINT					PartitionNum;
	SV_LONGLONG				PartitionLength;
	SV_LONGLONG				StartingOffset;

	DLM_PARTITION_INFO_SUB()
    {
        memset(DiskName, '\0', MAX_PATH_COMMON+1);
		memset(PartitionName, '\0', MAX_PATH_COMMON+1);
        memset(PartitionType, '\0', MAX_PATH_COMMON+1);
		memset(MD5Checksum, '\0', 16);
		PartitionNum = 0;
        PartitionLength = 0;
        StartingOffset  = 0;
    }
};

struct DLM_PARTITION_INFO
{
	DLM_PARTITION_INFO_SUB	PartitionInfoSub;
	SV_UCHAR				*PartitionSector;

	DLM_PARTITION_INFO()
	{
	}

	DLM_PARTITION_INFO(const DLM_PARTITION_INFO & Dp)
	{
		PartitionInfoSub = Dp.PartitionInfoSub;
		PartitionSector = new SV_UCHAR[(unsigned int)Dp.PartitionInfoSub.PartitionLength];
		inm_memcpy_s(PartitionSector, (size_t)Dp.PartitionInfoSub.PartitionLength, Dp.PartitionSector, (size_t)Dp.PartitionInfoSub.PartitionLength);
	}

	DLM_PARTITION_INFO& operator=(const DLM_PARTITION_INFO& Dp) 
	{
		PartitionInfoSub = Dp.PartitionInfoSub;
		inm_memcpy_s(PartitionSector, (size_t)Dp.PartitionInfoSub.PartitionLength,Dp.PartitionSector, (size_t)Dp.PartitionInfoSub.PartitionLength);
		return *this;
	}
	~DLM_PARTITION_INFO()
	{
		if(NULL != PartitionSector)
			delete [] PartitionSector;
	}

    	
};

typedef std::vector<DLM_PARTITION_INFO_SUB> DlmPartitionInfoSubVec_t;
typedef std::vector<DLM_PARTITION_INFO_SUB>::iterator DlmPartitionInfoSubIterVec_t;
typedef std::map<DiskName_t, DlmPartitionInfoSubVec_t> DlmPartitionInfoSubMap_t;
typedef std::map<DiskName_t, DlmPartitionInfoSubVec_t>::iterator DlmPartitionInfoSubIterMap_t;

typedef std::vector<DLM_PARTITION_INFO> DlmPartitionInfoVec_t;
typedef std::vector<DLM_PARTITION_INFO>::iterator DlmPartitionInfoIterVec_t;
typedef std::map<DiskName_t, DlmPartitionInfoVec_t> DlmPartitionInfoMap_t;
typedef std::map<DiskName_t, DlmPartitionInfoVec_t>::iterator DlmPartitionInfoIterMap_t;

#if defined(SV_SUN) || defined(SV_AIX)
    #pragma pack()
#else
    #pragma pack( pop )
#endif /* SV_SUN || SV_AIX */

/*********************************************************
                    API FUNCTIONS 
**********************************************************/

/*
This function collects all the required disks and required partition information and write it into required files in 
binary format and same will be used during recovery.
This function is expected to be called from Register Host in Vx Agent.
    Return Code: There are three return codes.
			In case of success the error codes are 
				DLM_FILE_CREATED,		//File created need to upload
				DLM_FILE_NOT_CREATED		//There is not chnage, File not required to upload
			In case of Failue, DLM failure error codes will reutrn.
			
	Input: Name of Binary File which will contain the disk layout information
	OutPut: List of Binary file names which need to upload. 
			It is always a union of the disk layout file name and the partition file names, if any.
            If the return value mentions that a newer disk layout file was written, then the
            outFileNames have been filled with newly created files. 
            If the return value mentions that no files have been newly created because nothing 
            has changed, then the uploadFiles are files that were previously created.
	OutPut: Non-functional volumes list if exists else empty
	Output: File flag which tells the required file to create, default is DLM_FILE_NORMAL(normal way of creation)
*/
DLM_ERROR_CODE StoreDisksInfo(const std::string& binaryFileName, 
							std::list<std::string>& outFileNames, 
							std::list<std::string>& erraticVolumeGuids, 
							const std::string& dlmFileFlag = DLM_FILE_NORMAL);


/*
This functions collects the complete source disks’ information from binary file 
and the target disks’ information from the system and then return both.
    Input: 		    Name of binary file which contains source disk info.
    Output:		    Map of source disk names to their disk info.
    Output:		    Map of target disk names to their disk info.
    Return Code:    0 on success
*/
DLM_ERROR_CODE CollectDisksInfoAndConvert(const char * BinaryFileName,
                                         DisksInfoMap_t & SrcMapDiskNamesToDiskInfo,
                                         DisksInfoMap_t & TgtMapDiskNamesToDiskInfo
                                         );

DLM_ERROR_CODE CollectDisksInfoFromSystem(DisksInfoMap_t & TgtMapDiskNamesToDiskInfo);


/*
This function, For the given source and target disk information, returns the list of corrupted disks and 
map of source disks to possible target disks.
    Input: 		    Map of source disk names to their disk info.
    Input:		    Map of target disk names to their disk info..
    Output:		    Vector of corrupted source disk names
    Output:		    Map of source disk names to vector of possible target disk names.
    Return Code:    0 on success
*/
DLM_ERROR_CODE GetCorruptedDisks (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                  DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                  std::vector<DiskName_t> & CorruptedSrcDiskNames,
                                  std::map<DiskName_t, std::vector<DiskName_t> > & MapSrcDisksToTgtDisks
                                  );


/*
This function, For the given map of source to target disk names, the source disk structure will be 
restored onto the corresponding target disk. Returns the vector of successfully restored source disks.
    Input: 		    Map of source disk names to their disk info.
    Input:		    Map of target disk names to their disk info.
    Input:		    Map of source to target disk names that has to be restored
    Output:		    Vector of successfully restored source disk names
    Return Code:    0 on success
	if Return Code is DLM_ERR_DISK_DISAPPEARED, then the Outpur parmater(RestoredSrcDiskNames) contains
	the disappeared target disks list.
*/
DLM_ERROR_CODE RestoreDiskStructure (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                     DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                     std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
                                     std::vector<DiskName_t> & RestoredSrcDiskNames,
                                     bool restartVds = true
                                     );

/*
This function, For the given map of source to target disk names, the source disk structure will be 
restored onto the corresponding target disk. Returns the vector of successfully restored source disks.
    Input: 		    Binary file which contains the partition informations of all the disks.
    Input:		    Map of source to target disk names that has to be restored
    Output:		    Vector of successfully restored source disk names
    Return Code:    0 on success
*/
DLM_ERROR_CODE RestoreDiskPartitions(const char * BinaryFileName,
                                     std::map<DiskName_t, DiskName_t>& MapSrcToTgtDiskNames,
                                     std::set<DiskName_t>& RestoredSrcDiskNames
                                     );


/*
This function, For the given map of source to target disk names, 
the map of source to target volumes will be returned.
    Input: 		    Map of source disk names to their disk info.
    Input:		    Map of source to target disk names that has been restored
    Output:		    Map of source to target volume names for input disks map
    Return Code:    0 on success
*/
DLM_ERROR_CODE GetMapSrcToTgtVol (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                  std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
                                  std::map<VolumeName_t, VolumeName_t> & MapSrcToTgtVolumeNames
                                  );

DLM_ERROR_CODE RestoreVolumeMountPoints (DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
                                         std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
                                         int mountType = MOUNT_NAME_TYPE_DRIVE_LETTER | MOUNT_NAME_TYPE_MOUNT_POINT // default restore both types
                                         );

DLM_ERROR_CODE RestoreVConVolumeMountPoints(DisksInfoMap_t  SrcMapDiskNamesToDiskInfo,
										 DisksInfoMap_t  TgtMapDiskNamesToDiskInfo,
                                         std::map<DiskName_t, DiskName_t> MapSrcToTgtDiskNames,
										 std::map<std::string, std::string> mapOfSrcVolToTgtVol
                                         );

/*
This function is for reading the input diskinfo binary file and construct the map of diskname s to diskinfo.
    Input: 		    DiskInfo binary file.
    Output:		    Map of disk names to their disk info.
    Return Code:    0 on success
*/
DLM_ERROR_CODE ReadDiskInfoMapFromFile(const char * BinaryFileName, DisksInfoMap_t & mapDiskNamesToDiskInfo);

/*
This function is for reading the dlm version from the DiskInfo binary file
	Input:			DiskInfo binary file name.
	Output:			A double value indicating the dlm version
	Return Code:	0 on success
*/
DLM_ERROR_CODE GetDlmVersion(const char * FileName, double & dlmVersion);

/*
This function is for reading the input partition binary file and construct the map of diskname to partition
	Input:			Partition binary file
	Output:			Map of disk names to their partitions which will have bianry info also
	Return Code:	0 on success
*/
DLM_ERROR_CODE ReadPartitionInfoFromFile(const char * BinaryFileName, DlmPartitionInfoMap_t & mapDiskToPartition);

/*
This function is for reading the input partition binary file and construct the map of diskname to partition
	Input:			Partition binary file
	Output:			Map of disk names to their partitions won't contains the partition binary info
	Return Code:	0 on success
*/
DLM_ERROR_CODE ReadPartitionSubInfoFromFile(const char * BinaryFileName, DlmPartitionInfoSubMap_t & mapDiskToPartition);

/*
 This function restores the given source disk structure onto the given target disk.
 Restore MBR and EBR if MBR disk
 Restore MBR and LDM if GPT disk
 Restore GPT if GPT disk
 Ignore if RAW disk - Need not restore and its not an error too.
*/
DLM_ERROR_CODE RestoreDisk(const char * TgtDiskName, const char * SrcDiskName, DISK_INFO& SrcDiskInfo);

/*
 This function restores the given source disk partiton information onto the given target disk.
	Input:			Target disk Name
	Input:			list of Source partition information(vector)
	Return Code:	0 on success
*/
DLM_ERROR_CODE RestorePartitions(const char * TgtDiskName, DlmPartitionInfoVec_t& vecPartitions);

/*
	This function will return the inputted disk ID is having EFI partiton or not.
	If it has EFI partition then the return value is true, else it is false.
*/
bool HasUEFI(const std::string& DiskID);


/*
	This function will regenerate the EFI disks list.
*/
void RefreshDiskLayoutManager();

/*
	This function will Finds out the specified disks staring and length of the EFI partition if available.
*/
void GetPartitionInfo(const std::string& DiskName, DlmPartitionInfoSubVec_t& vecPartitions);

/*
	This function is to online or offline the given input disk based on the input bool value.
	If bOnline ==  true, then API will try to online the disk
	If bOnline ==  false, then API will try to offline the disk
*/

DLM_ERROR_CODE OnlineOrOfflineDisk(const std::string& DiskName, const bool& bOnline);

/*
	Post operations after restoreing the source dynamic disks structure onto the given target disks.
	This will find out the required dynamic disks at target and do's the needful operations on it.
	Operations are like:
		Import, Recovery, fix unknown disk.
	Input: List of disks restored in the previous calls
	       restartVds: true (default): restart vds, false: skip vds restart 
	return 0 if succeess
*/
DLM_ERROR_CODE PostDynamicDiskRestoreOperns(std::vector<std::string>& RestoredDisk, bool restartVds = true);

/*
	This function will convert EFI partition of the disk into normal partition.
	Input: diskname
	Input: partition information generated from partition file

	return 0 if succeess
*/
DLM_ERROR_CODE ConvertEfiToNormalPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp);

/*
	This function will convert a normal partition of the disk into EFI partition. Its a preceeding call for ConvertEfiToNormalPartition
	Input: diskname
	Input: partition information generated from partition file

	return 0 if succeess
*/
DLM_ERROR_CODE ConvertNormalToEfiPartition(const std::string& DiskName, DlmPartitionInfoSubVec_t & vecDp);

/*
	This function delets the registry entries which were created by DLM library

	return 0 if succeess
*/
DLM_ERROR_CODE DeleteDlmRegitryEntries();

/*
	This function will do the EFI changes related to W2K8.
	Input: 		    Binary file which contains the partition informations of all the disks.
    Input:		    Map of source to target disk names which are already restored.
    Input:		    Vector of successfully restored source disk names return from API call "RestoreDiskPartitions"
    Return Code:    0 on success
*/
DLM_ERROR_CODE RestoreW2K8Efi(const char * BinaryFileName,
                             std::map<DiskName_t, DiskName_t>& MapSrcToTgtDiskNames,
                             std::set<DiskName_t>& RestoredSrcDiskNames
                             );

/*
	This function will clean the initial configuration of the disk
	Input: diskname on which want to do cleanup

	Returns 0 on success
*/
DLM_ERROR_CODE CleanDisk(const std::string& DiskName);

/*
	This function will import the dynamic disks which are in foreign state
	Input: disknumbers which are in foreign state

	Returns 0 on success
*/
DLM_ERROR_CODE ImportDisks(std::set<std::string>& DiskNames);

/*
	This function will clear the read-only attribute of a particular disk.
	Input: disknumber for which read-only attribute need to clear.

	Returns 0 on success
*/
DLM_ERROR_CODE ClearReadOnlyAttrOfDisk(const std::string& DiskName);

/*
	This function will Initialize the disk. By default the disk will get initialized to basic MBR.
	Currently GPT disk initialize is not in place.
	Input: disknumber which need to initialize.

	Returns 0 on success
*/
DLM_ERROR_CODE InitializeDisk(std::string DiskName, DISK_FORMAT_TYPE FormatType = MBR, DISK_TYPE type = BASIC);

/*
	This function will enable\disbale the auto mount for newly created volumes.
	By default the value is true (enables the autmount)

	Returns 0 on success
*/
DLM_ERROR_CODE AutoMount(bool bEnableFlag = true);

/*
This function will filter the corrupted disks list by considering the selected recover disks.
It will exlude the non slected list from the list by comparing the selected disks.
    Input: 		    Vector of selected disks for recovery.
    Input\Output:   Vector of corrupted source disk names returned from GetCorruptedDisks
    Return Code:    0 on success
*/
DLM_ERROR_CODE GetFilteredCorruptedDisks (std::vector<DiskName_t>& SelectedSrcDiskNames,
										std::vector<DiskName_t> & CorruptedSrcDiskNames
										);

DLM_ERROR_CODE DiskCheckAndRectify();

/*
This function will download the required amount of data from the disk.
    Input:    Disk name from which data need to collect (format of diskname as \\.\PHYSICALDRIVE<disknum>)
	Input:    Number bytes to skip in the disk from beginning to collect the data
	Input:    Number of bytes to read from the disk
	Output:   The data which was collected will get stored in this
	Return Code:  True on success
*/
bool DownloadFromDisk(const char *DiskName, SV_LONGLONG BytesToSkip, size_t BytesToRead, SV_UCHAR *DiskData);

/*
This function will update the required amount of data on the disk.
    Input:    Disk name on which data need to update (format of diskname as \\.\PHYSICALDRIVE<disknum>)
	Input:    Number bytes to skip in the disk from beginning to update the data
	Input:    Number of bytes to update on the disk
	Input:    The data which need to update on the disk
	Return Code:  True on success
*/
bool UpdateOnDisk(const char *DiskName, SV_LONGLONG BytesToSkip, size_t BytesToWrite, SV_UCHAR *DiskData);


#endif  /* DLMAPI__H */

