obj-m := hid-kye.o hid-uclogic.o hid-polostar.o hid-viewsonic.o
hid-uclogic-objs := \
	hid-uclogic-core.o \
	hid-uclogic-rdesc.o \
	hid-uclogic-params.o
KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)
DESTDIR =
UDEV_RULES = $(DESTDIR)/lib/udev/rules.d/90-digimend.rules
DEPMOD_CONF = $(DESTDIR)/etc/depmod.d/digimend.conf
HID_REBIND = $(DESTDIR)/lib/udev/hid-rebind
XORG_CONF := $(DESTDIR)/usr/share/X11/xorg.conf.d/50-digimend.conf
PACKAGE_NAME = digimend-kernel-drivers
PACKAGE_VERSION = 10
PACKAGE = $(PACKAGE_NAME)-$(PACKAGE_VERSION)
DKMS_MODULES_NAME = digimend
DKMS_MODULES = $(DKMS_MODULES_NAME)/$(PACKAGE_VERSION)
DKMS_SOURCE_DIR = $(DESTDIR)/usr/src/$(DKMS_MODULES_NAME)-$(PACKAGE_VERSION)

modules modules_install clean:
	$(MAKE) -C $(KDIR) M=$(PWD) $@

depmod_conf_install:
	install -D -m 0644 depmod.conf $(DEPMOD_CONF)

depmod_conf_uninstall:
	rm -vf $(DEPMOD_CONF)

xorg_conf_install:
	install -D -m 0644 xorg.conf $(XORG_CONF)

xorg_conf_uninstall:
	rm -vf $(XORG_CONF)

udev_rules_install:
	install -D -m 0755 hid-rebind $(HID_REBIND)
	install -D -m 0644 udev.rules $(UDEV_RULES)

udev_rules_uninstall:
	rm -vf $(UDEV_RULES) $(HID_REBIND)

modules_uninstall:
	rm -vf /lib/modules/*/extra/hid-kye.ko \
	       /lib/modules/*/extra/hid-polostar.ko \
	       /lib/modules/*/extra/hid-uclogic.ko \
	       /lib/modules/*/extra/hid-viewsonic.ko

install: modules_install udev_rules_install depmod_conf_install xorg_conf_install
	udevadm control --reload
	depmod -a

uninstall: modules_uninstall udev_rules_uninstall depmod_conf_uninstall xorg_conf_uninstall
	udevadm control --reload
	depmod -a

dkms_check:
	@if ! which dkms >/dev/null; then \
	    echo "DKMS not found, aborting." >&2; \
	    echo "Make sure DKMS is installed,"; \
	    echo "and \"make\" is running under sudo, or as root."; \
	    exit 1; \
	fi

dkms_source_install:
	install -m 0755 -d $(DKMS_SOURCE_DIR)
	install -m 0644 Makefile *.[hc] $(DKMS_SOURCE_DIR)
	install -m 0755 -d $(DKMS_SOURCE_DIR)/usbhid
	install -m 0644 usbhid/*.[hc] $(DKMS_SOURCE_DIR)/usbhid

dkms_modules_install: dkms_check
	@if dkms status $(DKMS_MODULES_NAME) | grep . >/dev/null; then \
	    echo "DKMS has DIGImend modules added already, aborting." >&2; \
	    echo "Run \"make dkms_uninstall\" first" >&2; \
	    exit 1; \
	fi
	dkms add .
	dkms build $(DKMS_MODULES)
	dkms install $(DKMS_MODULES)

dkms_modules_uninstall: dkms_check
	set -e -x; \
	dkms status $(DKMS_MODULES_NAME) | \
	    while IFS=':' read -r modules status; do \
	        echo "$$modules" | { \
	            IFS=', ' read -r modules_name modules_version \
	                             kernel_version kernel_arch ignore; \
	            if [ -z "$$kernel_version" ]; then \
	                dkms remove \
	                            "$$modules_name/$$modules_version" \
	                            --all; \
	            else \
	                dkms remove \
	                            "$$modules_name/$$modules_version" \
	                            -k "$$kernel_version/$$kernel_arch"; \
	            fi; \
	        } \
	    done

dkms_install: dkms_modules_install udev_rules_install xorg_conf_install
	udevadm control --reload

dkms_uninstall: dkms_modules_uninstall udev_rules_uninstall xorg_conf_uninstall
	udevadm control --reload

dist:
	git archive --format=tar.gz --prefix=$(PACKAGE)/ HEAD > $(PACKAGE).tar.gz
