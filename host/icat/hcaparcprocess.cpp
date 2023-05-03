#include "archiveprocess.h"
#include "defs.h"
#include "configvalueobj.h"
#include "archivecontroller.h"
#include "icatexception.h"
#include "resumetracker.h"
#include "archivefactory.h"

ACE_Atomic_Op<ACE_MT_SYNCH, double> archiveProcess::m_totalBytesXferred ;
ACE_Atomic_Op<ACE_MT_SYNCH, double> archiveProcess::m_filesRequested ;
ACE_Atomic_Op<ACE_MT_SYNCH, double> archiveProcess::m_totalBytesRequested ;
ACE_Atomic_Op<ACE_MT_SYNCH, double> archiveProcess::m_filesXferred ;

archiveProcess::archiveProcess()
{
	m_isDeleteQueue = false ;
	m_resume.reset(new (std::nothrow) Event(false, false, "")) ;
	if( m_resume.get() == NULL )
	{
		throw icatOutOfMemoryException("Failed to create the event object\n") ;
	}
	m_arcObjPtr = archiveFactory::getInstance()->createArchiveObject() ;
}
boost::shared_ptr<ArchiveObj> archiveProcess::getArchiveObject()
{
	return m_arcObjPtr ;
}
void archiveProcess::signalResume()
{
	m_resume->Signal(true) ;
}


void archiveProcess::markAsDeleteQueue()
{
	m_isDeleteQueue = true ;
}

bool archiveProcess::isDeleteQueue()
{
	return m_isDeleteQueue ;
}


void archiveProcess::setArchiveAddress(const std::string& address)
{
	m_archiveAddress = address;
}
std::string archiveProcess::getArchiveAddress()
{
	return m_archiveAddress ;
}
void archiveProcess::setPort(int port)
{
	m_port = port ;
}
void archiveProcess::setArchiveRootDir(const std::string& archiveRootDir)
{
	m_archiveRootDir = archiveRootDir ;
}
std::string archiveProcess::getArchiveRootDir()
{
	return m_archiveRootDir ;
}

archiveProcess::archiveProcess(ArcQueuePtr queuePtr)
{
	m_isDeleteQueue = false ;
	m_queuePtr = queuePtr ;
}
void archiveProcess::setProcessQueue(ArcQueuePtr queuePtr)
{
     ACE_Guard<ACE_Mutex> queueGuard( m_QueueLock ) ;
	m_queuePtr = queuePtr ;
}

bool archiveProcess::getNextObj(fileSystemObj ** obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	while( !archiveController::getInstance()->QuitSignalled() && m_queuePtr.get() != NULL )
	{
        ACE_Message_Block *mb = NULL ;
        ACE_Time_Value waitTime = ACE_OS::gettimeofday() ;
        waitTime += ICAT_DEQUEUE_WAIT_TIME ;
        ARC_QUEUE_STATE state = ARC_Q_NONE ;

        ACE_Guard<ACE_Mutex> queueGuard( m_QueueLock ) ;
        if( m_queuePtr.get() != NULL )
        {
            m_queuePtr->queue()->dequeue_head(mb, &waitTime) ;
            state = m_queuePtr->state() ;
        }
        else
        {
            break ;
        }
        queueGuard.release() ;        

        if( mb == NULL )
        {
            if( state == ARC_Q_DONE )
            {
                break ;
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Unable to get file system objects from processor queue even after %d seconds.. Trying again\n", ICAT_DEQUEUE_WAIT_TIME) ;
                continue ;
            }
        }
        else
        {
            int type = mb->msg_type() ;
            *obj = ((fileSystemObj*) mb->base()) ;
            mb->release() ;
            return true ;
        }
    }
    m_resume->Signal(false) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return false ;
}

bool archiveProcess::getResumeState()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__); 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__); 
	return m_resume->IsSignalled() ;
}


