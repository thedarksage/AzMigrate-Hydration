#ifndef _FSTREAM_RAW
#define _FSTREAM_RAW

#include "osdep.h"
#include "flt_bio.h"

typedef struct fr_block {
    inm_bio_dev_t   *fb_disk;
    inm_u64_t       fb_offset;
} fr_block_t;

#define FSRAW_BLK_PER_PAGE              (PAGE_SIZE/sizeof(fr_block_t))
#define FSRAW_BLK_PER_PAGE_SHIFT        8 /* TODO: Make it dynamic */

#define FSRAW_BLK_MAP_OFF(hdl,offset)           /* Offset to block          */ \
    ((offset - hdl->frh_offset) >> hdl->frh_bshift)
#define FSRAW_BLK_PAGE(hdl, offset)             /* Offset to blkmap page    */ \
    (FSRAW_BLK_MAP_OFF(hdl, offset) >> FSRAW_BLK_PER_PAGE_SHIFT)
#define FSRAW_BLK_IDX(hdl, offset)              /* Offset to fr_block idx   */ \
    (FSRAW_BLK_MAP_OFF(hdl, offset)  & ~(UINT_MAX << FSRAW_BLK_PER_PAGE_SHIFT))

typedef struct fstream_raw_hdl {
    inm_spinlock_t  frh_slock;
    inm_u64_t       frh_fsize;                  /* File Size                */
    inm_u32_t       frh_bsize;                  /* Block Size               */
    inm_u32_t       frh_bshift;                 /* Block Size Shift         */
    inm_u64_t       frh_offset;                 /* Offset Mapped            */
    inm_u32_t       frh_len;                    /* Length Mapped            */
    inm_u32_t       frh_alen;                   /* Length aligned to block  */
    inm_u32_t       frh_nblks;                  /* Num Blocks               */
    inm_u32_t       frh_npages;                 /* Num Mapping Pages        */
    fr_block_t      **frh_blocks;               /* Mapping                  */
} fstream_raw_hdl_t;

void fstream_raw_map_bio(struct bio *);
inm_s32_t fstream_raw_open(char *, inm_u64_t, inm_u32_t, fstream_raw_hdl_t **);
inm_s32_t fstream_raw_get_fsize(fstream_raw_hdl_t *);
inm_s32_t fstream_raw_close(fstream_raw_hdl_t *);
inm_s32_t fstream_raw_read(fstream_raw_hdl_t *, char *, inm_u32_t, inm_u64_t);
inm_s32_t fstream_raw_write(fstream_raw_hdl_t *, char *, inm_u32_t, inm_u64_t);

#endif

