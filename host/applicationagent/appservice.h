#ifndef INMAGE_VXSERVICE_H_
#define INMAGE_VXSERVICE_H_

#include <set>
#include <string>
#include <vector>
#include "ace/Signal.h"
#include "sigslot.h"
#include "cservice.h"
#include "servicemajor.h"
#include <boost/shared_ptr.hpp>
#include "controller.h"
#include "LogCutter.h"


const ACE_TCHAR SERVICE_NAME[]   = ACE_TEXT("InMage Scout Application Service");
const ACE_TCHAR SERVICE_TITLE[]  = ACE_TEXT("InMage Scout Application Service");
const ACE_TCHAR SERVICE_DESC[]   = ACE_TEXT("Helps in the discovery, protection and recovery of applications");

class AppService : public ACE_Event_Handler, public sigslot::has_slots<>
{
public:

    std::list < boost::shared_ptr < Logger::LogCutter > > m_LogCutters;

	enum WAIT_FOR_STATUS { WAIT_FOR_ERROR = -1, WAIT_FOR_SUCCESS = 0, WAIT_FOR_STOP_AGENTS_FOR_OFFLINE_RESOURCE = 1 };

    //typedef std::set<boost::shared_ptr<AgentProcess>, AgentProcess::LessAgentProcess> AgentProcesses_t;
    
    AppService ();
    ~AppService ();


    virtual int handle_exit (ACE_Process *proc);
    virtual int handle_signal(int signum,siginfo_t * = 0,ucontext_t * = 0);

    int parse_args(int argc, ACE_TCHAR* argv[]);
    void display_usage(char *exename = NULL);

    int install_service();
    int remove_service();
    int start_service();
    int stop_service();
    void name(const ACE_TCHAR *name, const ACE_TCHAR *long_name = NULL, const ACE_TCHAR *desc = NULL);
    int run_startup_code();
    static int StartWork(void *data);
    static int StopWork(void *data);

    const bool isInitialized() const;
	void CleanUp();
	void ApplicationMessageLoop();
	void StopProcesses();
	bool IsUninstallSignalled() const;
    void ApplicationSettingsCallback(ApplicationSettings settings) ;
    sigslot::signal1<std::map<SV_ULONG, TaskInformation> > m_applicationSchedSettingsSignal ;
    sigslot::signal1<std::map<SV_ULONG, TaskInformation> >& getApplicationSchedSettingsSignal() ;
protected:
    int init();
    bool initConfigurator();
    SVERROR initAppConfigurator();
    SVERROR initScheduler();
    SVERROR initController();
    SVERROR initDiscoveryController();
    SVERROR startAppControllers();
    SVERROR startAppConfigurator();
    SVERROR startScheduler();
    SVERROR startDiscoveryController();
    ACE_Thread_Manager* thrMgr() ;
	
private:
	void RunRecoveryCommand();
	SVERROR CompleteRecovery();
    bool QuitRequested(int waitTimeSeconds);
   	
public:

	int opt_install;
    int opt_remove;
    int opt_start;
    int opt_kill;	
    int quit;
private:

	bool m_bInitialized;
    bool m_bUninstall;
    
    time_t m_lastUpdateTime;
	
    ACE_Sig_Handler sig_handler_;
    
    ACE_Mutex m_AgentProcessesLock;
    ACE_Thread_Manager m_ThreadManager;
    ControllerPtr m_controllerPtr ;
    DiscoveryControllerPtr m_discoveryControllerPtr ; 
    AppAgentConfiguratorPtr m_configuratorPtr ;
	AppSchedulerPtr m_schedulerPtr ;
	std::set<std::string> m_selectedApplicationNameSet;
};

//Logger macros
#define LOG2SYSLOG(EVENT_MSG) \
    do { \
    ACE_LOG_MSG->set_flags(ACE_Log_Msg::SYSLOG);\
    ACE_LOG_MSG->open(SERVICE_NAME, ACE_Log_Msg::SYSLOG, NULL);\
    ACE_DEBUG(EVENT_MSG);\
    } while (0)

#define LOG2STDERR(EVENT_MSG) \
    do { \
    ACE_LOG_MSG->set_flags(ACE_Log_Msg::STDERR);\
    ACE_DEBUG(EVENT_MSG);\
    } while (0)

#define LOG_EVENT(EVENT_MSG) \
    do { \
    if (SERVICE::instance ()->isInstalled() == true) \
    LOG2SYSLOG(EVENT_MSG); \
        else \
        LOG2STDERR(EVENT_MSG); \
    } while (0)


#endif /* INMAGE_VXSERVICE_H */

