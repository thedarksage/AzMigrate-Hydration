#ifndef __EXECUTION_ENGINE_H_
#define __EXECUTION_ENGINE_H_

#include "ERP.h"
#include "Log.h"
#include "thread.h"
using namespace sch;
class ExecutionEngine
{
	static ExecutionEngine* m_Instance;
	bool m_Stop;

	ExecutionEngine(ExecutionEngine&){	}

	ExecutionEngine(){
		m_Stop = false;
	}

	ExecutionEngine &operator =(ExecutionEngine&){
		return *this;
	}

public:

	void run();
	void notify(PERP perp);

	static ExecutionEngine * instance()
	{
		if(m_Instance == 0)
		{
			m_Instance = new ExecutionEngine;
		}

		return m_Instance;
	}

	void stopEngine(){ 
        m_Stop = true;
	}
};

class ExecutionAgent
{
	void execute(PERP perp)
	{
		switch(perp->getType())
		{
		case 0:
			//
			//Execute process
			//
			break;

		case 1:
			//
			//Execute Script
			//
			break;

		case 2:
			{
				//
				//Invoke method on object
				//
				BaseObject *obj = perp->getObject();

				if(obj == NULL)
				{
					perp->setExecutionStatus(SCH_ERROR);

					Logger::Log(__FILE__,__LINE__,1,2,"No Target object specified.");
					Logger::Log(__FILE__,__LINE__,1,2,"Could not execute request.");
				}
				else
				{
					SCH_STATUS stat = obj->execute(perp);
					perp->setExecutionStatus(stat);
				}

				ExecutionEngine *ee = ExecutionEngine::instance();

				if(ee != NULL)
				{
					ee->notify(perp);
				}
			}

		default:
			break;

		}
	}

public:

	ExecutionAgent(){ }
	
	static void executeERP(void *context)
	{
		ExecutionAgent *ea = new ExecutionAgent;

		if(ea == NULL)
		{
			Logger::Log(__FILE__,__LINE__,1,2,"Could not create execution agent.");
			return;
		}

		PERP perp = static_cast<PERP>(context);

		if(perp != NULL)
		{
			ea->execute(perp);
		}

		int ret = 0;
		Logger::Log(__FILE__, __LINE__, 0, 1, "Thread Exited");
		SAFE_DELETE( perp );
		SAFE_DELETE( ea );

		pthreadExit(&ret);		
	}
};

#endif
