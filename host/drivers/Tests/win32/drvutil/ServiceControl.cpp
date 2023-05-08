#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include "ListEntry.h"
#include "drvstatus.h"
#include "InstallInVirVol.h"
#include "ServiceControl.h"

DRSTATUS
SCInstallService(
    PTCHAR ServiceName, 
    PTCHAR DisplayName, 
    PTCHAR PathAndFileName, 
    PTCHAR GroupName, 
    DWORD *pdwTagId
)
{
    /*
    TCHAR    *ServiceName = InstallDriverData->ServiceName;
    TCHAR    *DisplayName = SERVICE_INVIRVOL_DISPLAY_NAME;
    TCHAR    *PathAndFileName = InstallDriverData->PathAndFileName;
    TCHAR    *GroupName = NULL;
    DWORD    *pdwTagId = NULL;
    */

    SC_HANDLE    schService;
    SC_HANDLE    schSCManager;
    DWORD        dwWin32Error;

    schSCManager = OpenSCManager(
                        NULL,                    // local machine
                        NULL,                    // local database
                        SC_MANAGER_ALL_ACCESS    // access required
                        );
    if (!schSCManager)
    {
        dwWin32Error = GetLastError();
        return DRSTATUS_OPEN_SCMANAGER_FAILED;
    }

    schService = CreateService(
                    schSCManager,                        // hSCManager
                    ServiceName,                         // lpServiceName
                    DisplayName,                        // lpDisplayName
                    SERVICE_ALL_ACCESS,                    // dwDesiredAccess
                    SERVICE_KERNEL_DRIVER,                // dwServiceType
                    SERVICE_DEMAND_START,                // dwStartType
                    SERVICE_ERROR_NORMAL,                // dwErrorControl
                    PathAndFileName,                    // lpBinaryPathName
                    GroupName,                            // lpLoadOrderGroup
                    pdwTagId,                            // lpdwTagId
                    NULL,                               // lpDendencies
                    NULL,                                // lpServiceStartAccount
                    NULL);                                // lpPassword
    
    if (schService == NULL)
    {
        dwWin32Error = GetLastError();
        if (dwWin32Error != ERROR_SERVICE_EXISTS)
        {
            return DRSTATUS_CREATE_SERVICE_FAILED;
        }
    }

    if (schService)
    {
        CloseServiceHandle(schService);
    }

    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
    }

    _tprintf(_T("%s\n"),PathAndFileName);

    if (!AddEventLogEntry(ServiceName, PathAndFileName))
        _tprintf(_T("Failed to add eventlog keys for %s\n"), ServiceName);

    return DRSTATUS_SUCCESS;
}

DRSTATUS SCUninstallDriver(PTCHAR ServiceName)
{
    DRSTATUS Status = DRSTATUS_SUCCESS;
    SC_HANDLE       schService;
    SC_HANDLE       schSCManager;
    DWORD           dwWin32Error;
    BOOL            bSuccess;

    //PTCHAR ServiceName = UninstallDriverData->ServiceName;

    schSCManager = OpenSCManager(
                        NULL,                    // local machine
                        NULL,                    // local database
                        SC_MANAGER_ALL_ACCESS    // access required
                        );
    if (!schSCManager)
    {
        dwWin32Error = GetLastError();
        return DRSTATUS_OPEN_SCMANAGER_FAILED;
    }

    schService = OpenService(schSCManager, ServiceName, SERVICE_ALL_ACCESS);
    if(schService == NULL)
    {
        _tprintf(_T("Could not open service:%s Error:%x\n"), ServiceName, GetLastError());
        return DRSTATUS_CREATE_SERVICE_FAILED;
    }
    
    SERVICE_STATUS ServiceStatus;
    bSuccess = ControlService(schService, SERVICE_CONTROL_STOP, &ServiceStatus);
    
    if(!bSuccess)
    {
        _tprintf(_T("Could not stop service:%s Error:%x\n"), ServiceName, GetLastError());
    }

    bSuccess = DeleteService(schService);

    if(!bSuccess)
        _tprintf(_T("Could not remove service:%s Error:%x\n"), ServiceName, GetLastError());
    
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return Status;
}

