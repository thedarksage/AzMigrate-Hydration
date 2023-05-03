#include "InsCDskFl.h"
#include <stdio.h>

#define CLUSDISK_DRIVER_NAME    _T("ClusDisk")
#define SERVICES_KEY        _T("SYSTEM\\CurrentControlSet\\Services")
#define EVENTLOG_KEY        _T("SYSTEM\\CurrentControlSet\\Services\\Eventlog")
#define GROUPORDERLIST_KEY    _T("SYSTEM\\CurrentControlSet\\Control\\GroupOrderList")
#define PATH_SEPARATOR        _T("\\")
#define GROUP_VALUE_NAME    _T("Filter")
#define TAG_VALUE_NAME        _T("Tag")
#define LOG_NAME            _T("System")
#define SERVICE_IMAGE_PATH_VALUE_NAME   _T("ImagePath")
#define SERVICE_START_VALUE_NAME        _T("Start")
#define SERVICE_TYPE_VALUE_NAME         _T("Type")
#define SERVICE_GROUP_VALUE_NAME        _T("Group")
#define SERVICE_ERRORCONTROL_VALUE_NAME _T("ErrorControl")
#define SERVICE_DISPLAYNAME_VALUE_NAME  _T("DisplayName")
#define SYSTEMROOT_PREFIX               _T("%SYSTEMROOT%\\")
#define TYPES_SUPPORTED     7

DWORD   dwWin32Error = 0;

TCHAR *ErrorDescriptions[CDSK_INSTALL_MAX_ERROR_VALUE + 1] = {
    _T("Success"),  // 0
    _T("ExpandEnvironmentStrings failed"),  // 1
    _T("malloc failed"),    // 2
    _T("ExpandEnvironmentStrings retry limit reached"), // 3
    _T("Failed to open file"),  // 4
    _T("OpenSCManager failed"), // 5
    _T("Driver does not exist"),    // 6
    _T("OpenService failed"),   // 7
    _T("QueryServiceStatus failed"),    // 8
    _T("Driver status is stopped"),     // 9
    _T("Driver status is running"),     // 10
    _T("RegOpenKeyEx failed"),          // 11
    _T("RegQueryValueEx failed"),       // 12
    _T("Failed to get tag of ClusDisk driver"), // 13
    _T("Number of tags and size mismatch"), // 14
    _T("ClusDisk tag not found in group"),      // 15
    _T("RegSetValueEx failed"),             // 16
    _T("CreateService failed"),             // 17
    _T("RegDeleteKey failed"),              // 18
    _T("ClusDisk driver is not installed"), // 19
    _T("Failed to create ImagePath value"), // 20
    _T("Failed to create Group value"),     // 21
    _T("Invalid Error code")    // CDSK_INSTALL_MAX_ERROR_VALUE
};

CDSK_INSTALL_STATUS
FileExists(TCHAR *Filepath);

CDSK_INSTALL_STATUS
GetCurrentDriverInstallStatus(
    TCHAR    *DriverName
    );

CDSK_INSTALL_STATUS
GetTagIDOfDriver(
    TCHAR *DriverName, 
    DWORD *pdwTagID
    );

CDSK_INSTALL_STATUS
InsertTagIdForNewDriver(DWORD dwTagId);

CDSK_INSTALL_STATUS
GetTagIdForNewDriver(DWORD &dwTagID, bool &bNewTag);

CDSK_INSTALL_STATUS
AddTagIDToService(
    TCHAR *DriverName,
    DWORD dwTagID
    );

CDSK_INSTALL_STATUS
InstallDriver(
    TCHAR    *DriverName,
    TCHAR    *DisplayName,
    TCHAR    *PathAndFileName,
    TCHAR    *GroupName,
    DWORD    *pdwTagId
    );

void
AddEventLogEntry(
    TCHAR *DriverName,
    TCHAR *pszMsgDLL,
    DWORD  dwNum
    );

CDSK_INSTALL_STATUS
RegDelNode (HKEY hKeyRoot, LPTSTR lpSubKey);

