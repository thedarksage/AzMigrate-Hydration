
/*****************************************************************************
 * vsnap_kernel_helpers.c
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:              InMage Systems
 *                      http://www.inmage.net
 * Creation date:       Wed, Feb 28, 2007
 * Description:         Kernel helper routines.
 *
 * Revision:            $Revision: 1.27 $
 * Checked in by:       $Author: lamuppan $
 * Last Modified:       $Date: 2015/02/24 03:29:23 $
 *
 * $Log: vsnap_kernel_helpers.c,v $
 * Revision 1.27  2015/02/24 03:29:23  lamuppan
 * Bug #1619650, 1619648, 1619642, 1619640, 1619637, 1619636, 1619635: Fix for policheck
 * bugs for Filter and Vsnap drivers. Please refer to bugs in TFS for more details.
 *
 * Revision 1.26  2014/11/17 12:59:48  lamuppan
 * TFS Bug #1057085: Fixed the issue by fixing the STR_COPY function. Please
 * go through the bug for more details.
 *
 * Revision 1.25  2014/10/10 08:04:09  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.24  2014/07/16 07:11:57  vegogine
 * Bug/Enhancement No : 30547
 *
 * Code changed/written by : Venu Gogineni
 *
 * Code reviewed by : N/A
 *
 * Unit tested by : N/A
 *
 * Smokianide-tested by : N/A
 *
 * Checkin Description : TRUNK rebase with SCOUT_7-2_OSS_SEC_FIXES_BRANCH code base.
 *
 * Revision 1.22.2.1  2014/05/06 09:35:38  chvirani
 * 29837, 29838, 29852: segmap changes, endian changes, crc changes
 * remove unused code, comments cleanup,
 *
 * Revision 1.22  2014/03/14 12:49:28  lamuppan
 * Changes for lists, kernel helpers.
 *
 * Revision 1.21  2012/09/17 08:02:15  hpadidakul
 * bug 23160: Btree corruption issue.
 *
 * Unit tested by: Hari
 *
 * Fix is to incremnt the file offset if write is partial because of non-page alinged data.
 *
 * Revision 1.20  2012/02/16 08:41:29  hpadidakul
 * 20366: changes to support common build for RHEL5.
 *
 * Since address_space_operations_ext is introduced in RHEL5u4 we need put few ifdef and pointer.
 *
 * Revision 1.19  2011/01/11 05:43:27  chvirani
 * 14377: Changes to support linux kernel 2.6.32 upwards
 *
 * Revision 1.18  2010/10/19 04:52:02  chvirani
 * 14174: Raw device no longer required
 *
 * Revision 1.17  2010/10/01 09:05:00  chvirani
 * 13869: Full disk support
 *
 * Revision 1.16  2010/04/30 10:40:33  hpadidakul
 * Changes is to support RHEL5.5, to use write_begin & write_end operations.
 *
 * Revision 1.15  2010/03/10 12:21:40  chvirani
 * Multithreaded driver - linux
 *
 * Revision 1.14  2010/02/08 10:52:34  hpadidakul
 * RHEL5U4 support, found a bug where vsnap creaction can fail on other Filesystem.
 * Code reviewed by: Chirag.
 * Unit test done by: Harikrishnan
 *
 * Revision 1.13  2010/01/23 06:16:13  hpadidakul
 * The code changes is to support vsnap driver on RHEL 5U4.
 * Fix is to use _begin & write_end function for doing write opration.
 *
 * Smokianide test done by: Brahamaprakash.
 *
 * Revision 1.12  2009/08/19 11:56:13  chvirani
 * Use splice/page_cache calls to support newer kernels
 *
 * Revision 1.11  2009/08/13 05:40:20  chvirani
 * 8903: Reduce debugging logs
 *
 * Revision 1.10  2009/05/19 13:59:14  chvirani
 * Change memset() to memzero() to support Solaris 9
 *
 * Revision 1.9  2009/05/04 13:03:29  chvirani
 * return 0 for success instead of number of bytes read/written
 *
 * Revision 1.8  2009/04/28 12:54:49  cswarna
 * These files were changed during vsnap driver code re-org
 *
 * Revision 1.7  2009/01/10 12:43:45  chvirani
 * 7384: Open files with O_LARGEFILE support
 *
 * Revision 1.6  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.5  2008/03/25 12:59:52  praveenkr
 * code cleanup for Solaris
 *
 * Revision 1.4  2007/12/04 12:53:38  praveenkr
 * fix for the following defects -
 *  4483 - Under low memory condition, vsnap driver suspended forever
 * 	4484 - vsnaps & dataprotection not responding
 * 	4485 - vsnaps utilizing more than 90% CPU
 *
 * Revision 1.3  2007/09/14 13:11:16  sudhakarp
 * Merge from DR_SCOUT_4-0_DEV/DR_SCOUT_4-1_BETA2 to HEAD ( Onbehalf of RP).
 *
 * Revision 1.1.2.15  2007/08/21 15:25:12  praveenkr
 * added more comments
 *
 * Revision 1.1.2.14  2007/07/26 13:08:32  praveenkr
 * fix for 3788: vsnap of diff target showing same data
 * replaced strncmp with strcmp
 *
 * Revision 1.1.2.13  2007/07/24 12:58:30  praveenkr
 * virtual volume (volpack) implementation
 *
 * Revision 1.1.2.12  2007/07/17 13:53:25  praveenkr
 * clean-up - unix-style naming, tab-spacing = 8, 78 col
 *
 * Revision 1.1.2.11  2007/07/12 11:39:21  praveenkr
 * removed devfs usage; fix for RH5 build failure
 *
 * Revision 1.1.2.10  2007/05/25 12:54:17  praveenkr
 * deleted unused functions - left behind during porting
 * DELETE_FILE, GET_FILE_SIZE, MEM_COPY
 *
 * Revision 1.1.2.9  2007/05/25 09:45:44  praveenkr
 * added comments & partial clean-up of code
 *
 * Revision 1.1.2.8  2007/05/15 15:40:16  praveenkr
 * Corrected a bad usage of mutex
 *
 * Revision 1.1.2.7  2007/05/02 11:32:28  praveenkr
 * fix for SLES10 build failure; structure difference between kernel versions
 *
 * Revision 1.1.2.6  2007/05/01 13:01:59  praveenkr
 * #3103: removed the reference to FSData file
 *
 * Revision 1.1.2.5  2007/04/25 12:52:29  praveenkr
 * fix for SLES9 build failure; structure difference between kernel versions
 *
 * Revision 1.1.2.4  2007/04/24 12:49:47  praveenkr
 * page cache implementation(write)
 *
 * Revision 1.1.2.2  2007/04/17 15:36:30  praveenkr
 * #2944: 4.0_rhl4target-target is crashed when writting the data to R/W vsnap.
 * Sync-ed most of the data types keeping Trunk Windows code as bench-mark.
 * Did minimal cleaning of the code too.
 *
 * Revision 1.1.2.1  2007/04/11 19:49:37  gregzav
 *   initial release of the new make system
 *   note that some files have been moved from Linux to linux (linux os is
 *   case sensitive and we had some using lower l and some using upper L for
 *   linux) this is to make things consitent also needed to move cdpdefs.cpp
 *   under config added the initial brocade work that had been done.
 *
 *****************************************************************************/

