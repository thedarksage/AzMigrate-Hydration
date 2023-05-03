//#pragma once

#ifndef SYNCHRONIZE__H
#define SYNCHRONIZE__H

#include "ace/Thread_Mutex.h"
#include "ace/Recursive_Thread_Mutex.h"


class Lockable;

class AutoLock
{
private:
    AutoLock();
    AutoLock& operator=(const AutoLock&);
    Lockable& m_Lockable;
public:
    AutoLock(Lockable&);
    ~AutoLock();
};


class Lockable
{
public:
	Lockable();
	virtual ~Lockable();
private:
	//ACE_Thread_Mutex m_CriticalSection;
    ACE_Recursive_Thread_Mutex m_CriticalSection;
	bool m_bLocked;
	void Lock();
	void Unlock();
    friend class AutoLock;
public:
	bool IsLocked() const;

};


#endif

