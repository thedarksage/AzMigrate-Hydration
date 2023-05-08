/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volumechangedata_port.cpp
 *
 * Description: Windows specific code for volumechangedata.
 */

#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>

#include <string>
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
const SV_ULONGLONG VolumeChangeData::GetCurrentChangeOffset() const
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    if ( Good() )	
	{
        if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
		{
			return ( ( UDIRTY_BLOCK_V2*)m_DirtyBlock)->ChangeOffsetArray[m_uliCurrentChange];
		}
		else
		{
			return ( ( UDIRTY_BLOCK*)m_DirtyBlock)->ChangeOffsetArray[m_uliCurrentChange];
		}
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
		if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
		{
        	return ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.ulicbChanges.LowPart;
		}
		else
		{
        	return ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.ulicbChanges.LowPart;
		}
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
	size_t dirtyBlkSize = m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ? sizeof( UDIRTY_BLOCK_V2 ) : sizeof( UDIRTY_BLOCK ) ;
	memset(m_DirtyBlock, 0, dirtyBlkSize );
    m_bCheckDB = false;
    Reset();

	if (SV_SUCCESS != stream.IoControl(IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS,
		static_cast<void *>(const_cast<wchar_t *>(sourceName.c_str())),
		sourceName.length() * sizeof(wchar_t),
		m_DirtyBlock,
		(long)dirtyBlkSize))
	{
		m_bGood = false;
		Status = DATA_ERROR;
		if (stream.GetErrorCode() == ERR_BUSY)
		{
			Status = DATA_NOTAVAILABLE;
			DebugPrintf(SV_LOG_WARNING, "Involflt driver has no data to be drained\n");
		}
        else if (stream.GetErrorCode() == ERR_AGAIN)
        {
            Status = DATA_RETRY;
			DebugPrintf(SV_LOG_DEBUG, "Filter driver has asked for get db retry\n");
        }
		else
		{
            DebugPrintf(SV_LOG_ERROR,
                "%s : IOCTL to get Dirty Block Transactions Failed. Error %d.\n",
                FUNCTION_NAME, 
                stream.GetErrorCode());
		}
	}
	else
	{
		m_bGood = true;
		ULARGE_INTEGER uliTransactionID;
		/*
		* For windows, ulFlags is ULONG, but on unix it is 32 bit number
		*/
		ULONG ulFlags;

		if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
		{
			uliTransactionID = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
			ulFlags = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.ulFlags;
		}
		else
		{
			uliTransactionID = ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
			ulFlags = ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.ulFlags;
		}
		m_ResyncReqdStatus = (ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED) ?
		RESYNC_REQD : RESYNC_NOT_REQD;
		if (m_pDeviceFilterFeatures->IsDiffOrderSupported())
		{
			Status = (ulFlags & UDIRTY_BLOCK_FLAG_TSO_FILE) ? DATA_0 : DATA_AVAILABLE;
		}
		else
		{
			Status = uliTransactionID.QuadPart ? DATA_AVAILABLE : DATA_0;
		}
		m_DataSource = DeduceDataSource();
		DebugPrintf(SV_LOG_DEBUG, "For Transaction ID: " ULLSPEC
			"Resync required: %s\n", uliTransactionID.QuadPart,
			(RESYNC_REQD == m_ResyncReqdStatus) ? YES : NO);
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
		ULARGE_INTEGER uliTransactionID ;
		if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported() )
		{
			uliTransactionID = ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock)->uHdr.Hdr.uliTransactionID ;
		}
		else
		{
	  		uliTransactionID = ( ( UDIRTY_BLOCK* ) m_DirtyBlock)->uHdr.Hdr.uliTransactionID ;
		}
        
        COMMIT_TRANSACTION commitTrans;
	    memset(&(commitTrans), '\000',sizeof( commitTrans) );
		commitTrans.ulTransactionID = uliTransactionID;
		if(IsResyncRequiredProcessed())
		{
			commitTrans.ulFlags |= COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG;
		}
        inm_wmemcpy_s(commitTrans.VolumeGUID, NELEMS(commitTrans.VolumeGUID), sourceName.c_str(), sourceName.length());
		
        if( (iStatus = stream.IoControl(IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS, 
						(LPVOID)&(commitTrans), 
						sizeof(commitTrans), 
						NULL, 
						0)) == SV_SUCCESS )
		{
            DebugPrintf( SV_LOG_DEBUG,"SUCCESS: Committed dirty block. Device IOCTL success.\n");
			m_DbState =  DB_COMMITED;
		}
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf( SV_LOG_ERROR,
					"FAILED: Failed to commit dirty blocks. Device IOCTL error. %s\n",
					Error::Msg().c_str());
        }
	}
    else
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,
            "FAILED: Failed to commit dirty blocks. Device not initialized.\n");
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

        ULARGE_INTEGER uliTransactionID;
        if (m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
        {
            uliTransactionID = ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
        }
        else
        {
            uliTransactionID = ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID;
        }

        COMMIT_DB_FAILURE_STATS commitFailureStats;
        memset(&(commitFailureStats), '\000', sizeof(commitFailureStats));
        commitFailureStats.ulTransactionID = uliTransactionID;
        commitFailureStats.ullFlags = ullFlags;
        commitFailureStats.ullErrorCode = ullErrorCode;

        inm_wmemcpy_s(commitFailureStats.DeviceID, NELEMS(commitFailureStats.DeviceID), sourceName.c_str(), sourceName.length());

        if ((iStatus = stream.IoControl(IOCTL_INMAGE_COMMITDB_FAIL_TRANS,
            (LPVOID)&(commitFailureStats),
            sizeof(commitFailureStats),
            NULL,
            0)) == SV_SUCCESS)
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
    if( !Good() || (SOURCE_FILE !=GetDataSource()))
    {
	    /* PR # 5113 : START */
        DebugPrintf(SV_LOG_FATAL,"VolumeChangeData::GetFileName(): source of data is not data file ..wrong call\n");
		assert(false);
	    /* PR # 5113 : END */
        return "";
    }

    char fileName [UDIRTY_BLOCK_MAX_FILE_NAME];
    size_t countCopied = 0;
    PWCH wcsfilename = m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ? ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uTagList.DataFile.FileName :
        ((UDIRTY_BLOCK*)m_DirtyBlock)->uTagList.DataFile.FileName;
    wcstombs_s(&countCopied, fileName, ARRAYSIZE(fileName), wcsfilename, ARRAYSIZE(fileName)-1);
	int fileLength = m_pDeviceFilterFeatures->IsPerIOTimestampSupported() ? 
						( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock)->uTagList.DataFile.usLength :
                        ( ( UDIRTY_BLOCK* ) m_DirtyBlock)->uTagList.DataFile.usLength ;

    if (countCopied != (fileLength / sizeof(wchar_t)))
    {
        DebugPrintf(SV_LOG_WARNING,
				" conversion from wide string to multibyte string %s may not be proper\n",
				fileName);
    }

	/* PR # 5113 : START 
	 * NOTE : Below Code strictly assumes GetEnvironmentVariable(SystemRoot)
	 * does not put more than 260 (MAX_PATH) characters on windows */
	const char BKSLSH_SYSTEMROOT[] = "\\SystemRoot";

	if (0 == strnicmp(fileName, BKSLSH_SYSTEMROOT, strlen(BKSLSH_SYSTEMROOT)))
	{
		char tempName[UDIRTY_BLOCK_MAX_FILE_NAME];
		if (0 == GetEnvironmentVariable(BKSLSH_SYSTEMROOT + 1, tempName, 
								        sizeof tempName))
		{
			/* GetEnvironmentVariable failed */
            DebugPrintf(SV_LOG_ERROR,
					"FAILED: GetEnvironmentVariable(). @LINE %d in FILE %s.\n",
					LINE_NO,FILE_NAME);
			return "";
		     	
		}
		inm_strcat_s(tempName,ARRAYSIZE(tempName), fileName + strlen(BKSLSH_SYSTEMROOT));
		inm_strcpy_s(fileName,ARRAYSIZE(fileName), tempName);
	}
	/* PR # 5113 : END */

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
    CRegKey cregkey;
    DWORD dwResult = ERROR_SUCCESS;
    dwResult = cregkey.Open( HKEY_LOCAL_MACHINE,
        SV_INVOLFLT_PARAMETER );
    if( ERROR_SUCCESS != dwResult )
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED cregkey.Open()...\n");
    }
    else 
    {
        dwResult = cregkey.QueryDWORDValue( SV_SERVICE_REQUEST_DATA_SIZE,
            RequestDataSize );
    }
    
    if( ( ERROR_SUCCESS != dwResult ) || ( 0 == RequestDataSize ) )
    {
        RequestDataSize = DEFAULT_SERVICE_REQUEST_DATA_SIZE;            
    }
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
			return ( ( UDIRTY_BLOCK_V2*) m_DirtyBlock)->uHdr.Hdr.ulicbTotalChangesPending.QuadPart;
		}
		else
		{
			return ( ( UDIRTY_BLOCK*) m_DirtyBlock)->uHdr.Hdr.ulicbTotalChangesPending.QuadPart;
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
        	return (( UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.liOutOfSyncTimeStamp.QuadPart;
		}
		else
		{
        	return (( UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.liOutOfSyncTimeStamp.QuadPart;
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
    ULONG ulFlags ;

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
			return ((UDIRTY_BLOCK_V2*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID.QuadPart;
		}
		else
		{
			return ((UDIRTY_BLOCK*)m_DirtyBlock)->uHdr.Hdr.uliTransactionID.QuadPart;
		}
	}
    return -1;        
}


int VolumeChangeData::AllocateBufferArray(inm_u32_t size)
{
    int iStatus = SV_SUCCESS ;
    return iStatus ;
}
void** VolumeChangeData::GetBufferArray()
{
    return m_ppBufferArray ;
}
inm_u32_t VolumeChangeData::getBufferSize()
{
    return m_ulcbChangesInStream ;
}


SV_INT VolumeChangeData::GetNumberOfDataStreamBuffers(void)
{
    SV_ULONG ulicbLength ; 
    SV_ULONG ulBufferSize ;

    if( m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
    {
        UDIRTY_BLOCK_V2* DirtyBlock = ( ( UDIRTY_BLOCK_V2* ) m_DirtyBlock ) ;
        ulicbLength = DirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
        ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
    }
    else
    {
        UDIRTY_BLOCK* DirtyBlock = ( ( UDIRTY_BLOCK* ) m_DirtyBlock ) ;
        ulicbLength = DirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
        ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
    }

    SV_INT iNoOfBuffers = (SV_INT)(ulicbLength / ulBufferSize);

    if (0 != (ulicbLength % ulBufferSize))
    {
        iNoOfBuffers++;
    }
    return iNoOfBuffers;
}


void VolumeChangeData::FreeReadBuffer(void)
{
    if (m_pData)
    {
        delete [] m_pData;
        m_pData = 0;
    }
}


bool VolumeChangeData::AllocateReadBuffer(const size_t &readsize)
{
    m_pData = new (std::nothrow) char[readsize];

    return m_pData;
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
			v = ( ( UDIRTY_BLOCK_V2* )m_DirtyBlock )->uHdr.Hdr.ulicbChanges.QuadPart;    
		}
		else
		{
			v = ( ( UDIRTY_BLOCK* )m_DirtyBlock )->uHdr.Hdr.ulicbChanges.QuadPart;    
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s with value " LLSPEC "\n",FUNCTION_NAME, v);
    return v;        
}

