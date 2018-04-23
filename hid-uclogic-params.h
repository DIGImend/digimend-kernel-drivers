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
	UCLOGIC_PARAMS_PEN_INRANGE_NORMAL,
	/* Inverted reports: zero - in proximity, one - out of proximity */
	UCLOGIC_PARAMS_PEN_INRANGE_INVERTED,
	/* No reports */
	UCLOGIC_PARAMS_PEN_INRANGE_NONE,
};

/* Convert a pen in-range reporting type to a string */
extern const char *uclogic_params_pen_inrange_to_str(
			enum uclogic_params_pen_inrange inrange);

/*
 * Tablet interface report parameters.
 * Must use declarative (descriptive) language, not imperative, to simplify
 * understanding and maintain consistency.
 * When filled with zeros represents a "noop" configuration - passes all
 * reports unchanged and lets the generic HID driver handle everything.
 */
struct uclogic_params {
	/*
	 * Pointer to replacement report descriptor allocated with kmalloc,
	 * or NULL if no replacement is needed.
	 */
	__u8 *desc_ptr;
	/*
	 * Size of the replacement report descriptor.
	 * Only valid, if desc_ptr is not NULL.
	 */
	unsigned int desc_size;
	/*
	 * True, if pen usage in report descriptor is unused, if present.
	 */
	bool pen_unused;
	/*
	 * Pen report ID, if pen reports should be tweaked, zero if not.
	 * Only valid if pen_unused is false.
	 */
	unsigned pen_id;
	/*
	 * Type of pen in-range reporting.
	 * Only valid if pen_id is valid and not zero.
	 */
	enum uclogic_params_pen_inrange pen_inrange;
	/*
	 * True, if pen reports include fragmented high resolution coords,
	 * with high-order X and then Y bytes following the pressure field
	 * Only valid if pen_id is valid and not zero.
	 */
	bool pen_fragmented_hires;
	/*
	 * Bitmask matching frame controls "sub-report" flag in the second
	 * byte of the pen report, or zero if it's not expected.
	 * Only valid if pen_id is valid and not zero.
	 */
	__u8 pen_frame_flag;
	/*
	 * Frame controls report ID. Used as the virtual frame report ID, for
	 * frame button reports extracted from pen reports, if
	 * pen_frame_flag is valid and not zero.
	 */
	unsigned pen_frame_id;
};

/* Initialize a tablet interface and discover its parameters */
extern int uclogic_params_probe(struct uclogic_params **pparams,
				struct hid_device *hdev);

/* Tablet interface parameters *printf format string */
#define UCLOGIC_PARAMS_FMT_STR \
		".desc_ptr = %p\n"              \
		".desc_size = %u\n"             \
		".pen_unused = %s\n"            \
		".pen_id = %u\n"                \
		".pen_inrange = %s\n"           \
		".pen_fragmented_hires = %s\n"  \
		".pen_frame_flag = 0x%02x\n"    \
		".pen_frame_id = %u\n"

/* Tablet interface parameters *printf format arguments */
#define UCLOGIC_PARAMS_FMT_ARGS(_params) \
		(_params)->desc_ptr,                                        \
		(_params)->desc_size,                                       \
		((_params)->pen_unused ? "true" : "false"),                 \
		(_params)->pen_id,                                          \
		uclogic_params_pen_inrange_to_str((_params)->pen_inrange),  \
		((_params)->pen_fragmented_hires ? "true" : "false"),       \
		(_params)->pen_frame_flag,                                  \
		(_params)->pen_frame_id

/* Free resources used by tablet interface's parameters */
extern void uclogic_params_free(struct uclogic_params *params);

#endif /* _HID_UCLOGIC_PARAMS_H */
