DIGImend kernel drivers
=======================

This is a collection of graphics tablet drivers for the Linux kernel, produced
and maintained by the DIGImend project. We maintain this package to provide
newer drivers for older kernel versions which don't have them, and to allow
users to test new drivers before we contribute them to the mainline kernel.

See the [list of supported tablets](http://digimend.github.io/drivers/digimend/tablets/)
on the [project website](http://digimend.github.io/).

Please send your testing results to DIGImend-devel@lists.sourceforge.net.

Installing
----------

Kernel v3.5 or newer is required. Kernel headers or the build tree for the
running kernel are required.

On Debian-derived systems (such as Ubuntu and Mint) headers can be obtained by
installing appropriate version of `linux-headers` package. On Red Hat and
derived distributions the package name is `kernel-headers`.

Download one of the releases from the [releases
page](https://github.com/DIGImend/digimend-kernel-drivers/releases). The
"Download ZIP" link on the right of the GitHub page leads to the current
development version, use it only if you know what you're doing.

To build the drivers run `make` in the package's directory.

Please disregard the possible "Can't read private key" messages. They don't
affect the driver functionality, unless you set up kernel module signature
verification.

To install the drivers run `sudo make install` in the package's directory.

Make sure the previous versions of the drivers were unloaded from memory with
the following commands:

    sudo rmmod hid-kye
    sudo rmmod hid-uclogic
    sudo rmmod hid-huion

and reconnect the tablet. Or simply reboot the machine.

See the DIGImend project [support page](http://digimend.github.io/support/)
for further setup instructions.

Uninstalling
------------

To uninstall the package execute `make uninstall` as root in the package
source directory.

Upgrading / downgrading
-----------------------

If you've installed a version of this package before, please uninstall it
before installing another one, using the sources you used for installation.
