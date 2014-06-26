#include "dumpfs.h"

static int
dumpfs_revalidate(struct dentry *dentry, unsigned int flags)
{
	int err = 0;
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return (err);
}

static void
dumpfs_release(struct dentry *dentry)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return;
}

const struct dentry_operations dumpfs_dops = {
	.d_revalidate	= dumpfs_revalidate,
	.d_release	= dumpfs_release,
};
