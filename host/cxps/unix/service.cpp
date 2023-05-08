
///
///  \file unix/service.cpp
///
/// \brief implements unix daemon for cx process server (cxpsctl manages the daemon)
///
/// \example cxpsctl
///

#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <cerrno>
#include <ctime>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

// #include "cxpslogger.h"
#include "serverctl.h"

/// \brief sets process/threads and number files limits
int setLimits()
{
    // NOTE: errors are ignored as only root can change these limits
    // and in general cxps is started as root. when developers run
    // it these may fail, but for the most part do not care.
    // if it matters can always switch to root before running
    struct rlimit rLimit;

    // TODO: RLIMIT_NPROC not supported on all platforms, should we worry about this or just remove it all together
    // ulimited number of processes this includes threads
    //rLimit.rlim_cur = RLIM_INFINITY;
    //rLimit.rlim_max = RLIM_INFINITY;       
    //setrlimit(RLIMIT_NPROC, &rLimit);

    // open files includes sockets (basically all file descriptos (fd))
    // can not set open files to unlimited, also you may not actually be
    // able to open the number requested as there is a system wide upper limit
    rLimit.rlim_cur = 65536;
    rLimit.rlim_max = 65536;
    return setrlimit(RLIMIT_NOFILE, &rLimit);
}

/// \brief initializes the service as a daemon.
///
/// follows the apporach used by the daemon_init function in
/// "UNIX Network Programming Network APIs: Sockets and XTI"
/// by W. Richard Stevens
int initService()
{
    pid_t pid;

    // fisrt fork is done to put the child process
    // into the background (if it was run from a shell)
    // also gives the child its own process id guaranteeing
    // that the child is not a process group leader
    if (0 != fork()) {
        exit(0); // exit parent
    }

    // make child session leader
    setsid();

    // mask SIGHUP because when the session leader
    // (the first child) terminates, all processes
    // in the session (which will be the second child)
    // are sent the SIGHUP.
    signal(SIGHUP, SIG_IGN);

    // second fork is done to guarantee that the daemon
    // cannot automatically aquire a controlling terminal
    if (0 != fork()) {
        exit(0); // exit first child
    }

    // MAYBE: use a different dir
    chdir("/");

    // clear file creation mask so that it doesn't
    // affect permission bits in the inherited
    // file mode creation flag
    umask(0);

    // change the standard file handles to /dev/null
    // so if there are any uses of them, it won't
    // cause a crash
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);

    return 0;
}

/// \brief callback to tell service that thread had an error that caused it to exit
void threadErrorCallback(std::string reason) ///< holds error text why thread exited early
{
    //CXPS_LOG_ERROR(AT_LOC << "exiting on thread error callback: " << reason);
    kill(0, SIGINT);
}

/// \brief wait for quit
///
/// waits on SIGINT signal to quit
///
/// \note if -d was used, you can use ctrl-c
/// to quit
void waitForQuit(sigset_t& signalSet,  ///< set of signals to wait on
                 bool debug)           ///< indicates if running is a debug mode true: yes, false: no
{
    if (debug) {
        std::cout << "to quit: enter ctrl-c or issue 'kill -s SIGINT " << getpid() << "' from another shell\n" << std::endl;
    }

    int sigNumber;
    while (true) {
        sigwait(&signalSet, &sigNumber);
        if (SIGINT == sigNumber || SIGTERM == sigNumber) {
            break;
        }
    }
}

/// \brief parse linux/unix command line
bool parseCmd(int argc,                ///< argument count from amin
              char* argv[],            ///< arguments from main
              bool& debugMode,         ///< set to indicate run in debug mode true: yes, false: no
              std::string& confFile,   ///< gets the full path to configuration file
              std::string& installDir, ///< gets the full path to cxps install dir
              std::string& result)     ///< holds error text
{
    bool argsOk = true;

    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            result = "invalid arg ";
            result += argv[i];
            result += "\n";
        } else {
            switch (argv[i][1]) {
                case 'c':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        result += " missing value for -c\n";
                        argsOk = false;
                        break;
                    }

                    confFile = argv[i];
                    break;

                case 'd':
                    debugMode = true;
                    break;

                case 'i':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        result += " missing value for -i\n";
                        argsOk = false;
                        break;
                    }

                    installDir = argv[i];
                    break;

                default:
                    result += " invalid arg: ";
                    result += argv[i];
                    result += '\n';
                    argsOk = false;
                    break;
            }
        }
        ++i;
    }

    if (confFile.empty()) {
        result += "missing -c <conf file>\n";
        argsOk = false;
    }

    return argsOk;
}


/// \brief main entry
///
/// usage: cxps -c conf_file [-d] [-i cxps_install_dir]
/// \li -c conf_file: cxps configuration file to use
/// \li -d: run debug mode (does *not* run as NT service)
/// \li -i cxps_install_dir: cxps install dir (use if not set in conf file and cxps not installed in same dir as conf file
int main(int argc, char* argv[])
{
    bool debugMode = false;

    std::string installDir;
    std::string confFile;
    std::string result;

    if (!parseCmd(argc, argv, debugMode, confFile, installDir, result)) {
        std::cerr << "*** ERROR invalid command line: " << result << '\n'
                  << '\n'
                  << "usage cxps -c <conf file> [-d] [-i <cxps install dir>]\n"
                  << '\n'
                  << "  -c <conf file>        (req): cxps configuration file to use\n"
                  << "  -d                    (opt): run debug mode (does *not* run as daemon)\n"
                  << "  -i <cxps install dir> (opt): cxps install dir (use if not set in conf file\n"
                  << "                               and cxps not installed in same dir as conf file\n"
                  << std::endl;        
        return -1;
    }

    sigset_t signalSet;

    sigemptyset(&signalSet);

    if (!debugMode) {
        if (0 != initService()) {
            return -1;
        }
    }

    setLimits();
    
    // mask signals of interest. since all threads
    // inherit the signal mask it is ok to only do it here
    sigaddset(&signalSet, SIGINT);
    sigaddset(&signalSet, SIGTERM);
    if (0 != pthread_sigmask(SIG_BLOCK, &signalSet, NULL)) {
        return -1;
    }

    try {
        startServers(confFile, &threadErrorCallback, installDir);
        waitForQuit(signalSet, debugMode);
        stopServers();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        // CXPS_LOG_ERROR(CATCH_LOC << e.what());
    }

    return 0;
}
