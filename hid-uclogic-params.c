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
 * @pbuf	Location for the kmalloc-allocated buffer pointer containing
 * 		the retrieved descriptor. Not modified in case of error.
 * 		Can be NULL to have retrieved descriptor discarded.
 * @idx		Index of the string descriptor to request from the device.
 * @len		Length of the buffer to allocate and the data to retrieve.
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
	__u8 *rdesc_ptr;
	/* Size of the report descriptor */
	unsigned int rdesc_size;
	/* Pen report ID */
	unsigned report_id;
	/* Type of pen in-range reporting */
	enum uclogic_params_pen_report_inrange report_inrange;
	/*
	 * True, if pen reports include fragmented high resolution coords,
	 * with high-order X and then Y bytes following the pressure field
	 */
	bool report_fragmented_hires;
};

/**
 * uclogic_params_pen_free - free resources used by struct uclogic_params_pen
 * (tablet interface's pen input parameters).
 *
 * @pen	Pen input parameters to free. Can be NULL.
 */
static void uclogic_params_pen_free(struct uclogic_params_pen *pen)
{
	if (pen != NULL) {
		kfree(pen->rdesc_ptr);
		memset(pen, 0, sizeof(*pen));
		kfree(pen);
	}
}

/**
 * uclogic_params_pen_v1_probe() - initialize tablet interface pen
 * input and retrieve its parameters from the device, using v1 protocol.
 *
 * @ppen 	Location for the pointer to resulting pen parameters (to be
 * 		freed with uclogic_params_pen_free()), or for NULL if the pen
 * 		parameters were not found or recognized.  Not modified in case
 * 		of error. Can be NULL to have parameters discarded after
 * 		retrieval.
 * @hdev	The HID device of the tablet interface to initialize and get
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
	s32 rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_NUM];
	__u8 *rdesc_ptr = NULL;
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
	rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] =
		get_unaligned_le16(buf + 2);
	rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] =
		get_unaligned_le16(buf + 4);
	rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] =
		get_unaligned_le16(buf + 8);
	resolution = get_unaligned_le16(buf + 10);
	if (resolution == 0) {
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = 0;
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = 0;
	} else {
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] =
			rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] * 1000 /
			resolution;
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] =
			rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] * 1000 /
			resolution;
	}
	kfree(buf);
	buf = NULL;

	/*
	 * Generate pen report descriptor
	 */
	rdesc_ptr = uclogic_rdesc_template_apply(
				uclogic_rdesc_pen_v1_template_arr,
				uclogic_rdesc_pen_v1_template_size,
				rdesc_params, ARRAY_SIZE(rdesc_params));
	if (rdesc_ptr == NULL) {
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
	pen->rdesc_ptr = rdesc_ptr;
	rdesc_ptr = NULL;
	pen->rdesc_size = uclogic_rdesc_pen_v1_template_size;
	pen->report_id = UCLOGIC_RDESC_PEN_V1_ID;
	pen->report_inrange = UCLOGIC_PARAMS_PEN_REPORT_INRANGE_INVERTED;

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
	kfree(rdesc_ptr);
	kfree(buf);
	return rc;
}

/**
 * uclogic_params_get_le24() - get a 24-bit little-endian number from a
 * buffer.
 *
 * @p	The pointer to the number buffer.
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
 * @ppen 	Location for the pointer to resulting pen parameters (to be
 * 		freed with uclogic_params_pen_free()), or for NULL if the pen
 * 		parameters were not found or recognized.  Not modified in case
 * 		of error. Can be NULL to have parameters discarded after
 * 		retrieval.
 * @hdev	The HID device of the tablet interface to initialize and get
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
	s32 rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_NUM];
	__u8 *rdesc_ptr = NULL;
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
	rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] =
		uclogic_params_get_le24(buf + 2);
	rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] =
		uclogic_params_get_le24(buf + 5);
	rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] =
		get_unaligned_le16(buf + 8);
	resolution = get_unaligned_le16(buf + 10);
	if (resolution == 0) {
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = 0;
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = 0;
	} else {
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] =
			rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] * 1000 /
			resolution;
		rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] =
			rdesc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] * 1000 /
			resolution;
	}
	kfree(buf);
	buf = NULL;

	/*
	 * Generate pen report descriptor
	 */
	rdesc_ptr = uclogic_rdesc_template_apply(
				uclogic_rdesc_pen_v2_template_arr,
				uclogic_rdesc_pen_v2_template_size,
				rdesc_params, ARRAY_SIZE(rdesc_params));
	if (rdesc_ptr == NULL) {
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
	pen->rdesc_ptr = rdesc_ptr;
	rdesc_ptr = NULL;
	pen->rdesc_size = uclogic_rdesc_pen_v2_template_size;
	pen->report_id = UCLOGIC_RDESC_PEN_V2_ID;
	pen->report_inrange = UCLOGIC_PARAMS_PEN_REPORT_INRANGE_NONE;
	pen->report_fragmented_hires = true;

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
	kfree(rdesc_ptr);
	kfree(buf);
	return rc;
}

