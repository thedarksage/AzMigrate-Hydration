#include <VVCommon.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <io.h>

#define INMAGE_KERNEL_MODE

#include "Misc.h"
#include "FileIO.h"
#include "BtreeC.h"

struct BtreeInfoTag
{
	LONGLONG RootOffset;
	LONGLONG FileIdMapOffset;
	LONGLONG NoOfFileIdEntries;
	LONGLONG version;
};

typedef struct BtreeInfoTag BtreeInfo;

unsigned long KeyIndexOffset = sizeof(int);
unsigned long ChildIndexOffset = sizeof(int)+(sizeof(KeyAndData)*MAX_KEYS);

BtreeNode *RootBufPtr = NULL;
BtreeInfo *BInfo = NULL;

char BtreeHeader[BTREE_HEADER];
char RootBuf[DISK_PAGE_SIZE];

DWORD NoOfReads = 0;
DWORD NoOfWrites = 0;

inline int DiskWriteHeader(HANDLE BtreeMapHandle, char *BtreeHeader)
{
	LARGE_INTEGER FileOffset;
	DWORD BytesWritten = 0;
	FileOffset.QuadPart = 0;

	if(!IN_SUCCESS(InWriteFile(BtreeMapHandle, BtreeHeader, 512, &FileOffset, &BytesWritten, NULL)))
		return 0;
	else
		return 1;
}

inline int DiskReadHeader(HANDLE BtreeMapHandle, char *BtreeHeader)
{
	LARGE_INTEGER FileOffset;
	DWORD BytesRead = 0;
	FileOffset.QuadPart = 0;

	if(!IN_SUCCESS(InReadFile(BtreeMapHandle, BtreeHeader, 512, &FileOffset, &BytesRead, NULL)))
		return 0;
	else
		return 1;
}

inline int DiskWrite(HANDLE BtreeMapHandle, LONGLONG Offset, void *Page)
{
	LARGE_INTEGER FileOffset;
	DWORD BytesWritten = 0;
	FileOffset.QuadPart = Offset;
	
	if(!IN_SUCCESS(InWriteFile(BtreeMapHandle, Page, DISK_PAGE_SIZE, &FileOffset, &BytesWritten, NULL)))
		return 0;
	else
		return 1;
}

inline int DiskRead(HANDLE BtreeMapHandle, LONGLONG Offset, void *Page)
{
	LARGE_INTEGER FileOffset;
	DWORD BytesRead = 0;

	FileOffset.QuadPart = Offset;

	if(!IN_SUCCESS(InReadFile(BtreeMapHandle, Page, DISK_PAGE_SIZE, &FileOffset, &BytesRead, NULL)))
		return 0;
	else
		return 1;
}

unsigned long GetFreeMapFileOffset(HANDLE BtreeMapHandle)
{
	LARGE_INTEGER NewOffset;
	LARGE_INTEGER ReturnedOffset;

	NewOffset.QuadPart = 0;
	InSetFilePointer(BtreeMapHandle, NewOffset, &ReturnedOffset, FILE_END);

	return (unsigned long)ReturnedOffset.QuadPart;
}

bool InitBtree(BtreeInfo *Header, HANDLE BtreeMapHandle)
{
	int i = 0;
	
	RootBufPtr = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
	ASSERT(RootBufPtr);

	RtlZeroMemory(RootBufPtr, DISK_PAGE_SIZE);

	Header->RootOffset = GetFreeMapFileOffset(BtreeMapHandle);
	DiskWrite(BtreeMapHandle, Header->RootOffset, RootBufPtr);
	DiskWriteHeader(BtreeMapHandle, (char *)Header);

	return true;
}

int BtreeGetRootNode(HANDLE BtreeMapHandle, LONGLONG RootOffset, char **RootPtr)
{
	*RootPtr = (char *)InAllocateMemory(DISK_PAGE_SIZE);
	if(!*RootPtr)
		return false;

	DiskRead(BtreeMapHandle, RootOffset, (void *)*RootPtr);

	return true;
}

int BtreeSearch(HANDLE BtreeMapHandle, LONGLONG RootOffset, LONGLONG Offset, LONGLONG Length, BtreeMatchEntry *Entry)
{
	int Position, ret_val;
	BtreeNode *Root = NULL;
    KeyAndData Key;

	Key.Offset = Offset;
	Key.Length = Length;
	
	Root = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
	if(!Root)
		return false;

	if(!DiskRead(BtreeMapHandle, RootOffset, Root))
	{
		InFreeMemory(Root);
		return false;
	}

	ret_val = BtreeSearch(BtreeMapHandle, Root, &Key, &Entry, Position);

	InFreeMemory(Root);

	return ret_val;
}

int DoesKeysMatch(BtreeNode *Node, int Position, KeyAndData *Key)
{
	LONGLONG NodeEnd = Node->Key[Position].Offset + Node->Key[Position].Length - 1;
	LONGLONG KeyEnd = (Key->Offset + Key->Length - 1);

	if(((Key->Offset >= Node->Key[Position].Offset) && (Key->Offset <= NodeEnd)) || 
						((KeyEnd >= Node->Key[Position].Offset) && (KeyEnd <= NodeEnd)) ||
						((Node->Key[Position].Offset >= Key->Offset) && (Node->Key[Position].Offset <= KeyEnd)) ||
						((NodeEnd >= Key->Offset) && (NodeEnd <= KeyEnd)))
		return 1;
	else
		return 0;
}

