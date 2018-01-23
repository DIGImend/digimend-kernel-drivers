/*
 *  HID driver for UC-Logic devices not fully compliant with HID standard
 *  - pen proximity-out event emulation
 *
 *  Copyright (c) 2018 Andrey Zabolotnyi
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef _HID_UCLOGIC_PROXEMU_H
#define _HID_UCLOGIC_PROXEMU_H

/* Generate a synthetic proximity-out after 250ms of tablet inactivity.
 * This can be made even smaller, as the pen in tablet proximity continuously
 * generates events, even if pen is not moved.
 */
#define EMULATE_PROXOUT_TIME	250

struct proxemu_data {
	/* if set, proximity-out events will be emulated by generating an
	 * synthetic event after some period of inactivity.
	 */
	volatile bool enabled;
	/* the emulated pen proximity state */
	bool state;
	/* the next system jiffie when proximity timeout will happen */
	unsigned long timeout;
	/* the background task that generates delayed proximity-out */
	struct delayed_work work;
	/* the HID device we're bound to */
	struct hid_device *hdev;
	/* place for the last motion report, so that we can build
	 * a proximity-out event from it */
	u8 last_motion_rep [16];
	unsigned last_motion_rep_size;
};

extern void uclogic_proxemu_init(struct proxemu_data *proxemu, struct hid_device *hdev);
extern void uclogic_proxemu_stop(struct proxemu_data *proxemu);
extern void uclogic_proxemu_raw_event(struct proxemu_data *proxemu, u8 *data, int size);

#endif /* _HID_UCLOGIC_PROXEMU_H */
