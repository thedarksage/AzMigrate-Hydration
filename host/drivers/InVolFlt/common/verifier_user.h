#ifndef _VUSER
#define _VUSER

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#define INM_LINUX

typedef int inm_s32_t;
typedef unsigned int inm_u32_t;
typedef long long  inm_s64_t;
typedef unsigned long long inm_u64_t;

static inline int
inm_is_little_endian(void)
{
    return 1;
    /*
    int i = 1;
    return (int)(*(char *)&i);
    */
}

#endif

