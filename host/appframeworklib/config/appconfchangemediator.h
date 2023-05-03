#ifndef APPCONF_CHANGE_MEDIATOR_H
#define APPCONF_CHANGE_MEDIATOR_H

#include <ace/Task.h>
#include <sigslot.h>
#include <boost/shared_ptr.hpp>

#include "applicationsettings.h"


class ConfigChangeMediator ;
typedef boost::shared_ptr<ConfigChangeMediator> ConfigChangeMediatorPtr ;

class ConfigChangeMediator : public ACE_Task<ACE_MT_SYNCH>
{
public:
    ConfigChangeMediator(ACE_Thread_Manager* thrMgr) ;
    virtual int open(void *args = 0) = 0 ;
    virtual int close(u_long flags = 0) = 0 ;
    virtual int svc() = 0 ;
    static ConfigChangeMediatorPtr CreateConfigChangeMediator() ;
    virtual void LoadCachedSettings() = 0;
    virtual void GetConfigInfoAndProcess() = 0;
} ;

class AppConfigChangeMediator ;
typedef boost::shared_ptr<AppConfigChangeMediator> AppConfigChangeMediatorPtr ;

class AppConfigChangeMediator : public ConfigChangeMediator
{
private:
    SV_UINT m_pollInterval;
    ApplicationSettings m_settings ;
    ReachableCx m_ReachableCx ;
    
    AppConfigChangeMediator() ;
    void PollConfigInfo() ;
    void GetApplicationSettings() ;
    AppConfigChangeMediator(SV_UINT pollInterval, ACE_Thread_Manager* thrMgr) ;
    void ProcessPendingUpdates();
    
public:
    static AppConfigChangeMediatorPtr CreateInstance(ACE_Thread_Manager* ptr) ;
    static AppConfigChangeMediatorPtr GetInstance() ;
    ~AppConfigChangeMediator() ;
    void LoadCachedSettings() ;
    void GetConfigInfoAndProcess();
    static AppConfigChangeMediatorPtr m_instancePtr ;
    
    //Implementing ConfigChangeMediator Interface
    int open(void *args = 0) ;
    int close(u_long flags = 0) ;
    int svc() ;
    
    bool remoteCxsReachable() ;
    bool getViableCxs(ViableCxs & viablecxs) ;
    sigslot::signal1<ApplicationSettings> m_applicationSettingsSignal ;
    sigslot::signal1<ApplicationSettings>& getApplicationSettingsSignal() ;
} ;


typedef boost::shared_ptr<AppConfigChangeMediator> AppConfigChangeMediatorPtr ;
#endif