int BtreeSearch( HANDLE BtreeMapHandle, /* IN */ BtreeNode *Current, 
				 /* IN */ KeyAndData * Key,
				 /* OUT */ BtreeMatchEntry ** Entry,
				 /* OUT */ int & index)
{
	int i = 0;
	int ret_val = true;

	if(Current->NumOfKeys == 0)
		return false;

	while( i < Current->NumOfKeys )
	{
		if(DoesKeysMatch(Current, i, Key))
		{
			(*Entry)->Offset = Current->Key[i].Offset;
			(*Entry)->Length = (unsigned long )Current->Key[i].Length;
			(*Entry)->FileId = (unsigned long )Current->Key[i].FileId;
			(*Entry)->DataFileOffset = (unsigned long )Current->Key[i].FileOffset;
			index = i;
			return true;
		}
		else if( Key->Offset > ((Current->Key[i].Offset + Current->Key[i].Length - 1)) )
			i++;
		else
			break;
	}

	if(NULL == Current->ChildPointer[0])
		return false;
	else
	{
		BtreeNode *Next = NULL;
		
		Next = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		if(!Next)
			return false;

		if(!DiskRead(BtreeMapHandle, Current->ChildPointer[i], Next))
		{
			InFreeMemory(Next);
			return false;
		}
		ret_val = BtreeSearch(BtreeMapHandle, Next, Key, Entry, index);
		if(Next)
			InFreeMemory(Next);

		return ret_val;
	}
}

// Routine to split a FULL btree node

int BtreeSplitNode( BtreeNode *ParentNonFull, unsigned long ParentOffset, int index,
					 BtreeNode *ChildFull, unsigned long ChildOffset, HANDLE BtreeMapHandle)
{
	int loop = 0;
	BtreeNode *New = NULL;
	unsigned long NewOffset = 0;
    
	New = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
	if(!New)
		return false;

	RtlZeroMemory(New, DISK_PAGE_SIZE);

	//When we split a FULL node, we get MIN_KEYS number of keys in the newly splitted nodes.
	New->NumOfKeys = MIN_KEYS;
	
	//Copy the keys from the old full node to new node.
	for(loop = 0; loop < MIN_KEYS; loop++)
	{
		New->Key[loop].Offset		= ChildFull->Key[loop+MIN_KEYS+1].Offset;
		New->Key[loop].FileOffset	= ChildFull->Key[loop+MIN_KEYS+1].FileOffset;
		New->Key[loop].FileId		= ChildFull->Key[loop+MIN_KEYS+1].FileId;
		New->Key[loop].Length		= ChildFull->Key[loop+MIN_KEYS+1].Length;
	}

	//if the fullnode is not a leaf, then we have to copy the child pointers also.
	if(NULL != ChildFull->ChildPointer[0])
	{
		for( loop = 0;  loop < (MIN_KEYS+1); loop++)
			New->ChildPointer[loop] = ChildFull->ChildPointer[loop+MIN_KEYS+1];
	}

	// Reset Number of nodes in the previously full node.
	ChildFull->NumOfKeys = MIN_KEYS;

	//Adjust the child pointers in the parent node where new entry is going to be added.
	//for(loop = NUM_KEYS(ParentNonFull); NUM_KEYS(ParentNonFull)!=0&& (loop >= index+1); loop--)
	for(loop = ParentNonFull->NumOfKeys; (ParentNonFull->NumOfKeys != 0) && (loop >= index+1); loop--)
		ParentNonFull->ChildPointer[loop+1] = ParentNonFull->ChildPointer[loop];
		
	NewOffset = GetFreeMapFileOffset(BtreeMapHandle);

	ParentNonFull->ChildPointer[index+1] = NewOffset;

	for(loop = ParentNonFull->NumOfKeys; (ParentNonFull->NumOfKeys!=0) && (loop >= index); loop--)
	{
		ParentNonFull->Key[loop].Offset = ParentNonFull->Key[loop-1].Offset;
		ParentNonFull->Key[loop].FileOffset = ParentNonFull->Key[loop-1].FileOffset;
		ParentNonFull->Key[loop].FileId = ParentNonFull->Key[loop-1].FileId;
		ParentNonFull->Key[loop].Length = ParentNonFull->Key[loop-1].Length;
	}

	ParentNonFull->Key[index].Offset = ChildFull->Key[MIN_KEYS].Offset;
	ParentNonFull->Key[index].FileOffset= ChildFull->Key[MIN_KEYS].FileOffset;
	ParentNonFull->Key[index].Length = ChildFull->Key[MIN_KEYS].Length;
	ParentNonFull->Key[index].FileId = ChildFull->Key[MIN_KEYS].FileId;
	
	ParentNonFull->NumOfKeys++;
	
	//Flush data to disk. 
	if(!DiskWrite(BtreeMapHandle, ParentOffset, ParentNonFull) || !DiskWrite(BtreeMapHandle, ChildOffset, ChildFull) ||
			!DiskWrite(BtreeMapHandle, NewOffset, New))
	{
		InFreeMemory(New);
		return 0;
	}

	//Free mem.
	InFreeMemory(New);
	return 1;
}

