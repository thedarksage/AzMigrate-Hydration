#pragma once

typedef struct _WRITE_META_DATA {
    LONGLONG    llOffset;
    ULONG       ulLength;
    ULONG TimeDelta;
    ULONG SequenceNumberDelta;
} WRITE_META_DATA, *PWRITE_META_DATA;