void ArchiveProcessImpl::archiveFile(fileSystemObj obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	boost::shared_ptr<ResumeTracker> tracker = archiveController::getInstance()->resumeTrackerInstance() ;
	bool shouldRetry = true ;
	bool success = false ;
	ICAT_OPER_STATUS deleteStatus ;
	ConfigValueObject* configObj = ConfigValueObject::getInstance() ;
	bool retryOnFailure = configObj ->getRetryOnFailure() ;
	int overwrite = configObj->getOverWrite() ;
	int maxRetries = configObj ->getRetryLimit() ;
	archiveControllerPtr controllerPtr = archiveController::getInstance() ;
	int retryInterval = configObj->getRetryInterval() ;
	bool exitOnRetryExpiry = configObj->getExitOnRetryExpiry() ;
	getArchiveObject()->setArchiveAddress(getArchiveAddress()) ;
	getArchiveObject()->setPort(getPort()) ;
	getArchiveObject()->setArchiveRootDirectory(getArchiveRootDir()) ;
	int retries = 0 ;

	do
	{
		switch( getArchiveObject()->archiveFile(obj) )
		{
			case ICAT_OPSTAT_BAD_TRANSPORT :
			case ICAT_OPSTAT_COULDNOT_CONNECT :
			case ICAT_OPSTAT_FAILED :
				success = false ;
				shouldRetry = false ;
				break ;

			case ICAT_OPSTAT_SUCCESS :
				success = true ;
				shouldRetry = false ;
				break ;
			case ICAT_OPSTAT_ALREADY_EXISTS :
				if( overwrite && !controllerPtr->QuitSignalled() )
				{
					deleteStatus = getArchiveObject()->deleteFile(obj) ;
					if( deleteStatus == ICAT_OPSTAT_DELETED )
					{
						retries -= 1 ;
						shouldRetry = true ;
						success = false ;
					}
					else
					{
						switch(deleteStatus)
						{
							case ICAT_OPSTAT_BAD_TRANSPORT:
							case ICAT_OPSTAT_COULDNOT_CONNECT:
							case ICAT_OPSTAT_FAILED:
								success = false ;
								shouldRetry = false ;
								break ;
							case ICAT_OPSTAT_CONN_TIMEOUT:
								success = false ;
								shouldRetry = true ;
								break ;
						}
					}
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Can't delete the %s as the overwrite is set to false\n", obj.absPath().c_str()) ;
					success = false ;
					shouldRetry = false ;
				}
				break ;
			case ICAT_OPSTAT_NOTREADABLE :
				success = false ;
				shouldRetry = false ;
				break ;
			case ICAT_OPSTAT_CONN_TIMEOUT :
				shouldRetry = true ;
				success = false ;
				break ;
			default:
				break ;
		}
		retries ++ ;
		if( !controllerPtr->QuitSignalled() && retries != 0 && retryInterval != 0 && shouldRetry )
		{
			DebugPrintf(SV_LOG_DEBUG, "The last transfer for %s is failed. Waiting for %d seconds before retrying..\n", 
										obj.absPath().c_str(), retryInterval) ;
			controllerPtr->Wait(retryInterval) ;
		}
	}while(retryOnFailure &&  ( retries  <   maxRetries ) && shouldRetry == true && !controllerPtr->QuitSignalled() );

	if( retries > maxRetries && exitOnRetryExpiry )
	{
		controllerPtr->QuitSignal() ;
	}
	if( !success )
	{
		tracker->updateFileStatus(obj.fileName(), obj.getStat(), obj.getFolderId(), ICAT_RESOBJ_STAT_FAILED ) ;
	}
	else
	{
		tracker->updateFileStatus(obj.fileName(), obj.getStat(), obj.getFolderId(), ICAT_RESOBJ_SUCCESS ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return ;
}

void ArchiveProcessImpl::deleteFile(fileSystemObj obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	boost::shared_ptr<ResumeTracker> tracker = archiveController::getInstance()->resumeTrackerInstance() ;
	bool shouldRetry = true ;
	bool success = false ;
	ConfigValueObject* configObj = ConfigValueObject::getInstance() ;
	bool retryOnFailure = configObj ->getRetryOnFailure() ;
	int maxRetries = configObj ->getRetryLimit() ;
	archiveControllerPtr controllerPtr = archiveController::getInstance() ;
	int retryInterval = configObj->getRetryInterval() ;
	bool exitOnRetryExpiry = configObj->getExitOnRetryExpiry() ;
	getArchiveObject()->setArchiveAddress(getArchiveAddress()) ;
	getArchiveObject()->setPort(getPort()) ;
	getArchiveObject()->setArchiveRootDirectory(getArchiveRootDir()) ;

	int retries = 0 ;

	do
	{
		switch( getArchiveObject()->deleteFile(obj) )
		{
			case ICAT_OPSTAT_BAD_TRANSPORT :
			case ICAT_OPSTAT_COULDNOT_CONNECT :
			case ICAT_OPSTAT_FAILED :
				success = false ;
				shouldRetry = false ;
				break ;

			case ICAT_OPSTAT_DELETED :
				shouldRetry = false ;
				success = true ;
				break ;
			case ICAT_OPSTAT_NOTREADABLE :
				success = false ;
				shouldRetry = false ;
				break ;
			case ICAT_OPSTAT_CONN_TIMEOUT :
				shouldRetry = true ;
				success = false ;
				break ;
			case ICAT_OPSTAT_FORBIDDEN : 
				shouldRetry = false ;
				success = false ;
				break ;
			default:
				break ;
		}
		retries ++ ;
		if( !controllerPtr->QuitSignalled() && retries != 0 && retryInterval != 0 && shouldRetry )
		{
			DebugPrintf(SV_LOG_DEBUG, "The last transfer for %s is failed. Waiting for %d seconds before retrying..\n", 
										obj.absPath().c_str(), retryInterval) ;
			controllerPtr->Wait(retryInterval) ;
		}

	}while(retryOnFailure &&  ( retries  <   maxRetries ) && shouldRetry == true && !controllerPtr->QuitSignalled() );

	if( retries > maxRetries && exitOnRetryExpiry )
	{
		controllerPtr->QuitSignal() ;
	}
	if( success )
	{
		tracker->updateDeleteStatus(obj.fileName(), obj.getFolderId(), obj.isDir(), ICAT_RESOBJ_DELETED ) ;
	}
	else
	{
		tracker->updateDeleteStatus(obj.fileName(), obj.getFolderId(), obj.isDir(), ICAT_RESOBJ_DELETE_FAILED ) ;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return ;
}

void ArchiveProcessImpl::doProcess(fileSystemObj obj)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	switch( obj.getOperation() )
	{
		case ICAT_OPER_ARCH_FILE :
			archiveFile(obj) ;
			break ;
		case ICAT_OPER_DEL_FILE : 
			deleteFile(obj) ;
			break ;
		default :
			break ;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

int ArchiveProcessImpl::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG, "Thread started. (Archive Process)\n") ;
	m_resume->Signal(true) ;
	fileSystemObj *obj = NULL ;
	try
	{
        while( !archiveController::getInstance()->QuitSignalled() && m_queuePtr.get() != NULL)
		{

			if( m_resume->IsSignalled() )
			{
				if( getNextObj(&obj) )
				{
					doProcess(*obj) ;
					DebugPrintf(SV_LOG_DEBUG, "processed %s\n", obj->absPath().c_str()) ;
                    delete obj ;
                    obj = NULL ;
				}
				else
				{
					if (!isDeleteQueue()) 
					{
						DebugPrintf(SV_LOG_DEBUG, "Didnt find any objects in queue... sending ICAT_MSG_AP_DONE to controller\n") ;
						archiveController::getInstance()->postMsg(ICAT_MSG_AP_DONE, ICAT_MSGPRIO_NORMAL, this) ;
					}
				}
			}
			else
			{
				if( isDeleteQueue() )
				{
					break ;
				}
			}
		}
	}
	catch(icatOutOfMemoryException &oome)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", oome.what()) ;
		archiveController::getInstance()->QuitSignal() ;
	}
	catch(icatException&e)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", e.what()) ;
		archiveController::getInstance()->QuitSignal() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Thread exited. (Archive Process)\n") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return 0 ;
}

int ArchiveProcessImpl::open(void *args)
{
    return this->activate(THR_BOUND);
}
int ArchiveProcessImpl::close(u_long flags)
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    return 0;
}
