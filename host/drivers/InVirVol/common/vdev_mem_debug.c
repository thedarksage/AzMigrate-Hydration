
/*****************************************************************************
 * vsnap_mem_debug.c
 *
 * Copyright (c) 2007 InMage
 * This file contains proprietary and confidential information and remains
 * the unpublished property of InMage. Use, disclosure, or reproduction is
 * prohibited except as permitted by express written license agreement with
 * Inmage
 *
 * Author:             InMage Systems
 *                     http://www.inmage.net
 * Creation date:      Wed, Jan 30, 2008
 * Description:        for memory debugging
 *
 * Revision:           $Revision: 1.13 $
 * Checked in by:      $Author: praweenk $
 * Last Modified:      $Date: 2014/10/10 08:04:06 $
 *
 * $Log: vdev_mem_debug.c,v $
 * Revision 1.13  2014/10/10 08:04:06  praweenk
 * Bug ID:917800
 * Description: Safe API changes for filter and VSNAP driver cross platform.
 *
 * Revision 1.12  2014/03/14 12:49:28  lamuppan
 * Changes for lists, kernel helpers.
 *
 * Revision 1.11  2011/01/13 06:34:52  chvirani
 * 14878: Changes to support AIX.
 *
 * Revision 1.10  2010/10/13 08:09:44  chvirani
 * use INM_ASSIGN() for memory unaligned access
 *
 * Revision 1.9  2010/07/21 10:14:10  chvirani
 * Correctly free leaked memory on driver exit
 *
 * Revision 1.8  2010/06/09 10:10:04  chvirani
 * Support for sparse retention
 *
 * Revision 1.7  2010/03/31 13:23:11  chvirani
 * o X: Convert disk offsets to inm_offset_t
 * o X: Increase/Decrease mem usage count inside lock
 *
 * Revision 1.6  2010/03/10 12:08:43  chvirani
 * Multithreaded Driver
 *
 * Revision 1.5  2009/07/03 06:52:04  chvirani
 * 8920: ZFS support and error reporting in solaris proc
 *
 * Revision 1.4  2009/05/27 14:16:54  chvirani
 * buffer overflow fixed
 *
 * Revision 1.3  2009/05/19 13:58:37  chvirani
 * Solaris 9 support. Change memset to memzero()
 *
 * Revision 1.2  2009/05/04 12:59:37  chvirani
 * proc support, stop empty file complaints for non debug compile
 *
 * Revision 1.1  2009/04/28 12:34:38  cswarna
 * Adding these files as part of vsnap driver code re-org
 *
 * Revision 1.1  2008/12/30 18:10:28  chvirani
 * Multithreaded pool changes, caching and other improvements - unfinished
 *
 * Revision 1.3  2008/08/12 22:27:23  praveenkr
 * fix for #5404: Decoupled the mount point away from creation of vsnap
 * Moved the decision on the device name away from driver into the CX
 * & 'cdpcli'
 *
 * Revision 1.2  2008/04/01 14:15:55  praveenkr
 * changed the memory-debugging approach from list to prefix
 *
 * Revision 1.1  2008/04/01 13:04:09  praveenkr
 * added memory debugging support (for debug build only)
 *
 *****************************************************************************/
#include "common.h"
#include "vdev_proc_common.h"

#ifdef _VSNAP_DEBUG_

/*
 * We define MEM_STATS as part of proc. In case proc is not enabled
 * define MEM_STATS as a static long
 */
inm_u64_t mem_used = 0;


/* to facilitate ident */
static const char rcsid[] = "$Id: vdev_mem_debug.c,v 1.13 2014/10/10 08:04:06 praweenk Exp $";

#define INM_PREFIX_SIGNATURE_SIZE	16
#define INM_SUFFIX_SIGNATURE_SIZE	16
#define INM_PREFIX_ALLOC_SIGNATURE	"900df00d900df00d"
#define INM_SUFFIX_ALLOC_SIGNATURE	"900df00d900df00d"
#define INM_PREFIX_FREE_SIGNATURE	"deadbeefdeadbeef"
#define INM_SUFFIX_FREE_SIGNATURE	"deadbeefdeadbeef"
#define INM_FUNC_LEN			64

#define INM_MEM_DEBUG_FILE		"/var/log/linvsnap_debug"

#include "vsnap_kernel_helpers.h"

void insert_into_mem_list(void *, inm_mem_flag_t, size_t);
void delete_from_mem_list(const void *, inm_mem_flag_t, size_t);

