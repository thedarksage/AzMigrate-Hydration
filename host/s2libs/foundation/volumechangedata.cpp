/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volumechangedata.cpp
 *
 * Description: 
 */
#include <string>
#include <sstream>
#include <iostream>

#include "entity.h"
#include "volume.h"
#include "error.h"
#include "portablehelpers.h"
#include "globs.h"

#include "genericstream.h"
#include "inputoutputstream.h"
#include "filestream.h"
#include "devicestream.h"

#include "genericdata.h"
#include "volumechangedata.h"
#include "logger.h"

/*********************************************************************************************
*** Note: Please write any platform specific code in ${platform}/volumechangedata_port.cpp ***
*********************************************************************************************/

/*
 * FUNCTION NAME : VolumeChangeData
 *
 * DESCRIPTION : default constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
VolumeChangeData::VolumeChangeData( const DeviceFilterFeatures *pinvolfltFeatures, 
                                    const SV_ULONGLONG ullVolumeStartOffset ):
                                        m_uliCurrentChange(0),
                                        m_pszData(NULL),
                                        m_uliDataLen(0),
                                        m_bGood(false),
					                    m_pDeviceFilterFeatures(pinvolfltFeatures),
                                        m_ullVolumeStartOffset(ullVolumeStartOffset),
                                        m_ResyncReqdStatus(RESYNC_NOT_REQD),
	                                    m_bCheckDB(false),
                                        m_DbState(DB_UNKNOWN),
                                        m_DataSource(SOURCE_UNDEFINED),
                                        m_ppBufferArray(NULL),
                                        m_ulcbChangesInStream(0),
                                        m_pData(0),
                                        m_bufLen(0)
{
	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
    {   
	    m_DirtyBlock = &dirtyBlockV2 ;
	}
	else
    {
	    m_DirtyBlock = &dirtyBlock ;
    }    
}

/*
 * FUNCTION NAME : Reset
 *
 * DESCRIPTION : resets the change
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void VolumeChangeData::Reset(const unsigned long int uliChangeCounter)
{
    m_uliCurrentChange = uliChangeCounter;
    m_bEmpty = true;
    m_uliDataLen = 0;
    m_bTransactionComplete = false;
}

/*
 * FUNCTION NAME : IsLastBuffer
 *
 * DESCRIPTION : returns true if the current buffer is last
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const bool VolumeChangeData::IsLastBuffer()const	
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    
    unsigned short usNumberOfBuffers ; 
	if ( Good() )
    {
        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {
	    usNumberOfBuffers = ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock )->uHdr.Hdr.usNumberOfBuffers ;
        }  
        else
        {
	    usNumberOfBuffers = ( ( UDIRTY_BLOCK* ) m_DirtyBlock )->uHdr.Hdr.usNumberOfBuffers ;
        }

        return ( usNumberOfBuffers == m_uliCurrentChange);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return false;        
}

/*
 * FUNCTION NAME : GetMaxDataFileSize
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
unsigned int VolumeChangeData::GetMaxDataFileSize()
{
	unsigned long  RequestDataSize = 0;
	RequestDataSize = GetServiceRequestDataSize( );
    RequestDataSize = RequestDataSize * 1024; 
	//these vales are in KB convert to bytes
	//This is maximum change size in byets for data mode. But we have to consider Header and DRTD prefixes also
	//Consiedring that there can be maximum 8192 changes and each DRTD prefix is 28 bytes 8192*28 = 229K memory + tag 
	// approximately 250 KB is sufficient
	const int HEADER_DATA_MEMORY = (250*1024);
	RequestDataSize += HEADER_DATA_MEMORY;
	return RequestDataSize;
}

const SV_ULONG VolumeChangeData::GetCurrentChangeTimeDelta() const 
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME ) ;
    if( Good() )
    {
    	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
	{
	    return ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock )->TimeDeltaArray[ m_uliCurrentChange ] ;
	}
    }
    return 0 ;
}

const SV_ULONG VolumeChangeData::GetCurrentChangeSequenceDelta() const 
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME ) ;
    if( Good() ) 
    {
    	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
	{
	    return ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock )->SequenceNumberDeltaArray[ m_uliCurrentChange ] ;
	}
    }
    return 0 ;
}

/*
 * FUNCTION NAME : GetCurrentChangeLength
 *
 * DESCRIPTION : returns the length of the current change
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const   SV_ULONG VolumeChangeData::GetCurrentChangeLength() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
   
    if ( Good() )
    {
	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {  
    	    return ( (UDIRTY_BLOCK_V2*) m_DirtyBlock )->ChangeLengthArray[ m_uliCurrentChange ] ; 
        }
        else
        {
    	    return ( ( UDIRTY_BLOCK*) m_DirtyBlock )->ChangeLengthArray[ m_uliCurrentChange ] ;
        }
    }

    return 0;        
}

/*
 * FUNCTION NAME : GetDataSource
 *
 * DESCRIPTION : returns the datasource of the dirtyblock
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
DATA_SOURCE VolumeChangeData::DeduceDataSource() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	if ( Good() )
	{
        SV_UINT ulFlags ;
        SV_UINT ulDataSource ;
	void** ppBufferArray ;

	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
	{
            UDIRTY_BLOCK_V2* tempDirtyBlk = ( UDIRTY_BLOCK_V2* ) m_DirtyBlock ;
	    ulFlags = tempDirtyBlk->uHdr.Hdr.ulFlags ;
	    ulDataSource = tempDirtyBlk->uTagList.TagList.TagDataSource.ulDataSource ;
	    ppBufferArray = tempDirtyBlk->uHdr.Hdr.ppBufferArray ;
	}
	else
	{
            UDIRTY_BLOCK* tempDirtyBlk = ( UDIRTY_BLOCK* ) m_DirtyBlock ;
	    ulFlags = tempDirtyBlk->uHdr.Hdr.ulFlags ;
	    ulDataSource = tempDirtyBlk->uTagList.TagList.TagDataSource.ulDataSource ;
	    ppBufferArray = tempDirtyBlk->uHdr.Hdr.ppBufferArray ;
	
	}

        if( ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE) 
        {
            return SOURCE_FILE;
        }
	else if( ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM)
	    {
		    return SOURCE_STREAM;
	    }

        switch( ulDataSource)
        {   
            case INVOLFLT_DATA_SOURCE_DATA:
                return SOURCE_DATA;
            break;

            case INVOLFLT_DATA_SOURCE_META_DATA:
                //assert( NULL == ppBufferArray );
                return SOURCE_META_DATA;
            break;

            case INVOLFLT_DATA_SOURCE_BITMAP:
                assert( NULL == ppBufferArray );
                return SOURCE_BITMAP;
            break;

            default:
                return SOURCE_UNDEFINED;
            break;
        }
    }        
 	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return SOURCE_UNDEFINED;
}


DATA_SOURCE VolumeChangeData::GetDataSource() const
{
    return m_DataSource;
}


/*
 * FUNCTION NAME : GetDirtyBlock
 *
 * DESCRIPTION : returns reference to dirtyblock
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void* VolumeChangeData::GetDirtyBlock() 
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return m_DirtyBlock;
}

/*
 * FUNCTION NAME : GetCurrentChangeIndex
 *
 * DESCRIPTION : returns the current change index that is has to be processed
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const unsigned long int VolumeChangeData::GetCurrentChangeIndex() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return m_uliCurrentChange;    
}

/*
 * FUNCTION NAME : IsSpanningMultipleDirtyBlocks
 *
 * DESCRIPTION : tells if the change is spanning multiple dirty blocks
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const int VolumeChangeData::IsSpanningMultipleDirtyBlocks() const
{
	/*
	 * BUGBUG: obsolete
	 */
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    SV_ULONG MULTIPLE_DIRTY_BLOCKS = 7;
    SV_UINT ulFlags ;
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
    	ulFlags = ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock )->uHdr.Hdr.ulFlags ;
    }
    else
    {
    	ulFlags = ( ( UDIRTY_BLOCK* ) m_DirtyBlock )->uHdr.Hdr.ulFlags ;
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return ( ulFlags & MULTIPLE_DIRTY_BLOCKS);
}

