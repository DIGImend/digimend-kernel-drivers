/*
 *  HID driver for UC-Logic devices not fully compliant with HID standard
 *  - tablet initialization and parameter retrieval
 *
 *  Copyright (c) 2018 Nikolai Kondrashov
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "hid-uclogic-params.h"
#include "hid-uclogic-rdesc.h"
#include "usbhid/usbhid.h"
#include "hid-ids.h"
#include <linux/ctype.h>
#include <asm/unaligned.h>

/**
 * uclogic_params_get_str_desc - retrieve a string descriptor from a HID
 * device interface, putting it into a kmalloc-allocated buffer as is, without
 * character encoding conversion.
 *
 * @pbuf:	Location for the kmalloc-allocated buffer pointer containing
 * 		the retrieved descriptor. Not modified in case of error.
 * 		Can be NULL to have retrieved descriptor discarded.
 * @idx:	Index of the string descriptor to request from the device.
 * @len:	Length of the buffer to allocate and the data to retrieve.
 *
 * Return:
 * 	number of bytes retrieved (<= len),
 * 	-EPIPE, if the descriptor was not found, or
 *	another negative errno code in case of other error.
 */
static int uclogic_params_get_str_desc(__u8 **pbuf, struct hid_device *hdev,
					__u8 idx, size_t len)
{
	int rc;
	struct usb_device *udev = hid_to_usb_dev(hdev);
	__u8 *buf = NULL;

	buf = kmalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
				(USB_DT_STRING << 8) + idx,
				0x0409, buf, len,
				USB_CTRL_GET_TIMEOUT);
	if (rc == -EPIPE) {
		hid_dbg(hdev, "string descriptor #%hhu not found\n", idx);
		goto cleanup;
	} else if (rc < 0) {
		hid_err(hdev,
			"failed retrieving string descriptor #%hhu: %d\n",
			idx, rc);
		goto cleanup;
	}

	if (pbuf != NULL) {
		*pbuf = buf;
		buf = NULL;
	}

cleanup:
	kfree(buf);
	return rc;
}

/* Tablet interface's pen input parameters */
/* TODO Consider stripping "report" from names */
struct uclogic_params_pen {
	/* Pointer to report descriptor allocated with kmalloc */
	__u8 *desc_ptr;
	/* Size of the report descriptor */
	unsigned int desc_size;
	/* Pen report ID */
	unsigned id;
	/* Type of pen in-range reporting */
	enum uclogic_params_pen_inrange inrange;
	/*
	 * True, if pen reports include fragmented high resolution coords,
	 * with high-order X and then Y bytes following the pressure field
	 */
	bool fragmented_hires;
};

/**
 * uclogic_params_pen_free - free resources used by struct uclogic_params_pen
 * (tablet interface's pen input parameters).
 *
 * @pen:	Pen input parameters to free. Can be NULL.
 */
static void uclogic_params_pen_free(struct uclogic_params_pen *pen)
{
	if (pen != NULL) {
		kfree(pen->desc_ptr);
		memset(pen, 0, sizeof(*pen));
		kfree(pen);
	}
}

