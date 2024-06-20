// SPDX-License-Identifier: GPL-2.0+
/*
 *  HID driver for XP-PEN G430S_B (black) tablet, which is not fully compliant with HID standard
 *  Based on hid-viewsonic.c by Copyright (c) 2017 Nikolai Kondrashov
 *  Version: 1.1
 *
 *  Copyright (c) 2020 Sturwandan
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "usbhid/usbhid.h"
#include "hid-ids.h"

/* Size of the original descriptor of G430S_B pen */
#define G430SB_RDESC_ORIG_SIZE	36
/* We only need to work with interface 2 and ignore 0 and 1 */
#define G430SB_PEN_INTERFACE	2

/* Fixed report descriptor for XP-PEN G430S_B 's pen
 * This format is sent after initialization. 
 * If the tablet is not initialized, the format is similar,
 * but there are only two 0x00 bytes at the end and
 * coordinates go from 0 to 32767 instead of native values
 * which are approximately Physical values in units of 10^-3 cm.
 * The report has following format, LE and ls bit first
 *Bits	Type
 *8	ID=0x02
 *1	TipSwitch
 *1	BarrelSwitch
 *1	BarrelSwitch2
 *1	Const = 0 (invert)
 *1	Const = 0
 *1	InRange
 *1	NotInRange (Unused)
 *1	Const = 1
 *16	X
 *16	Y
 *16	Pressure
 *16	Const = 0x0000 (Xtilt)
 *16	Const = 0x0000 (Ytilt)
 */
static __u8 g430sb_rdesc_fixed[] = {
	0x05, 0x0D,        // Usage Page (Digitizer)
	0x09, 0x02,        // Usage (Pen)
	0xA1, 0x01,        // Collection (Application)
	0x85, 0x02,        //   Report ID (2) // First byte is always 0x02
	0x09, 0x20,        //   Usage (Stylus)
	0xA1, 0x00,        //   Collection (Physical)
	// Second byte is 0xAx or 0xC0, A=1010b C=1100b, thus we use bits 0-2 for buttons,
	// (bit order is: 7654 3210) skip bits 3 and 4, bit 5 = InRange, skip bits 6 and 7.
	0x09, 0x42,        //     Usage (Tip Switch)
	0x09, 0x44,        //     Usage (Barrel Switch / BS1)
	0x09, 0x46,        //     Usage (Tablet Pick / BS2)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x03,        //     Report Count (3)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x95, 0x02,        //     Report Count (2)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x09, 0x32,        //     Usage (In Range)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x95, 0x02,        //     Report Count (2)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	// Lmin, Lmax and RS are propagated up to this point

	// The rest of report are 5 int16le numbers: X, Y, Pressure, Xtilt/Unused, Ytilt/Unused
	// X and Y are in different usage page, Generic Desktop, so we save usage page
	0xA4,              //     Push
	0x05, 0x01,        //       Usage Page (Generic Desktop Ctrls)

	0x09, 0x30,        //       Usage (X)
	0x15, 0x00,        //       Logical Minimum (0)
	0x26, 0xB0, 0x27,  //       Logical Maximum (10160)
	0x65, 0x11,        //       Unit (System: SI Linear, Length: Centimeter)
	0x55, 0x0D,        //       Unit Exponent (-3)
	// Physical minimum and Maximum are approximately same as logical, since tablet area is 101x77mm 
	0x35, 0x00,        //       Physical Minimum (0)
	0x46, 0xB0, 0x27,  //       Physical Maximum (10160)
	0x75, 0x10,        //       Report Size (16)
	0x95, 0x01,        //       Report Count (1)
	0x81, 0x22,        //       Input (Data,Var,Abs,No Wrap,Linear,No Preferred State,No Null Position)

	0x09, 0x31,        //       Usage (Y)
	0x26, 0xC4, 0x1D,  //       Logical Maximum (7620)
	0x46, 0xC4, 0x1D,  //       Physical Maximum (7620)
	0x81, 0x22,        //       Input (Data,Var,Abs,No Wrap,Linear,No Preferred State,No Null Position)

	// And then restore Usage Page before defining pen Pressure
	0xB4,              //     Pop

	0x09, 0x30,        //     Usage (Tip Pressure)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, 0xFF, 0x1F,  //     Logical Maximum (8191)
	0x75, 0x10,        //     Report Size (16)
	0x95, 0x01,        //     Report Count (1)
	// This variable was with null state flag before
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	// The rest is unused so we define two 16-bit variables with no usage

	0x95, 0x02,        //     Report Count (2)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0xC0,              //   End Collection
	0xC0,              // End Collection
};

