
///
///  \file win/service.cpp
///
///  \brief implements windows service for cx process server
///

#include <windows.h>
#include <tchar.h>
#include <string>
#include <iostream>

#include "cxpslogger.h"
#include "serverctl.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#define SERVICENAME TEXT("cxprocessserver")

// wait on quit for an hour before checking handle count
#define WAIT_TIME 1000 * 60 * 60 

SERVICE_STATUS g_serviceStatus;

SERVICE_STATUS_HANDLE g_serviceStatusHandle;

HANDLE g_serviceStopEvent = NULL;

bool g_debugMode = false;

std::string g_installDir;
std::string g_confFile;
std::string g_action;

void restartService();
void start();
void serviceInstall();
void serviceUninstall();
void WINAPI serviceMain(DWORD argc, LPSTR* argv);
void WINAPI serviceCtrlHandler(DWORD ctrl);
void reportServiceStatus(DWORD currentState,  DWORD win32ExitCode, DWORD waitHint);

DWORD handleCount();

/// \brief callback to tell service that thread had an error that caused it to exit
void threadErrorCallback(std::string reason) ///< holds error text why thread exited early
{
    CXPS_LOG_ERROR(AT_LOC << "exiting on thread error callback: " << reason);
    if (g_debugMode) {
        GenerateConsoleCtrlEvent(0, 0);
    } else {
        SetEvent(g_serviceStopEvent);
    }
}

/// \brief debugging mode wait
BOOL WINAPI debuggingWait(DWORD ctrlType) ///< control event that caused this function to be called
{
    if (CTRL_C_EVENT == ctrlType) { 
        SetEvent(g_serviceStopEvent);           
    }
    return true;
}

/// \brief starts the servers in debug mode
///
/// bypasses the NT service initialization to make it easier to debug
/// the servers
void startDebuggingServers()
{
    try {        
        std::cout << "enter ctrl-c to quit" << std::endl;
        g_serviceStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        SetConsoleCtrlHandler(&debuggingWait, true); 
        startServers(g_confFile, &threadErrorCallback, g_installDir);              
        WaitForSingleObject(g_serviceStopEvent, INFINITE);                
        stopServers();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        try {
            // just in case the logger has an issue
            CXPS_LOG_ERROR(CATCH_LOC << e.what());
        } catch (std::exception const & e) {
            std::cerr << "Exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "unknonw exception\n";
        }
    } catch (...) {
        std::cerr << "unknown exception\n";
        try {
            // just in case the logger has an issue
            CXPS_LOG_ERROR(CATCH_LOC << "unknown exception");
        } catch (std::exception const & e) {
            std::cerr << "Exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "unknonw exception\n";
        }
    }
}

/// \brief starts the servers as an NT service
void start()
{
    SERVICE_TABLE_ENTRY DispatchTable[] = {
        { SERVICENAME, (LPSERVICE_MAIN_FUNCTION) serviceMain },
        { NULL, NULL }
    };

    StartServiceCtrlDispatcher(DispatchTable);
}

