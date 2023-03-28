//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : hashcomparedata.h
// 
// Description: 
// 
#ifndef HASHCOMPAREDATA_H
#define HASHCOMPAREDATA_H
#include <iterator>

#include "svdparse.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <string>
#if defined(SV_SUN) || defined(SV_AIX)
    #pragma pack(1)
#else
    #pragma pack(push, 1)
#endif /* SV_SUN || SV_AIX */

struct HashCompareInfo {    
    SV_UINT m_ChunkSize;
    SV_UINT m_CompareSize;
    int   m_Algo;
};

struct HashCompareNode {
    SV_LONGLONG   m_Offset; 
    SV_UINT      m_Length;
    unsigned char m_Hash[16];
};

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack()
#else
#pragma pack(pop)
#endif /* SV_SUN || SV_AIX */

class HashCompareData {
public:

    // supported HASH algorithms
    typedef int HcdAlgo;	
    static HcdAlgo const MD5 = (HcdAlgo)1; 

    typedef HashCompareNode* iterator;
    
    HashCompareData(int count, SV_ULONG chunkSize, SV_ULONG compareSize, HcdAlgo algo, unsigned int endiantagsize);
    HashCompareData(char* data, int length);

    ~HashCompareData();           

    iterator Begin() { return m_Begin; }
    iterator End()   { return m_End; }
    
    void Clear();

    bool ComputeHash(SV_LONGLONG offset, SV_ULONG length, char* buffer, bool bDICheck = false );
	bool UpdateHash(SV_LONGLONG offset, SV_ULONG length, unsigned char digest[16], bool bDICheck = false);
  
    int Count() const { return (m_Count); }
    int DataLength() const { 
        return (m_endianTagSize + sizeof(SVD_PREFIX) + sizeof(HashCompareInfo) + (sizeof(HashCompareNode) * (m_End == m_Begin ? 1 : (m_Count + 1)))); 
    }

    char const* Data() const { return (char const*)m_Data; }

    void Dump();

    static int CalculateDataLength(int count) {
        int nc;
        INM_SAFE_ARITHMETIC(nc = InmSafeInt<int>::Type(count) + 1, INMAGE_EX(count))
        int rval;
        INM_SAFE_ARITHMETIC(rval = sizeof(SVD_PREFIX) + sizeof(HashCompareInfo) + (InmSafeInt<size_t>::Type(sizeof(HashCompareNode)) * nc), INMAGE_EX(sizeof(SVD_PREFIX))(sizeof(HashCompareInfo))(sizeof(HashCompareNode))(nc))
        return rval;
    }
 
    int GetHeaderLength(void);
    
protected: 
    void Allocate(int hashCount);

private:
    // no copying HashCompareData
    HashCompareData(HashCompareData const & hcd);
    HashCompareData& operator=(HashCompareData const & hcd);    
    
    int m_MaxCount;
    int m_Count;
    int m_endianTagSize;

    char* m_Data;  // pointer to the buffer holding all the hash compare data which is layed out as follows
                   // +------------+-----------------+-----------------+------+-----------------+
                   // | SVD_PRIFIX | HashCompareInfo | HashCompareNode | .... | HashCompareNode |
                   // +------------+-----------------+-----------------+------+-----------------+
                   // since this is a custom container, the SVD_PREFIX is included as part of the buffer
                   // to make it easy to send to a remote site

    SVD_PREFIX* m_SvdPrefix;   // points to the prefix header    

    HashCompareInfo* m_HashCompareInfo; // point to the HashCompareInfo

    HashCompareNode* m_Begin;     // points to the begining of the hash data
    HashCompareNode* m_End;       // points to 1 past the last valid HashCompareNode in the container
    HashCompareNode* m_Last;      // points to the last available location in the buffer 
                                  // it will never be filled in with hash compare data
                                  // is used to make sure the buffer is not over run
};


#endif // ifndef HASHCOMPAREDATA__H
