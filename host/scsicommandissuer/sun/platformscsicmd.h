#ifndef PLATFORM__SCSICMD__H_
#define PLATFORM__SCSICMD__H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/scsi/generic/sense.h>

#define RECOVERABLE_ERROR KEY_RECOVERABLE_ERROR

/* solaris directly gives scsi status; not
 * vendor information set here */

#endif /* PLATFORM__SCSICMD__H_ */
