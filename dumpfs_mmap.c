#include "dumpfs.h"
#include <linux/mpage.h>

static int
dumpfs_get_block(struct inode *inode, sector_t block, struct buffer_head *bh,
					int create)
{
	int	err = -EIO;
	printk(KERN_INFO "%s sector %ld\n", __FUNCTION__, block);
	return (err);
}

static int
dumpfs_readpage(struct file *file, struct page *page)
{
	int	err = 0;
	err = mpage_readpage(page, dumpfs_get_block);
	printk(KERN_INFO "%s retval is %d\n", __FUNCTION__, err);
	return (err);
}

static int
dumpfs_readpages(struct file *file, struct address_space *mapping,
		struct list_head *pages, unsigned nr_pages)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (-1);
}

static int
dumpfs_writepage(struct page *page, struct writeback_control *wbc)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (-1);
}

static int
dumpfs_writepages(struct address_space *map, struct writeback_control *wbc)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (-1);
}

static int
dumpfs_write_begin(struct file *file, struct address_space *map,
	loff_t pos, unsigned len, unsigned flags,
	struct page **pagep, void **fsdata)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (-1);
}

static int
dumpfs_write_end(struct file *file, struct address_space *map,
	loff_t pos, unsigned len, unsigned copied,
	struct page *page, void *fsdata)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (-1);
}

static ssize_t
dumpfs_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov,
	loff_t	offset, unsigned long nr_segs)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (-1);
}

const struct address_space_operations dumpfs_aops = {
	.readpage		= dumpfs_readpage,
	//.readpages		= dumpfs_readpages,
	//.writepage		= dumpfs_writepage,
	//.writepages		= dumpfs_writepages,
	//.write_begin		= dumpfs_write_begin,
	//.write_end		= dumpfs_write_end,
	//.direct_IO		= dumpfs_direct_IO,
};
