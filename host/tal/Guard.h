#ifndef GUARD__H
#define GUARD__H

#include <string>
//**added by BSR to fix the issue#3292
//#include <ace/Auto_Event.h>
#include <ace/Manual_Event.h>
//** End of change issue#3292

#include <ace/Thread_Mutex.h>
#include <ace/Time_Value.h>

using std::string;

namespace tal 
{
class Mutex
{

private:
	ACE_Thread_Mutex m_hMutex;

public:
	Mutex()	
	{		
						
	}
	
	void Acquire()
	{	
		m_hMutex.acquire();
		
	}
	~Mutex()
	{		
		
	}		
	void Release()
	{	
		m_hMutex.release();			
	}
};

class NoMutex
{
public:
	NoMutex(string name)	
	{		
		
	}
	
	void Acquire()
	{	
		
	}
/*
	bool Mutex::TryAcquire()
	{		
		return (Thread::waitThread(_mutex, 0) == WAIT_OBJECT_0);	
	}
*/
	~NoMutex()
	{		
		
	}
		
	void Release()
	{	
		
	}
	
}; 

class Event
{

private:		
	//**added by BSR to fix the issue#3292
	//ACE_Auto_Event m_hEvent;	
	ACE_Manual_Event m_hEvent ;
	//** End of change issue#3292
public:
	Event()	
	{	
					
	}
	
	int Wait()
	{	
		//**added by BSR to fix the issue#3292
		int nResult = m_hEvent.wait();
		//return m_hEvent.wait();	
		SetNonSignalState();
		return nResult ;
		//** End of change issue#3292
	}

	int Wait(long nSeconds)//Time in milli seconds
	{
		ACE_Time_Value Time;
        Time.msec(nSeconds);
		//**added by BSR to fix the issue#3292
		//return m_hEvent.wait(&Time,0);	
		int nResult = m_hEvent.wait(&Time,0);
		SetNonSignalState() ;
		return nResult ;
		//** End of change issue#3292
	}

	void SetNonSignalState()
	{
        m_hEvent.reset();
	}

	~Event()
	{		
		
	}
		
	void SetSignalState()
	{	
        m_hEvent.signal();		
	}
	
};

template<class LOCK> 
class Guard
{
private :
	LOCK& m_lock;

public:
	Guard(LOCK &lock) : m_lock(lock)	
	{
		m_lock.Acquire();
	}

	~Guard()
	{
		m_lock.Release();
	}

};
};//namespace
#endif