CDSK_INSTALL_STATUS
GetCurrentDriverInstallStatus(
    TCHAR    *DriverName
    )
{
    SC_HANDLE    schService;
    SC_HANDLE    schSCmanager;
    CDSK_INSTALL_STATUS    eInstallStatus;
    SERVICE_STATUS    Status;

    eInstallStatus = CDSK_INSTALL_SUCCESS;
    schSCmanager = OpenSCManager(
                        NULL,                    // local machine
                        NULL,                    // local database
                        SC_MANAGER_ALL_ACCESS    // access required
                        );
    if (!schSCmanager)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_OPEN_SCMANAGER_FAILED;
    }

    schService = OpenService(
                        schSCmanager,
                        DriverName,
                        SC_MANAGER_ALL_ACCESS);
    if (schService) {
        if (QueryServiceStatus(schService, &Status)) {
            if ((Status.dwCurrentState == SERVICE_STOPPED) ||
                (Status.dwCurrentState == SERVICE_STOP_PENDING))
            {
                eInstallStatus = CDSK_INSTALL_DRIVER_STATUS_IS_STOPPED;
            } else {
                eInstallStatus = CDSK_INSTALL_DRIVER_STATUS_IS_RUNNING;
            }
        } else {
            dwWin32Error = GetLastError();
            if (ERROR_FILE_NOT_FOUND == dwWin32Error) {
                eInstallStatus = CDSK_INSTALL_CLUSDISK_NOT_INSTALLED;
            } else {
                eInstallStatus = CDSK_INSTALL_QUERY_SERVICE_STATUS_FAILED;
            }
        }
    } else {
        dwWin32Error = GetLastError();
        if (dwWin32Error == ERROR_SERVICE_DOES_NOT_EXIST) {
            eInstallStatus = CDSK_INSTALL_DRIVER_DOES_NOT_EXIST;
        } else {            
            eInstallStatus = CDSK_INSTALL_OPEN_SERVICE_FAILED;
        }
    }

    if (schService)
    {
        CloseServiceHandle(schService);
    }

    if (schSCmanager)
    {
        CloseServiceHandle(schSCmanager);
    }

    return eInstallStatus;
}


#define RETRY_LIMIT 5

CDSK_INSTALL_STATUS
FileExists(TCHAR *Filepath)
{
    DWORD        dwResultSize;
    HANDLE        hFile;
    TCHAR        *ExpandedFilepath = NULL;
    DWORD        dwBufferSize;
    ULONG       ulRetryCount = 0;
    BOOL        bDone = TRUE;


    if ((dwBufferSize = ExpandEnvironmentStrings(Filepath, NULL, 0)) == 0) {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_EXPAND_ENV_STRING_FAILED;
    }

    do {
        ulRetryCount++;

        ExpandedFilepath = (TCHAR *)malloc(dwBufferSize * sizeof(TCHAR));
        if (!ExpandedFilepath ) {
            dwWin32Error = GetLastError();
            return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
        }

        dwResultSize = ExpandEnvironmentStrings(Filepath, ExpandedFilepath, dwBufferSize);
        if (dwResultSize == 0) {
            dwWin32Error = GetLastError();
            return CDSK_INSTALL_EXPAND_ENV_STRING_FAILED;
        }

        if (dwResultSize > dwBufferSize) {
            free(ExpandedFilepath);
            dwBufferSize = dwResultSize;
            ExpandedFilepath = NULL;
            bDone = FALSE;
        }
    } while ((FALSE == bDone) && (ulRetryCount < RETRY_LIMIT));

    if (ulRetryCount >= RETRY_LIMIT)
        return CDSK_INSTALL_EXPAND_ENV_STRING_RETRY_LIMIT_REACHED;

    hFile = CreateFile(    ExpandedFilepath,            // FileName
                        GENERIC_READ,                // dwDesiredAccess
                        0,                            // dwShareMode
                        NULL,                        // lpSecurityAttributes
                        OPEN_EXISTING,                // dwCreationDisposition
                        FILE_ATTRIBUTE_NORMAL,        // dwFlagsAndAttributes
                        NULL);                        // hTemplateFile

    if (hFile == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("CreateFile failed for file \"%s\"."), ExpandedFilepath);
        dwWin32Error = GetLastError();
        free(ExpandedFilepath);
        return CDSK_INSTALL_OPEN_FILE_FAILED;
    }

    free(ExpandedFilepath);
    CloseHandle(hFile);
    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS
