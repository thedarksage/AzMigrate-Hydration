#include "appscheduler.h"
#include <boost/lexical_cast.hpp>
#include <boost/shared_array.hpp>
#include "serializetask.h"
#include "cdputil.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <exception>

#include "LogCutter.h"

using namespace Logger;

Log gSchedTelemetryLog;

extern ACE_Recursive_Thread_Mutex appconfigLock ;
AppSchedulerPtr AppScheduler::m_schedulerInstancePtr ;

void AppScheduler::ApplicationScheduleCallback(std::map<SV_ULONG, TaskInformation> tasks)
{
    DebugPrintf(SV_LOG_DEBUG, "Got Scheduler callback\n") ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    updateTasks(tasks) ;
    PersistScheduleInfo() ;
}

AppScheduler::AppScheduler(ACE_Thread_Manager* threadManager, const std::string& cachePath) 
:ACE_Task<ACE_MT_SYNCH>(threadManager),
m_settingsChanged(false),
m_cachePath(cachePath),
m_active(true)
{

}
AppSchedulerPtr AppScheduler::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_schedulerInstancePtr.get()  == NULL )
    {
        throw "getInstance is called before create instance\n" ;
    }
    return m_schedulerInstancePtr ;
}

void AppScheduler::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<SV_ULONG,TaskInformation> ActiveScheduleTasks ;
    AppSchedulingInfo schedulingInfo ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    if( m_taskExecutionPtr.get() == NULL )
    {
        m_taskExecutionPtr = TaskExecution::getInstance() ;
    }
    try
    {
        if( UnserializeSchedulingInfo(ActiveScheduleTasks) != SVS_OK )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unserialize the scheduling Info\n") ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Previously saved scheduling information loaded to memory. Number of tasks %d\n",ActiveScheduleTasks.size() ) ;
        }
    }
    catch(std::exception &ex)
    {
        DebugPrintf(SV_LOG_INFO,"Exception caught while unserializing scheduling info.Ignoring it safely.%s\n",ex.what());
    }
    m_taskExecutionPtr->settasks(ActiveScheduleTasks) ; 
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppScheduler::loadTasks(const std::map<SV_ULONG, TaskInformation> newTasks) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<SV_ULONG, TaskInformation>& existingTasks = m_taskExecutionPtr->gettasks() ;
    std::map<SV_ULONG, TaskInformation>::const_iterator newTasksIter = newTasks.begin() ;
    for (/*empty*/; newTasksIter != newTasks.end(); newTasksIter++)
    {
        bool insertTask = false ;
        std::map<SV_ULONG, TaskInformation>::iterator existingTaskIter = existingTasks.find(newTasksIter->first) ;
        if( existingTaskIter == existingTasks.end() )
        {
            insertTask = true ; 
        }
        else
        {
            if( existingTaskIter->second.m_RunEvery != newTasksIter->second.m_RunEvery || 
                existingTaskIter->second.m_ScheduleExcludeFromTime != newTasksIter->second.m_ScheduleExcludeFromTime ||
                existingTaskIter->second.m_ScheduleExcludeToTime != newTasksIter->second.m_ScheduleExcludeToTime ||
                existingTaskIter->second.m_bIsActive != newTasksIter->second.m_bIsActive ||
                newTasksIter->second.m_bRemoveFromExisting)
            {
                existingTasks.erase( existingTaskIter ) ;
                insertTask = true ;
            }
        }
        if( insertTask )
        {
            DebugPrintf(SV_LOG_DEBUG, "Inserting the task %ld\n", newTasksIter->first) ;
            existingTasks.insert(std::make_pair(newTasksIter->first, newTasksIter->second)) ;
        }
    }
    m_taskExecutionPtr->settasks(existingTasks) ; 
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppScheduler::start()
{
    if ( -1 == open() ) 
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED AppScheduler::init failed to open: %d\n", 
            ACE_OS::last_error());
        throw "Failed to start scheduler thread\n" ;
    }
}

int AppScheduler::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

int AppScheduler::close(u_long flags)
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return 0 ;
}



void AppScheduler::updateTasks( std::map<SV_ULONG, TaskInformation> tasks )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_taskExecutionPtr->getEligibleTasks(tasks) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

