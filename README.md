# HUST_fs
A filesystem driver for linux.

## How to use this?
1. Compile  
Install linux kernel sources (Linux 4.14) and run make from the checkedout directory.

2. Test  
````shell
$ make
$ dd bs=4096 count=100 if=/dev/zero of=image
$ ./mkfs ./image
$ sudo insmod HUST_fs.ko
$ sudo mount -o loop -t HUST_fs image ./test
$ sudo chmod 0777 ./test -R
$ cd test
$ cat file
$ echo "Hello World!" > file
$ cat file
````
## Disk layout

Dummy block | Super block | bmap | imap |inode table | data block0 | data block1 | ... ...
| :-: | -:| -:| -:| -:| -:| -:

You will see it clearly on mkfs.c

# TODO
- [ ] fix bug: vim e667  
- [ ] code refactoring

# [How to write a simple filesystem?](https://zhangshurong.github.io/)   