GetTagIDOfDriver(
    TCHAR *DriverName, 
    DWORD *pdwTagID
    )
{
    DWORD    dwError, dwSize, dwValueType, dwTagID;
    HKEY    hService;
    TCHAR    *ServiceKey;

    *pdwTagID = 0;

    dwSize = (DWORD)_tcslen(SERVICES_KEY) + (DWORD)_tcslen(PATH_SEPARATOR) +
             (DWORD)_tcslen(DriverName) + 1;
    dwSize = dwSize * sizeof(TCHAR);

    ServiceKey = (TCHAR *)malloc(dwSize);
    if (!ServiceKey) {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

	_tcscpy_s(ServiceKey, dwSize, SERVICES_KEY);
	_tcscat_s(ServiceKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(ServiceKey, dwSize, DriverName);

    dwError = RegOpenKeyEx(    HKEY_LOCAL_MACHINE,        //hKey
                            ServiceKey,                //lpSubKey
                            0,                        //ulOptions - Reserved
                            KEY_QUERY_VALUE,        //samDesired
                            &hService);

    if (dwError != ERROR_SUCCESS) {
        dwWin32Error = dwError;
        free(ServiceKey);
        return CDSK_INSTALL_REG_OPEN_KEY_EX_FAILED;
    }

    free(ServiceKey);
    ServiceKey = NULL;

    dwSize = sizeof(DWORD);

    dwError = RegQueryValueEx(    hService,                // hKey
                                TAG_VALUE_NAME,            // lpValueName
                                NULL,                    // lpReserved
                                &dwValueType,            // lpType
                                (LPBYTE)&dwTagID,                // lpData
                                &dwSize);
    if (dwError != ERROR_SUCCESS) {
        dwWin32Error = dwError;
        RegCloseKey(hService);
        return CDSK_INSTALL_REG_QUERY_VALUE_EX_FAILED;
    }

    *pdwTagID = dwTagID;

    RegCloseKey(hService);
    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS
InsertTagIdForNewDriver(DWORD dwTagId)
{
    DWORD    i, dwError, dwSize, dwValueType, dwNumTags, dwTargetSize;
    HKEY    hGroupOrderList;
    DWORD    dwClusdiskTagId = 0;
    DWORD    dwNetBtTagId = 0;
    PDWORD    pSourceBinaryData, pTargetBinaryData, pSource, pTarget;
    BOOL    bClusdiskTagFound;

    // Query the Registry and get the Tag value for Tcpip.
    if (CDSK_INSTALL_SUCCESS != GetTagIDOfDriver(CLUSDISK_DRIVER_NAME, &dwClusdiskTagId))
    {
        return CDSK_INSTALL_FAILED_TO_GET_TAG_OF_CLUSDISK;
    }

    _tprintf(_T("Tag for %s is %d\n"), CLUSDISK_DRIVER_NAME, dwClusdiskTagId);

    dwError = RegOpenKeyEx(    HKEY_LOCAL_MACHINE,        //hKey
                            GROUPORDERLIST_KEY,        //lpSubKey
                            0,                        //ulOptions - Reserved
                            KEY_QUERY_VALUE | KEY_SET_VALUE,        //samDesired
                            &hGroupOrderList);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_OPEN_KEY_EX_FAILED;
    }

    dwError = RegQueryValueEx(hGroupOrderList,
                                GROUP_VALUE_NAME,
                                NULL,
                                &dwValueType,
                                NULL,
                                &dwSize);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_QUERY_VALUE_EX_FAILED;
    }

    pSourceBinaryData = (PDWORD) malloc(dwSize);
    if (!pSourceBinaryData)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

    dwTargetSize = dwSize + sizeof(DWORD);

    pTargetBinaryData = (PDWORD) malloc(dwTargetSize);
    if (!pTargetBinaryData)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

    dwError = RegQueryValueEx(hGroupOrderList,
                                GROUP_VALUE_NAME,
                                NULL,
                                &dwValueType,
                                (LPBYTE)pSourceBinaryData,
                                &dwSize);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_QUERY_VALUE_EX_FAILED;
    }

    dwNumTags = *pSourceBinaryData;
    if (dwSize < ((dwNumTags + 1)*sizeof(DWORD)))
    {
        return CDSK_INSTALL_NUM_TAGS_AND_SIZE_MISMATCH;
    }

    pSource = pSourceBinaryData;
    pTarget = pTargetBinaryData;
    *pTarget = *pSource + 1;

    pSource++;
    pTarget++;

    bClusdiskTagFound = FALSE;

    for (i = 0; i < dwNumTags; i++)
    {
        if (bClusdiskTagFound == FALSE)
        {
            if  (*pSource == dwClusdiskTagId)
            {
                bClusdiskTagFound = TRUE;
                *pTarget++ = *pSource++;
                *pTarget++ = dwTagId;
            }
            else
            {
                *pTarget++ = *pSource++;
            }
            continue;
        }

        *pTarget++ = *pSource++;
    }

    if (!bClusdiskTagFound)
        return CDSK_INSTALL_CLUSDISK_TAG_NOT_FOUND_IN_GROUP;

    dwError = RegSetValueEx(hGroupOrderList,
                                GROUP_VALUE_NAME,
                                0,
                                dwValueType,
                                (LPBYTE)pTargetBinaryData,
                                dwTargetSize);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_SET_VALUE_EX_FAILED;
    }

    RegCloseKey(hGroupOrderList);

    free(pTargetBinaryData);
    free(pSourceBinaryData);

    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS
