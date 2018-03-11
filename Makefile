obj-m := HUST_fs.o
HUST_fs-objs := HUST_fs.o HUST_Utils.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
