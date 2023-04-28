// tmansvc.cpp : Implementation of WinMain


#include "stdafx.h"
#include "resource.h"
#include "tmansvc.h"
#include <atlstr.h>
#include "svconfig.h"
#include <list>
#include <sys/stat.h>
#include <io.h>
#include <cstdlib>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "terminateonheapcorruption.h"
#include <time.h>
#include <fstream>

#define SCHEDULER_SERVICE "INMAGE-AppScheduler"

/*
* Bug 4658 - Changed the maximum number of timeshot manager processes from
* 16 to 24.
*/
#define MAX_TMID 24
#define TMAN_INTERVALS "60,120,300,900,1800,3600"

// Monitor interval for perl process, default 15 mins
#define PROCESSS_MONITOR_INTERVAL 900


const std::string getCurrentDateTime() {
	time_t     now = time(0);
	struct tm  tstruct;
	char       bufTime[80];
	tstruct = *localtime(&now);
	strftime(bufTime, sizeof(bufTime), "%Y-%m-%d\t%X", &tstruct);

	return bufTime;
}

std::ofstream gLogStream;

void OutputDebugStr(LPCTSTR pString)
{
	gLogStream << "\n" + std::string(getCurrentDateTime()) + "\t" + std::string(pString);
	gLogStream.flush();
}

/*
* Allowed values for CX Type.
*
*   CX_TYPE = 1 is a configuration server
*   CX_TYPE = 2 is a process server
*   CX_TYPE = 3 is both a process and configuration server.
*/
#define CONFIGURATION_SERVER 1
#define PROCESS_SERVER 2
#define CS_PS_COMBINED 3

//tman service status
#define status_on "ON"
#define status_off "OFF"

/*#if (defined (_MSC_VER) && (_MSC_VER >= 1800))
#define OutputDebugStr OutputDebugString
#endif*/

#define INM_SERVICE_FAILURE_COUNT_RESET_PERIOD_SEC			3600
#define INM_SERVICE_FIRST_FAILURE_RESTART_DELAY_MSEC		300000
#define INM_SERVICE_SECOND_FAILURE_RESTART_DELAY_MSEC		600000

/*
Description: SetServiceFailureActions()

Sets the service recovery options as per the provided input parameters.
The service handle must be valid service handle and it should have SERVICE_CHANGE_CONFIG access.

Refer ChangeServiceConfig2() API documentation in msdn for more details.

Return code:
On success ERROR_SUCCESS will be returns, otherwise a win32 error code.
*/
DWORD SetServiceFailureActions(
	SC_HANDLE hService,
	DWORD resetPeriodSec,
	DWORD cActions,
	LPSC_ACTION lpActions,
	LPTSTR szRebootMsg,
	LPTSTR szCommand);

DWORD SetServiceFailureActionsRecomended(SC_HANDLE hService);
const std::string currentDateTime();
std::string getCSInstallationPath();
std::string readProgramDataEnvVariable();
void Trim(std::string& s, const std::string &trim_chars);
std::string getInstallationPathFromAppConf(const std::string &confFile);

using namespace std;

class CtmansvcModule : public CAtlServiceModuleT< CtmansvcModule, IDS_SERVICENAME >
{
	BOOL startProcess(char *cmdLine);
	bool stopAllProcesses();
	bool Monitor();
	static unsigned __stdcall MonitorThread(LPVOID pParam);
	DWORD StartMonitor();
	void StopMonitor();
	bool CtmansvcModule::QuitRequested(DWORD waitSeconds);

	//Modified from list<HANDLE> to map by BSR to fix 1741 issue
	map<std::string, HANDLE> m_hProcess;
	HANDLE m_MonitorHandle;
	unsigned m_MonitorThreadId;

public:
	DECLARE_LIBID(LIBID_tmansvcLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_TMANSVC, "{F2CE8BB1-5C3A-4D04-8182-99EA83D0FA9A}")
	HRESULT InitializeSecurity() throw()
	{
		// TODO : Call CoInitializeSecurity and provide the appropriate security settings for 
		// your service
		// Suggested - PKT Level Authentication, 
		// Impersonation Level of RPC_C_IMP_LEVEL_IDENTIFY 
		// and an appropiate Non NULL Security Descriptor.

		return S_OK;
	}
	std::string getPIDPath()
	{
		return PID_PATH;
	}

