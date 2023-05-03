#ifndef _VDEV_ENDIAN_H_
#define _VDEV_ENDIAN_H_

#include "vsnap_kernel_helpers.h"

#ifdef _BIG_ENDIAN_

#define INM_LE_8(x)	    (x)
#define INM_LE_16(x)	memrev_16(x)
#define INM_LE_32(x)	memrev_32(x)
#define INM_LE_64(x)	memrev_64(x)
#define INM_BE_8(x)	    (x)
#define INM_BE_16(x)	(x)
#define INM_BE_32(x)	(x)
#define INM_BE_64(x)	(x)

#else

#ifdef _LITTLE_ENDIAN_ /* i386 */

#define INM_LE_8(x)	    (x)
#define INM_LE_16(x)	(x)
#define INM_LE_32(x)	(x)
#define INM_LE_64(x)	(x)
#define INM_BE_8(x)	    (x)
#define INM_BE_16(x)	memrev_16(x)
#define INM_BE_32(x)	memrev_32(x)
#define INM_BE_64(x)	memrev_64(x)

#else

#error Unknown Endianess

#endif /*_LITTLE_ENDIAN_*/

#endif /*_BIG_ENDIAN_*/

#endif
