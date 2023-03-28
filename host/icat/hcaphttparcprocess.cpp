#include "archiveprocess.h"
#include "defs.h"
#include "configvalueobj.h"
#include "archivecontroller.h"
#include "icatexception.h"
#include "resumetracker.h"


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
}

void archiveProcess::signalResume()
{
	m_resume->Signal(true) ;
}

std::string archiveProcess::getIpAddress()
{
	return m_IpAddress ; 
}
void archiveProcess::setIpAddress(const std::string& ipAddress) 
{
	m_IpAddress = ipAddress ;
}

void archiveProcess::setPort(int port) 
{
	m_port = port ;
}

archiveProcess::archiveProcess(ArcQueuePtr queuePtr)
{
	m_isDeleteQueue = false ;
	m_queuePtr = queuePtr ;
}
void archiveProcess::setProcessQueue(ArcQueuePtr queuePtr)
{
	m_queuePtr = queuePtr ;
}



bool archiveProcess::getNextObj(fileSystemObj & obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool retVal = false ;
	bool done = false ;
	while( !done )
	{
		ACE_Message_Block *mb = NULL ;
		ACE_Time_Value waitTime = ACE_OS::gettimeofday() ;
		waitTime += ICAT_DEQUEUE_WAIT_TIME ;
		if( archiveController::getInstance()->QuitSignalled() || m_queuePtr.get() == NULL )
		{
			done = true ;
			retVal = false ;
			m_resume->Signal(false) ;
			continue ;
		}
		m_queuePtr->queue()->dequeue_head(mb, &waitTime) ;
		if( mb == NULL )
		{
			if( 	m_queuePtr->state() == ARC_Q_DONE )
			{
				done = true ;
				retVal = false ;
				m_resume->Signal(false) ;
				continue ;
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
			obj = *((fileSystemObj*) mb->base()) ;
			retVal = true ;
			done = true ;
			mb->release() ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

bool archiveProcess::getResumeState()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__); 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__); 
	return m_resume->IsSignalled() ;
}


void hcapHttpArchiveProcess::archiveFile(fileSystemObj obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	SqliteResumeTrackerPtr tracker = archiveController::getInstance()->resumeTrackerInstance() ;
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
	m_arcObj.setHcapAddress(getIpAddress(), false) ;
	int retries = 0 ;

	while(retryOnFailure &&  ( retries  <   maxRetries ) && shouldRetry == true )
	{
		switch( m_arcObj.archiveFile(obj) )
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
				deleteStatus = m_arcObj.deleteFile(obj) ;
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
		if( retries != 0 && retryInterval != 0 && shouldRetry )
		{
			DebugPrintf(SV_LOG_DEBUG, "The last transfer for %s is failed. Waiting for %d seconds before retrying..\n", 
										obj.absPath().c_str(), retryInterval) ;
			controllerPtr->Wait(retryInterval) ;
		}
	}

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

void hcapHttpArchiveProcess::deleteFile(fileSystemObj obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	SqliteResumeTrackerPtr tracker = archiveController::getInstance()->resumeTrackerInstance() ;
	bool shouldRetry = true ;
	bool success = false ;
	ConfigValueObject* configObj = ConfigValueObject::getInstance() ;
	bool retryOnFailure = configObj ->getRetryOnFailure() ;
	int overwrite = configObj->getOverWrite() ;
	int maxRetries = configObj ->getRetryLimit() ;
	archiveControllerPtr controllerPtr = archiveController::getInstance() ;
	int retryInterval = configObj->getRetryInterval() ;
	bool exitOnRetryExpiry = configObj->getExitOnRetryExpiry() ;
	m_arcObj.setHcapAddress(getIpAddress(), false) ;
	int retries = 0 ;

	while(retryOnFailure &&  ( retries  <   maxRetries ) && shouldRetry == true )
	{
		switch( m_arcObj.deleteFile(obj) )
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
		if( retries != 0 && retryInterval != 0 && shouldRetry )
		{
			DebugPrintf(SV_LOG_DEBUG, "The last transfer for %s is failed. Waiting for %d seconds before retrying..\n", 
										obj.absPath().c_str(), retryInterval) ;
			controllerPtr->Wait(retryInterval) ;
		}

	}
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

void hcapHttpArchiveProcess::doProcess(fileSystemObj obj)
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
int hcapHttpArchiveProcess::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_INFO, "Thread started. (Archive Process)\n") ;
	m_resume->Signal(true) ;
	fileSystemObj obj ;
	try
	{
		while( !archiveController::getInstance()->QuitSignalled() )
		{
			if( m_resume->IsSignalled() )
			{
				if( getNextObj(obj) )
				{
					doProcess(obj) ;
					DebugPrintf(SV_LOG_DEBUG, "processed %s\n", obj.absPath().c_str()) ;
				}
				else
				{
					
					m_resume->Signal(false) ;
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
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", oome.to_string().c_str()) ;
		archiveController::getInstance()->QuitSignal() ;
	}
	DebugPrintf(SV_LOG_INFO, "Thread exited. (Archive Process)\n") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return 0 ;
}

int hcapHttpArchiveProcess::open(void *args)
{
    return this->activate(THR_BOUND);
}
int hcapHttpArchiveProcess::close(u_long flags)
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    return 0;
}

void archiveProcess::markAsDeleteQueue()
{
	m_isDeleteQueue = true ;
}

bool archiveProcess::isDeleteQueue()
{
	return m_isDeleteQueue ;
}