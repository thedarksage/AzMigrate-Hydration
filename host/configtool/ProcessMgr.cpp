#include "ProcessMgr.h"

int GetExitCodeProcessSmart(HANDLE hProcess, DWORD* pdwOutExitCode)
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
bool ExecuteProc(const std::string& cmd, DWORD& exitcode, std::stringstream& errmsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    bool rv = TRUE;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));


    // Start the child process. 
    if (!CreateProcess(NULL,   // No module name (use command line)
        (LPSTR)cmd.c_str(),    // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
		CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        )
    {
        errmsg << "CreateProcess failed, Error: " << GetLastError();
        DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
        return false;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD dwExitCode = 9999;

    int processExitCode = GetExitCodeProcessSmart(pi.hProcess, &dwExitCode);
    //= -1 if error, check GetLastError() for more info
    //= 0 if process has not exited yet
    //= 1 if success, exit code is returned in 'pdwOutExitCode'
    if (processExitCode == -1)
    {
        errmsg << "Failed to get process exit code, Error: " << GetLastError();
        DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
        rv = false;
    }
    else if (processExitCode == 0)
    {
        errmsg << "Failed to get process exit code, after the wait time out." << WAIT_TIMEOUT;
        DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
        rv = false;
    }
    else
    {
        std::stringstream msg;
        msg << "Process " << "successfully returned exit code " << dwExitCode;
        DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
        exitcode = dwExitCode;
    }

    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}