typedef struct inm_mem_list {
        inm_list_head_t        	mem_next;
        void			*alloced_mem;
} inm_mem_list_t;

inm_list_head_t        	*mem_list_head;
inm_mutex_t		mem_list_mutex;

#define INSERT_INTO_MEM_LIST(ptr, flags, size)		\
					insert_into_mem_list(ptr, flags, size)
#define DELETE_FROM_MEM_LIST(ptr, flags, size)			\
					delete_from_mem_list(ptr, flags, size)

static void dump_alloc_info(const void *);

/*
 * the prefix and suffix structures
 * NOTE - the structs are mirror images of eachother, i.e.,
 *          -----------------------------------
 * PREFIX - | SIZE | PTR | FUNC | LINE | SIGN |
 *          -----------------------------------
 *          -----------------------------------
 * SUFFIX - | SIGN | LINE | FUNC | PTR | SIZE |
 *          -----------------------------------
 */
struct mdbg_pre_entry {
	inm_mem_flag_t	pre_flags;
	size_t		pre_size;			/* size of mem region */
	unsigned long	pre_ptr;			/* user returned ptr */
	char		pre_func[INM_FUNC_LEN];		/* caller func */
	inm_32_t	pre_line;			/* line # of caller */
	char		pre_sign[INM_PREFIX_SIGNATURE_SIZE];
};

struct mdbg_post_entry {
	char		post_sign[INM_SUFFIX_SIGNATURE_SIZE];
	inm_32_t	post_line;			/* line # of caller */
	char		post_func[INM_FUNC_LEN];	/* caller func */
	unsigned long	post_ptr;			/* user returned ptr */
	size_t		post_size;			/* size of mem region */
	inm_mem_flag_t	post_flags;
};

void
inm_mem_init()
{
	mem_list_head = INM_ALLOC_MEM(sizeof(inm_mem_list_t), GFP_KERNEL);
	INM_INIT_LIST_HEAD(mem_list_head);
	INM_INIT_MUTEX(&mem_list_mutex);
	MEM_STATS = 0;
}

void
free_memory_list(void)
{
	inm_mem_list_t		*mem_alloc_entry = NULL;
	struct mdbg_pre_entry	*pre_entry = NULL;
	size_t 			size;
	void			*ptr;
	inm_mem_flag_t		flags;

	/* Remove memory entry from mem list */
	while( !inm_list_empty(mem_list_head) ) {
		mem_alloc_entry = inm_list_first_entry(mem_list_head,
						   inm_mem_list_t, mem_next);

		pre_entry = mem_alloc_entry->alloced_mem;
		size = pre_entry->pre_size;
		flags = pre_entry->pre_flags;
		ptr = (char *)pre_entry + sizeof(struct mdbg_pre_entry);

		DBG("Freeing base %p, ptr = %p", pre_entry, ptr);
		inm_verify_for_mem_leak_kfree(ptr, size, flags, __FUNCTION__,
					      __LINE__, 0);
	}
}

void
dump_memory_list()
{
	inm_list_head_t		*mem_entry_ptr = NULL;
	inm_mem_list_t		*mem_alloc_entry = NULL;

	if ( MEM_STATS ) {
		UPRINT("Memory In Use = %lld Bytes", MEM_STATS);

		UPRINT("=================== Start Dump =====================");
		/* Remove memory entry from mem list */
		INM_ACQ_MUTEX(&mem_list_mutex);

		inm_list_for_each(mem_entry_ptr, mem_list_head) {
			mem_alloc_entry = inm_list_entry(mem_entry_ptr,
						inm_mem_list_t, mem_next);

			dump_alloc_info((char *)mem_alloc_entry->alloced_mem +
						sizeof(struct mdbg_pre_entry));
		}

		INM_REL_MUTEX(&mem_list_mutex);
		UPRINT("==================== End Dump ======================");
	}
}

void
inm_mem_destroy()
{
	if( MEM_STATS ) {
		ERR("Memory Leak - %lld bytes", (inm_u64_t)MEM_STATS);
		dump_memory_list();
		free_memory_list();
	}

	INM_FREE_MEM(mem_list_head, sizeof(inm_mem_list_t),
		     GFP_KERNEL);
}

