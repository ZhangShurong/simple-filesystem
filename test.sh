dd bs=4096 count=100 if=/dev/zero of=image
insmod HUST_fs.ko
mount -o loop -t simplefs image ./test
dmesg
