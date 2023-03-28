// UnLockDrives.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "UnLockDrives.h"
#include "UnLockDrivesDlg.h"
#include "globs.h"
#include <Objbase.h>
#include "terminateonheapcorruption.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CUnLockDrivesApp
using namespace std;

BEGIN_MESSAGE_MAP(CUnLockDrivesApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


// CUnLockDrivesApp construction

CUnLockDrivesApp::CUnLockDrivesApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

void CUnLockDrivesApp::InitializePlatformDeps(void)
{
	HRESULT hr;

    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        if (hr != S_OK)
        {
            //DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            //DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        hr = CoInitializeSecurity
            (
            NULL,                                //  IN PSECURITY_DESCRIPTOR         pSecDesc,
            -1,                                  //  IN LONG                         cAuthSvc,
            NULL,                                //  IN SOLE_AUTHENTICATION_SERVICE *asAuthSvc,
            NULL,                                //  IN void                        *pReserved1,
            RPC_C_AUTHN_LEVEL_CONNECT,           //  IN DWORD                        dwAuthnLevel,
            RPC_C_IMP_LEVEL_IMPERSONATE,         //  IN DWORD                        dwImpLevel,
            NULL,                                //  IN void                        *pAuthList,
            EOAC_NONE,                           //  IN DWORD                        dwCapabilities,
            NULL                                 //  IN void                        *pReserved3
            );


        // Check if the securiy has already been initialized by someone in the same process.
        if (hr == RPC_E_TOO_LATE)
        {
            hr = S_OK;
        }

        if (hr != S_OK)
        {
            //DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
            //DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
            CoUninitialize();
            break;
        }
    } while(false);
}

void CUnLockDrivesApp::DeInitializePlatformDeps(void)
{
	CoUninitialize();
}



// The one and only CUnLockDrivesApp object

CUnLockDrivesApp theApp;

// CUnLockDrivesApp initialization

BOOL CUnLockDrivesApp::InitInstance()
{
	TerminateOnHeapCorruption();
	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	InitializePlatformDeps();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	CUnLockDrivesDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	DeInitializePlatformDeps();
	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
const char* SV_OUTPOST_PROTECTED_DRIVES = "TargetDrives";



SVERROR GetProtectedDrivesFromRegistry()
{
	SVERROR hr = SVS_OK;

	LocalConfigurator localConfigurator;
	do
	{
		string ProtectedDrives= localConfigurator.getProtectedVolumes();

		DebugPrintf("ProtectedDrives : %ld\n", ProtectedDrives);

	}while(FALSE);

	return hr ;
}

bool IsSystemDrive( const char * pch_VolumeName )
{
	if (pch_VolumeName)
	{
		TCHAR rgtszSystemDirectory[ MAX_PATH + 1 ];
		if( GetSystemDirectory( rgtszSystemDirectory, MAX_PATH + 1 ))
		{
			if(IsDrive(pch_VolumeName))
			{
				if( toupper( pch_VolumeName[ 0 ] ) == toupper( rgtszSystemDirectory[ 0 ] ) )
				{
					return (true);
				}
			}
		}
	}
	return (false);
}
