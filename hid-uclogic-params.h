/* SPDX-License-Identifier: GPL-2.0+ */
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

#ifndef _HID_UCLOGIC_PARAMS_H
#define _HID_UCLOGIC_PARAMS_H

#include <linux/usb.h>
#include <linux/hid.h>

/* Types of pen in-range reporting */
enum uclogic_params_pen_inrange {
	/* Normal reports: zero - out of proximity, one - in proximity */
	UCLOGIC_PARAMS_PEN_INRANGE_NORMAL = 0,
	/* Inverted reports: zero - in proximity, one - out of proximity */
	UCLOGIC_PARAMS_PEN_INRANGE_INVERTED,
	/* No reports */
	UCLOGIC_PARAMS_PEN_INRANGE_NONE,
};

/* Convert a pen in-range reporting type to a string */
extern const char *uclogic_params_pen_inrange_to_str(
			enum uclogic_params_pen_inrange inrange);


/*
 * Pen report's subreport data.
 */
struct uclogic_params_pen_subreport {
	/*
	 * The value of the second byte of the pen report indicating this
	 * subreport. If zero, the subreport should be considered invalid and
	 * not matched.
	 */
	__u8 value;

	/*
	 * The ID to be assigned to the report, if the second byte of the pen
	 * report is equal to "value". Only valid if "value" is not zero.
	 */
	__u8 id;
};

/*
 * Tablet interface's pen input parameters.
 *
 * Must use declarative (descriptive) language, not imperative, to simplify
 * understanding and maintain consistency.
 *
 * Noop (preserving functionality) when filled with zeroes.
 */
struct uclogic_params_pen {
	/*
	 * True if pen usage is invalid for this interface and should be
	 * ignored, false otherwise.
	 */
	bool usage_invalid;
	/*
	 * Pointer to report descriptor part describing the pen inputs.
	 * Allocated with kmalloc. NULL if the part is not specified.
	 */
	__u8 *desc_ptr;
	/*
	 * Size of the report descriptor.
	 * Only valid, if "desc_ptr" is not NULL.
	 */
	unsigned int desc_size;
	/* Report ID, if reports should be tweaked, zero if not */
	unsigned int id;
	/* The list of subreports, only valid if "id" is not zero */
	struct uclogic_params_pen_subreport subreport_list[3];
	/* Type of in-range reporting, only valid if "id" is not zero */
	enum uclogic_params_pen_inrange inrange;
	/*
	 * True, if reports include fragmented high resolution coords, with
	 * high-order X and then Y bytes following the pressure field.
	 * Only valid if "id" is not zero.
	 */
	bool fragmented_hires;
	/*
	 * True if the pen reports tilt in bytes at offset 10 (X) and 11 (Y),
	 * and the Y tilt direction is flipped.
	 * Only valid if "id" is not zero.
	 */
	bool tilt_y_flipped;
};

/*
 * Parameters of frame control inputs of a tablet interface.
 *
 * Must use declarative (descriptive) language, not imperative, to simplify
 * understanding and maintain consistency.
 *
 * Noop (preserving functionality) when filled with zeroes.
 */
struct uclogic_params_frame {
	/*
	 * Pointer to report descriptor part describing the frame inputs.
	 * Allocated with kmalloc. NULL if the part is not specified.
	 */
	__u8 *desc_ptr;
	/*
	 * Size of the report descriptor.
	 * Only valid, if "desc_ptr" is not NULL.
	 */
	unsigned int desc_size;
	/*
	 * Report ID, if reports should be tweaked, zero if not.
	 */
	unsigned int id;
	/*
	 * The suffix to add to the input device name, if not NULL.
	 */
	const char *suffix;
	/*
	 * Number of the least-significant bit of the 2-bit state of a rotary
	 * encoder, in the report. Cannot point to a 2-bit field crossing a
	 * byte boundary. Zero if not present. Only valid if "id" is not zero.
	 */
	unsigned int re_lsb;
	/*
	 * Offset of the Wacom-style device ID byte in the report, to be set
	 * to pad device ID (0xf), for compatibility with Wacom drivers. Zero
	 * if no changes to the report should be made. The ID byte will be set
	 * to zero whenever the byte pointed by "touch_ring_byte" is zero, if
	 * the latter is valid. Only valid if "id" is not zero.
	 */
	unsigned int dev_id_byte;
	/*
	 * Offset of the touch ring state byte, in the report.
	 * Zero if not present. If dev_id_byte is also valid and non-zero,
	 * then the device ID byte will be cleared when the byte pointed to by
	 * this offset is zero. Only valid if "id" is not zero.
	 */
	unsigned int touch_ring_byte;

