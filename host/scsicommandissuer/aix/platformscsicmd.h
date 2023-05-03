#ifndef PLATFORM__SCSICMD__H_
#define PLATFORM__SCSICMD__H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* AIX does not predefine this scsi constant */
#define RECOVERABLE_ERROR 0x01

#endif /* PLATFORM__SCSICMD__H_ */
