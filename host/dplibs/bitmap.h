
#ifndef BITMAP_H
#define BITMAP_H
#include <string>
#include <sstream>
#include "ace/Basic_Types.h"
#include "svtypes.h"
#include "svbitset.h"
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <ace/Manual_Event.h>
#include <ace/Task.h>
#include <ace/Atomic_Op.h>
#include "volumegroupsettings.h"
#include "dataprotectionexception.h"
#include "cxtransport.h"
#include "hostagenthelpers_ported.h"

typedef std::vector<std::string> Files_t;
typedef struct BitMapHeader
{
	SV_ULONG m_bitMapGranularity ;
	SV_ULONGLONG m_bytesProcessed ;
	SV_ULONGLONG m_bytesNotProcessed ; //only used by Sync apply tasks
} BitMapHdr;

class PersistentBitMap
{
private:

	BitMapHdr m_bitMapHeader ;
	std::string m_fileName ;
public: 
	SV_ULONGLONG addToBytesProcessed( SV_ULONG bytesProcessed ) 
	{
		m_bitMapHeader.m_bytesProcessed += bytesProcessed ;
		return m_bitMapHeader.m_bytesProcessed ;
	}

	SV_ULONGLONG addToBytesNotProcessed( SV_ULONG bytesNotProcessed ) 
	{ 
		m_bitMapHeader.m_bytesNotProcessed += bytesNotProcessed ; 
		return m_bitMapHeader.m_bytesNotProcessed ;
	}

	BitMapHdr getBitMapHdr() { return this->m_bitMapHeader; }

	PersistentBitMap(){}
	PersistentBitMap(const std::string &fileName, const SV_ULONG  bitMapGranularity) ;

	std::string getFileName();
	SV_ULONG getBitMapGranularity() const { return m_bitMapHeader.m_bitMapGranularity ; } 
	SV_ULONGLONG getBytesProcessed() const ;
	SV_ULONGLONG getBytesNotProcessed() const ;
	SVERROR loadBitMapFileDataToMemory(const char* info, const SV_UINT infoSize, std::string &  bitMapData ) ;
	SVERROR createBitMapFileDataFromMemory(boost::shared_array <char>& info, SV_UINT& infoSize, const std::string& bitMapData ) ;
	//setters
	void setFileName( std::string );
	void setBitMapGranularity(SV_ULONG );
	void setBytesProcessed( SV_ULONGLONG ) ;
	void setBytesNotProcessed( SV_ULONGLONG ) ;

    virtual SVERROR read(std::string &, SV_ULONG expectedBitMapSize, CxTransport::ptr, const bool &bShouldCreateIfNotPresent) = 0 ;
    virtual SVERROR write(const std::string&,CxTransport::ptr) = 0;
    virtual SVERROR deleteFromPersistentStore(CxTransport::ptr) = 0 ;

	virtual ~PersistentBitMap() ;
};

class OnDiskBitMap : public PersistentBitMap
{
private:
public:
	OnDiskBitMap( std::string fileName, SV_ULONG  bitMapGranularity ) ;
	virtual ~OnDiskBitMap() {}

    virtual SVERROR read(std::string &,SV_ULONG expectedBitMapSize, CxTransport::ptr, const bool &bShouldCreateIfNotPresent) ;
    virtual SVERROR write(const std::string &,CxTransport::ptr) ;
    virtual SVERROR deleteFromPersistentStore(CxTransport::ptr cxTransport);
	OnDiskBitMap(){}
};


class RemoteBitMap : public PersistentBitMap
{
	bool m_Secure ;
    bool m_bForceRename;
public:
	explicit RemoteBitMap(const std::string &RemoteName, const bool bForceRename, const SV_ULONG  bitMapGranularity, bool isSecure) ;

	virtual ~RemoteBitMap() {}
    virtual SVERROR read(std::string&, SV_ULONG expectedBitMapSize, CxTransport::ptr, const bool &bShouldCreateIfNotPresent) ;
	virtual SVERROR write(const std::string&,CxTransport::ptr) ;
	virtual SVERROR deleteFromPersistentStore(CxTransport::ptr cxTransport);
};

