ifneq ($(KERNELRELEASE),)
obj-m := hid-kye.o hid-uclogic.o hid-polostar.o
else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
UDEV_RULES := /lib/udev/rules.d/90-hid-rebind.rules
DEPMOD_CONF := /etc/depmod.d/digimend.conf
HID_REBIND := /sbin/hid-rebind
modules modules_install clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $@
install: modules_install
	install -D -m 0644 digimend.conf $(DEPMOD_CONF)
	depmod -a
	install hid-rebind $(HID_REBIND)
	install -m 0644 hid-rebind.rules $(UDEV_RULES)
	udevadm control --reload
uninstall:
	rm -vf $(UDEV_RULES) $(HID_REBIND) $(DEPMOD_CONF) \
		/lib/modules/*/extra/hid-kye.ko \
		/lib/modules/*/extra/hid-polostar.ko \
		/lib/modules/*/extra/hid-uclogic.ko
	udevadm control --reload
	depmod -a
endif
