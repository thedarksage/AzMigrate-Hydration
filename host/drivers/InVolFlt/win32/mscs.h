#ifndef _INMAGE_MSCS_H_
#define _INMAGE_MSCS_H_

NTSTATUS
DeviceIoControlMSCSDiskRemovedFromCluster(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
DeviceIoControlMSCSDiskAddedToCluster(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );


#define REG_CLUSTER_GROUPS              L"\\REGISTRY\\MACHINE\\Cluster\\Groups"
#define REG_CLUSTER_RESOURCES           L"\\REGISTRY\\MACHINE\\Cluster\\Resources"
#define REG_CLUSTER_GROUP_CONTAINS_VAL  L"Contains"
#define REG_CLUSTER_RES_PARAMETERS      L"Parameters"
#define REG_CLUSDISK_DISKNAME_VALUE     L"DiskName"
#define REG_CLUSTER_RES_TYPE_VAL        L"Type"
#define REG_CLUSTER_RES_PHYSICAL_DISK   L"Physical Disk"
#define REG_CLUSTER_RES_SIGNATURE_VAL   L"Signature"
#define REG_CLUSTER_ATTACHED_DISKS_VAL  L"AttachedDisks"

VOID
LoadClusterConfigFromClustDiskDriverParameters();

#define MAX_DISKNAME_SIZE_IN_CHAR   0x20

typedef struct _BASIC_DISK BASIC_DISK, *PBASIC_DISK;

#define BV_FLAGS_INSERTED_IN_BASIC_DISK     0x0001
#define BV_FLAGS_HAS_DRIVE_LETTER           0x0002

struct _VOLUME_CONTEXT;

typedef struct _BASIC_VOLUME
{
    LIST_ENTRY  ListEntry; // Used to link all basic volumes on basic disk
    LONG        lRefCount;
    ULONG       ulFlags;
    WCHAR       GUID[GUID_SIZE_IN_CHARS + 1];   // 36 + 1
    WCHAR       DriveLetter;
    USHORT      usReserved[2];
    RTL_BITMAP  DriveLetterBitMap;
    ULONG       BuferForDriveLetterBitMap;
    PBASIC_DISK  pDisk;
    struct _VOLUME_CONTEXT *pVolumeContext;
} BASIC_VOLUME, *PBASIC_VOLUME;

// If this flag is set, the disk is part of cluster.
#define BD_FLAGS_IS_CLUSTER_RESOURCE    0x0001
// If this flag is set, the disk is part of cluster and the disk is online.
#define BD_FLAGS_CLUSTER_ONLINE         0x0002

struct _BASIC_DISK
{
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    
#if (NTDDI_VERSION >= NTDDI_VISTA)
    etPartStyle  PartitionStyle;
    union{
        ULONG           ulDiskSignature;
        GUID            DiskId;
    };
#else 
    ULONG           ulDiskSignature;
#endif

    ULONG       ulDiskNumber;
    ULONG       ulFlags;
    ULONG	IsAccessible;
    WCHAR       DiskName[MAX_DISKNAME_SIZE_IN_CHAR];
    KSPIN_LOCK  Lock;
    LIST_ENTRY  VolumeList;
};


PBASIC_VOLUME AllocateBasicVolume();
LONG ReferenceBasicVolume(PBASIC_VOLUME pBasicVolume);
LONG DereferenceBasicVolume(PBASIC_VOLUME pBasicVolume);

PBASIC_DISK AllocateBasicDisk();
LONG ReferenceBasicDisk(PBASIC_DISK pBasicDisk);
LONG DereferenceBasicDisk(PBASIC_DISK pBasicDisk);

PBASIC_VOLUME
AddVolumeToBasicDisk(PBASIC_DISK pBasicDisk, struct _VOLUME_CONTEXT *VolumeContext);

PBASIC_VOLUME
GetVolumeFromBasicDisk(PBASIC_DISK pBasicDisk, PWCHAR pGUID);

VOID
GetVolumeListForBasicDisk(PBASIC_DISK pBasicDisk, PLIST_ENTRY ListHead);

VOID
GetBasicDiskList(PLIST_ENTRY ListHead);

#endif //_INMAGE_MSCS_H_