int BtreeInsert(char *RootPtr, VirtualSnapShotInfo *vinfo, KeyAndData & NewKey, PSINGLE_LIST_ENTRY Head, bool SpecialFlag, int *CopyData)
{
	BtreeNode *NewRoot = NULL;
	unsigned long NewRootOffset = 0;
	BtreeNode *SavedRoot = NULL;
	BtreeNode *RootNodePtr = NULL;
	BtreeInfo *Bthdr = NULL;
	int ret_val = false;
	RootNodePtr = (BtreeNode *)RootPtr;

	//Split the root node if it is full already.
	if(MAX_KEYS == RootNodePtr->NumOfKeys)
	{
		SavedRoot = (BtreeNode *)RootNodePtr;

		NewRoot = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		if(!NewRoot)
		{
			return false;
		}
		
		RtlZeroMemory(NewRoot, DISK_PAGE_SIZE);

		Bthdr = (BtreeInfo *)InAllocateMemory(512);
		if(!Bthdr)
		{
			InFreeMemory(NewRoot);
			return false;
		}

		RootNodePtr = (BtreeNode *)NewRoot;

		NewRoot->NumOfKeys = 0;
		NewRoot->ChildPointer[0] = vinfo->RootNodeOffset;
				
		NewRootOffset = GetFreeMapFileOffset(vinfo->BtreeMapFileHandle);
		if(!DiskWrite(vinfo->BtreeMapFileHandle, NewRootOffset, NewRoot))
		{
			InFreeMemory(NewRoot);
			InFreeMemory(Bthdr);
			return false;
		}
		
		if(!BtreeSplitNode(NewRoot, NewRootOffset, 0, SavedRoot, vinfo->RootNodeOffset, vinfo->BtreeMapFileHandle))
		{
			InFreeMemory(NewRoot);
			InFreeMemory(Bthdr);
			return false;
		}

		vinfo->RootNodeOffset = NewRootOffset;

		if(!DiskReadHeader(vinfo->BtreeMapFileHandle, (char *)Bthdr))
		{
			InFreeMemory(NewRoot);
			InFreeMemory(Bthdr);
			return false;
		}

		Bthdr->RootOffset = NewRootOffset;
		if(!DiskWriteHeader(vinfo->BtreeMapFileHandle, (char *)Bthdr))
		{
			InFreeMemory(NewRoot);
			InFreeMemory(Bthdr);
			return false;
		}
		InFreeMemory(Bthdr);
		
		ret_val = BtreeInsertNonFull(NewRoot, NewRootOffset, NewKey, Head, vinfo->BtreeMapFileHandle, SpecialFlag, CopyData);
		if(NewRoot)
			InFreeMemory(NewRoot);
		return ret_val;
	}	
	else
		return BtreeInsertNonFull(RootNodePtr, vinfo->RootNodeOffset, NewKey, Head, vinfo->BtreeMapFileHandle, SpecialFlag, CopyData);
}

void InsertInList(PSINGLE_LIST_ENTRY Head, ULONGLONG Offset, ULONGLONG Length,
					ULONGLONG FileId, ULONGLONG FileOffset)

{
	MapAttribute *Attrib = NULL;

	Attrib = (MapAttribute *)InAllocateMemory(sizeof(MapAttribute));
	ASSERT(Attrib);
	
	Attrib->Offset = Offset;
	Attrib->Length = Length;
	Attrib->FileOffset = FileOffset;
	Attrib->FileId = (int)FileId;

	PushEntryList(Head, (PSINGLE_LIST_ENTRY)Attrib);
}


