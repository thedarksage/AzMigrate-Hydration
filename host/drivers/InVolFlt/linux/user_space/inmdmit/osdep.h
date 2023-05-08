#ifndef OSDEP_H
#define OSDEP_H
#ifdef INM_LINUX
#include <linux/kdev_t.h>
#include <stdint.h>

typedef unsigned long inm_addr_t;

#define INM_MEM_ALIGN(bufptr, sz)	\
	posix_memalign(&(bufptr), sysconf(_SC_PAGE_SIZE), (sz))
#define INM_PAGESZ          (0x1000)
#define INM_PAGESHIFT           (12)

#endif

#ifdef INM_SOLARIS
typedef caddr_t inm_addr_t;

#define INM_MEM_ALIGN(bufptr, sz)	\
		((bufp = memalign(sysconf(_SC_PAGE_SIZE), (sz))) == NULL) ? \
			(-1) : (0) 
#ifdef __sparcv9
#define INM_PAGESZ          (0x2000)
#define INM_PAGESHIFT           (13)
#else
#define INM_PAGESZ          (0x1000)
#define INM_PAGESHIFT           (12)
#endif

#define __FUNCTION__       __func__
#endif
#ifdef INM_AIX
#include <stdlib.h>
typedef caddr_t inm_addr_t;
#define INM_MEM_ALIGN(bufptr, sz)      ((bufptr = valloc(sz)) == NULL ? (-1) : (0))

#define INM_PAGESZ          (0x1000)
#define INM_PAGESHIFT           (12)
#define __FUNCTION__       __func__
#endif
#endif
/* FIX ME: for other platforms */
