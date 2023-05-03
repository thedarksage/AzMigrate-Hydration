#ifndef LOCK__H
#define LOCK__H

#include <windows.h>

class SimpleLock {
public:
    SimpleLock()  { InitializeCriticalSection(&m_CriticalSection); }
    ~SimpleLock() { DeleteCriticalSection(&m_CriticalSection); }
    void Lock()   { EnterCriticalSection(&m_CriticalSection); }
    void Unlock() { LeaveCriticalSection(&m_CriticalSection); }
    
protected:

private:
    CRITICAL_SECTION m_CriticalSection;    

};

class LockGuard
{
public:
    LockGuard(SimpleLock& lock) : m_Lock(lock) { m_Lock.Lock(); }
    ~LockGuard() { m_Lock.Unlock(); }

private:
    SimpleLock& m_Lock;
};

#endif // LOCK__H
