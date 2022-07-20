DIGImend kernel drivers
=======================

[![Travis CI Build Status][travis_ci_badge]][travis_ci_page]

This is a collection of graphics tablet drivers for the Linux kernel, produced
and maintained by the DIGImend project. We maintain this package to provide
newer drivers for older kernel versions which don't have them, and to allow
users to test new drivers before we contribute them to the mainline kernel.

See the [list of supported tablets][supported_tablets] on the [project
website][website].

Consider [becoming a patron][patreon_pledge] of the [project
maintainer][patreon_profile] to help make more tablets work with Linux.
You can also [support the maintainer on Liberapay][liberapay_profile], or you
can [buy him a coffee][buymeacoffee_profile]!

Installing
----------

Kernel v3.5 or newer is required.

Download appropriate files for one of the releases from the [releases
page][releases]. The "Download ZIP" link on the right of the GitHub page leads
to the source of the current development version, use it only if you know what
you're doing.

### Installing Debian package ###

If you're using Debian or a derived distro, such as Ubuntu, and are installing
a release, please use the .deb package. If you're not using a Debian-based
distro, or the .deb package didn't work, you can install the driver using
DKMS, or manually, as described below.

### Installing from source ###

Source is either an unpacked release tarball (.tar.gz file), an unpacked
source code archive downloaded from GitHub (.zip file), or source code checked
out from Git.

Before installing from source in any way, make sure you have the headers for
your kernel installed (on Debian-based systems):

    sudo apt-get install -y "linux-headers-$(uname -r)"

or (on Fedora-based systems):

    sudo dnf install -y "kernel-devel-uname-r == $(uname -r)"

If you get "Error: Unable to find a match" from the above command, make sure
your kernel is up-to-date, and if not, update it and try again.

#### Installing from source with DKMS ####

DKMS (Dynamic Kernel Module Support) is a system for installing out-of-tree
Linux kernel modules, such as DIGImend kernel drivers. It helps make sure the
modules are built with correct kernel headers and are properly installed, and
also automatically reinstalls the modules when the kernel is updated.

Installing with DKMS is the recommended way of installing development versions
of DIGImend kernel drivers.

To install with DKMS, make sure you have the `dkms` package installed (on
Debian-based distros):

    sudo apt-get install -y dkms

or (on Fedora-based distros):

    sudo dnf install -y dkms

After that, run the following command from the source directory to install:

    sudo make dkms_install

Watch for any errors in the output, and if the drivers installed successfully,
they will be automatically rebuilt and reinstalled each time the kernel is
updated.

#### Installing from source manually ####

To install from source manually, first build the drivers. Run the following
command in the source directory:

    make

Then, to install the drivers, run this command in the same directory:

    sudo make install

Note that if you have built and installed the drivers this way, you will need to
run `make clean` in the source directory, and then redo the above, after each
kernel upgrade.

#### SSL errors during installation ####

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
After installing the drivers, make sure the previous versions of the drivers
were unloaded. To do that, simply reboot the machine. Alternatively, execute
the following command:

    sudo modprobe -r hid-kye hid-uclogic hid-polostar hid-viewsonic

and reconnect the tablet.

If your tablet is supported, its pen will work after this, and applications
will be able to recognize the pressure after appropriate configuration. Refer
to the application documentation for instructions on how to do that, but in
most cases it is enough to simply enable the tablet in the application.

### X.org drivers ###

By default, your tablet will be handled by the libinput X.org driver
(`xserver-xorg-input-libinput` package in Debian, Ubuntu, and derived distros,
`xorg-x11-drv-libinput` in Fedora and derived distros). This driver will
support reporting pen coordinates and pressure, and some frame controls.

Some tablets, however, can work with the Wacom driver, which has support for
configuring pressure curves, keyboard shortcuts for buttons on the tablet
frame, and other advanced features. This driver package includes configuration
enabling Wacom driver for tablets that are known to work with it.

For those tablets, all you need to do is make sure the Wacom driver is
installed, which is done automatically when installing the driver from the
.deb package. In other cases, you can install the driver package
(`xserver-xorg-input-wacom` or `xorg-x11-drv-wacom`) yourself. To verify that
your tablet is handled by the Wacom driver, see if its devices appear in the
output of `xsetwacom list`.

### Enabling Wacom X.org driver ###

If this driver package hasn't configured your tablet to work with the Wacom
driver, but you know it works, or would like to try to make it work, you can
write the following to `/etc/X11/xorg.conf.d/50-tablet.conf` file:

    Section "InputClass"
        Identifier "Tablet"
        Driver "wacom"
        MatchDevicePath "/dev/input/event*"
        MatchUSBID "<VID>:<PID>"
    EndSection

Here `<VID>` and `<PID>` would be the tablet's USB vendor and product IDs
respectively, as seen in `lsusb` output. E.g. if your tablet's line in `lsusb`
output looks like this:

    Bus 001 Device 003: ID 1234:abcd

then your `/etc/X11/xorg.conf.d/50-tablet.conf` should look like this:

    Section "InputClass"
        Identifier "Tablet"
        Driver "wacom"
        MatchDevicePath "/dev/input/event*"
        MatchUSBID "1234:abcd"
    EndSection

You will need to log out of your X.org session and login again, or simply
restart your machine for these changes to take effect. If your configuration
worked, please open an issue, or contribute code directly, to have
configuration for your tablet included into this driver package.

### Configuring Wacom X.org driver ###

If your tablet is handled by the Wacom driver, you should be able to use the
`xsetwacom` tool to configure the advanced features.

For example, if `xsetwacom list` produces this output:

    HID 256c:006e Pad pad                   id: 9   type: PAD
    HID 256c:006e Pen stylus                id: 10  type: STYLUS

