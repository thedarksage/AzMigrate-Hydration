#ifndef USER_MODE_TEST
#include "VVCommon.h"
#endif

#include "VVCommon.h"

#include "DiskChange.h"

VOID
SplitDiskChange(
    PDISK_CHANGE SourceDiskChange, 
    PVOID SearchContext, 
    FIND_CHANGE_IN_CONTEXT FindChangeInContext, 
    PLIST_ENTRY SplitDiskChangeList
)
{
    LIST_ENTRY SourceChangeList;
    PDISK_CHANGE NewDiskChange = NULL;

    InitializeListHead(&SourceChangeList);
    
    //Insert copy of original change as after split the changes get freed in while loop
    NewDiskChange = (PDISK_CHANGE)ExAllocatePoolWithTag(NonPagedPool, sizeof(DISK_CHANGE), VVTAG_GENERIC_NONPAGED);
    if(!NewDiskChange)
        return;
    
    NewDiskChange->ByteOffset   = SourceDiskChange->ByteOffset;
    NewDiskChange->Length       = SourceDiskChange->Length;
    NewDiskChange->DataSource   = SourceDiskChange->DataSource;

    InsertHeadList(&SourceChangeList, &NewDiskChange->ListEntry);

	while(!IsListEmpty(&SourceChangeList))
	{
        PLIST_ENTRY ListEntry = RemoveHeadList(&SourceChangeList);
        PDISK_CHANGE CurrChange = (PDISK_CHANGE) ListEntry;
        DISK_CHANGE PartialChange;
        LONGLONG FirstChangeLength;
        LONGLONG MidChangeLength;
        LONGLONG LastChangeLength;

        PartialChange = *CurrChange;
        FindChangeInContext(CurrChange, SearchContext, &PartialChange);

        //  |--------------------------------------------|  Change Before Split
        //  |------------|-----------------------|-------|  Maximum Three changes after split
        //      First           Mid                 Last

        FirstChangeLength   = PartialChange.ByteOffset.QuadPart - CurrChange->ByteOffset.QuadPart;
        MidChangeLength     = PartialChange.Length;
        LastChangeLength    = (CurrChange->ByteOffset.QuadPart + CurrChange->Length) - 
                                            (PartialChange.ByteOffset.QuadPart + PartialChange.Length);

        if(FirstChangeLength)
        {
            NewDiskChange  = (PDISK_CHANGE)ExAllocatePoolWithTag(NonPagedPool, sizeof(DISK_CHANGE), VVTAG_GENERIC_NONPAGED);
            if(!NewDiskChange)
                break;
            NewDiskChange->ByteOffset   = CurrChange->ByteOffset;
            NewDiskChange->Length       = (ULONG)FirstChangeLength;
            NewDiskChange->DataSource   = CurrChange->DataSource;
            InsertHeadList(&SourceChangeList, &NewDiskChange->ListEntry);
        }

        if(MidChangeLength)
        {
            NewDiskChange  = (PDISK_CHANGE)ExAllocatePoolWithTag(NonPagedPool, sizeof(DISK_CHANGE), VVTAG_GENERIC_NONPAGED);
            if(!NewDiskChange)
                break;
            NewDiskChange->ByteOffset   = PartialChange.ByteOffset;
            NewDiskChange->Length       = (ULONG)MidChangeLength;
            NewDiskChange->DataSource   = PartialChange.DataSource;
            InsertHeadList(SplitDiskChangeList, &NewDiskChange->ListEntry);
        }

        if(LastChangeLength)
        {
            NewDiskChange          = (PDISK_CHANGE)ExAllocatePoolWithTag(NonPagedPool, sizeof(DISK_CHANGE), VVTAG_GENERIC_NONPAGED);
            if(!NewDiskChange)
                break;
            NewDiskChange->ByteOffset.QuadPart  = PartialChange.ByteOffset.QuadPart + PartialChange.Length;
            NewDiskChange->Length               = (ULONG)LastChangeLength;
            NewDiskChange->DataSource           = CurrChange->DataSource;
            InsertHeadList(&SourceChangeList, &NewDiskChange->ListEntry);
        }

        ExFreePool(CurrChange);
	}
}