/*
 * FUNCTION NAME : GetData
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const char* VolumeChangeData::GetData() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	if ( Good() )
	    return m_pszData;
    return NULL;	    
}

/*
 * FUNCTION NAME : IsTransactionComplete
 *
 * DESCRIPTION : returns true if all the changes in the dirty block have been processed
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const bool VolumeChangeData::IsTransactionComplete() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    if ( Good() )
        return m_bTransactionComplete;
    return false;        
}

/*
 * FUNCTION NAME : GetTotalChanges
 *
 * DESCRIPTION : returns the number of changes in the dirty block
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const unsigned long int VolumeChangeData::GetTotalChanges() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	if ( Good() )
    {
	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
	{
	    return ( ( UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.cChanges;    
	}
	else
	{
	    return ( ( UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.cChanges;    
	}
    }
    return 0;        
}

/*
 * FUNCTION NAME : Size
 *
 * DESCRIPTION : returns the length of the data
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
unsigned long int VolumeChangeData::Size()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    if ( Good() )
	    return m_uliDataLen;
    return 0;	    
}

/*
 * FUNCTION NAME : IsResyncRequired
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const bool VolumeChangeData::IsResyncRequired() const
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bresyncreqd = false;

    if (Good() )
    {
        bresyncreqd = (RESYNC_REQD == m_ResyncReqdStatus);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with retval = %s\n",FUNCTION_NAME, bresyncreqd?"true":"false");
    return bresyncreqd;
}

/*
 * FUNCTION NAME : SetResyncRequiredProcessed
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :
 *
 * OUTPUT PARAMETERS :
 *
 * NOTES :
 *
 * return value :
 *
 */
