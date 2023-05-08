//#pragma once

#ifndef THREAD__H
#define THREAD__H

#ifdef SV_WINDOWS
#include <windows.h>
#endif

#include "runnable.h"
#include "event.h"
#include "ace/Thread.h"
#include "ace/OS_NS_Thread.h"
#include "ace/config-macros.h"

class ThreadManager;

#define INFINITE_WAIT        0xFFFFFFFF

const int THREAD_STOP_RETRY_ATTEMPTS           =   10;
const unsigned int THREAD_STOP_WAIT_TIME = INFINITE_WAIT; //SENTINEL_EXIT_TIME - (1 * 30 * 1000); // milliseconds

const long int PROCESS_QUIT_IMMEDIATELY     = 0x1001;
const long int PROCESS_QUIT_REQUESTED       = 0x1002;
const long int PROCESS_QUIT_TIME_OUT        = 0x1003;
const long int PROCESS_RUN                  = 0x1004;

enum THREAD_TYPE
{
    THREAD_PROC=1,
    THREAD_RUNNABLE=2
};

struct THREAD_INFO
{
    unsigned long int   Flags;
    unsigned long int   Priority;
    unsigned long int   StackSize;
    std::string         Task;
    THREAD_INFO():Flags(0),Priority(0),StackSize(0),Task("") { }
};

typedef THREAD_INFO *PTHREAD_INFO;

#ifdef SV_WINDOWS
#define STD_CALL __stdcall
#endif

//typedef ACE_THR_FUNC_RETURN THREAD_FUNC_RETURN;
typedef ACE_hthread_t   THREAD_HANDLE;
typedef ACE_thread_t    THREAD_ID;
//typedef unsigned int ( STD_CALL *THREAD_FUNC )(void*);
//typedef ACE_THR_FUNC THREAD_FUNC;

class Thread
{

public:
    Thread();
    ~Thread();
    const THREAD_ID GetThreadID() const;
    THREAD_ID Start(Runnable*,PTHREAD_INFO pThreadInfo = NULL);
    THREAD_ID Start(THREAD_FUNC,void* pArg=NULL,PTHREAD_INFO pThreadInfo = NULL);
    bool Stop(const long int PARAM=0);
    const bool  IsRunning();
    const bool  IsStopped();
//    const bool  IsCancelled();
    bool        Suspend();
    bool        Resume();
    const int GetPriority() const;
    const  int GetStackSize() const;
    const std::string& GetTaskName() const;
    bool        Terminate(int sigval=0);
    const bool Spawned() const;
    THREAD_TYPE GetThreadType();
    static const unsigned long int RUNNING;
    static const unsigned long int STOPPED;
    //static const unsigned long int CANCELLED;
private:
    unsigned long int m_uliState;
    PTHREAD_INFO m_pThreadInfo;
    THREAD_ID m_ThreadID;
    Runnable* m_pRunnableObject;
    THREAD_TYPE m_ThreadType;
    bool m_bSpawned;
    // Maybe we need to drop this event??
    //Event m_hStop;

private:
    friend class ThreadManager;
    static THREAD_FUNC_RETURN RunnableThread( void * );
    void SetState(const long int);
    bool m_bWaitForExit;
    bool WaitForThread();
//  Following Operations should not be allowed on Threads.
private:
    Thread(const Thread &right);
    Thread& operator=(const Thread &right);
    int operator==(const Thread &right) const;
    int operator!=(const Thread &right) const;
    int operator<(const Thread &right) const;
    int operator>(const Thread &right) const;
    int operator<=(const Thread &right) const;
    int operator>=(const Thread &right) const;

};
#endif
