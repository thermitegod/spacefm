#!/bin/bash

#CC=gcc CXX=g++ meson setup --buildtype=debug --prefix=$(pwd)/build ./build
#CC=gcc CXX=g++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

CC=clang CXX=clang++ meson setup \
    -Ddeprecated=false \
    -Dwith-system-cli11=true \
    -Dwith-system-concurrencpp=true \
    -Dwith-system-glaze=true \
    -Dwith-system-magic-enum=true \
    -Dwith-system-spdlog=true \
    -Dwith-system-toml11=false \
    -Dwith-system-ztd=true \
    --buildtype=debug \
    --prefix=$(pwd)/build ./build

#CC=clang CXX=clang++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=undefined -Db_lundef=false ./build
#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=address -Db_lundef=false ./build
#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=address,undefined -Db_lundef=false ./build