	CtmansvcModule()
	{
		inm_strcpy_s(m_szServiceName, ARRAYSIZE(m_szServiceName), "tmansvc");

		m_InstallationPath = getCSInstallationPath();

		BW_SHAPING_CMDLINE = std::string("perl \"") + m_InstallationPath + "\\home\\svsystems\\bin\\bpm.pl\"";

		VOLSYNC_CMDLINE = std::string("perl \"") + m_InstallationPath + "\\home\\svsystems\\bin\\volsync.pl\" volsync";

		RSYNC_CMDLINE = std::string("inmsync --daemon --config=\"") + m_InstallationPath + "\\home\\svsystems\\etc\\rsyncd.conf\" --no-detach";

		PID_FILE = m_InstallationPath + "\\home\\svsystems\\var\\tmansvc.pid";

		PID_PATH = m_InstallationPath + "\\home\\svsystems\\var";

		BIN_DIRECTORY = m_InstallationPath + "\\home\\svsystems\\bin";

		AMETHYST_CONF = m_InstallationPath + "\\home\\svsystems\\etc\\amethyst.conf";

		TMANAGERD = std::string("perl \"") + m_InstallationPath + "\\home\\svsystems\\bin\\eventmanager.pl\"";

		SYS_MONITOR = std::string("perl \"") + m_InstallationPath + "\\home\\svsystems\\bin\\systemmonitor.pl\"";

		RECONFIG_PASSWD_GROUP = "\"" + m_InstallationPath + "\\home\\svsystems\\bin\\recreate_passwd_group.bat\"";

		SHUTDOWNFILE = m_InstallationPath + "\\home\\svsystems\\.tmanshutdown";

		TMAN_FILE = m_InstallationPath + "\\home\\svsystems\\etc\\tman_services.conf"; //tman configuration files
	}


