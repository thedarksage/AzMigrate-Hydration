//Linux port of devicestream.cpp

#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>

#include "portablehelpers.h"

#include "error.h"
#include "entity.h"

#include "genericfile.h"
#include "device.h"

#include "genericfile.h"
#include "genericstream.h"
#include "filestream.h"
#include "genericdevicestream.h"
#include "devicestream.h"
#include "ace/OS.h"
#include "devicefilter.h"

int DeviceStream::Open(STREAM_MODE Mode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    
    std::stringstream ssmode;
    ssmode << std::hex << Mode;

    m_Mode = Mode;

    int iStatus = SV_SUCCESS;
    m_bOpen = false;
    if ( m_pGenericFile )
    {
        std::string sName = m_pGenericFile->GetName();

        if ( sName.empty() )
        {
            SetErrorCode(ERR_INVALID_PARAMETER);
            DebugPrintf(SV_LOG_ERROR, "Empty device name specified for device stream. LINE %d in FILE %s \n", LINE_NO, FILE_NAME);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            SetState(STRM_BAD);
            return SV_FAILURE;
        }
        
        unsigned long int uliAccess     = 0;
        unsigned long int uliShareMode  = 0;
        unsigned long int uliFlags      = 0;
        unsigned long int uliDisposition= 0;

        if (ModeOn(Mode_RW, m_Mode)) 
        {
            uliAccess = O_RDWR;
        } 
        else if (ModeOn(Mode_Read, m_Mode)) 
        {
            uliAccess = O_RDONLY;
        } 
        else if (ModeOn(Mode_Write, m_Mode)) 
        {
            uliAccess = O_WRONLY;
        } 
        else 
        {
            SetErrorCode(ERR_INVALID_PARAMETER);
            DebugPrintf(SV_LOG_ERROR,"Invalid Open mode: %s specified for device stream. LINE %d in FILE %s \n",ssmode.str().c_str(), LINE_NO,FILE_NAME);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            SetState(STRM_BAD);
            return SV_FAILURE;
        }   
        
        if ( ModeOn(Mode_ShareRW,m_Mode))
        {
            uliShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        }
        else if ( ModeOn(Mode_ShareRead,m_Mode) )
        {
            uliShareMode = FILE_SHARE_READ;
        }
        else if ( ModeOn(Mode_ShareWrite,m_Mode) )
        {
            uliShareMode = FILE_SHARE_WRITE;
        }
        else 
        {
            SetErrorCode(ERR_INVALID_PARAMETER);
            DebugPrintf(SV_LOG_ERROR, "Invalid Open mode: %s specified for device stream. LINE %d in FILE %s \n", ssmode.str().c_str(), LINE_NO, FILE_NAME);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            SetState(STRM_BAD);
            return SV_FAILURE;
        }

        if (ModeOn(Mode_Asynchronous, m_Mode)) 
        {
            uliFlags |= FILE_FLAG_OVERLAPPED;
        }

     
        if (ModeOn(Mode_Open, m_Mode)) 
        {
            uliDisposition = OPEN_EXISTING;
        } 
        else 
        {
            SetErrorCode(ERR_INVALID_PARAMETER);
            DebugPrintf(SV_LOG_ERROR, "Invalid Open mode: %s specified for device stream. LINE %d in FILE %s \n", ssmode.str().c_str(), LINE_NO, FILE_NAME);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            SetState(STRM_BAD);         
            return SV_FAILURE;
        }

        // check for special flags
        if (ModeOn(Mode_NoBuffer, m_Mode)) 
        {
            uliFlags |= FILE_FLAG_NO_BUFFERING;
        }
        if (ModeOn(Mode_WriteThrough, m_Mode)) 
        {
            uliFlags |= FILE_FLAG_WRITE_THROUGH;
        }

        uliAccess = (uliAccess | uliDisposition | uliFlags );

        // PR#10815: Long Path support
        m_pGenericFile->SetHandle( ACE_OS::open(getLongPathName(sName.c_str()).c_str(),uliAccess,uliShareMode) );
        
        if( ACE_INVALID_HANDLE != m_pGenericFile->GetHandle())
        {
            if (ModeOn(Mode_Asynchronous, m_Mode) )
            {
                    SetState(STRM_GOOD);
                    m_bOpen = true;
                    m_bSync = false;
                    iStatus = SV_SUCCESS;
            }
            else
            {
                m_bOpen = true;
                m_bSync = true;
                SetState(STRM_GOOD);
                iStatus = SV_SUCCESS;
            }
        }
        else
        {
            int systemErrorCode = Error::Code();
            int userErrorCode = Error::OSToUser(systemErrorCode);
            SetErrorCode(userErrorCode);
            SetState(STRM_BAD);
            m_bOpen = false;
            iStatus = SV_FAILURE;
            DebugPrintf( SV_LOG_ERROR,"FAILED: An error occurred while attempting to open device %s. %s\n",sName.c_str(),Error::Msg(systemErrorCode).c_str());
        }

    }
    else
    {
        SetErrorCode(SV_FAILURE);
        iStatus = SV_FAILURE;
        SetState(STRM_BAD);
        DebugPrintf(SV_LOG_ERROR, "Failed to create device object in device stream. LINE %d in FILE %s \n", LINE_NO, FILE_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::CancelIO()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;

    if ( IsOpen() && (NULL != m_pGenericFile ))
    {
        iStatus = SV_SUCCESS;
        SetState(STRM_GOOD);
        DebugPrintf(SV_LOG_WARNING,"Cancel IO not supported on non MS Windows platforms\n");
        
    }
    else
    {
        SetState(STRM_FAIL);
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"Invalid device specified\n");
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::CancelIOEx()
{
    return CancelIO();
}

int DeviceStream::IoControl(unsigned long int liControlCode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    
    if ( (!IsOpen()) || (ACE_INVALID_HANDLE == m_pGenericFile->GetHandle()) ) 
    {
        SetState(STRM_BAD);
        DebugPrintf(SV_LOG_ERROR, "Device stream is not open.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
        return SV_FAILURE;
    }

    ResetErrorCode();
    iStatus = ioctl(m_pGenericFile->GetHandle(), liControlCode);
    if (0 != iStatus)
    {
        int iErrorCode = Error::OSToUser(Error::Code());
        SetErrorCode(iErrorCode);
    }
    int UserErrorCode = GetErrorCode();
    if (0 == iStatus)
    {
        SetState(STRM_GOOD);
        iStatus = SV_SUCCESS;
        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: IOCTL executed successfully.\n");
    }
    else if (((IOCTL_INMAGE_WAIT_FOR_DB == liControlCode) || (IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY == liControlCode))
            && (ETIMEDOUT == UserErrorCode))
    {
        SetState(STRM_GOOD);
        iStatus = SV_SUCCESS;
        const char *pcode = (IOCTL_INMAGE_WAIT_FOR_DB == liControlCode) ? "IOCTL_INMAGE_WAIT_FOR_DB" : "IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY";
        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: %s timed out.\n", pcode);
    }
    else
    {
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while executing IOCTL. %s \n", Error::Msg(Error::UserToOS(UserErrorCode)).c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::IoControl(unsigned long int liControlCode,void* pInBuffer)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    if(NULL == pInBuffer)
    {
        //sanity check.
        DebugPrintf(SV_LOG_ERROR, "Invalid input buffer to IoControl.");
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
        return SV_FAILURE;
    }
    
    if ( (!IsOpen()) || (ACE_INVALID_HANDLE == m_pGenericFile->GetHandle()) ) 
    {
        SetState(STRM_BAD);
        DebugPrintf(SV_LOG_ERROR, "Device stream is not open.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
        return SV_FAILURE;
    }

    ResetErrorCode();
    iStatus = ioctl(m_pGenericFile->GetHandle(), liControlCode, pInBuffer);
    if (0 != iStatus)
    {
        int iErrorCode = Error::OSToUser(Error::Code());
        SetErrorCode(iErrorCode);
    }
    int UserErrorCode = GetErrorCode();
    if (0 == iStatus)
    {
        SetState(STRM_GOOD);
        iStatus = SV_SUCCESS;
        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: IOCTL executed successfully.\n");
    }
    else if (((IOCTL_INMAGE_WAIT_FOR_DB == liControlCode) || (IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY == liControlCode))
            && (ETIMEDOUT == UserErrorCode))
    {
        SetState(STRM_GOOD);
        iStatus = SV_SUCCESS;
        const char *pcode = (IOCTL_INMAGE_WAIT_FOR_DB == liControlCode) ? "IOCTL_INMAGE_WAIT_FOR_DB" : "IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY";
        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: %s timed out.\n", pcode);
    }
    else if (ERR_BUSY == UserErrorCode)
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_WARNING,"An error occurred while executing IOCTL. %s \n", Error::Msg(Error::UserToOS(UserErrorCode)).c_str());
    }
    else if (ERR_AGAIN == UserErrorCode)
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_DEBUG,"An error occurred while executing IOCTL. %s \n", Error::Msg(Error::UserToOS(UserErrorCode)).c_str());
    }
    else
    {
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while executing IOCTL. %s \n", Error::Msg(Error::UserToOS(UserErrorCode)).c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::IoControl(unsigned long int liControlCode,void* pInBuffer,long int liBufferLen)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    if(0 >= liBufferLen || NULL == pInBuffer) 
    {
        //sanity check.
        DebugPrintf(SV_LOG_ERROR, "Invalid input to IoControl.");
        return SV_FAILURE;
    }
    if ( (!IsOpen()) || (ACE_INVALID_HANDLE == m_pGenericFile->GetHandle()) ) 
    {
        SetState(STRM_BAD);
        DebugPrintf(SV_LOG_ERROR, "Device stream is not open.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
        return SV_FAILURE;
    }
    ResetErrorCode();
    iStatus = ioctl(m_pGenericFile->GetHandle(), liControlCode, pInBuffer,liBufferLen);
    if ( 0 == iStatus)
    {
        SetState(STRM_GOOD);
        iStatus = SV_SUCCESS;
        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: IOCTL executed successfully.\n");
    }
    else
    {
        int iErrorCode = Error::OSToUser(Error::Code());
        SetErrorCode(iErrorCode);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while executing IOCTL. %s \n", Error::Msg(Error::UserToOS(iErrorCode)).c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::IoControl(unsigned long int liControlCode,void* pInBuffer,long int liBufferLen,void* pOutBuffer,long int liOutBufferLen)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    if(0 >= liBufferLen || NULL == pInBuffer)
    {
        //sanity check.
        DebugPrintf(SV_LOG_ERROR, "Invalid input to IoControl.");
        return SV_FAILURE;
    }

    if ( (!IsOpen()) || (ACE_INVALID_HANDLE == m_pGenericFile->GetHandle()) ) 
    {
        SetState(STRM_BAD);
        DebugPrintf(SV_LOG_ERROR, "Device stream is not open.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
        return SV_FAILURE;
    }
    ResetErrorCode();
    if ( 0 == ioctl(m_pGenericFile->GetHandle(),liControlCode,pInBuffer, liBufferLen,pOutBuffer,liOutBufferLen) )
    {
        iStatus = SV_SUCCESS;
        SetState(STRM_GOOD);
    }
    else
    {
        int iErrorCode = Error::OSToUser(Error::Code());
        SetErrorCode(iErrorCode);
        int UserErrorCode = GetErrorCode();
        if ((IOCTL_INMAGE_STOP_FILTERING_DEVICE == liControlCode) && (ERR_NODEV == UserErrorCode))
        {
            SetState(STRM_GOOD);
            iStatus = SV_SUCCESS;
        }
        else
        {
        iStatus = SV_FAILURE;
        SetState(STRM_BAD);
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while executing IOCTL. %s \n",Error::Msg(Error::UserToOS(iErrorCode)).c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::WaitForEvent(unsigned int milliseconds)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    assert(!"to be implemented");
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
    return SV_FAILURE;
}