// Returns 0 if New key is equal to Node key
// Returns 1 if New key is less than Node Key
// Returns -1 is New key is greater than Node key
int IsNewKeyLessThanNodeKey(BtreeNode *Node, int NodePosition, KeyAndData & NewKey, PSINGLE_LIST_ENTRY Head, 
								HANDLE BtreeMapHandle, unsigned long NodeOffset, bool SpecialFlag, int *CopyData)
{
	LONGLONG NodeKeyOffset = Node->Key[NodePosition].Offset; //KEYPTR(Node, NodePosition)->Offset;
	LONGLONG NodeKeyLength = Node->Key[NodePosition].Length; //KEYPTR(Node, NodePosition)->Length;
	LONGLONG NodeKeyEnd = NodeKeyOffset + NodeKeyLength - 1;
	LONGLONG NewKeyEnd = NewKey.Offset + NewKey.Length - 1;;
		
	if((NewKey.Offset >= NodeKeyOffset) && (NewKeyEnd <= NodeKeyEnd))
	{
		if(SpecialFlag)
			return 0;
		else
		{
			if(NodeKeyEnd > NewKeyEnd)
				InsertInList(Head, NewKeyEnd+1, (ULONGLONG)(NodeKeyEnd-NewKeyEnd), 
					(ULONGLONG)Node->Key[NodePosition].FileId,
						(ULONGLONG)(Node->Key[NodePosition].FileOffset+NewKeyEnd-NodeKeyOffset+1));

			if(NewKey.Offset > NodeKeyOffset)
				InsertInList(Head, NodeKeyOffset, (ULONGLONG)(NewKey.Offset - NodeKeyOffset),
				(ULONGLONG)Node->Key[NodePosition].FileId,	(ULONGLONG)(Node->Key[NodePosition].FileOffset));

			Node->Key[NodePosition].Offset = NewKey.Offset;
			Node->Key[NodePosition].FileId = NewKey.FileId;
			Node->Key[NodePosition].FileOffset = NewKey.FileOffset;
			Node->Key[NodePosition].Length = NewKey.Length;
			if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
				return 2;
					
			return 0;
		}
	}
	else if((NewKey.Offset < NodeKeyOffset) && (NewKeyEnd >= NodeKeyOffset) && (NewKeyEnd <= NodeKeyEnd))
	{
		if(SpecialFlag)
		{
			InsertInList(Head, NewKey.Offset, NodeKeyOffset-NewKey.Offset, NewKey.FileId, NewKey.FileOffset);
			return 0;
		}
		else
		{
			if(NodeKeyEnd > NewKeyEnd)
				InsertInList(Head, NewKeyEnd+1, NodeKeyEnd-NewKeyEnd, Node->Key[NodePosition].FileId, 
					(ULONGLONG)(Node->Key[NodePosition].FileOffset+NewKeyEnd-NodeKeyOffset+1));

			if(NewKeyEnd > NodeKeyOffset)
			{
				InsertInList(Head, NewKey.Offset, NodeKeyOffset-NewKey.Offset, NewKey.FileId, NewKey.FileOffset);

				Node->Key[NodePosition].Length = NewKeyEnd-NodeKeyOffset+1;
				Node->Key[NodePosition].FileId = NewKey.FileId;
				Node->Key[NodePosition].FileOffset = NewKey.FileOffset+NodeKeyOffset-NewKey.Offset;
				if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
					return 2;
			}
			else
			{
				InsertInList(Head, NewKey.Offset, NewKeyEnd-NewKey.Offset, NewKey.FileId, NewKey.FileOffset);
				Node->Key[NodePosition].Length = 1;
				Node->Key[NodePosition].FileId = NewKey.FileId;
				Node->Key[NodePosition].FileOffset = NewKey.FileOffset+NewKeyEnd-NewKey.Offset;
				if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
					return 2;
			}
			return 0;
		}
	}
	else if((NewKey.Offset >= NodeKeyOffset) && (NewKey.Offset <= NodeKeyEnd) && (NewKeyEnd > NodeKeyEnd))
	{
		if(SpecialFlag)
		{
			InsertInList(Head, NodeKeyEnd+1, NewKeyEnd-NodeKeyEnd, NewKey.FileId, NewKey.FileOffset + (NodeKeyEnd-NewKey.Offset+1));
			return 0;	
		}
		else
		{
			if(NewKey.Offset > NodeKeyOffset)
				InsertInList(Head, NodeKeyOffset, NewKey.Offset-NodeKeyOffset, Node->Key[NodePosition].FileId,
					Node->Key[NodePosition].FileOffset);

			if(NewKey.Offset < NodeKeyEnd)
			{
				InsertInList(Head, NodeKeyEnd+1, NewKeyEnd-NodeKeyEnd, NewKey.FileId, 
					NewKey.FileOffset+(NodeKeyEnd-NewKey.Offset+1));

				Node->Key[NodePosition].Offset = NewKey.Offset;
				Node->Key[NodePosition].Length = NodeKeyEnd-NewKey.Offset+1;
				Node->Key[NodePosition].FileId = NewKey.FileId;
				Node->Key[NodePosition].FileOffset = NewKey.FileOffset;

				if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
					return 2;
			}
			else
			{
				InsertInList(Head, NewKey.Offset+1, NewKeyEnd-NewKey.Offset, NewKey.FileId, NewKey.FileOffset+1);
				Node->Key[NodePosition].Length = 1;
				Node->Key[NodePosition].FileId = NewKey.FileId;
				Node->Key[NodePosition].FileOffset = NewKey.FileOffset;
				if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
					return 2;
			}

			return 0;
		}
	}
	else if((NewKey.Offset < Node->Key[NodePosition].Offset) && (NewKeyEnd > NodeKeyEnd))
	{
		if(SpecialFlag)
		{
			InsertInList(Head, NewKey.Offset, NodeKeyOffset - NewKey.Offset, NewKey.FileId, NewKey.FileOffset);
			InsertInList(Head, NodeKeyEnd+1, NewKeyEnd-NodeKeyEnd, NewKey.FileId, NewKey.FileOffset + (NodeKeyEnd-NewKey.Offset+1));
			return 0;
		}
		else
		{
			InsertInList(Head, NewKey.Offset, NodeKeyOffset-NewKey.Offset, NewKey.FileId, NewKey.FileOffset);
			InsertInList(Head, NodeKeyEnd+1, NewKeyEnd-NodeKeyEnd, NewKey.FileId, 
				NewKey.FileOffset+NodeKeyEnd-NewKey.Offset+1);

			Node->Key[NodePosition].FileId = NewKey.FileId;
			Node->Key[NodePosition].FileOffset = NewKey.FileOffset+NodeKeyOffset-NewKey.Offset;
			if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
				return 2;

			return 0;
		}
	}
	else if(NewKey.Offset < Node->Key[NodePosition].Offset)
		return 1;
	else
		return -1;
}

void InsertAndMoveKeys(BtreeNode *Node, KeyAndData Key, int i)
{
	KeyAndData Temp;

	while( (i <= Node->NumOfKeys) && (MAX_KEYS >= Node->NumOfKeys) )
	{
	    RtlCopyMemory(&Temp, &Node->Key[i], sizeof(KeyAndData));
		RtlCopyMemory(&Node->Key[i], &Key, sizeof(KeyAndData));
		RtlCopyMemory(&Key, &Temp, sizeof(KeyAndData));
		i++;
	}
}