/// \brief installs the server with the service manager
void serviceInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR path[MAX_PATH];

    if( !GetModuleFileName(NULL, path, MAX_PATH)) {
        std::cerr << "Cannot install service: " << GetLastError() << '\n';
        return;
    }

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (NULL == schSCManager) {
        std::cerr << "OpenSCManager failed: " << GetLastError() << '\n';
        return;
    }

    std::string cmd;
    if ('"' != path[0]) {
        cmd += "\"";
    }
    cmd += path;
    if ('"' != path[0]) {
        cmd += "\"";
    }
        
    cmd += " -c ";
    if ('"' !=g_confFile[0]) {
        cmd += "\"";
    }
    cmd += g_confFile;
    if ('"' !=g_confFile[0]) {
        cmd += "\"";
    }
    if (!g_installDir.empty()) {
        cmd += " -i ";
        if ('"' != g_installDir[0]) {
            cmd += "\'";
        }
        cmd += g_installDir;
        if ('"' != g_installDir[0]) {
            cmd += "\'";
        }
    }
    
    schService = CreateService(schSCManager,
                               SERVICENAME,
                               SERVICENAME,
                               SERVICE_ALL_ACCESS,
                               SERVICE_WIN32_OWN_PROCESS,
                               SERVICE_AUTO_START,
                               SERVICE_ERROR_NORMAL,
                               cmd.c_str(),
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL);

    if (schService == NULL)  {
        std::cerr << "CreateService failed: " << GetLastError() << '\n';
        CloseServiceHandle(schSCManager);
        return;
    }


    SERVICE_DESCRIPTION ServiceDescription = { "InMage CX Process Server" };
    ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription);       

    SC_ACTION actions[3] = {
        { SC_ACTION_RESTART , 1000 }, 
        { SC_ACTION_RESTART , 1000 },        
        { SC_ACTION_NONE , 1000 }  
    };

    SERVICE_FAILURE_ACTIONS action;
    action.dwResetPeriod = 0;
    action.lpRebootMsg = NULL;
    action.lpCommand = NULL;
    action.cActions = 3;
    action.lpsaActions = (SC_ACTION*)&actions;

    if (!ChangeServiceConfig2(schService, SERVICE_CONFIG_FAILURE_ACTIONS, &action)) {
        std::cout << "set actions failed: " << GetLastError() << '\n';
    }

    SERVICE_FAILURE_ACTIONS_FLAG actionFlag;
    actionFlag.fFailureActionsOnNonCrashFailures = false;
    if (!ChangeServiceConfig2(schService, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &actionFlag)) {
        std::cout << "set action flag failed: " << GetLastError() << '\n';
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    std::cout << "service installed\n";
}

