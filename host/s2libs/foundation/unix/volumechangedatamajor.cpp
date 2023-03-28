/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volumechangedata_port.cpp
 *
 * Description: Linux specific code for volumechangedata.
 */

#include <string>
#include <iostream>
#include <cstdlib>

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
#include "inmsafecapis.h"

/*
 * FUNCTION NAME : GetCurrentChangeOffset
 *
 * DESCRIPTION : gets the offset of the current change
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
const   SV_ULONGLONG VolumeChangeData::GetCurrentChangeOffset() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SV_ULONGLONG currentChangeOffset ;
	unsigned int currentChangeLength ;
	
	if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
	{
	    currentChangeOffset = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->ChangeOffsetArray[m_uliCurrentChange] ;
	    currentChangeLength = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->ChangeLengthArray[m_uliCurrentChange] ;
	}
	else
	{
	    currentChangeOffset = ((UDIRTY_BLOCK*)m_DirtyBlock)->ChangeOffsetArray[m_uliCurrentChange] ;
	    currentChangeLength = ((UDIRTY_BLOCK*)m_DirtyBlock)->ChangeLengthArray[m_uliCurrentChange] ;
	}
	DebugPrintf(SV_LOG_DEBUG, 
		"\n\tCurrent Change: %ld \n\tCurrent Change Offset: %lld\n\tCurrent Change Length: %u\n", 
		m_uliCurrentChange,
		currentChangeOffset,
		currentChangeLength ) ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    if ( Good() )	
    {
 		return currentChangeOffset ;
    }
    return 0;        
}

/*
 * FUNCTION NAME : GetChangesInBytes
 *
 * DESCRIPTION : returns the bytes of in the dirty block
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
unsigned long int VolumeChangeData::GetChangesInBytes()
{
    if ( Good() )
    {
        unsigned long int retVal ;
        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
        {
            retVal = ( ( UDIRTY_BLOCK_V2*) m_DirtyBlock )->uHdr.Hdr.ulicbChanges;
        }
        else
        {
            retVal = ( ( UDIRTY_BLOCK*) m_DirtyBlock )->uHdr.Hdr.ulicbChanges;
        }	
        return retVal;
    }
    return 0;        
}

/*
 * FUNCTION NAME : BeginTransaction
 *
 * DESCRIPTION : get the dirtyblock from the driver
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES : issues the ioctl on the device object and not on the volume
 *
 * return value : 
 *
 */
