#include "boost/thread.hpp"
#include "IPlatformAPIs.h"
#include <iostream>


Event::Event() :m_bSignalled(false), m_sEventName("")
{
}
Event::Event(bool bInitialState, bool bAutoReset, const std::string& strName)
    : m_bInitialState(bInitialState),
    m_bAutoReset(bAutoReset),
    m_bSignalled(bInitialState),
    m_sEventName(strName)
{
    //Windows equivalent is CreateEvent()
}

Event::Event(bool bInitialState, bool bAutoReset, const char* name, int iEventType)
    : m_bInitialState(bInitialState),
    m_bAutoReset(bAutoReset),
    m_bSignalled(bInitialState),
    m_sEventName(name)
{
    m_hEvent = this;
}

Event::~Event()
{
}

bool Event::operator()(int secs)
{
    WAIT_STATE waitRet = WAIT_TIMED_OUT;
    if (IsSignalled())
    {
        waitRet = WAIT_SUCCESS;
    }
    else if (secs)
    {
        waitRet = Wait(secs);
    }
    m_hEvent = this;
    return (waitRet == WAIT_SUCCESS);
}

void Event::Signal(bool newState)
{
    //Windows equivalent is SetEvent()
    boost::mutex::scoped_lock lock(m_mutex);
    m_bSignalled = newState;
    m_condition.notify_all();
}

void Event::ResetEvent()
{
    boost::mutex::scoped_lock lock(m_mutex);
    m_bSignalled = m_bInitialState;

}

const bool Event::IsSignalled() const
{
    return m_bSignalled;
}

const void* Event::Self() const
{
    return m_hEvent;
}

const std::string& Event::GetEventName() const
{
    return m_sEventName;
}

void Event::SetEventName(const std::string& strEventName)
{
    m_sEventName = strEventName;

}

WAIT_STATE Event::Wait(long int seconds, long int liMilliSeconds)
{
    boost::mutex::scoped_lock lock(m_mutex);
    if (!m_bSignalled) {
        bool bWait = m_condition.timed_wait(
            lock, boost::posix_time::milliseconds((seconds * 1000) + liMilliSeconds));
        if (bWait) {
            return WAIT_SUCCESS;
        } else {
            return WAIT_TIMED_OUT;
        }
    }
}
