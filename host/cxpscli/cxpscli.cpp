
///
///  \file cxpscli.cpp
///
///  \brief cx process server cli for enabling/disabling xfer log, monitor log, and tracing
///

#include <iostream>
#include <stdexcept>
#include <string>
#include <map>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "cxpslogger.h"
#include "serverctl.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#if !defined(__GNUC__) || (__GNUC__ >= 4)
#include "cxpsmsgqueue.h"
#endif

class CxpsOptions {
public:
    explicit CxpsOptions(std::string const& confFile = std::string())
        : m_confFile(confFile),
          m_appendAllowed(false),
          m_appendExclude(false)
        {
        }

    void setConfFile(std::string const& confFile)
        {
            m_confFile = confFile;
        }

    void addTagValue(std::string const& tag, std::string const& value)
        {
            m_options.insert(std::make_pair(tag, value));
        }

    void setAppendAllowed()
        {
            m_appendAllowed = true;
        }

    void setAppendExclude()
        {
            m_appendExclude = true;
        }

    std::string getError()
        {
            return m_error;
        }

    bool haveOptionsMissingConf()
        {
            return m_confFile.empty() && m_options.size() > 0;
        }

    bool save()
        {
            m_error.clear();
            if (!m_options.empty()) {
                // FIXME: for now need a special lock file as the version boost
                // being used does not correctly lock files on windows in that the
                // file_lock uses its own file handle which prevents accessing the
                // file through other file handes e.g. std:fstrem. Once we upgrade
                // to the latest boost (1.47.0 or higher), we can properly lock
                // the file directly as the issue was fixed
                std::string lockFile(m_confFile);
                lockFile += ".lck";
                if (!boost::filesystem::exists(lockFile)) {
                    std::ofstream tmpLockFile(lockFile.c_str());
                }
                boost::interprocess::file_lock fileLock(lockFile.c_str());
                boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);
                writeOptions();
                if (m_error.empty()) {
                    renameFiles();
                }
            }
            return m_error.empty();
        }
    void clearOptions()
        {
            m_options.clear();
        }

protected:
    typedef std::map<std::string, std::string> options_t; ///< holds cxps options to save first: name, second: value

    void writeOptions()
        {
            std::ifstream confFile(m_confFile.c_str());
            if (!confFile.good()) {
                m_error = "unable to open conf file ";
                m_error += m_confFile;
                m_error += ": ";
                m_error += boost::lexical_cast<std::string>(errno);
                return;
            }
            std::string tmpConfFile(m_confFile);
            tmpConfFile += ".tmp";
            std::ofstream confTmp(tmpConfFile.c_str());
            if (!confTmp.good()) {
                m_error = "unable to open conf tmp file ";
                m_error += tmpConfFile;
                m_error += ": ";
                m_error += boost::lexical_cast<std::string>(errno);
                return;
            }

            std::string line;  // assumes conf file lines are < 1024
            while (confFile.good()) {
                std::getline(confFile, line);
                if (confFile.eof()) {
                    break;
                }
                if (!confFile.good()) {
                    m_error = "error reading file ";
                    m_error += m_confFile;
                    m_error += ": ";
                    m_error += boost::lexical_cast<std::string>(errno);
                    return;
                }
                std::string::size_type idx = line.find_first_of("=");
                if (std::string::npos != idx) {
                    if (' ' == line[idx - 1]) {
                        --idx;
                    }
                    options_t::iterator iter = m_options.find(line.substr(0, idx));
                    if (m_options.end() != iter) {
                        if ((m_appendAllowed && boost::algorithm::equals("allowed_dirs", (*iter).first))
                            || (m_appendExclude && boost::algorithm::equals("exclude_dirs", (*iter).first))) {
                            line += ";";
                            line += (*iter).second;
                            confTmp << line << '\n';
                        } else {
                            confTmp << (*iter).first << " = " << (*iter).second << '\n';
                        }
                        if (!confTmp.good()) {
                            m_error = "error reading file ";
                            m_error += tmpConfFile;
                            m_error += ": ";
                            m_error += boost::lexical_cast<std::string>(errno);
                            return;
                        }
                        m_options.erase(iter);
                        continue;
                    }
                }
                confTmp << line << '\n';
                if (!confTmp.good()) {
                    m_error = "error reading file ";
                    m_error += tmpConfFile;
                    m_error += ": ";
                    m_error += boost::lexical_cast<std::string>(errno);
                    return;
                }
            }
            // finally write any options not yet written this is in case for some
            // reason the option was removed from cxps.conf and was not required
            // by cxps for it to run
            options_t::iterator iter(m_options.begin());
            options_t::iterator iterEnd(m_options.end());
            for (/* empty */; iter != iterEnd; ++iter) {
                confTmp << (*iter).first << " = " << (*iter).second << '\n';
            }
        }

    void renameFiles()
        {
            std::string backName(m_confFile);
            backName += ".";
            backName += boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time());
            backName += ".bak";
            try {
                boost::filesystem::rename(m_confFile, backName);
            } catch (std::exception const& e) {
                std::cout << "failed to back up cxps.conf file: " << e.what() << '\n';
                return;
            }
            std::string tmpName(m_confFile);
            tmpName += ".tmp";
            try {
                boost::filesystem::remove(m_confFile);
                boost::filesystem::rename(tmpName, m_confFile);
            } catch (std::exception const& e) {
                std::cout << "failed to rename " << tmpName << " to " << m_confFile << ": " << e.what() << '\n';
                return;
            }
        }

