/*
 * Works with RHEL7
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/bio.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>

typedef int inm_s32_t;
typedef unsigned int inm_u32_t;

/*
 * BIO
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)) || defined SLES12SP3

#ifndef REQ_OP_MASK
#define BIO_REQ_MASK            ((1 << REQ_OP_BITS) - 1)
#else
#define BIO_REQ_MASK            REQ_OP_MASK
#endif

#define BIO_RW(bio)             ((bio)->bi_opf)
#define BIO_OP(bio)             bio_op(bio)
#define BIO_IS_WRITE(bio)       (op_is_write(bio_op(bio)))

#define BIO_REQ_WRITE           REQ_OP_WRITE
#define BIO_REQ_WRITE_SAME      REQ_OP_WRITE_SAME
#define BIO_REQ_DISCARD         REQ_OP_DISCARD
#ifdef REQ_OP_WRITE_ZEROES
#define BIO_REQ_WRITE_ZEROES    REQ_OP_WRITE_ZEROES
#else
#define BIO_REQ_WRITE_ZEROES    0
#endif

#else  /* LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0) */

#ifndef bio_set_op_attrs
#define bio_set_op_attrs(bio, op, flags) (BIO_RW(bio) = op | flags)
#endif

#define BIO_REQ_MASK            0

#define BIO_RW(bio)             ((bio)->bi_rw)
#define BIO_OP(bio)             BIO_RW(bio)
#define BIO_IS_WRITE(bio)       (bio_rw(bio) == WRITE)

#define BIO_REQ_WRITE           REQ_WRITE

#ifdef RHEL5

#define BIO_REQ_DISCARD         0
#define BIO_REQ_WRITE_SAME      0
#define BIO_REQ_WRITE_ZEROES    0

#else  /* !RHEL5 */

#define BIO_REQ_DISCARD         REQ_DISCARD
#define BIO_REQ_WRITE_ZEROES    0

#ifdef REQ_WRITE_SAME
#define BIO_REQ_WRITE_SAME      REQ_WRITE_SAME
#else
#define BIO_REQ_WRITE_SAME      0
#endif

#endif /* !RHEL5 */
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)    */

#define BIO_OFFLOAD              \
    (BIO_REQ_DISCARD | BIO_REQ_WRITE_SAME | BIO_REQ_WRITE_ZEROES)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#define BVEC_ITER_SECTOR(iter)  ((iter).bi_sector)
#define BVEC_ITER_SZ(iter)      ((iter).bi_size)

#define BIO_ITER(bio)           ((bio)->bi_iter)

#define BIO_SECTOR(bio)         BVEC_ITER_SECTOR(BIO_ITER(bio)) 
#define BIO_SIZE(bio)           BVEC_ITER_SZ(BIO_ITER(bio))
#else
#define BIO_SECTOR(bio)         ((bio)->bi_sector)
#define BIO_SIZE(bio)           ((bio)->bi_size)
#endif

#define info(format, arg...) 						\
	printk(KERN_ERR "VDEV[%s:%d]:(INF) " format "\n",		\
		__FUNCTION__, __LINE__, ## arg)

#if 0
#define dbg(format, arg...) 						\
	printk(KERN_DEBUG "VDEV[%s:%d]:(INF) " format "\n",		\
		__FUNCTION__, __LINE__, ## arg)
#else 
#define dbg(format, arg...)
#endif

#define ASRDISK "asrdisk"
#define ASRDEV "/dev/asrdisk"
#define ASRFILE "/dev/sdb"
#define DISKSZ  (512 * 1024 *1024)
#define MAX_WRITE_SAME_SECS ((4 * 1024 * 1024) >> 9 )  
#define SLEEP_INTERVAL  243
#define MIN_IO_SECTORS  1
#define MAX_IO_SECTORS  8192

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0)) && !defined RHEL5
#if !((LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32)) && defined UEK)
#define Q_DISCARD_ZEROES_DATA
#endif
#endif

struct task_struct *wthread = NULL;
wait_queue_head_t shutdown_event;
int shutdown = 0;
struct mutex iomutex;
struct file *bfile = NULL;
struct block_device *bbdev = NULL;
int write = 0;
char *wsbuf = NULL;

struct bio_pvt {
    struct block_device *bp_bdev;
    void                *bp_private;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
    void                (*bp_end_io)(struct bio *bio);
#else
    void                (*bp_end_io)(struct bio *bio, int);
#endif
};       

struct bio_multi {
    struct completion   *bm_wait;
    unsigned long       bm_error;
    atomic_t            bm_done;
};


struct file *
fopen(char *fname, mode_t mode)
{
    mm_segment_t fs;
    void *file = NULL;

    fs = get_fs();
    set_fs (KERNEL_DS);

    file = filp_open(fname, mode, 0700);
    
    set_fs(fs);
    return file;
}

