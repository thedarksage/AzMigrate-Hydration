/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : file.cpp
*
* Description: 
*/

/*********************************************************************************
*** Note: Please write any platform specific code in ${platform}/file_port.cpp ***
*********************************************************************************/

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

#include "fileclusterinformerimp.h"

Lockable File::m_ExistLock;

/*
* FUNCTION NAME : File
*
* DESCRIPTION : Default Constructor
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
File::File():	m_uliAttribute(0), m_pFileClusterInformer(0)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_ullCreationTime = 0;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


/*
* FUNCTION NAME : ~File
*
* DESCRIPTION : Destructor
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
File::~File()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);	

    if (m_pFileClusterInformer)
    {
        delete m_pFileClusterInformer;
        m_pFileClusterInformer = 0;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : File
*
* DESCRIPTION : Copy Constructor
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
File::File(const File& file):	m_uliAttribute(0), m_pFileClusterInformer(0),
GenericFile(file)
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_ullCreationTime = 0;

    Init();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

/*
* FUNCTION NAME : File
*
* DESCRIPTION : Constructor
*
* INPUT PARAMETERS : File name as pointer to char.
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
File::File(const char* szName, bool check_for_existence):	m_uliAttribute(0), m_pFileClusterInformer(0),
GenericFile(szName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_ullCreationTime = 0;
    if(check_for_existence)
    {
        Init();
    } else
    {
        m_bInitialized = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : File
*
* DESCRIPTION : Constructor
*
* INPUT PARAMETERS : File name as string reference.
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
File::File(const std::string& sName, bool check_for_existence):	m_uliAttribute(0), m_pFileClusterInformer(0),
GenericFile(sName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_ullCreationTime = 0;

    if(check_for_existence)
    {
        Init();
    } else
    {
        m_bInitialized = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}
/*
* FUNCTION NAME : GetType
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
unsigned long int File::GetType()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);	
    return m_uliAttribute;
}

/*
* FUNCTION NAME : GetCreationTime
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
SV_ULONGLONG File::GetCreationTime()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return m_ullCreationTime;
}

/*
* FUNCTION NAME : GetLastAccessTime
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
SV_ULONGLONG File::GetLastAccessTime()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_ULONGLONG llLastAccessTime = 0;

    ACE_stat fileStat;
    // PR#10815: Long Path support
    if ( sv_stat(getLongPathName(GetName().c_str()).c_str(),&fileStat) >= 0 )
    {
        llLastAccessTime = fileStat.st_atime;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return llLastAccessTime;
}

/*
* FUNCTION NAME : GetLastModifiedTime
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
SV_ULONGLONG File::GetLastModifiedTime()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SV_ULONGLONG llLastModifiedTime = 0;

    ACE_stat fileStat;
    // PR#10815: Long Path support
    if ( sv_stat(getLongPathName(GetName().c_str()).c_str(),&fileStat) >= 0 )
    {
        llLastModifiedTime = fileStat.st_atime;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return llLastModifiedTime;
}

/*
* FUNCTION NAME : IsExisting
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
bool File::IsExisting(const std::string& sName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    AutoLock ExistGuard(m_ExistLock);
    bool bExist = false;
    ACE_HANDLE hHandle = ACE_INVALID_HANDLE;

    unsigned long int uliShareMode = 0;
    std::string sFile = sName;

    FormatVolumeNameToGuid(sFile);

	SV_ULONG access = O_RDONLY;
#ifdef SV_WINDOWS
	SV_ULONG flags = FILE_ATTRIBUTE_NORMAL;
	if (IsDirectoryExisting(sFile))
	{
		flags |= FILE_FLAG_BACKUP_SEMANTICS;
		access |= flags;
	}
#endif

    // PR#10815: Long Path support
    hHandle = ACE_OS::open(getLongPathName(sFile.c_str()).c_str(),access);

    bExist = (ACE_INVALID_HANDLE == hHandle) ? false : true;
    if ( (ACE_INVALID_HANDLE != hHandle) && ( bExist ) )
    {
        ACE_OS::close(hHandle);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bExist;
}

/*
* FUNCTION NAME : GetSize
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
SV_LONGLONG File::GetSize()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SV_LONGLONG llSize = 0;
	ACE_HANDLE& hHandle = GetHandle();
	if ( ACE_INVALID_HANDLE == hHandle)
	{
		// PR#10815: Long Path support
		if ( ACE_INVALID_HANDLE == (hHandle = ACE_OS::open(getLongPathName(GetName().c_str()).c_str(),O_RDWR) ) )
		{
			DebugPrintf( SV_LOG_ERROR,
				"FAILED: Failed to determine size of file %s. %s. @ LINE %d in FILE %s.",
				GetName().c_str(),
				Error::Msg().c_str(),
				LINE_NO,
				FILE_NAME);
			return 0;
		}
		llSize = ACE_OS::filesize(hHandle);
		ACE_OS::close(hHandle);
		hHandle = ACE_INVALID_HANDLE;

	} else {
		llSize = ACE_OS::filesize(hHandle);
	}

	if ( llSize < 0 )
	{
		DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to determine size of file %s. %s. @ LINE %d in FILE %s. ",GetName().c_str(),Error::Msg().c_str(),LINE_NO,FILE_NAME );
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return llSize;
}


/*
* FUNCTION NAME : IsSystemFile
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
int File::IsSystemFile()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_uliAttribute &= File_System); 
}

/*
* FUNCTION NAME : IsDirectory
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
int File::IsDirectory()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_uliAttribute &= File_Directory);
}

/*
* FUNCTION NAME : IsReadOnly
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
int File::IsReadOnly()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_uliAttribute &= File_ReadOnly);
}

/*
* FUNCTION NAME : CanRead
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
int File::CanRead()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_uliAttribute &= File_Readable);
}

/*
* FUNCTION NAME : CanWrite
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
int File::CanWrite()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_uliAttribute &= File_Writable);
}

/*
* FUNCTION NAME : CanExecute
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
int File::CanExecute()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return (m_uliAttribute &= File_Executable);
}

/*
* FUNCTION NAME : FlushFileBuffers
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
bool File::FlushFileBuffers()
{

    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do 
    {
        if (ACE_INVALID_HANDLE == GetHandle()) {
            break;
        }

        if (ACE_OS::fsync(GetHandle()) != 0)
        {
            DebugPrintf(SV_LOG_DEBUG,"ACE_OS::fsync() FAILED\n");
            break;
        }

        rv = true;
    } while (false);

    return rv;
}

/*
* FUNCTION NAME : GetBaseName
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : 
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : returns base name of the file as a std::string
*
*/
std::string File::GetBaseName() const
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string basename = basename_r(GetName().c_str());
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return basename;
}


