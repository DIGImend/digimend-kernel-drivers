// SPDX-License-Identifier: GPL-2.0+
/*
 *  HID driver for UC-Logic devices not fully compliant with HID standard
 *
 *  Copyright (c) 2010-2014 Nikolai Kondrashov
 *  Copyright (c) 2013 Martin Rusko
 *  Copyright (c) 2019-2021 Korvox
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "hid-uclogic-xppen.h"
#include "hid-ids.h"

#include <asm/unaligned.h>

void uclogic_xppen_apply_tilt_compensation(
    struct hid_device *hdev,
    u8 *data
) {
    /* All tangent lengths for pen angles 1-64
    * degrees with a sensor height of 1.8mm
    */
    u16 tangents[] = {
        3, 6, 9, 12, 15, 18, 21, 25, 28, 30, 33, 36,
        39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70,
        73, 76, 79, 82, 85, 88, 92, 95, 98, 102,
        105, 109, 112, 116, 120, 124, 127, 131,
        135, 140, 144, 148, 153, 158, 162, 167,
        173, 178, 184, 189, 195, 202, 208, 215,
        223, 231, 239, 247, 257, 266, 277
    };
    s32 discriminant;
    s8 tx = data[8];
    s8 ty = data[9];
    s8 abs_tilt;
    s32 skew;

    if (hdev->vendor != USB_VENDOR_ID_UGEE) {
        return;
    }

    switch (hdev->product) {
    case USB_DEVICE_ID_UGEE_XPPEN_PENDISPLAY_ARTIST_156_PRO:
        // sqrt(8) / 4 = 0.7071067811865476
        discriminant = 707106781;

        if (tx != 0 && ty != 0) {
            abs_tilt = abs(tx);
            skew = get_unaligned_le16(&data[2]) -
                (tx / abs_tilt) * tangents[abs_tilt] *
                    discriminant / 10000000;
            skew = clamp(skew, 0, 34419);
            put_unaligned_le16(skew, &data[2]);

            abs_tilt = abs(ty);
            skew = get_unaligned_le16(&data[4]) -
                (ty / abs_tilt) * tangents[abs_tilt] *
                    discriminant / 10000000;
            skew = clamp(skew, 0, 19461);
            put_unaligned_le16(skew, &data[4]);
        } else if (tx != 0) {
            abs_tilt = abs(tx);
            skew = get_unaligned_le16(&data[2]) -
                (tx / abs_tilt) * tangents[abs_tilt];
            skew = clamp(skew, 0, 34419);
            put_unaligned_le16(skew, &data[2]);
        } else if (ty != 0) {
            abs_tilt = abs(ty);
            skew = get_unaligned_le16(&data[4]) -
                (ty / abs_tilt) * tangents[abs_tilt];
            skew = clamp(skew, 0, 19461);
            put_unaligned_le16(skew, &data[4]);
        }

        break;
    
    default:
        break;
    }
}
