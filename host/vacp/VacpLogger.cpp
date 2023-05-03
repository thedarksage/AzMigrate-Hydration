#include <stdafx.h>

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

#include <ace/OS.h>
#include <ace/Time_Value.h>

#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "inmsafecapis.h"
#include "createpaths.h"

#ifdef _WIN32
#define VSNPRINTF(buf, size, maxCount, format, args) _vsnprintf_s(buf, size, maxCount, format, args)
#else
#define VSNPRINTF(buf, size, maxCount, format, args) inm_vsnprintf_s(buf, size, format, args)
#endif

using namespace std;

#define TRIM_CHARS "\n"

#define TIME_STAMP_LENGTH 70

#define MAX_VACP_LOG_FILE_SIZE (10 * 1024 * 1024)

const int MAX_OUTPUT_SIZE = 256 * 1024; // in bytes, 256KB

extern short        gsVerbose;
extern bool         gbParallelRun;

std::string const PreUnderscore("pre_");
std::string const VacpLog("VacpLog");
std::string const LogExtension(".log");
std::string const VacpLogPattern(VacpLog + "_*" + LogExtension);

std::stringstream   glogStream;
std::ofstream       glogFileStream;
std::string         glogFilePath;
uint32_t            glogFileMaxSize;
boost::mutex        g_logMutex;
uint64_t            glogToBuffer;

void inm_printf(const char* format, ...);
void inm_printf(short logLevl, const char* format, ...);

int inm_init_file_log(const std::string& logFilePath)
{
    glogFilePath = logFilePath;
    try
    {
        if (!boost::filesystem::exists(logFilePath))
        {
            CreatePaths::createPathsAsNeeded(logFilePath);
        }

        glogFileStream.clear();
        glogFileStream.open(logFilePath.c_str(), ios::out | ios::app);
        glogFileMaxSize = MAX_VACP_LOG_FILE_SIZE;
    }
    catch (const std::exception ex)
    {
        inm_printf("inm_init_file_log: failed to create file %s with exception %s\n", logFilePath.c_str(), ex.what());
    }
    catch (...)
    {
        inm_printf("inm_init_file_log: failed to create file %s with an unknown exception.\n", logFilePath.c_str());
    }
    return 0;
}

void inm_truncate_file_log()
{
    int currPos = glogFileStream.tellp();
    if (currPos != -1)
    {
        if (currPos > glogFileMaxSize)
        {
            glogFileStream.close();
            glogFileStream.open(glogFilePath.c_str(), ios::out | ios::trunc);
        }
    }
    else
    {
#if defined(WIN32) || defined(WIN64)
        glogFileStream.clear(0);
#else
        glogFileStream.clear();
#endif
    }
}

void inm_printf(const char* format, ... )
{
    boost::mutex::scoped_lock guard(g_logMutex);

    std::string msg(MAX_OUTPUT_SIZE, 0);

    va_list a;
    va_start( a, format );

    VSNPRINTF(&msg[0], msg.size(), msg.size() - 1, format, a);
    
    if (0 == strlen(&msg[0]))
    {
        return;
    }

    va_end( a );

    boost::trim_left_if(msg, boost::is_any_of(TRIM_CHARS));

    struct tm today;
    time_t ltime;
    time(&ltime);

    ACE_UINT64 msecs = 0;
    const ACE_Time_Value tv = ACE_OS::gettimeofday();
    tv.msec(msecs);

#if defined(WIN32) || defined(WIN64) 
        localtime_s(&today, &ltime);
#else
        localtime_r(&ltime, &today);
#endif

    std::string present;
    present.resize(TIME_STAMP_LENGTH);
    inm_sprintf_s(&present[0], present.capacity(), "(%02d-%02d-20%02d %02d:%02d:%02d:%llu): DEBUG ",
        today.tm_mon + 1,
        today.tm_mday,
        today.tm_year - 100,
        today.tm_hour,
        today.tm_min,
        today.tm_sec,
        msecs
        );

    if (glogToBuffer)
    {
        if (gbParallelRun)
            glogStream << "#~> " << present.c_str() << ACE_OS::getpid() << " " << ACE_OS::thr_self() << " ";
        glogStream << msg.c_str();
    }
    else
    {
        if (gbParallelRun)
        {
            inm_truncate_file_log();
            glogFileStream << "#~> " << present.c_str() << ACE_OS::getpid() << " " << ACE_OS::thr_self() << " " << msg.c_str();
            glogFileStream.flush();
        }
        else
        {
            cout << msg.c_str();
        }
    }

    return;
}

