/***********************************************************************************************************************
*
* Copyright 2003-2014 Viking Technology, All Rights Reserved.
*
* This software is furnished under license and may be used and
* copied only in accordance with the following terms and conditions.
* Subject to these conditions, you may download, copy, install,
* use, modify and distribute modified or unmodified copies of this
* software in source and/or binary form.  No title or ownership is
* transferred hereby.
*
*  1) Any source code used, modified or distributed must reproduce
*     and retain this copyright notice and list of conditions
*     as they appear in the source file.
*
*  2) No right is granted to use any trade name, trademark, or
*     logo of Viking Technology.  The " Viking Technology "
*     name may not be used to endorse or promote products derived
*     from this software without the prior written permission of
*     Viking Technology.
*
*  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
*     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
*     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT
*     SHALL VIKING TECHNOLOGY BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN
*     PARTICULAR, VIKING TECHNOLOGY SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
*     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
*     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
*     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
*     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
*     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF
*     THE POSSIBILITY OF SUCH DAMAGE.
*
********************************************************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#include <linux/fs.h>
#else
#include <linux/buffer_head.h>
#endif
#include <linux/hdreg.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>

#include "arxcis_b.h"

#define VERSION_STR          "1.0"

/* Enable or disable debug print statements */
#define ARX_DEBUG 1
#if ARX_DEBUG
#define DPRINTK(fmt, arg...) printk("%s: debug: " fmt, ARX_PREFIX, ## arg)
#else
#define DPRINTK(fmt, arg...) (void)0
#endif

#define ARX_PREFIX           "arxcis_b"
#define DEVICE_NAME          "arxcis_b"
#define PROC_NODE            "arxcis_b"

enum { /* ioremap_mode */
    IOREMAP_NOCACHE    = 1,
    IOREMAP_WC         = 2,
    IOREMAP_CACHE      = 3,
};

enum  { /* E820 types to search for  Arxcis NVDIMM */
    E820_TYPE_2       = 2,
    E820_TYPE_12      = 12,
    E820_TYPE_90      = 90,
};
#define NUM_E820_TYPES  3
int e820_types[NUM_E820_TYPES] = {E820_TYPE_12, E820_TYPE_90, E820_TYPE_2};

#define BYTES_PER_SECTOR     512
#define SECTOR_SHIFT         9  /* multiply or divide by 512 */
#define PAGE_SECTORS_SHIFT   (PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS         (1 << PAGE_SECTORS_SHIFT)

#define SIZE_1MB             (1024 * 1024)
#define SIZE_1GB             (1024 * 1024 * 1024)
#define SEARCH_INC           (512 * SIZE_1MB) /* Memory search increment, in MB */

/* #defines to cleanly support bio structure changes between kernels */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
#define BIO_SECTOR(bio)          bio->bi_iter.bi_sector
#define BVEC_CUR_SECTORS(biovec) ((biovec.bv_len) >> SECTOR_SHIFT)
#define BVEC_LEN(biovec)         (biovec.bv_len)
#else
#define BIO_SECTOR(bio)          bio->bi_sector
#define BVEC_CUR_SECTORS(biovec) ((biovec->bv_len) >> SECTOR_SHIFT)
#define BVEC_LEN(biovec)         (biovec->bv_len)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#define BIO_KMAP(page)      kmap_atomic(page)
#define BIO_KUNMAP(buffer)  kunmap_atomic(buffer)
#else
#define BIO_KMAP(page)      kmap_atomic(page, KM_USER0)
#define BIO_KUNMAP(buffer)  kunmap_atomic(buffer, KM_USER0)
#endif

/* #defines used in debug print statements to display offsets of each NVRamDisk within entire NVDIMM space */
#define ARX_BYTE_OFFSET(x)   ((x)-arxcis_viraddr)
#define ARX_SECTOR_OFFSET(x) (((x)-arxcis_viraddr)/BYTES_PER_SECTOR)

#define ARX_MAX_DEVS            128 /* Default Max number of NVRamDisks that can be created */
#define INVALID_CDQUERY_IOCTL   0x5331 /* Debug Only: To display IOCTL name being called */

static DEFINE_MUTEX(proc_mutex);
static DEFINE_MUTEX(list_mutex);

