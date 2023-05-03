#include <string>
//#include <process.h>
#include <map>
#include <map>


#include "event.h"
#include "runnable.h"

#include "s2libsthread.h"
#include "error.h"
#include "portable.h"
#include "ace/OS_NS_Thread.h"
#include "ace/Thread.h"
#include "ace/Thread_Manager.h"

/***********************************************************************************
*** Note: Please write any platform specific code in ${platform}/thread_port.cpp ***
***********************************************************************************/
const unsigned long int Thread::RUNNING     =   0x0001;
const unsigned long int Thread::STOPPED     =   0x0002;
//const unsigned long int Thread::CANCELLED   =   0x0004;

Thread::Thread():   m_pThreadInfo(NULL),
                    m_ThreadID(0),
                    m_pRunnableObject(NULL),
                    m_ThreadType(THREAD_PROC),
                    //m_hStop(false,true),
                    m_uliState(Thread::STOPPED),
                    m_bWaitForExit(true),
                    m_bSpawned(false)
{

}

Thread::~Thread()
{
    
}

const bool Thread::Spawned() const
{
    return m_bSpawned;
}

const int Thread::GetPriority() const
{
    if ( Spawned() )
    {
        if ( m_pThreadInfo )
            return m_pThreadInfo->Priority;
    }
    return -1;
}

const std::string& Thread::GetTaskName() const
{
    if ( Spawned() )
    {
        if ( m_pThreadInfo )
            return m_pThreadInfo->Task;
    }
    return std::string();
}

const int Thread::GetStackSize() const
{
    if ( Spawned() )
    {
        if ( m_pThreadInfo )
            return m_pThreadInfo->StackSize;
    }
    return SV_FAILURE;
}

const THREAD_ID Thread::GetThreadID() const
{
    return m_ThreadID;
}

const bool Thread::IsRunning()
{
    return (m_uliState & RUNNING);
}

THREAD_FUNC_RETURN Thread::RunnableThread(void* pParam)
{
    Runnable* pRunnable = reinterpret_cast<Runnable*>(pParam);
    return pRunnable->Run();
}

THREAD_TYPE Thread::GetThreadType()
{
    return m_ThreadType;
}

const bool Thread::IsStopped()
{
    return (m_uliState & STOPPED);
}

void Thread::SetState(const long int uliState)
{
    m_uliState = m_uliState & 0x000;
    m_uliState = m_uliState | uliState;
}

/*
const bool  Thread::IsCancelled()
{
    return (m_uliState & CANCELLED)
}
*/

