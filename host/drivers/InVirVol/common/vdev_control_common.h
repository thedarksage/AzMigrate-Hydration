#ifndef _INMAGE_VDEV_CONTROL_COMMON_H
#define _INMAGE_VDEV_CONTROL_COMMON_H

#include "vsnap_kernel_helpers.h"
#include "vsnap_control.h"
#include "vv_control.h"

typedef struct sparse_lookup {
	inm_u32_t sl_dev_name_size;
	inm_u32_t sl_file_name_size;
	char *sl_dev_name;
	char *sl_file_name;
} sparse_lookup_t;


/*
 * The following functions are expected
 * for create/delete ioctls to work
 */
/* Vsnap */
inm_32_t inm_vs_alloc_dev(inm_vdev_t **, inm_minor_t *, char *);
inm_32_t inm_vs_free_dev(inm_vdev_t *);
inm_32_t inm_vs_init_dev (inm_vdev_t *, inm_vs_mdata_t *);
inm_32_t inm_vs_uninit_dev (inm_vdev_t *, void *);
inm_32_t inm_delete_vsnap_bydevid (inm_vdev_t *vdev);

/* Volpack */
inm_32_t vv_alloc_dev(inm_vdev_t **, char *);
inm_32_t vv_init_dev(inm_vdev_t *, void *, char *, char *);
inm_32_t vv_free_dev(inm_vdev_t *);
inm_32_t vv_uninit_dev(inm_vdev_t *vdev);

/*
 * these are used to allocate and free minor
 */
inm_minor_t get_free_vs_minor(void);
inm_32_t put_vs_minor(inm_minor_t);
inm_minor_t get_free_vv_minor(void);
inm_32_t put_vv_minor(inm_minor_t);
inm_32_t insert_into_minor_list(inm_32_t minor);
inm_32_t inm_atoi(char *str);

#endif

