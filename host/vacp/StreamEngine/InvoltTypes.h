#pragma once

#ifndef _INMAGE_TYPES_H_
#define _INMAGE_TYPES_H_

#include <windows.h>
#include <winbase.h>

typedef __int64				LONGLONG;	//WINDOWS
typedef long				LONG;
typedef int					INT;
typedef char				CHAR;
typedef short				SHORT;
//typedef void				VOID;

//typedef unsigned long long	ULONGLONG;
typedef unsigned __int64	ULONGLONG; 		//WINDOWS
typedef unsigned long		ULONG;
typedef unsigned int		UINT;
typedef unsigned char		UCHAR;
typedef unsigned short		USHORT;


typedef unsigned int		STATUS; // STATUS
typedef STATUS				RETURNCODE;

//typedef unsigned char		WCHAR;

typedef UINT				UINT32;
typedef ULONGLONG			UINT64;
typedef UINT				ULONG32;
typedef ULONGLONG			ULONG64;


//typedef struct _LARGE_INTEGER {
//    LONGLONG QuadPart;
//} LARGE_INTEGER;



typedef LONGLONG			*PLONGLONG;
typedef LONG				*PLONG;
typedef INT					*PINT;
typedef CHAR				*PCHAR;
typedef SHORT				*PSHORT;
typedef VOID				*PVOID;
typedef LARGE_INTEGER		*PLARGE_INTEGER;

typedef ULONGLONG			*PULONGLONG;
typedef ULONG				*PULONG;
typedef UINT				*PUINT;
typedef UCHAR				*PUCHAR;
typedef USHORT				*pUSHORT;

#ifndef NULL
#define NULL				((VOID *)0)
#endif /* NULL */
 
#endif /* _INMAGE_TYPES_H_ */
