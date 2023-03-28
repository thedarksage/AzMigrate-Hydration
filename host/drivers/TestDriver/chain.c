/*
 * Works with 5.3
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
typedef struct gendisk bio_disk_t;
#define BIO_DISK(bio)           ((bio)->bi_disk)
#define BIO_SET_DISK(bio, bdev) bio_set_dev(bio, bdev)
#else
typedef struct block_device bio_disk_t;
#define BIO_DISK(bio)           ((bio)->bi_dev)
#define BIO_SET_DISK(bio, bdev) ((bio)->bi_dev = bdev)
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
#define ASRFILE "/dev/md0"
#define SLEEP_INTERVAL 2
#define CHAIN_LEN 4096
#define NTHREADS  8

struct task_struct *wthread[NTHREADS];
wait_queue_head_t shutdown_event[NTHREADS];
struct bio *bioaa[NTHREADS][CHAIN_LEN] = {0};
struct page *pageaa[NTHREADS][CHAIN_LEN] = {0};
int shutdown = 0;
struct file *bfile = NULL;
struct block_device *bbdev = NULL;
struct gendisk *gdisk = NULL;
int write = 0;

struct bio_pvt {
    struct block_device *bp_bdev;
    void                *bp_private;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
    void                (*bp_end_io)(struct bio *bio);
#else
    void                (*bp_end_io)(struct bio *bio, int);
#endif
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
asr_bio_endio(struct bio *bio, int error)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0) || defined SLES12SP4
if (error < 0)
        error = BLK_STS_IOERR;

    bio->bi_status = error;
    bio_endio(bio);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
    bio->bi_error = error;
    bio_endio(bio);
#else
    bio_endio(bio, error);
#endif
}

void
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
asr_clone_end_io(struct bio *cbio)
#else
asr_clone_end_io(struct bio *cbio, int error)
#endif
{
    struct bio *bio = cbio->bi_private;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
    int error = bio->bi_status;

    bio->bi_iter = cbio->bi_iter;
#else
    BIO_SECTOR(bio) = BIO_SECTOR(cbio);
    BIO_SIZE(bio) = BIO_SIZE(cbio);
    bio->bi_idx = cbio->bi_idx;
#endif

    asr_bio_endio(bio, error);
    bio_put(cbio);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
typedef blk_qc_t mrfr_t;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
typedef void mrfr_t;
#else
typedef int mrfr_t;
#endif

mrfr_t
asr_passthrough_clone_bio(struct bio *bio)
{
    struct bio *cbio = NULL;

    cbio = bio_clone_fast(bio, GFP_NOIO, NULL);
    if (!cbio) {
        asr_bio_endio(bio, -ENOMEM);
        return BLK_QC_T_NONE;
    }

    BIO_SET_DISK(cbio, bbdev);
    cbio->bi_private = bio;
    cbio->bi_end_io = asr_clone_end_io;
    bio_clear_flag(cbio, BIO_CHAIN);

    return submit_bio(cbio);
}
 
mrfr_t
asr_make_request(struct request_queue *queue, struct bio *bio)
{
        if (BIO_IS_WRITE(bio))
                    write = 1;

            return asr_passthrough_clone_bio(bio);
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
alloc_bio_list(int n, struct block_device *bdev, struct bio *bioa[],
               struct page *pagea[])
{
    char *buf = NULL;
    int i = 0;
    int j = 0;
    struct bio *biop = NULL;
    struct page *pagep = NULL;
    /*
     * Create a file and find its layout on disk and put the
     * sector and nsect here
     *
     * dd if=/dev/zero of=/mnt/write_same bs=16k count=1 oflag=sync
     *
     * hdparm --fibmap /mnt/write_same
     * /mnt/write_same:
     * FS blocksize 4096, begins at LBA 0; assuming 512 byte sectors
     *      byte_offset  begin_LBA    end_LBA    sectors
     *                0     266240     274431       8192
     */
    sector_t sector = 266240;

    for (i = 0; i < n; i++) {
        if (!pagea[i]) {
            pagep = alloc_pages(GFP_NOIO, 0);
            if (!pagep)
                break;

            pagea[i] = pagep;

            buf = kmap(pagep);
            for (j = 0; j < PAGE_SIZE; j++)
                buf[j] = (i % 10) + '0';
            kunmap(pagep);
        }

        biop = bio_alloc(GFP_NOIO, 1);
        if (!biop)
            break;

        BIO_SET_DISK(biop, bdev);
        bio_set_op_attrs(biop, REQ_OP_WRITE, 0);
        biop->bi_private = biop->bi_end_io = NULL;
        BIO_SECTOR(biop) = sector + (8 * i);
        if (PAGE_SIZE != bio_add_page(biop, pagea[i], PAGE_SIZE, 0)) {
            bio_put(biop);
            break;
        }

        bioa[i] = biop;
    }

    return i;
}

