/*
 *  HID driver for UC-Logic devices not fully compliant with HID standard
 *
 *  Copyright (c) 2010-2014 Nikolai Kondrashov
 *  Copyright (c) 2013 Martin Rusko
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <asm/unaligned.h>
#include "usbhid/usbhid.h"
#include "hid-uclogic-params.h"

#include "hid-ids.h"

#include "compat.h"
#include <linux/version.h>

/* Driver data */
struct uclogic_drvdata {
	/* Interface parameters */
	struct uclogic_params *params;
	/* Pen input device */
	struct input_dev *pen_input;
	/* In-range timer */
	struct timer_list inrange_timer;
	/* Last rotary encoder state, or U8_MAX for none */
	u8 re_state;
};

static void uclogic_inrange_timeout(struct timer_list *t)
{
	struct uclogic_drvdata *drvdata = from_timer(drvdata, t,
							inrange_timer);
	struct input_dev *input = drvdata->pen_input;
	if (input == NULL) {
		return;
	}
	input_report_abs(input, ABS_PRESSURE, 0);
	/* If BTN_TOUCH state is changing */
	if (test_bit(BTN_TOUCH, input->key)) {
		input_event(input, EV_MSC, MSC_SCAN,
				/* Digitizer Tip Switch usage */
				0xd0042);
		input_report_key(input, BTN_TOUCH, 0);
	}
	input_report_key(input, BTN_TOOL_PEN, 0);
	input_sync(input);
}

static __u8 *uclogic_report_fixup(struct hid_device *hdev, __u8 *rdesc,
					unsigned int *rsize)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	struct uclogic_params *params = drvdata->params;
	if (params->desc_ptr != NULL) {
		rdesc = params->desc_ptr;
		*rsize = params->desc_size;
	}
	return rdesc;
}

static int uclogic_input_mapping(struct hid_device *hdev,
				 struct hid_input *hi,
				 struct hid_field *field,
				 struct hid_usage *usage,
				 unsigned long **bit,
				 int *max)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	struct uclogic_params *params = drvdata->params;

	/* discard the unused pen interface */
	if (params->pen_unused && (field->application == HID_DG_PEN))
		return -1;

	/* let hid-core decide what to do */
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
#define RETURN_SUCCESS return
static void uclogic_input_configured(struct hid_device *hdev,
		struct hid_input *hi)
#else
#define RETURN_SUCCESS return 0
static int uclogic_input_configured(struct hid_device *hdev,
		struct hid_input *hi)
#endif
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	struct uclogic_params *params = drvdata->params;
	char *name;
	const char *suffix = NULL;
	struct hid_field *field;
	size_t len;

	/* no report associated (HID_QUIRK_MULTI_INPUT not set) */
	if (!hi->report)
		RETURN_SUCCESS;

	/*
	 * If this is the input corresponding to the pen report
	 * in need of tweaking.
	 */
	if (hi->report->id == params->pen_id) {
		/* Remember the input device so we can simulate events */
		drvdata->pen_input = hi->input;
	}

	field = hi->report->field[0];

	switch (field->application) {
	case HID_GD_KEYBOARD:
		suffix = "Keyboard";
		break;
	case HID_GD_MOUSE:
		suffix = "Mouse";
		break;
	case HID_GD_KEYPAD:
		suffix = "Pad";
		break;
	case HID_DG_PEN:
		suffix = "Pen";
		break;
	case HID_CP_CONSUMER_CONTROL:
		suffix = "Consumer Control";
		break;
	case HID_GD_SYSTEM_CONTROL:
		suffix = "System Control";
		break;
	}

	if (suffix) {
		len = strlen(hdev->name) + 2 + strlen(suffix);
		name = devm_kzalloc(&hi->input->dev, len, GFP_KERNEL);
		if (name) {
			snprintf(name, len, "%s %s", hdev->name, suffix);
			hi->input->name = name;
		}
	}
	RETURN_SUCCESS;
}
#undef RETURN_SUCCESS


static int uclogic_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	int rc;
	struct uclogic_drvdata *drvdata = NULL;

	/*
	 * libinput requires the pad interface to be on a different node
	 * than the pen, so use QUIRK_MULTI_INPUT for all tablets.
	 */
	hdev->quirks |= HID_QUIRK_MULTI_INPUT;
	hdev->quirks |= HID_QUIRK_NO_EMPTY_INPUT;

	/* Allocate and assign driver data */
	drvdata = devm_kzalloc(&hdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL) {
		rc = -ENOMEM;
		goto failure;
	}
	timer_setup(&drvdata->inrange_timer, uclogic_inrange_timeout, 0);
	drvdata->re_state = U8_MAX;
	hid_set_drvdata(hdev, drvdata);

	/* Initialize the device and retrieve parameters */
	rc = uclogic_params_probe(&drvdata->params, hdev);
	if (rc != 0) {
		hid_err(hdev, "failed probing parameters: %d\n", rc);
		goto failure;
	}
	if (drvdata->params == NULL) {
		hid_info(hdev, "parameters not found, "
				"ignoring the interface");
		rc = -ENODEV;
		goto failure;
	} else {
		hid_dbg(hdev, "parameters:\n" UCLOGIC_PARAMS_FMT_STR,
			UCLOGIC_PARAMS_FMT_ARGS(drvdata->params));
	}

	rc = hid_parse(hdev);
	if (rc) {
		hid_err(hdev, "parse failed\n");
		goto failure;
	}

	rc = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (rc) {
		hid_err(hdev, "hw start failed\n");
		goto failure;
	}

	return 0;
