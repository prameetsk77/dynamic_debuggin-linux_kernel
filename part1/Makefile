KERNELDIR := /lib/modules/$(shell uname -r)/build
CLEANFILE := *.dis *.o *.ko *.mod.* *.symvers *.*.old *.order *.cmd
obj-m := ht530_drv.o

default:
	make -C $(KERNELDIR)  M=$(CURDIR) modules
	gcc main.c -pthread -o main
	gcc main_nosleep.c -pthread -o main_nosleep
	sudo insmod ht530_drv.ko
	sudo chmod 666 /dev/ht530
	
clean:
	rm -f $(CLEANFILE)
	sudo rmmod ht530_drv
	
