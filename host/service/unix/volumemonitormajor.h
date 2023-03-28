
//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : unixvolumemonitor.h
// 
// Description: 
// 

#ifndef UNIXVOLUMEMONITOR__H
#define UNIXVOLUMEMONITOR__H

#include <string>
#include <vector>
#include <map>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <sys/time.h>
#include <ace/Task.h>
#include <ace/Process_Manager.h>
#include "sigslot.h"
#include <boost/shared_ptr.hpp>
#include "hostagenthelpers_ported.h"
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "volumeinfocollector.h"



class FileInformation
{
public:


    FileInformation():m_removed(false) {}

    FileInformation(std::string filename,bool CheckContents = false);

    bool CheckContents()   { return m_CheckContents;};
    const std::string& FileName()  { return m_FileName;};
    bool isModified();
    bool ReadFileContent(std::string &filecontent);  
    bool IsContentModified();

private:	

    // This is special handling for /proc/partitions. Useful for file in memomry filesystems like /proc.
    bool m_CheckContents;                             
    std::string m_FileName;
    timeval m_LastReadTime;	 	
    std::string m_Data;
    bool m_removed;
};


class VolumeMonitorTask : public ACE_Task<ACE_MT_SYNCH>
{
public:

    typedef boost::shared_ptr<VolumeMonitorTask> VolumeMonitorTaskPtr;

    VolumeMonitorTask( ACE_Thread_Manager* threadManager = 0);
	
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
	bool CancelMonitoring();
    sigslot::signal1<void*>& getVolumeMonitorSignal();

protected:	
	
    void CleanUp();	
    void StartPolling();
    void setVolumeMonitorSignal();
    void MonitorFiles();
    void GetLvmFilesToMonitor(std::vector<FileInformation>& fileinfo);
    void AddLvmFilesToList(char const * dirName, std::vector<FileInformation>& fileinfo);

    bool GetFileListToMonitor(std::vector<struct FileInformation> &fileinfo);
    bool MaskOffSIGCLDForThisThread(void);

private:
	
    std::vector <FileInformation> m_fileinfo;  

    sigslot::signal1<void *> m_VolumeMonitorSignal;

    mutable ACE_Mutex m_lock;
    typedef ACE_Guard<ACE_Mutex> AutoGuard;    
};

 
#endif // ifndef UNIXVOLUMEMONITOR__H
