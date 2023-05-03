/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : basicio.cpp
*
* Description: 
*/

#include "basicio.h"
#include "portable.h"
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "localconfigurator.h"
#include "portablehelpers.h"

BasicIo::BioOpenMode const BasicIo::BioRead            = (BasicIo::BioOpenMode)0x0001;             // read only  
BasicIo::BioOpenMode const BasicIo::BioWrite           = (BasicIo::BioOpenMode)0x0002;             // write only
BasicIo::BioOpenMode const BasicIo::BioRW              = (BasicIo::BioOpenMode)(BasicIo::BioRead | BasicIo::BioWrite);  // read write (convienence)
BasicIo::BioOpenMode const BasicIo::BioBinary          = (BasicIo::BioOpenMode)0x0004;             // if not set opens as text
BasicIo::BioOpenMode const BasicIo::BioAppend          = (BasicIo::BioOpenMode)0x0008;             // all writes done at end of file

/*
* open creation flags must provide one of the following        
* note: BasicIo::BioOverwrite and BasicIo::BioTruncate 
* can not be used with any other open/creation flags
*/
BasicIo::BioOpenMode const BasicIo::BioOpen            = (BasicIo::BioOpenMode)0x0010;             // open if exists else fail
BasicIo::BioOpenMode const BasicIo::BioCreate          = (BasicIo::BioOpenMode)0x0020;             // create if it doesn't exist else fail
BasicIo::BioOpenMode const BasicIo::BioOpenCreate      = (BasicIo::BioOpenMode)(BasicIo::BioOpen | BasicIo::BioCreate);  // open if exists else create        
BasicIo::BioOpenMode const BasicIo::BioOverwrite       = (BasicIo::BioOpenMode)0x0040;             // create if not exists, else overwrite existing
BasicIo::BioOpenMode const BasicIo::BioTruncate        = (BasicIo::BioOpenMode)0x0080;             // open existing and truncate it

/*
* sharing not sure if these names will be useful on other OSes.
* the semantics might be different to.)
* on windows it means you will allow subsequent opens this share access
*/
BasicIo::BioOpenMode const BasicIo::BioShareDelete     = (BasicIo::BioOpenMode)0x0100;
BasicIo::BioOpenMode const BasicIo::BioShareRead       = (BasicIo::BioOpenMode)0x0200;
BasicIo::BioOpenMode const BasicIo::BioShareWrite      = (BasicIo::BioOpenMode)0x0400;               
BasicIo::BioOpenMode const BasicIo::BioShareRW         = (BasicIo::BioOpenMode)(BasicIo::BioShareRead   | BasicIo::BioShareWrite);
BasicIo::BioOpenMode const BasicIo::BioShareAll        = (BasicIo::BioOpenMode)(BasicIo::BioShareDelete | BasicIo::BioShareRW);

BasicIo::BioOpenMode const BasicIo::BioNoBuffer        = (BasicIo::BioOpenMode)0x1000;
BasicIo::BioOpenMode const BasicIo::BioWriteThrough    = (BasicIo::BioOpenMode)0x2000;
BasicIo::BioOpenMode const BasicIo::BioDirect          = (BasicIo::BioOpenMode)0x4000;
BasicIo::BioOpenMode const BasicIo::BioSequentialAccess = (BasicIo::BioOpenMode)0x8000;

BasicIo::BioOpenMode const BasicIo::BioReadExisting    = (BasicIo::BioOpenMode)(BasicIo::BioOpen  | BasicIo::BioRead);
BasicIo::BioOpenMode const BasicIo::BioWriteNew        = (BasicIo::BioOpenMode)(BasicIo::BioWrite | BasicIo::BioCreate);
BasicIo::BioOpenMode const BasicIo::BioWriteAlways     = (BasicIo::BioOpenMode)(BasicIo::BioWrite | BasicIo::BioOpenCreate);
BasicIo::BioOpenMode const BasicIo::BioRWExisitng      = (BasicIo::BioOpenMode)(BasicIo::BioRW    | BasicIo::BioOpen);
BasicIo::BioOpenMode const BasicIo::BioRWNew           = (BasicIo::BioOpenMode)(BasicIo::BioRW    | BasicIo::BioCreate);
BasicIo::BioOpenMode const BasicIo::BioRWAlways        = (BasicIo::BioOpenMode)(BasicIo::BioRW    | BasicIo::BioOpenCreate);

