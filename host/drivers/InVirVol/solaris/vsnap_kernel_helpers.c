#include "common.h"
#include "vsnap_common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"
#include "vdev_mem_debug.h"
#include "vdev_helper.h"

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

/*
char *
inm_get_raw_device(char *dsk_path)
{
	char *dev_name;
	char *dsk_ptr;
	char *dsk_org_ptr;
	size_t	len;

	dev_name = INM_ALLOC(strlen(dsk_path) + 2, GFP_KERNEL);
	if( !dev_name ) {
		printf("Cannot allocate dev_name buffer");
		return NULL;
	}

	if (strcpy_s(dev_name, (strlen(dsk_path) + 2, dsk_path)) {
		INM_FREE(&dev_name, (strlen(dsk_path) + 2);
		return NULL;
	}

	dsk_ptr = strstr(dev_name, "/dsk/");
	if( dsk_ptr) {
		len = strlen(dsk_ptr);
		dsk_ptr++;
		*dsk_ptr++ = 'r';
		dsk_org_ptr = strstr(dsk_path, "/dsk/"); 
		dsk_org_ptr++;
		if (strcpy_s(dsk_ptr, len + 1, dsk_org_ptr)) {
			INM_FREE(&dev_name, (strlen(dsk_path) + 2);
			return NULL;
		}
	}

	DBG("Raw Device = %s", dev_name);

	return dev_name;
}
*/

void
memrev(char *mem, size_t size)
{
    char c;
    int i = 1;
    for( i = ( size - 1 )/2; i >= 0; i-- ) {
        c = mem[i];
        mem[i] = mem[size - i - 1];
        mem[size - i - 1] = c;
    }
}

inm_u16_t
memrev_16(inm_u16_t x)
{
    memrev((char *)&x, sizeof(x));
    return x;
}
 
inm_u32_t
memrev_32(inm_u32_t x)
{
    memrev((char *)&x, sizeof(x));
    return x;
}
 
inm_u64_t
memrev_64(inm_u64_t x)
{
    memrev((char *)&x, sizeof(x));
    return x;
}
 
INLINE inm_page_t *
alloc_page(inm_u32_t flag)
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
	return ddi_copyin(userptr, kerptr, size, NULL);
}

