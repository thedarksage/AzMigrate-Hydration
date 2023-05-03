// LoggerMajor.cpp : Contains Logger class implementation

#include "Logger.h"
#include "LoggerMajor.h"
#include <time.h>
#include <string>
#include <vector>

LOG_MUTEX g_mutex = PTHREAD_MUTEX_INITIALIZER;
//Logger Lock 
void CLogger::Lock()
{
	pthread_mutex_lock(&g_mutex);	
}

//Logger Unlock 
void CLogger::Unlock()
{
	pthread_mutex_unlock(&g_mutex);
}

//getProcessId
unsigned long CLogger::getProcessId()
{
	return getpid();
}

//getThreadIs
unsigned long CLogger::getThreadId()
{
	return pthread_self();
}
