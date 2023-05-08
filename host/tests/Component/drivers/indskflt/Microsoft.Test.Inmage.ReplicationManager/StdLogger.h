#ifndef _STD_LOGGER_H_
#define _STD_LOGGER_H_

#include "PlatformAPIs.h"
#include "Logger.h"

class StdLogger : public ILogger
{
private:
	CHAR	m_msgBuffer[MAX_PATH];
	UINT32	m_msgBufferLength = MAX_PATH;

public:
	void LogError(const char* format, ...);
	void LogComment(const char* format, ...);
	void LogWarning(const char* format, ...);
};

#endif