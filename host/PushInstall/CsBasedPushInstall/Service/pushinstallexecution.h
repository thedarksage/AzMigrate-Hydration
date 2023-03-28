#ifndef __PUSHINSTALL_EXEC__
#define __PUSHINSTALL_EXEC__

#include <vector>
#include <boost/shared_ptr.hpp>
#include <ace/Thread_Manager.h>

#include "pushconfig.h"
#include "pushjob.h"

namespace PI {

	class PushInstallerThread
	{

	public:

		PushInstallerThread(PushJobDefinition& jobInfoObj,
			ACE_Thread_Manager* tManager)
			: m_jobInfo(jobInfoObj),
			m_threadid(-1),
			m_bIsActive(false),
			m_ThreadManager(tManager)
		{
		}

		~PushInstallerThread()
		{
			if (-1 != m_threadid) {
				Wait();
			}
		}

		SVERROR Start();

		ACE_thread_t GetThreadId() const { return m_threadid; }
		bool IsActive() const { return m_bIsActive; }

	private:

		unsigned long Wait()  { return m_ThreadManager->join(m_threadid); }
		void stop() { m_bIsActive = false; }

		static void* EntryPoint(void* pThis);
		SVERROR doWork();

		PushJobDefinition m_jobInfo;
		ACE_Thread_Manager* m_ThreadManager;

		ACE_thread_t m_threadid;
		bool m_bIsActive;
	};

	typedef boost::shared_ptr<PushInstallerThread> PushInstallerThreadPtr;
}


#endif
