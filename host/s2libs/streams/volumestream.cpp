/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volumestream.cpp
 *
 * Description: 
 */
#include <string>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "error.h"
#include "portable.h"

#include "entity.h"
#include "genericdata.h"
#include "genericfile.h"

#include "genericstream.h"
#include "inputoutputstream.h"
#include "genericdevicestream.h"
#include "filestream.h"
#include "volumechangedata.h"

#include "portablehelpers.h"

#include "volumestream.h"
#include "ace/os_include/sys/os_types.h"

#ifndef MIN_VALUE
#define MIN_VALUE(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifdef SV_WINDOWS
	#include <winnt.h>
#endif


/*
 * FUNCTION NAME :  VolumeStream
 *  
 * DESCRIPTION :    Constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :          The stream is created with supplied handle. It also initializes the data processed to zero and sets seek to true
 *
 * return value : 
 *
 */
 
VolumeStream::VolumeStream(const ACE_HANDLE &h)
: FileStream(h),
  m_uliTotalDataProcessed(0),
  m_bSeek(true)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
 * FUNCTION NAME :  ~VolumeStream
 *
 * DESCRIPTION :    Destructor. Closes open handle to the volume
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
 
VolumeStream::~VolumeStream()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	Close();

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
* FUNCTION NAME :  Open
*
* DESCRIPTION :    Not implemented because the stream already has handle which is externally provided and opened.
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
int VolumeStream::Open(const STREAM_MODE)
{
	throw std::runtime_error("Not supported. VolumeStream supports only externally supplied handle.");
}