GetTagIdForNewDriver(DWORD &dwTagID, bool &bNewTag)
{
    DWORD   i, dwError, dwSize, dwValueType, dwNumTags;
    HKEY    hGroupOrderList;
    DWORD   dwClusdiskTagId = 0;
    DWORD   dwMaxTagValue = 0;
    PDWORD  pBinaryData;
    BOOL    bClusdiskTagFound;

    bNewTag = false;
    dwTagID = 0;

    // Query the Registry and get the Tag value for Clusdisk driver.
    if (CDSK_INSTALL_SUCCESS != GetTagIDOfDriver(CLUSDISK_DRIVER_NAME, &dwClusdiskTagId))
    {
        return CDSK_INSTALL_FAILED_TO_GET_TAG_OF_CLUSDISK;
    }

    dwError = RegOpenKeyEx(    HKEY_LOCAL_MACHINE,        //hKey
                            GROUPORDERLIST_KEY,        //lpSubKey
                            0,                        //ulOptions - Reserved
                            KEY_QUERY_VALUE | KEY_SET_VALUE,        //samDesired
                            &hGroupOrderList);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_OPEN_KEY_EX_FAILED;
    }

    dwError = RegQueryValueEx(hGroupOrderList,
                                GROUP_VALUE_NAME,
                                NULL,
                                &dwValueType,
                                NULL,
                                &dwSize);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_QUERY_VALUE_EX_FAILED;
    }

    pBinaryData = (DWORD *)malloc(dwSize);
    if (!pBinaryData)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

    dwError = RegQueryValueEx(hGroupOrderList,
                                GROUP_VALUE_NAME,
                                NULL,
                                &dwValueType,
                                (LPBYTE)pBinaryData,
                                &dwSize);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        return CDSK_INSTALL_REG_QUERY_VALUE_EX_FAILED;
    }

    dwNumTags = *pBinaryData;
    if (dwSize < ((dwNumTags + 1)*sizeof(DWORD)))
    {
        return CDSK_INSTALL_NUM_TAGS_AND_SIZE_MISMATCH;
    }

    pBinaryData++;
    bClusdiskTagFound = FALSE;

    for (i = 0; i < dwNumTags; i++) {
        if (dwMaxTagValue < *pBinaryData) {
            dwMaxTagValue = *pBinaryData;
        }

        if (bClusdiskTagFound == FALSE)
        {
            if  (*pBinaryData == dwClusdiskTagId)
                bClusdiskTagFound = TRUE;

            pBinaryData++;
            continue;
        }

        if (!dwTagID)
            dwTagID = *pBinaryData;

        break;
    }

    RegCloseKey(hGroupOrderList);

    if (!dwTagID) {
        dwTagID = dwMaxTagValue + 1;
        bNewTag = true;
    }

    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS
