#ifndef CONTROLLER_H
#define CONTROLLER_H
#include <ace/Atomic_Op.h>
#include "config/appconfigurator.h"
#include "applicationsettings.h"
#include "discovery/discoverycontroller.h"
#include "app/appcontroller.h"
#include "appscheduler.h"
#include "util/volpack.h"
#include <ace/Atomic_Op.h>
#include <sigslot.h>
#include <boost/shared_ptr.hpp>
#include <ace/Thread_Manager.h>
#include<ace/Message_Queue.h>


typedef ACE_Message_Queue<ACE_MT_SYNCH> ACE_SHARED_MQ ;

class Controller ;
typedef boost::shared_ptr<Controller> ControllerPtr ;

class Controller : public sigslot::has_slots<>,public ACE_Task<ACE_MT_SYNCH>
{
    AppControllerPtr m_appProtectionControllerPtr ;
    AppSchedulerPtr m_schedulerPtr ;
    sigslot::signal1<std::map<SV_ULONG, TaskInformation> > m_applicationSchedSettingsSignal ;
    sigslot::signal1<std::map<std::string, std::map<std::string, AppProtectionSettings> > > m_applicationProtectionSettingsSignal ;
    ApplicationSettings m_settings ;
    static ControllerPtr m_instancePtr ;    
    DiscoveryControllerPtr m_discoveryControllerPtr ; 
    AppAgentConfiguratorPtr m_configuratorPtr ;
    ACE_Thread_Manager* m_ThreadManagerPtr;
    ACE_SHARED_MQ m_MsgQueue ;
    std::set<std::string> m_selectedApplicationNameSet;
    std::map<InmProtectedAppType, AppControllerPtr> m_appControllersMap ;

    AppControllerPtr getAppController(InmProtectedAppType type) ;
	SVERROR createVolPack(SV_ULONG policyId, VolPack objVolPack, std::map<std::string,std::string>& mountPointToDeviceFileMap, SV_ULONG& exitCode);
    SVERROR deleteVolPack(SV_ULONG policyId, VolPack& objVolPack, SV_ULONG& exitCode);
    SVERROR resizeVolPack(SV_ULONG policyId, VolPack& objVolPack, SV_ULONG& exitCode);
    SVERROR ProcessVolPack(SV_ULONG policyId);
	SVERROR executeScript(const SV_ULONG& policyId );
	SVERROR createConfFile(ScriptPolicyDetails, std::string);
    mutable ACE_Recursive_Thread_Mutex m_userTokenLock ;
#ifdef SV_WINDOWS
    HANDLE m_userToken_ ;
#endif
public:
	ACE_Atomic_Op<ACE_Thread_Mutex, bool> m_shouldProcessRequests ;
	int m_volpackIndex ;
	bool m_bActive ;
    Controller(ACE_Thread_Manager* tm) ;
    SVERROR init(const std::set<std::string>& selectedApplicationNameSet, bool impersonate=true) ;
    SVERROR checkMigrationRollbackPending();
	SVERROR run() ;
    bool isActive() ;
    void Stop() ;
    bool QuitRequested(int seconds) ;
    sigslot::signal1<std::map<SV_ULONG, TaskInformation> >& getApplicationSchedSettingsSignal() ;
    void ApplicationSettingsCallback(ApplicationSettings settings) ;
    void PostMsg(SV_ULONG PolicyId) ;
    void ProcessRequest(SV_ULONG policyId) ;
	int getPolicyPriority(SV_ULONG policyId,std::string policyType="");
    ACE_Thread_Manager* thrMgr() ;
    static ControllerPtr createInstance(ACE_Thread_Manager* tm) ;
    static ControllerPtr getInstance() ;
    typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuard;
    void setProcessRequests(bool process) ;
#ifdef SV_WINDOWS
	//This function will return the duplicate handle of the logon user handle
	//It will return NULL in case of failure i.e the original is not a valied handle or failure in duplucating handle
	//Caller is responsible for closing the handle return from this function.
	HANDLE getDuplicateUserHandle();
    HANDLE getUserHandle() 
    { 
        AutoGuard guard(m_userTokenLock) ;
        return m_userToken_ ; 
    }
    void setUserHandle(HANDLE h) { m_userToken_= h ; }
	SVERROR resetUserHandle();
#else
	void* getUserHandle() { return NULL ; }
#endif   
} ;
#endif
