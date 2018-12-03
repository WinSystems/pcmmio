# Makefile for pcmmio_ws

ifneq ($(KERNELRELEASE),) # called by kbuild
	obj-m := pcmmio_ws.o
else # called from command line
	KERNEL_VERSION = `uname -r`
	KERNELDIR := /lib/modules/$(KERNEL_VERSION)/build
	PWD  := $(shell pwd)
	MODULE_INSTALLDIR = /lib/modules/$(KERNEL_VERSION)/kernel/drivers/gpio/

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

mio_io.o: mio_io.c mio_io.h Makefile
	gcc -c $(EXTRA_CFLAGS) mio_io.c

all:    default install adctest getvolt getall buffered repeat dactest dacout dacbuff diotest flash poll

install:
	mkdir -p $(MODULE_INSTALLDIR)
	rm -f $(MODULE_INSTALLDIR)pcmmio_ws.ko
	install -c -m 0644 pcmmio_ws.ko $(MODULE_INSTALLDIR)
	/sbin/depmod -a

uninstall:
	rm -f $(MODULE_INSTALLDIR)pcmmio_ws.ko
	/sbin/depmod -a

adctest: adctest.c mio_io.h mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -D_REENTRANT -static adctest.c mio_io.o -o adctest -lpthread
	chmod a+x adctest

getvolt: getvolt.c mio_io.h mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -static getvolt.c mio_io.o -o getvolt
	chmod a+x getvolt

getall: getall.c mio_io.h mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -static getall.c mio_io.o -o getall
	chmod a+x getall

buffered: buffered.c mio_io.o mio_io.h Makefile kbhit.c
	gcc $(EXTRA_CFLAGS) -static buffered.c kbhit.c mio_io.o -o buffered
	chmod a+x buffered

repeat: repeat.c mio_io.o mio_io.h Makefile kbhit.c
	gcc $(EXTRA_CFLAGS) -D_REENTRANT -static repeat.c kbhit.c mio_io.o -o repeat -lpthread
	chmod a+x repeat

dactest: dactest.c mio_io.h mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -D_REENTRANT -static dactest.c mio_io.o -o dactest -lpthread
	chmod a+x dactest

dacout: dacout.c mio_io.h mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -static dacout.c mio_io.o -o dacout
	chmod a+x dacout

dacbuff: dacbuff.c mio_io.o mio_io.h Makefile kbhit.c
	gcc $(EXTRA_CFLAGS) -static dacbuff.c kbhit.c mio_io.o -o dacbuff
	chmod a+x dacbuff

diotest: diotest.c mio_io.h mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -D_REENTRANT -static diotest.c mio_io.o -o diotest -lpthread
	chmod a+x diotest

flash: flash.c mio_io.h kbhit.c mio_io.o Makefile
	gcc $(EXTRA_CFLAGS) -static flash.c kbhit.c mio_io.o -o flash
	chmod a+x flash

poll:  poll.c mio_io.o mio_io.h Makefile
	gcc $(EXTRA_CFLAGS) -D_REENTRANT -static poll.c mio_io.o -o poll -lpthread
	chmod a+x poll

endif
 
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions pcmmio_ws Module.symvers

spotless:
	rm -rf adctest getvolt getall buffered repeat dactest dacout dacbuff diotest flash poll 
	rm -rf Module.* *.o *~ core .depend .*.cmd *.ko *.mod.c *.order .tmp_versions /dev/pcmmio_ws?