AddTagIDToService(
    TCHAR *DriverName,
    DWORD dwTagID
    )
{
    DWORD    dwError, dwSize;
    HKEY    hService;
    TCHAR    *ServiceKey;

    dwSize = (DWORD)_tcslen(SERVICES_KEY) + (DWORD)_tcslen(PATH_SEPARATOR) +
             (DWORD)_tcslen(DriverName) + 1;
    dwSize = dwSize * sizeof(TCHAR);

    ServiceKey = (TCHAR *)malloc(dwSize);
    if (!ServiceKey)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

	_tcscpy_s(ServiceKey, dwSize, SERVICES_KEY);
	_tcscat_s(ServiceKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(ServiceKey, dwSize, DriverName);

    dwError = RegOpenKeyEx(    HKEY_LOCAL_MACHINE,        //hKey
                            ServiceKey,                //lpSubKey
                            0,                        //ulOptions - Reserved
                            KEY_SET_VALUE,            //samDesired
                            &hService);

    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        free(ServiceKey);
        return CDSK_INSTALL_REG_OPEN_KEY_EX_FAILED;
    }

    free(ServiceKey);
    ServiceKey = NULL;

    dwSize = sizeof(DWORD);

    dwError = RegSetValueEx(hService,                // hKey
                            TAG_VALUE_NAME,            // lpValueName
                            0,                        // Reserved
                            REG_DWORD,                // lpType
                            (LPBYTE)&dwTagID,        // lpData
                            dwSize);
    if (dwError != ERROR_SUCCESS)
    {
        dwWin32Error = dwError;
        RegCloseKey(hService);
        return CDSK_INSTALL_REG_SET_VALUE_EX_FAILED;
    }

    RegCloseKey(hService);
    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS
InstallDriverUsingRegKeys(
    TCHAR    *DriverName,
    TCHAR    *DisplayName,
    TCHAR    *PathAndFileName,
    TCHAR    *GroupName,
    DWORD    dwTagID,
    BOOL     bIsWindows8AndLater
    )
{
    LONG    lReturn = ERROR_SUCCESS;
    HKEY    hService;
    TCHAR   ServiceRegKey[MAX_PATH + 1];
    DWORD   dwErrorControl = 1;
    DWORD   dwStart = SERVICE_SYSTEM_START;
    DWORD   dwType = SERVICE_KERNEL_DRIVER;
    DWORD   dwDisp = 0;
    TCHAR   *ImagePath = NULL;

    _stprintf(ServiceRegKey, _T("System\\CurrentControlSet\\Services\\%s"), DriverName);

    lReturn = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                             ServiceRegKey,
                             0,
                             NULL,
                             REG_OPTION_NON_VOLATILE,
                             KEY_WRITE,
                             NULL,
                             &hService,
                             &dwDisp);
    if (ERROR_SUCCESS != lReturn) {
        // Failed to open/create service key.
        return CDSK_INSTALL_CREATE_SERVICE_FAILED;
    }

    // Service key is created.
    if (NULL != DisplayName) {
        lReturn = RegSetValueEx(hService, SERVICE_DISPLAYNAME_VALUE_NAME, 0, REG_SZ,
                                (BYTE *)DisplayName, (DWORD)(_tcslen(DisplayName) + 1)*sizeof(TCHAR));
        if (ERROR_SUCCESS != lReturn) {
        }
    }

    if (NULL != PathAndFileName) {
        TCHAR   TempImagePath[MAX_PATH+1];
        if (0 == _tcsnicmp(SYSTEMROOT_PREFIX, PathAndFileName, _tcslen(SYSTEMROOT_PREFIX))) {
            ImagePath = PathAndFileName + _tcslen(SYSTEMROOT_PREFIX);
        } else {
            TCHAR   SystemRoot[MAX_PATH + 1];

            GetWindowsDirectory(SystemRoot, MAX_PATH);
			_tcscat_s(SystemRoot, TCHAR_ARRAY_SIZE(SystemRoot), _T("\\"));
            if (0 == _tcsnicmp(SystemRoot, PathAndFileName, _tcslen(SystemRoot))) {
                ImagePath = PathAndFileName + _tcslen(SystemRoot);
            } else {
                ImagePath = PathAndFileName;
                if (IS_DRIVE_LETTER(ImagePath[0])) {
					_tcscpy_s(TempImagePath, TCHAR_ARRAY_SIZE(TempImagePath), _T("\\??\\"));
					_tcscat_s(TempImagePath, TCHAR_ARRAY_SIZE(TempImagePath), ImagePath);
                   ImagePath = TempImagePath;
                }
            }
        }
        lReturn = RegSetValueEx(hService, SERVICE_IMAGE_PATH_VALUE_NAME, 0, REG_EXPAND_SZ,
                                (BYTE *)ImagePath, (DWORD)(_tcslen(ImagePath) + 1)*sizeof(TCHAR));
        if (ERROR_SUCCESS != lReturn) {
            return CDSK_INSTALL_IMAGEPATH_VALUE_CREATE_FAILED;
        }
    }

    if (NULL != GroupName) {
        lReturn = RegSetValueEx(hService, SERVICE_GROUP_VALUE_NAME, 0, REG_SZ,
                                (BYTE *)GroupName, (DWORD)(_tcslen(GroupName) + 1)*sizeof(TCHAR));
        if (ERROR_SUCCESS != lReturn) {
            return CDSK_INSTALL_GROUP_VALUE_CREATE_FAILED;
        }
    }

    lReturn = RegSetValueEx(hService, SERVICE_ERRORCONTROL_VALUE_NAME, 0, REG_DWORD, (BYTE *)&dwErrorControl, sizeof(DWORD));
    lReturn = RegSetValueEx(hService, SERVICE_START_VALUE_NAME, 0, REG_DWORD, (BYTE *)&dwStart, sizeof(DWORD));
    lReturn = RegSetValueEx(hService, SERVICE_TYPE_VALUE_NAME, 0, REG_DWORD, (BYTE *)&dwType, sizeof(DWORD));
    // We don't set tag value from Windows 2012 onwards
    if (!bIsWindows8AndLater && dwTagID) {
        lReturn = RegSetValueEx(hService, TAG_VALUE_NAME, 0, REG_DWORD, (BYTE *)&dwTagID, sizeof(DWORD));
    }

    AddEventLogEntry(DriverName,PathAndFileName,TYPES_SUPPORTED);
    _tprintf(_T("%s\n"),PathAndFileName);    
    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS
InstallDriver(
    TCHAR    *DriverName,
    TCHAR    *DisplayName,
    TCHAR    *PathAndFileName,
    TCHAR    *GroupName,
    DWORD    *pdwTagId
    )
{
    SC_HANDLE    schService;
    SC_HANDLE    schSCManager;

    schSCManager = OpenSCManager(
                        NULL,                    // local machine
                        NULL,                    // local database
                        SC_MANAGER_ALL_ACCESS    // access required
                        );
    if (!schSCManager)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_OPEN_SCMANAGER_FAILED;
    }

    schService = CreateService(
                    schSCManager,                        // hSCManager
                    DriverName,                         // lpServiceName
                    DisplayName,                        // lpDisplayName
                    SERVICE_ALL_ACCESS,                    // dwDesiredAccess
                    SERVICE_KERNEL_DRIVER,                // dwServiceType
                    SERVICE_SYSTEM_START,                // dwStartType
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
            return CDSK_INSTALL_CREATE_SERVICE_FAILED;
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

    AddEventLogEntry(DriverName,PathAndFileName,TYPES_SUPPORTED);
    _tprintf(_T("%s\n"),PathAndFileName);    
    return CDSK_INSTALL_SUCCESS;
}

void AddEventLogEntry(
    TCHAR *DriverName,
    TCHAR *pszMsgDLL,  // path for message DLL
    DWORD  dwNum)      // number of categories
{
    HKEY hKeyEvent; 
    DWORD dwData, dwDisp; 
    TCHAR    *EventKey;

    DWORD dwSize = (DWORD)_tcslen(EVENTLOG_KEY) + (DWORD)_tcslen(PATH_SEPARATOR) + 
             (DWORD)_tcslen(LOG_NAME) + (DWORD)_tcslen(PATH_SEPARATOR) + 
             (DWORD)_tcslen(DriverName) + 1 ;           //Terminating null character
    dwSize = dwSize * sizeof(TCHAR);

    EventKey = (TCHAR *)malloc(dwSize);
    if (!EventKey)
    {
        dwWin32Error = GetLastError();
        return;
    }

	_tcscpy_s(EventKey, dwSize, EVENTLOG_KEY);
	_tcscat_s(EventKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(EventKey, dwSize, LOG_NAME);
	_tcscat_s(EventKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(EventKey, dwSize, DriverName);
 
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, EventKey, 
          0, NULL, REG_OPTION_NON_VOLATILE,
          KEY_WRITE, NULL, &hKeyEvent, &dwDisp)) 
    {
        _tprintf(_T("Could not create the registry key.")); 
        goto AddEventLogEntry_Cleanup;
    }
  
   // Set the name of the message file. 
    if (RegSetValueEx(hKeyEvent,              // subkey handle 
            _T("EventMessageFile"),        // value name 
            0,                         // must be zero 
            REG_EXPAND_SZ,             // value type 
            (LPBYTE) pszMsgDLL,        // pointer to value data 
            (DWORD) _tcslen(pszMsgDLL) + 1)) // length of value data  + terminating null character
    {
        _tprintf(_T("Could not set the event message file.")); 
        RegCloseKey(hKeyEvent); 
        goto AddEventLogEntry_Cleanup;
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
        RegCloseKey(hKeyEvent); 
        goto AddEventLogEntry_Cleanup;
    }

AddEventLogEntry_Cleanup:
    RegCloseKey(hKeyEvent);
    free(EventKey);
    return;
}

CDSK_INSTALL_STATUS
InstallInMageClusDiskFilterDriver(
    TCHAR        *DriverName,
    TCHAR       *DisplayName,
    TCHAR        *Filepath
    )
{
    CDSK_INSTALL_STATUS Status;
    DWORD               dwTagId = 0;
    bool                bNewTag = false;

//  On 64-bit environments 32-bit app cannot open any file in c:\windows\system32
//  directory. 32-bit apps trying to read or write to c:\windows\system32 are 
//  redirected to c:\windows\SysWOW64 instead
//
//    Status = FileExists(Filepath);
//    if (CDSK_INSTALL_SUCCESS != Status) {
//        _tprintf(_T("Failed to find file \"%s\"."), Filepath);
//        return Status;
//    }

    Status = GetCurrentDriverInstallStatus(CLUSDISK_DRIVER_NAME);
    if ((Status == CDSK_INSTALL_OPEN_SCMANAGER_FAILED) ||
        (Status == CDSK_INSTALL_OPEN_SERVICE_FAILED ) ||
        (Status == CDSK_INSTALL_QUERY_SERVICE_STATUS_FAILED ))
    {
        return Status;        
    }

    if (Status == CDSK_INSTALL_DRIVER_DOES_NOT_EXIST)
        return CDSK_INSTALL_CLUSDISK_NOT_INSTALLED;

    // Get OS version
    // OS version is less than Windows 8/Windows 2012
    OSVERSIONINFO osvi;
    BOOL bIsWindows8AndLater = 0;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    GetVersionEx(&osvi);

    // Windows 8/Windows 2012 has version 6.2

    bIsWindows8AndLater = ((osvi.dwMajorVersion == 6) && (osvi.dwMinorVersion > 1)) ;
    if(!bIsWindows8AndLater) {
        Status = GetTagIdForNewDriver(dwTagId, bNewTag);
        if (CDSK_INSTALL_SUCCESS != Status) {
            return Status;
        }
    }

    Status = InstallDriverUsingRegKeys(DriverName,DisplayName, Filepath, GROUP_VALUE_NAME, dwTagId, bIsWindows8AndLater);
    if (CDSK_INSTALL_SUCCESS != Status)
    {
        return Status;
    }

    if (bNewTag)
    {
        Status = InsertTagIdForNewDriver(dwTagId);
        if (CDSK_INSTALL_SUCCESS != Status)
        {
            return Status;
        }
    }

    return CDSK_INSTALL_SUCCESS;
}

CDSK_INSTALL_STATUS 
RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey)
{
    LPTSTR lpEnd;
    LONG lResult;
    DWORD dwSize;
    TCHAR szName[MAX_PATH];
    HKEY hKey;
    FILETIME ftWrite;    
    CDSK_INSTALL_STATUS status;

    // First, see if we can delete the key without having
    // to recurse.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS) 
        return CDSK_INSTALL_SUCCESS;

    lResult = RegOpenKeyEx (hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);

    if (lResult != ERROR_SUCCESS) 
    {
        if (lResult == ERROR_FILE_NOT_FOUND) {
            return CDSK_INSTALL_SUCCESS;
        } 
        else {
            dwWin32Error = lResult;
            return CDSK_INSTALL_REG_OPEN_KEY_EX_FAILED;
        }
    }

    // Check for an ending slash and add one if it is missing.

    lpEnd = lpSubKey + lstrlen(lpSubKey);

    if (*(lpEnd - 1) != TEXT('\\')) 
    {
        *lpEnd =  TEXT('\\');
        lpEnd++;
        *lpEnd =  TEXT('\0');
    }

    // Enumerate the keys

    dwSize = MAX_PATH;

    lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                           NULL, NULL, &ftWrite);

    if (lResult == ERROR_SUCCESS) 
    {
        do {

            lstrcpy (lpEnd, szName);

            if (CDSK_INSTALL_SUCCESS != (status = RegDelnodeRecurse(hKeyRoot, lpSubKey))) {
                RegCloseKey (hKey);
                return status;
            }

            dwSize = MAX_PATH;           

            lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                                   NULL, NULL, &ftWrite);

        } while (lResult == ERROR_SUCCESS);
    } 
    
    if (ERROR_NO_MORE_ITEMS != lResult) {
        RegCloseKey(hKey);
        dwWin32Error = lResult;
        return CDSK_INSTALL_REG_DELETE_KEY_FAILED;
    }

    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey (hKey);

    // Try again to delete the key.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS) 
        return CDSK_INSTALL_SUCCESS;

    dwWin32Error = lResult;
    return CDSK_INSTALL_REG_DELETE_KEY_FAILED;
}

