#include "common.h"
#include "vdev_proc_common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "vsnap_control.h"
#include "vv_control.h"

extern void *vs_listp;
extern void *vv_listp;

inm_32_t
inm_vdev_init_proc()
{
	return inm_vdev_init_proc_common();
}

void
inm_vdev_exit_proc()
{
	inm_vdev_exit_proc_common();
}
