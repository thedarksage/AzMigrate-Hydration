//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : unixvolumemonitor.cpp
// 
// Description: 
// 

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <dirent.h>
#include <ace/OS.h>
#include <boost/shared_array.hpp>

#include "volumemonitormajor.h"
#include "portable.h"
#include "file.h"
#include "host.h"

#define MAX_SIZE 40960
#define MAX_READ 400

using namespace std;

VolumeMonitorTask::VolumeMonitorTask( ACE_Thread_Manager* threadManager )
: ACE_Task<ACE_MT_SYNCH>(threadManager)
{
	// Filelist need to be monitored 	
	if(! GetFileListToMonitor(m_fileinfo))
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED VolumeMonitorTask::VolumeMonitorTask GetFileListToMonitor() \n");			
	}		
}

int VolumeMonitorTask::open(void *args)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return this->activate(THR_BOUND);
}


int VolumeMonitorTask::close(u_long flags)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0;
	
}


int VolumeMonitorTask::svc()
{	
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);	

        MaskOffSIGCLDForThisThread();
		StartPolling();
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return 0;
}


void VolumeMonitorTask::StartPolling()
{
	struct stat statbuf;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	while(true)
	{
		// test whether thread is in cancelled state or not.
		// if it is in cancelled state just clean up and exit that thread
		if(this->thr_mgr()->testcancel(this->thr_mgr()->thr_self()))
		{
			DebugPrintf(SV_LOG_DEBUG,"Volume Monitoring thread received cancel request \n");
			CleanUp();
			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
			ACE_Thread::exit();
		}		

		vector <FileInformation>::iterator iter = m_fileinfo.begin();
		for(iter; iter < m_fileinfo.end(); iter++)
		{			
			if((*iter).isModified())
			{
				DebugPrintf( SV_LOG_DEBUG, "VolumeMonitorTask::StartPolling modified file is %s \n", (*iter).FileName().c_str());
				setVolumeMonitorSignal();
			}
		}
		sleep(10);
	}
}


sigslot::signal1<void*>& VolumeMonitorTask::getVolumeMonitorSignal() 
{ 
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return m_VolumeMonitorSignal;
	
}


void VolumeMonitorTask::setVolumeMonitorSignal()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	AutoGuard lock(m_lock);	
	m_VolumeMonitorSignal(NULL);
	lock.release();
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void VolumeMonitorTask::CleanUp()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_VolumeMonitorSignal.disconnect_all();	

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


FileInformation::FileInformation(string filename,bool CheckContents)
{
    m_FileName = filename;
    m_removed = false;
    struct stat statbuf;
    if( -1 == stat(m_FileName.c_str(),&statbuf))
    {
	/* For debugging */
	/* sleep(100); */
        DebugPrintf( SV_LOG_ERROR, "FAILED: FileInformation::IsContentModified() getting stats for %s \n", m_FileName.c_str());
        return; // force a register since we don't real know
    }

    m_LastReadTime.tv_sec = statbuf.st_mtime;
    IsContentModified(); // useing to load up current contents 
    m_CheckContents = CheckContents;
};

bool FileInformation::isModified()
{
    bool modified = false;
   
    if(m_removed)
      return false;
 
    struct stat statbuf;
    if( stat(m_FileName.c_str(),&statbuf) != -1 )
    {
        if(statbuf.st_mtime > m_LastReadTime.tv_sec)
        {
            m_LastReadTime.tv_sec = statbuf.st_mtime;

            if(!m_CheckContents)
            {
                modified = true;
            } else if(IsContentModified())
            {
                modified = true;
            }			

        }
    }			
    else
    {
       if(ENOENT == errno)
       { 
         m_removed = true;
         modified = true;
       } else
       {
           DebugPrintf( SV_LOG_ERROR, "FAILED: getting stats for %s \n", m_FileName.c_str());		
       }
    }
	
    return modified;
}

/*
  checks for checkcontents flag
  if not set 
     No need to check contents
  else
     Read the file 
  if content modified 
     return true;
  else
     return false;
*/
bool FileInformation::IsContentModified()
{
    // Read File Content in currentFileContent 
    string currentfilecontent;
    if( ! ReadFileContent(currentfilecontent))
    {
        DebugPrintf( SV_LOG_ERROR, "FAILED: IsContentModified ReadFileContent() for file %s \n", m_FileName.c_str() );
        return true; // force a register since we don't real know
    }

    if (currentfilecontent == m_Data)
    {		
        return false;
    }

    // Temporary code for testing 
    if(m_Data.size() != 0 )
        DebugPrintf( SV_LOG_DEBUG, "Current contents are %s \n",&m_Data[0]);		
    DebugPrintf( SV_LOG_DEBUG, "Modified Contents are %s \n",& currentfilecontent[0] );		

    m_Data = currentfilecontent;
    return true;
}

