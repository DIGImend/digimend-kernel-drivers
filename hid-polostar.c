// SPDX-License-Identifier: GPL-2.0+
/*
 *  HID driver for Polostar devices not fully compliant with HID standard
 *
 *  Copyright (c) 2015 Yann Vernier
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

/* Size of the original descriptor of PT-1001 tablets */
#define PT1001_RDESC_ORIG_SIZE	317

/* Fixed PT1001 report descriptor */
static __u8 pt1001_rdesc_fixed[] = {
	0x05, 0x01,         /*  Usage Page (Desktop),                   */
	0x09, 0x02,         /*  Usage (Mouse),                          */
	0xA1, 0x01,         /*  Collection (Application),               */
	0x85, 0x01,         /*      Report ID (1),                      */
	0x09, 0x01,         /*      Usage (Pointer),                    */
	0xA1, 0x00,         /*      Collection (Physical),              */
	0x05, 0x09,         /*          Usage Page (Button),            */
	/* Swap mouse buttons for consistency with tablets */
	0x09, 0x01,         /*          Usage (1),                      */
	0x09, 0x03,         /*          Usage (3),                      */
	0x09, 0x02,         /*          Usage (2),                      */
	0x09, 0x04,         /*          Usage (4),                      */
	0x09, 0x05,         /*          Usage (5),                      */
	0x95, 0x05,         /*          Report Count (5),               */
	0x75, 0x01,         /*          Report Size (1),                */
	0x15, 0x00,         /*          Logical Minimum (0),            */
	0x25, 0x01,         /*          Logical Maximum (1),            */
	0x81, 0x02,         /*          Input (Variable),               */
	0x95, 0x03,         /*          Report Count (3),               */
	0x81, 0x01,         /*          Input (Constant),               */
	0x05, 0x01,         /*          Usage Page (Desktop),           */
	0x09, 0x30,         /*          Usage (X),                      */
	0x09, 0x31,         /*          Usage (Y),                      */
	0x95, 0x02,         /*          Report Count (2),               */
	0x75, 0x10,         /*          Report Size (16),               */
	0x16, 0x01, 0x80,   /*          Logical Minimum (-32767),       */
	0x26, 0xFF, 0x7F,   /*          Logical Maximum (32767),        */
	0x81, 0x06,         /*          Input (Variable, Relative),     */
	/* Scroll functionality */
	0x15, 0x81,         /*          Logical Minimum (-127),         */
	0x25, 0x7F,         /*          Logical Maximum (127),          */
	0x75, 0x08,         /*          Report Size (8),                */
	0x95, 0x01,         /*          Report Count (2),               */
	0x09, 0x38,         /*          Usage (Wheel),                  */
	0x05, 0x0C,         /*          Usage Page (Consumer),          */
	0x0A, 0x38, 0x02,   /*          Usage (AC Pan),                 */
	0x81, 0x06,         /*          Input (Variable, Relative),     */
	0xC0,               /*      End Collection,                     */
	0xC0,               /*  End Collection,                         */

	/* Report ID 5 is used for some periphery buttons */
	0x05, 0x0C,         /*  Usage Page (Consumer),                  */
	0x09, 0x01,         /*  Usage (Consumer Control),               */
	0xA1, 0x01,         /*  Collection (Application),               */
	0x85, 0x05,         /*      Report ID (5),                      */
	0x95, 0x01,         /*      Report Count (1),                   */
	0x75, 0x08,         /*      Report Size (8),                    */
	0x81, 0x01,         /*      Input (Constant),                   */
	0x15, 0x00,         /*      Logical Minimum (0),                */
	0x25, 0x01,         /*      Logical Maximum (1),                */
	0x75, 0x01,         /*      Report Size (1),                    */
	0x95, 0x12,         /*      Report Count (18),                  */
	0x0A, 0x83, 0x01,   /*      Usage (AL Consumer Control Config), */
	0x0A, 0x8A, 0x01,   /*      Usage (AL Email Reader),            */
	0x0A, 0x92, 0x01,   /*      Usage (AL Calculator),              */
	0x0A, 0x94, 0x01,   /*      Usage (AL Local Machine Brwsr),     */
	0x0A, 0x21, 0x02,   /*      Usage (AC Search),                  */
	0x0A, 0x23, 0x02,   /*      Usage (AC Home),                    */
	0x0A, 0x24, 0x02,   /*      Usage (AC Back),                    */
	0x0A, 0x25, 0x02,   /*      Usage (AC Forward),                 */
	0x0A, 0x26, 0x02,   /*      Usage (AC Stop),                    */
	0x0A, 0x27, 0x02,   /*      Usage (AC Refresh),                 */
	0x0A, 0x2A, 0x02,   /*      Usage (AC Bookmarks),               */
	0x09, 0xB5,         /*      Usage (Scan Next Track),            */
	0x09, 0xB6,         /*      Usage (Scan Previous Track),        */
	0x09, 0xB7,         /*      Usage (Stop),                       */
	0x09, 0xCD,         /*      Usage (Play Pause),                 */
	0x09, 0xE2,         /*      Usage (Mute),                       */
	0x09, 0xE9,         /*      Usage (Volume Inc),                 */
	0x09, 0xEA,         /*      Usage (Volume Dec),                 */
	0x81, 0x62,         /*      Input (Variable, No Preferred,      */
			    /*             Null State),                 */
	0x95, 0x06,         /*      Report Count (6),                   */
	0x75, 0x01,         /*      Report Size (1),                    */
	0x81, 0x03,         /*      Input (Constant, Variable),         */
	0xC0,               /*  End Collection,                         */

	/* Report 9 is the primary digitizer report. */
	0x05, 0x0D,         /*  Usage Page (Digitizer),                 */
	0x09, 0x01,         /*  Usage (Digitizer),                      */
	0xA1, 0x01,         /*  Collection (Application),               */
	0x85, 0x09,         /*      Report ID (9),                      */
	0x09, 0x20,         /*      Usage (Stylus),                     */
	0xA1, 0x00,         /*      Collection (Physical),              */

	0x09, 0x42,         /*          Usage (Tip Switch),             */
	0x09, 0x44,         /*          Usage (Barrel Switch),          */
	0x09, 0x46,         /*          Usage (Tablet Pick),            */
	0x15, 0x00,         /*          Logical Minimum (0),            */
	0x25, 0x01,         /*          Logical Maximum (1),            */
	0x95, 0x03,         /*          Report Count (3),               */
	0x75, 0x01,         /*          Report Size (1),                */
	0x81, 0x02,         /*          Input (Variable),               */

	0x95, 0x05,         /*          Report Count (5),               */
	0x81, 0x01,         /*          Input (Constant),               */

	0x15, 0x00,         /*          Logical Minimum (0),            */
	0x26, 0x00, 0x10,   /*          Logical Maximum (4096),         */
	0x75, 0x10,         /*          Report Size (16),               */
	0x95, 0x01,         /*          Report Count (1),               */

	0xa4,		    /*          Push                            */
	0x05, 0x01,         /*          Usage Page (Desktop),           */
	0x55, 0xfe,	    /*          Unit exponent -2: cm -> 0.1mm   */
	0x65, 0x11,         /*          Unit: SI linear centimeters     */

	0x09, 0x30,         /*          Usage (X),                      */
	0x35, 0x00,         /*          Physical Minimum (0),           */
	0x46, 0x20, 0x04,   /*          Physical Maximum (1056),        */
	0x81, 0x02,         /*          Input (Variable),               */

	0x09, 0x31,         /*          Usage (Y),                      */
	0x46, 0x94, 0x02,   /*          Physical Maximum (660),         */
	0x81, 0x02,         /*          Input (Variable),               */

	0xb4,		    /*          Pop (restore no unit)           */

	0x09, 0x30,         /*          Usage (Tip Pressure),           */
	0x26, 0xFF, 0x03,   /*          Logical Maximum (1023),         */
	0x81, 0x02,         /*          Input (Variable),               */
	0xC0,               /*      End Collection,                     */
	0xC0,               /*  End Collection                          */
};

static __u8 *polostar_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				   unsigned int *rsize)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;

	switch (hdev->product) {
	case USB_DEVICE_ID_POLOSTAR_TABLET_PT1001:
		if (iface_num == 1 && *rsize == PT1001_RDESC_ORIG_SIZE) {
			rdesc = pt1001_rdesc_fixed;
			*rsize = sizeof(pt1001_rdesc_fixed);
		}
		break;
	}

	return rdesc;
}

static int polostar_probe(struct hid_device *hdev,
			  const struct hid_device_id *id)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	int rc;

	if (hdev->product == USB_DEVICE_ID_POLOSTAR_TABLET_PT1001 &&
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

static const struct hid_device_id polostar_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_POLOSTAR,
				USB_DEVICE_ID_POLOSTAR_TABLET_PT1001),
	  .driver_data = HID_QUIRK_MULTI_INPUT },
	{ }
};
MODULE_DEVICE_TABLE(hid, polostar_devices);

static struct hid_driver polostar_driver = {
	.name = "polostar",
	.id_table = polostar_devices,
	.probe = polostar_probe,
	.report_fixup = polostar_report_fixup,
};
module_hid_driver(polostar_driver);

MODULE_LICENSE("GPL");
MODULE_VERSION("11");
