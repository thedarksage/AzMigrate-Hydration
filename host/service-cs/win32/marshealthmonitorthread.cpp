#include <boost/chrono.hpp>
#include "marshealthmonitorthread.h"
#include "logger.h"
#include "configwrapper.h"
#include "cdputil.h"

using namespace boost::chrono;

MarsHealthMonitorThread::MarsHealthMonitorThread(Configurator* configurator) :
  m_configurator(configurator),
  m_started(false),
  m_QuitEvent(new ACE_Event(1, 0))
{
}

MarsHealthMonitorThread::~MarsHealthMonitorThread()
{
}

void MarsHealthMonitorThread::Start()
{
  if (m_started)
  {
      return;
  }
  m_QuitEvent->reset();
  m_threadManager.spawn( ThreadFunc, this );
  m_started = true;
}

ACE_THR_FUNC_RETURN MarsHealthMonitorThread::ThreadFunc(void* arg)
{
    MarsHealthMonitorThread* p = static_cast<MarsHealthMonitorThread*>(arg);
    return p->run();
}

ACE_THR_FUNC_RETURN MarsHealthMonitorThread::run()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string hostId = m_configurator->getVxAgent().getHostId();
    ACE_Time_Value waitTime(m_configurator->getVxAgent().getMarsHealthCheckInterval(), 0);

    do
    {
        std::list<HRESULT> healthIssues;
        std::string emptyStr;
        AzureAgentInterface::AzureAgent::ptr azureagent_ptr;

        try
        {
            azureagent_ptr.reset(new AzureAgentInterface::AzureAgent(
                m_configurator->GetEndpointHostName(hostId),
                m_configurator->GetEndpointHostId(hostId),
                emptyStr,
                0,
                0,
                emptyStr,
                (AzureAgentInterface::AzureAgentImplTypes) m_configurator->getVxAgent().getAzureImplType(),
                m_configurator->getVxAgent().getMaxAzureAttempts(),
                m_configurator->getVxAgent().getAzureRetryDelayInSecs()));

            healthIssues = azureagent_ptr->CheckMarsAgentHealth();
        }
        catch (std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "\nCaught an exception %s while checking for Mars Agent Health Issues.\n", e.what());
            // COM initialization tries for 10 times in each 1 minute gap. If it still fails in all 10 retries, then
            // reporting it as service unavailable health issue for MARS agent.
            healthIssues.push_back(HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE));
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "\nCaught a generic exception while checking for Mars Agent Health Issues.\n");
        }

        std::list<HRESULT>::iterator it;
        it = std::find(healthIssues.begin(), healthIssues.end(), HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE));

        if (it != healthIssues.end())
        {
            // Server unavailable error is exist, retrying for another 15 minutes for the service availability.
            steady_clock::time_point nextMonitorTime = steady_clock::now() +
                seconds(m_configurator->getVxAgent().getMarsServerUnavailableCheckInterval());

            while ((steady_clock::now() < nextMonitorTime) && !CDPUtil::QuitRequested(0))
            {
                try
                {
                    // Trying 3 times in 60 sec interval. Sometimes observed that COM initialize failure is taking nearly 5 minutes.
                    //Already we placed the maximum time to monitor the failure to 15 minutes, So in any case it will keep on retry for at least 15 minutes.
                    int maxAttempts = 3; 
                    azureagent_ptr.reset(new AzureAgentInterface::AzureAgent(
                        m_configurator->GetEndpointHostName(hostId),
                        m_configurator->GetEndpointHostId(hostId),
                        emptyStr,
                        0,
                        0,
                        emptyStr,
                        (AzureAgentInterface::AzureAgentImplTypes) m_configurator->getVxAgent().getAzureImplType(),
                        maxAttempts,
                        m_configurator->getVxAgent().getAzureRetryDelayInSecs()));

                    // Azure agent reset confirms that MARS service is in good state.
                    // So removing the earlier Server unavailable error from the detected list of errors.
                    // and updating the remaining errors to CS.
                    // If during this run, if any new errors were present in MARS then those will be get detected in the next run.
                    // So not calling the API to check for current errors again.
                    healthIssues.erase(it);
                    break;
                }
                catch (std::exception& e)
                {
                    DebugPrintf(SV_LOG_ERROR, "\nCaught an exception %s while checking for Mars Agent Health Issues.\n", e.what());
                }
                catch (...)
                {
                    DebugPrintf(SV_LOG_ERROR, "\nCaught a generic exception while checking for Mars Agent Health Issues.\n");
                }
            }
        }

        DebugPrintf(SV_LOG_INFO, "Reporting below Mars Health issues to CS: \n");
        for (std::list<HRESULT>::iterator hr = healthIssues.begin(); hr != healthIssues.end(); hr++)
        {
            DebugPrintf(SV_LOG_INFO, "\thr = %#x.\n", hr);
        }

        m_configurator->getVxAgent().ReportMarsAgentHealthStatus(healthIssues);
        m_QuitEvent->wait(&waitTime, 0);
    } while (!m_threadManager.testcancel(ACE_Thread::self()));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return 0;
}

void MarsHealthMonitorThread::Stop()
{
    m_threadManager.cancel_all();
    m_QuitEvent->signal();
    m_threadManager.wait();
    m_started = false;
}