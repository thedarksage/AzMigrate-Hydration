#pragma once

#ifndef __AFXWIN_H__
	//include stdafx.h before including any other header files in host config source
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"	

class ChostconfigwxcommonApp : public CWinAppEx
{
public:
	ChostconfigwxcommonApp();

	UINT GetScoutAgentInstallCode();
	void StartLogging();
	void EndLogging();

public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

extern ChostconfigwxcommonApp theApp;