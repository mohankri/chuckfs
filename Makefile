obj-m := dumpfs.o

dumpfs-objs := dumpfs_main.o dumpfs_super.o dumpfs_inode.o dumpfs_dir.o \
		dumpfs_mmap.o dumpfs_dentry.o

#dumpfs_file.o dumpfs_dir.o dumpfs_namei.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
