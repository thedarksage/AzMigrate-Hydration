//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : DiskBtreeCore.h
//
// Description: Header file for Core Disk-Based btree class.
//
#ifndef _DISK_BTREE_CORE_H
#define _DISK_BTREE_CORE_H

#if defined(VSNAP_UNIX_KERNEL_MODE) || defined(VSNAP_WIN_KERNEL_MODE) || defined(VSNAP_USER_MODE)

#include "inmsafecapis.h"
#include "DiskBtreeHelpers.h"
#include "svtypes.h"
#include <string.h>
#if defined(SV_WINDOWS)
#include<windows.h>
#endif

#ifdef DISK_PAGE_SIZE
#undef DISK_PAGE_SIZE
#endif

#ifndef NULL
#define NULL 0
#endif

enum BTREE_STATUS { BTREE_FAIL, BTREE_SUCCESS, BTREE_SUCCESS_MODIFIED };

struct BtreeHeaderTag
{
	SV_ULONGLONG	RootOffset;
	SV_ULONGLONG	rsvd1;
	SV_ULONGLONG	rsvd2;
	SV_UINT		version;
	SV_UINT		MaxKeysPerNode;
	SV_UINT		MinKeysPerNode;
	SV_UINT		KeySize;
	SV_UINT		NodeSize;
};
typedef struct BtreeHeaderTag BtreeHeader;

#define BTREE_HEADER_OFFSET_TO_USE 128
#define BTREE_HEADER_SIZE 512
#define DISK_PAGE_SIZE 4096

class DiskBtree
{
public:
	DiskBtree(){};
	~DiskBtree(){};
	void Init(int);
	void Uninit();
	int InitFromDiskFileName(const char *, unsigned int, unsigned int);
	int InitBtreeMapFile();
	int InitFromMapHandle(ACE_HANDLE);
	int GetHeader(void *); 
	int WriteHeader(void *);
	int GetRootNode(void *);
	SV_ULONGLONG GetBtreeRootNodeOffset();
	void SetCallBack(void (*NewCallBack)(void *, void *, void *, bool));
	void SetCallBackThirdParam(void *);
	void ResetCallBack();
	void ResetCallBackParam();
    template<class Type> BTREE_STATUS BtreeSearch(const Type *SearchKey, Type *MatchKey, const size_t MatchKeySize);
	template<class Type> BTREE_STATUS BtreeInsert(Type *, bool);
	template<class Type> BTREE_STATUS BtreePreOrderTraverse(Type);
	inline int GetNumKeys(void *);
	inline void *NodePtr(int, void *);

private:
	int GetHeader();
	int GetRootNode();
	int DiskRead(SV_ULONGLONG, void *, int);
	int DiskWrite(SV_ULONGLONG, void *, int);
	int GetFreeMapFileOffset(SV_ULONGLONG *);
	int InitBtree();
	
	inline void SetNumKeys(void *, int);
	inline void IncrNumKeysByOne(void *);
	inline SV_ULONGLONG *ChildPtr(int, void *);
	inline SV_ULONGLONG GetChildValue(int, void *);
	
	template<class Type> BTREE_STATUS BtreeSplitNode(void*, SV_ULONGLONG, int, void *, SV_ULONGLONG, Type*);
    template<class Type> BTREE_STATUS BtreeSearch(void *Node, const Type *SearchKey, Type *MatchKey, const size_t MatchKeySize);
	template<class Type> BTREE_STATUS BtreeInsertNonFull(void *, SV_ULONGLONG, Type *, bool);
	template<class Type> void MoveAndInsertKeys(void *Node, Type Key, int Index);
	void (*CallBack)(void *, void *, void *, bool); 
	template<class Type> BTREE_STATUS BtreeTraverseRecursion(void *, SV_ULONGLONG, Type);

private:
	int m_MaxKeysInNode;
	int m_MinKeysInNode;
	int m_KeySize;

	void		*m_RootNode;
	BtreeHeader *m_Header;
	SV_ULONGLONG	m_RootNodeOffset;
	ACE_HANDLE	m_BtreeMapHandle;
	void		*CallBackParam;
	bool		m_InitFromHandle;
};

inline int DiskBtree::GetNumKeys(void *Node)
{
	return ((*(int *)Node));
}

inline void DiskBtree::SetNumKeys(void *Node, int Keys)
{
	(*(int *)Node) = Keys;
}

