#include "stdafx.h"

void DiskBtree::DisplayHeader()
{
    printf("RootOffset is %I64d \nversion is %x \nMaxKeysPerNode is %x\nMinKeysPerNode is %x\nKeySize is %x\nNodeSize is %x\n",
        m_Header->RootOffset, m_Header->version, m_Header->MaxKeysPerNode, m_Header->MinKeysPerNode, m_Header->KeySize, m_Header->NodeSize);
}

inline int DiskBtree::GetNumKeys(void *Node)
{
	return ((*(int *)Node));
}


inline  ULONGLONG *DiskBtree::ChildPtr(int Index, void *Node)
{
	return (( ULONGLONG *)(((char *)Node + sizeof(int) + ( Index * sizeof(ULONGLONG) ))));
}

inline ULONGLONG DiskBtree::GetChildValue(int Index, void *Node)
{
	return ((ULONGLONG)(*( ULONGLONG *)((char *)Node + sizeof(int) + ( Index * sizeof(ULONGLONG) ))));
}

inline void *DiskBtree::NodePtr(int Index, void *Node)
{
	return ((char *)Node + sizeof(int) + ((m_MaxKeysInNode + 1) * (sizeof(ULONGLONG))) + (Index * m_KeySize));
}





int DiskBtree::InitFromDiskFileName(const char *FileName, unsigned int OpenMode, unsigned int SharedMode)
{
bool Success = true;

	do
	{
		if(!Openfile(FileName, OpenMode, SharedMode, m_BtreeMapHandle))
		{
			Success = false;
			break;
		}
		
		if(!GetHeader() )
		{
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
			CloseHandle (m_BtreeMapHandle);
		if(m_Header != NULL)
			free (m_Header);
		if(m_RootNode != NULL)
			free (m_RootNode);

		m_BtreeMapHandle	= NULL;
		m_Header			= NULL;
		m_RootNode			= NULL;
	}

	return Success;
}

int DiskBtree::GetFreeMapFileOffset(ULONGLONG *NewFileOffset)
{
    LARGE_INTEGER     NewFileLoc,DistToMove;
    DistToMove.QuadPart = 0;
    NewFileLoc.QuadPart = *NewFileOffset;
	if(!SetFilePointerEx(m_BtreeMapHandle, DistToMove, &NewFileLoc, FILE_END))
		return false;

	return true;
}


BTREE_STATUS DiskBtree::BtreePreOrderTraverse()
{
	char *RootNode = NULL;
	enum BTREE_STATUS Status = BTREE_SUCCESS;

	RootNode = (char *)ALLOC_MEMORY(DISK_PAGE_SIZE);
	if(!RootNode)
		return BTREE_FAIL;

	memcpy(RootNode, m_RootNode, DISK_PAGE_SIZE);
	Status = BtreeTraverseRecursion(RootNode, m_RootNodeOffset);
	FREE_MEMORY(RootNode);

	return Status;
}


BTREE_STATUS DiskBtree::BtreeTraverseRecursion(void *Node, ULONGLONG NodeOffset)
{
	
    int index = 0;
	ULONGLONG ChildOffset = 0;
	enum BTREE_STATUS Status = BTREE_SUCCESS;
	
	if(NULL == GetChildValue(0, Node)) //If leaf
	{
		int i = 0;
		while(i < GetNumKeys(Node))
		{
			VsnapKeyData *NodeKey = (VsnapKeyData *)NodePtr(i, Node);
            printf("offset = %I64d\nfileoffset = %I64d\nfileid = %x\nLength = %x\n\n",
                NodeKey->Offset, NodeKey->FileOffset, NodeKey->FileId, NodeKey->Length);
			i++;
		}

		return BTREE_SUCCESS;
	}

	while(index <= GetNumKeys(Node))
	{
		if(!DiskRead(NodeOffset, Node, DISK_PAGE_SIZE))
			return BTREE_FAIL;

		ChildOffset = GetChildValue(index, Node);	
		
		if(!DiskRead(ChildOffset, Node, DISK_PAGE_SIZE))
			return BTREE_FAIL;
		
		Status = BtreeTraverseRecursion(Node, ChildOffset);
		if(Status == BTREE_FAIL)
			break;
		
		if(!DiskRead(NodeOffset, Node, DISK_PAGE_SIZE))
			return BTREE_FAIL;
		
		if(index < GetNumKeys(Node))
		{
			VsnapKeyData *NodeKey = (VsnapKeyData *)NodePtr(index, Node);
            printf("offset = %I64d\nfileoffset = %I64d\nfileid = %x\nLength = %x\n\n",
                NodeKey->Offset, NodeKey->FileOffset, NodeKey->FileId, NodeKey->Length);
			
        }
		index++;
	}
    

	return BTREE_SUCCESS;
}


int DiskBtree::GetHeader(void *Hdr)
{
	if(NULL == m_BtreeMapHandle)
		return false;

	if(!GetHeader())
		return false;

	memcpy(Hdr, m_Header, BTREE_HEADER_SIZE);

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
	
	memcpy(Root, m_RootNode, DISK_PAGE_SIZE);

	return true;
}

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

	if(!DiskRead(  0, m_Header, BTREE_HEADER_SIZE ))
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
		m_RootNode = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
		if(!m_RootNode)
			return false;
	}
	else
		return true;


	if(!DiskRead( m_Header->RootOffset, m_RootNode, DISK_PAGE_SIZE))
	{
		FREE_MEMORY(m_RootNode);
		m_RootNode = NULL;
		return false;
	}

	return true;
}

int DiskBtree::DiskRead(ULONGLONG Offset, void *Page, int Size)
{
    DWORD         BytesRead = 0;

	if((NULL == m_BtreeMapHandle) || (NULL == Page))
		return false;

    LARGE_INTEGER     NewFileLoc;
    NewFileLoc.QuadPart = Offset;

    if(!SetFilePointerEx(m_BtreeMapHandle, NewFileLoc, &NewFileLoc, FILE_BEGIN))
		return false;

	if(!ReadFile(m_BtreeMapHandle, Page,  Size, &BytesRead, NULL))
		return false;

	return true;
}