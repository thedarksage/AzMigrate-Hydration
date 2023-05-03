#include "ListEntry.h"

/*
void __inline InitializeListHead( IN PLIST_ENTRY ListHead );

bool __inline IsListEmpty( IN PLIST_ENTRY ListHead );

bool __inline RemoveEntryList( IN PLIST_ENTRY Entry );

PLIST_ENTRY __inline RemoveHeadList( IN PLIST_ENTRY ListHead );

PLIST_ENTRY __inline RemoveTailList( IN PLIST_ENTRY ListHead );

void __inline InsertTailList( IN PLIST_ENTRY ListHead, IN PLIST_ENTRY Entry );

void __inline InsertHeadList( IN PLIST_ENTRY ListHead, IN PLIST_ENTRY Entry );
*/

void
__inline
InitializeListHead(
    IN PLIST_ENTRY ListHead
    )
{
    ListHead->Flink = ListHead->Blink = ListHead;
}

//
//  BOOLEAN
//  IsListEmpty(
//      PLIST_ENTRY ListHead
//      );
//

bool
__inline
IsListEmpty(
    IN PLIST_ENTRY ListHead
    )
{
    return ((ListHead)->Flink == (ListHead));
}

bool
__inline
RemoveEntryList(
    IN PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Flink;

    Flink = Entry->Flink;
    Blink = Entry->Blink;
    Blink->Flink = Flink;
    Flink->Blink = Blink;
    return (bool)(Flink == Blink);
}

PLIST_ENTRY
__inline
RemoveHeadList(
    IN PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Flink;
    PLIST_ENTRY Entry;

    Entry = ListHead->Flink;
    Flink = Entry->Flink;
    ListHead->Flink = Flink;
    Flink->Blink = ListHead;
    return Entry;
}



PLIST_ENTRY
__inline
RemoveTailList(
    IN PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Entry;

    Entry = ListHead->Blink;
    Blink = Entry->Blink;
    ListHead->Blink = Blink;
    Blink->Flink = ListHead;
    return Entry;
}


void
__inline
InsertTailList(
    IN PLIST_ENTRY ListHead,
    IN PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Blink;

    Blink = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Blink;
    Blink->Flink = Entry;
    ListHead->Blink = Entry;
}


void
__inline
InsertHeadList(
    IN PLIST_ENTRY ListHead,
    IN PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY Flink;

    Flink = ListHead->Flink;
    Entry->Flink = Flink;
    Entry->Blink = ListHead;
    Flink->Blink = Entry;
    ListHead->Flink = Entry;
}

