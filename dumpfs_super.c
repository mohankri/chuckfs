#include "dumpfs.h"
//#include "dumpfs_priv.h"

static void
dumpfs_put_super(struct super_block *sb)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
}


static int
dumpfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (0);
}

static int
dumpfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	return (0);
}

static void
dumpfs_umount_begin(struct super_block *sb)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
}

static void
dumpfs_evict_inode(struct inode *inode)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	printk(KERN_INFO "inode %p\n", inode);
	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
}
extern struct kmem_cache *dumpfs_inode_cachep;

static struct inode *
dumpfs_alloc_inode(struct super_block *sb)
{
	struct dumpfs_inode_info *di;
	di = (struct dumpfs_inode_info *)kmem_cache_alloc(dumpfs_inode_cachep, GFP_KERNEL);
	if (!di) {
		printk(KERN_INFO "Alloc failed FUNCTION %s\n", __FUNCTION__);
		return NULL;
	}
	printk(KERN_INFO "FUNCTION %s %p\n", __FUNCTION__, &di->vfs_inode);
	//di->i_block_alloc_info = NULL;
	di->i_dir_start_lookup = 0;
	di->vfs_inode.i_version = 1;
	return &di->vfs_inode;
}

static void
dumpfs_i_callback(struct rcu_head *head)
{
	struct	inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(dumpfs_inode_cachep, DUMPFS_I(inode));
}

static void
dumpfs_destroy_inode(struct inode *inode)
{
	printk(KERN_INFO "FUNCTION %s\n", __FUNCTION__);
	call_rcu(&inode->i_rcu, dumpfs_i_callback);
}

const struct super_operations dumpfs_sops = {
	.alloc_inode = dumpfs_alloc_inode,
	.destroy_inode = dumpfs_destroy_inode,
	.put_super = dumpfs_put_super,
	.statfs	   = dumpfs_statfs,
	.remount_fs = dumpfs_remount_fs,
	.umount_begin = dumpfs_umount_begin,
	.show_options = generic_show_options,
	.evict_inode = dumpfs_evict_inode,
};
