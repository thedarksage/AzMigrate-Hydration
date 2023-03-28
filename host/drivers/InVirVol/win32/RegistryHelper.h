#ifndef _REGISTRY_HELPER_H_
#define _REGISTRY_HELPER_H_
#include <ntifs.h>

NTSTATUS OpenKeyReadWrite(PUNICODE_STRING keyName, HANDLE  *hRegistry,BOOLEAN bCreateIfFail);
NTSTATUS ReadULONG(HANDLE hRegistry,PCWSTR nameString, ULONG * value, ULONG32 ulDefaultValue, BOOLEAN bCreateIfFail);
NTSTATUS WriteULONG(HANDLE hRegistry, PCWSTR nameString, ULONG32 value);
NTSTATUS CloseKey(HANDLE hRegistry);



#endif