/**
 * uclogic_params_pen_v1_probe() - initialize tablet interface pen
 * input and retrieve its parameters from the device, using v1 protocol.
 *
 * @ppen: 	Location for the pointer to resulting pen parameters (to be
 * 		freed with uclogic_params_pen_free()), or for NULL if the pen
 * 		parameters were not found or recognized.  Not modified in case
 * 		of error. Can be NULL to have parameters discarded after
 * 		retrieval.
 * @hdev:	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_pen_v1_probe(struct uclogic_params_pen **ppen,
					struct hid_device *hdev)
{
	int rc;
	/* Buffer for (part of) the string descriptor */
	__u8 *buf = NULL;
	/* Minimum descriptor length required, maximum seen so far is 18 */
	const int len = 12;
	s32 resolution;
	/* Pen report descriptor template parameters */
	s32 desc_params[UCLOGIC_RDESC_PEN_PH_ID_NUM];
	__u8 *desc_ptr = NULL;
	struct uclogic_params_pen *pen = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	/*
	 * Read string descriptor containing pen input parameters.
	 * The specific string descriptor and data were discovered by sniffing
	 * the Windows driver traffic.
	 * NOTE: This enables fully-functional tablet mode.
	 */
	rc = uclogic_params_get_str_desc(&buf, hdev, 100, len);
	if (rc == -ENOENT) {
		hid_dbg(hdev,
			"string descriptor with pen parameters not found, "
			"assuming not compatible\n");
		goto output;
	} else if (rc < 0) {
		hid_err(hdev, "failed retrieving pen parameters: %d\n", rc);
		goto cleanup;
	} else if (rc != len) {
		hid_dbg(hdev,
			"string descriptor with pen parameters has "
			"invalid length (got %d, expected %d), "
			"assuming not compatible\n",
			rc, len);
		goto output;
	}

	/*
	 * Fill report descriptor parameters from the string descriptor
	 */
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] =
		get_unaligned_le16(buf + 2);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] =
		get_unaligned_le16(buf + 4);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] =
		get_unaligned_le16(buf + 8);
	resolution = get_unaligned_le16(buf + 10);
	if (resolution == 0) {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = 0;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = 0;
	} else {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] * 1000 /
			resolution;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] * 1000 /
			resolution;
	}
	kfree(buf);
	buf = NULL;

	/*
	 * Generate pen report descriptor
	 */
	desc_ptr = uclogic_rdesc_template_apply(
				uclogic_rdesc_pen_v1_template_arr,
				uclogic_rdesc_pen_v1_template_size,
				desc_params, ARRAY_SIZE(desc_params));
	if (desc_ptr == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/*
	 * Fill-in the parameters
	 */
	pen = kzalloc(sizeof(*pen), GFP_KERNEL);
	if (pen == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	pen->desc_ptr = desc_ptr;
	desc_ptr = NULL;
	pen->desc_size = uclogic_rdesc_pen_v1_template_size;
	pen->id = UCLOGIC_RDESC_PEN_V1_ID;
	pen->inrange = UCLOGIC_PARAMS_PEN_INRANGE_INVERTED;

output:
	/*
	 * Output the parameters, if requested
	 */
	if (ppen != NULL) {
		*ppen = pen;
		pen = NULL;
	}

	rc = 0;
cleanup:
	uclogic_params_pen_free(pen);
	kfree(desc_ptr);
	kfree(buf);
	return rc;
}

/**
 * uclogic_params_get_le24() - get a 24-bit little-endian number from a
 * buffer.
 *
 * @p:	The pointer to the number buffer.
 *
 * Return:
 * 	The retrieved number
 */
static s32 uclogic_params_get_le24(const void *p)
{
	const __u8* b = p;
	return b[0] | (b[1] << 8UL) | (b[2] << 16UL);
}

/**
 * uclogic_params_pen_v2_probe() - initialize tablet interface pen
 * input and retrieve its parameters from the device, using v2 protocol.
 *
 * @ppen: 	Location for the pointer to resulting pen parameters (to be
 * 		freed with uclogic_params_pen_free()), or for NULL if the pen
 * 		parameters were not found or recognized.  Not modified in case
 * 		of error. Can be NULL to have parameters discarded after
 * 		retrieval.
 * @hdev:	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_pen_v2_probe(struct uclogic_params_pen **ppen,
					struct hid_device *hdev)
{
	int rc;
	/* Buffer for (part of) the string descriptor */
	__u8 *buf = NULL;
	/* Descriptor length required */
	const int len = 18;
	s32 resolution;
	/* Pen report descriptor template parameters */
	s32 desc_params[UCLOGIC_RDESC_PEN_PH_ID_NUM];
	__u8 *desc_ptr = NULL;
	struct uclogic_params_pen *pen = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	/*
	 * Read string descriptor containing pen input parameters.
	 * The specific string descriptor and data were discovered by sniffing
	 * the Windows driver traffic.
	 * NOTE: This enables fully-functional tablet mode.
	 */
	rc = uclogic_params_get_str_desc(&buf, hdev, 200, len);
	if (rc == -ENOENT) {
		hid_dbg(hdev,
			"string descriptor with pen parameters not found, "
			"assuming not compatible\n");
		rc = 0;
		goto cleanup;
	} else if (rc < 0) {
		hid_err(hdev, "failed retrieving pen parameters: %d\n", rc);
		goto cleanup;
	} else if (rc != len) {
		hid_dbg(hdev,
			"string descriptor with pen parameters has "
			"invalid length (got %d, expected %d), "
			"assuming not compatible\n",
			rc, len);
		goto output;
	} else {
		size_t i;
		/*
		 * Check it's not just a catch-all UTF-16LE-encoded ASCII
		 * string (such as the model name) some tablets put into all
		 * unknown string descriptors.
		 */
		for (i = 2;
		     i < len &&
			(buf[i] >= 0x20 && buf[i] < 0x7f && buf[i + 1] == 0);
		     i += 2);
		if (i >= len) {
			hid_dbg(hdev,
				"string descriptor with pen parameters "
				"seems to contain only text, "
				"assuming not compatible\n");
			goto output;
		}
	}

	/*
	 * Fill report descriptor parameters from the string descriptor
	 */
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] =
		uclogic_params_get_le24(buf + 2);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] =
		uclogic_params_get_le24(buf + 5);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] =
		get_unaligned_le16(buf + 8);
	resolution = get_unaligned_le16(buf + 10);
	if (resolution == 0) {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = 0;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = 0;
	} else {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] * 1000 /
			resolution;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] * 1000 /
			resolution;
	}
	kfree(buf);
	buf = NULL;

	/*
	 * Generate pen report descriptor
	 */
	desc_ptr = uclogic_rdesc_template_apply(
				uclogic_rdesc_pen_v2_template_arr,
				uclogic_rdesc_pen_v2_template_size,
				desc_params, ARRAY_SIZE(desc_params));
	if (desc_ptr == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/*
	 * Fill-in the parameters
	 */
	pen = kzalloc(sizeof(*pen), GFP_KERNEL);
	if (pen == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	pen->desc_ptr = desc_ptr;
	desc_ptr = NULL;
	pen->desc_size = uclogic_rdesc_pen_v2_template_size;
	pen->id = UCLOGIC_RDESC_PEN_V2_ID;
	pen->inrange = UCLOGIC_PARAMS_PEN_INRANGE_NONE;
	pen->fragmented_hires = true;

output:
	/*
	 * Output the parameters, if requested
	 */
	if (ppen != NULL) {
		*ppen = pen;
		pen = NULL;
	}

	rc = 0;
cleanup:
	uclogic_params_pen_free(pen);
	kfree(desc_ptr);
	kfree(buf);
	return rc;
}

/* Parameters of frame control inputs of a tablet interface */
struct uclogic_params_frame {
	/* Pointer to report descriptor allocated with kmalloc */
	__u8 *desc_ptr;
	/* Size of the report descriptor */
	unsigned int desc_size;
};

/**
 * uclogic_params_frame_free - free resources used by struct
 * uclogic_params_frame (tablet interface's frame controls input parameters).
 *
 * @frame:	Frame controls input parameters to free. Can be NULL.
 */
static void uclogic_params_frame_free(struct uclogic_params_frame *frame)
{
	if (frame != NULL) {
		kfree(frame->desc_ptr);
		memset(frame, 0, sizeof(*frame));
		kfree(frame);
	}
}

/**
 * uclogic_params_frame_create() - create tablet frame controls
 * parameters from a static report descriptor.
 *
 * @pframe:	Location for the pointer to resulting frame controls
 * 		parameters (to be freed with uclogic_params_frame_free()).
 * 		Not modified in case of error. Can be NULL to have parameters
 * 		discarded after creation.
 * @desc_ptr:	Report descriptor pointer. Can be NULL, if desc_size is zero.
 * @desc_size:	Report descriptor size.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_frame_create(struct uclogic_params_frame **pframe,
				       const __u8 *desc_ptr,
				       size_t desc_size)
{
	int rc;
	struct uclogic_params_frame *frame = NULL;

	frame = kzalloc(sizeof(*frame), GFP_KERNEL);
	if (frame == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	frame->desc_ptr = kmemdup(desc_ptr, desc_size, GFP_KERNEL);
	if (frame->desc_ptr == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	frame->desc_size = desc_size;

	/*
	 * Output the parameters, if requested
	 */
	if (pframe != NULL) {
		*pframe = frame;
		frame = NULL;
	}

	rc = 0;
cleanup:
	uclogic_params_frame_free(frame);
	return rc;
}

