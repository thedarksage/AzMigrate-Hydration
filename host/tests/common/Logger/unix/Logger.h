///
/////  \file  Logger.h
/////
/////  \brief contains Logger class declarations 

#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <iostream>

using namespace std;

//ILogger interface
class ILogger
{
public:
    virtual void LogInfo(const char* format, ...) = 0;
    virtual void LogWarning(const char* format, ...) = 0;
    virtual void LogError(const char* format, ...) = 0;
    virtual void Flush() = 0;
    virtual ~ILogger() {}
};

class CLogger : public ILogger
{
private:
    string      m_logFileName;
    char        m_timeStr[32];
    ofstream    m_fileStream;
public:
    CLogger(string fileName, bool appendLog = false);
    ~CLogger();
    void LogInfo(const char* format, ...);
    void LogWarning(const char* format, ...);
    void LogError(const char* format, ...);
    void Flush();

    void Lock();
    void Unlock();
    unsigned long getProcessId();
    unsigned long getThreadId();
    // TODO: add your methods here.
};
#endif /* LOGGER_H */