template<class T>
class BitMap
{
private:
	svbitset* m_bitset;
	PersistentBitMap* m_persistentBitMap;
	SV_ULONG m_bitmapSize;
	bool m_isLastChunkProcessed;
	SV_ULONG m_chunksProcessed;
	SV_ULONGLONG m_lastChunkSize;
	mutable T m_Lock ;
	typedef ACE_Guard< T> LockGuard;

private:
	SV_ULONGLONG addToBytesProcessed( SV_ULONG bytesProcessed ) ;
	SV_ULONGLONG addToBytesNotProcessed( SV_ULONG bytesNotProcessed ) ;
	void markInMemory(size_t bitPos, bool value) ;
	void setBytesProcessed( const SV_ULONGLONG bytesProcessed )  ;
	void setBytesNotProcessed( const SV_ULONGLONG bytesNotProcessed ) ;

public:
	~BitMap();
	BitMap( const std::string &fileName, const SV_ULONG bitmapSize, const bool bForceRename, const SV_ULONG bitMapGranularity,
        bool isSecure = false) ;
	SVERROR initialize(CxTransport::ptr cxTransport, const bool & bShouldCreateIfNotPresent) ;
	bool operator==(BitMap& bitmap);
	std::string GetBitMapFileName();
	SVERROR deleteFromPersistentStore(CxTransport::ptr cxTransport);
	SVERROR syncToPersistentStore(CxTransport::ptr cxTransport, std::string fileName="");
    std::string getBitMapInfo();
    void dumpBitMapInfo(SV_LOG_LEVEL logLevel = SV_LOG_DEBUG);
	bool isBitSet(size_t  bitPos) ;
	size_t getNextUnsetBit(size_t);
	size_t getNextSetBit(size_t);
	size_t getFirstSetBit();
	size_t getFirstUnSetBit();

	SV_ULONGLONG getBytesProcessed() const ;
	SV_ULONGLONG getBytesNotProcessed()  const;
	SV_ULONG getBitMapGranualarity()  const;

	void setBitMapGranualarity( const SV_ULONG chunkSize ) ;
	void markAsMatched(SV_ULONGLONG offset, SV_ULONG bytes) ;
	void markAsUnMatched(SV_ULONGLONG offset, SV_ULONG bytes=0) ;
	void markAsNotProcessed( SV_ULONGLONG offset );
	void setPartialMatchedBytes( SV_ULONGLONG bytes) ;
	void markAsApplied(SV_ULONGLONG offset, SV_ULONG appliedBytes, SV_ULONG partialMatchedBytes) ;
	void markAsNotApplied( SV_ULONGLONG offset ) ;
	void markAsTransferred(SV_ULONGLONG offset, SV_ULONG bytes) ;
	void markAsNotTransferred(SV_ULONGLONG offset, SV_ULONG bytes) ;
    void setExplicitBytesProcessed( const SV_ULONGLONG bytesProcessed )  ;
    bool isAnyChunkProcessed() ;
    SV_ULONG getBitMapSize() ;
    void markAsFullyUsed(SV_ULONGLONG offset);
    void markAsFullyUnUsed(SV_ULONGLONG offset, SV_ULONG unusedbytes = 0);
    void markAsPartiallyUsed(SV_ULONGLONG offset);
};
typedef BitMap<ACE_Recursive_Thread_Mutex> SharedBitMap;

//private
template <class T>
void BitMap<T>::setBytesProcessed( const SV_ULONGLONG bytesProcessed )  
{ 
	LockGuard guard(m_Lock);
	m_persistentBitMap->setBytesProcessed( bytesProcessed ) ; 
}


template <class T>
void BitMap<T>::setBytesNotProcessed( const SV_ULONGLONG bytesNotProcessed ) 
{ 
	LockGuard guard(m_Lock);
	m_persistentBitMap->setBytesNotProcessed( bytesNotProcessed ) ; 
}


template <class T>
void BitMap<T>::markInMemory(size_t  bitPos, bool value) 
{
	LockGuard guard(m_Lock);
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	if( bitPos > this->m_bitmapSize - 1 )
		DebugPrintf(SV_LOG_ERROR, "The bit position is invalid... ignoring....\n");
	else
	{
		if(value)
			m_bitset->setByPos(bitPos);
		else
			m_bitset->clearbyPos(bitPos);		
	}
	DebugPrintf(SV_LOG_DEBUG,"Marked Index %d and its value is %d\n", bitPos, value);
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}

template <class T>
SV_ULONGLONG BitMap<T>::addToBytesProcessed( SV_ULONG bytesProcessed ) 
{ 
	LockGuard guard(m_Lock);
	return m_persistentBitMap->addToBytesProcessed( bytesProcessed ) ; 
}

template <class T>
SV_ULONGLONG BitMap<T>::addToBytesNotProcessed( SV_ULONG bytesNotProcessed ) 
{ 
	LockGuard guard(m_Lock);
	return m_persistentBitMap->addToBytesNotProcessed( bytesNotProcessed ) ; 
}