// This function inserts a key onto a non-full btree node.
int BtreeInsertNonFull( BtreeNode *Node, unsigned long NodeOffset, KeyAndData & NewKey, PSINGLE_LIST_ENTRY Head,
									HANDLE BtreeMapHandle, bool SpecialFlag, int * CopyData)
{
	int i = 0;
	int ret_val;

	if(NULL == Node->ChildPointer[0])
	{
		while(i < Node->NumOfKeys)
		{
			ret_val = IsNewKeyLessThanNodeKey(Node, i, NewKey, Head, BtreeMapHandle, NodeOffset, SpecialFlag, CopyData);
			if( 0 == ret_val)
				return true;
			else if( -1 == ret_val)
				i++;
			else if( 2 == ret_val)
				return false;
			else 
				break;
		}
		
		*CopyData = 1;
		InsertAndMoveKeys(Node, NewKey, i);

		Node->NumOfKeys++;
		if(!DiskWrite(BtreeMapHandle, NodeOffset, Node))
			return false;
		return true;
	}
	else
	{
		i = Node->NumOfKeys-1;

		while( i >= 0 )
		{
			ret_val = IsNewKeyLessThanNodeKey(Node, i, NewKey, Head, BtreeMapHandle, NodeOffset, SpecialFlag, CopyData);
			if( 0 == ret_val)
				return true;
			else if( 1 == ret_val)
					i--;
			else if( 2 == ret_val)
				return false;
			else
				break;
		}

		i++;

		BtreeNode *ChildNode = NULL;
		ChildNode = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		if(!ChildNode)
			return false;

		unsigned long Child_Offset = 0; 
		
		Child_Offset = Node->ChildPointer[i];
		if(!DiskRead(BtreeMapHandle, Child_Offset, ChildNode))
		{
			InFreeMemory(ChildNode);
			return false;
		}
	
		if(MAX_KEYS == ChildNode->NumOfKeys) 
		{
			if(!BtreeSplitNode(Node, NodeOffset, i , ChildNode, Child_Offset, BtreeMapHandle))
			{
				InFreeMemory(ChildNode);
				return false;
			}
			if(NewKey.Offset > Node->Key[i].Offset) 
				i++;
			
			if(!DiskRead(BtreeMapHandle, Node->ChildPointer[i], ChildNode))
			{
				InFreeMemory(ChildNode);
				return false;
			}
			ret_val = BtreeInsertNonFull(ChildNode, Node->ChildPointer[i], NewKey, Head, BtreeMapHandle, SpecialFlag, CopyData);
			if(ChildNode)
				InFreeMemory(ChildNode);
		}
		else
		{
			ret_val = BtreeInsertNonFull(ChildNode, Child_Offset, NewKey, Head, BtreeMapHandle, SpecialFlag, CopyData);
			if(ChildNode)
				InFreeMemory(ChildNode);
		}
		return ret_val;
	}
}

/* Btree deletion logic: Given a tree Node pointer this code recursively searches the correct child nodes
 * and deletes the key.
 */
int BtreeDelete(BtreeNode *Node, unsigned long NodeOffset, LONGLONG Key, HANDLE BtreeMapHandle)
{
	int Position = 0;
	BtreeNode *Child = NULL;
	int ret_val = 1;

	if(Node->NumOfKeys == 0)
		return false;

	if(SearchForKeyInNode(Node, Key, &Position, BtreeMapHandle))
	{
		/* Node is a leaf if its first child pointer is NULL. In this case, just remove that
		 * particular key from the node. As we are ensuring all the child nodes having sufficient
		 * keys before branching. When we are here, it means that the leaf node has sufficient key
		 * to perform deletion.
	     */
        if(Node->ChildPointer[0] == NULL)
		{
			DeleteKeyInNode(Node, NodeOffset, Position, 0 /* Invalid for leaf node */, BtreeMapHandle);
		}
		else /* Internal Node */
		{
			BtreeNode *Predecessor = NULL;
			BtreeNode *Successor = NULL;
			
			Predecessor = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
			ASSERT(Predecessor);
			
			DiskRead(BtreeMapHandle, Node->ChildPointer[Position], Predecessor);
			
			/* if the predecessor has sufficient keys then borrow one from it */
			if(Predecessor->NumOfKeys > MIN_KEYS)
			{
				LONGLONG PredecessorKey = 0;
				FindPredecessorAndDelete(Predecessor, Node->ChildPointer[Position], &PredecessorKey, BtreeMapHandle);
			
				/* We got the predecessor key. Replace the key with the predecessor key */
				Node->Key[Position].Offset = PredecessorKey;
				DiskWrite(BtreeMapHandle, NodeOffset, Node);
				InFreeMemory(Predecessor);
			}

			/* else if the successor has sufficient keys then borrow one from it */
			else 
			{
				Successor = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
				ASSERT(Successor);
				
				DiskRead(BtreeMapHandle, Node->ChildPointer[Position+1], Successor);

				if(Successor->NumOfKeys > MIN_KEYS)
				{
					ULONGLONG SuccessorKey = 0;
					FindSuccessorAndDelete(Successor, Node->ChildPointer[Position+1], &SuccessorKey, BtreeMapHandle);

					/* Similar to predecessor case */
					Node->Key[Position].Offset = SuccessorKey;
					DiskWrite(BtreeMapHandle, NodeOffset, Node);
					InFreeMemory(Successor);
				}
				else 
				{
					/* We are here because both the predecessor/successor has only MIN_KEYS.
					* In this case, we have to merge the siblings and then recursively delete
					* the element.
					*/
					unsigned long PredecessorOffset = Node->ChildPointer[Position];
					MergeChildren(Node, NodeOffset, Predecessor, PredecessorOffset, Successor, Node->ChildPointer[Position+1],  Position, BtreeMapHandle);
					
					/* Recursively delete the inserted element in predecessor. Enhancement possible by specifying
					* the position here rather than the key. TBD
					*/
					ret_val = BtreeDelete(Predecessor, PredecessorOffset, Key, BtreeMapHandle);

					/* Dont free Root Buffer */
					if(Predecessor != RootBufPtr)
						InFreeMemory(Predecessor);
					InFreeMemory(Successor);
				}
			}
		}
	}
	else
	{
		if(0 == Node->ChildPointer[0])
		{
			/* We are at the leaf and the key is not present. return false
			 */
			return 0;
		}

		Child = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		ASSERT(Child);
		
		unsigned long ChildOffset = Node->ChildPointer[Position];
		DiskRead(BtreeMapHandle, ChildOffset, Child);
		
		if(Child->NumOfKeys == MIN_KEYS)
		{
			RedistributeOrMerge(Node, NodeOffset, &Child, ChildOffset, Position, BtreeMapHandle);
		}
		
		ret_val = BtreeDelete(Child, ChildOffset, Key, BtreeMapHandle);

		//Free the dynamic memory.
		if(Child != RootBufPtr)
			InFreeMemory(Child);
	}	
	return ret_val;
}

