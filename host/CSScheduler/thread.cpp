#include "thread.h"
#include "common.h"

int sch::pthreadCreate(THREAD_T *threadHandle, void (*start_routine)(void*), void *arg)
{
	#ifdef WIN32

        if(threadHandle == NULL)
            return SCH_EINVAL;
        
        *threadHandle = _beginthread(start_routine, 0, arg);
		
        if(*threadHandle == -1L)
		{
			if(errno == EAGAIN)
			{
				return SCH_EAGAIN;
			}
			else 
			{
				return SCH_EINVAL;
			}
		}

		return SCH_SUCCESS;

	#else

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		int hr = pthread_create(threadHandle, &attr, (void *(*)(void *))start_routine, arg);
		pthread_attr_destroy(&attr);
		
		if(hr != 0)
		{
			return SCH_EINVAL;
		}

		return SCH_SUCCESS;

	#endif
}
void sch::pthreadExit(int *exitStatus)
{
	#ifdef WIN32
		*exitStatus = 0;
	#else
		pthread_exit(exitStatus);
	#endif
}
void *sch::pthreadJoin(THREAD_T threadHandle)
{
    void *retValue = NULL;

    #ifdef WIN32
        WaitForSingleObject((HANDLE)threadHandle, INFINITE);
    #else
        pthread_join(threadHandle,&retValue);
    #endif

    return retValue;
}

void sch::pthreadDetach(THREAD_T threadHandle)
{
    #ifndef WIN32
	if( pthread_detach(threadHandle) != 0)
	{
		Logger::Log("thread.cpp", __LINE__, 0, 0, "pthread_detach failed with %d", errno);
	}
    #endif
}

void sch::Sleep(long millisec)
{

#ifdef WIN32

	::Sleep(millisec);

#else

	usleep(millisec*1000);

#endif

}
