#include "common.h"

#ifdef DISK_PAGE_SIZE
#undef DISK_PAGE_SIZE
#endif

#define BTREE_HEADER 512
#define DISK_PAGE_SIZE 4096
#define BTREE_ORDER 57

#define MAX_KEYS ((2*BTREE_ORDER)-1)
#define MIN_KEYS (BTREE_ORDER-1)


#pragma pack(push, 1)
struct BtreeNodeTag
{
	int NumOfKeys;
	unsigned long ChildPointer[MAX_KEYS+1];
	KeyAndData Key[MAX_KEYS];
};

typedef struct BtreeNodeTag BtreeNode;
#pragma pack(pop)

void InitBtree();
void GetInputFromUser(void);
int BtreeSearch(HANDLE BtreeMapHandle, BtreeNode * Root, KeyAndData * Key, BtreeMatchEntry ** Node, int & index);
int BtreeSearch(HANDLE BtreeMapHandle, LONGLONG RootOffset, LONGLONG Offset, LONGLONG Length, BtreeMatchEntry *Entry);
int BtreeDelete(BtreeNode *Node, unsigned long NodeOffset, ULONGLONG key, HANDLE BtreeMapHandle);
void RedistributeRight(BtreeNode *Node, unsigned long NodeOffset, BtreeNode *Child, unsigned long ChildOffset, 
					   BtreeNode *RightSibling, unsigned long RightSiblingOffset, int Position, HANDLE BtreeMapHandle);
void RedistributeLeft(BtreeNode *Node, unsigned long NodeOffset, BtreeNode *Child, unsigned long ChildOffset, 
					  BtreeNode *LeftSibling, unsigned long LeftSiblingOffset, int Position, HANDLE BtreeMapHandle);
void MergeSiblings(BtreeNode *Node, int position, BtreeNode *Left, BtreeNode *Right, HANDLE BtreeMapHandle);
void RedistributeOrMerge(BtreeNode *Node, unsigned long NodeOffset, BtreeNode **Child, unsigned long ChildOffset, int position, HANDLE BtreeMapHandle);
int SearchForKeyInNode(BtreeNode *Node, LONGLONG key, int *position, HANDLE BtreeMapHandle);
void DeleteKeyInNode(BtreeNode *Node, unsigned long NodeOffset, int position, int preserve_left, HANDLE BtreeMapHandle);
void MergeChildren(BtreeNode *Node, unsigned long NodeOffset, BtreeNode *Predecessor, unsigned long PredecessorOffset, 
				   BtreeNode *Successor, unsigned long SuccessorOffset, int Position, HANDLE BtreeMapHandle);
void FindPredecessorAndDelete(BtreeNode *Node, unsigned long NodeOffset, LONGLONG *PredecessorKey,
							  HANDLE BtreeMapHandle);
void FindSuccessorAndDelete(BtreeNode *Node, unsigned long NodeOffset, ULONGLONG *SuccessorKey, HANDLE BtreeMapHandle);
void BtreePrintNode(BtreeNode *Node, unsigned long NodeOffset, HANDLE BtreeMapHandle);
int BtreeInsert(char *RootPtr, VirtualSnapShotInfo *VsnapInfo, KeyAndData & NewKey, PSINGLE_LIST_ENTRY Head, bool SpecialFlag, int *CopyData);
void BtreeSplitNode(BtreeNode *Parent, unsigned long ParentOffset, int index, char *Child, unsigned long ChildOffset, HANDLE BtreeMapHandle);
int BtreeInsertNonFull( BtreeNode *Node, unsigned long NodeOffset, KeyAndData & NewKey, PSINGLE_LIST_ENTRY Head, HANDLE BtreeMapHandle, bool SpecialFlag, int * CopyData);