void
fclose(struct file *file)
{
    filp_close(file, NULL);
}

int asrdopen(struct block_device *, fmode_t);
void asrdclose(struct gendisk *, fmode_t);

struct block_device_operations asrdevops = {
        .open = asrdopen,
        .release = asrdclose,
        .owner = THIS_MODULE,
};

struct vdev {
    struct request_queue *queue;
    struct gendisk  *disk;
};

struct vdev vdev = {NULL, NULL};
int open = 0;

int
asrdopen(struct block_device *dev, fmode_t mode)
{
    dbg("Open");
    open++;
    return 0;
}

void
asrdclose(struct gendisk *disk, fmode_t mode)
{
    dbg("Close");
    if (open) {
        open--;
    }
}

void
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
asr_end_io(struct bio *bio)
#else
asr_end_io(struct bio *bio, int error)
#endif
{
    struct bio_pvt *pvt = bio->bi_private;

    bio->bi_bdev = pvt->bp_bdev;
    bio->bi_end_io = pvt->bp_end_io;
    bio->bi_private = pvt->bp_private;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
  	bio_endio(bio);
#else
  	bio_endio(bio, error);
#endif

    kfree(pvt);
}

void
asr_bio_endio(struct bio *bio, int error)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0) || defined SLES12SP4 
    bio->bi_status = BLK_STS_IOERR;
    bio_endio(bio);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
    bio->bi_error = error;
    bio_endio(bio);
#else
    bio_endio(bio, error);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
typedef blk_qc_t mrfr_t;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
typedef void mrfr_t;
#else
typedef int mrfr_t;
#endif

mrfr_t
asr_passthrough_bio(struct bio *bio)
{
    int error = 0;
    struct bio_pvt *pvt = NULL;
    struct request_queue *bq = bdev_get_queue(bbdev);
    
    pvt = kmalloc(sizeof(struct bio_pvt), GFP_NOIO);
    if (!pvt) {
        asr_bio_endio(bio, -ENOMEM);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
        return BLK_QC_T_NONE;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
        return;
#else
        typedef int -ENOMEM;
#endif
    }

    pvt->bp_bdev = bio->bi_bdev;
    pvt->bp_private = bio->bi_private;
    pvt->bp_end_io = bio->bi_end_io;

    bio->bi_bdev = bbdev;
    bio->bi_private = pvt;
    bio->bi_end_io = asr_end_io;
    
    return bq->make_request_fn(bq, bio);
}   

void 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
asr_multi_end_io(struct bio *bio)
#else
asr_multi_end_io(struct bio *bio, int error)
#endif
{
    struct bio_multi *biom = bio->bi_private;
    int nvec = bio->bi_vcnt;
    void *page = NULL;
    struct bio_vec *vec = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
    int error = bio->bi_error;
#endif

    if (error) {
        info("Multi Error: %d", error);
        biom->bm_error = error;
    }

    if (atomic_dec_and_test(&biom->bm_done))
        complete(biom->bm_wait);

    while (nvec) {
        vec = &(bio->bi_io_vec[nvec - 1]);
        page = (void *)kmap_atomic(vec->bv_page);
        /* We dont expect these pages from high mem so safe to unmap */
        kunmap_atomic(page);
        free_page((unsigned long)page);
        nvec--;
    }
    
    bio_put(bio);
}

mrfr_t
asr_passthrough_vector(struct bio *obio, struct bio_vec *vector)
{
    int error = 0;
    struct bio *bio = NULL;
    struct bio_multi biom;
    off_t sector = BIO_SECTOR(obio);
    int len = BIO_SIZE(obio);
    struct request_queue *bq = bdev_get_queue(bbdev);
    int max_vecs = bq->limits.max_hw_sectors >> (PAGE_SHIFT - 9);
    DECLARE_COMPLETION_ONSTACK(wait);
    char *opage = NULL;
    char *page = NULL;
    int poffset = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
    mrfr_t ret;
#endif

    atomic_set(&biom.bm_done, 1);
    biom.bm_error = 0;
    biom.bm_wait = &wait;

    while (len != 0) {
        bio = bio_alloc(GFP_NOIO, min(len >> 9, max_vecs));
        if (!bio) {
            error = -ENOMEM;
            break;
        }

        BIO_SECTOR(bio) = sector;
        bio->bi_bdev   = bbdev;
        bio->bi_end_io = asr_multi_end_io;
        bio->bi_private = &biom;
        bio_set_op_attrs(bio, BIO_REQ_WRITE, 0);

        while (len != 0) {
            page = (char *)__get_free_page(GFP_NOIO);
            if (!page) {
                error = -ENOMEM;
                break;
            }

            poffset = 0;
            opage = kmap(vector->bv_page); 
            while (poffset < PAGE_SIZE && poffset < len) {
                memcpy(page + poffset, opage + vector->bv_offset, 
                       vector->bv_len);
                poffset += vector->bv_len;
            }
            kunmap(vector->bv_page);

            if (poffset != bio_add_page(bio, virt_to_page(page), poffset, 0)) {
                free_page((unsigned long)page);
                break;
            }
            
            len -= poffset;
            sector += (poffset >> 9);
        }

        if (error)
            break;

        atomic_inc(&biom.bm_done);
        
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
        ret = bq->make_request_fn(bq, bio);
#else
        bq->make_request_fn(bq, bio);
#endif
    }

    if (!atomic_dec_and_test(&biom.bm_done))
        wait_for_completion_io(&wait);

    if (!error)
        error = biom.bm_error;

    if (!error)
        BIO_SIZE(obio) = 0;
        
    asr_bio_endio(obio, error);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) ||\
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
    return ret;