void VolumeChangeData::SetResyncRequiredProcessed(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (Good() )
    {
        m_ResyncReqdStatus = RESYNC_REQD_PROCESSED;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
 * FUNCTION NAME : IsResyncRequiredProcessed
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :
 *
 * OUTPUT PARAMETERS :
 *
 * NOTES :
 *
 * return value :
 *
 */
const bool VolumeChangeData::IsResyncRequiredProcessed(void) const
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bresyncreqdprocessed = false;

    if (Good() )
    {
        bresyncreqdprocessed = (RESYNC_REQD_PROCESSED == m_ResyncReqdStatus);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with retval = %s\n",FUNCTION_NAME, bresyncreqdprocessed?"true":"false");
    return bresyncreqdprocessed;
}


/*
 * FUNCTION NAME : IsContinuationEnd
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const bool VolumeChangeData :: IsContinuationEnd() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if ( Good() )
    {
        SV_UINT ulFlags ;
	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
	{
	    ulFlags = ( ( UDIRTY_BLOCK_V2* )m_DirtyBlock)->uHdr.Hdr.ulFlags ; 
	}
	else
	{
	    ulFlags = ( ( UDIRTY_BLOCK* )m_DirtyBlock)->uHdr.Hdr.ulFlags ; 
	}

    DebugPrintf( SV_LOG_DEBUG, "ulFlags: %u\n", ulFlags ) ; 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return !( ulFlags & 
		( UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE | UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE ) ) ;
    }
    return false;        
}

/*
 * FUNCTION NAME : operator++
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
VolumeChangeData& VolumeChangeData::operator++()
{   
    if ( Good() )
    {
        switch ( GetDataSource() )
        {
            case SOURCE_STREAM:
	    {
#ifdef SV_WINDOWS
                SV_ULONG ulcbChangesInStream ;
                SV_ULONG ulBufferSize ;

#elif SV_UNIX
                SV_UINT ulcbChangesInStream ;
                SV_UINT ulBufferSize ;
#endif
		if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
		{
		    ulcbChangesInStream = ( (UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.ulcbChangesInStream ;
		    ulBufferSize = ( (UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.ulBufferSize ;
		}
		else
				{
		    ulcbChangesInStream = ( (UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.ulcbChangesInStream ;
		    ulBufferSize = ( (UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.ulBufferSize ;
		}
#ifdef SV_WINDOWS
        //We are not relying on Data.m_DirtyBlock.uHdr.Hdr.usNumberOfBuffers as it returns invalid numbers some times.
        if ( m_uliCurrentChange == (GetNumberOfDataStreamBuffers() - 1))            
        {
            m_bTransactionComplete = true;
        }
        ++m_uliCurrentChange;
#elif SV_UNIX
                if ( m_uliCurrentChange == ulcbChangesInStream) 
                {
                    m_bTransactionComplete = true;
                }
#endif
	    }
            break;
            
            case SOURCE_DATA:

            case SOURCE_META_DATA:

            case SOURCE_BITMAP:
                ++m_uliCurrentChange;
                if ( GetCurrentChangeIndex() >= GetTotalChanges() )
                {
                    m_bTransactionComplete = true;
                }    
            break;

            case SOURCE_UNDEFINED:
            default:
                DebugPrintf(SV_LOG_FATAL,"Unknown data source. Should not get there. LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            break;
        
        }
    }        
    return *this;
}

/*
 * FUNCTION NAME : SetDataLength
 *
 * DESCRIPTION : Sets the data length member to the new value
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void VolumeChangeData::SetDataLength(const  unsigned long int uliNewLen)
{
    m_uliDataLen = uliNewLen;
    if ( 0 == m_uliDataLen)
    {
        m_bEmpty = true;
        if ( Good() && ( GetCurrentChangeIndex() < GetTotalChanges()) )
        {
            m_bTransactionComplete = false;
        }    
    }        
    else if ( 0 < m_uliDataLen)
        m_bEmpty = false;    
}

/*
 * FUNCTION NAME : GetDataLength
 *
 * DESCRIPTION : returns the length of the data as unsigned long
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
unsigned long int VolumeChangeData::GetDataLength() const
{
    return m_uliDataLen;
}

/*
 * FUNCTION NAME : GetData
 *
 * DESCRIPTION : returns the pointer to the data buffer
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
char* VolumeChangeData::GetData() 
{
	/*
	 * BUGBUG: duplicate definition
	 */
    return m_pszData;
}


/*
 * FUNCTION NAME : SetData
 *
 * DESCRIPTION : Sets the m_pszData to pszData and length to len
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void VolumeChangeData::SetData(char* pszData,unsigned long int len)
{
    m_pszData = pszData;
    SetDataLength(len);
}


/*
 * FUNCTION NAME : GetFirstChangeTimeStamp
 *
 * DESCRIPTION : returns the time stamp of the first change as a long long
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const SV_ULONGLONG VolumeChangeData::GetFirstChangeTimeStamp() const
{
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
        return  ( ( UDIRTY_BLOCK_V2* )m_DirtyBlock )->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
    }
    else
    {
        return ( ( UDIRTY_BLOCK* ) m_DirtyBlock )->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
    }
}

/*
 * FUNCTION NAME : GetFirstChangeSequenceNumber
 *
 * DESCRIPTION : returns the sequence number of the first change as a long long
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const SV_ULONG  VolumeChangeData::GetFirstChangeSequenceNumber() const
{
    return ( ( UDIRTY_BLOCK*)m_DirtyBlock)->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber;
}

const SV_ULONGLONG VolumeChangeData::GetFirstChangeSequenceNumberV2() const
{
    return ( ( UDIRTY_BLOCK_V2*)m_DirtyBlock)->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber;
}
/*
 * FUNCTION NAME : GetLastChangeTimeStamp
 *
 * DESCRIPTION : returns the time stamp of the last change as a long long
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const SV_ULONGLONG VolumeChangeData::GetLastChangeTimeStamp() const
{
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
    {
        return ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock )->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
    }
    else
    {
        return ( ( UDIRTY_BLOCK* ) m_DirtyBlock )->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
    }
}

/*
 * FUNCTION NAME : GetLastChangeSequenceNumber
 *
 * DESCRIPTION : returns the sequence number of the last change
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const SV_ULONG  VolumeChangeData::GetLastChangeSequenceNumber() const
{
    return ( ( UDIRTY_BLOCK* ) m_DirtyBlock )->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber;
}

const SV_ULONGLONG VolumeChangeData::GetLastChangeSequenceNumberV2() const
{
    return ( ( UDIRTY_BLOCK_V2*) m_DirtyBlock )->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber ;
}
/*
 * FUNCTION NAME : GetSequenceIdForSplitIO
 *
 * DESCRIPTION : returns the sequence id of the split io.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const SV_ULONG  VolumeChangeData::GetSequenceIdForSplitIO() const
{
    if ( Good() )
    {
	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
	{
    	    return ( (UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.ulSequenceIDforSplitIO;
	}
	else
	{
    	    return ( (UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.ulSequenceIDforSplitIO;
	}
    }
    return 0;        
}

/*
 * FUNCTION NAME : Good
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : returns true if the object is good.
 *
 */
const bool VolumeChangeData::Good() const
{
    return m_bGood;
}


const SV_ULONGLONG  VolumeChangeData::GetPrevEndSeqNo() const
{
    SV_ULONGLONG ullPrevEndSeqNo = 0;
    if (Good() && m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
        if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            ullPrevEndSeqNo = ( (UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.ullPrevEndSequenceNumber;
        }
        else
        {
            ullPrevEndSeqNo = ( (UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.ulPrevEndSequenceNumber;
        }
    }
    return ullPrevEndSeqNo;
}


const SV_ULONGLONG  VolumeChangeData::GetPrevEndTS() const
{
    SV_ULONGLONG ullPrevEndTS = 0;
    if (Good() && m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
        if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            ullPrevEndTS = ( (UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.ullPrevEndTimeStamp;
        }
        else
        {
            ullPrevEndTS = ( (UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.ullPrevEndTimeStamp;
        }
    }
    return ullPrevEndTS;
}


const SV_ULONG  VolumeChangeData::GetPrevSeqIDForSplitIO() const
{
    SV_ULONG ulPrevSeqIDForSplitIO = 0;
    if (Good() && m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
        if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            ulPrevSeqIDForSplitIO = ( (UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.ulPrevSequenceIDforSplitIO;
        }
        else
        {
            ulPrevSeqIDForSplitIO = ( (UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.ulPrevSequenceIDforSplitIO;
        }
    }
    return ulPrevSeqIDForSplitIO;
}


bool VolumeChangeData::IsWriteOrderStateData(void) const
{
    bool bretval = false;
    if (m_pDeviceFilterFeatures->IsWriteOrderStateSupported())
    {
        etWriteOrderState ewriteorderstate;
        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {
            UDIRTY_BLOCK_V2* tempDirtyBlk = ( UDIRTY_BLOCK_V2* ) m_DirtyBlock ;
            ewriteorderstate = tempDirtyBlk->uHdr.Hdr.eWOState;
        }
        else
        {
            UDIRTY_BLOCK* tempDirtyBlk = ( UDIRTY_BLOCK* ) m_DirtyBlock ;
            ewriteorderstate = tempDirtyBlk->uHdr.Hdr.eWOState;
        }

        bretval = ( ecWriteOrderStateData == ewriteorderstate );
    }
    else
    {
        bretval = (SOURCE_STREAM == GetDataSource());
    }

    return bretval;
}


SV_ULONGLONG VolumeChangeData::GetVolumeStartOffset(void) const
{
    return m_ullVolumeStartOffset;
}


/*
 * FUNCTION NAME : Init
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
int VolumeChangeData::Init(const size_t *pmetadatareadbuflen)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus;
	
	FreeReadBuffer();
	iStatus = AllocateReadBuffer(*pmetadatareadbuflen) ? SV_SUCCESS : SV_FAILURE;
	if (SV_SUCCESS == iStatus)
	{
		m_bufLen = *pmetadatareadbuflen;
	}
	else
	{
		std::stringstream ss;
		ss << (*pmetadatareadbuflen);
		DebugPrintf(SV_LOG_ERROR, "Failed to allocate %s bytes for metadata read buffer\n", ss.str().c_str());
	}

    if (SV_SUCCESS == iStatus)
    {
	    m_bInitialized = true;
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return iStatus;
}
//These methods are needed to get disk read latencies in profiler buckets (like 2 to 4 msec, 4 to 8 msec etc...)
Profiler VolumeChangeData::GetProfiler()
{
	return m_profiler;
}

void VolumeChangeData::SetProfiler(const Profiler& pProf)
{
	m_profiler = pProf;
}