	/*
	 * Maximum value of the touch ring report.
	 * The minimum valid value is considered to be one,
	 * with zero being out-of-proximity (finger lift) value.
	 */
	__s8 touch_ring_max;

	/*
	 * The value to anchor the reversed reports at.
	 * I.e. one, if the reports should be flipped without offset.
	 * Zero if no reversal should be done.
	 */
	__s8 touch_ring_flip_at;
	/*
	 * Offset of the bitmap dial byte, in the report. Zero if not present.
	 * Only valid if "id" is not zero. A bitmap dial sends reports with a
	 * dedicated bit per direction: 1 means clockwise rotation, 2 means
	 * counterclockwise, as opposed to the normal 1 and -1.
	 */
	unsigned int bitmap_dial_byte;
};

/*
 * Tablet interface report parameters.
 *
 * Must use declarative (descriptive) language, not imperative, to simplify
 * understanding and maintain consistency.
 *
 * When filled with zeros represents a "noop" configuration - passes all
 * reports unchanged and lets the generic HID driver handle everything.
 *
 * The resulting device report descriptor is assembled from all the report
 * descriptor parts referenced by the structure. No order of assembly should
 * be assumed. The structure represents original device report descriptor if
 * all the parts are NULL.
 */
struct uclogic_params {
	/*
	 * True if the whole interface is invalid, false otherwise.
	 */
	bool invalid;
	/*
	 * Pointer to the common part of the replacement report descriptor,
	 * allocated with kmalloc. NULL if no common part is needed.
	 * Only valid, if "invalid" is false.
	 */
	__u8 *desc_ptr;
	/*
	 * Size of the common part of the replacement report descriptor.
	 * Only valid, if "desc_ptr" is valid and not NULL.
	 */
	unsigned int desc_size;
	/*
	 * Pen parameters and optional report descriptor part.
	 * Only valid, if "invalid" is false.
	 */
	struct uclogic_params_pen pen;
	/*
	 * The list of frame control parameters and optional report descriptor
	 * parts. Only valid, if "invalid" is false.
	 */
	struct uclogic_params_frame frame_list[3];
};

/* Initialize a tablet interface and discover its parameters */
extern int uclogic_params_init(struct uclogic_params *params,
				struct hid_device *hdev);

/* Tablet interface parameters *printf format string */
#define UCLOGIC_PARAMS_FMT_STR \
	".invalid = %s\n"                   \
	".desc_ptr = %p\n"                  \
	".desc_size = %u\n"                 \
	".pen = {\n"                        \
	"\t.usage_invalid = %s\n"           \
	"\t.desc_ptr = %p\n"                \
	"\t.desc_size = %u\n"               \
	"\t.id = %u\n"                      \
	"\t.subreport_list = {\n"           \
	"\t\t{0x%02hhx, %hhu},\n"           \
	"\t\t{0x%02hhx, %hhu},\n"           \
	"\t\t{0x%02hhx, %hhu},\n"           \
	"\t}\n"                             \
	"\t.inrange = %s\n"                 \
	"\t.fragmented_hires = %s\n"        \
	"\t.tilt_y_flipped = %s\n"          \
	"}\n"                               \
	".frame_list = {\n"                 \
	"\t{\n"                             \
	"\t\t.desc_ptr = %p\n"              \
	"\t\t.desc_size = %u\n"             \
	"\t\t.id = %u\n"                    \
	"\t\t.suffix = %s\n"                \
	"\t\t.re_lsb = %u\n"                \
	"\t\t.dev_id_byte = %u\n"           \
	"\t\t.touch_ring_byte = %u\n"       \
	"\t\t.touch_ring_max = %hhd\n"      \
	"\t\t.touch_ring_flip_at = %hhd\n"  \
	"\t\t.bitmap_dial_byte = %u\n"      \
	"\t},\n"                            \
	"\t{\n"                             \
	"\t\t.desc_ptr = %p\n"              \
	"\t\t.desc_size = %u\n"             \
	"\t\t.id = %u\n"                    \
	"\t\t.suffix = %s\n"                \
	"\t\t.re_lsb = %u\n"                \
	"\t\t.dev_id_byte = %u\n"           \
	"\t\t.touch_ring_byte = %u\n"       \
	"\t\t.touch_ring_max = %hhd\n"      \
	"\t\t.touch_ring_flip_at = %hhd\n"  \
	"\t\t.bitmap_dial_byte = %u\n"      \
	"\t},\n"                            \
	"\t{\n"                             \
	"\t\t.desc_ptr = %p\n"              \
	"\t\t.desc_size = %u\n"             \
	"\t\t.id = %u\n"                    \
	"\t\t.suffix = %s\n"                \
	"\t\t.re_lsb = %u\n"                \
	"\t\t.dev_id_byte = %u\n"           \
	"\t\t.touch_ring_byte = %u\n"       \
	"\t\t.touch_ring_max = %hhd\n"      \
	"\t\t.touch_ring_flip_at = %hhd\n"  \
	"\t\t.bitmap_dial_byte = %u\n"      \
	"\t},\n"                            \
	"}\n"

