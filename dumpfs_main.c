#include <linux/module.h>
#include <linux/fs.h>

#include "dumpfs.h"
#include "dumpfs_priv.h"

static int
dumpfs_read_sb(struct super_block *sb, void *data, int silent)
{
	int	err = 0;
	struct	inode *inode;
	struct	buffer_head *bh;
	struct dumpfs_sb_info	*sbi;
	dumpfs_super_block_t	*ds;	
	char *dev_name = (char *)data;

	if (!dev_name) {
		err = -EINVAL;
		goto out;
	}
	/* 
	 * dev_name is device_name or file that needs to be mounted 
	 * mount -t dumpfs /mnt/filename /mnt/dumpfs, dev_name points
 	 * to /mnt/filename.
 	 */
	/* connect dumpfs superblock later */
	sbi = kzalloc(sizeof(struct dumpfs_sb_info), GFP_KERNEL);
	if (!sbi) {
		err = -ENOMEM;
		goto out;
	}
	sb->s_fs_info = sbi;

	/* read the superblock from the disk */
	if (!(bh = sb_bread(sb, 0))) {
		goto free;
	}
	ds = (dumpfs_super_block_t *)bh->b_data;

	sb->s_magic = ds->s_magic;
	sb->s_time_gran = 1;
	sb->s_op = &dumpfs_sops;
	sbi->s_buf = ds;
	printk(KERN_INFO "sbi->s_buf %p\n", sb->s_fs_info);
	inode = dumpfs_iget(sb, DUMPFS_ROOT_INUM);
	if (IS_ERR(inode)) {
		printk(KERN_INFO "%d \n", __LINE__);
		err = PTR_ERR(inode);
		goto out;
	}
	printk(KERN_INFO "inode %p magic %x\n", inode, ds->s_magic);
	
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto free;
	}
	d_rehash(sb->s_root);
	d_set_d_op(sb->s_root, &dumpfs_dops);
	goto out;
free:
	printk(KERN_INFO "Failed free superblock");
	kfree(sb->s_fs_info);
out:
	return (err);
}

/*
 * mount entry point of filesystem.
 * mount_nodev: mount a filesystem on a file with callback routine to 
 * 	initialize superblock.
 * mount_bdev: 
 */
struct	dentry *
dumpfs_mount(struct file_system_type *fs_type, int flags, const char *name,
								void *data)
{
	void	*path_name = (void *)name;
	struct	dentry	*dentry;

	dentry = mount_bdev(fs_type, flags, name, path_name, dumpfs_read_sb); 
	if (dentry) {
		printk(KERN_INFO "%s %p path %s\n", __FUNCTION__, dentry, name);
		printk(KERN_INFO "dentry allocated %s\n", dentry->d_name.name);
	}
	return (dentry);
}

static void
dumpfs_kill_sb(struct super_block *sb)
{
	struct	block_device *bdev = sb->s_bdev;
	fmode_t	mode = sb->s_mode;
	kfree(sb->s_fs_info);
	//kill_litter_super(sb);
	generic_shutdown_super(sb);
	blkdev_put(bdev, mode | FMODE_EXCL);
}

static struct file_system_type dumpfs_type = {
	.owner	= THIS_MODULE,
	.name	= DUMPFS_NAME,
	.mount	= dumpfs_mount,
	.kill_sb = dumpfs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
};

static void
init_once(void *info)
{
	struct	dumpfs_inode_info *di = (struct dumpfs_inode_info *)info;
	inode_init_once(&di->vfs_inode);
}

static int
init_inode_cache(void)
{
	dumpfs_inode_cachep = kmem_cache_create("dumpfs_inode_cache",
				sizeof(struct dumpfs_inode_info),
				0, (SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD),
				init_once);	
	if (dumpfs_inode_cachep == NULL) {
		return -ENOMEM;
	}
	return 0;
}

static void
destroy_inode_cache(void)
{
	kmem_cache_destroy(dumpfs_inode_cachep);
}

/*
 * register dumpfs with vfs layer.
 */
static int
__init init_dumpfs(void)
{
	int	err;
	err = init_inode_cache();
	if (err < 0) {
		printk(KERN_INFO "Failed Register dumpfs filesystem %d", err);
		return err;
	}
	if ((err = register_filesystem(&dumpfs_type))) {
		printk(KERN_INFO "Failed to register filesystem %d", err);
	}
	printk(KERN_INFO "Register dumpfs filesystem %d", err);
	
	return (err);
}

static void
__exit exit_dumpfs(void)
{
	destroy_inode_cache();
	unregister_filesystem(&dumpfs_type);
}

MODULE_AUTHOR("Krishna Mohan");
MODULE_LICENSE("GPL");
module_init(init_dumpfs);
module_exit(exit_dumpfs);