#include "common.h"
#include "vsnap_common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap_kernel.h"
#include "vdev_helper.h"

/* to facilitate ident */
static const char rcsid[] = "$Id: vsnap_kernel_helpers.c,v 1.27 2015/02/24 03:29:23 lamuppan Exp $";

#define VSNAP_READ		0
#define VSNAP_WRITE		1

inline void
inm_bzero_page(inm_page_t *page, inm_u32_t offset, inm_u32_t len)
{
	char *tmp_buf;

	tmp_buf = INM_KMAP_ATOMIC(page, KM_USER0);
 	memzero(tmp_buf + offset, len);
	INM_KUNMAP_ATOMIC(tmp_buf, KM_USER0);
}

inline void
inm_copy_buf_to_page(inm_page_t *page, void *buf, inm_u32_t len)
{
	char *tmp_buf;

	tmp_buf = INM_KMAP_ATOMIC(page, KM_USER0);
	memcpy_s(tmp_buf, len, buf, len);
	INM_KUNMAP_ATOMIC(tmp_buf, KM_USER0);
}


inline void
inm_copy_page_to_buf(void *buf, inm_page_t *page, inm_u32_t len)
{
	char *tmp_buf;

	tmp_buf = INM_KMAP_ATOMIC(page, KM_USER0);
        memcpy_s(buf, len, tmp_buf, len);
        INM_KUNMAP_ATOMIC(tmp_buf, KM_USER0);
}

