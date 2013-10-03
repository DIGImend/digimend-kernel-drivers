ifneq ($(KERNELRELEASE),)
obj-m := hid-huion.o
else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
modules modules_install clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $@
modules_depmod: modules_install
	depmod -a
install: modules_depmod
endif
