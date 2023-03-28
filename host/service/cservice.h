#ifndef INMAGE_CSERVICE_H_
#define INMAGE_CSERVICE_H_

#include <string>
#include <cstring>
#include "ace/OS_main.h"
#include "ace/OS_NS_strings.h"
#include "ace/OS_NS_wchar.h"
#include "ace/Log_Msg.h"
#include "sigslot.h"

const int MSG_DEVICE_EVENT  = (long int)0x10000;

class CService:public sigslot::has_slots<>
{
public:
	CService ();
	~CService ();

  virtual int install_service();
  virtual int remove_service();
  virtual int start_service();
  virtual int stop_service();
  virtual void name(const char *name = NULL, const char *long_name = NULL, const char *desc = NULL);
  virtual int run_startup_code();
  virtual bool isInstalled();

  virtual int RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data);
  virtual int RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data);
  virtual int daemonized();
  virtual bool InstallDriver();
  virtual bool UninstallDriver();
  virtual bool startClusterMonitor();
  virtual bool stopClusterMonitor();
  virtual bool startVolumeMonitor();
  virtual bool stopVolumeMonitor();
 
  virtual bool isDriverInstalled() const;
  virtual sigslot::signal1<void*>& getVolumeMonitorSignal();
  virtual void ProcessVolumeMonitorSignal(void*);
  virtual sigslot::signal1<void*>& getBitlockerMonitorSignal();
  virtual void ProcessBitlockerMonitorSignal(void*);
  virtual bool VolumeAvailableForNode(std::string const & volumeName) = 0;
  virtual bool isClusterVolume(std::string const & volumeName) = 0;
  virtual bool EnableResumeFilteringForOfflineVolume(std::string const & volumeName) = 0;
  
  // support for cluster resources that go offline
  // NOTE: for now no need to make virtual even though only windows supports clusters
  // as these are simple seting/resetting of stopAgentsForOfflineResource
  void ResourceOfflineCallback() { 
      stopAgentsForOfflineResource = true; 
  }
  void ResetStopAgentsForOfflineResource() { 
      stopAgentsForOfflineResource = false; 
  }
  bool StopAgentsForOfflineResource() { 
      return stopAgentsForOfflineResource; 
  }

public:

  int opt_install;
  int opt_remove;
  int opt_start;
  int opt_kill;

  int daemon;

  int (*startfn)(void *);
  int (*stopfn)(void *);
  void *startfn_data;
  void *stopfn_data;

protected:
    sigslot::signal1<void*> m_VolumeMonitorSignal;
    sigslot::signal1<void*> m_BitlockerMonitorSignal;

private:
    bool stopAgentsForOfflineResource;
};

#endif /* INMAGE_CSERVICE_H */

