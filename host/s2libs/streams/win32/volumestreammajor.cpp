/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volumestream_port.cpp
 *
 * Description: 
 */
 
#include <string>
#include <iostream>
#include <cassert>

#include "error.h"
#include "portablehelpers.h"

#include "entity.h"
#include "genericdata.h"
#include "genericfile.h"

#include "genericstream.h"
#include "inputoutputstream.h"
#include "genericdevicestream.h"
#include "filestream.h"
#include "volume.h"
#include "volumechangedata.h"

#include "volumestream.h"
#include "ace/os_include/sys/os_types.h"


/*
 * FUNCTION NAME :  GetStreamData   
 *
 * DESCRIPTION :    Reads data from the dirty block while in data stream mode
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if read succeeded else SV_FAILURE
 *
 */
 
VolumeStream& VolumeStream::GetStreamData(VolumeChangeData& Data)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	if ( Data.IsInitialized() )
	{
		Data.SetDataLength(0);
		void** ppBufferArray ;
                SV_ULONG ulicbLength ; 
		SV_ULONG ulBufferSize ;
		if( Data.m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
		{
		    UDIRTY_BLOCK_V2* DirtyBlock = ( ( UDIRTY_BLOCK_V2* ) Data.m_DirtyBlock ) ;
  		    ppBufferArray = DirtyBlock->uHdr.Hdr.ppBufferArray ;
		    ulicbLength = DirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
		    ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
		}
		else
		{
		    UDIRTY_BLOCK* DirtyBlock = ( ( UDIRTY_BLOCK* ) Data.m_DirtyBlock ) ;
  		    ppBufferArray = DirtyBlock->uHdr.Hdr.ppBufferArray ;
		    ulicbLength = DirtyBlock->uHdr.Hdr.ulcbChangesInStream ;
		    ulBufferSize = DirtyBlock->uHdr.Hdr.ulBufferSize ;
		}
        unsigned  usBufferIndex = Data.GetCurrentChangeIndex();
		//We are not relying on Data.m_DirtyBlock.uHdr.Hdr.usNumberOfBuffers as it returns invalid numbers some times.
		SV_INT iNoOfBuffers = Data.GetNumberOfDataStreamBuffers();
        
		//assert(strncmp((char*) ppBufferArray[0],"SVD1",4) == 0);

		if(usBufferIndex < (unsigned)(iNoOfBuffers-1))
		{
            Data.SetData((char*) ppBufferArray[usBufferIndex],ulBufferSize);

			/* We process here all buffers except last*/
			if(Good())
			{
				SetState(STRM_MORE_DATA);
			}
			else
			{
				SetState(STRM_BAD);
				Data.SetDataLength(0);
				DebugPrintf(SV_LOG_ERROR, "FAILED to read data.Stream Corrupted.@LINE %d in FILE %s; \n", LINE_NO, FILE_NAME);
				return *this;
			}		
		}
		else if ( usBufferIndex == (iNoOfBuffers-1))
		{
			/*
				We are with last buffer .
				1. Set no more data.
				2. Transaction complete.
				3. Data.m_bEmpty = false
			*/
            Data.SetData((char*)ppBufferArray[usBufferIndex],(ulicbLength - (usBufferIndex * ulBufferSize)));
			
			if(Good())
			{
				SetState(STRM_GOOD);
			}
			else
			{
				SetState(STRM_BAD);
				Data.SetDataLength(0);
				DebugPrintf(SV_LOG_ERROR, "FAILED to read data.Stream Corrupted.@LINE %d in FILE %s; \n",LINE_NO, FILE_NAME);
				return *this;
			}
		}
        ++Data;
 	}
	else
	{
		SetState(STRM_GOOD);
        Data.SetDataLength(0);
		DebugPrintf(SV_LOG_ERROR,"FAILED to read data. Change Data is not initialized.@LINE %d in FILE %s; \n",LINE_NO,FILE_NAME);
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return *this;
}