you can assign Ctrl-Z ("Undo") key combination to the fifth button on the
tablet frame this way:

    xsetwacom set "HID 256c:006e Pad pad" button 9 key Ctrl Z

Note that buttons are numbered 1, 2, 3, 8, 9, 10, and so on, i.e. buttons 4,
5, 6, and 7 are not used. They're reserved for vertical and horizontal
scrolling events by the X server.

Another example: if `xrandr` output has this line:

    HDMI-3 connected 1440x900+0+0 (normal left inverted right x axis y axis) 408mm x 255mm

you can restrict the tablet input to that display like this:

    xsetwacom set "HID 256c:006e Pen stylus" MapToOutput HDMI-3

See [the `xsetwacom` man page][xsetwacom_manpage] for more parameters and
details.

### Wacom GUI configuration tools ###

Note that so far, in most cases, graphical Wacom tablet configuration tools
won't work with non-Wacom tablets, and you will need to use the `xsetwacom`
tool, even if the Wacom X.org driver supports them.

Uninstalling
------------

### Debian package ###

To uninstall a Debian package simply use your favorite package-management
tools.

### DKMS-installed package ###

To uninstall a DKMS-installed package execute `make dkms_uninstall` as root in
the package source directory.

### Manually-installed package ###

To uninstall a manually-installed package execute `make uninstall` as root in
the package source directory.

Upgrading / downgrading
-----------------------

### Manually-installed package ###

If you've manually installed a version of this package before, please
uninstall it before installing another one, using the sources you used for
installation.

Building Debian package
-----------------------

If you're a developer, or simply want to install a development version of the
drivers as a Debian package, make sure you have `dpkg-dev`, `debhelper`, and
`dkms` packages installed, and run the following command in the source
directory:

    dpkg-buildpackage -b -uc

The resulting package files will be written to the parent directory.

Known issues
------------

### DKMS issues preventing correct installation ###

If you're installing Debian packages, or installing from source with DKMS, you
might hit one of DKMS bugs which prevent some of the driver modules from
installing.

They make DKMS produce messages like this:

    hid-uclogic.ko:
    Running module version sanity check.
    Error! Module version 7 for hid-uclogic.ko
    is not newer than what is already found in kernel 4.9.0-5-amd64 (7).
    You may override by specifying --force.

or this:

    hid-uclogic.ko.xz:
    Running module version sanity check.
    Error! Module version 9 for hid-uclogic.ko.xz
    is not newer than what is already found in kernel 3.10.0-862.14.4.el7.x86_64 (27A2028780DCB320780F53D).

while trying to install the drivers.

Fixes for these were accepted upstream ([first fix][dkms_issue1_pr] and
[second fix][dkms_issue2_pr]) and should eventually appear in distributions.
Meanwhile, to fix the issues, you can apply these yourself, or execute the
following command:

    sudo sed -i \
             -e '/^get_module_verinfo()/,+3 s/\<unset res$\|\<res=()$/res=("" "" "")/' \
             /usr/sbin/dkms

Be aware that the operation of the above command is inexact, and might not
work, or might break DKMS. You've been warned. In any case, simply reinstall
DKMS to restore it.

### Systems with Secure Boot enabled ###

If your system has [Secure Boot][secure_boot] enabled, then the installed
driver modules won't be permitted to load. You will see messages like
"Required key not available". To make them work, you will need to sign them,
or disable Secure Boot entirely. See documentation for your Linux distribution
on how to sign kernel modules, or documentation for your computer's UEFI
firmware on how to disable Secure Boot.

### Touch ring/strip scrolling doesn't work in Gnome ###

Scrolling with a touch ring or a touch strip [doesn't
work][gnome_touch_scroll_issue] in Gnome version 3.24 and later. At the moment
there doesn't seem to be a workaround beside not using Gnome.

Support
-------

If you have any problems with the drivers, look through [HOWTOs][howtos] on
[the project website][website], and search for solutions and report new issues
at [the issues page on GitHub][issues].

If you find somebody has already reported your issue and it's not resolved
yet, leave a thumbs-up :thumbsup: reaction on the first post in the issue.
This will let developers identify popular issues and pick them first for
solving!

Join the [#DIGImend channel on irc.libera.chat][irc_channel] to discuss the
drivers, tablets, development, to ask for help, and to help others!

[travis_ci_badge]: https://travis-ci.org/DIGImend/digimend-kernel-drivers.svg?branch=master
[travis_ci_page]: https://travis-ci.org/DIGImend/digimend-kernel-drivers
[website]: http://digimend.github.io/
[supported_tablets]: http://digimend.github.io/drivers/digimend/tablets/
[releases]: https://github.com/DIGImend/digimend-kernel-drivers/releases
[patreon_profile]: https://www.patreon.com/spbnick
[patreon_pledge]: https://www.patreon.com/bePatron?c=930980
[liberapay_profile]: https://liberapay.com/spbnick/
[buymeacoffee_profile]: https://www.buymeacoffee.com/spbnick
[dkms_issue1_pr]: https://github.com/dell/dkms/pull/47
[dkms_issue2_pr]: https://github.com/dell/dkms/pull/66
[secure_boot]: https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface#Secure_boot
[xsetwacom_manpage]: https://www.mankier.com/1/xsetwacom
[howtos]: http://digimend.github.io/support/
[issues]: https://github.com/DIGImend/digimend-kernel-drivers/issues
[irc_channel]: https://web.libera.chat/#DIGImend
[gnome_touch_scroll_issue]: https://gitlab.gnome.org/GNOME/gnome-control-center/issues/118
