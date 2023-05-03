//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : hashcomparedata.cpp
// 
// Description: 
//

#include <exception>
#include <sstream>
#include <cstdlib>

#include <string>
#include "hostagenthelpers_ported.h"
#include "svdparse.h"
#include "inm_md5.h"
#include "hashcomparedata.h"
#include "dataprotectionexception.h"
#include "portablehelpers.h"
#include "theconfigurator.h"
#include "fileconfigurator.h"

#include "hashconvertor.h"
#include "svdconvertor.h"
#include "convertorfactory.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

// --------------------------------------------------------------------------
// constructor: used when you want to create HashCompareData. typically when 
// you are creating HashCompareData to send to a remote site. allocates enough 
// space to hold count HashCompareData
// --------------------------------------------------------------------------
HashCompareData::HashCompareData(int count, SV_ULONG chunkSize, SV_ULONG compareSize, HcdAlgo algo, unsigned int endiantagsize)
    : m_MaxCount(0),
      m_Count(0),
      m_Data(0),
      m_Begin(0),
      m_End(0),
      m_endianTagSize(endiantagsize)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    Allocate(count);
    m_HashCompareInfo->m_ChunkSize = chunkSize;
    m_HashCompareInfo->m_CompareSize = compareSize;
    m_HashCompareInfo->m_Algo = algo;
}


// --------------------------------------------------------------------------
// constructor: used when you have a raw pointer to HashCompareData. 
// typically when you have retrieved the data from a remote location. 
// validates buffer and takes onwership of it (i.e. it will delete it)
// --------------------------------------------------------------------------
HashCompareData::HashCompareData(char* buffer, int length)
    : m_MaxCount(0),
      m_Count(0),
      m_Data(buffer),
      m_Begin(0),
      m_End(0)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    char* bufferEnd = buffer + length;
        
    SV_UINT* dataFormatFlags = reinterpret_cast<SV_UINT*>(buffer) ;
    size_t dataFormatFlagSize = sizeof( SV_UINT ) ;
    HashConvertorPtr hashConvertor ;
    SVDConvertorPtr svdConvertor ;
    ConvertorFactory::getHashConvertor( *dataFormatFlags, hashConvertor ) ;
    ConvertorFactory::getSVDConvertor( *dataFormatFlags, svdConvertor ) ;
    if( ConvertorFactory::isOldSVDFile( *dataFormatFlags ) == true  )
         dataFormatFlagSize = 0 ;

    m_endianTagSize = dataFormatFlagSize;
    // set up the SVD_PREFIX
    m_SvdPrefix = (SVD_PREFIX*) ( m_Data + dataFormatFlagSize ) ;
    
    /* NEEDTOSWAP : START */ 
    svdConvertor->convertPrefix(*m_SvdPrefix ) ;
    /* NEEDTOSWAP : END */ 

    if ( m_SvdPrefix->tag != SVD_TAG_SYNC_HASH_COMPARE_DATA )
        throw CorruptResyncFileException("HashCompareData buffer does not have HCD header");

    // set up HashCompareInfo
    m_HashCompareInfo = (HashCompareInfo*)(m_Data + dataFormatFlagSize + sizeof(SVD_PREFIX));

    /* NEEDTOSWAP : START */ 
    hashConvertor->convertInfo( *m_HashCompareInfo ) ;
    /* NEEDTOSWAP : END */ 

    if (m_HashCompareInfo->m_Algo != MD5)
        throw CorruptResyncFileException("HashCompareData buffer does not have MD5 as algorithm");

    // now set up the HashCompareNodes
    m_Begin = (HashCompareNode*)(m_Data + dataFormatFlagSize + sizeof(SVD_PREFIX) + sizeof(HashCompareInfo));    
    m_End = m_Begin;
    bool zeroTerminated = false;
    while ((char*)m_End < bufferEnd ) 
    {
    /* NEEDTOSWAP : START */ 
        hashConvertor->convertNode( *m_End ) ;
    /* NEEDTOSWAP : END */
	    zeroTerminated = (0 == m_End->m_Length);
        if(zeroTerminated)
          break;

        ++m_End;
        ++m_MaxCount;  
        ++m_Count;
    }
    m_Last = m_End;

    // final sanity check
    if (!zeroTerminated) {
        // oops something wrong HashCompareBuffers should always have
        // at least 1 HashCompareNode at the end that is set to zero's
        throw CorruptResyncFileException("HashCompareData buffer is not terminated with a NULL checksum");
    }
}


