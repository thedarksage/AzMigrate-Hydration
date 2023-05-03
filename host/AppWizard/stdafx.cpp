// stdafx.cpp : source file that includes just the standard includes
// AppWizard.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

#include <ace/Init_ACE.h>
#include "configvalueobj.h"
#include "util/common/util.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#pragma comment ( linker, "/SUBSYSTEM:CONSOLE" )

void init() 
{
    HRESULT hr;
 do
 {
  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

  if (hr != S_OK)
  {
   DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
   DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
   break;
  }

  hr = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_CONNECT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
 
  if (hr == RPC_E_TOO_LATE)
  {
   hr = S_OK;
  }

  if (hr != S_OK)
  {
   DebugPrintf("FILE: %s, LINE %d\n",__FILE__, __LINE__);
   DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
   CoUninitialize();
   break;
  }
 } while(false);
 SetupWinSockDLL() ;
}

int _tmain(int argc, ACE_TCHAR* argv[], ACE_TCHAR* envp[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    ACE::init();
	if( argc == 1 )
        FreeConsole();
	LPTSTR lpszCommandLine = ::GetCommandLine();
	if(lpszCommandLine == NULL)
		return -1;
	STARTUPINFO StartupInfo;
	StartupInfo.dwFlags = 0;
	::GetStartupInfo(&StartupInfo);
	init() ;
    if( argc > 1 )
    {
	    if( !ConfigValueObj::parse(argv, argc))
	    {
		    std::cout<<"Invalid options" <<std::endl ;
		    exit(-1) ;
	    }
    }
	return _tWinMain(::GetModuleHandle(NULL), NULL, lpszCommandLine,
		(StartupInfo.dwFlags & STARTF_USESHOWWINDOW) ?
		StartupInfo.wShowWindow : SW_SHOWDEFAULT);
}