#!/bin/bash

#CC=gcc CXX=g++ meson setup --buildtype=debug --prefix=$(pwd)/build ./build
#CC=gcc CXX=g++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

CC=clang CXX=clang++ meson setup \
    -Dtests=true \
    -Ddev=true \
    -Dtools=true \
    -Dwith-system-glaze=true \
    -Dwith-system-ztd=true \
    --buildtype=debugoptimized \
    -Db_lto=true -Db_lto_mode=thin -Db_thinlto_cache=true \
    --prefix=$(pwd)/build ./build

    # -Db_sanitize=address -Db_lundef=false \

#CC=clang CXX=clang++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=undefined -Db_lundef=false ./build
#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=address -Db_lundef=false ./build
#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=address,undefined -Db_lundef=false ./build
