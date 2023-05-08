
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

#include <sys/atomic_op.h>
#include <sys/lock_def.h>
#include <sys/lock_alloc.h>

#include <inm_types.h>

typedef Simple_lock inm_mutex_t;
typedef Simple_lock inm_spinlock_t;
//typedef krwlock_t  inm_rwlock_t;
typedef Complex_lock inm_rwsem_t;
typedef Simple_lock inm_sem_t;
typedef inm_u32_t inm_atomic_t;

/* mutex lock apis */
#define INM_MUTEX_ALLOC(lock)		lock_alloc(lock, LOCK_ALLOC_PAGED, 0, -1)
#define INM_MUTEX_INIT(lock)            \
	INM_MUTEX_ALLOC(lock);		\
	simple_lock_init(lock)
#define INM_MUTEX_DESTROY(lock)         lock_free(lock)
#define INM_MUTEX_LOCK(lock)            simple_lock(lock)
#define INM_MUTEX_UNLOCK(lock)          simple_unlock(lock)

#if 0
/* condition variables */
#define INM_CV_INIT(cvp)                cv_init(cvp, NULL, CV_DRIVER, NULL)
#define INM_CV_DESTROY(cvp)		cv_destroy(cvp)
#endif

/* spin lock api */
#define INM_SPIN_LOCK_ALLOC(lock)	lock_alloc(lock, LOCK_ALLOC_PIN, 0, -1)
#define INM_INIT_SPIN_LOCK(lock)       	\
	INM_SPIN_LOCK_ALLOC(lock);	\
	simple_lock_init(lock)
#define INM_DESTROY_SPIN_LOCK(lock)	lock_free(lock)
#if 0
#define INM_SPIN_LOCK(lock)             simple_lock(lock)
#define INM_SPIN_UNLOCK(lock)           simple_unlock(lock)
#endif
#define INM_SPIN_LOCK(lock, ipl)        (ipl = disable_lock(INTIODONE, lock))
#define INM_SPIN_UNLOCK(lock, ipl)      unlock_enable(ipl, lock)
#define INM_SPIN_LOCK_BH(lock, ipl)     (ipl = disable_lock(INTTIMER, lock))
#define INM_SPIN_UNLOCK_BH(lock, ipl)   unlock_enable(ipl, lock)
#define INM_SPIN_LOCK_IRQSAVE(lock, ipl) \
                                        (ipl = disable_lock(INTMAX, lock))
#define INM_SPIN_UNLOCK_IRQRESTORE(lock, ipl) \
                                        unlock_enable(ipl, lock)
#define INM_VOL_LOCK(lock, flag)	\
			INM_SPIN_LOCK_IRQSAVE(lock, flag)
#define INM_VOL_UNLOCK(lock, flag)	\
			INM_SPIN_UNLOCK_IRQRESTORE(lock, flag)
			
#define INM_SPIN_LOCK_WRAPPER(lock, flag) INM_SPIN_LOCK(lock, flag)
#define INM_SPIN_UNLOCK_WRAPPER(lock, flag) INM_SPIN_UNLOCK(lock, flag)

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
#define INM_INIT_RW_SPIN_LOCK(lock)     lock_init(lock, NULL, RW_DRIVER, NULL)

#define INM_READ_SPIN_LOCK(rwlock)      lock_read(rwlock)
#define INM_READ_SPIN_UNLOCK(rwlock)    lock_done(rwlock)
#define INM_WRITE_SPIN_LOCK(rwlock)     lock_write(rwlock)
#define INM_WRITE_SPIN_UNLOCK(rwlock)   lock_done(rwlock)


#define INM_READ_SPIN_LOCK_BH(rwlock)   lock_read(rwlock)
#define INM_READ_SPIN_UNLOCK_BH(rwlock) lock_done(rwlock)
#define INM_WRITE_SPIN_LOCK_BH(rwlock)  lock_write(rwlock)
#define INM_WRITE_SPIN_UNLOCK_BH(rwlock) lock_done(rwlock)

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
#define INM_SEM_ALLOC(sem)		lock_alloc(sem, LOCK_ALLOC_PAGED, 0, -1)
#define INM_INIT_SEM(sem)               \
	INM_SEM_ALLOC(sem);		\
	simple_lock_init(sem)
#define INM_INIT_SEM_LOCKED(sem)        \
	INM_INIT_SEM(sem);		\
	simple_lock(sem);
#define INM_DESTROY_SEM(sem)		lock_free(sem)

#define INM_DOWN(sem)                   simple_lock(sem)
#define INM_DOWN_INTERRUPTIBLE(sem)     simple_lock(sem)
#define INM_DOWN_TRYLOCK(sem)           simple_lock_try(sem)
#define INM_UP(sem)                     simple_unlock(sem)

/* read write locks to fill up the rw semaphore on linux */


#define INM_RW_SEM_ALLOC(rw_sem, inst)	lock_alloc(rw_sem, LOCK_ALLOC_PAGED, 0, -1)
#define INM_RW_SEM_INIT(rw_sem)         \
	INM_RW_SEM_ALLOC(rw_sem, inst);	\
	lock_init(rw_sem, NULL)
#define INM_RW_SEM_DESTROY(rw_sem)	lock_free(rw_sem)
#define INM_DOWN_READ(rw_sem)           lock_read(rw_sem)
#define INM_READ_DOWN_TRYLOCK(rw_sem)   lock_try_read(rw_sem)
#define INM_UP_READ(rw_sem)             lock_done(rw_sem)
#define INM_DOWN_WRITE(rw_sem)          lock_write(rw_sem)
#define INM_UP_WRITE(rw_sem)            lock_done(rw_sem)
#define INM_DOWNGRADE_WRITE(rw_sem)     lock_try_write(rw_sem)

/* atomic operations */
#define INM_ATOMIC_SET(var, val)        (*((inm_atomic_t *)var) = (val))
#define INM_ATOMIC_INC(var)             fetch_and_add((atomic_p)var, 1)
#define INM_ATOMIC_DEC(var)             fetch_and_add((atomic_p)var, -1)
/* TODO - fix this work around for following atomic operations */
#define INM_ATOMIC_READ(var)            fetch_and_add((atomic_p)var, 0)
#define INM_ATOMIC_DEC_AND_TEST(var)    (fetch_and_add((atomic_p)var, -1) == 1)
#define INM_ATOMIC_DEC_RET(var)		INM_ATOMIC_DEC(var)

#define INM_IS_SPINLOCK_HELD(lock_addr) lock_mine(lock_addr)

#endif /* _INM_LOCKS_H */
