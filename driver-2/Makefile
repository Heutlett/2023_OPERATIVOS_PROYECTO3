obj-m += my_driver.o

all:
	sudo modprobe usbserial
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo insmod my_driver.ko
	sudo dmesg

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rmmod my_driver