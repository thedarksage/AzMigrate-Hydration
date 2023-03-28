#include "common.h"
#include "vsnap_common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"
#include "vdev_mem_debug.h"
#include "vdev_helper.h"

extern void *vdev_log_filp;

/*
 * WARNING: If you are returning system error codes, change it to negative
 */

/*
 * __func__ is defined as static. As such, sun compiler
 * complains when fuctions defined as non-static inline
 * contain debug messages which make use of __func__. So
 * for debug builds we do not mark these functions as inline.
 */
#ifdef _VSNAP_DEBUG_
#define INLINE
#else
#define INLINE inline
#endif

INLINE inm_page_t *
alloc_page(inm_mem_flag_t flag)
{
	return INM_ALLOC(INM_PAGE_SIZE, flag);
}

INLINE void
__free_page(inm_page_t *page)
{
	INM_FREE(&page, INM_PAGE_SIZE);
}

INLINE void
inm_bzero_page(inm_page_t *page, inm_u32_t offset, inm_u32_t len)
{
	if ( len )
		bzero((char *)page + offset, len);
}

INLINE inm_32_t
inm_copy_from_user(void *kerptr, void __user *userptr, size_t size)
{
	return copyin(userptr, kerptr, size);
}

INLINE inm_32_t
inm_copy_to_user(void __user *userptr, void *kerptr, size_t size)
{
	return copyout(kerptr, userptr, size);
}

INLINE void
inm_copy_buf_to_page(inm_page_t *page, void *buf, inm_u32_t len)
{
	memcpy_s(page, len, buf, len);
}

INLINE void
inm_copy_page_to_buf(void *buf, inm_page_t *page, inm_u32_t len)
{
	memcpy_s(buf, len, page, len);
}

INLINE void
STR_COPY(char *dest, char *src, inm_32_t len)
{
	(void) STR_COPY_S(dest, (size_t)len, src, (size_t)len);
}

INLINE void
STRING_CAT(char *s1, inm_32_t dlen, char *s2, inm_32_t n)
{
        char *is1 = s1;

        n++;
        while (*is1++ != '\0') {
        	if (--dlen == 0) {
				return;
			}       
		}
        --is1;
        while ((*is1++ = *s2++) != '\0') {
                if (--n == 0) {
                        is1[-1] = '\0';
                        break;
                }

				if (--dlen == 0) {
						is1[-1] = '\0';
						break;
				}
        }

        return;
}

/*
 * On return, dividend should contain
 * the quotient and remainder is the returned value
 */
INLINE inm_64_t
divide(inm_64_t *dividend, inm_64_t divisor)
{
	inm_u64_t div = *dividend;

	*dividend = div / divisor;

	return div % divisor;
}

INLINE inm_32_t
STRING_MEM(char *str)
{
	return (strlen(str) + 1);
}

/* Start - Exported kernel APIs */
inm_32_t
INM_OPEN_FILE(char *file_name, inm_u32_t mode, void **filp)
{
	inm_32_t	retval = 0;
	struct file	*file = NULL;

	ASSERT(!file_name);
	*filp = NULL;

	mode = mode | INM_LARGEFILE;

	retval = fp_open(file_name, mode, 0755, 0, SYS_ADSPACE, &file);
	if ( !file ) {
		ERR("Not able to open %s", file_name);
		/* error code should be < 0 */
		retval = -retval;
		goto out;
	}

	/*
	 * return the file handle
	 */
	*filp = (void *)file;
	DEV_DBG("%s = %p", file_name, filp);

  out:
	return retval;
}

/*
 * For I/O to files, we bypass the LFS and go directly to vnode.
 * We cannot use LFS operations as it requires us to seek and do io.
 * Since some file handles are shared (target, private and log files),
 * seek and read/write is not atomic and not safe across multiple threads
 */
static inm_32_t
inm_vnode_rdwr(struct file *filp, void *Buffer, inm_u64_t offset,
	       inm_u32_t len, inm_u32_t *BytesDone, enum uio_rw rw)
{
	struct vnode *vnode = filp->f_vnode;
	struct uio uio_struct;
	struct iovec uio_vector;
	inm_32_t retval = 0;
	struct ucred *cred;

	uio_vector.iov_base = Buffer;
	uio_vector.iov_len = len;

	uio_struct.uio_iov = &uio_vector;
	uio_struct.uio_iovcnt = 1;
	uio_struct.uio_offset = offset;
	uio_struct.uio_segflg = UIO_SYSSPACE;
	uio_struct.uio_resid = len;
	uio_struct.uio_fmode = filp->f_flag;

	cred = crref();

	retval = VNOP_RDWR(vnode, rw, filp->f_flag, &uio_struct, 0, NULL, NULL,
			   filp->f_cred);
	if( retval || uio_struct.uio_resid != 0 ) {
		ERR("IO failed");
		if( retval ) {
			retval = -retval;
		} else {
			ERR("Trying to read beyond EOF?");
			retval = -ENXIO;
		}
	} else {
		if ( BytesDone ) {
			*BytesDone = (inm_u32_t)len;
		}
	}

	crfree(cred);

	return retval;
}

INLINE inm_32_t
INM_READ_FILE(void *filp, void *Buffer, inm_u64_t offset, inm_u32_t len,
	  inm_u32_t *BytesRead)
{
	return inm_vnode_rdwr(filp, Buffer, offset, len, BytesRead, UIO_READ);

}

INLINE inm_32_t
INM_WRITE_FILE(void *filp, void *Buffer, inm_u64_t offset, inm_u32_t len,
	  inm_u32_t *BytesRead)
{
	return inm_vnode_rdwr(filp, Buffer, offset, len, BytesRead, UIO_WRITE);

}

/*
 * inm_vdev_read_page
 *
 * DESC
 *      read a page from the file using the sendfile function sendfile
 *      invokes vdev_read_actor function
 *
 * Return
 * 	Success of Failure unlike INM_READ_FILE
 */

INLINE inm_32_t
inm_vdev_read_page(void *filp, inm_vdev_page_data_t *page_data)
{
	return inm_vnode_rdwr(filp,
			page_data->pd_bvec_page + page_data->pd_bvec_offset,
			page_data->pd_file_offset, page_data->pd_size, NULL,
			UIO_READ);
}

/*
 * inm_vdev_write_page
 *
 * DESC
 *      write a page pointed by page_data into the file @ a given offset
 *      (a wrapper for write_page_data)
 *
 * Return
 *	Return success/failure unlike INM_WRITE_FILE
 */
INLINE inm_32_t
inm_vdev_write_page(void *filp, inm_vdev_page_data_t *page_data)
{
	return inm_vnode_rdwr(filp,
			page_data->pd_bvec_page + page_data->pd_bvec_offset,
			page_data->pd_file_offset, page_data->pd_size, NULL,
			UIO_WRITE);
}

/*
 * INM_SEEK_FILE
 *
 * DESC
 * 	wrapper around the seek ops
 */
inm_32_t
INM_SEEK_FILE_END(void *filp, inm_u64_t *new_offset)
{
	struct stat stat;
	inm_32_t retval = 0;

	retval = fp_fstat(filp, &stat, STATSIZE, SYS_ADSPACE);
	if( retval ) {
		ERR("Cannot stat filp %p", filp);
		retval = -retval;
		goto out;
	}

	*new_offset = stat.st_size;
out:
	return retval;
}

/*
 * INM_CLOSE_FILE
 *
 * DESC
 * 	wrapper around close file
 */
void
INM_CLOSE_FILE(void *filp, inm_32_t mode)
{
	ASSERT(!filp);
	fp_close(filp);
}
