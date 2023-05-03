#include <stdlib.h>
#include <boost/lexical_cast.hpp>
#include "taskexecution.h"
#include "portablehelpers.h"
#include "util.h"

#include "SchedTelemetry.h"
#include "LogCutter.h"

using namespace SchedTelemetry;
using namespace Logger;

extern Log gSchedTelemetryLog;

TaskExecutionPtr TaskExecution::m_TaskObjPtr ;

void TaskExecution::getEligibleTasks(std::map<SV_ULONG,TaskInformation> tempTaskMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<SV_ULONG,TaskInformation>::iterator tempIter;


    std::list<std::map<SV_ULONG, TaskInformation>::iterator> listPoliliesToRemove;
    tempIter = tempTaskMap.begin();
    for (/*empty*/; tempIter != tempTaskMap.end(); tempIter++)
    {
        DebugPrintf(SV_LOG_DEBUG, "Checking for the task Id %ld in the map\n", tempIter->first) ;
        std::map<SV_ULONG,TaskInformation>::iterator exeTaskIter= m_ExecutionTaskMap.find(tempIter->first);
        if( exeTaskIter == m_ExecutionTaskMap.end())
        {
            DebugPrintf(SV_LOG_INFO, "Config Id %ld is a new task\n", tempIter->first) ;
            if (tempIter->second.m_bRemoveFromExisting)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: Skipping scheduling for policy Id %lu - policy disabled\n", FUNCTION_NAME, tempIter->first);
                continue;
            }
            m_ExecutionTaskMap.insert(std::make_pair(tempIter->first,tempIter->second));
        }
        else //Same Task Id is found
        {
            if (tempIter->second.m_bRemoveFromExisting)
            {
                // Single vm consistency upgrade case - policy already loaded from scheduler cache but disabled by configurator after fetched a fresh from CS
                // Remove policy from scheduler - policy disabled
                DebugPrintf(SV_LOG_DEBUG, "%s: Removing policy Id %lu from scheduler - policy disabled\n", FUNCTION_NAME, tempIter->first);
                m_ExecutionTaskMap.erase(exeTaskIter);
                continue;
            }
            DebugPrintf(SV_LOG_INFO, "Config Id %ld is an old task\n", tempIter->first) ;
            if(exeTaskIter->second.m_RunEvery != tempIter->second.m_RunEvery)
            {
                DebugPrintf(SV_LOG_DEBUG, "earlier Frequency %ld and current Frequency %ld\n", 
                    exeTaskIter->second.m_RunEvery,
                    tempIter->second.m_RunEvery) ;
				exeTaskIter->second.m_StartTime = tempIter->second.m_firstTriggerTime ;
                exeTaskIter->second.m_NextScheduleTickCount = 0;
                exeTaskIter->second.m_PreviousScheduleTickCount = 0;
                exeTaskIter->second.m_NextExecutionTime = tempIter->second.m_firstTriggerTime ;
                exeTaskIter->second.m_RunEvery = tempIter->second.m_RunEvery ;
            }
            if(exeTaskIter->second.m_ScheduleExcludeFromTime != tempIter->second.m_ScheduleExcludeFromTime ||
                exeTaskIter->second.m_ScheduleExcludeToTime != tempIter->second.m_ScheduleExcludeToTime)
            {
                DebugPrintf(SV_LOG_DEBUG, "earlier Schedule FromTime %ld and current Schedule FromTime %ld\n", 
                    exeTaskIter->second.m_ScheduleExcludeFromTime,
                    tempIter->second.m_ScheduleExcludeFromTime) ;
                DebugPrintf(SV_LOG_DEBUG, "earlier Schedule ToTime %ld and current Schedule ToTime %ld\n", 
                    exeTaskIter->second.m_ScheduleExcludeToTime,
                    tempIter->second.m_ScheduleExcludeToTime) ;
                exeTaskIter->second.m_ScheduleExcludeFromTime = tempIter->second.m_ScheduleExcludeFromTime;
                exeTaskIter->second.m_ScheduleExcludeToTime = tempIter->second.m_ScheduleExcludeToTime;
            }
            if( exeTaskIter->second.m_bIsActive != tempIter->second.m_bIsActive )
            {
                DebugPrintf(SV_LOG_DEBUG, "earlier Active State %d and current Active state %d\n", exeTaskIter->second.m_bIsActive, 
                    tempIter->second.m_bIsActive ) ;
                exeTaskIter->second.m_bIsActive = tempIter->second.m_bIsActive ;
                exeTaskIter->second.m_State = tempIter->second.m_State ;
				exeTaskIter->second.m_StartTime = tempIter->second.m_firstTriggerTime ;
                exeTaskIter->second.m_NextExecutionTime = tempIter->second.m_firstTriggerTime ;
            }
            if( exeTaskIter->second.m_firstTriggerTime != tempIter->second.m_firstTriggerTime )
            {
				DebugPrintf(SV_LOG_DEBUG, "earlier StartTime from CX: %d and current StartTime from CX: %d\n", exeTaskIter->second.m_firstTriggerTime, 
                    tempIter->second.m_firstTriggerTime ) ;
				exeTaskIter->second.m_StartTime = tempIter->second.m_firstTriggerTime ;
                exeTaskIter->second.m_firstTriggerTime = tempIter->second.m_firstTriggerTime ;
                exeTaskIter->second.m_NextExecutionTime = tempIter->second.m_firstTriggerTime ;
			}
        }

    } // for loop
    /*
    std::map<SV_ULONG,TaskInformation>::iterator exeTaskIterator; 
    exeTaskIterator = m_ExecutionTaskMap.begin();
    while(exeTaskIterator != m_ExecutionTaskMap.end())
    {
        if( exeTaskIterator->first == 0 )
        {
            exeTaskIterator++;
            continue ;
        }
        tempIter = tempTaskMap.find(exeTaskIterator->first);

        if( tempIter  == tempTaskMap.end() )
        {
            DebugPrintf(SV_LOG_INFO, "Config Id %ld is a outdated task\n", exeTaskIterator->first) ;
            tempIter = exeTaskIterator ;
            exeTaskIterator++ ;
            m_ExecutionTaskMap.erase(tempIter);
            continue ;
        }
        exeTaskIterator++;
    }
    */
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void TaskExecution::SchedulePendingTasks()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_ULONGLONG scheduleTickCount = 0; 
    SV_ULONGLONG prevScheduleTickCount = 0;
    SV_ULONGLONG currentTickCount = 0;
    std::map<SV_ULONG,TaskInformation>::iterator exeTaskIter  = m_ExecutionTaskMap.begin();
	while(exeTaskIter != m_ExecutionTaskMap.end())
	{
        TaskInformation& task = exeTaskIter->second ;
        SV_ULONG configId = exeTaskIter->first;
        SV_ULONGLONG curTime = (SV_ULONGLONG)time(NULL);
		SV_ULONGLONG lastExecTime = task.m_LastExecutionTime;
		SV_ULONGLONG ScheduleTime = task.m_NextExecutionTime;
		SV_ULONGLONG scheduleExcludeFromTime = task.m_ScheduleExcludeFromTime;
		SV_ULONGLONG scheduleExcludeToTime = task.m_ScheduleExcludeToTime;
		SV_ULONG frequency = task.m_RunEvery;



#ifdef SV_WINDOWS
       scheduleTickCount = task.m_NextScheduleTickCount;
       prevScheduleTickCount = task.m_PreviousScheduleTickCount;
       currentTickCount = GetTickCount()/1000;
#else
       scheduleTickCount = task.m_NextExecutionTime;
       prevScheduleTickCount = task.m_LastExecutionTime;
       currentTickCount = (SV_ULONGLONG)time(NULL);
#endif
        if(task.m_State != TASK_STATE_NOT_STARTED)
        {
            DebugPrintf(SV_LOG_DEBUG, "The scheduler stopped before completion last time. job id:  %ld and state: %d\n", configId, task.m_State) ;
            task.m_State = TASK_STATE_SCHEDULED ;
            if(frequency !=0)
            {
                task.m_NextExecutionTime = curTime + frequency ;
                task.m_PreviousScheduleTickCount = currentTickCount;
                task.m_LastExecutionTime = curTime;
                task.m_NextScheduleTickCount = currentTickCount + frequency;
                SV_ULONGLONG nextTime = task.m_NextExecutionTime;
                task.m_StartTime = curTime ;
                if(configId == 0 )
                {
                    task.m_InstanceId = 0;
                }
                else
                {
                    task.m_InstanceId = curTime;
                    try {
                        SchedStatus schedStatus;

                        ADD_INT_TO_MAP(schedStatus, POLICYID, configId);
                        ADD_INT_TO_MAP(schedStatus, CONFINTERVAL, frequency);
                        ADD_INT_TO_MAP(schedStatus, LASTSCHTIME, lastExecTime);
                        ADD_INT_TO_MAP(schedStatus, CURRSCHTIMEEXP, ScheduleTime);
                        ADD_INT_TO_MAP(schedStatus, CURRTIME, curTime);
                        ADD_INT_TO_MAP(schedStatus, NEXTSCHTIME, nextTime);
                        ADD_INT_TO_MAP(schedStatus, LASTSCHSTATUS, AGENT_SHUTDOWN_BEFORE_COMPLETED);
                        ADD_INT_TO_MAP(schedStatus, MESSAGETYPE, SCHED_TABLE_AGENT_LOG);

                        std::string strSchedStatus = JSON::producer<SchedStatus>::convert(schedStatus);
                        gSchedTelemetryLog.Printf(strSchedStatus);
                    }
                    catch (const std::exception &e)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "Serializing the schedule telemetry failed. error %s\n",
                            e.what());
                    }
                    catch (...)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "Serializing the schedule telemetry failed with unknown exception.\n");
                    }
                }
            }
            TriggerTask(configId) ;
            DebugPrintf(SV_LOG_DEBUG,"StartTime Time    = %s\n",convertTimeToString(curTime).c_str());
        }
        exeTaskIter ++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void TaskExecution::schedule()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bExecute = false;
    SV_ULONGLONG scheduleTickCount = 0; 
    SV_ULONGLONG prevScheduleTickCount = 0;
    SV_ULONGLONG currentTickCount = 0;
    
	std::map<SV_ULONG,TaskInformation>::iterator exeTaskIter  = m_ExecutionTaskMap.begin();
	while(exeTaskIter != m_ExecutionTaskMap.end())
	{
        TaskInformation& task = exeTaskIter->second ;
        SV_ULONG configId = exeTaskIter->first;
        SV_ULONGLONG curTime = (SV_ULONGLONG)time(NULL);
		SV_ULONGLONG lastExecTime = task.m_LastExecutionTime;
		SV_ULONGLONG ScheduleTime = task.m_NextExecutionTime;
		SV_ULONGLONG scheduleExcludeFromTime = task.m_ScheduleExcludeFromTime ;
		SV_ULONGLONG scheduleExcludeToTime = task.m_ScheduleExcludeToTime ;
		SV_ULONG frequency = task.m_RunEvery;
#ifdef SV_WINDOWS
       scheduleTickCount = task.m_NextScheduleTickCount;
       prevScheduleTickCount = task.m_PreviousScheduleTickCount;
       currentTickCount = GetTickCount()/1000;
#else
       scheduleTickCount = task.m_NextExecutionTime;
       prevScheduleTickCount = task.m_LastExecutionTime;
       currentTickCount = (SV_ULONGLONG)time(NULL);
#endif
		time_t rawtime;
		time ( &rawtime );
		struct tm * timeinfo;
		timeinfo = localtime ( &rawtime );
		timeinfo->tm_min = 0;
		timeinfo->tm_hour = 0;
		timeinfo->tm_sec = 0;
		//scheduleExcludeFromTime = add last mid night time.
		//scheduleExcludeToTime = add last mid night time.	
		scheduleExcludeFromTime = scheduleExcludeFromTime + (SV_ULONG)mktime(timeinfo); 
		scheduleExcludeToTime = scheduleExcludeToTime + (SV_ULONG)mktime (timeinfo); 
    
        if(scheduleExcludeToTime < scheduleExcludeFromTime)
        {
            SV_ULONGLONG tempTime = scheduleExcludeFromTime;
            scheduleExcludeFromTime = scheduleExcludeToTime;
            scheduleExcludeToTime = tempTime;
            if(!((scheduleExcludeFromTime <= curTime )&& (curTime <= scheduleExcludeToTime)))
            {
                DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeFromTime = %s\n",convertTimeToString(scheduleExcludeToTime).c_str());
                DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeToTime   = %s\n",convertTimeToString(scheduleExcludeFromTime).c_str());
                DebugPrintf(SV_LOG_DEBUG,"Job %ld is excluded from running\n",configId);
                exeTaskIter++ ;
			    continue;
            }
        }
        else if((scheduleExcludeFromTime <= curTime) && (curTime <= scheduleExcludeToTime))
		{
            DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeFromTime = %s\n",convertTimeToString(scheduleExcludeFromTime).c_str());
            DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeToTime   = %s\n",convertTimeToString(scheduleExcludeToTime).c_str());
            DebugPrintf(SV_LOG_DEBUG,"Job %ld is excluded from running\n",configId);
            exeTaskIter++ ;
			continue;
		}
        if(currentTickCount < prevScheduleTickCount)
        {
                prevScheduleTickCount = currentTickCount;
                scheduleTickCount = currentTickCount;
        }
        if (scheduleTickCount <= currentTickCount && ( task.m_State == TASK_STATE_NOT_STARTED ))
        {
            task.m_State = TASK_STATE_SCHEDULED ;
            DebugPrintf(SV_LOG_DEBUG,"Task %ld schedule details\n",configId);
#ifdef SV_WINDOWS
            DebugPrintf(SV_LOG_DEBUG,"Schedule StartTickCount      = %ld\n",scheduleTickCount);
            DebugPrintf(SV_LOG_DEBUG,"Schedule currentTickCount      = %ld\n",currentTickCount);
#else
            DebugPrintf(SV_LOG_DEBUG,"Schedule StartTime      = %s\n",convertTimeToString(ScheduleTime).c_str());
#endif
            DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeFromTime = %s\n",convertTimeToString(scheduleExcludeFromTime).c_str());
            DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeToTime   = %s\n",convertTimeToString(scheduleExcludeToTime).c_str());
            DebugPrintf(SV_LOG_DEBUG,"Schedule StartTime      = %s\n",convertTimeToString(ScheduleTime).c_str());
            DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeFromTime = %s\n",convertTimeToString(scheduleExcludeFromTime).c_str());
            DebugPrintf(SV_LOG_DEBUG,"scheduleExcludeToTime   = %s\n",convertTimeToString(scheduleExcludeToTime).c_str());
            task.m_StartTime = curTime ;
            if(configId == 0 )
            {
                task.m_InstanceId = 0;
            }
            else
            {
                task.m_InstanceId = curTime;
            }
            if(frequency !=0)
            {
                task.m_NextExecutionTime =curTime + frequency ;
                task.m_NextScheduleTickCount = currentTickCount + frequency;
                task.m_PreviousScheduleTickCount = currentTickCount;
                task.m_LastExecutionTime = curTime;
                SV_ULONGLONG nextTime = task.m_NextExecutionTime;
#ifdef SV_WINDOWS                
                DebugPrintf(SV_LOG_DEBUG,"Next TickCount    = %ld\n",task.m_NextScheduleTickCount);
                
#else
                DebugPrintf(SV_LOG_DEBUG,"Next Schedulue Time    = %s\n",convertTimeToString(nextTime).c_str());
#endif 

                if (configId != 0)
                {
                    try {
                        SchedStatus schedStatus;

                        ADD_INT_TO_MAP(schedStatus, POLICYID, configId);
                        ADD_INT_TO_MAP(schedStatus, CONFINTERVAL, frequency);
                        ADD_INT_TO_MAP(schedStatus, LASTSCHTIME, lastExecTime);
                        ADD_INT_TO_MAP(schedStatus, CURRSCHTIMEEXP, ScheduleTime);
                        ADD_INT_TO_MAP(schedStatus, CURRTIME, curTime);
                        ADD_INT_TO_MAP(schedStatus, NEXTSCHTIME, nextTime);
                        ADD_INT_TO_MAP(schedStatus, LASTSCHSTATUS, SCHEDULE_COMPLETED);
                        ADD_INT_TO_MAP(schedStatus, MESSAGETYPE, SCHED_TABLE_AGENT_LOG);

                        std::string strSchedStatus = JSON::producer<SchedStatus>::convert(schedStatus);
                        gSchedTelemetryLog.Printf(strSchedStatus);
                    }
                    catch (std::exception &e)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "Serializing the schedule telemetry failed. error %s\n",
                            e.what());
                    }
                    catch (...)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "Serializing the schedule telemetry failed with unknown exception.\n");
                    }
                }
            }
            TriggerTask(configId) ;
		}

        exeTaskIter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


TaskExecutionPtr TaskExecution::getInstance()
{
    if(m_TaskObjPtr.get() == NULL)
    {
		m_TaskObjPtr.reset( new TaskExecution() );
        
    }
    return m_TaskObjPtr ;
}


sigslot::signal1<SV_ULONG >& TaskExecution::getTaskTriggerSignal()
{
    return m_taskTriggerSignal ;
}

void TaskExecution::TriggerTask( SV_ULONG configId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_taskTriggerSignal( configId ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
std::map<SV_ULONG,TaskInformation>& TaskExecution::gettasks()
{
    return m_ExecutionTaskMap  ;
}

void TaskExecution::settasks(std::map<SV_ULONG,TaskInformation> tasks)
{
    m_ExecutionTaskMap  = tasks ;
}