/* Tablet interface parameters *printf format arguments */
#define UCLOGIC_PARAMS_FMT_ARGS(_params) \
	((_params)->invalid ? "true" : "false"),                    \
	(_params)->desc_ptr,                                        \
	(_params)->desc_size,                                       \
	((_params)->pen.usage_invalid ? "true" : "false"),          \
	(_params)->pen.desc_ptr,                                    \
	(_params)->pen.desc_size,                                   \
	(_params)->pen.id,                                          \
	(_params)->pen.subreport_list[0].value,                     \
	(_params)->pen.subreport_list[0].id,                        \
	(_params)->pen.subreport_list[1].value,                     \
	(_params)->pen.subreport_list[1].id,                        \
	(_params)->pen.subreport_list[2].value,                     \
	(_params)->pen.subreport_list[2].id,                        \
	uclogic_params_pen_inrange_to_str((_params)->pen.inrange),  \
	((_params)->pen.fragmented_hires ? "true" : "false"),       \
	((_params)->pen.tilt_y_flipped ? "true" : "false"),         \
	(_params)->frame_list[0].desc_ptr,                          \
	(_params)->frame_list[0].desc_size,                         \
	(_params)->frame_list[0].id,                                \
	(_params)->frame_list[0].suffix,                            \
	(_params)->frame_list[0].re_lsb,                            \
	(_params)->frame_list[0].dev_id_byte,                       \
	(_params)->frame_list[0].touch_ring_byte,                   \
	(_params)->frame_list[0].touch_ring_max,                    \
	(_params)->frame_list[0].touch_ring_flip_at,                \
	(_params)->frame_list[0].bitmap_dial_byte,                  \
	(_params)->frame_list[1].desc_ptr,                          \
	(_params)->frame_list[1].desc_size,                         \
	(_params)->frame_list[1].id,                                \
	(_params)->frame_list[1].suffix,                            \
	(_params)->frame_list[1].re_lsb,                            \
	(_params)->frame_list[1].dev_id_byte,                       \
	(_params)->frame_list[1].touch_ring_byte,                   \
	(_params)->frame_list[1].touch_ring_max,                    \
	(_params)->frame_list[1].touch_ring_flip_at,                \
	(_params)->frame_list[1].bitmap_dial_byte,                  \
	(_params)->frame_list[2].desc_ptr,                          \
	(_params)->frame_list[2].desc_size,                         \
	(_params)->frame_list[2].id,                                \
	(_params)->frame_list[2].suffix,                            \
	(_params)->frame_list[2].re_lsb,                            \
	(_params)->frame_list[2].dev_id_byte,                       \
	(_params)->frame_list[2].touch_ring_byte,                   \
	(_params)->frame_list[2].touch_ring_max,                    \
	(_params)->frame_list[2].touch_ring_flip_at,                \
	(_params)->frame_list[2].bitmap_dial_byte

/* Get a replacement report descriptor for a tablet's interface. */
extern int uclogic_params_get_desc(const struct uclogic_params *params,
					__u8 **pdesc,
					unsigned int *psize);

/* Free resources used by tablet interface's parameters */
extern void uclogic_params_cleanup(struct uclogic_params *params);

#endif /* _HID_UCLOGIC_PARAMS_H */
