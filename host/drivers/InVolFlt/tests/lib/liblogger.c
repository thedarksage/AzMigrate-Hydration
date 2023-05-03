#include "libinclude.h"

extern void usage(int error);

int
setup_logfile(char *logfile, int truncate)
{
    int logfd = -1;
    int error = 0;
    int flags = O_RDWR;
    
    if (truncate)
        flags |= O_TRUNC;

    logfd = open(logfile, flags);
    if (logfd != -1) {
        error = -1;
    } else {
        fclose(stdout);
        dup(logfd);
        fclose(stderr);
        dup(logfd);
    }
    
    return error;
}