/// \ uninstalls the server from the service manager
void serviceUninstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (NULL == schSCManager) {
        std::cerr << "OpenSCManager failed: " << GetLastError() << '\n';
        return;
    }

    schService = OpenService(schSCManager, SERVICENAME, SERVICE_ALL_ACCESS);

    if (schService == NULL)  {
        std::cerr << "OpenService failed: " << GetLastError() << '\n';
        CloseServiceHandle(schSCManager);
        return;
    }

    SERVICE_STATUS serviceStatus;
    ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus);

    if (DeleteService(schService)) {
        std::cout << "service uninstalled\n";
    } else {
        std::cerr << "Deleteservice failed: " << GetLastError() << '\n';
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

/// \brief restarts the service
void restartService()
{
    std::stringstream cmd;
    cmd << "cmd.exe /C \"(net stop " << SERVICENAME << " && net start " << SERVICENAME << ") || net start " << SERVICENAME << '\"';
    
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process. 
    if( !CreateProcess(NULL, (LPSTR)cmd.str().c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {        
        return;
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}

/// \brief gets the process's current total handle count
///
/// \return
/// \li \c DWORD > 0 current total handle count, 0 failed to get handle count
unsigned long handleCount()
{
    DWORD count = 0;
#ifndef SCOUT_WIN2K_SUPPORT
    GetProcessHandleCount(GetCurrentProcess(), &count);
#endif
    return static_cast<unsigned long>(count);
}

/// \brief starts the servers
///
/// called when running as an NT service
void WINAPI serviceMain(DWORD argc, LPSTR* argv)
{
    g_serviceStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (g_serviceStopEvent == NULL) {
        return;
    }

    g_serviceStatusHandle = RegisterServiceCtrlHandler(SERVICENAME, serviceCtrlHandler);

    if (!g_serviceStatusHandle) {
        return;
    }

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwServiceSpecificExitCode = 0;

    reportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    try {        
        startServers(g_confFile, &threadErrorCallback, g_installDir);
        unsigned long recycleHandleCount = getRecycleHandleCount(g_confFile, g_installDir);
        reportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
        while (true) {
            if (handleCount() > recycleHandleCount) {
                restartService();
            }
            if (WAIT_TIMEOUT != WaitForSingleObject(g_serviceStopEvent, WAIT_TIME)) {
                // error or stop received
                break;
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        try {
            // just in case the logger has an issue
            CXPS_LOG_ERROR(CATCH_LOC << e.what());
        } catch (std::exception const & e) {
            std::cerr << "Exception logging : " << e.what() << '\n';
        } catch (...) {
            std::cerr << "Unknown exception logging\n";
        }
    } catch (...) {
        std::cerr << "unknown exception\n";        
        try {
            // just in case the logger has an issue
            CXPS_LOG_ERROR(CATCH_LOC << "unknown exception");
        } catch (std::exception const & e) {
            std::cerr << "Exception logging : " << e.what() << '\n';
        } catch (...) {
            std::cerr << "Unknown exception logging\n";            
        }
    }

    try {
        stopServers();
        reportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
    } catch (...) {
    }
}

/// \brief reports the NT service status 
void reportServiceStatus(DWORD currentState,    ///< current state of the service
                         DWORD win32ExitCode,   ///< error code to report when start/stopping
                         DWORD waitHint)        ///< estimated time needed to process control
{
    static DWORD checkPoint = 0;

    g_serviceStatus.dwCurrentState = currentState;
    g_serviceStatus.dwWin32ExitCode = win32ExitCode;
    g_serviceStatus.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING) {
        g_serviceStatus.dwControlsAccepted = 0;
    } else {
        g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
        g_serviceStatus.dwCheckPoint = 0;
    } else {
        g_serviceStatus.dwCheckPoint = ++checkPoint;
    }

    SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus );
}

/// \brief handles service control messages sent to the NT service
void WINAPI serviceCtrlHandler(DWORD ctrl) ///< control message
{
    switch (ctrl)  {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            reportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            SetEvent(g_serviceStopEvent);
            return;

        case SERVICE_CONTROL_INTERROGATE:
            break;

        default:
            break;
    }

    reportServiceStatus(g_serviceStatus.dwCurrentState, NO_ERROR, 0);
}

bool parseCmd(int argc,                ///< argument count from amin
              char* argv[],            ///< arguments from main
              bool& debugMode,         ///< set to indicate run in debug mode true: yes, false: no
              std::string& confFile,   ///< gets the full path to configuration file
              std::string& action,     ///< gets the action (either "install", "uninstall" or empty
              std::string& installDir, ///< gets the full path to the cxps install dir
              std::string& result)     ///< holds error text
{    
    bool argsOk = true;
    
    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            action = argv[i];
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

    if (confFile.empty() && (action.empty() || action != "uninstall")) {
        result += "missing -c <conf file>\n";
        argsOk = false;
    }

    return argsOk;
}

/// \brief main entry
///
/// usage: cxps -c conf_file [install | uninstall | -d] [-i cxps_install_dir]
/// \li -c conf_file: cxps configuration file to use
/// \li install: install cxps as an NT service
/// \li uninstall: uninstall cxps NT service
/// \li -d: run debug mode (does *not* run as NT service)
/// \li -i cxps_install_dir: cxps install dir (use if not set in conf file and cxps not installed in same dir as conf file
int __cdecl _tmain(int argc, TCHAR *argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    std::string result;

    if (!parseCmd(argc, argv, g_debugMode, g_confFile, g_action, g_installDir, result)) {
        std::cerr << "*** ERROR invalid command line: " << result << '\n'
                  << '\n'
                  << "usage cxps -c <conf file> [install | uninstall | -d] [-i <cxps install dir>]\n"
                  << '\n'
                  << "  -c <conf file>        (req): cxps configuration file to use. \n"
                  << "                               NOTE: not required for uninstall\n"
                  << "  install               (opt): install cxps as an NT service\n"
                  << "  uninstall             (opt): uninstall cxps NT service\n"
                  << "  -d                    (opt): run debug mode (does *not* run as NT service)\n"
                  << "  -i <cxps install dir> (opt): cxps install dir (use if not set in conf file\n"
                  << "                               and cxps not installed in same dir as conf file\n"
                  << std::endl;
        return 0;
    }

    if (g_debugMode) {
        startDebuggingServers();
    } else if (g_action.empty()) {
        start();
    } else if (g_action == "install") {
        serviceInstall();
    } else if (g_action == "uninstall") {
        serviceUninstall();
    } else {
        std::cerr << "unknown option: " << argv[1] << '\n';
    }

    return -1;
}