VOID
SearchChangeInDBList(
    IN PDISK_CHANGE DiskChangeToBeSearched, 
    IN PVOID SearchContext, 
    OUT PDISK_CHANGE PartOfDiskChangeFound
)
{
    PLIST_ENTRY ListHead = (PLIST_ENTRY) SearchContext;
    PLIST_ENTRY CurNode = ListHead->Blink;

    while(ListHead != CurNode)
    {
        PDISK_CHANGE CurChange = (PDISK_CHANGE) CurNode;
        LONGLONG S1, S2, E1, E2;
        LONGLONG StartOffset, EndOffset;
        S1 = CurChange->ByteOffset.QuadPart;
        E1 = CurChange->ByteOffset.QuadPart + CurChange->Length;
        S2 = DiskChangeToBeSearched->ByteOffset.QuadPart;
        E2 = DiskChangeToBeSearched->ByteOffset.QuadPart + DiskChangeToBeSearched->Length;
        
        // |---------------------------|    CurChange
        // S1                          E1
        //          |----------------------------------------|      DiskChangeToBeSearched
        //          S2                                       E2
        //
        //S1: StartOffset                                    E2: EndOffset

        //          |-------------------|   PartOfDiskChangeFound
        //          S12                 E12

        StartOffset = S1 > S2 ? S2 : S1;                                
        EndOffset   = E1 > E2 ? E1 : E2;

        if(EndOffset - StartOffset < CurChange->Length + DiskChangeToBeSearched->Length)    //Overlap
        {
            PartOfDiskChangeFound->DataSource = (PDATA_SOURCE) ExAllocatePoolWithTag(NonPagedPool, sizeof(DATA_SOURCE), VVTAG_GENERIC_NONPAGED);
            if(!PartOfDiskChangeFound->DataSource)
                break;

            PartOfDiskChangeFound->ByteOffset.QuadPart = S1 > S2 ? S1 : S2;
            PartOfDiskChangeFound->Length = (ULONG)((CurChange->Length + DiskChangeToBeSearched->Length) - (EndOffset - StartOffset));

            PartOfDiskChangeFound->DataSource->Type = DATA_SOURCE_MEMORY_BUFFER;
            PartOfDiskChangeFound->DataSource->u.MemoryBuffer.Address = S1 > S2 ? CurChange->DataSource->u.MemoryBuffer.Address:
                                            CurChange->DataSource->u.MemoryBuffer.Address + (S2 - S1);
            PartOfDiskChangeFound->DataSource->GetData = GetMemoryBufferData;

            break;
        }
        CurNode = CurNode->Blink;
    }
}

#ifdef USER_MODE_TEST
#include <stdio.h>
#include <tchar.h>
#include "ListEntry.h"

//User Mode Testing
PVOID ExAllocatePoolWithTag(DWORD Type, DWORD Length, DWORD Tag)
{
    return malloc(Length);
}

VOID ExFreePool(PVOID Buffer)
{
    free(Buffer);
}

void TestSplitDiskChange()
{
    const ULONG NumberOfChanges = 5;
    LIST_ENTRY ListHead;
    InitializeListHead(&ListHead);

    //Test1: Empty Search List
    _tprintf(_T("Test1: Empty Search List\n"));
    TestCaseExecuteSplitDiskChange(1000, 20, &ListHead);

    AddChangesToList(NumberOfChanges, &ListHead);

    //Test2: Change spanning single block
    _tprintf(_T("Test2: Change spanning single block\n"));
    //SubTest11: First Block
    TestCaseExecuteSplitDiskChange(1000, 20, &ListHead);

    //SubTest12: Middle Block
    TestCaseExecuteSplitDiskChange(1045, 20, &ListHead);

    //SubTest13: Last Block
    TestCaseExecuteSplitDiskChange(1165, 20, &ListHead);

    //Test3: Change spanning multiple blocks
    _tprintf(_T("Test3: Change spanning multiple blocks\n"));
    //SubTest31: Including First Block
    TestCaseExecuteSplitDiskChange(1000, 90, &ListHead);

    //SubTest32: Excluding First And Last Blocks
    TestCaseExecuteSplitDiskChange(1030, 80, &ListHead);

    //SubTest33: Including Last Block
    TestCaseExecuteSplitDiskChange(1100, 100, &ListHead);

    //SubTest34: All Blocks
    TestCaseExecuteSplitDiskChange(1000, 200, &ListHead);
    TestCaseExecuteSplitDiskChange(990, 250, &ListHead);

    //Test4: Change spanning no blocks
    _tprintf(_T("Test4: Change spanning no blocks\n"));
    //SubTest41: Before First Block
    TestCaseExecuteSplitDiskChange(980, 15, &ListHead);

    //SubTest42: In Between Middle Blocks
    TestCaseExecuteSplitDiskChange(1060, 20, &ListHead);

    //SubTest43: After Last Block
    TestCaseExecuteSplitDiskChange(1200, 30, &ListHead);
}