inline inm_32_t
inm_copy_from_user(void *kerptr, void __user *userptr, size_t size)
{
        if ( !access_ok(VERIFY_READ, userptr, size) ) {
                ERR("err while access_ok.");
                return -EACCES;
        }

        if ( copy_from_user(kerptr, userptr, size) ) {
                ERR("err while copy_from_user.");
                return -EFAULT;
        }

	return 0;
}

inline inm_32_t
inm_copy_to_user(void __user *userptr, void *kerptr, size_t size)
{
        if ( !access_ok(VERIFY_WRITE, userptr, size) ) {
                ERR("err while access_ok.");
		return -EACCES;
	}

        if ( copy_to_user(userptr, kerptr, size) ) {
		ERR("err while copy_to_user.");
		return -EFAULT;
	}

	return 0;
}


inline void
STR_COPY(char *dest, char *src, inm_32_t len)
{
	(void) STR_COPY_S(dest, (size_t)len, src, (size_t)len);
}

inline void
STRING_CAT(char *str1, inm_32_t dlen, char *str2, inm_32_t len)
{
	(void) strncat_s(str1, (size_t)dlen, str2, (size_t)len);
}

inline inm_32_t
STRING_MEM(char *str)
{
	return (strlen(str) + 1);
}

/* Start - Exported  kernel APIs */
inm_32_t
INM_OPEN_FILE(const char *file_name, inm_u32_t mode, void **file_hdle)
{
	inm_32_t	retval = 0;
	struct file	*fp = NULL;
	mm_segment_t	fs;
	inm_address_space_operations_t *aops = NULL;

	ASSERT(!file_name);
	*file_hdle = NULL;

	mode = mode | O_LARGEFILE;
	/*
	 * the highest virutal address that is considered valid differs from
	 * kernel space to user space. and this value can be read & written
	 * using get_fs() & set_fs() apis. this 'address_limit' must be set
	 * when ever we invoke a system call from within the kernel space
	 * ToDo - get_fs may not be required for sendfile
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(file_name, (inm_32_t)mode, 0777);
	if (IS_ERR(fp)) {
		ERR("Not able to open %s", file_name);
		retval = PTR_ERR(fp);
		goto out;
	}

	if (S_ISREG(fp->f_mapping->host->i_mode)) {
		if ((!fp->f_op->write) || !(fp->f_op->read)) {
			ERR("No write/read found for the fild id %p", fp);
			retval = -EINVAL;
			goto error;
		}

		/* Check for Read  */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))

		if (!fp->f_op->splice_read) {
			ERR("no splice_read defined.");
			retval = -EINVAL;
			goto error;
		}

#else
		if (!fp->f_op->sendfile) {
			ERR("no sendfile defined.");
			retval = -EINVAL;
			goto error;
		}
#endif
		aops = (inm_address_space_operations_t *)fp->f_mapping->a_ops;


		/* Check for Write */
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) || \
     (defined(RHEL_MAJOR) && (OS_RELEASE_VERSION(RHEL_MAJOR, RHEL_MINOR)>= OS_RELEASE_VERSION(5, 4))))

		if (!aops->write_begin || !aops->write_end) {

#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 5)))
			if (!aops->orig_aops.prepare_write || !aops->orig_aops.commit_write) {
				ERR("no write defined.");
				retval = -EINVAL;
				goto error;
			}
#else
			retval = -EINVAL;
		 	goto error;
#endif
		}
		DBG("valid file");
#else

		if (!aops->prepare_write || !aops->commit_write) {
			ERR("no write defined.");
			retval = -EINVAL;
			goto error;
		}
		DEV_DBG("valid file");

#endif
	}

	/*
	 * return the file handle
	 */
	*file_hdle = (void *)fp;

  out:
	set_fs(fs);
	return retval;

  error:
	filp_close(fp, NULL);
	goto out;
}

/*
 * vdev_transfer
 *
 * DESC
 *      copies into/from the disk_page & user_page
 */
static inline void
vdev_transfer(inm_32_t cmd, struct page *disk_page, unsigned disk_offset,
	      struct page *user_page, unsigned user_offset, unsigned long size)
{
	char *disk_buf = INM_KMAP_ATOMIC(disk_page, KM_USER0) + disk_offset;
	char *user_buf = INM_KMAP_ATOMIC(user_page, KM_USER1) + user_offset;

