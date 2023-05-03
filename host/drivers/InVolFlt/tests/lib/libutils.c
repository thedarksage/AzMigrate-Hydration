#include "libinclude.h"
#include "libutils.h"

off_t
get_size(char *str)
{
	off_t size = 0;

    if (!str) {
        errno = EINVAL;
    } else {

	    size = atoll(str);

	    if( strstr(str, "t") || strstr(str, "T") )
		    size = size * TB;
	    else if( strstr(str, "g") || strstr(str, "G") )
		    size = size * GB;
	    else if( strstr(str, "m") || strstr(str, "M") )
		    size = size * MB;
	    else if( strstr(str, "k") || strstr(str, "K") )
		    size = size * KB;
    }

	return size;
}