	HRESULT Run(int nShowCmd = SW_HIDE)
	{
		//char szCmdLine[512];
		char errMsg[512];
		HANDLE hPidFile = NULL;
		DWORD dwCurrentProcessId;
		DWORD dwBytesWritten = 0;//new DWORD;
		if (m_bService)
		{
			SetServiceStatus(SERVICE_RUNNING);
		}

		// change service Start type to AUTO_START
		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = ::OpenService(hSCM, m_szServiceName, SERVICE_CHANGE_CONFIG);
		::ChangeServiceConfig(hService, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_NO_CHANGE,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		::CloseServiceHandle(hService);
		::CloseServiceHandle(hSCM);

		// Read AMETHYST.conf file for MAX_TMID jobs
		SVConfig *config = SVConfig::GetInstance();
		config->Parse(AMETHYST_CONF);

		std::string szMaxJobs;
		// assign default value
		int nMaxJobs = MAX_TMID;
		config->GetValue("MAX_TMID", szMaxJobs);
		if (false == szMaxJobs.empty()) {
			nMaxJobs = atoi(szMaxJobs.c_str());
		}

		// Read AMETHYST.conf file for FTP_USER and create an entry for it under "c:\home"
		std::string szFTPUser;
		// assign default value
		szFTPUser;
		config->GetValue("FTP_USER", szFTPUser);

		//Cleaning events directory
		//deleteEventsFolder(EVENTS_DIRECTORY);

		//Added by BSR to fix 1741
		DeleteFile((char *)SHUTDOWNFILE.c_str());

		if (false == szFTPUser.empty()) {
			string cmd = std::string("md ") + m_InstallationPath + "\\home\\";
			cmd += szFTPUser.c_str();
			system(cmd.c_str());
		}

		// recreate passwd and group file
		system((char *)RECONFIG_PASSWD_GROUP.c_str());

		// BKNATHAN -- Read AMETHYST.conf and get the type of CX installation.
		std::string szCXType;
		// assign default value
		int nCXType = CS_PS_COMBINED;
		config->GetValue("CX_TYPE", szCXType);
		if (false == szCXType.empty()) {
			nCXType = atoi(szCXType.c_str());
		}

		//Config file path based on cx_type
		std::string tmanConfig;
		// assign default value
		tmanConfig = TMAN_FILE;

		// Create our PID file
		OutputDebugStr("Creating PID file...\n");
		CreateDirectory((char *)PID_PATH.c_str(), NULL);
		hPidFile = CreateFile(TEXT((char *)PID_FILE.c_str()),
			GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (INVALID_HANDLE_VALUE == hPidFile) {
			LPVOID errMsg;
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				0, // Default language
				(LPTSTR)&errMsg,
				0,
				NULL);
			OutputDebugStr((LPCTSTR)errMsg);
			LogEventEx(1, (LPCTSTR)errMsg, EVENTLOG_ERROR_TYPE);
			LocalFree(errMsg);
			// ERROR creating file. EXIT
			return (-1);
		}

		if (hPidFile)
		{
			BOOL result;

			// Write PID to file
			dwCurrentProcessId = GetCurrentProcessId();
			char* buffer = new char[20];
			inm_sprintf_s(buffer, 20, "%ul", dwCurrentProcessId);

			if (WriteFile(hPidFile, buffer, sizeof(buffer), &dwBytesWritten, NULL))
			{

				// Read tman conf file for service status
				SVConfig *configTman = SVConfig::GetInstance();
				configTman->Parse(tmanConfig);

				/* BKNATHAN -- Start the volsync child processes
				* and tmanager.pl monitor_ps thread only if
				* the installation type is PROCESS_SERVER or
				* CS_PS_COMBINED
				*/

				if ((nCXType == PROCESS_SERVER) ||
					(nCXType == CS_PS_COMBINED))
				{

					// BKNATHAN -- Start tmanager.pl monitor_ps process 
					// if the installation type is PS


					// Run tmanager.pl volsync_child processes
					std::string volsyncStatus;
					configTman->GetValue("volsync", volsyncStatus);
					if (strcmpi(volsyncStatus.c_str(), status_on) == 0)
					{
						result = startProcess((char *)VOLSYNC_CMDLINE.c_str());
						if (0 == result) {
							//ERROR starting volsync process. EXIT
							inm_sprintf_s(errMsg, ARRAYSIZE(errMsg), "FAILED TO EXECUTE COMMAND %s\n", VOLSYNC_CMDLINE);
							OutputDebugStr(errMsg);

							return (-1);
						}
					}

					// Start Bandwidth Shaping process
					std::string bpmStatus;
					configTman->GetValue("bpm", bpmStatus);
					if (strcmpi(bpmStatus.c_str(), status_on) == 0)
					{
						result = startProcess((char *)BW_SHAPING_CMDLINE.c_str());
						if (0 == result) {
							// ERROR starting gentrends process. EXIT
							inm_sprintf_s(errMsg, ARRAYSIZE(errMsg), "FAILED TO EXECUTE COMMAND %s\n", BW_SHAPING_CMDLINE);
							OutputDebugStr(errMsg);

							return (-1);
						}
					}

				}

				
				// Start TMANAGERD process
				result = startProcess((char *)TMANAGERD.c_str());
				if (0 == result)
				{
					// ERROR starting resync process. EXIT
					inm_sprintf_s(errMsg, ARRAYSIZE(errMsg), "FAILED TO EXECUTE COMMAND %s\n", TMANAGERD);
					OutputDebugStr(errMsg);
					return (-1);
				}
			}

			CloseHandle(hPidFile);
			if (ERROR_SUCCESS != StartMonitor())
			{
				// ERROR starting resync process. EXIT
				inm_sprintf_s(errMsg, ARRAYSIZE(errMsg), "FAILED TO START MONITOR THREAD\n");
				OutputDebugStr(errMsg);
				return (-1);
			}
		}
		return __super::Run();
	}

	void OnStop()
	{
		DeleteFile((char *)PID_FILE.c_str());

		// Stop monitor of perl process
		StopMonitor();
		stopAllProcesses();

		map<std::string, HANDLE>::iterator hIter;
		bool bCompleted = false;
		DWORD dwMaxTimeout = 120;
		DWORD dwTimeout = 10;
		DWORD dwCurrentTimeout = 0;
		while (dwCurrentTimeout < dwMaxTimeout && !bCompleted)
		{
			bCompleted = true; // assume all processess stopped
			dwCurrentTimeout += dwTimeout;
			Sleep(dwTimeout * 1000);
			for (hIter = m_hProcess.begin(); hIter != m_hProcess.end(); hIter++)
			{
				std::string cmdLine = hIter->first;
				if (cmdLine.find(VOLSYNC_CMDLINE) != string::npos)
				{
					DWORD dwExitCode;
					if (GetExitCodeProcess(hIter->second, &dwExitCode) &&
						dwExitCode == STILL_ACTIVE)
					{
						bCompleted = false;
					}
					else
					{
						if (0 != hIter->second)
						{
							CloseHandle(hIter->second);
							hIter->second = 0;
						}
					}
				}
			}
		}

		if (bCompleted == false)
			this->LogEventEx(0, "Volume sync children Still running. Logging it as Service Stop error. Please retry after some time");

		__super::OnStop();
	}

	inline HRESULT RegisterAppId(bool bService = false)
	{
		if (__super::RegisterAppId(bService) == S_OK)
		{
			if (!Install())
				return E_FAIL;
		}
		return S_OK;
	}

	BOOL Install()
	{
		SC_HANDLE hSCManager;
		SC_HANDLE hSchService;

		hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		hSchService = OpenService(hSCManager, "tmansvc", SERVICE_ALL_ACCESS);

		if (hSchService == NULL)
		{
			return false;
		}
		SERVICE_DESCRIPTION tmanDesc;
		tmanDesc.lpDescription = _T("InMage Volsync Thread Manager Service");

		if (!ChangeServiceConfig2(hSchService, SERVICE_CONFIG_DESCRIPTION, &tmanDesc))
			return false;

		DWORD dwErrorServiceFailureActions = SetServiceFailureActionsRecomended(hSchService);
		if (ERROR_SUCCESS != dwErrorServiceFailureActions)
			return false;

		return true;
	}

private:

	std::string m_InstallationPath; //installation path

	std::string  BW_SHAPING_CMDLINE;

	std::string  VOLSYNC_CMDLINE;

	std::string  RSYNC_CMDLINE;

	std::string  PID_FILE;

	std::string  PID_PATH;

	std::string  BIN_DIRECTORY;

	std::string  AMETHYST_CONF;

	std::string  TMANAGERD;

	std::string  SYS_MONITOR;

	std::string  RECONFIG_PASSWD_GROUP;

	std::string  SHUTDOWNFILE;

	std::string  TMAN_FILE;
};


BOOL
CtmansvcModule::startProcess(char *cmdLine)
{
	PROCESS_INFORMATION ProcessInfo;
	STARTUPINFO StartupInfo = { 0 };
	StartupInfo.cb = sizeof(StartupInfo);
	BOOL result = CreateProcess(NULL,
		cmdLine,
		NULL,
		NULL,
		FALSE,
		CREATE_DEFAULT_ERROR_MODE,
		NULL,
		(char *)BIN_DIRECTORY.c_str(),
		&StartupInfo,
		&ProcessInfo);
	if (0 == result) {
		// ERROR executing command. EXIT
		LPVOID errMsg;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			0, // Default language
			(LPTSTR)&errMsg,
			0,
			NULL);
		OutputDebugStr((LPCTSTR)errMsg);
		char* completeMessage;
		char* temp = " while trying to run the command: ";
		size_t completeMessageLength;
		INM_SAFE_ARITHMETIC(completeMessageLength = InmSafeInt<size_t>::Type(strlen((LPTSTR)errMsg)) + strlen(temp) + strlen(cmdLine) + 1, INMAGE_EX(strlen((LPTSTR)errMsg))(strlen(temp))(strlen(cmdLine)))
			completeMessage = new char[completeMessageLength];
		::memset(completeMessage, 0, completeMessageLength);

		inm_strcpy_s(completeMessage, completeMessageLength, (LPTSTR)errMsg);
		inm_strcat_s(completeMessage, completeMessageLength, temp);
		inm_strcat_s(completeMessage, completeMessageLength, cmdLine);

		OutputDebugStr(completeMessage);
		LogEventEx(0, completeMessage, EVENTLOG_ERROR_TYPE);

		stopAllProcesses();
		//startProcess(STOP_SCHED );
		DeleteFile((char *)PID_FILE.c_str());
		LocalFree(errMsg);
	}
	else {
		std::string commandLine;
		commandLine.append(cmdLine);
		m_hProcess[commandLine] = ProcessInfo.hProcess; //.push_back(ProcessInfo.hProcess);
		//m_hThread.push_back(ProcessInfo.hThread);	
	}
	return result;
}

