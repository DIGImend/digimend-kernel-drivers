/*
 *  HID driver for UC-Logic devices not fully compliant with HID standard
 *
 *  Copyright (c) 2010-2014 Nikolai Kondrashov
 *  Copyright (c) 2013 Martin Rusko
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
#include <asm/unaligned.h>
#include "usbhid/usbhid.h"
#include "hid-uclogic-rdesc.h"

#include "hid-ids.h"

#include "compat.h"
#include <linux/version.h>

#define UCLOGIC_PEN_REPORT_ID	0x07
#define UCLOGIC_HIRES_PEN_REPORT_ID	0x08

#define UCLOGIC_PRM_STR_ID (0x64)
#define UCLOGIC_HIRES_PRM_STR_ID (0xC8)
#define UCLOGIC_PRM_STR_LENGTH (UCLOGIC_PRM_NUM * sizeof(__le16))
#define UCLOGIC_HIRES_PRM_STR_LENGTH (18)

/* Parameter indices */
enum uclogic_prm {
	UCLOGIC_PRM_X_LM	= 1,
	UCLOGIC_PRM_Y_LM	= 2,
	UCLOGIC_PRM_PRESSURE_LM	= 4,
	UCLOGIC_PRM_RESOLUTION	= 5,
	UCLOGIC_PRM_NUM
};

enum uclogic_hires_prm_offset {
	UCLOGIC_HIRES_PRM_OFS_X_LM = 2,
	UCLOGIC_HIRES_PRM_OFS_Y_LM = 5,
	UCLOGIC_HIRES_PRM_OFS_PRESSURE_LM = 8,
	UCLOGIC_HIRES_PRM_OFS_RESOLUTION = 10
};

/* Driver data */
struct uclogic_drvdata {
	__u8 *rdesc;
	unsigned int rsize;
	bool tablet_enabled;
	bool buttons_enabled;
	bool invert_pen_inrange;
	bool ignore_pen_usage;
	bool has_virtual_pad_interface;
	bool is_hires;
};

