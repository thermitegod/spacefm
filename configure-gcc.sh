#!/usr/bin/env bash

export CC=gcc
export CXX=g++
export LD=ld.bfd
# binutils
export AR=ar
export AS=as
export CPP=cpp
export NM=nm
export STRIP=strip
export RANLIB=ranlib
export OBJCOPY=objcopy
export STRINGS=strings
export OBJDUMP=objdump
export READELF=readelf
export ADDR2LINE=addr2line

meson setup \
    -Dtests=true \
    -Ddev=true \
    -Dtools=true \
    -Dwith-system-glaze=true \
    -Dwith-system-ztd=true \
    --buildtype=debugoptimized \
    --prefix=$(pwd)/build ./build-gcc

# debug
# debugoptimized

# -Db_sanitize=undefined -Db_lundef=false \
# -Db_sanitize=address -Db_lundef=false \
# -Db_sanitize=address,undefined -Db_lundef=false \
