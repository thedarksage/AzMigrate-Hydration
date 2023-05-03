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

#if defined(VSNAP_WIN_KERNEL_MODE)
	#include "WinVsnapKernelHelpers.h"
#endif

#include "DiskBtreeHelpers.h"
#include "DiskBtreeCore.h"

/********************* PUBLIC INTERFACE DEFINITIONS ********************************/

void DiskBtree::Init(int KeySz)
{
	int order = 0;

	m_MaxKeysInNode		= (DISK_PAGE_SIZE - sizeof(ULONGLONG) - sizeof(int))/(KeySz + sizeof(ULONGLONG));
	if(0 == (m_MaxKeysInNode % 2))
		m_MaxKeysInNode -= 1;

	order				= (m_MaxKeysInNode + 1)/2;
	m_MinKeysInNode		= order - 1;
	m_KeySize			= KeySz;
	m_BtreeMapHandle	= NULL;
	m_RootNode			= NULL;
	m_Header			= (BtreeHeader *)NULL;
	m_RootNodeOffset	= 0;
	CallBack			= NULL;
	CallBackParam		= NULL;
	m_InitFromHandle	= false;
	KeInitializeMutex(&m_Mutex,0);
}

void DiskBtree::Uninit()
{
	if(!m_InitFromHandle && (NULL != m_BtreeMapHandle))
		CLOSE_FILE(m_BtreeMapHandle);
	if(NULL != m_RootNode)
		FreeNode(m_RootNode);
	if(NULL != m_Header)
		FREE_MEMORY(m_Header);

	m_BtreeMapHandle	= NULL;
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
		if(m_BtreeMapHandle != NULL)
			CLOSE_FILE(m_BtreeMapHandle);
		if(m_Header != NULL)
			FREE_MEMORY(m_Header);
		if(m_RootNode != NULL)
			//FREE_MEMORY(m_RootNode);
            FreeNode(m_RootNode);

		m_BtreeMapHandle	= NULL;
		m_Header			= NULL;
		m_RootNode			= NULL;
	}

	return Success;
}

int DiskBtree::InitFromMapHandle(void *MapHandle)
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
        if(!InitCache())
        {
            Success = false;
            break;
        }

	} while (FALSE);

	return Success;
}

int DiskBtree::WriteHeader(void *Hdr)
{

    RtlCopyMemory(m_Header, Hdr, BTREE_HEADER_SIZE);
	
	if(!DiskWrite(0, m_Header, BTREE_HEADER_SIZE,0))
		return false;

	return true;
}

int DiskBtree::GetHeader(void *Hdr)
{
	if(NULL == m_BtreeMapHandle)
		return false;

	if(!GetHeader())
		return false;

   
    RtlCopyMemory(Hdr, m_Header, BTREE_HEADER_SIZE);

	return true;
}

