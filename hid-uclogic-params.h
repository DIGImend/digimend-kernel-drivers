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
enum uclogic_params_pen_report_inrange {
	/* Normal reports: zero - out of proximity, one - in proximity */
	UCLOGIC_PARAMS_PEN_REPORT_INRANGE_NORMAL,
	/* Inverted reports: zero - in proximity, one - out of proximity */
	UCLOGIC_PARAMS_PEN_REPORT_INRANGE_INVERTED,
	/* No reports */
	UCLOGIC_PARAMS_PEN_REPORT_INRANGE_NONE,
};

/* Tablet interface parameters */
/* TODO Consider stripping "report" from names */
struct uclogic_params {
	/*
	 * Pointer to replacement report descriptor allocated with kmalloc,
	 * or NULL if no replacement is needed.
	 */
	__u8 *rdesc_ptr;
	/*
	 * Size of the replacement report descriptor.
	 * Only valid, if rdesc_ptr is not NULL.
	 */
	unsigned int rdesc_size;
	/*
	 * True, if pen usage in report descriptor is present, but unused.
	 */
	bool pen_unused;
	/*
	 * Pen report ID, if pen reports should be tweaked, zero if not
	 * Only valid if pen_unused is false.
	 */
	unsigned pen_report_id;
	/*
	 * Type of pen in-range reporting.
	 * Only valid if pen_report_id is not zero.
	 */
	enum uclogic_params_pen_report_inrange pen_report_inrange;
	/*
	 * Bitmask matching frame controls "sub-report" flag in the second
	 * byte of the pen report, or zero if it's not expected.
	 * Only valid if pen_report_id is not zero.
	 */
	__u8 pen_report_frame_flag;
	/*
	 * True, if pen reports include fragmented high resolution coords,
	 * with high-order X and then Y bytes following the pressure field
	 * Only valid if pen_report_id is not zero.
	 */
	bool pen_report_fragmented_hires;
	/*
	 * Frame controls report ID. Used as a virtual frame report ID, for
	 * frame button reports extracted from pen reports, if
	 * pen_report_frame_flag is valid and not zero.
	 */
	unsigned frame_report_id;
};

/* Initialize a tablet interface and discover its parameters */
extern int uclogic_params_probe(struct uclogic_params **pparams,
				struct hid_device *hdev);

/* Dump tablet interface parameters with hid_dbg */
extern void uclogic_params_dump(const struct uclogic_params *params,
				const struct hid_device *hdev,
				const char *prefix);

/* Free resources used by tablet interface's parameters */
extern void uclogic_params_free(struct uclogic_params *params);

#endif /* _HID_UCLOGIC_PARAMS_H */
