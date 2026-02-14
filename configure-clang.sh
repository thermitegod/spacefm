#!/usr/bin/env bash

CC=clang
CXX=clang++
LD=ld.lld
# LLVM binutils
AR=llvm-ar
AS=llvm-as
CPP=clang-cpp
NM=llvm-nm
STRIP=llvm-strip
RANLIB=llvm-ranlib
OBJCOPY=llvm-objcopy
STRINGS=llvm-strings
OBJDUMP=llvm-objdump
READELF=llvm-readelf
ADDR2LINE=llvm-addr2line

meson setup \
    -Dtests=true \
    -Ddev=true \
    -Dtools=true \
    -Dwith-system-glaze=true \
    -Dwith-system-ztd=true \
    --buildtype=debug \
    --prefix=$(pwd)/build ./build-clang

# debug
# debugoptimized

# -Db_sanitize=undefined -Db_lundef=false \
# -Db_sanitize=address -Db_lundef=false \
# -Db_sanitize=address,undefined -Db_lundef=false \
