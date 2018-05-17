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
to reinstall the driver after each kernel upgrade.

If you're not using a Debian-based distro, or the .deb package didn't work,
you can try installing the driver using DKMS directly, if you know how, or
manually as described below.

### Installing source package with DKMS ###

If you know how to use DKMS, you can try installing the package with it
directly, employing the experimental DKMS support.

### DKMS issue preventing correct installation ###

If you're installing Debian packages, or installing source packages with DKMS
directly, you might hit a bug in DKMS which prevents some of the driver
modules from installing. If you do, you will see a message like this while
trying to install the drivers:

    hid-uclogic.ko:
    Running module version sanity check.
    Error! Module version 7 for hid-uclogic.ko
    is not newer than what is already found in kernel 4.9.0-5-amd64 (7).
    You may override by specifying --force.

For details see [upstream pull-request fixing the issue][dkms_issue_pr].

To fix that you can apply the patch linked above yourself, or execute the
below command:

    sudo sed -i -e 's/\<unset res$/res=()/' /usr/sbin/dkms

Be aware that the operation the above command does is inexact, and might not
work, or might break DKMS. You've been warned. In any case, simply reinstall
DKMS to restore it if something goes wrong.

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

### SSL errors during installation ###

On Ubuntu, and possibly other distros, the driver installation process
attempts to cryptographically sign the modules being installed. Most of the
users don't have the system configured to support this, so during the
installation they get error messages similar to these:

      INSTALL /home/danghai/digimend-kernel-drivers/hid-uclogic.ko
    At main.c:160:
    - SSL error:02001002:system library:fopen:No such file or directory: bss_file.c:175
    - SSL error:2006D080:BIO routines:BIO_new_file:no such file: bss_file.c:178
    sign-file: certs/signing_key.pem: No such file or directory

The above basically means that the system tried to sign the module, but
couldn't find the key to sign with. This does not interfere with module
installation and operation and can safely be ignored. That is, unless you set
up module signature verification, but then you would recognize the problem,
and would be able to fix it.

Configuration
-------------
If your tablet is supported, its pen will work after installing these drivers,
and applications will be able to recognize the pressure after appropriate
configuration. Refer to the application documentation for instructions on how
to do that, but in most cases it is enough to simply enable the tablet in the
application.

By default, your tablet will be handled by the libinput X.org driver
(`xserver-xorg-input-libinput` package in Debian, Ubuntu, and derived distros,
`xorg-x11-drv-libinput` in Fedora and derived distros). This driver will
support reporting pen coordinates and pressure, and some frame controls.

However, it will not support configuring pressure curves, keyboard shortcuts
for buttons on the tablet frame, or other advanced features. For that you
will need to use the Wacom driver. To do that, make sure you have the package
installed (`xserver-xorg-input-wacom` or `xorg-x11-drv-wacom`) and write the
following to `/etc/X11/xorg.conf.d/50-tablet.conf` file:

    Section "InputClass"
        Identifier "Tablet"
        Driver "wacom"
        MatchDevicePath "/dev/input/event*"
        MatchUSBID "<VID>:<PID>"
    EndSection

Here `<VID>` and `<PID>` would be the tablet's USB vendor and product IDs
respectively, as seen in `lsusb` output. E.g. if your tablet's line in `lsusb`
output looks like this:

    Bus 001 Device 003: ID 256c:006e

then your `/etc/X11/xorg.conf.d/50-tablet.conf` should look like this:

    Section "InputClass"
        Identifier "Tablet"
        Driver "wacom"
        MatchDevicePath "/dev/input/event*"
        MatchUSBID "256c:006e"
    EndSection

Next, log out of your X.org session and login again, or simply restart your
machine. To verify that the tablet is now handled by the Wacom driver execute
`xsetwacom list` and check that your tablet appears in the output at least
once.

After that, you should be able to use the `xsetwacom` tool to configure the
advanced features.

For example, if `xsetwacom list` produces this output:

    HID 256c:006e Pad pad                   id: 9   type: PAD
    HID 256c:006e Pen stylus                id: 10  type: STYLUS

you can assign Ctrl-Z ("Undo") key combination to the fifth button on the
tablet frame this way (note that buttons are numbered 1, 2, 3, 8, 9, 10, and so
on):

    xsetwacom set "HID 256c:006e Pad pad" button 9 key Ctrl Z

and if `xrandr` output has this line:

    HDMI-3 connected 1440x900+0+0 (normal left inverted right x axis y axis) 408mm x 255mm

you can restrict the tablet input to that display like this:

    xsetwacom set "HID 256c:006e Pen stylus" MapToOutput HDMI-3

See [the `xsetwacom` man page][xsetwacom_manpage] for more parameters and
details.

Note that so far, in most cases, graphical Wacom tablet configuration tools
won't work with non-Wacom tablets.

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

    dpkg-buildpackage -b -uc

The resulting package files will be written to the parent directory.

Upgrading / downgrading
-----------------------

### Manually-installed package ###

If you've manually installed a version of this package before, please
uninstall it before installing another one, using the sources you used for
installation.

Support
-------

If you have any problems with the drivers, look through [HOWTOs][howtos] on
[the project website][website], and search for solutions and report new issues
at [the issues page on GitHub][issues]. Join the [#DIGImend channel on
irc.freenode.net][irc_channel] to discuss the drivers, tablets, development,
to ask for help, and to help others!

[website]: http://digimend.github.io/
[supported_tablets]: http://digimend.github.io/drivers/digimend/tablets/
[releases]: https://github.com/DIGImend/digimend-kernel-drivers/releases
[patreon_profile]: https://www.patreon.com/spbnick
[patreon_pledge]: https://www.patreon.com/bePatron?c=930980
[dkms_issue_pr]: https://github.com/dell/dkms/pull/47
[xsetwacom_manpage]: https://www.mankier.com/1/xsetwacom
[howtos]: http://digimend.github.io/support/
[issues]: https://github.com/DIGImend/digimend-kernel-drivers/issues
[irc_channel]: https://webchat.freenode.net/?channels=DIGImend