/**
 * uclogic_params_frame_buttonpad_v1_probe() - initialize abstract buttonpad
 * on a v1 tablet interface.
 *
 * @pframe:	Location for the pointer to resulting frame controls
 * 		parameters (to be freed with uclogic_params_frame_free()), or
 * 		for NULL if the frame controls parameters were not found or
 * 		recognized.  Not modified in case of error. Can be NULL to
 * 		have parameters discarded after retrieval.
 * @hdev:	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_frame_buttonpad_v1_probe(
					struct uclogic_params_frame **pframe,
					struct hid_device *hdev)
{
	int rc;
	struct usb_device *usb_dev = hid_to_usb_dev(hdev);
	char *str_buf;
	size_t str_len = 16;
	struct uclogic_params_frame *frame = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	/*
	 * Enable generic button mode
	 */
	str_buf = kzalloc(str_len, GFP_KERNEL);
	if (str_buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = usb_string(usb_dev, 123, str_buf, str_len);
	if (rc == -EPIPE) {
		hid_dbg(hdev, "generic button -enabling string descriptor "
				"not found\n");
	} else if (rc < 0) {
		goto cleanup;
	} else if (strncmp(str_buf, "HK On", rc) != 0) {
		hid_dbg(hdev, "invalid response to enabling generic "
				"buttons: \"%s\"\n", str_buf);
	} else {
		hid_dbg(hdev, "generic buttons enabled\n");
		rc = uclogic_params_frame_create(
				&frame,
				uclogic_rdesc_buttonpad_v1_arr,
				uclogic_rdesc_buttonpad_v1_size);
		if (rc != 0) {
			goto cleanup;
		}
	}

	/*
	 * Output the parameters, if requested
	 */
	if (pframe != NULL) {
		*pframe = frame;
		frame = NULL;
	}

	rc = 0;
cleanup:
	uclogic_params_frame_free(frame);
	kfree(str_buf);
	return rc;
}

