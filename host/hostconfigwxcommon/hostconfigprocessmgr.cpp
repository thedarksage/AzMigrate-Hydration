#include "stdafx.h"
#include "hostconfigprocessmgr.h"
#include "extendedlengthpath.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "setpermissions.h"
#include "securityutils.h"
#include "genpassphrase.h"

#include <fstream>

#include "ace/config-win32.h"


#include "ace/Process.h"
#include "ace/Process_Manager.h"
#include <ace/OS_NS_sys_wait.h>

#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>



#define BUFSIZE 4096
HANDLE g_hReadChldStdOut = NULL;
HANDLE g_hWriteChldStdOut = NULL;
HANDLE g_hReadChidStdErr = NULL;
HANDLE g_hWriteChldStdErr = NULL;

int GetExitCodeProcSmart(HANDLE hProcess, DWORD* pdwOutExitCode)
{
	//Get exit code for the process
	//'hProcess' = process handle
	//'pdwOutExitCode' = if not NULL, receives the exit code (valid only if returned 1)
	//RETURN:
	//= 1 if success, exit code is returned in 'pdwOutExitCode'
	//= 0 if process has not exited yet
	//= -1 if error, check GetLastError() for more info
	int nRes = -1;

	DWORD dwExitCode = 0;
	if (::GetExitCodeProcess(hProcess, &dwExitCode))
	{
		if (dwExitCode != STILL_ACTIVE)
		{
			nRes = 1;
		}
		else
		{
			//Check if process is still alive
			DWORD dwR = ::WaitForSingleObject(hProcess, 0);
			if (dwR == WAIT_OBJECT_0)
			{
				nRes = 1;
			}
			else if (dwR == WAIT_TIMEOUT)
			{
				nRes = 0;
			}
			else
			{
				//Error
				nRes = -1;
			}
		}
	}

	if (pdwOutExitCode)
		*pdwOutExitCode = dwExitCode;

	return nRes;
}
bool CreateChProc(PROCESS_INFORMATION&, const std::string& cmdline, DWORD& exitcode);
bool ReadPipe(std::string&, std::string&);