/**
 * uclogic_params_pen_probe() - initialize tablet interface pen
 * input and retrieve its parameters from the device.
 *
 * @ppen 	Location for the pointer to resulting pen parameters (to be
 * 		freed with uclogic_params_pen_free()), or for NULL if the pen
 * 		parameters were not found or recognized.  Not modified in case
 * 		of error. Can be NULL to have parameters discarded after
 * 		retrieval.
 * @hdev	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_pen_probe(struct uclogic_params_pen **ppen,
					struct hid_device *hdev)
{
	int rc;
	/* The resulting parameters */
	struct uclogic_params_pen *pen = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		return -EINVAL;
	}

	/* Try to probe v2 pen parameters */
	rc = uclogic_params_pen_v2_probe(&pen, hdev);
	if (rc < 0) {
		hid_err(hdev, "failed probing pen v2 parameters: %d\n", rc);
	} else if (pen == NULL) {
		hid_dbg(hdev, "pen v2 parameters not found\n");
		/* Try to probe v1 pen parameters */
		rc = uclogic_params_pen_v1_probe(&pen, hdev);
		if (rc < 0) {
			hid_err(hdev,
				"failed probing pen v1 parameters: %d\n", rc);
		} else if (pen == NULL) {
			hid_dbg(hdev, "pen v1 parameters not found\n");
		} else {
			hid_dbg(hdev, "pen v1 parameters found\n");
		}
	} else {
		hid_dbg(hdev, "pen v2 parameters found\n");
	}

	/* Output the parameters if succeeded, and asked to */
	if (rc == 0 && ppen != NULL) {
		*ppen = pen;
		pen = NULL;
	}

	uclogic_params_pen_free(pen);
	return rc;
}

/* Parameters of frame control inputs of a tablet interface */
struct uclogic_params_frame {
	/* Pointer to report descriptor allocated with kmalloc */
	__u8 *rdesc_ptr;
	/* Size of the report descriptor */
	unsigned int rdesc_size;
};

/**
 * uclogic_params_frame_free - free resources used by struct
 * uclogic_params_frame (tablet interface's frame controls input parameters).
 *
 * @frame	Frame controls input parameters to free. Can be NULL.
 */
static void uclogic_params_frame_free(struct uclogic_params_frame *frame)
{
	if (frame != NULL) {
		kfree(frame->rdesc_ptr);
		memset(frame, 0, sizeof(*frame));
		kfree(frame);
	}
}

/**
 * uclogic_params_frame_probe() - initialize tablet interface frame controls
 * input and retrieve its parameters from the device.
 *
 * @frame 	Location for the resulting frame controls parameters.
 * 		Needs to be cleaned up with uclogic_params_frame_cleanup()
 * 		after use, if this function succeeds.
 * @ppen 	Location for the pointer to resulting frame controls
 * 		parameters (to be freed with uclogic_params_frame_free()), or
 * 		for NULL if the frame controls parameters were not found or
 * 		recognized.  Not modified in case of error. Can be NULL to
 * 		have parameters discarded after retrieval.
 * @hdev	The HID device of the tablet interface to initialize and get
 * 		parameters from. Cannot be NULL.
 *
 * Return:
 * 	Zero, if successful. A negative errno code on error.
 */
static int uclogic_params_frame_probe(struct uclogic_params_frame **pframe,
					struct hid_device *hdev)
{
	int rc;
	struct usb_device *usb_dev = hid_to_usb_dev(hdev);
	char *str_buf;
	size_t str_len = 16;
	struct uclogic_params_frame *frame = NULL;

