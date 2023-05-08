//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : dataprotectionexception.h
//
// Description: 
//

#ifndef DATAPROTECTIONEXCEPTION_H
#define DATAPROTECTIONEXCEPTION_H

#include <stdexcept>
#include <string>

#include "portable.h"

class DataProtectionException : public std::runtime_error {
public:        
    explicit DataProtectionException(std::string const & errStr, SV_LOG_LEVEL logLevel = SV_LOG_ERROR) 
        : std::runtime_error(errStr),
          m_LogLevel(logLevel)
    {}

    SV_LOG_LEVEL LogLevel() { return m_LogLevel; }

protected:

private:   
    SV_LOG_LEVEL m_LogLevel;

};

class DataProtectionInvalidArgsException : public std::runtime_error {
public:
    explicit DataProtectionInvalidArgsException(std::string const & errStr, SV_LOG_LEVEL logLevel = SV_LOG_ERROR)
        : std::runtime_error(errStr),
        m_LogLevel(logLevel)
    {}

    SV_LOG_LEVEL LogLevel() { return m_LogLevel; }

protected:

private:
    SV_LOG_LEVEL m_LogLevel;

};

class DataProtectionInvalidConfigPathException : public std::runtime_error {
public:
    explicit DataProtectionInvalidConfigPathException(std::string const & errStr, SV_LOG_LEVEL logLevel = SV_LOG_ERROR)
        : std::runtime_error(errStr),
        m_LogLevel(logLevel)
    {}

    SV_LOG_LEVEL LogLevel() { return m_LogLevel; }

protected:

private:
    SV_LOG_LEVEL m_LogLevel;

};

class DataProtectionNWException : public std::runtime_error {
public:
    explicit DataProtectionNWException(std::string const & errStr, SV_LOG_LEVEL logLevel = SV_LOG_ERROR)
        : std::runtime_error(errStr),
        m_LogLevel(logLevel)
    {}

    SV_LOG_LEVEL LogLevel() { return m_LogLevel; }

protected:

private:
    SV_LOG_LEVEL m_LogLevel;

};

class CorruptResyncFileException : public std::runtime_error {
public:
    explicit CorruptResyncFileException(std::string const & errStr, SV_LOG_LEVEL logLevel = SV_LOG_ERROR)
        : std::runtime_error(errStr),
        m_LogLevel(logLevel)
    {}

    SV_LOG_LEVEL LogLevel() { return m_LogLevel; }

protected:

private:
    SV_LOG_LEVEL m_LogLevel;

};

#endif // ifndef DATAPROTECTIONEXCEPTION__H
