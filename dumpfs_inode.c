#include <linux/buffer_head.h>
#include "dumpfs.h"

/*
 * Get the permission of the inode.
 */
static int
dumpfs_permission(struct inode *inode, int mask)
{
	int	err = 0;
        umode_t mode;
	//inode_permission(inode, mask);
	//printk(KERN_INFO "Function %s %p %p\n", __FUNCTION__, inode, inode->i_sb);
	//printk(KERN_INFO "mask %d flag %ld mode %d\n", mask, inode->i_sb->s_flags, inode->i_mode);

	if (unlikely(mask & MAY_WRITE)) {
		//printk(KERN_INFO "Write ...\n");
                mode = inode->i_mode;
 
                 /* Nobody gets write access to a read-only fs. */
                 if ((inode->i_sb->s_flags & MS_RDONLY) &&
                     (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
                         printk(KERN_INFO "ReadOnly ...\n");
         }
	return (err);
}

static int
dumpfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int	err = 0;
	printk(KERN_INFO "Function %s\n", __FUNCTION__);
	return (err);
}

static int
dumpfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
					struct kstat *stat)
{
	int	err = 0;
	struct	inode	*inode;
	inode = dentry->d_inode;
	generic_fillattr(inode, stat);	
	printk(KERN_INFO "Function %s\n", __FUNCTION__);
	printk(KERN_INFO "inode %p\n", inode);
	printk(KERN_INFO "DIR %d\n", S_ISDIR(inode->i_mode));
	return (err);
}

static struct dentry *
dumpfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
	struct	inode *inode = NULL;
	//struct	dentry	*de;
	//printk(KERN_INFO "Function %s %s\n", __FUNCTION__, dentry->d_iname);
	//printk(KERN_INFO "inode %p dentry %p\n", dir, dentry);
	//inode = dumpfs_iget(dir->i_sb, 3);
	//printk(KERN_INFO "dentry name is %p \n", inode);
	return NULL;
}

const struct inode_operations dumpfs_dir_iops = {
	.permission	= dumpfs_permission,
	.setattr	= dumpfs_setattr,
	.getattr	= dumpfs_getattr,
	.lookup		= dumpfs_lookup,
};

static struct dumpfs_inode *
dumpfs_get_inode(struct super_block *sb, ino_t ino, struct buffer_head **p)
{
	struct dumpfs_sb_info *sb_info;
	unsigned long	block;
	unsigned long	sector;
	struct	buffer_head *bh;

	*p = NULL;
	if (ino != DUMPFS_ROOT_INUM) {
		printk(KERN_INFO "inode number is %ld\n", ino);
	} else {
		printk(KERN_INFO "inode number is %ld\n", ino);
		sb_info = DUMPFS_SB(sb);
		block = (ino * sizeof(struct dumpfs_inode)) + (sb_info->s_buf->s_start_inode_blk * 4096);
		sector = block/4096;
		if (!(bh = sb_bread(sb, sector)))
			goto eio;
		*p = bh;
		return (struct dumpfs_inode *) (bh->b_data);	
	}	
eio:		
	return ERR_PTR(-EINVAL);
}

struct inode *
dumpfs_iget(struct super_block *sb, unsigned long ino)
{
	struct	inode *inode;
	struct	dumpfs_inode_info *ei;
	struct	dumpfs_inode *raw_inode;
	struct	buffer_head *bh;
	long	ret	= -EIO;

	/* call superblock operation method alloc_inode */
	inode = iget_locked(sb, ino);
	if (!inode) {
		return (ERR_PTR(-ENOMEM));
	}
	
	if (!(inode->i_state & I_NEW)) {
		printk(KERN_INFO "New ...%p\n", inode);
		return (inode);
	}

	ei = DUMPFS_I(inode);

	raw_inode = dumpfs_get_inode(inode->i_sb, ino, &bh);	
	if (IS_ERR(raw_inode)) {
		ret = PTR_ERR(raw_inode);
		goto end;
	}
	printk(KERN_INFO "%s %p\n", __FUNCTION__, DUMPFS_SB(sb));
	
	/* It's not being currently read from Device */
	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	inode->i_size = 2;
	if (S_ISREG(inode->i_mode)) {
		inode->i_mapping->a_ops = &dumpfs_aops;
		printk(KERN_INFO "File mode ..\n");
	} else if (S_ISDIR(inode->i_mode)) {
		printk(KERN_INFO "Dir mode ..\n");
		inode->i_op = &dumpfs_dir_iops;
		inode->i_fop = &dumpfs_dir_fops;
		inode->i_mapping->a_ops = &dumpfs_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		printk(KERN_INFO "SYMLINK mode ..\n");
	}
	unlock_new_inode(inode);
	return (inode);

end:
	iget_failed(inode);
	return ERR_PTR(ret);

}
