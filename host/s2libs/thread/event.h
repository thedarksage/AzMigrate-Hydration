//#pragma once

#ifndef EVENT__H
#define EVENT__H

#include <string>
#include "boost/shared_ptr.hpp"
#include <boost/thread/mutex.hpp>
#include <ace/Event.h>
#include <ace/Time_Value.h>
#include <ace/OS.h>

#include "inmsafecapis.h"

#define AGENT_QUITEVENT_PREFIX		"InMageAgent"

enum WAIT_STATE
{
    WAIT_SUCCESS=0,
    WAIT_TIMED_OUT=1,
    WAIT_FAILURE=2
};

class Event
{
public:
    Event(const bool bInitialState, const bool bAutoReset, const std::string& sName = std::string()) :
            m_bInitialState(bInitialState ? 1 : 0),
            m_bSignalled(bInitialState),
            m_bAutoReset(bAutoReset),
            m_sEventName(sName)
    {
        int iEventType = sName.empty() ? USYNC_THREAD : USYNC_PROCESS;
        m_hEvent.reset(new ACE_Event(!m_bAutoReset, m_bInitialState, iEventType, ACE_TEXT_CHAR_TO_TCHAR(sName.c_str()), 0));
    }

    Event(const bool bInitialState, const bool bAutoReset, const char *name, const int iEventType) :
            m_bInitialState(bInitialState ? 1 : 0),
            m_bSignalled(bInitialState),
            m_bAutoReset(bAutoReset)
    {
        m_hEvent.reset(new ACE_Event(!m_bAutoReset, m_bInitialState, iEventType, ACE_TEXT_CHAR_TO_TCHAR(name), 0));
        if (name) m_sEventName = name;
    }

    bool operator()(const int secs)
    {
        WAIT_STATE waitRet = WAIT_TIMED_OUT;

        /* checking for nonzero secs to
        * pass to wait since for 0,
        * wait for one year is happening */
        if (IsSignalled())
        {
            waitRet = WAIT_SUCCESS;
        }
        else if (secs)
        {
            waitRet = Wait(secs);
        }
        return (waitRet == WAIT_SUCCESS);
    }
    
    void Signal(const bool bState)
    {
        m_bSignalled = bState;
        m_bSignalled ? m_hEvent->signal() : m_hEvent->reset();
    }

    const bool IsSignalled() const
    {
        return m_bSignalled;
    }

    const void* Self() const
    {
#ifdef SV_WINDOWS
        if (m_hEvent != NULL)
            return static_cast<HANDLE>(m_hEvent->handle());
        return NULL;
#else
        return m_hEvent.get();
#endif
    }
    
    const std::string& GetEventName() const
    {
        return m_sEventName;
    }

    void SetEventName(const std::string& sName)
    {
        m_sEventName = sName;
    }
    
    WAIT_STATE Wait(const long int liSeconds, const long int liMilliSeconds = 0)
    {
        long int liStatus = 0;
        ACE_Time_Value time_value;
        if (m_hEvent)
        {
            if ((0 == liSeconds) && (0 == liMilliSeconds))
                // Wait for a year !!
                time_value.set((3600 * 24 * 365), 0);
            else
                time_value.set(liSeconds, (liMilliSeconds * 1000));

            if (0 == m_hEvent->wait(&time_value, 0))
            {
                // Event state is signalled
                m_bSignalled = true; //Set this value so that if svagents signals this event, the IsSignalled function returns true.
                return WAIT_SUCCESS;
            }

            if (errno == ETIME)
            {
                // Event state is non-signalled.
                return WAIT_TIMED_OUT;
            }


        }
        return WAIT_FAILURE;
    }

    ~Event() {}

private:
    Event() : m_bSignalled(false), m_sEventName("") {}

private:
	boost::shared_ptr<ACE_Event> m_hEvent;
    bool m_bSignalled;
    bool m_bAutoReset;
    bool m_bInitialState;
    std::string m_sEventName;
};

class QuitEvent
{
public:
    QuitEvent()
    {
        if (m_Quit == NULL)
        {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            if (m_Quit == NULL)
            {
                char EventName[256];
                m_ProcessId = ACE_OS::getpid();
                if (ACE_INVALID_PID != m_ProcessId)
                {
                    inm_sprintf_s(EventName, ARRAYSIZE(EventName), "%s%d", AGENT_QUITEVENT_PREFIX, m_ProcessId);
                    m_Quit.reset(new Event(false, false, EventName));
                }
            }
        }
    }

    void Signal(const bool bState)
    {
        if (!m_Quit->IsSignalled())
        {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            if (!m_Quit->IsSignalled())
            {
                m_Quit->Signal(true);
            }
        }
    }

    bool QuitSignalled()
    {
        return m_Quit->IsSignalled();
    }

    bool QuitRequested(int waitTimeSeconds = 0 )
    {
        return (WAIT_SUCCESS == Wait(waitTimeSeconds));
    }

private:
    WAIT_STATE Wait(const int iSeconds)
    {
        WAIT_STATE waitRet = WAIT_TIMED_OUT;
        if (m_Quit->IsSignalled())
        {
            waitRet = WAIT_SUCCESS;
        }
        boost::unique_lock<boost::mutex> lock(m_mutex);
        waitRet = m_Quit->Wait(iSeconds);
        return waitRet;
    }

private:
    pid_t m_ProcessId;
    boost::shared_ptr<Event> m_Quit;
    boost::mutex m_mutex;
};

#endif