void TestCaseExecuteSplitDiskChange(LONGLONG Offset, ULONG Length, PLIST_ENTRY ListHead)
{
    DISK_CHANGE DiskChangeToBeSearched;
    LIST_ENTRY SplitDiskChangeList;
    
    DiskChangeToBeSearched.ByteOffset.QuadPart = Offset;
    DiskChangeToBeSearched.Length = Length;
    DiskChangeToBeSearched.DataSource = NULL;

    InitializeListHead(&SplitDiskChangeList);
    SplitDiskChange(&DiskChangeToBeSearched, ListHead, &SearchChangeInDBList, &SplitDiskChangeList);
    
    PrintDiskChange(&DiskChangeToBeSearched);
    _tprintf("\n");
    while(!IsListEmpty(&SplitDiskChangeList))
    {
        PDISK_CHANGE PartOfDiskChangeFound = (PDISK_CHANGE) RemoveHeadList(&SplitDiskChangeList);
        PrintDiskChange(PartOfDiskChangeFound);
    }
    _tprintf("\n\n");
}

void TestSearchChangeInDBList()
{
    const ULONG NumberOfChanges = 5;
    LIST_ENTRY ListHead;
    InitializeListHead(&ListHead);
    AddChangesToList(NumberOfChanges, &ListHead);

    //Test 1: ToBeSearched Offset includes Search offset
    TestCaseExecuteSearchChangeInDBList(990, 100, &ListHead);

    //Test 2: Search Offset includes ToBeSearched offset
    TestCaseExecuteSearchChangeInDBList(1005, 10, &ListHead);

    //Test 3: Search Offset precedes ToBeSearched offset    Overlapped
    TestCaseExecuteSearchChangeInDBList(995, 15, &ListHead);

    //Test 4: ToBeSearched Offset precedes Search offset    Overlapped
    TestCaseExecuteSearchChangeInDBList(1005, 20, &ListHead);

    //Test 4: ToBeSearched Offset precedes Search offset    Not Overlapped
    TestCaseExecuteSearchChangeInDBList(990, 10, &ListHead);

    //Test 4: Search Offset precedes ToBeSearched offset    Not Overlapped
    TestCaseExecuteSearchChangeInDBList(1030, 10, &ListHead);
}

void TestCaseExecuteSearchChangeInDBList(LONGLONG Offset, ULONG Length, PLIST_ENTRY ListHead)
{
    DISK_CHANGE DiskChangeToBeSearched;
    DISK_CHANGE PartOfDiskChangeFound;
    PartOfDiskChangeFound.ByteOffset.QuadPart = 0;
    PartOfDiskChangeFound.Length = 0;
    PartOfDiskChangeFound.DataSource = NULL;

    DiskChangeToBeSearched.ByteOffset.QuadPart = Offset;
    DiskChangeToBeSearched.Length = Length;
    DiskChangeToBeSearched.DataSource = NULL;

    SearchChangeInDBList(&DiskChangeToBeSearched, ListHead, &PartOfDiskChangeFound);
    PrintDiskChange(&DiskChangeToBeSearched);
    PrintDiskChange(&PartOfDiskChangeFound);
    _tprintf("\n");
}

void AddChangesToList(ULONG NumberOfChanges, PLIST_ENTRY ListHead)
{
    ULONG i = 0;
    const ULONG ChangeLength = 20;
    const ULONG DistanceBetTwoChanges = 20;
    const ULONG BufferStartOffset = 1000;

    for(i = 0; i < NumberOfChanges; i++)
    {
        PDISK_CHANGE DiskChange = new DISK_CHANGE;
        DiskChange->ByteOffset.QuadPart = GenerateNextOffsetSerial(i, BufferStartOffset, ChangeLength, DistanceBetTwoChanges);
        DiskChange->Length = ChangeLength;
        DiskChange->DataSource = new DATA_SOURCE;
        DiskChange->DataSource->GetData = NULL;
        DiskChange->DataSource->u.MemoryBuffer.Address = 0;

        InsertHeadList(ListHead, &DiskChange->ListEntry);
    }
}

LONGLONG GenerateNextOffsetSerial(ULONG Index, ULONG StartOffset, ULONG Length, ULONG Distance)
{
    return (StartOffset + Index * (Length + Distance));
}

void PrintDiskChange(PDISK_CHANGE DiskChange)
{
    _tprintf(_T("ByteOffset:%I64d\n"), DiskChange->ByteOffset.QuadPart);
    _tprintf(_T("Length:%d\n"), DiskChange->Length);
    if(!DiskChange->DataSource)
        return;

    _tprintf(_T("DataSource Type:%d\n"), DiskChange->DataSource->Type);
    _tprintf(_T("DataSource Address:%#p\n"), DiskChange->DataSource->u.MemoryBuffer.Address);
}

#endif  //USER_MODE_TEST