bool
CtmansvcModule::stopAllProcesses()
{
	HANDLE shutdownFile = CreateFile((char *)SHUTDOWNFILE.c_str(),
		GENERIC_WRITE,
		0,
		0,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		0);
	CloseHandle(shutdownFile);

	// close handle of the process
	//Added by BSR to fix the issue#1741
	map<std::string, HANDLE>::iterator hIter;
	for (hIter = m_hProcess.begin(); hIter != m_hProcess.end(); hIter++)
	{
		std::string cmdLine = hIter->first;

		if (cmdLine.find(VOLSYNC_CMDLINE) == string::npos)
		{
			if (cmdLine.find(SYS_MONITOR) == string::npos){
				TerminateProcess(hIter->second, 0);
			}

			CloseHandle(hIter->second);
		}
	}
	return true;
	//End of fix for 1741	
}

bool CtmansvcModule::Monitor()
{
	short int counter = 0;
	map<std::string, HANDLE>::iterator hIter;
	std::string cmdLine;
	DWORD dwExitCode;
	char errMsg[512];


	// Read AMETHYST.conf file for MAX_TMID jobs
	SVConfig *config = SVConfig::GetInstance();
	config->Parse(AMETHYST_CONF);

	// assign default value
	int monitorInterval = PROCESSS_MONITOR_INTERVAL;
	std::string szMaxInterval;

	config->GetValue("PROCESS_MONITOR_INTERVAL", szMaxInterval);
	if (false == szMaxInterval.empty()) {
		monitorInterval = atoi(szMaxInterval.c_str());
	}

	do {
		for (hIter = m_hProcess.begin(); hIter != m_hProcess.end(); hIter++)
		{
			cmdLine = hIter->first;

			// Monitor only volsync, event manager and system monitor
			if (cmdLine.find(VOLSYNC_CMDLINE) != string::npos || cmdLine.find(TMANAGERD) != string::npos || cmdLine.find(SYS_MONITOR) != string::npos)
			{
				if (GetExitCodeProcess(hIter->second, &dwExitCode) && dwExitCode != STILL_ACTIVE){
					CString exitCode;
					string  failedCmd = std::string("FAILED TO EXECUTE COMMAND::") + cmdLine.c_str();
					exitCode.Format(", exit code %lu", dwExitCode);
					OutputDebugStr((char *)failedCmd.c_str() + exitCode);
					CloseHandle(hIter->second);
					startProcess((LPSTR)cmdLine.c_str());
				}
			}
		}

		// Wait for Quit request till the time interval
		if (QuitRequested(monitorInterval))	return true;
	} while (true);


	return true;
}

