# MCOMMS (MeidoComms)
SioFS file access host, serial monitor with limited terminal support and loader program for [LITELOAD](https://github.com/Lameguy64/liteload).

SioFS host supports opening/creating files, reading, writing, directory navigation and listing on the host side all done through serial. Originally developed for PlayStation homebrew purposes it can also be used for other devices with a serial interface as well.

## Compiling
This program can be compiled using the MinGW GCC toolchain on Windows. Compatibility with Microsoft Visual C is not guaranteed.

Compile by opening the project with Netbeans or execute 'mingw32-make CONF=Release' in a command line.

This program can also be compiled on Linux but the serial routines need to be reworked.