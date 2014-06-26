#include <linux/pagemap.h>
#include "dumpfs.h"

static inline unsigned long
num_dir_pages(struct inode *inode)
{
	return (inode->i_size + PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT;
}

ino_t
dumpfs_inode_by_name(struct inode *dir, struct qstr *child)
{
	//ino_t	inum;
	//struct	page *page;
	//dumpfs_find_entry(dir, child, &page);	
	return (-1);
}

static struct page *
dumpfs_get_page(struct inode *dir, unsigned long n, int quiet)
{
	struct	address_space *mapping = dir->i_mapping;
	struct	page *page = read_mapping_page(mapping, n, NULL);
	return page;
}

static inline void
dumpfs_put_page(struct page *page)
{
	kunmap(page);
	page_cache_release(page);
}

static int
dumpfs_readdir(struct file *file, struct dir_context *ctx)
{
	static	int	callnum = 0;
        struct  dentry  *dentry;
	loff_t	pos = ctx->pos;
	unsigned long n = pos >> PAGE_CACHE_SHIFT;
        dentry  = file->f_path.dentry;
	printk(KERN_INFO "%s called \n", __FUNCTION__);
	//printk(KERN_INFO "position %lld\n", ctx->pos >> PAGE_CACHE_SHIFT);
       	//printk(KERN_INFO "inode %p \n", file_inode(file));
       	//printk(KERN_INFO "file pos is %lld \n", file->f_pos);
	for (; n < num_dir_pages(file_inode(file)); n++) {
		struct	page *page = dumpfs_get_page(file_inode(file), n, 0);
		printk(KERN_INFO "n %ld page is %p\n", n, page);
		if (callnum == 0) {
			dir_emit(ctx, ".", 128, 2, DT_UNKNOWN);
			ctx->pos += 4096;
			callnum++;
		} else {
			dir_emit(ctx, "..", 128, 2, DT_UNKNOWN);
			ctx->pos += 4096;
		}
       		printk(KERN_INFO "ctx pos is %lld \n", ctx->pos);
		if (!IS_ERR(page))
			dumpfs_put_page(page);
	}
	file->f_pos = ctx->pos;
       	printk(KERN_INFO "End file pos is %lld \n", file->f_pos);
        return (0);
}

static int
dumpfs_open(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "%s \n", __FUNCTION__);
        return (0);
}

static int
dumpfs_release(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "%s \n", __FUNCTION__);
        return (0);
}

const struct file_operations dumpfs_dir_fops = {
        .llseek = generic_file_llseek,
        .read   = generic_read_dir,
        .iterate = dumpfs_readdir,
        .open   = dumpfs_open,
        .release = dumpfs_release,
};