private:
    std::string m_confFile;
    std::string m_error;
    options_t m_options;
    bool m_appendAllowed;
    bool m_appendExclude;
};

typedef std::pair<bool, bool> loggingPair_t;
typedef std::pair<bool, int> loggingLevelPair_t;

/// \brief how to use this command line tool
///
///
/// cxpscli [-m <on|off>] [-x <on|off>] [-t <on|off>] [-r] [-h]\n
/// \li \c -m <on|off>: turn monitor logging on or off
/// \li \c -x <on|off>: turn data transfer logging on or off
/// \li \c -t <on|off>: turn tracing on or off
/// (note has no effect if process that owns the message queue
///  is running as an NT service or unix/linux daemon)
/// \li \c -r: remove the shared message queue.
///  this should only be used if the cx process server won't start
///  because the message queue already exists\n
/// \li \c -h: display help
void usage()
{
    std::cout << "usage: cxpscli options | -h\n"
              << '\n'
              << "  -h : display help\n"
              << '\n'
              << "options: \n"
#if !defined(__GNUC__) || (__GNUC__ >= 4)
              << "  These are temporary, take affect with out restarting cxps and will be lost if cxps is restarted\n"
              << "  -w <on|off> : turn warning logging on or off\n"
              << "  -m <on|off> : turn monitor logging on or off\n"
              << "  -x <on|off> : turn data transfer logging on or off\n"
              << "  -l 1 | 2 | 3: set monitor log level\n"
              << "                1 - log server start and session create/start/end\n"
              << "                2 - level 1 plus log request received/begin/end/fail\n"
              << "                3 - levels 1 and 2 plus log request processing\n"
              << "  -r          : remove the shared message queue\n"
              << "                this should only be used if the cx process server won't start\n"
              << "                because the message queue already exists\n"
              << '\n'
#endif // !defined(__GNUC__) || (__GNUC__ >= 4)\
              << '\n'
              << "  These are written to the cxps.conf file and require you to restart cxps process to be picked up\n"
              << "  --conf <conf file>         : cxps conf file. Default is cxps.conf in cxpscli directory\n"
              << "  --id <id>                  : host id.\n"
              << "  --csip <cs ip>             : CS ip address.\n"
              << "  --csport <port>            : CS non ssl port\n"
              << "  --cssslport <ssl port>     : CS ssl port\n"
              << "  --sslport <ssl port>       : PS ssl port\n"
              << "  --cssecure <yes | no>      : use secure connection to CS\n"
              << "  --cfsmode <yes | no>       : enable cfs mode.\n"
              << "  --cfssecurelogin <yes | no>: enble secure cfs login\n"
              << "  --allowed <dirs>           : comma separated list of allowed dirs. Replaces current allowed dirs.\n"
              << "                               If any spaces in dirs place in quotes\n"
              << "  --exclude <dirs>           : comma separated list of exclude dirs. Replaces current exclude dirs.\n"
              << "                               If any spaces in dirs place in quotes\n"
              << "  --append_allowed <dirs>    : comma separated list of dirs to appends to current allowed dirs.\n"
              << "                               If any spaces in dirs place in quotes\n"
              << "  --append_exclude <dirs>    : comma separated list of dirs to appends to current allowed dirs.\n"
              << "                               If any spaces in dirs place in quotes\n"
              << '\n';
    exit(0);
}

