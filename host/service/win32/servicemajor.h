#ifndef INMAGE_WINSERVICE_H_
#define INMAGE_WINSERVICE_H_

#ifdef SV_WINDOWS 

#include <windows.h>
#include <atlbase.h>
#include <string>

#include "..\cservice.h"
#include "ace/Event_Handler.h"
#include "ace/NT_Service.h"
#include "ace/Singleton.h"
#include "ace/Mutex.h"

#include "cluster.h"
#include "volumemonitor.h"


#ifndef FILTER_NAME
const TCHAR FILTER_NAME[] = _T( "involflt" );
#endif

class WinService : public ACE_NT_Service, public CService
{
public:

  WinService();
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
  virtual bool InstallDriver();
  virtual bool UninstallDriver();
  virtual bool startClusterMonitor();
  virtual bool stopClusterMonitor();
  virtual bool VolumeAvailableForNode(std::string const & volumeName);
  virtual bool isClusterVolume(std::string const & volumeName);
  virtual bool EnableResumeFilteringForOfflineVolume(std::string const & volumeName) { return true; }
  virtual bool startVolumeMonitor();
  virtual bool stopVolumeMonitor();
  
  virtual bool isDriverInstalled() const;
  const std::string GetVolumesOnlineAsString();
  virtual void ProcessVolumeMonitorSignal(void*);
  virtual void ProcessBitlockerMonitorSignal(void*);
  
//Job object related funtions
  bool InitializeJobObject();
  bool AssignChildProcessToJobObject(pid_t lProcessId);
  void closeJobObjectHandle();

private:
  
  void SetServiceRecoveryOptions();
  void ChangeServiceDescription();
  void SetupWinSockDLL();
  void CleanupWinSockDLL();
  void InitServiceHandle();
  
  ACE_TCHAR *servicename;
  ACE_TCHAR *servicetitle;
  ACE_TCHAR *servicedesc;
  typedef ACE_NT_Service inherited;
  int	quit;
  SC_HANDLE m_hServiceHandle;
  std::string m_sVolumesOnline;

  VolumeMonitor m_VolumeMonitor;
  Cluster m_Cluster;
  bool m_bClusterMonitorStarted;  


  bool m_bVolumeMonitorStarted;
  
  HANDLE m_jobObjectHandle;
};

typedef ACE_Singleton<WinService, ACE_Mutex> SERVICE;

#endif /* SV_WINDOWS */

#endif /* INMAGE_WINSERVICE_H */

