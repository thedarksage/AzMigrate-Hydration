//Windows port of devicestream.cpp

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

int DeviceStream::Open(STREAM_MODE Mode)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
//    if ( !IsInitialized() )
//            Init();
	
//    if ( !IsInitialized() )
//	{

//        DebugPrintf(SV_LOG_ERROR,"FAILED: Device is not initialized. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
//		DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
//		return SV_FAILURE;
//	}
    
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
			DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
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
			DebugPrintf(SV_LOG_ERROR, "Invalid Open mode: %s specified for device stream. LINE %d in FILE %s \n", ssmode.str().c_str(), LINE_NO, FILE_NAME);
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

        // Following Modes do not apply to devices
/*
        if (ModeOn(Mode_OpenCreate, m_Mode)) 
        {
            uliDisposition = OPEN_ALWAYS;
        } 
        else if (ModeOn(Mode_Create, m_Mode)) 
        {
            uliDisposition = CREATE_NEW;
        } 
        else if (ModeOn(Mode_Truncate, m_Mode)) 
        {
            uliDisposition = TRUNCATE_EXISTING;
        } 
        else if (ModeOn(Mode_Overwrite, m_Mode)) 
        {
            uliDisposition = CREATE_ALWAYS;
        }
*/
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
                m_IoEvent.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		        if( INVALID_HANDLE_VALUE != m_IoEvent.hEvent)
		        {
//                    m_bGood = true;
                    SetState(STRM_GOOD);
                    m_bOpen = true;
			        m_bSync = false;
			        iStatus = SV_SUCCESS;
		        }
		        else
                {
					int systemErrorCode = Error::Code();
					int userErrorCode = Error::OSToUser(systemErrorCode);
					SetErrorCode(userErrorCode);
                    m_bOpen = false;
                    SetState(STRM_BAD);
			        DebugPrintf( SV_LOG_ERROR,"FAILED Couldn't allocate event for asynchronous io. %s \n ",Error::Msg(systemErrorCode).c_str());
                    ACE_OS::close(m_pGenericFile->GetHandle());
			        iStatus = SV_FAILURE;
                }
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
			DebugPrintf(SV_LOG_ERROR, "FAILED: An error occurred while attempting to open device %s. %s \n ", sName.c_str(), Error::Msg(systemErrorCode).c_str());
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
        if (!CancelIo(m_pGenericFile->GetHandle()))
        {
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR,"Failed to Cancel IO on device\n");
        }
        else
        {
            iStatus = SV_SUCCESS;
            SetState(STRM_GOOD);
        }
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
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int iStatus = SV_FAILURE;

    if (IsOpen() && (NULL != m_pGenericFile))
    {
        if (!CancelIoEx(m_pGenericFile->GetHandle(), &m_IoEvent))
        {
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "Failed to Cancel IO on device\n");
        }
        else
        {
            iStatus = SV_SUCCESS;
            SetState(STRM_GOOD);
        }
    }
    else
    {
        SetState(STRM_FAIL);
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR, "Invalid device specified\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return iStatus;
}

int DeviceStream::IoControl(unsigned long int liControlCode,void* pInBuffer)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
	assert(!"to be implemented");
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
	return SV_FAILURE;
}

int DeviceStream::IoControl(unsigned long int liControlCode,void* pInBuffer,long int liBufferLen)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
	int iStatus = SV_FAILURE;
    assert(!"to be implemented");
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
	return iStatus;
}

int DeviceStream::IoControl(unsigned long int liControlCode)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
	int iStatus = SV_FAILURE;
    assert(!"to be implemented");
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
	return iStatus;
}

int DeviceStream::IoControl(unsigned long int liControlCode,void* pInBuffer,long int liBufferLen,void* pOutBuffer,long int liOutBufferLen)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
	int iStatus = SV_FAILURE;
	if ( !IsOpen() && (0 != m_pGenericFile->GetHandle()))
	{
        SetState(STRM_BAD);
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
		return SV_FAILURE;
	}
	unsigned long int uliBytes = 0;
    ResetErrorCode();
    BOOL bReturn = DeviceIoControl( m_pGenericFile->GetHandle(), liControlCode, pInBuffer, liBufferLen, pOutBuffer, liOutBufferLen, &uliBytes, (!m_bSync ? &m_IoEvent : NULL));
    if (!bReturn)
    {
        int iErrorCode = Error::OSToUser(Error::Code());
        SetErrorCode(iErrorCode);
    }
    int UserErrorCode = GetErrorCode();
    if ( (!bReturn) && (ERR_IO_PENDING != UserErrorCode) )
	{
		/* ERR_INVALID_FUNCTION needed for proper exception handling in upper levels
        if ( ERR_INVALID_FUNCTION == UserErrorCode )
        {
            SetState(STRM_GOOD);
    	    iStatus = SV_SUCCESS;
        }
        else*/ if (ERR_BUSY == UserErrorCode)
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_WARNING,"An error occurred while executing IOCTL. %s \n",Error::Msg(Error::UserToOS(UserErrorCode)).c_str());
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
            DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while executing IOCTL. %s \n",Error::Msg(Error::UserToOS(UserErrorCode)).c_str());
        }
	}
	else
	{
        SetState(STRM_GOOD);
		iStatus = SV_SUCCESS;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

	return iStatus;
}

int DeviceStream::WaitForEvent(unsigned int milliseconds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int iStatus = SV_FAILURE;

    if (IsOpen() && !m_bSync && m_IoEvent.hEvent != INVALID_HANDLE_VALUE)
    {
        int ret = WaitForSingleObject(m_IoEvent.hEvent, milliseconds ? milliseconds : INFINITE);
        if (ret)
        {
            if (ret == WAIT_FAILED)
            {
                SetState(STRM_BAD);
             
                int error = GetLastError();
                DebugPrintf(SV_LOG_ERROR, "Wait for event failed on device with error 0x%x\n", error);
            }
            else if (ret == WAIT_TIMEOUT)
            {
                DebugPrintf(SV_LOG_DEBUG, "Wait timedout for device.\n");
            }
            else
            {
                SetState(STRM_BAD);

                DebugPrintf(SV_LOG_ERROR, "Wait for event failed to device with return code 0x%x\n", ret);
            }
        }
        else
        {
            iStatus = SV_SUCCESS;
            SetState(STRM_GOOD);
        }
    }
    else
    {
        SetState(STRM_FAIL);
        DebugPrintf(SV_LOG_ERROR, "%s: Invalid operation for device specified\n" , FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return iStatus;
}