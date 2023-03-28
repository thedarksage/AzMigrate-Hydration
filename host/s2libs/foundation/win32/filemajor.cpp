/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : file_port.cpp
 *
 * Description: Windows specific implementation of file.cpp
 */


#include <windows.h>
#include <winioctl.h>
#include "hostagenthelpers.h"
#include "portablehelpersmajor.h"

#include <string>
#include "ace/OS.h"
#include "entity.h"

#include "globs.h"

#include "portable.h"
#include "error.h"
#include "synchronize.h"
#include "globs.h"

#include "genericfile.h"
#include "file.h"


#include "ace/Default_Constants.h"
#include "ace/ACE.h"

/*
 * FUNCTION NAME : Init
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
int File::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	
	m_bInitialized = true;
    std::string sFile = GetName();
    if ( IsExisting(sFile ) )
	{
     
        ACE_stat fileStat;

        // stat does not work if the file name is in UNC format ie. \\Z:\
        // it should only be Z:\

        ToStandardFileName(sFile);
		// PR#10815: Long Path support
		if ( sv_stat(getLongPathName(sFile.c_str()).c_str(),&fileStat) >= 0 )
        {

            m_bInitialized = true;
            m_ullCreationTime = fileStat.st_ctime;


            if ( (fileStat.st_mode & S_IREAD) && !(fileStat.st_mode & S_IWRITE) && !(fileStat.st_mode & S_IEXEC))
            {
                m_uliAttribute |= File_ReadOnly;
            }
            else 
            {
                if ( fileStat.st_mode & S_IREAD )
                {
                    m_uliAttribute |= File_Readable;
                }

                if ( fileStat.st_mode & S_IWRITE )
                {
                    m_uliAttribute |= File_Writable;
                }

                if ( fileStat.st_mode & S_IEXEC )
                {
                    m_uliAttribute |= File_Executable;
                }
            }

            if ( fileStat.st_mode & S_IFDIR )
            {
                m_uliAttribute |= File_Directory;
            }
            if ( fileStat.st_mode & _S_IFIFO )
            {
                m_uliAttribute |= File_Pipe;
            }

            if ( fileStat.st_mode & _S_IFCHR )
            {
			    m_uliAttribute |= File_Char;
            }          
            if ( fileStat.st_mode & S_IFREG )
            {
			    m_uliAttribute |= File_Normal;
            }
        }
        else
	    {

            m_bInitialized = false;
		    DebugPrintf( SV_LOG_WARNING,
						 "Unable to determine attributes of file %s. %s. @LINE %d in FILE %s.\n",
						 GetName().c_str(),
						 Error::Msg().c_str(),
						 LINE_NO,
						 FILE_NAME);
	    }

	}
    else
    {
		DebugPrintf( SV_LOG_WARNING,
					 "WARNING: File %s does not exist. Is it a new file? or specify absolute path correctly %s. @LINE %d in FILE %s\n",
					 GetName().c_str(),
					 Error::Msg().c_str(),
					 LINE_NO,
					 FILE_NAME);
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return ( m_bInitialized==true ? 1:0);
}

/*
 * FUNCTION NAME : SetEndOfFile
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
bool File::SetEndOfFile()
 {
     DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
     if (ACE_INVALID_HANDLE == GetHandle())
	 {
         return false;
     }
	 //TODO: Check if ftruncate supports files > 2GB/4GB size
     if ( false == ::SetEndOfFile(GetHandle()))
     {
		DebugPrintf(SV_LOG_DEBUG,"ACE_OS::ftruncate() FAILED\n");
		return false;
     }
     return true;
 }

/*
 * FUNCTION NAME : GetSizeOnDisk
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
 SV_ULONGLONG File::GetSizeOnDisk(const std::string & fName)
{
    SV_ULONGLONG ullSizeOnDisk = 0;

    ULARGE_INTEGER ulFileSize = { 0 };
	// PR#10815: Long Path support
    ulFileSize.LowPart = SVGetCompressedFileSize(fName.c_str(), &ulFileSize.HighPart);
    if( INVALID_FILE_SIZE == ulFileSize.LowPart )
    {
        if( NO_ERROR != Error::Code() )
        {
            ulFileSize.QuadPart = 0;
        }
    }
    ullSizeOnDisk = ulFileSize.QuadPart;
	return ullSizeOnDisk;
}


/*
 * FUNCTION NAME : GetNumberOfBytes
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
SV_ULONGLONG File::GetNumberOfBytes(const std::string & fName)
{
    SV_ULONGLONG s = 0;

    HANDLE hFile = SVCreateFile(fName.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if(INVALID_HANDLE_VALUE == hFile)
    {
        DWORD err = GetLastError();
        std::stringstream sserr;
        sserr << err;
        DebugPrintf(SV_LOG_DEBUG, "open failed in GetNumberOfBytes with error %s for file %s\n", sserr.str().c_str(), fName.c_str());
    }
    else
    {
        LARGE_INTEGER li;
        li.QuadPart = 0;

        if (GetFileSizeEx(hFile, &li))
        {
            s = li.QuadPart;
            DebugPrintf(SV_LOG_DEBUG, "size of file %s is " ULLSPEC "\n", fName.c_str(), s);
        }
        else
        {
            DWORD err = GetLastError();
            std::stringstream sserr;
            sserr << err;
            DebugPrintf(SV_LOG_ERROR, "GetFileSizeEx failed with error %s for file %s\n", sserr.str().c_str(), fName.c_str());
        }

        if (!CloseHandle(hFile))
        {
            DWORD err = GetLastError();
            std::stringstream sserr;
            sserr << err;
            DebugPrintf(SV_LOG_ERROR, "CloseHandle failed with error %s for file %s in function GetNumberOfBytes\n", sserr.str().c_str(), fName.c_str());
        }
    }

    return s;
}

/*
 * FUNCTION NAME : SetSparse
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
bool File::SetSparse()
{
	bool rv = false;

	do 
	{
 		SV_ULONG liError = this -> LastError();
		
		// OK if at EOF, but not if there is any other error
		if (ERROR_SUCCESS !=  liError && ERROR_HANDLE_EOF != liError)
	   	{
			break;
		}

		HANDLE & handle = GetHandle();
		if (INVALID_HANDLE_VALUE == handle)
	   	{
			this -> SetLastError(ERROR_INVALID_HANDLE);
			break;
		}

		DWORD bytesReturned;
		if(!DeviceIoControl(handle, FSCTL_SET_SPARSE, 0,0,0,0, &bytesReturned,0))
		{
			break;
		}
		m_uliAttribute |= File_Sparse;
		rv = true;
	} while (false);

	return rv ;
}

/*
 * FUNCTION NAME : PreAllocate
 *
 * DESCRIPTION : Allocates space on the secondary memory
 *
 * INPUT PARAMETERS : size in bytes to allocate
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 * return value : true if success false otherwise
 *
 */
bool File::PreAllocate(const SV_ULONGLONG &size)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	Seek(size, BasicIo::BioBeg);
	SetEndOfFile();

	FILE_ZERO_DATA_INFORMATION input;
	input.FileOffset.QuadPart = 0;
	DWORD bytesReturned =0;
	input.BeyondFinalZero.QuadPart = (size + 1) ;
	if(!DeviceIoControl(GetHandle(), FSCTL_SET_ZERO_DATA, &input,sizeof(FILE_ZERO_DATA_INFORMATION),0,0,&bytesReturned,0))
	{
		DebugPrintf(SV_LOG_ERROR, "FSCTL_SET_ZERO_DATA failed for %s, Error code: %d\n",
			GetName().c_str(), GetLastError());
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}