/* This function recursively traverses the node until it reaches the leaf and gets the
 * greatest key and stores it in the pointer and deletes the key from the leaf.
 * VERIFIED partially.
 */
void FindPredecessorAndDelete(BtreeNode *Node, unsigned long NodeOffset, LONGLONG *PredecessorKey, 
										HANDLE BtreeMapHandle)
{
	/* The idea here is to traverse the Node and get the highest key.
	 * Recursively call this routine until a leaf is reached. While
	 * the Nodes ensure the next node to be branched has more than MIN_KEYS, if not
	 * perform 3a or 3b rule.
	 */

	if(Node->ChildPointer[0] == NULL)
	{
		/* Leaf node */
		*PredecessorKey = Node->Key[Node->NumOfKeys-1].Offset;
		DeleteKeyInNode(Node, NodeOffset, Node->NumOfKeys-1, 0/*Invalid for a leaf node*/, BtreeMapHandle);
		return;
	}

	BtreeNode *Child = NULL;
	unsigned long ChildOffset = 0;
	Child = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
	ASSERT(Child != NULL);
	
	ChildOffset = Node->ChildPointer[Node->NumOfKeys];
	DiskRead(BtreeMapHandle, ChildOffset, Child);
		
	/* Ensure the child always has sufficient keys before branching to the child */
	if(Child->NumOfKeys == MIN_KEYS)
	{
		RedistributeOrMerge(Node, NodeOffset, &Child, ChildOffset, Node->NumOfKeys-1, BtreeMapHandle);
	}
	
	FindPredecessorAndDelete(Child, ChildOffset, PredecessorKey, BtreeMapHandle);
	
	//Free the memory allocated above.
	InFreeMemory(Child);
}

/* This function recursively traverses the node until it reaches the leaf and gets the
 * lowest key and stores it in the pointer and deletes the key from the leaf.
 * VERIFIED partially.
 */
void FindSuccessorAndDelete(BtreeNode *Node, unsigned long NodeOffset, ULONGLONG *SuccessorKey, HANDLE BtreeMapHandle)
{
	/* The idea here is to traverse the Node and get the key with least value.
	 * Recursively call this routine until a leaf is reached. While traversing
	 * the Nodes ensure the next node to be branched has more than MIN_KEYS, if not
	 * perform necessary redistribution or merging and then branch.
	 */

	if(Node->ChildPointer[0] == NULL)
	{
		/* Leaf node */
		*SuccessorKey = Node->Key[0].Offset;
		DeleteKeyInNode(Node, NodeOffset, 0, 0/*Invalid for a leaf node*/, BtreeMapHandle);
		return;
	}

	BtreeNode *Child = NULL;
	unsigned long ChildOffset = 0;
	Child = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
	ASSERT(Child != NULL);
	
	ChildOffset = Node->ChildPointer[0];
	DiskRead(BtreeMapHandle, ChildOffset, Child);
	
	/* Ensure the child always has sufficient keys before branching to the child */
	if(Child->NumOfKeys == MIN_KEYS)
	{
		RedistributeOrMerge(Node, NodeOffset, &Child, ChildOffset, 0, BtreeMapHandle);
	}
	
	FindSuccessorAndDelete(Child, ChildOffset, SuccessorKey, BtreeMapHandle);

	//Free the memory allocated above.
	InFreeMemory(Child);
}

/* VERIFIED.
 */
void RedistributeRight(BtreeNode *Node, unsigned long NodeOffset, BtreeNode *Child, unsigned long ChildOffset,
					   BtreeNode *RightSibling, unsigned long RightSiblingOffset, int Position, HANDLE BtreeMapHandle)
{
	/* Step 1: Move the first element in Node to the Child */
	Child->Key[Child->NumOfKeys].Offset = Node->Key[Position].Offset;
	Child->ChildPointer[Child->NumOfKeys+1] = RightSibling->ChildPointer[0];
	Child->NumOfKeys++;
	DiskWrite(BtreeMapHandle, ChildOffset, Child);

	/* Step 2: Move the first key from right sibling into first element of Node */
	Node->Key[Position].Offset = RightSibling->Key[0].Offset;
	DiskWrite(BtreeMapHandle, NodeOffset, Node);

	/* Step 3: Delete the first key in RightSibling */
	DeleteKeyInNode(RightSibling,RightSiblingOffset, 0, 0, BtreeMapHandle);
}

/* VERIFIED
 */
