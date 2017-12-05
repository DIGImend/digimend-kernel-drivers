#ifndef _HID_UCLOGIC_RDESCS_H
#define _HID_UCLOGIC_RDESCS_H

/* Report descriptor template placeholder head */
#define UCLOGIC_PH_HEAD	0xFE, 0xED, 0x1D

/* Report descriptor template placeholder */
#define UCLOGIC_PH(_ID) UCLOGIC_PH_HEAD, UCLOGIC_PH_ID_##_ID
#define UCLOGIC_PEN_REPORT_ID	0x07

/* Size of the original descriptor of WPXXXXU tablets */
#define UCLOGIC_WPXXXXU_RDESC_ORIG_SIZE	212

/* Fixed WP4030U report descriptor */
extern const __u8 uclogic_wp4030u_rdesc_fixed[];

/* Size of the original descriptor of the new WP5540U tablet */
#define UCLOGIC_WP5540U_V2_RDESC_ORIG_SIZE	232

/* Fixed WP5540U report descriptor */
extern const __u8 uclogic_wp5540u_rdesc_fixed[];

/* Fixed WP8060U report descriptor */
extern const __u8 uclogic_wp8060u_rdesc_fixed[];

/* Size of the original descriptor of WP1062 tablet */
#define UCLOGIC_WP1062_RDESC_ORIG_SIZE	254

/* Fixed WP1062 report descriptor */
extern const __u8 uclogic_wp1062_rdesc_fixed[];

/* Size of the original descriptor of PF1209 tablet */
#define UCLOGIC_PF1209_RDESC_ORIG_SIZE	234

/* Fixed PF1209 report descriptor */
extern const __u8 uclogic_pf1209_rdesc_fixed[];

/* Size of the original descriptors of TWHL850 tablet */
#define UCLOGIC_TWHL850_RDESC_ORIG_SIZE0	182

/* Fixed PID 0522 tablet report descriptor, interface 0 (stylus) */
extern const __u8 uclogic_twhl850_rdesc_fixed0[];

/* Size of the original descriptors of TWHL850 tablet */
#define UCLOGIC_TWHL850_RDESC_ORIG_SIZE1	161

/* Fixed PID 0522 tablet report descriptor, interface 1 (mouse) */
extern const __u8 uclogic_twhl850_rdesc_fixed1[];

/* Size of the original descriptors of TWHL850 tablet */
#define UCLOGIC_TWHL850_RDESC_ORIG_SIZE2	92

/* Fixed PID 0522 tablet report descriptor, interface 2 (frame buttons) */
extern const __u8 uclogic_twhl850_rdesc_fixed2[];

/* Size of the original descriptors of TWHA60 tablet */
#define UCLOGIC_TWHA60_RDESC_ORIG_SIZE0 254

/* Fixed TWHA60 report descriptor, interface 0 (stylus) */
extern const __u8 uclogic_twha60_rdesc_fixed0[];

/* Size of the original descriptors of TWHA60 tablet */
#define UCLOGIC_TWHA60_RDESC_ORIG_SIZE1 139

/* Fixed TWHA60 report descriptor, interface 1 (frame buttons) */
extern const __u8 uclogic_twha60_rdesc_fixed1[];

/* Fixed report descriptor template */
extern const __u8 uclogic_tablet_rdesc_template[];

/* Fixed report descriptor template for Ugee EX07 */
extern const __u8 uclogic_ugee_ex07_rdesc_template[];

/* Fixed virtual pad report descriptor */
extern const __u8 uclogic_buttonpad_rdesc[];

#endif /* _HID_UCLOGIC_RDESCS_H */
