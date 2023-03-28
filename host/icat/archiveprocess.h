#ifndef ARCHIVE_PROCESS_H
#define ARCHIVE_PROCESS_H
#include<boost/shared_ptr.hpp>
#include<ace/Task.h>
#include <boost/shared_ptr.hpp>
#include<ace/Atomic_Op_T.h>
#include "defs.h"
#include "archiveobject.h"
#include "filelister.h"
#include "filesystemobj.h"
#include "icatqueue.h"
#include "event.h"
#include "s2libsthread.h"


class archiveProcess  :public ACE_Task<ACE_MT_SYNCH>
{
protected:
    static ACE_Atomic_Op<ACE_MT_SYNCH, double> m_totalBytesXferred ;
	static ACE_Atomic_Op<ACE_MT_SYNCH, double> m_filesRequested ;
	static ACE_Atomic_Op<ACE_MT_SYNCH, double> m_totalBytesRequested ;
	static ACE_Atomic_Op<ACE_MT_SYNCH, double> m_filesXferred ;
	boost::shared_ptr<ArchiveObj> m_arcObjPtr ;
	ArcQueuePtr m_queuePtr ;
	bool m_isDeleteQueue ;
	archiveProcess(ArcQueuePtr) ;
	std::string m_mountPoint ;

	std::string m_archiveAddress  ;
	int m_port ;
	std::string m_archiveRootDir ;
    ACE_Mutex m_QueueLock ; 

public:
	archiveProcess() ;
	boost::shared_ptr<Event> m_resume ;
	void setProcessQueue(ArcQueuePtr queue) ;
	virtual void doProcess(fileSystemObj) = 0;
	virtual int svc() = 0 ;
	bool getNextObj(fileSystemObj** obj) ;
	bool getResumeState() ;
	void signalResume() ;
	void markAsDeleteQueue() ;
	bool isDeleteQueue() ;
	boost::shared_ptr<ArchiveObj>getArchiveObject() ;

	void setArchiveAddress(const std::string& ipAddress) ;
	std::string getArchiveAddress() ;
	void setPort(int port) ;
	void setArchiveRootDir(const std::string& archiverootDir) ;
	std::string getArchiveRootDir() ;
	int getPort() {return m_port ; }
    ACE_Message_Block* retrieveQueueEntry() ;

} ;


class ArchiveProcessImpl : public archiveProcess
{
	void archiveFile(fileSystemObj) ;
	void deleteFile(fileSystemObj) ;
	void listingIntoFile(fileSystemObj);
public:
	void doProcess(fileSystemObj) ;
	virtual int svc() ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
} ;

class FilelistProcessImpl : public archiveProcess
{
	std::ofstream m_outputfile;
	void listingIntoFile(fileSystemObj);
	
public:
	void doProcess(fileSystemObj) ;
	void init();
	void closeFile();
	virtual int svc() ;
	virtual int open(void *args = 0);
	virtual int close(u_long flags = 0);
};


typedef boost::shared_ptr<archiveProcess> archiveProcessPtr ;
typedef boost::shared_ptr<ArchiveProcessImpl> ArchiveProcessImplPtr ;
typedef boost::shared_ptr<archiveProcess> filelistProcessPtr ;
typedef boost::shared_ptr<FilelistProcessImpl> FilelistProcessImplPtr ;

#endif

