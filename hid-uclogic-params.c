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
 * Convert a pen in-range reporting type to a string.
 *
 * @inrange:	The in-range reporting type to convert.
 *
 * Return:
 * 	The string representing the type, or NULL if the type is unknown.
 */
const char *uclogic_params_pen_inrange_to_str(
			enum uclogic_params_pen_inrange inrange)
{
	switch (inrange) {
	case UCLOGIC_PARAMS_PEN_INRANGE_NORMAL:
		return "normal";
	case UCLOGIC_PARAMS_PEN_INRANGE_INVERTED:
		return "inverted";
	case UCLOGIC_PARAMS_PEN_INRANGE_NONE:
		return "none";
	default:
		return NULL;
	}
}

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

/*
 * Tablet interface's pen input parameters.
 * Noop (preserving functionality) when filled with zeroes.
 */
struct uclogic_params_pen {
	/* Pointer to report descriptor allocated with kmalloc */
	__u8 *desc_ptr;
	/* Size of the report descriptor */
	unsigned int desc_size;
	/* Report ID, if reports should be tweaked, zero if not */
	unsigned id;
	/* Type of in-range reporting, only valid if id is not zero */
	enum uclogic_params_pen_inrange inrange;
	/*
	 * True, if reports include fragmented high resolution coords, with
	 * high-order X and then Y bytes following the pressure field.
	 * Only valid if id is not zero.
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
	if (rc == -EPIPE) {
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
	if (rc == -EPIPE) {
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

/*
 * Parameters of frame control inputs of a tablet interface.
 * Noop (preserving functionality) when filled with zeroes.
 */
struct uclogic_params_frame {
	/* Pointer to report descriptor allocated with kmalloc */
	__u8 *desc_ptr;
	/* Size of the report descriptor */
	unsigned int desc_size;
	/*
	 * Report ID, if reports should be tweaked, zero if not.
	 */
	unsigned id;
	/*
	 * Number of the least-significant bit of the 2-bit state of a rotary
	 * encoder, in the report. Zero if not present. Only valid if id is
	 * not zero.
	 */
	unsigned re_lsb;
	/*
	 * Offset of the Wacom-style device ID byte in the report, to be set
	 * to pad device ID (0xf), for compatibility with Wacom drivers. Zero
	 * if no changes to the report should be made. Only valid if id is not
	 * zero.
	 */
	unsigned dev_id_byte;
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
 * uclogic_params_frame_from_desc() - create tablet frame controls
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
static int uclogic_params_frame_from_desc(
					struct uclogic_params_frame **pframe,
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
		rc = uclogic_params_frame_from_desc(
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
 * Create tablet interface parameters from pen report parameters, frame report
 * parameters, and parameters for extracting frame reports from pen reports.
 *
 * @pparams: 		Location for the pointer to resulting parameters (to
 * 			be freed with uclogic_params_free()). Not modified in
 * 			case of error. Can be NULL to have parameters
 * 			discarded after creation.
 * @pen:		Pen parameters to use for creation.
 * 			Can be NULL, if none.
 * @frame:		Frame parameters to use for creation.
 * 			Can be NULL, if none.
 * @pen_frame_flag:	Bitmask matching frame controls "sub-report" flag in
 * 			the second byte of the pen report, or zero if it's not
 * 			expected. Only valid if both "pen" and "frame" are not
 * 			NULL.
 * @pen_frame_id: 	Report ID to assign to frame reports extracted from
 * 			pen reports. Only valid if "pen_frame_flag" is valid
 * 			and not zero.
 *
 * Return:
 * 	Zero, if successful.
 * 	-ENOMEM, if failed to allocate memory.
 */
static int uclogic_params_from_pen_and_frame(
					struct uclogic_params **pparams,
					struct uclogic_params_pen *pen,
					struct uclogic_params_frame *frame,
					__u8 pen_frame_flag,
					unsigned pen_frame_id)
{
	int rc;
	/* The resulting interface parameters */
	struct uclogic_params *params = NULL;

	/* Allocate parameters */
	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (params == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Merge parameters */
	if (pen != NULL) {
		params->desc_size += pen->desc_size;
		params->pen_id = pen->id;
		params->pen_inrange = pen->inrange;
		params->pen_fragmented_hires = pen->fragmented_hires;
	}
	if (frame != NULL) {
		params->desc_size += frame->desc_size;
		params->frame_id = frame->id;
		params->frame_re_lsb = frame->re_lsb;
		params->frame_dev_id_byte = frame->dev_id_byte;
	}
	if (pen != NULL && frame != NULL) {
		params->pen_frame_flag = pen_frame_flag;
		params->pen_frame_id = pen_frame_id;
	}

	/* Merge report descriptors, if any */
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
 * uclogic_params_with_opt_desc() - create tablet interface parameters with an
 * optional replacement report descriptor. Only modify report descriptor, if
 * the original report descriptor matches the expected size.
 *
 * @pparams: 		Location for the pointer to resulting parameters (to
 * 			be freed with uclogic_params_free()). Not modified in
 * 			case of error. Can be NULL to have parameters
 * 			discarded after creation.
 * @hdev:		The HID device of the tablet interface create the
 * 			parameters for. Cannot be NULL.
 * @orig_desc_size:	Expected size of the original report descriptor to
 * 			be replaced.
 * @desc_ptr:		Pointer to the replacement report descriptor.
 * @desc_size:		Size of the replacement report descriptor.
 *
 * Return:
 * 	Zero, if successful. -EINVAL if an invalid argument was passed.
 * 	-ENOMEM, if failed to allocate memory.
 */
static int uclogic_params_with_opt_desc(struct uclogic_params **pparams,
					struct hid_device *hdev,
					unsigned int orig_desc_size,
					__u8 *desc_ptr,
					unsigned int desc_size)
{
	int rc;
	/* The resulting parameters */
	struct uclogic_params *params = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		return -EINVAL;
	}

	/* Create parameters */
	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (params == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Replace report descriptor, if it matches */
	if (hdev->dev_rsize == orig_desc_size) {
		hid_dbg(hdev, "device report descriptor matches "
				"the expected size, replacing\n");
		params->desc_ptr = kmemdup(desc_ptr, desc_size, GFP_KERNEL);
		if (params->desc_ptr == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}
		params->desc_size = desc_size;
	} else {
		hid_dbg(hdev,
			"device report descriptor doesn't match "
			"the expected size (%u != %u), preserving\n",
			hdev->dev_rsize, orig_desc_size);

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
 * uclogic_params_with_pen_unused() - create tablet interface parameters
 * preserving original reports and generic HID processing, but disabling pen
 * usage.
 *
 * @pparams: 		Location for the pointer to resulting parameters (to
 * 			be freed with uclogic_params_free()). Not modified in
 * 			case of error. Can be NULL to have parameters
 * 			discarded after creation.
 *
 * Return:
 * 	Zero, if successful.
 * 	-ENOMEM, if failed to allocate memory.
 */
static int uclogic_params_with_pen_unused(struct uclogic_params **pparams)
{
	int rc;
	/* The resulting parameters */
	struct uclogic_params *params = NULL;

	/* Create parameters */
	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (params == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	params->pen_unused = true;

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
	int rc;
	struct usb_device *udev = hid_to_usb_dev(hdev);
	__u8  bNumInterfaces = udev->config->desc.bNumInterfaces;
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 bInterfaceNumber = iface->cur_altsetting->desc.bInterfaceNumber;
	struct uclogic_params_pen *pen = NULL;
	struct uclogic_params_frame *frame = NULL;
	/* The resulting parameters */
	struct uclogic_params *params = NULL;

	/* Check arguments */
	if (hdev == NULL) {
		return -EINVAL;
	}

#define WITH_OPT_DESC(_orig_desc_token, _new_desc_token) \
	uclogic_params_with_opt_desc(                       \
		&params, hdev,                              \
		UCLOGIC_RDESC_##_orig_desc_token##_SIZE,    \
		uclogic_rdesc_##_new_desc_token##_arr,      \
		uclogic_rdesc_##_new_desc_token##_size);

#define VID_PID(_vid, _pid) \
	(((__u32)(_vid) << 16) | ((__u32)(_pid) & U16_MAX))

	switch (VID_PID(hdev->vendor, hdev->product)) {
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_PF1209):
		rc = WITH_OPT_DESC(PF1209_ORIG, pf1209_fixed);
		if (rc != 0) {
			goto cleanup;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U):
		rc = WITH_OPT_DESC(WPXXXXU_ORIG, wp4030u_fixed);
		if (rc != 0) {
			goto cleanup;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U):
		if (hdev->dev_rsize == UCLOGIC_RDESC_WP5540U_V2_ORIG_SIZE &&
		    bInterfaceNumber == 0) {
			rc = uclogic_params_pen_v1_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			rc = uclogic_params_from_pen_and_frame(
					&params, pen, NULL, 0, 0);
			if (rc != 0) {
				goto cleanup;
			}
		} else {
			rc = WITH_OPT_DESC(WPXXXXU_ORIG, wp5540u_fixed);
			if (rc != 0) {
				goto cleanup;
			}
		}

		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U):
		rc = WITH_OPT_DESC(WPXXXXU_ORIG, wp8060u_fixed);
		if (rc != 0) {
			goto cleanup;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP1062):
		rc = WITH_OPT_DESC(WP1062_ORIG, wp1062_fixed);
		if (rc != 0) {
			goto cleanup;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850):
		switch (bInterfaceNumber) {
		case 0:
			rc = WITH_OPT_DESC(TWHL850_ORIG0, twhl850_fixed0);
			if (rc != 0) {
				goto cleanup;
			}
			break;
		case 1:
			rc = WITH_OPT_DESC(TWHL850_ORIG1, twhl850_fixed1);
			if (rc != 0) {
				goto cleanup;
			}
			break;
		case 2:
			rc = WITH_OPT_DESC(TWHL850_ORIG2, twhl850_fixed2);
			if (rc != 0) {
				goto cleanup;
			}
			break;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60):
		/*
		 * If it is not a three-interface version, which is known to
		 * respond to initialization.
		 */
		if (bNumInterfaces != 3) {
			switch (bInterfaceNumber) {
			case 0:
				rc = WITH_OPT_DESC(TWHA60_ORIG0,
							twha60_fixed0);
				if (rc != 0) {
					goto cleanup;
				}
				break;
			case 1:
				rc = WITH_OPT_DESC(TWHA60_ORIG1,
							twha60_fixed1);
				if (rc != 0) {
					goto cleanup;
				}
				break;
			}
			break;
		}
		/* FALL THROUGH */
	case VID_PID(USB_VENDOR_ID_HUION,
		     USB_DEVICE_ID_HUION_TABLET):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_HUION_TABLET):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_YIYNOVA_TABLET):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47):
		/* If it's not a pen interface */
		if (bInterfaceNumber != 0) {
			rc = uclogic_params_with_pen_unused(&params);
			if (rc != 0) {
				goto cleanup;
			}
			break;
		}

		/* Try to probe v2 pen parameters */
		rc = uclogic_params_pen_v2_probe(&pen, hdev);
		if (rc != 0) {
			hid_err(hdev,
				"failed probing pen v2 parameters: %d\n", rc);
		} else if (pen != NULL) {
			hid_dbg(hdev, "pen v2 parameters found\n");
			/* Create v2 buttonpad parameters */
			rc = uclogic_params_frame_from_desc(
					&frame,
					uclogic_rdesc_buttonpad_v2_arr,
					uclogic_rdesc_buttonpad_v2_size);
			if (rc != 0) {
				hid_err(hdev, "failed creating v2 buttonpad "
					"parameters: %d\n", rc);
				goto cleanup;
			}
			/* Combine pen and frame parameters */
			rc = uclogic_params_from_pen_and_frame(
					&params, pen, frame, 0x20,
					UCLOGIC_RDESC_BUTTONPAD_V2_ID);
			if (rc != 0) {
				goto cleanup;
			}
			break;
		}
		hid_dbg(hdev, "pen v2 parameters not found\n");

		/* Try to probe v1 pen parameters */
		rc = uclogic_params_pen_v1_probe(&pen, hdev);
		if (rc != 0) {
			hid_err(hdev,
				"failed probing pen v1 parameters: %d\n", rc);
		} else if (pen != NULL) {
			hid_dbg(hdev, "pen v1 parameters found\n");
			/* Try to probe v1 buttonpad */
			rc = uclogic_params_frame_buttonpad_v1_probe(
							&frame, hdev);
			if (rc != 0) {
				hid_err(hdev, "v1 buttonpad probing "
					"failed: %d\n", rc);
				goto cleanup;
			}
			hid_dbg(hdev, "buttonpad v1 parameters%s found\n",
				(frame == NULL ? " not" : ""));
			/* Combine pen and frame parameters */
			rc = uclogic_params_from_pen_and_frame(
					&params, pen, frame, 0x20,
					UCLOGIC_RDESC_BUTTONPAD_V1_ID);
			if (rc != 0) {
				goto cleanup;
			}
			break;
		}
		hid_dbg(hdev, "pen v1 parameters not found\n");

		break;
	case VID_PID(USB_VENDOR_ID_UGTIZER,
		     USB_DEVICE_ID_UGTIZER_TABLET_GP0610):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540):
		/* If this is the pen interface */
		if (bInterfaceNumber == 1) {
			rc = uclogic_params_pen_v1_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			rc = uclogic_params_from_pen_and_frame(
					&params, pen, NULL, 0, 0);
			if (rc != 0) {
				goto cleanup;
			}
		} else {
			rc = uclogic_params_with_pen_unused(&params);
			if (rc != 0) {
				goto cleanup;
			}
		}
		break;
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO01):
		/* If this is the pen and frame interface */
		if (bInterfaceNumber == 1) {
			rc = uclogic_params_pen_v1_probe(&pen, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			rc = uclogic_params_frame_from_desc(
				&frame,
				uclogic_rdesc_xppen_deco01_frame_arr,
				uclogic_rdesc_xppen_deco01_frame_size);
			if (rc != 0) {
				goto cleanup;
			}
			rc = uclogic_params_from_pen_and_frame(
					&params, pen, frame, 0, 0);
			if (rc != 0) {
				goto cleanup;
			}
		} else {
			rc = uclogic_params_with_pen_unused(&params);
			if (rc != 0) {
				goto cleanup;
			}
		}
		break;
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_TABLET_G5):
		/* Ignore non-pen interfaces */
		if (bInterfaceNumber != 1) {
			break;
		}

		rc = uclogic_params_pen_v1_probe(&pen, hdev);
		if (rc != 0) {
			hid_err(hdev, "pen probing failed: %d\n", rc);
			goto cleanup;
		} else if (pen != NULL) {
			rc = uclogic_params_frame_from_desc(
				&frame,
				uclogic_rdesc_ugee_g5_frame_arr,
				uclogic_rdesc_ugee_g5_frame_size);
			if (rc != 0) {
				hid_err(hdev, "failed creating buttonpad "
					"parameters: %d\n", rc);
				goto cleanup;
			}
			frame->id = UCLOGIC_RDESC_UGEE_G5_FRAME_ID;
			frame->re_lsb = UCLOGIC_RDESC_UGEE_G5_FRAME_RE_LSB;
			frame->dev_id_byte =
				UCLOGIC_RDESC_UGEE_G5_FRAME_DEV_ID_BYTE;
		}

		rc = uclogic_params_from_pen_and_frame(
				&params, pen, frame, 0, 0);
		if (rc != 0) {
			goto cleanup;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_TABLET_EX07S):
		/* Ignore non-pen interfaces */
		if (bInterfaceNumber != 1) {
			break;
		}

		rc = uclogic_params_pen_v1_probe(&pen, hdev);
		if (rc != 0) {
			hid_err(hdev, "pen probing failed: %d\n", rc);
			goto cleanup;
		} else if (pen != NULL) {
			rc = uclogic_params_frame_from_desc(
				&frame,
				uclogic_rdesc_ugee_ex07_buttonpad_arr,
				uclogic_rdesc_ugee_ex07_buttonpad_size);
			if (rc != 0) {
				hid_err(hdev, "failed creating buttonpad "
					"parameters: %d\n", rc);
				goto cleanup;
			}
		}

		rc = uclogic_params_from_pen_and_frame(
				&params, pen, frame, 0, 0);
		if (rc != 0) {
			goto cleanup;
		}
		break;
	}

#undef VID_PID
#undef WITH_OPT_DESC

	/* Output parameters, if requested */
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
