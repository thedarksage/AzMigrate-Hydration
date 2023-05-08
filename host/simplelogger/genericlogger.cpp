
///
///  \file  genericlogger.cpp
///
///  \brief <TBD>
///
/// \example simplelogger/<TBD>.cpp

#include "genericlogger.h"

/// \brief simple logger namesapce
namespace SIMPLE_LOGGER {

    bool LogWriterCreator::m_isInit = false;
    LogWriter* LogWriterCreator::m_logWriter;
    boost::mutex LogWriterCreator::m_lock;

} // namespace SIMPLE_LOGGER