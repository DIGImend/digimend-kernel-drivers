/*
 *  HID driver for UC-Logic devices not fully compliant with HID standard
 *  - original and fixed report descriptors
 *
 *  Copyright (c) 2010-2018 Nikolai Kondrashov
 *  Copyright (c) 2013 Martin Rusko
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef _HID_UCLOGIC_RDESC_H
#define _HID_UCLOGIC_RDESC_H

#include <linux/usb.h>

/* Size of the original descriptor of WPXXXXU tablets */
#define UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE		212

/* Fixed WP4030U report descriptor */
extern __u8 uclogic_rdesc_wp4030u_fixed_arr[];
extern const size_t uclogic_rdesc_wp4030u_fixed_size;

/* Fixed WP5540U report descriptor */
extern __u8 uclogic_rdesc_wp5540u_fixed_arr[];
extern const size_t uclogic_rdesc_wp5540u_fixed_size;

/* Fixed WP8060U report descriptor */
extern __u8 uclogic_rdesc_wp8060u_fixed_arr[];
extern const size_t uclogic_rdesc_wp8060u_fixed_size;

/* Size of the original descriptor of the new WP5540U tablet */
#define UCLOGIC_RDESC_WP5540U_V2_ORIG_SIZE	232

/* Size of the original descriptor of WP1062 tablet */
#define UCLOGIC_RDESC_WP1062_ORIG_SIZE		254

/* Fixed WP1062 report descriptor */
extern __u8 uclogic_rdesc_wp1062_fixed_arr[];
extern const size_t uclogic_rdesc_wp1062_fixed_size;

/* Size of the original descriptor of PF1209 tablet */
#define UCLOGIC_RDESC_PF1209_ORIG_SIZE		234

/* Fixed PF1209 report descriptor */
extern __u8 uclogic_rdesc_pf1209_fixed_arr[];
extern const size_t uclogic_rdesc_pf1209_fixed_size;

/* Size of the original descriptors of TWHL850 tablet */
#define UCLOGIC_RDESC_TWHL850_ORIG0_SIZE	182
#define UCLOGIC_RDESC_TWHL850_ORIG1_SIZE	161
#define UCLOGIC_RDESC_TWHL850_ORIG2_SIZE	92

/* Fixed PID 0522 tablet report descriptor, interface 0 (stylus) */
extern __u8 uclogic_rdesc_twhl850_fixed0_arr[];
extern const size_t uclogic_rdesc_twhl850_fixed0_size;

/* Fixed PID 0522 tablet report descriptor, interface 1 (mouse) */
extern __u8 uclogic_rdesc_twhl850_fixed1_arr[];
extern const size_t uclogic_rdesc_twhl850_fixed1_size;

/* Fixed PID 0522 tablet report descriptor, interface 2 (frame buttons) */
extern __u8 uclogic_rdesc_twhl850_fixed2_arr[];
extern const size_t uclogic_rdesc_twhl850_fixed2_size;

/* Size of the original descriptors of TWHA60 tablet */
#define UCLOGIC_RDESC_TWHA60_ORIG0_SIZE 	254
#define UCLOGIC_RDESC_TWHA60_ORIG1_SIZE 	139

/* Fixed TWHA60 report descriptor, interface 0 (stylus) */
extern __u8 uclogic_rdesc_twha60_fixed0_arr[];
extern const size_t uclogic_rdesc_twha60_fixed0_size;

/* Fixed TWHA60 report descriptor, interface 1 (frame buttons) */
extern __u8 uclogic_rdesc_twha60_fixed1_arr[];
extern const size_t uclogic_rdesc_twha60_fixed1_size;

/* Report descriptor template placeholder head */
#define UCLOGIC_RDESC_PH_HEAD	0xFE, 0xED, 0x1D

/* Report descriptor template placeholder IDs */
enum uclogic_rdesc_ph_id {
	UCLOGIC_RDESC_PH_ID_X_LM,
	UCLOGIC_RDESC_PH_ID_X_PM,
	UCLOGIC_RDESC_PH_ID_Y_LM,
	UCLOGIC_RDESC_PH_ID_Y_PM,
	UCLOGIC_RDESC_PH_ID_PRESSURE_LM,
	UCLOGIC_RDESC_PH_ID_NUM
};

/* Report descriptor template placeholder */
#define UCLOGIC_RDESC_PH(_ID) UCLOGIC_RDESC_PH_HEAD, UCLOGIC_RDESC_PH_ID_##_ID

/* Fixed report descriptor template */
extern const __u8 uclogic_rdesc_tablet_template_arr[];
extern const size_t uclogic_rdesc_tablet_template_size;

/* Fixed report descriptor template for Ugee EX07 */
extern const __u8 uclogic_rdesc_ugee_ex07_template_arr[];
extern const size_t uclogic_rdesc_ugee_ex07_template_size;

/* Fixed virtual pad report descriptor */
extern const __u8 uclogic_rdesc_buttonpad_arr[];
extern const size_t uclogic_rdesc_buttonpad_size;

/* Report descriptor for the newer high resolution tablets */
extern const __u8 uclogic_rdesc_tablet_hires_template_arr[];
extern const size_t uclogic_rdesc_tablet_hires_template_size;

/* Report descriptor for the newer high resolution tablet pad */
extern const __u8 uclogic_rdesc_buttonpad_hires_arr[];
extern const size_t uclogic_rdesc_buttonpad_hires_size;

#endif /* _HID_UCLOGIC_RDESC_H */
