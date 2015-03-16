DIGImend kernel drivers
=======================

This is a collection of graphics tablet drivers for the Linux kernel, produced
and maintained by the DIGImend project. We maintain this package to provide
newer drivers for older kernel versions which don't have them, and to allow
users to test new drivers before we contribute them to the mainline kernel.

This package supersedes the huion-driver package.

Model support is as follows:

    Original                    Rebranded as            Status*             Upstream in

       Huion 540                                        works               3.17
             580                                        likely works        3.11
             H420               osu!tablet              works               3.17
             H610                                       works               3.17
             H610 Pro                                   works               3.17
             H690                                       works               3.17
             K58                                        works               3.11
             W58                                        likely works        3.11
             W58L                                       works               3.11
             other                                      possibly works      3.17

     Yiynova MSP19U                                     works
             MSP19U+                                    works
             MVP22U+                                    works
             MVP10U                                     works (t)
             MVP10U IPS                                 works (t)
             MVP10U HD IPS                              works (t)
             DP10U                                      works (t)
             DP10U+                                     works (t)

         KYE MousePen i608X v2  Genius MousePen i608X   works

    * "works"           - tested, works
      "likely works"    - not tested, likely works (75% chance)
      "possibly works"  - not tested, possibly works (50% chance)

    (t) concerns only the input part of this display tablet, output (display)
        support is not provided by this package and should be verified
        separately

Please send your testing results to DIGImend-devel@lists.sourceforge.net, or
do a pull request with updates.

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