/* The structure of arxcis device */
struct arxcis_device {
    int index;    /* arxcis disk number (i.e. arxcis_b#)*/
    struct request_queue *rq;
    struct workqueue_struct *wq;
    struct work_struct  work;
    struct gendisk *disk;
    struct list_head list;

    /* virtual address based from global arxcis_viraddr (Actual address, NOT Offset) */
    char __iomem *viraddr_start;
    char __iomem *viraddr_end;
    uint64_t nr_bytes;
};

static uint64_t arxcis_phyaddr_start = 0; /* physical addr of Arxcis NVDIMM */
static uint64_t arxcis_phyaddr_nr_bytes = 0;

/* virtual addr of Arxcis NVDIMM after ioremap.  */
static char __iomem *arxcis_viraddr = NULL;

static int arx_ma_no = 0; /* major no. */
static int arx_dev_cnt = 0; /* no. of attached devices */
struct proc_dir_entry *arx_proc = NULL;
static LIST_HEAD(arxcis_list);  /* list of arxcis_devices */

/* On some systems, request_mem_region() fails for some unknown reason.
 * Bypass this operation in that environment for now, until we can find
 * the root-cause.  THIS IS A WORKAROUND: NOT A PERMANENT SOLUTION */
static int arx_request_mem_region = 1;

/*********************************************************************************
 * Function Declarations
 ********************************************************************************/

static int arx_attach_device(int num, int nr_sectors);
static int arx_detach_specific_device(int num);
static int arx_detach_all_devices(void);

/*********************************************************************************
 * Supported module params
 ********************************************************************************/

static int max_devs = ARX_MAX_DEVS;
module_param(max_devs, int, S_IRUGO);
MODULE_PARM_DESC(max_devs,
    " Total NVRamDisk devices available for use. (Default = 128)");

