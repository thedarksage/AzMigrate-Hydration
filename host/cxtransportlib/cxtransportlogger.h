
///
/// \file cxtransportlogger.h
///
/// \brief cx transport transfer loggging
///

#ifndef CXTRANSPORTLOGGER_H
#define CXTRANSPORTLOGGER_H


#include "simplelogger.h"

// *** xfer logging ***
SIMPLE_LOGGER::SimpleLogger& cxTransportGetXferLogger();

/// \brief setup transfer logging
void cxTransportSetupXferLogging(boost::filesystem::path const& name,  ///< name of log file
                                 int maxSize,                          ///< maximum size of the log file
                                 int rotateCount,                      ///< number of days to retain rotated log files
                                 int retainSizeBytes,                  ///< number of bytes to keep in log file when rotating
                                 bool warningsEnabled                  ///< indicates if log file is enabled true: yes, false: no
                                 );

/// \brief starts the xfer log monitor
boost::shared_ptr<boost::thread> cxTransportStartXferLogMonitor();

/// \brief stops the xfer log monitor
void cxTransportStopXferLogMonitor();

/// \brief enable monitor logging
void cxTransportXferLogOn();

/// \brief disable monitor logging
void cxTransportXferLogOff();

/// \brief used to log data transfers to the xfer log
///
/// \note
/// \li \c you must use this macro for serialized access to the transfer log
/// \li \c do not use any of the atloc macros when logging transfer info
///
// do not add any of the atloc macros to this macro
// do not let exception be thrown, just catch all and ignore
#define CX_TRANSPORT_LOG_XFER(xData)                                    \
    do {                                                                \
        try {                                                           \
            SIMPLE_LOGGER::SimpleLogger& aLogger = cxTransportGetXferLogger(); \
            SIMPLE_LOGGER_ALWAYS(aLogger, xData, false);                \
        } catch (...) {                                                 \
        }                                                               \
    } while (false)


#endif // CXTRANSPORTLOGGER_H
