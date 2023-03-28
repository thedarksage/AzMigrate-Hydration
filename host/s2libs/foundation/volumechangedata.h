/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volumechangedata.h
 *
 * Description: 
 */
#ifndef VOLUME_CHANGE_DATA__H
#define VOLUME_CHANGE_DATA__H

#ifdef SV_WINDOWS
#pragma once
#endif

#include <cstddef>
#include <cstdio>
#include <string>

#include "genericprofiler.h"
#include "genericdata.h"
#include "svdparse.h"

#include "devicefilter.h"

#ifdef SV_WINDOWS
typedef unsigned int inm_u32_t ;
#endif

#include "devicefilterfeatures.h"
#include "filterdrvvolmajor.h"

const unsigned long int PREFIX_SIZE_V2 = sizeof( SVD_PREFIX ) + sizeof( SVD_DIRTY_BLOCK_V2 ) ;
const unsigned long int PREFIX_SIZE = sizeof(SVD_PREFIX) + sizeof(SVD_DIRTY_BLOCK);

class DeviceStream;
class DeviceStream;
enum DATA_SOURCE
{
    SOURCE_UNDEFINED=0,
    SOURCE_BITMAP=1,
    SOURCE_META_DATA=2,
    SOURCE_DATA=4,
    SOURCE_FILE=5,
	SOURCE_STREAM=6
};

const char * const STRDATASOURCE[] = {"SOURCE_UNDEFINED", "SOURCE_BITMAP",
                                      "SOURCE_META_DATA", "", "SOURCE_DATA",
                                      "SOURCE_FILE", "SOURCE_STREAM"};

enum DB_DATA_STATUS
{
    DATA_ERROR=1,
    DATA_AVAILABLE=2,
    DATA_0=3,
	DATA_NOTAVAILABLE=4,
	DATA_RETRY
};

typedef enum ResyncRequiredStatus
{
    RESYNC_NOT_REQD,
    RESYNC_REQD,
    RESYNC_REQD_PROCESSED

} RESYNC_REQD_STATUS;


typedef enum DbState
{
    DB_UNKNOWN,
    DB_GOT,
    DB_COMMITED

} DBSTATE;


class VolumeChangeData :
	public GenericData
{
public:
	VolumeChangeData( const DeviceFilterFeatures *pDeviceFilterFeatures , 
                      const SV_ULONGLONG ullVolumeStartOffset );
	virtual ~VolumeChangeData();
	int Init(const size_t *pmetadatareadbuflen);
	unsigned long int Size();

	/// \brief commits dirty block to driver
	///
	/// \return 
    /// SV_SUCCESS : success
	/// SV_FAILURE : failure
	int Commit(DeviceStream &stream,     ///< driver stream
		const FilterDrvVol_t &sourceName ///< source name
		);

    /// \brief Notifies a dirty block transfer failure to driver
    ///
    /// \return 
    /// SV_SUCCESS : success
    /// SV_FAILURE : failure
    int CommitFailNotify(DeviceStream &stream,     ///< driver stream
        const FilterDrvVol_t &sourceName, ///< source name
        unsigned long long ullFlags,
        unsigned long long ullErrorCode);

	const char* GetData() const;
    /** Modified by BSR for source io time stamps **/
    //UDIRTY_BLOCK& GetDirtyBlock();
    void*  GetDirtyBlock();
    /** End of the changes **/
	unsigned long int GetChangesInBytes();
    const   SV_ULONGLONG GetCurrentChangeOffset() const;
    const   SV_ULONG GetCurrentChangeLength() const;

    /** Added by BSR for source io time stamps **/
    const SV_ULONG GetCurrentChangeTimeDelta() const ;
    const SV_ULONG GetCurrentChangeSequenceDelta() const ;
    /** End of the change **/

	/// \brief commits dirty block to driver
	///
	/// \return see DB_DATA_STATUS
	DB_DATA_STATUS BeginTransaction(DeviceStream& stream,            ///< driver stream
                                    const FilterDrvVol_t &sourceName ///< source name
									);

    const bool IsTransactionComplete() const;
    const unsigned long int GetTotalChanges() const;
    const unsigned long int GetCurrentChangeIndex() const;
    const int IsSpanningMultipleDirtyBlocks() const;
    DATA_SOURCE DeduceDataSource() const;
    DATA_SOURCE GetDataSource() const;
	const bool  IsContinuationEnd() const;
    void Reset(const unsigned long int uliChangeCounter=0);
    const std::string GetFileName()const;
    unsigned long GetServiceRequestDataSize();
    const bool IsResyncRequired() const;
    void SetResyncRequiredProcessed(void);
    const bool IsResyncRequiredProcessed(void) const;
	const SV_LONGLONG GetTotalChangesPending() const;
	const SV_LONGLONG GetCurrentChanges() const;
	const SV_LONGLONG GetOutOfSyncTimeStamp() const;
	const bool IsLastBuffer() const;	
    const bool IsTagsOnly() const;
    const bool IsTimeStampsOnly() const;
    const bool Good() const;
    const SV_LONGLONG GetTransactionID() const;
    VolumeChangeData& operator++();
    char* GetData();
    void SetData(char*,unsigned long int);
    void SetDataLength(const  unsigned long int);
    unsigned long int GetDataLength() const;
    const SV_ULONGLONG GetFirstChangeTimeStamp() const;
    const SV_ULONG  GetFirstChangeSequenceNumber() const;
    /** Added by BSR for source io time stamps **/
    const SV_ULONGLONG GetFirstChangeSequenceNumberV2() const ;

    const SV_ULONGLONG GetLastChangeTimeStamp() const;
    const SV_ULONG  GetLastChangeSequenceNumber() const;
    /** Added by BSR for source io time stamps **/
    const SV_ULONGLONG GetLastChangeSequenceNumberV2() const ;
    const SV_ULONG  GetSequenceIdForSplitIO() const;
    bool IsWriteOrderStateData(void) const;
    const SV_ULONGLONG  GetPrevEndSeqNo() const;
    const SV_ULONGLONG  GetPrevEndTS() const;
    const SV_ULONG  GetPrevSeqIDForSplitIO() const;
    SV_ULONGLONG GetVolumeStartOffset(void) const;

	/* Added to check if already sent and committed file is once again 
	 * given by driver to send */
	bool m_bCheckDB ;
    DBSTATE m_DbState;

    int AllocateBufferArray(inm_u32_t) ;
    void** GetBufferArray() ;    
    inm_u32_t getBufferSize() ;
    SV_INT GetNumberOfDataStreamBuffers(void);

	void SetProfiler(const Profiler& pProf);
	Profiler GetProfiler();

private:
	unsigned int GetMaxDataFileSize();
    bool AllocateReadBuffer(const size_t &readsize);
    void FreeReadBuffer(void);

private:
	bool m_bGood;	
	char* m_pszData;
    unsigned long int m_uliDataLen;
    void  ** m_ppBufferArray ;
    inm_u32_t m_ulcbChangesInStream ;
    bool m_bTransactionComplete;
	friend class VolumeStream;
	friend class DeviceStream;
	unsigned long int m_uliCurrentChange;
	UDIRTY_BLOCK dirtyBlock ;
	UDIRTY_BLOCK_V2 dirtyBlockV2 ;
	void* m_DirtyBlock ;
	const DeviceFilterFeatures *m_pDeviceFilterFeatures;
    RESYNC_REQD_STATUS m_ResyncReqdStatus;
    DATA_SOURCE m_DataSource;
    SV_ULONGLONG m_ullVolumeStartOffset;
    char *m_pData;
    size_t m_bufLen;
	Profiler m_profiler;
};

#endif