/**
 * uclogic_params_free - free resources used by struct uclogic_params
 * (tablet interface's input parameters).
 *
 * @params:	Input parameters to free. Can be NULL.
 */
void uclogic_params_free(struct uclogic_params *params)
{
	if (params != NULL) {
		kfree(params->desc_ptr);
		params->desc_ptr = NULL;
		kfree(params);
	}
}

/**
 * uclogic_params_probe_static() - probe a tablet interface with
 * statically-assigned parameters.
 *
 * @pparams: 	Location for the pointer to resulting parameters (to be
 * 		freed with uclogic_params_free()), or for NULL if the
 * 		parameters were not found.  Not modified in case of error.
 * 		Can be NULL to have parameters discarded after retrieval.
 * @hdev:	The HID device of the tablet interface to initialize and
 * 		possibly get parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_probe_static(struct uclogic_params **pparams,
			 		struct hid_device *hdev)
{
	int rc;
	struct usb_device *udev = hid_to_usb_dev(hdev);
	__u8  bNumInterfaces = udev->config->desc.bNumInterfaces;
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 bInterfaceNumber = iface->cur_altsetting->desc.bInterfaceNumber;
	/* The device's original report descriptor size */
	unsigned orig_desc_size = hdev->dev_rsize;
	/*
	 * The replacement report descriptor pointer,
	 * or NULL if no replacement is needed.
	 */
	const __u8 *desc_ptr = NULL;
	/*
	 * The replacement report descriptor size.
	 * Only valid if desc_ptr is not NULL.
	 */
	unsigned int desc_size = 0;
	/*
	 * True if the report descriptor contains several reports, one of
	 * which is a pen report, which is unused, but some of the others may
	 * still report something. TODO Switch to just returning NULL
	 * parameters, so the whole interface is ignored instead, if the other
	 * reports are also unused after all, or to rewriting the report
	 * descriptor.
	 */
	bool pen_unused = false;
	/* The resulting parameters */
	struct uclogic_params *params = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	/*
	 * Handle some models with static report descriptors
	 */
	switch (hdev->product) {
	case USB_DEVICE_ID_UCLOGIC_TABLET_PF1209:
		if (orig_desc_size == UCLOGIC_RDESC_PF1209_ORIG_SIZE) {
			desc_ptr = uclogic_rdesc_pf1209_fixed_arr;
			desc_size = uclogic_rdesc_pf1209_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U:
		if (orig_desc_size == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			desc_ptr = uclogic_rdesc_wp4030u_fixed_arr;
			desc_size = uclogic_rdesc_wp4030u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U:
		if (orig_desc_size == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			desc_ptr = uclogic_rdesc_wp5540u_fixed_arr;
			desc_size = uclogic_rdesc_wp5540u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U:
		if (orig_desc_size == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			desc_ptr = uclogic_rdesc_wp8060u_fixed_arr;
			desc_size = uclogic_rdesc_wp8060u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP1062:
		if (orig_desc_size == UCLOGIC_RDESC_WP1062_ORIG_SIZE) {
			desc_ptr = uclogic_rdesc_wp1062_fixed_arr;
			desc_size = uclogic_rdesc_wp1062_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850:
		switch (bInterfaceNumber) {
		case 0:
			if (orig_desc_size == UCLOGIC_RDESC_TWHL850_ORIG0_SIZE) {
				desc_ptr = uclogic_rdesc_twhl850_fixed0_arr;
				desc_size = uclogic_rdesc_twhl850_fixed0_size;
			}
			break;
		case 1:
			if (orig_desc_size == UCLOGIC_RDESC_TWHL850_ORIG1_SIZE) {
				desc_ptr = uclogic_rdesc_twhl850_fixed1_arr;
				desc_size = uclogic_rdesc_twhl850_fixed1_size;
			}
			break;
		case 2:
			if (orig_desc_size == UCLOGIC_RDESC_TWHL850_ORIG2_SIZE) {
				desc_ptr = uclogic_rdesc_twhl850_fixed2_arr;
				desc_size = uclogic_rdesc_twhl850_fixed2_size;
			}
			break;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60:
		/*
		 * If it is the three-interface version, which is known to
		 * respond to initialization.
		 */
		if (bNumInterfaces == 3) {
			pen_unused = (bInterfaceNumber != 0);
			break;
		}
		switch (bInterfaceNumber) {
		case 0:
			if (orig_desc_size == UCLOGIC_RDESC_TWHA60_ORIG0_SIZE) {
				desc_ptr = uclogic_rdesc_twha60_fixed0_arr;
				desc_size = uclogic_rdesc_twha60_fixed0_size;
			}
			break;
		case 1:
			if (orig_desc_size == UCLOGIC_RDESC_TWHA60_ORIG1_SIZE) {
				desc_ptr = uclogic_rdesc_twha60_fixed1_arr;
				desc_size = uclogic_rdesc_twha60_fixed1_size;
			}
			break;
		}
		break;
	case USB_DEVICE_ID_HUION_TABLET:
	case USB_DEVICE_ID_YIYNOVA_TABLET:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81:
	case USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47:
		pen_unused = (bInterfaceNumber != 0);
		break;
	case USB_DEVICE_ID_UGTIZER_TABLET_GP0610:
	case USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540:
		pen_unused = (bInterfaceNumber != 1);
		break;
	}

	/*
	 * If we got a replacement for the report descriptor,
	 * or we have to tweak its interpretation.
	 */
	if (desc_ptr != NULL || pen_unused) {
		/* Create parameters */
		params = kzalloc(sizeof(*params), GFP_KERNEL);
		if (params == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}
		params->pen_unused = pen_unused;
		if (desc_ptr != NULL) {
			params->desc_ptr =
				kmemdup(desc_ptr, desc_size, GFP_KERNEL);
			if (params->desc_ptr == NULL) {
				rc = -ENOMEM;
				goto cleanup;
			}
			params->desc_size = desc_size;
		}
	}

	/* Output parameters, if requested */
	if (pparams != NULL) {
		*pparams = params;
		params = NULL;
	}

	rc = 0;

cleanup:
	uclogic_params_free(params);
	return rc;
}

/**
 * uclogic_params_probe_dynamic() - initialize a tablet interface and retrieve
 * its parameters from the device.
 *
 * @pparams: 	Location for the pointer to resulting parameters (to be
 * 		freed with uclogic_params_free()), or for NULL if the
 * 		parameters were not found.  Not modified in case of error.
 * 		Can be NULL to have parameters discarded after retrieval.
 * @hdev:	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_probe_dynamic(struct uclogic_params **pparams,
					struct hid_device *hdev)
{
	int rc;
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 bInterfaceNumber = iface->cur_altsetting->desc.bInterfaceNumber;
	/* Pen input parameters */
	struct uclogic_params_pen *pen = NULL;
	/*
	 * Bitmask matching frame controls "sub-report" flag in the second
	 * byte of the pen report, or zero if it's not expected.
	 */
	__u8 pen_frame_flag = 0;
	/* Frame controls' input parameters */
	struct uclogic_params_frame *frame = NULL;
	/*
	 * Frame controls report ID. Used as the virtual frame report ID, for
	 * frame button reports extracted from pen reports, if
	 * pen_frame_flag is valid and not zero.
	 */
	unsigned pen_frame_id = 0;
	/* The resulting interface parameters */
	struct uclogic_params *params = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	switch (hdev->product) {
	case USB_DEVICE_ID_HUION_TABLET:
	case USB_DEVICE_ID_YIYNOVA_TABLET:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81:
	case USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47:
	case USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60:
		/* Skip non-pen interfaces */
		if (bInterfaceNumber != 0) {
			break;
		}

		/* Try to probe v2 pen parameters */
		rc = uclogic_params_pen_v2_probe(&pen, hdev);
		if (rc < 0) {
			hid_err(hdev,
				"failed probing pen v2 parameters: %d\n", rc);
		} else if (pen != NULL) {
			hid_dbg(hdev, "pen v2 parameters found\n");
			rc = uclogic_params_frame_create(
					&frame,
					uclogic_rdesc_buttonpad_v2_arr,
					uclogic_rdesc_buttonpad_v2_size);
			if (rc != 0) {
				hid_err(hdev, "failed creating v2 buttonpad "
					"parameters: %d\n", rc);
				goto cleanup;
			}
			pen_frame_flag = 0x20;
			pen_frame_id = UCLOGIC_RDESC_BUTTONPAD_V2_ID;
			break;
		}
		hid_dbg(hdev, "pen v2 parameters not found\n");

		/* Try to probe v1 pen parameters */
		rc = uclogic_params_pen_v1_probe(&pen, hdev);
		if (rc < 0) {
			hid_err(hdev,
				"failed probing pen v1 parameters: %d\n", rc);
		} else if (pen != NULL) {
			hid_dbg(hdev, "pen v1 parameters found\n");
			rc = uclogic_params_frame_buttonpad_v1_probe(
							&frame, hdev);
			if (rc != 0) {
				hid_err(hdev, "v1 buttonpad probing "
					"failed: %d\n", rc);
				goto cleanup;
			}
			pen_frame_flag = 0x20;
			pen_frame_id = UCLOGIC_RDESC_BUTTONPAD_V1_ID;
			break;
		}
		hid_dbg(hdev, "pen v1 parameters not found\n");

		break;
	case USB_DEVICE_ID_UGTIZER_TABLET_GP0610:
	case USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540:
		/* If this is the pen interface */
		if (bInterfaceNumber == 1) {
			rc = uclogic_params_pen_v1_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
		}
		break;
	case USB_DEVICE_ID_UGEE_TABLET_EX07S:
		/* Skip non-pen interfaces */
		if (bInterfaceNumber != 1) {
			break;
		}

		/* Probe pen parameters */
		rc = uclogic_params_pen_v1_probe(&pen, hdev);
		if (rc != 0) {
			hid_err(hdev, "pen probing failed: %d\n", rc);
			goto cleanup;
		} else if (pen != NULL) {
			/* Create frame parameters */
			rc = uclogic_params_frame_create(
				&frame,
				uclogic_rdesc_ugee_ex07_buttonpad_arr,
				uclogic_rdesc_ugee_ex07_buttonpad_size);
			if (rc != 0) {
				hid_err(hdev, "failed creating buttonpad "
					"parameters: %d\n", rc);
				goto cleanup;
			}
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U:
		/* If this is the pen interface of WP5540U v2 */
		if (hdev->dev_rsize == UCLOGIC_RDESC_WP5540U_V2_ORIG_SIZE &&
		    bInterfaceNumber == 0) {
			rc = uclogic_params_pen_v1_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
		}
		break;
	}

	/*
	 * Check if we found anything
	 */
	hid_dbg(hdev, "pen parameters %sfound\n",
			(pen == NULL ? "not " : ""));
	hid_dbg(hdev, "frame parameters %sfound\n",
			(frame == NULL ? "not " : ""));
	if (pen == NULL && frame == NULL) {
		goto output;
	}

	/*
	 * Create parameters
	 */
	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (params == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	if (pen != NULL) {
		params->desc_size += pen->desc_size;
		params->pen_id = pen->id;
		params->pen_inrange = pen->inrange;
		params->pen_fragmented_hires = pen->fragmented_hires;
		params->pen_frame_flag = pen_frame_flag;
		params->pen_frame_id = pen_frame_id;
	}
	if (frame != NULL) {
		params->desc_size += frame->desc_size;
	}

	/*
	 * Merge report descriptors, if any
	 */
	if (params->desc_size > 0) {
		__u8 *p;

		params->desc_ptr = kmalloc(params->desc_size, GFP_KERNEL);
		if (params->desc_ptr == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}

		p = params->desc_ptr;
		if (pen != NULL) {
			memcpy(p, pen->desc_ptr, pen->desc_size);
			p += pen->desc_size;
		}
		if (frame != NULL) {
			memcpy(p, frame->desc_ptr, frame->desc_size);
			p += frame->desc_size;
		}
		WARN_ON(p != params->desc_ptr + params->desc_size);
	}

output:
	/*
	 * Output parameters, if requested
	 */
	if (pparams != NULL) {
		*pparams = params;
		params = NULL;
	}

	rc = 0;

cleanup:
	uclogic_params_frame_free(frame);
	uclogic_params_pen_free(pen);
	uclogic_params_free(params);
	return rc;
}

/**
 * uclogic_params_dump() - dump tablet interface parameters with hid_dbg.
 *
 * @params: 	The interface parameters to dump. Cannot be NULL.
 * @hdev:	The HID device of the tablet interface to refer to while
 * 		dumping. Cannot be NULL.
 * @prefix:   	String to output before the dump. Cannot be NULL.
 */
void uclogic_params_dump(const struct uclogic_params *params,
				const struct hid_device *hdev,
				const char *prefix)
{
#define BOOL_STR(_x) ((_x) ? "true" : "false")
#define INRANGE_STR(_x) \
	((_x) == UCLOGIC_PARAMS_PEN_INRANGE_NORMAL \
		? "normal" \
		: ((_x) == UCLOGIC_PARAMS_PEN_INRANGE_INVERTED \
			? "inverted" \
			: ((_x) == UCLOGIC_PARAMS_PEN_INRANGE_NONE \
				? "none" \
				: "unknown")))

	hid_dbg(hdev,
		"%s"
		".desc_ptr = %p\n"
		".desc_size = %u\n"
		".pen_unused = %s\n"
		".pen_id = %u\n"
		".pen_inrange = %s\n"
		".pen_fragmented_hires = %s\n"
		".pen_frame_flag = 0x%02x\n"
		".pen_frame_id = %u\n",
		prefix,
		params->desc_ptr,
		params->desc_size,
		BOOL_STR(params->pen_unused),
		params->pen_id,
		INRANGE_STR(params->pen_inrange),
		BOOL_STR(params->pen_fragmented_hires),
		params->pen_frame_flag,
		params->pen_frame_id);

#undef INRANGE_STR
#undef BOOL_STR
}

/**
 * uclogic_params_probe() - initialize a tablet interface and discover its
 * parameters.
 *
 * @pparams: 	Location for the pointer to resulting parameters (to be
 * 		freed with uclogic_params_free()), or for NULL if the
 * 		parameters were not found.  Not modified in case of error.
 * 		Can be NULL to have parameters discarded after retrieval.
 * @hdev:	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
int uclogic_params_probe(struct uclogic_params **pparams,
			 struct hid_device *hdev)
{
	int rc = 0;
	/* The resulting parameters */
	struct uclogic_params *params = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		return -EINVAL;
	}

	/* Try to probe static parameters */
	rc = uclogic_params_probe_static(&params, hdev);
	if (rc < 0) {
		hid_err(hdev, "failed probing static parameters: %d\n", rc);
	} else if (params == NULL) {
		hid_dbg(hdev, "static parameters not found\n");
		/* Try to probe dynamic parameters */
		rc = uclogic_params_probe_dynamic(&params, hdev);
		if (rc < 0) {
			hid_err(hdev,
				"failed probing dynamic parameters: %d\n",
				rc);
		} else if (params == NULL) {
			hid_dbg(hdev, "dynamic parameters not found\n");
		} else {
			hid_dbg(hdev, "dynamic parameters found\n");
		}
	} else {
		hid_dbg(hdev, "static parameters found\n");
	}

	/* Output the parameters if succeeded, and asked to */
	if (rc == 0 && pparams != NULL) {
		*pparams = params;
		params = NULL;
	}

	uclogic_params_free(params);
	return rc;
}
