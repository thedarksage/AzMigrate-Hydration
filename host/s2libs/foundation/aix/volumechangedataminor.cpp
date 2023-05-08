#include "inm_types.h"
#include "ioctl_codes.h"
#include "volumechangedata.h"
#include "portable.h"
int VolumeChangeData::AllocateBufferArray(inm_u32_t size)
{
    int iStatus = SV_FAILURE ;
    if( size == 0 )
    {
        size = 16 * 1024 * 1024  ;
    }
    DebugPrintf(SV_LOG_DEBUG, "Previous buffer size is  %ld\n", m_ulcbChangesInStream) ;
    DebugPrintf(SV_LOG_DEBUG, "Required buffer size is  %ld\n", size) ;
    if( size > m_ulcbChangesInStream  )
    {
        if( m_ppBufferArray != NULL )
        {
            free(m_ppBufferArray) ;
            m_ppBufferArray = NULL ;
        }
        m_ulcbChangesInStream = 0 ;
        m_ppBufferArray = (void **) malloc( size ) ;
        if( m_ppBufferArray == NULL )
        {
            DebugPrintf(SV_LOG_FATAL, "Failed to allocate memory for BufferArray\n") ;
        }
        else
        {
            iStatus = SV_SUCCESS ;
            m_ulcbChangesInStream = size ;
        }
    }
    else
    {
     iStatus = SV_SUCCESS ;
    }
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