DRSTATUS SCStartDriver(PTCHAR ServiceName)
{
    DRSTATUS Status = DRSTATUS_SUCCESS;
    SC_HANDLE       schService;
    SC_HANDLE       schSCManager;
    DWORD           dwWin32Error;
    BOOL            bStart;

    //PTCHAR ServiceName = StartDriverData->DriverName;

    schSCManager = OpenSCManager(
                        NULL,                    // local machine
                        NULL,                    // local database
                        SC_MANAGER_ALL_ACCESS    // access required
                        );
    if(!schSCManager)
    {
        dwWin32Error = GetLastError();
        return DRSTATUS_OPEN_SCMANAGER_FAILED;
    }

    schService = OpenService(schSCManager, ServiceName, SERVICE_ALL_ACCESS);
    if(schService == NULL)
    {
        _tprintf(_T("Could not open service:%s Error:%x\n"), ServiceName, GetLastError());
        return DRSTATUS_CREATE_SERVICE_FAILED;
    }
    
    bStart = StartService(schService, 0, NULL);
    
    if(!bStart)
        _tprintf(_T("Could not start driver Error:%x\n"),GetLastError());

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return Status;
}

DRSTATUS SCStopDriver(PTCHAR ServiceName)
{
    DRSTATUS Status = DRSTATUS_SUCCESS;
    SC_HANDLE    schService;
    SC_HANDLE    schSCManager;
    DWORD        dwWin32Error;
    BOOL         bStop;

    //_asm int 3
    //PTCHAR ServiceName = StopDriverData->DriverName;

    schSCManager = OpenSCManager(
                        NULL,                    // local machine
                        NULL,                    // local database
                        SC_MANAGER_ALL_ACCESS    // access required
                        );
    if (!schSCManager)
    {
        dwWin32Error = GetLastError();
        return DRSTATUS_OPEN_SCMANAGER_FAILED;
    }

    schService = OpenService(schSCManager, ServiceName, SERVICE_ALL_ACCESS);
    if(schService == NULL)
    {
        _tprintf(_T("Could not open service:%s Error:%x\n"), ServiceName, GetLastError());
        return DRSTATUS_CREATE_SERVICE_FAILED;
    }
    
    SERVICE_STATUS ServiceStatus;
    bStop = ControlService(schService, SERVICE_CONTROL_STOP, &ServiceStatus);

    if(!bStop)
    {
        _tprintf(_T("Could not stop service:%s Error:%x\n"), ServiceName, GetLastError());
    }
    
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return Status;
}

bool AddEventLogEntry(
    TCHAR *DriverName,
    TCHAR *pszMsgDLL)  // path for message DLL
{
    HKEY hKeyEvent; 
    DWORD dwData, dwDisp; 
    TCHAR    *EventKey;
    bool     bStatus = false;

    DWORD dwSize = (DWORD)_tcslen(EVENTLOG_KEY) + (DWORD)_tcslen(PATH_SEPARATOR) + 
             (DWORD)_tcslen(LOG_NAME) + (DWORD)_tcslen(PATH_SEPARATOR) + 
             (DWORD)_tcslen(DriverName) + 1 ;           //Terminating null character
    dwSize = dwSize * sizeof(TCHAR);

    EventKey = (TCHAR *)malloc(dwSize);
    if (!EventKey)
    {
		_tprintf(_T("Failed to allocate memory for EventKey %d"), GetLastError());
        return bStatus;
    }

	_tcscpy_s(EventKey, dwSize,EVENTLOG_KEY);
	_tcscat_s(EventKey, dwSize,PATH_SEPARATOR);
	_tcscat_s(EventKey, dwSize, LOG_NAME);
	_tcscat_s(EventKey, dwSize,PATH_SEPARATOR);
	_tcscat_s(EventKey, dwSize, DriverName);
 
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, EventKey, 
          0, NULL, REG_OPTION_NON_VOLATILE,
          KEY_WRITE, NULL, &hKeyEvent, &dwDisp)) 
    {
        _tprintf(_T("Could not create the registry key.")); 
        free(EventKey);
        return bStatus;
    }

   // Set the name of the message file. 
    if (RegSetValueEx(hKeyEvent,              // subkey handle 
            _T("EventMessageFile"),        // value name 
            0,                         // must be zero 
            REG_EXPAND_SZ,             // value type 
            (BYTE *) pszMsgDLL,        // pointer to value data 
            (DWORD) (_tcslen(pszMsgDLL) + 1) * sizeof(WCHAR))) // length of value data  + terminating null character
    {
        _tprintf(_T("Could not set the event message file.")); 
        goto Cleanup_And_Exit;
    }
 
   // Set the supported event types. 
 
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | 
        EVENTLOG_INFORMATION_TYPE; 
 
    if (RegSetValueEx(hKeyEvent,      // subkey handle 
            _T("TypesSupported"),  // value name 
            0,                 // must be zero 
            REG_DWORD,         // value type 
            (LPBYTE) &dwData,  // pointer to value data 
            sizeof(DWORD)))    // length of value data 
    {
        _tprintf(_T("Could not set the supported types."));
        goto Cleanup_And_Exit;
    } else {
        bStatus = true;
    }

Cleanup_And_Exit :
    RegCloseKey(hKeyEvent); 
    free(EventKey);
    return bStatus;
}
