
///
///  \file  simplelogger.h
///
///  \brief a simple file logger
///
/// \example simplelogger/example.cpp

#ifndef SIMPLELOGGER_H
#define SIMPLELOGGER_H

#include <cstddef>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include "zlib.h"

#include "zflate.h"
#include "errorexception.h"
#include "listfile.h"
#include "simpleloggermajor.h"

/// \brief simple logger namesapce
namespace SIMPLE_LOGGER {

    inline std::string getPidTid()
    {
        std::string pidTid;
        try {
            pidTid = boost::lexical_cast<std::string>(GET_PID());
            pidTid += "\t";
            pidTid += boost::lexical_cast<std::string>(boost::this_thread::get_id());
            pidTid += "\t";
        } catch (...) {
            // do nothing
        }
        return pidTid;
    }

    /// \brief creates any missing dirs for the log file
    inline void createDirsAsNeeded(boost::filesystem::path const& name)
    {
        try {
            boost::filesystem::path parentDirs(name.parent_path());
            boost::filesystem::path::iterator iter(parentDirs.begin());
            boost::filesystem::path::iterator iterEnd(parentDirs.end());
            boost::filesystem::path createDir;
            // skip over root name as on windows it is typically the drive
            // letter colon name,  which can not be created as a directory
            // if not on windows there is no root name
            if (name.has_root_name()) {
                createDir /= *iter;
                ++iter;
            }
            // skip over root directory as it is the top most
            // directory '/' on all systems. if it does not exist
            // then there is a bigger problem and it will fail
            // when trying to create it (most likely trying to use
            // a disk that does not have a file system on it)
            if (name.has_root_directory()) {
                createDir /= *iter;
                ++iter;
            }
            for (/* empty */; iter != iterEnd; ++iter) {
                createDir /= *iter;
                if (!boost::filesystem::exists(createDir)) {
                    boost::filesystem::create_directory(createDir);
                }
            }
        } catch (std::exception const& e) {
            throw ERROR_EXCEPTION << "Creating parent directories failed for " << name.string() << ". Error: " << e.what();
        }
    }

    /// \brief available log levels.
    enum LOG_LEVEL {
        LEVEL_ALWAYS = 0,  ///< log everything
        LEVEL_ERROR,       ///< log errors
        LEVEL_WARNING,     ///< log warnings
        LEVEL_INFO,        ///< log info
        LEVEL_DEBUG        ///< log debug
    };

    /// \brief helper for SimpleLogger to support primitive types as well as ansi and wide char
    ///
    /// \note wide chars are converted to ansi chars.
    /// a dot('.') is used for any wchars that can not be converted to ansi char
    class SimpleMsg {
    public:
        /// \brief default constructor
        SimpleMsg()
            {}

        /// \brief copy constructor
        SimpleMsg(const SimpleMsg & othr)
            {
                m_msg << othr.str();
            }

        /// \brief assignment operator
        SimpleMsg & operator=(const SIMPLE_LOGGER::SimpleMsg & othr)
        {
            m_msg << othr.str();
            return *this;
        }

        /// \brief destructor
        ~SimpleMsg()
            {}

        /// \brief append data to the msg
        template<typename DATA_TYPE>
        SimpleMsg& operator<<(DATA_TYPE const& data) ///< data to log
            {
                m_msg << data;
                return *this;
            }

        /// \brief append wchar_t to msg.
        ///
        /// converts to char before appending.
        /// a dot ('.') is used of it can not be converted
        SimpleMsg& operator<<(wchar_t data) ///< data to log
            {

                std::locale loc;
                m_msg << std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.');
                return *this;
            }

        /// \brief append wchar_t* to msg.
        ///
        /// converts to char* before appending.
        /// a dot ('.') is used for any chars that can not be converted
        SimpleMsg& operator<<(wchar_t const* data) ///< data to log
            {
                for (/* empty */ ; L'\0' != *data; ++data) {
                    operator<<(*data);
                }
                return *this;
            }

        /// \brief append std::wstring to msg.
        ///
        /// converts to char* before appending.
        /// a dot ('.') is used for any chars that can not be converted
        SimpleMsg& operator<<(std::wstring const& data) ///< data to log
            {
                return operator<<(data.c_str());
            }

        std::string str() const
            {
                return m_msg.str();
            }