int HashCompareData::GetHeaderLength(void)
{
    return (m_endianTagSize + sizeof(SVD_PREFIX) + sizeof(HashCompareInfo));
}


// --------------------------------------------------------------------------
// destructor: deletes the m_Data
// --------------------------------------------------------------------------
HashCompareData::~HashCompareData()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	free(m_Data);
}

// --------------------------------------------------------------------------
// allocates enough space for count HashCompareNodes
// ------------------------------------------- -------------------------------
void HashCompareData::Allocate(int count)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_MaxCount = count;

    int length;
    INM_SAFE_ARITHMETIC(length = InmSafeInt<int>::Type(CalculateDataLength(count)) + m_endianTagSize, INMAGE_EX(CalculateDataLength(count))(m_endianTagSize))

    m_Data = (char *)calloc(length, sizeof(*m_Data));

    if (m_endianTagSize)
    {
        SV_UINT* dataFormatFlags = reinterpret_cast<SV_UINT*>( m_Data ) ;
        ConvertorFactory::setDataFormatFlags( *dataFormatFlags ) ;
    }

    // set up the SDV_PREFIX
    m_SvdPrefix = (SVD_PREFIX*)(m_Data + m_endianTagSize);
    m_SvdPrefix->tag = SVD_TAG_SYNC_HASH_COMPARE_DATA;
    m_SvdPrefix->count = 1;
    m_SvdPrefix->Flags = 0;

    // set up HashCompareInfo
    m_HashCompareInfo = (HashCompareInfo*)(m_Data +  m_endianTagSize + sizeof(SVD_PREFIX));

    // now set up the HashCompareNodes
    m_Begin = (HashCompareNode*)(m_Data + m_endianTagSize + sizeof(SVD_PREFIX) + sizeof(HashCompareInfo));       
    m_End = m_Begin;
    m_Last = m_Begin + count;
}

// --------------------------------------------------------------------------
// clears m_Data 
// --------------------------------------------------------------------------
void HashCompareData::Clear()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (0 != m_Data) {
        m_Begin = (HashCompareNode*)(m_Data + m_endianTagSize + sizeof(SVD_PREFIX) + sizeof(HashCompareInfo));
        memset(m_Begin, 0, sizeof(HashCompareNode) * m_MaxCount);        
        m_End = m_Begin;        
        m_Count = 0;
    }
}

// --------------------------------------------------------------------------
// Computes the md5 hash for the data in the buffer and places the results 
// along with the offset and length in the next available HashCompareNode
// if there are no more available HashCompareNodes it returns false else 
// returns true
// --------------------------------------------------------------------------
bool HashCompareData::ComputeHash(SV_LONGLONG offset, SV_ULONG length, char* buffer, bool bDICheck)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	unsigned char digest[INM_MD5TEXTSIGLEN / 2];

	// compute hash
	INM_MD5_CTX ctx;
	INM_MD5Init(&ctx);
	INM_MD5Update(&ctx, (unsigned char*)buffer, length);
	INM_MD5Final(digest, &ctx);
	return UpdateHash(offset,length, digest,bDICheck);
}

// --------------------------------------------------------------------------
// This routine places the digest along with the offset and length in the next
// available HashCompareNode
// if there are no more available HashCompareNodes it returns false else 
// returns true
// --------------------------------------------------------------------------
bool HashCompareData::UpdateHash(SV_LONGLONG offset, SV_ULONG length, unsigned char digest[16], bool bDICheck)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool ok;

	if (m_End == m_Last) {
		// we're at the end of the buffer can't add anymore 
		ok = false;
	}
	else {
	
		// m_End always points to the next unused location
		// and since we check that m_End is not pointing to the last location 
		// (the last location is never used so that we always have an end indicator)
		// it is OK to use m_End in this case. 

		m_End->m_Offset = offset;
		m_End->m_Length = length;
		inm_memcpy_s(m_End->m_Hash, 16, digest, 16);
		if (bDICheck)
		{
			char md5localcksum[INM_MD5TEXTSIGLEN + 1];
			for (int i = 0; i < INM_MD5TEXTSIGLEN / 2; i++)
			{
				inm_sprintf_s((md5localcksum + i * 2), (ARRAYSIZE(md5localcksum)-(i * 2)), "%02X", m_End->m_Hash[i]);
			}
			std::stringstream msg;
			msg << "Volume Offset:" << offset << "Checksum:" << md5localcksum << std::endl;
			DebugPrintf(SV_LOG_DEBUG, msg.str().c_str());
		}
		++m_End;
		++m_Count;
		ok = true;
	}

	return ok;
}