inline void DiskBtree::IncrNumKeysByOne(void *Node)
{
	int a = GetNumKeys(Node);
	a++;
	SetNumKeys(Node,a);
}

inline SV_ULONGLONG *DiskBtree::ChildPtr(int Index, void *Node)
{
	return ((SV_ULONGLONG *)(((char *)Node + sizeof(int) + ( Index * sizeof(SV_ULONGLONG) ))));
}

inline SV_ULONGLONG DiskBtree::GetChildValue(int Index, void *Node)
{
	return ((SV_ULONGLONG)(*(SV_ULONGLONG *)((char *)Node + sizeof(int) + ( Index * sizeof(SV_ULONGLONG) ))));
}

inline void *DiskBtree::NodePtr(int Index, void *Node)
{
	return ((char *)Node + sizeof(int) + ((m_MaxKeysInNode + 1) * (sizeof(SV_ULONGLONG))) + (Index * m_KeySize));
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeSearch(const Type *SearchKey, Type *MatchKey, const size_t MatchKeySize)
{
    return BtreeSearch(m_RootNode, SearchKey, MatchKey, MatchKeySize);
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeInsert(Type *NewKey, bool OverwriteFlag)
{
	void				*SavedRoot = NULL, *NewRoot = NULL;
	SV_ULONGLONG	NewRootOffset = 0;
	BTREE_STATUS		Success = BTREE_SUCCESS;	

	//Check if root node is full.
	if(m_MaxKeysInNode == GetNumKeys(m_RootNode))
	{
		//Root node is full, so split the node before insertion.

        /**
        *
        * Since m_RootNode is allocated using ALLOCATE_ALIGNED_MEMORY_FOR_NODE,
        * SavedRoot should not have alignment problems.
        *
        */

		SavedRoot = m_RootNode;
		
		//NewRoot = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
		NewRoot = (void *)ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
		if(!NewRoot)
			return BTREE_FAIL;

		memset(NewRoot, 0, DISK_PAGE_SIZE);

		m_RootNode = NewRoot; //Make RootNode point to the new root node.

		SetNumKeys(NewRoot, 0);
 
        /**
        *
        * This should work now since alignment taken care
        * by functions ALLOCATE_ALIGNED_MEMORY_FOR_NODE 
        * and FREE_ALIGNED_MEMORY_FOR_NODE.
        * 
        */
         
		*(SV_ULONGLONG *)ChildPtr(0, NewRoot) = m_RootNodeOffset;
		
		if(!GetFreeMapFileOffset(&NewRootOffset))
		{
			//FREE_MEMORY(NewRoot);
			FREE_ALIGNED_MEMORY_FOR_NODE(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		if(!DiskWrite(NewRootOffset, NewRoot, DISK_PAGE_SIZE))
		{
			//FREE_MEMORY(NewRoot);
			FREE_ALIGNED_MEMORY_FOR_NODE(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		if(!BtreeSplitNode(NewRoot, NewRootOffset, 0, SavedRoot, m_RootNodeOffset, NewKey))
		{
			//FREE_MEMORY(NewRoot);
			FREE_ALIGNED_MEMORY_FOR_NODE(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		m_RootNodeOffset = NewRootOffset;

		m_Header->RootOffset = m_RootNodeOffset;
		
		if(!DiskWrite(0, m_Header, BTREE_HEADER_SIZE))
		{
			//FREE_MEMORY(NewRoot);
			FREE_ALIGNED_MEMORY_FOR_NODE(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		Success = BtreeInsertNonFull(NewRoot, NewRootOffset, NewKey, OverwriteFlag);
		
		//FREE_MEMORY(SavedRoot);
		FREE_ALIGNED_MEMORY_FOR_NODE(SavedRoot);

		return Success;
	}
	else
		return BtreeInsertNonFull(m_RootNode, m_RootNodeOffset, NewKey, OverwriteFlag);
}


template <class Type>
BTREE_STATUS DiskBtree::BtreeSearch(void *Node, const Type *SearchKey, Type *MatchKey, const size_t MatchKeySize)
{
	int					Index = 0;
	BTREE_STATUS		Success = BTREE_SUCCESS;
	void				*ChildNode = NULL;
	bool				alloc = false;

	if(GetNumKeys(Node) == 0)
		return BTREE_FAIL;

	while(Index < GetNumKeys(Node))
	{
		Type *CurKey = (Type *)NodePtr(Index, Node);

		if( *CurKey < *SearchKey)
			Index++;
		else if( *CurKey > *SearchKey)
			break;
		else /* Case *CurKey == *SearchKey */
		{
			//Make Sure, we dont copy more than key size.
            inm_memcpy_s(MatchKey, MatchKeySize, CurKey, sizeof(Type) > m_KeySize ? m_KeySize : sizeof(Type));
			return BTREE_SUCCESS;
		}
	}

	if(NULL == GetChildValue(0, Node))
		return BTREE_FAIL;
	else
	{
		if(Node != m_RootNode)
			ChildNode = Node; //Re-use the memory.
		else
		{
			//ChildNode = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
			ChildNode = (void *)ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
			if(!ChildNode)
				return BTREE_FAIL;
			alloc = true;
		}

        if(!DiskRead(GetChildValue(Index, Node), ChildNode, DISK_PAGE_SIZE))
		{
			Success = BTREE_FAIL;
			goto cleanup_exit;
		}

		Success = BtreeSearch(ChildNode, SearchKey, MatchKey);
	}

cleanup_exit:

	if(alloc)
    {
        //FREE_MEMORY(ChildNode);
		FREE_ALIGNED_MEMORY_FOR_NODE(ChildNode);
    }
		
	return Success;
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeSplitNode(void *Parent, SV_ULONGLONG ParentOffset, int Index, void *Child,
									SV_ULONGLONG ChildOffset, Type *NewKey)
{
	int					Loop = 0;
	void				*NewNode = NULL;
	SV_ULONGLONG	NewOffset = 0;
	
	//NewNode = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
	NewNode = (void *)ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
	if(!NewNode)
		return BTREE_FAIL;

	memset(NewNode, 0, DISK_PAGE_SIZE);
	
	//When we split a FULL node, we get MIN_KEYS number of keys in the newly splitted nodes.
	SetNumKeys(NewNode, m_MinKeysInNode);

	//Copy the keys from the old full node to new node.
	for(Loop = 0; Loop < m_MinKeysInNode; Loop++)
		memcpy(NodePtr(Loop, NewNode), NodePtr(Loop + m_MinKeysInNode + 1, Child), 
					sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

	//if the fullnode is not a leaf, then we have to copy the child pointers also.

    /**
    *
    * This should work since alignment taken care
    * by functions ALLOCATE_ALIGNED_MEMORY_FOR_NODE 
    * and FREE_ALIGNED_MEMORY_FOR_NODE. The incoming 
    * parameter node "Child" also has to be allocated
    * and freed using the above ALLOCATE_ALIGNED_MEMORY_FOR_NODE
    * and FREE_ALIGNED_MEMORY_FOR_NODE.
    *
    */
    
	if(0 != GetChildValue(0, Child))
	{
		for( Loop = 0;  Loop < (m_MinKeysInNode + 1); Loop++)
			(*(SV_ULONGLONG *)ChildPtr(Loop, NewNode)) = (*(SV_ULONGLONG*)ChildPtr(Loop + m_MinKeysInNode + 1, Child));
	}

	// Reset Number of nodes in the previously full node.
	SetNumKeys(Child, m_MinKeysInNode);

	//Adjust the child pointers in the parent node where new entry is going to be added.

    /**
    *
    * This should work since alignment taken care
    * by functions ALLOCATE_ALIGNED_MEMORY_FOR_NODE 
    * and FREE_ALIGNED_MEMORY_FOR_NODE. The incoming 
    * parameter node "Child" also has to be allocated
    * and freed using the above ALLOCATE_ALIGNED_MEMORY_FOR_NODE
    * and FREE_ALIGNED_MEMORY_FOR_NODE.
    *
    */

	for(Loop = GetNumKeys(Parent); (GetNumKeys(Parent) != 0) && (Loop >= Index+1); Loop--)
		(*(SV_ULONGLONG *)ChildPtr(Loop+1, Parent)) = (*(SV_ULONGLONG *)ChildPtr(Loop, Parent));

	if(!GetFreeMapFileOffset(&NewOffset))
	{
		//FREE_MEMORY(NewNode);
		FREE_ALIGNED_MEMORY_FOR_NODE(NewNode);
		return BTREE_FAIL;
	}

    /**
    *
    * This should work since alignment taken care
    * by functions ALLOCATE_ALIGNED_MEMORY_FOR_NODE 
    * and FREE_ALIGNED_MEMORY_FOR_NODE. The incoming 
    * parameter node "Child" also has to be allocated
    * and freed using the above ALLOCATE_ALIGNED_MEMORY_FOR_NODE
    * and FREE_ALIGNED_MEMORY_FOR_NODE.
    *
    */

	(*(SV_ULONGLONG *)ChildPtr(Index+1, Parent)) = NewOffset;

	for(Loop = GetNumKeys(Parent); (GetNumKeys(Parent) != 0) && (Loop >= Index); Loop--)
		memcpy(NodePtr(Loop, Parent), NodePtr(Loop -1, Parent), sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

	memcpy(NodePtr(Index, Parent), NodePtr(m_MinKeysInNode, Child), sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

	IncrNumKeysByOne(Parent);

	//Flush the data to disk
	if(!DiskWrite(ParentOffset, Parent, DISK_PAGE_SIZE) || !DiskWrite(ChildOffset, Child, DISK_PAGE_SIZE) || !DiskWrite(NewOffset, NewNode, DISK_PAGE_SIZE))
	{
		//FREE_MEMORY(NewNode);
		FREE_ALIGNED_MEMORY_FOR_NODE(NewNode);
		return BTREE_FAIL;
	}
	
	//FREE_MEMORY(NewNode);
	FREE_ALIGNED_MEMORY_FOR_NODE(NewNode);
	return BTREE_SUCCESS;
}

template <class Type>
void DiskBtree::MoveAndInsertKeys(void *Node, Type Key, int Index)
{
	Type Temp;

	while(Index <= GetNumKeys(Node))
	{
		memcpy(&Temp, NodePtr(Index, Node), sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));
		memcpy(NodePtr(Index, Node), &Key, sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));
		Key = Temp;
		Index++;
	}
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeInsertNonFull(void *Node, SV_ULONGLONG NodeOffset, Type *NewKey, bool OverwriteFlag)
{
	int				Index = 0;
	BTREE_STATUS	Success = BTREE_SUCCESS;

	if(0 == GetChildValue(Index, Node))
	{
		while(Index < GetNumKeys(Node))
		{
			Type *NodeKey = (Type *)NodePtr(Index, Node);

			if( *NewKey < *NodeKey )
				break;
			else if(*NewKey > *NodeKey)
				Index++;
			else /* case *NewKey == *NodeKey; */
			{
				if(NULL != CallBack)
				{	
					CallBack((void *)NodeKey, (void *)NewKey, CallBackParam, OverwriteFlag);
				}

				if(OverwriteFlag)
				{

					if(!DiskWrite(NodeOffset, Node, DISK_PAGE_SIZE))
							return BTREE_FAIL;

					return BTREE_SUCCESS_MODIFIED;
				}
				
				return BTREE_SUCCESS;
			}
		}

		MoveAndInsertKeys(Node, *NewKey, Index);

		IncrNumKeysByOne(Node);
		if(!DiskWrite(NodeOffset, Node, DISK_PAGE_SIZE))
			return BTREE_FAIL;
		
		return BTREE_SUCCESS_MODIFIED;
	}
	else
	{
		Index = GetNumKeys(Node) - 1;

		while(Index >= 0)
		{
			Type *NodeKey = (Type *)NodePtr(Index, Node);

			if( *NewKey < *NodeKey )
				Index--;
			else if(*NewKey > *NodeKey )
				break;
			else /* case *NewKey == *NodeKey; */
			{
				if(NULL != CallBack)
				{	
					CallBack((void *)NodeKey, (void *)NewKey, CallBackParam, OverwriteFlag);
				}

				if(OverwriteFlag)
				{

					if(!DiskWrite(NodeOffset, Node, DISK_PAGE_SIZE))
							return BTREE_FAIL;

					return BTREE_SUCCESS_MODIFIED;
				}

				return BTREE_SUCCESS;
			}
		}

		Index++;
		
		void *Child = NULL;

		//Child = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
		Child = (void *)ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
		if(!Child)
			return BTREE_FAIL;

        /**
        *
        * This should work since alignment taken care
        * by functions ALLOCATE_ALIGNED_MEMORY_FOR_NODE 
        * and FREE_ALIGNED_MEMORY_FOR_NODE. The incoming 
        * parameter node "Child" also has to be allocated
        * and freed using the above ALLOCATE_ALIGNED_MEMORY_FOR_NODE
        * and FREE_ALIGNED_MEMORY_FOR_NODE.
        *
        */

		SV_ULONGLONG Child_Offset = 0;
		Child_Offset = GetChildValue(Index, Node);

		if(!DiskRead(Child_Offset, Child, DISK_PAGE_SIZE))
		{
			//FREE_MEMORY(Child);
			FREE_ALIGNED_MEMORY_FOR_NODE(Child);
			return BTREE_FAIL;
		}

		if(m_MaxKeysInNode == GetNumKeys(Child))
		{
			if(!BtreeSplitNode(Node, NodeOffset, Index, Child, Child_Offset, NewKey))
			{
				//FREE_MEMORY(Child);
				FREE_ALIGNED_MEMORY_FOR_NODE(Child);
				return BTREE_FAIL;
			}

			Type *NodeKey = (Type *)NodePtr(Index, Node);
			if(*NodeKey < *NewKey)
			{
				Index++;
			}
			else if(*NodeKey == *NewKey)
			{
				if(NULL != CallBack)
				{	
					CallBack((void *)NodeKey, (void *)NewKey, CallBackParam, OverwriteFlag);
				}

				if(OverwriteFlag)
				{
   					if(!DiskWrite(NodeOffset, Node, DISK_PAGE_SIZE))
					{
						//FREE_MEMORY(Child);
						FREE_ALIGNED_MEMORY_FOR_NODE(Child);
						return BTREE_FAIL;
					}
					
					//FREE_MEMORY(Child);
					FREE_ALIGNED_MEMORY_FOR_NODE(Child);
					return BTREE_SUCCESS_MODIFIED;
				}

				//FREE_MEMORY(Child);
				FREE_ALIGNED_MEMORY_FOR_NODE(Child);
				return Success;
			}

			if(!DiskRead(GetChildValue(Index, Node), Child, DISK_PAGE_SIZE))
			{
				//FREE_MEMORY(Child);
				FREE_ALIGNED_MEMORY_FOR_NODE(Child);
				return BTREE_FAIL;
			}	

			Success = BtreeInsertNonFull(Child, GetChildValue(Index, Node) , NewKey, OverwriteFlag);
			//FREE_MEMORY(Child);
			FREE_ALIGNED_MEMORY_FOR_NODE(Child);
		}
		else
		{
			Success = BtreeInsertNonFull(Child, Child_Offset, NewKey, OverwriteFlag);
			//FREE_MEMORY(Child);
			FREE_ALIGNED_MEMORY_FOR_NODE(Child);
		}

		return Success;
	}
}

template <class Type>
BTREE_STATUS DiskBtree::BtreePreOrderTraverse(Type X)
{
	char *RootNode = NULL;
	enum BTREE_STATUS Status;

	//RootNode = (char *)ALLOC_MEMORY(DISK_PAGE_SIZE);
	RootNode = (char *)ALLOCATE_ALIGNED_MEMORY_FOR_NODE(DISK_PAGE_SIZE);
	if(!RootNode)
		return BTREE_FAIL;

	memcpy(RootNode, m_RootNode, DISK_PAGE_SIZE);
	Status = BtreeTraverseRecursion(RootNode, m_RootNodeOffset, X);
	//FREE_MEMORY(RootNode);
	FREE_ALIGNED_MEMORY_FOR_NODE(RootNode);

	return Status;
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeTraverseRecursion(void *Node, SV_ULONGLONG NodeOffset, Type X)
{
	int index = 0;
	SV_ULONGLONG ChildOffset = 0;
	enum BTREE_STATUS Status = BTREE_SUCCESS;
	
	if(NULL == GetChildValue(0, Node)) //If leaf
	{
		int i = 0;
		while(i < GetNumKeys(Node))
		{
			Type *NodeKey = (Type *)NodePtr(i, Node);
			if(NULL != CallBack)
				CallBack((void *)NodeKey, (void *)NULL, (void *)CallBackParam, true);
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
		
		Status = BtreeTraverseRecursion(Node, ChildOffset, X);
		if(Status == BTREE_FAIL)
			break;
		
		if(!DiskRead(NodeOffset, Node, DISK_PAGE_SIZE))
			return BTREE_FAIL;
		
		if(index < GetNumKeys(Node))
		{
			Type *NodeKey = (Type *)NodePtr(index, Node);
			if(NULL != CallBack)
				CallBack((void *)NodeKey, (void *)NULL, (void *)CallBackParam, true);
		}
		index++;
	}

	return BTREE_SUCCESS;
}

#endif
#endif