static __u8 *uclogic_report_fixup(struct hid_device *hdev, __u8 *rdesc,
					unsigned int *rsize)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);

	if (drvdata->rdesc != NULL) {
		rdesc = drvdata->rdesc;
		*rsize = drvdata->rsize;
		return rdesc;
	}

	switch (hdev->product) {
	case USB_DEVICE_ID_UCLOGIC_TABLET_PF1209:
		if (*rsize == UCLOGIC_RDESC_PF1209_ORIG_SIZE) {
			rdesc = uclogic_rdesc_pf1209_fixed_arr;
			*rsize = uclogic_rdesc_pf1209_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U:
		if (*rsize == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			rdesc = uclogic_rdesc_wp4030u_fixed_arr;
			*rsize = uclogic_rdesc_wp4030u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U:
		if (*rsize == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			rdesc = uclogic_rdesc_wp5540u_fixed_arr;
			*rsize = uclogic_rdesc_wp5540u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U:
		if (*rsize == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			rdesc = uclogic_rdesc_wp8060u_fixed_arr;
			*rsize = uclogic_rdesc_wp8060u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP1062:
		if (*rsize == UCLOGIC_RDESC_WP1062_ORIG_SIZE) {
			rdesc = uclogic_rdesc_wp1062_fixed_arr;
			*rsize = uclogic_rdesc_wp1062_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850:
		switch (iface_num) {
		case 0:
			if (*rsize == UCLOGIC_RDESC_TWHL850_ORIG0_SIZE) {
				rdesc = uclogic_rdesc_twhl850_fixed0_arr;
				*rsize = uclogic_rdesc_twhl850_fixed0_size;
			}
			break;
		case 1:
			if (*rsize == UCLOGIC_RDESC_TWHL850_ORIG1_SIZE) {
				rdesc = uclogic_rdesc_twhl850_fixed1_arr;
				*rsize = uclogic_rdesc_twhl850_fixed1_size;
			}
			break;
		case 2:
			if (*rsize == UCLOGIC_RDESC_TWHL850_ORIG2_SIZE) {
				rdesc = uclogic_rdesc_twhl850_fixed2_arr;
				*rsize = uclogic_rdesc_twhl850_fixed2_size;
			}
			break;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60:
		switch (iface_num) {
		case 0:
			if (*rsize == UCLOGIC_RDESC_TWHA60_ORIG0_SIZE) {
				rdesc = uclogic_rdesc_twha60_fixed0_arr;
				*rsize = uclogic_rdesc_twha60_fixed0_size;
			}
			break;
		case 1:
			if (*rsize == UCLOGIC_RDESC_TWHA60_ORIG1_SIZE) {
				rdesc = uclogic_rdesc_twha60_fixed1_arr;
				*rsize = uclogic_rdesc_twha60_fixed1_size;
			}
			break;
		}
		break;
	}

	return rdesc;
}

static int uclogic_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);

	/* discard the unused pen interface */
	if ((drvdata->ignore_pen_usage) &&
	    (field->application == HID_DG_PEN))
		return -1;

	/* let hid-core decide what to do */
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
#define RETURN_SUCCESS return
static void uclogic_input_configured(struct hid_device *hdev,
		struct hid_input *hi)
#else
#define RETURN_SUCCESS return 0
static int uclogic_input_configured(struct hid_device *hdev,
		struct hid_input *hi)
#endif
{
	char *name;
	const char *suffix = NULL;
	struct hid_field *field;
	size_t len;

	/* no report associated (HID_QUIRK_MULTI_INPUT not set) */
	if (!hi->report)
		RETURN_SUCCESS;

	field = hi->report->field[0];

	switch (field->application) {
	case HID_GD_KEYBOARD:
		suffix = "Keyboard";
		break;
	case HID_GD_MOUSE:
		suffix = "Mouse";
		break;
	case HID_GD_KEYPAD:
		suffix = "Pad";
		break;
	case HID_DG_PEN:
		suffix = "Pen";
		break;
	case HID_CP_CONSUMER_CONTROL:
		suffix = "Consumer Control";
		break;
	case HID_GD_SYSTEM_CONTROL:
		suffix = "System Control";
		break;
	}

	if (suffix) {
		len = strlen(hdev->name) + 2 + strlen(suffix);
		name = devm_kzalloc(&hi->input->dev, len, GFP_KERNEL);
		if (name) {
			snprintf(name, len, "%s %s", hdev->name, suffix);
			hi->input->name = name;
		}
	}
	RETURN_SUCCESS;
}
#undef RETURN_SUCCESS

/**
 * Enable fully-functional tablet mode and retrieve device parameters.
 *
 * @hdev:	HID device
 * @pbuf:	Location for the kmalloc'ed parameter array with
 * 		UCLOGIC_PRM_NUM elements.
 */
static int uclogic_enable_tablet(struct hid_device *hdev, __u8 rdescid, __u8 **pbuf, size_t len)
{
	int rc;
	struct usb_device *usb_dev = hid_to_usb_dev(hdev);
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	__u8 *buf = NULL;

	/*
	 * Read string descriptor containing tablet parameters. The specific
	 * string descriptor and data were discovered by sniffing the Windows
	 * driver traffic.
	 * NOTE: This enables fully-functional tablet mode.
	 */
	buf = kmalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	rc = usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0),
				USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
				(USB_DT_STRING << 8) + rdescid,
				0x0409, buf, len,
				USB_CTRL_GET_TIMEOUT);
	if (rc == -EPIPE) {
		hid_err(hdev, "device parameters not found\n");
		rc = -ENODEV;
		goto cleanup;
	} else if (rc < 0) {
		hid_err(hdev, "failed to get device parameters: %d\n", rc);
		rc = -ENODEV;
		goto cleanup;
	} else if (rc != len) {
		hid_err(hdev, "invalid device parameters\n");
		rc = -ENODEV;
		goto cleanup;
	}

	drvdata->tablet_enabled = true;
	if (pbuf) {
		*pbuf = buf;
		buf = NULL;
	}
	rc = 0;

cleanup:
	kfree(buf);
	return rc;
}

static void uclogic_fill_placeholders(__u8* prdesc, unsigned int rsize, s32* params, size_t param_count)
{
	__u8 *p;
	s32 v;
	for (p = prdesc;
	     p <= prdesc + rsize -4;) {
		if (p[0] == 0xFE && p[1] == 0xED && p[2] == 0x1D &&
		    p[3] < param_count) {
			v = params[p[3]];
			put_unaligned(cpu_to_le32(v), (s32 *)p);
			p += 4;
		} else {
			p++;
		}
	}
}

/**
 * Enable fully-functional tablet mode, retrieve device parameters and
 * generate corresponding report descriptor.
 *
 * @hdev:		HID device
 * @rdesc_template_ptr	Report descriptor template pointer
 * @rdesc_template_len	Report descriptor template length
 */
static int uclogic_probe_tablet(struct hid_device *hdev,
				const __u8 *rdesc_template_ptr,
				size_t rdesc_template_len)
{
	int rc;
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	__u8 *bbuf = NULL;
	__le16 *buf = NULL;
	s32 params[UCLOGIC_RDESC_PH_ID_NUM];
	s32 resolution;

	/* Enable tablet mode and get raw device parameters */
	rc = uclogic_enable_tablet(hdev, UCLOGIC_PRM_STR_ID, &bbuf, UCLOGIC_PRM_STR_LENGTH);
	if (rc != 0) {
		goto cleanup;
	}
	buf = (__le16*)bbuf;

	/* Extract device parameters */
	params[UCLOGIC_RDESC_PH_ID_X_LM] = le16_to_cpu(buf[UCLOGIC_PRM_X_LM]);
	params[UCLOGIC_RDESC_PH_ID_Y_LM] = le16_to_cpu(buf[UCLOGIC_PRM_Y_LM]);
	params[UCLOGIC_RDESC_PH_ID_PRESSURE_LM] =
		le16_to_cpu(buf[UCLOGIC_PRM_PRESSURE_LM]);
	resolution = le16_to_cpu(buf[UCLOGIC_PRM_RESOLUTION]);
	if (resolution == 0) {
		params[UCLOGIC_RDESC_PH_ID_X_PM] = 0;
		params[UCLOGIC_RDESC_PH_ID_Y_PM] = 0;
	} else {
		params[UCLOGIC_RDESC_PH_ID_X_PM] =
			params[UCLOGIC_RDESC_PH_ID_X_LM] * 1000 / resolution;
		params[UCLOGIC_RDESC_PH_ID_Y_PM] =
			params[UCLOGIC_RDESC_PH_ID_Y_LM] * 1000 / resolution;
	}

	/* Allocate fixed report descriptor */
	drvdata->rdesc = devm_kzalloc(&hdev->dev,
				rdesc_template_len,
				GFP_KERNEL);
	if (drvdata->rdesc == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	drvdata->rsize = rdesc_template_len;

	/* Format fixed report descriptor */
	memcpy(drvdata->rdesc, rdesc_template_ptr, drvdata->rsize);
	uclogic_fill_placeholders(drvdata->rdesc, drvdata->rsize, params, sizeof(params)/sizeof(*params));

	rc = 0;

cleanup:
	kfree(buf);
	return rc;
}

static inline s32 le24_to_cpu(const __u8* ptr)
{
	return ptr[0] | (ptr[1] << 8UL) | (ptr[2] << 16UL);
}

static int uclogic_probe_tablet_hires(struct hid_device *hdev,
				const __u8 *rdesc_template_ptr,
				size_t rdesc_template_len)
{
	int rc;
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	__u8 *buf = NULL;
	s32 params[UCLOGIC_RDESC_PH_ID_NUM];
	s32 resolution;

	/* Enable tablet mode and get raw device parameters */
	rc = uclogic_enable_tablet(hdev, UCLOGIC_HIRES_PRM_STR_ID, &buf, UCLOGIC_HIRES_PRM_STR_LENGTH);
	if (rc != 0) {
		goto cleanup;
	}

	/* Extract device parameters */
	params[UCLOGIC_RDESC_PH_ID_X_LM] = le24_to_cpu(buf + UCLOGIC_HIRES_PRM_OFS_X_LM);
	params[UCLOGIC_RDESC_PH_ID_Y_LM] = le24_to_cpu(buf + UCLOGIC_HIRES_PRM_OFS_Y_LM);
	params[UCLOGIC_RDESC_PH_ID_PRESSURE_LM] =	le16_to_cpu(*(__le16*)(buf + UCLOGIC_HIRES_PRM_OFS_PRESSURE_LM));
	resolution = le16_to_cpu(*(__le16*)(buf + UCLOGIC_HIRES_PRM_OFS_RESOLUTION));
	if (resolution == 0) {
		params[UCLOGIC_RDESC_PH_ID_X_PM] = 0;
		params[UCLOGIC_RDESC_PH_ID_Y_PM] = 0;
	} else {
		params[UCLOGIC_RDESC_PH_ID_X_PM] = params[UCLOGIC_RDESC_PH_ID_X_LM] *
						   1000 / resolution;
		params[UCLOGIC_RDESC_PH_ID_Y_PM] = params[UCLOGIC_RDESC_PH_ID_Y_LM] *
						   1000 / resolution;
	}

	/* Allocate fixed report descriptor */
	drvdata->rdesc = devm_kzalloc(&hdev->dev,
				rdesc_template_len,
				GFP_KERNEL);
	if (drvdata->rdesc == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	drvdata->rsize = rdesc_template_len;

	/* Format fixed report descriptor */
	memcpy(drvdata->rdesc, rdesc_template_ptr, drvdata->rsize);
	uclogic_fill_placeholders(drvdata->rdesc, drvdata->rsize, params, sizeof(params)/sizeof(*params));
	rc = 0;

cleanup:
	kfree(buf);
	return rc;
}

/**
 * Enable generic button mode.
 *
 * @hdev:	HID device
 */
static int uclogic_enable_buttons(struct hid_device *hdev)
{
	int rc;
	struct usb_device *usb_dev = hid_to_usb_dev(hdev);
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	char *str_buf;
	size_t str_len = 16;

	str_buf = kzalloc(str_len, GFP_KERNEL);
	if (str_buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = usb_string(usb_dev, 0x7b, str_buf, str_len);
	if (rc == -EPIPE) {
		hid_info(hdev, "button mode setting not found\n");
		rc = 0;
		goto cleanup;
	} else if (rc < 0) {
		hid_err(hdev, "failed to enable abstract keyboard\n");
		goto cleanup;
	} else if (strncmp(str_buf, "HK On", rc)) {
		hid_info(hdev, "invalid answer when requesting buttons: '%s'\n",
			str_buf);
		rc = -EINVAL;
		goto cleanup;
	}

	drvdata->buttons_enabled = true;
	rc = 0;
cleanup:
	kfree(str_buf);
	return rc;
}

/**
 * Enable generic button mode, and substitute corresponding report descriptor.
 *
 * @hdev:	HID device
 */
static int uclogic_probe_buttons(struct hid_device *hdev)
{
	int rc;
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	unsigned char *rdesc;
	size_t rdesc_len;

	/* Enable generic button mode */
	rc = uclogic_enable_buttons(hdev);
	if (rc != 0) {
		goto cleanup;
	}

	/* Re-allocate fixed report descriptor */
	rdesc_len = drvdata->rsize + uclogic_rdesc_buttonpad_size;
	rdesc = devm_kzalloc(&hdev->dev, rdesc_len, GFP_KERNEL);
	if (!rdesc) {
		rc = -ENOMEM;
		goto cleanup;
	}

	memcpy(rdesc, drvdata->rdesc, drvdata->rsize);

	/* Append the buttonpad descriptor */
	memcpy(rdesc + drvdata->rsize, uclogic_rdesc_buttonpad_arr,
	       uclogic_rdesc_buttonpad_size);

	/* clean up old rdesc and use the new one */
	drvdata->rsize = rdesc_len;
	devm_kfree(&hdev->dev, drvdata->rdesc);
	drvdata->rdesc = rdesc;

	rc = 0;

cleanup:
	return rc;
}

static void uclogic_probe_model (const struct hid_device_id *id, struct hid_device *hdev)
{
	int rc;
	struct usb_device *usb_dev = hid_to_usb_dev(hdev);
	const char *vendor = NULL;
	char str_buf [64];

	switch (id->vendor) {
		case USB_VENDOR_ID_HUION:	vendor = "Huion"; break;
		case USB_VENDOR_ID_KYE:		vendor = "Genius"; break;
		case USB_VENDOR_ID_UCLOGIC:	vendor = "UC-Logic"; break;
		case USB_VENDOR_ID_POLOSTAR:	vendor = "Polostar"; break;
		case USB_VENDOR_ID_UGTIZER:	vendor = "UGTizer"; break;
		case USB_VENDOR_ID_UGEE:	vendor = "Ugee"; break;
	}

	rc = usb_string(usb_dev, 0x79, str_buf, sizeof(str_buf));
	if (rc >= 0) {
		if (vendor)
			snprintf(hdev->name, sizeof(hdev->name), "%s %s Tablet", vendor, str_buf);
		else
			snprintf(hdev->name, sizeof(hdev->name), "%s Tablet", str_buf);
	} else if (vendor)
		snprintf(hdev->name, sizeof(hdev->name), "%s Tablet", vendor);
}

static int uclogic_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	int rc;
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct usb_device *udev = hid_to_usb_dev(hdev);
	struct uclogic_drvdata *drvdata;

	/*
	 * libinput requires the pad interface to be on a different node
	 * than the pen, so use QUIRK_MULTI_INPUT for all tablets.
	 */
	hdev->quirks |= HID_QUIRK_MULTI_INPUT;
	hdev->quirks |= HID_QUIRK_NO_EMPTY_INPUT;

	/* Allocate and assign driver data */
	drvdata = devm_kzalloc(&hdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL)
		return -ENOMEM;

	hid_set_drvdata(hdev, drvdata);

	switch (id->product) {
	case USB_DEVICE_ID_HUION_TABLET:
	case USB_DEVICE_ID_YIYNOVA_TABLET:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81:
	case USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47:
		/* If this is the pen interface */
		if (intf->cur_altsetting->desc.bInterfaceNumber == 0) {
			rc = uclogic_probe_tablet_hires(
					hdev,
					uclogic_rdesc_tablet_hires_template_arr,
					uclogic_rdesc_tablet_hires_template_size);
			drvdata->is_hires = !rc;
			if (!drvdata->is_hires) {
				rc = uclogic_probe_tablet(
						hdev,
						uclogic_rdesc_tablet_template_arr,
						uclogic_rdesc_tablet_template_size);
			}
			if (rc) {
				hid_err(hdev, "tablet enabling failed\n");
				return rc;
			}
			drvdata->invert_pen_inrange = true;

			rc = uclogic_probe_buttons(hdev);
			drvdata->has_virtual_pad_interface = !rc;
		} else {
			drvdata->ignore_pen_usage = true;
		}
		break;
	case USB_DEVICE_ID_UGTIZER_TABLET_GP0610:
	case USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540:
		/* If this is the pen interface */
		if (intf->cur_altsetting->desc.bInterfaceNumber == 1) {
			rc = uclogic_probe_tablet(
					hdev,
					uclogic_rdesc_tablet_template_arr,
					uclogic_rdesc_tablet_template_size);
			if (rc) {
				hid_err(hdev, "tablet enabling failed\n");
				return rc;
			}
			drvdata->invert_pen_inrange = true;
		} else {
			drvdata->ignore_pen_usage = true;
		}
		break;
	case USB_DEVICE_ID_UGEE_TABLET_EX07S:
		/* If this is the pen interface */
		if (intf->cur_altsetting->desc.bInterfaceNumber == 1) {
			rc = uclogic_probe_tablet(
					hdev,
					uclogic_rdesc_ugee_ex07_template_arr,
					uclogic_rdesc_ugee_ex07_template_size);
			if (rc) {
				hid_err(hdev, "tablet enabling failed\n");
				return rc;
			}
			drvdata->invert_pen_inrange = true;
		} else {
			/* Ignore unused interface #0 */
			return -ENODEV;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60:
		/*
		 * If it is the three-interface version, which is known to
		 * respond to initialization.
		 */
		if (udev->config->desc.bNumInterfaces == 3) {
			/* If it is the pen interface */
			if (intf->cur_altsetting->desc.bInterfaceNumber == 0) {
				rc = uclogic_probe_tablet(
					hdev,
					uclogic_rdesc_tablet_template_arr,
					uclogic_rdesc_tablet_template_size);
				if (rc) {
					hid_err(hdev, "tablet enabling failed\n");
					return rc;
				}
				drvdata->invert_pen_inrange = true;

				rc = uclogic_probe_buttons(hdev);
				drvdata->has_virtual_pad_interface = !rc;
			} else {
				drvdata->ignore_pen_usage = true;
			}
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U:
		/* If this is the pen interface of WP5540U v2 */
		if (hdev->dev_rsize == UCLOGIC_RDESC_WP5540U_V2_ORIG_SIZE &&
		    intf->cur_altsetting->desc.bInterfaceNumber == 0) {
			rc = uclogic_probe_tablet(
					hdev,
					uclogic_rdesc_tablet_template_arr,
					uclogic_rdesc_tablet_template_size);
			if (rc) {
				hid_err(hdev, "tablet enabling failed\n");
				return rc;
			}
			drvdata->invert_pen_inrange = true;
		}
		break;
	}

	/* Give device a human-readable name rather than "HID xxxx:yyyy" */
	uclogic_probe_model(id, hdev);

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

#ifdef CONFIG_PM
static int uclogic_resume(struct hid_device *hdev)
{
	int rc;
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);

	/* Re-enable tablet, if needed */
	if (drvdata->tablet_enabled) {
		if (drvdata->is_hires) {
			rc = uclogic_enable_tablet(hdev, UCLOGIC_HIRES_PRM_STR_ID, NULL, UCLOGIC_HIRES_PRM_STR_LENGTH);
		}
		else {
			rc = uclogic_enable_tablet(hdev, UCLOGIC_PRM_STR_ID, NULL, UCLOGIC_PRM_STR_LENGTH);
		}
		if (rc != 0) {
			return rc;
		}
	}

	/* Re-enable buttons, if needed */
	if (drvdata->buttons_enabled) {
		rc = uclogic_enable_buttons(hdev);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}
#endif

static int uclogic_raw_event(struct hid_device *hdev, struct hid_report *report,
			u8 *data, int size)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);

	if ((report->type == HID_INPUT_REPORT) &&
	    (report->id == UCLOGIC_PEN_REPORT_ID || report->id == UCLOGIC_HIRES_PEN_REPORT_ID) &&
	    (size >= 2)) {
		if (drvdata->has_virtual_pad_interface && (data[1] & 0x20))
			/* Change to virtual frame button report ID */
			data[0] = 0xf7;
		else if (drvdata->invert_pen_inrange)
			/* Invert the in-range bit */
			data[1] ^= 0x40;
	}

	return 0;
}

static const struct hid_device_id uclogic_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_PF1209) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP1062) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_HUION, USB_DEVICE_ID_HUION_TABLET) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_HUION_TABLET) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_YIYNOVA_TABLET) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGTIZER, USB_DEVICE_ID_UGTIZER_TABLET_GP0610) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE, USB_DEVICE_ID_UGEE_TABLET_EX07S) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE, USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540) },
	{ }
};
MODULE_DEVICE_TABLE(hid, uclogic_devices);

static struct hid_driver uclogic_driver = {
	.name = "uclogic",
	.id_table = uclogic_devices,
	.probe = uclogic_probe,
	.report_fixup = uclogic_report_fixup,
	.raw_event = uclogic_raw_event,
	.input_mapping = uclogic_input_mapping,
	.input_configured = uclogic_input_configured,
#ifdef CONFIG_PM
	.resume	          = uclogic_resume,
	.reset_resume     = uclogic_resume,
#endif
};
module_hid_driver(uclogic_driver);

MODULE_AUTHOR("Martin Rusko");
MODULE_AUTHOR("Nikolai Kondrashov");
MODULE_LICENSE("GPL");
MODULE_VERSION("7");
