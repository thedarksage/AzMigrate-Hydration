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

#include "DiskBtreeHelpers.h"

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
	ULONGLONG	RootOffset;
	ULONGLONG	rsvd1;
	ULONGLONG	rsvd2;
	ULONG		version;
	ULONG		MaxKeysPerNode;
	ULONG		MinKeysPerNode;
	ULONG		KeySize;
	ULONG		NodeSize;
};
typedef struct BtreeHeaderTag BtreeHeader;

#define BTREE_HEADER_OFFSET_TO_USE 128
#define BTREE_HEADER_SIZE 512
#define DISK_PAGE_SIZE 4096
#define BT_NODE_HDR_SIZE 4
#define MEM_TAG_SIZE 8
#define BT_NODE_SIZE (DISK_PAGE_SIZE - MEM_TAG_SIZE - BT_NODE_HDR_SIZE)
#define BT_NODE_CACHED(NODE)	\
            *((int *)((char *)(NODE) - BT_NODE_HDR_SIZE))


class DiskBtree
{
public:
	DiskBtree(){};
	~DiskBtree(){};
	void Init(int);
	void Uninit();
	int InitFromDiskFileName(const char *, unsigned int, unsigned int);
	int InitBtreeMapFile();
	int InitFromMapHandle(void *);
	int GetHeader(void *); 
	int WriteHeader(void *);
	int GetRootNode(void *);
	ULONGLONG GetBtreeRootNodeOffset();
	void SetCallBack(void (*NewCallBack)(void *, void *, void *, bool));
	void SetCallBackThirdParam(void *);
	void ResetCallBack();
	void ResetCallBackParam();

    int  InitCache();
    void InValidateCache();
    void AllocAndReadCache(int ChildNode);
    int AllocAndReadNode(void *parent,int idx, void **child);
    void *GetBtreeNodeFromCache(void *node, int ChildNodeIdx);
    int ValidateCache();
    void DestroyCache();//place it at the appropriate place
	static void *AllocNode();
    static void FreeNode(void *);

	template<class Type> BTREE_STATUS BtreeSearch(Type *, Type *);
	template<class Type> BTREE_STATUS BtreeInsert(Type *, bool);
	template<class Type> BTREE_STATUS BtreePreOrderTraverse(Type);
	template<class Type> BTREE_STATUS BtreeSearch(void *, Type *, Type *,void **);
	inline int GetNumKeys(void *);
	inline void *NodePtr(int, void *);
	inline void *GetRootNodeFromDiskBtree();


private:
	int GetHeader();
	int GetRootNode();
	int DiskRead(ULONGLONG, void *, int);
	int DiskWrite(ULONGLONG, void *, int, int PadSize = (BT_NODE_HDR_SIZE + MEM_TAG_SIZE) );
	int GetFreeMapFileOffset(ULONGLONG *);
	int InitBtree();
	
	inline void SetNumKeys(void *, int);
	inline void IncrNumKeysByOne(void *);
	inline UNALIGNED ULONGLONG *ChildPtr(int, void *);
	inline ULONGLONG GetChildValue(int, void *);
	
	template<class Type> BTREE_STATUS BtreeSplitNode(void*, ULONGLONG, int, void *, ULONGLONG, Type*);
	
	template<class Type> BTREE_STATUS BtreeInsertNonFull(void *, ULONGLONG, Type *, bool);
	template<class Type> void MoveAndInsertKeys(void *Node, Type Key, int Index);
	void (*CallBack)(void *, void *, void *, bool); 
	template<class Type> BTREE_STATUS BtreeTraverseRecursion(void *, ULONGLONG, Type);

private:
	int m_MaxKeysInNode;
	int m_MinKeysInNode;
	int m_KeySize;

	void		*m_RootNode;
	BtreeHeader *m_Header;
	ULONGLONG	m_RootNodeOffset;
	void		*m_BtreeMapHandle;
	void		*CallBackParam;
	bool		m_InitFromHandle;
    void      **m_CacheNodes;
	KMUTEX      m_Mutex;
    int         m_NumCachedNodes;
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
	(*(int *)Node) = (*(int *)Node)++;
}

inline void *DiskBtree::GetRootNodeFromDiskBtree()
{
	return m_RootNode;
}

inline UNALIGNED ULONGLONG *DiskBtree::ChildPtr(int Index, void *Node)
{
	return ((UNALIGNED ULONGLONG *)(((char *)Node + sizeof(int) + ( Index * sizeof(ULONGLONG) ))));
}

