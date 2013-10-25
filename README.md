Huion-driver
============

This is a Linux kernel driver for Huion graphics tablets.

Model support is as follows:

    H610    tested      works
    580     untested    likely works
    K58     untested    likely works
    W58     untested    likely works
    other   untested    can work

This driver should support more models than the hid-huion driver present in
the kernel as of v3.12-rc1, but it wasn't tested with many and so is not yet
merged with the mainline. The table above will be updated as additional models
are tested.

Please send your testing results to DIGImend-devel@lists.sourceforge.net.

Installing
----------

Kernel v3.5 or newer is required. Kernel headers or the build tree for the
running kernel are required.

On Debian-derived systems (such as Ubuntu and Mint) headers can be obtained by
installing appropriate version of "linux-headers" package.

To build the driver run "make" in the driver's source directory.

If you run Ubuntu or a derived distro, you might need to add "extra" to the
directory search list in /etc/depmod/ubuntu.conf before installation.

To install the driver and the associated rebinding script run "make install"
as root in the source directory.

If you run a v3.11 or later kernel you will need to unload the mainline driver
with a "rmmod hid-huion" command so this driver can be loaded, first time you
use the tablet. Then, (re-)plugging the tablet should be sufficient to make it
work.
