// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (c) 2019 Gabriel Zöller
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

#include "hid-ids.h"

/* Size of the original descriptor of Artist 13.3 tablets */
#define ARTIST133_RDESC_ORIG_SIZE	140

/* Fixed ARTIST133 report descriptor */
static __u8 artist133_rdesc_fixed[] = {
    0x05, 0x0D,                     /*  Usage Page (Digitizer),             */
    0x09, 0x02,                     /*  Usage (Pen),                        */
    0xA1, 0x01,                     /*  Collection (Application),           */
    0x85, 0x07,                     /*      Report ID (7),                  */
    0x09, 0x20,                     /*      Usage (Stylus),                 */
    0xA1, 0x00,                     /*      Collection (Physical),          */
    0x09, 0x42,                     /*          Usage (Tip Switch),         */
    0x09, 0x44,                     /*          Usage (Barrel Switch),      */
    0x09, 0x46,                     /*          Usage (Tablet Pick),        */
    0x15, 0x00,                     /*          Logical Minimum (0),        */
    0x25, 0x01,                     /*          Logical Maximum (1),        */
    0x75, 0x01,                     /*          Report Size (1),            */
    0x95, 0x03,                     /*          Report Count (3),           */
    0x81, 0x02,                     /*          Input (Variable),           */
    0x95, 0x02,                     /*          Report Count (2),           */
    0x81, 0x03,                     /*          Input (Constant, Variable), */
    0x09, 0x32,                     /*          Usage (In Range),           */
    0x95, 0x01,                     /*          Report Count (1),           */
    0x81, 0x02,                     /*          Input (Variable),           */
    0x95, 0x02,                     /*          Report Count (2),           */
    0x81, 0x03,                     /*          Input (Constant, Variable), */
    0x75, 0x10,                     /*          Report Size (16),           */
    0x95, 0x01,                     /*          Report Count (1),           */
    0x35, 0x00,                     /*          Physical Minimum (0),       */
    0xA4,                           /*          Push,                       */
    0x05, 0x01,                     /*          Usage Page (Desktop),       */
    0x09, 0x30,                     /*          Usage (X),                  */
    0x65, 0x13,                     /*          Unit (Inch),                */
    0x55, 0x0D,                     /*          Unit Exponent (13),         */
    0x46, 0x2D, 0x2D,               /*          Physical Maximum (11565),   */
    0x27, 0xC0, 0x72, 0x00, 0x00,   /*          Logical Maximum (29376),    */
    0x81, 0x02,                     /*          Input (Variable),           */
    0x09, 0x31,                     /*          Usage (Y),                  */
    0x46, 0x69, 0x19,               /*          Physical Maximum (6505),    */
    0x27, 0x8C, 0x40, 0x00, 0x00,   /*          Logical Maximum (16524),    */
    0x81, 0x02,                     /*          Input (Variable),           */
    0xB4,                           /*          Pop,                        */
    0x09, 0x30,                     /*          Usage (Tip Pressure),       */
    0x45, 0x00,                     /*          Physical Maximum (0),       */
    0x26, 0xFF, 0x1F,               /*          Logical Maximum (8191),     */
    0x81, 0x02,                     /*          Input (Variable),           */
    0xC0,                           /*      End Collection,                 */
    0xC0,                           /*  End Collection,                     */
    0x09, 0x0E,                     /*  Usage (Configuration),              */
    0xA1, 0x01,                     /*  Collection (Application),           */
    0x85, 0x05,                     /*      Report ID (5),                  */
    0x09, 0x23,                     /*      Usage (Device Settings),        */
    0xA1, 0x02,                     /*      Collection (Logical),           */
    0x09, 0x52,                     /*          Usage (Device Mode),        */
    0x09, 0x53,                     /*          Usage (Device Identifier),  */
    0x25, 0x0A,                     /*          Logical Maximum (10),       */
    0x75, 0x08,                     /*          Report Size (8),            */
    0x95, 0x02,                     /*          Report Count (2),           */
    0xB1, 0x02,                     /*          Feature (Variable),         */
    0xC0,                           /*      End Collection,                 */
    0xC0,                           /*  End Collection,                     */
    0x05, 0x0C,                     /*  Usage Page (Consumer),              */
    0x09, 0x36,                     /*  Usage (Function Buttons),           */
    0xA1, 0x00,                     /*  Collection (Physical),              */
    0x85, 0x06,                     /*      Report ID (6),                  */
    0x05, 0x09,                     /*      Usage Page (Button),            */
    0x19, 0x01,                     /*      Usage Minimum (01h),            */
    0x29, 0x20,                     /*      Usage Maximum (20h),            */
    0x15, 0x00,                     /*      Logical Minimum (0),            */
    0x25, 0x01,                     /*      Logical Maximum (1),            */
    0x95, 0x20,                     /*      Report Count (32),              */
    0x75, 0x01,                     /*      Report Size (1),                */
    0x81, 0x02,                     /*      Input (Variable),               */
    0xC0                            /*  End Collection                      */
};

static __u8 *artist133_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				   unsigned int *rsize)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;

	switch (hdev->product) {
	case USB_DEVICE_ID_UGEE_XPPEN_ARTIST_133:
		if (iface_num == 1 && *rsize == ARTIST133_RDESC_ORIG_SIZE) {
			rdesc = artist133_rdesc_fixed;
			*rsize = sizeof(artist133_rdesc_fixed);
		}
		break;
	}

	return rdesc;
}

static int artist133_probe(struct hid_device *hdev,
			  const struct hid_device_id *id)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	int rc;

	if (hdev->product == USB_DEVICE_ID_UGEE_XPPEN_ARTIST_133 &&
	    iface_num == 2)
		return -ENODEV;

	hdev->quirks |= id->driver_data;

	rc = hid_parse(hdev);
	if (rc) {
		hid_err(hdev, "parse failed\n");
		return rc;
	}

	rc = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (rc) {
		hid_err(hdev, "hw start failed\n");
		return rc;
	}

	return 0;
}

static const struct hid_device_id artist133_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE,
				USB_DEVICE_ID_UGEE_XPPEN_ARTIST_133),
	  .driver_data = HID_QUIRK_MULTI_INPUT },
	{ }
};
MODULE_DEVICE_TABLE(hid, artist133_devices);

static struct hid_driver xppen_driver = {
	.name = "xppen",
	.id_table = artist133_devices,
	.probe = artist133_probe,
	.report_fixup = artist133_report_fixup,
};
module_hid_driver(xppen_driver);

MODULE_LICENSE("GPL");
MODULE_VERSION("10");
