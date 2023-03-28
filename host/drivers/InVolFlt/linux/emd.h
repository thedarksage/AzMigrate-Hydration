#ifndef _EMD_TARGET_H_
#define _EMD_TARGET_H_

/* This file should be in sync with dev_handlers/scst_utap.h file.
 * TODO: we should remove this file use driver scst/utap related fiesl.
 */

typedef unsigned long long emd_handle_t;

typedef struct emd_dev {
        struct list_head        dev_list;
        int                     dev_id;
        emd_handle_t            dev_handle;
        unsigned char           dev_name[256];
}emd_dev_t;

/* struct for Read/write data from emd driver */
typedef struct emd_io {
        unsigned long   eio_rw;
        void            *eio_iovp;
        unsigned int    eio_iovcnt;
        unsigned int    eio_len;
        emd_handle_t    eio_dev_handle;
        unsigned long long eio_start;
        const char *    eio_iname;
}emd_io_t;

typedef struct emd_dev_cap {
        inm_u32_t       bsize;
        inm_u64_t       nblocks;
        inm_u64_t       startoff;
}emd_dev_cap_t;


typedef struct emd_dev_type {
        int (*exec)(unsigned char *, unsigned char *);  // For vendor Commands.

        /* For attach() with given device name our filter driver will return target_context_t pointer.
         * Our us attach() will be similar to get_tgt_ctxt_from_uuid_nowait_fabric().
         */
        emd_handle_t (*attach)(const char *);

        void (*detach)(emd_handle_t);
        int (*get_capacity)(emd_handle_t, emd_dev_cap_t *);
	int (*prepare_write)(emd_handle_t,inm_u64_t, inm_u64_t);
        int (*exec_write)(emd_io_t *io);
        int (*exec_read)(emd_io_t *io);
        int (*exec_io_cancel)(emd_dev_t *, inm_u64_t,
                                inm_u64_t);
        int (*exec_vacp_write)(emd_handle_t, char *, loff_t);
        const char* (*get_path)(emd_handle_t, int *);

}emd_dev_type_t;

int emd_unregister_virtual_device(int dev_id);
int emd_register_virtual_device(char *name);
int emd_register_virtual_dev_driver(emd_dev_type_t *dev_type);
int emd_unregister_virtual_dev_driver(emd_dev_type_t *dev_type);
#endif // _EMD_TARGET_H_
