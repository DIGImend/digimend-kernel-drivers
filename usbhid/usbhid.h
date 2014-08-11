/*
 * Definitions from the Linux kernel's private header
 * drivers/hid/usbhid/usbhid.h.
 */
#ifndef __USBHID_H
#define __USBHID_H

#define	hid_to_usb_dev(hid_dev) \
	container_of(hid_dev->dev.parent->parent, struct usb_device, dev)

#endif
