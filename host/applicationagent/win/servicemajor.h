#ifndef INMAGE_WINSERVICE_H_
#define INMAGE_WINSERVICE_H_

#ifdef SV_WINDOWS 

#include <windows.h>
#include <atlbase.h>
#include <string>

#include "cservice.h"
#include "ace/Event_Handler.h"
#include "ace/NT_Service.h"
#include "ace/Singleton.h"
#include "ace/Mutex.h"

class WinService : public ACE_NT_Service, public CService
{
public:

	WinService ();
	~WinService();

  virtual int install_service();
  virtual int remove_service();
  virtual int start_service();
  virtual int stop_service();
  virtual void name(const ACE_TCHAR *svcname = NULL, const ACE_TCHAR *svctitle = NULL, const ACE_TCHAR *svcdesc = NULL);
  virtual bool isInstalled();
  
  virtual int run_startup_code();

  virtual void handle_control (DWORD control_code);
  virtual int svc ();

  virtual int RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data);
  virtual int RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data);
  //virtual int RegisterServiceEventHandler(int (*fn)(signed long,void *), void *);

private:
  
  void SetServiceRecoveryOptions();
  void ChangeServiceDescription();
  void SetupWinSockDLL();
  void CleanupWinSockDLL();
  void InitServiceHandle();
  
private:
  ACE_TCHAR *servicename;
  ACE_TCHAR *servicetitle;
  ACE_TCHAR *servicedesc;
  typedef ACE_NT_Service inherited;
  int	quit;
  SC_HANDLE m_hServiceHandle;
};

typedef ACE_Singleton<WinService, ACE_Mutex> SERVICE;

#endif /* SV_WINDOWS */

#endif /* INMAGE_WINSERVICE_H */