/*
 * FUNCTION NAME :  Clear
 *
 * DESCRIPTION :    Resets the internal counters. Sets dataprocessed to zero and file seek to true.
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
 
void VolumeStream::Clear()
{
    m_uliTotalDataProcessed = 0;
    m_bSeek = true;
    FileStream::Clear();

}

/*
 * FUNCTION NAME :  Close
 *
 * DESCRIPTION :    Clears the volumestream and invokes base class Close   
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
 
int VolumeStream::Close()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    
    this->Clear();
    FileStream::Close();

    m_bOpen = false;		

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return SV_SUCCESS;
}


/*
 * FUNCTION NAME :  operator >> 
 *
 * DESCRIPTION :    Output operator. Determines the source of the data and processes the dirty block
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
 
VolumeStream& VolumeStream::operator >> (VolumeChangeData& Data)
{
    DATA_SOURCE source = Data.GetDataSource();
    void** ppBufferArray ;
    if( Data.m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
    {
    	ppBufferArray = ( ( UDIRTY_BLOCK_V2*) Data.GetDirtyBlock() )->uHdr.Hdr.ppBufferArray ;
    }
    else
    {
    	ppBufferArray = ( ( UDIRTY_BLOCK*) Data.GetDirtyBlock() )->uHdr.Hdr.ppBufferArray ;
    } 
    
    switch(source)
    {
        case SOURCE_BITMAP:
            DebugPrintf(SV_LOG_DEBUG,"Source of Data is BITMAP.@LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            //assert(ppBufferArray == NULL);
            return GetMetaData(Data);
        case SOURCE_META_DATA:
            DebugPrintf(SV_LOG_DEBUG,"Source of Data is META DATA.@LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            //assert(ppBufferArray == NULL);
            return GetMetaData(Data);
        break;
        case SOURCE_DATA:
            assert(ppBufferArray != NULL);
            DebugPrintf(SV_LOG_DEBUG,"Source of Data is FULL WRITE ORDERED DATA.@LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            return GetData(Data);
        break;
		case SOURCE_STREAM:
            assert(ppBufferArray != NULL);
            DebugPrintf(SV_LOG_DEBUG,"Source of Data is DATA STREAM.@LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            return GetStreamData(Data);
        case SOURCE_FILE:
            DebugPrintf(SV_LOG_FATAL,"FATAL: Source of data is data file: @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            assert("Fatal Error: Source of data is data file: This should not come here");
            break;
        default:
            DebugPrintf(SV_LOG_FATAL,"FATAL: Unknown source of Data @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
            assert("Fatal Error: Unknown source of Data");
        break;
    }

	return *this;
}

VolumeStream& VolumeStream::operator <<(VolumeChangeData& Data)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"To be IMPLEMENTED\n");

    assert(!"Not Implemented");
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return *this;
}


/*
 * FUNCTION NAME :  GetMetaData   
 *
 * DESCRIPTION :    Reads data from the volume in meta data mode
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
 
VolumeStream& VolumeStream::GetMetaData(VolumeChangeData& Data)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    long int uliActualBytesRead = 0;
    long int uliBytesToRead = 0;
    long int uliBufferLen = 0;
    const unsigned long int CHANGES = Data.GetTotalChanges();
	if ( Data.IsInitialized() )
	{
        if ( Data.GetCurrentChangeIndex() < CHANGES ) 
		{
			SV_LONGLONG Offset = (ACE_LOFF_T)Data.GetVolumeStartOffset();

            Offset += (ACE_LOFF_T)Data.GetCurrentChangeOffset();
            uliBufferLen = Data.GetCurrentChangeLength();
            
            if ( uliBufferLen > 0 )
            {
                uliBytesToRead =  MIN_VALUE((uliBufferLen - m_uliTotalDataProcessed), Data.m_bufLen);

                uliActualBytesRead = 0;
                if (m_bSeek)
                {
                    Seek(Offset,POS_BEGIN);
                }      

                if ( Good() )
                {
                    if (Data.GetProfiler().get() != NULL)
                        (Data.GetProfiler())->start();//measure disk read latency
                    uliActualBytesRead = Read(Data.m_pData,uliBytesToRead);
                    if (uliActualBytesRead > 0)
                    {
                        if (Data.GetProfiler().get() != NULL)
                            (Data.GetProfiler())->stop(uliActualBytesRead);
                    }
                    if ( uliActualBytesRead <= 0 )
                    {
                        if ( Fail()  || Bad())
                        {
                            Offset = (ACE_LOFF_T)0LL;

                            // Determine the current positionof the file.
                            SV_LONGLONG dwCurrentFilePosition = Seek(Offset,POS_CURRENT);

                            std::stringstream ss;
                            ss << "FAILED to read data at: Offset " << dwCurrentFilePosition
                               << ", Length requested: " << uliBytesToRead;
                            SetLastErrMsg(ss.str());
				            Data.SetDataLength(0);
                        }
			        }

			        if ((uliActualBytesRead > 0 ) && ( Good() ) )
                    {
                        Data.m_pszData = Data.m_pData;
                        m_uliTotalDataProcessed += uliActualBytesRead;
                        Data.SetDataLength(uliActualBytesRead);

                        if (m_uliTotalDataProcessed < uliBufferLen) 
                        {
                            if ( !Eof()  && !Bad() && !Fail()  && Good() )
                            {
                                SetState(STRM_MORE_DATA);
                                m_bSeek = false;
                            }                                
                        }
                        else if ( m_uliTotalDataProcessed == uliBufferLen )
                        {
                            SetState(STRM_GOOD);
                            m_bSeek = true;
                            m_uliTotalDataProcessed = 0;
                            ++Data;
                        }
                        else if ( m_uliTotalDataProcessed > uliBufferLen )
                        {
                            DebugPrintf(SV_LOG_FATAL,"FAILED: Read beyond the required length. @LINE %d in FILE %s \n",LINE_NO,FILE_NAME);

                            SetState(STRM_BAD);
                            Data.SetDataLength(0);
                            m_uliTotalDataProcessed = uliBufferLen;
                            m_bSeek = true;
                        }
                    }
                    else
                    {
                        SetState(STRM_BAD);
                        Data.SetDataLength(0);
                        m_uliTotalDataProcessed = 0;

                        DebugPrintf(SV_LOG_ERROR,"FAILED to read data. @LINE %d in FILE %s; \n",LINE_NO,FILE_NAME);
                    }
                }
                else
                {
                    SetState(STRM_BAD);
                    Data.SetDataLength(0);
                    m_uliTotalDataProcessed = 0;

                    DebugPrintf(SV_LOG_ERROR,"FAILED: Bad stream state. @LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING,"WARNING: Read length is ZERO. Skipping current disk change no. %lu of %lu. @LINE %d in FILE %s \n",Data.m_uliCurrentChange,CHANGES,LINE_NO,FILE_NAME);
                ++Data;
                SetState(STRM_GOOD);
                m_bSeek = true;
                Data.SetDataLength(0);
                m_uliTotalDataProcessed = 0;
            }
    	}
		else
		{
            Data.SetDataLength(0);
            m_bSeek = true;
            SetState(STRM_GOOD);
            m_uliTotalDataProcessed = 0;
		}
	}
	else
	{
        Data.SetDataLength(0);
        m_bSeek = true;
        SetState(STRM_GOOD);
        m_uliTotalDataProcessed = 0;
		DebugPrintf(SV_LOG_ERROR,"FAILED to read data. Change Data is not initialized.@LINE %d in FILE %s; \n",LINE_NO,FILE_NAME);
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return *this;
}


VolumeStream& VolumeStream::GetData(VolumeChangeData& Data)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED made No-Op %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return *this;
}