int AppScheduler::svc()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    {
        boost::shared_ptr<Logger::LogCutter> telemetryLogCutter(new Logger::LogCutter(gSchedTelemetryLog));

        std::string logPath;
        if (GetLogFilePath(logPath))
        {
            boost::filesystem::path schedLogPath(logPath);
            schedLogPath /= "schedulerTelemetry.log";
            gSchedTelemetryLog.m_logFileName = schedLogPath.string();

            // TODO : Move to config params
            const int maxCompletedLogFiles = 3;
            const boost::chrono::seconds cutInterval(300); // 5 mins.

            telemetryLogCutter->Initialize(schedLogPath, maxCompletedLogFiles, cutInterval);
            telemetryLogCutter->StartCutting();
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s unable to init sched telemetry logger\n", FUNCTION_NAME);
        }

        m_taskExecutionPtr->SchedulePendingTasks();
        do
        {
            ACE_Guard<ACE_Recursive_Thread_Mutex> lock(appconfigLock);
            m_taskExecutionPtr->schedule();
        } while (!QuitRequested(10));
        PersistScheduleInfo();
    }
    gSchedTelemetryLog.CloseLogFile();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return 0 ;
}

void AppScheduler::UpdateTaskStatus(SV_ULONG taskId,InmTaskState state)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME) ;	
    if( !QuitRequested(1) )	
    {
        ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
        std::map<SV_ULONG,TaskInformation>& tasks = m_taskExecutionPtr->gettasks();
        std::map<SV_ULONG,TaskInformation>::iterator taskIter = tasks.find(taskId);
        if (taskIter != tasks.end())
        {
            taskIter->second.m_State = state ;
            if(	state == TASK_STATE_COMPLETED || state == TASK_STATE_FAILED)
            {
                if(taskIter->second.m_RunEvery == 0)
                {
                    DebugPrintf(SV_LOG_DEBUG,"Task %ld is RunOnce,Erasing from map\n", taskId);
                    tasks.erase(taskId);
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG,"Task %ld is Runevery,Setting status as TASK_STATE_NOT_STARTED\n",taskId);
                    taskIter->second.m_State = TASK_STATE_NOT_STARTED;
                }
            }
            else if(state == TASK_STATE_DELETED && taskId != 0)
            {
                DebugPrintf(SV_LOG_DEBUG,"Task %ld state is DELETED,Erasing from map\n", taskId);
                tasks.erase(taskId);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Task %ld not found in map\n", taskId);
        }
        PersistScheduleInfo() ;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

SVERROR AppScheduler::PersistScheduleInfo()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME) ;	
    SVERROR bRet = SVS_FALSE ;
    AppSchedulingInfo schedulingInfo ;

    std::map<SV_ULONG,TaskInformation> tasks = m_taskExecutionPtr->gettasks();
    schedulingInfo.m_activeTasks = tasks ;
    if( SerializeSchedulingInfo(schedulingInfo) == SVS_OK )
    {
        DebugPrintf(SV_LOG_DEBUG, "Serialization Successful\n") ;
        bRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to Serialize scheduling information\n") ;
    }    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet ;
}

