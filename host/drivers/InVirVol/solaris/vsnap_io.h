#ifndef _INMAGE_VSNAP_IO_H
#define _INMAGE_VSNAP_IO_H

#include "common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "vsnap_control.h"
#include "vv_control.h"
#include "vdev_disk_helpers.h"

int vdev_strategy(struct buf *);
int vdev_read(dev_t , struct uio *, struct cred *);
int vdev_write(dev_t dev, struct uio *uio, struct cred *credp);
int vdev_aread(dev_t dev, struct aio_req *aio, struct cred *credp);
int vdev_awrite(dev_t dev, struct aio_req *aio, struct cred *credp);
void inm_handle_end_of_device(inm_vdev_t *vdev, inm_block_io_t *bio);
inm_u64_t inm_get_parition_offset(inm_vdev_t *, inm_block_io_t *);

#define INM_HANDLE_END_OF_DEVICE(vdev, bio)	\
					inm_handle_end_of_device(vdev, bio)
#define INM_DEVICE_MAX_OFFSET(vdev, bio)	\
					inm_get_parition_offset(vdev, bio)

// BIO related macros
#define READ			B_READ
#define WRITE			B_WRITE
#define BIO_TYPE(BIO, RW)	*(RW) = bio->b_flags & B_READ ? B_READ : B_WRITE
#define BIO_NEXT(bio)           ((bio)->b_chain)
#define BIO_OFFSET(bio)		\
			(inm_offset_t)((inm_u64_t)((bio)->b_lblkno) * DEV_BSIZE)
#define BIO_PRIVATE(bio)        ((bio)->b_private)
#define BIO_SIZE(bio)		((bio)->b_bcount)
#define BIO_RESID(bio)          (bio)->b_resid

#endif
