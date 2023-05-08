///
///  \file  Logger.h
///
///  \brief contains Logger class declarations
///

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <iostream>
#include <time.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include "LoggerMajor.h"
#include "inmsafecapis.h"

using namespace std;

//#define NELEMS(ARR) ((sizeof (ARR)) / (sizeof (ARR[0])))

//ILogger interface
class LOGGER_API ILogger
{
public:
    virtual void LogInfo(const char* format, ...) = 0;
    virtual void LogWarning(char* format, ...) = 0;
    virtual void LogError(char* format, ...) = 0;
    virtual void Flush() = 0;
    virtual ~ILogger() {}
};


class LOGGER_API CLogger : public ILogger
{
private:
    string      m_logFileName;
    char        m_timeStr[32];
    ofstream    m_fileStream;
    LOG_MUTEX    m_logLock;

public:
    CLogger(string fileName, bool appendLog = false);
    ~CLogger();
    void LogInfo(const char* format, ...);
    void LogWarning(char* format, ...);
    void LogError(char* format, ...);
    void Flush();
    
    void Lock();
    void Unlock();
    unsigned long getProcessId();
    unsigned long getThreadId();
    
}; 
#endif /* LOGGER_H */
