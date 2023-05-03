#pragma once
#include <iostream>
#include <Windows.h>
#include <string>
#include "Shlwapi.h"

class ServiceManager
{
private:
  BOOL IsInstalled();
  bool stopService();
  bool deleteService();

  std::string m_strServiceDomainName;
  std::string m_strServiceUserName;
  std::string m_strServicePassword;
  bool m_bStopFlag;
	
  

public:

	STARTUPINFO si;
	STARTUPINFO si1;
	PROCESS_INFORMATION pi;
	PROCESS_INFORMATION pi1;

  ServiceManager(void);
  ~ServiceManager(void);
  int CommandLineHelp();
  bool Install();
  bool UnInstall();
  //static int SpawnAProcess(const char* pszImageName);
  //static void LogEvent( LPCTSTR pFormat, ... );
  void SetDomainName(const char* pszDomainName);
  void SetUserName(const char* pszUserName);
  void SetPassword(const char* pszPassword);
  void SvcInit(DWORD argc, LPTSTR *argv);
  VOID ReportSvcStatus( DWORD dwCurrentState,
                      DWORD dwWin32ExitCode,
                      DWORD dwWaitHint);
  HANDLE GetRunJobsHandle();
  HANDLE GetMonitroJobsHandle();
  void SetStopFlag();
  bool GetFlagStatus();
  void InitializeProcessHandles();
};