BasicIo::BioSeekFrom const BasicIo::BioBeg             = (BasicIo::BioSeekFrom)1;
BasicIo::BioSeekFrom const BasicIo::BioCur             = (BasicIo::BioSeekFrom)2;
BasicIo::BioSeekFrom const BasicIo::BioEnd             = (BasicIo::BioSeekFrom)3;

/*
* FUNCTION NAME : BasicIo
*
* DESCRIPTION : Constructor
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
* return value : none.
*
*/
BasicIo::BasicIo()
: m_File(ACE_INVALID_HANDLE),
m_Status(SV_SUCCESS),
m_WaitFunction(NULL),
m_iVolumeRetries(0),
m_iVolumeRetryDelay(0),
m_bRetry(false),
m_Mode(0)
{
}


/*
* FUNCTION NAME : BasicIo
*
* DESCRIPTION : Constructor
*
* INPUT PARAMETERS : filename in a char array and mode
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
* return value : none.
*
*/
BasicIo::BasicIo(char const * name, BioOpenMode mode) 
: m_Name(name),
m_File(ACE_INVALID_HANDLE), 
m_Status(SV_SUCCESS),
m_NativeError(Error::UserToOS(SV_SUCCESS)),
m_WaitFunction(NULL),
m_iVolumeRetries(0),
m_iVolumeRetryDelay(0),
m_bRetry(false),
m_Mode(0)
{
    Open(name, mode);
}

/*
* FUNCTION NAME : BasicIo
*
* DESCRIPTION : Constructor
*
* INPUT PARAMETERS : file name in a std::string and open mode.
*
* OUTPUT PARAMETERS : none.
*
* NOTES :
*
* return value : none.
*
*/
BasicIo::BasicIo(std::string const & name, BioOpenMode mode) 
: m_Name(name),
m_File(ACE_INVALID_HANDLE), 
m_Status(SV_SUCCESS),
m_NativeError(Error::UserToOS(SV_SUCCESS)),
m_WaitFunction(NULL),
m_iVolumeRetries(0),
m_iVolumeRetryDelay(0),
m_bRetry(false),
m_Mode(0)
{
    Open(name.c_str(), mode);
}

/*
* FUNCTION NAME : ~BasicIo
*
* DESCRIPTION : Destructor
*
* INPUT PARAMETERS : none.
*
* OUTPUT PARAMETERS : none.
*
* NOTES :
*
* return value :
*
*/
BasicIo::~BasicIo()
{
    Close();
}

/*
* FUNCTION NAME : Open
*
* DESCRIPTION :
*
* INPUT PARAMETERS : file name as std::string and open mode.
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value :
*
*/
SV_INT BasicIo::Open(std::string const & name, BioOpenMode mode)
{
    return Open(name.c_str(), mode);
}

