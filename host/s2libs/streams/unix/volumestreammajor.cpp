#include "volumestream.h"
#include "error.h"
#include "portablehelpers.h"


VolumeStream& VolumeStream::GetStreamData(VolumeChangeData& Data)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	if ( Data.IsInitialized() )
	{
		char* ppBufferArray ;
		unsigned int ulcbChangesInStream ;
		if( Data.m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
		{
			ppBufferArray = ( char* ) ( ( UDIRTY_BLOCK_V2* ) Data.m_DirtyBlock )->uHdr.Hdr.ppBufferArray ;
			ulcbChangesInStream = ( ( UDIRTY_BLOCK_V2* )Data.m_DirtyBlock )->uHdr.Hdr.ulcbChangesInStream ;
		}
		else
		{
	    ppBufferArray = ( char* ) ( ( UDIRTY_BLOCK* ) Data.m_DirtyBlock )->uHdr.Hdr.ppBufferArray ;
	    ulcbChangesInStream = ( ( UDIRTY_BLOCK* )Data.m_DirtyBlock )->uHdr.Hdr.ulcbChangesInStream ;
		}
		Data.SetData(ppBufferArray, ulcbChangesInStream);
		Data.m_uliCurrentChange = ulcbChangesInStream;
		++Data;
		SetState(STRM_GOOD);
	}
	else
	{
		SetState(STRM_GOOD);
		Data.SetDataLength(0);
		m_uliTotalDataProcessed = 0;

		DebugPrintf(SV_LOG_ERROR, "FAILED to read data. Change Data is not initialized.@LINE %d in FILE %s; \n", LINE_NO, FILE_NAME);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return *this;
}