void insert_into_mem_list(void *ptr, inm_mem_flag_t flags, size_t size)
{

	inm_mem_list_t		*mem_alloc_entry = NULL;


	mem_alloc_entry = INM_ALLOC_MEM(sizeof(inm_mem_list_t), flags);
	if ( !mem_alloc_entry )	{
		ERR("no mem");
		return;
	}
	mem_alloc_entry->alloced_mem = ptr;
	INM_ACQ_MUTEX(&mem_list_mutex);

	MEM_STATS += size;

	if ( MAX_MEM_ALLOWED && MEM_STATS >= MAX_MEM_ALLOWED ) {
		INM_REL_MUTEX(&mem_list_mutex);
		DEV_DBG("Memory Hard Limit Reached");
		dump_memory_list();
		ASSERT(1);
	}

	inm_list_add(&mem_alloc_entry->mem_next, mem_list_head);

	INM_REL_MUTEX(&mem_list_mutex);
}

void delete_from_mem_list(const void *ptr, inm_mem_flag_t flags, size_t size)
{

	inm_list_head_t		*mem_entry_ptr = NULL;
	inm_list_head_t		*mem_entry_next = NULL;
	inm_mem_list_t		*mem_alloc_entry;

	INM_ACQ_MUTEX(&mem_list_mutex);

	MEM_STATS -= size;

	/* Remove memory entry from mem list */
	inm_list_for_each_safe(mem_entry_ptr, mem_entry_next, mem_list_head) {
		mem_alloc_entry = inm_list_entry(mem_entry_ptr, inm_mem_list_t,
								mem_next);

		if ( (char *)mem_alloc_entry->alloced_mem +
					sizeof(struct mdbg_pre_entry) == ptr ) {
			inm_list_del(mem_entry_ptr);
			INM_FREE_MEM(mem_alloc_entry, sizeof(inm_mem_list_t),
				     flags);
			break;
		}
	}

	INM_REL_MUTEX(&mem_list_mutex);
}

static void
dump_alloc_info(const void *ptr)
{
	void			*tmp = NULL;
	inm_32_t		offset = 0;
	char			tmp_pre[INM_PREFIX_SIGNATURE_SIZE + 1];
	char			tmp_post[INM_SUFFIX_SIGNATURE_SIZE + 1];
	struct mdbg_pre_entry	pre_entry;
	struct mdbg_post_entry	post_entry;

	/* retreive the prefix */
	UPRINT("Base = %p", (char *)ptr - sizeof(struct mdbg_pre_entry));
	UPRINT("------------------------------------------------------");
	offset = sizeof(struct mdbg_pre_entry);
	tmp = (char *)ptr - offset;
	memcpy_s(&pre_entry, sizeof(struct mdbg_pre_entry), tmp,
					sizeof(struct mdbg_pre_entry));
	UPRINT("PREFIX - ");
	UPRINT("\tsize         - %d", (int)(pre_entry.pre_size));
	UPRINT("\tptr          - 0x%lx", pre_entry.pre_ptr);
	UPRINT("\tfunc         - %s", pre_entry.pre_func);
	UPRINT("\tline         - %d", pre_entry.pre_line);
	memcpy_s(tmp_pre, INM_PREFIX_SIGNATURE_SIZE, pre_entry.pre_sign,
						INM_PREFIX_SIGNATURE_SIZE);
	tmp_pre[INM_PREFIX_SIGNATURE_SIZE] = '\0';
	UPRINT("\tsign         - %s", tmp_pre);

	/* retrieve the suffix */
	offset = pre_entry.pre_size;
	tmp = (char *)ptr + offset;
	memcpy_s(&post_entry, sizeof(struct mdbg_post_entry), tmp,
					sizeof(struct mdbg_post_entry));
	UPRINT("SUFFIX - ");
	UPRINT("\tsize         - %d", (int)(post_entry.post_size));
	UPRINT("\tptr          - 0x%lx", post_entry.post_ptr);
	UPRINT("\tfunc         - %s", post_entry.post_func);
	UPRINT("\tline         - %d", post_entry.post_line);
	memcpy_s(tmp_post, INM_PREFIX_SIGNATURE_SIZE, post_entry.post_sign,
						INM_PREFIX_SIGNATURE_SIZE);
	tmp_post[INM_PREFIX_SIGNATURE_SIZE] = '\0';
	UPRINT("\tsign         - %s", tmp_post);
	UPRINT("------------------------------------------------------");
}

/*
 * verify_alloc
 *
 * DESC
 *	wrapper around native alloc to verify the memory leak
 *
 * LOGIC
 *	additional memory is allocated over and above the requested size; this
 *	extra memory is used to store signature (both as prefix and suffix);
 *	while freeing this prefix & suffix signature is checked and any
 *	violation lead to ASSERT
 *
 * ALGO
 *	1. allocate additional memory to store SIGNATURE
 *	2. prefix & suffix the signature
 */
