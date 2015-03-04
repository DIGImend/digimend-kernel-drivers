/*
 * Backported definitions to support building with older kernels
 */
#ifndef __COMPAT_H
#define __COMPAT_H

#ifndef HID_CP_CONSUMER_CONTROL
#define HID_CP_CONSUMER_CONTROL 0x000c0001
#endif

#ifndef HID_GD_SYSTEM_CONTROL
#define HID_GD_SYSTEM_CONTROL   0x00010080
#endif

#endif
