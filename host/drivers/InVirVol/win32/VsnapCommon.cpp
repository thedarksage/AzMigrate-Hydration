//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : VsnapCommon.cpp
//
// Description: Common vsnap definitions used by both User/Kernel mode Vsnap library.
//

#include "VsnapCommon.h"

// Overload "<" operator for VsnapKeyData structure.
bool operator<(VsnapKeyData & lhs, VsnapKeyData & rhs)
{
	if((lhs.Offset + lhs.Length - 1) < (rhs.Offset))
		return true;	
	else
		return false;
}

// Overload ">" operator for VsnapKeyData structure.
bool operator>(VsnapKeyData & lhs, VsnapKeyData & rhs)
{
	if((rhs.Offset + rhs.Length - 1) < (lhs.Offset))
		return true;	
	else
		return false;
}

// Overload "==" operator for VsnapKeyData structure.
bool operator==(VsnapKeyData & lhs, VsnapKeyData & rhs)
{
	ULONGLONG lhsEnd = lhs.Offset + lhs.Length - 1;
	ULONGLONG rhsEnd = rhs.Offset + rhs.Length - 1;

	if(((lhs.Offset >= rhs.Offset) && (lhs.Offset <= rhsEnd)) ||
		((lhsEnd >= rhs.Offset) && (lhsEnd <= rhsEnd)) ||
		((rhs.Offset >= lhs.Offset) && (rhs.Offset <= lhsEnd)) ||
		((rhsEnd >= lhs.Offset) && (rhsEnd <= lhsEnd)))
		return true;	
	else
		return false;
}

void InsertInList(PSINGLE_LIST_ENTRY Head, ULONGLONG Offset, ULONG Length,
					ULONG FileId, ULONGLONG FileOffset, ULONG TrackingId)

{
	VsnapOffsetSplitInfo *OffsetSplit = NULL;

	OffsetSplit = (VsnapOffsetSplitInfo *)ALLOC_MEMORY(sizeof(VsnapOffsetSplitInfo));
    if(NULL == OffsetSplit)
        return;
		
	OffsetSplit->Offset		= Offset;
	OffsetSplit->Length		= Length;
	OffsetSplit->FileOffset = FileOffset;
	OffsetSplit->FileId		= FileId;
	OffsetSplit->TrackingId = TrackingId;

	PushEntryList(Head, (PSINGLE_LIST_ENTRY)OffsetSplit);
}