/*
* FUNCTION NAME : Open
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : file name as char array and open mode.
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value :
*
*/
SV_INT BasicIo::Open(char const * name, BioOpenMode mode)
{	 	
    if (ACE_INVALID_HANDLE != m_File) 
    {
        if (m_Name == name) 
        {
            return SV_SUCCESS;
        }
        else 
        {
            if (SV_SUCCESS != Close()) 
            {
                return m_Status;
            }
        }
    }

    m_Name = name;

    // map mode to the various windows equivelants
    // windows access
    SV_ULONG access;
    if (ModeOn(BioRW, mode)) 
    {
        access = O_RDWR;
    }
    else if (ModeOn(BioRead, mode)) 
    {
        access = O_RDONLY;
    }
    else if (ModeOn(BioWrite, mode)) 
    {
        access = O_WRONLY;
    }
    else
    {
        // required
        m_Status = ERR_INVALID_PARAMETER;
        m_NativeError = Error::UserToOS(ERR_INVALID_PARAMETER);
        return m_Status;
    }

    // windows disposition
    SV_ULONG disp;

    // check for invalid combination
    // can't specify BioTruncate nor BioOverwrite with any of the other open/create modes
    if ((ModeOn(BioTruncate, mode) && ModeOn(BioOverwrite, mode))
        || ((ModeOn(BioTruncate, mode) || ModeOn(BioOverwrite, mode)) && (ModeOn(BioOpen, mode) || ModeOn(BioCreate, mode)))) 
    {
        m_Status = ERR_INVALID_PARAMETER;
        m_NativeError = Error::UserToOS(ERR_INVALID_PARAMETER);
        return m_Status;
    }
    // get open/create mode
    if (ModeOn(BioOpenCreate, mode))
    {
        disp = O_CREAT;
    }
    else if (ModeOn(BioOpen, mode))
    {
        disp = 0;
    }
    else if (ModeOn(BioCreate, mode))
    {
        disp = (O_CREAT | O_EXCL);
    }
    else if(ModeOn(BioTruncate, mode))
    {
        disp = O_TRUNC;
    }
    else if (ModeOn(BioOverwrite, mode))
    {
        disp = (O_CREAT | O_TRUNC);
    }
    else
    {
        // required         
        m_Status = ERR_INVALID_PARAMETER;
        m_NativeError = Error::UserToOS(ERR_INVALID_PARAMETER);
        return m_Status;
    }

    // windows share access
    SV_ULONG share = 0;

#ifdef SV_UNIX

    umask(S_IWGRP | S_IWOTH);
    share = ACE_DEFAULT_FILE_PERMS;

#else

    if (ModeOn(BioShareDelete, mode))
    {
        share |= FILE_SHARE_DELETE;
    }
    if (ModeOn(BioShareRead, mode))
    {
        share |= FILE_SHARE_READ;
    }
    if (ModeOn(BioShareWrite, mode))
    {
        share |= FILE_SHARE_WRITE;
    }

#endif

    // check for special flags
    SV_ULONG flags = FILE_ATTRIBUTE_NORMAL;

    if (ModeOn(BioNoBuffer, mode))
    {
        flags |= FILE_FLAG_NO_BUFFERING;
    }

    if (ModeOn(BioWriteThrough, mode))
    {
        flags |= FILE_FLAG_WRITE_THROUGH;
    }

#ifdef SV_WINDOWS

	if (IsDirectoryExisting(name))
	{
		flags |= FILE_FLAG_BACKUP_SEMANTICS;
	}

#endif

#ifdef SV_UNIX
    if (ModeOn(BioDirect, mode))
    {
        setdirectmode(access);
    }
#endif


    access = (access | disp | flags );
	// PR#10815: Long Path support
	m_File = ACE_OS::open(getLongPathName(name).c_str(), access, share);
    if (ACE_INVALID_HANDLE == m_File)
    {
        m_Status = SV_FAILURE;
        m_NativeError = ACE_OS::last_error();
    }
    else
    {
        m_Mode = mode;
        m_Status = SV_SUCCESS;
        m_NativeError = Error::UserToOS(SV_SUCCESS);
    }
 
    return m_Status;
}


BasicIo::BioOpenMode BasicIo::OpenMode(void)
{
    return m_Mode;
}


/*
* FUNCTION NAME : RetryableRead
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : char pointer to buffer and length of the buffer.
*
* OUTPUT PARAMETERS :
*
* NOTES : Re-try on failure as per the configuration
*    
* return value : Number of bytes read.
*                0 on EOF, -1 on error
*
*/

ssize_t BasicIo::RetryableRead(char* buffer, SV_UINT length)
{	 
    SV_UINT retryCount = 0;
    ssize_t bytesRead = 0;
    SV_INT lastError = 0;

    while(true)
    {
        bytesRead = ACE_OS::read(m_File, buffer, length);

        // check for EOF. ACE_OS::read returns zero  on EOF
        // clear previous errors
        if (0 == bytesRead) 
        {
            lastError = 0;
            break;
        }

        // check for success and clear previous errors
        if(bytesRead > 0)
        {
            lastError = 0;
            break;
        }

        // store the error returned from the i/o operation
        // it will be used to overwrite error returned by the timer expiry
        lastError = ACE_OS::last_error();

        // check if quit request/wait function is installed
        if(!m_WaitFunction)
        {
            break;
        }

        // check if we have already retried specified no. of times
        if(retryCount > m_iVolumeRetries)
        {
            break;
        }

        ++retryCount;
        DebugPrintf(SV_LOG_DEBUG, "Read Retry  %d on volume %s\n", retryCount, m_Name.c_str());
        if((*m_WaitFunction)(m_iVolumeRetryDelay))
        {
            // quit signalled.
            DebugPrintf(SV_LOG_DEBUG, "Got a quit request during read retries on volume %s.\n", m_Name.c_str());
            break;
        }
    }

    // restore the error to the one returned by I/O operation
    ACE_OS::last_error(lastError);
    return bytesRead;
}