    protected:

    private:
        std::ostringstream m_msg;
    };

    /// \brief simple file logger
    ///
    /// see Makefile and vcproj for info on include and lib dirs required
    class SimpleLogger {
    public:
        /// \brief default constuctor
        ///
        /// need to call SimpleLogger::set when using the default constructor before the logger will actual work
        explicit SimpleLogger()
            : m_maxLogSizeBytes(1024*1024*100),
              m_rotateCount(0),
              m_retainSizeBytes(1024*1024*5),
              m_quit(false),
              m_on(false),
              m_level(SIMPLE_LOGGER::LEVEL_ALWAYS)
            {}

        /// \brief constructor to set log name (optional enable) and use defaults
        ///
        /// do not need to call SimpleLogger::set when using this constructor
        explicit SimpleLogger(boost::filesystem::path const& name, ///< name of log file
                              bool enabled = false)                ///< indicates if logging is enabled true: yes false: no (default no)
            : m_logName(name),
              m_maxLogSizeBytes(1024*1024*100),
              m_rotateCount(0),
              m_retainSizeBytes(1024*1024*5),
              m_quit(false),
              m_on(enabled),
              m_level(LEVEL_ERROR)
            {}

        /// \brief constructor to override defaults
        ///
        /// do not need to call SimpleLogger::set when using this constructor
        explicit SimpleLogger(boost::filesystem::path const& name, ///< name of log file
                              int maxLogSizeBytes,                 ///< maximum size of the log file before rotating (use 0 to keep default)
                              int rotateCount,                     ///< number of days to retain rotated log files (use 0 to keep default)
                              int retainSizeBytes,                 ///< number of bytes to keep in log file when rotating if rotateCount is 0 (use 0 to keep default)
                              bool enabled,                        ///< is logging enabled true: yes, false: noe
                              LOG_LEVEL logLevel                   ///< user defined value for the current log level
                              )
            {
                set(name, maxLogSizeBytes, rotateCount, retainSizeBytes, enabled, logLevel);
            }

        /// \brief used to set various options of the logger in one shot
        void set(boost::filesystem::path const& name, ///< name of log file
                 int maxLogSizeBytes,                 ///< maximum size of the log file before rotating (use 0 to keep default)
                 int rotateCount,                     ///< number of days to retain rotated log files (use 0 to keep default)
                 int retainSizeBytes,                 ///< number of bytes to keep in log file when rotating if rotateCount is 0 (use 0 to keep default)
                 bool enabled,                        ///< is logging enabled true: yes, false: noe
                 LOG_LEVEL logLevel,                  ///< user defined value for the current log level
                 bool createMissingDirs = true        ///< should missing dirs be created true: yes false: no
                 )
            {
                boost::mutex::scoped_lock guard(m_mutex);

                if (createMissingDirs) {
                    createDirsAsNeeded(name);
                }

                if (!name.has_root_path()) {
                    m_logName = "./";
                }
                m_logName /= name;

                openLogIfNeeded();

                if (maxLogSizeBytes > 0) {
                    m_maxLogSizeBytes = maxLogSizeBytes;
                }

                if (retainSizeBytes > 0) {
                    m_retainSizeBytes = retainSizeBytes;
                }

                if (m_retainSizeBytes >= m_maxLogSizeBytes) {
                    m_retainSizeBytes = 0;
                }

                if (rotateCount > 0) {
                    m_rotateCount = rotateCount;
                }

                m_on = enabled;
                m_level = logLevel;
            }

        /// \brief log data for generic data type
        template<typename DATA_TYPE>
        void log(LOG_LEVEL level,        ///< level required to log data
                 DATA_TYPE const& data,  ///< data to log
                 bool flushData = false  ///< true: flush after write, false: no flush
                 )
            {
                if (okToLog(level)) {
                    // intra-process first
                    boost::mutex::scoped_lock guard(m_mutex);
                    openLogIfNeeded();
                    rotateLogIfNeeded();
                    doLog(data, flushData);
                }
            }

