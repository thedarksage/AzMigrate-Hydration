/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : filestream.cpp
 *
 * Description: 
 */
 
#ifdef SV_WINDOWS
#include <winerror.h>
#endif

#include <stdio.h>
#include <string>
#include <iostream>

#include "portable.h"
#include "entity.h"

#include "error.h"
#include "genericstream.h"
#include "inputoutputstream.h"
#include "genericfile.h"
#include "file.h"
#include "filestream.h"
#include "ace/OS.h"
#include "localconfigurator.h"

//using namespace inmage::foundation;

//namespace inmage {

/*
 * FUNCTION NAME :  FileStream   
 *
 * DESCRIPTION :    Constructor. Initializes the instance. Sets the pointer to a generic file to the file object.
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
 
FileStream::FileStream(const File& file)
	: m_pGenericFile(NULL),
	  m_iVolumeRetries(0),
	  m_iVolumeRetryDelay(0),
	  m_WaitEvent(NULL),
	  m_bRetry(false),
      m_ProfileStream(0),
	  m_bIsHandleExternal(false)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_pGenericFile = new File(file);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
 * FUNCTION NAME :  FileStream   
 *
 * DESCRIPTION :    Constructor. Initializes the instance. Set the pointer to a generic file as NULL.
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
 
FileStream::FileStream()
	: m_pGenericFile(NULL),
	  m_iVolumeRetries(0),
	  m_iVolumeRetryDelay(0),
	  m_WaitEvent(NULL),
	  m_bRetry(false),
      m_ProfileStream(0),
	  m_bIsHandleExternal(false)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME :  ~FileStream     
 *
 * DESCRIPTION :     Destructor. Closes the open file. Deletes the file object.
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
 
FileStream::~FileStream()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	Close();
	if ( NULL != m_pGenericFile )
	{
        	m_pGenericFile->SetHandle(ACE_INVALID_HANDLE);
        	delete m_pGenericFile;
	}
    m_pGenericFile  = NULL;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

/*
 * FUNCTION NAME :  FileStream     
 *
 * DESCRIPTION :    Constructor. Creates the file object with the given file name and 
 *                  initializes the file stream to point to the file object     
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
 
FileStream::FileStream(const char* szName)
	: m_pGenericFile(NULL),
	  m_iVolumeRetries(0),
	  m_iVolumeRetryDelay(0),
	  m_WaitEvent(NULL),
      m_ProfileStream(0),
	  m_bIsHandleExternal(false)
{
	m_pGenericFile = new File(szName);

}

/*
 * FUNCTION NAME :     
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
 
FileStream::FileStream(const std::string& sName)
	: m_pGenericFile(NULL),
	  m_iVolumeRetries(0),
	  m_iVolumeRetryDelay(0),
	  m_WaitEvent(NULL),
      m_ProfileStream(0),
	  m_bIsHandleExternal(false)

{
	m_pGenericFile = new File(sName);
}


FileStream::FileStream(const ACE_HANDLE &externalHandle)
: m_EffectiveHandle(externalHandle),
  m_pGenericFile(NULL),
  m_iVolumeRetries(0),
  m_iVolumeRetryDelay(0),
  m_WaitEvent(NULL),
  m_bRetry(false),
  m_ProfileStream(0),
  m_bIsHandleExternal(true)
{
	m_bOpen = true;
}


/*
 * FUNCTION NAME :  Open     
 *
 * DESCRIPTION :    Opens the file for read/write operations
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
 
int FileStream::Open(const STREAM_MODE mode)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	int iStatus = SV_SUCCESS;
	m_bOpen = false;
	if ( m_pGenericFile )
    {
        std::string sFile = m_pGenericFile->GetName();
	    if ( sFile.empty() )
	    {
		    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
		    SetState(STRM_BAD);		    
		    return SV_FAILURE;

	    }
    	
        FormatVolumeNameToGuid(sFile);
                       
	    unsigned long int uliAccess = 0;
        unsigned long int uliShareMode = 0;
	    unsigned long int uliFlags = 0;
        unsigned long int uliDisposition = 0;
        m_Mode = mode;
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
            DebugPrintf(SV_LOG_ERROR,"FAILED: Invalid file open flags.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
     	    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
		    SetState(STRM_BAD);     	    
            return SV_FAILURE;
        }

        if (ModeOn(Mode_Binary,m_Mode) )
        {
            uliAccess = O_BINARY;
        }

        if ( (ModeOn(Mode_Write,m_Mode) || ModeOn(Mode_RW,m_Mode)) && ( ModeOn(Mode_Append,m_Mode) ) )
        {
            uliAccess = O_APPEND;
        }

        // check for invalid combination
        // can't specify Mode_Truncate nor Mode_Overwrite with any of the other open/create modes
        if ((ModeOn(Mode_Truncate, m_Mode) && ModeOn(Mode_Overwrite, m_Mode))
            || ((ModeOn(Mode_Truncate, m_Mode) || ModeOn(Mode_Overwrite, m_Mode)) && (ModeOn(Mode_Open, m_Mode) || ModeOn(Mode_Create, m_Mode)))
            ) 
        {
            SetState(STRM_BAD);
            DebugPrintf(SV_LOG_ERROR,"FAILED: Invalid file open flags.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
     	    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            return SV_FAILURE;
        }
        // get open/create mode
        if (ModeOn(Mode_OpenCreate, m_Mode)) 
        {
            uliDisposition = O_CREAT;
        } 
        else if (ModeOn(Mode_Open, m_Mode)) 
        {
            uliDisposition = 0;
        } 
        else if (ModeOn(Mode_Create, m_Mode)) 
        {
            uliDisposition = (O_CREAT | O_EXCL);
        } 
        else if (ModeOn(Mode_Truncate, m_Mode)) 
        {
            uliDisposition = O_TRUNC;
        } 
        else if (ModeOn(Mode_Overwrite, m_Mode)) 
        {
            uliDisposition = (O_CREAT | O_TRUNC);
        } 
        else 
        {
            SetState(STRM_BAD);
            DebugPrintf(SV_LOG_ERROR,"FAILED: Invalid file open flags.@LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
     	    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            return SV_FAILURE;
        }

        // windows share access
        if ( ModeOn(Mode_ShareRW,m_Mode))
        {
            uliShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        }
        else if (ModeOn(Mode_ShareRead, m_Mode)) 
        {
            uliShareMode |= FILE_SHARE_READ;
        }
        else if (ModeOn(Mode_ShareWrite, m_Mode)) 
        {
            uliShareMode |= FILE_SHARE_WRITE;
        }

        if (ModeOn(Mode_ShareDelete, m_Mode)) 
        {
            uliShareMode |= FILE_SHARE_DELETE;
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
#ifdef SV_UNIX
		if (ModeOn(Mode_Direct, m_Mode))
		{
            setdirectmode(uliAccess);
		}
#endif
        uliAccess = (uliAccess | uliDisposition | uliFlags );

        Error::Code(SV_SUCCESS);
		// PR#10815: Long Path support
		m_pGenericFile->SetHandle( ACE_OS::open(getLongPathName(sFile.c_str()).c_str(),uliAccess,uliShareMode) );

	    if( ACE_INVALID_HANDLE != m_pGenericFile->GetHandle())
	    {
			m_EffectiveHandle.Set(m_pGenericFile->GetHandle());
            SetState(STRM_GOOD);
		    iStatus = SV_SUCCESS;
            m_bOpen = true;
	    }
        else
        {
            SetState(STRM_BAD);
		    iStatus = SV_FAILURE;
		    DebugPrintf( SV_LOG_ERROR,"FAILED: An error occurred while attempting to open file %s. %s \n ",sFile.c_str(),Error::Msg().c_str());
        }
    }
    else
    {
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
        DebugPrintf( SV_LOG_ERROR,"FAILED: Invalid file name specified.\n");
    }
 	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

	return iStatus;
}

/*
 * FUNCTION NAME :  Read     
 *
 * DESCRIPTION :    Reads data from the file for the specified length and fills the output buffer.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if read succeeds else SV_FAILURE
 *
 */
 
