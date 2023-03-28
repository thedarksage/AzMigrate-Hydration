#ifndef INMAGE_UNIXSERVICE_H_
#define INMAGE_UNIXSERVICE_H_

#ifndef SV_WINDOWS

#include "cservice.h"
#include "ace/Singleton.h"
#include "ace/Mutex.h"

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
  int start_daemon();
  bool service_started;
};

typedef ACE_Singleton<UnixService, ACE_Mutex > SERVICE;

#endif /* !SV_WINDOWS */

#endif /* INMAGE_UNIXSERVICE_H */