static __u8 *g430sb_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				    unsigned int *rsize)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	if (intf->cur_altsetting->desc.bInterfaceNumber != G430SB_PEN_INTERFACE) {
		/* Skip interfaces we don't need */
		return 0;
	}
	
	if (*rsize == G430SB_RDESC_ORIG_SIZE) {
		rdesc = g430sb_rdesc_fixed;
		*rsize = sizeof(g430sb_rdesc_fixed);
		hid_info(hdev, "Report descriptor fixed, new *rsize=%d, interface=%d\n", *rsize, intf->cur_altsetting->desc.bInterfaceNumber);
	} else {
		/* Should never happen now */
		hid_info(hdev, "Report descriptor unchanged, because *rsize=%d, interface=%d\n", *rsize, intf->cur_altsetting->desc.bInterfaceNumber);
	}

	return rdesc;
}

static int g430sb_probe(struct hid_device *hdev,
			   const struct hid_device_id *id)
{
	int rc;
	__u8 g430sb_init_data[3] = {0x02, 0xB0, 0x04};
	__u8 *init_data;
	unsigned char endpoint = 0x03;
	int bytes_transferred = 0;
	int timeout = 1000;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		return rc;
	}

	struct usb_device *udev = hid_to_usb_dev(hdev);
	/* If we at wrong interface, exit early */
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	if (intf->cur_altsetting->desc.bInterfaceNumber != G430SB_PEN_INTERFACE) {
		hid_info(hdev, "Not a pen interface, disable.\n");
		return -ENODEV;
	}
	
	/* We need to use kernel memory instead of stack to make the function below*/
	init_data = kcalloc(sizeof(g430sb_init_data), 1, GFP_KERNEL);
	if (init_data == NULL) {
		hid_err(hdev, "failed to allocate memory\n");
		rc = -ENOMEM;
		goto cleanup;
	}
	memcpy(init_data, g430sb_init_data, sizeof(g430sb_init_data));


	/* We need to send three bytes to G430S_B to switch it into native mode
	*
	* extern int usb_interrupt_msg(struct usb_device *usb_dev, 
	*                                    unsigned int pipe, 
	*                                            void *data, 
	*                                             int len, 
	*                                             int *actual_length, 
	*                                             int timeout);
	*/
	rc = usb_interrupt_msg(udev, usb_sndintpipe(udev, endpoint), init_data, sizeof(g430sb_init_data), &bytes_transferred, timeout);
	if (rc) {
		hid_err(hdev, "init failed: rc:%d transferred:%d\n", rc, bytes_transferred);
		goto cleanup;
	} else {
		hid_info(hdev, "init success:%d, %d bytes sent\n", rc, bytes_transferred);
	}

	rc = hid_parse(hdev);
	if (rc) {
		hid_err(hdev, "parse failed\n");
		goto cleanup;
	}

	rc = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (rc) {
		hid_err(hdev, "hw start failed\n");
		goto cleanup;
	}

	rc = 0;
cleanup:
	kfree(init_data);
	return rc;
}

static const struct hid_device_id g430sb_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE,
				USB_DEVICE_ID_UGEE_XPPEN_TABLET_G430SB) },
	{ }
};
MODULE_DEVICE_TABLE(hid, g430sb_devices);

static struct hid_driver g430sb_driver = {
	.name = "g430sb",
	.id_table = g430sb_devices,
	.probe = g430sb_probe,
	.report_fixup = g430sb_report_fixup,
};

module_hid_driver(g430sb_driver);

MODULE_LICENSE("GPL");
