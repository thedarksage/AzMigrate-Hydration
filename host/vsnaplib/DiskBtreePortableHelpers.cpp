//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : DiskBtreeHelpers.cpp
//
// Description: Helper functions used by the btree module.
//

#include "DiskBtreeHelpers.h"
#include "portablehelpers.h"
#include "inmsafeint.h"
#include "inmageex.h"

#ifndef SV_WINDOWS
#include "VsnapCommon.h"
#endif

bool OPEN_FILE(const char *FileName, unsigned int OpenMode, unsigned int SharedMode, ACE_HANDLE* Handle)
{
	// PR#10815: Long Path support
    *Handle = ACE_OS::open(getLongPathName(FileName).c_str(), OpenMode, SharedMode);
    	if(*Handle == ACE_INVALID_HANDLE)
		return false ;

    return true;
}

bool READ_FILE(ACE_HANDLE Handle, void *Buffer, unsigned long long FileOffset, unsigned int BufferSize, int *BytesRead)
{
    int AceBytesRead = 0;
    ACE_LOFF_T ret_val = 0, SeekOffset = 0;

    SeekOffset = (ACE_LOFF_T)FileOffset;

    try
    {
        ret_val = ACE_OS::llseek(Handle, SeekOffset, SEEK_SET);
    } catch(...)
    {
        ret_val = -1;
    }

    if(ret_val < 0)
    {
        if(BytesRead != NULL)
            *BytesRead = 0;

        return false;
    }

    try
    {
        AceBytesRead = ACE_OS::read(Handle, Buffer, BufferSize);
    } catch(...)
    {
        AceBytesRead = 0;
    }

    if(AceBytesRead != BufferSize)
    {
        if(BytesRead != NULL)
            *BytesRead = AceBytesRead;

        return false;
    }

    return true;
}

bool WRITE_FILE(ACE_HANDLE Handle, void *Buffer, unsigned long long FileOffset, unsigned int BufferSize, int *BytesWritten)
{
    int AceBytesWritten = 0;
    ACE_LOFF_T ret_val = 0, SeekOffset = 0;

    SeekOffset = (ACE_LOFF_T)FileOffset;

    try
    {
        ret_val = ACE_OS::llseek(Handle, SeekOffset, SEEK_SET);
    } catch(...)
    {
        ret_val = -1;
    }

    if(ret_val < 0)
    {
        if(BytesWritten != NULL)
            *BytesWritten = 0;

        return false;
    }

    try
    {
        AceBytesWritten = ACE_OS::write(Handle, Buffer, BufferSize);
    } catch(...)
    {	
        AceBytesWritten = 0;
    }

    if(AceBytesWritten != BufferSize)
    {
        if(BytesWritten != NULL)
            *BytesWritten = AceBytesWritten;

        return false;
    }

    return true;
}

bool SEEK_FILE(ACE_HANDLE Handle, unsigned long long FileOffset, unsigned long long *NewFileOffset, unsigned int WhereToSeekFrom)
{
    long long NewOffset = 0;

    NewOffset = ACE_OS::llseek(Handle, FileOffset, WhereToSeekFrom);
    if(NewOffset < 0)
    {
        if(NewFileOffset != NULL)
            *NewFileOffset = 0;

        return false;
    }
	
    if(NewFileOffset != NULL)
        *NewFileOffset = NewOffset;

    return true;
}

void CLOSE_FILE(ACE_HANDLE Handle)
{
    try
    {
        ACE_OS::close(Handle);
    } catch (...)
    {
        return;
    }
}

void *ALLOC_MEMORY(int Size)
{
    return malloc(Size);
}

void FREE_MEMORY(void *Buffer)
{
    return free(Buffer);
}

void *ALLOCATE_ALIGNED_MEMORY_FOR_NODE(int Size)
{
    void *pv;
    void *pv_toreturn = NULL;



    size_t len;
    INM_SAFE_ARITHMETIC(len = InmSafeInt<int>::Type(Size) + ALIGNMENT_PAD_SIZE, INMAGE_EX(Size))
    pv = malloc(len);

    if (pv)
    {
        pv_toreturn = (unsigned char *)pv + ALIGNMENT_PAD_SIZE;
    }

    return pv_toreturn;
}

void FREE_ALIGNED_MEMORY_FOR_NODE(void *Buffer)
{
    unsigned char *tofree = (unsigned char *)Buffer - ALIGNMENT_PAD_SIZE;
    free(tofree);
}

bool IsSingleListEmpty(PSINGLE_LIST_ENTRY ListHead)
{
    return (ListHead->Next==NULL);
}

void InitializeEntryList(PSINGLE_LIST_ENTRY ListHead)
{
    ListHead->Next = NULL;
}


void PushEntryList(PSINGLE_LIST_ENTRY ListHead, PSINGLE_LIST_ENTRY Entry)
{
    Entry->Next = ListHead->Next;
    ListHead->Next = Entry;
}

SINGLE_LIST_ENTRY *PopEntryList(PSINGLE_LIST_ENTRY ListHead)
{
    PSINGLE_LIST_ENTRY FirstEntry = NULL;
    
    FirstEntry = ListHead->Next;
    if (FirstEntry != NULL) {     
        ListHead->Next = FirstEntry->Next;
    }
    
    return FirstEntry;
}