/// \brief parse the command line
void parseCmdline(int argc,
                  char* argv[],
                  loggingPair_t& warning,
                  loggingPair_t& xfer,
                  loggingPair_t& monitor,
                  bool& removeMsgQueue,
                  loggingLevelPair_t& monitorLevel,
                  CxpsOptions& cxpsOptions)
{
    bool argsOk = true;
    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0] || '\0' == argv[i][1]) {
            // no dash or only single dash
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        } else if ('-' == argv[i][1]) {
            // long options
            if (boost::algorithm::equals("--conf", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --conf" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.setConfFile(argv[i]);
                }
            } else if (boost::algorithm::equals("--id", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --id" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("id", argv[i]);
                }
            } else if (boost::algorithm::equals("--csip", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --csip" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("cs_ip_address", argv[i]);
                }
            } else if (boost::algorithm::equals("--csport", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --csport" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("cs_port", argv[i]);
                }
            } else if (boost::algorithm::equals("--cssslport", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --cssslport" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("cs_ssl_port", argv[i]);
                }
            }
            else if (boost::algorithm::equals("--sslport", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --sslport" << '\n';
                    argsOk = false;
                }
                else {
                    cxpsOptions.addTagValue("ssl_port", argv[i]);
                }
            } else if (boost::algorithm::equals("--cssecure", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --cssecure" << '\n';
                    argsOk = false;
                } else {
                    if (boost::algorithm::iequals(argv[i], "yes") || boost::algorithm::iequals(argv[i], "no")) {
                        cxpsOptions.addTagValue("cs_use_secure", argv[i]);
                    }
                }
            } else if (boost::algorithm::equals("--cfsmode", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --cfsmode" << '\n';
                    argsOk = false;
                } else {
                    if (boost::algorithm::iequals(argv[i], "yes") || boost::algorithm::iequals(argv[i], "no")) {
                        cxpsOptions.addTagValue("cfs_mode", argv[i]);
                    }
                }
            } else if (boost::algorithm::equals("--cfssecurelogin", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --cfssecurelogin" << '\n';
                    argsOk = false;
                } else {
                    if (boost::algorithm::iequals(argv[i], "yes") || boost::algorithm::iequals(argv[i], "no")) {
                        cxpsOptions.addTagValue("cfs_secure_login", argv[i]);
                    }
                }
            } else if (boost::algorithm::equals("--allowed", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --allowed" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("allowed_dirs", argv[i]);
                }                                               
            } else if (boost::algorithm::equals("--exclude", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --exclude" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("exclude_dirs", argv[i]);
                }                                               
            } else if (boost::algorithm::equals("--append_allowed", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --append_allowed" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("allowed_dirs", argv[i]);
                    cxpsOptions.setAppendAllowed();
                }                                               
            } else if (boost::algorithm::equals("--append_exclude", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --eappend_xclude" << '\n';
                    argsOk = false;
                } else {
                    cxpsOptions.addTagValue("exclude_dirs", argv[i]);
                    cxpsOptions.setAppendExclude();
                }                                               
            } else {
                std::cout << "*** invalid option: " << argv[i] << '\n';
                argsOk = false;
            }

        } else if ('\0' != argv[i][2]) {
            // single dash followed by more then 1 char
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        } else {
            // hosrt options
            switch (argv[i][1]) {
                case 'h':
                    usage();
                    break;

#if !defined(__GNUC__) || (__GNUC__ >= 4)
                case 'm':
                    ++i;
                    monitor.first = true;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "\n*** missing value for -m\n";
                        argsOk = false;
                        break;
                    }

                    monitor.second = (0 == strcmp(argv[i], "on"));
                    break;

                case 'l':
                    ++i;
                    monitorLevel.first = true;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "\n*** missing value for -l\n";
                        argsOk = false;
                        break;
                    }

                    monitorLevel.second = boost::lexical_cast<int>(argv[i]);
                    break;

                case 'r':
                    removeMsgQueue = true;
                    break;

                case 'w':
                    ++i;
                    warning.first = true;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "\n*** missing value for -w\n";
                        argsOk = false;
                        break;
                    }

                    warning.second = (0 == strcmp(argv[i], "on"));
                    break;

                case 'x':
                    ++i;
                    xfer.first = true;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "\n*** missing value for -x\n";
                        argsOk = false;
                        break;
                    }

                    xfer.second = (0 == strcmp(argv[i], "on"));
                    break;
