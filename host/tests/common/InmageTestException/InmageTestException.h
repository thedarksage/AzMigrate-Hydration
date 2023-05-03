///
///  \file  InmageTestException.h
///
///  \brief contains InmageTestException class implementation
///

#ifndef INMAGETESTEXCEPTION_H
#define INMAGETESTEXCEPTION_H

#include <stdio.h>
#include <stdarg.h>
#include <exception>
#include "InmageTestExceptionMajor.h"
#ifdef SV_UNIX
#include "inmsafecapismajor.h"
#endif

#ifdef WIN32
#define VSNPRINTF(buf, size, maxCount, format, args) _vsnprintf_s(buf, size, maxCount, format, args)
#else
#define VSNPRINTF(buf, size, maxCount, format, args) inm_vsnprintf_s(buf, size, format, args)
#endif

using namespace std;

#define EXCEPTION_MSG_BUFFER_LENGTH  1024

class INMAGETESTEXCEPTION_API CInmageTestException : public exception
{
protected:
    char m_exceptionMsg[EXCEPTION_MSG_BUFFER_LENGTH];

public:
    virtual const char* what() const throw()
    {
        return m_exceptionMsg;
    }

};


class InvalidSVDStreamException : public CInmageTestException
{
public:
    InvalidSVDStreamException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CBlockDeviceException : public CInmageTestException
{
public:
    CBlockDeviceException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CBlockDIValidatorException : public CInmageTestException
{
public:
    CBlockDIValidatorException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CVirtualDiskException : public CInmageTestException
{
public:
    CVirtualDiskException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CAgentException : public CInmageTestException
{
public:
    CAgentException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CDataProcessorException : public CInmageTestException
{
public:
    CDataProcessorException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CAgentInvalidSteamException : public CInmageTestException
{
public:
    CAgentInvalidSteamException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CPlatformException : public CInmageTestException
{
public:
    CPlatformException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CInmageConsistencyProviderException : public CInmageTestException
{
public:
    CInmageConsistencyProviderException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CInmageChecksumProviderException : public CInmageTestException
{
public:
    CInmageChecksumProviderException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

class CInmageExcludeBlocksException : public CInmageTestException
{
public:
    CInmageExcludeBlocksException(char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};
#ifdef SV_WINDOWS
class CInmageDataPoolException : public CInmageTestException
{
public:
    CInmageDataPoolException(char *format, ...)
#else
class CInmageDataThreadException : public CInmageTestException
{
public:
    CInmageDataThreadException(char *format, ...)
#endif
    {
        va_list argptr;
        va_start(argptr, format);
        VSNPRINTF(m_exceptionMsg, sizeof(m_exceptionMsg),
            EXCEPTION_MSG_BUFFER_LENGTH, format, argptr);
        va_end(argptr);
    }
};

#endif /* INMAGETESTEXCEPTION_H */