	switch (cmd) {
	    case VSNAP_READ:
		DEV_DBG("read");
		memcpy_s(user_buf, size, disk_buf, size);
		break;

	    case VSNAP_WRITE:
		DEV_DBG("write");
		memcpy_s(disk_buf, size, user_buf, size);
		break;

	    default:
		ERR("error!");
		ASSERT(1);
	}
	INM_KUNMAP_ATOMIC(disk_buf, KM_USER0);
	INM_KUNMAP_ATOMIC(user_buf, KM_USER1);
}


/*
#ifdef _VSNAP_DEV_DEBUG_
static void
print_pd(inm_vdev_page_data_t* pd)
{
	DEV_DBG("bvec offset       - %d", pd->pd_bvec_offset);
	DEV_DBG("disk offset       - %lld", pd->pd_disk_offset);
	DEV_DBG("file offset       - %lld", pd->pd_file_offset);
	DEV_DBG("size              - %d", pd->pd_size);
}
#endif
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)

static int
vdev_splice_actor(struct pipe_inode_info *pipe, 
		  struct pipe_buffer *buf,
		  struct splice_desc *desc)
{
	inm_vdev_page_data_t *page_data = desc->u.data;
        size_t sz;
        int retval;

        DEV_DBG("Splice Read");
	retval = buf->ops->confirm(pipe, buf);
        if (retval)
                return retval;

        if (desc->len > page_data->pd_size)
                sz = page_data->pd_size;
	else
        	sz = desc->len;

	/* Transfer appropriate amount of data */
	DEV_DBG("Transfer %u bytes data", (inm_32_t)sz);
        vdev_transfer(VSNAP_READ, buf->page, buf->offset,
		      page_data->pd_bvec_page, page_data->pd_bvec_offset,
		      sz);

        flush_dcache_page(page_data->pd_bvec_page);

	/*
	 * prepare for next iteration, if any
	 */
	page_data->pd_bvec_offset += sz;
        return sz;

}

inline static int
vdev_read_actor(struct pipe_inode_info *pipe, struct splice_desc *desc)
{
        return __splice_from_pipe(pipe, desc, vdev_splice_actor);
}

inline inm_32_t
inm_vdev_read_page(void *file_handle, inm_vdev_page_data_t *page_data)
{
	struct splice_desc desc;
	inm_32_t retval = 0;

	desc.flags = 0;
	desc.pos = page_data->pd_file_offset;
	desc.total_len = page_data->pd_size;
	desc.len = 0;
	desc.u.data = page_data;

	DEV_DBG("Reading from offset = %llu len = %u",
		page_data->pd_file_offset, page_data->pd_size);

	retval = splice_direct_to_actor(file_handle, &desc, vdev_read_actor);

	return (retval < 0)? retval:0;

}

#else

/*
 * vdev_read_actor
 *
 * DESC
 *      copies the data b/w disk_page and bvec_page this function is invoked
 *      by sendpage iteratively until the requested amount of data is in the
 *      page. sendfile passes pointer to a page containing data from the file
 */
static inm_32_t
vdev_read_actor(read_descriptor_t *desc, struct page *disk_page,
		unsigned long disk_offset, unsigned long size)
{
	inm_vdev_page_data_t *page_data;

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,5)
	page_data = (inm_vdev_page_data_t *)desc->buf;
#else
	page_data = (inm_vdev_page_data_t *)desc->arg.data;
#endif

	if (size > desc->count) {
		size = desc->count;
	}
	DEV_DBG("size to read - %ld", size);

	vdev_transfer(VSNAP_READ, disk_page, disk_offset,
		      page_data->pd_bvec_page, page_data->pd_bvec_offset,
		      size);
	flush_dcache_page(page_data->bvec_page);

	/*
	 * prepare for next iteration, if any
	 */
	desc->count -= size;
	desc->written += size;
	page_data->pd_bvec_offset += size;

	return size;
}

/*
 * inm_vdev_read_page
 *
 * DESC
 *      read a page from the file using the sendfile function sendfile
 *      invokes vdev_read_actor function
 */
