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

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include "hid-uclogic-proxemu.h"

static void uclogic_proxemu_worker(struct work_struct *work)
{
	struct proxemu_data *proxemu = container_of(work,
		struct proxemu_data, work.work);
	struct hid_device *hdev = proxemu->hdev;
	int sleep = 0;

	/* if hdev is null, we're finished */
	if (!hdev) {
		/* acknowledge that we quit */
		proxemu->enabled = false;
		return;
	}

	if (proxemu->timeout) {
		sleep = proxemu->timeout - jiffies;

		/* If we finally overtook the timeout,
		 * generate a prox-out event */
		if (sleep <= 0) {
			if (proxemu->last_motion_rep_size >= 2) {
				u8 *data = proxemu->last_motion_rep;

				/* If last pressure was not reported as zero,
				 * report zero pressure now, otherwise many programs
				 * that do not handle proximity reports (e.g. terminals)
				 * will not release the active mouse grab.
				 * Same applies to tip switch bit, reset it so that
				 * a button release event is generated.
				 */
				if ((data [1] & 1) || data [6] || data [7]) {
					/* tip switch is released */
					data [1] &= ~1;
					/* force pen pressure to zero */
					data [6] = data [7] = 0;
					hid_input_report(hdev, HID_INPUT_REPORT,
						data, proxemu->last_motion_rep_size, 0);
				}

				proxemu->state = false;

				/* uclogic_raw_event will clear the proximity
				 * bit in report due to proximity == false */
				hid_input_report(hdev, HID_INPUT_REPORT,
					data, proxemu->last_motion_rep_size, 0);
			}

			/* don't send more prox-outs, first motion event will be prox-in */
			proxemu->timeout = 0;
		}
	}

	if (sleep <= 0)
		/* Next prox-out event will be generated not earlier
		   than EMULATE_PROXOUT_TIME milliseconds */
		sleep = msecs_to_jiffies(EMULATE_PROXOUT_TIME);

	/* Re-schedule ourselves */
	schedule_delayed_work(&proxemu->work, sleep);
}

void uclogic_proxemu_raw_event(struct proxemu_data *proxemu, u8 *data, int size)
{
	if (!proxemu->enabled)
		return;

	/* Ignore all reports except UCLOGIC_HIRES_PEN_REPORT_ID */
	if (data [0] != 8)
		return;

	/* Remember the last valid pen report */
	if (size <= sizeof(proxemu->last_motion_rep)) {
		proxemu->last_motion_rep_size = size;
		memcpy(proxemu->last_motion_rep, data, size);
	}

	/* If this is the first report after inactivity,
	 * it is a proximity-in event */
	if (!proxemu->state && (proxemu->timeout == 0))
		proxemu->state = true;

	if (proxemu->state)
		data [1] |= 0x80;
	else
		data [1] &= ~0x80;

	/* We can't even rely on pressure being 0 to start waiting for inactivity,
	 * because if you pull quickly the pen from the tablet, a event with zero
	 * pressure won't even be generated. So we count inactivity starting from
	 * last received any motion event.
	 */
	proxemu->timeout = jiffies + msecs_to_jiffies(EMULATE_PROXOUT_TIME);
}

void uclogic_proxemu_init(struct proxemu_data *proxemu, struct hid_device *hdev)
{
	/* proxemu_data is supposed to be zeroed by e.g. kzalloc() */

	proxemu->enabled = true;
	proxemu->hdev = hdev;

	INIT_DELAYED_WORK(&proxemu->work, uclogic_proxemu_worker);
	schedule_delayed_work(&proxemu->work,
		msecs_to_jiffies(EMULATE_PROXOUT_TIME));
}

void uclogic_proxemu_stop(struct proxemu_data *proxemu)
{
	if (!proxemu->enabled)
		return;

	/* tell the worker to finish */
	proxemu->hdev = NULL;
	/* we could do some math here, but meh... */
	while (proxemu->enabled)
		msleep(EMULATE_PROXOUT_TIME / 2);
}