/*
* FUNCTION NAME : Read
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : pointer to output buffer and length
*
* OUTPUT PARAMETERS : pointer to output buffer and length
*
* NOTES :
*
* return value :
*
*/
SV_UINT BasicIo::Read(char* buffer, SV_UINT length)
{	 
    if (ACE_INVALID_HANDLE == m_File) {
        m_Status = ERR_INVALID_HANDLE;
        m_NativeError = Error::UserToOS(ERR_INVALID_HANDLE);
        return 0;
    }

    ssize_t bytesRead = RetryableRead(buffer, length);
    if (bytesRead < 0) {
        m_Status = SV_FAILURE;
        m_NativeError = ACE_OS::last_error();
        return 0;
    } else if (0 == bytesRead) {
        m_Status = ERR_HANDLE_EOF;
        m_NativeError = Error::UserToOS(ERR_HANDLE_EOF);
    }

    return bytesRead;
}

/*
* FUNCTION NAME : FullRead
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
SV_ULONG BasicIo::FullRead(char* buffer, SV_ULONG length)
{	 	 	

    if (ACE_INVALID_HANDLE == m_File) {
        m_Status = ERR_INVALID_HANDLE;
        m_NativeError = Error::UserToOS(ERR_INVALID_HANDLE);
        return 0;
    }

    if (length == 0) {
        return 0;
    }	 

    ssize_t bytesRead = 0;
    SV_ULONG totalBytesRead = 0;

    do {         		 
        bytesRead = RetryableRead(buffer + totalBytesRead, length - totalBytesRead);
        if (bytesRead < 0) {
            m_Status = SV_FAILURE;
            m_NativeError = ACE_OS::last_error();
            return 0;
        } else if (0 == bytesRead) {			 
            m_Status = ERR_HANDLE_EOF;
            m_NativeError = Error::UserToOS(ERR_HANDLE_EOF);
            break;
        }
        totalBytesRead += bytesRead;                      
    } while (totalBytesRead < length);

    return totalBytesRead;
}

/*
* FUNCTION NAME : RetryableWrite
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : char pointer to buffer and length of the buffer.
*
* OUTPUT PARAMETERS :
*
* NOTES : Re-try on failure as per the configuration
*    
* return value : Number of bytes written to the file.
*
*/

ssize_t BasicIo::RetryableWrite(char const * buffer, SV_UINT length)
{
    SV_UINT retryCount = 0;
    ssize_t bytesWritten = -1;
    SV_INT lastError = 0;

    while(true)
    {

        bytesWritten = ACE_OS::write( m_File,  buffer , length );

        // check for success. ACE_OS::write  returns -1 on failure
        // clear error on success
        if(-1 != bytesWritten)
        {
            lastError = 0;
            break;
        }

        // store the error returned from the i/o operation
        // it will be used to overwrite error returned by the timer expiry
        lastError = ACE_OS::last_error();

        // check if quit request/wait function is installed
        if(!m_WaitFunction)
        {
            break;
        }

        // check if we have already retried specified no. of times
        if(retryCount > m_iVolumeRetries)
        {
            break;
        }

        ++retryCount;

        DebugPrintf(SV_LOG_DEBUG, "Write Retry %d on volume %s\n", retryCount, m_Name.c_str());
        if((*m_WaitFunction)(m_iVolumeRetryDelay))
        {
            // quit signalled.
            DebugPrintf(SV_LOG_DEBUG, 
                "Got a quit request during write retries on volume %s.\n", m_Name.c_str());
            break;
        }

    }

    // restore the error to the one returned by I/O operation
    ACE_OS::last_error(lastError);
    return bytesWritten;
}