inm_32_t
inm_vdev_read_page(void *file_handle, inm_vdev_page_data_t *page_data)
{
	loff_t		pos = 0;
	inm_32_t	old_gfp_mask;
	inm_32_t	retval;
	struct file	*fp;
	mm_segment_t	fs;

	ASSERT(!page_data);
	ASSERT(!file_handle);
	fp = (struct file *)file_handle;
	ASSERT(!fp->f_op && !fp->f_op->sendfile);

	/*
	 * the highest virutal address that is considered valid differs from
	 * kernel space to user space. and this value can be read & written
	 * using get_fs() & set_fs() apis. this 'address_limit' must be set
	 * when ever we invoke a system call from within the kernel space
	 * ToDo - get_fs may not be required for sendfile
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = (loff_t)page_data->pd_file_offset;
	DEV_DBG("read from posn - %lld", pos);
	//print_pd(page_data);

	/*
	 * change the gfp mask; useful (esp) when system is low in memory
	 * and (now with this new gfp mask) __alloc_pages does not flush
	 * FS pages or initiate a new IO
	 */
	old_gfp_mask = mapping_gfp_mask(fp->f_mapping);
	mapping_set_gfp_mask(fp->f_mapping,
			     old_gfp_mask & ~(__GFP_IO | __GFP_FS));

	retval = fp->f_op->sendfile(fp, &pos, page_data->pd_size,
				    vdev_read_actor, page_data);
	if ((retval > 0) & (retval != page_data->pd_size)) {
		/*
		 * if the retval is +ve (i.e., no error) and the # of bytes
		 * read is same as the requested (page_data->pd_size) then
		 * fail the request.
		 */
		ERR("sendfile read only %d against requested %d", retval,
		    page_data->pd_size);
		retval = -EIO;
	}

	/* reset the gfpmask */
	mapping_set_gfp_mask(fp->f_mapping, old_gfp_mask);
	set_fs(fs);

	return (retval < 0)? retval:0;
}

#endif

