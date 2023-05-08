#ifndef INMAGE_UNIXSERVICE_H_
#define INMAGE_UNIXSERVICE_H_

#ifndef SV_WINDOWS

#include "cservice.h"
#include "ace/Singleton.h"
#include "ace/Mutex.h"
#include "volumemonitormajor.h"

class UnixService : public CService
{
public:
  UnixService ();
  ~UnixService();

  virtual int install_service();
  virtual int start_service();
  virtual int run_startup_code();
  virtual bool isInstalled();

  virtual int RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data);
  virtual int RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data);
//  virtual int RegisterServiceEventHandler(int (*fn)(signed long,void *), void *);
  virtual bool InstallDriver();
  virtual bool UninstallDriver();

  //Volume monitor functions
  virtual void ProcessVolumeMonitorSignal(void*);  
  virtual bool startVolumeMonitor();
  virtual bool stopVolumeMonitor();
  virtual bool VolumeAvailableForNode(std::string const & volumeName);
  virtual bool isClusterVolume(std::string const & volumeName) {
      return false; // NOTE currently no special logic for cluster volume on linux so not a cluster volume
  }
  virtual bool EnableResumeFilteringForOfflineVolume(std::string const & volumeName);

  int start_daemon();

  bool service_started;

bool InitializeJobObject();
void closeJobObjectHandle();
  

private:
  VolumeMonitorTask::VolumeMonitorTaskPtr m_VolumeMonitor;
};

typedef ACE_Singleton<UnixService, ACE_Mutex> SERVICE;

#endif /* !SV_WINDOWS */

#endif /* INMAGE_UNIXSERVICE_H */

