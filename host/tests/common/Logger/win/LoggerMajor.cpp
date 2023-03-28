// Logger.cpp : Contains Logger class implementation

#include "stdafx.h"
#include "Logger.h"
#include "LoggerMajor.h"
#include <Windows.h>
//#include <strsafe.h>
#include <time.h>
#include <string>
#include <vector>

//Logger Lock 
void CLogger::Lock()
{
    m_logLock.lock();    
}

//Logger Lock 
void CLogger::Unlock()
{
    m_logLock.unlock();
}

//getProcessId
unsigned long CLogger::getProcessId()
{
    return GetCurrentProcessId();
}

//getThreadIs
unsigned long CLogger::getThreadId()
{
    return GetCurrentThreadId();
}
