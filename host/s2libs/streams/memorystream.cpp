/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : memorystream.cpp
*
* Description: 
*/
#include <string>
#include <cassert>

#include "portable.h"
#include "error.h"

#include "memorystream.h"
#include "filestream.h"
#ifdef SV_WINDOWS
#include "atlbase.h"
#endif

#include "cdptargetapply.h"

unsigned long int MemoryStream::m_defaultSize = 0 ;
int MemoryStream::m_strictModeEnabled = -1 ;
std::string MemoryStream::m_installpath ;
bool MemoryStream::m_init = false;
static ACE_Mutex g_lock ;
void MemoryStream::init()
{
	ACE_Guard<ACE_Mutex> guard(g_lock) ;
	if( !m_init )
	{
		LocalConfigurator lconfig ;
		m_strictModeEnabled = lconfig.getS2StrictMode() ;
		m_installpath = lconfig.getInstallPath() ;
		m_defaultSize = 1 * 1024 * 1024 ;
#ifdef SV_WINDOWS
		CRegKey cregkey ;
		DWORD dwResult = cregkey.Open( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\services\\involflt\\Parameters" ) ;
		if( dwResult == ERROR_SUCCESS )
		{
			cregkey.QueryDWORDValue("MaxDataSizePerNonDataModeDirtyBlock", m_defaultSize ) ;
			cregkey.Close() ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to read the Max Data Size Per Non Data Mode Dirty Block Value\n") ;
		}
		m_defaultSize = m_defaultSize * 1024 ; 
		DebugPrintf(SV_LOG_DEBUG, "Max Data Size Per Non Data Mode Dirty Block Value : %lu\n", m_defaultSize) ;
#endif
		m_init = true ;
	}
}

MemoryStream::MemoryStream(TRANSPORT_PROTOCOL protocol,
                             TRANSPORT_CONNECTION_SETTINGS const& transportSettings,
                             const std::string & volumename,
                             const SV_ULONGLONG & source_capacity,
                             const CDP_SETTINGS & settings,
                             DifferentialSync * diffSync)
        : m_bMemoryAllocated(false),
          m_protocol(protocol),
          m_transportSettings(transportSettings),
          m_TgtVolume(volumename),
          m_SrcCapacity(source_capacity),
          m_CDPSettings(settings),
          m_secure(false),
          m_DiffsyncObj(diffSync)
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_ProcessIncomingBuffer[0] = &MemoryStream::AppendToBuffer;
    m_ProcessIncomingBuffer[1] = &MemoryStream::ApplyFullBuffer;

    m_uliBufferTopPos = 0;
    m_uliBufferSize = 0;
    m_pszBuffer = NULL;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME :  Abort
*
* DESCRIPTION : aborts an existing stream
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


int MemoryStream::Abort(char const* pData)
{
    int iStatus = SV_FAILURE;
    iStatus = SV_SUCCESS;
    SetState(STRM_GOOD);
    DebugPrintf(SV_LOG_DEBUG,"SUCCESS: Abort request successful\n");
    return iStatus;
}

/*
* FUNCTION NAME :  Open  
*
* DESCRIPTION :    Opens the memorystream for read / write operations    
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

int MemoryStream::Open(STREAM_MODE Mode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
	init(); 
    m_Mode = Mode;
    if ( !ModeOn(Mode_RW, m_Mode) &&  !ModeOn(Mode_Read, m_Mode) && !ModeOn(Mode_Write, m_Mode) )
    {
        DebugPrintf(SV_LOG_ERROR,"Invalid Open mode. LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
        SetState(STRM_BAD);
        return iStatus;
    }


    if (ModeOn(Mode_Asynchronous, m_Mode)) 
    {
        DebugPrintf(SV_LOG_WARNING,"Currently no support for async data transfers. LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
    }

    m_secure = ModeOn(Mode_Secure, m_Mode);

    try { 
        m_CdpApplyPtr.reset(new (nothrow) CDPTargetApply(m_TgtVolume,m_SrcCapacity,m_CDPSettings,true));
        
        unsigned long int uliSize = m_defaultSize ;
		m_pszBuffer =  (char *)malloc(uliSize);
        if(NULL == m_pszBuffer)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to allocate %lu bytes\n",uliSize);
            SetState(STRM_BAD);
        }
        else
        {
            memset(m_pszBuffer, 0,uliSize);
            m_bMemoryAllocated = true;
            m_uliBufferSize  =  uliSize;
            m_uliBufferTopPos = 0;
            DebugPrintf(SV_LOG_DEBUG,"%lu Bytes of Memory Allocated\n",uliSize);
            iStatus = SV_SUCCESS;
            SetState(STRM_GOOD);
        }
    } catch (std::exception const& e) {
        DebugPrintf(SV_LOG_ERROR,"%s create MemoryStream transport failed: %s\n ", FUNCTION_NAME, e.what());
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    } catch (...) {
        DebugPrintf(SV_LOG_ERROR,"%s create MemoryStream transport failed: unkown error\n ", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }   

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
* FUNCTION NAME :  
*
* DESCRIPTION :    Closes the open transport session and resets the state.
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

int MemoryStream::Close()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    if ( 0 != m_CdpApplyPtr.get() )
    {
        m_CdpApplyPtr.reset(static_cast<CDPTargetApply*>(0));
        this->Clear();
        SetState(STRM_CLEAR);
        iStatus = SV_SUCCESS;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
* FUNCTION NAME :  Rename  
*
* DESCRIPTION :    Given old filename renames it to a new file.
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value :   returns SV_SUCCESS if transport rename succeeds else SV_FAILURE
*
*/

int MemoryStream::Rename(const std::string& sOldFileName,const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;

    if ( !sOldFileName.empty() && !sNewFileName.empty() )
    {
        if ( RenameFile(sOldFileName, sNewFileName) )
        {
            SetState(STRM_GOOD);
            iStatus = SV_SUCCESS;
        }
        else
        {
            SetState(STRM_BAD);
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to rename file.@LINE %d in FILE %s: \n",
                LINE_NO,FILE_NAME);
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to rename file. Invalid name specified. Old Filename: %s. New Filename: %s. @LINE %d in FILE %s\n",sOldFileName.c_str(),sNewFileName.c_str(),LINE_NO,FILE_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
* FUNCTION NAME :  Write 
*
* DESCRIPTION :    Uploads in-memory data to the remote host. The target file must be specified as the absolute path.
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value :   returns SV_SUCCESS if transport upload succeeds else SV_FAILURE
*
*/

int MemoryStream::Write(const char* sData, unsigned long int uliDataLen, const std::string& sDestination, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs, bool bIsFull)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

  
        if ( ModeOn(Mode_Write, m_Mode) )
        {
            if ( !sDestination.empty())
            {
                ProcessIncomingBuffer_t p = m_ProcessIncomingBuffer[bIsFull];
                iStatus = (this->*p)(sDestination, sData,uliDataLen, bMoreData);
                if (iStatus != SV_SUCCESS)
                {
                    DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to apply  data.@LINE %d in FILE %s: \n",
                        LINE_NO,FILE_NAME);
                    SetState(STRM_BAD);
                }
                else
                {
                    SetState(STRM_GOOD);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to write data. Invalid destination @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
                SetState(STRM_BAD);
                iStatus = SV_FAILURE;
            }
        }                        
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to write data. Stream not opened in Write mode. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
        }
   
   
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
* FUNCTION NAME :  Write  
*
* DESCRIPTION :    Uploads the local file to the remote host. The local and remote file names must be specified as the absolute path
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value :   returns SV_SUCCESS if transport upload succeeds else SV_FAILURE
*
*/

int MemoryStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths, bool createDirs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    if ( ModeOn(Mode_Write, m_Mode) )
    {
        if ( !localFile.empty() && !targetFile.empty() && !renameFile.empty())
        {
            DebugPrintf(SV_LOG_DEBUG,"renameFile = %s\n",renameFile.c_str());
			std::string renamedFileName = GetFileNameToApply(renameFile);
            if(!renamedFileName.empty())
            {
                iStatus = AppendToBuffer( renamedFileName, localFile ) ; 
				if (iStatus != SV_SUCCESS)
                {
                    DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to apply data file mode file @LINE %d in FILE %s: \n",
                        LINE_NO,FILE_NAME);
                    SetState(STRM_BAD);
                }
                else
                {
                    SetState(STRM_GOOD);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to write data. Invalid file name specified. Source File: %s. Target File: %s. @LINE %d in FILE %s\n",localFile.c_str(),targetFile.c_str(),LINE_NO,FILE_NAME);
                SetState(STRM_BAD);
                iStatus = SV_FAILURE;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to write data. Invalid file name specified. Source File: %s. Target File: %s. @LINE %d in FILE %s\n",localFile.c_str(),targetFile.c_str(),LINE_NO,FILE_NAME);
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
        }
    }               
    else
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to write data. Stream not opened in Write mode. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

int MemoryStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, bool createDirs)
{
    return Write(localFile, targetFile, compressMode, std::string(), std::string(), createDirs);
}

/*
* FUNCTION NAME :  DeleteFile  
*
* DESCRIPTION :    Deletes the remote file.  The remote file name must be specified as the absolute path 
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value :   returns SV_SUCCESS if transport delete succeeds else SV_FAILURE
*
*/

int MemoryStream::DeleteFile(std::string const& names, std::string const& fileSpec, int mode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;
    SetState(STRM_GOOD);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iStatus;

}


int MemoryStream::DeleteFile(std::string const& names, int mode)
{
    return this->DeleteFile(names, std::string(), mode);
}

/*
* FUNCTION NAME :  DeleteFiles
*
* DESCRIPTION :    Deletes the list of files specified. All filenames must be the absolute path
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value :   returns SV_SUCCESS if transport delete succeeds else SV_FAILURE
*
*/

int MemoryStream::DeleteFiles(const ListString& DeleteFileList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;
    SetState(STRM_GOOD);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}           

/* TODO: Remove this ?
 * keep this for use later if required */
bool MemoryStream::IsMemoryAllocated(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return m_bMemoryAllocated;

}
int MemoryStream::AllocateMemoryForStream(unsigned long int uliSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    if(IsMemoryAvailable(uliSize))
    {
        m_uliBufferTopPos = 0;
    }
    else
    {
		DebugPrintf(SV_LOG_ERROR, "Not error.. reallocating memory %ld\n", uliSize) ;
        if(ReAllocateMemoryForStream(uliSize) != SV_SUCCESS)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to re-allocate %lu bytes\n",uliSize);
            iStatus = SV_FAILURE;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;

}

bool MemoryStream::IsMemoryAvailable(unsigned long int uliSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bAvailable = false;
    DebugPrintf(SV_LOG_DEBUG,"Current Buffer Size = %lu, Requried Buffer Size = %lu\n",m_uliBufferSize,uliSize);

    if(m_uliBufferSize >= uliSize)
    {
        bAvailable = true;
    }
    else if(m_uliBufferSize < uliSize)
    {
        bAvailable = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bAvailable;
}

int MemoryStream::ReAllocateMemoryForStream(unsigned long int uliSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;
    char *p = (char*) realloc(m_pszBuffer, uliSize);
    if(NULL == p)
    {
        iStatus = SV_FAILURE;
		m_bMemoryAllocated = false ;
    }
    else
    {
		m_bMemoryAllocated = true ;
        m_pszBuffer = p;
        memset(m_pszBuffer, 0,uliSize);
        m_uliBufferSize = uliSize;
        m_uliBufferTopPos = 0;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

int MemoryStream::AppendToBuffer( std::string const& remoteName, std::string const& localFile )
{        
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    memset( m_pszBuffer, 0, m_uliBufferSize ) ;
    FileStream localFileObject( localFile) ; 
    localFileObject.Open( GenericStream::Mode_ReadExisting ) ;
    if( !localFileObject.Good() )
    {
        DebugPrintf( SV_LOG_ERROR, "The file %s does not exist. Can not continue further\n", localFile.c_str() ) ;
        return iStatus ;
    }
    SV_UINT localFileSize = (SV_UINT)localFileObject.Seek( 0, POS_END ) ;
    AllocateMemoryForStream( localFileSize ) ;
    localFileObject.Seek( 0, POS_BEGIN ) ;

    /* TODO: This is a bug; use full read instead of read */
	if( IsMemoryAllocated() )
	{
	    SV_ULONG bytesRead = localFileObject.Read( m_pszBuffer, localFileSize ) ;
	    localFileObject.Close() ;
	    m_uliBufferTopPos = localFileSize ;
	    if (bytesRead !=  localFileSize || ApplyChanges("",remoteName,m_pszBuffer,m_uliBufferTopPos) != SV_SUCCESS) 
	    {
	        DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to apply data.@LINE %d in FILE %s  BytesRead: %d TotalSize: %d FileName: %s\n",
	        LINE_NO,FILE_NAME, bytesRead, localFileSize, localFile.c_str());
	        SetState(STRM_BAD);
	        iStatus = SV_FAILURE;     
	    }
	    else
	    {
	        SetState(STRM_GOOD);
	        iStatus = SV_SUCCESS;
	        DebugPrintf(SV_LOG_DEBUG,"Success: apply data.@LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
	    }
	}
      DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus ;
}

int MemoryStream::ApplyFullBuffer(std::string const& remoteName,const char *pszData,unsigned long int uliSize,bool bMoreData)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"Size of data to apply: %lu\n",uliSize);
	int iStatus;

	std::string fileNameToApply  = GetFileNameToApply(remoteName);
	if (!fileNameToApply.empty())
	{
		iStatus = ApplyChanges("",fileNameToApply,pszData,uliSize);
		if (iStatus != SV_SUCCESS)
		{
			DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to apply data.@LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
			SetState(STRM_BAD);
		}
		else
		{
			SetState(STRM_GOOD);
			DebugPrintf(SV_LOG_DEBUG,"Success: apply data.@LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
		}
	}
	else
	{
	    DebugPrintf(SV_LOG_ERROR, "FAILED: Failed to write data. Invalid file name %s specified in memory stream apply\n", 
                                  remoteName.c_str());
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

int MemoryStream::AppendToBuffer(std::string const& remoteName,const char *pszData,unsigned long int uliSize,bool bMoreData)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"Size of buffer = %lu Buffer top pos = %lu \n",m_uliBufferSize,m_uliBufferTopPos);
    DebugPrintf(SV_LOG_DEBUG,"Size of data to append to buffer is %lu\n",uliSize);
    int iStatus = SV_FAILURE;
    if( IsMemoryAllocated() )
    {        
		inm_memcpy_s(m_pszBuffer + m_uliBufferTopPos, m_uliBufferSize - m_uliBufferTopPos, pszData, uliSize);
        m_uliBufferTopPos += uliSize; 
        DebugPrintf(SV_LOG_DEBUG,"%lu of Bytes appended to buffer.Current buffer top is %lu\n",uliSize,m_uliBufferTopPos);
        if(!bMoreData)
        {
            std::string fileNameToApply = GetFileNameToApply(remoteName);
			if (!fileNameToApply.empty())
			{
				if (ApplyChanges("",fileNameToApply,m_pszBuffer,m_uliBufferTopPos) != SV_SUCCESS) 
				{
					DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to apply data.@LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
					SetState(STRM_BAD);
					iStatus = SV_FAILURE;
				}
				else
				{
					SetState(STRM_GOOD);
					iStatus = SV_SUCCESS;
					DebugPrintf(SV_LOG_DEBUG,"Success: apply data.@LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "FAILED: Failed to write data. Invalid file name %s specified in memory stream apply\n", 
                                          remoteName.c_str());
				SetState(STRM_BAD);
				iStatus = SV_FAILURE;
			}
        }
        else
        {
            iStatus = SV_SUCCESS;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Enough memory not available.. Would be retried after sometime\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

int MemoryStream::ApplyChanges(const std::string& remoteName, const std::string& localName,const char* pszData,size_t uliSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_LONGLONG totalDataApplied = 0;
    int iStatus = SV_SUCCESS;
	int retries = 0 ;
	bool bretry = true ;
    DebugPrintf(SV_LOG_DEBUG,"File Name to apply is %s\n",localName.c_str());
    do
	{
		if(!m_CdpApplyPtr->Applychanges(localName,totalDataApplied, const_cast<char*>(pszData),uliSize,m_DiffsyncObj))
		{
			DebugPrintf( SV_LOG_ERROR, "Failed to apply data\n" ) ;
			retries++;
			if( retries > 2 )
			{
				bretry = false ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Retrying the apply for %s\n", localName.c_str()) ;
			}
			iStatus = SV_FAILURE;
	       
			if( m_strictModeEnabled ) 
			{
				std::string fullPath = m_installpath ;
				fullPath += "\\tmp\\" + localName  ;
				 File fileTobApplied( fullPath) ;
				 fileTobApplied.Open( BasicIo::BioOpenCreate | BasicIo::BioWrite ) ;
				if( fileTobApplied.Write( pszData, uliSize ) != uliSize )
				{
					DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to create file using dirtyblock.@LINE %d in FILE %s \n",LINE_NO,FILE_NAME);
					SetState(STRM_BAD);
					iStatus = SV_FAILURE;
				}
				fileTobApplied.Close() ;
				/* while( 1 ) { ACE_OS::sleep(1) ; } */
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"%lu Bytes applied successfully\n",uliSize);
			iStatus = SV_SUCCESS;
			SetState(STRM_GOOD);
			bretry = false ;
		}
	} while( bretry ) ;
    m_uliBufferTopPos = 0;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}


void MemoryStream::SetSizeOfStream(unsigned long int uliSize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AllocateMemoryForStream(uliSize);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool MemoryStream::RenameFile(std::string const& oldName,  ///< name of the file to be renamed
                                      std::string const& newName)
 {
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, " Old File Name is %s\n",oldName.c_str());
    DebugPrintf(SV_LOG_DEBUG, " New File Name is %s\n",newName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "memory allocated was: %lu\n", m_uliBufferSize);
    m_FileName = newName;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
     return true;
 }
MemoryStream::~MemoryStream()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	if(m_pszBuffer != NULL)
	{
		free(m_pszBuffer);
	}
	m_pszBuffer = NULL;
	m_bMemoryAllocated = false ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool MemoryStream::heartbeat( bool forceSend )
{
    return true ;
}

std::string MemoryStream::GetFileNameToApply(const std::string &difffile)
{
	std::string filenametoapply;

	size_t pos = difffile.find("completed_diff_");
    if(pos != std::string::npos)
    {
        std::string tempstr = difffile.substr(pos);
        filenametoapply = "completed_ediff" + tempstr;
		DebugPrintf(SV_LOG_DEBUG, "File name to apply is %s\n", filenametoapply.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Invalid diff file name %s supplied to memory stream apply\n", difffile.c_str());
	}
 
	return filenametoapply;
}
