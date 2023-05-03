#include "registerhostthread.h"
#include "logger.h"
#include "portablehelpers.h"
#include "inmquitfunction.h"

#include <boost/bind.hpp>

RegisterHostThread::RegisterHostThread(unsigned int registerHostInterval, 
  unsigned int eventPollInterval):
  m_started(false),
  m_QuitEvent(new ACE_Event(1, 0)),
  m_EventPollInterval(eventPollInterval),
  m_RegisterHostInterval(registerHostInterval)
{
    m_prevRegisterTime = steady_clock::time_point::min();
}

RegisterHostThread::~RegisterHostThread()
{
}

void RegisterHostThread::Start()
{
  if (m_started)
  {
      return;
  }    
  m_QuitEvent->reset();
  m_threadManager.spawn( ThreadFunc, this );
  m_started = true;
}

ACE_THR_FUNC_RETURN RegisterHostThread::ThreadFunc( void* arg )
{
  RegisterHostThread* p = static_cast<RegisterHostThread*>( arg );
  return p->run();
}

ACE_THR_FUNC_RETURN RegisterHostThread::run()
{
  ACE_Time_Value waitTime(m_EventPollInterval, 0);
  while( true )
  {
    ///needed until the cx has a way to request the host to re-register
    RegisterHostIfNeeded();
    m_QuitEvent->wait(&waitTime,0);
    if( m_threadManager.testcancel( ACE_Thread::self() ) )
    {
      break;
    }
  }
  return 0;
}

void RegisterHostThread::Stop()
{
  m_threadManager.cancel_all();
  m_QuitEvent->signal();
  m_threadManager.wait();
  m_started = false;
}

void RegisterHostThread::SetRegisterImmediatelyCallback(std::string requestId, std::string disksLayoutOption)
{
  DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with requestId: %s, disksLayoutOption: %s\n", FUNCTION_NAME, requestId.c_str(), disksLayoutOption.c_str());
  SetRequest(requestId, disksLayoutOption);
  DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/* Accepts new registration request if it's different from the one under progress, if any.
 * This method doesn't accept the request if the same request is being processed by 
 * this RegisterHostThread */
void RegisterHostThread::SetRequest(const std::string & requestId, const std::string & disksLayoutOption)
{
  ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_RegisterImmediatePropertyLock);
  if (m_strCurrentRequestId != requestId)
  {
    m_strNewRequestId = requestId;
    m_strNewDisksLayoutOption = disksLayoutOption;
  }
}

/* If there is an outstanding new request, first accept the request by loading it
 * into m_strCurrentRequestId and then clears off m_strNewRequestId to avoid duplicate
 * processing. Returns true if there is an outstaning new request to be processed*/
bool RegisterHostThread::PopRequest()
{
  ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_RegisterImmediatePropertyLock);
  bool popped = false;
  m_strCurrentRequestId = m_strNewRequestId;
  m_strCurrentDisksLayoutOption = m_strNewDisksLayoutOption;
  if (!m_strNewRequestId.empty())
  {
    m_strNewRequestId.clear();
    m_strNewDisksLayoutOption.clear();
    popped = true;
  }
  return popped;
}

void RegisterHostThread::SetLastRegisterHostTime(void)
{
  ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_RegisterHostTimeLock);
  m_prevRegisterTime = steady_clock::now();
}

void RegisterHostThread::RegisterImmediatelyIfNeeded(void)
{
  if (PopRequest())
  {
    RegisterHost(m_strCurrentRequestId, m_strCurrentDisksLayoutOption);
    SetLastRegisterHostTime();
  }
}

// TODO:
// this is needed in case a db clean up is done on the cx (which wipes out all volume information).
// once we have a way for the cx to request hosts to re-register this will no longer be needed
// NOTE: volume monitor is used to request RegisterHost when there are changes to any volume
void RegisterHostThread::RegisterHostIfNeeded()
{
  RegisterImmediatelyIfNeeded();

  steady_clock::time_point currentTime = steady_clock::now();
  steady_clock::duration retryInterval = seconds(m_RegisterHostInterval);
  if (currentTime > m_prevRegisterTime + retryInterval)
  {
    QuitFunction_t qf = std::bind1st(std::mem_fun(&RegisterHostThread::QuitRequested), this);
    DebugPrintf( SV_LOG_DEBUG,"RegisterHost if needed\n" );
    RegisterHost(std::string(), std::string(), qf);
    SetLastRegisterHostTime();
  }
}

bool RegisterHostThread::QuitRequested(int seconds)
{
    ACE_Time_Value waitTime(seconds, 0);
    m_QuitEvent->wait(&waitTime, 0);
    return m_threadManager.testcancel(ACE_Thread::self());
}