void RedistributeLeft(BtreeNode *Node, unsigned long NodeOffset, BtreeNode *Child, unsigned long ChildOffset, 
					  BtreeNode *LeftSibling, unsigned long LeftSiblingOffset, int Position, HANDLE BtreeMapHandle)
{
	/* Step 1: Move the respective element in Node to the Child */
	int i = Child->NumOfKeys;
	while( i >= 1)
	{
		Child->Key[i].Offset = Child->Key[i-1].Offset;
		Child->ChildPointer[i+1] = Child->ChildPointer[i];
		i--;
	}

	Child->Key[i].Offset = Node->Key[Position].Offset;
	Child->ChildPointer[i] = LeftSibling->ChildPointer[LeftSibling->NumOfKeys];
	Child->NumOfKeys++;
	DiskWrite(BtreeMapHandle, ChildOffset, Child);

	/* Step 2: Move the last key from the sibling in to the Node. */
	Node->Key[Position].Offset = LeftSibling->Key[LeftSibling->NumOfKeys-1].Offset;
	DiskWrite(BtreeMapHandle, NodeOffset, Node);

	/* Step 3: Delete the last key in the left sibling */
	DeleteKeyInNode(LeftSibling, LeftSiblingOffset, LeftSibling->NumOfKeys-1, 1, BtreeMapHandle);
}

/* This function shall perform necessary things to ensure the child has atleast one key greater than
 * MIN_KEYS.
 */
void RedistributeOrMerge(BtreeNode *Node, unsigned long NodeOffset, BtreeNode **Child, unsigned long ChildOffset, int position,
									HANDLE BtreeMapHandle)
{
	BtreeNode *LeftSibling = NULL;
	BtreeNode *RightSibling = NULL;

	if(position == 0)
	{
		//Check only the right sibling
		RightSibling = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		ASSERT(RightSibling);
		
		DiskRead(BtreeMapHandle, Node->ChildPointer[position+1], RightSibling);
		
		if(RightSibling->NumOfKeys > MIN_KEYS)
		{
			RedistributeRight(Node, NodeOffset, *Child, ChildOffset, RightSibling, Node->ChildPointer[position+1], position, BtreeMapHandle);
		}
		else
		{
			MergeChildren(Node, NodeOffset, *Child, ChildOffset, RightSibling, Node->ChildPointer[position+1], position, BtreeMapHandle);
		}

		//Free dynamic memory.
		InFreeMemory(RightSibling);
	}
	else if(position == Node->NumOfKeys)
	{
		//Check only the left sibling
		LeftSibling = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		ASSERT(LeftSibling);
		
		DiskRead(BtreeMapHandle, Node->ChildPointer[position-1], LeftSibling);
		
		if(LeftSibling->NumOfKeys > MIN_KEYS)
		{
			RedistributeLeft(Node, NodeOffset, *Child, ChildOffset, LeftSibling, Node->ChildPointer[position-1], position-1, BtreeMapHandle);
			InFreeMemory(LeftSibling);
		}
		else
		{
			MergeChildren(Node, NodeOffset, LeftSibling, Node->ChildPointer[position-1], *Child, ChildOffset, position-1, BtreeMapHandle);
			//Kinda hack here.
			InFreeMemory(*Child);
			*Child = LeftSibling;
		}
	}
	else
	{
		//Check both left and right siblings
		//LeftSibling = (BtreeNode *)Node->ChildPointer[position-1];
		//RightSibling = (BtreeNode *)Node->ChildPointer[position+1];
		LeftSibling = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
		ASSERT(LeftSibling);
		
		DiskRead(BtreeMapHandle, Node->ChildPointer[position-1], LeftSibling);
		
		if(LeftSibling->NumOfKeys > MIN_KEYS)
		{
			RedistributeLeft(Node, NodeOffset, *Child, ChildOffset, LeftSibling, Node->ChildPointer[position-1], position, BtreeMapHandle);
			InFreeMemory(LeftSibling);
		}
		else 
		{
			RightSibling = (BtreeNode *)InAllocateMemory(DISK_PAGE_SIZE);
			ASSERT(RightSibling);
			
			DiskRead(BtreeMapHandle, Node->ChildPointer[position+1], RightSibling);
			if(RightSibling->NumOfKeys > MIN_KEYS)
			{
				RedistributeRight(Node, NodeOffset, *Child, ChildOffset, RightSibling, Node->ChildPointer[position+1], position, BtreeMapHandle);
			}
			else
			{
				MergeChildren(Node, NodeOffset, *Child, ChildOffset, RightSibling, Node->ChildPointer[position+1], position, BtreeMapHandle);
			}

			InFreeMemory(LeftSibling);
			InFreeMemory(RightSibling);
		}
	}
}

/* This function searches for a key in the "Node" and returns true
 * if the key is present and saves its key index in "position". Returns false
 * otherwise and the position points to the index into the respective
 * child in "Node" to branch to find the key.
 * 
 * The current approach uses a linear search to find the key. Better approach
 * is to perform binary search to make the search faster. This will be done next
 * after understanding the performance using the current approach. TBD.
 * VERIFIED.
 */

int SearchForKeyInNode(BtreeNode *Node, LONGLONG Key, int *Position, HANDLE BtreeMapHandle)
{
	int Temp = 0;
	int Found = 0;
	
	do
	{
		/* As a quick check, if the key is less than first key or greater than the last key
		 * break the loop.
		 */
		if( (Key < Node->Key[0].Offset) )
		{
			*Position = 0;
			break;
		}

		if( Key > Node->Key[Node->NumOfKeys-1].Offset ) 
		{
			*Position = Node->NumOfKeys;
			break;
		}

		while( Temp < Node->NumOfKeys)
		{
			if(Key <= Node->Key[Temp].Offset)
				break;
	        
			Temp++;
		}

		*Position = Temp;

		if((Temp != Node->NumOfKeys) && (Key == Node->Key[Temp].Offset))
			Found = 1;
		
	} while ( 0 );

	if( Found )
		return true;
	else	
		return false;
}

