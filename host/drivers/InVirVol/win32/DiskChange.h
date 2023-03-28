#ifndef _DISK_CHANGE_H_
#define _DISK_CHANGE_H_

#define DATA_SOURCE_MEMORY_BUFFER   0x0001
//Additional data sources could be virtual disk (volume image) and retenetion log

struct _DATA_SOURCE;
struct _DISK_CHANGE;

typedef PVOID (*GET_DATA)(IN _DATA_SOURCE *);

typedef VOID (__stdcall *FIND_CHANGE_IN_CONTEXT)(
                    IN _DISK_CHANGE* DiskChangeToBeSearched, 
                    IN PVOID SearchContext, 
                    OUT _DISK_CHANGE* PartOfDiskChangeFound);

typedef struct _MEMORY_BUFFER
{
    PCHAR Address;
}MEMORY_BUFFER, *PMEMORY_BUFFER;

typedef struct _DATA_SOURCE
{
    ULONG       Type;
    GET_DATA    GetData;
    union{
        MEMORY_BUFFER MemoryBuffer;
    }u;
}DATA_SOURCE, *PDATA_SOURCE;

typedef struct _DISK_CHANGE
{
    LIST_ENTRY          ListEntry;
    LARGE_INTEGER       ByteOffset;
    ULONG               Length;
    PDATA_SOURCE        DataSource;
}DISK_CHANGE, *PDISK_CHANGE;

VOID
SplitDiskChange(
    PDISK_CHANGE DiskChange, 
    PVOID SearchContext, 
    FIND_CHANGE_IN_CONTEXT FindChangeInContext, 
    PLIST_ENTRY SplitDiskChangeList
);

VOID
SearchChangeInDBList(
    IN PDISK_CHANGE DiskChangeToBeSearched, 
    IN PVOID SearchContext, 
    OUT PDISK_CHANGE PartOfDiskChangeFound
);

inline PVOID GetMemoryBufferData (IN _DATA_SOURCE *DataSource)
{
    return DataSource->u.MemoryBuffer.Address;
}

//#define USER_MODE_TEST
#ifdef USER_MODE_TEST

PVOID ExAllocatePoolWithTag(DWORD Type, DWORD Length, DWORD Tag);
VOID ExFreePool(PVOID Buffer);
LONGLONG GenerateNextOffsetSerial(ULONG Index, ULONG StartOffset, ULONG Length, ULONG Distance);
void AddChangesToList(ULONG NumberOfChanges, PLIST_ENTRY ListHead);
void TestSearchChangeInDBList();
void TestCaseExecuteSearchChangeInDBList(LONGLONG Offset, ULONG Length, PLIST_ENTRY ListHead);
void TestSplitDiskChange();
void TestCaseExecuteSplitDiskChange(LONGLONG Offset, ULONG Length, PLIST_ENTRY ListHead);

#define VVTAG_GENERIC_NONPAGED  'nDRV'
#define NonPagedPool    0x0001
void PrintDiskChange(PDISK_CHANGE DiskChange);

#endif  //USER_MODE_TEST

#endif
