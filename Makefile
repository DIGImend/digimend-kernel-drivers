ifneq ($(KERNELRELEASE),)
obj-m := hid-huion.o
else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
modules modules_install clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $@
conf_install:
	install -D -m 0644 digimend.conf /etc/depmod.d/digimend.conf
modules_depmod: modules_install conf_install
	depmod -a
udev_install:
	install hid-rebind /sbin
	install -m 0644 hid-rebind.rules /lib/udev/rules.d/70-hid-rebind.rules
	udevadm control --reload
install: modules_depmod udev_install
endif