//*************************************************************
//
//  RegDelnode()
//
//  Purpose:    Deletes a registry key and all it's subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

CDSK_INSTALL_STATUS
RegDelNode (HKEY hKeyRoot, LPTSTR lpSubKey)
{
    TCHAR szDelKey[2 * MAX_PATH];

    lstrcpy (szDelKey, lpSubKey);
    return RegDelnodeRecurse(hKeyRoot, szDelKey);

}

CDSK_INSTALL_STATUS
DeleteDriverKeys(LPTSTR DriverName)
{
    DWORD    dwSize;
    TCHAR    *DriverKey;
    TCHAR   *EventKey;
    CDSK_INSTALL_STATUS Status;

    dwSize = (DWORD)_tcslen(SERVICES_KEY) + (DWORD)_tcslen(PATH_SEPARATOR) +
             (DWORD)_tcslen(DriverName) + 1;
    dwSize = dwSize * sizeof(TCHAR);

    DriverKey = (TCHAR *)malloc(dwSize);
    if (!DriverKey) {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

	_tcscpy_s(DriverKey, dwSize, SERVICES_KEY);
	_tcscat_s(DriverKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(DriverKey, dwSize, DriverName);

    Status = RegDelNode(HKEY_LOCAL_MACHINE, DriverKey);

    dwSize = (DWORD)_tcslen(EVENTLOG_KEY) + (DWORD)_tcslen(PATH_SEPARATOR) +
             (DWORD)_tcslen(LOG_NAME) + (DWORD)_tcslen(PATH_SEPARATOR) +
             (DWORD)_tcslen(DriverName) + 1;

    dwSize = dwSize * sizeof(TCHAR);

    EventKey = (TCHAR *)malloc(dwSize);
    if (!EventKey)
    {
        dwWin32Error = GetLastError();
        return CDSK_INSTALL_MEMORY_ALLOC_FAILED;
    }

	_tcscpy_s(EventKey, dwSize, EVENTLOG_KEY);
	_tcscat_s(EventKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(EventKey, dwSize, LOG_NAME);
	_tcscat_s(EventKey, dwSize, PATH_SEPARATOR);
	_tcscat_s(EventKey, dwSize, DriverName);

    Status = RegDelNode(HKEY_LOCAL_MACHINE, EventKey);

    free(DriverKey);
    free(EventKey);

    return Status;
}

TCHAR *
GetCDskInstallErrorDescription(ULONG ulErrorNum)
{
    if (ulErrorNum > CDSK_INSTALL_MAX_ERROR_VALUE)
        ulErrorNum = CDSK_INSTALL_MAX_ERROR_VALUE;

    return ErrorDescriptions[ulErrorNum];
}
