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

/* Tablet interface parameters */
struct uclogic_params {
	/*
	 * Pointer to replacement report descriptor allocated with kmalloc,
	 * or NULL if no replacement is needed.
	 */
	__u8 *rdesc_ptr;
	/*
	 * Size of the replacement report descriptor,
	 * or zero if no replacement is needed.
	 */
	unsigned int rdesc_size;
	/*
	 * True, if pen usage in report descriptor is present, but unused.
	 * TODO Switch to just disabling unused interfaces, if possible.
	 */
	bool pen_unused;
	/*
	 * Pen report ID, if pen reports should be tweaked, zero if not
	 * Only valid if pen_unused is false.
	 */
	unsigned pen_report_id;
	/*
	 * True, if pen in-range bit is inverted in reports.
	 * Only valid if pen_report_id is not zero.
	 */
	bool pen_report_inrange_inverted;
	/*
	 * True if pen doesn't report in-range (proximity) event.
	 * Only valid if pen_report_id is not zero.
	 */
	bool pen_report_inrange_none;
	/*
	 * Bitmask matching frame controls "sub-report" flag in the second
	 * byte of the pen report, or zero if it's not expected.
	 * Only valid if pen_report_id is not zero.
	 */
	bool pen_report_frame_flag;
	/*
	 * True, if pen reports include fragmented high resolution coords,
	 * with high-order X and then Y bytes following the pressure field
	 * Only valid if pen_report_id is not zero.
	 */
	bool pen_report_fragmented_hires;
	/*
	 * Virtual report ID to use for frame controls "sub-reports" extracted
	 * from the pen reports.
	 */
	unsigned frame_virtual_report_id;
};

extern int uclogic_params_probe(struct uclogic_params *pparams,
				struct hid_device *hdev);
extern void uclogic_params_cleanup(struct uclogic_params *params);

#endif /* _HID_UCLOGIC_PARAMS_H */