int DiskBtree::GetRootNode(void *Root)
{
	if((NULL == m_BtreeMapHandle) || (NULL == m_RootNode))
		return false;

	if(!GetHeader())
		return false;

	if(!GetRootNode())
		return false;

   
    RtlCopyMemory(Root, m_RootNode, DISK_PAGE_SIZE);

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

int  DiskBtree::InitCache()
{
    int retval = 1;

    m_NumCachedNodes = 0;

    m_CacheNodes = (void **)ALLOC_MEMORY((m_MaxKeysInNode + 1) * sizeof(void *),PagedPool);

    if ( !m_CacheNodes ) {
        return 0;
    }

    RtlZeroMemory(m_CacheNodes,(m_MaxKeysInNode + 1) * sizeof(void *));

    return retval;
}

void DiskBtree::InValidateCache()
{
    
	void *tmp;

	if (!m_RootNode)
		return;

	/*
	 * We do not need this mutex as this will only happen in
	 * write path which is already protected by btree lock
	 * This is just for correctness
	 */

	//acquire mutex here
	KeWaitForMutexObject(&m_Mutex,Executive,KernelMode,FALSE,NULL);
	
	/*
	 * We just free all the first level nodes as they are now invalid
	 * The new level 1 cached nodes would be populated on need basis
	 * by bt_get_from_cache()
	 */
	
	for( int i = 0; i <= m_MaxKeysInNode; i++ ) {
		if ( m_CacheNodes[i] ) {
			tmp = m_CacheNodes[i];

			BT_NODE_CACHED(tmp) = 0;
			FreeNode(tmp);

			 m_CacheNodes[i] = NULL;
			m_NumCachedNodes--;
		}
	}

	//Release mutex here
	KeReleaseMutex(&m_Mutex,FALSE);
}

void *DiskBtree::GetBtreeNodeFromCache(void *node, int ChildNodeIdx)
{
    void *retptr = NULL;
	
	if ( !m_RootNode )
		return NULL;
	
	if ( node == m_RootNode ) {
		
		//acquire mutex here
		KeWaitForMutexObject(&m_Mutex,Executive,KernelMode,FALSE,NULL);
		
		/*
		 * Once here, the request has to be valid
		 * but we may not have it in cache
		 */
		retptr = m_CacheNodes[ChildNodeIdx];

		if ( !retptr ) {
			/* If not in cache, read it and send cached node */
			AllocAndReadCache(ChildNodeIdx);
			retptr = m_CacheNodes[ChildNodeIdx];
		}

		//release mutex here
		KeReleaseMutex(&m_Mutex,FALSE);
	}

    return retptr;
}

void DiskBtree::AllocAndReadCache(int ChildNodeIdx)
{
    void *tmp = NULL;
	int retval = 0;
	
	tmp = AllocNode();
	if ( !tmp ) {
		goto out;
	}

	retval = DiskRead(GetChildValue(ChildNodeIdx, m_RootNode), tmp, BT_NODE_SIZE);
	if ( retval == 0 ) {
		FreeNode(tmp);
		goto out;
	}

	BT_NODE_CACHED(tmp) = 1;
	m_CacheNodes[ChildNodeIdx] = tmp;
	m_NumCachedNodes++;
	
out:
	return;
}

int DiskBtree::AllocAndReadNode(void *parent,int idx, void **child)
{
    
	int retval = 1;
	void *node = NULL;
	
	node = GetBtreeNodeFromCache(parent, idx);
	if ( node == NULL ) {
		node = AllocNode();

		if ( node ) {
			retval = DiskRead(GetChildValue(idx, parent),
					 node, BT_NODE_SIZE);
			if ( retval == 0 ) {
				goto err;
			}
        } else {
            retval = 0;
        }
	} else {
		//found in cache
	}

	*child = node;
	
out:
	return retval;

err:
	FreeNode(node);
	goto out;
}

int DiskBtree::ValidateCache()
{
    return 0;
}

void DiskBtree::DestroyCache()
{
        if ( m_CacheNodes ) {
			//Do we need to acquire mutex here
            /* Free the cache pages */
            for(int i = 0; i <= m_MaxKeysInNode; i++ ) {
                if( m_CacheNodes[i] ) {
                    BT_NODE_CACHED(m_CacheNodes[i])=0;
                    FreeNode(m_CacheNodes[i]);
                    m_CacheNodes[i] = NULL;
                }
            }
            
            FREE_MEMORY(m_CacheNodes);
            m_CacheNodes = NULL;
        }
}

void *DiskBtree::AllocNode()
{
    void *tmp = NULL;
    tmp = ALLOC_MEMORY_WITH_TAG(BT_NODE_SIZE + BT_NODE_HDR_SIZE, PagedPool, VVTAG_BTREE_NODE);
    if(tmp){
        RtlZeroMemory(tmp,BT_NODE_SIZE + BT_NODE_HDR_SIZE);
		tmp = (char *)tmp + BT_NODE_HDR_SIZE;
    }
    return tmp;
}

void DiskBtree::FreeNode(void *node)
{
    void *tmp = node;
    if(tmp != NULL){
		
		tmp = (char *)tmp - BT_NODE_HDR_SIZE;
		if(1 != *(int *)tmp)
		    FREE_MEMORY(tmp);
    }
}



/********************* PRIVATE INTERFACE DEFINITIONS ********************************/

int DiskBtree::GetHeader()
{
	if(NULL == m_BtreeMapHandle)
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
	if(NULL == m_BtreeMapHandle)
		return false;

	if(!GetHeader())
		return false;

	if(m_RootNode == NULL)
	{
		m_RootNode = AllocNode();
		if(!m_RootNode)
			return false;
	}
	else
		return true;

	if(!READ_FILE(m_BtreeMapHandle, m_RootNode, m_Header->RootOffset, BT_NODE_SIZE, NULL))
	{
		//FREE_MEMORY(m_RootNode);
        FreeNode(m_RootNode);
		m_RootNode = NULL;
		return false;
	}

	return true;
}

int DiskBtree::DiskRead(ULONGLONG Offset, void *Page, int Size)
{
	if((NULL == m_BtreeMapHandle) || (NULL == Page))
		return false;

	if(!READ_FILE(m_BtreeMapHandle, Page, Offset, Size, NULL))
		return false;

	return true;
}

int DiskBtree::DiskWrite(ULONGLONG Offset, void *Page, int Size, int PadSize)
{
	void *node = NULL;
    int alloc = 0;


	if((NULL == m_BtreeMapHandle) || (NULL == Page))
		return false;

	if(PadSize) {
		node = ALLOC_MEMORY(DISK_PAGE_SIZE, PagedPool);
		if(NULL == node)
		return false;

		RtlCopyMemory( node, Page, Size );
		RtlZeroMemory((char *)node + Size, PadSize);
        alloc = 1;
	} else {
		node = Page;
	}

	if(!WRITE_FILE(m_BtreeMapHandle, node, Offset, Size + PadSize, NULL)) {
        if(1 == alloc)
		    FREE_MEMORY( node);	
		return false;
	}
    if(1 == alloc)
	    FREE_MEMORY( node);

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
		
		RtlZeroMemory(m_Header, BTREE_HEADER_SIZE);

		m_Header->RootOffset = BTREE_HEADER_SIZE;
		m_Header->MaxKeysPerNode = m_MaxKeysInNode;
		m_Header->MinKeysPerNode = m_MinKeysInNode;
		m_Header->KeySize = m_KeySize;
		m_Header->version = 1;
		
		if(!DiskWrite(0, m_Header, BTREE_HEADER_SIZE,0))
		{
			Success = false;
			break;
		}

		m_RootNode = AllocNode();
		if(!m_RootNode)
		{
			Success = false;
			break;
		}

		//memset(m_RootNode, 0, DISK_PAGE_SIZE);

		if(!DiskWrite(m_Header->RootOffset, m_RootNode, BT_NODE_SIZE))
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
			//FREE_MEMORY(m_RootNode);
            FreeNode(m_RootNode);
	}
	return Success;
}