static int ioremap_mode = IOREMAP_CACHE;
module_param(ioremap_mode, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(ioremap_mode,
    " Cache attribute of NVRamDisk. \n"
    "\t\t   1-Nocache \n"
    "\t\t   2-Write Combine\n"
    "\t\t   3-Cache (Default)");

static int create_disk = 1;
module_param(create_disk, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(create_disk,
    " Create 1 NVRamDisk device using entire NVDIMM at driver load. \n"
    "\t\t   1-NVRamDisk created (Default)\n"
    "\t\t   0-NVRamDisk NOT created");



/*********************************************************************************
 * Functions Definitions
 ********************************************************************************/

/*****  Proc Functions *******/


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t
arx_write_proc(struct file *file, const char __user *buffer, size_t count, loff_t *data){
#else
static int
arx_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data){
#endif
    int num, size, err = (int)count;
    char *ptr, *buf;

    DPRINTK("Entering %s\n", __func__);

    mutex_lock(&proc_mutex);

    if (!buffer || count > PAGE_SIZE)
        return -EINVAL;

    buf = (char *)__get_free_page(GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if(copy_from_user(buf, buffer, count))
        return -EFAULT;

    if(!strncmp("arxcis_b attach", buffer, 15)) {
        ptr = buf + 16;
        num = simple_strtoul(ptr, &ptr, 0);
        size = simple_strtoul(ptr + 1, &ptr, 0);

        if(arx_attach_device(num, size) != 0){
            printk(KERN_ERR "%s: Unable to attach %s%d\n", ARX_PREFIX, DEVICE_NAME, num);
            err = -EINVAL;
        }
    }else if(!strncmp("arxcis_b detach all", buffer, 19)) {
        if(arx_detach_all_devices() != 0){
            printk(KERN_ERR "%s: Unable to detach all\n", ARX_PREFIX);
            err = -EINVAL;
        }
    }else if(!strncmp("arxcis_b detach", buffer, 15)) {
        ptr = buf + 16;
        num = simple_strtoul(ptr, &ptr, 0);

        if(arx_detach_specific_device(num) != 0){
            printk(KERN_ERR "%s: Unable to detach %s%d\n", ARX_PREFIX, DEVICE_NAME, num);
            err = -EINVAL;
        }
    }else{
        printk(KERN_ERR "%s: Unsupported command: %s\n", ARX_PREFIX, buffer);
        err = -EINVAL;
    }

    free_page((unsigned long)buf);
    mutex_unlock(&proc_mutex);

    return err;
} // arx_write_proc //


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t
arx_read_proc(struct file *file, char __user *buf, size_t size, loff_t *ppos){
#else
static int
arx_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data){
#endif
    int len;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    char page[BYTES_PER_SECTOR] = {'\0'};
#endif

    DPRINTK("Entering %s\n", __func__);
    len = sprintf(page,
        "ArxcisDisk (arxcis_b) %s\n\n"
        "Maximum Number of Attachable NVRamDisk Devices: %d\n"
        "Number of Attached NVRamDisk Devices: %d\n", VERSION_STR, max_devs, arx_dev_cnt);

    /* Display existing device info */
    len += sprintf(page+len, "       Device:   Start    -     End:    # Sectors:   #GB\n");

    mutex_lock(&list_mutex);
    if (!list_empty(&arxcis_list)) {
        struct arxcis_device *arxcis;
        list_for_each_entry(arxcis, &arxcis_list, list) {
            len+= sprintf(page+len, "    %s%d: 0x%08lx - 0x%08lx: 0x%08lx   %lu.%1lu\n", DEVICE_NAME, arxcis->index,
                ARX_SECTOR_OFFSET(arxcis->viraddr_start), ARX_SECTOR_OFFSET(arxcis->viraddr_end),
                (unsigned long int) arxcis->nr_bytes/BYTES_PER_SECTOR,
                (unsigned long int) arxcis->nr_bytes/SIZE_1GB,
                (unsigned long int) arxcis->nr_bytes%SIZE_1GB);
             /* NOTE: Since Linux does not gracefully support floating point numbers, using modulo to get fraction.
              *       Need to find a way to truncate, as %# only denoted minimum # digits */
        }
    }
    mutex_unlock(&list_mutex);

    /* Display total NVDIMM device info */
    len += sprintf(page+len, " Total NVDIMM: 0x%08x - 0x%08lx: 0x%08lx   %lu.%1lu\n\n", 0,
        (unsigned long int) arxcis_phyaddr_nr_bytes/BYTES_PER_SECTOR,
        (unsigned long int) arxcis_phyaddr_nr_bytes/BYTES_PER_SECTOR,
        (unsigned long int) arxcis_phyaddr_nr_bytes/SIZE_1GB,
        (unsigned long int) arxcis_phyaddr_nr_bytes%SIZE_1GB);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    len = simple_read_from_buffer(buf, size, ppos, page, len);
#endif
    return len;
} // arx_read_proc //


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    static const struct file_operations proc_fops = {
        .read =   arx_read_proc,
        .write =  arx_write_proc,
        .llseek = default_llseek,
    };
#endif


static int
arx_init_proc(void) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    if((arx_proc = proc_create (PROC_NODE, S_IRWXU | S_IRWXG | S_IRWXO, NULL, &proc_fops)) == NULL){
#else
    if((arx_proc = create_proc_entry (PROC_NODE, S_IRWXU | S_IRWXG | S_IRWXO, NULL)) == NULL){
#endif
            printk(KERN_ERR "%s: Error: proc entry %s not created\n", ARX_PREFIX, PROC_NODE);
            return -1;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        arx_proc->read_proc = arx_read_proc;
        arx_proc->write_proc = arx_write_proc;
#endif
    return 0;
} // arx_init_proc //

/*****  I/O Functions *******/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
static void
#else
static int
#endif
arx_bio_request(struct request_queue *q, struct bio *bio)
{
    struct arxcis_device *arxcis = q->queuedata;
    char *buffer;
    char __iomem *arxcis_ptr;
    int rw;
    sector_t sector = BIO_SECTOR(bio);
    int err = -EIO;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    struct bio_vec bvec;
    struct bvec_iter iter;
#else
    struct bio_vec *bvec;
    int iter;
#endif

    if ((sector + bio_sectors(bio)) > (arxcis->nr_bytes >> SECTOR_SHIFT)) {
        printk("%s:%d: Copy beyond end of device. (%x, %x).", ARX_PREFIX, arxcis->index,
            (unsigned int)(sector + bio_sectors(bio)),
            (unsigned int)arxcis->nr_bytes >> SECTOR_SHIFT);
        goto bio_out;
    }

    rw = bio_rw(bio);
    if (rw == READA)
        rw = READ;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    bio_for_each_segment(bvec, bio, iter) {
        buffer = BIO_KMAP(bvec.bv_page) + bvec.bv_offset;
        arxcis_ptr = arxcis->viraddr_start + (iter.bi_sector << SECTOR_SHIFT);
        if (bio_data_dir(bio) == WRITE) {
            memcpy_toio(arxcis_ptr, buffer, BVEC_LEN(bvec));
        } else {
            memcpy_fromio(buffer, arxcis_ptr, BVEC_LEN(bvec));
        }
        BIO_KUNMAP(buffer);
    }
#else
    sector = BIO_SECTOR(bio) << 9;
    /* Process all bio segments and transfer all data*/
    bio_for_each_segment(bvec, bio, iter) {
        buffer = BIO_KMAP(bvec->bv_page) + bvec->bv_offset;
        arxcis_ptr = arxcis->viraddr_start + sector;
        if (bio_data_dir(bio) == WRITE) {
            //DPRINTK("%d:%ld: WRITE TOIO %p, %p, 0x%x\n", arxcis->index, sector << SECTOR_SHIFT, arxcis_ptr, buffer, BVEC_LEN(bvec));
            memcpy_toio(arxcis_ptr, buffer, BVEC_LEN(bvec));
        } else {
            //DPRINTK("%d:%ld: READ FROMIO %p, %p, 0x%x\n", arxcis->index,sector << SECTOR_SHIFT, buffer, arxcis_ptr, BVEC_LEN(bvec));
            memcpy_fromio(buffer, arxcis_ptr, BVEC_LEN(bvec));
        }
        sector += BVEC_LEN(bvec);
        BIO_KUNMAP(buffer);
    }
#endif
    err = 0;

bio_out:
    set_bit(BIO_UPTODATE, &bio->bi_flags);
    bio_endio(bio, err);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
    return 0;
#endif
} // arx_bio_request //

/*****  Device Operation Functions *******/

int
arx_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    loff_t size;
    struct arxcis_device *arxcis = bdev->bd_disk->private_data;

    switch (cmd) {
    case BLKGETSIZE:
        /* return device size/512 (long *arg) */
        size = arxcis->nr_bytes / BYTES_PER_SECTOR;

        DPRINTK("%d:%s: BLKGETSIZE  %ld\n", arxcis->index, __func__, (unsigned long int)size);
        if ((size >> 9) > ~0UL)
            return -EFBIG;
        return copy_to_user ((void __user *)arg, &size, sizeof(size)) ? -EFAULT : 0;

    case BLKGETSIZE64:
        /* return device size in bytes (u64 *arg) */
        DPRINTK("%d:%s: BLKGETSIZE64  %llu\n", arxcis->index, __func__, arxcis->nr_bytes);
        return copy_to_user ((void __user *)arg, &arxcis->nr_bytes, sizeof(arxcis->nr_bytes)) ? -EFAULT : 0;

    case INVALID_CDQUERY_IOCTL:
        //DPRINTK("%d:%s: CDQUERY IOCTL unsupported\n", arxcis->index, __func__);
        return -EINVAL;

    case BLKFLSBUF:
        //DPRINTK("%d:%s: BLKFLSBUF IOCTL unsupported\n", arxcis->index, __func__);
        return -ENOTTY;

    case BLKPBSZGET:
    case BLKBSZGET:
    case BLKSSZGET:
        /* get block device sector size */
        size = BYTES_PER_SECTOR;

        DPRINTK("%d:%s: BLKxSZGET %ld\n", arxcis->index, __func__, (unsigned long int) size);
        return copy_to_user ((void __user *)arg, &size, sizeof(size)) ? -EFAULT : 0;
    }

    DPRINTK("%s: unsupported ioctl 0x%x: return -ENOTTY\n", __func__, cmd);

    return -ENOTTY; /* unknown command */
} // arx_ioctl //

/*
 *  open character device to auto ARM 
 **/

static int arcxcis_open(char *path)
{
	mm_segment_t oldfs;
	int dimm_index;
	int	savemode = HOST_INTERRUPT_SAVE | POWER_FAIL_SAVE;
	struct arxcis_info_user info;
	struct arxcis_reg reg_transaction = {0};

	struct file *fp = filp_open(path, 0, O_RDWR);
	if (IS_ERR(fp)) {
		return (-1);
	}
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp->f_op->unlocked_ioctl(fp, ARXCISGETINFO, (unsigned long)&info);

	for(dimm_index = 0; dimm_index < info.num_nv_dimms; dimm_index++) {
		reg_transaction.dimm_index = dimm_index;
		reg_transaction.reg = MOD_CONFIG_OFFSET;
		fp->f_op->unlocked_ioctl(fp, ARXCISGETREG, (unsigned long)&reg_transaction);
		msleep(1000);

		switch(savemode) {
		case DISABLE_SAVE:
			reg_transaction.value = (SAVDIS_BIT | AUTO_SAVE_BIT);
			break;
		case HOST_INTERRUPT_SAVE:
			reg_transaction.value = 0;
			break;
		case POWER_FAIL_SAVE:
			reg_transaction.value = AUTO_SAVE_BIT | INTMASK_BIT;
			break;
		case CKE_SAVE:
			reg_transaction.value = CKE_BIT | INTMASK_BIT;
			break;
		case HOST_INTERRUPT_SAVE | POWER_FAIL_SAVE:
			reg_transaction.value = AUTO_SAVE_BIT;
			break;
		case FORCE_SAVE_ONLY:
			reg_transaction.value = INTMASK_BIT;
			break;
		default:
			goto end;
		}
		fp->f_op->unlocked_ioctl(fp, ARXCISPUTREG, (unsigned long)&reg_transaction);
		msleep(1000);
	}
end:
	set_fs(oldfs);
	filp_close(fp, 0);
	return (0);
}

static long arx_direct_access(struct block_device *bdev, sector_t sector,
	void **kaddr, unsigned long *pfn, long size)
{
    struct arxcis_device *arxcis = bdev->bd_disk->private_data;
    size_t offset = sector << 9;

    if (!arxcis) return ENODEV;

    *kaddr = (void __force *) arxcis->viraddr_start + offset;
    *pfn = (arxcis_phyaddr_start + offset) >> PAGE_SHIFT;
	
    return arxcis_phyaddr_nr_bytes - offset;
}

/* SSD's don't have geometry, but older bootloader/partitioning tools need this info.
 * So make up some geometry such that C*H*S = disk capacity */
int
arx_getgeo (struct block_device *bdev, struct hd_geometry *geo) {
    DPRINTK("Entering %s\n", __func__);
    geo->heads = (u8) (1 << 2);
    geo->sectors = (u8) (1 << 4);
    geo->cylinders = get_capacity(bdev->bd_disk) >> 6;
    geo->start = 0;
    return 0;
} // arx_getgeo //

static const struct block_device_operations arx_bdev_fops = {
    .owner = THIS_MODULE,
    .ioctl = arx_ioctl,
    .getgeo = arx_getgeo,
    .direct_access = arx_direct_access,
};

/*****  Device Functions *******/

void arx_dump_dev_offsets(struct arxcis_device *arxcis) {
    DPRINTK("%d: (viraddr) start: %p, off_end: %p\n", arxcis->index, arxcis->viraddr_start, arxcis->viraddr_end);
    DPRINTK("%d: (sector offsets) start: 0x%lx, end: 0x%lx, sectors 0x%lx\n", arxcis->index,
        ARX_SECTOR_OFFSET(arxcis->viraddr_start),
        ARX_SECTOR_OFFSET(arxcis->viraddr_end),
        (unsigned long int) arxcis->nr_bytes/BYTES_PER_SECTOR);
    DPRINTK("%d: (byte offsets)   start: 0x%lx, end: 0x%lx, bytes 0x%lx\n", arxcis->index,
        ARX_BYTE_OFFSET(arxcis->viraddr_start),
        ARX_BYTE_OFFSET(arxcis->viraddr_end),
        (unsigned long int) arxcis->nr_bytes);
}

/**
 * arx_attach_device: Adds a device to the tail end of free buffer space
 * @num: unique disk number (i.e. /dev/arxcis_bN)
 * @nr_sectors: size of NVRamDisk device in sectors
 *
 * NOTE: In a multi-device configuration, if a device was deleted in any space
 *           before the last device (leaving a hole), a new device will NOT be created
 *           in that hole.  Devices are only created at the tail end, not in the middle.
**/
static int
arx_attach_device(int num, int nr_sectors) {
    struct arxcis_device *arxcis, *tmp_dev = NULL;
    struct gendisk *disk;

    DPRINTK("%d: Entering %s: num %d, nr_sectors 0x%lx\n", num, __func__, num, (unsigned long int) nr_sectors);

    if(arx_dev_cnt > max_devs){
        printk(KERN_WARNING "%s:%d: reached maximum number of attached disks. unable to attach more.\n", ARX_PREFIX, num);
        goto out;
    }

    /* See if a device with the given index already exists.
     * If not, derive start addr of new device from end addr of last previous device */
    mutex_lock(&list_mutex);
    if (!list_empty(&arxcis_list)) {
        list_for_each_entry(tmp_dev, &arxcis_list, list) {
            if(tmp_dev->index == num){
                printk(KERN_WARNING "%s: device %s%d already exists.\n", ARX_PREFIX, DEVICE_NAME, num);
                mutex_unlock(&list_mutex);
                goto out;
            }
            /* We want to exit loop pointing to the last item, instead of garbage */
            if (list_is_last(&tmp_dev->list, &arxcis_list)) {
                break;
            }
        }
    }
    mutex_unlock(&list_mutex);

    /* Allocate and setup device structure */
    arxcis = kzalloc(sizeof(*arxcis), GFP_KERNEL);
    if(!arxcis)
        goto out;
    arxcis->index = num;
    arxcis->nr_bytes = (uint64_t) nr_sectors * BYTES_PER_SECTOR;
    arxcis->viraddr_start = (tmp_dev) ? tmp_dev->viraddr_end+1 : arxcis_viraddr;
    arxcis->viraddr_end = arxcis->viraddr_start + arxcis->nr_bytes-1;
    if ((arxcis->viraddr_end >= arxcis_viraddr + arxcis_phyaddr_nr_bytes)) {
        printk(KERN_WARNING "%s: Disk %s%d not created. Exceeds end of disk\n", ARX_PREFIX, DEVICE_NAME, num);
        arx_dump_dev_offsets(arxcis);
        goto out_free_dev;
    }

    /* Setup request queue*/
    arxcis->rq = blk_alloc_queue(GFP_KERNEL);
    if(!arxcis->rq)
        goto out_free_dev;

    arxcis->rq->queuedata = arxcis;  /* setup to retrieve device from rq */
    blk_queue_make_request(arxcis->rq, arx_bio_request);
    blk_queue_logical_block_size (arxcis->rq, BYTES_PER_SECTOR);
    /* Allocate and setup gendisk */
    if (!(disk = arxcis->disk = alloc_disk (1)))
        goto out_free_queue;
    disk->major         = arx_ma_no;
    disk->first_minor   = num;
    disk->fops          = &arx_bdev_fops;
    disk->private_data  = arxcis;
    disk->queue         = arxcis->rq;
    disk->flags |= GENHD_FL_EXT_DEVT;
    sprintf(disk->disk_name, "%s%d", DEVICE_NAME, num);
    set_capacity(disk, nr_sectors);

    mutex_lock(&list_mutex);
    list_add_tail(&arxcis->list, &arxcis_list);
    arx_dev_cnt++;
    mutex_unlock(&list_mutex);

    printk(KERN_INFO "%s: Attached %s  sectors 0x%lx size 0x%lx/%luMB/%lu.%1luGB.\n", ARX_PREFIX, disk->disk_name,
        (unsigned long int) nr_sectors,
        (unsigned long int) arxcis->nr_bytes,
        (unsigned long int) arxcis->nr_bytes/SIZE_1MB,
        (unsigned long int) arxcis->nr_bytes/SIZE_1GB,
        (unsigned long int) arxcis->nr_bytes%SIZE_1GB);
    arx_dump_dev_offsets(arxcis);

    /* NOTE: I/O could occur at this point */
    add_disk(arxcis->disk);
    return 0;

out_free_queue:
    blk_cleanup_queue(arxcis->rq);
out_free_dev:
    kfree(arxcis);
out:
    return -1;
} // arx_attach_device //

static void
arx_detach_device(struct arxcis_device *arxcis){
    int num;

    BUG_ON(!arxcis);

    DPRINTK("%d: Entering %s\n", arxcis->index, __func__);

    num = arxcis->index;
    list_del(&arxcis->list);
    del_gendisk(arxcis->disk);
    put_disk(arxcis->disk);
    blk_cleanup_queue(arxcis->rq);
    kfree(arxcis);
    arx_dev_cnt--;
    printk(KERN_INFO "%s:%d: Detached %s%d.\n", ARX_PREFIX, num, DEVICE_NAME, num);
} // arx_detach_device //

static int
arx_detach_specific_device(int num) {
    struct arxcis_device *arxcis = NULL;
    int err = -ENODEV;

    DPRINTK("%d: Entering %s\n", num, __func__);

    mutex_lock(&list_mutex);
    if (!list_empty(&arxcis_list)) {
        list_for_each_entry(arxcis, &arxcis_list, list) {
            if(arxcis->index == num){
                err = 0;
                arx_detach_device(arxcis);
                break;
            }
        }
    }
    mutex_unlock(&list_mutex);

    return err;
} // arx_detach_specific_device //

static int
arx_detach_all_devices() {
    struct arxcis_device *arxcis = NULL;

    DPRINTK("Entering %s\n", __func__);

    mutex_lock(&list_mutex);
    while (!list_empty(&arxcis_list)) {
        arxcis = list_entry(arxcis_list.next, struct arxcis_device, list);
        arx_detach_device(arxcis);
    }
    mutex_unlock(&list_mutex);

    return 0;
} //arx_detach_all_devices //


/*****  Initialization Functions *******/

/**
 * Locate ArxCis NVDIMM and map the memory range
 * @Return: 0  on success
 *
 * Global variables updated:
 * @arxcis_phyaddr_start:
 * @arxcis_phyaddr_nr_bytes:
 * @arxcis_viraddr:
 **/
static int arx_discover_nvdimm(void) {
    uint64_t phyaddr;
    //uint64_t phyaddr_end = 0x800000000; /* 32 GB */
    uint64_t phyaddr_end = 0x2000000000; /* 128 GB */
    int e820_type;
    int i;

    DPRINTK("Entering %s\n", __func__);

    /* Start at 4GB offset and search for the arxcis memory type.
     * NOTE: This could break if the systemis configured to have a non-arxcis device mapped
     * as reserve memory above 4GB and the mem type used is reserved (default).
     * Originally, we used a unigue memory type for Arxcis in the E820 table,
     * but empirical testing discovered that different kernels respond differently
     * to unknown memory types and arxcis memory was not always discovered
     * across all kernels */

    for (i = 0; i < NUM_E820_TYPES; i++) {
        e820_type = e820_types[i];

        /* Search for Arxcis memory location */
        phyaddr = 0x100000000; /* 4 GB */
        while (phyaddr <= phyaddr_end) {
            if (arxcis_phyaddr_start == 0) {
                if (e820_any_mapped(phyaddr, phyaddr + 1, e820_type)) {
                    arxcis_phyaddr_start = phyaddr;
                    printk("%s: Arxcis NVDIMM starting address is 0x%lx\n", ARX_PREFIX, (unsigned long int) arxcis_phyaddr_start);
                }
            } else {
                if (!e820_any_mapped(phyaddr, phyaddr + 1, e820_type)) {
                    arxcis_phyaddr_nr_bytes = phyaddr - arxcis_phyaddr_start;
                    printk("%s: Arxcis NVDIMM ending address is 0x%lx  sectors 0x%lx size 0x%lx/%luMB/%lu.%1luGB)\n", ARX_PREFIX,
                        (unsigned long int) phyaddr - 1,
                        (unsigned long int)(arxcis_phyaddr_nr_bytes/BYTES_PER_SECTOR),
                        (unsigned long int) arxcis_phyaddr_nr_bytes,
                        (unsigned long int) arxcis_phyaddr_nr_bytes/SIZE_1MB,
                        (unsigned long int) arxcis_phyaddr_nr_bytes/SIZE_1GB,
                        (unsigned long int) arxcis_phyaddr_nr_bytes%SIZE_1GB);
                    break;
                }
            }
            phyaddr += SEARCH_INC;
        }

        /* Found Arxcis NVDIMM, no need to search for a different e820 type */
        if (arxcis_phyaddr_nr_bytes) {
            DPRINTK("E820 Type = %d\n", e820_type);
            break;
        }
    }

    if (!arxcis_phyaddr_start || !arxcis_phyaddr_nr_bytes) {
        printk("%s: Arxcis NVDIMM not found\n", ARX_PREFIX);
        return -ENXIO;
    }

    DPRINTK("Calling request_mem_region\n");
    if (request_mem_region(arxcis_phyaddr_start, arxcis_phyaddr_nr_bytes, DEVICE_NAME) == NULL) {
        arx_request_mem_region = 0;
        printk("%s: unable to request mem region, bypassing ...\n", ARX_PREFIX);
    }

    if (ioremap_mode == IOREMAP_NOCACHE)
        arxcis_viraddr = ioremap_nocache(arxcis_phyaddr_start, arxcis_phyaddr_nr_bytes);
    else if (ioremap_mode == IOREMAP_WC)
        arxcis_viraddr = ioremap_wc(arxcis_phyaddr_start, arxcis_phyaddr_nr_bytes);
    else if (ioremap_mode == IOREMAP_CACHE)
        arxcis_viraddr = ioremap_cache(arxcis_phyaddr_start, arxcis_phyaddr_nr_bytes);
    if(!arxcis_viraddr) {
        if (arx_request_mem_region)
            release_mem_region(arxcis_phyaddr_start, arxcis_phyaddr_nr_bytes); 
        printk("%s: failed to map memory region\n", ARX_PREFIX);
        return -ENOMEM;
    }

    DPRINTK("Arxcis NVDIMM mapped to virtual addr %p\n", arxcis_viraddr);

    return 0;
} // arx_discover_nvdimm //

/**
 * description: Init routine for module insertion.
 * return: (int)
 */
static int
__init arx_init (void){

    DPRINTK("Entering %s\n", __func__);

    if (arx_discover_nvdimm() != 0)
        return -ENOMEM;

    arx_ma_no = register_blkdev (arx_ma_no, DEVICE_NAME);
    if (arx_ma_no < 0){
        printk(KERN_ERR "%s: Failed registering %s, returned %d\n", ARX_PREFIX, DEVICE_NAME, arx_ma_no);
        return arx_ma_no;
    }

    DPRINTK("ioremap_mode=%d\n", ioremap_mode);

    INIT_LIST_HEAD(&arxcis_list);
    if (create_disk)
        (void) arx_attach_device(0, arxcis_phyaddr_nr_bytes / BYTES_PER_SECTOR);

    (void) arx_init_proc();

    printk(KERN_INFO "%s: Module successfully loaded.\n", ARX_PREFIX);

    arcxcis_open("/dev/arxcis0"); 
    
    return 0;
} // arx_init //

/**
 * description: Exit routine for module removal.
 * return: (void)
 */
static void
__exit arx_exit (void){
    DPRINTK("Entering %s\n", __func__);
    remove_proc_entry(PROC_NODE, NULL);
    arx_detach_all_devices();
    unregister_blkdev (arx_ma_no, DEVICE_NAME);
    iounmap(arxcis_viraddr);
    if (arx_request_mem_region)
        release_mem_region(arxcis_phyaddr_start, arxcis_phyaddr_nr_bytes);
    printk(KERN_INFO "%s: Module successfully unloaded.\n", ARX_PREFIX);
} // arx_exit //

module_init(arx_init);
module_exit(arx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Viking Technology");
MODULE_DESCRIPTION("ArxcisDisk (arxcis_b) is an enhanced NVRamDisk block device driver");
MODULE_VERSION(VERSION_STR); /* This driver was inspired by RapidDisk rxdsk v2.12 */
MODULE_INFO(Copyright, "Copyright 2014 Viking Technology");