#endif // !defined(__GNUC__) || (__GNUC__ >= 4)
                default:
                    std::cout << "\n*** invalid arg: " << argv[i] << '\n';
                    argsOk = false;
                    break;
            }
        }
        ++i;

    }
    if (cxpsOptions.haveOptionsMissingConf()){
        std::cout << "*** --conf file requred to set cxps.conf values\n";
        argsOk = false;
    }

    if (!argsOk) {
        usage();
    }
}

/// \brief execute the requested command(s)
int main(int argc, char* argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    try {
        loggingPair_t warning(false, false);
        loggingPair_t xfer(false, false);
        loggingPair_t monitor(false, false);

        loggingLevelPair_t monitorLevel(false, MONITOR_LOG_LEVEL_2);

        bool removeMsgQueue = false;

        boost::filesystem::path progName(argv[0]);
        boost::filesystem::path confName(progName.parent_path());
        confName /= "cxps.conf";
        CxpsOptions cxpsOptions(confName.string().c_str());

        parseCmdline(argc, argv, warning, xfer, monitor, removeMsgQueue, monitorLevel, cxpsOptions);

#if !defined(__GNUC__) || (__GNUC__ >= 4)

        if (removeMsgQueue) {
            removeCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE);
            return 0;
        }

        if (warning.first) {
            if (warning.second) {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_WARNING_LOG_ON);
            } else {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_WARNING_LOG_OFF);
            }
        }

        if (xfer.first) {
            if (xfer.second) {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_XFER_LOG_ON);
            } else {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_XFER_LOG_OFF);
            }
        }

        if (monitor.first) {
            if (monitor.second) {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_MONITOR_LOG_ON);
            } else {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_MONITOR_LOG_OFF);
            }
        }

        if (monitorLevel.first) {
            if (MONITOR_LOG_LEVEL_1 == monitorLevel.second) {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_MONITOR_LOG_LEVEL_1);
            } else if (MONITOR_LOG_LEVEL_2 == monitorLevel.second) {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_MONITOR_LOG_LEVEL_2);
            } else if (MONITOR_LOG_LEVEL_3 == monitorLevel.second) {
                sendCxPsMsgQueue(CX_PROCESS_SERVER_MSG_QUEUE,
                                 CxPsMsgQueue::CXPS_MSG_MONITOR_LOG_LEVEL_3);
            }
        }
#else
    std::cout << "warning only updating cxps.conf is supported on this system" << std::endl;

#endif
    cxpsOptions.save();

    } catch (std::exception const& e) {
        std::cerr << e.what() << '\n';
        return -1;
    } catch (...) {
        std::cerr << "unknonw error\n";
        return -1;
    }

    return 0;
}
