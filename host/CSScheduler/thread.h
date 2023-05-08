#ifndef __PTHREAD_H_
#define __PTHREAD_H_

#ifdef WIN32
#include <windows.h>
#include <process.h>
#include <Errno.h>
#else
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <error.h>
#include<errno.h>
#endif
#include "Log.h"
#ifndef WIN32
#define THREAD_T pthread_t
#else
#define THREAD_T uintptr_t
#endif

namespace sch
{

int pthreadCreate(THREAD_T *thread, void (*start_routine)(void*), void *arg);
void *pthreadJoin(THREAD_T threadHandle);
void pthreadDetach(THREAD_T threadHandle);
void Sleep(long millisec);
void pthreadExit(int *exitStatus);
}

#endif
