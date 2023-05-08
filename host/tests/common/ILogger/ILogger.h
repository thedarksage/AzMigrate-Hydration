///
///  \file  ILogger.h
///
///  \brief contains ILogger interface
///

#ifndef ILOGGER_H
#define ILOGGER_H

#include "boost/shared_ptr.hpp"

#include "Logger.h"

/// \brief ILogger Ptr type
typedef boost::shared_ptr<ILogger> ILoggerPtr;

#endif /* ILOGGER_H */