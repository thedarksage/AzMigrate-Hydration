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
#include "ntifs.h"
#ifndef _DISK_BTREE_HELPERS_H
#define _DISK_BTREE_HELPERS_H

bool OPEN_FILE(const char *, unsigned int, unsigned int, void **, bool EnableLog = false, NTSTATUS *Status = STATUS_SUCCESS, bool Eventlog = false);
bool READ_FILE(void *, void *, unsigned __int64, unsigned int , int *, bool EnableLog = false, NTSTATUS *Status = STATUS_SUCCESS);
bool WRITE_FILE(void *, void *, unsigned __int64, unsigned int , int *, bool EnableLog = false, NTSTATUS *Status = STATUS_SUCCESS);
bool SEEK_FILE(void *, unsigned __int64, unsigned __int64*, unsigned int );
void CLOSE_FILE(void *);
bool DELETE_FILE(char *);
bool OPEN_RAW_FILE(char *, void *);
bool READ_RAW_FILE(void *, void *, unsigned __int64, unsigned int , int *);
void CLOSE_RAW_FILE(void *);

void *ALLOC_MEMORY(int);
void FREE_MEMORY(void *);

#endif