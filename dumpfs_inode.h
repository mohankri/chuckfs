#include <linux/types.h>
#include <linux/stat.h>

#define	DUMPFS_NDIR_BLOCKS	12
#define DUMPFS_IND_BLOCK	DUMPFS_NDIR_BLOCKS
#define	DUMPFS_N_BLOCKS		(DUMPFS_IND_BLOCK + 1)

#define	DUMPFS_ROOT_INUM	2

#define	DUMPFS_FT_UNKNOWN	0
#define	DUMPFS_FT_FILE		1
#define	DUMPFS_FT_DIR		2


struct 	dumpfs_dir_entry {
	__le32	inode;
	__le16	rec_len;
	__u8	name_len;
	__u8	file_type;
	char	name[];
};

struct	dumpfs_inode {
	__le16	i_mode;
	__le16	i_uid;
	__le32	i_size;
	__le32	i_atime;
	__le32	i_ctime;
	__le32	i_blocks;
	__le32	i_block[DUMPFS_N_BLOCKS];
	uint8_t	reserved[256-72];
};

#ifdef __KERNEL__

struct  dumpfs_inode_info {
        struct  inode   vfs_inode;
        __u32   i_dir_start_lookup;
};

#endif

