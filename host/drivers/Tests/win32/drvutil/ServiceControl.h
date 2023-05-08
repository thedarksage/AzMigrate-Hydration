#ifndef _SERVICE_CONTROL_H_
#define _SERVICE_CONTROL_H_

#define EVENTLOG_KEY        _T("SYSTEM\\CurrentControlSet\\Services\\Eventlog")
#define LOG_NAME            _T("System")
#define PATH_SEPARATOR      _T("\\")

DRSTATUS
SCInstallService(
    PTCHAR ServiceName, 
    PTCHAR DisplayName, 
    PTCHAR PathAndFileName, 
    PTCHAR GroupName, 
    DWORD *pdwTagId
);

DRSTATUS SCUninstallDriver(PTCHAR ServiceName);
DRSTATUS SCStartDriver(PTCHAR ServiceName);
DRSTATUS SCStopDriver(PTCHAR ServiceName);
bool AddEventLogEntry(
    TCHAR *DriverName,
    TCHAR *pszMsgDLL);  // path for message DLL
  

#endif