/* This function deletes the key in the Node. If the last argument is 1, then the left
 * pointer of the deleted key will be preserved otherwise the right key will be
 * preserved during deletion. Both are required at some point.
 *
 * VERIFIED.
 */

void DeleteKeyInNode(BtreeNode *Node, unsigned long NodeOffset, int Position, int PreserveLeft, HANDLE BtreeMapHandle)
{
	/* If we have to delete the last last element, just adjust child pointer
	 * and decrement the total count and exit.
	 */

	if(Position == (Node->NumOfKeys-1))
	{
		if(!PreserveLeft)
		{
			Node->ChildPointer[Position] = Node->ChildPointer[Position+1];
		}
		Node->NumOfKeys--;
		DiskWrite(BtreeMapHandle, NodeOffset, Node);
		return;
	}

	if(PreserveLeft)
	{
		Node->Key[Position].Offset = Node->Key[Position+1].Offset;
		Position++;	
	}

	while( Position < (Node->NumOfKeys-1) )
	{
		Node->Key[Position].Offset = Node->Key[Position+1].Offset;
		Node->ChildPointer[Position] = Node->ChildPointer[Position+1];
		Position++;
	}
	Node->ChildPointer[Position] = Node->ChildPointer[Position+1];

	Node->NumOfKeys--;
	DiskWrite(BtreeMapHandle, NodeOffset, Node);
}

/* This function Merges two children into one node and make necessary
 * adjustments for deletion to work. This is the opposite of split
 * operation 
 *
 * VERIFIED.
 */
void MergeChildren(BtreeNode *Node, unsigned long NodeOffset, BtreeNode *Predecessor, unsigned long PredecessorOffset, 
				   BtreeNode *Successor, unsigned long SuccessorOffset, int Position, HANDLE BtreeMapHandle)
{

	/*  First step is to copy the element in Node onto Predecessor and all the elements in Successor
	 *  onto Predecessor node, so that predecessor node has all the elements having 2t-1 keys(MAX_KEYS)
     */
	int PredNumKeys;
	int SucNumKeys;
	int i;

	SucNumKeys = Successor->NumOfKeys;
    PredNumKeys = Predecessor->NumOfKeys;
	
	if(PredNumKeys != MIN_KEYS)
		DbgPrint("Predecessor doesn't have MIN_KEYS, but has %d keys\n", PredNumKeys);
	if(SucNumKeys != MIN_KEYS)
		DbgPrint("Successor doesn't have MIN_KEYS, but has %d keys\n", SucNumKeys);

	Predecessor->Key[PredNumKeys].Offset = Node->Key[Position].Offset;
	PredNumKeys++;

	for( i = 0 ; PredNumKeys < MAX_KEYS; PredNumKeys++, i++)
	{
		Predecessor->Key[PredNumKeys].Offset = Successor->Key[i].Offset;
		Predecessor->ChildPointer[PredNumKeys] = Successor->ChildPointer[i];
	}
	Predecessor->ChildPointer[PredNumKeys] = Successor->ChildPointer[i];
	Predecessor->NumOfKeys += (Successor->NumOfKeys+1);
	DiskWrite(BtreeMapHandle, PredecessorOffset, Predecessor);


	/* Free the Successor. CHECKIT.
	 */ 
	//free(Successor);

	/* Second Step is to delete the key in a non-leaf node. Ensure that we preserve the predecessor child
	 * pointer.
	 */
	DeleteKeyInNode(Node, NodeOffset, Position, 1, BtreeMapHandle);

	if((Node == RootBufPtr) && (Node->NumOfKeys == 0))
	{
		//CHECKIT. the below comment
		InFreeMemory(RootBufPtr);
		RootBufPtr = Predecessor;
		BInfo->RootOffset = PredecessorOffset;
		DiskWriteHeader(BtreeMapHandle, (char *)BInfo);
	}
}

bool InitMap(int SnapShotId, char *LogDir, bool create, HANDLE & BtreeMapHandle, LONGLONG & RootNodeOffset)
{
	char filename[256];
	DWORD BytesTransferred = 0;
	BtreeInfo *Header = NULL;
	bool ret_val = true;

	do
	{
		RtlStringCchPrintfA(filename, 256, "%s\\%d_VsnapMap", LogDir, SnapShotId);

		if( !create )
		{
			Header = (BtreeInfo *)InAllocateMemory(BTREE_HEADER);
			ASSERT(Header);

			ASSERT(IN_SUCCESS(InCreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 
						OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL, NULL, &BtreeMapHandle)));
			
			ASSERT(IN_SUCCESS(InReadFile(BtreeMapHandle, Header, (DWORD)BTREE_HEADER, 0, &BytesTransferred, NULL)));
			
			RootNodeOffset = Header->RootOffset;
			InFreeMemory(Header);
		}
		else
		{
			BInfo = (BtreeInfo *)BtreeHeader;

			RtlZeroMemory(BInfo, BTREE_HEADER);

			ASSERT(IN_SUCCESS(InCreateFile(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS,		
						FILE_ATTRIBUTE_NORMAL, NULL, NULL,	&BtreeMapHandle)));
				
			ASSERT(IN_SUCCESS(InWriteFile(BtreeMapHandle, BInfo, (DWORD)BTREE_HEADER, 0, &BytesTransferred, NULL)));
			
			ASSERT(InitBtree(BInfo, BtreeMapHandle));
			
			RootNodeOffset = BInfo->RootOffset;
		}

	} while (FALSE);

	return ret_val;;
}