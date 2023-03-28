#ifndef __MUTEX_H_
#define __MUTEX_H_
#ifdef WIN32
#include <windows.h>
#endif

#ifdef WIN32
class Mutex
{
	HANDLE hMutex;
public:
	Mutex()
	{
		hMutex = CreateMutex(NULL,false,"");
	}

	void lock()
	{
		DWORD dwWaitResult = WaitForSingleObject( 
			hMutex,   // handle to mutex
			INFINITE);   // five-second time-out interval
	}

	void unlock()
	{
		ReleaseMutex(hMutex);
	}
	
};
#else
class Mutex
{
	pthread_mutex_t hMutex;
public:
	Mutex()
	{
		pthread_mutexattr_t   attr;
		pthread_mutexattr_init(&attr);		
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE_NP );
		pthread_mutex_init(&hMutex, &attr);
	}

	void lock()
	{
		pthread_mutex_lock(&hMutex);
	}

	void unlock()
	{
		pthread_mutex_unlock(&hMutex) ;
	}
};
#endif
#endif
