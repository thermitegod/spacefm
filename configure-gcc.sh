#!/usr/bin/env bash

CC=gcc
CXX=g++
LD=ld.bfd
# binutils
AR=ar
AS=as
CPP=cpp
NM=nm
STRIP=strip
RANLIB=ranlib
OBJCOPY=objcopy
STRINGS=strings
OBJDUMP=objdump
READELF=readelf
ADDR2LINE=addr2line

meson setup \
    -Dtests=true \
    -Ddev=true \
    -Dtools=true \
    -Dgtk3=true \
    -Dgtk4=true \
    -Dwith-system-glaze=true \
    -Dwith-system-ztd=true \
    --buildtype=debug \
    --prefix=$(pwd)/build ./build-gcc

# debug
# debugoptimized

# -Db_sanitize=undefined -Db_lundef=false \
# -Db_sanitize=address -Db_lundef=false \
# -Db_sanitize=address,undefined -Db_lundef=false \
