
/************************************************************************
 * Copyright 2005 InMage Systems.                                       *
 * This file contains proprietary and confidential information and      *
 * remains the unpublished property of InMage. Use, disclosure,         *
 * or reproduction is prohibited except as permitted by express         *
 * written license aggreement with InMage.                              *
 *                                                                      *
 * File : 
 * Desc :                                                               *
 ************************************************************************/

#ifndef _INM_LOCKS_H
#define _INM_LOCKS_H

#include <sys/ksynch.h>
#include <sys/atomic.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/semaphore.h>

typedef kmutex_t inm_mutex_t;
typedef kmutex_t inm_spinlock_t;
typedef krwlock_t  inm_rwlock_t;
typedef krwlock_t  inm_rwsem_t;
typedef ksema_t inm_sem_t;
typedef inm_u32_t inm_atomic_t;

/* mutex lock apis */
#define INM_MUTEX_INIT(lock)            mutex_init(lock, NULL, MUTEX_ADAPTIVE, NULL)
#define INM_MUTEX_DESTROY(lock)         mutex_destroy(lock)
#define INM_MUTEX_LOCK(lock)            mutex_enter(lock)
#define INM_MUTEX_UNLOCK(lock)          mutex_exit(lock)

/* condition variables */
#define INM_CV_INIT(cvp)                cv_init(cvp, NULL, CV_DRIVER, NULL)
#define INM_CV_DESTROY(cvp)		cv_destroy(cvp)

/* spin lock api */
#define INM_INIT_SPIN_LOCK(lock)        mutex_init(lock, NULL, MUTEX_SPIN, NULL)
#define INM_DESTROY_SPIN_LOCK(lock)	mutex_destroy(lock)
#define INM_SPIN_LOCK(lock)             mutex_enter(lock)
#define INM_SPIN_UNLOCK(lock)           mutex_exit(lock)
#define INM_SPIN_LOCK_BH(lock)          mutex_enter(lock)
#define INM_SPIN_UNLOCK_BH(lock)        mutex_exit(lock)
#define INM_SPIN_LOCK_IRQSAVE(lock, flag) \
                                        mutex_enter(lock)
#define INM_SPIN_UNLOCK_IRQRESTORE(lock, flag) \
                                        mutex_exit(lock)

#define INM_SPIN_LOCK_WRAPPER(lock, flag) INM_SPIN_LOCK(lock)
#define INM_SPIN_UNLOCK_WRAPPER(lock, flag) INM_SPIN_UNLOCK(lock)

#define INM_VOL_LOCK(lock, flag)	\
			INM_SPIN_LOCK_IRQSAVE(lock, flag)
#define INM_VOL_UNLOCK(lock, flag)	\
			INM_SPIN_UNLOCK_IRQRESTORE(lock, flag)
			
/* read write spin lock apis
     The rw_init() function initializes a readers/writer lock. It
     is  an  error  to initialize a lock more than once. The type
     argument should be set to RW_DRIVER. If the lock is used  by
     the  interrupt  handler,  the  type-specific  argument, arg,
     should   be   the   interrupt   priority    returned
     from ddi_intr_get_pri(9F)  or ddi_intr_get_softint_pri(9F). Note
     that arg should be the value of the interrupt priority  cast
     by  calling  the DDI_INTR_PRI macro. If the lock is not used
     by any interrupt handler, the argument should be NULL.
*/

/* Need to make initializations interrupt handling aware
   This implementation is not complete and needs for investigation
#define INM_INIT_RW_SPIN_LOCK(lock)     rw_init(lock, NULL, RW_DRIVER, NULL)

#define INM_READ_SPIN_LOCK(rwlock)      rw_enter(rwlock, RW_READER)
#define INM_READ_SPIN_UNLOCK(rwlock)    rw_exit(rwlock)
#define INM_WRITE_SPIN_LOCK(rwlock)     rw_enter(rwlock, RW_WRITER)
#define INM_WRITE_SPIN_UNLOCK(rwlock)   rw_exit(rwlock)


#define INM_READ_SPIN_LOCK_BH(rwlock)   rw_enter(rwlock, RW_READER)
#define INM_READ_SPIN_UNLOCK_BH(rwlock) rw_exit(rwlock)
#define INM_WRITE_SPIN_LOCK_BH(rwlock)  rw_enter(rwlock, RW_WRITER)
#define INM_WRITE_SPIN_UNLOCK_BH(rwlock) \
                                        rw_exit(rwlock)

#define INM_READ_SPIN_LOCK_IRQSAVE(rwlock, flag) \
                                        rw_enter(rwlock, RW_READER)
#define INM_READ_SPIN_UNLOCK_IRQRESTORE(rwlock, flag) \
                                        rw_exit(rwlock)
#define INM_WRITE_SPIN_LOCK_IRQSAVE(rwlock, flag)  \
                                        rw_enter(rwlock, RW_WRITER)
#define INM_WRITE_SPIN_UNLOCK_IRQRESTORE(rwlock, flag)  \
                                        rw_exit(rwlock)
*/

/* semahore apis */
#define INM_INIT_SEM(sem)               sema_init(sem, 1, NULL, SEMA_DRIVER, NULL)
#define INM_INIT_SEM_LOCKED(sem)        sema_init(sem, 0, NULL, SEMA_DRIVER, NULL)
#define INM_DESTROY_SEM(sem)		sema_destroy(sem)

#define INM_DOWN(sem)                   sema_p(sem)
#define INM_DOWN_INTERRUPTIBLE(sem)     sema_p(sem)
#define INM_DOWN_TRYLOCK(sem)           sema_tryp(sem)
#define INM_UP(sem)                     sema_v(sem)

/* read write locks to fill up the rw semaphore on linux */

#define INM_RW_SEM_INIT(rw_sem)         rw_init(rw_sem, NULL, RW_DRIVER, NULL)
#define INM_RW_SEM_DESTROY(rw_sem)	rw_destroy(rw_sem);
#define INM_DOWN_READ(rw_sem)           rw_enter(rw_sem, RW_READER)
/*#define INM_READ_DOWN_TRYLOCK(rw_sem)   read_down_trylock(rw_sem) */
#define INM_UP_READ(rw_sem)             rw_exit(rw_sem)
#define INM_DOWN_WRITE(rw_sem)          rw_enter(rw_sem, RW_WRITER)
#define INM_UP_WRITE(rw_sem)            rw_exit(rw_sem)
#define INM_DOWNGRADE_WRITE(rw_sem)     rw_downgrade(rw_sem)

/* atomic operations */
#define INM_ATOMIC_SET(var, val)        (*((inm_atomic_t *)var) = (val))
#define INM_ATOMIC_INC(var)             atomic_add_32((inm_u32_t *)var, 1)
#define INM_ATOMIC_DEC(var)             atomic_add_32((inm_u32_t *)var, -1)
/* TODO - fix this work around for following atomic operations */
#define INM_ATOMIC_READ(var)            atomic_add_32_nv((inm_u32_t*)var, 0)
#define INM_ATOMIC_DEC_AND_TEST(var)    (atomic_add_32_nv((inm_u32_t *)var, -1) == 0)
#define INM_ATOMIC_DEC_RET(var)         atomic_add_32_nv((inm_u32_t *)var, -1)

#define INM_IS_SPINLOCK_HELD(lock_addr) mutex_owned(lock_addr)

#endif /* _INM_LOCKS_H */
