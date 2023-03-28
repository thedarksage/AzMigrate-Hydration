#ifndef _INMAGE_SCOPE_SPINLOCK_H_
#define _INMAGE_SCOPE_SPINLOCK_H_

// this class will automatically lock a spin lock on scope entry and unlock on exit
class ScopeSpinLock {
public:
    KIRQL oldIrql;
    PKSPIN_LOCK theLock;

    __inline ScopeSpinLock(PKSPIN_LOCK objLock)
    { theLock = objLock; 
      KeAcquireSpinLock(objLock, &oldIrql); }
    __inline ~ScopeSpinLock()
    { KeReleaseSpinLock(theLock, oldIrql); }
};

#endif // _INMAGE_SCOPE_SPINLOCK_H_