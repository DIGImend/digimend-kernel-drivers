ifneq ($(KERNELRELEASE),)
obj-m := hid-huion.o
else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
modules modules_install clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $@
endif