unsigned __stdcall CtmansvcModule::MonitorThread(LPVOID pParam)
{
	char errMsg[512];

	try {
		CtmansvcModule* tmnasvc = (CtmansvcModule*)pParam;

		tmnasvc->Monitor();
		return ERROR_SUCCESS;
	}
	catch (std::exception const & e) {
		inm_sprintf_s(errMsg, ARRAYSIZE(errMsg), "Failed CtmansvcModule::MonitorThread %s\n", e.what());
		OutputDebugStr(errMsg);
	}

	return ERROR_FUNCTION_FAILED;
}

DWORD CtmansvcModule::StartMonitor()
{
	char errMsg[512];
	DWORD result = ERROR_SUCCESS;
	m_MonitorHandle = (HANDLE)_beginthreadex(0, 0, CtmansvcModule::MonitorThread, (void*)this, 0, &m_MonitorThreadId);
	if (0 == m_MonitorHandle) {
		result = errno;

		inm_sprintf_s(errMsg, ARRAYSIZE(errMsg), "FAILED CtmansvcModule::Monitor _beginthreadex: \n");
		OutputDebugStr(errMsg);
	}
	return result;
}

void CtmansvcModule::StopMonitor()
{
	if (0 != m_MonitorThreadId) {
		PostThreadMessage(m_MonitorThreadId, WM_QUIT, 0, 0);
		WaitForSingleObject(m_MonitorHandle, INFINITE);// 60 * 1000); // give the thread 1 minute to shut
	}
}

