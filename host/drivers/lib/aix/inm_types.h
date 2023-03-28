#ifndef INM_TYPES_H
#define	INM_TYPES_H 
#define __inm_aix__

#include "inm_types.h"
#include <sys/types.h>
#include <sys/stdint.h>

/* signed types */
typedef long long               inm_sll64_t;
typedef int64_t                 inm_s64_t;
typedef int32_t                 inm_s32_t;
typedef int16_t                 inm_s16_t;
typedef int8_t                  inm_s8_t;
typedef char                    inm_schar;

/* unsigned types */
typedef unsigned long long      inm_ull64_t;
typedef uint64_t                inm_u64_t;
typedef uint32_t                inm_u32_t;
typedef uint16_t                inm_u16_t;
typedef uint8_t                 inm_u8_t;
typedef unsigned char           inm_uchar;
typedef void			inm_iodone_t;

#endif /* INM_TYPES_H */
