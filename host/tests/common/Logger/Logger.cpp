// Logger.cpp : Contains Logger class implementation

#include "Logger.h"
#include <iterator>

using namespace std;

#ifdef WIN32
#include <strsafe.h>
#define VSPRINTF        StringCchVPrintfA
#else
#define VSPRINTF        vsnprintf
#endif

CLogger::CLogger(string fileName, bool appendLog)
{
    m_logFileName = fileName;
    m_fileStream.open(fileName.c_str(), (appendLog ? ios::app : ios::out));
}

CLogger::~CLogger()
{
    m_fileStream.close();
}

void CLogger::LogInfo(const char* format, ...)
{
    Lock();
    std::vector<char> buf(4096);
    // Put time
    time_t t = time(NULL);
    std::string st = ctime(&t);

    st.erase(st.end() - 1);

    // Remove \n from the end
    m_fileStream << "[" << st << "] ";

    m_fileStream << " [" << getProcessId() << "] ";
    m_fileStream << " [" << getThreadId() << "]: ";

    va_list argptr;

    va_start(argptr, format);
    VSPRINTF(&buf[0], 4095, format, argptr);
    va_end(argptr);

    m_fileStream << std::string(&buf[0]) << std::endl;

#ifdef SV_UNIX
    Flush();
#endif

    Unlock();
}

void CLogger::Flush()
{
    m_fileStream.flush();
}

void CLogger::LogWarning(char* format, ...)
{
    Lock();

    std::vector<char> buf(4096);

    // Put time
    time_t t = time(NULL);
    std::string st = ctime(&t);


    st.erase(st.end()-1);


    // Remove \n from the end
    m_fileStream << "[" << st  << "] ";

    m_fileStream << " [" << getProcessId() << "] ";
    m_fileStream << " [" << getThreadId() << "]: ";

    va_list argptr;
    va_start(argptr, format);
    VSPRINTF(&buf[0], 4095, format, argptr);
    va_end(argptr);

    m_fileStream << std::string(&buf[0]) << std::endl;
#ifdef SV_UNIX
    Flush();
#endif
    Unlock();
}

void CLogger::LogError(char* format, ...)
{
    Lock();
    std::vector<char> buf(4096);
    // Put time
    time_t t = time(NULL);
    std::string st = ctime(&t);

    st.erase(st.end()-1);

    // Remove \n from the end
    m_fileStream << "[" << st << "] ";

    m_fileStream << " [" << getProcessId() << "] ";
    m_fileStream << " [" << getThreadId() << "]: ";

    va_list argptr;

    va_start(argptr, format);
    VSPRINTF(&buf[0], 4095, format, argptr);
    va_end(argptr);

    m_fileStream << std::string(&buf[0]) << std::endl;

#ifdef SV_UNIX
    Flush();
#endif
    Unlock();
}

