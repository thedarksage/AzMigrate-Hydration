#ifndef _VSNAP_OPEN_H_
#define _VSNAP_OPEN_H_

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
inm_32_t inm_vs_blk_open(struct inode *, struct file *);
inm_32_t inm_vs_blk_close(struct inode *inode, struct file *file);
inm_32_t inm_vv_blk_open(struct inode *, struct file *);
inm_32_t inm_vv_blk_close(struct inode *inode, struct file *file);
#else
inm_32_t inm_vs_blk_open(struct block_device *, fmode_t );
inm_32_t inm_vs_blk_close(struct gendisk *, fmode_t);
inm_32_t inm_vv_blk_open(struct block_device *, fmode_t );
inm_32_t inm_vv_blk_close(struct gendisk *, fmode_t);
#endif


#endif