	/*
	 * Enable generic keyboard mode
	 */
	str_buf = kzalloc(str_len, GFP_KERNEL);
	if (str_buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = usb_string(usb_dev, 123, str_buf, str_len);
	if (rc == -EPIPE) {
		hid_info(hdev, "button mode setting not found\n");
	} else if (rc < 0) {
		hid_err(hdev, "failed to enable abstract keyboard\n");
		goto cleanup;
	} else if (strncmp(str_buf, "HK On", rc) != 0) {
		hid_info(hdev,
			 "invalid answer when requesting buttons: '%s'\n",
			 str_buf);
	} else {
		frame = kzalloc(sizeof(*frame), GFP_KERNEL);
		if (frame == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}
		frame->rdesc_ptr = kzalloc(uclogic_rdesc_buttonpad_size,
						GFP_KERNEL);
		if (frame->rdesc_ptr == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}
		memcpy(frame->rdesc_ptr,
			uclogic_rdesc_buttonpad_arr,
			uclogic_rdesc_buttonpad_size);
		frame->rdesc_size = uclogic_rdesc_buttonpad_size;
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
 * @params	Input parameters to free. Can be NULL.
 */
void uclogic_params_free(struct uclogic_params *params)
{
	if (params != NULL) {
		kfree(params->rdesc_ptr);
		params->rdesc_ptr = NULL;
		kfree(params);
	}
}

/**
 * uclogic_params_probe_static() - probe a tablet interface with
 * statically-assigned parameters.
 *
 * @pparams 	Location for the pointer to resulting parameters (to be
 * 		freed with uclogic_params_free()), or for NULL if the
 * 		parameters were not found.  Not modified in case of error.
 * 		Can be NULL to have parameters discarded after retrieval.
 * @hdev	The HID device of the tablet interface to initialize and
 * 		possibly get parameters from. Cannot be NULL.
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
	unsigned orig_rdesc_size = hdev->dev_rsize;
	/*
	 * The replacement report descriptor pointer,
	 * or NULL if no replacement is needed.
	 */
	const __u8 *rdesc_ptr = NULL;
	/*
	 * The replacement report descriptor size.
	 * Only valid if rdesc_ptr is not NULL.
	 */
	unsigned int rdesc_size = 0;
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
		if (orig_rdesc_size == UCLOGIC_RDESC_PF1209_ORIG_SIZE) {
			rdesc_ptr = uclogic_rdesc_pf1209_fixed_arr;
			rdesc_size = uclogic_rdesc_pf1209_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U:
		if (orig_rdesc_size == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			rdesc_ptr = uclogic_rdesc_wp4030u_fixed_arr;
			rdesc_size = uclogic_rdesc_wp4030u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U:
		if (orig_rdesc_size == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			rdesc_ptr = uclogic_rdesc_wp5540u_fixed_arr;
			rdesc_size = uclogic_rdesc_wp5540u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U:
		if (orig_rdesc_size == UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE) {
			rdesc_ptr = uclogic_rdesc_wp8060u_fixed_arr;
			rdesc_size = uclogic_rdesc_wp8060u_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_TABLET_WP1062:
		if (orig_rdesc_size == UCLOGIC_RDESC_WP1062_ORIG_SIZE) {
			rdesc_ptr = uclogic_rdesc_wp1062_fixed_arr;
			rdesc_size = uclogic_rdesc_wp1062_fixed_size;
		}
		break;
	case USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850:
		switch (bInterfaceNumber) {
		case 0:
			if (orig_rdesc_size == UCLOGIC_RDESC_TWHL850_ORIG0_SIZE) {
				rdesc_ptr = uclogic_rdesc_twhl850_fixed0_arr;
				rdesc_size = uclogic_rdesc_twhl850_fixed0_size;
			}
			break;
		case 1:
			if (orig_rdesc_size == UCLOGIC_RDESC_TWHL850_ORIG1_SIZE) {
				rdesc_ptr = uclogic_rdesc_twhl850_fixed1_arr;
				rdesc_size = uclogic_rdesc_twhl850_fixed1_size;
			}
			break;
		case 2:
			if (orig_rdesc_size == UCLOGIC_RDESC_TWHL850_ORIG2_SIZE) {
				rdesc_ptr = uclogic_rdesc_twhl850_fixed2_arr;
				rdesc_size = uclogic_rdesc_twhl850_fixed2_size;
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
			if (orig_rdesc_size == UCLOGIC_RDESC_TWHA60_ORIG0_SIZE) {
				rdesc_ptr = uclogic_rdesc_twha60_fixed0_arr;
				rdesc_size = uclogic_rdesc_twha60_fixed0_size;
			}
			break;
		case 1:
			if (orig_rdesc_size == UCLOGIC_RDESC_TWHA60_ORIG1_SIZE) {
				rdesc_ptr = uclogic_rdesc_twha60_fixed1_arr;
				rdesc_size = uclogic_rdesc_twha60_fixed1_size;
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
	if (rdesc_ptr != NULL || pen_unused) {
		/* Create parameters */
		params = kzalloc(sizeof(*params), GFP_KERNEL);
		if (params == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}
		params->pen_unused = pen_unused;
		if (rdesc_ptr != NULL) {
			params->rdesc_ptr = kmalloc(rdesc_size, GFP_KERNEL);
			if (params->rdesc_ptr == NULL) {
				rc = -ENOMEM;
				goto cleanup;
			}
			memcpy(params->rdesc_ptr, rdesc_ptr, rdesc_size);
			params->rdesc_size = rdesc_size;
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
 * @pparams 	Location for the pointer to resulting parameters (to be
 * 		freed with uclogic_params_free()), or for NULL if the
 * 		parameters were not found.  Not modified in case of error.
 * 		Can be NULL to have parameters discarded after retrieval.
 * @hdev	The HID device of the tablet interface to initialize and get
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
	/* Frame controls' input parameters */
	struct uclogic_params_frame *frame = NULL;
	/* The resulting interface parameters */
	struct uclogic_params *params = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	switch (hdev->product) {
	case USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60:
		/* If it is the pen interface */
		if (bInterfaceNumber == 0) {
			rc = uclogic_params_pen_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			rc = uclogic_params_frame_probe(&frame, hdev);
			if (rc != 0) {
				hid_err(hdev, "frame controls probing "
					"failed: %d\n", rc);
				goto cleanup;
			}
		}
		break;
	case USB_DEVICE_ID_HUION_TABLET:
	case USB_DEVICE_ID_YIYNOVA_TABLET:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81:
	case USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45:
	case USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47:
		/* If this is the pen interface */
		if (bInterfaceNumber == 0) {
			rc = uclogic_params_pen_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			rc = uclogic_params_frame_probe(&frame, hdev);
			if (rc != 0) {
				hid_err(hdev, "frame controls probing "
					"failed: %d\n", rc);
				goto cleanup;
			}
		}
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
		/* If this is the pen interface */
		if (bInterfaceNumber == 1) {
			rc = uclogic_params_pen_v1_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
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
		params->rdesc_size += pen->rdesc_size;
		params->pen_report_id = pen->report_id;
		params->pen_report_inrange = pen->report_inrange;
		params->pen_report_fragmented_hires =
			pen->report_fragmented_hires;
	}
	if (frame != NULL) {
		params->rdesc_size += frame->rdesc_size;
		params->pen_report_frame_flag = 0x20;
		params->pen_frame_report_id = 0xf7;
	}

	/*
	 * Merge report descriptors, if any
	 */
	if (params->rdesc_size > 0) {
		__u8 *p;

		params->rdesc_ptr = kmalloc(params->rdesc_size, GFP_KERNEL);
		if (params->rdesc_ptr == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}

		p = params->rdesc_ptr;
		if (pen != NULL) {
			memcpy(p, pen->rdesc_ptr, pen->rdesc_size);
			p += pen->rdesc_size;
		}
		if (frame != NULL) {
			memcpy(p, frame->rdesc_ptr, frame->rdesc_size);
			p += frame->rdesc_size;
		}
		WARN_ON(p != params->rdesc_ptr + params->rdesc_size);
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
 * @params 	The interface parameters to dump. Cannot be NULL.
 * @hdev	The HID device of the tablet interface to refer to while
 * 		dumping. Cannot be NULL.
 * @prefix   	String to output before the dump. Cannot be NULL.
 */
void uclogic_params_dump(const struct uclogic_params *params,
				const struct hid_device *hdev,
				const char *prefix)
{
#define BOOL_STR(_x) ((_x) ? "true" : "false")
#define INRANGE_STR(_x) \
	((_x) == UCLOGIC_PARAMS_PEN_REPORT_INRANGE_NORMAL \
		? "normal" \
		: ((_x) == UCLOGIC_PARAMS_PEN_REPORT_INRANGE_INVERTED \
			? "inverted" \
			: ((_x) == UCLOGIC_PARAMS_PEN_REPORT_INRANGE_NONE \
				? "none" \
				: "unknown")))

	hid_dbg(hdev,
		"%s"
		".rdesc_ptr = %p\n"
		".rdesc_size = %u\n"
		".pen_unused = %s\n"
		".pen_report_id = %u\n"
		".pen_report_inrange = %s\n"
		".pen_report_frame_flag = 0x%02x\n"
		".pen_report_fragmented_hires = %s\n"
		".pen_frame_report_id = %u\n",
		prefix,
		params->rdesc_ptr,
		params->rdesc_size,
		BOOL_STR(params->pen_unused),
		params->pen_report_id,
		INRANGE_STR(params->pen_report_inrange),
		params->pen_report_frame_flag,
		BOOL_STR(params->pen_report_fragmented_hires),
		params->pen_frame_report_id);

#undef INRANGE_STR
#undef BOOL_STR
}

/**
 * uclogic_params_probe() - initialize a tablet interface and discover its
 * parameters.
 *
 * @pparams 	Location for the pointer to resulting parameters (to be
 * 		freed with uclogic_params_free()), or for NULL if the
 * 		parameters were not found.  Not modified in case of error.
 * 		Can be NULL to have parameters discarded after retrieval.
 * @hdev	The HID device of the tablet interface to initialize and get
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
