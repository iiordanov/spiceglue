# Spiceglue Library

Serves as a middle layer between a UI application and spice-gtk. Originally
forked by the FlexVDI project for their own purposes from the
[glue code in the Remote Desktop Clients codebase]
(https://github.com/iiordanov/remote-desktop-clients/tree/master/remoteClientLib/jni).

There may still be some references to proprietary FlexVDI extensions which
may be removed over time but are easily disabled by configure and compile
time parameters.

## Prerequisites

Install the `spice-gtk` library as a dependency first. Install instructions
vary by operating system, but for Debian and Ubuntu you could use `apt`
and for MacOS you could use `brew`.

## Compilation

Run the following to compile the library with all proprietary dependencies
from FlexVDI disabled.

```bash
autoreconf --install
export CFLAGS='-DSPICEGLUE_DISABLE_POWER'
export LDFLAGS="-L/opt/homebrew/Cellar/libusb/1.0.29/lib"
./configure --enable-printing=no
make
```