        /// \brief log data for wchar_t type
        void log(LOG_LEVEL level,        ///< level required to log data
                 wchar_t data,           ///< data to log
                 bool flushData = false  ///< true: flush after write, false: no flush
                 )
            {
                std::locale loc;
                std::string narrowedData;
                log(level, std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.'), flushData);
            }

        /// \brief log data for wchar_t* type
        void log(LOG_LEVEL level,      ///< level required to log data
                 wchar_t const* data,  ///< data to log
                 bool flushData = false  ///< true: flush after write, false: no flush
                 )
            {
                std::locale loc;
                std::string narrowedData;
                for (/* empty */ ; L'\0' != *data; ++data) {
                    narrowedData += std::use_facet<std::ctype<wchar_t> >(loc).narrow(*data, '.');
                }
                log(level, narrowedData, flushData);
            }

        /// \brief log data for std::wstring type
        void log(LOG_LEVEL level,           ///< level required to log data
                 std::wstring const& data,  ///< data to log
                 bool flushData = false     ///< true: flush after write, false: no flush
                 )
            {
                log(level, data.c_str(), flushData);
            }

        /// \brief flush the logger
        SimpleLogger& flush()
            {
                boost::mutex::scoped_lock guard(m_mutex);
                m_log.flush();
                return *this;
            }

        /// \brief monitors compress log requests as well as checking for old logs to delete
        ///
        /// use startLoggerMonitor to start it in its own thread. if using your own thread mechinism, make sure to to pass in true for continious.
        void monitorLog(bool continuous) ///< do continious monitoring ture: yes do not return until stopMonitor issued, false: no just do work if needed and exit
            {
                try {
                    if (continuous) {
                        boost::gregorian::day_iterator dayIter(boost::gregorian::day_clock::local_day(), 1);

                        // set to current day midnight (adjusted for UTC time as the time_wait
                        // assumes it is getting a utc time not local time). by using current
                        // day midnight (time in the past), it will check for old logs when
                        // the server first starts up
                        boost::system_time waitUntilTime(*dayIter);
                        waitUntilTime += boost::posix_time::seconds(
                            boost::posix_time::time_duration(boost::posix_time::second_clock::universal_time()
                                                             - boost::posix_time::second_clock::local_time()).total_seconds());

                        // add 1 hour + 5 minutes to avoid having to properly adjust universal_time
                        // for daylight savings time. this means that when it is daylight savings
                        // rotate at 12:05am  (local time), not daylight savings rotate at
                        // 1:05am (local time). good enough
                        waitUntilTime += boost::posix_time::seconds(60*60+(60*5)); // 1 hour 5 minutes

                        boost::unique_lock<boost::mutex> lock(m_monitorLogMutex);
                        while (!m_quit) {
                            if (m_monitorLogCond.timed_wait(lock, waitUntilTime)) {
                                if (m_quit) {
                                    break;
                                } else if (!m_compressLog.empty()) {
                                    gzCompressLog(m_compressLog);
                                    m_compressLog.clear();
                                    deleteOldLogs();
                                } else {
                                    // woken up with nothing to do
                                }
                            } else {
                                waitUntilTime += boost::posix_time::hours(24); // check again in 24 hours
                                deleteOldLogs();
                            }
                        }
                    } else {
                        if (!m_compressLog.empty()) {
                            gzCompressLog(m_compressLog);
                            m_compressLog.clear();
                        }
                        deleteOldLogs();
                    }
                } catch (...) {
                    // TODO: not sure if anyting useful can be done at this point
                }
            }

        /// \brief stops the log montior
        void stopMonitor()
            {
                m_quit = true;
                m_monitorLogCond.notify_one();
            }

        /// \brief turn logging on
        void on()
            {
                m_on = true;
            }

        /// \brief turn logging off
        void off()
            {
                m_on = false;
            }

        /// \brief set the current log level
        void setLevel(LOG_LEVEL level) ///< level to set
            {
                m_level = level;
            }

        /// \brief returns the current logger level
        LOG_LEVEL level()
            {
                return m_level;
            }

        /// \brief checks if ok to log msg (true: yes, false: no)
        bool okToLog(LOG_LEVEL logLevel) ///< level of the msg to be logged
            {
                return (m_on && (LEVEL_ALWAYS == m_level || LEVEL_ALWAYS == logLevel || logLevel <= m_level));
            }

        /// \brief checks it logging is on (true: ues, false: no)
        bool isOn()
            {
                return m_on;
            }

    protected:
        /// \brief rotate log if needed
        void rotateLogIfNeeded(bool monitorThreadON = true)
            {
                if (m_logSize > m_maxLogSizeBytes) {
                    if (m_rotateCount > 0) {
                        rotate(monitorThreadON);
                    } else if (m_retainSizeBytes > 0) {
                        recycle();
                    } else {
                        truncate();
                    }
                }
            }

        /// <brief opens log file if needed
        ///
        /// \exception throws ERROR_EXCEPTION on error
        void openLogIfNeeded()
            {
                if (!m_log.is_open()) {
                    if (m_logName.empty()) {
                        throw ERROR_EXCEPTION << "missing log file name: " << m_logName.string();
                    }
                    m_log.open(m_logName.string().c_str(), std::ios::out | std::ios::app | std::ios::ate);
                    if (!m_log.good()) {
                        throw ERROR_EXCEPTION << "error opening log file: " << m_logName.string() << ". Error: " << strerror(errno);
                    }
                    m_logSize = m_log.tellp();
                }
            }

        /// \brief does the actual logging
        template<typename DATA_TYPE>
        void doLog(DATA_TYPE const& data,  ///< data to log
                   bool flushData          ///< true: flush after write, false: no flush
                   )
            {
                m_log << data;
                if (!m_log.good()) {
                    m_log.clear();
                    m_log.close();
                    if (m_logName.empty()) {
                        throw ERROR_EXCEPTION << "missing log file name: " << m_logName.string();
                    }
                    m_log.open(m_logName.string().c_str(), std::ios::out | std::ios::app | std::ios::ate);
                    if (!m_log.good()) {
                        throw ERROR_EXCEPTION << "error opening log file: " << m_logName.string() << ". Error: " << strerror(errno);
                    }
                    m_log << data;
                    if (!m_log.good()) {
                        m_log.clear();
                        m_log.close();
                        throw ERROR_EXCEPTION << "error writing data to file: " << m_logName.string() << ". Error: " << strerror(errno);
                    }
                }
                if (flushData) {
                    m_log.flush();
                }
                m_logSize = m_log.tellp();
            }

        /// \brief rotates log based on options passed in set
        void rotate(bool monitorThreadON = true)
            {
                m_log.close();

                // rename current log to rotate name
                boost::filesystem::path rotateName(m_logName.string());
                rotateName.replace_extension(boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time()) + ".log");
                boost::filesystem::rename(m_logName, rotateName);

                {
                    // scoped so the lock is released after setting compress log name
                    // lock is not need when notifying the monitor log
                    boost::unique_lock<boost::mutex> lock(m_monitorLogMutex);
                    m_compressLog = rotateName;
                }

                monitorThreadON ? m_monitorLogCond.notify_one() : monitorLog(false);

                // truncate existing log
                openTruncate();
            }