/*
* FUNCTION NAME : Write
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : char pointer to buffer and length of the buffer.
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value : Number of bytes written to the file.
*
*/
SV_UINT BasicIo::Write(char const * buffer, SV_UINT length)
{   

    if (ACE_INVALID_HANDLE == m_File)
    {
        m_Status = ERR_INVALID_HANDLE;
        m_NativeError = Error::UserToOS(ERR_INVALID_HANDLE);
        return 0;
    }

    ssize_t bytesWritten = 0;
    SV_UINT totalBytesWritten = 0;

    do
    {
        // ACE_OS::write  returns -1 on failure
        bytesWritten = RetryableWrite(buffer + totalBytesWritten, length - totalBytesWritten);
        if (bytesWritten == -1) {
            m_Status = SV_FAILURE;
            m_NativeError = ACE_OS::last_error();
            // Should we return the actual bytes written or zero?
            // Lot of code may already dependent on the value zero for 
            // failure... let it be zero until we check all places
            // where this routine is being used.
            return 0;             
        }

        totalBytesWritten += bytesWritten; 

    } while(totalBytesWritten < length);

    m_Status = SV_SUCCESS;
    m_NativeError = Error::UserToOS(SV_SUCCESS);
    return totalBytesWritten;
}

/*
* FUNCTION NAME : RetryableSeek
*
* DESCRIPTION : Seeks to the specified offset.
*
* INPUT PARAMETERS : longlong offset and seektype.
*
* OUTPUT PARAMETERS :
*
* NOTES : Re-try on failure as per the configuration
*
* return value : new offset.
*
*/

ACE_LOFF_T BasicIo::RetryableSeek(ACE_LOFF_T seekOffset, int seekFrom)
{    

    SV_UINT retryCount = 0;
    ACE_LOFF_T newOffset = (ACE_LOFF_T)0LL;
    SV_INT lastError = 0;

    while(true)
    {

        newOffset = ACE_OS::llseek(m_File, seekOffset, seekFrom);

        // check for success. ACE_OS::llseek  returns negative on failure
        // clear error on success
        if(newOffset >= 0)
        {
            lastError = 0;
            break;
        }

        // store the error returned from the i/o operation
        // it will be used to overwrite error returned by the timer expiry
        lastError = ACE_OS::last_error();

        // check if quit request/wait function is installed
        if(!m_WaitFunction)
        {
            break;
        }

        // check if we have already retried specified no. of times
        if(retryCount > m_iVolumeRetries)
        {
            break;
        }

        ++retryCount;

        DebugPrintf(SV_LOG_DEBUG, "Seek Retry %d on volume %s\n", retryCount, m_Name.c_str());
        if((*m_WaitFunction)(m_iVolumeRetryDelay))
        {
            // quit signalled.
            DebugPrintf(SV_LOG_DEBUG, "Got a quit request during Seek retries on volume %s.\n", 
                m_Name.c_str());
            break;
        }

    }

    // restore the error to the one returned by I/O operation
    ACE_OS::last_error(lastError);
    return newOffset;
}

/*
* FUNCTION NAME : Seek
*
* DESCRIPTION : Seeks to the offset.
*
* INPUT PARAMETERS : longlong offset and seektype.
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value : new offset.
*
*/
SV_LONGLONG BasicIo::Seek(SV_LONGLONG offset, BioSeekFrom from)
{    

    if (ACE_INVALID_HANDLE == m_File) {
        m_Status = ERR_INVALID_HANDLE;
        m_NativeError = Error::UserToOS(ERR_INVALID_HANDLE);
        return -1;
    }

    ACE_LOFF_T seekOffset;
    seekOffset = (ACE_LOFF_T)offset;
    ACE_LOFF_T newOffset;
    newOffset = (ACE_LOFF_T)0LL;
    int seekFrom;
    switch (from) {
         case BioBeg:
             seekFrom = SEEK_SET;
             break;
         case BioCur:
             seekFrom = SEEK_CUR;
             break;
         case BioEnd:
             seekFrom = SEEK_END;
             break;
         default:
             m_Status = ERR_INVALID_PARAMETER;
             m_NativeError = Error::UserToOS(ERR_INVALID_PARAMETER);
             return -1;
    }
    if ((newOffset = RetryableSeek(seekOffset, seekFrom)) < 0) {
        m_Status = SV_FAILURE;
        m_NativeError = ACE_OS::last_error();
        return -1;
    } 

    m_Status = SV_SUCCESS;
    m_NativeError = Error::UserToOS(SV_SUCCESS);
    return (SV_LONGLONG)newOffset;
}


