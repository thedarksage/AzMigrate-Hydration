#ifndef PLATFORM__SCSICMD__H_
#define PLATFORM__SCSICMD__H_

#include <scsi/scsi.h>

#define RECOVERABLE_ERROR RECOVERED_ERROR
/* from sg how to: 
 * This is the SCSI status byte as defined by the SCSI standard. Note that it can have vendor information set in bits 0, 6 and 7 
 * (although this is uncommon) */
#define SCSISTATUSMASK 0x3e

#endif /* PLATFORM__SCSICMD__H_ */