        /// \brief tuncates the log
        void truncate()
            {
                m_log.close();
                openTruncate();
            }

        /// \brief recyles the current log leaving reatin size bytes in the log
        void recycle()
            {
                m_log.close();

                if (0 == m_retainSizeBytes) {
                    openTruncate();
                    return;
                }

                std::vector<char> data(m_retainSizeBytes);

                std::vector<char>::size_type idx = 0;

                std::ifstream tmpLog(m_logName.string().c_str());
                if (!tmpLog.good()) {
                    throw ERROR_EXCEPTION << "error open log " << m_logName.string() << ": " << errno;
                }

                // MAYBE: check the actual retain size and if it is "large"
                // then create tmp file to write too instead of trying to read
                // it all into memory and then just read tmp and write to
                // log file or delete log file and rename tmp file.
                tmpLog.seekg(-m_retainSizeBytes, std::ios::end);

                tmpLog.getline(&data[0], data.size());

                std::vector<std::string> logData;
                while (tmpLog.good()) {
                    tmpLog.getline(&data[0], data.size());
                    if (tmpLog.gcount() > 0) {
                        logData.push_back(&data[0]);
                    }
                }

                tmpLog.close();
                openTruncate();

                std::vector<std::string>::iterator iter(logData.begin());
                std::vector<std::string>::iterator iterEnd(logData.end());
                for (/* empty*/; iter != iterEnd; ++iter) {
                    m_log << (*iter) << '\n';
                }
                m_log.flush();
            }


