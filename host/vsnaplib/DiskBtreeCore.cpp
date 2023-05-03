//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : DiskBtreeCore.cpp
//
// Description: Core Disk-based Btree algorithm class definitions.
//

#include "DiskBtreeCore.h"
#if defined(VSNAP_WIN_KERNEL_MODE)
	#include "WinVsnapKernelHelpers.h"
#endif
#include <string.h>
typedef unsigned long long ULONGLONG;
#include "DiskBtreeHelpers.h"
//#include "DiskBtreeCore.h"

#define FILE_END            2
/********************* PUBLIC INTERFACE DEFINITIONS ********************************/

void DiskBtree::Init(int KeySz)
{
    int order = 0;

    m_MaxKeysInNode = (DISK_PAGE_SIZE - sizeof(ULONGLONG) - sizeof(int))/(KeySz + sizeof(ULONGLONG));
    if(0 == (m_MaxKeysInNode % 2))
        m_MaxKeysInNode -= 1;

    order = (m_MaxKeysInNode + 1)/2;
    m_MinKeysInNode = order - 1;
    m_KeySize = KeySz;
    m_BtreeMapHandle = ACE_INVALID_HANDLE;
    m_RootNode = NULL;
    m_Header = (BtreeHeader *)NULL;
    m_RootNodeOffset = 0;
    CallBack = NULL;
    CallBackParam = NULL;
    m_InitFromHandle = false;
}

void DiskBtree::Uninit()
{
    if(!m_InitFromHandle && (ACE_INVALID_HANDLE != m_BtreeMapHandle))
        CLOSE_FILE(m_BtreeMapHandle);

    if(NULL != m_RootNode)
    {
        //FREE_MEMORY(m_RootNode);
        FREE_ALIGNED_MEMORY_FOR_NODE(m_RootNode);
    }
    if(NULL != m_Header)
        FREE_MEMORY(m_Header);
    m_BtreeMapHandle = ACE_INVALID_HANDLE;
    m_RootNode			= NULL;
    m_Header			= NULL;
}

int DiskBtree::InitFromDiskFileName(const char *FileName, unsigned int OpenMode, unsigned int SharedMode)
{
    bool Success = true;

    do
    {
        if(!OPEN_FILE(FileName, OpenMode, SharedMode, &m_BtreeMapHandle))
        {
            Success = false;
            break;
        }

        if(!GetHeader() )
        {
            if(!InitBtreeMapFile())
                Success = false;
            break;
        }
        else
        {
            if(!GetRootNode())
            {
                Success = false;
                break;
            }

            m_MaxKeysInNode = m_Header->MaxKeysPerNode;
            m_MinKeysInNode = m_Header->MinKeysPerNode;
            m_KeySize		= m_Header->KeySize;
            m_RootNodeOffset= m_Header->RootOffset;
        }
    } while (FALSE);

    if(!Success)
    {
        if(m_BtreeMapHandle != ACE_INVALID_HANDLE)
            CLOSE_FILE(m_BtreeMapHandle);

        //if(m_Header != NULL)
        //	FREE_MEMORY(m_Header);
        if(m_RootNode != NULL)
        {
            //FREE_MEMORY(m_RootNode);
            FREE_ALIGNED_MEMORY_FOR_NODE(m_RootNode);
        }

        m_BtreeMapHandle = ACE_INVALID_HANDLE;
        m_Header = NULL;
        m_RootNode = NULL;
    }

    return Success;
}

int DiskBtree::InitFromMapHandle(ACE_HANDLE MapHandle)
{
    bool Success = true;

    do
    {
        m_BtreeMapHandle = MapHandle;
        m_InitFromHandle = true;

        if(!GetHeader())
        {
            Success = false;
            break;
        }
	
        if(!GetRootNode())
        {
            Success = false;
            break;
        }

        m_MaxKeysInNode = m_Header->MaxKeysPerNode;
        m_MinKeysInNode = m_Header->MinKeysPerNode;
        m_KeySize		= m_Header->KeySize;
        m_RootNodeOffset = m_Header->RootOffset;
    } while (FALSE);

    return Success;
}

int DiskBtree::WriteHeader(void *Hdr)
{
	memcpy(m_Header, Hdr, BTREE_HEADER_SIZE);

	if(!DiskWrite(0, m_Header, BTREE_HEADER_SIZE))
		return false;

	return true;
}

int DiskBtree::GetHeader(void *Hdr)
{
    if(ACE_INVALID_HANDLE == m_BtreeMapHandle)
        return false;

    if(!GetHeader())
        return false;

    memcpy(Hdr, m_Header, BTREE_HEADER_SIZE);

    return true;
}