bool CtmansvcModule::QuitRequested(DWORD waitSeconds)
{
	if (WAIT_OBJECT_0 == MsgWaitForMultipleObjects(0, NULL, FALSE, static_cast<unsigned>(waitSeconds * 1000), QS_ALLINPUT)) {
		MSG msg;
		if (0 == GetMessage(&msg, NULL, 0, 0)) {
			return true; // WM_QUIT
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return false;
}

CtmansvcModule _AtlModule;


//
extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
	LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	TerminateOnHeapCorruption();
	//calling init funtion of inmsafec library
	init_inm_safe_c_apis();
	gLogStream.open(_AtlModule.getPIDPath() + std::string("\\tmansvc.log"), std::ios::app);
	
	return _AtlModule.WinMain(nShowCmd);
}

/*
Description:
Sets the service recovery options as per the provided input parameters.
The service handle must be valid service handle and it should have SERVICE_CHANGE_CONFIG access.

Refer ChangeServiceConfig2() API documentation in msdn for more details.

Return code:
On success ERROR_SUCCESS will be returns, otherwise a win32 error code.
*/
DWORD SetServiceFailureActions(
	SC_HANDLE hService,
	DWORD resetPeriodSec,
	DWORD cActions,
	LPSC_ACTION lpActions,
	LPTSTR szRebootMsg,
	LPTSTR szCommand)
{
	DWORD dwRet = ERROR_SUCCESS;
	do {
		if (!hService || hService == INVALID_HANDLE_VALUE)
		{
			dwRet = ERROR_INVALID_HANDLE;
			break;
		}

		SERVICE_FAILURE_ACTIONS srv_failure_actions = { 0 };
		srv_failure_actions.dwResetPeriod = resetPeriodSec;
		srv_failure_actions.lpRebootMsg = szRebootMsg;
		srv_failure_actions.lpCommand = szCommand;
		srv_failure_actions.cActions = cActions;
		srv_failure_actions.lpsaActions = lpActions;

		if (!ChangeServiceConfig2(hService, SERVICE_CONFIG_FAILURE_ACTIONS, &srv_failure_actions))
			dwRet = GetLastError();

	} while (false);

	return dwRet;
}

DWORD SetServiceFailureActionsRecomended(SC_HANDLE hService)
{
	SC_ACTION scActions[3];

	//On first failure restart the service
	scActions[0].Delay = INM_SERVICE_FIRST_FAILURE_RESTART_DELAY_MSEC;
	scActions[0].Type = SC_ACTION_RESTART;

	//On second failure restart the service
	scActions[1].Delay = INM_SERVICE_SECOND_FAILURE_RESTART_DELAY_MSEC;
	scActions[1].Type = SC_ACTION_RESTART;

	//No-action on further failures
	scActions[2].Delay = 0;
	scActions[2].Type = SC_ACTION_NONE;

	//Set failure count reset period
	DWORD dwRestPeriodSec = INM_SERVICE_FAILURE_COUNT_RESET_PERIOD_SEC;

	return SetServiceFailureActions(
		hService,
		dwRestPeriodSec,
		sizeof(scActions) / sizeof(SC_ACTION),
		scActions,
		NULL,
		NULL
		);
}

std::string readProgramDataEnvVariable()
{
	const unsigned BUFSIZE = 4096;
	const std::string ENV_VAR = "ProgramData";

	// Allocate buffer to get env. variable
	char *pBuf = (char *)malloc(BUFSIZE);
	if (NULL == pBuf) {
		OutputDebugStr("Allocating memory for GetEnvironmentVariable for failed.\n");
		return std::string();
	}

	// Get ENV_VAR
	std::string value;
	DWORD dwRet = GetEnvironmentVariable(ENV_VAR.c_str(), pBuf, BUFSIZE);
	if (0 == dwRet) {
		// Failure
		if (ERROR_ENVVAR_NOT_FOUND == GetLastError())
			OutputDebugStr("Environment variable is not present.\n");
		else
			OutputDebugStr("GetEnvironmentVariable failed.\n");
	}
	else if (BUFSIZE < dwRet) {
		// Buffer not enough; reallocate
		char *tmpPtr = (char *)realloc(pBuf, dwRet);
		if (NULL == tmpPtr)
			OutputDebugStr("Reallocating memory for GetEnvironmentVariable for failed.\n");
		else {
			pBuf = tmpPtr;
			dwRet = GetEnvironmentVariable(ENV_VAR.c_str(), pBuf, BUFSIZE);
			if (dwRet)
				value = pBuf; // success
			else
				OutputDebugStr("GetEnvironmentVariable failed after reallocating buffer.\n");
		}
	}
	else // success
		value = pBuf;

	if (pBuf)
		free(pBuf);

	return value;
}

std::string getCSInstallationPath()
{
	std::string programDataPath = readProgramDataEnvVariable();
	if (programDataPath.empty()) {
		OutputDebugStr("ProgramData path is empty.\n");
		return std::string();
	}

	const char APP_CONF_SUFFIX_PATH[] = "\\Microsoft Azure Site Recovery\\Config\\App.conf";
	std::string app_conf_path = programDataPath + APP_CONF_SUFFIX_PATH;
	return getInstallationPathFromAppConf(app_conf_path);
}


std::string getInstallationPathFromAppConf(const std::string &confFile)
{
	fstream file(confFile.c_str());
	if (!file){
		OutputDebugStr("Failed to open appconf file.\n");
		return std::string();
	}
		

	std::string line, cspsInstallPath;

	while (getline(file, line))
	{
		if ('#' == line[0] || '[' == line[0] || line.find_first_not_of(' ') == line.npos)
		{
			continue;
		}
		else
		{
			std::string::size_type idx = line.find_first_of("=");
			if (std::string::npos != idx) {
				if (' ' == line[idx - 1]) {
					--idx;
				}
				if (line.substr(0, idx) == "INSTALLATION_PATH")
				{
					if (' ' == line[idx])
					{
						++idx;
					}
					cspsInstallPath = line.substr(idx + 1);
					Trim(cspsInstallPath, " \"");
				}
			}
		}
	}

	return cspsInstallPath;
}


void Trim(std::string& s, const std::string &trim_chars)
{
	size_t begIdx = s.find_first_not_of(trim_chars);
	if (begIdx == std::string::npos)
	{
		begIdx = 0;
	}
	size_t endIdx = s.find_last_not_of(trim_chars);
	if (endIdx == std::string::npos)
	{
		endIdx = s.size() - 1;
	}
	s = s.substr(begIdx, endIdx - begIdx + 1);
}