int FileStream::Read(void* sOutBuffer,unsigned long int uliBytes)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERING %s \n",FUNCTION_NAME);

	int iReadRetry=0;

	long int uliStatus = 0;
	if ( IsOpen() && ( ACE_INVALID_HANDLE != m_EffectiveHandle.Get() ) )
    {
        if ( NULL == sOutBuffer )
        {
            SetState(STRM_BAD);
            DebugPrintf(SV_LOG_ERROR,"FAILED: Cannot read into a null string. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
            return SV_FAILURE;
        }

        sOutBuffer = static_cast<char*>(sOutBuffer);

		// Read also when handle is externally supplied
        if ( IsOpen() && ( m_bIsHandleExternal || (Mode_RW,m_Mode) || ModeOn(Mode_Read,m_Mode) ) )
        {

	        long int uliBytesRead = 0;
	        if (uliBytes > 0 )
	        {
                ACE_Time_Value TimeBeforeRead, TimeAfterRead;
				do
				{
					Error::Code(SV_SUCCESS);
                    /* TODO: currently count for retry too */
                    (m_ProfileStream & E_PROFILEREAD) && (TimeBeforeRead=ACE_OS::gettimeofday());

					uliBytesRead = ACE_OS::read(m_EffectiveHandle.Get(), sOutBuffer, uliBytes);

                    (m_ProfileStream & E_PROFILEREAD) && (TimeAfterRead=ACE_OS::gettimeofday()) && (m_TimeForRead+=(TimeAfterRead-TimeBeforeRead));

					DebugPrintf(SV_LOG_DEBUG,"%s: Bytes to Read = %d; Bytes Read = %d\n",FUNCTION_NAME,uliBytes,uliBytesRead);
					int iErrorCode = Error::OSToUser(Error::Code());
					if ( (uliBytesRead == 0 ) && (SV_SUCCESS == iErrorCode ) )
					{
               			// successful ReafFile with 0 bytes reads indicates eof
						SetState(STRM_EOF);
						uliStatus = SV_FAILURE;
					}
					else if ( ( (uliBytes  - uliBytesRead ) > 0  ) && ( SV_SUCCESS == iErrorCode))//if ( liBytesRead != uliBytes)  || ( SV_SUCCESS != iErrorCode) )
					{                    
						DebugPrintf(SV_LOG_DEBUG,"INFO: Data read is of shorter length than requested length. Retry:%d\n", iReadRetry);
						SetState(STRM_GOOD);
						uliStatus = uliBytesRead;
						break;
	    			}
	    			else if ( uliBytesRead < 0 )
	    			{
						DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred during read.%s Retrying:%d\n",Error::Msg().c_str(), iReadRetry);
						SetState(STRM_BAD);
						uliStatus=SV_FAILURE;
						if(m_bRetry)
						{
							iReadRetry++;
							if(NULL != m_WaitEvent)
							{
								if(WAIT_SUCCESS == m_WaitEvent->Wait(m_iVolumeRetryDelay))
								{
									//quit signalled. m_Status and m_NativeError are already set.
									DebugPrintf(SV_LOG_DEBUG, "The quit signalled while waiting between retries. %d,  %s\n", LINE_NO, FILE_NAME);
									break;
								}
							}
						}
					}
					else if ( ((ERR_MORE_DATA == iErrorCode) || ( ERR_IO_PENDING == iErrorCode )))
					{
            			SetState(STRM_MORE_DATA);
						uliStatus = uliBytesRead;
               			DebugPrintf(SV_LOG_DEBUG,"INFO: More Data is available for read. %s.\n",Error::Msg().c_str());
						break;
					} 
	    			else if (ERR_HANDLE_EOF == iErrorCode )
	    			{
						uliStatus=SV_FAILURE;
            			SetState(STRM_EOF);
               			DebugPrintf( SV_LOG_ERROR,"FAILED: End of Stream. %s. \n ",Error::Msg().c_str());
						break;

	    			}
					else if ( SV_SUCCESS != iErrorCode ) 
					{
						DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred during read. Bytes to Read = %d; Bytes Read = %d. %s Retrying:%d \n",uliBytes,uliBytesRead,Error::Msg().c_str(), iReadRetry);
						SetState(STRM_FAIL);
						uliStatus=SV_FAILURE;
						if(m_bRetry)
						{
							iReadRetry++;
							if(NULL != m_WaitEvent)
							{
								if(WAIT_SUCCESS == m_WaitEvent->Wait(m_iVolumeRetryDelay))
								{
									DebugPrintf(SV_LOG_DEBUG, "The quit signalled while waiting between retries. %d,  %s\n", LINE_NO, FILE_NAME);
									break;
								}
							}
						}
	    			}
					else
					{
						SetState(STRM_GOOD);
						uliStatus = uliBytesRead;
						break;
					}
				}while ((NULL != m_WaitEvent) && (iReadRetry < m_iVolumeRetries));
	        }
            else
            {
                SetState(STRM_BAD);
                uliStatus=SV_FAILURE;
                DebugPrintf( SV_LOG_ERROR,"FAILED: The length of read requested is 0 bytes. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
            }	           
        }
        else
        {
            SetState(STRM_BAD);
            uliStatus=SV_FAILURE;
            DebugPrintf( SV_LOG_ERROR,"FAILED: Stream is not open or had been opened in incorrect mode. Reopen stream correctly. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        }
    }
    else
    {
        SetState(STRM_BAD);
        uliStatus = SV_FAILURE;
        DebugPrintf( SV_LOG_ERROR,"FAILED: Invalid file/handle name specified.\n");
    }	
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);

	return uliStatus;
}


unsigned long int FileStream::FullRead(void* sOutBuffer,unsigned long int uliBytes)
{
    int bytesRead = 0;
    unsigned long int totalBytesRead = 0;
    char *buf = static_cast<char*>(sOutBuffer);

    do {         		 
        bytesRead = Read(buf + totalBytesRead, uliBytes - totalBytesRead);
        if (bytesRead < 0) {
            return 0;
        } else if (0 == bytesRead) {			 
            break;
        }
        totalBytesRead += bytesRead;                      
    } while (totalBytesRead < uliBytes);

    return totalBytesRead;
}


/*
 * FUNCTION NAME :  Write     
 *
 * DESCRIPTION :    Writes data of specified length to the file.      
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if read succeeds else SV_FAILURE
 *
 */
 
int FileStream::Write(const void* sBuffer,const unsigned long int liBytes)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	int iStatus = SV_FAILURE;

	int iReadRetry = 0;

	//Write also when handle is externally supplied
	if ( IsOpen()  && ( m_bIsHandleExternal || ModeOn(Mode_RW,m_Mode) || ModeOn(Mode_Write,m_Mode)))
	{
		if ( (NULL == sBuffer ) || ( liBytes <= 0 ) )
		{
            SetState(STRM_BAD);
       		DebugPrintf( SV_LOG_ERROR,"FAILED: Invalid buffer or buffer size. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
			return iStatus;
		}
		if (ACE_INVALID_HANDLE == m_EffectiveHandle.Get())
		{
            SetState(STRM_BAD);
            DebugPrintf( SV_LOG_ERROR,"FAILED: Invalid Handle. Cannot write. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
            return iStatus;
		}
	
		long int liBytesWritten = 0;
	
		Error::Code(SV_SUCCESS);
		do
		{
			liBytesWritten = ACE_OS::write(m_EffectiveHandle.Get(), sBuffer, liBytes);
			if ( liBytesWritten == liBytes )
			{
				SetState(STRM_GOOD);
				iStatus = liBytesWritten;
				break;
			}
			else
			{
				if(m_WaitEvent && (WAIT_SUCCESS == m_WaitEvent->Wait(m_iVolumeRetryDelay)))
				{
					DebugPrintf(SV_LOG_DEBUG, "The quit signalled while waiting between retries. %d,  %s\n", LINE_NO, FILE_NAME);
					break;
				}
				iReadRetry++;
			}
		}while ((NULL != m_WaitEvent) && (iReadRetry < m_iVolumeRetries));
		if(liBytesWritten != liBytes)
		{
			SetState(STRM_BAD);
			DebugPrintf( SV_LOG_ERROR,"FAILED: An error occurred during write. %s. @LINE %d in FILE %s\n",Error::Msg().c_str(),LINE_NO,FILE_NAME);
			iStatus = SV_FAILURE;
		}
	}
	else
	{
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
        DebugPrintf( SV_LOG_ERROR,"FAILED: Could not write to file.\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return iStatus;
}


/*
 * FUNCTION NAME :  Clear     
 *
 * DESCRIPTION :    Clears the stream state.
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
 
void FileStream::Clear()
{
    GenericStream::Clear();
    m_TimeForRead = 0;
}

/*
 * FUNCTION NAME :  Close     
 *
 * DESCRIPTION :    Closes the file and frees the resources.
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
 
int FileStream::Close()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    this->Clear();

    if ( IsOpen()  && ( NULL != m_pGenericFile ) )
    {
        if ( ACE_INVALID_HANDLE != m_pGenericFile->GetHandle())
        {
            ACE_OS::close(m_pGenericFile->GetHandle());
            m_pGenericFile->SetHandle(ACE_INVALID_HANDLE);
        }
        delete m_pGenericFile;
    }

    
    m_bOpen         = false;
    m_pGenericFile = NULL;
    SetState(STRM_CLEAR);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return SV_SUCCESS;
}

/*
 * FUNCTION NAME :  mapToFilePos     
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
 
int FileStream::mapToFilePos(FILEPOS position)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	long int liPosition = 0;
	switch (position)
	{
		case POS_BEGIN:
			liPosition = SEEK_SET;
		break;
		case POS_CURRENT:
			liPosition = SEEK_CUR;
		break;
		case POS_END:
			liPosition = SEEK_END;
		break;
		
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return liPosition;
}

/*
 * FUNCTION NAME :  Seek     
 *
 * DESCRIPTION :    Seeks to the specified location in file.  
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if seek succeeded else SV_FAILURE
 *
 */
 
SV_LONGLONG FileStream::Seek(SV_LONGLONG OffsetToSeek,FILEPOS pos)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
    
	SV_LONGLONG llStatus = 0;
    if ( IsOpen() ) 
    {
	    

        ACE_LOFF_T seekOffset;
        seekOffset = (ACE_LOFF_T)OffsetToSeek;
        ACE_LOFF_T newOffset;
        newOffset = (ACE_LOFF_T)0LL;

		int iReadRetry = 0;

        int iSeekFrom = mapToFilePos(pos);
		do
		{
			Error::Code(SV_SUCCESS);
			newOffset = ACE_OS::llseek(m_EffectiveHandle.Get(), seekOffset, iSeekFrom);
			if (newOffset < 0) 
			{
				SetState(STRM_BAD);
				llStatus = SV_FAILURE;
				if(m_bRetry)
				{
					iReadRetry++;
					if(NULL != m_WaitEvent)
					{
						if(WAIT_SUCCESS == m_WaitEvent->Wait(m_iVolumeRetryDelay))
						{
							DebugPrintf(SV_LOG_DEBUG, "The quit signalled while waiting between retries. %d,  %s\n", LINE_NO, FILE_NAME);
							break;
						}
					}
				}
				DebugPrintf( SV_LOG_DEBUG,"FAILED: An error occurred during seek to file position. %s, Retrying: %d \n ",Error::Msg().c_str(), iReadRetry);
			}
			else
			{
				llStatus = newOffset;
				SetState(STRM_GOOD);
				break;
			}
		}while((NULL != m_WaitEvent) && (iReadRetry < m_iVolumeRetries));
    }
    else
    {
        SetState(STRM_BAD);
        llStatus = SV_FAILURE;
        DebugPrintf( SV_LOG_ERROR,"FAILED: An error occurred during seek to file position. %s \n ",Error::Msg().c_str());
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
	return llStatus;

}

/*
 * FUNCTION NAME :  Tell     
 *
 * DESCRIPTION :    Gets the current position of the file pointer as an scalar
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns the position of the file pointer in the file
 *
 */
 
const SV_LONGLONG FileStream::Tell()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_LONGLONG llPos = -1;
    if ( IsOpen() )
    {
		int iReadRetry=0;
        ACE_LOFF_T seekOffset;
        seekOffset = (ACE_LOFF_T)0LL;
        ACE_LOFF_T newOffset;
        newOffset = (ACE_LOFF_T)0LL;

        int iSeekFrom = mapToFilePos(POS_CURRENT);
		do
		{
			Error::Code(SV_SUCCESS);
			newOffset = ACE_OS::llseek(m_EffectiveHandle.Get(), seekOffset, iSeekFrom);

			if (newOffset < 0) 
			{
				SetState(STRM_BAD);
				if(m_bRetry)
				{
					iReadRetry++;
					if(NULL != m_WaitEvent)
					{
						if(WAIT_SUCCESS == m_WaitEvent->Wait(m_iVolumeRetryDelay))
						{
							//quit signalled. m_Status and m_NativeError are already set.
							DebugPrintf(SV_LOG_DEBUG, "The quit signalled while waiting between retries. %d,  %s\n", LINE_NO, FILE_NAME);
							break;
						}
					}
				}
				DebugPrintf(SV_LOG_DEBUG,"FAILED: Failed to Tell current location of file pointer. %s\n", Error::Msg().c_str());
				llPos = -1;
			}
			else
			{
				llPos = newOffset;
				SetState(STRM_GOOD);
				break;
			}
		}while((NULL != m_WaitEvent) && (iReadRetry < m_iVolumeRetries));
    }
    else
    {
        SetState(STRM_BAD);
        llPos = SV_FAILURE;
        DebugPrintf( SV_LOG_ERROR,"FAILED: An error occurred during Tell of file position. %s \n ",Error::Msg().c_str());
    }
    
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return llPos;

}


/*
 * FUNCTION NAME : EnableReadRetry
 *
 * DESCRIPTION : Enables the read retry functionality. if reads fail, read function will retry.
 *
 * INPUT PARAMETERS : Function pointer to wait, number of retries, delay between retry.
 *
 * OUTPUT PARAMETERS :
 *
 * NOTES :
 *
 * return value : 
 *
 */
void FileStream::EnableRetry(Event* pEvent, int iRetries, int iRetryDelay)
{
	m_WaitEvent = pEvent;
	m_iVolumeRetries = iRetries;
	m_iVolumeRetryDelay = iRetryDelay;
	m_bRetry = true;
}

/*
 * FUNCTION NAME : DisableReadRetry
 *
 * DESCRIPTION : Disables the read retry functionality.
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
void FileStream::DisableRetry()
{
	m_WaitEvent = NULL;
	m_iVolumeRetries = 0;
	m_iVolumeRetryDelay = 0;
	m_bRetry = false;
}


void FileStream::ProfileRead(void)
{
    m_ProfileStream |= E_PROFILEREAD;
}


ACE_Time_Value FileStream::GetTimeForRead(void)
{
    return m_TimeForRead;
}