bool File::GetFilesystemClusters(const bool &recurse, FileClusterInformer::FileClusterRanges_t &clusterranges)
{
    bool brval = false;

    if (m_pFileClusterInformer)
    {
		/* TODO: handle recurse */
        brval = m_pFileClusterInformer->GetRanges(clusterranges);
        if (brval)
        {
			if (recurse && IsDirectory())
			{
				ACE_DIR *dirp = sv_opendir(GetName().c_str());

				if (dirp)
				{
					struct ACE_DIRENT *dirent = NULL;
					std::string insidefile;
					std::string insidepath;
					/* invalid names on windows also */
					const std::string CURRENTDIR = ".";
					const std::string PARENTDIR = "..";

					for (dirent = ACE_OS::readdir(dirp); dirent != NULL; dirent = ACE_OS::readdir(dirp))
					{
						insidefile = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
						if ((insidefile == CURRENTDIR) || (insidefile == PARENTDIR))
						{
							continue;
						}
						insidepath = GetName()+ACE_DIRECTORY_SEPARATOR_CHAR_A+insidefile;

						File f(insidepath);
						f.Open(BasicIo::BioReadExisting | BasicIo::BioShareAll);
						if (!f.Good())
						{
							DebugPrintf(SV_LOG_ERROR, "File %s, is not present with error %lu\n", 
								insidepath.c_str(), f.LastError());
							continue;
						}

						if (!f.SetFilesystemCapabilities(m_sFileSystem))
						{
							DebugPrintf(SV_LOG_ERROR, "For file %s, filesystem %s, could not set capabilities\n",
								insidepath.c_str(), m_sFileSystem.c_str());
							continue;
						}

						if (!f.GetFilesystemClusters(recurse, clusterranges))
						{
							DebugPrintf(SV_LOG_ERROR, "For file %s, filesystem %s, could not get file system clusters\n",
								insidepath.c_str(), m_sFileSystem.c_str());
							continue;
						}
					}
					ACE_OS::closedir(dirp);
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "opendir failed, path = %s, errno = %d\n", GetName().c_str(), ACE_OS::last_error()); 
				}
			}

			/* TODO: not printing for now since in case of directory, redundant printing
			happens as we go recursively in
            DebugPrintf(SV_LOG_DEBUG, "Following cluster ranges detected for file %s\n", GetName().c_str());
            clusterranges.Print();
			*/
        }
        else
        {
			/* TODO: for some files like the ones generated by generatetesttree, always
			 * FSCTL_GET_RETRIEVAL_POINTERS fail with EOF; should that be recoreded as error ? 
			 * EOF comes when asked VCN is greater than end of file; are above files sparse ? */
            DebugPrintf(SV_LOG_ERROR, "could not get cluster ranges for file %s, with error %s\n", GetName().c_str(), 
                                      m_pFileClusterInformer->GetErrorMessage().c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "file cluster informer not instantiated but clusters asked for file %s\n", GetName().c_str());
    }
   
    return brval;
}


bool File::SetFilesystemCapabilities(const std::string &filesystem)
{
    bool brval = true;

    if (0 == m_pFileClusterInformer)
    { 
        m_sFileSystem = filesystem;
        m_pFileClusterInformer = new (std::nothrow) FileClusterInformerImp();
        if (m_pFileClusterInformer)
        {
            FileClusterInformer::FileDetails_t fd(GetHandle(), m_sFileSystem);
            brval = m_pFileClusterInformer->Init(fd);
            if (false == brval)
            {
                DebugPrintf(SV_LOG_ERROR, "could not initialize file cluster informer for file %s\n", GetName().c_str());
            }
        }
        else
        {
            brval = false;
            DebugPrintf(SV_LOG_ERROR, "No memory available to instantiate file cluster informer for file %s\n", GetName().c_str());
        }
    }

    return brval;
}
