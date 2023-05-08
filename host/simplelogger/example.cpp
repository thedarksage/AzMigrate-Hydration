
///
/// \file example.cpp
///
/// \brief shows how to use SimpleLogger
///

#include <iostream>

#include "simplelogger.h"

SIMPLE_LOGGER::SimpleLogger g_logger;

 std::string g_str("std::string test");
 std::wstring g_wstr(L"std::wstring test");

void test(bool pidTid)
{
    if (pidTid) {
        SIMPLE_LOGGER_ALWAYS(g_logger, "***** start test should see pid and tid *****", pidTid);
    } else {
        SIMPLE_LOGGER_ALWAYS(g_logger, "***** start test should not see pid and tid *****", pidTid);
    }
    
        // ***** errors *****
    g_logger.setLevel(SIMPLE_LOGGER::LEVEL_ERROR);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** begin error level *****", pidTid);

    // should see these
    SIMPLE_LOGGER_ERROR(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_WARNING(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, "S", pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_INFO(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, "S", pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_DEBUG(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, "S", pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_ALWAYS(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_wstr.c_str(), pidTid);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** end error level *****", pidTid);

    // ***** errors and warnings *****
    g_logger.setLevel(SIMPLE_LOGGER::LEVEL_WARNING);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** begin error, warning levels *****", pidTid);

    // should see these
    SIMPLE_LOGGER_ERROR(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_WARNING(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, "S", pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_INFO(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, "S", pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_DEBUG(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, "S", pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_ALWAYS(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_wstr.c_str(), pidTid);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** end error, warning levels *****", pidTid);

    // errors, warnings, and info
    g_logger.setLevel(SIMPLE_LOGGER::LEVEL_INFO);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** begin error, warning, info levels *****", pidTid);

    // should see these
    SIMPLE_LOGGER_ERROR(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_WARNING(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, "S", pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_INFO(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, "S", pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_wstr.c_str(), pidTid);

    // should not see these
    SIMPLE_LOGGER_DEBUG(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, "S", pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_ALWAYS(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_wstr.c_str(), pidTid);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** end error, warning, info levels *****", pidTid);

    // errors, warnings, info, and debug
    g_logger.setLevel(SIMPLE_LOGGER::LEVEL_DEBUG);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** begin error, warning, info, debug levels *****", pidTid);

    // should see these
    SIMPLE_LOGGER_ERROR(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ERROR(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_WARNING(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, "S", pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_WARNING(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_INFO(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, "S", pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_INFO(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_INFO(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_DEBUG(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, "S", pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_DEBUG(g_logger, g_wstr.c_str(), pidTid);

    // should see these
    SIMPLE_LOGGER_ALWAYS(g_logger, "char str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, "S", pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_str, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L'W', pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, L"wchar str test: " << 1, pidTid);
    SIMPLE_LOGGER_ALWAYS(g_logger, g_wstr.c_str(), pidTid);

    // should see this
    SIMPLE_LOGGER_ALWAYS(g_logger, "***** end error, warning, info, debug levels *****", pidTid);

    SIMPLE_LOGGER_ALWAYS(g_logger, "***** end test *****", pidTid);
}

int main(int argc, char* argv[])
{
    boost::filesystem::path logName("simplelooger-example.log");   
    
    // initalize logger (note: if you know this info when instantiating the logger you can
    // pass this info directly to the consctuctor and skip this call
    g_logger.set(logName, 0, 0, 0, true, SIMPLE_LOGGER::LEVEL_ALWAYS);

    // run logger monitor in its own thread
    boost::shared_ptr<boost::thread> logMonitor(SIMPLE_LOGGER::startLoggerMonitor(&g_logger));

    test(true);
    test(false);
    
    // stop monitor
    g_logger.stopMonitor();

    // wait for monitor thread to exit
    logMonitor->join();

    return 0;
};
