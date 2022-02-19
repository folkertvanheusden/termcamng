what it is
----------

This program runs an other program in an emulated ANSI terminal.
The terminal is rendered to a PNG file which is then served via a http-
(web-)server.
You can connect to it using a telnet program and then interact with
the program that is running.


required
--------

 * libpng-dev
 * libmicrohttpd-dev
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



Written by Folkert van Heusden <mail@vanheusden.com>

License: Apache License v2.0
