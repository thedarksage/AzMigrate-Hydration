#ifndef SNAPSHOTREQUESTPROCESSOR_H
#define SNAPSHOTREQUESTPROCESSOR_H

#include <ace/Task.h>
#include "cdpsnapshot.h"
#include <ace/Manual_Event.h>
#include <boost/shared_ptr.hpp>

class SnapShotRequestProcessor
{

public:
	SnapShotRequestProcessor(Configurator *);
	~SnapShotRequestProcessor(){};

	bool processSnapshotRequest(std::string volumename);
	bool processSnapshotRequest(std::string volumename,svector_t &bookmarks);
	void waitForSnapshotTasks();
	bool notifySnapshotProgressToCx(const CDPMessage * msg);

private:
	ACE_Thread_Manager m_ThreadManager;
	SnapshotTasks_t m_SnapshotTasks;
	boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > m_MsgQueue;
	Configurator * m_Configurator;
};

#endif