INLINE inm_32_t
inm_copy_to_user(void __user *userptr, void *kerptr, size_t size)
{
	return ddi_copyout(kerptr, userptr, size, NULL);
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
	STR_COPY_S(dest, (size_t)len, src, (size_t)len);
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

/* Start - Exported  kernel APIs */
inm_32_t
INM_OPEN_FILE(char *file_name, inm_u32_t mode, void **vnode_ptr)
{
	inm_32_t	retval = 0;
	vnode_t		*vp = NULL;
	enum create	crwhy = 0;

	ASSERT(!file_name);
	*vnode_ptr = NULL;

	if ( mode & INM_CREAT )
		crwhy = CRCREAT;

	mode = mode | INM_LARGEFILE;

	retval = vn_open(file_name, UIO_SYSSPACE, (inm_32_t)mode, 0777, &vp, crwhy, 0);
	if ( !vp ) {
		ERR("Not able to open %s", file_name);
		/* error code should be < 0 */
		retval = -retval;
		goto out;
	}

	if (S_ISREG(vp->v_type)) {
		if ((!vp->v_op->vop_read) || !(vp->v_op->vop_write)) {
			ERR("No write/read found for the fild id %p", vp);
			retval = -EINVAL;
			goto error;
		}
	}

	/*
	 * return the file handle
	 */
	*vnode_ptr = (void *)vp;
	DEV_DBG("%s = %p", file_name, vp);

  out:
	return retval;

  error:
	INM_CLOSE_FILE(vp, (inm_32_t)mode);
	goto out;
}


inm_32_t
INM_READ_FILE(void *vnode_ptr, void *Buffer, inm_u64_t offset, inm_u32_t len,
	  inm_u32_t *BytesRead)
{
	vnode_t *vp;
	int retval = 0;

	ASSERT(!vnode_ptr || !Buffer);
	vp = (vnode_t *)vnode_ptr;
	ASSERT(!vp->v_op && !vp->v_op->vop_read);

	DEV_DBG("Reading from file %p, offset = %llu to buf = %p of len = %d",
		vp, offset, Buffer, len);

	/*
	 * Do not send resid pointer to vn_rdwr
	 * we want things to be successful or fail entirely
	 */
	retval = vn_rdwr(UIO_READ, vp, Buffer, (ssize_t)len,
			 (inm_offset_t)offset, UIO_SYSSPACE, 0, 0,
			 CRED(), NULL);

	if ( retval ) {
		ERR("Read failed for vnode 0x%p", vnode_ptr);
		retval = -retval;
	} else {
		if ( BytesRead ) {
			*BytesRead = (inm_u32_t)len;
		}
	}

	return retval;
}

inm_32_t
INM_WRITE_FILE(void *vnode_ptr, void *Buffer, inm_u64_t offset, inm_u32_t len,
	  inm_u32_t *BytesWrite)
{
	vnode_t *vp;
	int retval = 0;

	ASSERT(!vnode_ptr || !Buffer);
	vp = (vnode_t *)vnode_ptr;
	ASSERT(!vp->v_op && !vp->v_op->vop_write);

	/*
	 * Do not send resid pointer to vn_rdwr
	 * we want things to be successful or fail entirely
	 */
	retval = vn_rdwr(UIO_WRITE, vp, Buffer, (ssize_t)len,
			 (inm_offset_t)offset, UIO_SYSSPACE,
			 0, (rlim64_t)MAXOFF_T, CRED(), NULL);

	if ( retval ) {
		ERR("Write failed for vnode 0x%p", vnode_ptr);
		retval = -retval;
	} else {
		if ( BytesWrite ) {
			*BytesWrite = (inm_u32_t)len;
		}
	}

	return retval;
}

inm_32_t
inm_mapped_rdwr(void		*vnode_ptr,
		void 		*Buffer,
		inm_u64_t 	offset,
		inm_u32_t 	length,
		enum seg_rw 	srw)
{
	int 		error = 0;
	offset_t 	aligned_offset = 0;
	offset_t    offset_skip = 0;
    size_t  	iosize = 0;
	int     	flags = 0;
	size_t  	fault_len = 0;
	caddr_t 	maddr = NULL;
    void        *src = NULL;
    void        *dest = NULL;

	DEV_DBG("Reading from target device");
	DEV_DBG("Mapping in offset = %llu,, len = %u buffer %p", 
            offset, length, Buffer);

	offset_skip = offset & MAXBOFFSET;
	aligned_offset = offset - offset_skip;

	while( length ) {
        /* map the disk block in memory */
        maddr = segmap_getmapflt(segkmap, (vnode_t *)vnode_ptr,
                                 aligned_offset, MAXBSIZE, 1, srw);
        
        /* Get the source and dest for data copy */
	    if ( srw == S_READ ) {
			src = maddr + offset_skip; 
            dest = Buffer;
			flags = SM_FREE;
		} else {
			src = Buffer;
            dest = maddr + offset_skip;
			flags = SM_WRITE;
		}

        /* 
         * io size = offset to end of current block or
         *           remaining length left
         */
        iosize = length > (MAXBSIZE - offset_skip) ?
                 (MAXBSIZE - offset_skip) : length;	
        
        fault_len = roundup(offset_skip + iosize, PAGESIZE);
		
		/* fault on the mapped block to read it in */
        error = segmap_fault(kas.a_hat, segkmap, maddr,
					         fault_len, F_SOFTLOCK, srw);
		if( error ) 
            break;

	    DEV_DBG("Offset skip = %d, iosize = %d, fault_len = %d", 
                (int)offset_skip, (int)iosize, (int)fault_len);
        /* Copy the data to/from Buffer */
        bcopy(src, dest, iosize);
	
        /* mark the mapping free */	
		segmap_fault(kas.a_hat, segkmap, maddr,
					 fault_len, F_SOFTUNLOCK, srw);
	    error = segmap_release(segkmap, maddr, flags);
		maddr = NULL;
        if( error )
            break;

        /* 
         * Partial IO can come in first block only
         * Second block onwards, we start at beginning
         * of the block 
         */
		offset_skip = 0;
		aligned_offset += MAXBSIZE;
        
        /* Update the buffer variables */    
        length -= iosize;
		Buffer = (char *)Buffer + iosize;
    } 

	if ( error ) {
	    if (FC_CODE(error) == FC_OBJERR)
		    error = FC_ERRNO(error);
		else
			error = EIO;
	  
        if( maddr ) 
		    segmap_release(segkmap, maddr, 0);
        
        error = -error;
        ERR("Read from block device at offset %llu failed", aligned_offset);
    }

	return error;
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
inm_vdev_read_page(void *vnode_ptr, inm_vdev_page_data_t *page_data)
{
	inm_32_t retval= 0;

	if ( ((vnode_t *)vnode_ptr)->v_type != VBLK ) {
		retval = INM_READ_FILE(vnode_ptr,
			page_data->pd_bvec_page + page_data->pd_bvec_offset,
			page_data->pd_file_offset, page_data->pd_size, NULL);
	} else {
		retval = inm_mapped_rdwr(vnode_ptr,
			page_data->pd_bvec_page + page_data->pd_bvec_offset,
			page_data->pd_file_offset, page_data->pd_size, S_READ);
	}

	return (retval < 0)? retval : 0;
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
inm_vdev_write_page(void *vnode_ptr, inm_vdev_page_data_t *page_data)
{
	inm_32_t retval= 0;

	/*
	 * segmap does not allow appending to file
	 * as a stop gap measure, use VOP calls
	 * for i/o to retention/writedata files
	 * and segmap i/o for target disk
	 */
	if ( ((vnode_t *)vnode_ptr)->v_type != VBLK ) {
		retval = INM_WRITE_FILE(vnode_ptr,
			page_data->pd_bvec_page + page_data->pd_bvec_offset,
			page_data->pd_file_offset, page_data->pd_size, NULL);
	} else {
		retval = inm_mapped_rdwr(vnode_ptr,
			page_data->pd_bvec_page + page_data->pd_bvec_offset,
			page_data->pd_file_offset, page_data->pd_size, S_WRITE);
	}

	return (retval < 0)? retval : 0;
}

/*
 * INM_SEEK_FILE
 *
 * DESC
 * 	wrapper around the seek ops
 */
inm_32_t
INM_SEEK_FILE_END(void *vnode_ptr, inm_u64_t *new_offset)
{
	inm_32_t	retval = 0;
	vnode_t		*vp;
	struct vattr	vattr;

	ASSERT(!vnode_ptr || !new_offset);
	vp = (vnode_t *)vnode_ptr;

	vattr.va_mask = AT_SIZE;
#ifdef _SOLARIS_11_
	if ( retval = VOP_GETATTR(vp, &vattr, 0, CRED(), NULL) )
#else
	if ( retval = VOP_GETATTR(vp, &vattr, 0, CRED()) )
#endif
	{

		retval = -retval;
	}

	*new_offset = (inm_u64_t)vattr.va_size;

	return retval;
}

/*
 * INM_CLOSE_FILE
 *
 * DESC
 * 	wrapper around close file
 */
void
INM_CLOSE_FILE(void *vnode_ptr, inm_32_t mode)
{
	ASSERT(!vnode_ptr);

#ifdef _SOLARIS_11_
	VOP_CLOSE((vnode_t *)vnode_ptr, mode, 1, 0, CRED(), NULL);
#else
	VOP_CLOSE((vnode_t *)vnode_ptr, mode, 1, 0, CRED());
#endif

	vn_rele((vnode_t *)vnode_ptr);
}
