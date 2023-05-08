#ifndef _INMAGE_VSNAP_IO_H
#define _INMAGE_VSNAP_IO_H

#include "common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "vsnap_control.h"
#include "vv_control.h"

inm_32_t vdev_strategy(struct buf *);
int vdev_read(dev_t, struct uio *, chan_t, inm_32_t);
int vdev_write(dev_t, struct uio *, chan_t, inm_32_t);
void inm_handle_end_of_device(inm_vdev_t *vdev, inm_block_io_t *bio);

#define INM_HANDLE_END_OF_DEVICE(vdev, bio)	\
inm_handle_end_of_device(vdev, bio)

#define INM_DEVICE_MAX_OFFSET(vdev, bio)	INM_VDEV_SIZE(vdev)

// BIO related macros
#define DEV_BSIZE		512
#define READ			B_READ
#define WRITE			B_WRITE
#define BIO_TYPE(BIO, RW)	*(RW) = bio->b_flags & B_READ ? B_READ : B_WRITE
#define BIO_NEXT(bio)           ((bio)->av_forw)
#define BIO_OFFSET(bio)		\
			(inm_offset_t)((inm_u64_t)((bio)->b_blkno) * DEV_BSIZE)
#define BIO_PRIVATE(bio)        ((bio)->av_back)
#define BIO_SIZE(bio)		((bio)->b_bcount)
#define BIO_RESID(bio)          ((bio)->b_resid)

/* AIX specific structure */
typedef struct bio_chain {
	struct buf		*bc_head;
	struct buf		**bc_append;
	inm_minor_t		bc_prev_minor;
	inm_ipc_t		bc_ipc;
	inm_spinlock_t		bc_spinlock;
	tid_t			bc_event;
	inm_32_t		bc_driver_unloading;
} bio_chain_t;

#ifdef __64BIT_KERNEL
#define IO_THREAD_KILL		0xDEADBEEFDEADBEEF
#else
#define IO_THREAD_KILL		0xDEADBEEF
#endif

/* Only defined for AIX */
inm_32_t vdev_io_init(void);
void vdev_io_exit(void);

#endif
