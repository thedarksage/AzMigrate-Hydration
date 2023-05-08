///
///  \file  LoggerMajor.h
///
///  \brief contains LoggerMajor definations for Windows 
///

#ifndef LOGGERMAJOR_H
#define LOGGERMAJOR_H


#include <stdio.h>
#include <fstream>
#include <mutex>


#ifdef LOGGER_EXPORTS
#define LOGGER_API __declspec(dllexport)
#else
#define LOGGER_API __declspec(dllimport)
#endif

typedef std::_Mutex_base LOG_MUTEX;

 
#endif /* LOGGERMAJOR_H */
