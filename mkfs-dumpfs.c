#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/uio.h>

#include "dumpfs.h"
#include "dumpfs_inode.h"

int
open_device(char *devname)
{
	int fd;
	fd = open(devname, O_RDWR);
	return fd;
}

int
close_device(int fd)
{
	close(fd);
	printf("Device Close\n");
}

void
write_super_block(int fd)
{
	dumpfs_super_block_t  sb;
	int	ret = 0;
	memset(&sb, 0, sizeof(dumpfs_super_block_t));
	sb.s_version = 0x1;
	sb.s_magic = DUMPFS_MAGIC;
	sb.s_block_size = DUMPFS_BLOCK_SIZE;
	sb.s_inode_size = sizeof(struct dumpfs_inode);
	sb.s_inodes_count = 5 * sb.s_block_size/sb.s_inode_size;
	sb.s_start_inode_blk = DUMPFS_START_INODE_BLK; 
	sb.s_start_data_blk = DUMPFS_START_DATA_BLK; 
	ret = write(fd, (char *)&sb, sizeof(dumpfs_super_block_t));
	printf("ret is %d inode cnt %d\n", ret, sb.s_inodes_count);
	printf("size of super block is %d\n", sizeof(dumpfs_super_block_t));
	return;
}

void
write_inode_block(int fd)
{
	struct	iovec iov[80];
	struct	dumpfs_inode	inode;
	struct	dumpfs_inode	e_inode;
	struct	dumpfs_dir_entry dir_en;
	int cnt = 0;

	inode.i_mode = S_IFDIR;
	inode.i_blocks = 1;

	memset(&e_inode, 0, sizeof(struct dumpfs_inode));
	iov[0].iov_base = &inode;
	iov[0].iov_len = sizeof(struct dumpfs_inode);
	inode.i_block[0] = 80 * sizeof(struct dumpfs_inode);
	for(cnt = 1; cnt < 80; cnt++) {
		iov[cnt].iov_base = &e_inode;
		iov[cnt].iov_len = sizeof(struct dumpfs_inode);
	} 
	printf("Write inode block...%d\n", sizeof(iov));
	writev(fd, iov, sizeof(iov)/sizeof(iov[0]));

	memset(&dir_en, 0, sizeof(dir_en));
	/* create root directory with inode 2 */
	dir_en.inode = DUMPFS_ROOT_INUM;
	dir_en.name_len = 1;
	dir_en.name[0] = '.';
	dir_en.name[1] = '\n';
	dir_en.file_type = DUMPFS_FT_DIR;
	dir_en.rec_len = 12;
	write(fd, &dir_en, dir_en.rec_len);

	/* create root directory with inode 2 */
	dir_en.inode = 0;
	dir_en.name_len = 1;
	dir_en.name[0] = '.';
	dir_en.name[1] = '.';
	dir_en.name[2] = '\n';
	dir_en.file_type = DUMPFS_FT_DIR;
	dir_en.rec_len = 12;
	write(fd, &dir_en, dir_en.rec_len);
}

void
write_block_bitmap(int fd)
{
	char zero[4*1024] = {0};
	
	write(fd, &zero, sizeof(zero));
}

void
write_inode_bitmap(int fd)
{
	char zero[4*1024] = {0};
	write(fd, &zero, sizeof(zero));
}

int
main(int argc, char *argv[])
{
	int fd;
	char	*dev = "/dev/sdb";
#if 0
	if (argc != 2) {
		perror("No device ...\n");
		return (-1);
	}
	
	if (fd = open_device(argv[1]) < 0) {
#endif

	fd = open_device(dev);
	printf("fd is %d\n", fd);
	printf("sizeof inode is %d\n", sizeof(struct dumpfs_inode));
	write_super_block(fd);
	write_block_bitmap(fd);
	write_inode_bitmap(fd);
        write_inode_block(fd);
	
	close_device(fd);
}