DB_DATA_STATUS VolumeChangeData::BeginTransaction(DeviceStream& stream, const FilterDrvVol_t &sourceName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DB_DATA_STATUS Status;
    size_t dirtyBlkSize ;
    
    m_bCheckDB = false;
    memset(m_DirtyBlock, 0, m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ? sizeof( UDIRTY_BLOCK_V2 ) : sizeof( UDIRTY_BLOCK ) );
    inm_u32_t ulcbChangesInStream = 0 ;
    Reset();
    int ioctlStatus = SV_FAILURE ;
    do
    {
        if( AllocateBufferArray(ulcbChangesInStream) != SV_SUCCESS )
        {        
            DebugPrintf(SV_LOG_FATAL, "AllocateBufferArray Failed \n") ;
            Status = DATA_ERROR;
            return Status ;
        }
        else
        {
            if( GetBufferArray() )
            {
                if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
                {
                    UDIRTY_BLOCK_V2* puDirtyBlockV2 = (UDIRTY_BLOCK_V2*) m_DirtyBlock ;
                    puDirtyBlockV2->uHdr.Hdr.ppBufferArray = GetBufferArray() ;
                    puDirtyBlockV2->uHdr.Hdr.ulcbChangesInStream = getBufferSize() ;
                    DebugPrintf(SV_LOG_DEBUG, "UDIRTY_BLOCK_V2 size %ld\n", getBufferSize()) ;
                }
                else
                {
                    UDIRTY_BLOCK* puDirtyBlock = (UDIRTY_BLOCK*)m_DirtyBlock ;
                    puDirtyBlock->uHdr.Hdr.ppBufferArray = GetBufferArray() ;
                    puDirtyBlock->uHdr.Hdr.ulcbChangesInStream = getBufferSize() ;
                    DebugPrintf(SV_LOG_DEBUG, "UDIRTY_BLOCK size %ld\n", getBufferSize()) ;
                }
            }
        }
        DebugPrintf(SV_LOG_DEBUG, "Performing IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS ioctl\n") ;
        ioctlStatus = stream.IoControl(IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,m_DirtyBlock) ;
        if ( SV_SUCCESS !=  ioctlStatus )
        {
            m_bGood = false;
            Status = DATA_ERROR;
            DebugPrintf(SV_LOG_DEBUG, "Error code from IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS ioctl is %d\n", ioctlStatus) ;
            if (ERR_BUSY == stream.GetErrorCode())
            {
                Status = DATA_NOTAVAILABLE;
                DebugPrintf(SV_LOG_WARNING, "Involflt driver has no data to be drained\n");
                break ;
            }
            else if (ERR_AGAIN == stream.GetErrorCode())
            {
                Status = DATA_RETRY;
                DebugPrintf(SV_LOG_DEBUG, "Filter driver has asked for get db retry\n");
                break;
            }
            if( GetBufferArray() && ERR_INVALID_PARAMETER == stream.GetErrorCode() )
            {                
                if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
                {
                    UDIRTY_BLOCK_V2* puDirtyBlockV2 = (UDIRTY_BLOCK_V2*) m_DirtyBlock ;
                    ulcbChangesInStream = puDirtyBlockV2->uHdr.Hdr.ulcbChangesInStream ;
                }
                else
                {
                    UDIRTY_BLOCK* puDirtyBlock = (UDIRTY_BLOCK*)m_DirtyBlock ;
                    ulcbChangesInStream = puDirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
                }
                if( ulcbChangesInStream <= getBufferSize() )
                {                    
                    DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS ioctl. The Supplied size %ld and returned size %ld\n", getBufferSize(), ulcbChangesInStream) ;
                    break ;
                }                
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s : IOCTL to get Dirty Block Transactions Failed. Error %d.\n",
                    FUNCTION_NAME,
                    stream.GetErrorCode());
                break ;
            }
        }
    } while( ioctlStatus != SV_SUCCESS ) ;
    
    if( ioctlStatus == SV_SUCCESS )
    {
		m_bGood = true;
		unsigned long long uliTransactionID ;
		unsigned long long ulicbChanges ;
		unsigned int cChanges ;
		unsigned int ulBufferSize ;
		unsigned int ulcbBufferArraySize ;
		unsigned int usNumberOfBuffers ;
		unsigned int ulFlags ;

		unsigned long long timeStampOfFirstChange ;
		unsigned long long timeStampOfLastChange ;
		if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
		{
			UDIRTY_BLOCK_V2* DirtyBlock = ( UDIRTY_BLOCK_V2*) m_DirtyBlock ;
			uliTransactionID = DirtyBlock->uHdr.Hdr.uliTransactionID ;
				ulicbChanges = DirtyBlock->uHdr.Hdr.ulicbChanges ;
			cChanges = DirtyBlock->uHdr.Hdr.cChanges ;
				ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
				usNumberOfBuffers = DirtyBlock->uHdr.Hdr.usNumberOfBuffers ;
				ulcbBufferArraySize = DirtyBlock->uHdr.Hdr.ulcbBufferArraySize ;
			ulFlags = DirtyBlock->uHdr.Hdr.ulFlags ; 
            if (!( ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE))
            {
			    timeStampOfFirstChange = DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 ;
			    timeStampOfLastChange = DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
                //TODO: Asserts to be removed from every where
                assert( timeStampOfLastChange >= timeStampOfFirstChange );
                DebugPrintf(SV_LOG_DEBUG, "Tag Data:\n\tTagTimeStampOfFirstChange: %lld\n\tTagTimeStampofLastChange: %lld\n",
                            timeStampOfFirstChange, timeStampOfLastChange );
            }
		}
		else
		{
			UDIRTY_BLOCK* DirtyBlock = ( UDIRTY_BLOCK*) m_DirtyBlock ;
			uliTransactionID = DirtyBlock->uHdr.Hdr.uliTransactionID ;
				ulicbChanges = DirtyBlock->uHdr.Hdr.ulicbChanges ;
			cChanges = DirtyBlock->uHdr.Hdr.cChanges ;
				ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
				usNumberOfBuffers = DirtyBlock->uHdr.Hdr.usNumberOfBuffers ;
				ulcbBufferArraySize = DirtyBlock->uHdr.Hdr.ulcbBufferArraySize ;
			ulFlags = DirtyBlock->uHdr.Hdr.ulFlags ; 
            if (!( ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE))
            {
			    timeStampOfFirstChange = DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 ;
			    timeStampOfLastChange = DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
                //TODO: Asserts to be removed from every where
                assert( timeStampOfLastChange >= timeStampOfFirstChange );
                DebugPrintf(SV_LOG_DEBUG, "Tag Data:\n\tTagTimeStampOfFirstChange: %lld\n\tTagTimeStampofLastChange: %lld\n",
                            timeStampOfFirstChange, timeStampOfLastChange );
            }
		}

        if (m_pDeviceFilterFeatures->IsDiffOrderSupported())
        {
            Status = (ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE)?DATA_0:DATA_AVAILABLE;
        }
        else
        {
            Status = uliTransactionID?DATA_AVAILABLE:DATA_0;
        }
        m_ResyncReqdStatus = (ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED)?
                             RESYNC_REQD:RESYNC_NOT_REQD;
        m_DataSource = DeduceDataSource();
        DebugPrintf(SV_LOG_DEBUG, "Issued a Get Dirty Block Trans successfully.\n\tTransaction ID: %lld\n\tcbChanges:%lld\n\tcChanges:%u\n\tulBufferSize:%u\n\tNumber of Buffers:%d\n\tBufferArraySize:%u\n\tResyncRequired: %s\n",
                uliTransactionID,
                ulicbChanges,
                cChanges,
                ulBufferSize,
                usNumberOfBuffers,
                ulcbBufferArraySize,
                (RESYNC_REQD == m_ResyncReqdStatus)?YES:NO);
    }
    stream.ResetErrorCode() ;
    if ((DATA_AVAILABLE == Status) || (DATA_0 == Status))
    {
        m_bCheckDB = (DB_COMMITED == m_DbState);
        m_DbState = DB_GOT;
    }
    else
    {
        m_DbState = DB_UNKNOWN;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return Status;
}