/*
	This function is useful when size of the file is not known 
*/

bool FileInformation::ReadFileContent(string &filecontent)
{
     File file(m_FileName);

    file.Open(BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary);

    if(!file.Good())
    {
        return false;
    }

    SV_ULONG bytesRead;
    
    std::vector<char> buffer(MAX_READ);

    filecontent.clear();
    
    while(!file.Eof() && file.Good())
    {
        memset(&buffer[0],0,MAX_READ);
        bytesRead = file.FullRead(&buffer[0],MAX_READ);
        if (bytesRead > 0) {
            filecontent.append(&buffer[0], bytesRead);
        }
    }
    
    if(file.Eof() )
    {
        file.Close();
        return true;	
    }

    if(!file.Good() )
    {
        DebugPrintf(SV_LOG_ERROR,"FileInformation::ReadFileContent failed for %s with Errno = %lu\n",m_FileName.c_str(),file.LastError());
        return false;
    }
    return true;
}


void VolumeMonitorTask::GetLvmFilesToMonitor(std::vector<FileInformation>& fileinfo)
{
    std::ifstream lvmConf("/etc/lvm/lvm.conf"); // TODO: default location for linux what if it gets moved?
                                                // maybe the installer could check for this file and add it
                                                // to the configuration if found?

    if (!lvmConf.good()) {
        // for now don't worry as this may not be on all systems
        return;
    }

    char* tag = 0;
    std::vector<char> buffer(255); // TODO: what if an entry in the lvm.conf is greater then 255 bytes?
    while (!lvmConf.eof()) {
        lvmConf.getline(&buffer[0], buffer.size());
        if (!lvmConf.good()) {
            break;
        }
        tag = strstr(&buffer[0], "backup_dir");
        if (0 != tag) {
            char* value = strchr(tag, '=');
            if (0 == value) {
                break;
            }
            ++value;
            // remove any leading/trailing spaces and double-quotes
            std::string lvmDir(value);
            std::string::size_type startIdx = lvmDir.find_first_not_of(" \"");
            std::string::size_type endIdx = lvmDir.find_last_not_of(" \"");
            if (std::string::npos == startIdx) {
                startIdx = 0;
            }
            if (std::string::npos == endIdx) {
                endIdx = lvmDir.length();
            }
            
            AddLvmFilesToList(lvmDir.substr(startIdx, endIdx - startIdx + 1).c_str(), fileinfo);
        }
    }
}

void VolumeMonitorTask::AddLvmFilesToList(char const * dirName, std::vector<FileInformation>& fileinfo)
{
	// PR#10815: Long Path support
    ACE_DIR* dir = sv_opendir(dirName);
    if (0 == dir) {
        return;
    }

    struct ACE_DIRENT* dirEnt;

    while (0 != (dirEnt =  ACE_OS::readdir(dir))) {
        if (0 == strcmp(".", ACE_TEXT_ALWAYS_CHAR(dirEnt->d_name))
			|| 0 == strcmp("..", ACE_TEXT_ALWAYS_CHAR(dirEnt->d_name))) {
            continue;
        }
        
        ACE_stat fileStat;
        std::string fullPath(dirName);
        fullPath += "/";
        fullPath += ACE_TEXT_ALWAYS_CHAR(dirEnt->d_name);
		// PR#10815: Long Path support
        if (-1 == sv_stat(getLongPathName(fullPath.c_str()).c_str(), &fileStat)) {            
            std::cout << "error stat " << fullPath << ": " << strerror(errno) << '\n';
            continue;
        }
        if (S_ISDIR(fileStat.st_mode)) {
            AddLvmFilesToList(fullPath.c_str(), fileinfo); // should not be a very deep tree so it should be OK to use recursion
        } else {
            fileinfo.push_back(FileInformation(fullPath.c_str(), false));
        }
    }

    //Close directory
    ACE_OS::closedir(dir);
}
