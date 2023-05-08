#include "archivecontroller.h"
#include "configvalueobj.h"
#include "filefilter.h"
#include "filesystemobj.h"
#include "archivefactory.h"
#include "icatexception.h"
#include "common.h"
#include "inmsafecapis.h"

archiveControllerPtr archiveController::m_instance ;

/*
* Function Name: archiveController::msgQueue
* Arguements: 
*                  None
* Return Value:
*                  ACE_SHARED_MQ& : Reference to the m_MsgQueue
* Description:
*                  This function returns the reference to the message Queue that is used by 
*                  other components to send messages for archive controller to process them
* Called by: 
*                  main
* Calls:
*                  None
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/

ACE_SHARED_MQ& archiveController::msgQueue()
{
	return m_MsgQueue ;
}

/*
* Function Name: archiveController::init
* Arguements: 
*                  None
* Return Value:
*                  true : if there are no errors while initializing the controller
*                  fasle : if there are errors while initializing the controller
* Description:
*                  The function initializes the all required components for icat to start.
* Called by: 
*
*                  main
* Calls:
*                  ConfigValueObject::getInstance - To get the configurator object instance
*                  initResumeTracker - Initializes the resume tracker
*                  initProessQueues - Initializes the process Queues
*                  initProcessObjects - Initiailzes the processor objects
*                  initFileListerObjects - initializes the file lister objects
*                  InitQuitEvent - Initializes the Quit Event
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/

bool archiveController::init()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool retVal = false ;
	InitQuitEvent() ;
	ConfigValueObject* conf = ConfigValueObject::getInstance() ;
	std::string filelistToTarget = conf->getFilelistToTarget();
	try
	{
			initResumeTracker() ;
		initProessQueues() ;
		initProcessObjects() ;
		initFileListerObjects() ;
		retVal = true ;
	}
	catch(icatOutOfMemoryException &oomex)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", oomex.what()) ;
	}
	catch(icatTransportException &it)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", it.what()) ;
	}
	catch(icatException &e)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception %s\n", e.what()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

/*
* Function Name: archiveController::run
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                  The function controls the execution of file lister, archive processor etc..
* Called by: 
*
*                  main
* Calls:
*                  File lister service routine.
*                  Archive Processor service routine
*                  waitforarchiveTasks - Waits until all file lister and processor threads to exit
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/


void archiveController ::run()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",__FUNCTION__);
	FileListerPtrList_t::iterator listerPtrIter = m_fileListerPtrList.begin() ;
	ArchiveProcessPtrList_t::iterator arcProcPtrIter = m_arcProcessPtrList.begin() ;
	ConfigValueObject *conf = ConfigValueObject::getInstance() ;
	std::string filelistToTarget = conf->getFilelistToTarget();
	try
	{
			doDelete() ;
		while( arcProcPtrIter != m_arcProcessPtrList.end() )
		{
			(*arcProcPtrIter)->open() ;
			arcProcPtrIter++ ;
		}
		while( listerPtrIter != m_fileListerPtrList.end() )
		{
			(*listerPtrIter)->open() ;
			listerPtrIter++ ;
		}
	}
	catch(icatThreadException &e)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception %s\n", e.what()) ;
		DebugPrintf(SV_LOG_DEBUG, "Setting Quit Signal\n") ;
		QuitSignal() ;
	}
	catch(icatException &e)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception %s\n", e.what()) ;
		DebugPrintf(SV_LOG_DEBUG, "Setting Quit Signal\n") ;
		QuitSignal() ;
	}
	waitforarchiveTasks() ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",__FUNCTION__);
}

/*
* Function Name: archiveController::waitforarchiveTasks
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                  Processes messages which are sent by processor and file lister
*                  Once all processors and file listers finishes sending the messages
*                  it waits for all file lister and archive processor threads to exit.
* Called by: 
*
*                  archiveController::run
* Calls:
*                  FileLister::isProcessing - To know whether file lister is processing any directories or not.
*                  ArchiveProcessor::setProcessQueue - Sets the apropriate process queue for a archive processor
*                  ArchiveProcessor::signalResume - Makes processor to resume processing the queue.
*                  ACE_Task::wait - The calling thread waits until the thread execution gets finishes.
*                  sqliteresumetracker::setEndTime - Set the end time and sets the final status of the content source in db.
*                  QuitSignal - Issues the Quit Signal for other threads to stop their execution.
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/


void archiveController ::waitforarchiveTasks()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    bool quit = false ;
    int processorCount = 0 ;
    ConfigValueObject* conf = ConfigValueObject::getInstance() ;
	std::string filelistToTarget = conf->getFilelistToTarget();
	while( !QuitSignalled() && processorCount != conf->getMaxConnects() )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() ;
        tv += ICAT_DEQUEUE_WAIT_TIME ;
        ACE_Message_Block *mb = NULL ;
        if(-1 == m_MsgQueue.dequeue_head(mb, &tv) ) 
        {
            if ( (errno == EWOULDBLOCK || errno == ESHUTDOWN ) &&  QuitSignalled())
            {
                break ;
            }
            else
            {
                continue ;
            }
        }
        FileListerPtrList_t::iterator  fileListerPtrIter = m_fileListerPtrList.begin() ;
        ArchiveProcessPtrList_t::iterator processorIter = m_arcProcessPtrList.begin() ;
        int type = mb->msg_type() ;
        archiveProcess* processorPtr ;
        processorPtr = (ArchiveProcessImpl*)mb->base();
        mb->release() ;
        switch(type ) 
        {
        case ICAT_MSG_AP_DONE :
            DebugPrintf(SV_LOG_DEBUG, "Recieved Message ICAT_MSG_AP_DONE \n") ;
            while( fileListerPtrIter != m_fileListerPtrList.end() )
            {
                if( (*fileListerPtrIter)->isProcessing() )
                {
                    break ;
                }
                fileListerPtrIter++ ;
            }

            if( fileListerPtrIter != m_fileListerPtrList.end())
            {
                processorPtr->setProcessQueue((*fileListerPtrIter)->getQueue()) ;
            }
            else
            {
                ArcQueuePtr nullQueuePtr ;
                processorPtr->setProcessQueue(nullQueuePtr) ;
                processorCount++ ;                
            }
            processorPtr->signalResume() ;
            break ;
        case ICAT_MSG_FL_DONE :
            DebugPrintf(SV_LOG_DEBUG, "Recieved Message ICAT_MSG_FL_DONE \n") ;
            break ;
        case ICAT_MSG_TRANSPORT_FAILED :
            DebugPrintf(SV_LOG_ERROR, "Transport errors\n") ;
            quit = true;
            break;
        default:
            break;
        }       
    }

    if( !QuitSignalled() )
        QuitSignal() ;

    ArchiveProcessPtrList_t::iterator processorIter = m_arcProcessPtrList.begin() ;
    int threadCount = 0 ;
    while( processorIter != m_arcProcessPtrList.end() )
    {
        (*processorIter)->wait() ;
        processorIter ++ ;
    }
    try
    {
	        m_tracker->setEndTime() ;
    }
    catch(icatException &e )
    {
        DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", e.what()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


/*
* Function Name: archiveController::postMsg
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                   This function is used by the file lister and archive processor to 
*                   notify their status of execution to the controller.
* Called by: 
*
*                  FileLister::doArchvie
*                  ArchiveProcessor::svc
* Calls:
*                  None
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/

void archiveController::postMsg(ICAT_MSG_TYPE msgType, int prio, void* ptr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	ACE_Message_Block *mb = new (std::nothrow) ACE_Message_Block(sizeof(ptr),	msgType, NULL, (const char*) ptr);
	if( mb == NULL )
	{
		throw icatOutOfMemoryException("Failed to create message block\n") ;
	}
    mb->msg_priority(prio);
	DebugPrintf(SV_LOG_DEBUG, "Adding message ICAT_MSG_TYPE: %d with priority: %d to message Queue\n", msgType, prio) ;
    if( m_MsgQueue.enqueue_prio(mb) == -1 )
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to enqueue message to Queue. errno : %d\n", errno) ;
		throw icatException("enqueue failed\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
}

/*
* Function Name: archiveController::getInstance
* Arguements: 
*                  None
* Return Value:
*                  returns controller instance to the caller functions
* Description:
*                  This function is used by other components of icat to get the controller instance reference
*                  to perform any action.                  
* Called by: 
*                  FileLister
*                  ArchiveProcessor
*                  main
* Calls:
*                  None
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/


archiveControllerPtr archiveController::getInstance()
{
	if( m_instance.get() == NULL )
	{
		m_instance.reset(new (std::nothrow) archiveController) ;
		if( m_instance.get() == NULL )
		{
			throw icatOutOfMemoryException("Failed to create the archive controller Object\n") ;
		}
	}
	return m_instance ;
}

/*
* Function Name: archiveController::initFileListerObjects
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                  Initializes the file listers. Creates n file lister objects where n is number of threads
*                  create file lister value objects from all available content sources and feeds them to 
*                  all file listers.
* Called by: 
*                  archiveController::init
* Calls:
*                  FileLister::getFileListerValueObjs - expands the content source on bfs basis and creates
*                                                              file lister value objects
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/

void archiveController::initFileListerObjects()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	
	ConfigValueObject *conf = ConfigValueObject::getInstance() ;
	std::string filelistFromSource = conf->getFilelistFromSource();
	int maxfilelisters = conf->getmaxfilelisters();
	
	ProcessQueuePtrList_t ::iterator processQPtrIter = m_processQueuePtrList.begin() ;
	for(int i =0; i < maxfilelisters;i++)
	{
		FileListerPtr listerPtr(new (std::nothrow) FileLister(*processQPtrIter) );
		if( listerPtr.get() == NULL )
		{
			throw icatOutOfMemoryException("Failed to create the file lister Object\n") ;
		}
		m_fileListerPtrList.push_back(listerPtr) ;
		processQPtrIter++;
	}
	while(processQPtrIter != m_processQueuePtrList.end())
	{
		(*processQPtrIter)->setState(ARC_Q_DONE);	
		processQPtrIter++;
	}
	if(filelistFromSource.empty() == true)
	{
		FileListerValueObj_t& listerValueObjsList = FileLister::getFileListerValueObjs() ;
		FileListerValueObj_t::iterator listerValueObjsIter = listerValueObjsList.begin() ;
		FileListerPtrList_t::iterator  fileListerPtrIter = m_fileListerPtrList.begin() ;
		while( listerValueObjsIter != listerValueObjsList.end() )
		{
			if( fileListerPtrIter == m_fileListerPtrList.end() )
			{
				fileListerPtrIter = m_fileListerPtrList.begin() ;
			}
			FileListerPtr listerPtr = (*fileListerPtrIter) ;
			listerPtr->addListerObj((*listerValueObjsIter )) ;

			listerValueObjsIter++ ;
			fileListerPtrIter++ ;
		}

		fileListerPtrIter = m_fileListerPtrList.begin() ;
		while( fileListerPtrIter != m_fileListerPtrList.end() )
		{
			DebugPrintf(SV_LOG_DEBUG, "initialized file lister with %d file lister objects\n", (*fileListerPtrIter)->fileListerObjsCount()) ;
			fileListerPtrIter ++ ;
		}

		DebugPrintf(SV_LOG_DEBUG, "Initialized %d file lister threads\n", m_fileListerPtrList.size()) ;
	
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

/*
* Function Name: archiveController::initProessQueues
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                 initializes the process n queue objects where n is number of threads
* Called by: 
*                  archiveController::init
* Calls:
*                  None
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
void archiveController::initProessQueues()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

	ConfigValueObject* conf = ConfigValueObject::getInstance() ;
	int maxProcessorThreads = conf->getMaxConnects() ;	
	for( int processorThreadIdx = 0 ; processorThreadIdx < maxProcessorThreads ;  processorThreadIdx++) 
	{
		ACE_SHARED_MQ_Ptr queueImpl;
		queueImpl.reset(new ACE_SHARED_MQ) ;
		ArcQueuePtr tempQPtr(new ArcQueue(queueImpl));
		m_processQueuePtrList.push_back(tempQPtr) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Initialized %d Process Queues\n", m_processQueuePtrList.size()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

/*
* Function Name: archiveController::initProcessObjects
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                 initializes the n process objects where n is number of threads.
* Called by: 
*                  archiveController::init
* Calls:
*                  createArchiveProcess - Create the archive process object
*                  setProcessQueue - Sets the process queue in the processor object
*                  setPort - sets the port number in the processor object
*                  setArchiveAddress - sets the archive address (Ipaddress/dnsname) in the processor object
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
void archiveController::initProcessObjects()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

	ConfigValueObject * conf = ConfigValueObject::getInstance() ;
	archiveFactoryPtr archiveFactory = archiveFactory::getInstance() ;
	std::list<Nodes> nodesList = conf->getNodeList() ; 
	std::list<Nodes>::iterator nodeListIter = nodesList.begin() ;
	ProcessQueuePtrList_t::iterator processQPtrItr = m_processQueuePtrList.begin() ;
	std::string FilelistToTarget = conf->getFilelistToTarget();
	int maxProcessorThreads = conf->getMaxConnects() ;
	if(FilelistToTarget.empty() == false )
	{
		filelistProcessPtr flistPocessPtr = archiveFactory->createFileListingProcess() ;
		flistPocessPtr->setProcessQueue(*processQPtrItr) ;
		m_arcProcessPtrList.push_back(flistPocessPtr) ;
	}
	else
	{
		
		while( processQPtrItr != m_processQueuePtrList.end() )
		{
			archiveProcessPtr arcProcessPtr = archiveFactory->createArchiveProcess() ;
			if( nodeListIter == nodesList.end() )
			{
				nodeListIter = nodesList.begin() ;
			}	
	        if( nodeListIter == nodesList.end() ) 
    	    {
        	    arcProcessPtr->setArchiveAddress(conf->getDNSname()) ;
	        }
    	    else
        	{
		    	arcProcessPtr->setArchiveAddress((*nodeListIter).m_szIp) ;
	        }
			arcProcessPtr->setPort((*nodeListIter).m_Port) ;
			arcProcessPtr->setArchiveRootDir(conf->getArchiveRootDirectory()) ;

			arcProcessPtr->setProcessQueue(*processQPtrItr) ;
			m_arcProcessPtrList.push_back(arcProcessPtr) ;
			nodeListIter++ ;
			processQPtrItr++;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "Initialized %d process objects\n", m_arcProcessPtrList.size()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;	
}


/*
* Function Name: archiveController::InitQuitEvent
* Arguements: 
*                  None
* Return Value:
*                  void
* Description:
*                 initializes the quit event for other threads to send quit events whenever requried.
* Called by: 
*                  archiveController::init
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
void archiveController::InitQuitEvent()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    char EventName[256];

    m_ProcessId = ACE_OS::getpid();
	DebugPrintf(SV_LOG_DEBUG, "icat is running with process Id %d\n", m_ProcessId) ;
	if (ACE_INVALID_PID != m_ProcessId)	
    {
        inm_sprintf_s(EventName, ARRAYSIZE(EventName), "%s%d", AGENT_QUITEVENT_PREFIX, m_ProcessId);
		m_Quit.reset(new (std::nothrow) Event(false, false, EventName));
		if( m_Quit.get() == NULL )
		{
			throw icatOutOfMemoryException("Failed to create Event Object\n") ;
		}
		DebugPrintf(SV_LOG_DEBUG, "Initialized Quit Event\n") ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

/*
* Function Name: archiveController::QuitSignalled
* Arguements: 
*                  None
* Return Value:
*                  true - if any thread issued the quit signal already
*                  false - if no thread has issued the quit signal already
* Description:
*                 Checks whether Quit signal being signalled by any thread.
* Called by: 
*                  all threads
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
bool archiveController::QuitSignalled()
{
    if( m_Quit->IsSignalled() )
        DebugPrintf(SV_LOG_DEBUG, "Quit Signalled\n") ;
    return m_Quit->IsSignalled();
}   

/*
* Function Name: archiveController::Wait
* Arguements: 
*                  None
* Return Value:
*                  WAIT_SUCCESS - if wait has successful
*                  WAIT_TIMED_OUT - if wait has timed out 
*                  WAIT_FAILURE - Wait caused any failure
* Description:
*                 Waits until iSeconds and exits if the quit signal is arrived before iSeconds seconds.
* Called by: 
*                  all threads
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
WAIT_STATE archiveController::Wait(const int iSeconds)
{
    WAIT_STATE waitRet = WAIT_TIMED_OUT;
	if (m_Quit->IsSignalled())
	{
        waitRet = WAIT_SUCCESS;
	}
	else
	{
        waitRet = m_Quit->Wait(iSeconds);
	}
    return waitRet;
}

/*
* Function Name: archiveController::QuitSignal
* Arguements: 
*                  None
* Return Value:
*                  None
* Description:
*                 This is used to issue quit signal.
* Called by: 
*                  all threads
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
void archiveController::QuitSignal()
{
    DebugPrintf(SV_LOG_DEBUG, "Issued Quit Signal\n") ;
	m_Quit->Signal(true) ;
}

/*
* Function Name: archiveController::initResumeTracker
* Arguements: 
*                  None
* Return Value:
*                  None
* Description:
*                 This is used to issue quit signal.
* Called by: 
*                  all threads
* Calls:
*                  ConfigValueObject::getInstance - To get the config object instance
*                  HttpArchiveObj::prepareBaseUrl - To prepare base url
*                  SqliteResumeTracker::SqliteResumeTracker - instantiate the sqlite tracker based on the icat mode ie normal mode or track mode
*                  ConfigValueObject::getResume - to find out whether icat instance is ran in resume or track mode
*                  ConfigValueObject::getMaxCopies - to know max number of copies at server
*                  ConfigValueObject::getMaxLifeTime - to know max life time of copies at server

* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
void archiveController::initResumeTracker()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ConfigValueObject * config = ConfigValueObject::getInstance() ;
	ICAT_RESUME_TRACKER_MODE trackerMode ;
	std::string basePath ;
	std::string filelistToTarget = config->getFilelistToTarget();
	if( config->getResume())
	{
		DebugPrintf(SV_LOG_DEBUG, "Tracker started in ICAT_TRACKER_MODE_RESUME mode \n") ;
		trackerMode = ICAT_TRACKER_MODE_RESUME ;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Tracker started in ICAT_TRACKER_MODE_TRACK mode \n") ;
		trackerMode = ICAT_TRACKER_MODE_TRACK ;
		basePath = HttpArchiveObj::prepareBaseUrl() ;

	}
	if(filelistToTarget.empty() == true)
	{
		m_tracker.reset( new SqliteResumeTracker(config->getBranchName(), 
											config->getServerName(), 
											config->getSourceVolume(), 
											basePath, 
											config->getMaxCopies(),
											config->getMaxLifeTime(),
											trackerMode)) ;
	}
	else	
	{
        m_tracker.reset( new EmptyTracker()) ;
	}
	m_tracker->init() ;
	DebugPrintf(SV_LOG_DEBUG, "Initialized Resume Tracker\n") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

/*
* Function Name: archiveController::resumeTrackerInstance
* Arguements: 
*                  None
* Return Value:
*                  SqliteResumeTrackerPtr
* Description:
*                 gives the resume tracker instance to the callers.
* Called by: 
*                  all threads
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
boost::shared_ptr<ResumeTracker> archiveController::resumeTrackerInstance() 
{
	return m_tracker ;
}

/*
* Function Name: archiveController::getStartTime
* Arguements: 
*                  std::string fmt - format in which the start time is required.
* Return Value:
*                  SqliteResumeTrackerPtr
* Description:
*                 gives the resume tracker instance to the callers.
* Called by: 
*                  all threads
* Calls:
*                  ConfigValueObject::getStartTime - Configvalue object has maintains the start time.
*                  getTimeToString - Converts the time to the required date format
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
std::string archiveController::getStartTime(std::string fmt)
{
	std::string timeStr ;
	time_t starttime = ConfigValueObject::getInstance()->getStartTime() ;
	getTimeToString(fmt, timeStr, &starttime) ;	
	return timeStr ;
}

/*
* Function Name: archiveController::doDelete
* Arguements: 
*                  0
* Return Value:
*                  SqliteResumeTrackerPtr
* Description:
*                 1. Creates a archive processor for deleting the files that reside at server.
*                 2. Gets the list of deleteable objects from the resume tracker.
*                 3. prepares the file path, url that will be processed by the archive processor.
*                 4. inserts the url, path to the delete queue and this is repeated for all files/directories that needs to be
                        deleted by the proessor.
                   5. Once all are finished marks the queue as done and waits for the delete processor thread to exit.
* Called by: 
*                  archiveController::run
* Calls:
*                  archiveFactory::createArchiveProcess - creates a archive processor for carrying out file deletion on server
*                  archiveProcessor::setArchiveAddress - sets the dns/ipaddress in the archive processor
*                  archiveProcessor::setArchiveRootDir - sets the archive root directory
*                  archiveProcessor::setProcessQueue - sets the process queue that will be processed by archive processor.
*                  archiveProcessor::markAsDeleteQueue - marks the processor as delete queue.
*                  sqliteresumetracker::getDeleteableObjects - gets the list of deleteable files, directories from the resume tracker
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
int archiveController::doDelete()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	int maxLifeTime = 0 ;
	int maxCopies = 0 ;
	
	std::list<ResumeTrackerObj> list; 
	char REMOTE_PATH_SEPERATOR ;
	
	ACE_SHARED_MQ_Ptr queueImpl;
	queueImpl.reset(new ACE_SHARED_MQ) ;
	ArcQueuePtr deleteQPtr(new ArcQueue(queueImpl));
	
	ConfigValueObject * confObj = ConfigValueObject::getInstance() ;
	archiveFactoryPtr archiveFactory = archiveFactory::getInstance() ;
	std::list<Nodes> nodesList = confObj ->getNodeList() ; 
	std::list<Nodes>::iterator nodeListIter = nodesList.begin() ;
	maxCopies = confObj ->getMaxCopies() ;
	maxLifeTime = confObj ->getMaxLifeTime() ;
	
	
	archiveProcessPtr deleteProcessor = archiveFactory->createArchiveProcess() ;
    if( nodeListIter == nodesList.end() )
    {
        deleteProcessor->setArchiveAddress(confObj->getDNSname()) ;
    }
    else
    {
	    deleteProcessor->setArchiveAddress((*nodeListIter).m_szIp) ;
    }
	deleteProcessor->setArchiveRootDir(confObj->getArchiveRootDirectory()) ;
	deleteProcessor->setPort((*nodeListIter).m_Port) ;
	deleteProcessor->setProcessQueue(deleteQPtr) ;
	deleteProcessor->markAsDeleteQueue() ;
	deleteProcessor->open() ;
	
	if( ConfigValueObject::getInstance()->getTransport().compare("http") == 0 ||
		ConfigValueObject::getInstance()->getTransport().compare("nfs") == 0 )
	{
		REMOTE_PATH_SEPERATOR = '/' ; 
	}
	else
	{
		REMOTE_PATH_SEPERATOR = '\\' ; 
	}
    
    m_tracker->markDeletePendingToSuccess();

	while(	true)
	{
		list.clear() ;
		m_tracker->getDeleteableObjects(list, 100) ;
		std::list<ResumeTrackerObj>::iterator begin = list.begin() ;
		if(list.empty() )
		{
			DebugPrintf(SV_LOG_DEBUG, "Nothing to delete from archive store\n") ;
			break ;
		}

		while( begin != list.end() && 
			!QuitSignalled() )
		{
			ResumeTrackerObj obj = (*begin) ;
			std::string url = obj.m_tgtPath ;
			if( obj.m_dirPath.compare("") != 0 )
			{
				url += REMOTE_PATH_SEPERATOR ;
				url += obj.m_dirPath ;
			}
			if( !obj.m_isDir )
			{
				url += REMOTE_PATH_SEPERATOR ;
				url += obj.m_fileName ;
			}
			
			ACE_Message_Block *mb ;
			fileSystemObj * msg  = NULL ;
			if( obj.m_isDir )
			{
				msg = new (std::nothrow) fileSystemObj( url,
					obj.m_dirId, 
					obj.m_isDir, 
					obj.m_dirPath ) ;
			}
			else
			{
				msg = new (std::nothrow) fileSystemObj(url,
					obj.m_dirId, 
					obj.m_isDir, 
					obj.m_fileName) ;
			}
			
			if( msg == NULL )
			{
				throw icatOutOfMemoryException("Failed to create the file system object\n") ;
			}

			mb = new (std::nothrow) ACE_Message_Block(sizeof(*msg),
				ACE_Message_Block::MB_DATA, 
				NULL, 
				(const char*) msg) ;

			if( mb == NULL )
			{
				throw icatOutOfMemoryException("Failed to create message block\n") ;
			}

			if( obj.m_isDir )
			{
				DebugPrintf(SV_LOG_DEBUG, "enqueued directory %s for deletion\n", obj.m_dirPath.c_str()) ;
				m_tracker->updateDeleteStatus(obj.m_dirPath, obj.m_dirId, obj.m_isDir, ICAT_RESOBJ_PENDINGDEL) ;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Enqueued file %s for deletion\n", obj.m_fileName.c_str()) ;
				m_tracker->updateDeleteStatus(obj.m_fileName, obj.m_dirId, obj.m_isDir, ICAT_RESOBJ_PENDINGDEL) ;
			}			
			deleteQPtr->queue()->enqueue_tail(mb) ;			
			begin++ ;
		}
	}
	deleteQPtr->setState(ARC_Q_DONE) ;
	deleteProcessor->wait() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return 0 ;
}
void archiveController::dumpTransferStat()
{
    DebugPrintf(SV_LOG_DEBUG, "\n =====================Transfer Statistics================\n") ;
    DebugPrintf(SV_LOG_DEBUG, "Total Number Of Files in Included dirs:"ULLSPEC "\n",m_tranferStat.total_Num_Of_Files.value()) ;
    DebugPrintf(SV_LOG_DEBUG, "Total Number Of Dirs:" ULLSPEC "\n",m_tranferStat.total_Num_Of_Dirs.value()) ;
    DebugPrintf(SV_LOG_DEBUG, "Total Number Of Transfered Files:" ULLSPEC "\n",m_tranferStat.total_Num_Of_Transfered_Files.value()) ;
    DebugPrintf(SV_LOG_DEBUG, "Total Number Of Skipped Dirs:" ULLSPEC "\n",m_tranferStat.total_Num_Of_Excluded_Dirs.value()) ;
    DebugPrintf(SV_LOG_DEBUG, "Total Number Of Bytes sent: %lf \n\n",m_tranferStat.total_Num_Of_Bytes_Sent.value()) ; 
}

