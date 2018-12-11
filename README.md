# MCOMMS (MeidoComms)
SioFS file access host, serial monitor with limited terminal support and loader program for [LITELOAD](https://github.com/Lameguy64/liteload).

SioFS host supports opening/creating files, reading, writing, directory navigation and listing on the host side all done through serial. Originally developed for PlayStation homebrew purposes it can also be used for other devices with a serial interface as well.

## Compiling
This program can be compiled using the MinGW GCC toolchain on Windows. Compatibility with Microsoft Visual C is not guaranteed.

Compile by opening the project with Netbeans or execute 'mingw32-make CONF=Release' in a command line.

This program can also be compiled on Linux but the serial routines need to be reworked.

## Patcher binaries
During the development of PSn00b Debugger a so called patch binary mechanism was implemented to allow for debug patches to be installed before uploading a program. Patch binaries are simply little binary executables that are always loaded to 0x80010000 and executed by the loader as a C function in which it can install patches and apply modifications to the kernel space.

For more information on how to make your own patch binaries please see LITELOAD's readme file.

## Changelog

**Version 0.82 (11/18/2018)**
* Updated protocol for LITELOAD 1.1. A command line option is available to revert back to LITELOAD 1.0's protocol.
* Added support for patcher binaries.

**Version 0.80 (06/22/2018)**
* Initial release.