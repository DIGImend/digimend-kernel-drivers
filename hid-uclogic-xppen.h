#ifndef _HID_UCLOGIC_XPPEN_H
#define _HID_UCLOGIC_XPPEN_H

#include <linux/hid.h>

extern void uclogic_xppen_apply_tilt_compensation(
    struct hid_device *hdev,
    u8 *data
);

#endif
