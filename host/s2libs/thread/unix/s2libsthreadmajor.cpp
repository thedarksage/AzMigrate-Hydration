//Linux port of thread.cpp
#include <string>
//#include <process.h>
#include <map>
#include <map>
#include <sstream>


#include "event.h"
#include "runnable.h"

#include "s2libsthread.h"
#include "error.h"
#include "portablehelpers.h"
#include "ace/OS_NS_Thread.h"
#include "ace/Thread.h"
#include "ace/Thread_Manager.h"


THREAD_ID Thread::Start(THREAD_FUNC func,void* pArg,THREAD_INFO* pThreadInfo)
{
    m_ThreadType = THREAD_PROC;

	if ( pthread_create(&m_ThreadID,NULL,func,pArg) != 0 )
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to create thread. %s. @LINE %d in FILE %s\n",Error::Msg().c_str(),LINE_NO,FILE_NAME);
        m_ThreadID = 0;
        SetState(STOPPED);
    }
	else
	{
        m_bSpawned = true;
        m_bWaitForExit = true;
        SetState(RUNNING);
        std::stringstream sstid;
        sstid << m_ThreadID;
        DebugPrintf(SV_LOG_INFO,"Thread id: %s\n",sstid.str().c_str());
	}

    return m_ThreadID;
}

THREAD_ID Thread::Start(Runnable* pRunnableThreadObject,PTHREAD_INFO pThreadInfo)
{
    if ( NULL == pRunnableThreadObject )
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Null object encountered. Cannot create thread. @LINE %d in FILE %s.\n",LINE_NO,FILE_NAME);
        return SV_FAILURE;
    }

    m_ThreadType = THREAD_RUNNABLE;

    m_pRunnableObject = dynamic_cast<Runnable*>(pRunnableThreadObject);
	if ( pthread_create(&m_ThreadID,NULL,RunnableThread,pRunnableThreadObject) != 0 )
	{
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to create thread. %s. @LINE %d in FILE %s\n",Error::Msg().c_str(),LINE_NO,FILE_NAME);
        SetState(STOPPED);
        m_ThreadID = 0;
	}
	else
	{
        m_bSpawned = true;
        std::stringstream sstid;
        sstid << m_ThreadID;
        DebugPrintf(SV_LOG_INFO,"Thread id: %s\n",sstid.str().c_str());
        m_bWaitForExit = true;
        SetState(RUNNING);
	}
    return m_ThreadID;
}

bool    Thread::Terminate(int sigval)
{
	DebugPrintf(SV_LOG_WARNING,"WARNING: Terminate thread is not supported on this platform.\n");

    if ( Spawned() )
    {
        if ( pthread_kill(GetThreadID(),sigval) >= 0)
        {
            return true;
        }
        else
        {
            std::stringstream sstid;
            sstid << GetThreadID();
            DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to terminate thread %s\n", sstid.str().c_str());
            return false;
        }
    }
    return false;
}

bool Thread::WaitForThread()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    bool bStatus = false;

    if ( Spawned())
    {
        int iStatus = SV_SUCCESS;
    
        ACE_THR_FUNC_RETURN ExitStatus = 0;

        std::stringstream sstid;
        sstid << GetThreadID();
		DebugPrintf(SV_LOG_DEBUG,"Joining Thread %s. Waiting for thread to exit.\n", sstid.str().c_str());

		if ( pthread_join(GetThreadID(),&ExitStatus) == 0 ) 
		{
			DebugPrintf(SV_LOG_DEBUG,"Thread %s exited cleanly.\n", sstid.str().c_str());
			/*BUGBUG: call thread detach after join.*/
			bStatus = true;
			SetState(STOPPED);
		}
		else
		{
			SetState(RUNNING);
			bStatus = false;
			DebugPrintf(SV_LOG_ERROR,"Failed to stop thread %s.\n", sstid.str().c_str());
		}
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    
    return bStatus;
}

bool Thread::Stop(const long int PARAM)
{
    bool bStatus = false;

	if ( NULL != m_pRunnableObject ) 
		m_pRunnableObject->Stop(PARAM);

	if ( !m_bWaitForExit )
	{           
		SetState(STOPPED);
		bStatus = true;
	}
	else
	{
		bStatus = WaitForThread();
	}            
    std::stringstream sstid;
    sstid << GetThreadID();
    DebugPrintf(SV_LOG_DEBUG,"Exiting Thread %s.\n", sstid.str().c_str());
    return bStatus;
}

bool Thread::Suspend()
{
	DebugPrintf(SV_LOG_WARNING,"WARNING: Suspend thread is not supported on this platform.\n");
/*
    if ( Spawned() )
    {
        if ( ACE_Thread_Manager::instance()->suspend(GetThreadID()) >= 0)
        {
            return true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to suspend thread. %s. @LINE %d in FILE %s.\n",Error::Msg().c_str(),LINE_NO,FILE_NAME);
            return false;
        }
    }
*/
    return false;
}

bool Thread::Resume()
{
	DebugPrintf(SV_LOG_WARNING,"WARNING: Resume thread is not supported on this platform.\n");
/*
    if ( Spawned() )
    {
        if ( ACE_Thread_Manager::instance()->resume(GetThreadID()) >= 0)
        {
            return true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Failed to resume thread. %s. @LINE %d in FILE %s.\n",Error::Msg().c_str(),LINE_NO,FILE_NAME);
            return false;
        }
    }
*/
    return false;
}

