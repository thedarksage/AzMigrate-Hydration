#ifndef PUSH_PROXY_H
#define PUSH_PROXY_H

#include<string>
#include <map>

#include <ace/Task.h>
#include <ace/Mutex.h>
#include <ace/Process_Manager.h>
#include <ace/Process.h>

#include <boost/shared_ptr.hpp>

#include "CsCommunicationImpl.h"
#include "pushconfig.h"
#include "pushjob.h"
#include "pushinstallexecution.h"
#include "pushInstallationSettings.h"

#include "cdputil.h"

namespace PI {

	enum JOB_STATUS
	{
		JOB_PENDING = 1,
		JOB_NOT_STARTED,
		JOB_STARTED,
		JOB_COMPLETED,
		JOB_FAILED,
	};

	struct ActiveJobInfo
	{
		std::string m_ipAddress;
		SV_ULONGLONG m_startTime;
		SV_ULONGLONG m_endTime;
		JOB_STATUS m_jobStatus;
		std::string m_jobId;

		ActiveJobInfo()
		{
			m_startTime = 0;
			m_endTime = 0;
			m_jobStatus = JOB_PENDING;
			m_jobId = "";
		}
	};

	class PushProxy : public ACE_Task<ACE_MT_SYNCH>
	{
	public:

		PushProxy();
		~PushProxy();

		virtual int svc();
		virtual int open(void *args = 0);
		virtual int close(u_long flags = 0);

		void onServiceDeletion();

		void stop() { m_stopThread = true; }
		bool stopped() { return m_stopThread; }

	protected:

	private:

		PushInstallationSettings m_settings;
		
		bool m_stopThread;
		int m_maxPushThreads;

		ACE_Thread_Manager m_ThreadManager;
		std::map<std::string, PushInstallerThreadPtr> m_pushInstallerThreadPtrMap;
		std::map<std::string, ActiveJobInfo> m_activeJobsMap;

		void dumpSettings();
		void RegisterPushInstaller();

		bool QuitRequested(int seconds)
		{
			if (CDPUtil::QuitRequested(seconds))  {
				m_stopThread = true;
			}
			return m_stopThread;
		}


		void createInstallationJobThreads();
		void startInstallationJobThreads();
		void checkAndUpdateInstallationJobStatus();
		void removeCompletedFailedJobs();
		bool getIPStateFromSettings(const std::string& ipAddress, enum PUSH_JOB_STATUS& cxJobStatus);
	};

	typedef boost::shared_ptr<PushProxy> PushProxyPtr;

}

#endif