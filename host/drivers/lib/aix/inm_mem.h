#ifndef _INM_MEM_H
#define _INM_MEM_H

#include <inm_list.h>
#include <inm_locks.h>
#include <sys/malloc.h>
#include <sys/pin.h>

#include <inm_types.h>

struct _target_context;
struct inm_bufinfo;

typedef struct inm_mempool_info {
    inm_list_head_t	pi_list;
    inm_spinlock_t 	pi_lock;
    inm_sem_t		pi_sem;
    inm_u32_t		pi_reserved;
    inm_u32_t		pi_allocd;
    inm_u32_t		pi_size;
    inm_u32_t		pi_min_nr;
    inm_u32_t		pi_max_nr;
    inm_u32_t		pi_pinned;
    char		*pi_name;
    heapaddr_t		pi_heap;
    inm_u32_t		pi_flags;
    inm_list_head_t     pi_balance_wait_list;
}inm_kmem_cache_t;

typedef inm_kmem_cache_t inm_mempool_t;

#define INM_BALANCE_POOL	0x1

inm_kmem_cache_t *inm_kmem_cache_create(char *, inm_u32_t, inm_u32_t,
					inm_u32_t, inm_s32_t);
void inm_kmem_cache_destroy(inm_kmem_cache_t *);
void *inm_kmem_cache_alloc(inm_kmem_cache_t *);
void inm_kmem_cache_free(inm_kmem_cache_t *, void *);
void inm_balance_pool(inm_kmem_cache_t *);

#define INM_KM_NORETRY		0x0
#define INM_KM_NOWARN		0x0
#define INM_KM_HIGHMEM		0x0

#define INM_KM_SLEEP		1
#define INM_KM_NOSLEEP		2

#if 0
#define INM_KM_NOIO         GFP_NOIO
#define INM_UMEM_SLEEP		GFP_KERNEL
#define	INM_UMEM_NOSLEEP	GFP_ATOMIC

#define INM_SLAB_HWCACHE_ALIGN	SLAB_HWCACHE_ALIGN
#endif

#define INM_PINNED				1
#define INM_UNPINNED				0

#define INM_KERNEL_HEAP				kernel_heap
#define INM_PINNED_HEAP				pinned_heap

#define CHECK_OVERFLOW(size)\
({\
    int ret;\
    unsigned long long tmp = (unsigned long long)size;\
    if(tmp < ((size_t) - 1)){\
        ret = 0;\
    }else {\
        ret = -1;\
    }\
    ret;\
})

#define INM_KMALLOC(size, flag, heap)\
({\
    void *rptr = NULL;\
    if(!CHECK_OVERFLOW(size)) {\
        rptr = xmalloc(size, 0, heap);\
    }\
    rptr;\
})

#define INM_KMALLOC_ALIGN(size, flags, align, heap)	\
						xmalloc(size, align, heap)
#define INM_KFREE(ptr, size, heap)		xmfree(ptr, heap)

#define INM_PIN(addr, size)	pin(addr, size)
#define INM_UNPIN(addr, size)	unpin(addr, size)							\

#define INM_VMALLOC				INM_KMALLOC
#define INM_VFREE				INM_KFREE

#define INM_KMEM_CACHE_CREATE(cache_name, obj_size, align, flags, ctor, dtor, nr_objs, min_nr, pinned)   \
        					inm_kmem_cache_create(cache_name, obj_size, nr_objs, min_nr, pinned)
#define INM_KMEM_CACHE_DESTROY(cachep)		inm_kmem_cache_destroy(cachep)
#define INM_KMEM_CACHE_ALLOC(cachep, flags)	inm_kmem_cache_alloc(cachep)
#define INM_KMEM_CACHE_FREE(cachep, objp)	inm_kmem_cache_free(cachep, objp)
#define INM_KMEM_CACHE_ALLOC_PATH(cachep, flags, size, heap) \
						INM_KMALLOC(size, flags, heap)
#define INM_KMEM_CACHE_FREE_PATH(cachep, objp, heap) \
						INM_KFREE(objp, 0, heap)

#define INM_MEMPOOL_CREATE(min_nr, alloc_slab, free_slab, cachep)	cachep
#define INM_MEMPOOL_FREE(objp, poolp)		INM_KMEM_CACHE_FREE(poolp, objp)
#define	INM_MEMPOOL_ALLOC(poolp, flag)		INM_KMEM_CACHE_ALLOC(poolp, flag)
#define INM_MEMPOOL_DESTROY(poolp)

#if 0
#define	INM_ALLOC_PAGE(flag)                    alloc_page(flag)
#define	__INM_FREE_PAGE(pagep, heap)            free_page((unsigned long )pagep)
#endif

#define	INM_ALLOC_MAPPABLE_PAGE(flag, heap)     INM_KMALLOC_ALIGN(INM_PAGESZ, flag, INM_PAGESHIFT, heap)
#define	INM_FREE_MAPPABLE_PAGE(page, heap)      INM_KFREE(page, INM_PAGESZ, heap)
#define	__INM_GET_FREE_PAGE(flag, heap)         INM_KMALLOC(INM_PAGESZ, flag, heap)
#define	INM_FREE_PAGE(pagep, heap)              INM_KFREE(pagep, INM_PAGESZ, heap)
#if 0

#define	INM_MEMPOOL_ALLOC_SLAB                  mempool_alloc_slab
#define	INM_MEMPOOL_FREE_SLAB                   mempool_free_slab

#define INM_MMAP_PGOFF(filp, addr, len, prot, flags, pgoff)	\
		do_mmap_pgoff(filp, addr, len, prot, flags, pgoff) 
	
typedef	unsigned     inm_kmalloc_flag;
#endif

#if 0
typedef	unsigned long inm_kmem_cache_flags;
typedef	mempool_alloc_t	inm_mempool_alloc_t;
#endif

#endif /* end of _INM_MEM_H */
