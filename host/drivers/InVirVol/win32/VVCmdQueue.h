#ifndef _VV_COMMAND_QUEUE_H_
#define _VV_COMMAND_QUEUE_H_
#include "VVCommon.h"
//#include "VsnapKernel.h"
#include "VVThreadPool.h"


// 128 characters including NULL.
#define MAX_QUEUE_DESCRIPTION_LEN   0x80
// This flag is set when thread is shutdown. Once this flag is
// set no more commands have to be queued to command queue.
// CmdQueuing routine checks for this flag, and if it is set 
// returns error.
#define CMDQUEUE_FLAGS_THREAD_IS_SHUTDOWN       0x0001

typedef struct _COMMAND_QUEUE {
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    ULONG       ulFlags;
    struct _DEVICE_EXTENSION *DeviceExtension;
    // This is primarily used for identifyin queue for debugging purposes.
    WCHAR       QueueDescription[MAX_QUEUE_DESCRIPTION_LEN];
    LIST_ENTRY  CmdList;
    PCOMMAND_ROUTINE CmdProcessingRoutine;
    KEVENT      WakeEvent;
    PKTHREAD    Thread;
    KSPIN_LOCK  Lock;
} COMMAND_QUEUE, *PCOMMAND_QUEUE;

typedef struct _IOCOMMAND_QUEUE {
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    ULONG       ulFlags;
    struct _DEVICE_EXTENSION *DeviceExtension;
	LIST_ENTRY  CmdList;
    FAST_MUTEX                  hMutex;
    LONG       Count;
} IOCOMMAND_QUEUE, *PIOCOMMAND_QUEUE;

#define COMMAND_FLAGS_THREAD_REQUIRES_SHUTDOWN  0x0001
#define COMMAND_FLAGS_WAITING_FOR_COMPLETION    0x0002

typedef struct _EXPORT_VOLUME_IMAGE_CMD_DATA {
    ULONG           ulFlags;
    UNICODE_STRING  usFileName;
    UNICODE_STRING  usDeviceName;
    UNICODE_STRING  usDeviceLinkName;
    PUCHAR          UniqueVolumeID;  //User defined structure
    ULONG           UniqueVolumeIDLength;
    PUCHAR          VolumeInfo;  //User defined structure
    ULONG           VolumeInfoLength;  //User defined structure
} EXPORT_VOLUME_IMAGE_CMD_DATA, *PEXPORT_VOLUME_IMAGE_CMD_DATA;

typedef struct _READ_CMD_DATA {
    PIRP            Irp;
    ULONG           ulLength;
    LARGE_INTEGER   ByteOffset;
} READ_CMD_DATA, *PREAD_CMD_DATA;

typedef struct _WRITE_CMD_DATA {
    PIRP            Irp;
    ULONG           ulLength;
    LARGE_INTEGER   ByteOffset;
} WRITE_CMD_DATA, *PWRITE_CMD_DATA;

typedef struct _CONTROL_FLAGS_DATA {
    PIRP            Irp;
} CONTROL_FLAGS_DATA, *PCONTROL_FLAGS_DATA;

typedef struct _EXPORT_VOLUME_FROM_LOG_CMD_DATA
{
    ULONG               ulFlags;
    PIRP                Irp;
    USHORT              UniqueVolumeIdLength;
    CHAR                UniqueVolumeID[1022];
    PUCHAR              VolumeInfo;  //User defined structure
    ULONG               VolumeInfoLength;
    UNICODE_STRING      usDeviceName;
    UNICODE_STRING      usDeviceLinkName;
}EXPORT_VOLUME_FROM_LOG_CMD_DATA, *PEXPORT_VOLUME_FROM_LOG_CMD_DATA;


typedef struct _REMOVE_VOLUME_FROM_RETENTION_OR_VOLUME_IMAGE
{
    PIRP                Irp;
    PVOID               VolumeInfo;  //User defined structure
    ULONG               VolumeInfoLength;
}REMOVE_VOLUME_FROM_RETENTION_OR_VOLUME_IMAGE, *PREMOVE_VOLUME_FROM_RETENTION_OR_VOLUME_IMAGE;


typedef struct _COMMAND_STRUCT {
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    ULONG       ulCommand;
    ULONG       ulFlags;
    // Cmd status after completion.
    NTSTATUS    CmdStatus;
    KEVENT      WaitEvent;
    // If this routine is set, instead of calling command processing routine from
    // COMMAND_QUEUE structure, this routine is called.
    PCOMMAND_ROUTINE CmdRoutine;
    // If this routine is set, thread instead of calling dereference on CommandStruct
    // Calls this function. This function has to dereference CommandStruct at end.
    PCOMMAND_ROUTINE FreeRoutine;
    LARGE_INTEGER    ExpiryTickCount;
    struct _DEVICE_EXTENSION *DevExtension;
    union {
        EXPORT_VOLUME_IMAGE_CMD_DATA                       ExportVolumeFromFile;
        EXPORT_VOLUME_FROM_LOG_CMD_DATA                    ExportVolumeFromRetentionLog;
        REMOVE_VOLUME_FROM_RETENTION_OR_VOLUME_IMAGE       RemoveVolumeFromRetentionOrImage;
        
        READ_CMD_DATA                   Read;
        WRITE_CMD_DATA                  Write;
        CONTROL_FLAGS_DATA              CotrolFlags;
    } Cmd;
} COMMAND_STRUCT, *PCOMMAND_STRUCT;



