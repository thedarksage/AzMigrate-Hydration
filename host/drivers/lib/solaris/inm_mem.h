#ifndef _INM_MEM_H
#define _INM_MEM_H

#include <sys/kmem.h>

#define INM_KM_NORETRY 		0x0
#define INM_KM_NOWARN		0x0
#define INM_KM_HIGHMEM		0x0

#define INM_KM_SLEEP		KM_SLEEP
#define INM_KM_NOSLEEP		KM_NOSLEEP
#define INM_KM_NOIO		KM_SLEEP
#define INM_UMEM_SLEEP		DDI_UMEM_SLEEP
#define INM_UMEM_NOSLEEP	DDI_UMEM_NOSLEEP

/*
 *INM_SLAB_HWCACHE_ALIGN is 0 instead of nothing because drivers should pass 0, see man page of 
 * kmem_cache_create()
 */
#define INM_SLAB_HWCACHE_ALIGN	0

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
        rptr = kmem_alloc(size, flag);\
    }\
    rptr;\
})

#define INM_KFREE(ptr, size, heap)    kmem_free(ptr, size)

#define INM_PIN(addr, size)     0
#define INM_UNPIN(addr, size)   0

#define INM_VMALLOC(size, flag, heap) INM_KMALLOC(size, flag)
#define INM_VFREE(ptr, size, heap)    INM_KFREE(ptr, size)

#define INM_KMEM_CACHE_CREATE(cache_name, obj_size, align, flags, ctor, \
				dtor, nr_objs, min_nr, pinned)  \
	kmem_cache_create(cache_name, obj_size, align, ctor, dtor,	\
				NULL, NULL, NULL, flags)
#define INM_KMEM_CACHE_DESTROY(cache)      kmem_cache_destroy(cache)
#define INM_KMEM_CACHE_ALLOC(cachep, flags)     kmem_cache_alloc(cachep, flags)
#define INM_KMEM_CACHE_ALLOC_PATH(cachep, flags, size, heap)		\
						INM_KMEM_CACHE_ALLOC(cachep, flags)
#define INM_KMEM_CACHE_FREE(cachep, objp)       kmem_cache_free(cachep, objp)
#define INM_KMEM_CACHE_FREE_PATH(cachep, objp, heap)			\
						INM_KMEM_CACHE_FREE(cachep, objp)
#define INM_MEMPOOL_CREATE(min_nr, alloc_slab, free_slab, cachep)	\
						cachep
#define INM_MEMPOOL_FREE(objp, poolp)           kmem_cache_free(poolp, objp)
#define INM_MEMPOOL_ALLOC(poolp, flag)          kmem_cache_alloc(poolp, flag)
#define INM_MEMPOOL_DESTROY(poolp)
#define	INM_ALLOC_PAGE(flags)                   INM_KMALLOC(INM_PAGESZ, flags)
#define __INM_GET_FREE_PAGE(flags, heap)        INM_KMALLOC(INM_PAGESZ, flags)
#define INM_FREE_PAGE(pagep, heap)              INM_KFREE((pagep), INM_PAGESZ)
#define __INM_FREE_PAGE(pagep)                  INM_FREE_PAGE(pagep)

#define INM_ALLOC_MAPPABLE_PAGE(flags, cookiep)	ddi_umem_alloc(INM_PAGESZ, \
                                                    DDI_UMEM_SLEEP, (cookiep))
#define INM_FREE_MAPPABLE_PAGE(cookie, heap)    ddi_umem_free((cookie))

/*
please find sol specific function corresponding to alloc_page() in linux and populate INM_ALLOC_PAGE() above
*/
#define  INM_MEMPOOL_ALLOC_SLAB
#define  INM_MEMPOOL_FREE_SLAB

typedef int           inm_kmalloc_flag;
typedef kmem_cache_t  inm_kmem_cache_t;
typedef kmem_cache_t  inm_mempool_t;
typedef	int           inm_kmem_cache_flags;

#endif /* end of _INM_MEM_H */