/*
 * FUNCTION NAME : Commit
 *
 * DESCRIPTION : Commits the dirty block 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES : commits even if its a tso
 *
 * return value : 
 *
 */
int VolumeChangeData::Commit(DeviceStream& stream, const FilterDrvVol_t &sourceName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;
	if ( IsInitialized() )
	{
	    COMMIT_TRANSACTION commitTrans;
	    memset(&(commitTrans), 0, sizeof( commitTrans) );
	    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
	    {
	        commitTrans.ulTransactionID = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
	    }
	    else
	    {
	        commitTrans.ulTransactionID = ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
	    }
	    if(IsResyncRequiredProcessed())
	    {
		commitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
	    }
		/* Here the volume guid member is filled with the name of the volume. */

    	    inm_memcpy_s( commitTrans.VolumeGUID,
				sizeof(commitTrans.VolumeGUID),
			    sourceName.c_str(),
				(strlen(sourceName.c_str()) < GUID_SIZE_IN_CHARS ? strlen(sourceName.c_str()) : GUID_SIZE_IN_CHARS-1)
			  );

		
    	if ( (iStatus = stream.IoControl(IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS, &(commitTrans), sizeof(commitTrans))) == SV_SUCCESS)
		{
    	    DebugPrintf( SV_LOG_DEBUG,"SUCCESS: Committed dirty block. Device IOCTL success.\n");
			m_DbState =  DB_COMMITED;
		}
    	else
    	{
    	    iStatus = SV_FAILURE;
    	    DebugPrintf( SV_LOG_ERROR,"FAILED: Failed to commit dirty blocks. Device IOCTL error. %s\n",Error::Msg().c_str());
    	}
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
* FUNCTION NAME : CommitFailNotify
*
* DESCRIPTION : Notifies driver about a dirty block transfer failure
*
* INPUT PARAMETERS :
*
* OUTPUT PARAMETERS :
*
* NOTES : 
*
* return value : SV_SUCCESS on success
*                SV_FAILURE on error
*/
int VolumeChangeData::CommitFailNotify(DeviceStream& stream,
    const FilterDrvVol_t &sourceName,
    unsigned long long ullFlags,
    unsigned long long ullErrorCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int iStatus = SV_SUCCESS;
    if (IsInitialized())
    {
        if (!m_pDeviceFilterFeatures->IsCxEventsSupported())
        {
            return iStatus;
        }

        unsigned long long ullTransactionID;
        if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            ullTransactionID = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
        }
        else
        {
            ullTransactionID = ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
        }

        COMMIT_DB_FAILURE_STATS commitFailureStats;
        memset(&(commitFailureStats), '\000', sizeof(commitFailureStats));
        commitFailureStats.ulTransactionID = ullTransactionID;
        commitFailureStats.ullFlags = ullFlags;
        commitFailureStats.ullErrorCode = ullErrorCode;

        inm_memcpy_s(commitFailureStats.DeviceID.volume_guid, NELEMS(commitFailureStats.DeviceID.volume_guid), sourceName.c_str(), sourceName.length());

        if ((iStatus = stream.IoControl(IOCTL_INMAGE_COMMITDB_FAIL_TRANS,
            (void*)&(commitFailureStats),
            sizeof(commitFailureStats))) == SV_SUCCESS)
        {
            DebugPrintf(SV_LOG_DEBUG, "SUCCESS: Notified driver about transfer failure. Device IOCTL success.\n");
        }
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR,
                "FAILED: Failed to notify driver about transfer failure. Device IOCTL error. %s\n",
                Error::Msg().c_str());
        }
    }
    else
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,
            "FAILED: Failed to notify about transfer failures. Device not initialized.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return iStatus;
}
/*
 * FUNCTION NAME : GetFileName
 *
 * DESCRIPTION : returns the file name if the mode is data file mode
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
const std::string  VolumeChangeData::GetFileName() const
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(!Good() || SOURCE_FILE !=GetDataSource())
    {
        assert("VolumeChangeData::GetFileName()..wrong call");
        DebugPrintf(SV_LOG_FATAL,"VolumeChangeData::GetFileName(): source of data is not data file \n");
    }

    char fileName [UDIRTY_BLOCK_MAX_FILE_NAME];
    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
    {
        inm_strcpy_s(fileName,ARRAYSIZE(fileName), (const char*) ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uTagList.DataFile.FileName);
    }
    else
    {
        inm_strcpy_s(fileName,ARRAYSIZE(fileName), (const char*) ((UDIRTY_BLOCK*)m_DirtyBlock)->uTagList.DataFile.FileName);
    }
    DebugPrintf(SV_LOG_DEBUG,"INFO: Source of Data is FILE - Filename = %s.\n",fileName);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return fileName;
}

/*
 * FUNCTION NAME : GetServiceRequestDataSize
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
unsigned long VolumeChangeData::GetServiceRequestDataSize()
{
    unsigned long RequestDataSize = 0;
    RequestDataSize = DEFAULT_SERVICE_REQUEST_DATA_SIZE;            
    return( RequestDataSize );
}

/*
 * FUNCTION NAME : GetTotalChangesPending
 *
 * DESCRIPTION : returns total bytes pending
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
const SV_LONGLONG VolumeChangeData::GetTotalChangesPending() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	if ( Good() )
	{
	    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
	    {
    	        return ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.ulicbTotalChangesPending;
	    }
	    else
	    {
    	        return ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.ulicbTotalChangesPending;
	    }
	}
    return 0;        
}

/*
 * FUNCTION NAME : GetOutOfSyncTimeStamp
 *
 * DESCRIPTION : returns out of sync timestamp
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
 const SV_LONGLONG VolumeChangeData::GetOutOfSyncTimeStamp() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	if ( Good() )
	{
	    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
	    {
    	        return ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.liOutOfSyncTimeStamp;
	    }
	    else
	    {
    	        return ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.liOutOfSyncTimeStamp;
	    }
	}

    return 0;
}

/*
 * FUNCTION NAME : ~VolumeChangeData
 *
 * DESCRIPTION : destructor
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
VolumeChangeData::~VolumeChangeData()
{
    if( m_ppBufferArray )
	{
		free(m_ppBufferArray) ;
	}
	m_ppBufferArray = NULL ;
	m_ulcbChangesInStream = 0 ;
	m_pszData = NULL;
    FreeReadBuffer();
    Reset();
}


/*
 * FUNCTION NAME : IsTagsOnly
 *
 * DESCRIPTION : returns true if the dirty block contains only tags and no data
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
const bool VolumeChangeData::IsTagsOnly() const
{
    if ( Good() && ( 0 == GetTotalChanges() ) && (!IsTimeStampsOnly()))
	{
        return true;
	}

    return false;        
}

/*
 * FUNCTION NAME : IsTimeStampsOnly
 *
 * DESCRIPTION : returns true if the dirty block is tso
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
const bool VolumeChangeData::IsTimeStampsOnly() const
{
    bool btimestampsonly = false;
    unsigned int ulFlags ;

    if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
    {
        UDIRTY_BLOCK_V2* DirtyBlock = ( UDIRTY_BLOCK_V2*) m_DirtyBlock ;
        ulFlags = DirtyBlock->uHdr.Hdr.ulFlags ;
    }
    else
    {
        UDIRTY_BLOCK* DirtyBlock = ( UDIRTY_BLOCK*) m_DirtyBlock ;
        ulFlags = DirtyBlock->uHdr.Hdr.ulFlags ;
    }

    if ( Good() && ( 0 == GetTotalChanges() ))
    {
        if (m_pDeviceFilterFeatures->IsDiffOrderSupported())
        {
            btimestampsonly = (ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE)?true:false;
        }
        else
        {
            btimestampsonly = (GetTransactionID())?false:true;
        }
    }

    return btimestampsonly;
}

/*
 * FUNCTION NAME : GetTransactionID
 *
 * DESCRIPTION : returns the transaction id of the dirty block
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
const SV_LONGLONG VolumeChangeData::GetTransactionID() const
{
    if ( Good() )
    {
        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
	{
	    return ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
	}
	else
	{
	    return ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
	}
    }

    return -1;        
}


SV_INT VolumeChangeData::GetNumberOfDataStreamBuffers(void)
{
    return 1;
}


bool VolumeChangeData::AllocateReadBuffer(const size_t &readsize)
{
    m_pData = (char*) valloc(readsize);

    return m_pData;
}


void VolumeChangeData::FreeReadBuffer(void)
{
    if (m_pData)
    {
        free(m_pData);
        m_pData = 0;
    }
}


/*
 * FUNCTION NAME : GetCurrentChanges
 *
 * DESCRIPTION : returns the changes in bytes held by the dirty block
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
const SV_LONGLONG VolumeChangeData::GetCurrentChanges() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SV_LONGLONG v = 0;

	if ( Good() )
	{
		if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ) 
		{
			v = ( ( UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.ulicbChanges;    
		}
		else
		{
			v = ( ( UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.ulicbChanges;    
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s with value " LLSPEC "\n",FUNCTION_NAME, v);
    return v;        
}

