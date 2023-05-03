#include <exception>

#include "synchronize.h"
#include "portable.h"


Lockable::Lockable():m_bLocked(false)
{

}

Lockable::~Lockable()
{
    m_CriticalSection.remove();
}

void Lockable::Lock()
{
		// Request ownership of the critical section.
    if ( m_CriticalSection.acquire() >= 0 )
    {
    	m_bLocked = true;
    }
    else
    {
    	m_bLocked = false;
    }
}

void Lockable::Unlock()
{
		// Release ownership of the critical section.
    if ( m_CriticalSection.release() >= 0 )
    {
    	m_bLocked = false;
    }
    else
    {
    	m_bLocked = true;
    }
}

bool Lockable::IsLocked() const
{
	return m_bLocked;
}

AutoLock::AutoLock(Lockable& lockable):m_Lockable(lockable)
{
    m_Lockable.Lock();
}

AutoLock::~AutoLock()
{
    m_Lockable.Unlock();
}