//Public
template <class T>
BitMap<T>::BitMap(const std::string &fileName, const SV_ULONG bitmapSize, const bool bForceRename, const SV_ULONG bitMapGranularity,
    bool isSecure)
{
	LockGuard guard(m_Lock);
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	m_isLastChunkProcessed = false;
	m_chunksProcessed = 0;
	m_bitset = NULL ;
	m_persistentBitMap = NULL ;
	
	m_persistentBitMap = new RemoteBitMap(fileName, bForceRename, bitMapGranularity, isSecure);
	this->m_bitmapSize = bitmapSize;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

template <class T>
SVERROR BitMap<T>::initialize(CxTransport::ptr cxTransport, const bool & bShouldCreateIfNotPresent)
{
	LockGuard guard(m_Lock);
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	std::string str;
	SVERROR status;
	if( this->m_persistentBitMap->read(str, this->m_bitmapSize, cxTransport, bShouldCreateIfNotPresent) != SVS_OK )
		status = SVS_FALSE;
	else
	{
        m_bitset = new svbitset();
        bool bisinitialized = false;
		if(str.empty())
        {
            bisinitialized = bShouldCreateIfNotPresent ? (m_bitset->initialize(this->m_bitmapSize, false)) :
                                                         false;
        }
		else
		{
            bisinitialized = m_bitset->initialize(str);
		}

		status = bisinitialized?SVS_OK:SVS_FALSE;
        if (bisinitialized)
        {
		    dumpBitMapInfo();
        }
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
	return status;
}


template <class T>
SVERROR BitMap<T>::syncToPersistentStore(CxTransport::ptr cxTransport,std::string fileName  )
{
	LockGuard guard(m_Lock);

	if( m_bitset == NULL || m_persistentBitMap == NULL )
	{
		DebugPrintf( SV_LOG_WARNING, "Bit Map not initialized properly.. will not be able to persist the bitmap\n");
		return SVS_FALSE ;
	}

	if( fileName.compare("") != 0 )
	{
		m_persistentBitMap->setFileName(fileName);
	}

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	SVERROR status = SVS_OK;
	dumpBitMapInfo();
	if( m_persistentBitMap->write(m_bitset->to_string(),cxTransport) != SVS_OK )
		status = SVS_FALSE;
	else
		status = SVS_OK;
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
	return status;
}


template <class T>
BitMap<T>::~BitMap()
{
	LockGuard guard(m_Lock);
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	SAFE_DELETE( this->m_bitset );
	SAFE_DELETE( this->m_persistentBitMap );
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}



template <class T>
size_t BitMap<T>::getNextUnsetBit(size_t pivot)
{
	LockGuard guard(m_Lock);
	size_t index = 0;
	index = m_bitset->getNextUnsetBit(pivot);
	DebugPrintf(SV_LOG_DEBUG, "Unset Bit after pivot Index %d is %d\n", pivot, index);
	return index;
}


template <class T>
size_t BitMap<T>::getNextSetBit(size_t pivot)
{
	LockGuard guard(m_Lock);
	size_t index = 0;
	index = m_bitset->getNextSetBit(pivot);
	DebugPrintf(SV_LOG_DEBUG, "Set Bit after pivot Index %d is %d\n", pivot, index);
	return index;
}

template <class T>
std::string BitMap<T>::GetBitMapFileName()
{
	LockGuard guard(m_Lock);
	std::string str = m_bitset->to_string();
	return m_persistentBitMap->getFileName();

}



template <class T>
bool BitMap<T>::operator==(BitMap& bm)
{
	LockGuard guard(m_Lock);
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	std::string bitmap_str1 = this->m_bitset->to_string();
	std::string bitmap_str2 = bm.m_bitset->to_string();
	bool status = false;
	if( bitmap_str1.compare(bitmap_str2) == 0 )
		status =  true;
	else
		status = false;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return status;
}


template <class T>
std::string BitMap<T>::getBitMapInfo()
{
    LockGuard guard(m_Lock);
    std::stringstream ssBitMapInfo;
    ssBitMapInfo << " BitMap FileName " << m_persistentBitMap->getFileName() << " and its size " << m_bitmapSize
        << "\n Processed Bytes " << m_persistentBitMap->getBytesProcessed()
        << "\n Not Processed Bytes " << m_persistentBitMap->getBytesNotProcessed()
        << "\n Granularity " << m_persistentBitMap->getBitMapGranularity()
        << "\n The BitMap String is " << m_bitset->to_string();
    return ssBitMapInfo.str();
}


template <class T>
void BitMap<T>::dumpBitMapInfo(SV_LOG_LEVEL logLevel)
{
    DebugPrintf(logLevel, "%s\n", getBitMapInfo().c_str());
}


template <class T>
bool BitMap<T>::isBitSet(size_t bitPos) 
{ 
	LockGuard guard(m_Lock);
	return m_bitset->isSetByPos(bitPos) ;
}


template <class T>
size_t BitMap<T>::getFirstSetBit()
{
	LockGuard guard(m_Lock);
	size_t index = 0;
	index = m_bitset->findFirstSetBit();
	DebugPrintf(SV_LOG_DEBUG, "First Set Bit index is %d\n", index);
	return index;
}

template <class T>
size_t BitMap<T>::getFirstUnSetBit()
{
	LockGuard guard(m_Lock);
	size_t index = 0;
	index = m_bitset->findFirstUnsetBit();
	DebugPrintf(SV_LOG_DEBUG, "First Unset Bit is %d\n", index);
	return index;
}


template <class T>
SV_ULONGLONG BitMap<T>::getBytesProcessed()  const
{ 
	LockGuard guard(m_Lock);
	return m_persistentBitMap->getBytesProcessed() ; 
}


template <class T>
SV_ULONGLONG BitMap<T>::getBytesNotProcessed()  const
{ 
	LockGuard guard(m_Lock);
	return m_persistentBitMap->getBytesNotProcessed() ; 
}


template <class T>
SV_ULONG BitMap<T>::getBitMapGranualarity()  const
{ 
	LockGuard guard(m_Lock);
	return m_persistentBitMap->getBitMapGranularity() ; 
}


template <class T>
void BitMap<T>::setBitMapGranualarity( const SV_ULONG chunkSize ) 
{ 
	LockGuard guard(m_Lock);
	m_persistentBitMap->setBitMapGranularity( chunkSize ) ;  
}

template <class T>
void BitMap<T>::markAsMatched(SV_ULONGLONG offset, SV_ULONG bytes)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, true );
	markInMemory( chunkNo * 2, true );
	addToBytesProcessed( bytes );
}


