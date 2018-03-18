sudo umount /home/zhang/HUST_OS_fs_experiment/test
sudo rmmod HUST_fs
dd bs=4096 count=100 if=/dev/zero of=image
./mkfs image
insmod HUST_fs.ko
mount -o loop -t HUST_fs image ./test
dmesg
sudo chmod 0777 ./test -R
