///
///  \file  Logger.h
///
///  \brief contains Logger class declarations
///

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <fstream>
#include <mutex>


#ifdef LOGGER_EXPORTS
#define LOGGER_API __declspec(dllexport)
#else
#define LOGGER_API __declspec(dllimport)
#endif

using namespace std;

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
	mutex		m_logLock;
public:
	CLogger(string fileName, bool appendLog = false);
	~CLogger();
	void LogInfo(const char* format, ...);
	void LogWarning(char* format, ...);
	void LogError(char* format, ...);
	void Flush();
	// TODO: add your methods here.
}; 
#endif /* LOGGER_H */
