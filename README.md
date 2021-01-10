# MCOMMS (MeidoComms)
SioFS file access host, serial monitor with limited terminal support and loader
program for [LITELOAD](https://github.com/Lameguy64/liteload).

SioFS host supports opening/creating files, reading, writing, directory
navigation and listing on the host side all done through serial. Originally
developed for PlayStation homebrew purposes it can also be used for other
devices with a serial interface as well.

## Compiling
Building MCOMMS under Windows requires MinGW32 and MSys2. Simply run the
makefile and it should produce an mcomms.exe file. Under Linux, the
build-essential(s) package is required. Simply run the makefile and
it should produce an mcomms executable file.

## Patcher binaries
During the development of PSn00b Debugger, a so called patch binary mechanism
was implemented to allow for debug monitor patches to be installed before
uploading a PS-EXE to the target console. Patch binaries are simply little
binary executables that are always loaded to 0x80010000 and executed by the
loader as a C function, of which, will install patches and apply modifications
to the kernel space to enable debugging functionality.

For more information on how to make your own patch binaries please see
LITELOAD's readme file.

## Changelog
**Version 0.86 (needs testing on Windows)**
* Includes Slamy's Linux support fixes.
* Terminal mode is now always enabled.
* Hardware handshake now supported on Linux.
* Fixed tty text output issues.
* Added support for uploading ELF files.
* Restored SioFS support (was accidentally left disabled in the last version).
* Fixed serial code for Linux further.
* Improved output messages.
* Added progress bars for PS-EXE and binary file upload operations.

**Version 0.85 (09/16/2020, not yet Linux tested)**
* Replaced std::couts with regular printf().
* Improved some code formatting.
* Ditched Netbeans project directory in favor of a simple makefile.
* DTR and RTS lines now kept high by default for compatibility with flow
  control enabled cables such as an FTDI232 with flow control lines wired,
  HIT-SERIAL adapter or Net Yaroze cable (for Win32 only).
* Added handshake enabled option for upcoming versions of n00bROM and
  LITELOAD (for Win32 only).

**Version 0.82 (11/18/2018)**
* Updated protocol for LITELOAD 1.1. A command line option is available to revert back to LITELOAD 1.0's protocol.
* Added support for patcher binaries.

**Version 0.80 (06/22/2018)**
* Initial release.
