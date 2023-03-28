#pragma once


typedef struct _TAG_STATUS {
    PDEV_CONTEXT    DevContext;
    /*-------------------------------------------------------------------------------------------------------------------
     * Possible values for Status
     * STATUS_PENDING -                 Initialized with this value.
     * STATUS_NOT_FOUND -               GUID for this volume is not found.
     * STATUS_INVALID_DEVICE_STATE -    Filtering is stopped on this volume.
     * STATUS_NO_MEMORY -               Failed to allocate memory.
for disk filter :
     * STATUS_NOT_SUPPORTED             For raw disks
     **/
    NTSTATUS        Status;
} TAG_STATUS, *PTAG_STATUS;


typedef enum _etTagState
{
    ecTagStateApplied         = 0,
    ecTagStateWaitForCommitSnapshot = 1
} etTagState, *petTagState;

typedef struct _TAG_TELEMETRY_COMMON {
    USHORT            usTotalNumDisk;// total number of disks known to driver
    USHORT            usNumProtectedDisk; // total number of driver protected disks
    LARGE_INTEGER     liTagRequestTime;
    LARGE_INTEGER     liTagInsertSystemTimeStamp;
    CHAR              TagMarkerGUID[GUID_SIZE_IN_CHARS + 1];
    ULONG             ulIoCtlCode;
    NTSTATUS          TagStatus;
    NTSTATUS          GlobaIOCTLStatus;
    ULONG             ulNumberOfDevicesTagsApplied;
    ULONGLONG         ullTagState;
    etTagType         TagType;
}TAG_TELEMETRY_COMMON, *PTAG_TELEMETRY_COMMON;

struct  _TAG_NODE;
typedef struct _TAG_NODE TAG_NODE, *PTAG_NODE;
typedef struct _TAG_INFO {
    GUID                    TagGUID;
    bool                    isCommitNotifyTagRequest;
	ULONG                   ulNumberOfDevices;
    ULONG                   ulNumberOfDevicesTagsApplied;
    ULONG                   ulNumberOfDevicesIgnored;
    TAG_TELEMETRY_COMMON    TagCommonAttribs;
    ULONG                   ulFlags;
    PVOID                   pBuffer;
    NTSTATUS                Status;   
    TAG_STATUS              TagStatus[1];
} TAG_INFO, *PTAG_INFO;

typedef struct _TAG_NODE
{
    LIST_ENTRY      ListEntry;
    PIRP            Irp;
    // pTagInfo - Keeps track of TagInformation received in Irp. This is only valid
    //            when TagState is ecTagStateWaitForCommitSnapshot.
    PTAG_INFO       pTagInfo;
    GUID            TagGUID;
    // ulNoOfVolumes -  Keeps track on number of volumes the tag is issed on.
    //                  This includes the volumes where GUID is not found or
    //                  filtering is stopped.
    ULONG           ulNoOfVolumes;
    // ulNoOfTagsCommited - Keeps track of volumes on which tags are issued and
    //                      have been commited.
    ULONG           ulNoOfTagsCommited;
    // ulNoOfVolumesIgnored - Keeps track of volumes on which tags are not issued
    //                        This is due to flags indicating to ignore volumes
    //                        that are filtering stopped and GUIDs not found.
    ULONG           ulNoOfVolumesIgnored;
    etTagStatus     TagStatus;
    etTagStatus     VolumeStatus[1];
} TAG_NODE, *PTAG_NODE;

PTAG_NODE
AllocateTagNode(PTAG_INFO TagInfo, PIRP Irp);

void
DeallocateTagNode(PTAG_NODE TagNode);

// AddStatusAndReturnNodeIfComplete returns TagNode in the following cases
// eTagStatus is not ecTagStatusCommited.
// eTagStatus is ecTagStatusCommited and all volumes have set the Statuses.
// Return value is NULL
// 1. If TagNode does not exist
// 2. If it exists but Status set is ecTagStatusCommited and all volumes havent
//    set there status yet.
PTAG_NODE
AddStatusAndReturnNodeIfComplete(GUID &TagGUID, ULONG ulIndex, etTagStatus eTagStatus);

PTAG_NODE
AddStatusAndReturnNodeIfComplete(PTAG_INFO TagInfo);

PTAG_NODE
RemoveTagNodeFromList(PIRP Irp);

PTAG_NODE
RemoveTagNodeFromList(GUID &TagGUID);

NTSTATUS
AddTagNodeToList(PTAG_INFO TagInfo, PIRP Irp, etTagState eTagState);

void
CopyStatusToOutputBuffer(PTAG_NODE TagNode, PTAG_VOLUME_STATUS_OUTPUT_DATA OutputData);

NTSTATUS
CopyStatusToOutputBuffer(GUID &TagGUID, PTAG_VOLUME_STATUS_OUTPUT_DATA OutputData, ULONG ulOutputDataSize);