template <class T>
void BitMap<T>::markAsUnMatched(SV_ULONGLONG offset, SV_ULONG bytes)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, true );
	markInMemory( chunkNo * 2, false );

	setBytesProcessed( getBytesProcessed() - bytes );
}


template <class T>
void BitMap<T>::markAsNotProcessed( SV_ULONGLONG offset )
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, false );
	markInMemory( chunkNo * 2, false );
}


template <class T>
void BitMap<T>::setPartialMatchedBytes( SV_ULONGLONG bytes)
{
	LockGuard guard(m_Lock);
	setBytesNotProcessed( bytes );
}


template <class T>
void BitMap<T>::setExplicitBytesProcessed(const SV_ULONGLONG bytesProcessed )
{
    LockGuard guard(m_Lock);
    setBytesProcessed( bytesProcessed );
}


template <class T>
void BitMap<T>::markAsApplied(SV_ULONGLONG offset, SV_ULONG appliedBytes, SV_ULONG partialMatchedBytes)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	if( isBitSet( chunkNo * 2 + 1 ) && isBitSet( chunkNo * 2 )  )
	{
		DebugPrintf(SV_LOG_DEBUG, "The sync Data for Chunk %d is already applied.. So not updating the bitmap for this chunk\n", chunkNo );
	}
	else
	{
		markInMemory( chunkNo * 2 + 1, true );
		markInMemory( chunkNo * 2, true );
		addToBytesProcessed( appliedBytes );
		addToBytesNotProcessed( partialMatchedBytes );
	}
}

template <class T>
void BitMap<T>::markAsNotApplied( SV_ULONGLONG offset )
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, true );
	markInMemory( chunkNo * 2, false );

}


template <class T>
void BitMap<T>::markAsTransferred(SV_ULONGLONG offset, SV_ULONG bytes)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo, true );
	addToBytesProcessed( bytes );
}


template <class T>
void BitMap<T>::markAsNotTransferred(SV_ULONGLONG offset, SV_ULONG bytes)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo, false );
	setBytesProcessed( getBytesProcessed() - bytes );
}
template <class T>
SVERROR BitMap<T>::deleteFromPersistentStore(CxTransport::ptr cxTransport )
{
	return m_persistentBitMap->deleteFromPersistentStore(cxTransport) ;
}
template <class T>
bool BitMap<T>::isAnyChunkProcessed()
{
    LockGuard guard(m_Lock) ;
    if( m_bitset->findFirstSetBit() == boost::dynamic_bitset<>::npos )
    {
        return false ;
    }
    else
    {
        return true ;
    }
}


template <class T>
SV_ULONG BitMap<T>::getBitMapSize()
{
	LockGuard guard(m_Lock);
    return m_bitmapSize;
}


template <class T>
void BitMap<T>::markAsFullyUsed(SV_ULONGLONG offset)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, true );
	markInMemory( chunkNo * 2, true );
}


template <class T>
void BitMap<T>::markAsFullyUnUsed(SV_ULONGLONG offset, SV_ULONG unusedbytes)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, false );
	markInMemory( chunkNo * 2, true );
	addToBytesNotProcessed( unusedbytes );
}


template <class T>
void BitMap<T>::markAsPartiallyUsed(SV_ULONGLONG offset)
{
	LockGuard guard(m_Lock);
	SV_ULONG chunkNo = (SV_ULONG) (offset / m_persistentBitMap->getBitMapGranularity());
	markInMemory( chunkNo * 2 + 1, true );
	markInMemory( chunkNo * 2, false );
}

#endif
