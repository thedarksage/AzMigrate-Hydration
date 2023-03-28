/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : file_port.cpp
 *
 * Description: Unix specific implementation of file.cpp
 */


#include <string>
#include "ace/OS.h"
#include "entity.h"

#include "globs.h"

#include "portablehelpers.h"
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
     if (ftruncate(GetHandle(), Tell()) != 0)
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
    ACE_stat s;
	// PR#10815: Long Path support
    ullSizeOnDisk = ( sv_stat( getLongPathName(fName.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;

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
    return GetSizeOnDisk(fName);
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
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;

	//
	// was hitting this assert for media retention pair. So temporarily commented and returning true as before
	//
	//assert( !"need to implemnt the linux version of this function." );
	do 
	{
		DebugPrintf(SV_LOG_DEBUG,"need to implement the linux version of SetSparse() function\n");
		rv = true;
	} while (false);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv ;
}