        /// \brief opens the log and truncates it
        void openTruncate()
            {
                m_log.open(m_logName.string().c_str(), std::ios::out | std::ios::trunc);
                if (!m_log.good()) {
                    throw ERROR_EXCEPTION << "error opening log " << m_logName.string() << ": " << errno;
                }
            }

        /// \brief delets old logs
        void deleteOldLogs()
            {
                boost::filesystem::path fileSpecPath(m_logName);
                fileSpecPath.replace_extension(".*.log*");

                std::string files;
                ListFile::listFileGlob(fileSpecPath, files, false); // only want file names in this case

                if (!files.empty()) {
                    typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
                    boost::char_separator<char> sep("\n");
                    tokenizer_t tokens(files, sep);
                    tokenizer_t::iterator iter(tokens.begin());
                    tokenizer_t::iterator iterEnd(tokens.end());
                    size_t numFiles = 0;
                    std::vector<std::string> vlogFiles;
                    for (/* empty */; iter != iterEnd; ++iter) {
                        std::string::size_type idx = (*iter).find(".log");
                        // this check is needed as the listFileGlob could return matches that are not really
                        // the log files. e.g. .log.xy, .log.asdf.gz, .logx, .log.gzx, etc.
                        // must really end in ".log" or ".log.gz"
                        if ((std::string::npos != idx                                                         // should not be needed, play it safe
                             && (4 == (*iter).size() - idx                                                    // check if ends in ".log"
                                 || (7 == (*iter).size() - idx && std::string::npos != (*iter).find(".gz")))) // check if ends in ".log.gz"
                            ) {
                            vlogFiles.push_back(*iter);
                            numFiles++;
                            if (numFiles > m_rotateCount)
                            {
                                boost::filesystem::remove(vlogFiles.front());
                                vlogFiles.erase(vlogFiles.begin());
                                numFiles--;
                            }
                        }
                    }
                }
            }


        /// \brief compress logs in m_logsToCompress
        ///
        /// \param name log to compress
        void gzCompressLog(boost::filesystem::path const& name)
            {
                std::string compressName(name.string() + ".gz");

                Zflate zFlate(Zflate::COMPRESS);

                zFlate.process(name.string(), compressName, true);
            }


        /// \brief remove a log file
        void removeLog(boost::filesystem::path const& name) ///< log to be removed
            {
                boost::filesystem::remove(name);
            }

        std::string getLogName() const ///< returns log file name
            {
            return m_logName.string();
            }

    private:
        boost::mutex m_mutex; ///< for intra-process serialization access to the log

        boost::filesystem::path m_logName; ///< log file name

        int m_maxLogSizeBytes;  ///< max size the log should grow before rotating

        int m_rotateCount; ///< number of files to retain rotated logs

        int m_retainSizeBytes; ///< max number of bytes to retain in the log file when rotating (ignored if m_rotateCount > 0), if 0, log is truncated

        std::ofstream::pos_type m_logSize; ///< current log size

        std::ofstream m_log; ///< log stream object

        boost::condition_variable m_monitorLogCond; ///< used to signal when a log is ready for compression

        boost::mutex m_monitorLogMutex; ///< used in conjuntion with m_monitorLogCond for serialization

        bool m_quit; ///< used to indicate log monitor should quit true: quit, false: do not quit

        boost::filesystem::path m_compressLog; ///< nome of log that should be compressed

        bool m_on; ///< indicates if logging is on true: on, false: off

        LOG_LEVEL m_level; ///< current log level that a message needs to be logged
    };

    /// \brief starts logger monitor in its own thread.
    ///
    /// issue SimpleLogger.stopMonitor() to tell the monitor to stop and exit the thread
    inline boost::shared_ptr<boost::thread> startLoggerMonitor(SimpleLogger* logger)
    {
        return boost::shared_ptr<boost::thread>(new boost::thread(
                                                    boost::bind(&SimpleLogger::monitorLog,
                                                                logger, true)));
    }