void *
inm_verify_alloc(size_t size, inm_mem_flag_t flags, const char func[],
		 inm_32_t line)
{
	void			*ptr = NULL;
	void			*tmp = NULL;
	inm_32_t		alloc_size = 0;
	struct mdbg_pre_entry	pre_entry;
	struct mdbg_post_entry	post_entry;

	alloc_size = size + sizeof(struct mdbg_pre_entry) +
			sizeof(struct mdbg_post_entry);
	tmp = INM_ALLOC_MEM(alloc_size, flags);
	if (!tmp) {
		ERR("no mem");
		goto out;
	}

	/* the address to be returned to the caller */
	ptr = ((char *)tmp) + sizeof(struct mdbg_pre_entry);

	/* fill the pre-entry */
	pre_entry.pre_flags = flags;
	memzero(&pre_entry, sizeof(struct mdbg_pre_entry));
	pre_entry.pre_size = size;
	memcpy_s(pre_entry.pre_func, (INM_FUNC_LEN - 1), func,
					(INM_FUNC_LEN - 1));
	pre_entry.pre_func[INM_FUNC_LEN - 1] = '\0';
	pre_entry.pre_line = line;
	memcpy_s(pre_entry.pre_sign, INM_PREFIX_SIGNATURE_SIZE,
		INM_PREFIX_ALLOC_SIGNATURE, INM_PREFIX_SIGNATURE_SIZE);

	/* fill the post-entry */
	memzero(&post_entry, sizeof(struct mdbg_post_entry));
	memcpy_s(post_entry.post_sign, INM_SUFFIX_SIGNATURE_SIZE,
		INM_SUFFIX_ALLOC_SIGNATURE, INM_SUFFIX_SIGNATURE_SIZE);
	post_entry.post_line = line;
	memcpy_s(post_entry.post_func, (INM_FUNC_LEN - 1), func,
						(INM_FUNC_LEN - 1));
	post_entry.post_func[INM_FUNC_LEN - 1] = '\0';
	post_entry.post_size = size;
	post_entry.post_flags = flags;


	/* complete the pre & post entry */
	pre_entry.pre_ptr = (unsigned long) ptr;
	post_entry.post_ptr = (unsigned long) ptr;

	/*
	 * Copy to the buffer
	 */
	memcpy_s(tmp, sizeof(struct mdbg_pre_entry), &pre_entry,
					sizeof(struct mdbg_pre_entry));
	memcpy_s((char *)tmp + sizeof(struct mdbg_pre_entry) + size,
		sizeof(struct mdbg_post_entry), &post_entry,
		sizeof(struct mdbg_post_entry));

	INSERT_INTO_MEM_LIST(tmp, flags, pre_entry.pre_size);

  out:
	return ptr;
}

