#!/usr/bin/env bash

export CC=clang
export CXX=clang++
export LD=ld.lld
# LLVM binutils
export AR=llvm-ar
export AS=llvm-as
export CPP=clang-cpp
export NM=llvm-nm
export STRIP=llvm-strip
export RANLIB=llvm-ranlib
export OBJCOPY=llvm-objcopy
export STRINGS=llvm-strings
export OBJDUMP=llvm-objdump
export READELF=llvm-readelf
export ADDR2LINE=llvm-addr2line

meson setup \
    -Dtests=true \
    -Ddev=true \
    -Dtools=true \
    -Dwith-system-glaze=true \
    -Dwith-system-ztd=true \
    --buildtype=debugoptimized \
    --prefix=$(pwd)/build ./build-clang

# debug
# debugoptimized

# -Db_sanitize=undefined -Db_lundef=false \
# -Db_sanitize=address -Db_lundef=false \
# -Db_sanitize=address,undefined -Db_lundef=false \