SVERROR AppScheduler::UnserializeSchedulingInfo(std::map<SV_ULONG, TaskInformation>& taskMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::ifstream inFile;
    unsigned int length =0;
    AppSchedulingInfo objAppSchedulingInfo;
    std::string cachePath;
    std::string lockPath;
    cachePath =  m_cachePath ;
    SV_ULONG fileSize = 0 ;
    inFile.open(cachePath.c_str(), std::ios::in) ;
    if( inFile.good() )
    {
        inFile.seekg(0, std::ios::end) ;
        fileSize = inFile.tellg() ;
        inFile.close() ;
    }
    // open the local store for read operation
    if( fileSize != 0 )
    {
        inFile.open(cachePath.c_str(), std::ios::in);
        if (!inFile.is_open())
        {
            throw "File could not be opened in read mode\n" ;
        }

        // get length of file
		inFile.seekg (0, std::ios::end);
        length = inFile.tellg();
		inFile.seekg (0, std::ios::beg);

        // allocate memory:
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned>::Type(length) + 1, INMAGE_EX(length))
        boost::shared_array<char> buffer(new char[buflen]);
        if(!buffer) 
        {
            throw "couldnot allocate enough memory for buffer\n" ;
            inFile.close();
        }

        // read initialsettings as binary data
        inFile.read (buffer.get(),length);
        *(buffer.get() + length) = '\0';
        // close the handle
        inFile.close();
        try
        {
            objAppSchedulingInfo =  unmarshal<AppSchedulingInfo> (buffer.get());
            taskMap = objAppSchedulingInfo.m_activeTasks;
            bRet = SVS_OK ;
        }
        catch ( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch(std::exception ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal AppSchedulingInfo %s\n", ex.what()) ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Unknown exception while unserializing the scheduling information\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "No Scheduled tasks are available\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR AppScheduler::SerializeSchedulingInfo(const AppSchedulingInfo& objAppSchedulingInfo )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        std::ofstream out;
        std::string cachePath;
        cachePath = m_cachePath ;

        out.open(cachePath.c_str(), std::ios::trunc);
        if (!out.is_open()) 
        {
            throw "File could not be opened in rw mode\n" ;
        }
        out <<  CxArgObj<AppSchedulingInfo>(objAppSchedulingInfo);
        out.flush() ;
        out.close();
        bRet = SVS_OK;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to marshalling AppSchedulingInfo %s\n", ex.what()) ;
    }
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to cache the Application settings.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool AppScheduler::QuitRequested(int seconds)
{
    if (seconds > 0)
    {
        for (unsigned int s = 0; s < seconds; s++)
        {
            if ( !isActive() ) 
            {
                return true;
            }
            ACE_OS::sleep(1);
        }
    }
    return (!isActive()) ;
}

bool AppScheduler::isActive()
{
    return m_active ;
}

void AppScheduler::stop()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_active = false ;
}

AppSchedulerPtr AppScheduler::createInstance(ACE_Thread_Manager* thrMgr, const std::string& cachePath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_schedulerInstancePtr.get() == NULL )
    {
        m_schedulerInstancePtr.reset( new AppScheduler( thrMgr, cachePath ) ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_schedulerInstancePtr ;
}

bool AppScheduler::isPolicyEnabled(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    bool enabled = false ;
    std::map<SV_ULONG, TaskInformation>& existingTasks = m_taskExecutionPtr->gettasks() ;
    std::map<SV_ULONG, TaskInformation>::const_iterator taskIter = existingTasks.find( policyId ) ;
    if( existingTasks.find( policyId ) != existingTasks.end() )
    {
        enabled  = taskIter->second.m_bIsActive;
    }
    DebugPrintf(SV_LOG_DEBUG,"Task state is %d\n",enabled);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return enabled ;
}
SV_ULONGLONG AppScheduler::getInstanceId(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ; 	
    SV_ULONGLONG instanceId = 0;
    std::map<SV_ULONG, TaskInformation>& existingTasks = m_taskExecutionPtr->gettasks() ;
    std::map<SV_ULONG, TaskInformation>::const_iterator taskIter = existingTasks.find( policyId ) ;
    if( existingTasks.find( policyId ) != existingTasks.end() )
    {
        instanceId = existingTasks.find( policyId )->second.m_InstanceId;
        DebugPrintf(SV_LOG_DEBUG, "Policy Id %ld Instance Id" ULLSPEC "\n", policyId, instanceId) ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy Id %ld Inot found in scheduler tasks map\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return instanceId ;
}
SV_ULONGLONG AppScheduler::getTaskFrequency(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SV_ULONGLONG taskFrequency = 0;
    std::map<SV_ULONG, TaskInformation>& existingTasks = m_taskExecutionPtr->gettasks() ;
    std::map<SV_ULONG, TaskInformation>::const_iterator taskIter = existingTasks.find( policyId ) ;
    if( existingTasks.find( policyId ) != existingTasks.end() )
    {
        taskFrequency = existingTasks.find( policyId )->second.m_RunEvery;
        DebugPrintf(SV_LOG_DEBUG, "Policy Id %ld Frequency is " ULLSPEC "\n", policyId, taskFrequency) ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy Id %ld Inot found in scheduler tasks map\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return taskFrequency ;
}
std::list<SV_ULONG> AppScheduler::getActivePolicies()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   	ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
   	std::list<SV_ULONG> policies ;
    std::map<SV_ULONG, TaskInformation>& existingTasks = m_taskExecutionPtr->gettasks() ;
    std::map<SV_ULONG, TaskInformation>::const_iterator taskIter = existingTasks.begin() ;
    while( taskIter != existingTasks.end() )
    {
        policies.push_back( taskIter->first) ;
        taskIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return policies ;
}