inline ULONGLONG DiskBtree::GetChildValue(int Index, void *Node)
{
	return ((ULONGLONG)(*(UNALIGNED ULONGLONG *)((char *)Node + sizeof(int) + ( Index * sizeof(ULONGLONG) ))));
}

inline void *DiskBtree::NodePtr(int Index, void *Node)
{
	return ((char *)Node + sizeof(int) + ((m_MaxKeysInNode + 1) * (sizeof(ULONGLONG))) + (Index * m_KeySize));
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeSearch(Type *SearchKey, Type *MatchKey)
{
	return BtreeSearch(m_RootNode, SearchKey, MatchKey, NULL);
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeInsert(Type *NewKey, bool OverwriteFlag)
{
	void				*SavedRoot = NULL, *NewRoot = NULL;
	ULONGLONG	NewRootOffset = 0;
	BTREE_STATUS		Success = BTREE_SUCCESS;	

	//Check if root node is full.
	if(m_MaxKeysInNode == GetNumKeys(m_RootNode))
	{
		InValidateCache();
		//Root node is full, so split the node before insertion.
		SavedRoot = m_RootNode;
		
		//NewRoot = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
		NewRoot = AllocNode();
		if(!NewRoot)
			return BTREE_FAIL;

		//memset(NewRoot, 0, DISK_PAGE_SIZE);

		m_RootNode = NewRoot; //Make RootNode point to the new root node.

		SetNumKeys(NewRoot, 0);
		*ChildPtr(0, NewRoot) = m_RootNodeOffset;
		
		if(!GetFreeMapFileOffset(&NewRootOffset))
		{
			//FREE_MEMORY(NewRoot);
            FreeNode(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		if(!DiskWrite(NewRootOffset, NewRoot, BT_NODE_SIZE))
		{
			//FREE_MEMORY(NewRoot);
            FreeNode(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		if(!BtreeSplitNode(NewRoot, NewRootOffset, 0, SavedRoot, m_RootNodeOffset, NewKey))
		{
			//FREE_MEMORY(NewRoot);
            FreeNode(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		m_RootNodeOffset = NewRootOffset;

		m_Header->RootOffset = m_RootNodeOffset;
		
		if(!DiskWrite(0, m_Header, BTREE_HEADER_SIZE,0))
		{
			//FREE_MEMORY(NewRoot);
            FreeNode(NewRoot);
			m_RootNode = SavedRoot;
			return BTREE_FAIL;
		}

		Success = BtreeInsertNonFull(NewRoot, NewRootOffset, NewKey, OverwriteFlag);
		
		//FREE_MEMORY(SavedRoot);
        FreeNode(SavedRoot);

		return Success;
	}
	else
		return BtreeInsertNonFull(m_RootNode, m_RootNodeOffset, NewKey, OverwriteFlag);
}


template <class Type>
BTREE_STATUS DiskBtree::BtreeSearch(void *Node, Type *SearchKey, Type *MatchKey, void **subRoot)
{
	int					Index = 0;
	BTREE_STATUS		Success = BTREE_SUCCESS;
	void				*ChildNode = NULL;
	//bool				alloc = false;
	int                 high = GetNumKeys(Node), mid = 0, low = -1;

	if(GetNumKeys(Node) == 0)
		return BTREE_FAIL;

	//while(Index < GetNumKeys(Node))        //TODO:  first place
	while(1)   
	{

        mid = (high - low) / 2;
		if ( 0 == mid )
			break;

		mid += low;
		Type *CurKey = (Type *)NodePtr(mid, Node);
        
        //Type *CurKey = (Type *)NodePtr(Index, Node);
		if( *CurKey < *SearchKey)
            //Index++;
			low = mid;
		else if( *CurKey > *SearchKey)
            //break;
			high = mid;
		else /* Case *CurKey == *SearchKey */
		{
			//Make Sure, we dont copy more than key size.
			RtlCopyMemory(MatchKey, CurKey, sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

			if(subRoot)
				*subRoot = Node;
			return BTREE_SUCCESS;
		}
	}

	if(NULL == GetChildValue(0, Node))
		return BTREE_FAIL;
	else
	{
		//if(Node != m_RootNode)
		//	ChildNode = Node; //Re-use the memory.
		//else
		//{
			//ChildNode = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
			
		if(!AllocAndReadNode(Node, high, &ChildNode))
				return BTREE_FAIL;
		//alloc = true;
		//}

        //if(!DiskRead(GetChildValue(Index, Node), ChildNode, BT_NODE_SIZE))
		//{
		//	Success = BTREE_FAIL;
		//	goto cleanup_exit;
		//}

		Success = BtreeSearch(ChildNode, SearchKey, MatchKey, subRoot);
	}


	if(!subRoot || (*subRoot != ChildNode))
		FreeNode(ChildNode);
		
	return Success;
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeSplitNode(void *Parent, ULONGLONG ParentOffset, int Index, void *Child,
									ULONGLONG ChildOffset, Type *NewKey)
{
	int					Loop = 0;
	void				*NewNode = NULL;
	ULONGLONG	NewOffset = 0;
	
	//NewNode = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
	NewNode = AllocNode();
	if(!NewNode)
		return BTREE_FAIL;

    //memset not required since already its done in AllocNode call.
	//memset(NewNode, 0, BT_NODE_SIZE);
	
	//When we split a FULL node, we get MIN_KEYS number of keys in the newly splitted nodes.
	SetNumKeys(NewNode, m_MinKeysInNode);

	//Copy the keys from the old full node to new node.
	for(Loop = 0; Loop < m_MinKeysInNode; Loop++)

		RtlCopyMemory(NodePtr(Loop, NewNode), NodePtr(Loop + m_MinKeysInNode + 1, Child), 
					sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

	//if the fullnode is not a leaf, then we have to copy the child pointers also.
	if(NULL != GetChildValue(0, Child))
	{
		for( Loop = 0;  Loop < (m_MinKeysInNode + 1); Loop++)
			(*ChildPtr(Loop, NewNode)) = (*ChildPtr(Loop + m_MinKeysInNode + 1, Child));
	}

	// Reset Number of nodes in the previously full node.
	SetNumKeys(Child, m_MinKeysInNode);

	//Adjust the child pointers in the parent node where new entry is going to be added.
	for(Loop = GetNumKeys(Parent); (GetNumKeys(Parent) != 0) && (Loop >= Index+1); Loop--)
		(*ChildPtr(Loop+1, Parent)) = (*ChildPtr(Loop, Parent));

	if(!GetFreeMapFileOffset(&NewOffset))
	{
		//FREE_MEMORY(NewNode);
        FreeNode(NewNode);
		return BTREE_FAIL;
	}

	(*ChildPtr(Index+1, Parent)) = NewOffset;

	for(Loop = GetNumKeys(Parent); (GetNumKeys(Parent) != 0) && (Loop >= Index); Loop--)
		RtlCopyMemory(NodePtr(Loop, Parent), NodePtr(Loop -1, Parent), sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

	RtlCopyMemory(NodePtr(Index, Parent), NodePtr(m_MinKeysInNode, Child), sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

	IncrNumKeysByOne(Parent);

	//Flush the data to disk
	if(!DiskWrite(NewOffset, NewNode, BT_NODE_SIZE) || !DiskWrite(ParentOffset, Parent, BT_NODE_SIZE) || !DiskWrite(ChildOffset, Child, BT_NODE_SIZE) )
	{
		//FREE_MEMORY(NewNode);
        FreeNode(NewNode);
		return BTREE_FAIL;
	}
	
	//FREE_MEMORY(NewNode);
    FreeNode(NewNode);
	return BTREE_SUCCESS;
}

template <class Type>
void DiskBtree::MoveAndInsertKeys(void *Node, Type Key, int Index)
{
	Type Temp;

	while(Index <= GetNumKeys(Node))
	{
		RtlCopyMemory(&Temp, NodePtr(Index, Node), sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

		RtlCopyMemory(NodePtr(Index, Node), &Key, sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));
		Key = Temp;
		Index++;
	}
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeInsertNonFull(void *Node, ULONGLONG NodeOffset, Type *NewKey, bool OverwriteFlag)
{
	int				Index = 0,NumberOfKeys=0;
	BTREE_STATUS	Success = BTREE_SUCCESS;
    int                 high = GetNumKeys(Node), mid = 0, low = -1;

	if(NULL == GetChildValue(Index, Node))
	{
        while(1)
		{

            mid = (high - low) / 2;
		    if ( 0 == mid )
				break;

		    mid += low;
		    Type *NodeKey = (Type *)NodePtr(mid, Node);
			//Type *NodeKey = (Type *)NodePtr(Index, Node);

			if( *NewKey < *NodeKey )  //TODO :Second place
				//break;
                high = mid;
			else if(*NewKey > *NodeKey)
				//Index++;
                low = mid;
			else /* case *NewKey == *NodeKey; */
			{
				if(NULL != CallBack)
				{	
					CallBack((void *)NodeKey, (void *)NewKey, CallBackParam, OverwriteFlag);
				}

				if(OverwriteFlag)
				{
					//memcpy(NodeKey, NewKey, sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

					if(!DiskWrite(NodeOffset, Node, BT_NODE_SIZE))
							return BTREE_FAIL;

					return BTREE_SUCCESS_MODIFIED;
				}
				
				return BTREE_SUCCESS;
			}
		}

		MoveAndInsertKeys(Node, *NewKey, high);

		IncrNumKeysByOne(Node);
		if(!DiskWrite(NodeOffset, Node, BT_NODE_SIZE))
			return BTREE_FAIL;
		
		return BTREE_SUCCESS_MODIFIED;
	}
	else
	{
		Index = GetNumKeys(Node) - 1;

		while(Index >= 0)
		{
			Type *NodeKey = (Type *)NodePtr(Index, Node);

			if( *NewKey < *NodeKey )//TODO        third place
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
					//memcpy(NodeKey, NewKey, sizeof(Type) > m_KeySize? m_KeySize:sizeof(Type));

					if(!DiskWrite(NodeOffset, Node, BT_NODE_SIZE))
							return BTREE_FAIL;

					return BTREE_SUCCESS_MODIFIED;
				}

				return BTREE_SUCCESS;
			}
		}

		Index++;
		
		void *Child = NULL;

		//Child = (void *)ALLOC_MEMORY(DISK_PAGE_SIZE);
		
		if(!AllocAndReadNode( Node, Index, &Child))
			return BTREE_FAIL;

		ULONGLONG Child_Offset = 0;
		Child_Offset = GetChildValue(Index, Node);

		//if(!DiskRead(Child_Offset, Child, DISK_PAGE_SIZE))
		//{
		//	FREE_MEMORY(Child);
		//	return BTREE_FAIL;
		//}

		if(m_MaxKeysInNode == GetNumKeys(Child))
		{
			if(!BtreeSplitNode(Node, NodeOffset, Index, Child, Child_Offset, NewKey))
			{
				//FREE_MEMORY(Child);
                FreeNode(Child);
				return BTREE_FAIL;
			}

			FreeNode(Child);
			Child = NULL;

			if(Node == m_RootNode)
				InValidateCache();

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
   					if(!DiskWrite(NodeOffset, Node, BT_NODE_SIZE))
					{
						//FREE_MEMORY(Child);
                        //FreeNode(Child);
						return BTREE_FAIL;
					}
					
					//FREE_MEMORY(Child);
                    //FreeNode(Child);
					return BTREE_SUCCESS_MODIFIED;
				}

				//FREE_MEMORY(Child);
                //FreeNode(Child);
				return Success;
			}

			//if(!DiskRead(GetChildValue(Index, Node), Child, BT_NODE_SIZE))
			//{
				//FREE_MEMORY(Child);
              //  FreeNode(Child);
				//return BTREE_FAIL;
			//}

			if(!AllocAndReadNode( Node, Index, &Child))
				return BTREE_FAIL;

			Success = BtreeInsertNonFull(Child, GetChildValue(Index, Node) , NewKey, OverwriteFlag);
			//FREE_MEMORY(Child);
            FreeNode(Child);
		}
		else
		{
			Success = BtreeInsertNonFull(Child, Child_Offset, NewKey, OverwriteFlag);
			//FREE_MEMORY(Child);
            FreeNode(Child);
		}

		return Success;
	}

}

template <class Type>
BTREE_STATUS DiskBtree::BtreePreOrderTraverse(Type X)
{
	char *RootNode = NULL;
	enum BTREE_STATUS Status;

	RootNode = m_;
	if(!RootNode)
		return BTREE_FAIL;

	RtlCopyMemory(RootNode, m_RootNode, DISK_PAGE_SIZE);
	Status = BtreeTraverseRecursion(RootNode, m_RootNodeOffset, X);
	//FREE_MEMORY(RootNode);
    FreeNode(RootNode);

	return Status;
}

template <class Type>
BTREE_STATUS DiskBtree::BtreeTraverseRecursion(void *Node, ULONGLONG NodeOffset, Type X)
{
	int index = 0;
	ULONGLONG ChildOffset = 0;
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
		if(!DiskRead(NodeOffset, Node, BT_NODE_SIZE))
			return BTREE_FAIL;

		ChildOffset = GetChildValue(index, Node);	
		
		if(!DiskRead(ChildOffset, Node, BT_NODE_SIZE))
			return BTREE_FAIL;
		
		Status = BtreeTraverseRecursion(Node, ChildOffset, X);
		if(Status == BTREE_FAIL)
			break;
		
		if(!DiskRead(NodeOffset, Node, BT_NODE_SIZE))
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