DIGImend kernel drivers
=======================

This is a collection of graphics tablet drivers for the Linux kernel, produced
and maintained by the DIGImend project. We maintain this package to provide
newer drivers for older kernel versions which don't have them, and to allow
users to test new drivers before we contribute them to the mainline kernel.

See the [list of supported tablets][supported_tablets] on the [project
website][website].

Consider [becoming a patron][patreon_pledge] of the [project
maintainer][patreon_profile] to help make more tablets work with Linux.

Installing
----------

Kernel v3.5 or newer is required. Kernel headers or the build tree for the
running kernel are required.

On Debian-derived systems (such as Ubuntu and Mint) headers can be obtained by
installing appropriate version of `linux-headers` package. On Red Hat and
derived distributions the package name is `kernel-headers`.

Download appropriate files for one of the releases from the [releases
page][releases]. The "Download ZIP" link on the right of the GitHub page leads
to the source of the current development version, use it only if you know what
you're doing.

### Installing Debian package ###

If you're using Debian or a derived distro, such as Ubuntu, please try using
the experimental .deb package. If it works for you, this will remove the need
to reinstall the driver after each kernel upgrade. If it doesn't work, please
[report the issue][report_issue].

If you're not using a Debian-based distro, or the .deb package didn't work,
you can try installing the driver using DKMS directly, if you know how, or
manually as described below.

### Installing source package with DKMS ###

If you know how to use DKMS, you can try installing the package with it
directly, employing the experimental DKMS support. Please [report
issues][report_issue] if you find any.

### Installing source package manually ###

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

Note that if you built and installed the driver with `make` and `sudo make
install` as described above, you will need to do that again after each kernel
upgrade.

See the DIGImend project [support page](http://digimend.github.io/support/)
for further setup instructions. Please [report issues][report_issue] if you
find any.

Uninstalling
------------

### Manually-installed package ###

To uninstall a manually-installed package execute `make uninstall` as root in
the package source directory.

### Building Debian package ###

If you're a developer, or simply want to install a development version of the
drivers as a Debian package, make sure you have `dpkg-dev`, `debhelper`, and
`dkms` packages installed, and run the following command in the source
directory:

    dpkg-buildpackage -b --no-sign

The resulting package files will be written to the parent directory.

Upgrading / downgrading
-----------------------

### Manually-installed package ###

If you've manually installed a version of this package before, please
uninstall it before installing another one, using the sources you used for
installation.


[website]: http://digimend.github.io/
[supported_tablets]: http://digimend.github.io/drivers/digimend/tablets/
[releases]: https://github.com/DIGImend/digimend-kernel-drivers/releases
[report_issue]: https://github.com/DIGImend/digimend-kernel-drivers/issues/new
[wrapping_up]: http://spbnick.github.io/2016/07/31/Wrapping-up-DIGImend-work.html
[patreon_profile]: https://www.patreon.com/spbnick
[patreon_pledge]: https://www.patreon.com/bePatron?c=930980
