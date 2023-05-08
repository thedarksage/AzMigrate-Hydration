#pragma once
#include <list>
#include <map>
#include <boost/shared_ptr.hpp>
#include <sstream>
#include <fstream>
#include <sigslot.h>

#include "task.h"

class TaskExecution ;
typedef boost::shared_ptr<TaskExecution> TaskExecutionPtr ;

class TaskExecution : public sigslot::has_slots<>
{
private:	
    static TaskExecutionPtr m_TaskObjPtr; 
    std::map<SV_ULONG,TaskInformation> m_ExecutionTaskMap ;
    sigslot::signal1<SV_ULONG> m_taskTriggerSignal ;
public:
    static TaskExecutionPtr getInstance() ;
	void SchedulePendingTasks() ;
    void schedule();
    std::map<SV_ULONG,TaskInformation>& gettasks() ;
    void settasks(std::map<SV_ULONG,TaskInformation> tasks) ;
	void getEligibleTasks(std::map<SV_ULONG,TaskInformation> tempTaskMap);
    void TriggerTask(SV_ULONG) ;
    void loadDefaultTasks() ;
    sigslot::signal1<SV_ULONG >& getTaskTriggerSignal() ;
    
};