void
inm_verify_mem(const void 	*ptr,
	       size_t 		mem_size,
	       const char 	func[],
	       inm_32_t 	line,
	       inm_32_t		verbose)
{
	char			tmp_pre[INM_PREFIX_SIGNATURE_SIZE + 1];
	char			tmp_post[INM_SUFFIX_SIGNATURE_SIZE + 1];
	inm_32_t		offset = 0;
	inm_32_t		failed = 0;
	inm_32_t		size = mem_size;
	struct mdbg_pre_entry	pre_entry;
	struct mdbg_post_entry	post_entry;
	struct mdbg_pre_entry	*pre_entry_ptr;
	struct mdbg_post_entry	*post_entry_ptr;

	if ( verbose ) {
		DBG("Free called for %p", ptr);
		dump_alloc_info(ptr);
	}

	/* retrieve the prefix */
	offset = sizeof(struct mdbg_pre_entry);
	pre_entry_ptr = (struct mdbg_pre_entry *)((char *)ptr - offset);
	memcpy_s(&pre_entry, sizeof(struct mdbg_pre_entry), pre_entry_ptr,
					sizeof(struct mdbg_pre_entry));

	/* retrieve the suffix */
	offset = pre_entry.pre_size;
	post_entry_ptr = (struct mdbg_post_entry *)((char *)ptr + offset);
	memcpy_s(&post_entry, sizeof(struct mdbg_post_entry), post_entry_ptr,
					sizeof(struct mdbg_post_entry));

	/* verify the size */
	if (pre_entry.pre_size != post_entry.post_size) {
		ERR("size comparison failed - pre[%d] Vs post[%d]",
		    (int)(pre_entry.pre_size), (int)(post_entry.post_size));
		failed = 1;
	}

	/* verify the size against the one that has been passed */
	if( mem_size ) {
		if (pre_entry.pre_size != mem_size) {
			ERR("Allocation/Free size mismatch - \
			     Allocated[%d] Vs Free[%d]",
			    (int)(pre_entry.pre_size), (int)(mem_size));
		size = pre_entry.pre_size; /* no mem leaks please */
		failed = 1;
		}
	}

	/* verify the ptr */
	if (pre_entry.pre_ptr != post_entry.post_ptr) {
		ERR("size comparison failed - pre[0x%lx] Vs post[0x%lx]",
		    pre_entry.pre_ptr, post_entry.post_ptr);
		failed = 1;
	}

	/* compare the func name */
	if (strcmp(pre_entry.pre_func, post_entry.post_func)) {
		ERR("func name comparison failed - pre[%s] Vs post[%s]",
		    pre_entry.pre_func, post_entry.post_func);
		failed = 1;
	}

	/* compare the line # */
	if (pre_entry.pre_line != post_entry.post_line) {
		ERR("line comparison failed - pre[%d] Vs post[%d]",
		    pre_entry.pre_line, post_entry.post_line);
		failed = 1;
	}

	/* verify the sign */
	if (memcmp(pre_entry.pre_sign, INM_PREFIX_ALLOC_SIGNATURE,
		   INM_PREFIX_SIGNATURE_SIZE)) {
		memcpy_s(tmp_pre, INM_PREFIX_SIGNATURE_SIZE, pre_entry.pre_sign,
					INM_PREFIX_SIGNATURE_SIZE);
		tmp_pre[INM_PREFIX_SIGNATURE_SIZE] = '\0';
		ERR("sign comparison failed - pre[%s}", tmp_pre);
		failed = 1;
	}
	if (memcmp(post_entry.post_sign, INM_SUFFIX_ALLOC_SIGNATURE,
		   INM_SUFFIX_SIGNATURE_SIZE)) {
		memcpy_s(tmp_post, INM_SUFFIX_SIGNATURE_SIZE,
			post_entry.post_sign, INM_SUFFIX_SIGNATURE_SIZE);
		tmp_post[INM_SUFFIX_SIGNATURE_SIZE] = '\0';
		ERR("sign comparison failed - post[%s}", tmp_post);
		failed = 1;
	}

	if (failed) {
		DBG("Wrong mem free at [%s:%d]", func, line);
		dump_alloc_info(ptr);
		ASSERT(1);
	}
}

void
inm_verify_for_mem_leak_kfree(const void 	*ptr,
			      size_t 		mem_size,
			      inm_mem_flag_t	flags,
			      const char 	func[],
			      inm_32_t 		line,
			      inm_32_t		verbose)
{
	struct mdbg_pre_entry	*pre_entry_ptr;
	struct mdbg_post_entry	*post_entry_ptr;

	pre_entry_ptr = (struct mdbg_pre_entry *)
			((char *)ptr - sizeof(struct mdbg_pre_entry));
	post_entry_ptr = (struct mdbg_post_entry *)
			 ((char *)ptr + mem_size);

	inm_verify_mem(ptr, mem_size, func, line, verbose);

	DELETE_FROM_MEM_LIST(ptr, flags, mem_size);

	memzero((void *)ptr, mem_size);
	memcpy_s(pre_entry_ptr->pre_sign, INM_PREFIX_SIGNATURE_SIZE,
			INM_PREFIX_FREE_SIGNATURE, INM_PREFIX_SIGNATURE_SIZE);
	memcpy_s(post_entry_ptr->post_sign, INM_PREFIX_SIGNATURE_SIZE,
			INM_PREFIX_FREE_SIGNATURE, INM_PREFIX_SIGNATURE_SIZE);

	/*Put the free info in the pre signature only */
	memcpy_s(post_entry_ptr->post_func, INM_FUNC_LEN, func, INM_FUNC_LEN);
	post_entry_ptr->post_func[INM_FUNC_LEN - 1] = '\0';

	/* There can be memory alignment issues with post entry */
	INM_ASSIGN(&(post_entry_ptr->post_line), &line, inm_32_t);

	/* decrease the memory statistics */
	mem_size += sizeof(struct mdbg_pre_entry) +
		    sizeof(struct mdbg_post_entry);
	INM_FREE_MEM(pre_entry_ptr, mem_size, flags);
}

#endif

