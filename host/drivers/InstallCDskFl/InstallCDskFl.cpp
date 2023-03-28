#include "InsCDskFl.h"
#include <stdio.h>

void WaitForEnterKey()
{ 
    _tprintf(_T("\nPress enter to continue..."));
    fflush(stdout);
    getc(stdin);
}

void Usage(TCHAR *ExeName)
{
    _tprintf(_T("Usage: %s /install <DriverName> <Display Name> <Filepath>\n"), ExeName);
    _tprintf(_T("Usage: %s /uninstall <DriverName>\n"), ExeName);
    _tprintf(_T("For Example %s /install InCDskFl \"InMage Cluster Filter Driver\" \"%%SystemRoot%%\\System32\\drivers\\InCDskFl.sys\"\n"), ExeName);
    _tprintf(_T("For Example %s /uninstall InCDskFl\n"), ExeName);
    WaitForEnterKey();    
    return;
}

void _cdecl _tmain(int argc, TCHAR *argv[])
{
    CDSK_INSTALL_STATUS eInstallStatus;

    if (argc < 2)
    {
        Usage(argv[0]);
        return;
    }

    if (0 == lstrcmpi(argv[1], _T("/install"))) {
        if (argc < 5)
        {
            Usage(argv[0]);
            return;
        }

        eInstallStatus = InstallInMageClusDiskFilterDriver(argv[2], argv[3], argv[4]);

        if (CDSK_INSTALL_SUCCESS != eInstallStatus) {
            _tprintf(_T("InstallInMageClusDiskFilterDriver failed with %s, InstallStatus = %d, Win32Error = %d\n"), 
                GetCDskInstallErrorDescription(eInstallStatus), eInstallStatus, dwWin32Error);
            if (CDSK_INSTALL_CLUSDISK_NOT_INSTALLED != eInstallStatus) {
                WaitForEnterKey();
            }
        } 
        return;
    }

    if (0 == lstrcmpi(argv[1], _T("/uninstall"))) {
        if (argc < 3)
        {
            Usage(argv[0]);
            return;
        }

        // for now we will not delete the registry entries as that can cause
        // an issue if you uninstall and reinstall with out booting in between
        // namely that the registry entries are removed but not recreated during 
        // install because InCDskFl is still running. since InCDskFl filter driver 
        // will run in bypass mode if clusdisk is not loaded we should be OK until 
        // we resolve this problem
        eInstallStatus = DeleteDriverKeys(argv[2]);

        if (CDSK_INSTALL_SUCCESS != eInstallStatus) {
            _tprintf(_T("DeleteDriverKeys failed with %s, InstallStatus = %d, Error = %d\n"), 
                GetCDskInstallErrorDescription(eInstallStatus), eInstallStatus, dwWin32Error); 
            WaitForEnterKey();
        }        
        return;
    }

    Usage(argv[0]);
    
    return;
}

