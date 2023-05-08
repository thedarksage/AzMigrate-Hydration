
///
/// \file cxtransportlogger.cpp
///
/// \brief
///

#include "cxtransportlogger.h"

//  xfer logging
SIMPLE_LOGGER::SimpleLogger g_cxTransportXferLogger;

void cxTransportXferLogOn()
{
    g_cxTransportXferLogger.on();
}

void cxTransportXferLogOff()
{
    g_cxTransportXferLogger.off();
}

SIMPLE_LOGGER::SimpleLogger& cxTransportGetXferLogger()
{
    return g_cxTransportXferLogger;
}

void cxTransportSetupXferLogging(
    boost::filesystem::path const& name,
    int maxSize,
    int rotateCount,
    int retainSizeBytes,
    bool enabled)
{
    
    SIMPLE_LOGGER::createDirsAsNeeded(name);
    g_cxTransportXferLogger.set(name, maxSize, rotateCount, retainSizeBytes, true, SIMPLE_LOGGER::LEVEL_ALWAYS, true);
    if (enabled) {
        cxTransportXferLogOn();
    } else {
        cxTransportXferLogOff();
    }
}

boost::shared_ptr<boost::thread> cxTransportStartXferLogMonitor()
{
    return SIMPLE_LOGGER::startLoggerMonitor(&g_cxTransportXferLogger);
}

void cxTransportStopXferLogMonitor()
{
    g_cxTransportXferLogger.stopMonitor();
}
