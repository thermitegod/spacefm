# SpaceFM

## Description

SpaceFM is a multi-panel tabbed file and desktop manager for Linux with a
customisable menu system, and bash integration. SpaceFM aims to provide
a stable, capable file manager with significant customisation capabilities.

## GTK3 Graphical Issue "Fix"

You can find the instructions on how to fix it [**here**](extra/README-GTK3.md).

## Building

__Gentoo__

ebuild at
https://github.com/thermitegod/overlay/tree/master/x11-misc/spacefm


__Everyone Else__

```
mkdir build
cd build
meson ..
ninja
```


## LICENSE

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.