int DiskBtree::GetRootNode(void *Root)
{
    if((ACE_INVALID_HANDLE == m_BtreeMapHandle) || (NULL == m_RootNode))
        return false;

    if(!GetHeader())
        return false;

    if(!GetRootNode())
        return false;
	
    /**
    * 
    * IMPORTANT README:
    * 
    * This function is not getting called at all.
    * The Root also has to get memory allocated
    * using the function "ALLOCATE_ALIGNED_MEMORY_FOR_NODE"
    * and free using "FREE_ALIGNED_MEMORY_FOR_NODE".
    * These functions take care of alignment issues
    * specific to some processor like SPARC. 
    *
    */
    memcpy(Root, m_RootNode, DISK_PAGE_SIZE);

    return true;
}

void DiskBtree::SetCallBack(void (*NewCallBack)(void *, void *, void *, bool))
{
	CallBack = NewCallBack;
}

void DiskBtree::ResetCallBack()
{
	CallBack = NULL;
}

void DiskBtree::SetCallBackThirdParam(void *Param)
{
	CallBackParam = Param;
}

void DiskBtree::ResetCallBackParam()
{
	CallBackParam = NULL;
}

/********************* PRIVATE INTERFACE DEFINITIONS ********************************/

int DiskBtree::GetHeader()
{
    if(ACE_INVALID_HANDLE == m_BtreeMapHandle)
        return false;
    if(m_Header == NULL)
    {
        m_Header = (BtreeHeader *)ALLOC_MEMORY(BTREE_HEADER_SIZE);
        if(!m_Header)
            return false;
    }
    else
        return true;

    if(!READ_FILE(m_BtreeMapHandle, m_Header, 0, BTREE_HEADER_SIZE, NULL))
    {
        FREE_MEMORY(m_Header);
        m_Header = NULL;
        return false;
    }
    return true;
}

int DiskBtree::GetRootNode()
{
    if(ACE_INVALID_HANDLE == m_BtreeMapHandle)
        return false;

    if(!GetHeader())
        return false;
    if(m_RootNode == NULL)
    {
        //m_RootNode = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
        m_RootNode = (void *)ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
        if(!m_RootNode)
            return false;
    }
    else
        return true;

    if(!READ_FILE(m_BtreeMapHandle, m_RootNode, m_Header->RootOffset, DISK_PAGE_SIZE, NULL))
    {
        //FREE_MEMORY(m_RootNode);
        FREE_ALIGNED_MEMORY_FOR_NODE(m_RootNode);
        m_RootNode = NULL;
        return false;
    }
    return true;
}

int DiskBtree::DiskRead(ULONGLONG Offset, void *Page, int Size)
{
    if((ACE_INVALID_HANDLE == m_BtreeMapHandle) || (NULL == Page))
        return false;
    if(!READ_FILE(m_BtreeMapHandle, Page, Offset, Size, NULL))
        return false;
    return true;
}

int DiskBtree::DiskWrite(ULONGLONG Offset, void *Page, int Size)
{
    if((ACE_INVALID_HANDLE == m_BtreeMapHandle) || (NULL == Page))
        return false;
    if(!WRITE_FILE(m_BtreeMapHandle, Page, Offset, Size, NULL))
        return false;
    return true;
}

int DiskBtree::GetFreeMapFileOffset(ULONGLONG *NewFileOffset)
{
    if(!SEEK_FILE(m_BtreeMapHandle, 0, NewFileOffset, FILE_END))
        return false;

    return true;
}

ULONGLONG DiskBtree::GetBtreeRootNodeOffset()
{
	return m_RootNodeOffset;
}

int DiskBtree::InitBtreeMapFile()
{
	int Success = true;

	do
	{
		m_Header = (BtreeHeader*)ALLOC_MEMORY(BTREE_HEADER_SIZE);	
		if(!m_Header)
		{
			Success = false;
			break;
		}
		
		memset(m_Header, 0, BTREE_HEADER_SIZE);

		m_Header->RootOffset = BTREE_HEADER_SIZE;
		m_Header->MaxKeysPerNode = m_MaxKeysInNode;
		m_Header->MinKeysPerNode = m_MinKeysInNode;
		m_Header->KeySize = m_KeySize;
		m_Header->version = 1;
		
		if(!DiskWrite(0, m_Header, BTREE_HEADER_SIZE))
		{
			Success = false;
			break;
		}

		//m_RootNode = (void *) ALLOC_MEMORY(DISK_PAGE_SIZE);
		m_RootNode = (void *) ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
		if(!m_RootNode)
		{
			Success = false;
			break;
		}

		memset(m_RootNode, 0, DISK_PAGE_SIZE);

		if(!DiskWrite(m_Header->RootOffset, m_RootNode, DISK_PAGE_SIZE))
		{
			Success = false;
			break;
		}

		m_RootNodeOffset = m_Header->RootOffset;

	} while (FALSE);

	if(!Success)
	{
		if(m_Header)
			FREE_MEMORY(m_Header);
		if(m_RootNode)
        {
            //FREE_MEMORY(m_RootNode);
			FREE_ALIGNED_MEMORY_FOR_NODE(m_RootNode);
        }
	}

	return Success;
}
