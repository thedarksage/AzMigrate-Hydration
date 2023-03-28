#include "ace/Reactor.h"
#include "ace/Mutex.h"
#include "ace/Null_Mutex.h"
#include "ace/Null_Condition.h"
#include "ace/OS_NS_unistd.h"
#include "ace/os_include/os_netdb.h"

#include "ClientAcceptor.h"
#include "CxCommunicator.h"

#include "configurator2.h"
#include "logger.h"
#include "inmageex.h"

static const u_short PORT = 20003;
#include <ace/Signal.h>
#include <ace/streams.h>
#include <ace/Thread_Manager.h>
#include <ace/TP_Reactor.h>


//Configurator* cxConfigurator = NULL;

/**
* This is the function run by all threads in the thread pool.
*
* @param arg is expected to be of type (ACE_Reactor *)
*/
ACE_THR_FUNC_RETURN threadFunc(void *arg) 
{
    DebugPrintf("Pooling Acceptor Thread.\n");
    ACE_Reactor *reactor = (ACE_Reactor *) arg;
    reactor->owner (ACE_OS::thr_self ());
    reactor->run_reactor_event_loop();

    return 0;
}

class SignalableTask : public ACE_Event_Handler
{
public:
  SignalableTask(ACE_Thread_Manager *amanager, int gp_id_to_signal, ACE_Reactor *r = 0 ):tm(amanager), gp_id(gp_id_to_signal),reactor(r) {}
  virtual int handle_signal (int signum, siginfo_t *  = 0, ucontext_t * = 0)
  {
    if (signum == SIGTERM)
    {
	DebugPrintf("Stopping All threads. Main thread received signal %d\n", signum);
	if (reactor)
		reactor->end_reactor_event_loop();
	tm -> kill_grp (gp_id, signum);
    }
    return 0;
  }
private:
	ACE_Thread_Manager *tm;
	int gp_id;
	ACE_Reactor *reactor;
};

void InitializeLogger()
{
    LocalConfigurator localConfigurator;
    std::string logPathname = localConfigurator.getLogPathname();
    logPathname += "vacps.log";
    SetLogFileName( logPathname.c_str() );
    SetDaemonMode();
    SetLogLevel( localConfigurator.getLogLevel() );
    SetLogHttpIpAddress( localConfigurator.getHttp().ipAddress );
    SetLogHttpPort( localConfigurator.getHttp().port );
    SetLogHostId( localConfigurator.getHostId().c_str() );
    SetLogRemoteLogLevel( localConfigurator.getRemoteLogLevel() );
}

int ACE_TMAIN(int argc, char ** argv)
{
    ACE::daemonize( ACE_TEXT ("/"), 1 /* close handles */ );
    SV_USHORT nThreads = 2;

    try
    {
        MaskRequiredSignals();
        InitializeLogger();
        LocalConfigurator localconfigurator;
        nThreads = localconfigurator.getMaxVacpServiceThreads();
	if (nThreads < 1 ) 
             nThreads = 2;
	if (nThreads > 64 ) 
             nThreads = 64;
    }
    catch(...)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("(%P|%t) Unable to Initialize the Logger/LocalConfigurator...\n")));
        return -1;
    }


    // create a reactor from a TP reactor
    ACE_TP_Reactor tpReactor;
    ACE_Reactor reactor(&tpReactor);

    CxCommunicator cxCommunicator(ACE_Thread_Manager::instance());
    cxCommunicator.open();
    // create a new accept handler using that reactor
    ClientAcceptor *acceptHandler = 0;
    ACE_NEW_NORETURN (acceptHandler, ClientAcceptor(cxCommunicator, &reactor, SIGINT));
    if (acceptHandler == 0)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to allocate acceptor handler. Error = %d\n", errno);
        return -1;
    }

    ACE_INET_Addr port_to_listen (PORT);

    // open the accept handler
    if (acceptHandler->open(port_to_listen) == -1) 
    {
        delete acceptHandler;
        DebugPrintf(SV_LOG_ERROR, "Failed to open accept handler. Exiting... \n");
        return -1;
    }

    // spawn some threads which run the reactor event loop(s)
    ACE_Thread_Manager::instance()->spawn_n(nThreads, threadFunc, &reactor);

 //   reactor.register_handler (&cxCommunicator, ACE_Event_Handler::ACCEPT_MASK);
    //SignalableTask st(ACE_Thread_Manager::instance(), cxCommunicator.grp_id(), &reactor);
    //reactor.register_handler (SIGTERM, &st);

    // let the thread manager wait for all threads
    ACE_Thread_Manager::instance()->wait();
    
    return (0);
}

