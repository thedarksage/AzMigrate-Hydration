#ifndef REGISTERHOSTTHREAD_H
#define REGISTERHOSTTHREAD_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>

#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>

#include "sigslot.h"

using namespace boost::chrono;

class RegisterHostThread : public sigslot::has_slots<>
{
public:
    RegisterHostThread(unsigned int registerHostInterval, unsigned int eventPollInterval);
    ~RegisterHostThread();
    void Start();
    void Stop();
    void SetRegisterImmediatelyCallback(std::string requestId, std::string disksLayoutOption);
    void SetLastRegisterHostTime(void);

    /// \brief waits for quit event for given seconds and returns true if quit is set (even before timeout expires).
    bool QuitRequested(int seconds); ///< seconds

protected:
    ACE_Thread_Manager m_threadManager;
    static ACE_THR_FUNC_RETURN ThreadFunc( void* pArg );
    ACE_THR_FUNC_RETURN run();
    void RegisterImmediatelyIfNeeded(void);
    void RegisterHostIfNeeded();
    bool PopRequest();
    void SetRequest(const std::string & requestId, const std::string &disksLayoutOption);
private:

    boost::shared_ptr<ACE_Event> m_QuitEvent;
    bool m_started;
    ACE_Mutex m_RegisterImmediatePropertyLock;
    ACE_Mutex m_RegisterHostTimeLock;

    // RequestId currently being processed by RegisterHostThread
    std::string m_strCurrentRequestId;
    std::string m_strCurrentDisksLayoutOption;
    // RequestId to accept from cx
    std::string m_strNewRequestId;
    std::string m_strNewDisksLayoutOption;
    unsigned int m_EventPollInterval;
    unsigned int m_RegisterHostInterval;

    steady_clock::time_point m_prevRegisterTime;

};

#endif /* REGISTERHOSTTHREAD_H */
