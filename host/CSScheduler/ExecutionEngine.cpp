#pragma warning(disable: 4786)

#include "ExecutionController.h"
#include "ExecutionEngine.h"
#include "queue.h"
#include "IRepositoryManager.h"
#include "thread.h"

using namespace sch;

extern Queue<PERP> exeQueue;

ExecutionEngine *ExecutionEngine::m_Instance = NULL;

void ExecutionEngine::run()
{
	Logger::Log("ExecutionEngine.cpp", __LINE__, 0, 0, "ExecutionEngine::run");
	while(!m_Stop)
	{
		if(exeQueue.size()>0)
		{
			PERP perp = exeQueue.dequeue();

			TaskDefinition* tskdef = static_cast<TaskDefinition*>(perp->getInformation());

			THREAD_T thrId=0;
			int hr = pthreadCreate(&thrId,ExecutionAgent::executeERP,perp);

			if( hr != SCH_SUCCESS)
			{
				if(hr == SCH_EAGAIN)
				{
					/*
					   Check the service stop flag before & after sleep.
					   If m_stop is set to true then exit loop without waiting sleep.
					*/
					if(m_Stop) break;

					sch::Sleep(100);

					if(m_Stop) break; // Don't start thread if stop set to true.

					hr = pthreadCreate(&thrId,ExecutionAgent::executeERP,perp);

					if(hr != SCH_SUCCESS)
					{
						Logger::Log("ExecutionEngine.cpp",__LINE__,1,1,"Thread Creation Failed Insufficient Resources with error=%d",hr);
						if( tskdef != NULL )
                             {
                                  Logger::Log("ExecutionEngine.cpp",__LINE__,1,1,"Dropping ERP:%d",tskdef->taskId);
                             }
					}
				}
				else
				{
					Logger::Log("ExecutionEngine.cpp",__LINE__,1,1,"Thread Creation Failed with error=%d",hr);
                        if( tskdef != NULL )
                        {
                         Logger::Log("ExecutionEngine.cpp",__LINE__,1,1,"Dropping ERP:%d",tskdef->taskId);
                        }
					
						if(m_Stop) break; // Exit the loop without sleep if stop is set to true.

					sch::Sleep(30000);
				}
			}			
		}
		else
		{
			if(m_Stop) break; // Exit the loop without slepp if stop is set to true.
			sch::Sleep(10000); // 10 - 10000
		}
	}
	Logger::Log("ExecutionEngine.cpp", __LINE__, 0, 0, "Exiting ExecutionEngine::run");
}

void ExecutionEngine::notify(PERP perp)
{
	//Logger::Log("ExecutionEngine.cpp", __LINE__, 0, 0, "ExecutionEngine::notify");
	ExecutionController *ec = ExecutionController::instance();
	if( perp != NULL && ec != NULL ) 
		ec->notify(perp);
	//Logger::Log("ExecutionEngine.cpp", __LINE__, 0, 0, "Exiting ExecutionEngine::notify");
}
