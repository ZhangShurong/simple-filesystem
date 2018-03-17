obj-m := HUST_fs.o
HUST_fs-objs := inode.o map.o block.o file.o super.o

all: drive mkfs

drive:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
mkfs_SOURCES:
	mkfs.c
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm mkfs
