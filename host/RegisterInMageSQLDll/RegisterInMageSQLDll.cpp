// RegisterInMageSQLdllAssembly.cpp : Defines the entry point for the console application.
//

#include<iostream>
#include<atlbase.h>
#include<conio.h>
#include<direct.h>
#include "Shlwapi.h"
#include "inmsafecapis.h"

using namespace std;
#define CLSID_InMageSQL "{46A951AC-C2D9-48E0-97BE-91F3C9E7B065}"
typedef  HRESULT  (__stdcall *FNPTR_INMGET_COR_SYS_DIR) ( LPWSTR pbuffer, DWORD cchBuffer, DWORD* dwlength);

bool IsDotNetInstalled()
{
	HINSTANCE hDotNetDLL = LoadLibrary(TEXT("mscoree.dll"));
	if(NULL!=hDotNetDLL)
	{ 
		return true;
	}
	else 
		return false;
}

void main()
{
	    USES_CONVERSION;
		  string strInMageSqlDll =" InMageSQL.dll /codebase /tlb /silent";
	      bool bIsDotNetFrameWorkInstalled =IsDotNetInstalled();
		  if(bIsDotNetFrameWorkInstalled)
		  {
			  HINSTANCE hDLL = LoadLibrary(TEXT("mscoree.dll"));
			  FNPTR_INMGET_COR_SYS_DIR   GetCORSystemDirectory = NULL;
			  GetCORSystemDirectory = (FNPTR_INMGET_COR_SYS_DIR) GetProcAddress (hDLL,"GetCORSystemDirectory"); 
		      if(GetCORSystemDirectory!=NULL)
			  {		 
				 WCHAR lInmBuffer[MAX_PATH + 1];
				 DWORD lInmLength; 
			     HRESULT hr = GetCORSystemDirectory(lInmBuffer,MAX_PATH,&lInmLength);	 
				 if(S_OK==hr)
				 {
					WCHAR exePathBuffer[MAX_PATH + 1];
				    DWORD length; 
			        HRESULT hr = GetCORSystemDirectory(exePathBuffer,MAX_PATH,&length);
					inm_wcscat_s( exePathBuffer,ARRAYSIZE(exePathBuffer), L"RegAsm.exe" );
				    STARTUPINFO lInmStartupInfo;
				    PROCESS_INFORMATION lInmProcessInfo;
				    ZeroMemory( &lInmStartupInfo, sizeof(lInmStartupInfo) );
				    lInmStartupInfo.cb = sizeof(lInmStartupInfo);
				    ZeroMemory( &lInmProcessInfo, sizeof(lInmProcessInfo) );	
				    if(!CreateProcess(W2A(exePathBuffer),(LPSTR)strInMageSqlDll.c_str(),NULL, NULL,FALSE, 0,NULL,NULL,&lInmStartupInfo,&lInmProcessInfo ) )
				    {
				    	 cout<<"Registering InMageSQLDll is Failed."<<GetLastError()<<endl;
				    }
					else
				    	cout<<"InMageSQL.dll Registered Successfully"<<endl;

				    WaitForSingleObject( lInmProcessInfo.hProcess, INFINITE );
					CloseHandle( lInmProcessInfo.hThread ); 
				    CloseHandle( lInmProcessInfo.hProcess );
	   	   		    
				 }
				 else
				 {
					cout<<"InValid memory pointer for .NET path"<<endl;
				 }
			  }
			  else
			  {
				  cout<<"Unable to get the DOTNET installation path"<<endl;
			  }
		  }
		  else
		  {
			  cout<<"DOTNET F/W is Not installed"<<endl;
		  }
		

}