/*
* FUNCTION NAME : RetryableTell
*
* DESCRIPTION : returns the current offset.
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value : current offset.
*
*/

ACE_LOFF_T BasicIo::RetryableTell() const
{

    SV_UINT retryCount = 0;
    ACE_LOFF_T seekOffset = (ACE_LOFF_T)0LL;
    ACE_LOFF_T newOffset = (ACE_LOFF_T)0LL;
    int seekFrom = SEEK_CUR;
    SV_INT lastError = 0;

    do
    {

        newOffset = ACE_OS::llseek(m_File, seekOffset, seekFrom);

        // check for success. ACE_OS::llseek  returns negative on failure
        if(newOffset >= 0)
        {
            lastError = 0;
            break;
        }

        // store the error returned from the i/o operation
        // it will be used to overwrite error returned by the timer expiry
        lastError = ACE_OS::last_error();

        // check if quit request/wait function is installed
        if(!m_WaitFunction)
        {
            break;
        }

        // check if we have already retried specified no. of times
        if(retryCount > m_iVolumeRetries)
        {
            break;
        }

        ++retryCount;

        DebugPrintf(SV_LOG_DEBUG, "Tell Retry %d on volume %s\n", retryCount, m_Name.c_str());
        if((*m_WaitFunction)(m_iVolumeRetryDelay))
        {
            // quit signalled.
            DebugPrintf(SV_LOG_DEBUG, "Got a quit request during Seek retries on volume %s.\n", 
                m_Name.c_str());
            break;
        }

    } while (retryCount < m_iVolumeRetries);

    // restore the error to the one returned by I/O operation
    ACE_OS::last_error(lastError);
    return newOffset;
}

/*
* FUNCTION NAME : Tell
*
* DESCRIPTION : returns the current offset.
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value : current offset.
*
*/
SV_LONGLONG BasicIo::Tell() const
{  

    if (ACE_INVALID_HANDLE == m_File) {
        m_Status = ERR_INVALID_HANDLE;
        m_NativeError = Error::UserToOS(ERR_INVALID_HANDLE);
        return -1;
    }


    ACE_LOFF_T newOffset = (ACE_LOFF_T)0LL;

    int seekFrom = SEEK_CUR;

    if ((newOffset = RetryableTell()) < 0) {
        m_Status = SV_FAILURE;
        m_NativeError = ACE_OS::last_error();
        return -1;
    }

    m_Status = SV_SUCCESS;
    m_NativeError = Error::UserToOS(SV_SUCCESS);
    return (SV_LONGLONG)newOffset;
}

/*
* FUNCTION NAME : Close
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
SV_INT BasicIo::Close()
{  
    /*
    * BUGBUG: check return values.
    */
    if (ACE_INVALID_HANDLE == m_File)
    {
        m_Status = SV_SUCCESS;
        m_NativeError = Error::UserToOS(SV_SUCCESS);
        return 0;
    }

    if (0 != ACE_OS::close(m_File))
    {
        m_Status = SV_FAILURE;
        m_NativeError = ACE_OS::last_error();
        return m_Status;
    }

    m_Status = SV_SUCCESS;
    m_NativeError = Error::UserToOS(SV_SUCCESS);
    m_File = ACE_INVALID_HANDLE;
    return 0;
}

/*
* FUNCTION NAME : SetHandle
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : ACE_HANDLE of the file.
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value : returns the status. (m_Status)
*
*/
SV_INT BasicIo::SetHandle(const ACE_HANDLE& handle)
{
    m_File = handle;
    m_Status = SV_SUCCESS;

    return m_Status;
}

/*
* FUNCTION NAME : EnableRetry
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
void BasicIo::EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay)
{
    m_WaitFunction = fWait;
    m_iVolumeRetries = iRetries;
    m_iVolumeRetryDelay = iRetryDelay;
    m_bRetry = true;
}

/*
* FUNCTION NAME : DisableRetry
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
void BasicIo::DisableRetry()
{
    m_WaitFunction = NULL;
    m_iVolumeRetries = 0;
    m_iVolumeRetryDelay = 0;
    m_bRetry = false;
}

