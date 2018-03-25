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

#ifndef module_hid_driver
/**
 * module_hid_driver() - Helper macro for registering a HID driver
 * @__hid_driver: hid_driver struct
 *
 * Helper macro for HID drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_hid_driver(__hid_driver) \
	module_driver(__hid_driver, hid_register_driver, \
		      hid_unregister_driver)
#endif

#ifndef from_timer
#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)
#endif

/* If we're on an old kernel where setup_timer was still defined */
#ifdef setup_timer
/*
 * Define a replacement of timer_setup found in newer kernels.
 * NOTE Casting function pointers like this is a dirty hack,
 * 	but will probably work, and should do for now.
 */
#define timer_setup(timer, callback, flags) \
	__setup_timer((timer), (void (*)(unsigned long))(callback), \
			(unsigned long)(timer), (flags))
#endif

#endif
