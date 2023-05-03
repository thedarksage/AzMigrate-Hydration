//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapCommon.h
//
// Description: Common Header file used by both User/Kernel mode Vsnap library.
//

#ifndef _VSNAP_COMMON_H
#define _VSNAP_COMMON_H

#include "DiskBtreeHelpers.h"

#if defined(VSNAP_WIN_KERNEL_MODE)
#include "WinVsnapKernelHelpers.h"
#endif

#if defined(VSNAP_USER_MODE)
#if defined(SV_WINDOWS)
#include<windows.h>
#endif
extern void PushEntryList(PSINGLE_LIST_ENTRY, PSINGLE_LIST_ENTRY);
#endif

#define VSNAP_RETENTION_FILEID_MASK	0x80000000
#define DEFAULT_VOLUME_SECTOR_SIZE	512

#define SET_WRITE_BIT_FOR_FILEID(x)		(x | VSNAP_RETENTION_FILEID_MASK)
#define RESET_WRITE_BIT_FOR_FILEID(x)	(x &= ~VSNAP_RETENTION_FILEID_MASK)

#endif