KERNELDIR := /lib/modules/$(shell uname -r)/build
CLEANFILE := *.dis *.o *.ko *.mod.* *.symvers *.*.old *.order *.cmd
obj-m := ht530_drv.o mprobe.o

default:
	make -C $(KERNELDIR)  M=$(CURDIR) modules
	gcc main2.c -pthread -o main2
	sudo insmod ht530_drv.ko
	sudo chmod 666 /dev/ht530
	sudo insmod mprobe.ko
	sudo chmod 666 /dev/Mprobe

clean:
	rm -f $(CLEANFILE)
	sudo rmmod ht530_drv
	sudo rmmod mprobe.ko
