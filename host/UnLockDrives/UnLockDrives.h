// UnLockDrives.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

#include "hostagenthelpers.h"
#include "defines.h"
#include "autoFs.h"

#include "localconfigurator.h"

// CUnLockDrivesApp:
// See UnLockDrives.cpp for the implementation of this class
//

class CUnLockDrivesApp : public CWinApp
{
public:
	CUnLockDrivesApp();
	void InitializePlatformDeps(void);
	void DeInitializePlatformDeps(void);

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CUnLockDrivesApp theApp;

SVERROR GetProtectedDrivesFromRegistry();
//HRESULT UnhideDrives(DWORD dwUnhideDrives, DWORD & FailedDrives);
bool IsSystemDrive( const char * pch_VolumeName );