    /// \brief helper macro to log data using a SimpleLogger
    ///
    /// outputs the data in the following format\n
    /// \n
    /// date time<tab>pid<tab>tid<tab>xEntryType<tab>xData<new Line>
    ///
    /// \note
    /// i pid and tid are only logged if requested
    /// \li xData can contain iostream style syntax.
    /// \li if you want to include details about line, function, file use one of the ATLOC macros
    ///     form host/errorexception/atlog.h (or standard compiler macros) in the call
    /// \n
    /// E.G. SIMPLE_LOGGER_LOG(logger, ATLOC << "could not create file " << fileName << ": " << errno);
    ///
    /// \param xLogger SimpleLogger to use to do the logging
    /// \param xData data to be logged
    /// \param xEntryType type of entry (typically ERROR, WARNING, INFO, DEBUG), 0 (NULL) do not add
    /// \param xLogLevel level needed to log the data
    /// \param xAddPidTid indicates if process id and thread id should be logged true: yes, false: no
    ///
    // do not add any atloc, __LINE__, __FUNCTION__, __FILE__ macros directly to this macro implementation
    // callers should add them as needed when they call it
    // do not let exception be thrown, just catch all and ignore
#define SIMPLE_LOGGER_LOG(xLogger, xData, xEntryType, xLogLevel, xAddPidTid) \
    do {                                                                \
        try {                                                           \
            if (xLogger.okToLog(xLogLevel)) {                           \
                SIMPLE_LOGGER::SimpleMsg xMsg;                          \
                xMsg << boost::posix_time::second_clock::local_time()   \
                     << '\t'                                            \
                     << (xAddPidTid ? SIMPLE_LOGGER::getPidTid() : std::string()) \
                     << (0 == xEntryType ? "" : xEntryType)             \
                     << (0 == xEntryType ? "" : "\t")                   \
                     << xData                                           \
                     << '\n';                                           \
                xLogger.log(xLogLevel, xMsg.str(), true);               \
            }                                                           \
        } catch (...) {                                                 \
        }                                                               \
    } while (false)

    /// \brief used to log error data
    ///
    /// outputs the data in the following format\n
    /// \n
    /// date time<tab>pid<tab>tid<tab>ERROR<tab>yData<new Line>
    ///
    /// \param yLogger SimpleLogger to use to do the logging
    /// \param yData data to be logged
    /// \param yAddPidTid indicates if process id and thread id should be logged true: yes, false: no
    ///
    /// \note
    /// \li pid and tid are only logged if requested
    /// \li yData can contain iostream style syntax.
    /// \li if you want to include details about line, function, file use one of the ATLOC macros
    ///     form host/errorexception/atlog.h (or standard compiler macros) in the call
    /// \n
    /// E.G. SIMPLE_LOGGER_ERROR(logger, ATLOC << "could not create file " << fileName << ": " << errno);
    ///
    // do not add any atloc, __LINE__, __FUNCTION__, __FILE__ macros directly to this macro implementation
    // callers should add them as needed when they call it
#define SIMPLE_LOGGER_ERROR(yLogger, yData, yAddPidTid)         \
    do {                                                        \
        SIMPLE_LOGGER_LOG(yLogger,                              \
                          yData,                                \
                          "ERROR",                              \
                          SIMPLE_LOGGER::LEVEL_ERROR,           \
                          yAddPidTid);                          \
    } while (false)

    /// \brief used to log warning data
    ///
    /// outputs the data in the following format\n
    /// \n
    /// \verbatim date time<tab>WARNING<tab>yData<new Line> \endverbatim
    ///
    /// \param yLogger SimpleLogger to use to do the logging
    /// \param yData data to be logged
    /// \param yAddPidTid indicates if process id and thread id should be logged true: yes, false: no
    ///
    /// \note
    /// \li pid and tid are only logged if requested
    /// \li yData can contain iostream style syntax.
    /// \li if you want to include details about line, function, file use one of the ATLOC macros
    ///     form host/errorexception/atlog.h (or standard compiler macros) in the call
    /// \n
    /// E.G. SIMPLE_LOGGER_WARNING(logger, ATLOC << fileName << " not found");
    ///
    // do not add any atloc, __LINE__, __FUNCTION__, __FILE__ macros directly to this macro implementation
    // callers should add them as needed when they call it
#define SIMPLE_LOGGER_WARNING(yLogger, yData, yAddPidTid)               \
    do {                                                                \
        SIMPLE_LOGGER_LOG(yLogger,                                      \
                          yData,                                        \
                          "WARNING",                                    \
                          SIMPLE_LOGGER::LEVEL_WARNING,                 \
                          yAddPidTid);                                  \
    } while (false)

