# Motivation
Nowadays, many developer are used to write code, cross-compile it, deploy on target and verify it.
The drawback of this approach is the need for a target hardware, requiring extra cost and effort.
The point is to start a simple bootloader application for arm64 that can be fully simulated using qemu emulator.

The bootloader will perform:
- loading the OS artifacts from NOR flash via QSPI interface
- validating the OS artifacts
- hand-off the artifacts.

Qemu is a linux emulator that simulated embedded software for various architectures on the x86 windows.

For more Information about the Qemu, the wike page can be visited:

https://en.wikipedia.org/wiki/QEMU



---

## Bootloader init procedure

### Exception table initialization

### Exception levels






