#ifndef _CLUS_DISK_NODE_H_
#define _CLUS_DISK_NODE_H_

#if (NTDDI_VERSION >= NTDDI_VISTA)
typedef struct _CLUSDISK_NODE {
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    etPartitionStyle    ePartitionStyle;
    union {
        ULONG       ulDiskSignature;
        GUID        DiskGuid;
    };
    PVOID       Context;
} CLUSDISK_NODE, *PCLUSDISK_NODE;

NTSTATUS
AddDiskGUIDToClusDiskList(GUID DiskGuid, PVOID Context);

PCLUSDISK_NODE
GetClusDiskNodeFromDiskGuid(GUID DiskGuid);

NTSTATUS
RemoveClusGPTDiskFromClusDiskList(GUID DiskGuid);
#else
enum DiskState {
	DISK_ACQUIRED,
	DISK_RELEASED
};
typedef struct _CLUSDISK_NODE {
    LIST_ENTRY  ListEntry;
    LONG        lRefCount;
    ULONG       ulDiskSignature;
    PVOID       Context;
    enum DiskState	  State;
} CLUSDISK_NODE, *PCLUSDISK_NODE;
#endif

PCLUSDISK_NODE
GetClusDiskNodeFromContext(PVOID Context);

PCLUSDISK_NODE
GetClusDiskNodeFromSignature(ULONG ulSignature);

NTSTATUS
AddSignatureToClusDiskList(ULONG ulSignature,PVOID FsContext);

NTSTATUS
RemoveClusDiskFromClusDiskList(ULONG ulSignature);

PCLUSDISK_NODE
AllocateClusDiskNode();

PCLUSDISK_NODE
ReferenceClusDiskNode(PCLUSDISK_NODE pClusDiskNode);

LONG
DereferenceClusDiskNode(PCLUSDISK_NODE pClusDiskNode);

#endif _CLUS_DISK_NODE_H_