#ifdef __KERNEL__
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "dumpfs_inode.h"

#define	DUMPFS_NAME	"dumpfs"

extern const struct super_operations dumpfs_sops;
extern const struct file_operations dumpfs_dir_fops;
extern const struct dentry_operations dumpfs_dops;
extern const struct address_space_operations dumpfs_aops;

struct inode * dumpfs_figet(struct super_block *, struct inode *);
struct inode * dumpfs_iget(struct super_block *, unsigned long);


static inline struct dumpfs_inode_info *DUMPFS_I(struct inode *inode)
{
	return container_of(inode, struct dumpfs_inode_info, vfs_inode);
}

static inline struct dumpfs_sb_info *DUMPFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}
#endif

#define	DUMPFS_MAGIC		0x5555
#define DUMPFS_BLOCK_SIZE	4096

#define DUMPFS_START_INODE_BLK	3
#define	DUMPFS_START_DATA_BLK	8

typedef struct	dumpfs_super_block {
	uint8_t		s_version;
	uint8_t		s_state;
	uint16_t	s_magic;
	uint16_t	s_block_size;
/*10*/	uint32_t	s_inodes_count;
	uint8_t		s_start_inode_blk;
	uint8_t		s_start_data_blk;
	uint32_t	s_free_block_count;
/*20*/	uint32_t	s_free_inode_count;
	uint32_t	s_mount_time;
	uint32_t	s_creator_os;
	uint32_t	s_mkfs_time;
/*34*/	uint16_t	s_inode_size;
	uint8_t		_reserved[DUMPFS_BLOCK_SIZE - 38];
} dumpfs_super_block_t;

struct dumpfs_sb_info {
	dumpfs_super_block_t	*s_buf; /* superblock buf from disk */	
};

