// AppWizard.h : main header file for the PROJECT_NAME application
//
#include "stdafx.h"
#pragma once

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "InitializerDlg.h"
#include "appglobals.h"
#include <list>
// CAppWizardApp:
// See AppWizard.cpp for the implementation of this class
//

class CAppWizardApp : public CWinApp
{
public:
	CAppWizardApp();

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

UINT discAppInfoThreadFunc (LPVOID pParam);// Applicato Discovery thread functon
void getExistedAppList() ;
SVERROR performDiscovery();