int
dowork(void *idxp)
{
    int idx = (uintptr_t)idxp;
    int kill = 0;
    int nr_bios = 0;
    int i = 0;
    int len = 1;
    struct block_device *bdev;
    struct file *file = NULL;
    struct bio **bioa = bioaa[idx];
    struct page **pagea = pageaa[idx];

    for (i = 0; i < CHAIN_LEN; i++) {
        bioa[i] = NULL;
        pagea[i] = NULL;
    }

    while (1) {
        kill = wait_event_interruptible_timeout(shutdown_event[idx], shutdown,
                                                SLEEP_INTERVAL * HZ);
        if (kill) {
            info("Die");
            break;
        }

        /* Start writing after a write has happened i.e. FS is mounted */
        if (!write)
            continue;

        file = fopen(ASRDEV, O_RDWR | O_SYNC);
        if (IS_ERR(file))
            return i;

        bdev = blkdev_get_by_dev((file_inode(file))->i_rdev, FMODE_WRITE, NULL);

        nr_bios = alloc_bio_list(len, bdev, bioa, pagea);

        //if (nr_bios == CHAIN_LEN)
        //    info("NR_BIOS: %d", nr_bios);

        if (nr_bios) {
            /* Create bio chain with bioa[0] as child and bioa[n] as ancestor*/
            for (i = 0; i < nr_bios - 1; i++) {
                //info("CHAIN: %p -> %p", bioa[i], bioa[i + 1]);
                bio_chain(bioa[i], bioa[i + 1]);
            }

            /* Send all the child IOs */
            for (i = nr_bios - 2; i >= 0; i--)
                submit_bio(bioa[i]);

            /* Wait on grand parent to finish processing complete chain */
            submit_bio_wait(bioa[nr_bios - 1]);

            for (i = 0; i < nr_bios; i++)
                bioa[i] = NULL;

        } else {
            info("Empty Bio List");
        }

        blkdev_put(bdev, FMODE_WRITE);
        fclose(file);

        if (len == CHAIN_LEN)
            len = 1;
        else
            len = len * 2;
    }

    for (i = 0; i < CHAIN_LEN; i++) {
        __free_pages(pagea[i], 0);
    }

    wake_up(&shutdown_event[idx]);
    return 0;
}

void
kill_thread(void)
{
    int i = 0;

    shutdown = 1;

    for (i = 0; i < NTHREADS; i++) {
        if (!wthread[i])
            continue;

        wake_up(&shutdown_event[i]);
        wait_event_interruptible_timeout(shutdown_event[i], shutdown,
                                         SLEEP_INTERVAL * HZ);
        kthread_stop(wthread[i]);
    }
}

int
create_thread(void)
{
    int error = -ENOMEM;
    int i = 0;

    for (i = 0; i < NTHREADS; i++) {
        init_waitqueue_head(&shutdown_event[i]);

        wthread[i] = kthread_run(dowork, (void *)((uintptr_t)i), "asrwork");
        if (IS_ERR(wthread[i])) {
            error = PTR_ERR(wthread[i]);
            wthread[i] = NULL;
        }

        error = 0; /* success if atleast one thread */
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
MODULE_LICENSE("GPL");