#endif
}


mrfr_t
asr_make_request(struct request_queue *queue, struct bio *bio)
{
    int len = BIO_SIZE(bio);
    struct bio *clone = NULL;
    struct request_queue *bq = bdev_get_queue(bbdev);
    char *src = NULL;
    char *dest= NULL;
    int error = 0;
    struct bio_vec vec;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
    mrfr_t ret;
#endif

    if (BIO_IS_WRITE(bio))
        write = 1;

    if (BIO_OP(bio) & BIO_OFFLOAD) {
        if (BIO_OP(bio) & BIO_REQ_WRITE_SAME) {
            info("WRITE_SAME: %llu->%lu", 
                (unsigned long long)BIO_SECTOR(bio) >> 9, 
                (unsigned long)BIO_SIZE(bio)); 
            vec = bio->bi_io_vec[0];
        } else {
            if (BIO_OP(bio) & BIO_REQ_WRITE_ZEROES)
                info("WRITE_ZEROES: %llu->%lu", 
                    (unsigned long long)BIO_SECTOR(bio) >> 9, 
                    (unsigned long)BIO_SIZE(bio));
            else if (BIO_OP(bio) & BIO_REQ_DISCARD)
                info("DISCARD: %llu->%lu", 
                    (unsigned long long)BIO_SECTOR(bio) >> 9, 
                    (unsigned long)BIO_SIZE(bio));
            else
                info("UNSUPP: %llu->%lu", 
                    (unsigned long long)BIO_SECTOR(bio) >> 9, 
                    (unsigned long)BIO_SIZE(bio));

            vec.bv_page = ZERO_PAGE(0);
            vec.bv_offset = 0;
            vec.bv_len = 512;
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
        ret = asr_passthrough_vector(bio, &vec);
#else
		asr_passthrough_vector(bio, &vec);
#endif
        info("OFFLOAD: END");
    } else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
        ret = asr_passthrough_bio(bio);
#else
        asr_passthrough_bio(bio);
#endif
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || \
    LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
    return ret;
#endif
}
    
int
delete_dev(void)
{
    if (open)
        return -EBUSY;

    if (bfile) {
        blkdev_put(bbdev, FMODE_WRITE);
        bbdev = NULL;
        fclose(bfile);
        bfile = NULL;
    }

    if (vdev.disk) {
	    del_gendisk(vdev.disk);
	    put_disk(vdev.disk);
        vdev.disk = NULL;
    }

    if (vdev.queue) {
        blk_cleanup_queue(vdev.queue);
        vdev.queue = NULL;
    }

    return 0;
}

int
create_dev(void)
{
    int error = 0;
    static int dmajor = -1;
    static int dminor = 0;
    loff_t size = 0;
    struct request_queue *bq = NULL;

    bfile = fopen(ASRFILE, O_RDWR | O_SYNC);
    if (IS_ERR(bfile)) {
        info("Cannot Open %s: %ld", ASRFILE, PTR_ERR(bfile));
        error = PTR_ERR(bfile);
        bfile = NULL;
        goto out;
    }

    if (!S_ISBLK(file_inode(bfile)->i_mode)) {
        error = -ENODEV;
        goto out;
    }

    bbdev = blkdev_get_by_dev((file_inode(bfile))->i_rdev, FMODE_WRITE, NULL);
    size = i_size_read(bbdev->bd_inode);

    if (dmajor < 0) {
    	dmajor = register_blkdev(0, ASRDISK);
	    if (dmajor <= 0) {
		    info("register_blkdev failed");
		    error = -EIO;
	    	goto error;
        }
	}

	if (!(vdev.queue = blk_alloc_queue(GFP_KERNEL))) {
		info("blk alloc que failed");
		error = -ENOMEM;
		goto error;
	}

    /* Get chars of original device */
    bq = bdev_get_queue(bbdev);
    memcpy(&(vdev.queue->limits), &(bq->limits), sizeof(struct queue_limits));

	vdev.disk = alloc_disk(16);
	if (!vdev.disk) {
		info("alloc_disk failed.");
		error = -ENOMEM;
		goto error;
	}

	vdev.disk->major = dmajor;
	vdev.disk->first_minor = dminor;
	vdev.disk->fops = &asrdevops;
	sprintf(vdev.disk->disk_name, "%s", ASRDISK);
	//vdev.disk->private_data = vdev;
	vdev.disk->queue = vdev.queue;

	blk_queue_make_request(vdev.queue, asr_make_request);
    /* queue limits */
    blk_queue_max_write_same_sectors(vdev.queue, MAX_WRITE_SAME_SECS);
    //if (BIO_REQ_WRITE_ZEROES != 0)
      //  blk_queue_max_write_zeroes_sectors(vdev.queue, MAX_WRITE_SAME_SECS);
#ifdef Q_DISCARD_ZEROES_DATA
    queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, vdev.queue);
    blk_queue_max_discard_sectors(vdev.queue, MAX_WRITE_SAME_SECS);
    vdev.queue->limits.discard_zeroes_data = 1;
#endif

	//vdev.queue->queuedata = vdev;

    info("Disk Capacity = %llu", size);
	set_capacity(vdev.disk, size >> 9);

    add_disk(vdev.disk);

out:
    return error;

error:
    delete_dev();
    goto out;
}