inm_32_t
INM_READ_FILE(void *file_hdle, void *Buffer, inm_u64_t offset, inm_u32_t len,
	  inm_u32_t * BytesRead)
{
	struct file *fp;
	ssize_t NrRead;
	mm_segment_t fs;

	ASSERT(!file_hdle || !Buffer);
	fp = (struct file *)file_hdle;
	ASSERT(!fp->f_op && !fp->f_op->read);
	if (BytesRead != NULL) {
		*BytesRead = 0;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	NrRead = fp->f_op->read(fp, (char *)Buffer, len, (loff_t *)&offset);
	set_fs(fs);

	if (NrRead < 0) {
		ERR("read failed - %d", (int)(NrRead));
	}

	if (NrRead != len) {
		ERR("read failed len[%d] vs nrread[%d]",len, (int)(NrRead));
		NrRead = -EIO;
	}

	if (BytesRead != NULL) {
		*BytesRead = NrRead;
	}

	if ( NrRead == len )
		NrRead = 0;

	return NrRead;
}


/*
 * write_page_data
 *
 * DESC
 *      write a page into the file using the prepare_write & commit_write
 *      apis of the underlying FS
 */
static inm_32_t
write_page_data(struct file *fp, inm_vdev_page_data_t *page_data)
{
	pgoff_t					idx;
	inm_32_t				retval = 0;
	inm_u32_t				len;
	inm_u32_t				boff;
	inm_u32_t				disk_offset;
	inm_u64_t                               file_offset;
	struct page				*disk_page = NULL;
	void                                    *data = NULL;
	struct address_space			*mapping = fp->f_mapping;
	const inm_address_space_operations_t	*aops = mapping->a_ops;
	int (*prepare_write)(struct file *, struct page *, unsigned, unsigned) = NULL;
        int (*commit_write)(struct file *, struct page *, unsigned, unsigned) = NULL;
	inm_u32_t	size;
	inm_32_t	old_gfp_mask;

/* 
 * Only on kernel below 2.6.24 prepare and commit fields 
 * are present in address_space_operations.
 * On RHEL5 i.e. RHEL5.4 and above we have address_space_operations_ext
 * which has prepare_write and commit_write on orig_aops field.
 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,24))    // On kernel below 2.6.24 repare_write and commit_write
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 5) && \
     (OS_RELEASE_VERSION(RHEL_MAJOR, RHEL_MINOR)>= OS_RELEASE_VERSION(5, 4))))
        prepare_write = aops->orig_aops.prepare_write;
        commit_write = aops->orig_aops.commit_write;
#else
        prepare_write = aops->prepare_write;
        commit_write = aops->commit_write;
#endif
#endif


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
	mutex_lock(&mapping->host->i_mutex);
#else
	down(&mapping->host->i_sem);
#endif

	/* Set up parameters for IO transfer */
	/* Offset within the source vector page */
	boff = page_data->pd_bvec_offset;
	/* length of IO */
	len = page_data->pd_size;
	
	/* Destination page index */
	idx = page_data->pd_file_offset >> PAGE_CACHE_SHIFT;
	/* Offset within the destination page */
	disk_offset = page_data->pd_file_offset &
	    ((pgoff_t)PAGE_CACHE_SIZE - 1);

	/* Destination file offset in case prepare/commit write not there */
	file_offset = page_data->pd_file_offset;

	while (len) {
		/* Don't span across cached page size */
		size = PAGE_CACHE_SIZE - disk_offset;
		/* IF IO length requested is lesser, readjust size */
		if (len < size)
			size = len;
		/*
		 * change the gfp mask; useful (esp) when system is low in
		 * memory and (now with this new gfp mask) __alloc_pages does
		 * not flush FS pages or initiate a new IO
		 */
		old_gfp_mask = mapping_gfp_mask(mapping);
		mapping_set_gfp_mask(mapping,
				     old_gfp_mask & ~(__GFP_IO | __GFP_FS));

		/* Here we are assuming, if prepare write & commit write is NULL.
		 * The Filesystem is supporting write_begin & write_end.
		 * we are already verifying this at INM_OPEN_FILE(), so we are safe here.
		 */

		if (prepare_write) {
			/*
			 * grab page from the cache based on the mapping provided
			 * returns a locked page at given index, creating it if needed
			 */
			disk_page = grab_cache_page(mapping, idx);
			if (unlikely(!disk_page)) {
				goto write_failed;
			}

			/*
			 * prepares a page for receiving data; if the page has to be
			 * partially filled then prepare_write read the parts of the
			 * page that are not covered by write.
			 */
			if (unlikely(prepare_write(fp, disk_page, disk_offset,
							 disk_offset + size))) {
				goto unlock_page;
			}

			/*
			 * transfer the data from the user-page to the disk_page
			 */
			vdev_transfer(VSNAP_WRITE, disk_page, disk_offset,
				      page_data->pd_bvec_page, boff, size);
			flush_dcache_page(disk_page);

			/*
			 * marks the page & inode dirty. commit_write does not start
			 * the IO.
			 */
			if (unlikely(commit_write(fp, disk_page, disk_offset,
						disk_offset + size))) {
				goto unlock_page;
			}
			unlock_page(disk_page);		/* page grabed was locked */
			page_cache_release(disk_page);

			/*
			 * reset the gfpmask
			 */
			mapping_set_gfp_mask(mapping, old_gfp_mask);
		} else {
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) || \
     (defined(RHEL_MAJOR) && (OS_RELEASE_VERSION(RHEL_MAJOR, RHEL_MINOR)>= OS_RELEASE_VERSION(5, 4))))
			retval = aops->write_begin(fp, mapping, file_offset, size,
					  0, &disk_page, &data);

			if ( retval < 0 ) {
				ERR("pagecache_write failed");
				goto write_failed;
			}
			vdev_transfer(VSNAP_WRITE, disk_page, disk_offset,
				      page_data->pd_bvec_page, boff, size);

			mark_page_accessed(disk_page);
			retval = aops->write_end(fp, mapping, file_offset, size,
					  size, disk_page, data);
	                if ( retval < 0 || retval != size ) {
        	                ERR("pagecache_write_end failed %d %d ", size, retval);
                        	goto write_failed;
                	}

			/*
			 * reset the gfpmask
			 */
			mapping_set_gfp_mask(mapping, old_gfp_mask);
#endif
		}
		/* increment the source pointer to new offset */
		boff += size;
		/* decrement the length to remainder of IO transfer */
		len -= size;
		/* Increment index of page to point to immediate next page */
		idx++;
		/* This time, start at beginning of the cached page */
		disk_offset = 0;
		/* Increment file offset for remainder of IO */
		file_offset += size;
	}

  out:
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
	mutex_unlock(&mapping->host->i_mutex);