void inm_printf(short logLevl, const char* format, ... )
{

    if (logLevl > gsVerbose)
        return;

    boost::mutex::scoped_lock guard(g_logMutex);

    std::string msg(MAX_OUTPUT_SIZE, 0);

    va_list a;
    va_start( a, format );

    VSNPRINTF(&msg[0], msg.size(), msg.size() - 1, format, a);
    
    if (0 == strlen(&msg[0]))
    {
        return;
    }

    va_end( a );

    boost::trim_left_if(msg, boost::is_any_of(TRIM_CHARS));

    struct tm today;
    time_t ltime;
    time(&ltime);

    ACE_UINT64 msecs = 0;
    const ACE_Time_Value tv = ACE_OS::gettimeofday();
    tv.msec(msecs);

#if defined(WIN32) || defined(WIN64) 
    localtime_s(&today, &ltime);
#else
    localtime_r(&ltime, &today);
#endif

    std::string present;
    present.resize(TIME_STAMP_LENGTH, '0');
    inm_sprintf_s(&present[0], present.capacity(), "(%02d-%02d-20%02d %02d:%02d:%02d:%llu): ALWAYS ",
        today.tm_mon + 1,
        today.tm_mday,
        today.tm_year - 100,
        today.tm_hour,
        today.tm_min,
        today.tm_sec,
        msecs
        );

    if (glogToBuffer)
    {
        if (gbParallelRun)
            glogStream << "#~> " << present.c_str() << ACE_OS::getpid() << " " << ACE_OS::thr_self() << " ";
        glogStream << msg.c_str();

    }
    else
    {
        if (gbParallelRun)
        {
            inm_truncate_file_log();
            glogFileStream << "#~> " << present.c_str() << ACE_OS::getpid() << " " << ACE_OS::thr_self() << " " << msg.c_str();
            glogFileStream.flush();
        }
        else
        {
            cout << msg.c_str();
        }
    }

    return;
}

void inm_flush_log()
{
    if (gbParallelRun)
    {
        inm_truncate_file_log();
        glogFileStream << glogStream.str();
        glogFileStream.flush();
    }
    else
    {
        cout << glogStream.str();
    }
    glogStream.str("");
    glogStream.clear();
}

void inm_log_to_buffer_start()
{
    inm_printf("inm_log_to_buffer_start\n");
    boost::mutex::scoped_lock guard(g_logMutex);
    ++glogToBuffer;
}

void inm_log_to_buffer_end()
{
    inm_printf("inm_log_to_buffer_end\n");
    boost::mutex::scoped_lock guard(g_logMutex);
    
    if (!glogToBuffer)
        return;

    --glogToBuffer;
    
    if (!glogToBuffer)
        inm_flush_log();
}

void NextLogFileName(const std::string& logFilePath,
    std::string& preTarget,
    std::string& target,
    const std::string& suffix = std::string())
{
    boost::mutex::scoped_lock guard(g_logMutex);

    static std::string prevname;
    do
    {
        std::ostringstream name;

        name << VacpLog << "_";
        if (!suffix.empty())
        {
            name << suffix << "_";
        }

        unsigned long long secSinceEpoch = boost::posix_time::time_duration(boost::posix_time::second_clock::universal_time()
            - boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();

        name << secSinceEpoch << LogExtension;

        preTarget = logFilePath;
        preTarget += ACE_DIRECTORY_SEPARATOR_CHAR;
        preTarget += PreUnderscore + name.str();

        target = logFilePath;
        target += ACE_DIRECTORY_SEPARATOR_CHAR;
        target += name.str();

        if (prevname.compare(preTarget))
        {
            break;
        }
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    } while (true);

    prevname = preTarget;

    return;
}

bool RenameLogFile(const std::string& logFilePath, const std::string& newLogFilePath, std::string& err)
{
    boost::filesystem::path path(logFilePath);
    const boost::filesystem::path new_path(newLogFilePath);
    try
    {
        if (!boost::filesystem::exists(path))
        {
            err = "file does not exist";
            return false;
        }

        int renameRetry = 4;
        boost::system::error_code ec;
        while (--renameRetry)
        {
            boost::filesystem::rename(path, new_path, ec);
            if (!ec.value())
            {
                return true;
            }
            boost::this_thread::sleep(boost::posix_time::seconds(1));
        }
        if (!renameRetry)
        {
            std::stringstream ss;
            ss << __FUNCTION__ << ": failed to rename file " << logFilePath << " to " << newLogFilePath << " with error " << ec.message();
            err = ss.str();
            inm_printf("%s\n", __FUNCTION__, ss.str().c_str());
        }
    }
    catch (const std::exception ex)
    {
        std::stringstream ss;
        ss << __FUNCTION__ << ": failed to rename file " << logFilePath << " to " << newLogFilePath<< " with exception " << ex.what();
        err = ss.str();
        inm_printf("%s\n", __FUNCTION__, ss.str().c_str());
    }
    catch (...)
    {
        std::stringstream ss;
        ss << __FUNCTION__ << ": failed to rename file " << logFilePath << " to " << newLogFilePath << " with an unknown exception.";
        err = ss.str();
        inm_printf("%s\n", __FUNCTION__, ss.str().c_str());
    }
    return false;
}