int
dowork(void *notused)
{
    int kill = 0;
    struct file *file = NULL;
    struct block_device *bdev = NULL;
    char *buf = NULL;
    int count = 0;
    int i = 0;
    int nsect = MIN_IO_SECTORS;
    int op = 0;

    buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    while (1) {
        kill = wait_event_interruptible_timeout(shutdown_event, shutdown, 
                                                SLEEP_INTERVAL * HZ);
        if (kill) {
            info("Die");
            break;
        }

        /* Start writing after a write has happened i.e. FS is mounted */
        if (!write)
            continue;

        for (i = 0; i < PAGE_SIZE; i++)
            buf[i] = count + '0';

        count++;
        if (count == 10)
            count = 0;

        file = fopen(ASRDEV, O_RDWR | O_SYNC);
        if (IS_ERR(file)) {
            info("Cannot Open %s: %ld", ASRDEV, PTR_ERR(file));
            file = NULL;
            continue;
        }

        bdev = blkdev_get_by_dev((file_inode(file))->i_rdev, FMODE_WRITE, NULL);

        /* 
         * Create a file and find its layout on disk and put the 
         * sector and nsect here
         *
         * dd if=/dev/zero of=/mnt/write_same bs=16k count=1 oflag=sync
         *
         * hdparm --fibmap /mnt/write_same
         * /mnt/write_same:                                                      
         * filesystem blocksize 4096, begins at LBA 0; assuming 512 byte sectors
         *      byte_offset  begin_LBA    end_LBA    sectors                  
         *                0     266240     274431       8192
         */
        if (op == 0) {
#if (BIO_REQ_WRITE_ZEROES != 0) 
            __blkdev_issue_zeroout(bdev, 266240, nsect, GFP_KERNEL, 0);
            op++;
        } else if (op == 1) {
#else
            op++;
#endif
            blkdev_issue_write_same(bdev, 266240, nsect, GFP_KERNEL, 
                                    virt_to_page(buf));
            op++;
#ifdef Q_DISCARD_ZEROES_DATA
        } else if (op == 2) {
            blkdev_issue_discard(bdev, 266240, nsect, GFP_KERNEL, 
                                 BLKDEV_DISCARD_ZERO);
#endif
            op = 0;
        }

        nsect *= 2;
        if (nsect > MAX_IO_SECTORS)
            nsect = MIN_IO_SECTORS;
        
        blkdev_put(bdev, FMODE_WRITE);
        fclose(file);
    }

    return 0;
}

void
kill_thread(void)
{
    shutdown = 1;
    wake_up(&shutdown_event);
    //msleep_interruptible(HZ);
    kthread_stop(wthread);
}

int
create_thread(void)
{
    int error = 0;

    init_waitqueue_head(&shutdown_event);

    wthread = kthread_run(dowork, NULL, "asrwork");
    if (IS_ERR(wthread)) {
        error = PTR_ERR(wthread);
        wthread = NULL;
    }

    return error;
}
    
int __init
inm_vdev_driver_init(void)
{
	int error = 0;

	info("successfully loaded module");

    error = create_thread();
    if (error)
        goto out;

    error = create_dev();

out:
	return error;
}

void __exit
inm_vdev_driver_exit(void)
{
    kill_thread();
    delete_dev();
}

module_init(inm_vdev_driver_init);
module_exit(inm_vdev_driver_exit);

