#include "snapshotrequestprocessor.h"
#include "cdpsnapshotrequest.h"
#include "theconfigurator.h"
#include "configurevxagent.h"
#include "fileconfigurator.h"
#include "cdputil.h"
#include "configwrapper.h"
#include "inmageex.h"

//The method should be called only if it is OktoProcessDiffs. 
//assumes the volume is in diff sync(not in resync) and volume is not visible and not marked for rollback

SnapShotRequestProcessor::SnapShotRequestProcessor(Configurator * theConfigurator)
{
	m_Configurator = theConfigurator;
	m_SnapshotTasks.clear();
	m_MsgQueue.reset(new ACE_Message_Queue<ACE_MT_SYNCH>());
}

bool SnapShotRequestProcessor::processSnapshotRequest(std::string volumename)
{
	bool rv = true;
	svector_t bookmarks;
	bookmarks.clear();

	return processSnapshotRequest(volumename,bookmarks);
}

bool SnapShotRequestProcessor::processSnapshotRequest(std::string volumename,svector_t &bookmarks)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",FUNCTION_NAME);
	bool rv = true;
	FormatVolumeName(volumename);

	SNAPSHOT_REQUESTS requests;

	try
	{
		requests = m_Configurator ->getSnapshotRequests(volumename, bookmarks );
	}
	catch ( ContextualException& ce )
	{
		DebugPrintf(SV_LOG_ERROR,
			"\ngetSnapshotRequests call to cx failed with exception %s.\n",ce.what());
		rv = false;
		return rv;
	}
	catch ( ... )
	{
		DebugPrintf(SV_LOG_ERROR,
			"\ngetSnapshotRequests call to cx failed with unknown exception.\n");
		rv = false;
		return rv;
	}

	SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
	SNAPSHOT_REQUESTS::iterator requests_end = requests.end();

	for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
	{
		std::string id = (*requests_iter).first;
		SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;

		if ( request ->eventBased )
		{
			//
			// PR #5393
			// wrap all configurator snapshot related calls
			// move the snapshot instance to ready on arrival of event
			// if it fails, ignore and continue to serve other requests
			//
			if(!makeSnapshotActive( (*m_Configurator),request -> id))
				continue;
		}

		SnapshotTaskPtr snapTask(new SnapshotTask(id, m_MsgQueue,request, &m_ThreadManager));
		if( -1 == snapTask ->open())
		{
			DebugPrintf(SV_LOG_ERROR, "FAILED SnapShotRequestProcessor::processSnapshotRequest vol=%s snapTask.open: %d\n",volumename.c_str() , ACE_OS::last_error());
			rv = false;
			continue;
		}

		m_SnapshotTasks.insert(m_SnapshotTasks.end(), SNAPSHOTTASK_PAIR(id,snapTask));
	}

	if(m_SnapshotTasks.empty())
		return rv;

	waitForSnapshotTasks();

	m_SnapshotTasks.clear();
	DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
	return rv;
}

void SnapShotRequestProcessor::waitForSnapshotTasks()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	//__asm int 3;
	bool quit = false;
	unsigned TaskDoneCount = 0;

	while(true)
	{
		ACE_Message_Block *mb;
		ACE_Time_Value waitTime = ACE_Time_Value::max_time;
		if (-1 == m_MsgQueue -> dequeue_head(mb, &waitTime))
		{
			if (errno != EWOULDBLOCK && errno != ESHUTDOWN)
			{
				quit = true;
				break;
			}
		} else {

			CDPMessage * msg = (CDPMessage*)mb ->base();

			//Send update to cx
			notifySnapshotProgressToCx(msg);

			if (msg -> executionState == SNAPSHOT_REQUEST::Complete
				|| msg -> executionState == SNAPSHOT_REQUEST::Failed)
				++TaskDoneCount;

			delete msg;
			mb -> release();

			if(TaskDoneCount >= m_SnapshotTasks.size())
				break;
		}

		if(CDPUtil::QuitRequested(0))
		{
			quit = true;
			break;
		}

#if 0 // BUG 617 DISABLED (enabled once cx is ready)
		StopAbortedSnapshots(tasks, mq);
#endif
	}

	if(quit)
	{
		// need to tell all snapshot tasks to quit
		SnapshotTasks_t::iterator iter(m_SnapshotTasks.begin());
		SnapshotTasks_t::iterator end(m_SnapshotTasks.end());
		for (/* empty */; iter != end; ++iter) {
			SnapshotTaskPtr taskptr = (*iter).second;
			taskptr ->PostMsg(CDP_QUIT, CDP_HIGH_PRIORITY);
		}
	}

	// Note:Do not call threadmanager.wait
	// as this function is invoked from a thread managed
	// by threadmanager
	//m_ThreadManager.wait();

	//wait for snapshot tasks
	SnapshotTasks_t::iterator iter(m_SnapshotTasks.begin());
	SnapshotTasks_t::iterator end(m_SnapshotTasks.end());
	for (/* empty */; iter != end; ++iter) {
		SnapshotTaskPtr taskptr = (*iter).second;
		m_ThreadManager.wait_task(taskptr.get());
	}

	//drain any pending messages
	while(true)
	{
		ACE_Message_Block *mb;
		ACE_Time_Value waitTime;
		if (-1 == m_MsgQueue -> dequeue_head(mb, &waitTime))
		{
			if (errno == EWOULDBLOCK || errno == ESHUTDOWN)
			{
				break;
			}
		}

		CDPMessage * msg = (CDPMessage*)mb ->base();

		//Send update to cx
		notifySnapshotProgressToCx(msg);

		delete msg;
		mb -> release();
	}

	DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);

}
//
// Function to invoke appropriate configurator call depending on CDPMessage Update or progress
//