bool execProcUsingPipe(const std::string& cmdline, std::string& err_msg, const bool isOutputRequire, std::string& out)
{
	bool rv = TRUE;
	SECURITY_ATTRIBUTES attr;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	attr.bInheritHandle = TRUE;
	attr.lpSecurityDescriptor = NULL;
	if (!CreatePipe(&g_hReadChidStdErr, &g_hWriteChldStdErr, &attr, 0)) {
		err_msg = "Failed to CreatePipe for child process's STDERR, error: " + boost::lexical_cast<std::string>(::GetLastError());
		DebugPrintf(SV_LOG_ERROR, "%s \n", err_msg.c_str());
		return false;
	}
	if (!SetHandleInformation(g_hReadChidStdErr, HANDLE_FLAG_INHERIT, 0)){
		err_msg = "Failed to SetHandleInformation for child process's  STDERR, error: " + boost::lexical_cast<std::string>(::GetLastError());
		DebugPrintf(SV_LOG_ERROR, "%s \n", err_msg.c_str());
		return false;
	}
	if (!CreatePipe(&g_hReadChldStdOut, &g_hWriteChldStdOut, &attr, 0)) {
		err_msg = "Failed to CreatePipe for child process's  STDOUT, error: " + boost::lexical_cast<std::string>(::GetLastError());
		DebugPrintf(SV_LOG_ERROR, "%s \n", err_msg.c_str());
		return false;
	}
	if (!SetHandleInformation(g_hReadChldStdOut, HANDLE_FLAG_INHERIT, 0)){
		err_msg = "Failed to SetHandleInformation for child process's  STDOUT, error: " + boost::lexical_cast<std::string>(::GetLastError());
		DebugPrintf(SV_LOG_ERROR, "%s \n", err_msg.c_str());
		return false;
	}
	PROCESS_INFORMATION pInfo;
	DWORD dwExitCode = 9999;
	if (!CreateChProc(pInfo, cmdline, dwExitCode))
	{
		err_msg = "Failed CreateChildProcess error: " + boost::lexical_cast<std::string>(::GetLastError());
		DebugPrintf(SV_LOG_ERROR, "%s \n", err_msg.c_str());
		return false;
	}
	std::string output = "";

	if (!ReadPipe(output, err_msg))
	{
		rv = false;
	}
	else
	{
		if (!output.empty())
		{
			if (!isOutputRequire)
			{
				err_msg = output;
				DebugPrintf(SV_LOG_ERROR, "Got Error from child process : %s\n", err_msg.c_str());
				rv = false;
			}
			else
			{
				out = output;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Error return from child process is empty. \n");
		}
	}

	if (isOutputRequire && dwExitCode != 0)
	{
		rv = false;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", __FUNCTION__);
	return rv;
}
bool CreateChProc(PROCESS_INFORMATION& pInfo, const std::string& cmdline, DWORD& exitcode){
	STARTUPINFO sInfo;
	bool bSuccess = FALSE;

	ZeroMemory(&pInfo, sizeof(PROCESS_INFORMATION));


	ZeroMemory(&sInfo, sizeof(STARTUPINFO));
	sInfo.cb = sizeof(STARTUPINFO);
	sInfo.hStdError = g_hWriteChldStdErr;
	sInfo.hStdOutput = g_hWriteChldStdOut;
	sInfo.dwFlags |= STARTF_USESTDHANDLES;

	bSuccess = CreateProcess(NULL,
		(LPSTR)cmdline.c_str(),
		NULL,
		NULL,
		TRUE,
		CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
		NULL,
		NULL,
		&sInfo,
		&pInfo);
	// Wait until child process exits.
	WaitForSingleObject(pInfo.hProcess, INFINITE);

	DWORD dwExitCode = 9999;

	int processExitCode = GetExitCodeProcSmart(pInfo.hProcess, &dwExitCode);

	std::stringstream errmsg;
	//= -1 if error, check GetLastError() for more info
	//= 0 if process has not exited yet
	//= 1 if success, exit code is returned in 'pdwOutExitCode'
	if (processExitCode == -1)
	{
		errmsg << "Failed to get process exit code, Error: " << GetLastError();
		DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
	}
	else if (processExitCode == 0)
	{
		errmsg << "Failed to get process exit code, after the wait time out." << WAIT_TIMEOUT;
		DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
	}
	else
	{
		std::stringstream msg;
		msg << "Process successfully returned exit code " << dwExitCode;
		DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
		exitcode = dwExitCode;
	}

	// Close process and thread handles. 
	CloseHandle(pInfo.hProcess);
	CloseHandle(pInfo.hThread);

	CloseHandle(g_hWriteChldStdErr);
	CloseHandle(g_hWriteChldStdOut);

	g_hWriteChldStdOut = NULL;
	g_hWriteChldStdErr = NULL;

	return bSuccess;
}
bool ReadPipe(std::string& out, std::string& err_msg) {
	bool rv = TRUE;
	DWORD dwRead;
	CHAR chBuf[BUFSIZE];
	bool bSuccess = FALSE;
	while (true) {
		if (!(bSuccess = ::ReadFile(g_hReadChldStdOut, chBuf, BUFSIZE, &dwRead, NULL)))
		{

			DWORD error = ::GetLastError();
			if (error != ERROR_BROKEN_PIPE)
			{
				rv = false;
				err_msg = " ReadFile using stdout Pipe Failed , Error: " + boost::lexical_cast<std::string>(error);
				DebugPrintf(SV_LOG_ERROR, "%s\n", err_msg.c_str());
			}

			break;
		}
		if (dwRead == 0)
		{
			DebugPrintf(SV_LOG_ERROR, "out dwRead == 0\n");
			break;
		}


		std::string data(chBuf, dwRead);
		out += data;
	}
	dwRead = 0;
	if (rv)
	{
		while (true) {
			if (!(bSuccess = ::ReadFile(g_hReadChidStdErr, chBuf, BUFSIZE, &dwRead, NULL)))
			{
				DWORD error = ::GetLastError();
				if (error != ERROR_BROKEN_PIPE)
				{
					rv = false;
					err_msg = " ReadFile using stderr Pipe Failed , Error: " + boost::lexical_cast<std::string>(error);
					DebugPrintf(SV_LOG_ERROR, "%s\n", err_msg.c_str());
				}

				break;
			}
			if (dwRead == 0)
			{
				DebugPrintf(SV_LOG_ERROR, "err dwRead == 0\n");
				break;
			}

			std::string data(chBuf, dwRead);
			out += data;

		}
	}


	CloseHandle(g_hReadChldStdOut);
	CloseHandle(g_hReadChidStdErr);

	g_hReadChldStdOut = NULL;
	g_hReadChidStdErr = NULL;

	return rv;
}