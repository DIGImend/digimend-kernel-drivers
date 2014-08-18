ifneq ($(KERNELRELEASE),)
obj-m := hid-huion.o
else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
UDEV_RULES := /lib/udev/rules.d/70-hid-rebind.rules
DEPMOD_CONF := /etc/depmod.d/digimend.conf
HID_REBIND := /sbin/hid-rebind
modules modules_install clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $@
conf_install:
	install -D -m 0644 digimend.conf $(DEPMOD_CONF)
modules_depmod: modules_install conf_install
	depmod -a
udev_install:
	install hid-rebind $(HID_REBIND)
	install -m 0644 hid-rebind.rules $(UDEV_RULES)
	udevadm control --reload
install: modules_depmod udev_install
uninstall:
	rm -vf $(UDEV_RULES) $(HID_REBIND) $(DEPMOD_CONF) \
		/lib/modules/*/extra/hid-huion.ko
	udevadm control --reload
	depmod -a
endif
