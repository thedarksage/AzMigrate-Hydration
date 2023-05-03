/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : basicio.h
*
* Description: 
*/

#ifndef BASICIO_H
#define BASICIO_H

#include <stdio.h>
#include <fcntl.h>
#include <string>
#include "ace/config-lite.h"
#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "svtypes.h"

#include "error.h"

#ifndef SV_WINDOWS
#define FILE_SHARE_WRITE  0
#define FILE_SHARE_DELETE 0
#endif // !SV_WINDOWS

typedef bool (*WaitFunction)(int);

class BasicIo {
public:
    typedef int BioOpenMode;
    typedef int BioSeekFrom;        
    /* 
    * constants for open/create
    * combine the following to get the combinations you want
    * make note of required and mutually exclusive OPEN_MODE settings
    */

    /* must specify at least one of BIO_READ, BIO_WRITE, or BIO_READ_WRITE*/
    static BioOpenMode const BioRead;    // read only  
    static BioOpenMode const BioWrite;    // write only
    static BioOpenMode const BioRW;    // read write (convienence)
    static BioOpenMode const BioBinary;    // if not set opens as text
    static BioOpenMode const BioAppend;    // all writes done at end of file

    /*
    * open/creation flags must provide one of the following        
    * note: BioOverwrite and BioTruncate can not be used with any other open/creation flags
    */
    static BioOpenMode const BioOpen;    // open if exists else fail
    static BioOpenMode const BioCreate;    // create if it doesn't exist else fail
    static BioOpenMode const BioOpenCreate;    // open if exists else create        
    static BioOpenMode const BioOverwrite;    // create if not exists, else overwrite existing
    static BioOpenMode const BioTruncate;    // open existing and truncate it

    /*
    * sharing not sure if these names will be useful on other OSes. the semantics might be different to.)
    * on windows it means you will allow subsequent opens this share access
    */
    static BioOpenMode const BioShareDelete;    
    static BioOpenMode const BioShareRead;    
    static BioOpenMode const BioShareWrite;    
    static BioOpenMode const BioShareRW;    
    static BioOpenMode const BioShareAll;    


    // special flags for windows (may be useful on other OSes)
#ifndef SV_WINDOWS 
#ifndef FILE_FLAG_NO_BUFFERING
#define FILE_FLAG_NO_BUFFERING 0
#endif
#endif

    // special flags for windows (may be useful on other OSes)
    static BioOpenMode const BioNoBuffer;    
    static BioOpenMode const BioWriteThrough;
    static BioOpenMode const BioSequentialAccess;

    // direct i/o for unix
    static BioOpenMode const BioDirect;

    /* TODO:
    * need to deal with windows atrributes and unix/linix permissions when creating
    * may need other options as well
    */

    // some (possibly) useful default combinations 
    static BioOpenMode const BioReadExisting;    
    static BioOpenMode const BioWriteNew;    
    static BioOpenMode const BioWriteAlways;    
    static BioOpenMode const BioRWExisitng;    
    static BioOpenMode const BioRWNew;    
    static BioOpenMode const BioRWAlways;    

    // constants for seeking
    static const BioSeekFrom BioBeg;    
    static const BioSeekFrom BioCur;    
    static const BioSeekFrom BioEnd;    

#ifdef SV_WINDOWS
#ifndef SEEK_SET
#define SEEK_SET FILE_BEGIN
#endif

#ifndef SEEK_CUR
#define SEEK_CUR FILE_CURRENT
#endif

#ifndef SEEK_END
#define SEEK_END FILE_END
#endif
#endif

    /*
    * both of these constructors will open the file
    * check LastError to see if they succeeded
    */
    BasicIo();
    BasicIo(char const * name, BioOpenMode mode);
    BasicIo(std::string const & name, BioOpenMode mode);


    // will close the file
    ~BasicIo();        

    /* opens the file. 
    * if the file is open and the name matches the open file returns success
    * if the name does not match the open file name, Open will close the file and 
    * then open using the name and mode. Note, mode is not used as part of the already open check
    * returns SV_SUCCESS on success else the error code defined in error.h
    */
    SV_INT Open(std::string const & name, BioOpenMode mode);
    SV_INT Open(char const * name, BioOpenMode mode);

    BioOpenMode OpenMode(void);

    // returns zero on success else SV_FAILURE
    SV_INT Close(); 

    /* returns number of bytes read/written on success
    * 0 on failure, check LastError (or Eof)
    */
    SV_UINT Read(char* buffer, SV_UINT length);    
    SV_UINT Write(char const* buffer, SV_UINT length);

    /* convenience function so one doesn't have to write a loop to make sure requested bytes are read
    * read until length bytes read, eof, or error
    * returns number of bytes read on success
    * 0 on failure, check LastError (or Eof)
    * note on EOF can return less then the number of bytes requested
    */
    SV_ULONG FullRead(char* buffer, SV_ULONG length); 

    /* returns the new offset on success else  -1 on failure, check LastError*/
    SV_LONGLONG Seek(SV_LONGLONG offset, BioSeekFrom from);

    /* returns the current offset on success else  -1 on failure, check LastError*/
    SV_LONGLONG Tell() const;

    /* ture if end of file else false*/
    bool Eof() const { return (m_Status == ERR_HANDLE_EOF); };

    /*
    * returns the last error,
    * note once set, remains set until a call to Clear or Close
    */
    SV_ULONG LastError() const { return m_NativeError; }

    /* convenience function so one doesn't have to code SV_SUCCESS == BasicIO.LastError() */
    bool Good() const { return ( m_Status == SV_SUCCESS ); }

    /* clears the last error*/
    void ClearError() { m_Status = SV_SUCCESS; m_NativeError = Error::UserToOS(SV_SUCCESS);}

    /* Check to find if the file is open*/
    bool isOpen()  { return (ACE_INVALID_HANDLE != m_File); }

    // Set Last Native Error
    void SetLastError(SV_ULONG liError) { m_NativeError = liError; 
    m_Status = Error::OSToUser(liError); }

    ACE_HANDLE& Handle() { return m_File;}

    /*
    * if the file is already open, it will close the file.
    * Reset error code and set the handle
    * returns SV_SUCCESS on success else the error code
    */
    SV_INT SetHandle(const ACE_HANDLE& handle);

    void EnableRetry(WaitFunction fWait, int iRetries = 2, int iRetryDelay = 2);
    void DisableRetry();

    const std::string& GetName() const { return m_Name; }
protected:

    bool ModeOn(BioOpenMode chkMode, BioOpenMode mode)
   	{
        return (chkMode == (chkMode & mode));
    }  

    ssize_t RetryableWrite(char const * buffer, SV_UINT length);
    ssize_t RetryableRead(char * buffer, SV_UINT length);
    ACE_LOFF_T RetryableSeek(ACE_LOFF_T seekOffset, int seekFrom);
    ACE_LOFF_T RetryableTell() const;

private:
    // for now no copying of BasicIo
    BasicIo(BasicIo const & bio);
    BasicIo& operator=(BasicIo* bio);

    std::string m_Name;  

    SV_UINT m_iVolumeRetries, m_iVolumeRetryDelay;
    WaitFunction m_WaitFunction;
    bool m_bRetry;

    ACE_HANDLE m_File;

    BioOpenMode m_Mode;

    //Emulate Eof on Unix platform
    //bool m_Eof;

    // FIXME:
    // switch to SVERROR
    mutable SV_INT m_Status;  // might have errors returned even in const functions
    mutable SV_INT m_NativeError;
};

#endif // ifndef BASICIO__H
