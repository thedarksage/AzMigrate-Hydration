/*
 * Copyright (C) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : statechange.h
 *
 * Description: This file contains data mode implementation of the
 *              filter driver.
 *
 * Functions defined are
 *
 *
 */

#ifndef _INMAGE_STATECHANGE_H_
#define _INMAGE_STATECHANGE_H_

#include "involflt-common.h"
#include "target-context.h"  

inm_s32_t create_service_thread(void);
void destroy_service_thread(void);
/*inm_s32_t process_target_state_change(target_context_t *tgt_ctxt, struct inm_list_head *lhptr, inm_s32_t statechanged); */

int service_state_change_thread(void *context);

#endif /* _INMAGE_STATECHANGE_H_ */
