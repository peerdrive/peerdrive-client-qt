
This repository provides the Qt 4 bindings and a command line utility to
interact with PeerDrive on the shell.

Requirements
============

For compilation you should have the following installed:
 * Qt 4 SDK
 * Google protocol buffers library and headers
   (https://developers.google.com/protocol-buffers/)

Compiling
=========

Should be as simple as:

    qmake && make

If you just want to build the library and applications without installing them
you should invoke qmake the following way:

    qmake CONFIG+=debug

This will build the debug version which runs from the directory where it was
built.

License
=======

The peerdrive-qt library is licensed under LGPL-V3. All other parts are
licensed under GPL-v3.

