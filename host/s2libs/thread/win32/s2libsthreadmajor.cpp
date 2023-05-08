//windows port of thread.cpp

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



    if ( ACE_Thread_Manager::instance()->spawn(func,pArg,THR_NEW_LWP|THR_JOINABLE|THR_INHERIT_SCHED,&m_ThreadID, 0,ACE_DEFAULT_THREAD_PRIORITY) < 0 )

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

    if ( ACE_Thread_Manager::instance()->spawn(RunnableThread,pRunnableThreadObject,THR_NEW_LWP|THR_JOINABLE|THR_INHERIT_SCHED | THR_CANCEL_ENABLE,&m_ThreadID, 0,ACE_DEFAULT_THREAD_PRIORITY) < 0 )

    {

        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to create thread. %s. @LINE %d in FILE %s\n",Error::Msg().c_str(),LINE_NO,FILE_NAME);

        SetState(STOPPED);

        m_ThreadID = SV_FAILURE;

    }

    else

    {

        m_bSpawned = true;

        m_bWaitForExit = true;

        SetState(RUNNING);

    }



    return m_ThreadID;

}





bool    Thread::Terminate(int sigval)

{

    if ( Spawned() )

    {

        // BUGBUG: TODO. Specify the correct exit status as the second parameter.

        if ( ACE_Thread_Manager::instance()->kill(GetThreadID(),sigval) >= 0)

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



        ACE_UINT32 uiState = 0;

        ACE_Thread_Manager::instance()->thr_state(GetThreadID(),uiState);

        std::stringstream sstid;

        sstid << GetThreadID();

        DebugPrintf(SV_LOG_DEBUG,"State of Thread %s is: %u.\n", sstid.str().c_str(),uiState);

        DebugPrintf(SV_LOG_DEBUG,"Joining Thread %s. Waiting for thread to exit.\n", sstid.str().c_str());



        if ( (iStatus = ACE_Thread_Manager::instance()->join(GetThreadID(),&ExitStatus)) >= 0 )

        {

            ACE_Thread_Manager::instance()->thr_state(GetThreadID(),uiState);

            DebugPrintf(SV_LOG_DEBUG,"Thread %s exited cleanly.\n", sstid.str().c_str());

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

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);



    if ( Spawned() )

    {





        if(ACE_Thread_Manager::instance()->testcancel(GetThreadID())== 0)

	    {

	        ACE_Thread_Manager::instance()->cancel(GetThreadID(),1);

	    }

        if ( (THREAD_RUNNABLE == m_ThreadType ) && ( NULL != m_pRunnableObject ) )

            m_pRunnableObject->Stop(PARAM);

        if ( !m_bWaitForExit )

        {

            SetState(STOPPED);

            return true;

        }

        else

        {

            return WaitForThread();

        }

    }

    std::stringstream sstid;

    sstid << GetThreadID();

    DebugPrintf(SV_LOG_DEBUG,"Exiting Thread %s.\n", sstid.str().c_str());

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);



    return false;

}



bool Thread::Suspend()

{

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

    return false;

}



bool Thread::Resume()

{

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

    return false;

}