#else
	up(&mapping->host->i_sem);
#endif
	return (retval < 0)? retval : 0;

  unlock_page:
	unlock_page(disk_page);
	page_cache_release(disk_page);

  write_failed:
	retval = -EIO;
	goto out;
}



/*
 * inm_vdev_write_page
 *
 * DESC
 *      write a page pointed by page_data into the file @ a given offset
 *      (a wrapper for write_page_data)
 */
inm_32_t
inm_vdev_write_page(void *file_handle, inm_vdev_page_data_t *page_data)
{
	ssize_t retval = 0;
	struct file *fp;
	mm_segment_t fs;

	ASSERT(!file_handle);
	fp = (struct file *)file_handle;
	fs = get_fs();
	set_fs (KERNEL_DS);
	retval = write_page_data(fp, page_data);
	set_fs (fs);
	if (retval < 0) {
		ERR("write failed with error 0x%x", (inm_32_t)retval);
	}

	return (retval < 0)? retval : 0;
}

/*
 * INM_WRITE_FILE
 *
 * DESC
 *      wrapper to read nbytes from the file and populate Buffer
 */
inm_32_t
INM_WRITE_FILE(void *file_hdle, void *Buffer, inm_u64_t offset, inm_u32_t len,
	       inm_u32_t * nbytes)
{
	ssize_t		retval;
	struct file	*fp;
	mm_segment_t	fs;

	ASSERT (!file_hdle || !Buffer);
	fp = (struct file *)file_hdle;
	ASSERT (!fp->f_op && !fp->f_op->write);
	fs = get_fs();
	set_fs(KERNEL_DS);
	retval = fp->f_op->write(fp,(char *)Buffer, len, (loff_t *)&offset);
	set_fs(fs);

	if ((retval <= 0) || (retval != len)) {
		ERR("write failed with error 0x%x", (inm_32_t)retval);
		ERR("write failed - len[%u] Vs nrbytes[%d]", len,
		    (inm_32_t)retval);
		retval = -EIO;
	}

	if (nbytes != NULL) {
		*nbytes = retval;
	}

	if ( retval == len )
		retval = 0;

	return retval;
}

/*
 * INM_SEEK_FILE
 *
 * DESC
 * 	wrapper around the seek ops
 */
inm_32_t
INM_SEEK_FILE_END(void *file_hdle, inm_u64_t *new_offset)
{
	inm_32_t	retval = 0;
	loff_t		temp_offset;
	struct file	*fp;
	mm_segment_t	fs;
	inm_64_t 	offset = 0; /* beginning of the file */
	inm_u32_t 	origin = 2; /* SEEK_END */

	ASSERT(!file_hdle || !new_offset);
	fp = (struct file *)file_hdle;
	fs = get_fs();
	set_fs(KERNEL_DS);

	/*
	 * SUSE build does not have this vfs_llseek defined. There is a
	 * kernel version problem. So doing a direct approach
	 * retval = vfs_llseek(fp, (loff_t)offset, origin);
	 */
	if (fp->f_op && fp->f_op->llseek) {
		DEV_DBG("llseek defined by the FS");
		temp_offset = fp->f_op->llseek(fp, (loff_t)offset,
					       (inm_32_t)origin);
	} else {
		DEV_DBG("no llseek so using default_llseek");
		temp_offset = default_llseek(fp, (loff_t)offset, (inm_32_t)origin);
	}
	set_fs(fs);
	if (temp_offset < 0) {
		ERR("offset[%lld] from [%d] in file-id[%p]failed.",
		    offset, origin, fp);
		retval = -EPERM;
	}
	*new_offset = (inm_64_t)temp_offset;	/* error/new_offset is super imposed
						   on the same return value */

	return retval;
}

/*
 * INM_CLOSE_FILE
 *
 * DESC
 * 	wrapper around close file
 */
void
INM_CLOSE_FILE(void *file_hdle, inm_32_t mode)
{
	inm_32_t retval = 0;

	ASSERT(!file_hdle);
	retval = filp_close((struct file *)file_hdle, NULL);
	if (retval < 0) {
		ERR("filp close failed - err code[%d]", retval);
	}

	/*
	 * this does not return any error code because CLOSE_FILE is called
	 * mostly in the context of a previous failure or a shutdown time
	 * so this error value actually does not add any value, so still
	 * using a void function
	 */
}
