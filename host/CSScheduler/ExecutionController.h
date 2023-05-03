#ifndef __EXECUTION_CONTROLLER_H_
#define __EXECUTION_CONTROLLER_H_
#include "EventGenarator.h"
#include "ERP.h"
#include "thread.h"
#include "ExecutionEngine.h"

using namespace sch;

class ExecutionController
{
	EventGenarator evntgen;

	static ExecutionController* m_instance;

	ExecutionController()
	{
		m_Stop = false;
		evntgen.init();
	}

	ExecutionController(ExecutionController&){ }

	ExecutionController& operator =(ExecutionController&){
		return *this;
	}

	bool executeERP(PERP erp,IRepositoryManager* repmgr);

	static void pollNewJobs(void* context);

	static bool m_Stop;

    static THREAD_T m_ThreadHandlePollNewJobs;

public:

	static ExecutionController* instance()
	{
		//This method is not thread safe.
		if(m_instance == 0)
		{
			m_instance = new ExecutionController;

			int hr = pthreadCreate(&m_ThreadHandlePollNewJobs,pollNewJobs,m_instance);

            if(hr != SCH_SUCCESS)
				SAFE_DELETE( m_instance );
		}
		return m_instance;
	}
	int setEvents(time_t starttime,PERP perp)
	{
		return evntgen.setEvent(starttime,perp);
	}

	bool execute(PERP erp,IRepositoryManager* repmgr);		//execute once

	void schedule(PERP erp,IRepositoryManager* repmgr);		//Schedule for next exeuction

	bool executeAndSchedule(PERP erp,IRepositoryManager* repmgr);	//execute and schedule for next execution

	bool start(PERP erp,IRepositoryManager* repmgr);			//Start asyncronously the scheduled erp and recalculate the schedule.

	bool stop();							//stop the next executing of the erp

	bool updateStatus(ERPID id);			//update status of the executed erp

	bool removeERP(ERP* erp);				//remove from the schedule the erp

	void notify(ERP* erp);					//Callback from the ExecutionEngine after completion
											//of execution

	void LoadERPQueue(IRepositoryManager* repmgr);	//Load new erps from the repository manager
    
    void stopController();
};

typedef ExecutionController EC;
typedef ExecutionController* PEC;

#endif
