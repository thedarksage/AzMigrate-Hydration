#ifndef _VERIFIER_H
#define _VERIFIER_H

#ifndef __INM_KERNEL_DRIVERS__  /* User Mode */

#include "verifier_user.h"
#define verify_err(verbose, format, arg...)             \
    do {                                                \
        if (verbose)                                    \
            printf("INFO: %llu: " format "\n", offset, ## arg);   \
    } while(0)

#else                           /* Kernel Mode */

#include "involflt.h"
#include "involflt-common.h"
#include "data-mode.h"
#include "utils.h"
#include "change-node.h"
#include "filestream.h"
#include "iobuffer.h"
#include "filestream_segment_mapper.h"
#include "segmented_bitmap.h"
#include "bitmap_api.h"
#include "VBitmap.h"
#include "work_queue.h"
#include "data-file-mode.h"
#include "target-context.h"
#include "driver-context.h"
#include "filter_host.h"
#include "metadata-mode.h"
#include "tunable_params.h"
#include "svdparse.h"
#include "db_routines.h"

inm_s32_t inm_verify_alloc_area(inm_u32_t size, int toggle);
void inm_verify_free_area(void);

#define verify_err(verbose, format, arg...) (verbose ? err(format, ## arg) : 0)

#endif                          /* Kernel Mode */

#include "svdparse.h"

inm_s32_t inm_verify_change_node_data(char *buf, int bufsz, int verbose);

#endif