failure:
	/* Assume "remove" might not be called if "probe" failed */
	if (drvdata != NULL) {
		uclogic_params_free(drvdata->params);
		drvdata->params = NULL;
	}
	return rc;
}

#ifdef CONFIG_PM
static int uclogic_resume(struct hid_device *hdev)
{
	int rc;

	/* Re-initialize the device, but discard parameters */
	rc = uclogic_params_probe(NULL, hdev);
	if (rc != 0) {
		hid_err(hdev, "failed to re-initialize the device");
	}

	return rc;
}
#endif

static int uclogic_raw_event(struct hid_device *hdev,
				struct hid_report *report,
				u8 *data, int size)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	struct uclogic_params *params = drvdata->params;

	/* Tweak pen reports, if necessary */
	if (!params->pen_unused &&
	    (report->type == HID_INPUT_REPORT) &&
	    (report->id == params->pen_id) &&
	    (size >= 2)) {
		/* If it's the "virtual" frame controls report */
		if (data[1] & params->pen_frame_flag) {
			/* Change to virtual frame controls report ID */
			data[0] = params->pen_frame_id;
			return 0;
		}
		/* If in-range reports are inverted */
		if (params->pen_inrange ==
			UCLOGIC_PARAMS_PEN_INRANGE_INVERTED) {
			/* Invert the in-range bit */
			data[1] ^= 0x40;
		}
		/*
		 * If report contains fragmented high-resolution pen
		 * coordinates
		 */
		if (size >= 10 && params->pen_fragmented_hires) {
			u8 pressure_low_byte;
			u8 pressure_high_byte;

			/* Lift pressure bytes */
			pressure_low_byte = data[6];
			pressure_high_byte = data[7];
			/*
			 * Move Y coord to make space for high-order X
			 * coord byte
			 */
			data[6] = data[5];
			data[5] = data[4];
			/* Move high-order X coord byte */
			data[4] = data[8];
			/* Move high-order Y coord byte */
			data[7] = data[9];
			/* Place pressure bytes */
			data[8] = pressure_low_byte;
			data[9] = pressure_high_byte;
		}
		/* If we need to emulate in-range detection */
		if (params->pen_inrange == UCLOGIC_PARAMS_PEN_INRANGE_NONE) {
			/* Set in-range bit */
			data[1] |= 0x40;
			/* (Re-)start in-range timeout */
			mod_timer(&drvdata->inrange_timer,
					jiffies + msecs_to_jiffies(100));
		}
	}

	/* Tweak frame control reports, if necessary */
	if ((report->type == HID_INPUT_REPORT) &&
	    (report->id == params->frame_id)) {
		/* If need to, and can, set pad device ID for Wacom drivers */
		if (params->frame_dev_id_byte > 0 &&
		    params->frame_dev_id_byte < size) {
			data[params->frame_dev_id_byte] = 0xf;
		}
		/* If need to, and can, read rotary encoder state change */
		if (params->frame_re_lsb > 0 &&
		    params->frame_re_lsb / 8 < size) {
			unsigned byte = params->frame_re_lsb / 8;
			unsigned bit = params->frame_re_lsb % 8;
			u8 change;
			u8 prev_state = drvdata->re_state;
			/* Read Gray-coded state */
			u8 state = (data[byte] >> bit) & 0x3;
			/* Encode state change into 2-bit signed integer */
			if ((prev_state == 1 && state == 0) ||
			    (prev_state == 2 && state == 3)) {
				change = 1;
			} else if ((prev_state == 2 && state == 0) ||
				   (prev_state == 1 && state == 3)) {
				change = 3;
			} else {
				change = 0;
			}
			/* Write change */
			data[byte] = (data[byte] & ~((u8)3 << bit)) |
					(change << bit);
			/* Remember state */
			drvdata->re_state = state;
		}
	}

	return 0;
}

static void uclogic_remove(struct hid_device *hdev)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);
	del_timer_sync(&drvdata->inrange_timer);
	hid_hw_stop(hdev);
	uclogic_params_free(drvdata->params);
	drvdata->params = NULL;
}

static const struct hid_device_id uclogic_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_PF1209) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_WP1062) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_HUION,
				USB_DEVICE_ID_HUION_TABLET) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_HUION_TABLET) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_YIYNOVA_TABLET) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UCLOGIC,
				USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGTIZER,
				USB_DEVICE_ID_UGTIZER_TABLET_GP0610) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE,
				USB_DEVICE_ID_UGEE_TABLET_G5) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE,
				USB_DEVICE_ID_UGEE_TABLET_EX07S) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE,
				USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_UGEE,
				USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO01) },
	{ }
};
MODULE_DEVICE_TABLE(hid, uclogic_devices);

static struct hid_driver uclogic_driver = {
	.name = "uclogic",
	.id_table = uclogic_devices,
	.probe = uclogic_probe,
	.remove = uclogic_remove,
	.report_fixup = uclogic_report_fixup,
	.raw_event = uclogic_raw_event,
	.input_mapping = uclogic_input_mapping,
	.input_configured = uclogic_input_configured,
#ifdef CONFIG_PM
	.resume	          = uclogic_resume,
	.reset_resume     = uclogic_resume,
#endif
};
module_hid_driver(uclogic_driver);

MODULE_AUTHOR("Martin Rusko");
MODULE_AUTHOR("Nikolai Kondrashov");
MODULE_LICENSE("GPL");
MODULE_VERSION("8");
