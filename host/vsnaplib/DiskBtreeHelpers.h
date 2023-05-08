//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : DiskBtreeHelpers.h
//
// Description: Helper function declarations used by the btree module.
//
//#include "svtypes.h"
#ifndef _DISK_BTREE_HELPERS_H
#define _DISK_BTREE_HELPERS_H

#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_fcntl.h>

bool OPEN_FILE(const char *, unsigned int, unsigned int, ACE_HANDLE *);
bool READ_FILE(ACE_HANDLE, void *, unsigned long long, unsigned int , int *);
bool WRITE_FILE(ACE_HANDLE, void *, unsigned long long, unsigned int , int *);
bool SEEK_FILE(ACE_HANDLE, unsigned long long, unsigned long long*, unsigned int );
void CLOSE_FILE(ACE_HANDLE);
bool DELETE_FILE(char *);
//bool OPEN_RAW_FILE(char *, void *);
//bool READ_RAW_FILE(void *, void *, unsigned long long, unsigned int , int *);
//void CLOSE_RAW_FILE(void *);

void *ALLOC_MEMORY(int);
void FREE_MEMORY(void *);

void *ALLOCATE_ALIGNED_MEMORY_FOR_NODE(int Size);
void FREE_ALIGNED_MEMORY_FOR_NODE(void *tofree);

/**
 * 
 * 
 * Currently define pad size only for
 * sparc. Other processors may be added
 * as needed.
 *
 *
 */

#undef ALIGNMENT_PAD_SIZE
#ifdef __sparc__
#define ALIGNMENT_PAD_SIZE 4
#else
#define ALIGNMENT_PAD_SIZE 0
#endif /* __sparc__ */

#endif