typedef struct _IO_COMMAND_STRUCT {
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    ULONG       ulCommand;
    ULONG       ulFlags;
    // Cmd status after completion.
    NTSTATUS    CmdStatus;
    KEVENT      WaitEvent;
    LARGE_INTEGER    ExpiryTickCount;
    union {
        struct VsnapInfoTag       *Vsnap;
        struct VVolumeInfoTag     *VVolume;
    };
    SINGLE_LIST_ENTRY         HeadForVsnapRetentionData;
    bool                      TargetReadRequired;
    PVOID                     pBuffer;
    PVOID                     pOrigBuffer;
    union {
        READ_CMD_DATA                   Read;
        WRITE_CMD_DATA                  Write;
        CONTROL_FLAGS_DATA              CotrolFlags;
    } Cmd;
} IO_COMMAND_STRUCT, *PIO_COMMAND_STRUCT;


typedef struct _VSNAP_UPDATE_TASK
{
    struct _UPDATE_VSNAP_VOLUME_INPUT        *InputData;
    LONG                             RefCnt;
    PVOID                            Buffer;
    PIRP                             Irp;
    NTSTATUS                         CmdStatus;
}VSNAP_UPDATE_TASK, *PVSNAP_UPDATE_TASK;

VOID
CmdThreadRoutine(PVOID pContext);

/*-----------------------------------------------------------------------------
 * COMMAND_QUEUE allocation, reference and dereference functions
 *----------------------------------------------------------------------------
 */

PCOMMAND_QUEUE
AllocateCmdQueue(struct _DEVICE_EXTENSION *DeviceExtension, PWCHAR CmdQueueDescription, PCOMMAND_ROUTINE CmdRoutine);

PCOMMAND_QUEUE
ReferenceCmdQueue(PCOMMAND_QUEUE pCmdQueue);

BOOLEAN
DereferenceCmdQueue(PCOMMAND_QUEUE pCmdQueue);

NTSTATUS
QueueCmdToCmdQueue(PCOMMAND_QUEUE pCmdQueue, PCOMMAND_STRUCT pCommand);


/*-----------------------------------------------------------------------------
 * COMMAND_STRUCT allocation, reference and dereference functions
 *-----------------------------------------------------------------------------
 */

PCOMMAND_STRUCT
AllocateCommand(VOID);

PCOMMAND_STRUCT
ReferenceCommand(PCOMMAND_STRUCT pCommand);

BOOLEAN
DereferenceCommand(PCOMMAND_STRUCT pCommand);

/*-----------------------------------------------------------------------------
 * IO_COMMAND_QUEUE allocation, reference and dereference functions
 *----------------------------------------------------------------------------
 */

PIOCOMMAND_QUEUE
AllocateIOCmdQueue(struct _DEVICE_EXTENSION *DeviceExtension);

VOID
DeallocateIOCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue);

PIOCOMMAND_QUEUE
ReferenceIOCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue);

BOOLEAN
DereferenceIOCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue);

NTSTATUS
QueueIOCmdToCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue, PIO_COMMAND_STRUCT pCommand);

PIO_COMMAND_STRUCT  RemoveIOCommandFromQueue(PIOCOMMAND_QUEUE pIOCmdQueue);

VOID UndoRemoveIOCommandFromQueue(PIOCOMMAND_QUEUE pIOCmdQueue, PIO_COMMAND_STRUCT pIOCommand) ;






PIO_COMMAND_STRUCT
AllocateIOCommand(VOID);


VOID
DeallocateIOCommand(PIO_COMMAND_STRUCT pCommand);


PIO_COMMAND_STRUCT
ReferenceIOCommand(PIO_COMMAND_STRUCT pCommand);

BOOLEAN
DereferenceIOCommand(PIO_COMMAND_STRUCT pCommand);

VSNAP_TASK*  AllocateTask();

void  DeAllocateTask(VSNAP_TASK *task);

//
//VSNAP_UPDATE_TASK allocation, Refrence and Derefernce function

VSNAP_UPDATE_TASK* AllocateUpdateCommand();


VSNAP_UPDATE_TASK* ReferenceUpdateCommand(VSNAP_UPDATE_TASK *);

bool DereferenceUpdateCommand(VSNAP_UPDATE_TASK *);

void DeallocateUpdateCommand(VSNAP_UPDATE_TASK *);

#endif