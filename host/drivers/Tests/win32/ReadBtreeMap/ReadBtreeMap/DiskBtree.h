#ifndef _DISK_BTREE_H
#define _DISK_BTREE_H
#include<windows.h>





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
enum BTREE_STATUS { BTREE_FAIL, BTREE_SUCCESS, BTREE_SUCCESS_MODIFIED };
struct VsnapKeyDataTag
{
	ULONGLONG	Offset;
	ULONGLONG	FileOffset;
	ULONG		FileId;
	ULONG		Length;
	ULONG		TrackingId;
};
typedef struct VsnapKeyDataTag VsnapKeyData;

class DiskBtree
{
public:
	DiskBtree()
    {
        m_MaxKeysInNode = 0;
        m_MinKeysInNode = 0;
        m_KeySize = 0;
        m_Header = NULL;
        m_RootNode = NULL;
        m_RootNodeOffset = 0;

    };
	~DiskBtree()
    {
        CloseHandle (m_BtreeMapHandle);
    };
	int InitFromDiskFileName(const char *, unsigned int, unsigned int);
	int GetHeader(void *); 
	int GetRootNode(void *);
	BTREE_STATUS BtreePreOrderTraverse();
	inline int GetNumKeys(void *);
	inline void *NodePtr(int, void *);
    void DisplayHeader();


private:
	int GetHeader();
	int GetRootNode();
	int DiskRead(ULONGLONG, void *, int);
	int GetFreeMapFileOffset(ULONGLONG *);
	
	
	inline ULONGLONG *ChildPtr(int, void *);
	inline ULONGLONG GetChildValue(int, void *);
	
	
	BTREE_STATUS BtreeTraverseRecursion(void *, ULONGLONG);


	int m_MaxKeysInNode;
	int m_MinKeysInNode;
	int m_KeySize;

	void		*m_RootNode;
	BtreeHeader *m_Header;
	ULONGLONG	m_RootNodeOffset;
	HANDLE		m_BtreeMapHandle;
	bool		m_InitFromHandle;
};





#endif