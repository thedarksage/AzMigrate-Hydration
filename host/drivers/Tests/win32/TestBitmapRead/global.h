#ifndef _GLOBAL_H
#define _GLOBAL_H

#include <windows.h>
#include <stdio.h>
#include <assert.h>

typedef int RETURNCODE;
#ifdef FUNCTRACE
#define TRC static int coverage##__FUNCTION__##__LINE__; ScopeTrace scopeTrace(&coverage##__FUNCTION__##__LINE__,__FUNCTION__, __FILE__, __LINE__);
#else
#define TRC
#endif

typedef int NTSTATUS;
#define STATUS_SUCCESS   1                   
#define STATUS_FAIL      0               
#define ASSERT assert
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#include "BitmapOperations.h"
RETURNCODE GetNextBitRun(ULONG32 * bitsInRun, ULONG64 *bitOffset);
NTSTATUS ReadAndLock(ULONG64 offset, UCHAR **returnBufferPointer, ULONG32 *partialBufferSize);
NTSTATUS CreateAndWriteFile(void);
NTSTATUS RunTest(FILE *);

#endif