bool  SnapShotRequestProcessor::notifySnapshotProgressToCx(const CDPMessage * msg)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	int timeval = 0 , rpoint = 0 , state =0 ;
	SV_ULONGLONG VsnapId=0;

	if(msg ->operation == SNAPSHOT_REQUEST::PIT_VSNAP
		|| msg ->operation == SNAPSHOT_REQUEST::RECOVERY_VSNAP
		|| msg ->operation == SNAPSHOT_REQUEST::VSNAP_UNMOUNT )
	{

		if(msg ->type != CDPMessage::MSG_STATECHANGE )
			return true;

		if(msg ->executionState != SNAPSHOT_REQUEST::Failed)
			return true;
	}


	std::stringstream cxstring;
	if (msg ->type == CDPMessage::MSG_STATECHANGE)
	{
		if(msg ->executionState == SNAPSHOT_REQUEST::SnapshotPrescriptRun
			|| msg ->executionState == SNAPSHOT_REQUEST::RecoveryPrescriptRun
			|| msg ->executionState == SNAPSHOT_REQUEST::RecoveryPrescriptRun
			|| msg ->executionState == SNAPSHOT_REQUEST::SnapshotPostscriptRun
			|| msg ->executionState == SNAPSHOT_REQUEST::RecoveryPostscriptRun
			//|| msg ->executionState == SNAPSHOT_REQUEST::RollbackPrescriptRun
			|| msg ->executionState == SNAPSHOT_REQUEST::RollbackPostScriptRun)
			return true;

		switch (msg ->executionState)
		{
		case SNAPSHOT_REQUEST::Ready:
			state = 1 ;
			break;

		case SNAPSHOT_REQUEST::Complete:
			state = 4 ;
			break;

		case SNAPSHOT_REQUEST::Failed:
			state = 5 ;
			break;

		case SNAPSHOT_REQUEST::SnapshotStarted:
		case SNAPSHOT_REQUEST::RecoveryStarted:
			state = 2 ;
			break;

		case SNAPSHOT_REQUEST::SnapshotInProgress:
		case SNAPSHOT_REQUEST::RecoverySnapshotPhaseInProgress:
			state = 3 ;
			break;

		case SNAPSHOT_REQUEST::RecoveryRollbackPhaseStarted:
		case SNAPSHOT_REQUEST::RollbackStarted:
			state = 6 ;
			break;

		case SNAPSHOT_REQUEST::RecoveryRollbackPhaseInProgress:
		case SNAPSHOT_REQUEST::RollbackInProgress:
			state = 7 ;
			break;
		}
		if(state)
		{
			//
			// PR #5393
			// wrap all configurator snapshot related calls
			//
			if(!notifyCxOnSnapshotStatus((*m_Configurator), msg->id , timeval ,VsnapId, msg ->err , state))
			{
				return false;
			}
		}


	}
	else        if (msg ->type == CDPMessage::MSG_PROGRESSUPDATE)
	{
		//
		// PR #5393
		// wrap all configurator snapshot related calls
		//
		if(!notifyCxOnSnapshotProgress((*m_Configurator),msg->id , msg->progress , rpoint))
		{
			return false;
		}

	}
	DebugPrintf(SV_LOG_DEBUG,"Leaving %s\n",FUNCTION_NAME);
	return true;
}