    /// \brief used to log information data
    ///
    /// outputs the data in the following format\n
    /// \n
    /// \verbatim date time<tab>INFO<tab>yData<new Line> \endverbatim
    ///
    /// \param yLogger SimpleLogger to use to do the logging
    /// \param yData data to be logged
    /// \param yAddPidTid indicates if process id and thread id should be logged true: yes, false: no
    ///
    /// \note
    /// \li pid and tid are only logged if requested
    /// \li yData can contain iostream style syntax.
    /// \li if you want to include details about line, function, file use one of the ATLOC macros
    ///     form host/errorexception/atlog.h (or standard compiler macros) in the call
    /// \n
    /// E.G. SIMPLE_LOGGER_INFO(logger, ATLOC << "number of matches found: " << matches);
    ///
    // do not add any atloc, __LINE__, __FUNCTION__, __FILE__ macros directly to this macro implementation
    // callers should add them as needed when they call it
#define SIMPLE_LOGGER_INFO(yLogger, yData, yAddPidTid)          \
    do {                                                        \
        SIMPLE_LOGGER_LOG(yLogger,                              \
                          yData,                                \
                          "INFO",                               \
                          SIMPLE_LOGGER::LEVEL_INFO,            \
                          yAddPidTid);                          \
    } while (false)

    /// \brief used to log debug
    ///
    /// outputs the data in the following format\n
    /// \n
    /// \verbatim date time<tab>DEBUG<tab>yData<new Line> \endverbatim
    ///
    /// \param yLogger SimpleLogger to use to do the logging
    /// \param yData data to be logged
    /// \param yAddPidTid indicates if process id and thread id should be logged true: yes, false: no
    ///
    /// \note
    /// \li pid and tid are only logged if requested
    /// \li yData can contain iostream style syntax.
    /// \li if you want to include details about line, function, file use one of the ATLOC macros
    ///     form host/errorexception/atlog.h (or standard compiler macros) in the call
    /// \n
    /// E.G. SIMPLE_LOGGER_DEBUG(logger, ATLOC << "calling foo(" << param1 <<", " << param2 << ')');
    ///
    // do not add any atloc, __LINE__, __FUNCTION__, __FILE__ macros directly to this macro implementation
    // callers should add them as needed when they call it
#define SIMPLE_LOGGER_DEBUG(yLogger, yData, yAddPidTid)         \
    do {                                                        \
        SIMPLE_LOGGER_LOG(yLogger,                              \
                          yData,                                \
                          "DEBUG",                              \
                          SIMPLE_LOGGER::LEVEL_DEBUG,           \
                          yAddPidTid);                          \
    } while (false)

    /// \brief used to always log the data independent of the current log level
    ///
    /// typically used when there is no need for tracking the type (ERROR, WARNING, DEBUG, INFO) of og entry.
    /// E.g. monitor, trace, transfer, etc. logs (i.e. non-error logs)
    ///
    /// outputs the data in the following format\n
    /// \n
    /// \verbatim date time<tab>WARNING<tab>yData<new Line> \endverbatim
    ///
    /// \param yLogger SimpleLogger to use to do the logging
    /// \param yData data to be logged
    /// \param yAddPidTid indicates if process id and thread id should be logged true: yes, false: no
    ///
    /// \note
    /// \li pid and tid are only logged if requested
    /// \li yData can contain iostream style syntax.
    /// \li if you want to include details about line, function, file use one of the ATLOC macros
    ///     form host/errorexception/atlog.h (or standard compiler macros) in the call
    /// \n
    /// E.G. SIMPLE_LOGGER_ALWAYS(logger, "successfully updated db. Number of affected rows: " << affectedRows);
    ///
    // do not add any atloc, __LINE__, __FUNCTION__, __FILE__ macros directly to this macro implementation
    // callers should add them as needed when they call it
#define SIMPLE_LOGGER_ALWAYS(yLogger, yData, yAddPidTid)        \
    do {                                                        \
        SIMPLE_LOGGER_LOG(yLogger,                              \
                          yData,                                \
                          0,                                    \
                          SIMPLE_LOGGER::LEVEL_ALWAYS,          \
                          yAddPidTid);                          \
    } while (false)

} // namespace SIMPLE_LOGGER


#endif // SIMPLELOGGER_H
