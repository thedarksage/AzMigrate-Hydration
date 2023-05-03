#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Task.h>
#include <sigslot.h>
#include <boost/shared_ptr.hpp>
#include "task.h"
#include "taskexecution.h"
class AppScheduler ; //forward declaration
typedef boost::shared_ptr<AppScheduler> AppSchedulerPtr ;

class AppScheduler: public sigslot::has_slots<>,public ACE_Task<ACE_MT_SYNCH>
{
    std::string m_cachePath ;
    bool m_active ;
    TaskExecutionPtr m_taskExecutionPtr ;
    AppScheduler(ACE_Thread_Manager* threadManager, const std::string& cachePath)  ;
    static AppSchedulerPtr m_schedulerInstancePtr ;
    bool m_settingsChanged ;
    void updateTasks( std::map<SV_ULONG, TaskInformation> tasks ) ;
	mutable ACE_Recursive_Thread_Mutex m_lock;
    typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuard;
    SVERROR PersistScheduleInfo() ;
    SVERROR UnserializeSchedulingInfo(std::map<SV_ULONG, TaskInformation>& taskMap) ;
    SVERROR SerializeSchedulingInfo(const AppSchedulingInfo& objAppSchedulingInfo ) ;
    bool QuitRequested(int seconds) ;
	void DoWork() ;
    bool isActive() ;
    void combinePendingTasks(std::map<SV_ULONG, TaskInformation>& ActiveScheduleTasks, 
                                                    const std::map<SV_ULONG, TaskInformation>& tasksFromOtherStore) ;
public:
    int open(void *args = 0) ;
    int close(u_long flags = 0) ;
    int svc() ;
	void init() ;
    void start() ;
    void stop() ;
	void ApplicationScheduleCallback(std::map<SV_ULONG, TaskInformation>);
    static AppSchedulerPtr getInstance() ;
    static AppSchedulerPtr createInstance(ACE_Thread_Manager* thrMgr, const std::string& cachePath) ;
	void UpdateTaskStatus(SV_ULONG taskId,InmTaskState state) ;
    void loadTasks(const std::map<SV_ULONG, TaskInformation> tasks) ;
    bool isPolicyEnabled(SV_ULONG policyId);
    SV_ULONGLONG getInstanceId(SV_ULONG policyId);
    SV_ULONGLONG getTaskFrequency(SV_ULONG policyId);
    std::list<SV_ULONG> getActivePolicies() ;
} ;

#endif
