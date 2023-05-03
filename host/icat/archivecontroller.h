#ifndef ARCHIVE_CONTROLLER_H
#define ARCHIVE_CONTROLLER_H
#include <list>
#include "cdputil.h"
#include "archiveprocess.h"
#include "archiveobject.h"
#include "filelister.h"

#include "resumetracker.h"

#include <vector>
#include "event.h"
#include "s2libsthread.h"

class archiveController ;
typedef boost::shared_ptr<archiveController> archiveControllerPtr ;
typedef std::list<archiveProcessPtr> ArchiveProcessPtrList_t ;
typedef std::list<FileListerPtr> FileListerPtrList_t;
typedef std::list<ArcQueuePtr> ProcessQueuePtrList_t ;

struct TransferStat
{
    ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> total_Num_Of_Files;
    ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> total_Num_Of_Dirs;
    ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> total_Num_Of_Transfered_Files;
    ACE_Atomic_Op<ACE_Thread_Mutex, SV_ULONGLONG> total_Num_Of_Excluded_Dirs;
    ACE_Atomic_Op<ACE_Thread_Mutex, double> total_Num_Of_Bytes_Sent;
    TransferStat()
    {   
        total_Num_Of_Files = 0; 
        total_Num_Of_Dirs = 0;
        total_Num_Of_Transfered_Files = 0;
        total_Num_Of_Excluded_Dirs = 0;
        total_Num_Of_Bytes_Sent = 0;
    }
};

class archiveController 
{
	static archiveControllerPtr m_instance ;
	
	ArchiveProcessPtrList_t  m_arcProcessPtrList ;
	ProcessQueuePtrList_t m_processQueuePtrList ;
	FileListerPtrList_t m_fileListerPtrList ;
	boost::shared_ptr<ResumeTracker> m_tracker ;

	int m_maxProcessorThreads ;
	ACE_SHARED_MQ m_MsgQueue ; 
	boost::shared_ptr<Event> m_Quit ; 
	pid_t m_ProcessId;
public:    
    TransferStat m_tranferStat;
public:
	bool init() ;
	void run() ;
	void postMsg(ICAT_MSG_TYPE msgId, int prio, void*) ;
	void initFileListerObjects() ;
	void initProessQueues() ;
	void initProcessObjects() ;
	void initResumeTracker();	
	void waitforarchiveTasks() ;
	void stopAllWorkers() ;
	static archiveControllerPtr getInstance() ;
	boost::shared_ptr<ResumeTracker> resumeTrackerInstance() ;
	bool IsarchivalPending() ;
	ACE_SHARED_MQ& msgQueue() ;
	/*
		Quit event Mangament
	*/
	void InitQuitEvent() ;
	bool QuitSignalled() ;
	void QuitSignal() ;
	std::string getStartTime(std::string fmt) ;
	WAIT_STATE Wait(const int iSeconds) ;
	int doDelete() ;
    void dumpTransferStat();
} ;



/*
TODO :  Controller should be able to respond to external signals and stop all other threads gracefully.
*/
#endif

