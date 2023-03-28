#include <string>

#include "IPlatformAPIs.h"


//Lockable Event::m_DestroyLock;

Event::Event() : m_bSignalled(false),
m_sEventName("")
{

}

Event::~Event()
{
	Close();
}

Event::Event(bool bInitialState,
	bool bAutoReset,
	const std::string& sName)
	:m_bSignalled(bInitialState),
	m_sEventName(sName)
{
	int iInitialState = (true == bInitialState ? 1 : 0);
	m_bAutoReset = bAutoReset;
	m_bInitialState = bInitialState;

	BOOL bManualReset = (bAutoReset) ? FALSE : TRUE;
	BOOL bInitState = (bInitialState) ? TRUE : FALSE;

	// PR#10815: Long Path support
	m_hEvent = CreateEvent(NULL, bManualReset, bInitialState, NULL);
	m_bSignalled = bInitialState;
}

Event::Event(bool bInitialState,
	bool bAutoReset,
	const char *name,
	int iEventType)
	:m_bSignalled(bInitialState)
{
	int iInitialState = (true == bInitialState ? 1 : 0);
	m_bAutoReset = bAutoReset;
	m_bInitialState = bInitialState;
	BOOL bManualReset = (bAutoReset) ? FALSE : TRUE;
	BOOL bInitState = (bInitialState) ? TRUE : FALSE;

	// PR#10815: Long Path support
	m_hEvent = CreateEvent(NULL, bManualReset, bInitialState, NULL);

	if (name)
	{
		m_sEventName = name;
	}
}

WAIT_STATE Event::Wait(long int liSeconds, long int liMilliSeconds)
{
	long int liStatus = 0;

	long int totalWaitTime = liSeconds * 1000 + liMilliSeconds;
	unsigned long dwStatus = WaitForSingleObject(m_hEvent, totalWaitTime);

	if (WAIT_OBJECT_0 == dwStatus)
	{
		return WAIT_SUCCESS;
	}

	if (WAIT_TIMEOUT == dwStatus)
	{
		return WAIT_TIMED_OUT;
	}

	return WAIT_FAILURE;
}

void Event::SetEventName(const std::string& sName)
{
	m_sEventName = sName;
}

const std::string& Event::GetEventName() const
{
	return m_sEventName;
}

void Event::Close()
{
	if (INVALID_HANDLE_VALUE != m_hEvent)
	{
		CloseHandle(m_hEvent);
		m_hEvent = INVALID_HANDLE_VALUE;
	}
}

void Event::Signal(bool bState)
{
	m_bSignalled = bState;
	(m_bSignalled == true ? SetEvent(m_hEvent) : ResetEvent(m_hEvent));
}

const bool Event::IsSignalled() const
{
	return m_bSignalled;
}

bool Event::operator()(int secs)
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

const void* Event::Self() const
{
	return m_hEvent;
}
