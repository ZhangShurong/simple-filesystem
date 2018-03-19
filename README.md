# HUST_fs
A filesystem driver for linux.

# How to use this?
1. Compile  
Install linux kernel sources (Linux 4.14) and run make from the checkedout directory.

2. Test  
````shell
$ make
$ dd bs=4096 count=100 if=/dev/zero of=image
$ ./mkfs ./image
# insmod HUST_fs.ko
# mount -o loop -t HUST_fs image ./test
# chmod 0777 ./test -R
$ cd test
$ cat file
$ echo "Hello World!" > file
$ cat file
````
# [How to write a simple filesystem?](https://zhangshurong.github.io/)   
