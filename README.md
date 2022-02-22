what it is
----------

This program runs an other program in an emulated ANSI terminal.
The terminal is rendered to a PNG file which is then served via a http-
(web-)server.
You can connect to it using an SSH or telnet program and then interact
with the program that is running.


required
--------

 * libmicrohttpd-dev
 * libpam0g-dev
 * libpng-dev
 * libssh-dev
 * libyaml-cpp-dev


creating
--------

 * mkdir build
 * cd build
 * cmake ..
 * make


running
-------

Run 'termcamng' with an optional path to a yaml-file containing the
configuration. See termcamng.yaml included for an example.

Note that you need to generate host-keys for the SSH functionality
to work (see "ssh-keygen -A").


http
----

 * http://ip-adres/        <-- 1 PNG frame
 * http://ip-adres/stream  <-- stream of PNG images


files
-----

CP850.F16 comes from https://github.com/viler-int10h/vga-text-mode-fonts/releases/download/2020-11-25/VGAfonts-20-11-25-NO-previews.zip


Written by Folkert van Heusden <mail@vanheusden.com>

License: Apache License v2.0