// This callback is called by btree module during insertions in case of keys matching. First key is the node key and the
// second key is the new key to be inserted. Third parameter the is the pointer to the singly linked list of entries to be
// inserted in the btree map. User/Kernel mode components will initialize the call back before performing insertions.
void VsnapCallBack(void *Key1, void *Key2, void *param, bool OverwriteFlag)
{
	PSINGLE_LIST_ENTRY	Head = (PSINGLE_LIST_ENTRY)param ;
	VsnapKeyData		*NodeKey = (VsnapKeyData *)Key1; //First parameter is the existing node's key.
	VsnapKeyData		*NewKey = (VsnapKeyData *)Key2; //Second is the new key being compared with.

	ULONGLONG NodeKeyEnd = NodeKey->Offset + NodeKey->Length - 1;
	ULONGLONG NewKeyEnd = NewKey->Offset + NewKey->Length - 1;

	if((NewKey->Offset >= NodeKey->Offset) && (NewKeyEnd <= NodeKeyEnd))
	{
		if(!OverwriteFlag)
			return;
		else
		{
			if(NodeKeyEnd > NewKeyEnd)
				InsertInList(Head, NewKeyEnd+1, (ULONG)(NodeKeyEnd-NewKeyEnd), 
					NodeKey->FileId, NodeKey->FileOffset + NewKeyEnd - NodeKey->Offset + 1, NodeKey->TrackingId);	

			if(NewKey->Offset > NodeKey->Offset)
				InsertInList(Head, NodeKey->Offset, (ULONG)(NewKey->Offset - NodeKey->Offset),
					NodeKey->FileId, NodeKey->FileOffset, NodeKey->TrackingId);

			NodeKey->Offset		= NewKey->Offset;
			NodeKey->FileId		= NewKey->FileId;
			NodeKey->FileOffset = NewKey->FileOffset;
			NodeKey->Length		= NewKey->Length;
			NodeKey->TrackingId |= NewKey->TrackingId;
		}
	}
	else if((NewKey->Offset < NodeKey->Offset) && (NewKeyEnd >= NodeKey->Offset) && (NewKeyEnd <= NodeKeyEnd))
	{
		if(!OverwriteFlag)
		{
			InsertInList(Head, NewKey->Offset, (ULONG)(NodeKey->Offset - NewKey->Offset), NewKey->FileId, 
				NewKey->FileOffset, NewKey->TrackingId);
			return;
		}
		else
		{
			if(NodeKeyEnd > NewKeyEnd)
				InsertInList(Head, NewKeyEnd+1, (ULONG)(NodeKeyEnd-NewKeyEnd), NodeKey->FileId, 
					NodeKey->FileOffset + (NewKeyEnd - NodeKey->Offset + 1), NodeKey->TrackingId);

			if(NewKeyEnd > NodeKey->Offset)
			{
				InsertInList(Head, NewKey->Offset, (ULONG)(NodeKey->Offset - NewKey->Offset), NewKey->FileId, 
					NewKey->FileOffset, NewKey->TrackingId);

				NodeKey->Length = (ULONG)(NewKeyEnd - NodeKey->Offset + 1);
				NodeKey->FileId = NewKey->FileId;
				NodeKey->FileOffset = NewKey->FileOffset + NodeKey->Offset - NewKey->Offset;
			}
			else
			{
				InsertInList(Head, NewKey->Offset, (ULONG)(NodeKey->Offset - NewKey->Offset), NewKey->FileId, 
					NewKey->FileOffset, NewKey->TrackingId);

				NodeKey->Length = 1;
				NodeKey->FileId = NewKey->FileId;
				NodeKey->FileOffset = NewKey->FileOffset + NewKeyEnd - NewKey->Offset;
			}

			NodeKey->TrackingId |= NewKey->TrackingId;
			return;
		}
	}
	else if((NewKey->Offset >= NodeKey->Offset) && (NewKey->Offset <= NodeKeyEnd) && (NewKeyEnd > NodeKeyEnd))
	{
		if(!OverwriteFlag)
		{
			InsertInList(Head, NodeKeyEnd+1, (ULONG)(NewKeyEnd-NodeKeyEnd), NewKey->FileId, 
				NewKey->FileOffset + (NodeKeyEnd-NewKey->Offset + 1), NewKey->TrackingId);
			return;	
		}
		else
		{
			if(NewKey->Offset > NodeKey->Offset)
				InsertInList(Head, NodeKey->Offset, (ULONG)(NewKey->Offset - NodeKey->Offset), NodeKey->FileId, 
					NodeKey->FileOffset, NodeKey->TrackingId);

			if(NewKey->Offset < NodeKeyEnd)
			{
				InsertInList(Head, NodeKeyEnd+1, (ULONG)(NewKeyEnd-NodeKeyEnd), NewKey->FileId, 
					NewKey->FileOffset + (NodeKeyEnd-NewKey->Offset+1), NewKey->TrackingId);

				NodeKey->Offset = NewKey->Offset;
				NodeKey->Length = (ULONG)(NodeKeyEnd - NewKey->Offset + 1);
				NodeKey->FileId = NewKey->FileId;
				NodeKey->FileOffset = NewKey->FileOffset;
			}
			else
			{
				InsertInList(Head, NewKey->Offset + 1, (ULONG)(NewKey->Length - 1), NewKey->FileId, 
					NewKey->FileOffset + 1, NewKey->TrackingId);

				NodeKey->Offset = NewKey->Offset;
				NodeKey->Length = 1;
				NodeKey->FileId = NewKey->FileId;
				NodeKey->FileOffset = NewKey->FileOffset;
			}

			NodeKey->TrackingId |= NewKey->TrackingId;
			return;
		}
	}
	else if((NewKey->Offset < NodeKey->Offset) && (NewKeyEnd > NodeKeyEnd))
	{
		if(!OverwriteFlag)
		{
			InsertInList(Head, NewKey->Offset, (ULONG)(NodeKey->Offset - NewKey->Offset), NewKey->FileId, 
				NewKey->FileOffset, NewKey->TrackingId);
			InsertInList(Head, NodeKeyEnd + 1, (ULONG)(NewKeyEnd - NodeKeyEnd), NewKey->FileId, 
				NewKey->FileOffset + (NodeKeyEnd - NewKey->Offset + 1), NewKey->TrackingId);
			return;
		}
		else
		{
			InsertInList(Head, NewKey->Offset, (ULONG)(NodeKey->Offset - NewKey->Offset), NewKey->FileId, 
				NewKey->FileOffset, NewKey->TrackingId);
			InsertInList(Head, NodeKeyEnd + 1, (ULONG)(NewKeyEnd - NodeKeyEnd), NewKey->FileId, 
				NewKey->FileOffset + (NodeKeyEnd - NewKey->Offset + 1), NewKey->TrackingId);

			NodeKey->FileId = NewKey->FileId;
			NodeKey->FileOffset = NewKey->FileOffset + NodeKey->Offset - NewKey->Offset;
			NodeKey->TrackingId |= NewKey->TrackingId;
			return;
		}
	}
}