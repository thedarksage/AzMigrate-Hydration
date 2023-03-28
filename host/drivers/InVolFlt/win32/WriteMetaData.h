#ifndef _WRITE_META_DATA__H
#define _WRITE_META_DATA__H

typedef struct _WRITE_META_DATA {
    LONGLONG    llOffset;
    ULONG       ulLength;
    ULONG TimeDelta;
    ULONG SequenceNumberDelta;
} WRITE_META_DATA, *PWRITE_META_DATA;


#endif // _WRITE_META_DATA__H