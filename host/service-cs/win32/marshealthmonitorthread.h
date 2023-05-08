#ifndef MARSHEALTHMONITORTHREAD_H
#define MARSHEALTHMONITORTHREAD_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>

#include <boost/shared_ptr.hpp>

#include "inmquitfunction.h"

#include "sigslot.h"
#include "rpcconfigurator.h"
#include "azureagent.h"

class MarsHealthMonitorThread : public sigslot::has_slots<>
{
public:
    MarsHealthMonitorThread(Configurator* configurator);
    ~MarsHealthMonitorThread();

    void Start();
    void Stop();

protected:
    ACE_Thread_Manager m_threadManager;
    static ACE_THR_FUNC_RETURN ThreadFunc(void* pArg);

    ACE_THR_FUNC_RETURN run();

private:
    Configurator*                   m_configurator;
    boost::shared_ptr<ACE_Event>    m_QuitEvent;
    bool                            m_started;
};

#endif /* MARSHEALTHMONITORTHREAD_H */
